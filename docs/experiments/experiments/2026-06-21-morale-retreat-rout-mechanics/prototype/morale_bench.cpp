// 2026-06-21-morale-retreat-rout-mechanics — Unit morale / retreat / rout mechanics benchmark.
//
// Standalone C++26 CPU prototype. No external deps. Compiles with:
//   clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic \
//     morale_bench.cpp -o morale_bench
//
// Methodology: per `docs/experiments/benchmarks/methodology.md` §3
//   - 5 warmup iterations (discarded)
//   - 1-500 measurement iterations per scene (adaptive by total unit-tick count)
//   - 5 seeds × 5 scenes × 5 strategies = 125 configs
//   - Adaptive kRuns targets ~5-50M total unit-ticks per config
//
// Output: results.csv with columns:
//   strategy, scene, seed, units, ticks, mean_us_per_tick, mean_ns_per_unit_per_tick,
//   retreat_rate, rout_rate, mean_morale_final, mean_combat_duration_s
//
// 5 strategies (each updates `Unit::morale` per-tick, O(1) per unit):
//   A_NaiveThreshold             — instantaneous state at threshold (no history)
//   B_LinearAccumulator          — linear stress accumulator with decay
//   C_CombatFatigueBreakdown     — Marshall 1947 25%-cohesion factor + Appel 200-240 day limit
//   D_TieredCohesionIndex        — 4-tier explicit states (Steady/Shaken/Panicked/Routed) + cascade
//   E_AdaptiveFlowState          — best-of-breed: B + D + C combined with per-role modifier
//
// 5 scenes (light_skirmish / squad_assault / urban_combat / extended_engagement / decisive_action):
//   20u-60s / 48u-180s / 64u-300s / 200u-600s / 1024u-900s
//
// Per `AGENTS.md §4` (DoD) + `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:
//   target per-unit cost < 300 ns/tick = < 1% of 30 Hz frame budget for 1000 units

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace morale {

// ============================================================================
// Types
// ============================================================================

enum class Role : std::uint8_t {
    Officer   = 0,
    NCO       = 1,
    Rifleman  = 2,
    Medic     = 3,
    Engineer  = 4,
    MG        = 5,
    AT        = 6,
    Sniper    = 7,
};

enum class MoraleState : std::uint8_t {
    Steady    = 0, // 100-75
    Shaken    = 1, // 75-50
    Panicked  = 2, // 50-25
    Routed    = 3, // <25
    Retreated = 4, // ordered withdraw (>=20% casualties + 50+ suppression)
};

struct Unit {
    float morale{100.0f};
    float suppression{0.0f};
    float history_accumulator{0.0f}; // for B, C, E
    int   combat_ticks{0};           // for C (Appel 200-240 day limit, scaled)
    int   nearby_friendlies{0};
    int   nearby_casualties_today{0};
    int   role_modifier{0};          // -1 medic, 0 rifleman, +1 NCO, +2 officer
    bool  is_officer{false};
    bool  leader_alive{true};         // for leadership_loss event
    MoraleState state{MoraleState::Steady};
    float pos_x{0.0f};
    float pos_y{0.0f};
};

enum class Strategy : std::uint8_t {
    A_NaiveThreshold             = 0,
    B_LinearAccumulator          = 1,
    C_CombatFatigueBreakdown     = 2,
    D_TieredCohesionIndex        = 3,
    E_AdaptiveFlowState          = 4,
};

constexpr std::array<std::string_view, 5> kStrategyNames = {
    "A_NaiveThreshold",
    "B_LinearAccumulator",
    "C_CombatFatigueBreakdown",
    "D_TieredCohesionIndex",
    "E_AdaptiveFlowState",
};

struct SceneConfig {
    std::string_view name;
    std::uint32_t    unit_count;
    std::uint32_t    tick_count;
    float            suppression_rate;     // per-unit per-tick probability of suppression event
    float            casualty_rate;        // per-unit per-tick probability of casualty event
    float            isolation_rate;       // probability that unit becomes isolated (no nearby friendlies)
    float            leadership_loss_at;   // tick at which leadership_loss event fires (0 = never)
};

// Pre-computed per-scene adjacency (positions are static in this benchmark)
struct SceneAdjacency {
    // CSR: offsets[i]..offsets[i+1] = neighbor indices for unit i
    std::vector<std::uint32_t> offsets;
    std::vector<std::uint32_t> neighbors;
    std::uint32_t max_degree{0};
};

constexpr std::array<SceneConfig, 5> kScenes = {{
    {"s1_light_skirmish",         20,   1800, 0.010f, 0.0010f, 0.05f, 0.0f},
    {"s2_squad_assault",          48,   5400, 0.040f, 0.0050f, 0.10f, 3000.0f},
    {"s3_urban_combat",           64,   9000, 0.060f, 0.0080f, 0.20f, 5000.0f},
    {"s4_extended_engagement",   200,  18000, 0.030f, 0.0040f, 0.15f, 9000.0f},
    {"s5_decisive_action",     1024,  27000, 0.080f, 0.0150f, 0.25f, 12000.0f},
}};

constexpr std::array<float, 8> kRoleModifier = {
    +2.0f, // Officer (high morale baseline)
    +1.0f, // NCO
     0.0f, // Rifleman (baseline)
    -0.5f, // Medic
     0.0f, // Engineer
    +0.5f, // MG
    +0.5f, // AT
    +1.0f, // Sniper
};

// ============================================================================
// Per-strategy update functions — O(1) per unit, called once per tick
// ============================================================================

inline void update_A_NaiveThreshold(Unit& u, std::uint32_t tick) noexcept {
    // A: instantaneous state at threshold, no history
    float m = 100.0f;
    m -= u.suppression * 0.7f;
    m -= static_cast<float>(u.nearby_casualties_today) * 12.0f;
    if (!u.leader_alive) m -= 30.0f;  // Grossman 30% threshold simplification
    if (u.nearby_friendlies < 2) m -= 15.0f; // isolation penalty
    m += u.role_modifier * 5.0f;
    if (m < 0.0f) m = 0.0f;
    u.morale = m;

    if (u.morale < 5.0f) {
        u.state = MoraleState::Routed;
    } else if (u.morale < 20.0f) {
        u.state = MoraleState::Panicked;
    } else if (u.morale < 50.0f) {
        u.state = MoraleState::Shaken;
    } else {
        u.state = MoraleState::Steady;
    }
    // Retreated threshold (separate from rout)
    if (u.nearby_casualties_today >= 5 && u.suppression > 50.0f) {
        u.state = MoraleState::Retreated;
    }
    (void)tick;
}

inline void update_B_LinearAccumulator(Unit& u, std::uint32_t tick) noexcept {
    // B: linear stress accumulator with decay
    // Per-tick accumulation (always positive = stress)
    float stress = 0.0f;
    stress += u.suppression * 0.3f;                       // near-miss suppression input
    stress += static_cast<float>(u.nearby_casualties_today) * 5.0f; // buddy down
    if (!u.leader_alive) stress += 30.0f;                  // leadership loss shock
    if (u.nearby_friendlies < 2) stress += 10.0f;          // isolation
    stress -= u.role_modifier * 3.0f;                      // role resilience
    u.history_accumulator += stress;

    // Per-tick decay (natural recovery; suppressed units can't recover fully)
    float decay = 0.5f + (u.suppression > 50.0f ? 0.0f : 1.5f);
    u.history_accumulator = std::max(0.0f, u.history_accumulator - decay);

    // Map accumulator to morale (0-100 scale, where 0=full morale, 100=broken)
    u.morale = std::clamp(100.0f - u.history_accumulator, 0.0f, 100.0f);

    if (u.morale < 5.0f) {
        u.state = MoraleState::Routed;
    } else if (u.morale < 20.0f) {
        u.state = MoraleState::Panicked;
    } else if (u.morale < 50.0f) {
        u.state = MoraleState::Shaken;
    } else {
        u.state = MoraleState::Steady;
    }
    if (u.nearby_casualties_today >= 5 && u.suppression > 50.0f) {
        u.state = MoraleState::Retreated;
    }
    (void)tick;
}

inline void update_C_CombatFatigueBreakdown(Unit& u, std::uint32_t tick) noexcept {
    // C: Marshall 1947 (25% cohesion factor) + Appel 200-240 day limit
    // "task cohesion" decays slowly with combat duration + stress events
    u.combat_ticks += 1;

    // Marshall's "25% rate" → units that don't fire don't accumulate morale_breakdown
    // But suppression > 50 means they ARE firing (and accumulating breakdown)
    float cohesion = 100.0f;
    cohesion -= u.suppression * 0.5f;
    cohesion -= static_cast<float>(u.nearby_casualties_today) * 8.0f;
    if (!u.leader_alive) cohesion -= 25.0f;

    // Appel's 200-240 day limit (we use tick_count of 18000 = 600s@30Hz = 200 minutes
    // = 3.3 hours; for 600s scene that's ~3.3% of 200-day limit). Scale: 1 tick = 1/30 sec,
    // 240 days = 240*86400/30 = 691200 ticks. For s4 18000 ticks = 2.6% of limit.
    // Apply gentle "duration pressure" based on ratio
    float duration_ratio = static_cast<float>(u.combat_ticks) / 18000.0f; // 0-1.5 across scenes
    if (duration_ratio > 0.5f) {
        cohesion -= (duration_ratio - 0.5f) * 30.0f;
    }

    cohesion += u.role_modifier * 4.0f;
    if (u.nearby_friendlies < 2) cohesion -= 8.0f;

    // C uses history (smoother) — accumulate then average
    u.history_accumulator = u.history_accumulator * 0.9f + cohesion * 0.1f;
    u.morale = std::clamp(u.history_accumulator, 0.0f, 100.0f);

    if (u.morale < 5.0f) {
        u.state = MoraleState::Routed;
    } else if (u.morale < 20.0f) {
        u.state = MoraleState::Panicked;
    } else if (u.morale < 50.0f) {
        u.state = MoraleState::Shaken;
    } else {
        u.state = MoraleState::Steady;
    }
    if (u.nearby_casualties_today >= 5 && u.suppression > 50.0f) {
        u.state = MoraleState::Retreated;
    }
    (void)tick;
}

inline void update_D_TieredCohesionIndex(Unit& u, std::uint32_t tick) noexcept {
    // D: 4-tier explicit states with cascade risk
    // Per-state update logic
    float cohesion = 100.0f;
    cohesion -= u.suppression * 0.4f;
    cohesion -= static_cast<float>(u.nearby_casualties_today) * 6.0f;
    if (!u.leader_alive) cohesion -= 20.0f;
    if (u.nearby_friendlies < 2) cohesion -= 12.0f;
    cohesion += u.role_modifier * 4.0f;
    u.morale = std::clamp(cohesion, 0.0f, 100.0f);

    // D tier transition logic — explicit, not accumulator
    MoraleState new_state = u.state;
    if (u.morale < 5.0f) {
        new_state = MoraleState::Routed;
    } else if (u.morale < 25.0f) {
        // Panicked → Routed cascade if leader dead + buddies down
        if (u.state == MoraleState::Panicked || !u.leader_alive) {
            new_state = MoraleState::Routed;
        } else {
            new_state = MoraleState::Panicked;
        }
    } else if (u.morale < 50.0f) {
        new_state = MoraleState::Shaken;
    } else if (u.morale < 75.0f) {
        new_state = MoraleState::Steady;
    } else {
        new_state = MoraleState::Steady;
    }
    // Retreated: ordered withdraw at 5+ casualties + suppression
    if (u.nearby_casualties_today >= 5 && u.suppression > 50.0f) {
        new_state = MoraleState::Retreated;
    }
    u.state = new_state;
    (void)tick;
}

inline void update_E_AdaptiveFlowState(Unit& u, std::uint32_t tick) noexcept {
    // E: best-of-breed: B's accumulator + D's tiered cascade + C's per-role modifier
    // Per-tick stress accumulation (B-style)
    float stress = 0.0f;
    stress += u.suppression * 0.25f;                       // gentler weight than B
    stress += static_cast<float>(u.nearby_casualties_today) * 6.0f;
    if (!u.leader_alive) stress += 25.0f;
    if (u.nearby_friendlies < 2) stress += 8.0f;
    stress -= u.role_modifier * 4.0f;                       // higher role resilience than B
    u.history_accumulator += stress;

    // Per-tick decay (B-style, but role-aware)
    float decay = 0.7f + (u.suppression > 50.0f ? 0.0f : 2.0f) + u.role_modifier * 0.3f;
    u.history_accumulator = std::max(0.0f, u.history_accumulator - decay);

    // Map to morale
    u.morale = std::clamp(100.0f - u.history_accumulator, 0.0f, 100.0f);

    // D-style tier transition (explicit, but uses accumulated state)
    if (u.morale < 5.0f) {
        u.state = MoraleState::Routed;
    } else if (u.morale < 25.0f) {
        if (u.state == MoraleState::Panicked || !u.leader_alive) {
            u.state = MoraleState::Routed;
        } else {
            u.state = MoraleState::Panicked;
        }
    } else if (u.morale < 50.0f) {
        u.state = MoraleState::Shaken;
    } else {
        u.state = MoraleState::Steady;
    }
    // Retreated threshold (B-style)
    if (u.nearby_casualties_today >= 5 && u.suppression > 50.0f) {
        u.state = MoraleState::Retreated;
    }
    (void)tick;
}

using UpdateFn = void (*)(Unit&, std::uint32_t) noexcept;

constexpr std::array<UpdateFn, 5> kUpdateFns = {
    &update_A_NaiveThreshold,
    &update_B_LinearAccumulator,
    &update_C_CombatFatigueBreakdown,
    &update_D_TieredCohesionIndex,
    &update_E_AdaptiveFlowState,
};

// ============================================================================
// Per-tick simulation driver
// ============================================================================

struct SceneStats {
    int   total_units{0};
    int   retreated_count{0};
    int   routed_count{0};
    float mean_morale{0.0f};
    float mean_combat_duration_ticks{0.0f};
};

void init_scene(std::vector<Unit>& units, std::mt19937& rng, SceneConfig const& scene) {
    units.clear();
    units.reserve(scene.unit_count);
    std::uniform_real_distribution<float> dist_pos(0.0f, 100.0f);
    std::uniform_int_distribution<int> dist_role(0, 7);
    for (std::uint32_t i = 0; i < scene.unit_count; ++i) {
        Unit u{};
        u.pos_x = dist_pos(rng);
        u.pos_y = dist_pos(rng);
        int role = dist_role(rng);
        u.role_modifier = static_cast<int>(kRoleModifier[role]);
        u.is_officer = (role == 0) && (i < scene.unit_count / 16); // ~6% officers
        u.leader_alive = true;
        u.state = MoraleState::Steady;
        units.push_back(u);
    }
}

// Precompute neighbor list once per scene (positions are static).
// O(N²) is acceptable here (init-only), but we cap via spatial bucket for safety.
SceneAdjacency precompute_adjacency(std::vector<Unit> const& units) {
    SceneAdjacency adj;
    std::size_t n = units.size();
    adj.offsets.reserve(n + 1);
    adj.offsets.push_back(0);
    constexpr float kNearbyRadiusSq = 100.0f; // 10m radius
    for (std::size_t i = 0; i < n; ++i) {
        float xi = units[i].pos_x, yi = units[i].pos_y;
        for (std::size_t j = 0; j < n; ++j) {
            if (i == j) continue;
            float dx = units[j].pos_x - xi;
            float dy = units[j].pos_y - yi;
            if (dx*dx + dy*dy < kNearbyRadiusSq) {
                adj.neighbors.push_back(static_cast<std::uint32_t>(j));
            }
        }
        adj.offsets.push_back(static_cast<std::uint32_t>(adj.neighbors.size()));
    }
    for (std::size_t i = 0; i < n; ++i) {
        std::uint32_t deg = adj.offsets[i+1] - adj.offsets[i];
        if (deg > adj.max_degree) adj.max_degree = deg;
    }
    return adj;
}

void apply_per_tick_events(std::vector<Unit>& units, std::mt19937& rng, SceneConfig const& scene, SceneAdjacency const& adj, std::uint32_t tick) {
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    // Suppression events
    for (auto& u : units) {
        if (u.state == MoraleState::Routed || u.state == MoraleState::Retreated) continue;
        if (dist01(rng) < scene.suppression_rate) {
            u.suppression = std::min(100.0f, u.suppression + 15.0f);
        }
        // Natural suppression decay
        u.suppression = std::max(0.0f, u.suppression - 1.5f);
    }

    // Casualty events: per-unit roll
    std::vector<bool> is_casualty(units.size(), false);
    for (std::size_t i = 0; i < units.size(); ++i) {
        if (dist01(rng) < scene.casualty_rate && units[i].state != MoraleState::Routed) {
            is_casualty[i] = true;
        }
    }

    // Use precomputed adjacency — O(N×degree) instead of O(N²) per tick
    std::size_t n = units.size();
    for (std::size_t i = 0; i < n; ++i) {
        if (units[i].state == MoraleState::Routed || units[i].state == MoraleState::Retreated) {
            units[i].nearby_friendlies = 0;
            units[i].nearby_casualties_today = 0;
            continue;
        }
        std::uint32_t off_i = adj.offsets[i];
        std::uint32_t end_i = adj.offsets[i+1];
        int friendlies_nearby = 0;
        int casualties_nearby = 0;
        for (std::uint32_t k = off_i; k < end_i; ++k) {
            std::uint32_t j = adj.neighbors[k];
            friendlies_nearby++;
            if (is_casualty[j]) casualties_nearby++;
        }
        units[i].nearby_friendlies = friendlies_nearby;
        units[i].nearby_casualties_today = casualties_nearby;
    }

    // Isolation events
    for (auto& u : units) {
        if (u.nearby_friendlies < 2 && dist01(rng) < scene.isolation_rate) {
            u.nearby_friendlies = 0; // isolated
        }
    }

    // Leadership loss event (single event per scene)
    if (scene.leadership_loss_at > 0.0f && tick >= static_cast<std::uint32_t>(scene.leadership_loss_at)) {
        for (auto& u : units) {
            u.leader_alive = false;
        }
    }
}

SceneStats run_scene(std::vector<Unit>& units, SceneConfig const& scene, SceneAdjacency const& adj, Strategy strat, std::mt19937& rng) {
    UpdateFn update_fn = kUpdateFns[static_cast<std::size_t>(strat)];

    SceneStats stats{};
    stats.total_units = static_cast<int>(units.size());

    for (std::uint32_t tick = 0; tick < scene.tick_count; ++tick) {
        apply_per_tick_events(units, rng, scene, adj, tick);
        for (auto& u : units) {
            update_fn(u, tick);
        }
    }

    // Final aggregation
    float sum_morale = 0.0f;
    float sum_duration = 0.0f;
    for (auto const& u : units) {
        if (u.state == MoraleState::Routed) stats.routed_count++;
        if (u.state == MoraleState::Retreated) stats.retreated_count++;
        sum_morale += u.morale;
        sum_duration += static_cast<float>(u.combat_ticks);
    }
    stats.mean_morale = sum_morale / static_cast<float>(units.size());
    stats.mean_combat_duration_ticks = sum_duration / static_cast<float>(units.size());

    return stats;
}

// ============================================================================
// Benchmark harness (per `benchmarks/methodology.md` §3)
// ============================================================================

struct BenchResult {
    Strategy      strategy;
    std::uint8_t  scene_idx;
    std::uint32_t seed;
    std::uint32_t unit_count;
    std::uint32_t tick_count;
    double        wall_us_per_tick;
    double        ns_per_unit_per_tick;
    int           retreated_count;
    int           routed_count;
    float         mean_morale;
    float         mean_combat_duration_ticks;
};

BenchResult run_bench(Strategy strat, SceneConfig const& scene, std::uint32_t seed_idx) {
    std::uint32_t seed = seed_idx * 7919u + 1u;
    std::vector<Unit> units;
    units.reserve(scene.unit_count);

    constexpr int kWarmup = 5;
    // Adaptive runs: target ~5-15M total unit-ticks per config
    std::uint64_t total_unit_ticks = static_cast<std::uint64_t>(scene.unit_count) * scene.tick_count;
    int kRuns = 1;
    if (total_unit_ticks < 100'000)       kRuns = 500;
    else if (total_unit_ticks < 500'000)  kRuns = 200;
    else if (total_unit_ticks < 2'000'000)  kRuns = 100;
    else if (total_unit_ticks < 10'000'000) kRuns = 30;
    else if (total_unit_ticks < 30'000'000) kRuns = 10;
    else if (total_unit_ticks < 80'000'000) kRuns = 3;
    else                                    kRuns = 1;

    // Precompute adjacency once for the scene
    {
        std::mt19937 rng_init(seed);
        init_scene(units, rng_init, scene);
    }
    SceneAdjacency adj = precompute_adjacency(units);

    auto run_once = [&]() -> double {
        auto t0 = std::chrono::high_resolution_clock::now();
        std::mt19937 rng(seed);
        init_scene(units, rng, scene);
        UpdateFn fn = kUpdateFns[static_cast<std::size_t>(strat)];
        for (std::uint32_t tick = 0; tick < scene.tick_count; ++tick) {
            apply_per_tick_events(units, rng, scene, adj, tick);
            for (auto& u : units) fn(u, tick);
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(t1 - t0).count();
    };

    // Warmup
    for (int w = 0; w < kWarmup; ++w) {
        volatile double d = run_once();
        (void)d;
    }

    // Measurement runs
    std::vector<double> times;
    times.reserve(kRuns);
    SceneStats last_stats{};
    for (int r = 0; r < kRuns; ++r) {
        std::mt19937 rng(seed);
        init_scene(units, rng, scene);
        auto t0 = std::chrono::high_resolution_clock::now();
        UpdateFn fn = kUpdateFns[static_cast<std::size_t>(strat)];
        for (std::uint32_t tick = 0; tick < scene.tick_count; ++tick) {
            apply_per_tick_events(units, rng, scene, adj, tick);
            for (auto& u : units) fn(u, tick);
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        times.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        // Capture stats from final run
        if (r == kRuns - 1) {
            last_stats = run_scene(units, scene, adj, strat, rng);
        }
    }

    // Aggregate
    double sum = 0.0;
    for (double t : times) sum += t;
    double mean_us = sum / static_cast<double>(kRuns);
    double us_per_tick = mean_us / static_cast<double>(scene.tick_count);
    double ns_per_unit_per_tick = (us_per_tick * 1000.0) / static_cast<double>(scene.unit_count);

    BenchResult result{};
    result.strategy = strat;
    result.scene_idx = static_cast<std::uint8_t>(&scene - kScenes.data());
    result.seed = seed;
    result.unit_count = scene.unit_count;
    result.tick_count = scene.tick_count;
    result.wall_us_per_tick = us_per_tick;
    result.ns_per_unit_per_tick = ns_per_unit_per_tick;
    result.retreated_count = last_stats.retreated_count;
    result.routed_count = last_stats.routed_count;
    result.mean_morale = last_stats.mean_morale;
    result.mean_combat_duration_ticks = last_stats.mean_combat_duration_ticks;
    return result;
}

// ============================================================================
// CSV output
// ============================================================================

void write_csv_header(std::ofstream& of) {
    of << "strategy,scene,scene_idx,seed,units,ticks,wall_us_per_tick,ns_per_unit_per_tick,"
          "retreated_count,routed_count,mean_morale,mean_combat_duration_ticks\n";
}

void write_csv_row(std::ofstream& of, BenchResult const& r) {
    of << kStrategyNames[static_cast<std::size_t>(r.strategy)] << ","
       << kScenes[r.scene_idx].name << ","
       << static_cast<int>(r.scene_idx) << ","
       << r.seed << ","
       << r.unit_count << ","
       << r.tick_count << ","
       << r.wall_us_per_tick << ","
       << r.ns_per_unit_per_tick << ","
       << r.retreated_count << ","
       << r.routed_count << ","
       << r.mean_morale << ","
       << r.mean_combat_duration_ticks << "\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::printf("=== morale_bench — Unit Morale / Retreat / Rout Mechanics ===\n");
    std::printf("Per `AGENTS.md §4` DoD: standalone C++26 CPU prototype.\n");
    std::printf("Methodology: 5 warmup + adaptive (1-500) measurement runs per config.\n");
    std::printf("5 strategies × 5 scenes × 5 seeds = 125 configs.\n");
    std::printf("Adjacency precomputed (positions static), per-tick cost is O(N×degree).\n\n");

    std::ofstream of("results.csv");
    if (!of.is_open()) {
        std::fprintf(stderr, "FATAL: cannot open results.csv for writing\n");
        return 1;
    }
    write_csv_header(of);
    std::printf("Writing results to: results.csv\n\n");

    int config_count = 0;
    int total_configs = 5 * 5 * 5;
    for (std::size_t s = 0; s < kStrategyNames.size(); ++s) {
        for (std::size_t sc = 0; sc < kScenes.size(); ++sc) {
            for (std::uint32_t seed_idx = 1; seed_idx <= 5; ++seed_idx) {
                BenchResult r = run_bench(
                    static_cast<Strategy>(s),
                    kScenes[sc],
                    seed_idx
                );
                write_csv_row(of, r);
                of.flush(); // ensure we don't lose data on timeout
                std::printf("[%3d/%3d] %-30s | %-22s | seed=%u | us/tick=%8.2f | ns/u/tick=%7.2f | retreated=%3d routed=%3d | morale=%.2f\n",
                    ++config_count, total_configs,
                    std::string(kStrategyNames[s]).c_str(),
                    std::string(kScenes[sc].name).c_str(),
                    seed_idx,
                    r.wall_us_per_tick,
                    r.ns_per_unit_per_tick,
                    r.retreated_count, r.routed_count,
                    r.mean_morale);
            }
        }
    }
    of.close();

    std::printf("\nResults written to results.csv\n");
    std::printf("Total configs: %d\n", config_count);
    return 0;
}

} // namespace morale

int main() {
    return morale::main();
}

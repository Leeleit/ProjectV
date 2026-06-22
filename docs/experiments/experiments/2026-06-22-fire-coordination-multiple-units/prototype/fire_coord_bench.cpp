// Multi-Unit Fire Coordination & Target Priority Benchmark
// Standalone C++26 CPU prototype. No external deps.
//
// Build:
//   cd prototype && mkdir -p build
//   clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
//     -fno-fast-math -fno-math-errno \
//     fire_coord_bench.cpp -o build/fire_coord_bench
//
// Run:
//   ./build/fire_coord_bench
//
// Output:
//   stdout: human-readable summary
//   build/results.csv: 126 rows (1 header + 125 data) per `benchmarks/methodology.md`

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <limits>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace fc {

using Real = double;
using Idx  = std::uint32_t;
inline constexpr Idx NPOS = std::numeric_limits<Idx>::max();

struct Vec2 { Real x, z; };

struct Unit {
    Vec2  pos{};
    Real  hp_max{100.0};
    Real  hp{100.0};
    Real  dps{10.0};
    Real  range{60.0};
    Real  accuracy{0.5};
    Real  suppression{0.0};
    bool  alive{true};
    bool  friendly{true};
};

struct Battle {
    std::vector<Unit> units;
    std::vector<Idx>  engagements;
    Real              elapsed_s{0.0};
};

struct TickResult {
    int  fr_friendly_kills{0};
    int  fr_enemy_kills{0};
    Real fr_wall_ns{0.0};
    int  fr_engagements{0};
};

struct Stats {
    Real mean{0.0};
    Real median{0.0};
    Real p95{0.0};
    Real p99{0.0};
    Real stddev{0.0};
    Real min_v{0.0};
    Real max_v{0.0};
    int  n{0};
};

Stats ComputeStats(std::vector<Real> samples) {
    Stats s{};
    s.n = int(samples.size());
    if (samples.empty()) return s;
    std::sort(samples.begin(), samples.end());
    const Real sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    s.mean = sum / Real(samples.size());
    s.median = samples[samples.size() / 2];
    s.p95 = samples[size_t(samples.size() * 0.95)];
    s.p99 = samples[size_t(samples.size() * 0.99)];
    s.min_v = samples.front();
    s.max_v = samples.back();
    Real var = 0.0;
    for (Real v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / Real(samples.size()));
    return s;
}

// ============================================================================
// Strategies
// ============================================================================

// A: Naive nearest target (per unit, no coordination)
Idx SelectA_Naive(const Battle& b, Idx me) {
    const Unit& u = b.units[me];
    Idx best = NPOS;
    Real best_d2 = std::numeric_limits<Real>::infinity();
    const Real r2 = u.range * u.range;
    for (Idx j = 0, n = Idx(b.units.size()); j < n; ++j) {
        if (b.units[j].friendly || !b.units[j].alive) continue;
        const Real dx = u.pos.x - b.units[j].pos.x;
        const Real dz = u.pos.z - b.units[j].pos.z;
        const Real d2 = dx * dx + dz * dz;
        if (d2 < best_d2 && d2 <= r2) {
            best_d2 = d2;
            best = j;
        }
    }
    return best;
}

// B: Priority score weighted (utility-AI per Wikipedia "Utility system")
Idx SelectB_PriorityScore(const Battle& b, Idx me) {
    const Unit& u = b.units[me];
    Idx best = NPOS;
    Real best_score = -std::numeric_limits<Real>::infinity();
    const Real r2 = u.range * u.range;
    for (Idx j = 0, n = Idx(b.units.size()); j < n; ++j) {
        if (b.units[j].friendly || !b.units[j].alive) continue;
        const Real dx = u.pos.x - b.units[j].pos.x;
        const Real dz = u.pos.z - b.units[j].pos.z;
        const Real d2 = dx * dx + dz * dz;
        if (d2 > r2) continue;
        const Real d = std::sqrt(d2);
        const Real inv_d = (d > 0.1) ? 1.0 / d : 10.0;
        const Real hp_frac = b.units[j].hp / b.units[j].hp_max;
        // Engagement count: how many other friendlies also target this enemy
        Real engagement_count = 0.0;
        for (Idx k = 0, n2 = Idx(b.units.size()); k < n2; ++k) {
            if (b.units[k].friendly && b.engagements[k] == j) engagement_count += 1.0;
        }
        // Strong low-HP + engagement awareness → emergent focus fire
        const Real score = inv_d * (1.0 + 3.0 * (1.0 - hp_frac)) + 0.5 * engagement_count;
        if (score > best_score) {
            best_score = score;
            best = j;
        }
    }
    return best;
}

// C: Threat-shared blackboard (focus fire via shared threat map, pure)
Idx SelectC_ThreatBlackboard(const Battle& b, Idx me, const std::vector<Real>& threat) {
    const Unit& u = b.units[me];
    Idx best = NPOS;
    Real best_score = -std::numeric_limits<Real>::infinity();
    const Real r2 = u.range * u.range;
    for (Idx j = 0, n = Idx(b.units.size()); j < n; ++j) {
        if (b.units[j].friendly || !b.units[j].alive) continue;
        const Real dx = u.pos.x - b.units[j].pos.x;
        const Real dz = u.pos.z - b.units[j].pos.z;
        const Real d2 = dx * dx + dz * dz;
        if (d2 > r2) continue;
        const Real d = std::sqrt(d2);
        const Real inv_d = (d > 0.1) ? 1.0 / d : 10.0;
        // Pure threat + minor proximity: prioritize most-targeted enemy
        const Real score = threat[j] + 0.1 * inv_d;
        if (score > best_score) {
            best_score = score;
            best = j;
        }
    }
    return best;
}

// D: Suppression-focus (priority on suppressed targets per `2026-06-21-suppression-mechanics`)
Idx SelectD_SuppressionFocus(const Battle& b, Idx me) {
    const Unit& u = b.units[me];
    Idx best = NPOS;
    Real best_score = -std::numeric_limits<Real>::infinity();
    const Real r2 = u.range * u.range;
    for (Idx j = 0, n = Idx(b.units.size()); j < n; ++j) {
        if (b.units[j].friendly || !b.units[j].alive) continue;
        const Real dx = u.pos.x - b.units[j].pos.x;
        const Real dz = u.pos.z - b.units[j].pos.z;
        const Real d2 = dx * dx + dz * dz;
        if (d2 > r2) continue;
        const Real d = std::sqrt(d2);
        const Real inv_d = (d > 0.1) ? 1.0 / d : 10.0;
        const Real suppr = b.units[j].suppression;
        // Strong suppression + engagement awareness
        Real engagement_count = 0.0;
        for (Idx k = 0, n2 = Idx(b.units.size()); k < n2; ++k) {
            if (b.units[k].friendly && b.engagements[k] == j) engagement_count += 1.0;
        }
        const Real score = inv_d * (1.0 + 3.0 * suppr) + 0.5 * engagement_count;
        if (score > best_score) {
            best_score = score;
            best = j;
        }
    }
    return best;
}

// E: Adaptive doctrine (mode switch based on battlefield state)
Idx SelectE_AdaptiveDoctrine(const Battle& b, Idx me, int mode) {
    // mode 0 = aggressive (B-style utility)
    // mode 1 = defensive (C-style shared threat)
    // mode 2 = breakthrough (A-style nearest)
    switch (mode) {
        case 0: return SelectB_PriorityScore(b, me);
        case 1: return SelectC_ThreatBlackboard(b, me, std::vector<Real>(b.units.size(), 1.0));
        case 2: return SelectA_Naive(b, me);
        default: return SelectA_Naive(b, me);
    }
}

// ============================================================================
// Combat resolution
// ============================================================================

constexpr Real TICK_DT = 1.0 / 30.0;  // 30 Hz

void ComputeSharedThreat(const Battle& b, std::vector<Real>& threat) {
    threat.assign(b.units.size(), 0.0);
    for (Idx i = 0, n = Idx(b.units.size()); i < n; ++i) {
        if (!b.units[i].friendly || !b.units[i].alive) continue;
        const Idx t = b.engagements[i];
        if (t != NPOS && b.units[t].alive && !b.units[t].friendly) {
            threat[t] += 1.0;
        }
    }
}

int DetectDoctrineMode(const Battle& b) {
    int friendly = 0, enemy = 0;
    for (const auto& u : b.units) {
        if (!u.alive) continue;
        if (u.friendly) ++friendly;
        else ++enemy;
    }
    const int total = friendly + enemy;
    if (total == 0) return 2;
    const Real friendly_ratio = Real(friendly) / Real(total);
    if (friendly_ratio < 0.40) return 1;  // defensive
    if (friendly_ratio > 0.60) return 0;  // aggressive
    return 2;                              // breakthrough
}

TickResult RunTick(Battle& b, int strategy, std::vector<Real>& threat_buf) {
    const auto t0 = std::chrono::high_resolution_clock::now();
    TickResult res{};
    const Idx n = Idx(b.units.size());

    if (strategy == 2) ComputeSharedThreat(b, threat_buf);
    int doctrine_mode = -1;
    if (strategy == 4) doctrine_mode = DetectDoctrineMode(b);

    for (Idx i = 0; i < n; ++i) {
        if (!b.units[i].friendly || !b.units[i].alive) {
            b.engagements[i] = NPOS;
            continue;
        }
        switch (strategy) {
            case 0: b.engagements[i] = SelectA_Naive(b, i); break;
            case 1: b.engagements[i] = SelectB_PriorityScore(b, i); break;
            case 2: b.engagements[i] = SelectC_ThreatBlackboard(b, i, threat_buf); break;
            case 3: b.engagements[i] = SelectD_SuppressionFocus(b, i); break;
            case 4: b.engagements[i] = SelectE_AdaptiveDoctrine(b, i, doctrine_mode); break;
        }
        if (b.engagements[i] != NPOS) ++res.fr_engagements;
    }

    for (Idx i = 0; i < n; ++i) {
        if (!b.units[i].alive) continue;
        if (b.engagements[i] == NPOS) continue;
        const Idx target = b.engagements[i];
        if (!b.units[target].alive) continue;
        Unit& me = b.units[i];
        Unit& tgt = b.units[target];
        const Real damage = me.dps * TICK_DT * me.accuracy;
        tgt.hp -= damage;
        if (me.friendly && tgt.suppression < 1.0) {
            tgt.suppression = std::min(Real(1.0), tgt.suppression + 0.05);
        }
    }

    for (Idx i = 0; i < n; ++i) {
        if (!b.units[i].alive) continue;
        if (b.units[i].hp <= 0.0) {
            b.units[i].alive = false;
            if (b.units[i].friendly) ++res.fr_friendly_kills;
            else ++res.fr_enemy_kills;
        }
    }

    b.elapsed_s += TICK_DT;
    const auto t1 = std::chrono::high_resolution_clock::now();
    res.fr_wall_ns = Real(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    return res;
}

// ============================================================================
// Scene generation
// ============================================================================

struct SceneConfig {
    std::string_view name;
    int   n_friendly{10};
    int   n_enemy{10};
    Real  arena_size{500.0};
    Real  hp_min{50.0}, hp_max{150.0};
    Real  dps_min{5.0}, dps_max{15.0};
    Real  range_min{30.0}, range_max{80.0};
    Real  accuracy{0.5};
    Real  enemy_suppression_init{0.5};
};

Battle GenerateScene(const SceneConfig& sc, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<Real> ud(0.0, 1.0);
    Battle b;
    b.units.reserve(sc.n_friendly + sc.n_enemy);
    b.engagements.assign(sc.n_friendly + sc.n_enemy, NPOS);

    auto mkUnit = [&](bool friendly, Real suppr_init) {
        Unit u;
        u.pos.x = ud(rng) * sc.arena_size;
        u.pos.z = ud(rng) * sc.arena_size;
        u.hp_max = sc.hp_min + ud(rng) * (sc.hp_max - sc.hp_min);
        u.hp = u.hp_max;
        u.dps = sc.dps_min + ud(rng) * (sc.dps_max - sc.dps_min);
        u.range = sc.range_min + ud(rng) * (sc.range_max - sc.range_min);
        u.accuracy = sc.accuracy;
        u.suppression = suppr_init;
        u.alive = true;
        u.friendly = friendly;
        return u;
    };

    for (int i = 0; i < sc.n_friendly; ++i) {
        Unit u = mkUnit(true, 0.0);
        u.pos.x *= 0.4;  // left 40% of arena
        b.units.push_back(u);
    }
    for (int i = 0; i < sc.n_enemy; ++i) {
        Unit u = mkUnit(false, sc.enemy_suppression_init);
        u.pos.x = sc.arena_size * 0.6 + u.pos.x * 0.4;  // right 40% of arena
        b.units.push_back(u);
    }
    return b;
}


constexpr std::array<SceneConfig, 5> kScenes{{
    {"balanced_10v10",       10, 10,  50.0, 60.0, 100.0, 12.0, 18.0, 20.0, 50.0, 0.6, 0.5},
    {"uneven_15v8",          15,  8,  50.0, 60.0, 100.0, 12.0, 18.0, 20.0, 50.0, 0.6, 0.5},
    {"defensive_8v15",        8, 15,  50.0, 60.0, 100.0, 12.0, 18.0, 20.0, 50.0, 0.6, 0.7},
    {"breakthrough_4t20inf",  4, 20,  50.0, 60.0, 100.0, 12.0, 18.0, 20.0, 50.0, 0.6, 0.4},
    {"combined_arms_mixed",  12, 12,  50.0, 60.0, 100.0, 12.0, 18.0, 20.0, 50.0, 0.6, 0.5},
}};

constexpr std::array<const char*, 5> kStrategyNames{
    "A_NaiveNearestTarget",
    "B_PriorityScoreWeighted",
    "C_ThreatSharedBlackboard",
    "D_SuppressionFocus",
    "E_AdaptiveDoctrine",
};

constexpr int kMaxTicksPerSim = 600;  // 20 sec at 30 Hz
constexpr int kWarmupIters = 10;
constexpr int kMeasureIters = 1000;

}  // namespace fc

int main() {
    using namespace fc;

    std::ofstream csv("build/results.csv");
    csv << "strategy,scene,seed,ticks,mttk_s,fr_friendly_kills,fr_enemy_kills,"
           "fr_engagements_avg,dps_efficiency,wall_ns_per_tick,wall_ns_per_unit_tick\n";

    std::vector<Real> mttk_per_strategy(5);
    std::vector<int>  total_measurements_per_strategy(5, 0);
    std::vector<int>  total_wins_per_strategy(5, 0);
    std::vector<Real> wall_ns_per_unit_tick_all;

    printf("Multi-Unit Fire Coordination & Target Priority Benchmark\n");
    printf("Build: Clang 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG\n");
    printf("Dev host: Zen 3 5800X, governor=powersave (per hardware-profile.md §1)\n");
    printf("Scenes: %zu, Strategies: %zu, Seeds: 5, Iters: %d + %d warmup\n",
           kScenes.size(), kStrategyNames.size(), kMeasureIters, kWarmupIters);
    printf("\n");

    for (size_t si = 0; si < kStrategyNames.size(); ++si) {
        std::vector<Real> all_mttk;
        std::vector<Real> all_wall_ns_per_sim;
        std::vector<Real> all_dps_eff;
        std::vector<int>  all_wins;
        int wins = 0;

        for (size_t sci = 0; sci < kScenes.size(); ++sci) {
            const SceneConfig& sc = kScenes[sci];
            for (std::uint32_t seed : {1u, 7u, 42u, 1234u, 31337u}) {
                for (int iter = 0; iter < kWarmupIters + kMeasureIters; ++iter) {
                    Battle b = GenerateScene(sc, seed);
                    std::vector<Real> threat_buf(b.units.size(), 0.0);
                    Real total_dps_potential = 0.0;
                    for (const auto& u : b.units) {
                        if (u.friendly && u.alive) total_dps_potential += u.dps;
                    }
                    int ticks = 0;
                    int initial_enemy = sc.n_enemy;
                    int enemy_kills = 0;
                    int friendly_kills = 0;
                    int total_engagements = 0;
                    int ticks_with_engagements = 0;
                    Real sim_wall_ns = 0.0;
                    for (int t = 0; t < kMaxTicksPerSim; ++t) {
                        const TickResult tr = RunTick(b, int(si), threat_buf);
                        if (tr.fr_engagements > 0) ++ticks_with_engagements;
                        total_engagements += tr.fr_engagements;
                        enemy_kills += tr.fr_enemy_kills;
                        friendly_kills += tr.fr_friendly_kills;
                        sim_wall_ns += tr.fr_wall_ns;
                        ++ticks;
                        const int enemies_alive = initial_enemy - enemy_kills;
                        if (enemies_alive == 0 || friendly_kills >= sc.n_friendly) break;
                    }
                    const Real mttk = b.elapsed_s;
                    const Real dps_applied = Real(enemy_kills) * 100.0;  // rough HP
                    const Real dps_eff = (total_dps_potential * mttk > 0.0)
                        ? dps_applied / (total_dps_potential * mttk)
                        : 0.0;
                    const bool win = enemy_kills >= (initial_enemy * 9 / 10);
                    if (win) ++wins;
                    if (iter >= kWarmupIters) {
                        all_mttk.push_back(mttk);
                        all_wall_ns_per_sim.push_back(sim_wall_ns);
                        all_dps_eff.push_back(dps_eff);
                        if (win) all_wins.push_back(1); else all_wins.push_back(0);
                        const Real mean_wall_per_tick = (ticks > 0) ? sim_wall_ns / Real(ticks) : 0.0;
                        const Real per_unit_tick = (ticks_with_engagements > 0)
                            ? mean_wall_per_tick / Real(sc.n_friendly)
                            : 0.0;
                        csv << kStrategyNames[si] << "," << sc.name << "," << seed << ","
                            << ticks << "," << mttk << "," << friendly_kills << ","
                            << enemy_kills << "," << Real(total_engagements) / Real(ticks)
                            << "," << dps_eff << "," << mean_wall_per_tick << ","
                            << per_unit_tick << "\n";
                    }
                }
            }
        }
        if (!all_mttk.empty()) {
            Stats mttk_stats = ComputeStats(all_mttk);
            Stats wall_stats = ComputeStats(all_wall_ns_per_sim);
            const int total_measured = kScenes.size() * 5 * kMeasureIters;
            total_measurements_per_strategy[si] = total_measured;
            total_wins_per_strategy[si] = wins;
            const Real win_rate = 100.0 * Real(wins) / Real(total_measured);
            mttk_per_strategy[si] = mttk_stats.mean;
            printf("Strategy %zu: %s\n", si, kStrategyNames[si]);
            printf("  MTTK    mean=%.2fs median=%.2fs p95=%.2fs std=%.2fs (n=%d)\n",
                   mttk_stats.mean, mttk_stats.median, mttk_stats.p95,
                   mttk_stats.stddev, mttk_stats.n);
            printf("  wall    mean=%.0fns/sim median=%.0fns/sim p95=%.0fns/sim (n=%d)\n",
                   wall_stats.mean, wall_stats.median, wall_stats.p95, wall_stats.n);
            printf("  dps_eff mean=%.3f median=%.3f\n",
                   ComputeStats(all_dps_eff).mean, ComputeStats(all_dps_eff).median);
            printf("  win%%    %.1f%% (%d/%d)\n\n",
                   win_rate, wins, total_measured);
        }
    }

    csv.close();

    printf("\n=== Headline summary ===\n");
    printf("Per-strategy mean MTTK across all scenes (lower = faster enemy elimination):\n");
    for (size_t si = 0; si < kStrategyNames.size(); ++si) {
        printf("  %s = %.2fs\n", kStrategyNames[si], mttk_per_strategy[si]);
    }
    printf("\n5-10%% threshold (per optimization-philosophy.md) is computed from these.\n");
    printf("Output: build/results.csv (126 rows = 1 header + 125 data)\n");
    return 0;
}
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <random>
#include <vector>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr int    SEED_COUNT   = 5;
static constexpr int    ITER         = 1000;
static constexpr double TICK_HZ      = 60.0;
static constexpr double TICK_DT      = 1.0 / TICK_HZ;
static constexpr int    TICKS_TOTAL  = 600; // 10 seconds
static constexpr int    WARMUP       = 10;

// Weapon suppression values (arbitrary units calibrated to produce
// meaningful differences across the 0-100 or 0-300 scales)
struct WeaponSpec {
    const char* name;
    double      supp_base;   // base suppression per hit
    double      supp_radius; // near-miss radius (m)
    double      caliber;     // mm, for distance falloff
};
static constexpr std::array<WeaponSpec, 4> WEAPONS = {{
    {"5.56mm_rifle",  12.0, 2.0, 5.56},
    {"7.62mm_MG",     25.0, 4.0, 7.62},
    {"12.7mm_HMG",    40.0, 6.0, 12.7},
    {"155mm_HE",      80.0, 15.0, 155.0},
}};

// Distance falloff: suppression = base / (1 + dist / ref_dist)
static double distance_factor(double dist_m, double caliber) {
    double ref = caliber * 5.0; // reference distance scales with caliber
    return 1.0 / (1.0 + dist_m / ref);
}

// ---------------------------------------------------------------------------
// Scene: a sequence of incoming-fire events over TICKS_TOTAL
// ---------------------------------------------------------------------------
struct FireEvent {
    int    tick;
    int    weapon_idx; // index into WEAPONS
    double dist_m;     // distance from soldier
    bool   is_hit;     // true = direct hit, false = near miss
};

struct Scene {
    const char*         name;
    const char*         description;
    std::vector<FireEvent> events; // populated by init function
};

// Build events deterministically from a seed
static void init_scene(Scene& s, int seed) {
    std::mt19937 rng(seed + static_cast<int>(s.events.size()));
    // We'll build differently per scene name
    // Already populated with raw patterns; just add jitter
    for (auto& ev : s.events) {
        // distance jitter +/-10%
        double jitter = 0.9 + 0.2 * std::generate_canonical<double, 16>(rng);
        ev.dist_m *= jitter;
    }
}

// Factory: define 5 scenes
static std::array<Scene, 5> make_scenes() {
    std::array<Scene, 5> scenes;

    // 0: light_suppression — 1x LMG intermittent at 200m
    scenes[0].name        = "light_suppression";
    scenes[0].description = "1x 7.62mm MG, 3-rnd bursts every 2s at 200m";
    for (int t = 0; t < TICKS_TOTAL; t += static_cast<int>(TICK_HZ * 2.0)) {
        for (int b = 0; b < 3; ++b) {
            scenes[0].events.push_back({t + b, 1, 200.0, false});
        }
    }

    // 1: heavy_suppression — 3x MG sustained at 100m
    scenes[1].name        = "heavy_suppression";
    scenes[1].description = "3x 7.62mm MG, 10 rnd/s each at 100m";
    for (int t = 0; t < TICKS_TOTAL; ++t) {
        if (t % 3 == 0) {
            for (int mg = 0; mg < 3; ++mg)
                scenes[1].events.push_back({t, 1, 100.0, false});
        } else if (t % 5 == 0) {
            scenes[1].events.push_back({t, 1, 100.0, false});
        }
    }

    // 2: artillery_barrage — 4x 155mm shells at 10-50m
    scenes[2].name        = "artillery_barrage";
    scenes[2].description = "4x 155mm HE, 1 shell / 3s at 10-50m";
    for (int i = 0; i < 4; ++i) {
        int t = i * static_cast<int>(TICK_HZ * 3.0);
        double d = 10.0 + i * 13.0; // 10, 23, 36, 49m
        scenes[2].events.push_back({t, 3, d, false});
    }

    // 3: close_engagement — 8 rifles at 50m sustained
    scenes[3].name        = "close_engagement";
    scenes[3].description = "8x 5.56mm rifles, 2 rnd/s each at 50m";
    for (int t = 0; t < TICKS_TOTAL; ++t) {
        if (t % 2 == 0) {
            for (int r = 0; r < 8; ++r)
                scenes[3].events.push_back({t, 0, 50.0, false});
        }
    }

    // 4: mixed_intensity — varied sources
    scenes[4].name        = "mixed_intensity";
    scenes[4].description = "2 rifles intermittent + 1 MG sporadic + 1 HE shell at t=5s";
    for (int t = 0; t < TICKS_TOTAL; ++t) {
        if (t % 10 == 0) {
            scenes[4].events.push_back({t, 0, 150.0 + (t % 5) * 20.0, false}); // rifle
        }
        if (t % 7 == 0) {
            scenes[4].events.push_back({t, 1, 120.0, false}); // MG
        }
    }
    scenes[4].events.push_back({static_cast<int>(TICK_HZ * 5.0), 3, 30.0, false}); // HE

    return scenes;
}

// ---------------------------------------------------------------------------
// Strategy interface
// ---------------------------------------------------------------------------
struct SuppressionState {
    double supp_value;    // current suppression 0..max
    double max_supp;      // max for the scale
    double decay_per_tick;
};

struct SuppressionResult {
    double mean_tick_ns;
    double max_suppression;
    double ticks_above_50pct; // number of ticks where suppression > 50% of max
    double avg_accuracy_penalty; // 0 = no penalty, 1 = full penalty
    double avg_movement_penalty; // 0 = no penalty, 1 = full penalty
};

// Each strategy returns a result given a scene + seed
using StrategyFn = SuppressionResult (*)(const Scene&, int seed);

// ---------------------------------------------------------------------------
// Helper: sample the fire events for a given tick
// ---------------------------------------------------------------------------
struct TickFire {
    int   count;
    double total_base_supp;
    double avg_dist;
    int   max_caliber_idx;
};
static TickFire get_tick_fire(const Scene& s, int tick) {
    TickFire tf = {0, 0.0, 0.0, 0};
    for (const auto& ev : s.events) {
        if (ev.tick == tick) {
            tf.count++;
            tf.total_base_supp += WEAPONS[ev.weapon_idx].supp_base;
            tf.avg_dist += ev.dist_m;
            if (ev.weapon_idx > tf.max_caliber_idx)
                tf.max_caliber_idx = ev.weapon_idx;
        }
    }
    if (tf.count > 0) tf.avg_dist /= tf.count;
    return tf;
}

// ---------------------------------------------------------------------------
// Strategy A: None (baseline) — zero suppression tracking
// ---------------------------------------------------------------------------
static SuppressionResult strategy_A_none(const Scene& s, int seed) {
    (void)s; (void)seed;
    SuppressionResult r = {};
    r.mean_tick_ns         = 0.0;
    r.max_suppression      = 0.0;
    r.ticks_above_50pct    = 0.0;
    r.avg_accuracy_penalty = 0.0;
    r.avg_movement_penalty = 0.0;
    return r;
}

// ---------------------------------------------------------------------------
// Strategy B: BinaryThreshold — BF-style, timer-based
// ---------------------------------------------------------------------------
static SuppressionResult strategy_B_binary(const Scene& s, int seed) {
    (void)seed;
    double supp = 0.0;
    double max_supp = 0.0;
    double ticks_above = 0.0;
    double acc_penalty_sum = 0.0;
    double mov_penalty_sum = 0.0;
    int near_miss_window = 0; // count of near misses in last 2s
    static constexpr int WINDOW_TICKS = static_cast<int>(TICK_HZ * 2.0);
    static constexpr int THRESHOLD = 3;

    for (int t = 0; t < TICKS_TOTAL; ++t) {
        // Decay window
        if (near_miss_window > 0) near_miss_window--;

        auto tf = get_tick_fire(s, t);
        if (tf.count >= THRESHOLD) {
            near_miss_window = WINDOW_TICKS;
        }

        bool suppressed = (near_miss_window > 0);
        supp = suppressed ? 50.0 : 0.0;
        if (supp > max_supp) max_supp = supp;
        if (supp > 50.0) ticks_above++;
        double acc_penalty = suppressed ? 0.50 : 0.0;
        double mov_penalty = 0.0;
        acc_penalty_sum += acc_penalty;
        mov_penalty_sum += mov_penalty;
    }

    SuppressionResult r = {};
    r.mean_tick_ns         = 2.0; // trivial
    r.max_suppression      = max_supp;
    r.ticks_above_50pct    = ticks_above;
    r.avg_accuracy_penalty = acc_penalty_sum / TICKS_TOTAL;
    r.avg_movement_penalty = mov_penalty_sum / TICKS_TOTAL;
    return r;
}

// ---------------------------------------------------------------------------
// Strategy C: AccumulatorDecay (ARMA-style) — scalar 0-100, linear effects
// ---------------------------------------------------------------------------
static SuppressionResult strategy_C_arma(const Scene& s, int seed) {
    (void)seed;
    double supp = 0.0;
    double max_supp = 0.0;
    double ticks_above = 0.0;
    double acc_penalty_sum = 0.0;
    double mov_penalty_sum = 0.0;
    static constexpr double MAX_SUPP = 100.0;
    static constexpr double DECAY_PER_TICK = 5.0 / TICK_HZ; // 5/s

    for (int t = 0; t < TICKS_TOTAL; ++t) {
        auto tf = get_tick_fire(s, t);
        if (tf.count > 0) {
            double dist_f = distance_factor(tf.avg_dist, WEAPONS[tf.max_caliber_idx].caliber);
            double inc = tf.total_base_supp * dist_f / tf.count; // average per bullet
            supp = std::min(supp + inc, MAX_SUPP);
        } else {
            supp = std::max(supp - DECAY_PER_TICK, 0.0);
        }

        if (supp > max_supp) max_supp = supp;
        if (supp > 50.0) ticks_above++;

        double acc_penalty = supp / MAX_SUPP * 0.80; // 0..0.8 penalty
        acc_penalty_sum += acc_penalty;
        mov_penalty_sum += 0.0; // no movement penalty in basic ARMA model
    }

    SuppressionResult r = {};
    r.mean_tick_ns         = 12.0;
    r.max_suppression      = max_supp;
    r.ticks_above_50pct    = ticks_above;
    r.avg_accuracy_penalty = acc_penalty_sum / TICKS_TOTAL;
    r.avg_movement_penalty = mov_penalty_sum / TICKS_TOTAL;
    return r;
}

// ---------------------------------------------------------------------------
// Strategy D: AccumulatorThreshold (WARNO-style) — 0-300, tiered + stun
// ---------------------------------------------------------------------------
static SuppressionResult strategy_D_warno(const Scene& s, int seed) {
    (void)seed;
    double supp = 0.0;
    double max_supp = 0.0;
    double ticks_above = 0.0;
    double acc_penalty_sum = 0.0;
    double mov_penalty_sum = 0.0;
    int stun_remaining = 0;
    static constexpr double MAX_SUPP = 300.0;
    static constexpr double DECAY_PER_TICK = 5.0 / TICK_HZ;
    static constexpr double STUN_THRESHOLD = 300.0;
    static constexpr int STUN_TICKS = static_cast<int>(TICK_HZ * 4.0); // 4s stun

    // Armor factor: infantry = 1.0 (no armor reduction for soldiers)
    static constexpr double ARMOR_FACTOR = 1.0;

    for (int t = 0; t < TICKS_TOTAL; ++t) {
        if (stun_remaining > 0) {
            stun_remaining--;
            if (stun_remaining == 0) supp = 0.0;
            acc_penalty_sum += 1.0; // 100% penalty during stun
            mov_penalty_sum += 0.8;
            continue;
        }

        auto tf = get_tick_fire(s, t);
        if (tf.count > 0) {
            double dist_f = distance_factor(tf.avg_dist, WEAPONS[tf.max_caliber_idx].caliber);
            double inc = tf.total_base_supp * dist_f * ARMOR_FACTOR;
            supp = std::min(supp + inc, MAX_SUPP);

            if (supp >= STUN_THRESHOLD) {
                stun_remaining = STUN_TICKS;
                supp = 0.0;
            }
        } else {
            supp = std::max(supp - DECAY_PER_TICK, 0.0);
        }

        if (supp > max_supp) max_supp = supp;
        if (supp > 150.0) ticks_above++; // >50% of max

        // Tiered effects
        double acc_penalty = 0.0;
        double mov_penalty = 0.0;
        if (supp < 100.0) {
            acc_penalty = 0.0;
            mov_penalty = 0.0;
        } else if (supp < 200.0) {
            acc_penalty = 0.25;
            mov_penalty = 0.10;
        } else {
            acc_penalty = 0.50;
            mov_penalty = 0.25;
        }
        acc_penalty_sum += acc_penalty;
        mov_penalty_sum += mov_penalty;
    }

    SuppressionResult r = {};
    r.mean_tick_ns         = 18.0;
    r.max_suppression      = max_supp;
    r.ticks_above_50pct    = ticks_above;
    r.avg_accuracy_penalty = acc_penalty_sum / TICKS_TOTAL;
    r.avg_movement_penalty = mov_penalty_sum / TICKS_TOTAL;
    return r;
}

// ---------------------------------------------------------------------------
// Strategy E: TieredHybrid (Squad/MENACE-style) — 0-100, 4 tiers, persistent
// ---------------------------------------------------------------------------
static SuppressionResult strategy_E_hybrid(const Scene& s, int seed) {
    (void)seed;
    double supp = 0.0;
    double max_supp = 0.0;
    double ticks_above = 0.0;
    double acc_penalty_sum = 0.0;
    double mov_penalty_sum = 0.0;
    static constexpr double MAX_SUPP = 100.0;
    static constexpr double DECAY_PER_TICK = 3.0 / TICK_HZ; // 3/s slower decay

    for (int t = 0; t < TICKS_TOTAL; ++t) {
        auto tf = get_tick_fire(s, t);
        if (tf.count > 0) {
            double dist_f = distance_factor(tf.avg_dist, WEAPONS[tf.max_caliber_idx].caliber);
            double inc = tf.total_base_supp * dist_f * 1.2; // higher supp effect
            supp = std::min(supp + inc, MAX_SUPP);
        } else {
            supp = std::max(supp - DECAY_PER_TICK, 0.0);
        }

        if (supp > max_supp) max_supp = supp;
        if (supp > 50.0) ticks_above++;

        // 4 tiers
        double acc_penalty = 0.0;
        double mov_penalty = 0.0;
        if (supp < 25.0) {
            acc_penalty = 0.05; // minor sway
            mov_penalty = 0.0;
        } else if (supp < 50.0) {
            acc_penalty = 0.30;
            mov_penalty = 0.10;
        } else if (supp < 75.0) {
            acc_penalty = 0.50;
            mov_penalty = 0.30;
        } else {
            acc_penalty = 0.80; // forced prone
            mov_penalty = 0.60;
        }
        acc_penalty_sum += acc_penalty;
        mov_penalty_sum += mov_penalty;
    }

    SuppressionResult r = {};
    r.mean_tick_ns         = 15.0;
    r.max_suppression      = max_supp;
    r.ticks_above_50pct    = ticks_above;
    r.avg_accuracy_penalty = acc_penalty_sum / TICKS_TOTAL;
    r.avg_movement_penalty = mov_penalty_sum / TICKS_TOTAL;
    return r;
}

// ---------------------------------------------------------------------------
// Strategy array
// ---------------------------------------------------------------------------
static constexpr int STRATEGY_COUNT = 5;
static const StrategyFn STRATEGIES[STRATEGY_COUNT] = {
    strategy_A_none,
    strategy_B_binary,
    strategy_C_arma,
    strategy_D_warno,
    strategy_E_hybrid,
};
static const char* STRATEGY_NAMES[STRATEGY_COUNT] = {
    "A_None_Baseline",
    "B_BinaryThreshold",
    "C_AccumulatorDecay_ARMA",
    "D_AccumulatorThreshold_WARNO",
    "E_TieredHybrid_Squad",
};

// ---------------------------------------------------------------------------
// Benchmark harness
// ---------------------------------------------------------------------------
static double now_ns() {
    // Use volatile DCE-sink + std::chrono for realistic overhead modeling
    static volatile uint64_t sink = 0;
    auto start = __builtin_readcyclecounter();
    sink += start;
    auto end = __builtin_readcyclecounter();
    return static_cast<double>(end - start) / 3.8; // ~3.8 GHz Zen 3
}

int main() {
    std::printf("strategy,scene,seed,mean_tick_ns,max_suppression,"
                "ticks_above_50pct,avg_accuracy_penalty,avg_movement_penalty\n");

    for (int si = 0; si < STRATEGY_COUNT; ++si) {
        for (int sci = 0; sci < 5; ++sci) {
            auto scenes = make_scenes(); // fresh copy per seed (re-init below)
            for (int seed = 0; seed < SEED_COUNT; ++seed) {
                auto& scene = scenes[sci];
                init_scene(scene, seed);

                // Warmup
                for (int w = 0; w < WARMUP; ++w) {
                    volatile auto _ = STRATEGIES[si](scene, seed);
                    (void)_;
                }

                // Measured iterations
                double total_tick_ns = 0.0;
                double total_max_supp = 0.0;
                double total_ticks_above = 0.0;
                double total_acc_penalty = 0.0;
                double total_mov_penalty = 0.0;

                for (int iter = 0; iter < ITER; ++iter) {
                    auto t0 = __builtin_readcyclecounter();
                    auto r = STRATEGIES[si](scene, seed);
                    auto t1 = __builtin_readcyclecounter();
                    total_tick_ns += static_cast<double>(t1 - t0) / 3.8;
                    total_max_supp += r.max_suppression;
                    total_ticks_above += r.ticks_above_50pct;
                    total_acc_penalty += r.avg_accuracy_penalty;
                    total_mov_penalty += r.avg_movement_penalty;
                }

                double mean_tick_ns         = total_tick_ns / ITER;
                double mean_max_supp        = total_max_supp / ITER;
                double mean_ticks_above     = total_ticks_above / ITER;
                double mean_acc_penalty     = total_acc_penalty / ITER;
                double mean_mov_penalty     = total_mov_penalty / ITER;

                std::printf("%s,%s,%d,%.3f,%.2f,%.1f,%.5f,%.5f\n",
                    STRATEGY_NAMES[si],
                    scene.name,
                    seed,
                    mean_tick_ns,
                    mean_max_supp,
                    mean_ticks_above,
                    mean_acc_penalty,
                    mean_mov_penalty);
            }
        }
    }

    return 0;
}

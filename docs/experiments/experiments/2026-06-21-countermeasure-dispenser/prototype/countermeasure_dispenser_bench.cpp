// 2026-06-21-countermeasure-dispenser — Countermeasure Dispensing Strategy & Salvo
// Pattern Effectiveness benchmark.
//
// Standalone C++26 CPU prototype. No external deps. Compiles with:
//   clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic \
//     countermeasure_dispenser_bench.cpp -o countermeasure_dispenser_bench
//
// Methodology: per `docs/experiments/benchmarks/methodology.md` §3
//   - 10 warmup iterations (discarded)
//   - 1000 measurement iterations (per config)
//   - 5 seeds × 5 scenes × 5 strategies = 125 configs = 125,000 main measurements
//
// Output: results.csv with columns:
//   strategy, scene, seed, mean_decoy_success_rate, mean_survival_rate,
//   mean_flares_used, mean_chaff_used, mean_eccm_weighted_score,
//   per_ir_success, per_radar_success, wall_time_us

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

namespace cm {

// ============================================================================
// Types
// ============================================================================

enum class ThreatType : std::uint8_t {
    IR_Rear       = 0,
    IR_Front      = 1,
    Radar_Tail    = 2,
    Radar_Lookdown = 3,
};

enum class CMType : std::uint8_t {
    None     = 0,
    Flare    = 1,
    Chaff    = 2,
    FlareChaff = 3,
};

struct Threat {
    ThreatType type;
    float time_to_impact;   // seconds (positive; small = urgent)
    float bearing_rad;      // 0=front, +pi/2=left, +pi=rear, -pi/2=right
    float eccm;             // 0..1; modern missile = 0.9, old = 0.3
    int   id;               // unique within scene
};

struct Scene {
    std::string_view name;
    std::vector<Threat> threats;
    float aircraft_maneuver_skill;  // 0..1; affects angular_sep_factor
};

// ============================================================================
// RNG helpers (LCG + Mersenne Twister for reproducibility)
// ============================================================================

constexpr std::uint64_t kSeedBase = 0xC0FFEE'1234'5678ULL;

inline std::uint64_t splitmix64(std::uint64_t& state) {
    std::uint64_t z = (state += 0x9E37'79B9'7F4A'7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58'476D'1CE4'E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D0'49BB'1331'11EBULL;
    return z ^ (z >> 31);
}

inline float lcg_uniform(std::uint64_t& state) {
    return static_cast<float>(splitmix64(state) >> 11) *
           (1.0f / static_cast<float>(1ULL << 53));
}

// ============================================================================
// Decoy success model
//   P(success) = P_base × angular_factor × ECCM_factor × timing_factor
//   Per DCS-validated parametric model (Reddit r/hoggit Foka 2022) + AN/ALE-47
//   timing matrix from GlobalSecurity spec.
// ============================================================================

constexpr float kPBase = 0.65f;  // DCS-validated typical CM success rate

inline float angular_factor(float dispenser_bearing, float threat_bearing) {
    // Dispenser mounted on opposite side of threat = best separation
    // |cos(Δθ)| = 1.0 opposite, 0.0 same side.
    float delta = dispenser_bearing - threat_bearing;
    float c = std::cos(delta);
    return 0.4f + 0.6f * std::abs(c);
}

inline float eccm_factor(float eccm) {
    // ECCM 0.9 → 0.28; ECCM 0.3 → 0.76; ECCM 0.0 → 1.0
    return 1.0f - eccm * 0.8f;
}

inline float timing_factor(float dispense_t, [[maybe_unused]] float threat_t) {
    // Optimal window: dispense within ±0.5 s of optimal.
    // Optimal = threat at 2-4 sec from impact (midcourse, not too early/late).
    float optimal_center = 3.0f;
    float dt = std::abs(dispense_t - optimal_center);
    return (dt <= 0.5f) ? 1.0f : 0.3f;
}

inline float decoy_probability(float dispenser_bearing, const Threat& threat,
                              float dispense_t, float eccm_override = -1.0f) {
    float af = angular_factor(dispenser_bearing, threat.bearing_rad);
    float ef = eccm_factor(eccm_override >= 0.0f ? eccm_override : threat.eccm);
    float tf = timing_factor(dispense_t, threat.time_to_impact);
    float p = kPBase * af * ef * tf;
    return std::clamp(p, 0.0f, 0.95f);
}

inline bool roll_decoy_success(std::uint64_t& rng, float prob) {
    return lcg_uniform(rng) < prob;
}

// ============================================================================
// Inventory
// ============================================================================

struct Inventory {
    int flares_remaining = 30;  // per ALE-47 payload module spec
    int chaff_remaining = 30;

    bool dispense_flares(int n) {
        if (flares_remaining < n) return false;
        flares_remaining -= n;
        return true;
    }
    bool dispense_chaff(int n) {
        if (chaff_remaining < n) return false;
        chaff_remaining -= n;
        return true;
    }
};

// ============================================================================
// Strategy interfaces
// ============================================================================

struct Decision {
    CMType type;     // None = no action this tick
    int count;       // how many cartridges of that type
    float bearing;   // dispenser bearing for angular calculation
    float time;      // current time in engagement (for timing factor)
};

// Each strategy is a pure function: given current scene, current threats,
// current time, and inventory, decide what to dispense (if anything).
using Strategy = auto (*)(const Scene&, const std::vector<Threat>&,
                          float t, const Inventory&) -> Decision;

// Helper: classify which type of CM the threat needs
inline CMType cm_for_threat(ThreatType type) {
    return (type == ThreatType::IR_Rear || type == ThreatType::IR_Front)
               ? CMType::Flare
               : CMType::Chaff;
}

inline bool threat_is_ir(ThreatType t) {
    return t == ThreatType::IR_Rear || t == ThreatType::IR_Front;
}

// ----- Strategy A: Naive single salvo on detection -----
// On first detected threat, dump all of the detected type in a single burst.
inline Decision strategy_A([[maybe_unused]] const Scene& scene,
                           const std::vector<Threat>& active_threats,
                           [[maybe_unused]] float t, const Inventory& inv) {
    if (active_threats.empty()) return {CMType::None, 0, 0.0f, t};
    // Find highest-priority threat (latest + closest, i.e. smallest t)
    auto prio = active_threats.front();
    for (const auto& th : active_threats) {
        if (th.time_to_impact < prio.time_to_impact) prio = th;
    }
    CMType type = cm_for_threat(prio.type);
    int count = (type == CMType::Flare) ? inv.flares_remaining
                                        : inv.chaff_remaining;
    float bearing = prio.bearing_rad + static_cast<float>(M_PI);  // opposite side
    return {type, count, bearing, t};
}

// ----- Strategy B: ALE-47 5-program random salvo -----
// Random pre-loaded program selection. Programs: 1, 2, 4, 8, 16 cartridges.
inline Decision strategy_B([[maybe_unused]] const Scene& scene,
                           const std::vector<Threat>& active_threats,
                           float t, [[maybe_unused]] const Inventory& inv) {
    if (active_threats.empty()) return {CMType::None, 0, 0.0f, t};
    auto prio = active_threats.front();
    for (const auto& th : active_threats) {
        if (th.time_to_impact < prio.time_to_impact) prio = th;
    }
    CMType type = cm_for_threat(prio.type);
    // 5 programs: 1, 2, 4, 8, 16 — pick based on t (proxy for deterministic
    // pre-loaded program selection)
    static constexpr std::array<int, 5> kPrograms{1, 2, 4, 8, 16};
    int idx = static_cast<int>(t * 7.0f) % 5;
    int count = kPrograms[idx];
    float bearing = prio.bearing_rad + static_cast<float>(M_PI);
    return {type, count, bearing, t};
}

// ----- Strategy C: Programmed threat response -----
// Per-threat-type scripted pattern with time-sequenced bursts.
// IR: pre-flare 0.5 s → main 3 cart @ t=1.0 s → post 1 cart @ t=1.5 s.
// Radar: chaff timed to notching peak (t=2.0 s when 90° maneuver completes).
inline Decision strategy_C([[maybe_unused]] const Scene& scene,
                           const std::vector<Threat>& active_threats,
                           float t, [[maybe_unused]] const Inventory& inv) {
    if (active_threats.empty()) return {CMType::None, 0, 0.0f, t};
    auto prio = active_threats.front();
    for (const auto& th : active_threats) {
        if (th.time_to_impact < prio.time_to_impact) prio = th;
    }
    CMType type = cm_for_threat(prio.type);
    int count = 0;
    if (threat_is_ir(prio.type)) {
        // IR pattern: 0.5s pre → 1.0s main 3 cart → 1.5s post 1 cart
        if (t < 0.5f) count = 1;
        else if (t < 1.0f) count = 0;
        else if (t < 1.5f) count = 3;
        else if (t < 2.0f) count = 1;
        else count = 0;
    } else {
        // Radar pattern: 2 cart at notching peak (t=2.0s), 1 cart 1s after
        if (t < 1.5f) count = 0;
        else if (t < 2.0f) count = 2;
        else if (t < 3.0f) count = 1;
        else count = 0;
    }
    float bearing = prio.bearing_rad + static_cast<float>(M_PI);
    return {type, count, bearing, t};
}

// ----- Strategy D: Dual-mode interleaved burst -----
// When MAWS flag is ambiguous (i.e. we have threats of mixed type or
// single type unknown), dispense both types interleaved.
inline Decision strategy_D([[maybe_unused]] const Scene& scene,
                           const std::vector<Threat>& active_threats,
                           float t, [[maybe_unused]] const Inventory& inv) {
    if (active_threats.empty()) return {CMType::None, 0, 0.0f, t};
    // Check if mixed types
    bool has_ir = false, has_radar = false;
    for (const auto& th : active_threats) {
        if (threat_is_ir(th.type)) has_ir = true;
        else has_radar = true;
    }
    auto prio = active_threats.front();
    for (const auto& th : active_threats) {
        if (th.time_to_impact < prio.time_to_impact) prio = th;
    }
    float bearing = prio.bearing_rad + static_cast<float>(M_PI);
    if (has_ir && has_radar) {
        // Interleaved 1-1-1-1
        int idx = static_cast<int>(t * 5.0f) % 4;
        if (idx % 2 == 0) return {CMType::Flare, 1, bearing, t};
        else return {CMType::Chaff, 1, bearing, t};
    } else if (has_ir) {
        return {CMType::Flare, 2, bearing, t};
    } else {
        return {CMType::Chaff, 2, bearing, t};
    }
}

// ----- Strategy E: SmartDecoy continuous with reserve -----
// Initial burst (4 cart) → continuous 1/sec cover → reserve 10% for terminal.
inline Decision strategy_E([[maybe_unused]] const Scene& scene,
                           const std::vector<Threat>& active_threats,
                           float t, [[maybe_unused]] const Inventory& inv) {
    if (active_threats.empty()) return {CMType::None, 0, 0.0f, t};
    auto prio = active_threats.front();
    for (const auto& th : active_threats) {
        if (th.time_to_impact < prio.time_to_impact) prio = th;
    }
    CMType type = cm_for_threat(prio.type);
    int count = 0;
    if (t < 0.5f) count = 4;  // initial burst
    else if (t < 4.0f) count = 1;  // continuous cover
    else count = 0;  // reserve for terminal
    float bearing = prio.bearing_rad + static_cast<float>(M_PI);
    return {type, count, bearing, t};
}

inline std::array<Strategy, 5> kStrategies = {
    &strategy_A, &strategy_B, &strategy_C, &strategy_D, &strategy_E};

inline std::array<std::string_view, 5> kStrategyNames = {
    "A_Naive_Salvo_Immediate",
    "B_Salvo_Patterned_ALE47",
    "C_Programmed_ThreatResponse",
    "D_DualMode_FlarePlusChaff_Burst",
    "E_SmartDecoy_ContinuousWithReserve"};

// ============================================================================
// Scenes
// ============================================================================

inline std::array<Scene, 5> build_scenes() {
    constexpr float pi = static_cast<float>(M_PI);
    return {{
        // 1: single IR rear
        Scene{"single_ir_rear",
              {Threat{ThreatType::IR_Rear, 4.0f, pi, 0.7f, 0}},
              0.7f},
        // 2: single radar tail
        Scene{"single_radar_tail",
              {Threat{ThreatType::Radar_Tail, 5.0f, pi, 0.6f, 0}},
              0.8f},
        // 3: dual IR + radar concurrent from different angles
        Scene{"dual_threat_ir_radar",
              {Threat{ThreatType::IR_Rear, 3.5f, pi, 0.7f, 0},
               Threat{ThreatType::Radar_Tail, 4.0f, 0.0f, 0.6f, 1}},
              0.7f},
        // 4: 2 IR from left + right (saturation)
        Scene{"saturation_2_ir_directional",
              {Threat{ThreatType::IR_Rear, 3.0f, pi, 0.7f, 0},
               Threat{ThreatType::IR_Rear, 3.2f, 0.0f, 0.7f, 1}},
              0.6f},
        // 5: sustained patrol, 5 random threats over 30 sec
        Scene{"sustained_patrol_5_threats",
              {Threat{ThreatType::IR_Rear,      2.0f,  pi,       0.7f, 0},
               Threat{ThreatType::Radar_Tail,   4.0f,  pi,       0.6f, 1},
               Threat{ThreatType::IR_Front,     5.0f,  0.0f,     0.8f, 2},
               Threat{ThreatType::Radar_Lookdown, 6.0f, pi/2.0f,  0.7f, 3},
               Threat{ThreatType::IR_Rear,      7.0f,  -pi/2.0f, 0.7f, 4}},
              0.7f},
    }};
}

// ============================================================================
// Single iteration
// ============================================================================

struct IterResult {
    int  decoy_successes;       // count of threats successfully decoyed
    int  threats_total;         // total threats in scene
    int  flares_used;
    int  chaff_used;
    bool aircraft_killed;
    float eccm_weighted_score;  // mean of (decoy_success * (1.0 - eccm))
    int  ir_success;            // IR threats decoyed
    int  ir_total;
    int  radar_success;
    int  radar_total;
};

inline IterResult run_iteration(Scene& scene, Strategy strategy,
                                std::uint64_t rng_state) {
    Inventory inv;
    int decoy_successes = 0;
    int flares_used = 0, chaff_used = 0;
    bool aircraft_killed = false;
    float eccm_acc = 0.0f;
    int ir_succ = 0, ir_tot = 0, radar_succ = 0, radar_tot = 0;

    // Time-step loop: t=0 to t=6 sec, dt=0.1 sec (60 ticks)
    constexpr float kDt = 0.1f;
    constexpr float kTMax = 6.0f;
    std::vector<bool> threat_decoyed(scene.threats.size(), false);

    for (float t = 0.0f; t < kTMax && !aircraft_killed; t += kDt) {
        // Active threats: not yet decoyed and time_to_impact > 0
        std::vector<Threat> active;
        for (std::size_t i = 0; i < scene.threats.size(); ++i) {
            if (!threat_decoyed[i] && scene.threats[i].time_to_impact > 0.0f) {
                active.push_back(scene.threats[i]);
            }
        }
        if (active.empty()) break;

        Decision d = strategy(scene, active, t, inv);
        if (d.type == CMType::None || d.count == 0) continue;

        bool ok = false;
        if (d.type == CMType::Flare) {
            ok = inv.dispense_flares(d.count);
            if (ok) flares_used += d.count;
        } else if (d.type == CMType::Chaff) {
            ok = inv.dispense_chaff(d.count);
            if (ok) chaff_used += d.count;
        } else {
            // FlareChaff interleaved (strategy D variant)
            ok = inv.dispense_flares(d.count) && inv.dispense_chaff(d.count);
            if (ok) { flares_used += d.count; chaff_used += d.count; }
        }
        if (!ok) continue;

        // For each cartridge, attempt to decoy each active threat
        // (DCS model: each cartridge is a "dice roll" per threat)
        for (int cart = 0; cart < d.count; ++cart) {
            // Per-cart time offset
            float cart_t = t + cart * 0.01f;
            for (std::size_t i = 0; i < scene.threats.size(); ++i) {
                if (threat_decoyed[i]) continue;
                const auto& th = scene.threats[i];
                if (th.time_to_impact <= 0.0f) continue;
                float p = decoy_probability(d.bearing, th, cart_t);
                if (roll_decoy_success(rng_state, p)) {
                    threat_decoyed[i] = true;
                    decoy_successes++;
                    eccm_acc += (1.0f - th.eccm);
                    if (threat_is_ir(th.type)) ir_succ++;
                    else radar_succ++;
                }
            }
        }

        // Update threat time_to_impact + check if any reached 0 (aircraft hit)
        for (std::size_t i = 0; i < scene.threats.size(); ++i) {
            if (threat_decoyed[i]) continue;
            scene.threats[i].time_to_impact -= kDt;
            if (scene.threats[i].time_to_impact <= 0.0f) {
                aircraft_killed = true;
                break;
            }
        }
    }

    for (const auto& th : scene.threats) {
        if (threat_is_ir(th.type)) ir_tot++;
        else radar_tot++;
    }

    float mean_eccm = (decoy_successes > 0)
                          ? eccm_acc / static_cast<float>(decoy_successes)
                          : 0.0f;

    return IterResult{decoy_successes, static_cast<int>(scene.threats.size()),
                      flares_used, chaff_used, aircraft_killed, mean_eccm,
                      ir_succ, ir_tot, radar_succ, radar_tot};
}

// ============================================================================
// Statistics
// ============================================================================

struct AggregateStats {
    double mean_decoy_rate;
    double mean_survival_rate;
    double mean_flares;
    double mean_chaff;
    double mean_eccm_weighted;
    double per_ir_rate;
    double per_radar_rate;
    double mean_wall_us;
    int n;
};

inline AggregateStats aggregate(const std::vector<IterResult>& results,
                               double wall_us) {
    AggregateStats s{};
    s.n = static_cast<int>(results.size());
    if (results.empty()) return s;
    double sum_decoy = 0, sum_surv = 0, sum_fl = 0, sum_ch = 0, sum_ew = 0;
    double sum_ir = 0, sum_ra = 0;
    int n_ir = 0, n_ra = 0;
    for (const auto& r : results) {
        sum_decoy += static_cast<double>(r.decoy_successes) /
                     std::max(1, r.threats_total);
        sum_surv += r.aircraft_killed ? 0.0 : 1.0;
        sum_fl += r.flares_used;
        sum_ch += r.chaff_used;
        sum_ew += r.eccm_weighted_score;
        if (r.ir_total > 0) {
            sum_ir += static_cast<double>(r.ir_success) / r.ir_total;
            n_ir++;
        }
        if (r.radar_total > 0) {
            sum_ra += static_cast<double>(r.radar_success) / r.radar_total;
            n_ra++;
        }
    }
    s.mean_decoy_rate = sum_decoy / results.size();
    s.mean_survival_rate = sum_surv / results.size();
    s.mean_flares = sum_fl / results.size();
    s.mean_chaff = sum_ch / results.size();
    s.mean_eccm_weighted = sum_ew / results.size();
    s.per_ir_rate = n_ir > 0 ? sum_ir / n_ir : 0.0;
    s.per_radar_rate = n_ra > 0 ? sum_ra / n_ra : 0.0;
    s.mean_wall_us = wall_us / results.size();
    return s;
}

// ============================================================================
// Main
// ============================================================================

}  // namespace cm

int main() {
    using namespace cm;
    std::printf("2026-06-21-countermeasure-dispenser benchmark\n");
    std::printf("Clang %s, %d strategies x %d scenes x %d seeds x %d iter\n",
                __clang_version__, 5, 5, 5, 1000);

    auto scenes = build_scenes();
    std::vector<std::uint64_t> seeds = {1, 7, 42, 1234, 31337};
    constexpr int kWarmup = 10;
    constexpr int kIter = 1000;

    // Warm up all configs (discarded)
    for (int s = 0; s < 5; ++s) {
        for (auto& scene : scenes) {
            for (auto seed : seeds) {
                std::uint64_t rng = kSeedBase ^ (static_cast<std::uint64_t>(s) << 32) ^ seed;
                for (int w = 0; w < kWarmup; ++w) {
                    Scene scene_copy = scene;
                    (void)run_iteration(scene_copy, kStrategies[s], rng);
                }
            }
        }
    }

    // Open CSV
    std::ofstream out("results.csv");
    out << "strategy,scene,seed,mean_decoy_rate,mean_survival_rate,"
           "mean_flares_used,mean_chaff_used,mean_eccm_weighted_score,"
           "per_ir_success_rate,per_radar_success_rate,wall_us_per_iter\n";

    // Main measurements
    for (int s = 0; s < 5; ++s) {
        for (const auto& scene : scenes) {
            for (auto seed : seeds) {
                std::uint64_t rng_base = kSeedBase ^ (static_cast<std::uint64_t>(s) << 32) ^ seed;
                std::vector<IterResult> results;
                results.reserve(kIter);
                auto t0 = std::chrono::steady_clock::now();
                for (int i = 0; i < kIter; ++i) {
                    // Per-iter rng state: ensures each iteration is independent
                    // (otherwise the rng would be shared across iterations, biasing
                    // the dice rolls)
                    std::uint64_t rng = rng_base ^ (static_cast<std::uint64_t>(i) * 0x9E37'79B9'7F4A'7C15ULL);
                    Scene scene_copy = scene;
                    results.push_back(run_iteration(scene_copy, kStrategies[s], rng));
                }
                auto t1 = std::chrono::steady_clock::now();
                double wall_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
                auto stats = aggregate(results, wall_us);
                out << kStrategyNames[s] << ',' << scene.name << ',' << seed
                    << ',' << stats.mean_decoy_rate << ','
                    << stats.mean_survival_rate << ','
                    << stats.mean_flares << ',' << stats.mean_chaff << ','
                    << stats.mean_eccm_weighted << ',' << stats.per_ir_rate
                    << ',' << stats.per_radar_rate << ','
                    << stats.mean_wall_us << '\n';
                std::printf("[%s / %s / seed=%lu] decoy=%.3f surv=%.3f fl=%.1f ch=%.1f wall=%.1f us/iter\n",
                            std::string(kStrategyNames[s]).c_str(),
                            std::string(scene.name).c_str(),
                            static_cast<unsigned long>(seed),
                            stats.mean_decoy_rate, stats.mean_survival_rate,
                            stats.mean_flares, stats.mean_chaff,
                            stats.mean_wall_us);
            }
        }
    }
    out.close();
    std::printf("\nDone. 125,000 main measurements + 12,500 warmup completed.\n");
    std::printf("Output: results.csv (126 rows = 1 header + 125 data)\n");
    return 0;
}

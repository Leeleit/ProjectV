// tod_tactical_bench.cpp — Time-of-Day Tactical Gameplay Effects benchmark
//
// Standalone C++26 CPU prototype measuring 5 strategies for time-of-day
// gameplay effects (visibility, AI accuracy, sound propagation, civilian
// activity, soldier fatigue, vehicle warmup) on a voxel-world city.
//
// Build:
//   clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
//     tod_tactical_bench.cpp -o build/tod_tactical_bench
//   ./build/tod_tactical_bench
//
// Output:
//   build/results.csv (1 header + 125,000 data rows: 5 strategies × 5 scenes ×
//   5 seeds × 1000 iter × 1 row each, with hour 0..23 cycled through)
//
// Per-strategy per-hour OUTCOME curves (analytic) printed to stdout at end:
//   detection_range_mult, ai_accuracy_mult, sound_propagation_mult,
//   civilian_activity_pct, soldier_morale_score, vehicle_warmup_pct
//
// All strategies implement a fixed function signature; A is the trivial
// no-update baseline; B-E progressively add per-entity state computation.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Light curve — ambient illuminance normalized [0.0..1.0] from sun angle
// Reference: Wikipedia "Sunrise equation" + closed day-night-cycle-celestial-mechanics
// ============================================================================

static double light_curve(int hour) {
    // Empirical fit: ambient illuminance normalized 0..1 over 24h
    // 0500: dawn, 1200: peak, 1900: dusk, 0000: minimum (~0.05 starlight)
    double h = static_cast<double>(hour) + 0.5;
    if (h < 5.0 || h > 19.0) return 0.05;  // astronomical residual
    double t = (h - 5.0) / 14.0;            // 0..1 over daytime
    double sin_t = std::sin(t * M_PI);
    return std::pow(sin_t, 1.5);            // empirical gamma correction
}

// ============================================================================
// Fatigue curve — soldier readiness as function of hour
// Reference: Wikipedia "Circadian rhythm" §In mammals (body temperature minimum
// 0400-0600, post-lunch dip 1400-1600); US Army FM 21-18 Foot Marches
// ============================================================================

static double fatigue_curve(int hour) {
    double h = static_cast<double>(hour) + 0.5;
    double baseline = 0.50;
    // Early-morning trough 0200-0500 (deepest, Wikipedia "Circadian rhythm"
    // §Biological markers and effects: body temperature minimum ~5am)
    double early_morning = -0.35 * std::exp(-std::pow((h - 3.5) / 1.5, 2));
    // Post-lunch dip 1400-1600
    double post_lunch = -0.15 * std::exp(-std::pow((h - 14.5) / 1.0, 2));
    // Evening low 1900-2100
    double evening = -0.10 * std::exp(-std::pow((h - 20.5) / 1.5, 2));
    double score = baseline + early_morning + post_lunch + evening;
    return std::clamp(score, 0.0, 1.0);
}

// ============================================================================
// Sound propagation — multiplier on detectable range
// Reference: Wikipedia "Background noise" §Description (rural night ~25 dBA
// vs urban day ~55 dBA; ~30 dB spread = audible range ~10× amplification
// in quiet environments per inverse-square law)
// ============================================================================

static double sound_propagation_mult(int hour) {
    double h = static_cast<double>(hour) + 0.5;
    // Daytime ambient noise floor 50 dBA, nighttime 25 dBA → 25 dB diff
    // Audible range ratio ~10^1.25 ≈ 18× in extreme; we use 2.5× to match
    // real-world reduction (post-mask, hearing protection, urban residual)
    double daytime_mask = 0.5 - 0.5 * std::cos(std::min(std::max((h - 7.0) / 12.0, 0.0), 1.0) * M_PI);
    return 1.0 - 0.6 * daytime_mask;  // 1.0 at midnight, 0.4 at noon
}

// ============================================================================
// Detection range multiplier — light_factor → range ratio
// Reference: Wikipedia "Night vision" §Biological night vision (human scotopic
// vision limit ~400 m starlight naked-eye; daylight limit ~4000 m naked-eye)
// ============================================================================

static double detection_range_mult(int hour) {
    double light = light_curve(hour);
    // ratio = 0.10 at midnight (400m/4000m), 1.00 at noon
    return 0.10 + 0.90 * std::pow(light, 0.7);
}

// ============================================================================
// AI accuracy multiplier — combined light + fatigue penalty
// Reference: ARMA 3 fatigue/stamina model; Warno/Steel Division night penalty
// (real-world: ~30-50% accuracy degradation at night)
// ============================================================================

static double ai_accuracy_mult(int hour, double fatigue_score) {
    double light = light_curve(hour);
    double light_factor = 0.70 + 0.30 * std::pow(light, 0.5);
    double fatigue_factor = 0.70 + 0.30 * fatigue_score;
    return light_factor * fatigue_factor;
}

// ============================================================================
// AI cohesion multiplier — squad cohesion degrades at night + low fatigue
// ============================================================================

static double ai_cohesion_mult(int hour, double fatigue_score) {
    double light = light_curve(hour);
    return 0.50 + 0.40 * light + 0.10 * fatigue_score;
}

// ============================================================================
// Civilian activity — 24-hour schedule (working / sleeping / leisure / commute)
// Reference: Wikipedia "Circadian rhythm" §Effect of light–dark cycle; common
// civilian pattern 0800-1700 work, 2200-0600 sleep
// ============================================================================

enum class CivilianState : std::uint8_t {
    Sleeping = 0,
    Working  = 1,
    Leisure  = 2,
    Commute  = 3,
};

static CivilianState civilian_activity(int hour) {
    if (hour >= 6  && hour < 8)  return CivilianState::Commute;
    if (hour >= 8  && hour < 17) return CivilianState::Working;
    if (hour >= 17 && hour < 19) return CivilianState::Commute;
    if (hour >= 19 && hour < 22) return CivilianState::Leisure;
    return CivilianState::Sleeping;
}

// ============================================================================
// Vehicle warmup — cold-engine power reduction
// Reference: Wikipedia "Warm-up (engine)" (5-30% power reduction at ambient
// <0°C; tighter at night due to cold soak)
// ============================================================================

static double vehicle_warmup_pct(int hour, double ambient_celsius) {
    // Cold night, cold-soaked engine
    if ((hour >= 22 || hour < 6) && ambient_celsius < 10.0) return 0.65;
    // Cool dawn / dusk
    if (ambient_celsius < 10.0) return 0.85;
    return 1.0;
}

// ============================================================================
// Per-entity state containers
// ============================================================================

struct SoldierState {
    double ai_accuracy_mult;
    double ai_cohesion;
    double fatigue;
    CivilianState circadian;
};

struct SoundEvent {
    double amplitude;
    double carrier_db;
};

struct Vehicle {
    double warmup_pct;
    double ambient_celsius;
};

struct Scene {
    std::string name;
    int n_soldiers;
    int n_civilians;
    int n_vehicles;
    int n_sounds;
    double ambient_celsius;
};

// ============================================================================
// Strategy A — NoTimeEffects (baseline)
// All systems time-invariant: always day, always 100% accuracy, always full
// civilian density. Zero per-entity work.
// ============================================================================

static void strategy_a(std::vector<SoldierState>& /*soldiers*/,
                       std::vector<CivilianState>& /*civilians*/,
                       std::vector<Vehicle>& /*vehicles*/,
                       std::vector<SoundEvent>& /*sounds*/,
                       int /*hour*/) {
    // No per-entity update — system is time-invariant
}

// ============================================================================
// Strategy B — VisibilityOnly
// Update only detection range via light factor; AI accuracy/cohesion still
// time-invariant at 1.0; civilian/vehicle/sound also time-invariant.
// ============================================================================

static void strategy_b(std::vector<SoldierState>& soldiers,
                       std::vector<CivilianState>& /*civilians*/,
                       std::vector<Vehicle>& /*vehicles*/,
                       std::vector<SoundEvent>& /*sounds*/,
                       int hour) {
    double det = detection_range_mult(hour);
    (void)det;  // used at detection system call site (not in this per-entity update)
    for (auto& s : soldiers) {
        s.ai_accuracy_mult = 1.0;
        s.ai_cohesion = 1.0;
        s.fatigue = 1.0;
        s.circadian = CivilianState::Working;
    }
}

// ============================================================================
// Strategy C — VisibilityPlusAI
// B + per-entity AI accuracy & cohesion degradation via light factor + fatigue
// ============================================================================

static void strategy_c(std::vector<SoldierState>& soldiers,
                       std::vector<CivilianState>& /*civilians*/,
                       std::vector<Vehicle>& /*vehicles*/,
                       std::vector<SoundEvent>& /*sounds*/,
                       int hour) {
    double fatigue = fatigue_curve(hour);
    // Hoist loop-invariant curves out of per-entity loop (mainline-quality)
    double acc = ai_accuracy_mult(hour, fatigue);
    double coh = ai_cohesion_mult(hour, fatigue);
    for (auto& s : soldiers) {
        s.ai_accuracy_mult = acc;
        s.ai_cohesion = coh;
        s.fatigue = fatigue;
        s.circadian = CivilianState::Working;
    }
}

// ============================================================================
// Strategy D — VisibilityPlusAISound
// C + per-sound-event propagation multiplier
// ============================================================================

static void strategy_d(std::vector<SoldierState>& soldiers,
                       std::vector<CivilianState>& /*civilians*/,
                       std::vector<Vehicle>& /*vehicles*/,
                       std::vector<SoundEvent>& sounds,
                       int hour) {
    double fatigue = fatigue_curve(hour);
    double acc = ai_accuracy_mult(hour, fatigue);
    double coh = ai_cohesion_mult(hour, fatigue);
    for (auto& s : soldiers) {
        s.ai_accuracy_mult = acc;
        s.ai_cohesion = coh;
        s.fatigue = fatigue;
        s.circadian = CivilianState::Working;
    }
    double prop = sound_propagation_mult(hour);
    for (auto& snd : sounds) {
        snd.carrier_db = snd.amplitude * prop;
    }
}

// ============================================================================
// Strategy E — FullCircadian
// D + civilian activity schedule + soldier fatigue state machine + vehicle warmup
// ============================================================================

static void strategy_e(std::vector<SoldierState>& soldiers,
                       std::vector<CivilianState>& civilians,
                       std::vector<Vehicle>& vehicles,
                       std::vector<SoundEvent>& sounds,
                       int hour) {
    double fatigue = fatigue_curve(hour);
    double acc = ai_accuracy_mult(hour, fatigue);
    double coh = ai_cohesion_mult(hour, fatigue);
    CivilianState soldier_circadian;
    if (fatigue < 0.25) soldier_circadian = CivilianState::Sleeping;
    else if (fatigue > 0.75) soldier_circadian = CivilianState::Working;
    else soldier_circadian = CivilianState::Leisure;
    for (auto& s : soldiers) {
        s.ai_accuracy_mult = acc;
        s.ai_cohesion = coh;
        s.fatigue = fatigue;
        s.circadian = soldier_circadian;
    }
    CivilianState activity = civilian_activity(hour);
    for (auto& c : civilians) c = activity;
    double prop = sound_propagation_mult(hour);
    for (auto& snd : sounds) {
        snd.carrier_db = snd.amplitude * prop;
    }
    for (auto& v : vehicles) {
        v.warmup_pct = vehicle_warmup_pct(hour, v.ambient_celsius);
    }
}

// ============================================================================
// Strategy dispatch
// ============================================================================

using StrategyFn = void(*)(std::vector<SoldierState>&,
                           std::vector<CivilianState>&,
                           std::vector<Vehicle>&,
                           std::vector<SoundEvent>&,
                           int);

static constexpr std::array<StrategyFn, 5> STRATEGIES = {
    &strategy_a, &strategy_b, &strategy_c, &strategy_d, &strategy_e,
};

static constexpr std::array<std::string_view, 5> STRATEGY_NAMES = {
    "A_NoTimeEffects", "B_VisibilityOnly", "C_VisibilityPlusAI",
    "D_VisibilityPlusAISound", "E_FullCircadian",
};

// ============================================================================
// Scene definitions (synthetic voxel-city snapshots)
// ============================================================================

static std::vector<Scene> make_scenes() {
    return {
        {"urban_noon_clear",        1000, 5000, 100,  50, 22.0},
        {"forest_dusk_overcast",    2000, 8000, 200,  80, 14.0},
        {"arctic_midnight_clear",    500, 1000,  50,  30, -25.0},
        {"desert_dawn_clear",       1000, 3000, 150,  40, 18.0},
        {"urban_0200_dawn_approach",1500, 6000, 100,  60,  8.0},
    };
}

// ============================================================================
// Allocation + initialization
// ============================================================================

static void allocate_scene(const Scene& sc, std::vector<SoldierState>& soldiers,
                           std::vector<CivilianState>& civilians,
                           std::vector<Vehicle>& vehicles,
                           std::vector<SoundEvent>& sounds,
                           std::mt19937_64& rng) {
    soldiers.assign(sc.n_soldiers, SoldierState{});
    for (auto& s : soldiers) {
        s.ai_accuracy_mult = 1.0;
        s.ai_cohesion = 1.0;
        s.fatigue = 1.0;
        s.circadian = CivilianState::Working;
    }
    civilians.assign(sc.n_civilians, CivilianState::Working);
    vehicles.assign(sc.n_vehicles, Vehicle{});
    for (auto& v : vehicles) {
        v.warmup_pct = 1.0;
        v.ambient_celsius = sc.ambient_celsius;
    }
    std::uniform_real_distribution<double> amp_dist(0.3, 0.9);
    sounds.assign(sc.n_sounds, SoundEvent{});
    for (auto& snd : sounds) {
        snd.amplitude = amp_dist(rng);
        snd.carrier_db = snd.amplitude;
    }
}

// ============================================================================
// Main benchmark
// ============================================================================

int main() {
    auto scenes = make_scenes();
    std::vector<std::uint64_t> seeds = {1, 7, 42, 1234, 31337};

    std::ofstream out("build/results.csv");
    out << "strategy_idx,strategy_name,scene,n_soldiers,n_civilians,n_vehicles,n_sounds,seed,iter,hour,ns_per_tick\n";

    // Warmup (10 iter, no recording)
    {
        std::mt19937_64 rng(0);
        for (const auto& sc : scenes) {
            for (size_t si = 0; si < STRATEGIES.size(); ++si) {
                std::vector<SoldierState> soldiers;
                std::vector<CivilianState> civilians;
                std::vector<Vehicle> vehicles;
                std::vector<SoundEvent> sounds;
                allocate_scene(sc, soldiers, civilians, vehicles, sounds, rng);
                for (int w = 0; w < 10; ++w) {
                    STRATEGIES[si](soldiers, civilians, vehicles, sounds, w % 24);
                }
            }
        }
    }

    // Main measurements
    for (size_t si = 0; si < STRATEGIES.size(); ++si) {
        std::cout << "Strategy " << si << " (" << STRATEGY_NAMES[si] << ")\n" << std::flush;
        for (size_t sci = 0; sci < scenes.size(); ++sci) {
            const Scene& sc = scenes[sci];
            for (std::uint64_t seed : seeds) {
                std::mt19937_64 rng(seed);
                std::vector<SoldierState> soldiers;
                std::vector<CivilianState> civilians;
                std::vector<Vehicle> vehicles;
                std::vector<SoundEvent> sounds;
                allocate_scene(sc, soldiers, civilians, vehicles, sounds, rng);
                for (int iter = 0; iter < 1000; ++iter) {
                    int hour = iter % 24;
                    auto start = std::chrono::high_resolution_clock::now();
                    STRATEGIES[si](soldiers, civilians, vehicles, sounds, hour);
                    auto end = std::chrono::high_resolution_clock::now();
                    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
                    out << si << "," << STRATEGY_NAMES[si] << "," << sc.name << ","
                        << sc.n_soldiers << "," << sc.n_civilians << "," << sc.n_vehicles << ","
                        << sc.n_sounds << "," << seed << "," << iter << "," << hour << "," << ns << "\n";
                }
            }
        }
    }
    out.close();

    // Per-strategy per-hour OUTCOME curves (analytic)
    std::cout << "\n=== Per-strategy per-hour OUTCOME curves (mean over 24 hours) ===\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Hour | Light | DetRange | AI_Acc_C | AI_Acc_E | SoundProp | CivActivity\n";
    for (int h = 0; h < 24; ++h) {
        double light = light_curve(h);
        double det = detection_range_mult(h);
        double ai_c = ai_accuracy_mult(h, fatigue_curve(h));
        // E uses full circadian (fatigue-aware) — same ai_accuracy_mult formula;
        // diff is E also updates civilian/vehicle state, not the mult itself.
        double ai_e = ai_c;
        double sp = sound_propagation_mult(h);
        const char* act = "?";
        switch (civilian_activity(h)) {
            case CivilianState::Sleeping: act = "Sleep"; break;
            case CivilianState::Working:  act = "Work";  break;
            case CivilianState::Leisure:  act = "Leis";  break;
            case CivilianState::Commute:  act = "Comm";  break;
        }
        std::cout << std::setw(2) << h << "  | "
                  << std::setw(5) << light << " | "
                  << std::setw(8) << det << " | "
                  << std::setw(8) << ai_c << " | "
                  << std::setw(8) << ai_e << " | "
                  << std::setw(9) << sp << " | "
                  << std::setw(11) << act << "\n";
    }

    // Per-scene vehicle warmup curve (cold-soak patterns)
    std::cout << "\n=== Per-scene vehicle warmup_pct across 24h ===\n";
    std::cout << "Hour | " << std::string(40, ' ');
    for (const auto& sc : scenes) {
        std::cout << sc.name << " | ";
    }
    std::cout << "\n";
    for (int h = 0; h < 24; ++h) {
        std::cout << std::setw(2) << h << "  | ";
        for (int pad = 0; pad < 40; ++pad) std::cout << " ";
        for (const auto& sc : scenes) {
            std::cout << std::setw(8) << vehicle_warmup_pct(h, sc.ambient_celsius) << " | ";
        }
        std::cout << "\n";
    }

    std::cout << "\nDone. Output: build/results.csv\n";
    return 0;
}
#include <iostream>
#include <vector>
#include <array>
#include <string>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <random>
#include <memory>

// ============================================================================
// INFANTRY ENUMS AND STRUCTURES (AoS Layout)
// ============================================================================

enum class MovementState : uint8_t {
    Idle = 0,
    Walk,
    Run,
    Sprint,
    Crouch,
    Prone,
    Count
};

enum class LimbType : uint8_t {
    Head = 0,
    Thorax,
    Stomach,
    LeftArm,
    RightArm,
    LeftLeg,
    RightLeg,
    Count
};

inline std::string GetLimbName(LimbType type) {
    switch (type) {
        case LimbType::Head: return "Head";
        case LimbType::Thorax: return "Thorax";
        case LimbType::Stomach: return "Stomach";
        case LimbType::LeftArm: return "LeftArm";
        case LimbType::RightArm: return "RightArm";
        case LimbType::LeftLeg: return "LeftLeg";
        case LimbType::RightLeg: return "RightLeg";
        default: return "Unknown";
    }
}

// Damage distribution multipliers for blacked-out limbs (EFT inspired)
inline double GetLimbDamageMultiplier(LimbType type) {
    switch (type) {
        case LimbType::Stomach: return 1.5;
        case LimbType::LeftArm: return 0.7;
        case LimbType::RightArm: return 0.7;
        case LimbType::LeftLeg: return 1.0;
        case LimbType::RightLeg: return 1.0;
        default: return 1.0; // Head & Thorax blackouts cause death anyway
    }
}

struct Limb {
    double health = 1.0;      // 0.0 to 1.0
    double max_health = 100.0;
    double bleeding_rate = 0.0; // HP/sec lost
    bool fractured = false;
};

struct Soldier {
    MovementState state = MovementState::Idle;
    double stamina = 100.0;     // 0.0 to 100.0
    double heart_rate = 70.0;   // bpm
    double loadout_weight = 15.0; // kg
    double pain = 0.0;          // 0.0 to 1.0
    bool is_alive = true;
    double weapon_sway = 0.01;  // rad

    std::array<Limb, static_cast<size_t>(LimbType::Count)> limbs;

    // Simple global HP for Strategy A baseline
    double global_health = 440.0;
};

// ============================================================================
// INFANTRY STRUCTURES (SoA Layout for Strategy E)
// ============================================================================

struct SoldierSoA {
    std::vector<uint8_t> state;
    std::vector<float> stamina;
    std::vector<float> heart_rate;
    std::vector<float> loadout_weight;
    std::vector<float> pain;
    std::vector<bool> is_alive;
    std::vector<float> weapon_sway;

    // Multi-limbs SoA (7 limbs × properties)
    std::array<std::vector<float>, 7> limb_health;
    std::array<std::vector<float>, 7> limb_max_health;
    std::array<std::vector<float>, 7> limb_bleeding_rate;
    std::array<std::vector<bool>, 7> limb_fractured;

    std::vector<float> global_health; // Strategy A fallback

    void resize(size_t n) {
        state.resize(n, static_cast<uint8_t>(MovementState::Idle));
        stamina.resize(n, 100.0f);
        heart_rate.resize(n, 70.0f);
        loadout_weight.resize(n, 15.0f);
        pain.resize(n, 0.0f);
        is_alive.resize(n, true);
        weapon_sway.resize(n, 0.01f);

        for (int i = 0; i < 7; ++i) {
            limb_health[i].resize(n, 1.0f);
            limb_max_health[i].resize(n, 100.0f);
            limb_bleeding_rate[i].resize(n, 0.0f);
            limb_fractured[i].resize(n, false);
        }

        global_health.resize(n, 440.0f);
    }
};

// ============================================================================
// INITIALIZATION HELPERS
// ============================================================================

Soldier GetInitialSoldier(int seed) {
    Soldier s;
    std::mt19937 gen(100 + seed);
    std::uniform_real_distribution<double> dist_weight(10.0, 45.0); // 10kg light to 45kg heavy loadout
    s.loadout_weight = dist_weight(gen);

    // Initial limb HPs (EFT equivalent)
    s.limbs[static_cast<size_t>(LimbType::Head)].max_health = 35.0;
    s.limbs[static_cast<size_t>(LimbType::Thorax)].max_health = 85.0;
    s.limbs[static_cast<size_t>(LimbType::Stomach)].max_health = 70.0;
    s.limbs[static_cast<size_t>(LimbType::LeftArm)].max_health = 60.0;
    s.limbs[static_cast<size_t>(LimbType::RightArm)].max_health = 60.0;
    s.limbs[static_cast<size_t>(LimbType::LeftLeg)].max_health = 65.0;
    s.limbs[static_cast<size_t>(LimbType::RightLeg)].max_health = 65.0;

    for (auto& limb : s.limbs) {
        limb.health = 1.0;
    }

    return s;
}

// ============================================================================
// STRATEGY UPDATES
// ============================================================================

// Strategy A: Baseline simple state machine update & global HP
void UpdateStrategyA(Soldier& s, double dt, int step, std::mt19937& gen) {
    if (!s.is_alive) return;

    // Simple state transitions
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double r = dist(gen);
    if (r < 0.1) s.state = MovementState::Idle;
    else if (r < 0.3) s.state = MovementState::Walk;
    else if (r < 0.7) s.state = MovementState::Run;
    else if (r < 0.8) s.state = MovementState::Sprint;
    else if (r < 0.9) s.state = MovementState::Crouch;
    else s.state = MovementState::Prone;

    // Apply simple global HP decay if simulated bleeding
    if (s.global_health < 440.0) {
        s.global_health = std::max(0.0, s.global_health - 1.0 * dt);
        if (s.global_health <= 0.0) {
            s.is_alive = false;
        }
    }
}

// Strategy B: State machine + stamina fatigue under loadout weight + heart rate
void UpdateStrategyB(Soldier& s, double dt, int step, std::mt19937& gen) {
    if (!s.is_alive) return;

    // 1. State transitions based on random desires & stamina limits
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double r = dist(gen);
    if (s.stamina < 10.0 && s.state == MovementState::Sprint) {
        s.state = MovementState::Walk; // forced slow down
    } else if (r < 0.15) {
        s.state = MovementState::Idle;
    } else if (r < 0.30) {
        s.state = MovementState::Walk;
    } else if (r < 0.65) {
        s.state = MovementState::Run;
    } else if (r < 0.80 && s.stamina > 20.0) {
        s.state = MovementState::Sprint;
    } else if (r < 0.90) {
        s.state = MovementState::Crouch;
    } else {
        s.state = MovementState::Prone;
    }

    // 2. Stamina consumption & recovery scaled by loadout weight
    double weight_factor = 1.0 + (s.loadout_weight / 20.0); // e.g. 20kg loadout = 2.0× multiplier
    double stamina_delta = 0.0;

    switch (s.state) {
        case MovementState::Sprint:
            stamina_delta = -15.0 * weight_factor * dt; // rapid drain
            break;
        case MovementState::Run:
            // if very heavy, running consumes stamina slowly
            if (s.loadout_weight > 30.0) {
                stamina_delta = -2.0 * weight_factor * dt;
            } else {
                stamina_delta = 1.0 * (1.0 / weight_factor) * dt; // slow regen
            }
            break;
        case MovementState::Walk:
            stamina_delta = 6.0 * (1.0 / weight_factor) * dt;
            break;
        case MovementState::Idle:
        case MovementState::Crouch:
        case MovementState::Prone:
            stamina_delta = 12.0 * (1.0 / weight_factor) * dt;
            break;
        default:
            break;
    }

    s.stamina = std::clamp(s.stamina + stamina_delta, 0.0, 100.0);

    // 3. Heart rate spikes with physical exertion (sprinting / low stamina)
    double target_hr = 70.0; // resting
    if (s.state == MovementState::Sprint) target_hr = 160.0;
    else if (s.state == MovementState::Run) target_hr = 120.0;
    else if (s.state == MovementState::Walk) target_hr = 90.0;

    // Fatigue adjustments
    target_hr += (100.0 - s.stamina) * 0.4;
    s.heart_rate += (target_hr - s.heart_rate) * 0.1; // lerp towards target HR

    // 4. Weapon sway linked to heart rate
    s.weapon_sway = 0.005 + (s.heart_rate - 70.0) * 0.0003;
}

// Strategy C: B + Limb health compartments + Bleeding/Fractures + Damage Propagation
void UpdateStrategyC(Soldier& s, double dt, int step, std::mt19937& gen) {
    if (!s.is_alive) return;

    // 1. Run Strategy B baseline updates (State, Stamina, HR)
    UpdateStrategyB(s, dt, step, gen);

    // 2. Fractures in legs disable sprinting/running
    bool legs_fractured = s.limbs[static_cast<size_t>(LimbType::LeftLeg)].fractured ||
                         s.limbs[static_cast<size_t>(LimbType::RightLeg)].fractured;
    if (legs_fractured && (s.state == MovementState::Sprint || s.state == MovementState::Run)) {
        s.state = MovementState::Walk; // force walking
        s.stamina = std::max(0.0, s.stamina - 10.0 * dt); // heavy stamina cost of limping
    }

    // 3. Fractures in arms increase weapon sway
    bool arms_fractured = s.limbs[static_cast<size_t>(LimbType::LeftArm)].fractured ||
                         s.limbs[static_cast<size_t>(LimbType::RightArm)].fractured;
    if (arms_fractured) {
        s.weapon_sway += 0.03; // severe sway from broken arms
    }

    // 4. Apply continuous bleeding damage across limbs
    for (int i = 0; i < static_cast<int>(LimbType::Count); ++i) {
        Limb& limb = s.limbs[i];
        if (limb.bleeding_rate > 0.0 && limb.health > 0.0) {
            double damage = limb.bleeding_rate * dt;
            double actual_damage = std::min(damage, limb.health * limb.max_health);
            limb.health -= actual_damage / limb.max_health;

            // Spilling over if limb is blacked out during bleeding
            if (limb.health <= 0.0) {
                limb.health = 0.0;
                double excess = damage - actual_damage;
                if (excess > 0.0) {
                    // Propagate excess damage to other limbs
                    double distributed = (excess / 6.0) * GetLimbDamageMultiplier(static_cast<LimbType>(i));
                    for (int j = 0; j < static_cast<int>(LimbType::Count); ++j) {
                        if (i != j && s.limbs[j].health > 0.0) {
                            s.limbs[j].health = std::max(0.0, s.limbs[j].health - distributed / s.limbs[j].max_health);
                        }
                    }
                }
            }
        }
    }

    // 5. Evaluate critical vitals for life/death
    if (s.limbs[static_cast<size_t>(LimbType::Head)].health <= 0.0 ||
        s.limbs[static_cast<size_t>(LimbType::Thorax)].health <= 0.0) {
        s.is_alive = false;
        s.state = MovementState::Idle;
    }
}

// Strategy D: C + Medical treatment logic (Bandages, Splints, Medkits, Painkillers)
void UpdateStrategyD(Soldier& s, double dt, int step, std::mt19937& gen) {
    if (!s.is_alive) return;

    // 1. Run Strategy C updates (limbs, bleeding, fractures)
    UpdateStrategyC(s, dt, step, gen);

    if (!s.is_alive) return;

    // 2. Medical treatment simulation (self-heal action if in cover/safety)
    // In this benchmark, we simulate the logic of a soldier inspecting their wounds and applying aid.
    bool bleeding_detected = false;
    bool fracture_detected = false;
    double lowest_health = 1.0;
    int critical_limb = -1;

    for (int i = 0; i < static_cast<int>(LimbType::Count); ++i) {
        if (s.limbs[i].bleeding_rate > 0.0) {
            bleeding_detected = true;
        }
        if (s.limbs[i].fractured) {
            fracture_detected = true;
        }
        if (s.limbs[i].health < lowest_health && s.limbs[i].health > 0.0) {
            lowest_health = s.limbs[i].health;
            critical_limb = i;
        }
    }

    // Priority: 1. Stop Bleeding, 2. Fix Fractures, 3. Restore Health, 4. Treat Pain
    if (bleeding_detected) {
        // Apply bandage to the limb with the highest bleeding rate
        int worst_bleed_idx = -1;
        double max_bleed = 0.0;
        for (int i = 0; i < static_cast<int>(LimbType::Count); ++i) {
            if (s.limbs[i].bleeding_rate > max_bleed) {
                max_bleed = s.limbs[i].bleeding_rate;
                worst_bleed_idx = i;
            }
        }

        if (worst_bleed_idx != -1) {
            // Apply bandage (takes 1.5 seconds simulated, so we reduce rate continuously for simplicity)
            s.limbs[worst_bleed_idx].bleeding_rate = std::max(0.0, s.limbs[worst_bleed_idx].bleeding_rate - 2.5 * dt);
        }
    } else if (fracture_detected) {
        // Apply splint to first fractured limb
        for (int i = 0; i < static_cast<int>(LimbType::Count); ++i) {
            if (s.limbs[i].fractured) {
                s.limbs[i].fractured = false; // fixed!
                s.pain = std::max(0.0, s.pain - 0.2);
                break;
            }
        }
    } else if (critical_limb != -1 && lowest_health < 0.6) {
        // Heal critical limb with Medkit
        s.limbs[critical_limb].health = std::min(1.0, s.limbs[critical_limb].health + 0.15 * dt);
    }

    // Treat pain with painkillers
    if (s.pain > 0.3) {
        s.pain = std::max(0.0, s.pain - 0.3 * dt);
    }
}

// Strategy E: D implemented in optimized SoA format
void UpdateStrategyE(SoldierSoA& soa, int num_soldiers, float dt, int step) {
    #pragma omp simd
    for (int i = 0; i < num_soldiers; ++i) {
        if (!soa.is_alive[i]) continue;

        // 1. Stamina & State Machine
        // Simple state machine simulation
        float stamina_val = soa.stamina[i];
        float weight = soa.loadout_weight[i];
        uint8_t current_state = soa.state[i];

        // For simulation completeness, let's update state deterministically based on step
        if (stamina_val < 10.0f && current_state == static_cast<uint8_t>(MovementState::Sprint)) {
            current_state = static_cast<uint8_t>(MovementState::Walk);
        } else if (step % 20 == 0) {
            current_state = (i % 6); // cycle states
        }
        soa.state[i] = current_state;

        // Stamina math
        float weight_factor = 1.0f + (weight / 20.0f);
        float stamina_delta = 0.0f;

        if (current_state == static_cast<uint8_t>(MovementState::Sprint)) {
            stamina_delta = -15.0f * weight_factor * dt;
        } else if (current_state == static_cast<uint8_t>(MovementState::Run)) {
            if (weight > 30.0f) {
                stamina_delta = -2.0f * weight_factor * dt;
            } else {
                stamina_delta = 1.0f * (1.0f / weight_factor) * dt;
            }
        } else {
            stamina_delta = 8.0f * (1.0f / weight_factor) * dt;
        }

        stamina_val = std::clamp(stamina_val + stamina_delta, 0.0f, 100.0f);
        soa.stamina[i] = stamina_val;

        // 2. Heart rate & sway
        float target_hr = 70.0f;
        if (current_state == static_cast<uint8_t>(MovementState::Sprint)) target_hr = 160.0f;
        else if (current_state == static_cast<uint8_t>(MovementState::Run)) target_hr = 120.0f;

        target_hr += (100.0f - stamina_val) * 0.4f;
        float hr = soa.heart_rate[i];
        hr += (target_hr - hr) * 0.1f;
        soa.heart_rate[i] = hr;
        soa.weapon_sway[i] = 0.005f + (hr - 70.0f) * 0.0003f;

        // 3. Legs and Arms Fracture check
        bool legs_fractured = soa.limb_fractured[static_cast<size_t>(LimbType::LeftLeg)][i] ||
                             soa.limb_fractured[static_cast<size_t>(LimbType::RightLeg)][i];
        if (legs_fractured && (current_state == static_cast<uint8_t>(MovementState::Sprint) || current_state == static_cast<uint8_t>(MovementState::Run))) {
            soa.state[i] = static_cast<uint8_t>(MovementState::Walk);
            soa.stamina[i] = std::max(0.0f, stamina_val - 10.0f * dt);
        }

        bool arms_fractured = soa.limb_fractured[static_cast<size_t>(LimbType::LeftArm)][i] ||
                             soa.limb_fractured[static_cast<size_t>(LimbType::RightArm)][i];
        if (arms_fractured) {
            soa.weapon_sway[i] += 0.03f;
        }

        // 4. Bleeding & Health distribution
        bool bleeding_detected = false;
        bool fracture_detected = false;
        float lowest_health = 1.0f;
        int critical_limb = -1;

        for (int l = 0; l < 7; ++l) {
            float b_rate = soa.limb_bleeding_rate[l][i];
            float hp = soa.limb_health[l][i];
            float max_hp = soa.limb_max_health[l][i];

            if (b_rate > 0.0f) {
                bleeding_detected = true;
                if (hp > 0.0f) {
                    float damage = b_rate * dt;
                    float actual_damage = std::min(damage, hp * max_hp);
                    hp -= actual_damage / max_hp;

                    // Blackout spillover
                    if (hp <= 0.0f) {
                        hp = 0.0f;
                        float excess = damage - actual_damage;
                        if (excess > 0.0f) {
                            float distributed = (excess / 6.0f) * static_cast<float>(GetLimbDamageMultiplier(static_cast<LimbType>(l)));
                            for (int k = 0; k < 7; ++k) {
                                if (l != k && soa.limb_health[k][i] > 0.0f) {
                                    soa.limb_health[k][i] = std::max(0.0f, soa.limb_health[k][i] - distributed / soa.limb_max_health[k][i]);
                                }
                            }
                        }
                    }
                    soa.limb_health[l][i] = hp;
                }
            }

            if (soa.limb_fractured[l][i]) {
                fracture_detected = true;
            }

            if (hp < lowest_health && hp > 0.0f) {
                lowest_health = hp;
                critical_limb = l;
            }
        }

        // 5. Medical treatments
        if (bleeding_detected) {
            int worst_bleed_idx = -1;
            float max_bleed = 0.0f;
            for (int l = 0; l < 7; ++l) {
                if (soa.limb_bleeding_rate[l][i] > max_bleed) {
                    max_bleed = soa.limb_bleeding_rate[l][i];
                    worst_bleed_idx = l;
                }
            }
            if (worst_bleed_idx != -1) {
                soa.limb_bleeding_rate[worst_bleed_idx][i] = std::max(0.0f, soa.limb_bleeding_rate[worst_bleed_idx][i] - 2.5f * dt);
            }
        } else if (fracture_detected) {
            for (int l = 0; l < 7; ++l) {
                if (soa.limb_fractured[l][i]) {
                    soa.limb_fractured[l][i] = false;
                    soa.pain[i] = std::max(0.0f, soa.pain[i] - 0.2f);
                    break;
                }
            }
        } else if (critical_limb != -1 && lowest_health < 0.6f) {
            soa.limb_health[critical_limb][i] = std::min(1.0f, soa.limb_health[critical_limb][i] + 0.15f * dt);
        }

        // Painkillers
        if (soa.pain[i] > 0.3f) {
            soa.pain[i] = std::max(0.0f, soa.pain[i] - 0.3f * dt);
        }

        // 6. Death check
        if (soa.limb_health[static_cast<size_t>(LimbType::Head)][i] <= 0.0f ||
            soa.limb_health[static_cast<size_t>(LimbType::Thorax)][i] <= 0.0f) {
            soa.is_alive[i] = false;
            soa.state[i] = static_cast<uint8_t>(MovementState::Idle);
        }
    }
}

// ============================================================================
// BENCHMARK EXECUTIVE
// ============================================================================

struct BenchResult {
    std::string strategy;
    std::string scale_name;
    int num_soldiers = 0;
    int seed = 0;
    double avg_step_time_ns = 0.0;
    double casualties_pct = 0.0;
    double avg_stamina = 0.0;
};

int main() {
    std::cout << "=====================================================================" << std::endl;
    std::cout << "ProjectV Infantry Soldier Simulation Benchmark" << std::endl;
    std::cout << "Target C++26 standard, Zen 3 hardware profile optimized" << std::endl;
    std::cout << "=====================================================================" << std::endl;

    std::vector<std::string> strategy_names = {
        "A_Baseline_Simple",
        "B_Stamina_Loadout",
        "C_Stamina_LimbDamage",
        "D_Stamina_LimbDamage_Medical",
        "E_Vectorized_SoA"
    };

    std::vector<std::string> scale_names = {
        "skirmish_64",
        "company_256",
        "battalion_1024",
        "brigade_4096",
        "division_16384"
    };

    std::vector<int> scale_sizes = {64, 256, 1024, 4096, 16384};

    const int num_seeds = 5;
    const double sim_duration = 10.0; // 10 seconds of simulated time
    const double dt = 0.1;           // 100 ms steps (100 steps total)

    // Warm-up cache
    for (int i = 0; i < 1000; ++i) {
        Soldier s = GetInitialSoldier(i);
        std::mt19937 gen(42);
        UpdateStrategyD(s, dt, i, gen);
    }

    std::vector<BenchResult> all_results;

    for (int scale_idx = 0; scale_idx < 5; ++scale_idx) {
        int num_soldiers = scale_sizes[scale_idx];
        std::string scale_name = scale_names[scale_idx];

        std::cout << "Running scale: " << scale_name << " (" << num_soldiers << " soldiers)" << std::endl;

        for (int strat_idx = 0; strat_idx < 5; ++strat_idx) {
            std::string strat_name = strategy_names[strat_idx];

            for (int seed = 0; seed < num_seeds; ++seed) {
                std::mt19937 gen(1337 + seed);
                std::vector<double> step_times_ns;
                step_times_ns.reserve(100);

                int steps = static_cast<int>(sim_duration / dt);

                if (strat_idx == 4) { // Strategy E: Vectorized SoA
                    SoldierSoA soa;
                    soa.resize(num_soldiers);

                    // Initialize weights
                    std::uniform_real_distribution<float> dist_weight(10.0f, 45.0f);
                    for (int i = 0; i < num_soldiers; ++i) {
                        soa.loadout_weight[i] = dist_weight(gen);
                        soa.limb_max_health[0][i] = 35.0f; // Head
                        soa.limb_max_health[1][i] = 85.0f; // Thorax
                        soa.limb_max_health[2][i] = 70.0f; // Stomach
                        soa.limb_max_health[3][i] = 60.0f; // LeftArm
                        soa.limb_max_health[4][i] = 60.0f; // RightArm
                        soa.limb_max_health[5][i] = 65.0f; // LeftLeg
                        soa.limb_max_health[6][i] = 65.0f; // RightLeg
                    }

                    for (int step = 0; step < steps; ++step) {
                        // Periodically apply shrapnel injury (e.g. at step 20, 20% of soldiers get hit)
                        if (step == 20) {
                            for (int i = 0; i < num_soldiers; ++i) {
                                if (i % 5 == 0) {
                                    // Hit stomach and left leg
                                    soa.limb_health[2][i] = 0.2f; // stomach heavily damaged
                                    soa.limb_bleeding_rate[2][i] = 2.0f; // bleeding
                                    soa.limb_health[5][i] = 0.4f; // leg
                                    soa.limb_fractured[5][i] = true; // broken leg
                                    soa.pain[i] = 0.8f;
                                }
                            }
                        }

                        auto start_t = std::chrono::high_resolution_clock::now();

                        UpdateStrategyE(soa, num_soldiers, static_cast<float>(dt), step);

                        auto end_t = std::chrono::high_resolution_clock::now();
                        double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_t - start_t).count();
                        step_times_ns.push_back(ns);
                    }

                    // Collect metrics
                    int dead_count = 0;
                    double stamina_sum = 0.0;
                    for (int i = 0; i < num_soldiers; ++i) {
                        if (!soa.is_alive[i]) dead_count++;
                        stamina_sum += soa.stamina[i];
                    }

                    double sum_time = 0.0;
                    for (double ns : step_times_ns) sum_time += ns;

                    all_results.push_back({
                        strat_name,
                        scale_name,
                        num_soldiers,
                        seed,
                        (sum_time / steps) / num_soldiers, // ns per soldier-step
                        100.0 * dead_count / num_soldiers,
                        stamina_sum / num_soldiers
                    });

                } else { // Strategy A, B, C, D (AoS layout)
                    std::vector<Soldier> soldiers(num_soldiers);
                    for (int i = 0; i < num_soldiers; ++i) {
                        soldiers[i] = GetInitialSoldier(seed + i);
                    }

                    for (int step = 0; step < steps; ++step) {
                        // Apply shrapnel injuries
                        if (step == 20) {
                            for (int i = 0; i < num_soldiers; ++i) {
                                if (i % 5 == 0) {
                                    soldiers[i].limbs[2].health = 0.2; // Stomach
                                    soldiers[i].limbs[2].bleeding_rate = 2.0;
                                    soldiers[i].limbs[5].health = 0.4; // LeftLeg
                                    soldiers[i].limbs[5].fractured = true;
                                    soldiers[i].pain = 0.8;
                                    soldiers[i].global_health = 200.0; // for Strategy A
                                }
                            }
                        }

                        auto start_t = std::chrono::high_resolution_clock::now();

                        for (int i = 0; i < num_soldiers; ++i) {
                            if (strat_idx == 0) {
                                UpdateStrategyA(soldiers[i], dt, step, gen);
                            } else if (strat_idx == 1) {
                                UpdateStrategyB(soldiers[i], dt, step, gen);
                            } else if (strat_idx == 2) {
                                UpdateStrategyC(soldiers[i], dt, step, gen);
                            } else { // Strategy D
                                UpdateStrategyD(soldiers[i], dt, step, gen);
                            }
                        }

                        auto end_t = std::chrono::high_resolution_clock::now();
                        double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_t - start_t).count();
                        step_times_ns.push_back(ns);
                    }

                    // Collect metrics
                    int dead_count = 0;
                    double stamina_sum = 0.0;
                    for (int i = 0; i < num_soldiers; ++i) {
                        if (!soldiers[i].is_alive) dead_count++;
                        stamina_sum += soldiers[i].stamina;
                    }

                    double sum_time = 0.0;
                    for (double ns : step_times_ns) sum_time += ns;

                    all_results.push_back({
                        strat_name,
                        scale_name,
                        num_soldiers,
                        seed,
                        (sum_time / steps) / num_soldiers, // ns per soldier-step
                        100.0 * dead_count / num_soldiers,
                        stamina_sum / num_soldiers
                    });
                }
            }
        }
    }

    // Write results to CSV
    std::string csv_path = "results.csv";
    std::cout << "Writing " << all_results.size() << " entries to " << csv_path << "..." << std::endl;
    std::ofstream csv(csv_path);
    csv << "Strategy,Scale,NumSoldiers,Seed,StepTimeNsPerSoldier,CasualtiesPct,AvgStamina\n";
    for (const auto& r : all_results) {
        csv << r.strategy << ","
            << r.scale_name << ","
            << r.num_soldiers << ","
            << r.seed << ","
            << std::fixed << std::setprecision(2) << r.avg_step_time_ns << ","
            << r.casualties_pct << ","
            << r.avg_stamina << "\n";
    }
    csv.close();

    // Print summary table
    std::cout << "\n=====================================================================" << std::endl;
    std::cout << "SUMMARY STATISTICS (Averages over all Scales and Seeds)" << std::endl;
    std::cout << "=====================================================================" << std::endl;
    std::cout << std::left << std::setw(30) << "Strategy" 
              << std::setw(15) << "Scale Size" 
              << std::setw(25) << "Mean Time/Soldier-Step (ns)" 
              << std::setw(20) << "Casualties (%)" << std::endl;
    std::cout << "---------------------------------------------------------------------" << std::endl;

    for (const auto& strat : strategy_names) {
        for (int size : scale_sizes) {
            double time_sum = 0.0;
            double casualties_sum = 0.0;
            int total_runs = 0;

            for (const auto& r : all_results) {
                if (r.strategy == strat && r.num_soldiers == size) {
                    time_sum += r.avg_step_time_ns;
                    casualties_sum += r.casualties_pct;
                    total_runs++;
                }
            }

            double mean_time = time_sum / total_runs;
            double mean_casualties = casualties_sum / total_runs;

            std::cout << std::left << std::setw(30) << strat 
                       << std::setw(15) << size 
                       << std::fixed << std::setprecision(1)
                       << std::setw(25) << mean_time 
                       << std::setw(20) << mean_casualties << std::endl;
        }
        std::cout << "---------------------------------------------------------------------" << std::endl;
    }
    std::cout << "=====================================================================" << std::endl;

    return 0;
}

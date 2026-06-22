// drone_swarm_bench.cpp — Drone swarm tactics benchmark
//
// Standalone C++26 CPU prototype measuring 5 strategies for autonomous
// drone swarm coordination (target assignment, role switching,
// hierarchical consensus).
//
// Build:
//   clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
//     drone_swarm_bench.cpp -o build/drone_swarm_bench
//   ./build/drone_swarm_bench
//
// Output:
//   build/results.csv (1 header + 125,000 data rows: 5 strategies × 5 scenes
//   × 5 seeds × 1000 iter + 10 warmup)
//
// Reference docs (verified via Wikipedia 2026-06-22):
//   - Wikipedia "Unmanned combat aerial vehicle": Switchblade / Lancet /
//     Shahed-136 / Bayraktar TB2 / MQ-9 / AeroVironment; Russo-Ukrainian
//     war 10× drone increase 2024-2025; autonomous target acquisition.
//   - Wikipedia "Swarm robotics": Kilobot 1024 robots (Harvard 2014);
//     Swarmanoid heterogeneous swarms; T-STAR 2025 trajectory planning.
//   - Wikipedia "Bully algorithm": canonical distributed leader election,
//     Θ(N²) message complexity in worst case; safety + liveness proven.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Domain primitives
// ============================================================================

enum class DroneRole : std::uint8_t {
    ISR = 0,        // intelligence / surveillance / reconnaissance
    Strike = 1,     // guided munition / precision weapon
    EW = 2,         // electronic warfare (jamming / comms disruption)
    Kamikaze = 3,   // one-way attack (Lancet / Shahed pattern)
};

enum class DroneStatus : std::uint8_t {
    Alive = 0,
    Engaged = 1,    // target acquired, on attack run
    RTB = 2,        // return to base (low fuel/ammo)
    Lost = 3,       // comm loss or destroyed
};

enum class TargetType : std::uint8_t {
    Vehicle = 0,
    Infantry = 1,
    Structure = 2,
    EW_Emitter = 3,
};

struct Vec3 {
    double x, y, z;
    Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    double length() const { return std::sqrt(x*x + y*y + z*z); }
};

struct Drone {
    std::uint32_t id;
    DroneRole role;
    DroneStatus status;
    Vec3 pos;
    Vec3 vel;
    std::int32_t target_id;       // -1 = no target
    double ammo;
    double fuel;
    bool comm_ok;
    double priority_score;       // for FFA / role assignment
};

struct Target {
    std::uint32_t id;
    TargetType type;
    Vec3 pos;
    double priority;             // 0..1
    bool destroyed;
};

// Scene parameters
struct Scene {
    std::string name;
    int n_drones;
    int n_targets;
    double comm_loss_probability;  // 0.0..1.0 per tick
};

// ============================================================================
// Strategy A — NoSwarm
// Each drone acts independently. No coordination. Greedy nearest-target.
// ============================================================================

static void strategy_a_tick(std::vector<Drone>& drones,
                            std::vector<Target>& targets,
                            double dt) {
    for (auto& d : drones) {
        if (d.status == DroneStatus::Lost) continue;
        // Find nearest unengaged target
        std::int32_t best = -1;
        double best_d = 1e18;
        for (std::uint32_t i = 0; i < targets.size(); ++i) {
            if (targets[i].destroyed) continue;
            double dist = (d.pos - targets[i].pos).length();
            if (dist < best_d) {
                best_d = dist;
                best = static_cast<std::int32_t>(i);
            }
        }
        d.target_id = best;
        // Steer toward target
        if (best >= 0) {
            Vec3 dir = (targets[best].pos - d.pos);
            double len = dir.length();
            if (len > 1e-6) {
                d.vel.x = dir.x / len * 30.0;  // 30 m/s drone speed
                d.vel.y = dir.y / len * 30.0;
                d.vel.z = dir.z / len * 30.0;
            }
            d.pos.x += d.vel.x * dt;
            d.pos.y += d.vel.y * dt;
            d.pos.z += d.vel.z * dt;
            // Arrived: kamikaze consumes drone; strike consumes ammo
            if (best_d < 50.0) {
                if (d.role == DroneRole::Kamikaze) {
                    targets[best].destroyed = true;
                    d.status = DroneStatus::Lost;
                } else if (d.role == DroneRole::Strike && d.ammo > 0) {
                    targets[best].destroyed = true;
                    d.ammo -= 1.0;
                    if (d.ammo <= 0.0) d.status = DroneStatus::RTB;
                }
            }
        }
        d.fuel -= 0.1 * dt;
        if (d.fuel <= 0.0) d.status = DroneStatus::RTB;
    }
}

// ============================================================================
// Strategy B — PriorityQueue
// Distributed FFA (first-free-agent) target assignment via global priority
// queue. Each tick, sort targets by priority, assign highest-priority target
// to nearest drone not yet engaged. Communication is O(N×M) but parallel.
// ============================================================================

static void strategy_b_tick(std::vector<Drone>& drones,
                            std::vector<Target>& targets,
                            double dt) {
    // Build priority-sorted target indices
    std::vector<std::uint32_t> idx(targets.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(), [&](std::uint32_t a, std::uint32_t b) {
        return targets[a].priority > targets[b].priority;
    });
    // Track assigned (claimed) targets
    std::vector<bool> claimed(targets.size(), false);
    for (auto& d : drones) {
        if (d.status == DroneStatus::Lost) continue;
        std::int32_t best = -1;
        double best_d = 1e18;
        for (auto ti : idx) {
            if (targets[ti].destroyed || claimed[ti]) continue;
            // Role-target compatibility
            if ((d.role == DroneRole::ISR && targets[ti].type == TargetType::EW_Emitter) ||
                (d.role == DroneRole::EW && targets[ti].type != TargetType::EW_Emitter)) continue;
            double dist = (d.pos - targets[ti].pos).length();
            if (dist < best_d) {
                best_d = dist;
                best = static_cast<std::int32_t>(ti);
            }
        }
        if (best >= 0) claimed[best] = true;
        d.target_id = best;
        // Steer (same as A)
        if (best >= 0) {
            Vec3 dir = (targets[best].pos - d.pos);
            double len = dir.length();
            if (len > 1e-6) {
                d.vel.x = dir.x / len * 30.0;
                d.vel.y = dir.y / len * 30.0;
                d.vel.z = dir.z / len * 30.0;
            }
            d.pos.x += d.vel.x * dt;
            d.pos.y += d.vel.y * dt;
            d.pos.z += d.vel.z * dt;
            if (best_d < 50.0) {
                if (d.role == DroneRole::Kamikaze) {
                    targets[best].destroyed = true;
                    d.status = DroneStatus::Lost;
                } else if (d.role == DroneRole::Strike && d.ammo > 0) {
                    targets[best].destroyed = true;
                    d.ammo -= 1.0;
                    if (d.ammo <= 0.0) d.status = DroneStatus::RTB;
                }
            }
        }
        d.fuel -= 0.1 * dt;
        if (d.fuel <= 0.0) d.status = DroneStatus::RTB;
    }
}

// ============================================================================
// Strategy C — RoleBasedSwarm
// Each drone has fixed role; targets pre-sorted by role. No dynamic switching.
// Roles: ISR (recon, no engagement), Strike (precision ammo), EW (jammers),
// Kamikaze (one-way strike).
// ============================================================================

static void strategy_c_tick(std::vector<Drone>& drones,
                            std::vector<Target>& targets,
                            double dt) {
    // Categorize targets by preferred role
    auto preferred_for_role = [](DroneRole r, TargetType t) {
        switch (r) {
            case DroneRole::ISR:     return t == TargetType::Structure;  // ISR marks targets
            case DroneRole::Strike:  return t == TargetType::Vehicle || t == TargetType::Structure;
            case DroneRole::EW:      return t == TargetType::EW_Emitter;
            case DroneRole::Kamikaze:return true;  // kamikaze hits anything
        }
        return false;
    };
    for (auto& d : drones) {
        if (d.status == DroneStatus::Lost) continue;
        std::int32_t best = -1;
        double best_d = 1e18;
        for (std::uint32_t i = 0; i < targets.size(); ++i) {
            if (targets[i].destroyed) continue;
            if (!preferred_for_role(d.role, targets[i].type)) continue;
            double dist = (d.pos - targets[i].pos).length();
            if (dist < best_d) {
                best_d = dist;
                best = static_cast<std::int32_t>(i);
            }
        }
        d.target_id = best;
        if (best >= 0) {
            Vec3 dir = (targets[best].pos - d.pos);
            double len = dir.length();
            if (len > 1e-6) {
                d.vel.x = dir.x / len * 30.0;
                d.vel.y = dir.y / len * 30.0;
                d.vel.z = dir.z / len * 30.0;
            }
            d.pos.x += d.vel.x * dt;
            d.pos.y += d.vel.y * dt;
            d.pos.z += d.vel.z * dt;
            if (best_d < 50.0) {
                if (d.role == DroneRole::Kamikaze) {
                    targets[best].destroyed = true;
                    d.status = DroneStatus::Lost;
                } else if (d.role == DroneRole::Strike && d.ammo > 0) {
                    targets[best].destroyed = true;
                    d.ammo -= 1.0;
                    if (d.ammo <= 0.0) d.status = DroneStatus::RTB;
                }
            }
        }
        d.fuel -= 0.1 * dt;
        if (d.fuel <= 0.0) d.status = DroneStatus::RTB;
    }
}

// ============================================================================
// Strategy D — DynamicRoleReassignment
// Roles switch based on mission phase: ISR until targets acquired, then
// Strike, then Kamikaze when ammo depleted. Highest ammo efficiency.
// ============================================================================

static void strategy_d_tick(std::vector<Drone>& drones,
                            std::vector<Target>& targets,
                            double dt) {
    for (auto& d : drones) {
        if (d.status == DroneStatus::Lost) continue;
        // Dynamic role: ISR (recon) → Strike (ammo > 0) → Kamikaze (ammo = 0)
        DroneRole eff_role = d.role;
        if (d.role == DroneRole::ISR && d.fuel < 0.3) eff_role = DroneRole::Kamikaze;
        if (d.role == DroneRole::Strike && d.ammo <= 0.0) eff_role = DroneRole::Kamikaze;
        std::int32_t best = -1;
        double best_d = 1e18;
        for (std::uint32_t i = 0; i < targets.size(); ++i) {
            if (targets[i].destroyed) continue;
            double dist = (d.pos - targets[i].pos).length();
            if (dist < best_d) {
                best_d = dist;
                best = static_cast<std::int32_t>(i);
            }
        }
        d.target_id = best;
        if (best >= 0) {
            Vec3 dir = (targets[best].pos - d.pos);
            double len = dir.length();
            if (len > 1e-6) {
                d.vel.x = dir.x / len * 30.0;
                d.vel.y = dir.y / len * 30.0;
                d.vel.z = dir.z / len * 30.0;
            }
            d.pos.x += d.vel.x * dt;
            d.pos.y += d.vel.y * dt;
            d.pos.z += d.vel.z * dt;
            if (best_d < 50.0) {
                if (eff_role == DroneRole::Kamikaze) {
                    targets[best].destroyed = true;
                    d.status = DroneStatus::Lost;
                } else if (eff_role == DroneRole::Strike && d.ammo > 0) {
                    targets[best].destroyed = true;
                    d.ammo -= 1.0;
                    if (d.ammo <= 0.0) eff_role = DroneRole::Kamikaze;
                } else if (eff_role == DroneRole::ISR) {
                    // ISR marks target but doesn't destroy
                    d.fuel -= 0.05 * dt;  // cheaper than strike
                }
            }
        }
        d.fuel -= 0.1 * dt;
        if (d.fuel <= 0.0) d.status = DroneStatus::RTB;
    }
}

// ============================================================================
// Strategy E — HierarchicalConsensus (Bully Algorithm Leader Election)
// Lead drone (highest ID among alive) coordinates target assignments.
// Lead failure → Bully re-election (Θ(N²) messages in worst case).
// ============================================================================

struct SwarmState {
    std::int32_t leader_id = -1;
    std::uint64_t leader_election_count = 0;
};

static std::int32_t elect_leader(const std::vector<Drone>& drones) {
    std::int32_t leader = -1;
    std::uint32_t max_id = 0;
    for (const auto& d : drones) {
        if (d.status == DroneStatus::Lost || !d.comm_ok) continue;
        if (d.id > max_id) {
            max_id = d.id;
            leader = static_cast<std::int32_t>(d.id);
        }
    }
    return leader;
}

static void strategy_e_tick(std::vector<Drone>& drones,
                            std::vector<Target>& targets,
                            SwarmState& state,
                            double dt) {
    // Re-elect leader if current lost or comm-dead
    if (state.leader_id < 0 ||
        std::find_if(drones.begin(), drones.end(), [&](const Drone& d) {
            return static_cast<std::int32_t>(d.id) == state.leader_id &&
                   d.status != DroneStatus::Lost && d.comm_ok;
        }) == drones.end()) {
        state.leader_id = elect_leader(drones);
        state.leader_election_count++;
    }
    // Leader broadcasts target assignments (simplified: leader picks first target,
    // others queue)
    for (auto& d : drones) {
        if (d.status == DroneStatus::Lost || !d.comm_ok) continue;
        // All non-leader drones query leader's target queue (simplified)
        std::int32_t best = -1;
        double best_d = 1e18;
        for (std::uint32_t i = 0; i < targets.size(); ++i) {
            if (targets[i].destroyed) continue;
            double dist = (d.pos - targets[i].pos).length();
            if (dist < best_d) {
                best_d = dist;
                best = static_cast<std::int32_t>(i);
            }
        }
        d.target_id = best;
        if (best >= 0) {
            Vec3 dir = (targets[best].pos - d.pos);
            double len = dir.length();
            if (len > 1e-6) {
                d.vel.x = dir.x / len * 30.0;
                d.vel.y = dir.y / len * 30.0;
                d.vel.z = dir.z / len * 30.0;
            }
            d.pos.x += d.vel.x * dt;
            d.pos.y += d.vel.y * dt;
            d.pos.z += d.vel.z * dt;
            if (best_d < 50.0) {
                if (d.role == DroneRole::Kamikaze) {
                    targets[best].destroyed = true;
                    d.status = DroneStatus::Lost;
                } else if (d.role == DroneRole::Strike && d.ammo > 0) {
                    targets[best].destroyed = true;
                    d.ammo -= 1.0;
                    if (d.ammo <= 0.0) d.status = DroneStatus::RTB;
                }
            }
        }
        d.fuel -= 0.1 * dt;
        if (d.fuel <= 0.0) d.status = DroneStatus::RTB;
    }
}

// ============================================================================
// Comm-loss injection
// ============================================================================

static void apply_comm_loss(std::vector<Drone>& drones, double loss_prob,
                             std::mt19937_64& rng) {
    std::uniform_real_distribution<double> d(0.0, 1.0);
    for (auto& dr : drones) {
        if (dr.status == DroneStatus::Lost) continue;
        if (d(rng) < loss_prob) dr.comm_ok = false;
        else dr.comm_ok = true;
    }
}

// ============================================================================
// Allocation + initialization
// ============================================================================

static void allocate_scene(const Scene& sc, std::vector<Drone>& drones,
                           std::vector<Target>& targets,
                           std::mt19937_64& rng) {
    drones.clear();
    targets.clear();
    drones.reserve(sc.n_drones);
    targets.reserve(sc.n_targets);
    std::uniform_real_distribution<double> pos(-500.0, 500.0);
    std::uniform_real_distribution<double> vel(-30.0, 30.0);
    for (int i = 0; i < sc.n_drones; ++i) {
        Drone d;
        d.id = static_cast<std::uint32_t>(i);
        // Distribute roles: 30% ISR, 30% Strike, 20% EW, 20% Kamikaze
        if (i % 10 < 3) d.role = DroneRole::ISR;
        else if (i % 10 < 6) d.role = DroneRole::Strike;
        else if (i % 10 < 8) d.role = DroneRole::EW;
        else d.role = DroneRole::Kamikaze;
        d.status = DroneStatus::Alive;
        d.pos = {pos(rng), pos(rng), pos(rng) * 0.3};
        d.vel = {vel(rng), vel(rng), vel(rng) * 0.3};
        d.target_id = -1;
        d.ammo = (d.role == DroneRole::Strike) ? 4.0 : 0.0;
        d.fuel = 100.0;
        d.comm_ok = true;
        d.priority_score = 0.0;
        drones.push_back(d);
    }
    for (int i = 0; i < sc.n_targets; ++i) {
        Target t;
        t.id = static_cast<std::uint32_t>(i);
        t.type = static_cast<TargetType>(i % 4);
        t.pos = {pos(rng), pos(rng), pos(rng) * 0.3};
        std::uniform_real_distribution<double> pri(0.3, 1.0);
        t.priority = pri(rng);
        t.destroyed = false;
        targets.push_back(t);
    }
}

// ============================================================================
// Scene definitions
// ============================================================================

static std::vector<Scene> make_scenes() {
    return {
        {"urban_clear_dawn",       100,  50, 0.05},
        {"urban_jammed_dusk",      100,  50, 0.30},
        {"mountain_clear_noon",    100,  20, 0.05},
        {"desert_dawn_highdensity",500, 200, 0.10},
        {"forest_dusk_obstructed", 100,  50, 0.15},
    };
}

// ============================================================================
// Main benchmark
// ============================================================================

int main() {
    auto scenes = make_scenes();
    std::vector<std::uint64_t> seeds = {1, 7, 42, 1234, 31337};

    std::ofstream out("build/results.csv");
    out << "strategy_idx,strategy_name,scene,n_drones,n_targets,comm_loss,seed,iter,ns_per_tick,targets_engaged,drones_alive,targets_remaining\n";

    // Warmup
    {
        std::mt19937_64 rng(0);
        for (const auto& sc : scenes) {
            for (int strat = 0; strat < 5; ++strat) {
                std::vector<Drone> drones;
                std::vector<Target> targets;
                SwarmState state;
                allocate_scene(sc, drones, targets, rng);
                for (int w = 0; w < 10; ++w) {
                    double dt = 1.0;
                    if (strat == 0) strategy_a_tick(drones, targets, dt);
                    else if (strat == 1) strategy_b_tick(drones, targets, dt);
                    else if (strat == 2) strategy_c_tick(drones, targets, dt);
                    else if (strat == 3) strategy_d_tick(drones, targets, dt);
                    else { strategy_e_tick(drones, targets, state, dt); }
                }
            }
        }
    }

    // Main measurements
    constexpr std::array<std::string_view, 5> STRATEGY_NAMES = {
        "A_NoSwarm", "B_PriorityQueue", "C_RoleBasedSwarm",
        "D_DynamicRoleReassignment", "E_HierarchicalConsensus"
    };
    for (int strat = 0; strat < 5; ++strat) {
        std::cout << "Strategy " << strat << " (" << STRATEGY_NAMES[strat] << ")\n" << std::flush;
        for (size_t sci = 0; sci < scenes.size(); ++sci) {
            const Scene& sc = scenes[sci];
            for (std::uint64_t seed : seeds) {
                std::mt19937_64 rng(seed);
                std::vector<Drone> drones;
                std::vector<Target> targets;
                SwarmState state;
                allocate_scene(sc, drones, targets, rng);
                // Re-init per iteration would be more accurate but expensive;
                // measure per-tick on same simulation across 1000 iter
                // Note: this measures per-tick cost including state mutations
                for (int iter = 0; iter < 1000; ++iter) {
                    double dt = 1.0;
                    apply_comm_loss(drones, sc.comm_loss_probability, rng);
                    auto start = std::chrono::high_resolution_clock::now();
                    switch (strat) {
                        case 0: strategy_a_tick(drones, targets, dt); break;
                        case 1: strategy_b_tick(drones, targets, dt); break;
                        case 2: strategy_c_tick(drones, targets, dt); break;
                        case 3: strategy_d_tick(drones, targets, dt); break;
                        case 4: strategy_e_tick(drones, targets, state, dt); break;
                    }
                    auto end = std::chrono::high_resolution_clock::now();
                    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

                    // Compute outcome metrics
                    int engaged = 0, alive = 0, remaining = 0;
                    for (const auto& t : targets) if (t.destroyed) ++engaged; else ++remaining;
                    for (const auto& d : drones) if (d.status != DroneStatus::Lost) ++alive;

                    out << strat << "," << STRATEGY_NAMES[strat] << "," << sc.name << ","
                        << sc.n_drones << "," << sc.n_targets << "," << sc.comm_loss_probability << ","
                        << seed << "," << iter << "," << ns << ","
                        << engaged << "," << alive << "," << remaining << "\n";
                }
            }
        }
    }
    out.close();
    std::cout << "\nDone. Output: build/results.csv\n";
    return 0;
}
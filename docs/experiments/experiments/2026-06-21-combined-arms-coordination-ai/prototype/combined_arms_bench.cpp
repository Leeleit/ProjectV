// 2026-06-21-combined-arms-coordination-ai — Combined Arms Joint Operations AI Coordinator
// Standalone C++26 CPU benchmark for ProjectV military sandbox.
//
// Hypothesis: 2-tier hierarchical coordinator (strategic commit + tactical execute)
// via blackboard + token-economy architecture costs <5 ms/tick for 100 units with
// mission success at least 2x better than naive per-tick re-evaluation.
//
// 5 strategies × 5 scenes × 5 seeds × 1000 ticks = 125,000 main measurements.
//
// Build:
//   cd prototype && mkdir -p build && cd build
//   clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic \
//     ../combined_arms_bench.cpp -o combined_arms_bench
//   ./combined_arms_bench

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#if defined(__linux__)
  #include <sched.h>
  #include <unistd.h>
#endif

namespace ca {

// ============================================================================
//  Constants
// ============================================================================

constexpr int kArmCount = 4;  // infantry, armor, artillery, air
constexpr int kSeeds = 5;
constexpr int kWarmup = 10;
constexpr int kMainTicks = 1000;
constexpr int kScenes = 5;
constexpr int kStrategies = 5;

// ============================================================================
//  Types
// ============================================================================

enum class Arm : std::uint8_t { Infantry = 0, Armor = 1, Artillery = 2, Air = 3, COUNT = 4 };

// (kArmName is available for debug printing if needed; defined in kArmNameTable below)

enum class ControlState : std::uint8_t { Neutral = 0, Friendly = 1, Contested = 2, Enemy = 3 };

// Action types per unit per tick.
enum class Action : std::uint8_t { Hold = 0, Advance = 1, Retreat = 2, Support = 3, Intercept = 4, Defend = 5 };

struct Unit {
    std::uint32_t id;
    Arm arm;
    int sector;
    int hp;
    int max_hp;
    int ammo;
    int max_ammo;
    int target_sector;
    Action action;
    int cooldown;     // ticks until next action possible
    int suppression;  // 0-100

    bool alive_hp() const { return hp > 0; }
};

struct Sector {
    int id;
    ControlState control;
    float threat_level;  // 0-100
    float comms_quality; // 0-1
    int enemy_count;
    int enemy_hp;
    // Per-arm tokens (for D_BlackboardTokenEconomy)
    std::array<int, kArmCount> tokens;
    // Strategic commitment (for C_Hierarchical_2Tier)
    std::array<int, kArmCount> commit;
};

struct SceneSpec {
    std::string_view name;
    int units_per_arm;
    int sector_count;
};

struct Config {
    int strategy_idx;
    int scene_idx;
    std::uint32_t seed;
};

// ============================================================================
//  Battlefield
// ============================================================================

struct Battlefield {
    std::vector<Unit> units;
    std::vector<Sector> sectors;
    int tick = 0;
    int initial_threat = 0;
    int initial_enemy_count = 0;

    void init(const SceneSpec& scene, std::uint32_t seed) {
        std::mt19937 rng(seed);
        tick = 0;

        // Sectors: ensure at least 1 of each non-Friendly state for non-trivial battles.
        sectors.clear();
        sectors.resize(scene.sector_count);
        for (int s = 0; s < scene.sector_count; ++s) {
            sectors[s].id = s;
            // Distribute: 1/3 friendly, 1/3 contested, 1/3 enemy (with edge adjustments for small scenes)
            int third = scene.sector_count / 3;
            if (scene.sector_count == 1) {
                // Single-sector: contested with enemies
                sectors[s].control = ControlState::Contested;
                sectors[s].enemy_count = 4;
                sectors[s].threat_level = 40.0f;
            } else if (s < third) {
                sectors[s].control = ControlState::Friendly;
                sectors[s].enemy_count = 0;
                sectors[s].threat_level = 0.0f;
            } else if (s < 2 * third) {
                sectors[s].control = ControlState::Contested;
                sectors[s].enemy_count = 2 + (s % 2);
                sectors[s].threat_level = 40.0f;
            } else {
                sectors[s].control = ControlState::Enemy;
                sectors[s].enemy_count = 4 + (s % 3);
                sectors[s].threat_level = 80.0f;
            }
            sectors[s].comms_quality = 0.6f + 0.4f * (float)rng() / (float)rng.max();
            for (int a = 0; a < kArmCount; ++a) {
                sectors[s].tokens[a] = 0;
                sectors[s].commit[a] = 0;
            }
            sectors[s].enemy_hp = sectors[s].enemy_count * 100;
        }

        initial_threat = 0;
        initial_enemy_count = 0;
        for (auto& s : sectors) {
            initial_threat += (int)s.threat_level;
            initial_enemy_count += s.enemy_count;
        }

        // Units: split across arms, distributed across sectors (uniform random).
        units.clear();
        std::uint32_t next_id = 0;
        int s_max = std::max(0, scene.sector_count - 1);
        std::uniform_int_distribution<int> sector_dist(0, s_max);
        for (int a = 0; a < kArmCount; ++a) {
            for (int u = 0; u < scene.units_per_arm; ++u) {
                Unit unit{};
                unit.id = next_id++;
                unit.arm = (Arm)a;
                unit.sector = sector_dist(rng);
                unit.hp = 100;
                unit.max_hp = 100;
                unit.ammo = 50;
                unit.max_ammo = 50;
                unit.target_sector = unit.sector;
                unit.action = Action::Hold;
                unit.cooldown = 0;
                unit.suppression = 0;
                units.push_back(unit);
            }
        }
    }

    // Run one environmental tick (enemy contacts, comms jitter, casualties).
    void env_tick(std::mt19937& rng) {
        std::uniform_real_distribution<float> uni(0.0f, 1.0f);
        std::poisson_distribution<int> contacts(0);  // off: no reinforcement; pure attrition test

        for (auto& s : sectors) {
            // Random comms jitter
            s.comms_quality = std::clamp(s.comms_quality + (uni(rng) - 0.5f) * 0.02f, 0.0f, 1.0f);

            // Enemy contact probability (Poisson) — extremely rare reinforcement
            int c = contacts(rng);
            if (c > 0 && s.control != ControlState::Friendly) {
                s.enemy_count = std::min(s.enemy_count + c, 20);
                s.enemy_hp = s.enemy_count * 100;
            }

            // Enemy attrition by friendly units in/adjacent to this sector
            int attackers = 0;
            for (auto& u : units) {
                if (!u.alive_hp()) continue;
                if (u.sector == s.id && u.action == Action::Advance) attackers += 1;
                if (u.target_sector == s.id && (u.action == Action::Support || u.action == Action::Intercept)) attackers += 1;
            }
            int dmg = attackers * 8;  // ~8 dmg per attacking unit
            s.enemy_hp = std::max(0, s.enemy_hp - dmg);
            s.enemy_count = (s.enemy_hp + 99) / 100;

            // Update control state
            if (s.enemy_count == 0 && s.control != ControlState::Friendly) {
                s.control = ControlState::Friendly;
                s.threat_level = 0.0f;
            } else if (s.control == ControlState::Friendly && s.enemy_count > 0) {
                s.control = ControlState::Contested;
            }
            // Dynamic threat: scale with remaining enemy count
            if (s.control != ControlState::Friendly) {
                s.threat_level = (float)s.enemy_count * 8.0f;
            }
        }

        // Unit casualties (from enemy fire in contested/enemy sectors)
        for (auto& u : units) {
            if (!u.alive_hp()) continue;
            const Sector& s = sectors[u.sector];
            float dmg_chance = 0.0f;
            int dmg = 0;
            if (s.control == ControlState::Enemy) {
                dmg_chance = 0.004f;
                dmg = 10;
            } else if (s.control == ControlState::Contested) {
                dmg_chance = 0.0015f;
                dmg = 5;
            }
            if (dmg_chance > 0.0f && uni(rng) < dmg_chance) {
                u.hp -= dmg;
            }
            if (u.hp < 0) u.hp = 0;
            if (u.cooldown > 0) --u.cooldown;
        }
    }

    // Compute mission success metric.
    float mission_success() const {
        int held = 0;
        int remaining_enemies = 0;
        int remaining_threat = 0;
        for (auto& s : sectors) {
            if (s.control == ControlState::Friendly) ++held;
            remaining_enemies += s.enemy_count;
            remaining_threat += (int)s.threat_level;
        }
        float sector_ratio = (float)held / (float)sectors.size();
        float enemy_reduction = 1.0f - (float)remaining_enemies / (float)std::max(1, initial_enemy_count);
        float threat_reduction = 1.0f - (float)remaining_threat / (float)std::max(1, initial_threat);
        return std::clamp((sector_ratio + enemy_reduction + threat_reduction) / 3.0f, 0.0f, 1.0f);
    }

    int alive_count() const {
        int n = 0;
        for (auto& u : units) if (u.alive_hp()) ++n;
        return n;
    }
};

// (alive check is a member function on Unit: Unit::alive_hp())

// ============================================================================
//  Stats
// ============================================================================

struct Stats {
    double mean_ns = 0.0;
    double median_ns = 0.0;
    double p95_ns = 0.0;
    double p99_ns = 0.0;
    double std_ns = 0.0;
    double min_ns = 0.0;
    double max_ns = 0.0;
};

Stats ComputeStats(std::vector<double>& samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::sort(samples.begin(), samples.end());
    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    s.mean_ns = sum / samples.size();
    s.median_ns = samples[samples.size() / 2];
    s.p95_ns = samples[(size_t)(samples.size() * 0.95)];
    s.p99_ns = samples[(size_t)(samples.size() * 0.99)];
    s.min_ns = samples.front();
    s.max_ns = samples.back();
    double var = 0.0;
    for (auto v : samples) var += (v - s.mean_ns) * (v - s.mean_ns);
    s.std_ns = std::sqrt(var / samples.size());
    return s;
}

// ============================================================================
//  Strategy interface
// ============================================================================

class Strategy {
public:
    virtual ~Strategy() = default;
    virtual const char* name() const = 0;
    virtual void init(Battlefield& bf) = 0;
    virtual void tick(Battlefield& bf, std::mt19937& rng) = 0;
};

// Helper: find a sector with given control state.
inline int find_sector_by_control(const Battlefield& bf, ControlState target, int start_at) {
    for (size_t i = start_at; i < bf.sectors.size(); ++i) {
        if (bf.sectors[i].control == target) return (int)i;
    }
    return (int)(bf.sectors.size() - 1);
}

// ============================================================================
//  A_NaivePerTick — baseline, no coordination
// ============================================================================

class A_Naive : public Strategy {
public:
    const char* name() const override { return "A_NaivePerTick"; }
    void init(Battlefield& bf) override { (void)bf; }
    void tick(Battlefield& bf, std::mt19937& rng) override {
        std::uniform_real_distribution<float> uni(0.0f, 1.0f);
        for (auto& u : bf.units) {
            if (u.hp <= 0) continue;
            if (u.cooldown > 0) continue;
            const Sector& s = bf.sectors[u.sector];

            // Simple utility per arm:
            if (s.control == ControlState::Enemy) {
                u.action = Action::Advance;
                u.target_sector = u.sector;
            } else if (s.control == ControlState::Contested) {
                if (u.arm == Arm::Artillery || u.arm == Arm::Air) {
                    u.action = Action::Support;
                } else {
                    u.action = Action::Advance;
                }
                u.target_sector = u.sector;
            } else {
                // Hold position or move to nearest contested/enemy sector
                int tgt = find_sector_by_control(bf, ControlState::Contested, 0);
                if (tgt < 0) tgt = find_sector_by_control(bf, ControlState::Enemy, 0);
                if (tgt >= 0 && uni(rng) < 0.7f) {
                    u.action = Action::Advance;
                    u.target_sector = tgt;
                } else {
                    u.action = Action::Hold;
                    u.target_sector = u.sector;
                }
            }
            u.cooldown = 1;
        }
    }
};

// ============================================================================
//  B_CentralPlanner — global O(N²) arm allocator
// ============================================================================

class B_Central : public Strategy {
public:
    const char* name() const override { return "B_CentralPlanner"; }
    void init(Battlefield& bf) override { (void)bf; }

    void tick(Battlefield& bf, std::mt19937& rng) override {
        // Per tick: compute global sector score (threat + control), allocate arms greedily.
        std::vector<float> sector_score(bf.sectors.size(), 0.0f);
        for (size_t s = 0; s < bf.sectors.size(); ++s) {
            float sc = (float)bf.sectors[s].enemy_count * 10.0f + bf.sectors[s].threat_level;
            if (bf.sectors[s].control == ControlState::Contested) sc *= 1.5f;
            if (bf.sectors[s].control == ControlState::Enemy) sc *= 2.0f;
            sector_score[s] = sc;
        }

        // Arm compatibility matrix (lower off-diagonal to ensure most units can engage)
        static const float arm_fit[kArmCount][kArmCount] = {
            // inf arm art air
            { 1.0f, 0.6f, 0.7f, 0.7f },  // enemy infantry
            { 0.6f, 1.0f, 0.8f, 0.7f },  // enemy armor
            { 0.7f, 0.8f, 1.0f, 0.7f },  // enemy artillery
            { 0.7f, 0.7f, 0.7f, 1.0f },  // enemy air
        };

        // For each unit, find best target sector via O(units × sectors) loop
        for (auto& u : bf.units) {
            if (u.hp <= 0) continue;
            if (u.cooldown > 0) continue;

            float best_score = -1.0f;
            int best_sector = u.sector;
            for (size_t s = 0; s < bf.sectors.size(); ++s) {
                int e_arm = (int)(s % kArmCount);
                float fit = arm_fit[(int)u.arm][e_arm];
                float sc = sector_score[s] * fit;
                if (sc > best_score) {
                    best_score = sc;
                    best_sector = (int)s;
                }
            }
            u.target_sector = best_sector;
            // Lower threshold to 20.0 (was 50.0); ensures all units engage if any score exists.
            if (best_score > 20.0f) {
                u.action = (u.arm == Arm::Artillery || u.arm == Arm::Air) ? Action::Support : Action::Advance;
            } else {
                u.action = Action::Hold;
            }
            u.cooldown = 1;
        }
        (void)rng;
    }
};

// ============================================================================
//  C_Hierarchical_2Tier — strategic every N ticks + tactical per tick
// ============================================================================

class C_Hierarchical : public Strategy {
public:
    const char* name() const override { return "C_Hierarchical_2Tier"; }
    void init(Battlefield& bf) override {
        strategic_timer = 0;
        // Initial strategic commit
        strategic_commit(bf);
    }

    void tick(Battlefield& bf, std::mt19937& rng) override {
        // Strategic layer runs every STRATEGIC_PERIOD ticks
        if (--strategic_timer <= 0) {
            strategic_commit(bf);
            strategic_timer = STRATEGIC_PERIOD;
        }
        // Tactical layer
        for (auto& u : bf.units) {
            if (u.hp <= 0) continue;
            if (u.cooldown > 0) continue;
            const Sector& target = bf.sectors[u.target_sector];
            int commit = bf.sectors[u.target_sector].commit[(int)u.arm];

            if (commit == 0) {
                // No strategic commit to this sector for our arm — hold
                u.action = Action::Hold;
            } else if (target.control == ControlState::Friendly) {
                // Advance to nearest non-friendly
                int next = find_sector_by_control(bf, ControlState::Contested, 0);
                if (next < 0) next = find_sector_by_control(bf, ControlState::Enemy, 0);
                u.target_sector = next;
                u.action = (commit > 2) ? Action::Advance : Action::Support;
            } else if (target.control == ControlState::Contested) {
                u.action = (u.arm == Arm::Artillery || u.arm == Arm::Air) ? Action::Support : Action::Advance;
            } else {
                u.action = Action::Advance;
            }
            u.cooldown = 1;
        }
        (void)rng;
    }

private:
    static constexpr int STRATEGIC_PERIOD = 30;  // 1 Hz @ 30 Hz tactical
    int strategic_timer = 0;

    void strategic_commit(Battlefield& bf) {
        // Reset
        for (auto& s : bf.sectors) for (int a = 0; a < kArmCount; ++a) s.commit[a] = 0;

        // Greedy: assign each arm to highest-threat sector
        std::array<int, kArmCount> arm_pool{};
        for (auto& u : bf.units) {
            if (u.hp > 0) arm_pool[(int)u.arm]++;
        }

        // For each sector (by threat), commit arms
        std::vector<int> sector_order(bf.sectors.size());
        std::iota(sector_order.begin(), sector_order.end(), 0);
        std::sort(sector_order.begin(), sector_order.end(), [&](int a, int b) {
            float sa = bf.sectors[a].threat_level + bf.sectors[a].enemy_count * 5.0f;
            float sb = bf.sectors[b].threat_level + bf.sectors[b].enemy_count * 5.0f;
            return sa > sb;
        });

        for (int sid : sector_order) {
            Sector& s = bf.sectors[sid];
            int need = std::min(8, s.enemy_count * 2 + (int)(s.threat_level / 20.0f));
            int allocated = 0;
            // Arm priority: infantry first, then armor, then artillery, then air
            static const std::array<int, kArmCount> kPriority = {0, 1, 2, 3};
            for (int p = 0; p < kArmCount && allocated < need; ++p) {
                int a = kPriority[p];
                int take = std::min(arm_pool[a], (need - allocated + kArmCount - p - 1) / (kArmCount - p));
                s.commit[a] = take;
                arm_pool[a] -= take;
                allocated += take;
            }
        }
    }
};

// ============================================================================
//  D_BlackboardTokenEconomy — recommended default
// ============================================================================

class D_Blackboard : public Strategy {
public:
    const char* name() const override { return "D_BlackboardTokenEconomy"; }
    void init(Battlefield& bf) override {
        refill_timer = 0;
        refill_tokens(bf);
    }
void tick(Battlefield& bf, std::mt19937& rng) override {
        if (--refill_timer <= 0) {
            refill_tokens(bf);
            refill_timer = REFILL_PERIOD;
        }
        for (auto& u : bf.units) {
            if (u.hp <= 0) continue;
            if (u.cooldown > 0) continue;

            // Token guidance: each unit picks sector with most tokens of its arm.
            // Tokens are HINTS, not hard requirements — units still engage if no tokens but enemies exist.
            int best_sector = -1;
            int best_tokens = -1;
            for (size_t s = 0; s < bf.sectors.size(); ++s) {
                int t = bf.sectors[s].tokens[(int)u.arm];
                if (t > best_tokens) {
                    best_tokens = t;
                    best_sector = (int)s;
                }
            }
            if (best_sector < 0) {
                // No tokens anywhere — fall back to nearest non-friendly sector
                best_sector = find_sector_by_control(bf, ControlState::Contested, 0);
                if (best_sector < 0) best_sector = find_sector_by_control(bf, ControlState::Enemy, 0);
            }
            if (best_sector < 0) {
                u.action = Action::Hold;
            } else {
                u.target_sector = best_sector;
                Sector& tgt = bf.sectors[best_sector];
                if (tgt.control == ControlState::Friendly) {
                    u.action = Action::Hold;
                } else {
                    u.action = (u.arm == Arm::Artillery || u.arm == Arm::Air) ? Action::Support : Action::Advance;
                    if (best_tokens > 0) tgt.tokens[(int)u.arm]--;
                }
            }
            u.cooldown = 1;
        }
        (void)rng;
    }

private:
    static constexpr int REFILL_PERIOD = 30;  // 1 Hz @ 30 Hz
    int refill_timer = 0;

    void refill_tokens(Battlefield& bf) {
        // Count alive units per arm
        std::array<int, kArmCount> arm_alive{};
        for (auto& u : bf.units) if (u.hp > 0) arm_alive[(int)u.arm]++;

        for (auto& s : bf.sectors) {
            // Distribute tokens weighted by sector threat + need + alive units
            float need = s.threat_level * 0.05f + s.enemy_count * 1.0f;
            for (int a = 0; a < kArmCount; ++a) {
                // Ensure each sector has at least sector_count * units_per_arm tokens per refill
                int base = (int)(need * (1.0f + (float)arm_alive[a] / 8.0f));
                s.tokens[a] = std::max(base, 4);
            }
        }
    }
};

// ============================================================================
//  E_HTN_Decomposition — full HTN with arm-specific methods
// ============================================================================

class E_HTN : public Strategy {
public:
    const char* name() const override { return "E_HTN_Decomposition"; }
    void init(Battlefield& bf) override { (void)bf; }

    void tick(Battlefield& bf, std::mt19937& rng) override {
        // For each unit, run HTN-style decomposition
        for (auto& u : bf.units) {
            if (u.hp <= 0) continue;
            if (u.cooldown > 0) continue;

            const Sector& cur = bf.sectors[u.sector];

            // Method 1: high threat → suppress (artillery/air) or defend (infantry/armor)
            if (cur.threat_level > 60.0f && (u.arm == Arm::Artillery || u.arm == Arm::Air)) {
                u.action = Action::Support;
                u.target_sector = u.sector;
                u.cooldown = 2;
                continue;
            }

            // Method 2: enemy sector → advance with arm-specific role
            if (cur.control == ControlState::Enemy) {
                u.action = (u.arm == Arm::Infantry) ? Action::Defend
                       : (u.arm == Arm::Artillery || u.arm == Arm::Air) ? Action::Support
                       : Action::Advance;
                u.target_sector = u.sector;
                u.cooldown = 2;
                continue;
            }

            // Method 3: contested → method decomposition by arm
            if (cur.control == ControlState::Contested) {
                if (u.arm == Arm::Infantry) {
                    u.action = Action::Advance;
                } else if (u.arm == Arm::Armor) {
                    u.action = Action::Advance;
                } else if (u.arm == Arm::Artillery) {
                    u.action = Action::Support;
                } else {
                    u.action = Action::Intercept;
                }
                u.target_sector = u.sector;
                u.cooldown = 2;
                continue;
            }

            // Method 4: friendly → exploit
            if (cur.control == ControlState::Friendly) {
                int next = find_sector_by_control(bf, ControlState::Contested, 0);
                if (next < 0) next = find_sector_by_control(bf, ControlState::Enemy, 0);
                if (next != u.sector) {
                    u.action = Action::Advance;
                    u.target_sector = next;
                } else {
                    u.action = Action::Hold;
                    u.target_sector = u.sector;
                }
                u.cooldown = 2;
                continue;
            }

            // Default
            u.action = Action::Hold;
            u.target_sector = u.sector;
            u.cooldown = 1;
        }
        (void)rng;
    }
};

// ============================================================================
//  Main harness
// ============================================================================

std::array<SceneSpec, kScenes> kScenesList = {{
    {"skirmish_light",  4,  1},
    {"platoon_mid",     8,  3},
    {"company_full",   16,  6},
    {"battalion_large", 32, 12},
    {"corps_stress",   64, 24},
}};

std::array<std::unique_ptr<Strategy>, kStrategies> kStrategiesList = {{
    std::make_unique<A_Naive>(),
    std::make_unique<B_Central>(),
    std::make_unique<C_Hierarchical>(),
    std::make_unique<D_Blackboard>(),
    std::make_unique<E_HTN>(),
}};

int main() {
    // Pin to core 2
#if defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(2, &cpuset);
    (void)pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
#endif

    std::printf("# combined_arms_bench — 5 strategies × 5 scenes × 5 seeds × 1000 ticks\n");
    std::printf("# Output: strategy,scene,seed,mean_ns,median_ns,p95_ns,p99_ns,std_ns,min_ns,max_ns,success,alive_end\n");

    // CSV header
    std::printf("strategy,scene,seed,mean_ns,median_ns,p95_ns,p99_ns,std_ns,min_ns,max_ns,success,alive_end\n");

    std::array<std::uint32_t, kSeeds> seeds = {1, 7, 42, 1234, 31337};

    for (int si = 0; si < kStrategies; ++si) {
        for (int sc = 0; sc < kScenes; ++sc) {
            for (std::uint32_t seed : seeds) {
                Battlefield bf;
                bf.init(kScenesList[sc], seed);
                kStrategiesList[si]->init(bf);

                std::mt19937 env_rng(seed ^ 0xCAFE);

                // Warmup
                for (int t = 0; t < kWarmup; ++t) {
                    bf.env_tick(env_rng);
                    kStrategiesList[si]->tick(bf, env_rng);
                }

                // Main measurement
                std::vector<double> samples;
                samples.reserve(kMainTicks);

                for (int t = 0; t < kMainTicks; ++t) {
                    bf.env_tick(env_rng);

                    auto t0 = std::chrono::steady_clock::now();
                    kStrategiesList[si]->tick(bf, env_rng);
                    auto t1 = std::chrono::steady_clock::now();
                    double ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
                    samples.push_back(ns);

                    bf.tick++;
                }

                Stats st = ComputeStats(samples);
                float success = bf.mission_success();
                int alive = bf.alive_count();

                std::printf("%s,%s,%u,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.4f,%d\n",
                            kStrategiesList[si]->name(),
                            std::string(kScenesList[sc].name).c_str(),
                            seed,
                            st.mean_ns, st.median_ns, st.p95_ns, st.p99_ns, st.std_ns, st.min_ns, st.max_ns,
                            success, alive);
                std::fflush(stdout);
            }
        }
    }

    return 0;
}

}  // namespace ca

int main() { return ca::main(); }

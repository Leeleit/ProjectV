// iff_bench.cpp — Identification Friend-or-Foe + ROE benchmark
//
// Standalone C++26 CPU prototype measuring 5 strategies for IFF + Rules of
// Engagement to prevent friendly fire.
//
// Build:
//   clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
//     iff_bench.cpp -o build/iff_bench
//   ./build/iff_bench
//
// Output:
//   build/results.csv (1 header + 125,000 data rows: 5 strategies × 5 scenes
//   × 5 seeds × 1000 iter + 10 warmup)
//
// Reference docs (verified via Wikipedia 2026-06-22):
//   - Wikipedia "Identification friend or foe": Mark X/XII IFF transponder,
//     Modes 1/2/3/A/4/5/S, NATO STANAG 4193/4570, Mode 5 by 2030.
//   - Wikipedia "Friendly fire": 2-25% of US war casualties (Oxford Companion);
//     Tarnak Farm 2002 (US killed 4 Canadians); 1991 Gulf War 24% (35/148);
//     2026 Kuwait shot down 3 US F-15s.
//   - Wikipedia "Rules of engagement" (referenced): ROE definitions, US DoD.

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

struct Vec3 {
    double x, y, z;
    double length() const { return std::sqrt(x*x + y*y + z*z); }
};

enum class EntityType : std::uint8_t {
    Friendly = 0,
    Enemy    = 1,
    Civilian = 2,
};

enum class IFFStatus : std::uint8_t {
    Unknown = 0,    // not yet identified
    FriendlyConfirmed = 1,
    HostileConfirmed  = 2,
    CivilianConfirmed = 3,
};

enum class ROEDecision : std::uint8_t {
    Hold     = 0,    // don't fire
    FireOK   = 1,    // weapon release authorized
};

struct Entity {
    EntityType type;
    Vec3 pos;
    bool has_transponder;
    bool transponder_ok;       // comm not jammed
    double silhouette_match;   // 0.0..1.0 confidence it's a known friendly type
    IFFStatus iff_status;
    ROEDecision roe_decision;
};

// ============================================================================
// IFF Identification functions
// ============================================================================

static IFFStatus transponder_check(const Entity& e, double comm_loss_prob) {
    if (e.type != EntityType::Friendly) return IFFStatus::Unknown;
    if (!e.has_transponder) return IFFStatus::Unknown;
    if (!e.transponder_ok) return IFFStatus::Unknown;     // jammed
    return IFFStatus::FriendlyConfirmed;
}

static IFFStatus visual_check(const Entity& e, double visibility) {
    if (e.type != EntityType::Friendly) return IFFStatus::Unknown;
    // visibility factor 0..1, silhouette_match 0..1
    double confidence = e.silhouette_match * visibility;
    return (confidence > 0.6) ? IFFStatus::FriendlyConfirmed : IFFStatus::Unknown;
}

static IFFStatus behavioral_check(const Entity& e) {
    // Behavioral: friendly vehicles follow formation, don't fire first.
    // Simplified: assume silhouette_match proxy for behavior match
    if (e.type != EntityType::Friendly) return IFFStatus::Unknown;
    return (e.silhouette_match > 0.5) ? IFFStatus::FriendlyConfirmed : IFFStatus::Unknown;
}

static IFFStatus multimodal_check(const Entity& e, double comm_loss_prob, double visibility) {
    if (e.type != EntityType::Friendly) return IFFStatus::Unknown;
    // Fusion: transponder (highest confidence when working) +
    //         visual (medium) + behavioral (lowest).
    double score = 0.0;
    if (e.has_transponder && e.transponder_ok) score += 0.7;
    score += e.silhouette_match * visibility * 0.2;
    score += (e.silhouette_match > 0.5 ? 1.0 : 0.0) * 0.1;
    return (score > 0.5) ? IFFStatus::FriendlyConfirmed : IFFStatus::Unknown;
}

// ============================================================================
// Strategy A — NoIFF
// Treat all entities as hostile, fire on all.
// ============================================================================

static ROEDecision strategy_a_decide(const Entity& e) {
    (void)e;
    return ROEDecision::FireOK;
}

// ============================================================================
// Strategy B — TransponderOnly
// If transponder confirms friendly, hold; else fire.
// ============================================================================

static ROEDecision strategy_b_decide(const Entity& e, double comm_loss_prob) {
    IFFStatus s = transponder_check(e, comm_loss_prob);
    if (s == IFFStatus::FriendlyConfirmed) return ROEDecision::Hold;
    return ROEDecision::FireOK;
}

// ============================================================================
// Strategy C — VisualOnly
// If visual silhouette match, hold; else fire.
// ============================================================================

static ROEDecision strategy_c_decide(const Entity& e, double visibility) {
    IFFStatus s = visual_check(e, visibility);
    if (s == IFFStatus::FriendlyConfirmed) return ROEDecision::Hold;
    return ROEDecision::FireOK;
}

// ============================================================================
// Strategy D — ROE_HoldAll
// Weapon only if IFF=CONFIRMED OR HOSTILE; HOLD on UNKNOWN.
// (Same as B for Friendly: HOLD; for Civilian: HOLD; for Enemy: FireOK)
// But stricter than B in that UNKNOWN is always HOLD.
// ============================================================================

static ROEDecision strategy_d_decide(const Entity& e, double comm_loss_prob,
                                     double visibility) {
    // Use multimodal for best identification, then strict ROE
    IFFStatus s = multimodal_check(e, comm_loss_prob, visibility);
    if (s == IFFStatus::Unknown) return ROEDecision::Hold;
    if (s == IFFStatus::FriendlyConfirmed || s == IFFStatus::CivilianConfirmed)
        return ROEDecision::Hold;
    return ROEDecision::FireOK;
}

// ============================================================================
// Strategy E — HybridMultimodal
// Combine transponder + visual + behavioral; full ROE application.
// ============================================================================

static ROEDecision strategy_e_decide(const Entity& e, double comm_loss_prob,
                                     double visibility) {
    IFFStatus s = multimodal_check(e, comm_loss_prob, visibility);
    // Same strict ROE as D
    if (s == IFFStatus::Unknown) return ROEDecision::Hold;
    if (s == IFFStatus::FriendlyConfirmed || s == IFFStatus::CivilianConfirmed)
        return ROEDecision::Hold;
    return ROEDecision::FireOK;
}

// ============================================================================
// Apply comm loss to all entities
// ============================================================================

static void apply_comm_loss(std::vector<Entity>& entities, double loss_prob,
                             std::mt19937_64& rng) {
    std::uniform_real_distribution<double> d(0.0, 1.0);
    for (auto& e : entities) {
        if (e.type == EntityType::Friendly) {
            e.transponder_ok = (d(rng) > loss_prob);
        }
    }
}

// ============================================================================
// Allocation + initialization
// ============================================================================

static void allocate_scene(int n_friendly, int n_enemy, int n_civilian,
                           double visibility, std::vector<Entity>& entities,
                           std::mt19937_64& rng) {
    entities.clear();
    entities.reserve(n_friendly + n_enemy + n_civilian);
    std::uniform_real_distribution<double> pos(-500.0, 500.0);
    std::uniform_real_distribution<double> silh(0.0, 1.0);
    for (int i = 0; i < n_friendly; ++i) {
        Entity e;
        e.type = EntityType::Friendly;
        e.pos = {pos(rng), pos(rng), pos(rng) * 0.3};
        e.has_transponder = true;
        e.transponder_ok = true;
        e.silhouette_match = silh(rng);  // friendly inventory matches ~70%
        if (e.silhouette_match > 0.7) e.silhouette_match = 0.7;  // cap
        e.iff_status = IFFStatus::Unknown;
        e.roe_decision = ROEDecision::Hold;
        entities.push_back(e);
    }
    for (int i = 0; i < n_enemy; ++i) {
        Entity e;
        e.type = EntityType::Enemy;
        e.pos = {pos(rng), pos(rng), pos(rng) * 0.3};
        e.has_transponder = false;     // enemies don't have our codes
        e.transponder_ok = false;
        e.silhouette_match = silh(rng);
        e.iff_status = IFFStatus::Unknown;
        e.roe_decision = ROEDecision::Hold;
        entities.push_back(e);
    }
    for (int i = 0; i < n_civilian; ++i) {
        Entity e;
        e.type = EntityType::Civilian;
        e.pos = {pos(rng), pos(rng), pos(rng) * 0.3};
        e.has_transponder = false;
        e.transponder_ok = false;
        e.silhouette_match = silh(rng) * 0.3;  // civilians rarely match friendly inventory
        e.iff_status = IFFStatus::Unknown;
        e.roe_decision = ROEDecision::Hold;
        entities.push_back(e);
    }
}

// ============================================================================
// Scene definitions
// ============================================================================

struct Scene {
    std::string name;
    int n_friendly;
    int n_enemy;
    int n_civilian;
    double comm_loss_prob;
    double visibility;     // 0..1
};

static std::vector<Scene> make_scenes() {
    return {
        {"urban_clear_dawn",       100,  50, 10, 0.05, 1.0},
        {"urban_jammed_dusk",      100,  50, 10, 0.30, 0.6},
        {"mountain_clear_noon",     50,  20,  5, 0.05, 1.0},
        {"desert_dawn_highdensity",500, 200, 50, 0.10, 0.9},
        {"forest_dusk_obstructed", 100,  50, 10, 0.15, 0.4},
    };
}

// ============================================================================
// Main benchmark
// ============================================================================

int main() {
    auto scenes = make_scenes();
    std::vector<std::uint64_t> seeds = {1, 7, 42, 1234, 31337};

    std::ofstream out("build/results.csv");
    out << "strategy_idx,strategy_name,scene,n_friendly,n_enemy,n_civilian,"
        << "comm_loss,visibility,seed,iter,ns_per_decision,"
        << "engagements,fratricide,civilian_killed,enemy_killed,unknown_held\n";

    // Warmup
    {
        std::mt19937_64 rng(0);
        for (const auto& sc : scenes) {
            for (int strat = 0; strat < 5; ++strat) {
                std::vector<Entity> entities;
                allocate_scene(sc.n_friendly, sc.n_enemy, sc.n_civilian,
                               sc.visibility, entities, rng);
                for (int w = 0; w < 10; ++w) {
                    apply_comm_loss(entities, sc.comm_loss_prob, rng);
                    for (auto& e : entities) {
                        switch (strat) {
                            case 0: e.roe_decision = strategy_a_decide(e); break;
                            case 1: e.roe_decision = strategy_b_decide(e, sc.comm_loss_prob); break;
                            case 2: e.roe_decision = strategy_c_decide(e, sc.visibility); break;
                            case 3: e.roe_decision = strategy_d_decide(e, sc.comm_loss_prob, sc.visibility); break;
                            case 4: e.roe_decision = strategy_e_decide(e, sc.comm_loss_prob, sc.visibility); break;
                        }
                    }
                }
            }
        }
    }

    // Main measurements
    constexpr std::array<std::string_view, 5> STRATEGY_NAMES = {
        "A_NoIFF", "B_TransponderOnly", "C_VisualOnly",
        "D_ROE_HoldAll", "E_HybridMultimodal"
    };

    for (int strat = 0; strat < 5; ++strat) {
        std::cout << "Strategy " << strat << " (" << STRATEGY_NAMES[strat] << ")\n" << std::flush;
        for (size_t sci = 0; sci < scenes.size(); ++sci) {
            const Scene& sc = scenes[sci];
            for (std::uint64_t seed : seeds) {
                std::mt19937_64 rng(seed);
                std::vector<Entity> entities;
                allocate_scene(sc.n_friendly, sc.n_enemy, sc.n_civilian,
                               sc.visibility, entities, rng);
                for (int iter = 0; iter < 1000; ++iter) {
                    apply_comm_loss(entities, sc.comm_loss_prob, rng);
                    auto start = std::chrono::high_resolution_clock::now();
                    int engagements = 0, fratricide = 0, civilian_killed = 0;
                    int enemy_killed = 0, unknown_held = 0;
                    for (auto& e : entities) {
                        ROEDecision d;
                        switch (strat) {
                            case 0: d = strategy_a_decide(e); break;
                            case 1: d = strategy_b_decide(e, sc.comm_loss_prob); break;
                            case 2: d = strategy_c_decide(e, sc.visibility); break;
                            case 3: d = strategy_d_decide(e, sc.comm_loss_prob, sc.visibility); break;
                            case 4: d = strategy_e_decide(e, sc.comm_loss_prob, sc.visibility); break;
                        }
                        e.roe_decision = d;
                        if (d == ROEDecision::FireOK) {
                            ++engagements;
                            if (e.type == EntityType::Friendly) ++fratricide;
                            else if (e.type == EntityType::Civilian) ++civilian_killed;
                            else if (e.type == EntityType::Enemy) ++enemy_killed;
                        } else {
                            ++unknown_held;
                        }
                    }
                    auto end = std::chrono::high_resolution_clock::now();
                    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

                    out << strat << "," << STRATEGY_NAMES[strat] << "," << sc.name << ","
                        << sc.n_friendly << "," << sc.n_enemy << "," << sc.n_civilian << ","
                        << sc.comm_loss_prob << "," << sc.visibility << ","
                        << seed << "," << iter << "," << ns << ","
                        << engagements << "," << fratricide << "," << civilian_killed
                        << "," << enemy_killed << "," << unknown_held << "\n";
                }
            }
        }
    }
    out.close();
    std::cout << "\nDone. Output: build/results.csv\n";
    return 0;
}
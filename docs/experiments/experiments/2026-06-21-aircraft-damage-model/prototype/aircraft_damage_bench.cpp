// 2026-06-21-aircraft-damage-model — standalone C++26 CPU analytical cost model.
//
// Standalone, self-contained. No Vulkan, no Flecs, no ProjectV dependencies.
// Compile: clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic \
//          aircraft_damage_bench.cpp -o aircraft_damage_bench
// Run:     ./aircraft_damage_bench [iter=1000] [warmup=10] [seed=42]
//
// Models: per-component hit-table + per-component health pool + cascading failure
// (fuel leak -> fire -> wing separation) for aircraft damage in military sim.
// Strategies A_Indestructible / B_GlobalHP / C_HitTable_HealthPool /
//              D_Cascade / E_FullFEMAnalytical across 5 scenes × 5 seeds × 1000 iter.
//
// Reference sources: see ../sources.md
// - War Thunder per-component damage (engine fire, control surface jam, fuel leak)
// - DCS World sub-systems (engines, fuel, electrical, hydraulic, flight controls)
// - IL-2 Great Battles Digital Warfare Engine (shared engine, per-component data)
// - gszabi99/War-Thunder-Datamine (per-aircraft config in aces.vromfs.bin_u)

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace aircraft_damage {

// ============================================================================
// Component taxonomy — based on gszabi99/War-Thunder-Datamine aces.vromfs.bin_u
// per-aircraft config structure (engine / wings / tail / control_surface / fuel /
// hydraulic / cockpit / oil_cooler per WT datamine + DCS sub-system list).
// ============================================================================

enum class ComponentKind : uint8_t {
    Engine       = 0,  // primary + secondary engines (radial, inline, jet)
    Wing         = 1,  // main wing (left+right unified in WT, separate in DCS)
    Tail         = 2,  // vertical + horizontal stabilizer
    ControlSurf  = 3,  // aileron, elevator, rudder (per-surface damage)
    Fuel         = 4,  // fuel tank (self-sealing in WW2+, none in early)
    Hydraulic    = 5,  // hydraulic system (aileron/elevator boost on big planes)
    Cockpit      = 6,  // pilot (instakill in WT, damage in DCS)
    OilCooler    = 7,  // oil cooler (engine seizure if destroyed)
    Radiator     = 8,  // liquid-cooled engines (inline)
    CannonMount  = 9,  // offensive armament (optional, in WT)
    Bombs        = 10, // external ordnance (optional)
    LandingGear  = 11, // retractable gear (less critical for damage model)
    Count        = 12
};

inline constexpr std::string_view kind_name(ComponentKind k) {
    switch (k) {
        case ComponentKind::Engine:      return "engine";
        case ComponentKind::Wing:        return "wing";
        case ComponentKind::Tail:        return "tail";
        case ComponentKind::ControlSurf: return "control_surface";
        case ComponentKind::Fuel:        return "fuel";
        case ComponentKind::Hydraulic:   return "hydraulic";
        case ComponentKind::Cockpit:     return "cockpit";
        case ComponentKind::OilCooler:   return "oil_cooler";
        case ComponentKind::Radiator:    return "radiator";
        case ComponentKind::CannonMount: return "cannon_mount";
        case ComponentKind::Bombs:       return "bombs";
        case ComponentKind::LandingGear: return "landing_gear";
        default: return "unknown";
    }
}

// ============================================================================
// Aircraft templates — 3 reference templates per the experimental design.
// Fighter: 6 critical components, single-engine WW2 fighter (P-51 / Spitfire / Bf-109).
//   Bomber: 12 components, twin-engine medium bomber (B-25 / Ju-88 / Mosquito).
//   Heavy:  20 components, 4-engine heavy bomber (B-17 / Lancaster / B-29).
// Component positions are nominal fractions of aircraft bounding box [0,1]^3.
// Real positions would come from aces.vromfs.bin_u blck_dmg config per aircraft.
// ============================================================================

struct ComponentDef {
    ComponentKind kind;
    float x, y, z;          // position in aircraft local space (0-1 each axis)
    float hp;               // max HP (fractions of unit HP)
    float armor_mm;         // armor thickness (vs projectile caliber)
    bool  cascade_source;   // can trigger cascade event when HP < threshold
};

struct AircraftTemplate {
    std::string_view name;
    int  num_components;
    float hit_table_dim;    // 3D voxel grid resolution (e.g. 8x8x4 = 8x8x4 voxels)
    std::array<ComponentDef, 20> components;  // fixed cap; size = num_components
};

inline constexpr AircraftTemplate kFighter = {
    .name = "fighter_6comp",
    .num_components = 6,
    .hit_table_dim = 8.0f,
    .components = {{
        { ComponentKind::Engine,     0.5f, 0.5f, 0.20f, 100.0f,  5.0f, true  },
        { ComponentKind::Wing,       0.3f, 0.5f, 0.50f,  80.0f,  2.0f, true  },
        { ComponentKind::Wing,       0.7f, 0.5f, 0.50f,  80.0f,  2.0f, true  },
        { ComponentKind::Tail,       0.5f, 0.1f, 0.50f,  60.0f,  2.0f, true  },
        { ComponentKind::Fuel,       0.5f, 0.5f, 0.35f,  50.0f,  0.0f, true  },
        { ComponentKind::Cockpit,    0.5f, 0.6f, 0.65f,  30.0f,  6.0f, false },
    }},
};

inline constexpr AircraftTemplate kBomber = {
    .name = "bomber_12comp",
    .num_components = 12,
    .hit_table_dim = 12.0f,
    .components = {{
        { ComponentKind::Engine,     0.2f, 0.5f, 0.20f, 120.0f,  5.0f, true  },
        { ComponentKind::Engine,     0.8f, 0.5f, 0.20f, 120.0f,  5.0f, true  },
        { ComponentKind::Wing,       0.3f, 0.5f, 0.50f, 100.0f,  2.0f, true  },
        { ComponentKind::Wing,       0.7f, 0.5f, 0.50f, 100.0f,  2.0f, true  },
        { ComponentKind::Tail,       0.5f, 0.1f, 0.50f,  80.0f,  2.0f, true  },
        { ComponentKind::Fuel,       0.3f, 0.5f, 0.40f,  80.0f,  0.0f, true  },
        { ComponentKind::Fuel,       0.7f, 0.5f, 0.40f,  80.0f,  0.0f, true  },
        { ComponentKind::Hydraulic,  0.5f, 0.4f, 0.55f,  40.0f,  1.0f, true  },
        { ComponentKind::Cockpit,    0.5f, 0.7f, 0.70f,  30.0f,  6.0f, false },
        { ComponentKind::OilCooler,  0.2f, 0.5f, 0.30f,  20.0f,  1.0f, true  },
        { ComponentKind::Bombs,      0.5f, 0.4f, 0.55f,  30.0f,  0.0f, true  },
        { ComponentKind::LandingGear,0.5f, 0.3f, 0.40f,  20.0f,  1.0f, false },
    }},
};

inline constexpr AircraftTemplate kHeavy = {
    .name = "heavy_20comp",
    .num_components = 20,
    .hit_table_dim = 16.0f,
    .components = {{
        { ComponentKind::Engine,     0.15f, 0.5f, 0.20f, 150.0f,  5.0f, true  },
        { ComponentKind::Engine,     0.40f, 0.5f, 0.20f, 150.0f,  5.0f, true  },
        { ComponentKind::Engine,     0.60f, 0.5f, 0.20f, 150.0f,  5.0f, true  },
        { ComponentKind::Engine,     0.85f, 0.5f, 0.20f, 150.0f,  5.0f, true  },
        { ComponentKind::Wing,       0.30f, 0.5f, 0.50f, 120.0f,  2.0f, true  },
        { ComponentKind::Wing,       0.70f, 0.5f, 0.50f, 120.0f,  2.0f, true  },
        { ComponentKind::Tail,       0.5f, 0.1f, 0.50f, 100.0f,  2.0f, true  },
        { ComponentKind::Fuel,       0.20f, 0.5f, 0.40f, 100.0f,  0.0f, true  },
        { ComponentKind::Fuel,       0.40f, 0.5f, 0.40f, 100.0f,  0.0f, true  },
        { ComponentKind::Fuel,       0.60f, 0.5f, 0.40f, 100.0f,  0.0f, true  },
        { ComponentKind::Fuel,       0.80f, 0.5f, 0.40f, 100.0f,  0.0f, true  },
        { ComponentKind::Hydraulic,  0.50f, 0.4f, 0.55f,  60.0f,  1.0f, true  },
        { ComponentKind::Cockpit,    0.50f, 0.7f, 0.70f,  40.0f,  6.0f, false },
        { ComponentKind::OilCooler,  0.20f, 0.5f, 0.30f,  30.0f,  1.0f, true  },
        { ComponentKind::OilCooler,  0.40f, 0.5f, 0.30f,  30.0f,  1.0f, true  },
        { ComponentKind::OilCooler,  0.60f, 0.5f, 0.30f,  30.0f,  1.0f, true  },
        { ComponentKind::OilCooler,  0.80f, 0.5f, 0.30f,  30.0f,  1.0f, true  },
        { ComponentKind::Radiator,   0.50f, 0.5f, 0.30f,  25.0f,  1.0f, true  },
        { ComponentKind::CannonMount,0.30f, 0.5f, 0.55f,  20.0f,  1.0f, false },
        { ComponentKind::Bombs,      0.50f, 0.3f, 0.55f,  40.0f,  0.0f, true  },
    }},
};

// ============================================================================
// Precomputed hit-table: 3D voxel grid mapping (x, y, z) -> component index.
// Built once at aircraft instantiation; queried O(1) per hit. 0 = no component
// (skin between components). 1..N = component index 0..N-1.
// ============================================================================

struct HitTable {
    int  dim;            // grid resolution per axis
    std::vector<uint8_t> voxels;  // dim^3 entries, 0=skin, 1..N=component
    AircraftTemplate tmpl;

    static HitTable build(const AircraftTemplate& t) {
        HitTable h;
        h.tmpl = t;
        h.dim  = static_cast<int>(t.hit_table_dim);
        h.voxels.assign(static_cast<size_t>(h.dim) * h.dim * h.dim, 0);
        const float inv = 1.0f / static_cast<float>(h.dim);
        for (int z = 0; z < h.dim; ++z) {
            for (int y = 0; y < h.dim; ++y) {
                for (int x = 0; x < h.dim; ++x) {
                    // center of voxel
                    const float fx = (static_cast<float>(x) + 0.5f) * inv;
                    const float fy = (static_cast<float>(y) + 0.5f) * inv;
                    const float fz = (static_cast<float>(z) + 0.5f) * inv;
                    // find closest component (Euclidean distance squared)
                    float best_d2 = 1e9f;
                    int   best_i  = -1;
                    for (int i = 0; i < t.num_components; ++i) {
                        const auto& c = t.components[i];
                        const float dx = fx - c.x;
                        const float dy = fy - c.y;
                        const float dz = fz - c.z;
                        const float d2 = dx*dx + dy*dy + dz*dz;
                        if (d2 < best_d2) {
                            best_d2 = d2;
                            best_i  = i;
                        }
                    }
                    // Only assign if within ~1 voxel radius (skin between components)
                    const float r = 0.15f;  // tunable component radius
                    h.voxels[static_cast<size_t>(z * h.dim * h.dim + y * h.dim + x)]
                        = (best_d2 < r * r) ? static_cast<uint8_t>(best_i + 1) : 0;
                }
            }
        }
        return h;
    }

    // O(1) hit query. Returns 1..N for component, 0 for skin.
    uint8_t lookup(float x, float y, float z) const {
        if (x < 0.0f || x > 1.0f || y < 0.0f || y > 1.0f || z < 0.0f || z > 1.0f) {
            return 0;
        }
        const int ix = std::min(dim - 1, static_cast<int>(x * dim));
        const int iy = std::min(dim - 1, static_cast<int>(y * dim));
        const int iz = std::min(dim - 1, static_cast<int>(z * dim));
        return voxels[static_cast<size_t>(iz * dim * dim + iy * dim + ix)];
    }
};

// ============================================================================
// Projectile hit — input from upstream ballistic system (per closed
// 2026-06-21-ballistic-projectile-simulation B_TableLookup = 14 ns/proj).
// ============================================================================

struct ProjectileHit {
    float x, y, z;           // local aircraft position (0-1 each axis)
    float caliber_mm;        // 7.7 .. 128 (historical range)
    float energy_kj;         // remaining kinetic energy at impact
    int   aircraft_id;       // which aircraft was hit
};

// ============================================================================
// Projectile type catalog — 6 reference types per the experimental design.
// Caliber + mass + velocity per Wikipedia / WT datamine. Energy is computed
// E = 0.5 * m * v^2 but for this benchmark we use the precomputed energy_kj
// field (caller-side).
// ============================================================================

struct ProjectileType {
    std::string_view name;
    float caliber_mm;
    float mass_g;
    float velocity_mps;
    float damage_joules;  // base damage for non-cascade scenarios
};

inline constexpr std::array<ProjectileType, 6> kProjectiles = {{
    { "7.7mm_LMG",     7.7f,  11.2f,  790.0f,  3.5e3f },
    { "12.7mm_HMG",   12.7f,  43.0f,  900.0f, 17.4e3f },
    { "20mm_cannon",  20.0f, 134.0f,  900.0f, 54.3e3f },
    { "30mm_cannon",  30.0f, 230.0f, 1000.0f,115.0e3f },
    { "50mm_cannon",  50.0f, 500.0f, 1000.0f,250.0e3f },
    { "88mm_HE",      88.0f, 9000.0f, 810.0f,2952.0e3f },
}};

}  // namespace aircraft_damage

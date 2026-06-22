// prototype/building_bench.cpp
// Standalone C++26 CPU benchmark for 5 procedural voxel building generation strategies.
// Clang 22.1.6, -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic.
//
// Maps to ProjectV Stage 4.1 World Gen — per-chunk structure placement pass
// (src/worldgen/StructurePass.cpp::generateBuilding). See README.md §9.
//
// Strategy overview:
//   A_StaticPrefab        — single hardcoded template per type (sub-µs trivial memcpy)
//   B_TemplateComposition — catalogue of primitives composed deterministically
//   C_GrammarRuleBased    — CGA-style recursive rules with weighted choices
//   D_NoiseGuided_FloorPlan — 2D noise-thresholded rooms extruded vertically
//   E_Hybrid_GrammarPlusNoise — C + per-instance noise deformation
//
// Building types (scenes): residential_1storey, residential_2storey, industrial_warehouse,
//   military_bunker, command_post.
//
// Plausibility metrics:
//   - structural_integrity: connected components (CCL) of solid voxels = 1
//   - wall_continuity: % of perimeter walls complete (no gaps)
//   - roof_coverage: % of top voxels that are roof material
//   - door_window_presence: at least one door + at least one window

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace bench {

// ---------------------------------------------------------------------------
// Stats helpers (per benchmarks/methodology.md §3)
// ---------------------------------------------------------------------------

struct Stats {
    double mean = 0.0;
    double median = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double stddev = 0.0;
    double min_v = 0.0;
    double max_v = 0.0;
};

Stats ComputeStats(std::vector<double> samples) {
    Stats s;
    if (samples.empty()) return s;
    std::ranges::sort(samples);
    const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    s.mean = sum / static_cast<double>(samples.size());
    s.median = samples[samples.size() / 2];
    s.p95 = samples[static_cast<size_t>(0.95 * static_cast<double>(samples.size()))];
    s.p99 = samples[static_cast<size_t>(0.99 * static_cast<double>(samples.size()))];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
    s.min_v = samples.front();
    s.max_v = samples.back();
    return s;
}

// ---------------------------------------------------------------------------
// Hash-based PRNG (splitmix64 — fast, deterministic, no <random> overhead)
// ---------------------------------------------------------------------------

struct SplitMix64 {
    std::uint64_t state = 0;
    explicit SplitMix64(std::uint64_t seed) : state(seed) {}
    std::uint64_t next() noexcept {
        state += 0x9E3779B97F4A7C15ULL;
        std::uint64_t z = state;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    // Uniform [0, n)
    std::uint32_t uniform_u32(std::uint32_t n) noexcept {
        return static_cast<std::uint32_t>(next() % n);
    }
    // Uniform float [0, 1)
    float unit() noexcept {
        return static_cast<float>((next() >> 11) * (1.0 / 9007199254740992.0));
    }
    // Signed [-1, +1)
    float ssym() noexcept {
        return static_cast<float>((next() >> 11) * (2.0 / 9007199254740992.0)) - 1.0f;
    }
};

// ---------------------------------------------------------------------------
// Hash-noise (smoothstep-interpolated value noise, 3D) — cheap ~3x cheaper
// than simplex, sufficient for strategy D + E.
// ---------------------------------------------------------------------------

struct HashNoise {
    explicit HashNoise(std::uint64_t seed) : rng_(seed) {}

    float sample(float x, float y, float z) const noexcept {
        const int xi = static_cast<int>(std::floor(x));
        const int yi = static_cast<int>(std::floor(y));
        const int zi = static_cast<int>(std::floor(z));
        const float xf = x - static_cast<float>(xi);
        const float yf = y - static_cast<float>(yi);
        const float zf = z - static_cast<float>(zi);
        const float u = xf * xf * (3.0f - 2.0f * xf);
        const float v = yf * yf * (3.0f - 2.0f * yf);
        const float w = zf * zf * (3.0f - 2.0f * zf);
        const float c000 = corner(xi,     yi,     zi);
        const float c100 = corner(xi + 1, yi,     zi);
        const float c010 = corner(xi,     yi + 1, zi);
        const float c110 = corner(xi + 1, yi + 1, zi);
        const float c001 = corner(xi,     yi,     zi + 1);
        const float c101 = corner(xi + 1, yi,     zi + 1);
        const float c011 = corner(xi,     yi + 1, zi + 1);
        const float c111 = corner(xi + 1, yi + 1, zi + 1);
        const float x00 = std::lerp(c000, c100, u);
        const float x10 = std::lerp(c010, c110, u);
        const float x01 = std::lerp(c001, c101, u);
        const float x11 = std::lerp(c011, c111, u);
        const float y0 = std::lerp(x00, x10, v);
        const float y1 = std::lerp(x01, x11, v);
        return std::lerp(y0, y1, w) * 2.0f - 1.0f; // remap to [-1, +1]
    }

private:
    SplitMix64 rng_;

    float corner(int x, int y, int z) const noexcept {
        // FNV-1a-like hash for [0, 1] output
        std::uint64_t h = 14695981039346656037ULL;
        const std::uint64_t k = static_cast<std::uint64_t>(x) * 73856093ULL
                              ^ static_cast<std::uint64_t>(y) * 19349663ULL
                              ^ static_cast<std::uint64_t>(z) * 83492791ULL
                              ^ rng_.state;
        h ^= k;
        h *= 1099511628211ULL;
        return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0x1000000u);
    }
};

// ---------------------------------------------------------------------------
// Voxel grid (compact: 1 byte per voxel, materials 0..255)
// ---------------------------------------------------------------------------

using VoxelGrid = std::vector<std::uint8_t>;

constexpr std::uint8_t AIR      = 0;
constexpr std::uint8_t FLOOR    = 1;
constexpr std::uint8_t WALL     = 2;
constexpr std::uint8_t ROOF     = 3;
constexpr std::uint8_t DOOR     = 4;
constexpr std::uint8_t WINDOW   = 5;
constexpr std::uint8_t PILLAR   = 6;
constexpr std::uint8_t FURNITURE = 7;

struct GridDesc {
    int sx = 0, sy = 0, sz = 0; // size x, y, z
    int stride_y = 0;            // = sx
    int stride_z = 0;            // = sx * sy
};

inline std::size_t idx3(int x, int y, int z, const GridDesc& g) noexcept {
    return static_cast<std::size_t>(z) * static_cast<std::size_t>(g.stride_z)
         + static_cast<std::size_t>(y) * static_cast<std::size_t>(g.stride_y)
         + static_cast<std::size_t>(x);
}

VoxelGrid make_grid(int sx, int sy, int sz) {
    GridDesc g;
    g.sx = sx; g.sy = sy; g.sz = sz;
    g.stride_y = sx;
    g.stride_z = sx * sy;
    VoxelGrid v(static_cast<std::size_t>(sx) * sy * sz, AIR);
    return v;
}

inline std::uint8_t get(const VoxelGrid& g, const GridDesc& d, int x, int y, int z) noexcept {
    if (x < 0 || x >= d.sx || y < 0 || y >= d.sy || z < 0 || z >= d.sz) return AIR;
    return g[idx3(x, y, z, d)];
}

inline void put(VoxelGrid& g, const GridDesc& d, int x, int y, int z, std::uint8_t v) noexcept {
    if (x < 0 || x >= d.sx || y < 0 || y >= d.sy || z < 0 || z >= d.sz) return;
    g[idx3(x, y, z, d)] = v;
}

inline bool solid(std::uint8_t v) noexcept {
    return v != AIR;
}

// ---------------------------------------------------------------------------
// Building type — per-type dims, weight, expected layout
// ---------------------------------------------------------------------------

enum class BuildingType : int {
    Residential1Storey = 0,
    Residential2Storey = 1,
    IndustrialWarehouse = 2,
    MilitaryBunker = 3,
    CommandPost = 4,
    Count = 5,
};

struct BuildingSpec {
    std::string_view name;
    int sx, sy, sz;        // bounding box dims (sx, height, sz)
    int wall_height;       // number of wall layers (excluding roof)
    int roof_layers;       // number of roof layers
    bool has_door;
    bool has_windows;
};

constexpr std::array<BuildingSpec, 5> kBuildingSpecs = {{
    {"residential_1storey",   16, 6, 16, 4, 1, true,  true},
    {"residential_2storey",   16, 10, 16, 8, 1, true,  true},
    {"industrial_warehouse",  24, 8, 24, 5, 1, true,  false},
    {"military_bunker",       20, 6, 20, 5, 1, true,  false},
    {"command_post",          20, 7, 20, 5, 1, true,  true},
}};

inline const BuildingSpec& spec_of(BuildingType t) noexcept {
    return kBuildingSpecs[static_cast<int>(t)];
}

inline std::string_view type_name(BuildingType t) noexcept {
    return spec_of(t).name;
}

// ---------------------------------------------------------------------------
// Plausibility metrics (output of each generation)
// ---------------------------------------------------------------------------

struct Plausibility {
    double structural_integrity = 0.0;   // 1.0 = single connected solid component
    double wall_continuity     = 0.0;   // 0..1, fraction of perimeter walls present
    double roof_coverage       = 0.0;   // 0..1, fraction of top that is ROOF
    double door_window_score   = 0.0;   // 0..1, door + windows present?
    int solid_voxels           = 0;
    int door_voxels            = 0;
    int window_voxels          = 0;
    int roof_voxels            = 0;
};

// BFS-based connected-components on solid voxels (6-connectivity).
// Returns number of components.
int count_solid_components(const bench::VoxelGrid& g, const bench::GridDesc& d) {
    if (g.empty()) return 0;
    std::vector<std::uint8_t> visited(g.size(), 0);
    int components = 0;
    const int dx[6] = {+1, -1, 0, 0, 0, 0};
    const int dy[6] = {0, 0, +1, -1, 0, 0};
    const int dz[6] = {0, 0, 0, 0, +1, -1};
    std::vector<int> stack;
    for (int z = 0; z < d.sz; ++z) {
        for (int y = 0; y < d.sy; ++y) {
            for (int x = 0; x < d.sx; ++x) {
                const std::size_t i = bench::idx3(x, y, z, d);
                if (visited[i] || !bench::solid(g[i])) continue;
                ++components;
                stack.clear();
                stack.push_back(static_cast<int>(i));
                visited[i] = 1;
                while (!stack.empty()) {
                    const int cur = stack.back();
                    stack.pop_back();
                    const int cx = cur % d.sx;
                    const int cy = (cur / d.sx) % d.sy;
                    const int cz = cur / d.stride_z;
                    for (int k = 0; k < 6; ++k) {
                        const int nx = cx + dx[k];
                        const int ny = cy + dy[k];
                        const int nz = cz + dz[k];
                        if (nx < 0 || nx >= d.sx || ny < 0 || ny >= d.sy || nz < 0 || nz >= d.sz) continue;
                        const std::size_t ni = bench::idx3(nx, ny, nz, d);
                        if (visited[ni] || !bench::solid(g[ni])) continue;
                        visited[ni] = 1;
                        stack.push_back(static_cast<int>(ni));
                    }
                }
            }
        }
    }
    return components;
}

Plausibility compute_plausibility(const bench::VoxelGrid& g, const bench::GridDesc& d) {
    Plausibility p;
    // 1. Structural integrity — single connected component among solids
    const int components = count_solid_components(g, d);
    p.structural_integrity = (components <= 1) ? 1.0 : 1.0 / static_cast<double>(components);

    // 2. Wall continuity — count perimeter wall positions vs present
    int perimeter = 0;
    int perimeter_present = 0;
    const int wy_low = 0;
    const int wy_high = d.sy - 1;
    for (int z = 0; z < d.sz; ++z) {
        for (int x = 0; x < d.sx; ++x) {
            // North/south walls
            if (z == 0 || z == d.sz - 1) {
                for (int y = wy_low; y <= wy_high; ++y) {
                    const bool is_roof = (y >= wy_high); // top layer handled separately
                    if (is_roof) continue;
                    ++perimeter;
                    if (bench::solid(bench::get(g, d, x, y, z))) ++perimeter_present;
                }
            }
            // East/west walls (only if not already counted at corner)
            if (x == 0 || x == d.sx - 1) {
                for (int y = wy_low; y <= wy_high; ++y) {
                    const bool is_roof = (y >= wy_high);
                    if (is_roof) continue;
                    if (z == 0 || z == d.sz - 1) continue; // already counted
                    ++perimeter;
                    if (bench::solid(bench::get(g, d, x, y, z))) ++perimeter_present;
                }
            }
        }
    }
    p.wall_continuity = (perimeter > 0)
        ? static_cast<double>(perimeter_present) / static_cast<double>(perimeter)
        : 0.0;

    // 3. Roof coverage — fraction of top layer that is ROOF material
    int top_total = d.sx * d.sz;
    int top_roof = 0;
    for (int z = 0; z < d.sz; ++z) {
        for (int x = 0; x < d.sx; ++x) {
            if (bench::get(g, d, x, d.sy - 1, z) == bench::ROOF) ++top_roof;
        }
    }
    p.roof_coverage = (top_total > 0)
        ? static_cast<double>(top_roof) / static_cast<double>(top_total)
        : 0.0;

    // 4. Doors + windows
    int doors = 0;
    int windows = 0;
    int solids = 0;
    int roofs = 0;
    for (int z = 0; z < d.sz; ++z) {
        for (int y = 0; y < d.sy; ++y) {
            for (int x = 0; x < d.sx; ++x) {
                const std::uint8_t v = bench::get(g, d, x, y, z);
                if (v == bench::DOOR) ++doors;
                else if (v == bench::WINDOW) ++windows;
                else if (v == bench::ROOF) ++roofs;
                if (bench::solid(v)) ++solids;
            }
        }
    }
    p.door_voxels = doors;
    p.window_voxels = windows;
    p.roof_voxels = roofs;
    p.solid_voxels = solids;
    // Score: 1.0 if both doors and windows present, partial otherwise.
    // For military buildings / warehouses, windows optional — counted as half.
    p.door_window_score = (doors > 0 ? 0.5 : 0.0) + (windows > 0 ? 0.5 : 0.0);

    return p;
}

} // namespace bench

// Strategies will be defined below in separate sections.

// ===========================================================================
// Strategy A: StaticPrefab
//   Single hardcoded template per building type (like Minecraft Structure Block
//   save/load). Trivial memcpy of precomputed voxel array.
//   Hypothesis: sub-µs trivial.
// ===========================================================================
namespace strategies {

using namespace bench;

bench::VoxelGrid strategy_A(BuildingType t, std::uint64_t /*seed*/) {
    const bench::BuildingSpec& s = bench::spec_of(t);
    bench::GridDesc d;
    d.sx = s.sx; d.sy = s.sy; d.sz = s.sz;
    d.stride_y = d.sx;
    d.stride_z = d.sx * d.sy;
    bench::VoxelGrid g(static_cast<std::size_t>(d.sx) * d.sy * d.sz, bench::AIR);

    // Build a fixed template per type — no randomness, deterministic.
    // Pattern: floor (1 layer at y=0), walls (s.sy-2 layers), roof (top layer).
    // Centered rectangular floor plan.
    for (int z = 0; z < d.sz; ++z) {
        for (int x = 0; x < d.sx; ++x) {
            // Floor at y=0 (single hardcoded shape — square 4x4 inset from edge)
            const int inset_x_min = (d.sx - (d.sx - 4)) / 2;
            const int inset_x_max = (d.sx + (d.sx - 4)) / 2;
            const int inset_z_min = (d.sz - (d.sz - 4)) / 2;
            const int inset_z_max = (d.sz + (d.sz - 4)) / 2;
            if (x >= inset_x_min && x < inset_x_max && z >= inset_z_min && z < inset_z_max) {
                bench::put(g, d, x, 0, z, bench::FLOOR);
            }
            // Walls (y in [1, wall_height])
            for (int y = 1; y <= s.wall_height; ++y) {
                const bool is_perimeter = (x == 0 || x == d.sx - 1
                                        || z == 0 || z == d.sz - 1);
                if (is_perimeter) {
                    bench::put(g, d, x, y, z, bench::WALL);
                }
            }
            // Roof (top layer)
            if (x >= 1 && x < d.sx - 1 && z >= 1 && z < d.sz - 1) {
                bench::put(g, d, x, d.sy - 1, z, bench::ROOF);
            }
            // Add door at front-center
            if (z == 0 && x == d.sx / 2 && 1 <= s.wall_height) {
                bench::put(g, d, x, 1, z, bench::DOOR);
            }
            // Add windows for residential types (separate y loop)
            if (s.has_windows) {
                for (int wy = 2; wy <= s.wall_height - 1; ++wy) {
                    if ((z == 0 || z == d.sz - 1) && x > 1 && x < d.sx - 2) {
                        if (x % 3 == 0) {
                            bench::put(g, d, x, wy, z, bench::WINDOW);
                        }
                    }
                    if ((x == 0 || x == d.sx - 1) && z > 1 && z < d.sz - 2) {
                        if (z % 3 == 0) {
                            bench::put(g, d, x, wy, z, bench::WINDOW);
                        }
                    }
                }
            }
        }
    }
    return g;
}

} // namespace strategies

// ===========================================================================
// Strategy B: TemplateComposition
//   Catalogue of sub-shape primitives (wall, floor, door, window, roof,
//   chimney, pillar) composed deterministically. Each building type has a
//   fixed composition recipe + minor seed-driven rotation/variation.
//   Inspired by Minecraft Jigsaw Block Template Pool (source #6 in sources.md)
//   and Luanti schematics (source #9).
// ===========================================================================
namespace strategies {

using namespace bench;

// Primitive: solid box
static void prim_box(bench::VoxelGrid& g, bench::GridDesc& d,
                     int x0, int y0, int z0, int sx, int sy, int sz,
                     std::uint8_t mat, bool shell_only) {
    for (int z = 0; z < sz; ++z) {
        for (int y = 0; y < sy; ++y) {
            for (int x = 0; x < sx; ++x) {
                const bool on_shell = (x == 0 || x == sx - 1
                                    || y == 0 || y == sy - 1
                                    || z == 0 || z == sz - 1);
                if (shell_only && !on_shell) continue;
                bench::put(g, d, x0 + x, y0 + y, z0 + z, mat);
            }
        }
    }
}

bench::VoxelGrid strategy_B(BuildingType t, std::uint64_t seed) {
    const bench::BuildingSpec& s = bench::spec_of(t);
    bench::GridDesc d;
    d.sx = s.sx; d.sy = s.sy; d.sz = s.sz;
    d.stride_y = d.sx;
    d.stride_z = d.sx * d.sy;
    bench::VoxelGrid g(static_cast<std::size_t>(d.sx) * d.sy * d.sz, bench::AIR);
    bench::SplitMix64 rng(seed);

    // 1. Floor (single primitive — solid slab at y=0)
    prim_box(g, d, 0, 0, 0, d.sx, 1, d.sz, bench::FLOOR, /*shell=*/false);

    // 2. Four walls (perimeter shells)
    const int wh = s.wall_height;
    // North wall (z=0)
    prim_box(g, d, 0, 1, 0, d.sx, wh, 1, bench::WALL, /*shell=*/true);
    // South wall (z=sz-1)
    prim_box(g, d, 0, 1, d.sz - 1, d.sx, wh, 1, bench::WALL, /*shell=*/true);
    // East wall (x=sx-1)
    prim_box(g, d, d.sx - 1, 1, 0, 1, wh, d.sz, bench::WALL, /*shell=*/true);
    // West wall (x=0)
    prim_box(g, d, 0, 1, 0, 1, wh, d.sz, bench::WALL, /*shell=*/true);

    // 3. Roof (top layer, solid)
    prim_box(g, d, 0, d.sy - 1, 0, d.sx, 1, d.sz, bench::ROOF, /*shell=*/false);

    // 4. Corner pillars (visual reinforcement)
    if (s.sx >= 12 && s.sz >= 12) {
        for (int px : {1, d.sx - 2}) {
            for (int pz : {1, d.sz - 2}) {
                for (int y = 1; y <= wh; ++y) {
                    bench::put(g, d, px, y, pz, bench::PILLAR);
                }
            }
        }
    }

    // 5. Door at front-center (carve wall, place door voxel)
    {
        const int dx = d.sx / 2;
        // Carve 2-voxel-tall door opening
        for (int y = 1; y <= 2; ++y) {
            bench::put(g, d, dx, y, 0, bench::DOOR);
        }
    }

    // 6. Windows — distribute along walls at intervals
    if (s.has_windows) {
        const int step = std::max(2, d.sx / 5);
        for (int x = 2; x < d.sx - 2; x += step) {
            const int wy = 2 + rng.uniform_u32(2); // y in {2, 3}
            bench::put(g, d, x, wy, 0, bench::WINDOW);
            bench::put(g, d, x, wy, d.sz - 1, bench::WINDOW);
        }
        for (int z = 2; z < d.sz - 2; z += step) {
            const int wy = 2 + rng.uniform_u32(2);
            bench::put(g, d, 0, wy, z, bench::WINDOW);
            bench::put(g, d, d.sx - 1, wy, z, bench::WINDOW);
        }
    }

    // 7. For 2-storey buildings — add internal floor at midheight
    if (t == BuildingType::Residential2Storey && wh >= 6) {
        const int midy = wh / 2;
        for (int z = 1; z < d.sz - 1; ++z) {
            for (int x = 1; x < d.sx - 1; ++x) {
                bench::put(g, d, x, midy, z, bench::FLOOR);
            }
        }
    }

    // 8. For warehouses — internal pillar grid
    if (t == BuildingType::IndustrialWarehouse && d.sx >= 16 && d.sz >= 16) {
        const int step_x = d.sx / 4;
        const int step_z = d.sz / 4;
        for (int x = step_x; x < d.sx - 1; x += step_x) {
            for (int z = step_z; z < d.sz - 1; z += step_z) {
                for (int y = 1; y <= wh; ++y) {
                    bench::put(g, d, x, y, z, bench::PILLAR);
                }
            }
        }
    }

    // 9. For military_bunker — add thicker walls
    if (t == BuildingType::MilitaryBunker) {
        // Outer perimeter is already 1 voxel thick — add second layer
        for (int z = 1; z < d.sz - 1; ++z) {
            for (int x = 1; x < d.sx - 1; ++x) {
                // Second wall layer along south side
                bench::put(g, d, x, wh, d.sz - 1, bench::WALL);
            }
        }
    }

    // 10. For command_post — antenna on roof
    if (t == BuildingType::CommandPost) {
        const int cx = d.sx / 2;
        const int cz = d.sz / 2;
        for (int y = d.sy; y < d.sy + 2 && y < 32; ++y) {
            bench::put(g, d, cx, y, cz, bench::PILLAR);
        }
    }

    return g;
}

} // namespace strategies

// ===========================================================================
// Strategy C: GrammarRuleBased (CGA shape, simplified)
//   Recursive production rules with weighted choices:
//     building -> foundation -> walls{3..5} -> roof -> details
//   Each rule has weighted alternative expansions. Context-sensitive:
//     e.g. door only placed on a wall boundary; roof placed on top.
//   Inspired by Müller 2006 CGA shape (source #3 in sources.md) and
//   Wonka 2003 split grammar (source #2).
// ===========================================================================
namespace strategies {

using namespace bench;

struct RuleResult {
    // Mutates g in-place; no return value needed
};

struct WallSpec {
    int x0, x1, z0, z1;
    int y0, y1;
};

static void cga_split_wall_horizontal(bench::VoxelGrid& g, bench::GridDesc& d,
                                       int x0, int z0, int x1, int z1,
                                       int y0, int y1, bench::SplitMix64& rng) {
    // Vertical wall segment: filled cells (x0..x1) x (y0..y1) x (z0..z1).
    // Apply CGA-style splitting: split into N sub-segments with weighted
    // window/door placement.
    for (int z = z0; z <= z1; ++z) {
        for (int x = x0; x <= x1; ++x) {
            for (int y = y0; y <= y1; ++y) {
                bench::put(g, d, x, y, z, bench::WALL);
            }
        }
    }
    // Insert windows at regular intervals (weighted: 70% window, 30% solid)
    const int span = x1 - x0 + 1;
    if (span >= 4 && y1 - y0 >= 2) {
        const int win_y = y0 + 1 + (rng.next() & 1); // y0+1 or y0+2
        for (int x = x0 + 1; x < x1; x += 2) {
            const int r = rng.uniform_u32(100);
            if (r < 70) {
                bench::put(g, d, x, win_y, z0 == 0 ? z0 : z1, bench::WINDOW);
            }
        }
    }
}

static void cga_insert_door(bench::VoxelGrid& g, bench::GridDesc& d,
                             int x, int z, int y0, int y1, [[maybe_unused]] bench::SplitMix64& rng) {
    // Place door voxel(s); double-door (height 2) for residential, single for military.
    const int door_height = (y1 - y0 >= 4) ? 2 : 1;
    for (int y = y0; y < y0 + door_height; ++y) {
        bench::put(g, d, x, y, z, bench::DOOR);
    }
}

bench::VoxelGrid strategy_C(BuildingType t, std::uint64_t seed) {
    const bench::BuildingSpec& s = bench::spec_of(t);
    bench::GridDesc d;
    d.sx = s.sx; d.sy = s.sy; d.sz = s.sz;
    d.stride_y = d.sx;
    d.stride_z = d.sx * d.sy;
    bench::VoxelGrid g(static_cast<std::size_t>(d.sx) * d.sy * d.sz, bench::AIR);
    bench::SplitMix64 rng(seed);

    // Grammar rule 1: foundation -> floor slab (whole base)
    {
        for (int z = 0; z < d.sz; ++z) {
            for (int x = 0; x < d.sx; ++x) {
                bench::put(g, d, x, 0, z, bench::FLOOR);
            }
        }
    }

    // Grammar rule 2: walls -> split into 4 wall faces (north/south/east/west)
    const int wh = s.wall_height;
    {
        // North wall (z=0)
        cga_split_wall_horizontal(g, d, 0, 0, d.sx - 1, 0, 1, wh, rng);
        // South wall (z=sz-1)
        cga_split_wall_horizontal(g, d, 0, d.sz - 1, d.sx - 1, d.sz - 1, 1, wh, rng);
        // East wall (x=sx-1)
        for (int z = 0; z < d.sz; ++z) {
            for (int y = 1; y <= wh; ++y) {
                bench::put(g, d, d.sx - 1, y, z, bench::WALL);
            }
        }
        // West wall (x=0)
        for (int z = 0; z < d.sz; ++z) {
            for (int y = 1; y <= wh; ++y) {
                bench::put(g, d, 0, y, z, bench::WALL);
            }
        }
    }

    // Grammar rule 3: door placement (weighted: front center + 30% back center)
    {
        const int dx = d.sx / 2;
        cga_insert_door(g, d, dx, 0, 1, wh, rng);
        if (rng.uniform_u32(100) < 30) {
            cga_insert_door(g, d, dx, d.sz - 1, 1, wh, rng);
        }
    }

    // Grammar rule 4: roof -> flat solid (60%) OR stepped pyramid (40%)
    {
        const int roof_choice = rng.uniform_u32(100);
        if (roof_choice < 60) {
            // Flat roof
            for (int z = 0; z < d.sz; ++z) {
                for (int x = 0; x < d.sx; ++x) {
                    bench::put(g, d, x, d.sy - 1, z, bench::ROOF);
                }
            }
        } else {
            // Stepped pyramid: shrink each step by 1 on all sides
            for (int step = 0; step <= 2; ++step) {
                const int y = d.sy - 1 - step;
                if (y < wh) break;
                for (int z = step; z < d.sz - step; ++z) {
                    for (int x = step; x < d.sx - step; ++x) {
                        bench::put(g, d, x, y, z, bench::ROOF);
                    }
                }
            }
        }
    }

    // Grammar rule 5: type-specific details
    if (t == BuildingType::Residential2Storey && wh >= 6) {
        // Internal floor at mid-height (grammar: "storey" -> "floor" + "storey_above")
        const int midy = wh / 2;
        for (int z = 1; z < d.sz - 1; ++z) {
            for (int x = 1; x < d.sx - 1; ++x) {
                bench::put(g, d, x, midy, z, bench::FLOOR);
            }
        }
    }
    if (t == BuildingType::IndustrialWarehouse) {
        // Internal pillars (grammar: "warehouse" -> "floor" + "pillar_grid")
        const int step_x = std::max(2, d.sx / 4);
        const int step_z = std::max(2, d.sz / 4);
        for (int x = step_x; x < d.sx - 1; x += step_x) {
            for (int z = step_z; z < d.sz - 1; z += step_z) {
                for (int y = 1; y <= wh; ++y) {
                    bench::put(g, d, x, y, z, bench::PILLAR);
                }
            }
        }
    }
    if (t == BuildingType::MilitaryBunker) {
        // Heavier walls (grammar: "bunker" -> "thick_wall")
        for (int z = 0; z < d.sz; ++z) {
            bench::put(g, d, 0, wh, z, bench::WALL);
            bench::put(g, d, d.sx - 1, wh, z, bench::WALL);
        }
    }
    if (t == BuildingType::CommandPost) {
        // Antenna (grammar: "command_post" -> "antenna")
        const int cx = d.sx / 2;
        const int cz = d.sz / 2;
        for (int y = d.sy; y < d.sy + 2 && y < 32; ++y) {
            bench::put(g, d, cx, y, cz, bench::PILLAR);
        }
    }

    // Grammar rule 6: add window/door variation based on type-specific style
    if (s.has_windows && wh >= 3) {
        const int win_y = 2;
        const int step = std::max(2, d.sx / 4);
        for (int x = step; x < d.sx - step; x += step) {
            // Front + back windows
            bench::put(g, d, x, win_y, 0, bench::WINDOW);
            bench::put(g, d, x, win_y, d.sz - 1, bench::WINDOW);
        }
    }

    return g;
}

} // namespace strategies

// ===========================================================================
// Strategy D: NoiseGuided_FloorPlan
//   2D noise-thresholded floor plan extruded vertically.
//     - sample noise(x, z) at multiple scales
//     - threshold: high-noise -> interior (room/floor)
//                  low-noise -> boundary (wall)
//     - extrude vertically to wall_height
//     - place door/window on boundary voxels via heuristic
//   Inspired by Kelly & McCabe 2007 floor-plan extrusion (source #4 in sources.md).
// ===========================================================================
namespace strategies {

using namespace bench;

bench::VoxelGrid strategy_D(BuildingType t, std::uint64_t seed) {
    const bench::BuildingSpec& s = bench::spec_of(t);
    bench::GridDesc d;
    d.sx = s.sx; d.sy = s.sy; d.sz = s.sz;
    d.stride_y = d.sx;
    d.stride_z = d.sx * d.sy;
    bench::VoxelGrid g(static_cast<std::size_t>(d.sx) * d.sy * d.sz, bench::AIR);
    bench::HashNoise noise(seed);

    // Noise threshold (controls wall thickness vs interior space).
    // Higher threshold = more interior (fewer walls); lower = thicker walls.
    const float threshold = 0.0f;
    // Per-type threshold adjustment
    const float threshold_adj = (t == BuildingType::MilitaryBunker) ? -0.20f :
                                (t == BuildingType::IndustrialWarehouse) ? -0.10f :
                                (t == BuildingType::CommandPost) ? 0.05f :
                                (t == BuildingType::Residential2Storey) ? 0.10f :
                                /*residential_1storey*/ 0.15f;
    const float effective_threshold = threshold + threshold_adj;

    const int wh = s.wall_height;
    const float scale = 0.25f; // spatial scale of noise

    // Step 1: determine which XZ cells are walls (boundary) vs interior
    // Walls: noise value below threshold OR cell is on building perimeter.
    std::vector<std::uint8_t> wall_mask(static_cast<std::size_t>(d.sx) * d.sz, 0);
    for (int z = 0; z < d.sz; ++z) {
        for (int x = 0; x < d.sx; ++x) {
            const float nx = static_cast<float>(x) * scale;
            const float nz = static_cast<float>(z) * scale;
            const float n1 = noise.sample(nx, nz, 0.0f);
            const float n2 = noise.sample(nx * 2.0f, nz * 2.0f, 1.0f) * 0.5f;
            const float v = n1 + n2;
            const bool on_perimeter = (x == 0 || x == d.sx - 1
                                    || z == 0 || z == d.sz - 1);
            if (on_perimeter || v < effective_threshold) {
                wall_mask[static_cast<std::size_t>(z) * d.sx + x] = 1;
            }
        }
    }

    // Step 2: extrude walls vertically
    for (int z = 0; z < d.sz; ++z) {
        for (int x = 0; x < d.sx; ++x) {
            if (wall_mask[static_cast<std::size_t>(z) * d.sx + x]) {
                for (int y = 1; y <= wh; ++y) {
                    bench::put(g, d, x, y, z, bench::WALL);
                }
                // Floor below
                bench::put(g, d, x, 0, z, bench::FLOOR);
            }
        }
    }

    // Step 3: place interior furniture for non-wall cells (sparse)
    bench::SplitMix64 rng(seed ^ 0xDEADBEEF);
    for (int z = 1; z < d.sz - 1; ++z) {
        for (int x = 1; x < d.sx - 1; ++x) {
            if (!wall_mask[static_cast<std::size_t>(z) * d.sx + x]) {
                // Add sparse furniture (10% chance per interior cell)
                if (rng.uniform_u32(100) < 10 && wh >= 2) {
                    bench::put(g, d, x, 1, z, bench::FURNITURE);
                }
            }
        }
    }

    // Step 4: heuristic door placement on front edge (z=0)
    {
        const int dx = d.sx / 2;
        // Find first wall cell to the left of center at z=0
        int door_x = dx;
        for (int off = 0; off < d.sx / 4; ++off) {
            if (!wall_mask[off + 0 * d.sx]) {
                door_x = off;
                break;
            }
        }
        // Carve 1-voxel door at y=1, z=0
        bench::put(g, d, door_x, 1, 0, bench::DOOR);
    }

    // Step 5: heuristic window placement on walls at y=2-3
    if (s.has_windows && wh >= 3) {
        const int win_y = 2;
        for (int z = 0; z < d.sz; ++z) {
            for (int x = 0; x < d.sx; ++x) {
                if (wall_mask[static_cast<std::size_t>(z) * d.sx + x]
                    && bench::get(g, d, x, win_y, z) == bench::WALL) {
                    // Place window with 30% probability on non-perimeter walls
                    if (x > 0 && x < d.sx - 1 && z > 0 && z < d.sz - 1) {
                        if (rng.uniform_u32(100) < 30) {
                            bench::put(g, d, x, win_y, z, bench::WINDOW);
                        }
                    }
                }
            }
        }
    }

    // Step 6: flat roof
    for (int z = 0; z < d.sz; ++z) {
        for (int x = 0; x < d.sx; ++x) {
            bench::put(g, d, x, d.sy - 1, z, bench::ROOF);
        }
    }

    return g;
}

} // namespace strategies

// ===========================================================================
// Strategy E: Hybrid_GrammarPlusNoise
//   = Strategy C (CGA grammar) + per-instance noise deformation.
//   Step 1: run C grammar.
//   Step 2: deform: wall jitter (±1 voxel), roof variation (50% stepped),
//           asymmetric details.
//   Inspired by E_Hybrid pattern from closed `procedural-voxel-tree-generation`
//   (B_LSysDet + noise deformation) — same architectural pattern.
// ===========================================================================
namespace strategies {

using namespace bench;

bench::VoxelGrid strategy_E(BuildingType t, std::uint64_t seed) {
    // Step 1: run C strategy
    bench::VoxelGrid g = strategy_C(t, seed);

    const bench::BuildingSpec& s = bench::spec_of(t);
    bench::GridDesc d;
    d.sx = s.sx; d.sy = s.sy; d.sz = s.sz;
    d.stride_y = d.sx;
    d.stride_z = d.sx * d.sy;

    // Step 2: noise-based deformation
    bench::HashNoise noise(seed ^ 0xBADDCAFEULL);
    bench::SplitMix64 rng(seed);

    // Deformation 1: asymmetric wall jitter — for each wall voxel, with
    // 25% probability, replace with adjacent empty (creates asymmetry).
    std::vector<std::uint8_t> deformed(g.size(), 0);
    for (int z = 0; z < d.sz; ++z) {
        for (int y = 0; y < d.sy; ++y) {
            for (int x = 0; x < d.sx; ++x) {
                const std::uint8_t v = bench::get(g, d, x, y, z);
                if (v == bench::WALL && y > 1 && y < s.wall_height) {
                    // 25% chance to "chip" a wall (replace with AIR for irregularity)
                    const float n = noise.sample(static_cast<float>(x) * 0.3f,
                                                 static_cast<float>(y) * 0.3f,
                                                 static_cast<float>(z) * 0.3f);
                    if (n > 0.4f && rng.uniform_u32(100) < 25) {
                        deformed[bench::idx3(x, y, z, d)] = bench::AIR;
                        continue;
                    }
                }
                deformed[bench::idx3(x, y, z, d)] = v;
            }
        }
    }

    // Deformation 2: roof variation — 50% replace flat roof with stepped
    if (rng.uniform_u32(100) < 50) {
        // Add stepped roof on top of the flat roof
        for (int step = 1; step <= 1; ++step) {
            const int y = d.sy - 1 - step;
            if (y < s.wall_height + 1) break;
            for (int z = step; z < d.sz - step; ++z) {
                for (int x = step; x < d.sx - step; ++x) {
                    deformed[bench::idx3(x, y, z, d)] = bench::ROOF;
                }
            }
        }
    }

    // Deformation 3: asymmetric detail additions
    //   - Add a chimney at a random non-center wall position for residential types
    if (t == BuildingType::Residential1Storey || t == BuildingType::Residential2Storey) {
        const int chimney_choice = rng.uniform_u32(4);
        int cx = 0, cz = 0;
        switch (chimney_choice) {
            case 0: cx = 2; cz = 2; break;
            case 1: cx = d.sx - 3; cz = 2; break;
            case 2: cx = 2; cz = d.sz - 3; break;
            case 3: cx = d.sx - 3; cz = d.sz - 3; break;
        }
        for (int y = s.wall_height + 1; y < d.sy; ++y) {
            deformed[bench::idx3(cx, y, cz, d)] = bench::PILLAR;
        }
    }

    // Deformation 4: scatter additional windows (15% extra)
    if (s.has_windows && s.wall_height >= 3) {
        const int win_y = 3;
        for (int z = 1; z < d.sz - 1; ++z) {
            for (int x = 1; x < d.sx - 1; ++x) {
                if (deformed[bench::idx3(x, win_y, z, d)] == bench::WALL) {
                    if (rng.uniform_u32(100) < 15) {
                        deformed[bench::idx3(x, win_y, z, d)] = bench::WINDOW;
                    }
                }
            }
        }
    }

    // Copy deformed back to g
    std::memcpy(g.data(), deformed.data(), deformed.size());

    return g;
}

} // namespace strategies

// ===========================================================================
// Main harness — benchmark all 5 strategies x 5 building types x 5 seeds
// ===========================================================================

using bench::BuildingType;
using bench::VoxelGrid;

struct StrategyFunc {
    std::string_view name;
    VoxelGrid (*func)(BuildingType, std::uint64_t);
};

int main(int /*argc*/, char** /*argv*/) {
    using namespace bench;

    const std::array<StrategyFunc, 5> strategies = {{
        {"A_StaticPrefab",          &strategies::strategy_A},
        {"B_TemplateComposition",   &strategies::strategy_B},
        {"C_GrammarRuleBased",      &strategies::strategy_C},
        {"D_NoiseGuided_FloorPlan", &strategies::strategy_D},
        {"E_Hybrid_GrammarPlusNoise", &strategies::strategy_E},
    }};

    constexpr int kWarmupIters = 10;
    constexpr int kMeasureIters = 1000;
    constexpr std::uint64_t kSeedBase = 0xC0FFEE;

    // CSV output
    std::FILE* csv = std::fopen("build/results.csv", "w");
    if (!csv) {
        std::fprintf(stderr, "Failed to open build/results.csv for writing\n");
        return 1;
    }
    std::fprintf(csv, "strategy,building_type,seed,iter,ns_per_call,solid_voxels,door_voxels,window_voxels,roof_voxels,structural_integrity,wall_continuity,roof_coverage,door_window_score\n");

    // Summary storage for means
    struct Summary {
        double mean_ns = 0;
        double p99_ns = 0;
        double plausibility = 0;
    };
    std::vector<std::tuple<std::string, std::string, Summary>> summary_rows;

    int total_rows = 0;
    auto t_start = std::chrono::steady_clock::now();

    for (int si = 0; si < 5; ++si) {
        const StrategyFunc& strat = strategies[si];
        for (int bi = 0; bi < 5; ++bi) {
            const BuildingType bt = static_cast<BuildingType>(bi);
            const std::string bt_name(type_name(bt));
            for (int seed_off = 0; seed_off < 5; ++seed_off) {
                const std::uint64_t seed = kSeedBase + seed_off * 0x100000001B3LL;

                // Warmup
                VoxelGrid warmup_g;
                for (int w = 0; w < kWarmupIters; ++w) {
                    warmup_g = strat.func(bt, seed);
                }
                // Prevent dead-code elimination
                if (warmup_g.empty()) std::fprintf(stderr, "warmup empty\n");

                // Measure
                std::vector<double> ns_samples;
                ns_samples.reserve(kMeasureIters);
                double plaus_accum = 0.0;
                for (int it = 0; it < kMeasureIters; ++it) {
                    const auto t0 = std::chrono::steady_clock::now();
                    VoxelGrid g = strat.func(bt, seed);
                    const auto t1 = std::chrono::steady_clock::now();
                    const double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
                    ns_samples.push_back(ns);

                    // Compute plausibility on the last iteration only (for cost amortisation)
                    if (it == kMeasureIters - 1) {
                        GridDesc d;
                        d.sx = spec_of(bt).sx; d.sy = spec_of(bt).sy; d.sz = spec_of(bt).sz;
                        d.stride_y = d.sx;
                        d.stride_z = d.sx * d.sy;
                        const Plausibility p = compute_plausibility(g, d);
                        const double composite_plaus = 0.30 * p.structural_integrity
                                                   + 0.25 * p.wall_continuity
                                                   + 0.25 * p.roof_coverage
                                                   + 0.20 * p.door_window_score;
                        plaus_accum = composite_plaus;
                        // Print final iteration as the row for this config
                        std::fprintf(csv, "%s,%s,%llu,%d,%.1f,%d,%d,%d,%d,%.4f,%.4f,%.4f,%.4f\n",
                                     std::string(strat.name).c_str(),
                                     bt_name.c_str(),
                                     static_cast<unsigned long long>(seed),
                                     it,
                                     ns,
                                     p.solid_voxels,
                                     p.door_voxels,
                                     p.window_voxels,
                                     p.roof_voxels,
                                     p.structural_integrity,
                                     p.wall_continuity,
                                     p.roof_coverage,
                                     p.door_window_score);
                        ++total_rows;
                    }
                }

                const Stats s = ComputeStats(ns_samples);
                Summary sum;
                sum.mean_ns = s.mean;
                sum.p99_ns = s.p99;
                sum.plausibility = plaus_accum;
                summary_rows.emplace_back(std::string(strat.name), bt_name, sum);
            }
        }
    }
    auto t_end = std::chrono::steady_clock::now();
    std::fclose(csv);

    // Write summary CSV
    std::FILE* sum_csv = std::fopen("build/summary_means.csv", "w");
    if (sum_csv) {
        std::fprintf(sum_csv, "strategy,building_type,mean_ns,p99_ns,plausibility\n");
        for (const auto& [sname, btname, sum] : summary_rows) {
            std::fprintf(sum_csv, "%s,%s,%.1f,%.1f,%.4f\n",
                         sname.c_str(), btname.c_str(),
                         sum.mean_ns, sum.p99_ns, sum.plausibility);
        }
        std::fclose(sum_csv);
    }

    // Print top-level summary to stdout
    std::printf("=== 2026-06-22-procedural-voxel-building-generation ===\n");
    std::printf("Total rows: %d. Wall time: %.3f sec.\n\n",
                total_rows,
                std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count() / 1000.0);

    // Aggregate per strategy across all building types + seeds
    std::printf("Per-strategy aggregate (mean of all building types × seeds):\n");
    for (int si = 0; si < 5; ++si) {
        const std::string sname = std::string(strategies[si].name);
        double total_mean = 0.0;
        double total_p99 = 0.0;
        double total_plaus = 0.0;
        int count = 0;
        for (const auto& [sn, bt, sum] : summary_rows) {
            if (sn == sname) {
                total_mean += sum.mean_ns;
                total_p99 += sum.p99_ns;
                total_plaus += sum.plausibility;
                ++count;
            }
        }
        if (count > 0) {
            std::printf("  %-32s mean=%7.1f ns   p99=%9.1f ns   plaus=%.3f\n",
                        sname.c_str(),
                        total_mean / count,
                        total_p99 / count,
                        total_plaus / count);
        }
    }

    std::printf("\nOutput: build/results.csv (per-iter) + build/summary_means.csv (per-config)\n");
    return 0;
}
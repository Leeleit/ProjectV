// prototype/road_bench.cpp
// Standalone C++26 CPU benchmark for 5 procedural voxel road/path/runway
// generation strategies. Clang 22.1.6, -O3 -march=native -std=c++26 -DNDEBUG.
//
// Maps to ProjectV Stage 4.1 World Gen — per-chunk road segment placement
// pass (src/worldgen/RoadPass.cpp::generateRoadSegment). See README.md §9.
//
// Strategy overview:
//   A_StaticFlat             — flat 3xN dirt strip on ground (Minecraft path)
//   B_TemplateComposition    — catalogue of segments/curves/intersections
//   C_GrammarRuleBased       — CGA-shape-style road grammar with weighted choices
//   D_NoiseGuided_Width      — width dithered by 2D noise → ragged edges
//   E_Hybrid_GrammarPlusNoise — C + noise deformation
//
// Road types (scenes):
//   dirt_path, cobble_road, gravel_runway, gravel_motorway, stone_highway.
//
// Plausibility metrics:
//   - connectivity: BFS from start to end (all voxels on road segment connected)
//   - surface_continuity: no gaps in road segment along its polyline
//   - edge_straightness: low stddev of XZ position perpendicular to direction

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
// Stats helpers
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
// Hash PRNG
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
    std::uint32_t uniform_u32(std::uint32_t n) noexcept {
        return static_cast<std::uint32_t>(next() % n);
    }
    float unit() noexcept {
        return static_cast<float>((next() >> 11) * (1.0 / 9007199254740992.0));
    }
};

// ---------------------------------------------------------------------------
// Hash noise 2D — smoothstep value noise
// ---------------------------------------------------------------------------

struct HashNoise2D {
    explicit HashNoise2D(std::uint64_t seed) : seed_(seed) {}
    float sample(float x, float y) const noexcept {
        const int xi = static_cast<int>(std::floor(x));
        const int yi = static_cast<int>(std::floor(y));
        const float xf = x - static_cast<float>(xi);
        const float yf = y - static_cast<float>(yi);
        const float u = xf * xf * (3.0f - 2.0f * xf);
        const float v = yf * yf * (3.0f - 2.0f * yf);
        const float c00 = corner(xi, yi);
        const float c10 = corner(xi + 1, yi);
        const float c01 = corner(xi, yi + 1);
        const float c11 = corner(xi + 1, yi + 1);
        const float x0 = std::lerp(c00, c10, u);
        const float x1 = std::lerp(c01, c11, u);
        return std::lerp(x0, x1, v) * 2.0f - 1.0f;
    }
private:
    std::uint64_t seed_;
    float corner(int x, int y) const noexcept {
        std::uint64_t h = 14695981039346656037ULL;
        h ^= static_cast<std::uint64_t>(x) * 73856093ULL;
        h ^= static_cast<std::uint64_t>(y) * 19349663ULL;
        h ^= seed_;
        h *= 1099511628211ULL;
        return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0x1000000u);
    }
};

// ---------------------------------------------------------------------------
// Voxel grid
// ---------------------------------------------------------------------------

using VoxelGrid = std::vector<std::uint8_t>;

constexpr std::uint8_t AIR     = 0;
constexpr std::uint8_t GROUND  = 1; // base ground layer (unchanged)
constexpr std::uint8_t ROAD    = 2; // road surface material
constexpr std::uint8_t SHOULDER = 3; // road shoulder (gravel)
constexpr std::uint8_t KERB    = 4; // road kerb (urban)
constexpr std::uint8_t LANE    = 5; // lane divider (motorway)

struct GridDesc {
    int sx = 0, sy = 0, sz = 0;
    int stride_y = 0;
    int stride_z = 0;
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

} // namespace bench

// ===========================================================================
// Road type — per-type dimensions, width, surface material
// ===========================================================================

enum class RoadType : int {
    DirtPath = 0,
    CobbleRoad = 1,
    GravelRunway = 2,
    GravelMotorway = 3,
    StoneHighway = 4,
    Count = 5,
};

struct RoadSpec {
    std::string_view name;
    int sx, sy, sz;        // bounding box (sx wide × sy tall × sz long)
    int road_width;        // primary surface width in voxels
    int shoulder_width;    // gravel shoulder (0 for dirt path)
    bool has_lanes;        // true for motorway / highway
    bool has_kerb;         // true for cobble + highway
    int lane_count;        // 1 (path/runway), 2 (road), 4 (motorway), 4 (highway)
};

constexpr std::array<RoadSpec, 5> kRoadSpecs = {{
    {"dirt_path",       5, 3, 24, 3, 0, false, false, 1},
    {"cobble_road",     7, 3, 24, 5, 1, false, true,  1},
    {"gravel_runway",   9, 3, 32, 7, 1, false, false, 1},
    {"gravel_motorway", 13, 3, 32, 11, 1, true,  false, 2},
    {"stone_highway",   11, 3, 32, 9, 1, true,  true,  2},
}};

inline const RoadSpec& road_spec(RoadType t) noexcept {
    return kRoadSpecs[static_cast<int>(t)];
}

inline std::string_view road_name(RoadType t) noexcept {
    return road_spec(t).name;
}

// ===========================================================================
// Plausibility metrics
// ===========================================================================

struct Plausibility {
    double connectivity = 0.0;   // BFS from (0,1,sx/2) reaches all road voxels
    double surface_continuity = 0.0; // % of segments along the road that have road voxel at y=1
    double edge_straightness = 0.0;  // 1.0 = perfectly straight, <1.0 = wobbly
    int road_voxels = 0;
    int shoulder_voxels = 0;
    int kerb_voxels = 0;
    int lane_voxels = 0;
};

// BFS from a starting voxel; returns fraction of solid voxels reachable.
double connectivity_score(const bench::VoxelGrid& g, const bench::GridDesc& d,
                          int start_x, int start_y, int start_z) {
    if (g.empty()) return 0.0;
    const std::uint8_t start_v = bench::get(g, d, start_x, start_y, start_z);
    if (!bench::solid(start_v)) return 0.0;
    std::vector<std::uint8_t> visited(g.size(), 0);
    int total_solid = 0;
    int reached = 0;
    const int dx[6] = {+1, -1, 0, 0, 0, 0};
    const int dy[6] = {0, 0, +1, -1, 0, 0};
    const int dz[6] = {0, 0, 0, 0, +1, -1};
    std::vector<int> stack;
    stack.push_back(static_cast<int>(bench::idx3(start_x, start_y, start_z, d)));
    visited[bench::idx3(start_x, start_y, start_z, d)] = 1;
    while (!stack.empty()) {
        const int cur = stack.back();
        stack.pop_back();
        ++reached;
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
    for (std::uint8_t v : g) if (bench::solid(v)) ++total_solid;
    return (total_solid > 0) ? static_cast<double>(reached) / static_cast<double>(total_solid) : 0.0;
}

Plausibility compute_plausibility(const bench::VoxelGrid& g, const bench::GridDesc& d,
                                  const RoadSpec& s) {
    Plausibility p;
    // Count voxel types
    int roads = 0, shoulders = 0, kerbs = 0, lanes = 0;
    for (int z = 0; z < d.sz; ++z) {
        for (int y = 0; y < d.sy; ++y) {
            for (int x = 0; x < d.sx; ++x) {
                const std::uint8_t v = bench::get(g, d, x, y, z);
                if (v == bench::ROAD) ++roads;
                else if (v == bench::SHOULDER) ++shoulders;
                else if (v == bench::KERB) ++kerbs;
                else if (v == bench::LANE) ++lanes;
            }
        }
    }
    p.road_voxels = roads;
    p.shoulder_voxels = shoulders;
    p.kerb_voxels = kerbs;
    p.lane_voxels = lanes;

    // 1. Connectivity — BFS from center of one end
    const int start_x = d.sx / 2;
    const int start_z = 0;
    p.connectivity = connectivity_score(g, d, start_x, 1, start_z);

    // 2. Surface continuity — % of (z, x=center) positions that have road voxel at y=1
    int z_steps = 0;
    int z_road_present = 0;
    for (int z = 0; z < d.sz; ++z) {
        ++z_steps;
        const std::uint8_t v = bench::get(g, d, start_x, 1, z);
        if (v == bench::ROAD) ++z_road_present;
    }
    p.surface_continuity = (z_steps > 0)
        ? static_cast<double>(z_road_present) / static_cast<double>(z_steps)
        : 0.0;

    // 3. Edge straightness — stddev of X position where road surface intersects y=1, lower is straighter
    std::vector<int> x_positions;
    for (int z = 0; z < d.sz; ++z) {
        for (int x = 0; x < d.sx; ++x) {
            if (bench::get(g, d, x, 1, z) == bench::ROAD) {
                x_positions.push_back(x);
                break;
            }
        }
    }
    if (x_positions.size() >= 2) {
        const double mean_x = std::accumulate(x_positions.begin(), x_positions.end(), 0.0) / x_positions.size();
        double var = 0.0;
        for (int xv : x_positions) var += (static_cast<double>(xv) - mean_x) * (static_cast<double>(xv) - mean_x);
        const double stddev_x = std::sqrt(var / x_positions.size());
        // Perfectly straight = stddev = 0; max stddev for s.sx = sx-1
        const double max_stddev = static_cast<double>(s.sx) / 2.0;
        p.edge_straightness = std::max(0.0, 1.0 - stddev_x / std::max(1.0, max_stddev));
    } else {
        p.edge_straightness = 0.0;
    }

    return p;
}

// ===========================================================================
// Strategy A: StaticFlat
//   Flat 3xN dirt strip on ground (Minecraft village path).
//   Trivial: just iterate over a rectangle and place ROAD voxels.
//   Hypothesis: <500 ns/segment.
// ===========================================================================
namespace strategies {

using namespace bench;

VoxelGrid strategy_A(RoadType t, std::uint64_t /*seed*/) {
    const RoadSpec& s = road_spec(t);
    GridDesc d;
    d.sx = s.sx; d.sy = s.sy; d.sz = s.sz;
    d.stride_y = d.sx;
    d.stride_z = d.sx * d.sy;
    VoxelGrid g(static_cast<std::size_t>(d.sx) * d.sy * d.sz, AIR);

    // Ground layer at y=0 (assume terrain)
    for (int z = 0; z < d.sz; ++z) {
        for (int x = 0; x < d.sx; ++x) {
            g[idx3(x, 0, z, d)] = GROUND;
        }
    }

    // Flat road surface: rows centered on sx/2, span width
    const int cx = d.sx / 2;
    const int hw = s.road_width / 2;
    for (int z = 0; z < d.sz; ++z) {
        for (int dx = -hw; dx <= hw; ++dx) {
            const int x = cx + dx;
            if (x < 0 || x >= d.sx) continue;
            g[idx3(x, 1, z, d)] = ROAD;
        }
    }

    return g;
}

} // namespace strategies

// ===========================================================================
// Strategy B: TemplateComposition
//   Catalogue of segment primitives (straight, curve, intersection)
//   composed deterministically along a polyline.
//   Inspired by Minecraft Jigsaw Block Template Pool (sources.md #6) and
//   Luanti schematics (#9), per closed `procedural-voxel-building-generation`
//   B_TemplateComposition validated pattern.
// ===========================================================================
namespace strategies {

using namespace bench;

// Primitive: straight segment along +Z axis, width w, material m
static void prim_straight(VoxelGrid& g, GridDesc& d, int cx, int hw, int z_start, int z_end,
                          std::uint8_t m) {
    for (int z = z_start; z <= z_end; ++z) {
        for (int dx = -hw; dx <= hw; ++dx) {
            const int x = cx + dx;
            if (x < 0 || x >= d.sx) continue;
            put(g, d, x, 1, z, m);
        }
    }
}

// Primitive: arc curve (left/right) using simple sin-based bend
static void prim_curve(VoxelGrid& g, GridDesc& d, int cx_start, int hw, int z_start, int z_len,
                       int bend_x, std::uint8_t m) {
    for (int i = 0; i < z_len; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(z_len);
        const float phase = t * 3.14159f / 2.0f;
        const int dx_center = static_cast<int>(bend_x * std::sin(phase));
        const int cx = cx_start + dx_center;
        for (int dx = -hw; dx <= hw; ++dx) {
            const int x = cx + dx;
            if (x < 0 || x >= d.sx) continue;
            put(g, d, x, 1, z_start + i, m);
        }
    }
}

// Primitive: T-intersection (perpendicular road crossing at this z)
static void prim_t_intersection(VoxelGrid& g, GridDesc& d, int cx, int hw_main, int hw_cross,
                                 int z, std::uint8_t m) {
    for (int dz = -hw_cross; dz <= hw_cross; ++dz) {
        const int zc = z + dz;
        if (zc < 0 || zc >= d.sz) continue;
        for (int dx = -hw_main - hw_cross; dx <= hw_main + hw_cross; ++dx) {
            const int x = cx + dx;
            if (x < 0 || x >= d.sx) continue;
            put(g, d, x, 1, zc, m);
        }
    }
}

VoxelGrid strategy_B(RoadType t, std::uint64_t seed) {
    const RoadSpec& s = road_spec(t);
    GridDesc d;
    d.sx = s.sx; d.sy = s.sy; d.sz = s.sz;
    d.stride_y = d.sx;
    d.stride_z = d.sx * d.sy;
    VoxelGrid g(static_cast<std::size_t>(d.sx) * d.sy * d.sz, AIR);
    SplitMix64 rng(seed);

    // Ground layer
    for (int z = 0; z < d.sz; ++z) {
        for (int x = 0; x < d.sx; ++x) {
            put(g, d, x, 0, z, GROUND);
        }
    }

    const int cx = d.sx / 2;
    const int hw = s.road_width / 2;

    // Compose segments along polyline: straight 40% + curve 30% + straight 30%
    int z_cursor = 0;
    const int total_z = d.sz;
    const int seg1 = total_z * 2 / 5; // 40% straight
    const int seg2 = total_z * 3 / 10; // 30% curve
    // const int seg3 = total_z * 3 / 10; // remaining

    // Segment 1: straight
    prim_straight(g, d, cx, hw, z_cursor, z_cursor + seg1 - 1, ROAD);
    z_cursor += seg1;

    // Segment 2: curve (alternating left/right based on seed)
    const int bend_dir = (rng.next() & 1) ? 1 : -1;
    const int bend_x = std::min(s.sx / 4, 3) * bend_dir;
    prim_curve(g, d, cx, hw, z_cursor, seg2, bend_x, ROAD);
    z_cursor += seg2;

    // Segment 3: straight (continue from curve end)
    if (z_cursor < total_z) {
        const int cx_after = cx + bend_x; // simplified — assume curve ended at max bend
        prim_straight(g, d, cx_after, hw, z_cursor, total_z - 1, ROAD);
    }

    // Optional: add shoulder, kerb, lanes for non-dirt types
    if (s.shoulder_width > 0) {
        // Shoulder = 1 voxel wide on each side
        for (int z = 0; z < d.sz; ++z) {
            for (int dx : {-hw - 1, hw + 1}) {
                const int x = cx + dx;
                if (x < 0 || x >= d.sx) continue;
                put(g, d, x, 1, z, SHOULDER);
            }
        }
    }
    if (s.has_kerb) {
        for (int z = 0; z < d.sz; ++z) {
            for (int dx : {-hw, hw}) {
                const int x = cx + dx;
                if (x < 0 || x >= d.sx) continue;
                put(g, d, x, 1, z, KERB);
            }
        }
    }
    if (s.has_lanes) {
        // Lane divider in center
        for (int z = 0; z < d.sz; ++z) {
            put(g, d, cx, 1, z, LANE);
        }
    }

    return g;
}

} // namespace strategies

// ===========================================================================
// Strategy C: GrammarRuleBased
//   CGA-shape-style road grammar with weighted choices:
//     road -> straight{N=2..5} -> curve{left|right|straight|T|Y} -> straight{N} -> ...
//   Inspired by Wonka 2003 split grammar + Müller 2006 CGA shape.
// ===========================================================================
namespace strategies {

using namespace bench;

enum class CurveType : int { Straight, Left, Right, T, Y };

struct GrammarChoice {
    CurveType type;
    int len;       // length in voxels
    int bend;      // bend offset for curves
};

static CurveType pick_curve(SplitMix64& rng) {
    const int r = rng.uniform_u32(100);
    if (r < 60) return CurveType::Straight;
    if (r < 80) return CurveType::Left;
    if (r < 95) return CurveType::Right;
    if (r < 99) return CurveType::T;
    return CurveType::Y;
}

static int pick_segment_length(SplitMix64& rng) {
    // Length: 3-8 voxels
    return 3 + rng.uniform_u32(6);
}

static void apply_segment(VoxelGrid& g, GridDesc& d, int cx, int hw,
                          int z_start, int z_end, CurveType type, int bend_x,
                          std::uint8_t m) {
    if (type == CurveType::Straight) {
        prim_straight(g, d, cx, hw, z_start, z_end, m);
    } else if (type == CurveType::Left || type == CurveType::Right) {
        const int dir = (type == CurveType::Left) ? -1 : +1;
        for (int i = 0; i < (z_end - z_start + 1); ++i) {
            const float t = static_cast<float>(i) / std::max(1, z_end - z_start);
            const float phase = t * 3.14159f / 2.0f;
            const int dx_center = static_cast<int>(dir * bend_x * std::sin(phase));
            const int cx_local = cx + dx_center;
            for (int dx = -hw; dx <= hw; ++dx) {
                const int x = cx_local + dx;
                if (x < 0 || x >= d.sx) continue;
                put(g, d, x, 1, z_start + i, m);
            }
        }
    } else if (type == CurveType::T) {
        // Perpendicular cross at midpoint
        const int z_mid = (z_start + z_end) / 2;
        prim_t_intersection(g, d, cx, hw, hw, z_mid, m);
        // Main road
        prim_straight(g, d, cx, hw, z_start, z_end, m);
    } else if (type == CurveType::Y) {
        // Branch point at midpoint with two diverging roads
        const int z_mid = (z_start + z_end) / 2;
        // Main road to midpoint
        prim_straight(g, d, cx, hw, z_start, z_mid, m);
        // Left branch
        for (int i = 0; i < (z_end - z_mid); ++i) {
            const float t = static_cast<float>(i) / std::max(1, z_end - z_mid);
            const int dx_left = static_cast<int>(bend_x * t);
            for (int dx = -hw; dx <= hw; ++dx) {
                const int x = cx - dx_left + dx;
                if (x < 0 || x >= d.sx) continue;
                put(g, d, x, 1, z_mid + i, m);
            }
        }
        // Right branch
        for (int i = 0; i < (z_end - z_mid); ++i) {
            const float t = static_cast<float>(i) / std::max(1, z_end - z_mid);
            const int dx_right = static_cast<int>(bend_x * t);
            for (int dx = -hw; dx <= hw; ++dx) {
                const int x = cx + dx_right + dx;
                if (x < 0 || x >= d.sx) continue;
                put(g, d, x, 1, z_mid + i, m);
            }
        }
    }
}

VoxelGrid strategy_C(RoadType t, std::uint64_t seed) {
    const RoadSpec& s = road_spec(t);
    GridDesc d;
    d.sx = s.sx; d.sy = s.sy; d.sz = s.sz;
    d.stride_y = d.sx;
    d.stride_z = d.sx * d.sy;
    VoxelGrid g(static_cast<std::size_t>(d.sx) * d.sy * d.sz, AIR);
    SplitMix64 rng(seed);

    // Ground
    for (int z = 0; z < d.sz; ++z) {
        for (int x = 0; x < d.sx; ++x) {
            put(g, d, x, 0, z, GROUND);
        }
    }

    const int cx = d.sx / 2;
    const int hw = s.road_width / 2;
    const int bend_x = std::min(s.sx / 4, 3);

    // Grammar rule: road -> straight{N} -> curve -> straight{N} -> ...
    int z_cursor = 0;
    int segments = 0;
    while (z_cursor < d.sz - 4 && segments < 8) {
        const CurveType type = pick_curve(rng);
        const int seg_len = std::min(pick_segment_length(rng), d.sz - z_cursor);
        apply_segment(g, d, cx, hw, z_cursor, z_cursor + seg_len - 1, type, bend_x, ROAD);
        z_cursor += seg_len;
        ++segments;
    }
    // Fill remaining with straight
    if (z_cursor < d.sz) {
        prim_straight(g, d, cx, hw, z_cursor, d.sz - 1, ROAD);
    }

    // Type-specific extras (shoulders, kerbs, lanes)
    if (s.shoulder_width > 0) {
        for (int z = 0; z < d.sz; ++z) {
            for (int dx : {-hw - 1, hw + 1}) {
                const int x = cx + dx;
                if (x < 0 || x >= d.sx) continue;
                put(g, d, x, 1, z, SHOULDER);
            }
        }
    }
    if (s.has_kerb) {
        for (int z = 0; z < d.sz; ++z) {
            for (int dx : {-hw, hw}) {
                const int x = cx + dx;
                if (x < 0 || x >= d.sx) continue;
                put(g, d, x, 1, z, KERB);
            }
        }
    }
    if (s.has_lanes) {
        for (int z = 0; z < d.sz; ++z) {
            put(g, d, cx, 1, z, LANE);
        }
    }

    return g;
}

} // namespace strategies

// ===========================================================================
// Strategy D: NoiseGuided_Width
//   Width dithered by 2D noise → ragged edges (natural-looking path).
//   Inspired by Kelly & McCabe 2007 floor-plan extrusion (closed
//   procedural-voxel-building-generation D_NoiseGuided_FloorPlan pattern).
// ===========================================================================
namespace strategies {

using namespace bench;

VoxelGrid strategy_D(RoadType t, std::uint64_t seed) {
    const RoadSpec& s = road_spec(t);
    GridDesc d;
    d.sx = s.sx; d.sy = s.sy; d.sz = s.sz;
    d.stride_y = d.sx;
    d.stride_z = d.sx * d.sy;
    VoxelGrid g(static_cast<std::size_t>(d.sx) * d.sy * d.sz, AIR);
    HashNoise2D noise(seed);

    // Ground
    for (int z = 0; z < d.sz; ++z) {
        for (int x = 0; x < d.sx; ++x) {
            put(g, d, x, 0, z, GROUND);
        }
    }

    const int cx = d.sx / 2;
    const int hw = s.road_width / 2;
    const float scale = 0.4f;

    // For each z, compute dithered width using noise
    for (int z = 0; z < d.sz; ++z) {
        // 2 noise samples for left/right edge variation
        const float n_left = noise.sample(static_cast<float>(z) * scale, 0.0f);
        const float n_right = noise.sample(static_cast<float>(z) * scale, 1.0f);
        const int dw_left = static_cast<int>(n_left * 1.5f);  // -1..+1
        const int dw_right = static_cast<int>(n_right * 1.5f);
        const int hw_eff_left = hw + dw_left;
        const int hw_eff_right = hw + dw_right;

        // Place ROAD voxels from -hw_eff_left to +hw_eff_right
        for (int dx = -hw_eff_left; dx <= hw_eff_right; ++dx) {
            const int x = cx + dx;
            if (x < 0 || x >= d.sx) continue;
            // Center is solid; edges are SHOULDER (ragged)
            if (std::abs(dx) <= hw) {
                put(g, d, x, 1, z, ROAD);
            } else {
                put(g, d, x, 1, z, SHOULDER);
            }
        }
    }

    return g;
}

} // namespace strategies

// ===========================================================================
// Strategy E: Hybrid_GrammarPlusNoise
//   = Strategy C + per-instance noise deformation.
//   Same architectural pattern as closed procedural-voxel-building-generation
//   E_Hybrid_GrammarPlusNoise.
// ===========================================================================
namespace strategies {

using namespace bench;

VoxelGrid strategy_E(RoadType t, std::uint64_t seed) {
    // Step 1: run C
    VoxelGrid g = strategy_C(t, seed);

    const RoadSpec& s = road_spec(t);
    GridDesc d;
    d.sx = s.sx; d.sy = s.sy; d.sz = s.sz;
    d.stride_y = d.sx;
    d.stride_z = d.sx * d.sy;

    // Step 2: noise deformation — dither edges, add occasional scatter
    HashNoise2D noise(seed ^ 0xBADCAFEULL);
    SplitMix64 rng(seed);

    const int cx = d.sx / 2;
    const int hw = s.road_width / 2;
    const float scale = 0.5f;

    std::vector<std::uint8_t> deformed(g.size(), 0);
    for (int z = 0; z < d.sz; ++z) {
        const float n_left = noise.sample(static_cast<float>(z) * scale, 0.0f);
        const float n_right = noise.sample(static_cast<float>(z) * scale, 1.0f);
        const int dw_left = static_cast<int>(n_left * 1.0f);
        const int dw_right = static_cast<int>(n_right * 1.0f);
        const int hw_eff_left = hw + dw_left;
        const int hw_eff_right = hw + dw_right;

        for (int dx = -hw_eff_left - 1; dx <= hw_eff_right + 1; ++dx) {
            const int x = cx + dx;
            if (x < 0 || x >= d.sx) continue;
            const std::uint8_t v = get(g, d, x, 1, z);
            if (v == 0) continue; // air, skip
            // Convert outermost road to shoulder (ragged)
            if (std::abs(dx) > hw) {
                deformed[idx3(x, 1, z, d)] = SHOULDER;
            } else {
                deformed[idx3(x, 1, z, d)] = v;
            }
        }
    }
    // Copy deformed back
    std::memcpy(g.data(), deformed.data(), deformed.size());

    return g;
}

} // namespace strategies

// ===========================================================================
// Main harness — benchmark all 5 strategies x 5 road types x 5 seeds
// ===========================================================================

using ::RoadType;
using bench::VoxelGrid;

struct StrategyFunc {
    std::string_view name;
    VoxelGrid (*func)(RoadType, std::uint64_t);
};

int main(int /*argc*/, char** /*argv*/) {
    using namespace bench;

    const std::array<StrategyFunc, 5> strategies = {{
        {"A_StaticFlat",            &strategies::strategy_A},
        {"B_TemplateComposition",   &strategies::strategy_B},
        {"C_GrammarRuleBased",      &strategies::strategy_C},
        {"D_NoiseGuided_Width",     &strategies::strategy_D},
        {"E_Hybrid_GrammarPlusNoise", &strategies::strategy_E},
    }};

    constexpr int kWarmupIters = 10;
    constexpr int kMeasureIters = 1000;
    constexpr std::uint64_t kSeedBase = 0xC0FFEE;

    std::FILE* csv = std::fopen("build/results.csv", "w");
    if (!csv) {
        std::fprintf(stderr, "Failed to open build/results.csv\n");
        return 1;
    }
    std::fprintf(csv, "strategy,road_type,seed,iter,ns_per_call,road_voxels,shoulder_voxels,kerb_voxels,lane_voxels,connectivity,surface_continuity,edge_straightness\n");

    struct Summary { double mean_ns; double p99_ns; double plausibility; };
    std::vector<std::tuple<std::string, std::string, Summary>> summary_rows;

    int total_rows = 0;
    auto t_start = std::chrono::steady_clock::now();

    for (int si = 0; si < 5; ++si) {
        const StrategyFunc& strat = strategies[si];
        for (int bi = 0; bi < 5; ++bi) {
            const RoadType rt = static_cast<RoadType>(bi);
            const std::string rt_name(road_name(rt));
            for (int seed_off = 0; seed_off < 5; ++seed_off) {
                const std::uint64_t seed = kSeedBase + seed_off * 0x100000001B3LL;

                VoxelGrid warmup_g;
                for (int w = 0; w < kWarmupIters; ++w) {
                    warmup_g = strat.func(rt, seed);
                }
                if (warmup_g.empty()) std::fprintf(stderr, "warmup empty\n");

                std::vector<double> ns_samples;
                ns_samples.reserve(kMeasureIters);
                double plaus_accum = 0.0;
                for (int it = 0; it < kMeasureIters; ++it) {
                    const auto t0 = std::chrono::steady_clock::now();
                    VoxelGrid g = strat.func(rt, seed);
                    const auto t1 = std::chrono::steady_clock::now();
                    const double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
                    ns_samples.push_back(ns);

                    if (it == kMeasureIters - 1) {
                        GridDesc dd;
                        const RoadSpec& rs = road_spec(rt);
                        dd.sx = rs.sx; dd.sy = rs.sy; dd.sz = rs.sz;
                        dd.stride_y = dd.sx;
                        dd.stride_z = dd.sx * dd.sy;
                        const Plausibility p = compute_plausibility(g, dd, rs);
                        const double composite_plaus = 0.40 * p.connectivity
                                                   + 0.35 * p.surface_continuity
                                                   + 0.25 * p.edge_straightness;
                        plaus_accum = composite_plaus;
                        std::fprintf(csv, "%s,%s,%llu,%d,%.1f,%d,%d,%d,%d,%.4f,%.4f,%.4f\n",
                                     std::string(strat.name).c_str(),
                                     rt_name.c_str(),
                                     static_cast<unsigned long long>(seed),
                                     it, ns,
                                     p.road_voxels, p.shoulder_voxels, p.kerb_voxels, p.lane_voxels,
                                     p.connectivity, p.surface_continuity, p.edge_straightness);
                        ++total_rows;
                    }
                }

                const Stats s = ComputeStats(ns_samples);
                Summary sum;
                sum.mean_ns = s.mean;
                sum.p99_ns = s.p99;
                sum.plausibility = plaus_accum;
                summary_rows.emplace_back(std::string(strat.name), rt_name, sum);
            }
        }
    }
    auto t_end = std::chrono::steady_clock::now();
    std::fclose(csv);

    std::FILE* sum_csv = std::fopen("build/summary_means.csv", "w");
    if (sum_csv) {
        std::fprintf(sum_csv, "strategy,road_type,mean_ns,p99_ns,plausibility\n");
        for (const auto& [sname, rtname, sum] : summary_rows) {
            std::fprintf(sum_csv, "%s,%s,%.1f,%.1f,%.4f\n",
                         sname.c_str(), rtname.c_str(),
                         sum.mean_ns, sum.p99_ns, sum.plausibility);
        }
        std::fclose(sum_csv);
    }

    std::printf("=== 2026-06-22-procedural-voxel-road-path-generation ===\n");
    std::printf("Total rows: %d. Wall time: %.3f sec.\n\n",
                total_rows,
                std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count() / 1000.0);

    std::printf("Per-strategy aggregate (mean of all road types × seeds):\n");
    for (int si = 0; si < 5; ++si) {
        const std::string sname = std::string(strategies[si].name);
        double total_mean = 0.0, total_p99 = 0.0, total_plaus = 0.0;
        int count = 0;
        for (const auto& [sn, rt, sum] : summary_rows) {
            if (sn == sname) {
                total_mean += sum.mean_ns;
                total_p99 += sum.p99_ns;
                total_plaus += sum.plausibility;
                ++count;
            }
        }
        if (count > 0) {
            std::printf("  %-32s mean=%8.1f ns   p99=%10.1f ns   plaus=%.3f\n",
                        sname.c_str(),
                        total_mean / count,
                        total_p99 / count,
                        total_plaus / count);
        }
    }

    std::printf("\nOutput: build/results.csv + build/summary_means.csv\n");
    return 0;
}
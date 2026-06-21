// 2026-06-21-explosion-crater-terrain-deformation — sphere-SDF carve on 8^3 voxel chunks
// Standalone C++26 CPU prototype. See experiments/2026-06-21-explosion-crater-terrain-deformation/
//
// Inherits scene + Grid + Stats harness from closed 2026-06-21-chunk-damage-fracture-model
// (cross-ref experiments/2026-06-21-chunk-damage-fracture-model/prototype/fracture_bench.cpp).
//
// 5 strategies: A_NaivePerVoxel, B_AABBPreFilter, C_BlockBased2x, D_BlockBased4x, E_RasterizedSphereMarch.
// 5 scenes x 5 seeds x 4 radii x 3 positions = 300 configs x 1000 iter + 10 warmup = 300,000 measurements.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <random>
#include <span>
#include <string_view>
#include <vector>

static constexpr int kChunkSize = 8;
static constexpr int kChunkVoxels = kChunkSize * kChunkSize * kChunkSize;

// ---- metrics accumulator ----
struct Stats {
    double mean = 0, median = 0, p95 = 0, p99 = 0, stddev = 0, min = 1e99, max = -1e99;
    uint64_t n = 0;
    void accumulate(double v) { vals.push_back(v); }
    void finalize() {
        if (vals.empty()) return;
        n = vals.size();
        std::ranges::sort(vals);
        min = vals.front(); max = vals.back();
        mean = std::accumulate(vals.begin(), vals.end(), 0.0) / n;
        median = vals[n / 2];
        p95 = vals[(size_t)(n * 0.95)];
        p99 = vals[(size_t)(n * 0.99)];
        double sq = 0;
        for (auto v : vals) sq += (v - mean) * (v - mean);
        stddev = std::sqrt(sq / n);
    }
    void reset() { *this = Stats{}; }
private:
    std::vector<double> vals;
};

// ---- voxel grid helpers ----
using Grid = std::array<uint8_t, kChunkVoxels>;

static int idx(int x, int y, int z) { return (y * kChunkSize + z) * kChunkSize + x; }

// ---- scene generators (inherited from fracture_bench.cpp scene set + thin_wall new) ----
static void gen_uniform_floor(Grid& g) { g.fill(1); }

static void gen_forest_floor(Grid& g) {
    std::mt19937 rng(7);
    for (int i = 0; i < kChunkVoxels; i++)
        g[i] = (rng() % 10 < 7) ? uint8_t(1) : uint8_t(0);
    for (int x = 0; x < kChunkSize; x++)
        for (int z = 0; z < kChunkSize; z++)
            g[idx(x, 0, z)] = 1;
}

static void gen_cave_stress(Grid& g) {
    g.fill(1);
    for (int x = 1; x < 6; x++) g[idx(x, 4, 4)] = 0;
    for (int y = 1; y < 6; y++) g[idx(5, y, 4)] = 0;
    for (int z = 1; z < 6; z++) g[idx(5, 4, z)] = 0;
    g[idx(2,2,2)] = 0; g[idx(2,2,3)] = 0; g[idx(2,3,2)] = 0;
    g[idx(6,6,6)] = 0; g[idx(6,6,5)] = 0; g[idx(6,5,6)] = 0;
}

static void gen_mixed_biome(Grid& g) {
    std::mt19937 rng(42);
    for (int i = 0; i < kChunkVoxels; i++) {
        uint8_t m = (rng() % 100);
        g[i] = (m < 40) ? 1 : (m < 55) ? 2 : (m < 70) ? 3 : (m < 80) ? 4 : 0;
    }
    for (int x = 0; x < kChunkSize; x++)
        for (int z = 0; z < kChunkSize; z++)
            g[idx(x, 0, z)] = 1;
}

static void gen_thin_wall(Grid& g) {
    g.fill(0);
    // 1-voxel floor + cross walls at y=4 — common explosion test (sphere should not punch through walls)
    for (int x = 0; x < kChunkSize; x++) g[idx(x, 0, 4)] = 1;  // floor at z=4
    for (int x = 0; x < kChunkSize; x++) g[idx(x, 4, 0)] = 1;  // wall at z=0
    for (int x = 0; x < kChunkSize; x++) g[idx(x, 4, 7)] = 1;  // wall at z=7
    for (int y = 0; y < kChunkSize; y++) g[idx(0, y, 4)] = 1;  // wall at x=0
    for (int y = 0; y < kChunkSize; y++) g[idx(7, y, 4)] = 1;  // wall at x=7
}

using SceneGen = void (*)(Grid&);

struct SceneInfo { std::string_view name; SceneGen gen; };
static constexpr std::array<SceneInfo, 5> kScenes = {{
    {"uniform_floor",  &gen_uniform_floor},
    {"forest_floor",   &gen_forest_floor},
    {"cave_stress",    &gen_cave_stress},
    {"mixed_biome",    &gen_mixed_biome},
    {"thin_wall",      &gen_thin_wall},
}};

// ---- reference sphere-SDF carve (baseline) ----
// Voxel center = (x+0.5, y+0.5, z+0.5). Carve if dist(center, explosion_origin) < radius.
static void carve_reference(Grid& g, float cx, float cy, float cz, float radius) {
    const float r2 = radius * radius;
    for (int x = 0; x < kChunkSize; x++)
        for (int y = 0; y < kChunkSize; y++)
            for (int z = 0; z < kChunkSize; z++) {
                if (g[idx(x, y, z)] == 0) continue;
                float dx = float(x) + 0.5f - cx;
                float dy = float(y) + 0.5f - cy;
                float dz = float(z) + 0.5f - cz;
                if (dx*dx + dy*dy + dz*dz < r2) g[idx(x, y, z)] = 0;
            }
}

// ========== STRATEGIES ==========

// A_NaivePerVoxel: 3 nested loops, no spatial prefilter. Baseline.
static void strategy_A(Grid& g, float cx, float cy, float cz, float radius) {
    carve_reference(g, cx, cy, cz, radius);
}

// B_AABBPreFilter: compute chunk AABB (entire 8^3 box); if sphere is far → skip entirely.
// Otherwise fall through to per-voxel. (This is a weak prefilter — it only helps when sphere
// is outside the chunk; in ProjectV, the explosion usually overlaps the chunk.)
static bool sphere_intersects_aabb(float cx, float cy, float cz, float radius,
                                   float ax0, float ay0, float az0,
                                   float ax1, float ay1, float az1) {
    float dx = std::max({ax0 - cx, 0.0f, cx - ax1});
    float dy = std::max({ay0 - cy, 0.0f, cy - ay1});
    float dz = std::max({az0 - cz, 0.0f, cz - az1});
    return (dx*dx + dy*dy + dz*dz) < radius * radius;
}

static void strategy_B(Grid& g, float cx, float cy, float cz, float radius) {
    // Chunk AABB = [0, 8]^3 in voxel space
    if (!sphere_intersects_aabb(cx, cy, cz, radius, 0.0f, 0.0f, 0.0f,
                                float(kChunkSize), float(kChunkSize), float(kChunkSize))) {
        return;  // sphere doesn't touch this chunk
    }
    carve_reference(g, cx, cy, cz, radius);
}

// C_BlockBased2x: divide 8^3 into 2x2x2 = 64 sub-blocks (each 4x4x4 = 64 voxels).
// Test sub-block AABB: if fully inside sphere → bulk set 0; if outside → skip;
// if partial → per-voxel fallback.
static bool aabb_fully_inside_sphere(float cx, float cy, float cz, float radius,
                                     float ax0, float ay0, float az0,
                                     float ax1, float ay1, float az1) {
    // All 8 corners of AABB must be inside sphere
    for (int i = 0; i < 8; i++) {
        float x = (i & 1) ? ax1 : ax0;
        float y = (i & 2) ? ay1 : ay0;
        float z = (i & 4) ? az1 : az0;
        float dx = x - cx, dy = y - cy, dz = z - cz;
        if (dx*dx + dy*dy + dz*dz >= radius * radius) return false;
    }
    return true;
}

static void strategy_C(Grid& g, float cx, float cy, float cz, float radius) {
    // 2x2x2 sub-blocks (each 4x4x4 voxels)
    for (int bx = 0; bx < 2; bx++)
        for (int by = 0; by < 2; by++)
            for (int bz = 0; bz < 2; bz++) {
                float ax0 = float(bx * 4), ay0 = float(by * 4), az0 = float(bz * 4);
                float ax1 = float((bx + 1) * 4), ay1 = float((by + 1) * 4), az1 = float((bz + 1) * 4);
                if (aabb_fully_inside_sphere(cx, cy, cz, radius, ax0, ay0, az0, ax1, ay1, az1)) {
                    for (int x = bx * 4; x < (bx + 1) * 4; x++)
                        for (int y = by * 4; y < (by + 1) * 4; y++)
                            for (int z = bz * 4; z < (bz + 1) * 4; z++)
                                g[idx(x, y, z)] = 0;
                } else if (sphere_intersects_aabb(cx, cy, cz, radius, ax0, ay0, az0, ax1, ay1, az1)) {
                    for (int x = bx * 4; x < (bx + 1) * 4; x++)
                        for (int y = by * 4; y < (by + 1) * 4; y++)
                            for (int z = bz * 4; z < (bz + 1) * 4; z++) {
                                if (g[idx(x, y, z)] == 0) continue;
                                float dx = float(x) + 0.5f - cx;
                                float dy = float(y) + 0.5f - cy;
                                float dz = float(z) + 0.5f - cz;
                                if (dx*dx + dy*dy + dz*dz < radius * radius)
                                    g[idx(x, y, z)] = 0;
                            }
                }
                // else: sub-block outside sphere → skip
            }
}

// D_BlockBased4x: divide 8^3 into 4x4x4 = 64 sub-blocks (each 2x2x2 = 8 voxels).
// More fine-grained coarse rejection. (Smaller blocks → less "fully inside" wins, but cheaper per-block test.)
static void strategy_D(Grid& g, float cx, float cy, float cz, float radius) {
    for (int bx = 0; bx < 4; bx++)
        for (int by = 0; by < 4; by++)
            for (int bz = 0; bz < 4; bz++) {
                float ax0 = float(bx * 2), ay0 = float(by * 2), az0 = float(bz * 2);
                float ax1 = float((bx + 1) * 2), ay1 = float((by + 1) * 2), az1 = float((bz + 1) * 2);
                if (aabb_fully_inside_sphere(cx, cy, cz, radius, ax0, ay0, az0, ax1, ay1, az1)) {
                    for (int x = bx * 2; x < (bx + 1) * 2; x++)
                        for (int y = by * 2; y < (by + 1) * 2; y++)
                            for (int z = bz * 2; z < (bz + 1) * 2; z++)
                                g[idx(x, y, z)] = 0;
                } else if (sphere_intersects_aabb(cx, cy, cz, radius, ax0, ay0, az0, ax1, ay1, az1)) {
                    for (int x = bx * 2; x < (bx + 1) * 2; x++)
                        for (int y = by * 2; y < (by + 1) * 2; y++)
                            for (int z = bz * 2; z < (bz + 1) * 2; z++) {
                                if (g[idx(x, y, z)] == 0) continue;
                                float dx = float(x) + 0.5f - cx;
                                float dy = float(y) + 0.5f - cy;
                                float dz = float(z) + 0.5f - cz;
                                if (dx*dx + dy*dy + dz*dz < radius * radius)
                                    g[idx(x, y, z)] = 0;
                            }
                }
            }
}

// E_RasterizedSphereMarch: for each (x, z) column, march through y. For each y sample (8 along the column),
// compute distance to sphere. Carve if any sample within radius. Equivalent to per-voxel but organized
// for cache locality along the column. (Faster than naive in practice due to better column access pattern.)
static void strategy_E(Grid& g, float cx, float cy, float cz, float radius) {
    const float r2 = radius * radius;
    for (int x = 0; x < kChunkSize; x++) {
        const float dx_const = float(x) + 0.5f - cx;
        const float xd2 = dx_const * dx_const;
        for (int z = 0; z < kChunkSize; z++) {
            const float dz_const = float(z) + 0.5f - cz;
            const float zd2 = dz_const * dz_const;
            // xz-distance squared to sphere center
            const float xzd2 = xd2 + zd2;
            // Max y where sphere reaches this column: if xzd2 > r2, sphere never reaches this column
            if (xzd2 > r2) continue;
            // 8 samples per column
            for (int y = 0; y < kChunkSize; y++) {
                if (g[idx(x, y, z)] == 0) continue;
                float dy = float(y) + 0.5f - cy;
                if (xzd2 + dy*dy < r2)
                    g[idx(x, y, z)] = 0;
            }
        }
    }
}

using Strategy = void (*)(Grid&, float, float, float, float);

struct StratInfo { std::string_view name; Strategy fn; };
static constexpr std::array<StratInfo, 5> kStrategies = {{
    {"A_NaivePerVoxel",        &strategy_A},
    {"B_AABBPreFilter",        &strategy_B},
    {"C_BlockBased2x",         &strategy_C},
    {"D_BlockBased4x",         &strategy_D},
    {"E_RasterizedSphereMarch",&strategy_E},
}};

// ---- explosion positions ----
struct PosInfo { std::string_view name; float x, y, z; };
static constexpr std::array<PosInfo, 3> kPositions = {{
    {"corner", 0.5f, 0.5f, 0.5f},
    {"center", 4.0f, 4.0f, 4.0f},
    {"edge",   1.0f, 4.0f, 7.0f},
}};

static constexpr std::array<float, 4> kRadii = {1.5f, 2.5f, 4.0f, 6.0f};

// ---- validation: compare strategy output to reference ----
static int validate_against(const Grid& a, const Grid& b) {
    int mismatches = 0;
    for (int i = 0; i < kChunkVoxels; i++) if (a[i] != b[i]) mismatches++;
    return mismatches;
}

// ---- timing utility ----
struct Timer {
    using clock = std::chrono::steady_clock;
    clock::time_point t0;
    void start() { t0 = clock::now(); }
    double stop_us() {
        return std::chrono::duration<double, std::micro>(clock::now() - t0).count();
    }
};

int main() {
    std::printf("=== 2026-06-21-explosion-crater-terrain-deformation ===\n");
    std::printf("5 strategies x 5 scenes x 5 seeds x 4 radii x 3 positions = 300 configs\n");
    std::printf("1000 iter + 10 warmup per config = 300,000 main measurements\n\n");

    // CSV header
    std::printf("strategy,scene,seed,radius,position,carved_count,boundary_ok,time_us_mean,time_us_median,time_us_p95,time_us_p99,time_us_std\n");

    constexpr int kWarmup = 10;
    constexpr int kIters = 1000;

    std::mt19937 rng_master(0xDEADBEEF);

    // Pre-generate scenes × seeds
    constexpr int kSeeds = 5;
    std::array<Grid, kScenes.size() * kSeeds> scene_cache;
    for (size_t s = 0; s < kScenes.size(); s++) {
        for (int sd = 0; sd < kSeeds; sd++) {
            kScenes[s].gen(scene_cache[s * kSeeds + sd]);
        }
    }

    // Mismatch tracker per strategy
    int total_mismatches[5] = {0, 0, 0, 0, 0};

    for (size_t st = 0; st < kStrategies.size(); st++) {
        Stats time_stats;
        int total_boundary_ok = 0;
        int total_configs = 0;

        for (size_t sc = 0; sc < kScenes.size(); sc++) {
            for (int sd = 0; sd < kSeeds; sd++) {
                for (float r : kRadii) {
                    for (size_t pp = 0; pp < kPositions.size(); pp++) {
                        const auto& pos = kPositions[pp];
                        Grid base = scene_cache[sc * kSeeds + sd];
                        Grid g = base;
                        // Generate reference carved (from base copy)
                        Grid ref = base;
                        carve_reference(ref, pos.x, pos.y, pos.z, r);
                        // Compute carved_count
                        int carved_count = 0;
                        for (int i = 0; i < kChunkVoxels; i++) if (ref[i] == 0) carved_count++;

                        // Warmup
                        for (int w = 0; w < kWarmup; w++) {
                            g = base;
                            kStrategies[st].fn(g, pos.x, pos.y, pos.z, r);
                        }
                        // Time
                        Timer timer;
                        for (int it = 0; it < kIters; it++) {
                            g = base;
                            timer.start();
                            kStrategies[st].fn(g, pos.x, pos.y, pos.z, r);
                            double t = timer.stop_us();
                            time_stats.accumulate(t);
                        }
                        // Validation
                        int mm = validate_against(g, ref);
                        int boundary_ok = (mm == 0) ? 1 : 0;
                        total_mismatches[st] += mm;
                        total_boundary_ok += boundary_ok;
                        total_configs++;

                        time_stats.finalize();
                        std::printf("%.*s,%.*s,%d,%.2f,%.*s,%d,%d,%.4f,%.4f,%.4f,%.4f,%.4f\n",
                            (int)kStrategies[st].name.size(), kStrategies[st].name.data(),
                            (int)kScenes[sc].name.size(), kScenes[sc].name.data(),
                            sd, r,
                            (int)pos.name.size(), pos.name.data(),
                            carved_count, boundary_ok,
                            time_stats.mean, time_stats.median, time_stats.p95, time_stats.p99, time_stats.stddev);
                        time_stats.reset();
                    }
                }
            }
        }
        std::fprintf(stderr, "STRATEGY %.*s: configs=%d, total_mismatches=%d (%.4f%% wrong), boundary_ok=%d/%d\n",
            (int)kStrategies[st].name.size(), kStrategies[st].name.data(),
            total_configs, total_mismatches[st],
            100.0 * total_mismatches[st] / (double)(total_configs * kChunkVoxels),
            total_boundary_ok, total_configs);
    }
    return 0;
}

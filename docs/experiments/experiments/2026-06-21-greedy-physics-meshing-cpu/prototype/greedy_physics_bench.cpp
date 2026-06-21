// SPDX-License-Identifier: MIT
// Greedy Physics Meshing benchmark — isolated from ProjectV mainline.
//
// Hypothesis (см. README.md §1): для ProjectV chunkSize=8 voxel chunks, current mainline baseline
// в `src/physics/PhysicsWorld.cpp:712-773::BuildStaticVoxelCollisionBody` добавляет per-solid-voxel
// `JPH::BoxShape(0.5f)` в CompoundShape = N shapes/chunk = 0× reduction. Greedy merge strategies
// должны достичь DoD "≥4× reduction" per `TODO.md §3.3`.
//
// 6 strategies: A_Naive (baseline) / B_1DZ / C_2DXZ / D_3D / E_Octree / F_TwoPass.
// 5 scenes × 5 seeds × 1000 iter + 10 warmup per config.
//
// Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -o bench bench.cpp
// Run:   ./bench --all > results.csv
//
// Measurement harness: per `docs/experiments/benchmarks/methodology.md §3`.
// Dev host: AMD Ryzen 7 5800X (Zen 3), governor `powersave`, AVX2 (no AVX-512).
// Per `hardware-profile.md §1`. Isolation: `taskset -c 2 ./bench` (caller responsibility).

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <numeric>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

// ---- Constants ----
constexpr int kChunkSize = 8;
constexpr int kChunkVolume = kChunkSize * kChunkSize * kChunkSize; // 512

// ---- Types ----
using SolidMask = std::vector<uint8_t>; // size = chunkVolume, 0=air, 1=solid
using Clock = std::chrono::high_resolution_clock;

struct AABB {
    int x0, y0, z0; // inclusive min
    int x1, y1, z1; // exclusive max
    int volume() const noexcept {
        return (x1 - x0) * (y1 - y0) * (z1 - z0);
    }
};

struct Strategy {
    std::string_view name;
    std::vector<AABB> (*emit)(SolidMask mask);
};

// ---- Solid mask access helpers ----
inline int idx3(int x, int y, int z) noexcept {
    return (y * kChunkSize + z) * kChunkSize + x; // x fastest
}
[[maybe_unused]] inline bool isSolid(const SolidMask &m, int x, int y, int z) noexcept {
    if (x < 0 || y < 0 || z < 0 || x >= kChunkSize || y >= kChunkSize || z >= kChunkSize)
        return false;
    return m[idx3(x, y, z)] != 0;
}
inline int countSolid(const SolidMask &m) noexcept {
    int n = 0;
    for (auto v : m)
        if (v)
            ++n;
    return n;
}

// =============================================================================
// Strategy A: NAIVE BASELINE (mirrors `src/physics/PhysicsWorld.cpp:712-740`)
// =============================================================================
std::vector<AABB> strategy_A_naive(SolidMask m) {
    std::vector<AABB> out;
    out.reserve(m.size());
    for (int y = 0; y < kChunkSize; ++y) {
        for (int z = 0; z < kChunkSize; ++z) {
            for (int x = 0; x < kChunkSize; ++x) {
                if (m[idx3(x, y, z)]) {
                    out.push_back({x, y, z, x + 1, y + 1, z + 1});
                }
            }
        }
    }
    return out;
}

// =============================================================================
// Strategy B: 1D greedy merge in Z (for each (X, Y) column, scan Z, emit runs)
// =============================================================================
std::vector<AABB> strategy_B_1dz(SolidMask m) {
    std::vector<AABB> out;
    out.reserve(m.size());
    for (int y = 0; y < kChunkSize; ++y) {
        for (int x = 0; x < kChunkSize; ++x) {
            int z = 0;
            while (z < kChunkSize) {
                if (!m[idx3(x, y, z)]) {
                    ++z;
                    continue;
                }
                int z0 = z;
                while (z < kChunkSize && m[idx3(x, y, z)])
                    ++z;
                out.push_back({x, y, z0, x + 1, y + 1, z});
            }
        }
    }
    return out;
}

// =============================================================================
// Strategy C: 2D greedy merge in XZ per Y-level (per-axis 2D scan, Lysenko pattern)
// =============================================================================
std::vector<AABB> strategy_C_2dxz_per_y(SolidMask m) { // copy (we mark processed)
    std::vector<AABB> out;
    out.reserve(m.size());
    for (int y = 0; y < kChunkSize; ++y) {
        // 2D greedy in XZ plane
        for (int z = 0; z < kChunkSize; ++z) {
            for (int x = 0; x < kChunkSize; ++x) {
                if (m[idx3(x, y, z)] == 0)
                    continue;
                // Find max width in X (continuous solid run)
                int x0 = x;
                while (x < kChunkSize && m[idx3(x, y, z)])
                    ++x;
                int x1 = x;
                // Try extending in Z: row [x0, x1) at z, then check if full row solid at z+1
                int z1 = z + 1;
                while (z1 < kChunkSize) {
                    bool fullRow = true;
                    for (int xi = x0; xi < x1; ++xi) {
                        if (m[idx3(xi, y, z1)] == 0) {
                            fullRow = false;
                            break;
                        }
                    }
                    if (!fullRow)
                        break;
                    for (int xi = x0; xi < x1; ++xi)
                        m[idx3(xi, y, z1)] = 0; // mark consumed
                    ++z1;
                }
                // mark current row as consumed
                for (int xi = x0; xi < x1; ++xi)
                    m[idx3(xi, y, z)] = 0;
                out.push_back({x0, y, z, x1, y + 1, z1});
            }
        }
    }
    return out;
}

// =============================================================================
// Strategy D: 3D full greedy (Mikola-Lysenko extension to 3D AABB)
// =============================================================================
std::vector<AABB> strategy_D_3d_full_greedy(SolidMask m) { // copy
    std::vector<AABB> out;
    out.reserve(m.size());
    for (int y = 0; y < kChunkSize; ++y) {
        for (int z = 0; z < kChunkSize; ++z) {
            for (int x = 0; x < kChunkSize; ++x) {
                if (m[idx3(x, y, z)] == 0)
                    continue;
                // Find max X-extent
                int x0 = x, x1 = x;
                while (x1 < kChunkSize && m[idx3(x1, y, z)])
                    ++x1;
                // Find max Y-extent (over X=[x0,x1), Z=fixed)
                int y0 = y, y1 = y + 1;
                while (y1 < kChunkSize) {
                    bool ok = true;
                    for (int xi = x0; xi < x1; ++xi) {
                        if (m[idx3(xi, y1, z)] == 0) {
                            ok = false;
                            break;
                        }
                    }
                    if (!ok)
                        break;
                    ++y1;
                }
                // Find max Z-extent (over X=[x0,x1), Y=[y0,y1))
                int z0 = z, z1 = z + 1;
                while (z1 < kChunkSize) {
                    bool ok = true;
                    for (int yi = y0; yi < y1; ++yi) {
                        for (int xi = x0; xi < x1; ++xi) {
                            if (m[idx3(xi, yi, z1)] == 0) {
                                ok = false;
                                break;
                            }
                        }
                        if (!ok)
                            break;
                    }
                    if (!ok)
                        break;
                    ++z1;
                }
                // Mark consumed
                for (int yi = y0; yi < y1; ++yi)
                    for (int xi = x0; xi < x1; ++xi)
                        m[idx3(xi, yi, z0)] = 0;
                for (int yi = y0; yi < y1; ++yi)
                    for (int xi = x0; xi < x1; ++xi)
                        m[idx3(xi, yi, z0)] = 0; // safety
                // Actually mark the full box
                for (int zi = z0; zi < z1; ++zi)
                    for (int yi = y0; yi < y1; ++yi)
                        for (int xi = x0; xi < x1; ++xi)
                            m[idx3(xi, yi, zi)] = 0;
                (void)x0; (void)x1; (void)y0; (void)y1; (void)z0; (void)z1;
                out.push_back({x, y, z, x1, y1, z1});
                // Bump x to end of current X-extent (skip already-consumed)
                // Note: x is already past first solid due to x1 logic above; ensure loop progresses
                // Since mark is full box [x,x1) × [y,y1) × [z,z1), next iteration starts past x1.
                // But outer loop increments x by 1; safe because we already set those cells to 0.
            }
        }
    }
    return out;
}

// =============================================================================
// Strategy E: Hierarchical octree (top-down, emit leaf AABB for all-solid subtree)
// =============================================================================
namespace e_octree_detail {
void subdivide(
    const SolidMask &m,
    int x0, int y0, int z0, int x1, int y1, int z1,
    std::vector<AABB> &out
) {
    int w = x1 - x0, h = y1 - y0, d = z1 - z0;
    // Check if fully solid
    bool allSolid = true;
    for (int y = y0; y < y1 && allSolid; ++y)
        for (int z = z0; z < z1 && allSolid; ++z)
            for (int x = x0; x < x1; ++x)
                if (m[idx3(x, y, z)] == 0) {
                    allSolid = false;
                    break;
                }
    if (allSolid) {
        out.push_back({x0, y0, z0, x1, y1, z1});
        return;
    }
    // Check if fully empty
    bool allEmpty = true;
    for (int y = y0; y < y1 && allEmpty; ++y)
        for (int z = z0; z < z1 && allEmpty; ++z)
            for (int x = x0; x < x1; ++x)
                if (m[idx3(x, y, z)] != 0) {
                    allEmpty = false;
                    break;
                }
    if (allEmpty)
        return;
    // Mixed: subdivide if box > 1; else emit per-voxel
    if (w == 1 && h == 1 && d == 1) {
        // Single mixed voxel (shouldn't happen given allEmpty check, but safe)
        for (int y = y0; y < y1; ++y)
            for (int z = z0; z < z1; ++z)
                for (int x = x0; x < x1; ++x)
                    if (m[idx3(x, y, z)])
                        out.push_back({x, y, z, x + 1, y + 1, z + 1});
        return;
    }
    // Subdivide at midpoint
    int xm = x0 + w / 2;
    int ym = y0 + h / 2;
    int zm = z0 + d / 2;
    // 8 octants
    subdivide(m, x0, y0, z0, xm, ym, zm, out);
    if (xm < x1) subdivide(m, xm, y0, z0, x1, ym, zm, out);
    if (ym < y1) subdivide(m, x0, ym, z0, xm, y1, zm, out);
    if (xm < x1 && ym < y1) subdivide(m, xm, ym, z0, x1, y1, zm, out);
    if (zm < z1) subdivide(m, x0, y0, zm, xm, ym, z1, out);
    if (xm < x1 && zm < z1) subdivide(m, xm, y0, zm, x1, ym, z1, out);
    if (ym < y1 && zm < z1) subdivide(m, x0, ym, zm, xm, y1, z1, out);
    if (xm < x1 && ym < y1 && zm < z1) subdivide(m, xm, ym, zm, x1, y1, z1, out);
}
} // namespace e_octree_detail

std::vector<AABB> strategy_E_octree(SolidMask m) {
    std::vector<AABB> out;
    out.reserve(m.size());
    e_octree_detail::subdivide(m, 0, 0, 0, kChunkSize, kChunkSize, kChunkSize, out);
    return out;
}

// =============================================================================
// Strategy F: TwoPass — 2D XZ per Y + vertical merge of identical-XZ slices
// =============================================================================
std::vector<AABB> strategy_F_twopass(SolidMask m) { // copy
    // Pass 1: 2D XZ per Y (reuse C-style, accumulate per-Y slice sets)
    // Then Pass 2: for adjacent Y-slices with identical XZ footprint, merge vertically.
    // Implementation: do C per Y-level, track per-cell membership. Then per (Y, Y+1) check identical.
    // Simpler implementation: C per Y, then collapse identical adjacent Y AABBs.
    // For grid (8x8x8) brute-force: O(K^2 * 8) where K = total C-output count.
    // For small grids, accept O(K^2) per chunk.

    struct PerYSlice {
        std::vector<std::array<int, 4>> rects; // [x0, x1, z0, z1]
    };
    std::vector<PerYSlice> slices(kChunkSize);
    for (int y = 0; y < kChunkSize; ++y) {
        auto &s = slices[y];
        for (int z = 0; z < kChunkSize; ++z) {
            for (int x = 0; x < kChunkSize; ++x) {
                if (m[idx3(x, y, z)] == 0)
                    continue;
                int x0 = x;
                while (x < kChunkSize && m[idx3(x, y, z)])
                    ++x;
                int x1 = x;
                int z1 = z + 1;
                while (z1 < kChunkSize) {
                    bool fullRow = true;
                    for (int xi = x0; xi < x1; ++xi)
                        if (m[idx3(xi, y, z1)] == 0) {
                            fullRow = false;
                            break;
                        }
                    if (!fullRow)
                        break;
                    for (int xi = x0; xi < x1; ++xi)
                        m[idx3(xi, y, z1)] = 0;
                    ++z1;
                }
                for (int xi = x0; xi < x1; ++xi)
                    m[idx3(xi, y, z)] = 0;
                s.rects.push_back({x0, x1, z, z1});
            }
        }
    }

    // Pass 2: vertical merge — for each Y, find runs of consecutive Y where slice is identical
    // and merge into taller AABBs.
    std::vector<AABB> out;
    out.reserve(slices[0].rects.size() * kChunkSize);
    // For each (Y, rect), check if (Y+1) has identical rect set; if so, extend y1.
    // We track "consumed" by setting rect's tracking via visited flag.
    std::vector<std::vector<bool>> visited(kChunkSize);
    for (int y = 0; y < kChunkSize; ++y)
        visited[y].assign(slices[y].rects.size(), false);

    for (int y = 0; y < kChunkSize; ++y) {
        for (size_t ri = 0; ri < slices[y].rects.size(); ++ri) {
            if (visited[y][ri])
                continue;
            const auto &r = slices[y].rects[ri];
            int y0 = y, y1 = y + 1;
            // Try to extend downward
            while (y1 < kChunkSize) {
                bool match = false;
                for (size_t rj = 0; rj < slices[y1].rects.size(); ++rj) {
                    if (visited[y1][rj])
                        continue;
                    const auto &rn = slices[y1].rects[rj];
                    if (rn[0] == r[0] && rn[1] == r[1] && rn[2] == r[2] && rn[3] == r[3]) {
                        visited[y1][rj] = true;
                        match = true;
                        break;
                    }
                }
                if (!match)
                    break;
                ++y1;
            }
            visited[y][ri] = true;
            out.push_back({r[0], y0, r[2], r[1], y1, r[3]});
        }
    }
    return out;
}

// ---- Strategy registry ----
constexpr std::array<Strategy, 6> kStrategies = {{
    {"A_Naive", &strategy_A_naive},
    {"B_1DZ", &strategy_B_1dz},
    {"C_2DXZ", &strategy_C_2dxz_per_y},
    {"D_3D", &strategy_D_3d_full_greedy},
    {"E_Octree", &strategy_E_octree},
    {"F_TwoPass", &strategy_F_twopass},
}};

// ---- Scene generators ----
// Each returns a SolidMask of size kChunkVolume for a given seed.
SolidMask scene_uniform_floor(uint64_t seed) {
    SolidMask m(kChunkVolume, 0);
    for (int x = 0; x < kChunkSize; ++x)
        for (int z = 0; z < kChunkSize; ++z)
            m[idx3(x, 0, z)] = 1; // Y=0 layer, all solid (ignore seed)
    (void)seed;
    return m;
}

SolidMask scene_uniform_half(uint64_t seed) {
    SolidMask m(kChunkVolume, 0);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < kChunkSize; ++x)
            for (int z = 0; z < kChunkSize; ++z)
                m[idx3(x, y, z)] = 1; // bottom half, all solid
    (void)seed;
    return m;
}

SolidMask scene_forest_floor(uint64_t seed) {
    SolidMask m(kChunkVolume, 0);
    // 3 Y-levels solid floor
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < kChunkSize; ++x)
            for (int z = 0; z < kChunkSize; ++z)
                m[idx3(x, y, z)] = 1;
    // 4 random vertical pillars (trees)
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> dx(0, kChunkSize - 1);
    std::uniform_int_distribution<int> dz(0, kChunkSize - 1);
    for (int t = 0; t < 4; ++t) {
        int x = dx(rng), z = dz(rng);
        for (int y = 3; y < kChunkSize; ++y)
            m[idx3(x, y, z)] = 1;
    }
    return m;
}

SolidMask scene_cave_stress(uint64_t seed) {
    SolidMask m(kChunkVolume, 0);
    // Solid shell (boundary)
    for (int y = 0; y < kChunkSize; ++y)
        for (int z = 0; z < kChunkSize; ++z) {
            m[idx3(0, y, z)] = 1;
            m[idx3(kChunkSize - 1, y, z)] = 1;
        }
    for (int x = 0; x < kChunkSize; ++x)
        for (int z = 0; z < kChunkSize; ++z) {
            m[idx3(x, 0, z)] = 1;
            m[idx3(x, kChunkSize - 1, z)] = 1;
        }
    for (int x = 0; x < kChunkSize; ++x)
        for (int y = 0; y < kChunkSize; ++y) {
            m[idx3(x, y, 0)] = 1;
            m[idx3(x, y, kChunkSize - 1)] = 1;
        }
    // Random interior chambers (worst case for 3D merge)
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> dc(1, 6); // chamber radius 1-3
    for (int c = 0; c < 3; ++c) {
        int cx = 1 + (rng() % (kChunkSize - 2));
        int cy = 1 + (rng() % (kChunkSize - 2));
        int cz = 1 + (rng() % (kChunkSize - 2));
        int r = dc(rng);
        for (int y = std::max(1, cy - r); y < std::min(kChunkSize - 1, cy + r + 1); ++y)
            for (int z = std::max(1, cz - r); z < std::min(kChunkSize - 1, cz + r + 1); ++z)
                for (int x = std::max(1, cx - r); x < std::min(kChunkSize - 1, cx + r + 1); ++x)
                    m[idx3(x, y, z)] = 0; // hollow out
    }
    return m;
}

SolidMask scene_mixed_biome(uint64_t seed) {
    SolidMask m(kChunkVolume, 0);
    // Layer 0: stone (floor)
    for (int x = 0; x < kChunkSize; ++x)
        for (int z = 0; z < kChunkSize; ++z)
            m[idx3(x, 0, z)] = 1;
    // Layer 1: stone continued
    for (int x = 0; x < kChunkSize; ++x)
        for (int z = 0; z < kChunkSize; ++z)
            m[idx3(x, 1, z)] = 1;
    // Layer 2: grass (selective based on noise)
    std::mt19937_64 rng(seed);
    for (int x = 0; x < kChunkSize; ++x)
        for (int z = 0; z < kChunkSize; ++z)
            if (rng() % 100 < 70)
                m[idx3(x, 2, z)] = 1;
    // Glass walls (4 walls)
    for (int y = 0; y < 4; ++y)
        for (int z = 0; z < kChunkSize; ++z) {
            m[idx3(0, y, z)] = 1;
            m[idx3(kChunkSize - 1, y, z)] = 1;
        }
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < kChunkSize; ++x) {
            m[idx3(x, y, 0)] = 1;
            m[idx3(x, y, kChunkSize - 1)] = 1;
        }
    return m;
}

struct Scene {
    std::string_view name;
    SolidMask (*gen)(uint64_t seed);
};

constexpr std::array<Scene, 5> kScenes = {{
    {"uniform_floor", &scene_uniform_floor},
    {"uniform_half", &scene_uniform_half},
    {"forest_floor", &scene_forest_floor},
    {"cave_stress", &scene_cave_stress},
    {"mixed_biome", &scene_mixed_biome},
}};

const std::array<uint64_t, 5> kSeeds = {1, 7, 42, 1234, 31337};

// ---- Volume sanity ----
int totalVolume(const std::vector<AABB> &boxes) {
    int v = 0;
    for (const auto &b : boxes)
        v += b.volume();
    return v;
}

// ---- Measurement ----
struct Measurement {
    std::string_view strategy;
    std::string_view scene;
    uint64_t seed;
    int solidCount;
    int shapeCount;
    int volumeEmitted;
    int volumeExpected;
    double volumeMatchPct; // (volumeEmitted / volumeExpected) * 100
    double shapeReduction; // (double)shapeCount / solidCount
    double buildUs;        // mean over iters
};

Measurement runOne(
    const Strategy &strategy,
    const Scene &scene,
    uint64_t seed,
    int iters,
    int warmup
) {
    auto mask = scene.gen(seed);
    const int solidCount = countSolid(mask);
    const int volumeExpected = solidCount; // each solid voxel = 1 unit volume

    // Warmup
    for (int i = 0; i < warmup; ++i) {
        auto boxes = strategy.emit(mask);
        (void)boxes;
    }

    // Measurement: time each iter
    std::vector<double> perIterUs;
    perIterUs.reserve(iters);
    int finalShapeCount = 0;
    int finalVolumeEmitted = 0;
    for (int i = 0; i < iters; ++i) {
        auto t0 = Clock::now();
        auto boxes = strategy.emit(mask);
        auto t1 = Clock::now();
        double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
        perIterUs.push_back(us);
        if (i == iters - 1) {
            finalShapeCount = static_cast<int>(boxes.size());
            finalVolumeEmitted = totalVolume(boxes);
        }
    }

    double meanUs = 0.0;
    for (double u : perIterUs)
        meanUs += u;
    meanUs /= static_cast<double>(iters);

    double matchPct = (volumeExpected > 0)
        ? (100.0 * static_cast<double>(finalVolumeEmitted) / static_cast<double>(volumeExpected))
        : 100.0;
    double reduction = (solidCount > 0)
        ? static_cast<double>(finalShapeCount) / static_cast<double>(solidCount)
        : 0.0;

    return Measurement{
        strategy.name,
        scene.name,
        seed,
        solidCount,
        finalShapeCount,
        finalVolumeEmitted,
        volumeExpected,
        matchPct,
        reduction,
        meanUs,
    };
}

// ---- CLI ----
struct Config {
    bool all = true;
    std::vector<std::string_view> strategiesFilter;
    std::vector<std::string_view> scenesFilter;
    int iters = 1000;
    int warmup = 10;
    std::string outputPath = "results.csv";
};

void printUsage() {
    std::fprintf(stderr,
        "Usage: greedy_physics_bench [options]\n"
        "  --all                 run all (default)\n"
        "  --strategy=NAME       filter strategies (comma-sep; A_Naive,B_1DZ,...)\n"
        "  --scene=NAME          filter scenes (comma-sep; uniform_floor,...)\n"
        "  --iters=N             iterations per config (default 1000)\n"
        "  --warmup=N            warmup iterations (default 10)\n"
        "  --output=PATH         output CSV path (default results.csv)\n"
        "  --help                print this help\n"
        "\n"
        "Strategies: A_Naive, B_1DZ, C_2DXZ, D_3D, E_Octree, F_TwoPass\n"
        "Scenes: uniform_floor, uniform_half, forest_floor, cave_stress, mixed_biome\n");
}

Config parseArgs(int argc, char **argv) {
    Config c;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage();
            std::exit(0);
        } else if (arg == "--all") {
            c.all = true;
        } else if (arg.starts_with("--strategy=")) {
            c.all = false;
            c.strategiesFilter.clear();
            std::string list = std::string(arg.substr(11));
            size_t pos = 0;
            while ((pos = list.find(',')) != std::string::npos) {
                c.strategiesFilter.push_back(list.substr(0, pos));
                list.erase(0, pos + 1);
            }
            if (!list.empty())
                c.strategiesFilter.push_back(list);
        } else if (arg.starts_with("--scene=")) {
            c.all = false;
            c.scenesFilter.clear();
            std::string list = std::string(arg.substr(8));
            size_t pos = 0;
            while ((pos = list.find(',')) != std::string::npos) {
                c.scenesFilter.push_back(list.substr(0, pos));
                list.erase(0, pos + 1);
            }
            if (!list.empty())
                c.scenesFilter.push_back(list);
        } else if (arg.starts_with("--iters=")) {
            c.iters = std::atoi(arg.substr(8).data());
        } else if (arg.starts_with("--warmup=")) {
            c.warmup = std::atoi(arg.substr(9).data());
        } else if (arg.starts_with("--output=")) {
            c.outputPath = std::string(arg.substr(9));
        } else {
            std::fprintf(stderr, "Unknown arg: %s\n", std::string(arg).c_str());
            printUsage();
            std::exit(1);
        }
    }
    return c;
}

bool isFiltered(const std::vector<std::string_view> &filter, std::string_view name) {
    if (filter.empty())
        return true;
    for (auto &f : filter)
        if (f == name)
            return true;
    return false;
}

} // namespace

int main(int argc, char **argv) {
    Config cfg = parseArgs(argc, argv);

    // Print header
    std::FILE *out = std::fopen(cfg.outputPath.c_str(), "w");
    if (!out) {
        std::fprintf(stderr, "Cannot open output: %s\n", cfg.outputPath.c_str());
        return 1;
    }
    std::fprintf(out, "strategy,scene,seed,solid_count,shape_count,volume_emitted,volume_expected,volume_match_pct,shape_reduction_ratio,build_us_mean\n");
    std::fprintf(stderr, "[bench] writing to %s, iters=%d, warmup=%d\n",
                 cfg.outputPath.c_str(), cfg.iters, cfg.warmup);

    size_t totalConfigs = 0;
    size_t totalMeasurements = 0;
    auto tStart = Clock::now();
    for (const auto &s : kStrategies) {
        if (!isFiltered(cfg.strategiesFilter, s.name))
            continue;
        for (const auto &sc : kScenes) {
            if (!isFiltered(cfg.scenesFilter, sc.name))
                continue;
            for (uint64_t seed : kSeeds) {
                ++totalConfigs;
                Measurement m = runOne(s, sc, seed, cfg.iters, cfg.warmup);
                std::fprintf(out, "%.*s,%.*s,%llu,%d,%d,%d,%d,%.4f,%.4f,%.4f\n",
                    static_cast<int>(m.strategy.size()), m.strategy.data(),
                    static_cast<int>(m.scene.size()), m.scene.data(),
                    static_cast<unsigned long long>(m.seed),
                    m.solidCount, m.shapeCount, m.volumeEmitted, m.volumeExpected,
                    m.volumeMatchPct, m.shapeReduction, m.buildUs);
                ++totalMeasurements;
                if (totalMeasurements % 10 == 0)
                    std::fflush(out);
            }
        }
    }
    std::fflush(out);
    std::fclose(out);
    auto tEnd = Clock::now();
    double totalSec = std::chrono::duration<double>(tEnd - tStart).count();
    std::fprintf(stderr, "[bench] %zu configs × %d iters = %zu main measurements + %d warmup each = done in %.2f s\n",
                 totalConfigs, cfg.iters, totalMeasurements, cfg.warmup, totalSec);
    return 0;
}

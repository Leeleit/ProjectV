#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <print>
#include <random>
#include <vector>

static constexpr int kChunkSize = 8;
static constexpr int kVoxelCount = kChunkSize * kChunkSize * kChunkSize; // 512
static constexpr int kWarmup = 10;
static constexpr int kIter = 1000;
static constexpr int kNumSeeds = 5;
static constexpr int kSeedValues[kNumSeeds] = {1, 7, 42, 1234, 31337};

using VoxelGrid = std::array<uint8_t, kVoxelCount>;
using LabelGrid = std::array<uint16_t, kVoxelCount>;

// ---------------------------------------------------------------------------
// Union-Find with path compression + union by rank
// ---------------------------------------------------------------------------
struct UnionFind {
    std::array<uint16_t, kVoxelCount> parent;
    std::array<uint8_t, kVoxelCount>  rank;

    void reset() {
        for (int i = 0; i < kVoxelCount; ++i) {
            parent[i] = static_cast<uint16_t>(i);
        }
        rank.fill(0);
    }

    uint16_t find(uint16_t x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]]; // path halving
            x = parent[x];
        }
        return x;
    }

    void unite(uint16_t a, uint16_t b) {
        a = find(a);
        b = find(b);
        if (a == b) return;
        if (rank[a] < rank[b]) {
            parent[a] = b;
        } else if (rank[a] > rank[b]) {
            parent[b] = a;
        } else {
            parent[b] = a;
            ++rank[a];
        }
    }
};

// ---------------------------------------------------------------------------
// Index helpers
// ---------------------------------------------------------------------------
static int idx(int x, int y, int z) {
    return (z * kChunkSize + y) * kChunkSize + x;
}

static bool in_bounds(int x, int y, int z) {
    return x >= 0 && x < kChunkSize && y >= 0 && y < kChunkSize && z >= 0 && z < kChunkSize;
}

// ---------------------------------------------------------------------------
// Strategy A: Two-pass CCL with 26-connectivity
// ---------------------------------------------------------------------------
struct ResultCCL {
    LabelGrid labels{};
    uint16_t  componentCount{};
    double    elapsed{};
};

ResultCCL ccl_26(const VoxelGrid &voxels) {
    UnionFind uf;
    uf.reset();

    LabelGrid labels{};
    labels.fill(0);
    uint16_t nextLabel = 1;

    // Pass 1: assign provisional labels
    for (int z = 0; z < kChunkSize; ++z) {
        for (int y = 0; y < kChunkSize; ++y) {
            for (int x = 0; x < kChunkSize; ++x) {
                int i = idx(x, y, z);
                if (voxels[i] == 0) continue; // air

                // Check mask neighbors (already visited in raster order)
                uint16_t neighborLabel = 0;
                auto check = [&](int nx, int ny, int nz) {
                    if (!in_bounds(nx, ny, nz)) return;
                    uint16_t lbl = labels[idx(nx, ny, nz)];
                    if (lbl == 0) return;
                    if (neighborLabel == 0) {
                        neighborLabel = lbl;
                    } else if (lbl != neighborLabel) {
                        uf.unite(neighborLabel, lbl);
                    }
                };

                // Same z-plane (4 neighbors: left, top-left, top, top-right)
                check(x - 1, y, z);
                check(x - 1, y - 1, z);
                check(x    , y - 1, z);
                check(x + 1, y - 1, z);

                // Previous z-plane (9 neighbors: all 3x3)
                check(x - 1, y - 1, z - 1);
                check(x    , y - 1, z - 1);
                check(x + 1, y - 1, z - 1);
                check(x - 1, y    , z - 1);
                check(x    , y    , z - 1);
                check(x + 1, y    , z - 1);
                check(x - 1, y + 1, z - 1);
                check(x    , y + 1, z - 1);
                check(x + 1, y + 1, z - 1);

                if (neighborLabel == 0) {
                    labels[i] = nextLabel++;
                } else {
                    labels[i] = neighborLabel;
                }
            }
        }
    }

    if (nextLabel == 1) {
        return {labels, 0, 0.0};
    }

    // Pass 2: resolve equivalences
    for (int i = 0; i < kVoxelCount; ++i) {
        if (labels[i] != 0) {
            labels[i] = uf.find(labels[i]);
        }
    }

    // Compact labels to consecutive
    std::array<uint16_t, 512> remap{};
    remap.fill(0);
    uint16_t compactCount = 0;
    for (int i = 0; i < kVoxelCount; ++i) {
        if (labels[i] == 0) continue;
        uint16_t r = labels[i];
        if (remap[r] == 0) {
            remap[r] = ++compactCount;
        }
        labels[i] = remap[r];
    }

    return {labels, compactCount, 0.0};
}

// ---------------------------------------------------------------------------
// Strategy B: Two-pass CCL with 6-connectivity
// ---------------------------------------------------------------------------
ResultCCL ccl_6(const VoxelGrid &voxels) {
    UnionFind uf;
    uf.reset();

    LabelGrid labels{};
    labels.fill(0);
    uint16_t nextLabel = 1;

    for (int z = 0; z < kChunkSize; ++z) {
        for (int y = 0; y < kChunkSize; ++y) {
            for (int x = 0; x < kChunkSize; ++x) {
                int i = idx(x, y, z);
                if (voxels[i] == 0) continue;

                uint16_t neighborLabel = 0;
                auto check = [&](int nx, int ny, int nz) {
                    if (!in_bounds(nx, ny, nz)) return;
                    uint16_t lbl = labels[idx(nx, ny, nz)];
                    if (lbl == 0) return;
                    if (neighborLabel == 0) {
                        neighborLabel = lbl;
                    } else if (lbl != neighborLabel) {
                        uf.unite(neighborLabel, lbl);
                    }
                };

                check(x - 1, y, z); // left
                check(x    , y - 1, z); // top
                check(x    , y, z - 1); // back

                if (neighborLabel == 0) {
                    labels[i] = nextLabel++;
                } else {
                    labels[i] = neighborLabel;
                }
            }
        }
    }

    if (nextLabel == 1) {
        return {labels, 0, 0.0};
    }

    for (int i = 0; i < kVoxelCount; ++i) {
        if (labels[i] != 0) {
            labels[i] = uf.find(labels[i]);
        }
    }

    std::array<uint16_t, 512> remap{};
    remap.fill(0);
    uint16_t compactCount = 0;
    for (int i = 0; i < kVoxelCount; ++i) {
        if (labels[i] == 0) continue;
        uint16_t r = labels[i];
        if (remap[r] == 0) {
            remap[r] = ++compactCount;
        }
        labels[i] = remap[r];
    }

    return {labels, compactCount, 0.0};
}

// ---------------------------------------------------------------------------
// Strategy C: Flood-fill BFS from seed (reachability analysis)
// ---------------------------------------------------------------------------
struct ResultFloodFill {
    std::array<bool, kVoxelCount> reachable{};
    int    reachableCount{};
    double elapsed{};
};

ResultFloodFill flood_fill(const VoxelGrid &voxels, int sx, int sy, int sz) {
    auto r = ResultFloodFill{};
    int seedI = idx(sx, sy, sz);
    if (voxels[seedI] != 0) {
        // Seed is solid — can't start there; find nearest air
        return r;
    }

    // BFS queue (ring buffer for 512 elements)
    std::array<int, kVoxelCount> queue{};
    int head = 0, tail = 0;

    r.reachable[seedI] = true;
    queue[tail++] = seedI;
    r.reachableCount = 1;

    // Direction offsets for 6-connectivity (face neighbors)
    static constexpr int kDx[] = {1, -1, 0, 0, 0, 0};
    static constexpr int kDy[] = {0, 0, 1, -1, 0, 0};
    static constexpr int kDz[] = {0, 0, 0, 0, 1, -1};

    while (head < tail) {
        int cur = queue[head++];
        int cx = cur % kChunkSize;
        int cy = (cur / kChunkSize) % kChunkSize;
        int cz = cur / (kChunkSize * kChunkSize);

        for (int d = 0; d < 6; ++d) {
            int nx = cx + kDx[d];
            int ny = cy + kDy[d];
            int nz = cz + kDz[d];
            if (!in_bounds(nx, ny, nz)) continue;

            int ni = idx(nx, ny, nz);
            if (voxels[ni] != 0) continue; // solid
            if (r.reachable[ni]) continue;

            r.reachable[ni] = true;
            ++r.reachableCount;
            queue[tail++] = ni;
        }
    }

    return r;
}

// ---------------------------------------------------------------------------
// Strategy D: Overhang detection
// ---------------------------------------------------------------------------
struct ResultOverhang {
    std::array<bool, kVoxelCount> unsupported{};
    int unsupportedCount{};
    double elapsed{};
};

ResultOverhang detect_overhangs(const VoxelGrid &voxels, int maxSpan) {
    auto r = ResultOverhang{};

    for (int x = 0; x < kChunkSize; ++x) {
        for (int y = 0; y < kChunkSize; ++y) {
            // bottom layer (z=0) is always "supported" by bedrock assumption
            if (voxels[idx(x, y, 0)] != 0) continue;

            for (int z = 0; z < kChunkSize; ++z) {
                int i = idx(x, y, z);
                if (voxels[i] == 0) continue; // air — skip
                // Check if there's solid support within maxSpan below
                bool supported = false;
                for (int dz = 1; dz <= maxSpan; ++dz) {
                    int nz = z - dz;
                    if (nz < 0) {
                        supported = true; // grounded on chunk bottom
                        break;
                    }
                    if (voxels[idx(x, y, nz)] != 0) {
                        // Check diagonal support too for stability
                        bool diagOk = false;
                        for (int dx = -1; dx <= 1; ++dx) {
                            for (int dy = -1; dy <= 1; ++dy) {
                                if (dx == 0 && dy == 0) continue;
                                int nx = x + dx, ny = y + dy;
                                if (!in_bounds(nx, ny, nz)) continue;
                                if (voxels[idx(nx, ny, nz)] != 0) {
                                    diagOk = true;
                                    break;
                                }
                            }
                            if (diagOk) break;
                        }
                        if (diagOk) {
                            supported = true;
                            break;
                        }
                    }
                }
                if (!supported) {
                    r.unsupported[i] = true;
                    ++r.unsupportedCount;
                }
            }
        }
    }

    return r;
}

// ---------------------------------------------------------------------------
// Strategy E: Exposed surface classification
// ---------------------------------------------------------------------------
struct ResultExposed {
    VoxelGrid exposed{};
    int exposedCount{};
    double elapsed{};
};

ResultExposed classify_exposed(const VoxelGrid &voxels) {
    auto r = ResultExposed{};
    r.exposed.fill(0);

    static constexpr int kDx[] = {1, -1, 0, 0, 0, 0};
    static constexpr int kDy[] = {0, 0, 1, -1, 0, 0};
    static constexpr int kDz[] = {0, 0, 0, 0, 1, -1};

    for (int z = 0; z < kChunkSize; ++z) {
        for (int y = 0; y < kChunkSize; ++y) {
            for (int x = 0; x < kChunkSize; ++x) {
                int i = idx(x, y, z);
                if (voxels[i] == 0) continue; // air
                for (int d = 0; d < 6; ++d) {
                    int nx = x + kDx[d];
                    int ny = y + kDy[d];
                    int nz = z + kDz[d];
                    if (!in_bounds(nx, ny, nz)) {
                        // chunk boundary = exposed (faces void)
                        r.exposed[i] = 1;
                        break;
                    }
                    if (voxels[idx(nx, ny, nz)] == 0) {
                        r.exposed[i] = 1;
                        break;
                    }
                }
                if (r.exposed[i]) ++r.exposedCount;
            }
        }
    }

    return r;
}

// ---------------------------------------------------------------------------
// Scene generators (deterministic from seed)
// ---------------------------------------------------------------------------
enum class SceneType {
    UniformAir,
    UniformFloor,
    ForestFloor,
    CaveStress,
    MixedBiome,
    kCount
};

static constexpr const char *kSceneNames[] = {
    "uniform_air", "uniform_floor", "forest_floor", "cave_stress", "mixed_biome"
};

VoxelGrid generate_scene(SceneType type, int seed) {
    std::mt19937 rng(static_cast<unsigned>(seed));
    VoxelGrid v{};
    v.fill(0);

    switch (type) {
    case SceneType::UniformAir:
        // All air — nothing to do
        break;

    case SceneType::UniformFloor:
        // Bottom 2 layers solid, rest air
        for (int x = 0; x < kChunkSize; ++x)
            for (int y = 0; y < kChunkSize; ++y)
                for (int z = 0; z < 2; ++z)
                    v[idx(x, y, z)] = 1;
        break;

    case SceneType::ForestFloor: {
        // Terrain height varies per column (1-5 solid layers), with some air pockets
        for (int x = 0; x < kChunkSize; ++x) {
            for (int y = 0; y < kChunkSize; ++y) {
                int height = 1 + (std::abs(static_cast<int>(rng())) % 5);
                for (int z = 0; z < height && z < kChunkSize; ++z) {
                    v[idx(x, y, z)] = 1;
                }
            }
        }
        // Add some air pockets inside solid regions
        for (int p = 0; p < 8; ++p) {
            int px = static_cast<int>(rng()) % kChunkSize;
            int py = static_cast<int>(rng()) % kChunkSize;
            int pz = 1 + (static_cast<int>(rng()) % 3);
            if (v[idx(px, py, pz)] != 0) {
                v[idx(px, py, pz)] = 0;
            }
        }
        break;
    }

    case SceneType::CaveStress: {
        // Solid block with multiple worm-like cave tunnels
        for (int i = 0; i < kVoxelCount; ++i) v[i] = 1;

        // Generate 3-5 worm tunnels
        int numTunnels = 3 + (static_cast<int>(rng()) % 3);
        for (int t = 0; t < numTunnels; ++t) {
            int cx = static_cast<int>(rng()) % kChunkSize;
            int cy = static_cast<int>(rng()) % kChunkSize;
            int cz = static_cast<int>(rng()) % kChunkSize;
            int length = 8 + (static_cast<int>(rng()) % 16);
            for (int step = 0; step < length; ++step) {
                for (int dx = -1; dx <= 1; ++dx)
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dz = -1; dz <= 1; ++dz) {
                            int nx = cx + dx, ny = cy + dy, nz = cz + dz;
                            if (in_bounds(nx, ny, nz))
                                v[idx(nx, ny, nz)] = 0;
                        }
                // Random walk
                cx += (static_cast<int>(rng()) % 3) - 1;
                cy += (static_cast<int>(rng()) % 3) - 1;
                cz += (static_cast<int>(rng()) % 3) - 1;
                cx = std::clamp(cx, 1, kChunkSize - 2);
                cy = std::clamp(cy, 1, kChunkSize - 2);
                cz = std::clamp(cz, 1, kChunkSize - 2);
            }
        }
        // Create some isolated air pockets (not connected to tunnels)
        for (int p = 0; p < 3; ++p) {
            int px = static_cast<int>(rng()) % kChunkSize;
            int py = static_cast<int>(rng()) % kChunkSize;
            int pz = static_cast<int>(rng()) % kChunkSize;
            v[idx(px, py, pz)] = 0;
            // Ensure pocket is small and isolated
            for (int dx = -1; dx <= 1; ++dx)
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dz = -1; dz <= 1; ++dz)
                        if (std::abs(dx) + std::abs(dy) + std::abs(dz) == 1) {
                            int nx = px + dx, ny = py + dy, nz = pz + dz;
                            if (in_bounds(nx, ny, nz) && v[idx(nx, ny, nz)] != 0) {
                                v[idx(nx, ny, nz)] = 0;
                            }
                        }
        }
        break;
    }

    case SceneType::MixedBiome: {
        // Varied terrain with overhangs and floating clumps
        for (int i = 0; i < kVoxelCount; ++i) v[i] = 0;

        // Solid floor (bottom 1-2 layers)
        for (int x = 0; x < kChunkSize; ++x)
            for (int y = 0; y < kChunkSize; ++y)
                for (int z = 0; z < 1; ++z)
                    v[idx(x, y, z)] = 1;

        // Pillars (some reaching top)
        for (int p = 0; p < 4; ++p) {
            int px = static_cast<int>(rng()) % kChunkSize;
            int py = static_cast<int>(rng()) % kChunkSize;
            int height = 4 + (static_cast<int>(rng()) % 4);
            for (int z = 1; z < std::min(height, kChunkSize); ++z) {
                v[idx(px, py, z)] = 1;
            }
        }

        // Floating platforms (overhangs)
        for (int p = 0; p < 3; ++p) {
            int px = static_cast<int>(rng()) % kChunkSize;
            int py = static_cast<int>(rng()) % kChunkSize;
            int pz = 3 + (static_cast<int>(rng()) % 4);
            int size = 1 + (static_cast<int>(rng()) % 2);
            for (int dx = -size; dx <= size; ++dx)
                for (int dy = -size; dy <= size; ++dy) {
                    int nx = px + dx, ny = py + dy;
                    if (in_bounds(nx, ny, pz))
                        v[idx(nx, ny, pz)] = 2; // different material
                }
        }

        // Floating isolated blocks
        for (int p = 0; p < 2; ++p) {
            int px = static_cast<int>(rng()) % kChunkSize;
            int py = static_cast<int>(rng()) % kChunkSize;
            int pz = 5 + (static_cast<int>(rng()) % 2);
            v[idx(px, py, pz)] = 3;
        }
        break;
    }
    }

    return v;
}

// ---------------------------------------------------------------------------
// Benchmark helper
// ---------------------------------------------------------------------------
struct Measurement {
    const char *scene;
    int seed;
    const char *strategy;
    double mean;
    double median;
    double p95;
    double p99;
    double stddev;
    int count; // component count or special metric
};

static double now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) * 1e9 + static_cast<double>(ts.tv_nsec);
}

Measurement benchmark_ccl(const VoxelGrid &voxels, const char *scene, int seed,
                          const char *strat, auto &&func) {
    // Warmup
    decltype(func()) result;
    for (int w = 0; w < kWarmup; ++w) {
        result = func();
    }

    double samples[kIter];
    for (int i = 0; i < kIter; ++i) {
        double t0 = now_ns();
        result = func();
        double t1 = now_ns();
        samples[i] = t1 - t0;
    }

    std::sort(samples, samples + kIter);
    double sum = 0;
    for (int i = 0; i < kIter; ++i) sum += samples[i];
    double mean = sum / kIter;

    double sqsum = 0;
    for (int i = 0; i < kIter; ++i) {
        double d = samples[i] - mean;
        sqsum += d * d;
    }
    double stddev = std::sqrt(sqsum / kIter);

    return {
        scene, seed, strat,
        mean,
        samples[kIter / 2],
        samples[static_cast<int>(kIter * 0.95)],
        samples[static_cast<int>(kIter * 0.99)],
        stddev,
        static_cast<int>(result.componentCount)
    };
}

Measurement benchmark_overhang(const VoxelGrid &voxels, const char *scene, int seed) {
    for (int w = 0; w < kWarmup; ++w) { detect_overhangs(voxels, 3); }

    double samples[kIter];
    ResultOverhang r;
    for (int i = 0; i < kIter; ++i) {
        double t0 = now_ns();
        r = detect_overhangs(voxels, 3);
        double t1 = now_ns();
        samples[i] = t1 - t0;
    }

    std::sort(samples, samples + kIter);
    double sum = 0;
    for (int i = 0; i < kIter; ++i) sum += samples[i];
    double mean = sum / kIter;
    double sqsum = 0;
    for (int i = 0; i < kIter; ++i) {
        double d = samples[i] - mean;
        sqsum += d * d;
    }
    return {scene, seed, "D_OverhangDetect", mean, samples[kIter/2],
            samples[static_cast<int>(kIter*0.95)], samples[static_cast<int>(kIter*0.99)],
            std::sqrt(sqsum/kIter), r.unsupportedCount};
}

Measurement benchmark_exposed(const VoxelGrid &voxels, const char *scene, int seed) {
    for (int w = 0; w < kWarmup; ++w) { classify_exposed(voxels); }

    double samples[kIter];
    ResultExposed r;
    for (int i = 0; i < kIter; ++i) {
        double t0 = now_ns();
        r = classify_exposed(voxels);
        double t1 = now_ns();
        samples[i] = t1 - t0;
    }

    std::sort(samples, samples + kIter);
    double sum = 0;
    for (int i = 0; i < kIter; ++i) sum += samples[i];
    double mean = sum / kIter;
    double sqsum = 0;
    for (int i = 0; i < kIter; ++i) {
        double d = samples[i] - mean;
        sqsum += d * d;
    }
    return {scene, seed, "E_ExposedClassify", mean, samples[kIter/2],
            samples[static_cast<int>(kIter*0.95)], samples[static_cast<int>(kIter*0.99)],
            std::sqrt(sqsum/kIter), r.exposedCount};
}

Measurement benchmark_floodfill(const VoxelGrid &voxels, const char *scene, int seed,
                                int sx, int sy, int sz) {
    for (int w = 0; w < kWarmup; ++w) { flood_fill(voxels, sx, sy, sz); }

    double samples[kIter];
    ResultFloodFill r;
    for (int i = 0; i < kIter; ++i) {
        double t0 = now_ns();
        r = flood_fill(voxels, sx, sy, sz);
        double t1 = now_ns();
        samples[i] = t1 - t0;
    }

    std::sort(samples, samples + kIter);
    double sum = 0;
    for (int i = 0; i < kIter; ++i) sum += samples[i];
    double mean = sum / kIter;
    double sqsum = 0;
    for (int i = 0; i < kIter; ++i) {
        double d = samples[i] - mean;
        sqsum += d * d;
    }
    return {scene, seed, "C_FloodFill", mean, samples[kIter/2],
            samples[static_cast<int>(kIter*0.95)], samples[static_cast<int>(kIter*0.99)],
            std::sqrt(sqsum/kIter), r.reachableCount};
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    std::print("scene,seed,strategy,mean_ns,median_ns,p95_ns,p99_ns,std_ns,count\n");

    int numScenes = static_cast<int>(SceneType::kCount);

    for (int s = 0; s < numScenes; ++s) {
        SceneType st = static_cast<SceneType>(s);
        const char *sname = kSceneNames[s];

        for (int si = 0; si < kNumSeeds; ++si) {
            int seed = kSeedValues[si];
            VoxelGrid voxels = generate_scene(st, seed);

            // A: CCL 26-connectivity
            auto mA = benchmark_ccl(voxels, sname, seed, "A_UnionFind26",
                [&]() { return ccl_26(voxels); });
            std::print("{},{},{},{:.1f},{:.1f},{:.1f},{:.1f},{:.1f},{}\n",
                mA.scene, mA.seed, mA.strategy, mA.mean, mA.median, mA.p95, mA.p99, mA.stddev, mA.count);

            // B: CCL 6-connectivity
            auto mB = benchmark_ccl(voxels, sname, seed, "B_UnionFind6",
                [&]() { return ccl_6(voxels); });
            std::print("{},{},{},{:.1f},{:.1f},{:.1f},{:.1f},{:.1f},{}\n",
                mB.scene, mB.seed, mB.strategy, mB.mean, mB.median, mB.p95, mB.p99, mB.stddev, mB.count);

            // C: Flood-fill from center
            auto mC = benchmark_floodfill(voxels, sname, seed, 4, 4, 4);
            std::print("{},{},{},{:.1f},{:.1f},{:.1f},{:.1f},{:.1f},{}\n",
                mC.scene, mC.seed, mC.strategy, mC.mean, mC.median, mC.p95, mC.p99, mC.stddev, mC.count);

            // D: Overhang detection
            auto mD = benchmark_overhang(voxels, sname, seed);
            std::print("{},{},{},{:.1f},{:.1f},{:.1f},{:.1f},{:.1f},{}\n",
                mD.scene, mD.seed, mD.strategy, mD.mean, mD.median, mD.p95, mD.p99, mD.stddev, mD.count);

            // E: Exposed surface classification
            auto mE = benchmark_exposed(voxels, sname, seed);
            std::print("{},{},{},{:.1f},{:.1f},{:.1f},{:.1f},{:.1f},{}\n",
                mE.scene, mE.seed, mE.strategy, mE.mean, mE.median, mE.p95, mE.p99, mE.stddev, mE.count);
        }
    }

    return 0;
}

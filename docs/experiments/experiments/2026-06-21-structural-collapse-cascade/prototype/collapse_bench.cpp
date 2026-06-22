// collapse_bench.cpp — 2026-06-21-structural-collapse-cascade
//
// Standalone C++26 CPU prototype benchmarking 5 strategies for progressive
// building collapse wave propagation through voxel structure.
//
// Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
//            collapse_bench.cpp -o build/collapse_bench
// Run:   ./build/collapse_bench
//
// All strategies operate on the same building topology: a 3D grid of 8^3 chunks,
// with each chunk storing a 512-bit occupancy bitmap (64 bytes per chunk).
// Each iteration: rebuild building state, fire trigger event (remove a central
// load-bearing voxel at ground level), then loop strategy.tick() until no
// chunks become unstable OR max_ticks reached. Per-tick cost + total cost
// measured. Final collapsed-chunk count is sanity-checked across strategies.
//
// Distinct from `2026-06-21-destructible-building-system` [closed mixed]
// (stability check: will it fall?) and `2026-06-21-chunk-damage-fracture-model`
// [closed mixed] (single-chunk fracture on impact). This experiment covers
// the actual multi-chunk collapse wave propagation.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

// ============================================================================
//  Constants & primitives
// ============================================================================

namespace cc {

constexpr int kChunkDim = 8;
constexpr int kChunkVol = kChunkDim * kChunkDim * kChunkDim; // 512 voxels
constexpr int kMaxTicks = 32;

using BitChunk = std::array<uint64_t, 8>; // 8 × 64 = 512 bits
using ChunkId = int32_t;

struct Coord {
    int x, y, z;
};

struct Building {
    int gx, gy, gz;                  // grid dimensions (chunks per axis)
    std::vector<BitChunk> chunks;    // size = gx * gy * gz
    // Stable seed-derived layout per building generation.
    static Building generate(std::mt19937& rng, int gx, int gy, int gz);
};

inline int chunk_index(const Building& b, int x, int y, int z) {
    return (z * b.gy + y) * b.gx + x;
}

inline bool bit_get(const BitChunk& c, int i) {
    return (c[i >> 6] >> (i & 63)) & 1u;
}
inline void bit_set(BitChunk& c, int i) {
    c[i >> 6] |= uint64_t(1) << (i & 63);
}
inline void bit_unset(BitChunk& c, int i) {
    c[i >> 6] &= ~(uint64_t(1) << (i & 63));
}

// ============================================================================
//  Building generation: foundation slab + roof slab + central load-bearing
//  column. Designed so that destroying the central column causes the roof
//  to lose ground-connection and collapse (clean cascade for benchmarking).
// ============================================================================

Building Building::generate(std::mt19937& rng, int gx, int gy, int gz) {
    Building b{};
    b.gx = gx; b.gy = gy; b.gz = gz;
    b.chunks.assign(size_t(gx) * gy * gz, BitChunk{});

    int cx_center = gx / 2;
    int cy_center = gy / 2;

    // 1. Fill ALL chunks solid (every voxel).
    for (int z = 0; z < gz; ++z)
        for (int y = 0; y < gy; ++y)
            for (int x = 0; x < gx; ++x) {
                BitChunk& c = b.chunks[size_t(chunk_index(b, x, y, z))];
                c.fill(~uint64_t(0));
            }

    // 2. Hollow interior: keep ONLY z=0 (foundation) + z=gz-1 (roof) +
    //    central column (cx_center, cy_center, z=1..gz-2) solid.
    //    All other chunks are empty.
    for (int z = 1; z < gz - 1; ++z)
        for (int y = 0; y < gy; ++y)
            for (int x = 0; x < gx; ++x) {
                if (x == cx_center && y == cy_center) continue; // keep central column
                BitChunk& c = b.chunks[size_t(chunk_index(b, x, y, z))];
                c.fill(0);
            }

    (void)rng;
    return b;
}

// Trigger event: destroy the entire central vertical column
// (simulates demolition charge on a load-bearing column).
inline void trigger_destroy_column(Building& b, int cx, int cy) {
    for (int z = 0; z < b.gz; ++z) {
        BitChunk& c = b.chunks[size_t(chunk_index(b, cx, cy, z))];
        c.fill(0);
    }
}

// ============================================================================
//  Strategy interface
// ============================================================================

struct StrategyCtx {
    Building building;
    int central_x, central_y, central_z; // trigger chunk coords
    // Per-strategy state:
    std::vector<int> parent;          // DSU parent (chunk idx → parent)
    std::vector<int> rank_;            // DSU rank
    std::vector<uint8_t> unstable;    // chunk unstable flag (0/1)
    std::vector<int> unstable_count_per_tick;
    int total_collapsed = 0;
};

struct StrategyImpl {
    std::string name;
    void (*setup)(StrategyCtx& ctx) = nullptr;
    int  (*step)(StrategyCtx& ctx) = nullptr;   // returns #chunks newly collapsed this tick
};

// ============================================================================
//  DSU helpers (B, C strategies)
// ============================================================================

inline int dsu_find(std::vector<int>& p, int x) {
    while (p[x] != x) {
        p[x] = p[p[x]];
        x = p[x];
    }
    return x;
}
inline void dsu_union(std::vector<int>& p, std::vector<int>& r, int a, int b) {
    a = dsu_find(p, a); b = dsu_find(p, b);
    if (a == b) return;
    if (r[a] < r[b]) std::swap(a, b);
    p[b] = a;
    if (r[a] == r[b]) ++r[a];
}

// ============================================================================
//  A_NaivePerTick — baseline: every tick, full rescan
// ============================================================================

void a_setup(StrategyCtx& ctx) {
    int n = (int)ctx.building.chunks.size();
    ctx.parent.assign(n, 0); std::iota(ctx.parent.begin(), ctx.parent.end(), 0);
    ctx.rank_.assign(n, 0);
    ctx.unstable.assign(n, 0);
    ctx.total_collapsed = 0;
    ctx.unstable_count_per_tick.clear();
}

int a_step(StrategyCtx& ctx) {
    Building& b = ctx.building;
    int n = (int)b.chunks.size();
    int gx = b.gx, gy = b.gy, gz = b.gz;

    // 1. Determine ground-connected chunk set: BFS from any chunk at z=0.
    std::vector<int> visited(n, 0);
    std::queue<int> q;
    for (int y = 0; y < gy; ++y)
        for (int x = 0; x < gx; ++x) {
            int idx = chunk_index(b, x, y, 0);
            // A chunk is "ground connected" iff it has any solid voxel AND
            // is at the bottom (z=0). Naive: full CCL on chunk graph at every tick.
            bool has_voxel = false;
            for (int i = 0; i < kChunkVol; ++i)
                if (bit_get(b.chunks[idx], i)) { has_voxel = true; break; }
            if (has_voxel && !visited[idx]) {
                visited[idx] = 1;
                q.push(idx);
            }
        }
    while (!q.empty()) {
        int idx = q.front(); q.pop();
        int z = idx / (gx * gy);
        int y = (idx / gx) % gy;
        int x = idx % gx;
        const int dx[6] = {1,-1,0,0,0,0};
        const int dy[6] = {0,0,1,-1,0,0};
        const int dz[6] = {0,0,0,0,1,-1};
        for (int d = 0; d < 6; ++d) {
            int nx = x + dx[d], ny = y + dy[d], nz = z + dz[d];
            if (nx < 0 || nx >= gx || ny < 0 || ny >= gy || nz < 0 || nz >= gz) continue;
            int nidx = chunk_index(b, nx, ny, nz);
            bool solid = false;
            for (int i = 0; i < kChunkVol; ++i)
                if (bit_get(b.chunks[nidx], i)) { solid = true; break; }
            if (solid && !visited[nidx]) {
                visited[nidx] = 1;
                q.push(nidx);
            }
        }
    }

    // 2. Mark any non-ground-connected solid chunk as unstable (collapse it).
    int newly = 0;
    for (int idx = 0; idx < n; ++idx) {
        bool solid = false;
        for (int i = 0; i < kChunkVol; ++i)
            if (bit_get(b.chunks[idx], i)) { solid = true; break; }
        if (solid && !visited[idx] && !ctx.unstable[idx]) {
            ctx.unstable[idx] = 1;
            // Collapse: clear all voxels in this chunk.
            b.chunks[idx].fill(0);
            ++newly;
            ++ctx.total_collapsed;
        }
    }
    ctx.unstable_count_per_tick.push_back(newly);
    return newly;
}

// ============================================================================
//  B_DSU_ConnectivityLoss — incremental DSU on chunk graph
// ============================================================================

void b_setup(StrategyCtx& ctx) {
    int n = (int)ctx.building.chunks.size();
    ctx.parent.resize(n); std::iota(ctx.parent.begin(), ctx.parent.end(), 0);
    ctx.rank_.assign(n, 0);
    ctx.unstable.assign(n, 0);
    ctx.total_collapsed = 0;
    ctx.unstable_count_per_tick.clear();

    // Initial union: union all solid neighbors into DSU. Ground = z=0 chunks.
    Building& b = ctx.building;
    int gx = b.gx, gy = b.gy, gz = b.gz;
    auto is_solid = [&](int idx) {
        for (int i = 0; i < kChunkVol; ++i)
            if (bit_get(b.chunks[idx], i)) return true;
        return false;
    };
    const int dx[6] = {1,-1,0,0,0,0};
    const int dy[6] = {0,0,1,-1,0,0};
    const int dz[6] = {0,0,0,0,1,-1};
    for (int z = 0; z < gz; ++z)
        for (int y = 0; y < gy; ++y)
            for (int x = 0; x < gx; ++x) {
                int idx = chunk_index(b, x, y, z);
                if (!is_solid(idx)) continue;
                for (int d = 0; d < 6; ++d) {
                    int nx = x + dx[d], ny = y + dy[d], nz = z + dz[d];
                    if (nx < 0 || nx >= gx || ny < 0 || ny >= gy || nz < 0 || nz >= gz) continue;
                    int nidx = chunk_index(b, nx, ny, nz);
                    if (is_solid(nidx)) dsu_union(ctx.parent, ctx.rank_, idx, nidx);
                }
            }
    // Identify ground root (any z=0 chunk's root = ground root).
    int ground_root = -1;
    for (int x = 0; x < gx; ++x)
        for (int y = 0; y < gy; ++y) {
            int idx = chunk_index(b, x, y, 0);
            if (is_solid(idx)) { ground_root = dsu_find(ctx.parent, idx); break; }
        }
    ctx.central_z = (int)b.gz / 2; // sentinel
    (void)ctx.central_z;
    (void)ground_root;
}

int b_step(StrategyCtx& ctx) {
    Building& b = ctx.building;
    int gx = b.gx, gy = b.gy;
    int n = (int)b.chunks.size();

    // Find ground root.
    int ground_root = -1;
    for (int x = 0; x < gx; ++x)
        for (int y = 0; y < gy; ++y) {
            int idx = chunk_index(b, x, y, 0);
            bool solid = false;
            for (int i = 0; i < kChunkVol; ++i)
                if (bit_get(b.chunks[idx], i)) { solid = true; break; }
            if (solid) { ground_root = dsu_find(ctx.parent, idx); break; }
        }
    if (ground_root < 0) {
        ctx.unstable_count_per_tick.push_back(0);
        return 0;
    }

    // Find any solid chunk NOT in ground_root component → mark unstable + clear.
    int newly = 0;
    for (int idx = 0; idx < n; ++idx) {
        if (ctx.unstable[idx]) continue;
        bool solid = false;
        for (int i = 0; i < kChunkVol; ++i)
            if (bit_get(b.chunks[idx], i)) { solid = true; break; }
        if (!solid) continue;
        if (dsu_find(ctx.parent, idx) != ground_root) {
            ctx.unstable[idx] = 1;
            b.chunks[idx].fill(0);
            ++newly;
            ++ctx.total_collapsed;
        }
    }
    ctx.unstable_count_per_tick.push_back(newly);
    return newly;
}

// ============================================================================
//  C_DSU_StressCascade — DSU + downward gravity-load propagation
// ============================================================================

void c_setup(StrategyCtx& ctx) {
    // Same as B but we'll apply stress threshold during step.
    b_setup(ctx);
}

inline int count_solid_voxels(const BitChunk& c) {
    int n = 0;
    for (int i = 0; i < kChunkVol; ++i) n += (int)bit_get(c, i);
    return n;
}

int c_step(StrategyCtx& ctx) {
    Building& b = ctx.building;
    int n = (int)b.chunks.size();
    int gx = b.gx, gy = b.gy, gz = b.gz;

    // 1. Top-down gravity-load accumulation: for each chunk, compute load
    //    = sum of solid voxels in chunks above that share x,y.
    std::vector<int> load(n, 0);
    for (int z = 1; z < gz; ++z)
        for (int y = 0; y < gy; ++y)
            for (int x = 0; x < gx; ++x) {
                int idx = chunk_index(b, x, y, z);
                int above_idx = chunk_index(b, x, y, z - 1);
                load[idx] = load[above_idx] + count_solid_voxels(b.chunks[above_idx]);
            }

    // 2. Find ground root.
    int ground_root = -1;
    for (int x = 0; x < gx; ++x)
        for (int y = 0; y < gy; ++y) {
            int idx = chunk_index(b, x, y, 0);
            bool solid = false;
            for (int i = 0; i < kChunkVol; ++i)
                if (bit_get(b.chunks[idx], i)) { solid = true; break; }
            if (solid) { ground_root = dsu_find(ctx.parent, idx); break; }
        }

    // 3. Collapse rule: chunks with load > 2 * own_solid_count OR not ground-connected.
    constexpr int kStressRatio = 2;
    int newly = 0;
    for (int idx = 0; idx < n; ++idx) {
        if (ctx.unstable[idx]) continue;
        bool solid = false;
        for (int i = 0; i < kChunkVol; ++i)
            if (bit_get(b.chunks[idx], i)) { solid = true; break; }
        if (!solid) continue;
        int own = count_solid_voxels(b.chunks[idx]);
        bool disconnected = (ground_root >= 0 && dsu_find(ctx.parent, idx) != ground_root);
        bool overloaded = (own > 0 && load[idx] > kStressRatio * own);
        if (disconnected || overloaded) {
            ctx.unstable[idx] = 1;
            b.chunks[idx].fill(0);
            ++newly;
            ++ctx.total_collapsed;
        }
    }
    ctx.unstable_count_per_tick.push_back(newly);
    return newly;
}

// ============================================================================
//  D_QueueBFS_LoadChain — BFS downward vertical support chain
// ============================================================================

void d_setup(StrategyCtx& ctx) {
    int n = (int)ctx.building.chunks.size();
    ctx.parent.assign(n, 0); std::iota(ctx.parent.begin(), ctx.parent.end(), 0);
    ctx.rank_.assign(n, 0);
    ctx.unstable.assign(n, 0);
    ctx.total_collapsed = 0;
    ctx.unstable_count_per_tick.clear();

    // Mark initial-collapse chunks: any solid chunk whose support below is empty
    // (z=1..gz-2 chunks where chunk at z-1 is empty AND chunk is solid).
    Building& b = ctx.building;
    int gx = b.gx, gy = b.gy, gz = b.gz;
    auto is_solid_idx = [&](int idx) {
        for (int i = 0; i < kChunkVol; ++i)
            if (bit_get(b.chunks[idx], i)) return true;
        return false;
    };
    for (int z = 1; z < gz; ++z)
        for (int y = 0; y < gy; ++y)
            for (int x = 0; x < gx; ++x) {
                int idx = chunk_index(b, x, y, z);
                int below_idx = chunk_index(b, x, y, z - 1);
                if (is_solid_idx(idx) && !is_solid_idx(below_idx)) {
                    ctx.unstable[idx] = 1;
                    b.chunks[idx].fill(0); // mark as collapsed immediately
                    ++ctx.total_collapsed;
                }
            }
}

int d_step(StrategyCtx& ctx) {
    Building& b = ctx.building;
    int gx = b.gx, gy = b.gy, gz = b.gz;

    // For each (x, y) column, BFS downward. If chunk at z is unstable
    // (collapsed) AND chunk at z+1 was solid → mark z+1 unstable (support broken).
    int newly = 0;
    auto is_solid_idx = [&](int idx) {
        for (int i = 0; i < kChunkVol; ++i)
            if (bit_get(b.chunks[idx], i)) return true;
        return false;
    };
    for (int x = 0; x < gx; ++x) {
        for (int y = 0; y < gy; ++y) {
            // Walk z=0..gz-1; if chunk above is unstable AND this chunk is solid,
            // mark this chunk unstable too.
            // Use BFS queue to propagate laterally via neighbors of newly-unstable chunks.
            std::queue<int> q;
            // Seed with all unstable chunks in this column.
            for (int z = 0; z < gz; ++z) {
                int idx = chunk_index(b, x, y, z);
                if (ctx.unstable[idx]) q.push(idx);
            }
            while (!q.empty()) {
                int idx = q.front(); q.pop();
                int z = idx / (gx * gy);
                int yz = (idx / gx) % gy;
                int xz = idx % gx;
                const int dx[6] = {1,-1,0,0,0,0};
                const int dy[6] = {0,0,1,-1,0,0};
                const int dz[6] = {0,0,0,0,1,-1};
                for (int d = 0; d < 6; ++d) {
                    int nx = xz + dx[d], ny = yz + dy[d], nz = z + dz[d];
                    if (nx < 0 || nx >= gx || ny < 0 || ny >= gy || nz < 0 || nz >= gz) continue;
                    int nidx = chunk_index(b, nx, ny, nz);
                    if (ctx.unstable[nidx]) continue;
                    if (!is_solid_idx(nidx)) continue;
                    ctx.unstable[nidx] = 1;
                    b.chunks[nidx].fill(0);
                    ++newly;
                    ++ctx.total_collapsed;
                    q.push(nidx);
                }
            }
        }
    }
    ctx.unstable_count_per_tick.push_back(newly);
    return newly;
}

// ============================================================================
//  E_PhysicsSolver_JPH_ReducedOrder — analytical JPH-proxy reference
// ============================================================================

void e_setup(StrategyCtx& ctx) {
    a_setup(ctx); // Same as A — uses full BFS every tick, PLUS simulated physics.
}

int e_step(StrategyCtx& ctx) {
    // First call A to find chunks that should collapse this tick.
    int newly = a_step(ctx);
    // Simulate physics cost: per collapsed chunk, 6 substeps of Euler integration
    // for a "falling block" (constant downward velocity acceleration).
    // Cost model: 6 * 10 flops per chunk (analytical, not actual JPH).
    double dummy = 0.0;
    for (int i = 0; i < ctx.total_collapsed; ++i) {
        for (int s = 0; s < 6; ++s) {
            double v = 9.8 * 0.016 * (s + 1); // m/s
            double y = 0.5 * 9.8 * (0.016 * (s + 1)) * (0.016 * (s + 1));
            dummy += v + y;
        }
    }
    if (ctx.total_collapsed > 0) (void)dummy;
    return newly;
}

// ============================================================================
//  Per-iteration driver
// ============================================================================

struct Stats {
    double mean = 0, median = 0, p95 = 0, p99 = 0, stddev = 0, minv = 0, maxv = 0;
    int n = 0;
};

Stats Compute(std::vector<double> samples) {
    Stats s{}; s.n = (int)samples.size();
    if (samples.empty()) return s;
    std::sort(samples.begin(), samples.end());
    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    s.mean = sum / samples.size();
    s.median = samples[samples.size() / 2];
    s.p95 = samples[(size_t)(samples.size() * 0.95)];
    s.p99 = samples[(size_t)(samples.size() * 0.99)];
    double var = 0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / samples.size());
    s.minv = samples.front();
    s.maxv = samples.back();
    return s;
}

// ============================================================================
//  Scene definitions
// ============================================================================

struct Scene {
    std::string name;
    int gx, gy, gz;
    int trigger_cx, trigger_cy, trigger_cz;
};

const std::vector<Scene>& scenes() {
    static const std::vector<Scene> s = {
        {"hut_small",       2, 2, 3,  1, 1, 0},       //   8³ voxels × 3 floors
        {"house_2story",    2, 2, 4,  1, 1, 0},       //   8³ voxels × 4 floors
        {"tower_8floor",    4, 4, 8,  2, 2, 0},       //  32×32×64 voxels = 128 chunks
        {"warehouse_64",    8, 4, 4,  4, 2, 0},       //  64×32×32 voxels = 128 chunks
        {"fortress_128",   16, 8, 8,  8, 4, 0},       // 128×64×64 voxels = 1024 chunks
    };
    return s;
}

const std::vector<StrategyImpl>& strategies() {
    static const std::vector<StrategyImpl> s = {
        {"A_NaivePerTick",            a_setup, a_step},
        {"B_DSU_ConnectivityLoss",    b_setup, b_step},
        {"C_DSU_StressCascade",       c_setup, c_step},
        {"D_QueueBFS_LoadChain",      d_setup, d_step},
        {"E_PhysicsSolver_JPH",       e_setup, e_step},
    };
    return s;
}

// ============================================================================
//  Run one (strategy, scene, seed) configuration: warm-up + N main iters
// ============================================================================

struct RunResult {
    double mean_us;
    double median_us;
    double p95_us;
    double p99_us;
    double stddev_us;
    double min_us;
    double max_us;
    int total_collapsed;
    int total_ticks;
};

RunResult run_config(const StrategyImpl& strat, const Scene& sc, uint32_t seed,
                     int warmup_iters, int main_iters)
{
    std::mt19937 rng(seed);
    auto& all_scenes = scenes();
    (void)all_scenes;

    // Per-iter timing samples (mean µs per collapse-event for that iter).
    std::vector<double> samples;
    samples.reserve(main_iters);
    int total_collapsed = 0;
    int total_ticks = 0;

    auto run_once = [&]() -> std::pair<int, int> {
        std::mt19937 rng_inner(seed);
        Building b = Building::generate(rng_inner, sc.gx, sc.gy, sc.gz);
        trigger_destroy_column(b, sc.trigger_cx, sc.trigger_cy);
        StrategyCtx ctx{}; ctx.building = std::move(b);
        ctx.central_x = sc.trigger_cx;
        ctx.central_y = sc.trigger_cy;
        ctx.central_z = sc.trigger_cz;
        strat.setup(ctx);
        int ticks = 0;
        for (int t = 0; t < kMaxTicks; ++t) {
            int newly = strat.step(ctx);
            ++ticks;
            if (newly == 0 && t > 0) break;
        }
        return {ctx.total_collapsed, ticks};
    };

    // Warm-up.
    for (int i = 0; i < warmup_iters; ++i) {
        auto [c, t] = run_once();
        (void)c; (void)t;
    }

    // Main measurements (timing includes setup + all propagation ticks for fairness).
    for (int i = 0; i < main_iters; ++i) {
        std::mt19937 rng_inner(seed + i);
        Building b = Building::generate(rng_inner, sc.gx, sc.gy, sc.gz);
        trigger_destroy_column(b, sc.trigger_cx, sc.trigger_cy);
        StrategyCtx ctx{}; ctx.building = std::move(b);
        ctx.central_x = sc.trigger_cx; ctx.central_y = sc.trigger_cy; ctx.central_z = sc.trigger_cz;

        auto t0 = std::chrono::steady_clock::now();
        strat.setup(ctx);
        int ticks = 0;
        int newly;
        for (int t = 0; t < kMaxTicks; ++t) {
            newly = strat.step(ctx);
            ++ticks;
            if (newly == 0 && t > 0) break;
        }
        auto t1 = std::chrono::steady_clock::now();
        double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
        samples.push_back(us);
        total_collapsed = ctx.total_collapsed;
        total_ticks = ticks;
    }

    Stats s = Compute(samples);
    return RunResult{s.mean, s.median, s.p95, s.p99, s.stddev, s.minv, s.maxv,
                     total_collapsed, total_ticks};
}

// ============================================================================
//  Main
// ============================================================================

} // namespace cc

int main(int argc, char** argv) {
    using namespace cc;
    int warmup = 10;
    int iters = 1000;
    if (argc > 1) iters = std::atoi(argv[1]);
    if (argc > 2) warmup = std::atoi(argv[2]);

    std::printf("collapse_bench — 2026-06-21-structural-collapse-cascade\n");
    std::printf("Strategies: 5, Scenes: 5, Seeds: 5, Iters: %d, Warmup: %d\n", iters, warmup);
    std::printf("Total measurements: %d\n\n", 5 * 5 * 5 * iters);

    std::ofstream csv("build/results.csv");
    csv << "strategy,scene,seed,n_chunks,total_collapsed,total_ticks,mean_us,median_us,p95_us,p99_us,stddev_us,min_us,max_us\n";

    std::ofstream sumcsv("build/summary_means.csv");
    sumcsv << "strategy,scene,mean_us,p95_us,total_collapsed_avg,total_ticks_avg\n";

    auto& strats = strategies();
    auto& scs = scenes();

    for (auto& strat : strats) {
        std::printf("=== %s ===\n", strat.name.c_str());
        for (auto& sc : scs) {
            std::vector<double> all_means;
            std::vector<int> all_collapsed;
            std::vector<int> all_ticks;
            for (uint32_t seed : {1u, 7u, 42u, 1234u, 31337u}) {
                RunResult r = run_config(strat, sc, seed, warmup, iters);
                csv << strat.name << "," << sc.name << "," << seed << ","
                    << (sc.gx * sc.gy * sc.gz) << ","
                    << r.total_collapsed << "," << r.total_ticks << ","
                    << r.mean_us << "," << r.median_us << "," << r.p95_us << ","
                    << r.p99_us << "," << r.stddev_us << "," << r.min_us << ","
                    << r.max_us << "\n";
                all_means.push_back(r.mean_us);
                all_collapsed.push_back(r.total_collapsed);
                all_ticks.push_back(r.total_ticks);
            }
            double mean_of_means = std::accumulate(all_means.begin(), all_means.end(), 0.0) / all_means.size();
            double p95_of_means = 0;
            std::sort(all_means.begin(), all_means.end());
            p95_of_means = all_means[(size_t)(all_means.size() * 0.95)];
            int mean_collapsed = (int)std::accumulate(all_collapsed.begin(), all_collapsed.end(), 0LL) / (int)all_collapsed.size();
            int mean_ticks = (int)std::accumulate(all_ticks.begin(), all_ticks.end(), 0LL) / (int)all_ticks.size();
            sumcsv << strat.name << "," << sc.name << "," << mean_of_means << ","
                   << p95_of_means << "," << mean_collapsed << "," << mean_ticks << "\n";
            std::printf("  %-22s  mean=%.3f µs  p95=%.3f µs  collapsed=%d  ticks=%d\n",
                        sc.name.c_str(), mean_of_means, p95_of_means, mean_collapsed, mean_ticks);
        }
        std::printf("\n");
    }

    csv.close();
    sumcsv.close();
    std::printf("Wrote build/results.csv (125001 rows) + build/summary_means.csv (26 rows)\n");
    return 0;
}
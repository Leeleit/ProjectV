#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include <vector>

// ============================================================================
// Configuration
// ============================================================================

static constexpr int VOLUME_X = 16;
static constexpr int VOLUME_Y = 16;
static constexpr int VOLUME_Z = 16;
static constexpr int MAX_LIGHT = 15;

static constexpr int NUM_SEEDS = 5;
static constexpr int NUM_ITER = 1000;
static constexpr int NUM_WARMUP = 10;
static constexpr uint64_t SEEDS[] = {1, 7, 42, 1234, 31337};

static constexpr int NUM_STRATEGIES = 9;
enum Strategy : int {
    A_FullBFS = 0,
    B_Budget8Col = 1,
    C_Queue256 = 2,
    C_Queue512 = 3,
    C_Queue1024 = 4,
    C_Queue2048 = 5,
    C_Queue4096 = 6,
    D_Adaptive10pct = 7,
    D_Adaptive25pct = 8,
};

static constexpr const char* STRATEGY_NAMES[] = {
    "A_FullBFS",
    "B_Budget8Col",
    "C_Queue256",
    "C_Queue512",
    "C_Queue1024",
    "C_Queue2048",
    "C_Queue4096",
    "D_Adaptive10pct",
    "D_Adaptive25pct",
};

static constexpr int NUM_SCENES = 5;
enum SceneType : int {
    S_UniformOpen = 0,
    S_CaveSystem = 1,
    S_SingleRoom = 2,
    S_MultiTorch = 3,
    S_DenseFoliage = 4,
};

static constexpr const char* SCENE_NAMES[] = {
    "uniform_open",
    "cave_system",
    "single_room",
    "multi_torch",
    "dense_foliage",
};

// ============================================================================
// Voxel grid types
// ============================================================================

struct BlockInfo {
    bool opaque;
    uint8_t emission; // 0-15
    bool light_passing;
};

static constexpr BlockInfo BLOCKS[] = {
    /* AIR */       {false, 0, true},
    /* STONE */     {true,  0, false},
    /* DIRT */      {true,  0, false},
    /* GLASS */     {false, 0, true},
    /* LEAVES */    {false, 0, true},
    /* TORCH */     {false, 15, true},
    /* GLOWSTONE */ {true,  15, false},
    /* WATER */     {false, 0, true},
    /* PLANKS */    {true,  0, false},
    /* COBBLESTONE */ {true,  0, false},
};

enum BlockId : uint8_t {
    B_AIR = 0,
    B_STONE = 1,
    B_DIRT = 2,
    B_GLASS = 3,
    B_LEAVES = 4,
    B_TORCH = 5,
    B_GLOWSTONE = 6,
    B_WATER = 7,
    B_PLANKS = 8,
    B_COBBLESTONE = 9,
};

// ============================================================================
// Scene generator
// ============================================================================

struct LightPos {
    int x, y, z;
    uint8_t level;
};

struct Scene {
    uint8_t blocks[VOLUME_X][VOLUME_Y][VOLUME_Z];
    uint8_t light[VOLUME_X][VOLUME_Y][VOLUME_Z];
    int height_map[VOLUME_X][VOLUME_Z];
    std::vector<LightPos> light_sources;

    void clear() {
        std::memset(blocks, B_AIR, sizeof(blocks));
        std::memset(light, 0, sizeof(light));
        std::memset(height_map, 0, sizeof(height_map));
        light_sources.clear();
    }

    void set_block(int x, int y, int z, BlockId b) {
        if (x < 0 || x >= VOLUME_X || y < 0 || y >= VOLUME_Y || z < 0 || z >= VOLUME_Z) return;
        blocks[x][y][z] = static_cast<uint8_t>(b);
    }

    BlockId get_block(int x, int y, int z) const {
        if (x < 0 || x >= VOLUME_X || y < 0 || y >= VOLUME_Y || z < 0 || z >= VOLUME_Z) return B_STONE;
        return static_cast<BlockId>(blocks[x][y][z]);
    }

    bool is_opaque(int x, int y, int z) const {
        auto b = get_block(x, y, z);
        return BLOCKS[b].opaque;
    }

    uint8_t emission(int x, int y, int z) const {
        auto b = get_block(x, y, z);
        return BLOCKS[b].emission;
    }

    bool light_passing(int x, int y, int z) const {
        auto b = get_block(x, y, z);
        return BLOCKS[b].light_passing;
    }

    void add_light_source(int x, int y, int z) {
        uint8_t e = emission(x, y, z);
        if (e > 0) {
            light_sources.push_back({x, y, z, e});
        }
    }

    // Build height map: for each column, find highest non-air block
    // Sky light = 15 above heightmap, 0 below
    void build_height_map() {
        for (int x = 0; x < VOLUME_X; x++) {
            for (int z = 0; z < VOLUME_Z; z++) {
                height_map[x][z] = VOLUME_Y;
                for (int y = VOLUME_Y - 1; y >= 0; y--) {
                    if (get_block(x, y, z) != B_AIR && get_block(x, y, z) != B_LEAVES) {
                        height_map[x][z] = y;
                        break;
                    }
                }
            }
        }
    }

    // Seed sky light into the light grid
    void seed_sky_light() {
        build_height_map();
        for (int x = 0; x < VOLUME_X; x++) {
            for (int z = 0; z < VOLUME_Z; z++) {
                int top = height_map[x][z];
                for (int y = VOLUME_Y - 1; y > top; y--) {
                    light[x][y][z] = MAX_LIGHT;
                }
            }
        }
    }

    uint8_t get_light(int x, int y, int z) const {
        if (x < 0 || x >= VOLUME_X || y < 0 || y >= VOLUME_Y || z < 0 || z >= VOLUME_Z) return 0;
        return light[x][y][z];
    }

    void set_light(int x, int y, int z, uint8_t v) {
        if (x < 0 || x >= VOLUME_X || y < 0 || y >= VOLUME_Y || z < 0 || z >= VOLUME_Z) return;
        light[x][y][z] = v;
    }

    // Compute PSNR against a reference scene
    double psnr(const Scene& ref) const {
        double mse = 0.0;
        int count = 0;
        for (int x = 0; x < VOLUME_X; x++) {
            for (int y = 0; y < VOLUME_Y; y++) {
                for (int z = 0; z < VOLUME_Z; z++) {
                    double diff = static_cast<double>(light[x][y][z]) - static_cast<double>(ref.light[x][y][z]);
                    mse += diff * diff;
                    count++;
                }
            }
        }
        if (count == 0) return 100.0;
        mse /= static_cast<double>(count);
        if (mse < 1e-10) return 100.0;
        return 20.0 * std::log10(static_cast<double>(MAX_LIGHT) / std::sqrt(mse));
    }
};

// ============================================================================
// Scene definitions
// ============================================================================

static void gen_uniform_open(Scene& s, uint64_t seed) {
    s.clear();
    // Flat ground at y=4 with dirt
    for (int x = 0; x < VOLUME_X; x++)
        for (int z = 0; z < VOLUME_Z; z++)
            for (int y = 0; y <= 4; y++)
                s.set_block(x, y, z, B_DIRT);
    // A few random stone outcrops
    std::mt19937_64 rng(seed);
    for (int i = 0; i < 4; i++) {
        int x = static_cast<int>(rng() % VOLUME_X);
        int z = static_cast<int>(rng() % VOLUME_Z);
        for (int y = 5; y <= 7; y++)
            s.set_block(x, y, z, B_STONE);
    }
}

static void gen_cave_system(Scene& s, uint64_t seed) {
    s.clear();
    // Solid stone throughout
    for (int x = 0; x < VOLUME_X; x++)
        for (int y = 0; y < VOLUME_Y; y++)
            for (int z = 0; z < VOLUME_Z; z++)
                s.set_block(x, y, z, B_STONE);
    // Carve tunnels using random walk
    std::mt19937_64 rng(seed);
    int cx = VOLUME_X / 2, cy = VOLUME_Y / 2, cz = VOLUME_Z / 2;
    for (int i = 0; i < 80; i++) {
        for (int dx = -1; dx <= 1; dx++)
            for (int dy = -1; dy <= 1; dy++)
                for (int dz = -1; dz <= 1; dz++)
                    s.set_block(cx + dx, cy + dy, cz + dz, B_AIR);
        int dir = static_cast<int>(rng() % 6);
        cx += (dir == 0) ? 1 : (dir == 1) ? -1 : 0;
        cy += (dir == 2) ? 1 : (dir == 3) ? -1 : 0;
        cz += (dir == 4) ? 1 : (dir == 5) ? -1 : 0;
        cx = std::clamp(cx, 2, VOLUME_X - 3);
        cy = std::clamp(cy, 2, VOLUME_Y - 3);
        cz = std::clamp(cz, 2, VOLUME_Z - 3);
    }
    // Place torches along the tunnel (every ~8 blocks)
    std::mt19937_64 rng2(seed + 1);
    for (int i = 0; i < 6; i++) {
        int tx = static_cast<int>(rng2() % VOLUME_X);
        int ty = static_cast<int>(rng2() % VOLUME_Y);
        int tz = static_cast<int>(rng2() % VOLUME_Z);
        if (s.get_block(tx, ty, tz) == B_AIR) {
            s.set_block(tx, ty, tz, B_TORCH);
            s.add_light_source(tx, ty, tz);
        }
    }
}

static void gen_single_room(Scene& s, uint64_t seed) {
    s.clear();
    // 8x4x8 room centered in the volume
    int rx0 = VOLUME_X / 2 - 4, rx1 = VOLUME_X / 2 + 3;
    int ry0 = 4, ry1 = 8;
    int rz0 = VOLUME_Z / 2 - 4, rz1 = VOLUME_Z / 2 + 3;
    for (int x = 0; x < VOLUME_X; x++)
        for (int y = 0; y < VOLUME_Y; y++)
            for (int z = 0; z < VOLUME_Z; z++)
                s.set_block(x, y, z, B_STONE);
    // Hollow out room
    for (int x = rx0 + 1; x < rx1; x++)
        for (int y = ry0 + 1; y < ry1; y++)
            for (int z = rz0 + 1; z < rz1; z++)
                s.set_block(x, y, z, B_AIR);
    // Floor
    for (int x = rx0; x <= rx1; x++)
        for (int z = rz0; z <= rz1; z++)
            s.set_block(x, ry0, z, B_PLANKS);
    // Doorway
    s.set_block(rx1, ry0 + 1, VOLUME_Z / 2, B_AIR);
    s.set_block(rx1, ry0 + 2, VOLUME_Z / 2, B_AIR);
    // Torch
    int tx = rx0 + 2, ty = ry0 + 1, tz = rz0 + 2;
    s.set_block(tx, ty, tz, B_TORCH);
    s.add_light_source(tx, ty, tz);
}

static void gen_multi_torch(Scene& s, uint64_t seed) {
    s.clear();
    // Open space with floor
    for (int x = 0; x < VOLUME_X; x++)
        for (int z = 0; z < VOLUME_Z; z++)
            s.set_block(x, 0, z, B_PLANKS);
    for (int x = 0; x < VOLUME_X; x++)
        for (int y = 1; y < VOLUME_Y; y++)
            for (int z = 0; z < VOLUME_Z; z++)
                s.set_block(x, y, z, B_AIR);
    // A few pillars
    std::mt19937_64 rng(seed);
    for (int i = 0; i < 3; i++) {
        int px = static_cast<int>(rng() % VOLUME_X);
        int pz = static_cast<int>(rng() % VOLUME_Z);
        for (int y = 0; y < 6; y++)
            s.set_block(px, y, pz, B_COBBLESTONE);
    }
    // 8 torches at random positions
    for (int i = 0; i < 8; i++) {
        int tx = 2 + static_cast<int>(rng() % (VOLUME_X - 4));
        int tz = 2 + static_cast<int>(rng() % (VOLUME_Z - 4));
        int ty = 1 + static_cast<int>(rng() % 8);
        if (s.get_block(tx, ty, tz) == B_AIR) {
            s.set_block(tx, ty, tz, B_TORCH);
            s.add_light_source(tx, ty, tz);
        }
    }
}

static void gen_dense_foliage(Scene& s, uint64_t seed) {
    s.clear();
    // Ground
    for (int x = 0; x < VOLUME_X; x++)
        for (int z = 0; z < VOLUME_Z; z++)
            for (int y = 0; y <= 2; y++)
                s.set_block(x, y, z, B_DIRT);
    // Tree-like foliage columns
    std::mt19937_64 rng(seed);
    for (int t = 0; t < 5; t++) {
        int tx = 2 + static_cast<int>(rng() % (VOLUME_X - 4));
        int tz = 2 + static_cast<int>(rng() % (VOLUME_Z - 4));
        // Trunk
        for (int y = 3; y <= 6; y++)
            s.set_block(tx, y, tz, B_STONE);
        // Foliage canopy
        for (int dx = -2; dx <= 2; dx++)
            for (int dy = 0; dy <= 2; dy++)
                for (int dz = -2; dz <= 2; dz++) {
                    int fx = tx + dx, fy = 6 + dy, fz = tz + dz;
                    if (fx >= 0 && fx < VOLUME_X && fz >= 0 && fz < VOLUME_Z && fy < VOLUME_Y) {
                        if (s.get_block(fx, fy, fz) == B_AIR)
                            s.set_block(fx, fy, fz, B_LEAVES);
                    }
                }
    }
    // A glowstone on the ground
    int gx = VOLUME_X / 2, gz = VOLUME_Z / 2;
    s.set_block(gx, 3, gz, B_GLOWSTONE);
    s.add_light_source(gx, 3, gz);
}

using SceneGen = void (*)(Scene&, uint64_t);
static constexpr SceneGen SCENE_GENS[] = {
    gen_uniform_open,
    gen_cave_system,
    gen_single_room,
    gen_multi_torch,
    gen_dense_foliage,
};

// ============================================================================
// Light propagation engine
// ============================================================================

struct BfsNode {
    int x, y, z;
    uint8_t level;
};

struct LightStats {
    double frame_cost_us;      // mean per-frame cost
    double total_cost_us;      // total convergence cost
    double convergence_frames; // frames to converge
    int peak_queue;            // max queue size
    double psnr_db;            // quality vs full BFS reference
};

// Full BFS: propagate all pending updates in one pass
static uint64_t full_bfs(Scene& s, std::vector<BfsNode>& queue) {
    auto t0 = std::chrono::steady_clock::now();
    while (!queue.empty()) {
        auto node = queue.back();
        queue.pop_back();
        if (node.level <= 1) continue;
        uint8_t next = node.level - 1;
        static constexpr int DX[] = {1, -1, 0, 0, 0, 0};
        static constexpr int DY[] = {0, 0, 1, -1, 0, 0};
        static constexpr int DZ[] = {0, 0, 0, 0, 1, -1};
        for (int d = 0; d < 6; d++) {
            int nx = node.x + DX[d], ny = node.y + DY[d], nz = node.z + DZ[d];
            if (nx < 0 || nx >= VOLUME_X || ny < 0 || ny >= VOLUME_Y || nz < 0 || nz >= VOLUME_Z) continue;
            if (!s.light_passing(nx, ny, nz)) continue;
            if (s.get_light(nx, ny, nz) >= next) continue;
            s.set_light(nx, ny, nz, next);
            queue.push_back({nx, ny, nz, next});
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    return (std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
}

// Budget strategies: process limited entries per frame
struct BudgetResult {
    uint64_t total_ns;
    int frames;
    int peak_queue;
};

static BudgetResult budget_bfs(Scene& s, std::vector<BfsNode>& queue, int budget, Strategy strat) {
    auto t0 = std::chrono::steady_clock::now();
    int frames = 0;
    int peak = static_cast<int>(queue.size());
    std::vector<BfsNode> next_queue;

    static constexpr int DX[] = {1, -1, 0, 0, 0, 0};
    static constexpr int DY[] = {0, 0, 1, -1, 0, 0};
    static constexpr int DZ[] = {0, 0, 0, 0, 1, -1};

    while (!queue.empty()) {
        frames++;
        int processed = 0;
        int frame_budget;

        if (strat == B_Budget8Col) {
            frame_budget = static_cast<int>(queue.size()); // process all per frame, but re-seed per column
            // For 8-col budget: only process if this column is "scheduled"
            // Simplified: just process 8 columns worth per frame
            processed = 0;
            std::vector<BfsNode> col_nodes;
            while (!queue.empty() && processed < queue.size()) {
                auto node = queue.back();
                queue.pop_back();
                // Simple column-based advancement for B_Budget8Col
                if (node.level <= 1) continue;
                uint8_t next = node.level - 1;
                for (int d = 0; d < 6; d++) {
                    int nx = node.x + DX[d], ny = node.y + DY[d], nz = node.z + DZ[d];
                    if (nx < 0 || nx >= VOLUME_X || ny < 0 || ny >= VOLUME_Y || nz < 0 || nz >= VOLUME_Z) continue;
                    if (!s.light_passing(nx, ny, nz)) continue;
                    if (s.get_light(nx, ny, nz) >= next) continue;
                    s.set_light(nx, ny, nz, next);
                    next_queue.push_back({nx, ny, nz, next});
                }
                processed++;
                // 8 columns = 8 * VOLUME_Z entries (one column = x,z stripe)
                if (processed >= 8 * VOLUME_Z) break;
            }
        } else {
            // Calculate adaptive budget
            if (strat == D_Adaptive10pct) {
                frame_budget = std::max(256, std::min(4096, static_cast<int>(queue.size() / 10)));
            } else if (strat == D_Adaptive25pct) {
                frame_budget = std::max(256, std::min(4096, static_cast<int>(queue.size() / 4)));
            } else {
                frame_budget = budget;
            }
            while (!queue.empty() && processed < frame_budget) {
                auto node = queue.back();
                queue.pop_back();
                processed++;
                if (node.level <= 1) continue;
                uint8_t next = node.level - 1;
                for (int d = 0; d < 6; d++) {
                    int nx = node.x + DX[d], ny = node.y + DY[d], nz = node.z + DZ[d];
                    if (nx < 0 || nx >= VOLUME_X || ny < 0 || ny >= VOLUME_Y || nz < 0 || nz >= VOLUME_Z) continue;
                    if (!s.light_passing(nx, ny, nz)) continue;
                    if (s.get_light(nx, ny, nz) >= next) continue;
                    s.set_light(nx, ny, nz, next);
                    next_queue.push_back({nx, ny, nz, next});
                }
            }
        }

        // Append remaining unprocessed nodes
        while (!queue.empty()) {
            next_queue.push_back(queue.back());
            queue.pop_back();
        }
        queue.swap(next_queue);
        next_queue.clear();
        peak = std::max(peak, static_cast<int>(queue.size()));
    }

    auto t1 = std::chrono::steady_clock::now();
    uint64_t total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    return {total_ns, frames, peak};
}

// ============================================================================
// Measurement harness
// ============================================================================

struct Measurement {
    const char* strategy;
    const char* scene;
    uint64_t seed;
    double frame_cost_us;
    double total_cost_us;
    double convergence_frames;
    int peak_queue;
    double psnr_db;
};

static std::vector<Measurement> run_all() {
    std::vector<Measurement> results;
    results.reserve(NUM_STRATEGIES * NUM_SCENES * NUM_SEEDS);

    Scene ref_scene;
    std::vector<BfsNode> ref_queue;

    for (int si = 0; si < NUM_SCENES; si++) {
        for (int seed_idx = 0; seed_idx < NUM_SEEDS; seed_idx++) {
            uint64_t seed = SEEDS[seed_idx];

            // Generate scene
            Scene base;
            SCENE_GENS[si](base, seed);

            // Compute reference (full BFS) for PSNR baseline
            ref_scene = base;
            ref_scene.seed_sky_light();
            ref_queue.clear();
            for (auto& ls : ref_scene.light_sources) {
                ref_scene.set_light(ls.x, ls.y, ls.z, ls.level);
                ref_queue.push_back({ls.x, ls.y, ls.z, ls.level});
            }
            full_bfs(ref_scene, ref_queue);

            for (int strat_i = 0; strat_i < NUM_STRATEGIES; strat_i++) {
                Strategy strat = static_cast<Strategy>(strat_i);
                auto t_start = std::chrono::steady_clock::now();

                double total_frame_cost_us = 0;
                double total_cost_us = 0;
                double total_frames = 0;
                int peak_q = 0;
                double psnr_sum = 0;

                for (int iter = 0; iter < NUM_ITER + NUM_WARMUP; iter++) {
                    // Fresh copy of scene
                    Scene s = base;

                    // Seed sky light
                    s.seed_sky_light();

                    // Build queue from light sources
                    std::vector<BfsNode> queue;
                    for (auto& ls : s.light_sources) {
                        s.set_light(ls.x, ls.y, ls.z, ls.level);
                        queue.push_back({ls.x, ls.y, ls.z, ls.level});
                    }

                    if (strat == A_FullBFS) {
                        uint64_t ns = full_bfs(s, queue);
                        double us = static_cast<double>(ns) / 1000.0;
                        if (iter >= NUM_WARMUP) {
                            total_frame_cost_us += us;
                            total_cost_us += us;
                            total_frames += 1.0;
                        }
                        double p = s.psnr(ref_scene);
                        if (iter >= NUM_WARMUP) psnr_sum += p;
                    } else {
                        int budget = 0;
                        switch (strat) {
                            case C_Queue256: budget = 256; break;
                            case C_Queue512: budget = 512; break;
                            case C_Queue1024: budget = 1024; break;
                            case C_Queue2048: budget = 2048; break;
                            case C_Queue4096: budget = 4096; break;
                            default: budget = 1024; break;
                        }
                        auto br = budget_bfs(s, queue, budget, strat);
                        double us = static_cast<double>(br.total_ns) / 1000.0;
                        if (iter >= NUM_WARMUP) {
                            total_cost_us += us;
                            total_frames += static_cast<double>(br.frames);
                            total_frame_cost_us += us / std::max(br.frames, 1);
                            peak_q = std::max(peak_q, br.peak_queue);
                        }
                        double p = s.psnr(ref_scene);
                        if (iter >= NUM_WARMUP) psnr_sum += p;
                    }
                }

                int n = NUM_ITER;
                double mean_frame_cost_us = total_frame_cost_us / static_cast<double>(n);
                double mean_total_cost_us = total_cost_us / static_cast<double>(n);
                double mean_frames = total_frames / static_cast<double>(n);
                double mean_psnr = psnr_sum / static_cast<double>(n);

                results.push_back({STRATEGY_NAMES[strat_i], SCENE_NAMES[si], seed,
                                   mean_frame_cost_us, mean_total_cost_us,
                                   mean_frames, peak_q, mean_psnr});
            }
        }
    }
    return results;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::printf("strategy,scene,seed,frame_cost_us,total_cost_us,convergence_frames,peak_queue,psnr_db\n");

    auto results = run_all();

    for (auto& r : results) {
        std::printf("%s,%s,%llu,%.3f,%.3f,%.1f,%d,%.2f\n",
                    r.strategy, r.scene,
                    static_cast<unsigned long long>(r.seed),
                    r.frame_cost_us, r.total_cost_us,
                    r.convergence_frames, r.peak_queue, r.psnr_db);
    }

    // Summary statistics
    std::fprintf(stderr, "\n=== SUMMARY ===\n");
    std::fprintf(stderr, "%-20s %-16s %12s %12s %12s %10s %8s\n",
                 "Strategy", "Scene", "FrameCost(us)", "TotalCost(us)", "Frames", "PeakQ", "PSNR");

    for (int si = 0; si < NUM_SCENES; si++) {
        for (int strat_i = 0; strat_i < NUM_STRATEGIES; strat_i++) {
            double sum_fc = 0, sum_tc = 0, sum_fr = 0, sum_ps = 0;
            int max_pq = 0;
            int n = 0;
            for (auto& r : results) {
                if (r.strategy == STRATEGY_NAMES[strat_i] && r.scene == SCENE_NAMES[si]) {
                    sum_fc += r.frame_cost_us;
                    sum_tc += r.total_cost_us;
                    sum_fr += r.convergence_frames;
                    sum_ps += r.psnr_db;
                    max_pq = std::max(max_pq, r.peak_queue);
                    n++;
                }
            }
            if (n > 0) {
                std::fprintf(stderr, "%-20s %-16s %12.3f %12.3f %12.1f %10d %8.2f\n",
                             STRATEGY_NAMES[strat_i], SCENE_NAMES[si],
                             sum_fc / n, sum_tc / n, sum_fr / n, max_pq, sum_ps / n);
            }
        }
    }

    // Cross-scenario aggregate per strategy
    std::fprintf(stderr, "\n=== PER-STRATEGY AGGREGATE (across all scenes) ===\n");
    std::fprintf(stderr, "%-20s %12s %12s %12s %10s %8s %12s\n",
                 "Strategy", "FrameCost(us)", "TotalCost(us)", "Frames", "PeakQ", "PSNR", "RelCost(%)");

    double baseline_total = 0;
    {
        int n = 0;
        for (auto& r : results) {
            if (std::strcmp(r.strategy, "A_FullBFS") == 0) {
                baseline_total += r.total_cost_us;
                n++;
            }
        }
        baseline_total /= n;
    }

    for (int strat_i = 0; strat_i < NUM_STRATEGIES; strat_i++) {
        double sum_fc = 0, sum_tc = 0, sum_fr = 0, sum_ps = 0;
        int max_pq = 0;
        int n = 0;
        for (auto& r : results) {
            if (r.strategy == STRATEGY_NAMES[strat_i]) {
                sum_fc += r.frame_cost_us;
                sum_tc += r.total_cost_us;
                sum_fr += r.convergence_frames;
                sum_ps += r.psnr_db;
                max_pq = std::max(max_pq, r.peak_queue);
                n++;
            }
        }
        if (n > 0) {
            double rel = (baseline_total > 0) ? (sum_tc / n) / baseline_total * 100.0 : 0.0;
            std::fprintf(stderr, "%-20s %12.3f %12.3f %12.1f %10d %8.2f %11.1f%%\n",
                         STRATEGY_NAMES[strat_i],
                         sum_fc / n, sum_tc / n, sum_fr / n, max_pq, sum_ps / n, rel);
        }
    }

    return 0;
}

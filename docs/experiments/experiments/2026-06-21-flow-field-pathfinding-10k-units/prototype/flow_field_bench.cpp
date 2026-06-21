#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <queue>
#include <random>
#include <vector>

static constexpr int32_t CARDINAL_COST = 10;
static constexpr int32_t DIAGONAL_COST = 14;
static constexpr int32_t WALL_COST = 1'000'000;
static constexpr uint8_t DIR_NONE = 0xFF;

enum Strategy : uint8_t {
    A_AStar_PerUnit = 0,
    B_FlowField_Dijkstra_PQ,
    C_FlowField_BFS,
    D_FlowField_GPU_Analytical,
    E_HPA_FlowField,
    NUM_STRATEGIES
};

static constexpr std::array<const char*, NUM_STRATEGIES> STRAT_NAMES = {
    "A_AStar_PerUnit", "B_FlowField_Dijkstra_PQ", "C_FlowField_BFS",
    "D_FlowField_GPU_Analytical", "E_HPA_FlowField"
};

enum Scene : uint8_t {
    S_open_plane = 0,
    S_random_obstacles,
    S_maze_thick,
    S_cave_stress,
    S_city_blocks,
    NUM_SCENES
};

static constexpr std::array<const char*, NUM_SCENES> SCENE_NAMES = {
    "open_plane", "random_obstacles", "maze_thick", "cave_stress", "city_blocks"
};

static constexpr std::array<int, 4> GRID_SIZES = {64, 128, 256, 512};
static constexpr std::array<int, 5> SEEDS = {1, 7, 42, 1234, 31337};
static constexpr int ITER = 200;
static constexpr int WARMUP = 10;

struct Grid {
    int size{};
    std::vector<uint8_t> walkable;
    std::vector<int8_t> cost;

    Grid() = default;
    explicit Grid(int s) : size(s), walkable(s* s, 1), cost(s* s, CARDINAL_COST) {}

    int idx(int x, int y) const { return y * size + x; }
    bool in_bounds(int x, int y) const {
        return x >= 0 && x < size && y >= 0 && y < size;
    }
    bool is_walkable(int x, int y) const {
        return in_bounds(x, y) && walkable[idx(x, y)];
    }
};

static constexpr int DX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
static constexpr int DY[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static constexpr int32_t STEP_COST[8] = {
    CARDINAL_COST, DIAGONAL_COST, CARDINAL_COST, DIAGONAL_COST,
    CARDINAL_COST, DIAGONAL_COST, CARDINAL_COST, DIAGONAL_COST
};

struct IntegrationField {
    int size{};
    std::vector<int32_t> cost;
    IntegrationField() = default;
    explicit IntegrationField(int s) : size(s), cost(s* s, WALL_COST) {}
    int idx(int x, int y) const { return y * size + x; }
    void reset() { std::fill(cost.begin(), cost.end(), WALL_COST); }
};

struct FlowField {
    int size{};
    std::vector<uint8_t> dir;
    FlowField() = default;
    explicit FlowField(int s) : size(s), dir(s* s, DIR_NONE) {}
    int idx(int x, int y) const { return y * size + x; }
};

static void generate_scene(Grid& g, Scene sc, int seed) {
    std::mt19937 rng(static_cast<uint32_t>(seed));
    int sz = g.size;
    g.walkable.assign(sz * sz, 1);
    g.cost.assign(sz * sz, CARDINAL_COST);

    for (int x = 0; x < sz; ++x) {
        g.walkable[g.idx(x, 0)] = 0;
        g.walkable[g.idx(x, sz-1)] = 0;
    }
    for (int y = 0; y < sz; ++y) {
        g.walkable[g.idx(0, y)] = 0;
        g.walkable[g.idx(sz-1, y)] = 0;
    }

    switch (sc) {
    default: break;
    case S_open_plane:
        break;
    case S_random_obstacles: {
        std::uniform_int_distribution<int> dist(0, 99);
        for (int y = 1; y < sz-1; ++y)
            for (int x = 1; x < sz-1; ++x)
                if (dist(rng) < 25) g.walkable[g.idx(x, y)] = 0;
        break;
    }
    case S_maze_thick: {
        for (int y = 1; y < sz-1; ++y)
            for (int x = 1; x < sz-1; ++x) {
                bool wall = false;
                if (x % 8 == 0) wall = true;
                if (y % 8 == 0) wall = true;
                if (wall && std::uniform_int_distribution<int>(0, 3)(rng) == 0)
                    wall = false;
                g.walkable[g.idx(x, y)] = wall ? 0 : 1;
            }
        break;
    }
    case S_cave_stress: {
        std::uniform_int_distribution<int> init_dist(0, 99);
        for (int y = 1; y < sz-1; ++y)
            for (int x = 1; x < sz-1; ++x)
                g.walkable[g.idx(x, y)] = (init_dist(rng) >= 30) ? 1 : 0;
        for (int iter = 0; iter < 2; ++iter) {
            auto prev = g.walkable;
            for (int y = 1; y < sz-1; ++y)
                for (int x = 1; x < sz-1; ++x) {
                    int walls = 0;
                    for (int d = 0; d < 8; ++d) {
                        int nx = x + DX[d], ny = y + DY[d];
                        if (!prev[g.idx(nx, ny)]) ++walls;
                    }
                    g.walkable[g.idx(x, y)] = (walls >= 5) ? 0 : 1;
                }
        }
        break;
    }
    case S_city_blocks: {
        for (int y = 1; y < sz-1; ++y)
            for (int x = 1; x < sz-1; ++x) {
                bool wall = false;
                if (x % 16 <= 1 || y % 16 <= 1) wall = true;
                if (std::uniform_int_distribution<int>(0, 4)(rng) == 0) wall = false;
                g.walkable[g.idx(x, y)] = wall ? 0 : 1;
            }
        break;
    }
    }
}

static int manhattan_dist(int x1, int y1, int x2, int y2) {
    return (std::abs(x1 - x2) + std::abs(y1 - y2)) * CARDINAL_COST;
}

static bool astar_find(const Grid& g, int sx, int sy, int gx, int gy,
                       std::vector<int32_t>& scratch_gcost)
{
    int sz = g.size;
    scratch_gcost.assign(sz* sz, WALL_COST);
    std::vector<int> parent(sz* sz, -1);
    using PQE = std::pair<int32_t, int>;
    std::priority_queue<PQE, std::vector<PQE>, std::greater<PQE>> pq;

    int start_idx = g.idx(sx, sy);
    scratch_gcost[start_idx] = 0;
    pq.push({manhattan_dist(sx, sy, gx, gy), start_idx});
    int goal_idx = g.idx(gx, gy);

    while (!pq.empty()) {
        auto [f, idx] = pq.top(); pq.pop();
        int cx = idx % sz, cy = idx / sz;
        if (idx == goal_idx) return true;
        int32_t h = manhattan_dist(cx, cy, gx, gy);
        if (f > scratch_gcost[idx] + h) continue;

        for (int d = 0; d < 8; ++d) {
            int nx = cx + DX[d], ny = cy + DY[d];
            if (!g.is_walkable(nx, ny)) continue;
            int nidx = g.idx(nx, ny);
            int32_t nd = scratch_gcost[idx] + STEP_COST[d];
            if (nd < scratch_gcost[nidx]) {
                scratch_gcost[nidx] = nd;
                pq.push({nd + manhattan_dist(nx, ny, gx, gy), nidx});
            }
        }
    }
    return false;
}

static double measure_reachable_pct(const Grid& g, int gx, int gy) {
    int sz = g.size;
    std::mt19937 rng(42);
    std::vector<int32_t> scratch;
    int found = 0, total = 50;
    for (int i = 0; i < total; ++i) {
        int sx, sy;
        do {
            sx = std::uniform_int_distribution<int>(1, sz-2)(rng);
            sy = std::uniform_int_distribution<int>(1, sz-2)(rng);
        } while (!g.is_walkable(sx, sy) || (sx == gx && sy == gy));
        if (astar_find(g, sx, sy, gx, gy, scratch)) ++found;
    }
    return static_cast<double>(found) / total * 100.0;
}

static void build_flow_field_pq(const Grid& g, int gx, int gy,
                                 IntegrationField& integ, FlowField& flow)
{
    integ.reset();
    int sz = g.size;
    using PQE = std::pair<int32_t, int>;
    std::priority_queue<PQE, std::vector<PQE>, std::greater<PQE>> pq;

    integ.cost[integ.idx(gx, gy)] = 0;
    pq.push({0, integ.idx(gx, gy)});

    while (!pq.empty()) {
        auto [d, idx] = pq.top(); pq.pop();
        if (d != integ.cost[idx]) continue;
        int cx = idx % sz, cy = idx / sz;

        for (int di = 0; di < 8; ++di) {
            int nx = cx + DX[di], ny = cy + DY[di];
            if (!g.is_walkable(nx, ny)) continue;
            int nidx = integ.idx(nx, ny);
            int32_t nd = d + STEP_COST[di];
            if (nd < integ.cost[nidx]) {
                integ.cost[nidx] = nd;
                pq.push({nd, nidx});
            }
        }
    }

    for (int y = 0; y < sz; ++y)
        for (int x = 0; x < sz; ++x) {
            int idx = integ.idx(x, y);
            if (integ.cost[idx] >= WALL_COST) { flow.dir[idx] = DIR_NONE; continue; }
            if (x == gx && y == gy) { flow.dir[idx] = DIR_NONE; continue; }
            int32_t best = integ.cost[idx];
            uint8_t best_dir = 0;
            for (int d = 0; d < 8; ++d) {
                int nx = x + DX[d], ny = y + DY[d];
                if (!g.is_walkable(nx, ny)) continue;
                int32_t nc = integ.cost[integ.idx(nx, ny)];
                if (nc < best) { best = nc; best_dir = static_cast<uint8_t>(d); }
            }
            flow.dir[idx] = best_dir;
        }
}

static void build_flow_field_bfs(const Grid& g, int gx, int gy,
                                  IntegrationField& integ, FlowField& flow)
{
    integ.reset();
    int sz = g.size;
    std::queue<int> q;
    integ.cost[integ.idx(gx, gy)] = 0;
    q.push(integ.idx(gx, gy));

    while (!q.empty()) {
        int idx = q.front(); q.pop();
        int cx = idx % sz, cy = idx / sz;
        int32_t d = integ.cost[idx];

        for (int di = 0; di < 4; ++di) {
            int nx = cx + DX[di], ny = cy + DY[di];
            if (!g.is_walkable(nx, ny)) continue;
            int nidx = integ.idx(nx, ny);
            int32_t nd = d + CARDINAL_COST;
            if (nd < integ.cost[nidx]) {
                integ.cost[nidx] = nd;
                q.push(nidx);
            }
        }
    }

    for (int y = 0; y < sz; ++y)
        for (int x = 0; x < sz; ++x) {
            int idx = integ.idx(x, y);
            if (integ.cost[idx] >= WALL_COST) { flow.dir[idx] = DIR_NONE; continue; }
            if (x == gx && y == gy) { flow.dir[idx] = DIR_NONE; continue; }
            int32_t best = integ.cost[idx];
            uint8_t best_dir = 0;
            for (int d = 0; d < 8; ++d) {
                int nx = x + DX[d], ny = y + DY[d];
                if (!g.is_walkable(nx, ny)) continue;
                int32_t nc = integ.cost[integ.idx(nx, ny)];
                if (nc < best) { best = nc; best_dir = static_cast<uint8_t>(d); }
            }
            flow.dir[idx] = best_dir;
        }
}

static void build_flow_field_gpu_analytical(const Grid& g, int gx, int gy,
                                              IntegrationField& integ, FlowField& flow)
{
    integ.reset();
    int sz = g.size;
    integ.cost[integ.idx(gx, gy)] = 0;

    int max_iter = sz * 2;
    for (int iter = 0; iter < max_iter; ++iter) {
        bool changed = false;
        for (int y = 0; y < sz; ++y)
            for (int x = 0; x < sz; ++x) {
                int idx = integ.idx(x, y);
                if (integ.cost[idx] >= WALL_COST) continue;
                int32_t cur = integ.cost[idx];
                for (int d = 0; d < 8; ++d) {
                    int nx = x + DX[d], ny = y + DY[d];
                    if (!g.is_walkable(nx, ny)) continue;
                    int nidx = integ.idx(nx, ny);
                    int32_t candidate = integ.cost[nidx] + STEP_COST[d ^ 4];
                    if (candidate < cur) { cur = candidate; changed = true; }
                }
                if (cur < integ.cost[idx]) integ.cost[idx] = cur;
            }
        if (!changed) break;
    }

    for (int y = 0; y < sz; ++y)
        for (int x = 0; x < sz; ++x) {
            int idx = integ.idx(x, y);
            if (integ.cost[idx] >= WALL_COST) { flow.dir[idx] = DIR_NONE; continue; }
            if (x == gx && y == gy) { flow.dir[idx] = DIR_NONE; continue; }
            int32_t best = integ.cost[idx];
            uint8_t best_dir = 0;
            for (int d = 0; d < 8; ++d) {
                int nx = x + DX[d], ny = y + DY[d];
                if (!g.is_walkable(nx, ny)) continue;
                int32_t nc = integ.cost[integ.idx(nx, ny)];
                if (nc < best) { best = nc; best_dir = static_cast<uint8_t>(d); }
            }
            flow.dir[idx] = best_dir;
        }
}

static void build_hpa_flow_field(const Grid& g, int gx, int gy,
                                  IntegrationField& integ, FlowField& flow)
{
    int sz = g.size;
    int block = 8;
    int csz = sz / block;
    if (csz < 2) { build_flow_field_pq(g, gx, gy, integ, flow); return; }

    Grid coarse(csz);
    for (int cy = 0; cy < csz; ++cy)
        for (int cx = 0; cx < csz; ++cx) {
            bool any = false;
            for (int dy = 0; dy < block && !any; ++dy)
                for (int dx = 0; dx < block && !any; ++dx) {
                    int wx = cx * block + dx, wy = cy * block + dy;
                    if (g.is_walkable(wx, wy)) any = true;
                }
            coarse.walkable[coarse.idx(cx, cy)] = any ? 1 : 0;
        }

    int cgx = std::clamp(gx / block, 0, csz - 1);
    int cgy = std::clamp(gy / block, 0, csz - 1);
    IntegrationField cinteg(csz);
    FlowField cflow(csz);
    build_flow_field_pq(coarse, cgx, cgy, cinteg, cflow);

    integ.reset();
    for (int y = 0; y < sz; ++y)
        for (int x = 0; x < sz; ++x) {
            if (!g.is_walkable(x, y)) continue;
            int cx = x / block, cy = y / block;
            int32_t coarse_cost = cinteg.cost[cinteg.idx(cx, cy)];
            if (coarse_cost >= WALL_COST) continue;
            integ.cost[integ.idx(x, y)] = coarse_cost * CARDINAL_COST;
        }

    for (int y = 0; y < sz; ++y)
        for (int x = 0; x < sz; ++x) {
            int idx = integ.idx(x, y);
            if (integ.cost[idx] >= WALL_COST) { flow.dir[idx] = DIR_NONE; continue; }
            if (x == gx && y == gy) { flow.dir[idx] = DIR_NONE; continue; }
            int32_t best = integ.cost[idx];
            uint8_t best_dir = 0;
            for (int d = 0; d < 8; ++d) {
                int nx = x + DX[d], ny = y + DY[d];
                if (!g.is_walkable(nx, ny)) continue;
                int32_t nc = integ.cost[integ.idx(nx, ny)];
                if (nc < best) { best = nc; best_dir = static_cast<uint8_t>(d); }
            }
            flow.dir[idx] = best_dir;
        }
}

struct TimingResult {
    double mean_us = 0;
    double median_us = 0;
    double p95_us = 0;
    double std_us = 0;
    int n_iter = 0;
};

static TimingResult measure_us(int iters, int warmup, auto fn) {
    for (int i = 0; i < warmup; ++i) fn();

    std::vector<double> times;
    times.reserve(iters);
    for (int i = 0; i < iters; ++i) {
        auto start = std::chrono::steady_clock::now();
        fn();
        auto end = std::chrono::steady_clock::now();
        double us = std::chrono::duration<double, std::micro>(end - start).count();
        times.push_back(us);
    }

    std::sort(times.begin(), times.end());
    double sum = 0;
    for (auto t : times) sum += t;
    double mean = sum / times.size();
    double median = times[times.size() / 2];
    double p95 = times[static_cast<size_t>(times.size() * 0.95)];
    double var = 0;
    for (auto t : times) var += (t - mean) * (t - mean);
    double stddev = std::sqrt(var / times.size());

    return {mean, median, p95, stddev, static_cast<int>(times.size())};
}

int main() {
    std::printf("strategy,scene,seed,grid_size,build_mean_us,build_median_us,"
                "build_p95_us,build_std_us,reachable_pct\n");
    std::fflush(stdout);

    int total = static_cast<int>(NUM_STRATEGIES) * static_cast<int>(NUM_SCENES) * static_cast<int>(SEEDS.size()) * static_cast<int>(GRID_SIZES.size());
    int done = 0;
    auto t0 = std::chrono::steady_clock::now();

    for (int si = 0; si < NUM_STRATEGIES; ++si) {
        for (int sci = 0; sci < NUM_SCENES; ++sci) {
            for (int seed : SEEDS) {
                for (int gs : GRID_SIZES) {
                    Grid grid(gs);
                    generate_scene(grid, static_cast<Scene>(sci), seed);

                    int gx = gs / 2, gy = gs / 2;
                    int tries = 0;
                    while (!grid.is_walkable(gx, gy) && tries < 100) {
                        gx = gs / 2 + (tries % 5 - 2);
                        gy = gs / 2 + ((tries / 5) % 5 - 2);
                        ++tries;
                    }
                    if (!grid.is_walkable(gx, gy)) {
                        ++done;
                        std::printf("%s,%s,%d,%d,SKIP,SKIP,SKIP,SKIP,0.0\n",
                            STRAT_NAMES[si], SCENE_NAMES[sci], seed, gs);
                        std::fflush(stdout);
                        continue;
                    }

                    IntegrationField integ(gs);
                    FlowField flow(gs);
                    std::vector<int32_t> astar_scratch;

                    int iters = ITER;
                    int warmup = WARMUP;
                    bool skip = false;

                    TimingResult tr{};
                    switch (static_cast<Strategy>(si)) {
                    default: skip = true; break;
                    case A_AStar_PerUnit: {
                        if (gs >= 256 && sci >= 2) { iters = 30; warmup = 2; }
                        if (gs >= 512 && sci >= 1) { iters = 10; warmup = 1; }
                        tr = measure_us(iters, warmup, [&]() {
                            astar_find(grid, 1, 1, gx, gy, astar_scratch);
                        });
                        break;
                    }
                    case B_FlowField_Dijkstra_PQ: {
                        tr = measure_us(iters, warmup, [&]() {
                            build_flow_field_pq(grid, gx, gy, integ, flow);
                        });
                        break;
                    }
                    case C_FlowField_BFS: {
                        tr = measure_us(iters, warmup, [&]() {
                            build_flow_field_bfs(grid, gx, gy, integ, flow);
                        });
                        break;
                    }
                    case D_FlowField_GPU_Analytical: {
                        if (gs >= 256) { skip = true; break; }
                        tr = measure_us(iters, warmup, [&]() {
                            build_flow_field_gpu_analytical(grid, gx, gy, integ, flow);
                        });
                        break;
                    }
                    case E_HPA_FlowField: {
                        tr = measure_us(iters, warmup, [&]() {
                            build_hpa_flow_field(grid, gx, gy, integ, flow);
                        });
                        break;
                    }
                    }

                    if (skip) {
                        ++done;
                        std::printf("%s,%s,%d,%d,SKIP,SKIP,SKIP,SKIP,0.0\n",
                            STRAT_NAMES[si], SCENE_NAMES[sci], seed, gs);
                        std::fflush(stdout);
                        continue;
                    }

                    double reachable_pct = measure_reachable_pct(grid, gx, gy);

                    ++done;
                    std::printf("%s,%s,%d,%d,%.3f,%.3f,%.3f,%.3f,%.1f\n",
                        STRAT_NAMES[si], SCENE_NAMES[sci], seed, gs,
                        tr.mean_us, tr.median_us, tr.p95_us, tr.std_us,
                        reachable_pct);
                    std::fflush(stdout);

                    if (done % 20 == 0) {
                        auto t1 = std::chrono::steady_clock::now();
                        double sec = std::chrono::duration<double>(t1 - t0).count();
                        std::fprintf(stderr, "progress: %d/%d (%.1f%%) elapsed=%.1fs\n",
                                     done, total, 100.0 * done / total, sec);
                    }
                }
            }
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    double sec = std::chrono::duration<double>(t1 - t0).count();
    std::fprintf(stderr, "DONE: %d configs in %.2f sec\n", total, sec);
    return 0;
}
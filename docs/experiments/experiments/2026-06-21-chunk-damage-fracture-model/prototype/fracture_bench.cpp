#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <random>
#include <vector>
#include <span>
#include <string_view>
#include <unordered_map>
#include <ranges>
#include <print>

static constexpr int kChunkSize = 8;
static constexpr int kChunkVoxels = kChunkSize * kChunkSize * kChunkSize;

// ---- metrics accumulator ----
struct Stats {
    double              mean = 0, median = 0, p95 = 0, p99 = 0, stddev = 0, min = 1e99, max = -1e99;
    uint64_t            n = 0;
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
private:
    std::vector<double> vals;
};

// ---- voxel grid helpers ----
using Grid = std::array<uint8_t, kChunkVoxels>;

static int idx(int x, int y, int z) { return (y * kChunkSize + z) * kChunkSize + x; }

// ---- scene generators ----
static void gen_uniform_floor(Grid &g) { g.fill(1); }

static void gen_forest_floor(Grid &g) {
    std::mt19937 rng(7);
    for (int i = 0; i < kChunkVoxels; i++)
        g[i] = (rng() % 10 < 7) ? uint8_t(1) : uint8_t(0);
    // ensure at least some structure
    for (int x = 0; x < kChunkSize; x++)
        for (int z = 0; z < kChunkSize; z++)
            g[idx(x, 0, z)] = 1;
}

static void gen_cave_stress(Grid &g) {
    // hollow with tunnels
    g.fill(1);
    // carve an L-shaped tunnel
    for (int x = 1; x < 6; x++) g[idx(x, 4, 4)] = 0;
    for (int y = 1; y < 6; y++) g[idx(5, y, 4)] = 0;
    for (int z = 1; z < 6; z++) g[idx(5, 4, z)] = 0;
    // air pockets
    g[idx(2,2,2)] = 0; g[idx(2,2,3)] = 0; g[idx(2,3,2)] = 0;
    g[idx(6,6,6)] = 0; g[idx(6,6,5)] = 0; g[idx(6,5,6)] = 0;
}

static void gen_mixed_biome(Grid &g) {
    std::mt19937 rng(42);
    for (int i = 0; i < kChunkVoxels; i++) {
        uint8_t m = (rng() % 100);
        g[i] = (m < 40) ? 1 : (m < 55) ? 2 : (m < 70) ? 3 : (m < 80) ? 4 : 0;
    }
    for (int x = 0; x < kChunkSize; x++)
        for (int z = 0; z < kChunkSize; z++)
            g[idx(x, 0, z)] = 1;
}

static void gen_custom_fracture(Grid &g) {
    // dense block with internal cavities
    g.fill(2);
    // internal cavity (4x4x4 void in center)
    for (int x = 2; x < 6; x++)
        for (int y = 2; y < 6; y++)
            for (int z = 2; z < 6; z++)
                g[idx(x, y, z)] = 0;
    // few pillars
    for (int y = 0; y < 6; y++) { g[idx(3, y, 3)] = 2; g[idx(4, y, 4)] = 2; }
    // weak seam (row of weak material)
    for (int x = 0; x < 8; x++) g[idx(x, 4, 7)] = 1;
    for (int z = 0; z < 8; z++) g[idx(7, 4, z)] = 1;
}

using SceneGen = void (*)(Grid &);

// ---- explosion damage model (Minecraft-style) ----
static void apply_explosion(Grid &g, float cx, float cy, float cz, float radius, float power,
                            std::minstd_rand &rng)
{
    float hardness = 3.0f; // stone-like
    // collect affected positions via BFS-like flood from origin
    std::vector<int> to_remove;
    for (int x = 0; x < kChunkSize; x++) {
        for (int y = 0; y < kChunkSize; y++) {
            for (int z = 0; z < kChunkSize; z++) {
                if (g[idx(x,y,z)] == 0) continue;
                float dx = float(x) + 0.5f - cx;
                float dy = float(y) + 0.5f - cy;
                float dz = float(z) + 0.5f - cz;
                float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
                if (dist > radius) continue;
                // MC-style: exposure + power decay
                float exposure = 1.0f - dist / radius;
                float effective = (power * exposure - hardness) + (rng() % 100) / 200.0f;
                if (effective > 0)
                    to_remove.push_back(idx(x,y,z));
            }
        }
    }
    for (int i : to_remove) g[i] = 0;
}

// ========== STRATEGIES ==========

// A_NaiveVoxel: each solid voxel after damage = 1 body
struct FragAABB {
    int x0, y0, z0, x1, y1, z1;
    uint32_t volume() const { return uint32_t((x1-x0+1)*(y1-y0+1)*(z1-z0+1)); }
};

static std::vector<FragAABB> strategy_A_naive(const Grid &g) {
    std::vector<FragAABB> out;
    for (int x = 0; x < kChunkSize; x++)
        for (int y = 0; y < kChunkSize; y++)
            for (int z = 0; z < kChunkSize; z++)
                if (g[idx(x,y,z)] != 0)
                    out.push_back({x,y,z,x,y,z});
    return out;
}

// B_GreedyCCL: Union-Find 26-conn → merged AABB per component
struct DSU {
    std::vector<int> p;
    DSU(int n) : p(n) { std::iota(p.begin(), p.end(), 0); }
    int find(int x) { while (p[x] != x) { p[x] = p[p[x]]; x = p[x]; } return x; }
    void unite(int a, int b) { p[find(a)] = find(b); }
};

static std::vector<FragAABB> strategy_B_CCL(const Grid &g) {
    int n = kChunkVoxels;
    DSU dsu(n);
    int ngh[][3] = {{1,0,0},{0,1,0},{0,0,1},{1,1,0},{1,-1,0},{1,0,1},{1,0,-1},{0,1,1},{0,1,-1},
                    {1,1,1},{1,1,-1},{1,-1,1},{1,-1,-1}};
    for (int x = 0; x < kChunkSize; x++)
        for (int y = 0; y < kChunkSize; y++)
            for (int z = 0; z < kChunkSize; z++) {
                int i = idx(x,y,z);
                if (g[i] == 0) continue;
                for (auto [dx,dy,dz] : ngh) {
                    int nx = x+dx, ny = y+dy, nz = z+dz;
                    if (nx < 0 || nx >= kChunkSize || ny < 0 || ny >= kChunkSize || nz < 0 || nz >= kChunkSize) continue;
                    int ni = idx(nx,ny,nz);
                    if (g[ni] != 0) dsu.unite(i, ni);
                }
            }
    // group by root
    std::unordered_map<int, FragAABB> comps;
    for (int x = 0; x < kChunkSize; x++)
        for (int y = 0; y < kChunkSize; y++)
            for (int z = 0; z < kChunkSize; z++) {
                int i = idx(x,y,z);
                if (g[i] == 0) continue;
                int r = dsu.find(i);
                auto &f = comps[r];
                if (comps.count(r) == 0) f = {x,y,z,x,y,z};
                else {
                    f.x0 = std::min(f.x0, x); f.y0 = std::min(f.y0, y); f.z0 = std::min(f.z0, z);
                    f.x1 = std::max(f.x1, x); f.y1 = std::max(f.y1, y); f.z1 = std::max(f.z1, z);
                }
            }
    std::vector<FragAABB> out;
    for (auto &[_,f] : comps) out.push_back(f);
    return out;
}

// C_Greedy3D: 3D greedy merge (per-axis scan, like physics meshing F_TwoPass)
static std::vector<FragAABB> strategy_C_greedy3D(const Grid &g) {
    Grid visited{};
    std::vector<FragAABB> out;
    for (int y = 0; y < kChunkSize; y++) {
        for (int z = 0; z < kChunkSize; z++) {
            for (int x = 0; x < kChunkSize; ) {
                int i = idx(x,y,z);
                if (g[i] == 0 || visited[i]) { x++; continue; }
                // scan X extent
                int x1 = x;
                while (x1+1 < kChunkSize && g[idx(x1+1,y,z)] != 0 && !visited[idx(x1+1,y,z)]) x1++;
                // scan Z extent
                int z1 = z;
                bool can_extend_z = true;
                while (can_extend_z && z1+1 < kChunkSize) {
                    for (int xx = x; xx <= x1; xx++)
                        if (g[idx(xx,y,z1+1)] == 0 || visited[idx(xx,y,z1+1)]) { can_extend_z = false; break; }
                    if (can_extend_z) z1++;
                }
                // scan Y extent
                int y1 = y;
                bool can_extend_y = true;
                while (can_extend_y && y1+1 < kChunkSize) {
                    for (int yy = y; yy <= y1+1; yy++) // check new row only
                        for (int xx = x; xx <= x1; xx++)
                            for (int zz = z; zz <= z1; zz++)
                                if (g[idx(xx,yy,zz)] == 0 || visited[idx(xx,yy,zz)]) { can_extend_y = false; goto y_done; }
                    y_done: ;
                    if (can_extend_y) y1++;
                }
                for (int yy = y; yy <= y1; yy++)
                    for (int zz = z; zz <= z1; zz++)
                        for (int xx = x; xx <= x1; xx++)
                            visited[idx(xx,yy,zz)] = 1;
                out.push_back({x, y, z, x1, y1, z1});
                x = x1 + 1;
            }
        }
    }
    return out;
}

// D_VoronoiPrecomputed: precompute N Voronoi 8³ fragment masks at init, match by damage origin cell
struct VoronoiCache {
    std::array<uint8_t, kChunkVoxels> cell_id{};
    int num_cells = 0;
};
static std::vector<VoronoiCache> generate_voronoi_templates(int count, std::minstd_rand &rng) {
    std::vector<VoronoiCache> caches;
    std::uniform_real_distribution<float> dist(0.f, float(kChunkSize));
    for (int t = 0; t < count; t++) {
        VoronoiCache vc;
        // generate random seed points (4-8 cells)
        int n_seeds = 4 + (rng() % 5);
        std::vector<std::array<float,3>> seeds;
        for (int s = 0; s < n_seeds; s++)
            seeds.push_back({dist(rng), dist(rng), dist(rng)});
        // assign each voxel to nearest seed
        for (int x = 0; x < kChunkSize; x++)
            for (int y = 0; y < kChunkSize; y++)
                for (int z = 0; z < kChunkSize; z++) {
                    float best_d = 1e9f;
                    int best_s = 0;
                    for (int s = 0; s < n_seeds; s++) {
                        float dx = float(x)+0.5f - seeds[s][0];
                        float dy = float(y)+0.5f - seeds[s][1];
                        float dz = float(z)+0.5f - seeds[s][2];
                        float d = dx*dx + dy*dy + dz*dz;
                        if (d < best_d) { best_d = d; best_s = s; }
                    }
                    vc.cell_id[idx(x,y,z)] = uint8_t(best_s);
                }
        vc.num_cells = n_seeds;
        caches.push_back(vc);
    }
    return caches;
}

static std::vector<FragAABB> strategy_D_voronoi(const Grid &g, const VoronoiCache &vc) {
    std::unordered_map<int, FragAABB> comps;
    for (int x = 0; x < kChunkSize; x++)
        for (int y = 0; y < kChunkSize; y++)
            for (int z = 0; z < kChunkSize; z++) {
                int i = idx(x,y,z);
                if (g[i] == 0) continue;
                int cid = vc.cell_id[i];
                auto &f = comps[cid];
                if (comps.count(cid) == 0) f = {x,y,z,x,y,z};
                else {
                    f.x0 = std::min(f.x0, x); f.y0 = std::min(f.y0, y); f.z0 = std::min(f.z0, z);
                    f.x1 = std::max(f.x1, x); f.y1 = std::max(f.y1, y); f.z1 = std::max(f.z1, z);
                }
            }
    std::vector<FragAABB> out;
    for (auto &[_,f] : comps) out.push_back(f);
    return out;
}

// E_Hybrid: CCL for small fragments, Voronoi for large components
static std::vector<FragAABB> strategy_E_hybrid(const Grid &g, const VoronoiCache &vc, int small_threshold = 4) {
    auto ccl = strategy_B_CCL(g);
    std::vector<FragAABB> out;
    for (auto &f : ccl) {
        if (f.volume() <= uint32_t(small_threshold)) {
            out.push_back(f);
        } else {
            // sub-fracture large component using Voronoi
            Grid sub{};
            for (int x = f.x0; x <= f.x1; x++)
                for (int y = f.y0; y <= f.y1; y++)
                    for (int z = f.z0; z <= f.z1; z++)
                        if (g[idx(x,y,z)] != 0)
                            sub[idx(x-f.x0, y-f.y0, z-f.z0)] = g[idx(x,y,z)];
            // apply Voronoi cell matching within the sub-grid
            std::unordered_map<int, FragAABB> sub_comps;
            for (int x = f.x0; x <= f.x1; x++)
                for (int y = f.y0; y <= f.y1; y++)
                    for (int z = f.z0; z <= f.z1; z++) {
                        int i = idx(x,y,z);
                        if (g[i] == 0) continue;
                        int cid = vc.cell_id[i];
                        auto &sf = sub_comps[cid];
                        if (sub_comps.count(cid) == 0) sf = {x,y,z,x,y,z};
                        else {
                            sf.x0 = std::min(sf.x0, x); sf.y0 = std::min(sf.y0, y); sf.z0 = std::min(sf.z0, z);
                            sf.x1 = std::max(sf.x1, x); sf.y1 = std::max(sf.y1, y); sf.z1 = std::max(sf.z1, z);
                        }
                    }
            for (auto &[_,sf] : sub_comps) out.push_back(sf);
        }
    }
    return out;
}

// ---- stats per config ----
struct ConfigResult {
    std::string name, scene;
    int seed = 0;
    double time_us = 0;
    int body_count = 0;
};

// ========== MAIN ==========
int main() {
    std::minstd_rand global_rng(42);
    auto templates = generate_voronoi_templates(8, global_rng);

    struct SceneDef { std::string_view name; SceneGen gen; };
    SceneDef scenes[] = {
        {"uniform_floor", gen_uniform_floor},
        {"forest_floor", gen_forest_floor},
        {"cave_stress", gen_cave_stress},
        {"mixed_biome", gen_mixed_biome},
        {"custom_fracture", gen_custom_fracture},
    };
    constexpr int kSeeds = 5;
    constexpr int kIter = 1000;
    constexpr int kWarmup = 10;
    int seeds[kSeeds] = {1, 7, 42, 1234, 31337};

    std::vector<ConfigResult> results;

    for (auto &sc : scenes) {
        for (int si = 0; si < kSeeds; si++) {
            int seed = seeds[si];
            // generate base scene
            Grid base;
            sc.gen(base);

            // explosion params deterministic from seed
            std::minstd_rand exp_rng((unsigned)seed);
            std::uniform_real_distribution<float> pos_dist(2.f, 6.f);
            float cx = pos_dist(exp_rng), cy = pos_dist(exp_rng), cz = pos_dist(exp_rng);
            float radius = 2.5f + (exp_rng() % 30) / 100.0f;
            float power = 8.0f + (exp_rng() % 40) / 10.0f;

            // ---- strategy A: naive ----
            {
                double total_t = 0;
                int bodies = 0;
                for (int iter = 0; iter < kIter + kWarmup; iter++) {
                    Grid g = base;
                    apply_explosion(g, cx, cy, cz, radius, power, exp_rng);
                    auto t0 = std::chrono::steady_clock::now();
                    auto frags = strategy_A_naive(g);
                    auto t1 = std::chrono::steady_clock::now();
                    if (iter >= kWarmup) {
                        double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
                        total_t += us;
                        bodies = (int)frags.size();
                    }
                }
                results.emplace_back("A_Naive", std::string(sc.name), seed, total_t / kIter, bodies);
            }

            // ---- strategy B: CCL ----
            {
                double total_t = 0;
                int bodies = 0;
                for (int iter = 0; iter < kIter + kWarmup; iter++) {
                    Grid g = base;
                    apply_explosion(g, cx, cy, cz, radius, power, exp_rng);
                    auto t0 = std::chrono::steady_clock::now();
                    auto frags = strategy_B_CCL(g);
                    auto t1 = std::chrono::steady_clock::now();
                    if (iter >= kWarmup) {
                        double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
                        total_t += us;
                        bodies = (int)frags.size();
                    }
                }
                results.emplace_back("B_CCL", std::string(sc.name), seed, total_t / kIter, bodies);
            }

            // ---- strategy C: greedy 3D ----
            {
                double total_t = 0;
                for (int iter = 0; iter < kIter + kWarmup; iter++) {
                    Grid g = base;
                    apply_explosion(g, cx, cy, cz, radius, power, exp_rng);
                    auto t0 = std::chrono::steady_clock::now();
                    auto frags = strategy_C_greedy3D(g);
                    auto t1 = std::chrono::steady_clock::now();
                    if (iter >= kWarmup)
                        total_t += std::chrono::duration<double, std::micro>(t1 - t0).count();
                }
                Grid g = base;
                apply_explosion(g, cx, cy, cz, radius, power, exp_rng);
                auto frags = strategy_C_greedy3D(g);
                results.emplace_back("C_Greedy3D", std::string(sc.name), seed, total_t / kIter, (int)frags.size());
            }

            // ---- strategy D: Voronoi precomputed ----
            {
                int best_t = 0;
                double best_t_us = 1e9;
                for (int ti = 0; ti < (int)templates.size(); ti++) {
                    double total_t = 0;
                    for (int iter = 0; iter < kIter + kWarmup; iter++) {
                        Grid g = base;
                        apply_explosion(g, cx, cy, cz, radius, power, exp_rng);
                        auto t0 = std::chrono::steady_clock::now();
                        auto frags = strategy_D_voronoi(g, templates[ti]);
                        auto t1 = std::chrono::steady_clock::now();
                        if (iter >= kWarmup)
                            total_t += std::chrono::duration<double, std::micro>(t1 - t0).count();
                    }
                    if (total_t < best_t_us) { best_t_us = total_t; best_t = ti; }
                }
                Grid g = base;
                apply_explosion(g, cx, cy, cz, radius, power, exp_rng);
                auto frags = strategy_D_voronoi(g, templates[best_t]);
                results.emplace_back("D_Voronoi", std::string(sc.name), seed, best_t_us / kIter, (int)frags.size());
            }

            // ---- strategy E: hybrid ----
            {
                int best_t = 0;
                double best_t_us = 1e9;
                for (int ti = 0; ti < (int)templates.size(); ti++) {
                    double total_t = 0;
                    for (int iter = 0; iter < kIter + kWarmup; iter++) {
                        Grid g = base;
                        apply_explosion(g, cx, cy, cz, radius, power, exp_rng);
                        auto t0 = std::chrono::steady_clock::now();
                        auto frags = strategy_E_hybrid(g, templates[ti]);
                        auto t1 = std::chrono::steady_clock::now();
                        if (iter >= kWarmup)
                            total_t += std::chrono::duration<double, std::micro>(t1 - t0).count();
                    }
                    if (total_t < best_t_us) { best_t_us = total_t; best_t = ti; }
                }
                Grid g = base;
                apply_explosion(g, cx, cy, cz, radius, power, exp_rng);
                auto frags = strategy_E_hybrid(g, templates[best_t]);
                results.emplace_back("E_Hybrid", std::string(sc.name), seed, best_t_us / kIter, (int)frags.size());
            }
        }
    }

    // print CSV
    std::println("strategy,scene,seed,time_us,body_count");
    for (auto &r : results)
        std::println("{},{},{},{:.3f},{}", r.name.c_str(), r.scene.c_str(), r.seed, r.time_us, r.body_count);
    // save CSV
    if (FILE *f = fopen("build/results.csv", "w")) {
        std::println(f, "strategy,scene,seed,time_us,body_count");
        for (auto &r : results)
            std::println(f, "{},{},{},{:.3f},{}", r.name.c_str(), r.scene.c_str(), r.seed, r.time_us, r.body_count);
        fclose(f);
    }

    // per-strategy summary
    std::println("\n--- per-strategy summary ---");
    std::println("{:<15} {:>10} {:>12} {:>12}", "strategy", "mean_us", "mean_bodies", "reduction_x");
    struct StratSum { double t_us = 0; double bodies = 0; int n = 0; };
    StratSum sums[5];
    const char *snames[5] = {"A_Naive", "B_CCL", "C_Greedy3D", "D_Voronoi", "E_Hybrid"};
    for (auto &r : results) {
        int sid = (r.name[0] - 'A');
        if (sid >= 0 && sid < 5) { sums[sid].t_us += r.time_us; sums[sid].bodies += r.body_count; sums[sid].n++; }
    }
    double baseline_bodies = sums[0].bodies / sums[0].n;
    for (int i = 0; i < 5; i++) {
        double mb = sums[i].bodies / sums[i].n;
        double mt = sums[i].t_us / sums[i].n;
        double red = (i == 0) ? 1.0f : baseline_bodies / mb;
        std::println("{:<15} {:>10.3f} {:>12.1f} {:>11.1f}x",
                     snames[i], mt, mb, red);
    }

    // optional: fidelity check (voxel preservation)
    std::println("\n--- voxel preservation ---");
    Grid base_check; gen_uniform_floor(base_check);
    std::minstd_rand check_rng(42);
    apply_explosion(base_check, 4.f, 4.f, 4.f, 3.f, 10.f, check_rng);
    int orig_vol = 0;
    for (auto v : base_check) if (v) orig_vol++;
    auto fa = strategy_A_naive(base_check);
    int vox_a = (int)fa.size(); // naive = 1 voxel per body = body count == voxel count
    auto fb = strategy_B_CCL(base_check);
    Grid check_gb{}; for (auto &f : fb) for (int x = f.x0; x <= f.x1; x++) for (int y = f.y0; y <= f.y1; y++) for (int z = f.z0; z <= f.z1; z++) if (base_check[idx(x,y,z)]) check_gb[idx(x-f.x0, y-f.y0, z-f.z0)] = 1;
    int vox_b = 0; for (auto v : check_gb) if (v) vox_b++;
    auto fc = strategy_C_greedy3D(base_check);
    Grid check_gc{}; for (auto &f : fc) for (int x = f.x0; x <= f.x1; x++) for (int y = f.y0; y <= f.y1; y++) for (int z = f.z0; z <= f.z1; z++) if (base_check[idx(x,y,z)]) check_gc[idx(x-f.x0, y-f.y0, z-f.z0)] = 1;
    int vox_c = 0; for (auto v : check_gc) if (v) vox_c++;
    std::println("orig_voxels={}, A_voxels={}, B_voxels={}, C_voxels={}, ok={}",
                 orig_vol, vox_a, vox_b, vox_c,
                 (vox_a == vox_b && vox_b == vox_c && vox_a == orig_vol) ? "PASS" : "FAIL");
    return 0;
}

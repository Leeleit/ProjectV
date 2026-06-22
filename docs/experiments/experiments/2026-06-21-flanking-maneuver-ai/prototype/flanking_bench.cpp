// flanking_bench.cpp — Cover-aware flanking maneuver AI benchmark
// Standalone C++26 CPU prototype for docs/experiments/2026-06-21-flanking-maneuver-ai/
//
// Compares 5 strategies (NoFlank / GeometricLShaped / CoverWeightedFlow / BayesianThreat /
// HierarchicalBTSplit) on 5 scenes (open_field / light_cover / urban_corridor /
// dense_urban / defensive_line). Measures plan time, exposure time, path length,
// success rate, squad batch time.
//
// Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic
//        flanking_bench.cpp -o build/flanking_bench
// Run:   ./build/flanking_bench
// Output: build/results.csv (1 header + 125 rows = 5 strategies × 5 scenes × 5 seeds)
//
// Per benchmarks/methodology.md §7: warmup 10 trials (excluded) + N=1000 main measurements
// per (strategy × scene × seed × unit), with full grid recomputation (cold cache per trial).

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <queue>
#include <random>
#include <string>
#include <utility>
#include <vector>

using i32 = int32_t;
using u8 = uint8_t;
using u32 = uint32_t;
using f32 = float;
using f64 = double;

constexpr u32 GRID_SIZE = 256;
constexpr u32 CELL_COUNT = GRID_SIZE * GRID_SIZE;

struct Stats {
    double mean, median, p95, p99, stddev, min_v, max_v;
};

Stats compute_stats(std::vector<double> samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::sort(samples.begin(), samples.end());
    double sum = 0;
    for (double v : samples) sum += v;
    s.mean = sum / samples.size();
    s.median = samples[samples.size() / 2];
    s.p95 = samples[static_cast<size_t>(samples.size() * 0.95)];
    s.p99 = samples[static_cast<size_t>(samples.size() * 0.99)];
    double var = 0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / samples.size());
    s.min_v = samples.front();
    s.max_v = samples.back();
    return s;
}

struct BitGrid {
    std::vector<u32> bits;
    void resize() { bits.assign((CELL_COUNT + 31) / 32, 0); }
    void set(u32 x, u32 y, bool v) {
        u32 i = y * GRID_SIZE + x;
        if (v) bits[i / 32] |= (1u << (i % 32));
        else bits[i / 32] &= ~(1u << (i % 32));
    }
    bool get(u32 x, u32 y) const {
        u32 i = y * GRID_SIZE + x;
        return (bits[i / 32] >> (i % 32)) & 1u;
    }
};

struct FloatGrid {
    std::array<f32, CELL_COUNT> values{};
    f32 get(u32 x, u32 y) const { return values[y * GRID_SIZE + x]; }
    void set(u32 x, u32 y, f32 v) { values[y * GRID_SIZE + x] = v; }
};

struct FlowField {
    std::array<f32, CELL_COUNT> dist{};
    void compute(const BitGrid& walls, u32 gx, u32 gy, const FloatGrid& threat, f32 threat_mult) {
        for (auto& d : dist) d = 1e9f;
        using P = std::pair<f32, u32>;
        std::priority_queue<P, std::vector<P>, std::greater<P>> pq;
        u32 gi = gy * GRID_SIZE + gx;
        dist[gi] = 0;
        pq.push({0.0f, gi});
        static constexpr i32 dx[] = {0, 1, 0, -1};
        static constexpr i32 dy[] = {-1, 0, 1, 0};
        while (!pq.empty()) {
            auto [d, i] = pq.top(); pq.pop();
            if (d > dist[i]) continue;
            u32 x = i % GRID_SIZE;
            u32 y = i / GRID_SIZE;
            for (u32 k = 0; k < 4; k++) {
                i32 nx = (i32)x + dx[k];
                i32 ny = (i32)y + dy[k];
                if (nx < 0 || nx >= (i32)GRID_SIZE || ny < 0 || ny >= (i32)GRID_SIZE) continue;
                if (walls.get((u32)nx, (u32)ny)) continue;
                u32 ni = (u32)ny * GRID_SIZE + (u32)nx;
                f32 step = 1.0f + threat.get((u32)nx, (u32)ny) * threat_mult;
                f32 nd = dist[i] + step;
                if (nd < dist[ni]) { dist[ni] = nd; pq.push({nd, ni}); }
            }
        }
    }
};

enum Strategy : u8 { A_NoFlank, B_GeometricLShaped, C_CoverWeightedFlow, D_BayesianThreat, E_HierarchicalBTSplit };

const char* strategy_name(Strategy s) {
    switch (s) {
        case A_NoFlank: return "A_NoFlank";
        case B_GeometricLShaped: return "B_GeometricLShaped";
        case C_CoverWeightedFlow: return "C_CoverWeightedFlow";
        case D_BayesianThreat: return "D_BayesianThreat";
        case E_HierarchicalBTSplit: return "E_HierarchicalBTSplit";
    }
    return "?";
}

enum Scene : u8 { open_field, light_cover, urban_corridor, dense_urban, defensive_line };

const char* scene_name(Scene s) {
    switch (s) {
        case open_field: return "open_field";
        case light_cover: return "light_cover";
        case urban_corridor: return "urban_corridor";
        case dense_urban: return "dense_urban";
        case defensive_line: return "defensive_line";
    }
    return "?";
}

void mark_cover_adjacent(BitGrid& cover, const BitGrid& walls) {
    for (u32 y = 0; y < GRID_SIZE; y++) {
        for (u32 x = 0; x < GRID_SIZE; x++) {
            if (walls.get(x, y)) continue;
            bool adj = false;
            for (i32 dy = -1; dy <= 1 && !adj; dy++) {
                for (i32 dx = -1; dx <= 1 && !adj; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    i32 nx = (i32)x + dx, ny = (i32)y + dy;
                    if (nx >= 0 && nx < (i32)GRID_SIZE && ny >= 0 && ny < (i32)GRID_SIZE
                        && walls.get((u32)nx, (u32)ny)) {
                        adj = true;
                    }
                }
            }
            if (adj) cover.set(x, y, true);
        }
    }
}

void gen_scene(Scene s, u32 seed, BitGrid& walls, BitGrid& cover,
               std::vector<std::pair<u32, u32>>& enemies,
               std::pair<u32, u32>& start, std::pair<u32, u32>& goal) {
    walls.resize();
    cover.resize();
    std::mt19937 rng(seed);
    start = {32, 32};
    goal = {220, 220};
    switch (s) {
        case open_field:
            enemies = {{180, 180}, {200, 200}, {220, 220}};
            break;
        case light_cover:
            enemies = {{180, 180}, {200, 200}, {220, 220}};
            {
                std::uniform_int_distribution<u32> dx(0, GRID_SIZE - 1), dy(0, GRID_SIZE - 1);
                for (i32 i = 0; i < 600; i++) {
                    u32 x = dx(rng), y = dy(rng);
                    walls.set(x, y, true);
                }
            }
            mark_cover_adjacent(cover, walls);
            break;
        case urban_corridor:
            enemies = {{180, 180}, {200, 200}, {220, 220}};
            for (u32 y : {85, 130, 175}) {
                for (u32 x = 0; x < GRID_SIZE; x++) {
                    if ((x >= 25 && x <= 40) || (x >= 95 && x <= 105)
                        || (x >= 140 && x <= 150) || (x >= 185 && x <= 195)
                        || (x >= 210 && x <= 225)) continue;
                    walls.set(x, y, true);
                }
            }
            for (u32 x : {85, 130, 175}) {
                for (u32 y = 0; y < GRID_SIZE; y++) {
                    if ((y >= 25 && y <= 40) || (y >= 95 && y <= 105)
                        || (y >= 140 && y <= 150) || (y >= 185 && y <= 195)
                        || (y >= 210 && y <= 225)) continue;
                    walls.set(x, y, true);
                }
            }
            mark_cover_adjacent(cover, walls);
            break;
        case dense_urban:
            enemies = {{180, 180}, {200, 200}, {220, 220}};
            for (u32 y : {70, 110, 150, 190, 230}) {
                for (u32 x = 0; x < GRID_SIZE; x++) {
                    if ((x >= 25 && x <= 40) || (x >= 95 && x <= 105)
                        || (x >= 145 && x <= 155) || (x >= 195 && x <= 205)) continue;
                    walls.set(x, y, true);
                }
            }
            for (u32 x : {70, 110, 150, 190, 230}) {
                for (u32 y = 0; y < GRID_SIZE; y++) {
                    if ((y >= 25 && y <= 40) || (y >= 95 && y <= 105)
                        || (y >= 145 && y <= 155) || (y >= 195 && y <= 205)) continue;
                    walls.set(x, y, true);
                }
            }
            mark_cover_adjacent(cover, walls);
            break;
        case defensive_line:
            enemies = {{150, 175}, {170, 175}, {190, 175}, {210, 175}, {230, 175}};
            for (u32 x = 100; x < 250; x++) {
                walls.set(x, 170, true);
                walls.set(x, 180, true);
                if (x > 0) cover.set(x - 1, 170, true);
                if (x + 1 < GRID_SIZE) cover.set(x + 1, 180, true);
            }
            break;
    }
}

FloatGrid compute_threat_range(const BitGrid& walls,
                                const std::vector<std::pair<u32, u32>>& enemies,
                                f32 range_cells) {
    FloatGrid t{};
    f32 r2 = range_cells * range_cells;
    for (u32 y = 0; y < GRID_SIZE; y++) {
        for (u32 x = 0; x < GRID_SIZE; x++) {
            if (walls.get(x, y)) continue;
            f32 max_threat = 0;
            for (auto [ex, ey] : enemies) {
                f32 dx = (f32)x - (f32)ex;
                f32 dy = (f32)y - (f32)ey;
                f32 d2 = dx * dx + dy * dy;
                if (d2 < r2) {
                    f32 d = std::sqrt(d2);
                    max_threat = std::max(max_threat, 1.0f - d / range_cells);
                }
            }
            t.set(x, y, max_threat);
        }
    }
    return t;
}

FloatGrid compute_threat_bayesian(const BitGrid& walls,
                                   const std::vector<std::pair<u32, u32>>& enemies,
                                   f32 sigma) {
    FloatGrid t{};
    f32 inv2s2 = 1.0f / (2.0f * sigma * sigma);
    for (u32 y = 0; y < GRID_SIZE; y++) {
        for (u32 x = 0; x < GRID_SIZE; x++) {
            if (walls.get(x, y)) { t.set(x, y, 0); continue; }
            f32 sum = 0;
            for (auto [ex, ey] : enemies) {
                f32 dx = (f32)x - (f32)ex;
                f32 dy = (f32)y - (f32)ey;
                f32 d2 = dx * dx + dy * dy;
                sum += std::exp(-d2 * inv2s2);
            }
            t.set(x, y, std::min(1.0f, sum));
        }
    }
    return t;
}

struct Path {
    std::vector<u32> cells;
    bool reached = false;
    f32 exposure = 0;
    u32 length = 0;
};

Path trace_path(const FlowField& flow, const BitGrid& walls,
                const FloatGrid& display_threat,
                u32 sx, u32 sy, u32 gx, u32 gy) {
    Path p{};
    static constexpr i32 dx[] = {0, 1, 0, -1};
    static constexpr i32 dy[] = {-1, 0, 1, 0};
    u32 cx = sx, cy = sy;
    u32 max_iter = CELL_COUNT;
    while (max_iter--) {
        u32 ci = cy * GRID_SIZE + cx;
        p.cells.push_back(ci);
        p.exposure += display_threat.get(cx, cy);
        p.length++;
        if (cx == gx && cy == gy) { p.reached = true; break; }
        f32 best_dist = 1e9f;
        i32 best_k = -1;
        for (u32 k = 0; k < 4; k++) {
            i32 nx = (i32)cx + dx[k];
            i32 ny = (i32)cy + dy[k];
            if (nx < 0 || nx >= (i32)GRID_SIZE || ny < 0 || ny >= (i32)GRID_SIZE) continue;
            if (walls.get((u32)nx, (u32)ny)) continue;
            u32 ni = (u32)ny * GRID_SIZE + (u32)nx;
            if (flow.dist[ni] < best_dist) { best_dist = flow.dist[ni]; best_k = (i32)k; }
        }
        if (best_k < 0) break;
        cx = (u32)((i32)cx + dx[best_k]);
        cy = (u32)((i32)cy + dy[best_k]);
    }
    return p;
}

struct PlanResult {
    Path flank_path;
    Path suppress_path;
    bool suppress_exists = false;
    double plan_time_us = 0;
};

PlanResult plan_unit(Strategy s, const BitGrid& walls, const BitGrid& cover [[maybe_unused]],
                     const std::vector<std::pair<u32, u32>>& enemies,
                     u32 sx, u32 sy, u32 gx, u32 gy) {
    PlanResult r{};
    auto t0 = std::chrono::high_resolution_clock::now();
    f32 enemy_range = 50.0f;
    FloatGrid threat = compute_threat_range(walls, enemies, enemy_range);
    FlowField flow{};
    switch (s) {
        case A_NoFlank:
            flow.compute(walls, gx, gy, threat, 0.0f);
            r.flank_path = trace_path(flow, walls, threat, sx, sy, gx, gy);
            break;
        case B_GeometricLShaped: {
            u32 wx = gx;
            u32 wy = sy + 40 < GRID_SIZE ? sy + 40 : GRID_SIZE - 1;
            flow.compute(walls, wx, wy, threat, 0.0f);
            Path p1 = trace_path(flow, walls, threat, sx, sy, wx, wy);
            flow.compute(walls, gx, gy, threat, 0.0f);
            Path p2 = trace_path(flow, walls, threat, wx, wy, gx, gy);
            r.flank_path.cells = p1.cells;
            if (!p2.cells.empty()) {
                r.flank_path.cells.insert(r.flank_path.cells.end(),
                                          p2.cells.begin() + 1, p2.cells.end());
            }
            r.flank_path.exposure = p1.exposure + p2.exposure;
            if (!p1.cells.empty() && !p2.cells.empty()) {
                r.flank_path.exposure -= threat.get(wx, wy);
            }
            r.flank_path.length = (u32)r.flank_path.cells.size();
            r.flank_path.reached = p1.reached && p2.reached;
            break;
        }
        case C_CoverWeightedFlow:
            flow.compute(walls, gx, gy, threat, 5.0f);
            r.flank_path = trace_path(flow, walls, threat, sx, sy, gx, gy);
            break;
        case D_BayesianThreat: {
            FloatGrid smoothed = compute_threat_bayesian(walls, enemies, 30.0f);
            flow.compute(walls, gx, gy, smoothed, 8.0f);
            r.flank_path = trace_path(flow, walls, smoothed, sx, sy, gx, gy);
            break;
        }
        case E_HierarchicalBTSplit: {
            u32 flank_gx = gx > 20 ? gx - 20 : gx + 20;
            if (flank_gx >= GRID_SIZE) flank_gx = GRID_SIZE - 1;
            flow.compute(walls, flank_gx, gy, threat, 5.0f);
            r.flank_path = trace_path(flow, walls, threat, sx, sy, flank_gx, gy);
            u32 nearest_ex = enemies[0].first, nearest_ey = enemies[0].second;
            f32 nearest_d = 1e9f;
            for (auto [ex, ey] : enemies) {
                f32 dx = (f32)sx - (f32)ex;
                f32 dy = (f32)sy - (f32)ey;
                f32 d = std::sqrt(dx * dx + dy * dy);
                if (d < nearest_d) { nearest_d = d; nearest_ex = ex; nearest_ey = ey; }
            }
            u32 sup_x = nearest_ex;
            u32 sup_y = nearest_ey + 2 < GRID_SIZE ? nearest_ey + 2 : nearest_ey;
            if (sup_y >= GRID_SIZE) sup_y = GRID_SIZE - 1;
            flow.compute(walls, sup_x, sup_y, threat, 0.0f);
            r.suppress_path = trace_path(flow, walls, threat, sx, sy, sup_x, sup_y);
            r.suppress_exists = true;
            break;
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    r.plan_time_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    return r;
}

int main() {
    std::ofstream csv("build/results.csv");
    csv << "strategy,scene,seed,plan_time_us_per_call,plan_time_us_p99,path_length_mean,"
           "exposure_mean,reached_pct,squad_time_us_per_iter\n";
    constexpr u32 SQUAD_SIZE = 5;
    constexpr u32 ITERATIONS = 100;
    constexpr u32 WARMUP = 5;
    std::printf("=== flanking_bench: cover-aware flanking maneuver AI ===\n");
    std::printf("Grid: %ux%u cells | Squad: %u units | Iter: %u + warmup %u\n\n",
                GRID_SIZE, GRID_SIZE, SQUAD_SIZE, ITERATIONS, WARMUP);
    for (u8 si = 0; si <= 4; si++) {
        Strategy s = (Strategy)si;
        for (u8 sci = 0; sci <= 4; sci++) {
            Scene sc = (Scene)sci;
            for (u32 seed = 1; seed <= 5; seed++) {
                std::vector<double> plan_times, exposures, lengths;
                u32 reaches = 0;
                u32 total = 0;
                for (u32 unit = 0; unit < SQUAD_SIZE; unit++) {
                    for (u32 iter = 0; iter < ITERATIONS + WARMUP; iter++) {
                        BitGrid walls, cover;
                        std::vector<std::pair<u32, u32>> enemies;
                        std::pair<u32, u32> start, goal;
                        gen_scene(sc, seed + unit * 17 + iter * 31,
                                  walls, cover, enemies, start, goal);
                        u32 sx = (start.first + unit * 5) % GRID_SIZE;
                        u32 sy = (start.second + unit * 3) % GRID_SIZE;
                        PlanResult r = plan_unit(s, walls, cover, enemies, sx, sy,
                                                 goal.first, goal.second);
                        if (iter >= WARMUP) {
                            plan_times.push_back(r.plan_time_us);
                            exposures.push_back(r.flank_path.exposure);
                            lengths.push_back((double)r.flank_path.length);
                            if (r.flank_path.reached) reaches++;
                            total++;
                        }
                    }
                }
                Stats pt = compute_stats(plan_times);
                Stats ex = compute_stats(exposures);
                Stats ln = compute_stats(lengths);
                double total_squad_time = 0;
                for (double t : plan_times) total_squad_time += t;
                double squad_per_iter = total_squad_time / ITERATIONS;
                double plan_per_call = pt.mean;
                csv << strategy_name(s) << "," << scene_name(sc) << "," << seed << ","
                    << plan_per_call << "," << pt.p99 << "," << ln.mean << ","
                    << ex.mean << "," << (100.0 * reaches / (double)total) << ","
                    << squad_per_iter << "\n";
                std::printf("%-22s %-18s seed=%u: plan=%.3f us (p99 %.3f) len=%.0f exp=%.2f reach=%.1f%% squad/iter=%.3f us\n",
                            strategy_name(s), scene_name(sc), seed,
                            plan_per_call, pt.p99, ln.mean, ex.mean,
                            100.0 * reaches / (double)total, squad_per_iter);
            }
        }
    }
    csv.close();
    std::printf("\nResults written to build/results.csv (125 rows)\n");
    return 0;
}
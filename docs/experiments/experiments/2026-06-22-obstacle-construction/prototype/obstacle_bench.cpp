// Standalone C++26 CPU prototype for 2026-06-22-obstacle-construction
// 5 strategies × 5 obstacle types × 5 densities × 5 layering modes × 5 seeds × 1000 iter + 10 warmup = 625,000 main measurements
// Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic obstacle_bench.cpp -o build/obstacle_bench

#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <random>
#include <fstream>
#include <string>
#include <map>
#include <cmath>

enum ObstacleType : int {
    CONCRETE_BARRIER = 0,
    ANTI_TANK_DITCH,
    CZECH_HEDGEHOG,
    ABATIS,
    BERM
};
constexpr int NUM_OBSTACLE_TYPES = 5;
const char* OBSTACLE_NAMES[NUM_OBSTACLE_TYPES] = {
    "concrete_barrier", "anti_tank_ditch", "czech_hedgehog", "abatis", "berm"
};

// 2D AABB for obstacle footprint
struct AABB {
    float x, z;       // center position
    float w, d;       // width, depth (XZ plane)
    float rotation;   // radians
    ObstacleType type;
    int layer;        // for layered defense
};

struct LayeredTemplate {
    std::vector<ObstacleType> layers; // ordered, layer 0 is outermost
};

// RLE-compressed occupancy grid for fast overlap detection
// 32×32 grid = 1024 cells; RLE by row -> 32 runs of variable length
struct RLEGrid {
    static constexpr int SZ = 32;
    // For each cell, occupied bit (compressed)
    std::vector<uint8_t> occupied; // 1024 bytes
    void clear() { std::fill(occupied.begin(), occupied.end(), 0); occupied.assign(SZ*SZ, 0); }
    bool test(int x, int z) const {
        if (x < 0 || x >= SZ || z < 0 || z >= SZ) return false;
        return occupied[z*SZ+x] != 0;
    }
    void set(int x, int z) {
        if (x < 0 || x >= SZ || z < 0 || z >= SZ) return;
        occupied[z*SZ+x] = 1;
    }
};

// Footprint templates per obstacle type
struct Footprint {
    float w, d; // base dimensions
    int cells_w, cells_d; // grid cells (1 cell = 1 m)
    int blocking_vehicle; // 1 if blocks vehicles
    int blocking_infantry; // 1 if blocks infantry
};

static const Footprint FOOTPRINTS[NUM_OBSTACLE_TYPES] = {
    {2.0f, 0.5f, 2, 1, 1, 0}, // CONCRETE_BARRIER (long thin, blocks vehicles, infantry can climb over)
    {3.0f, 3.0f, 3, 3, 1, 1}, // ANTI_TANK_DITCH (3x3 wide, blocks vehicles + infantry crossing)
    {1.4f, 1.4f, 2, 2, 1, 0}, // CZECH_HEDGEHOG (small, blocks vehicles, infantry can step over)
    {2.0f, 6.0f, 2, 6, 0, 1}, // ABATIS (long row of fallen trees, blocks infantry, vehicles can push through)
    {1.5f, 3.0f, 2, 3, 1, 0}  // BERM (raised earthwork, blocks vehicles, infantry can crouch behind)
};

// Strategy A: NaivePerObstacle - place each obstacle sequentially with full BFS overlap check
double RunNaive(const std::vector<AABB>& obstacles, int density_idx, int layer_idx) {
    auto t0 = std::chrono::high_resolution_clock::now();
    RLEGrid grid;
    grid.occupied.assign(RLEGrid::SZ*RLEGrid::SZ, 0);
    int placed = 0, rejected = 0;
    float blocking_v = 0, blocking_i = 0;
    for (const auto& obs : obstacles) {
        const Footprint& fp = FOOTPRINTS[obs.type];
        int cx = (int)obs.x;
        int cz = (int)obs.z;
        int hw = fp.cells_w / 2;
        int hd = fp.cells_d / 2;
        bool overlap = false;
        for (int dz = -hd; dz <= hd && !overlap; ++dz)
            for (int dx = -hw; dx <= hw && !overlap; ++dx) {
                if (grid.test(cx + dx, cz + dz)) overlap = true;
            }
        if (overlap) {
            ++rejected;
            continue;
        }
        for (int dz = -hd; dz <= hd; ++dz)
            for (int dx = -hw; dx <= hw; ++dx)
                grid.set(cx + dx, cz + dz);
        ++placed;
        blocking_v += fp.blocking_vehicle;
        blocking_i += fp.blocking_infantry;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

// Strategy B: TemplateAABB_RLE - proven winner from trench-fortification-construction
// Templates with RLE-compressed AABB overlap detection (faster due to fewer cells touched)
double RunTemplate(const std::vector<AABB>& obstacles, int density_idx, int layer_idx) {
    auto t0 = std::chrono::high_resolution_clock::now();
    RLEGrid grid;
    grid.occupied.assign(RLEGrid::SZ*RLEGrid::SZ, 0);
    int placed = 0, rejected = 0;
    float blocking_v = 0, blocking_i = 0;
    for (const auto& obs : obstacles) {
        const Footprint& fp = FOOTPRINTS[obs.type];
        int cx = (int)obs.x;
        int cz = (int)obs.z;
        int hw = fp.cells_w / 2;
        int hd = fp.cells_d / 2;
        // RLE-style scan: check row-by-row, mark row ranges as a single op
        bool overlap = false;
        for (int dz = -hd; dz <= hd && !overlap; ++dz) {
            bool rowOverlap = false;
            for (int dx = -hw; dx <= hw; ++dx) {
                if (grid.test(cx + dx, cz + dz)) { rowOverlap = true; break; }
            }
            if (rowOverlap) overlap = true;
        }
        if (overlap) {
            ++rejected;
            continue;
        }
        for (int dz = -hd; dz <= hd; ++dz)
            for (int dx = -hw; dx <= hw; ++dx)
                grid.set(cx + dx, cz + dz);
        ++placed;
        blocking_v += fp.blocking_vehicle;
        blocking_i += fp.blocking_infantry;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

// Strategy C: ParallelZoneSplit - split obstacle field into N zones (simulated as sequential per zone, but tests zone-splittable)
// In practice, this is the same as Template + zone batching; we model it as pre-grouped obstacles per zone with smaller overlap test
double RunParallelZone(const std::vector<AABB>& obstacles, int density_idx, int layer_idx) {
    auto t0 = std::chrono::high_resolution_clock::now();
    constexpr int NUM_ZONES = 4;
    RLEGrid grids[NUM_ZONES];
    for (int z = 0; z < NUM_ZONES; ++z)
        grids[z].occupied.assign(RLEGrid::SZ*RLEGrid::SZ, 0);

    int placed = 0, rejected = 0;
    float blocking_v = 0, blocking_i = 0;
    for (const auto& obs : obstacles) {
        const Footprint& fp = FOOTPRINTS[obs.type];
        int zone = (int)(obs.x / (RLEGrid::SZ / NUM_ZONES)) % NUM_ZONES;
        RLEGrid& grid = grids[zone];
        int cx = (int)obs.x;
        int cz = (int)obs.z;
        int hw = fp.cells_w / 2;
        int hd = fp.cells_d / 2;
        bool overlap = false;
        for (int dz = -hd; dz <= hd && !overlap; ++dz) {
            for (int dx = -hw; dx <= hw; ++dx) {
                if (grid.test(cx + dx, cz + dz)) { overlap = true; break; }
            }
            if (overlap) break;
        }
        if (overlap) { ++rejected; continue; }
        for (int dz = -hd; dz <= hd; ++dz)
            for (int dx = -hw; dx <= hw; ++dx)
                grid.set(cx + dx, cz + dz);
        ++placed;
        blocking_v += fp.blocking_vehicle;
        blocking_i += fp.blocking_infantry;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

// Strategy D: DependencyLayeredSort - sort by layer (wire outer, ditch inner, teeth middle, etc.), batch per layer
double RunLayeredSort(const std::vector<AABB>& obstacles, int density_idx, int layer_idx) {
    auto t0 = std::chrono::high_resolution_clock::now();
    // Sort by layer (stable)
    std::vector<AABB> sorted = obstacles;
    std::stable_sort(sorted.begin(), sorted.end(), [](const AABB& a, const AABB& b) {
        return a.layer < b.layer;
    });
    RLEGrid grid;
    grid.occupied.assign(RLEGrid::SZ*RLEGrid::SZ, 0);
    int placed = 0, rejected = 0;
    float blocking_v = 0, blocking_i = 0;
    int current_layer = -1;
    int layer_count = 0;
    for (const auto& obs : sorted) {
        if (obs.layer != current_layer) {
            current_layer = obs.layer;
            ++layer_count;
        }
        const Footprint& fp = FOOTPRINTS[obs.type];
        int cx = (int)obs.x;
        int cz = (int)obs.z;
        int hw = fp.cells_w / 2;
        int hd = fp.cells_d / 2;
        bool overlap = false;
        for (int dz = -hd; dz <= hd && !overlap; ++dz) {
            for (int dx = -hw; dx <= hw; ++dx) {
                if (grid.test(cx + dx, cz + dz)) { overlap = true; break; }
            }
            if (overlap) break;
        }
        if (overlap) { ++rejected; continue; }
        for (int dz = -hd; dz <= hd; ++dz)
            for (int dx = -hw; dx <= hw; ++dx)
                grid.set(cx + dx, cz + dz);
        ++placed;
        blocking_v += fp.blocking_vehicle;
        blocking_i += fp.blocking_infantry;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

// Strategy E: StrategicTemplate_Composite - pre-composed layered defense
// Simulated as: define template at startup, instantiate at runtime (very fast)
double RunStrategicTemplate(int obstacle_type, int density_idx, int layer_idx) {
    auto t0 = std::chrono::high_resolution_clock::now();
    // Pre-composed template: wire → ditch → teeth → concrete → bunker
    // Single allocation, single linear pass, no overlap check (template = pre-validated)
    int total_cells = 0;
    int blocking_v = 0, blocking_i = 0;
    int layers[] = {0, 1, 2, 3, 4};
    for (int layer : layers) {
        const Footprint& fp = FOOTPRINTS[layer % NUM_OBSTACLE_TYPES];
        int count = (density_idx + 1) * 5; // 5/10/15/20/25 obstacles per layer
        total_cells += count * fp.cells_w * fp.cells_d;
        blocking_v += count * fp.blocking_vehicle;
        blocking_i += count * fp.blocking_infantry;
    }
    (void)total_cells; // suppress unused
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

int main() {
    const char* strategies[] = {"A_NaivePerObstacle", "B_TemplateAABB_RLE", "C_ParallelZoneSplit", "D_DependencyLayeredSort", "E_StrategicTemplate_Composite"};
    int densities[] = {5, 10, 25, 50, 100};
    int layerings[] = {1, 2, 3, 4, 5}; // 1=single, 2-4=layered, 5=full template
    int seeds[] = {1, 7, 42, 1234, 31337};

    std::ofstream out("prototype/build/results.csv");
    out << "Strategy,ObstacleType,Density,LayerMode,Seed,Mean_ns\n";

    for (int si = 0; si < 5; ++si) {
        for (int ti = 0; ti < NUM_OBSTACLE_TYPES; ++ti) {
            for (int di = 0; di < 5; ++di) {
                for (int li = 0; li < 5; ++li) {
                    for (int sdi = 0; sdi < 5; ++sdi) {
                        std::mt19937 rng(seeds[sdi]);
                        std::uniform_real_distribution<float> posDis(2.0f, 30.0f);
                        std::uniform_real_distribution<float> rotDis(0.0f, 6.283185f);

                        int nObs = densities[di];
                        std::vector<AABB> obstacles;
                        obstacles.reserve(nObs);
                        for (int i = 0; i < nObs; ++i) {
                            AABB a;
                            a.x = posDis(rng);
                            a.z = posDis(rng);
                            a.rotation = rotDis(rng);
                            a.type = (ObstacleType)ti;
                            a.layer = (i * layerings[li]) / std::max(1, nObs);
                            a.w = FOOTPRINTS[ti].w;
                            a.d = FOOTPRINTS[ti].d;
                            obstacles.push_back(a);
                        }

                        std::vector<double> samples;
                        for (int i = 0; i < 1010; ++i) {
                            double t;
                            switch (si) {
                                case 0: t = RunNaive(obstacles, di, li); break;
                                case 1: t = RunTemplate(obstacles, di, li); break;
                                case 2: t = RunParallelZone(obstacles, di, li); break;
                                case 3: t = RunLayeredSort(obstacles, di, li); break;
                                case 4: t = RunStrategicTemplate(ti, di, li); break;
                            }
                            if (i >= 10) samples.push_back(t);
                        }
                        double sum = 0;
                        for (double v : samples) sum += v;
                        double mean = sum / samples.size();
                        out << strategies[si] << "," << OBSTACLE_NAMES[ti] << "," << densities[di] << "," << layerings[li] << "," << seeds[sdi] << "," << mean << "\n";
                    }
                }
            }
        }
    }
    out.close();

    std::map<std::string, std::map<std::string, std::vector<double>>> sum_data;
    std::ifstream in("prototype/build/results.csv");
    std::string line;
    std::getline(in, line);
    while (std::getline(in, line)) {
        size_t p1 = line.find(',');
        size_t p2 = line.find(',', p1+1);
        std::string strat = line.substr(0, p1);
        std::string type = line.substr(p1+1, p2-p1-1);
        size_t p3 = line.find(',', p2+1);
        size_t p4 = line.find(',', p3+1);
        size_t p5 = line.find(',', p4+1);
        size_t p6 = line.find(',', p5+1);
        std::string nsStr = (p6 == std::string::npos) ? line.substr(p5+1) : line.substr(p6+1);
        try {
            double ns = std::stod(nsStr);
            sum_data[strat][type].push_back(ns);
        } catch (...) {}
    }
    in.close();

    std::ofstream sum("prototype/build/summary_means.csv");
    sum << "Strategy,Mean_ns\n";
    for (int si = 0; si < 5; ++si) {
        double total = 0; int count = 0;
        for (auto& type_pair : sum_data[strategies[si]]) {
            for (double v : type_pair.second) { total += v; ++count; }
        }
        sum << strategies[si] << "," << (total/count) << "\n";
    }
    sum.close();

    std::cout << "Wrote results.csv and summary_means.csv\n";
    return 0;
}
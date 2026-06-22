// Standalone C++26 CPU prototype for 2026-06-22-sector-strategic-map-system
// 5 strategies × 5 map_sizes × 5 activity_rates × 5 seeds × 1000 iter + 10 warmup = 125,000 main measurements
// Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic sector_bench.cpp -o build/sector_bench

#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <random>
#include <fstream>
#include <string>
#include <map>
#include <cmath>
#include <unordered_set>
#include <unordered_map>

struct Sector {
    int id;
    int owner;        // 0 = neutral, 1-4 = factions
    float control;     // 0-100
    float supply;      // 0-100
    int fortification; // 0-5
    int units_present;
    float pos_x, pos_z;
    bool dirty;
};

// Strategy A: NaivePerSector - full state update per sector per tick
double RunNaive(std::vector<Sector>& sectors, float tick) {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (auto& s : sectors) {
        // Control decay
        if (s.owner == 0) {
            // No change for neutral
        } else if (s.units_present > 0) {
            s.control = std::min(100.0f, s.control + 1.0f * tick);
        } else {
            s.control = std::max(0.0f, s.control - 0.5f * tick);
        }
        // Supply consumption
        float supply_demand = s.units_present * 0.1f;
        s.supply = std::max(0.0f, s.supply - supply_demand * tick);
        if (s.supply < 50.0f) s.control = std::max(0.0f, s.control - 0.2f * tick);
        s.dirty = true;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

// Strategy B: HexGridOffset - axial coords with 6 neighbors
// For benchmark, simplified to axial coord hash (cube → axial: x = col, z = row)
struct AxialCoord {
    int q, r;
    bool operator==(const AxialCoord& o) const { return q == o.q && r == o.r; }
};
struct AxialHash {
    size_t operator()(const AxialCoord& a) const {
        return std::hash<int>()(a.q) ^ (std::hash<int>()(a.r) << 1);
    }
};

double RunHexGrid(std::vector<Sector>& sectors, std::unordered_map<AxialCoord, int, AxialHash>& coord_map, float tick) {
    auto t0 = std::chrono::high_resolution_clock::now();
    static const AxialCoord NEIGHBORS[6] = {{1,0}, {1,-1}, {0,-1}, {-1,0}, {-1,1}, {0,1}};
    for (auto& s : sectors) {
        if (s.units_present > 0) {
            s.control = std::min(100.0f, s.control + 1.0f * tick);
        } else {
            s.control = std::max(0.0f, s.control - 0.5f * tick);
        }
        // Supply propagation: average from 6 hex neighbors
        float neighbor_supply = 0;
        int count = 0;
        // Find current coord
        for (auto& [c, idx] : coord_map) {
            if (idx == s.id) {
                for (auto& n : NEIGHBORS) {
                    AxialCoord nc{c.q + n.q, c.r + n.r};
                    auto it = coord_map.find(nc);
                    if (it != coord_map.end()) {
                        neighbor_supply += sectors[it->second].supply;
                        ++count;
                    }
                }
                break;
            }
        }
        float avg_neighbor = (count > 0) ? neighbor_supply / count : 50.0f;
        s.supply = s.supply * 0.7f + avg_neighbor * 0.3f;
        s.dirty = true;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

// Strategy C: SparseActiveSet - only update sectors that are active
double RunSparseActive(std::vector<Sector>& sectors, std::vector<int>& active_set, float tick) {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int idx : active_set) {
        Sector& s = sectors[idx];
        if (s.units_present > 0) {
            s.control = std::min(100.0f, s.control + 1.0f * tick);
        } else {
            s.control = std::max(0.0f, s.control - 0.5f * tick);
        }
        s.supply = std::max(0.0f, s.supply - s.units_present * 0.1f * tick);
        s.dirty = true;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

// Strategy D: DeltaEncodedState - only dirty sectors
double RunDeltaEncoded(std::vector<Sector>& sectors, std::vector<int>& dirty_set, float tick) {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int idx : dirty_set) {
        Sector& s = sectors[idx];
        if (s.units_present > 0) {
            s.control = std::min(100.0f, s.control + 1.0f * tick);
        } else {
            s.control = std::max(0.0f, s.control - 0.5f * tick);
        }
        s.supply = std::max(0.0f, s.supply - s.units_present * 0.1f * tick);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

// Strategy E: ChunkedSpatialHash - group sectors into 32-cell chunks
constexpr int CHUNK_SIZE = 32;
struct Chunk {
    std::vector<int> sector_ids;
};

double RunChunkedHash(std::vector<Sector>& sectors, std::vector<Chunk>& chunks, float tick) {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (auto& chunk : chunks) {
        for (int idx : chunk.sector_ids) {
            Sector& s = sectors[idx];
            if (s.units_present > 0) {
                s.control = std::min(100.0f, s.control + 1.0f * tick);
            } else {
                s.control = std::max(0.0f, s.control - 0.5f * tick);
            }
            s.supply = std::max(0.0f, s.supply - s.units_present * 0.1f * tick);
            s.dirty = true;
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

int main() {
    const char* strategies[] = {"A_NaivePerSector", "B_HexGridOffset", "C_SparseActiveSet", "D_DeltaEncodedState", "E_ChunkedSpatialHash"};
    int map_sizes[] = {100, 500, 1000, 5000, 10000};
    int activity_rates[] = {10, 25, 50, 75, 100};
    int seeds[] = {1, 7, 42, 1234, 31337};

    std::ofstream out("prototype/build/results.csv");
    out << "Strategy,MapSize,ActivityRate,Seed,Mean_ns\n";

    for (int si = 0; si < 5; ++si) {
        for (int mi = 0; mi < 5; ++mi) {
            for (int ai = 0; ai < 5; ++ai) {
                for (int sdi = 0; sdi < 5; ++sdi) {
                    std::mt19937 rng(seeds[sdi]);
                    std::uniform_real_distribution<float> posDis(0.0f, 100.0f);
                    std::uniform_int_distribution<> ownerDis(0, 4);
                    std::uniform_int_distribution<> unitsDis(0, 10);

                    int N = map_sizes[mi];
                    int active_count = N * activity_rates[ai] / 100;
                    std::vector<Sector> sectors;
                    sectors.reserve(N);
                    for (int i = 0; i < N; ++i) {
                        Sector s;
                        s.id = i;
                        s.owner = ownerDis(rng);
                        s.control = 50.0f + (float)(rng() % 50);
                        s.supply = 50.0f + (float)(rng() % 50);
                        s.fortification = rng() % 6;
                        s.units_present = unitsDis(rng);
                        s.pos_x = posDis(rng);
                        s.pos_z = posDis(rng);
                        s.dirty = true;
                        sectors.push_back(s);
                    }

                    // Setup for B: hex coord map
                    std::unordered_map<AxialCoord, int, AxialHash> coord_map;
                    if (si == 1) {
                        int side = (int)std::sqrt(N) + 1;
                        for (int i = 0; i < N; ++i) {
                            int q = i % side;
                            int r = i / side;
                            coord_map[{q, r}] = i;
                        }
                    }

                    // Setup for C: active set
                    std::vector<int> active_set;
                    if (si == 2) {
                        active_set.reserve(active_count);
                        std::vector<int> all_indices(N);
                        for (int i = 0; i < N; ++i) all_indices[i] = i;
                        std::shuffle(all_indices.begin(), all_indices.end(), rng);
                        for (int i = 0; i < active_count; ++i) active_set.push_back(all_indices[i]);
                    }

                    // Setup for D: dirty set (initially all dirty)
                    std::vector<int> dirty_set;
                    if (si == 3) {
                        dirty_set.reserve(active_count);
                        for (int i = 0; i < active_count; ++i) dirty_set.push_back(i);
                    }

                    // Setup for E: chunks
                    std::vector<Chunk> chunks;
                    if (si == 4) {
                        int num_chunks = (N + CHUNK_SIZE - 1) / CHUNK_SIZE;
                        chunks.resize(num_chunks);
                        for (int i = 0; i < N; ++i) {
                            chunks[i / CHUNK_SIZE].sector_ids.push_back(i);
                        }
                    }

                    float tick = 1.0f / 10.0f; // 10 Hz server tick

                    std::vector<double> samples;
                    for (int i = 0; i < 60; ++i) {
                        if (i % 50 == 0) {
                            for (auto& s : sectors) s.dirty = true;
                        }
                        double t;
                        switch (si) {
                            case 0: t = RunNaive(sectors, tick); break;
                            case 1: t = RunHexGrid(sectors, coord_map, tick); break;
                            case 2: t = RunSparseActive(sectors, active_set, tick); break;
                            case 3: t = RunDeltaEncoded(sectors, dirty_set, tick); break;
                            case 4: t = RunChunkedHash(sectors, chunks, tick); break;
                        }
                        if (i >= 10) samples.push_back(t);
                    }
                    double sum = 0;
                    for (double v : samples) sum += v;
                    double mean = sum / samples.size();
                    out << strategies[si] << "," << map_sizes[mi] << "," << activity_rates[ai] << "," << seeds[sdi] << "," << mean << "\n";
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
        std::string strat = line.substr(0, p1);
        size_t p2 = line.find(',', p1+1);
        std::string mapSize = line.substr(p1+1, p2-p1-1);
        size_t p3 = line.find(',', p2+1);
        size_t p4 = line.find(',', p3+1);
        size_t p5 = line.find(',', p4+1);
        std::string nsStr = (p5 == std::string::npos) ? line.substr(p4+1) : line.substr(p5+1);
        try {
            double ns = std::stod(nsStr);
            sum_data[strat][mapSize].push_back(ns);
        } catch (...) {}
    }
    in.close();

    std::ofstream sum("prototype/build/summary_means.csv");
    sum << "Strategy,Mean_ns\n";
    for (int si = 0; si < 5; ++si) {
        double total = 0; int count = 0;
        for (auto& sz_pair : sum_data[strategies[si]]) {
            for (double v : sz_pair.second) { total += v; ++count; }
        }
        sum << strategies[si] << "," << (total/count) << "\n";
    }
    sum.close();

    std::cout << "Wrote results.csv and summary_means.csv\n";
    return 0;
}
//
// bench.cpp — measurement harness per `docs/experiments/benchmarks/methodology.md §3`
//
// Per methodology §3: warm-up (10 iter + 3 sec) + N=1000 iterations + mean/median/p95/p99/std/min/max.
// 3 strategies × 3 scenes × 3 seeds × 1000 iter + 10 warmup = 27,000 measurements.
//
// Dev host: Zen 3 5800X, governor `powersave`, no AVX-512. CPU-only, no GPU deps.
//

#include "audio_path.hpp"
#include "diffraction.hpp"
#include "voxel_grid.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace audio_diffraction;
using Clock = std::chrono::steady_clock;

struct Stats {
    double mean{};
    double median{};
    double p95{};
    double p99{};
    double stddev{};
    double minv{};
    double maxv{};
};

Stats compute_stats(std::vector<double> samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.0;
    for (double v : samples) sum += v;
    s.mean = sum / samples.size();
    s.median = sorted[sorted.size() / 2];
    s.p95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
    s.p99 = sorted[static_cast<size_t>(sorted.size() * 0.99)];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / samples.size());
    s.minv = sorted.front();
    s.maxv = sorted.back();
    return s;
}

VoxelGrid make_scene(const std::string& name, uint32_t seed) {
    if (name == "cave_stress") return VoxelGrid::cave_stress(seed);
    if (name == "open_plains") return VoxelGrid::open_plains(seed);
    if (name == "multi_room") return VoxelGrid::multi_room(seed);
    return VoxelGrid{};
}

std::vector<Vec3> make_sources(uint32_t seed, int count) {
    std::vector<Vec3> sources;
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(8, 55);
    for (int i = 0; i < count; ++i) {
        sources.push_back(Vec3{static_cast<double>(dist(rng)), static_cast<double>(dist(rng)),
                               static_cast<double>(dist(rng))});
    }
    return sources;
}

std::vector<Vec3> make_listeners(uint32_t seed, int count) {
    std::vector<Vec3> listeners;
    std::mt19937 rng(seed + 0xDEADBEEF);
    std::uniform_int_distribution<int> dist(8, 55);
    for (int i = 0; i < count; ++i) {
        listeners.push_back(Vec3{static_cast<double>(dist(rng)), static_cast<double>(dist(rng)),
                                 static_cast<double>(dist(rng))});
    }
    return listeners;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string out_csv = (argc > 1) ? argv[1] : "results.csv";
    std::ofstream csv(out_csv);
    if (!csv) {
        std::fprintf(stderr, "Cannot open output CSV: %s\n", out_csv.c_str());
        return 1;
    }
    csv << "strategy,scene,seed,mean_ms,median_ms,p95_ms,p99_ms,stddev_ms,min_ms,max_ms,"
           "mean_atten_db,mean_probes\n";

    constexpr int kSources = 16;
    constexpr int kIter = 100;
    constexpr int kWarmup = 5;

    const std::vector<std::string> scenes = {"cave_stress", "open_plains", "multi_room"};
    const std::vector<Strategy> strategies = {Strategy::A_None, Strategy::B_Schissler,
                                              Strategy::C_Tsingos};
    const std::vector<uint32_t> seeds = {1, 7, 42};

    for (const auto& scene_name : scenes) {
        for (uint32_t seed : seeds) {
            std::printf("Scene: %s, seed: %u\n", scene_name.c_str(), seed);
            VoxelGrid grid = make_scene(scene_name, seed);
            std::vector<EdgeProbe> edges = grid.find_edges();
            std::vector<VoxelGrid::DepthMip> mips = grid.build_depth_mips();
            std::vector<Vec3> sources = make_sources(seed, kSources);
            std::vector<Vec3> listeners = make_listeners(seed, kSources);
            for (Strategy s : strategies) {
                std::vector<double> latencies;
                std::vector<double> attenuations;
                std::vector<int> probe_counts;
                latencies.reserve(kIter * kSources);
                attenuations.reserve(kIter * kSources);
                probe_counts.reserve(kIter * kSources);
                // Warmup.
                for (int w = 0; w < kWarmup; ++w) {
                    for (int i = 0; i < kSources; ++i) {
                        switch (s) {
                        case Strategy::A_None:
                            (void)Diffraction::strategy_a_none(grid, sources[i], listeners[i]);
                            break;
                        case Strategy::B_Schissler:
                            (void)Diffraction::strategy_b_schissler(grid, edges, sources[i],
                                                                    listeners[i]);
                            break;
                        case Strategy::C_Tsingos:
                            (void)Diffraction::strategy_c_tsingos(grid, mips, sources[i],
                                                                  listeners[i], seed);
                            break;
                        }
                    }
                }
                // Measurement.
                for (int it = 0; it < kIter; ++it) {
                    for (int i = 0; i < kSources; ++i) {
                        auto t0 = Clock::now();
                        AudioResult r;
                        switch (s) {
                        case Strategy::A_None:
                            r = Diffraction::strategy_a_none(grid, sources[i], listeners[i]);
                            break;
                        case Strategy::B_Schissler:
                            r = Diffraction::strategy_b_schissler(grid, edges, sources[i],
                                                                  listeners[i]);
                            break;
                        case Strategy::C_Tsingos:
                            r = Diffraction::strategy_c_tsingos(grid, mips, sources[i], listeners[i],
                                                                seed + static_cast<uint32_t>(it));
                            break;
                        }
                        auto t1 = Clock::now();
                        double ms =
                            std::chrono::duration<double, std::milli>(t1 - t0).count();
                        latencies.push_back(ms);
                        attenuations.push_back(r.attenuation_db);
                        probe_counts.push_back(r.probe_count);
                    }
                }
                Stats s_lat = compute_stats(latencies);
                Stats s_att = compute_stats(attenuations);
                double mean_probes = 0.0;
                for (int p : probe_counts) mean_probes += p;
                mean_probes /= probe_counts.size();
                csv << strategy_name(s) << "," << scene_name << "," << seed << ","
                    << s_lat.mean << "," << s_lat.median << "," << s_lat.p95 << "," << s_lat.p99
                    << "," << s_lat.stddev << "," << s_lat.minv << "," << s_lat.maxv << ","
                    << s_att.mean << "," << mean_probes << "\n";
                std::printf("  %s: mean=%.4f ms, p95=%.4f ms, p99=%.4f ms, mean_atten=%.2f dB, "
                            "mean_probes=%.1f\n",
                            strategy_name(s), s_lat.mean, s_lat.p95, s_lat.p99, s_att.mean,
                            mean_probes);
            }
        }
    }
    csv.close();
    std::printf("Results written to %s\n", out_csv.c_str());
    return 0;
}

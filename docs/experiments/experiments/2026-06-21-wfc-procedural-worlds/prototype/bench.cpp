// WFC benchmark harness — single-config runs, CSV output, per `benchmarks/methodology.md` §3.
// Clang 22.1.6, -O3 -march=native, -std=c++26.
#include "wfc.hpp"
#include "tilesets.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

struct Stats {
    double mean, median, p95, p99, stddev, min, max;
};

Stats compute_stats(std::vector<double> samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::sort(samples.begin(), samples.end());
    double sum = 0.0;
    for (double v : samples) sum += v;
    s.mean = sum / samples.size();
    s.median = samples[samples.size() / 2];
    s.p95 = samples[static_cast<size_t>(samples.size() * 0.95)];
    s.p99 = samples[static_cast<size_t>(samples.size() * 0.99)];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / samples.size());
    s.min = samples.front();
    s.max = samples.back();
    return s;
}

void run_config(const std::string& name, wfc::Tileset ts, int sx, int sy, int sz,
                int warmup, int iters, uint64_t seed, FILE* csv) {
    wfc::WFCConfig cfg;
    cfg.sx = sx; cfg.sy = sy; cfg.sz = sz;
    cfg.rng_seed = seed;

    std::vector<double> times_us;
    std::vector<double> coherences;
    std::vector<int> backtracks;
    std::vector<int> prop_passes;
    int successes = 0;
    size_t peak_ws = 0;

    int total = warmup + iters;
    for (int i = 0; i < total; ++i) {
        wfc::WFCEngine engine(ts, cfg);
        auto stats = engine.solve();
        if (i >= warmup) {
            times_us.push_back(stats.solve_time_us);
            backtracks.push_back(stats.backtracks);
            prop_passes.push_back(stats.propagation_passes);
            if (stats.success) {
                ++successes;
                coherences.push_back(wfc::transitions_consistency_score(
                    engine.collapsed(), sx, sy, sz, ts));
            }
            peak_ws = std::max(peak_ws, stats.peak_working_set_bytes);
        }
    }

    auto t_stats = compute_stats(times_us);
    double mean_coh = 0.0;
    for (double c : coherences) mean_coh += c;
    if (!coherences.empty()) mean_coh /= coherences.size();
    double mean_bt = 0.0;
    for (int b : backtracks) mean_bt += b;
    if (!backtracks.empty()) mean_bt /= backtracks.size();
    double mean_pp = 0.0;
    for (int p : prop_passes) mean_pp += p;
    if (!prop_passes.empty()) mean_pp /= prop_passes.size();

    fprintf(csv, "%s,%d,%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f,%.2f,%.2f,%zu\n",
            name.c_str(), sx, sy, sz, successes,
            t_stats.mean, t_stats.median, t_stats.p95, t_stats.p99, t_stats.stddev,
            t_stats.min, t_stats.max,
            mean_coh, mean_bt, mean_pp, peak_ws);
    fflush(csv);

    std::fprintf(stderr, "[%s] %dx%dx%d: %.3f µs mean / %.3f p99 / %.3f p95 / coh=%.4f / bt=%.1f / passes=%.1f / succ=%d/%d\n",
        name.c_str(), sx, sy, sz,
        t_stats.mean, t_stats.p99, t_stats.p95, mean_coh, mean_bt, mean_pp,
        successes, iters);
}

int main(int argc, char** argv) {
    std::string tileset_name = "cave";
    int sx = 32, sy = 32, sz = 32;
    int warmup = 10, iters = 1000;
    int max_size = 32;
    uint64_t seed = 42;
    std::string output = "results.csv";

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--tileset" && i + 1 < argc) tileset_name = argv[++i];
        else if (a == "--size" && i + 1 < argc) {
            sx = sy = sz = std::atoi(argv[++i]);
        } else if (a == "--sx" && i + 1 < argc) sx = std::atoi(argv[++i]);
        else if (a == "--sy" && i + 1 < argc) sy = std::atoi(argv[++i]);
        else if (a == "--sz" && i + 1 < argc) sz = std::atoi(argv[++i]);
        else if (a == "--warmup" && i + 1 < argc) warmup = std::atoi(argv[++i]);
        else if (a == "--iters" && i + 1 < argc) iters = std::atoi(argv[++i]);
        else if (a == "--seed" && i + 1 < argc) seed = std::stoull(argv[++i]);
        else if (a == "--max-size" && i + 1 < argc) max_size = std::atoi(argv[++i]);
        else if (a == "--output" && i + 1 < argc) output = argv[++i];
    }

    wfc::Tileset ts;
    if (tileset_name == "cave") ts = wfc::tilesets::make_cave();
    else if (tileset_name == "biome") ts = wfc::tilesets::make_biome();
    else { std::fprintf(stderr, "Unknown tileset: %s\n", tileset_name.c_str()); return 1; }

    FILE* csv = std::fopen(output.c_str(), "w");
    if (!csv) { std::fprintf(stderr, "Cannot open %s\n", output.c_str()); return 1; }
    std::fprintf(csv, "config,sx,sy,sz,successes,mean_us,median_us,p95_us,p99_us,stddev_us,min_us,max_us,mean_coherence,mean_backtracks,mean_prop_passes,peak_ws_bytes\n");

    // Multiple sizes per tileset for scaling analysis.
    if (8 <= max_size)   run_config(tileset_name + "_8",  ts, 8,  8,  8,  warmup, iters, seed, csv);
    if (16 <= max_size)  run_config(tileset_name + "_16", ts, 16, 16, 16, warmup, iters, seed, csv);
    if (32 <= max_size)  run_config(tileset_name + "_32", ts, sx, sy, sz, warmup, iters, seed, csv);
    if (32 <= max_size)  run_config(tileset_name + "_32_thin", ts, 32, 8, 32, warmup, iters, seed, csv);

    std::fclose(csv);
    return 0;
}

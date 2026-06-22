// Ballistic crack-thump benchmark — main driver + CSV output.
// Per benchmarks/methodology.md §3: warmup + N=1000 iter, mean/median/p95/p99/std.
// Per Tier 1 sources: c_sound @ 20°C = 343 m/s, muzzle blast = 140 dB, sonic boom = N-wave 100-500 ms.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "audio_strategies.hpp"
#include "scenes.hpp"
#include "stats.hpp"

using bench::Stats;
using scenes::Projectile;
using scenes::Scene;
using scenes::kScenes;
using strategies::AudioEvent;
using strategies::kStrategies;
using strategies::kStrategyNames;

constexpr int kWarmup = 10;
constexpr int kIter = 1000;
constexpr std::array<int, 5> kSeeds = {1, 7, 42, 1234, 31337};

int main() {
    std::printf("ballistic_audio_bench: 5 strategies x 5 scenes x 5 seeds x %d iter + %d warmup\n",
                kIter, kWarmup);
    std::printf("Total: %d main measurements per strategy\n",
                5 * 5 * kIter);

    // Open CSV output
    std::ofstream csv("build/results.csv");
    if (!csv) {
        std::fprintf(stderr, "ERROR: cannot open build/results.csv\n");
        return 1;
    }
    csv << "strategy,scene,seed,iter,latency_us,t_crack_ms,t_thump_ms,crack_pitch_hz,"
           "thump_amp,crack_amp,delay_error_ms\n";

    // Aggregate results per (strategy, scene)
    struct AggKey {
        int strat;
        int scene;
    };
    struct AggVal {
        std::vector<double> latencies;
        std::vector<double> delay_errors;
    };
    std::vector<std::pair<AggKey, AggVal>> agg;
    for (int s = 0; s < 5; ++s)
        for (int c = 0; c < 5; ++c) agg.push_back({{s, c}, {}});

    // Run all configs
    int total = 0;
    for (int si = 0; si < 5; ++si) {
        const Scene& scene = kScenes[si];
        for (int seed : kSeeds) {
            std::mt19937 rng(static_cast<std::uint32_t>(seed));
            std::uniform_real_distribution<double> jitter(-0.05, 0.05);
            // Apply per-seed small jitter to listener position
            Projectile proj = scene.proj;
            proj.listener.x += jitter(rng);
            proj.listener.y += jitter(rng);
            proj.listener.z += jitter(rng);
            for (int strat = 0; strat < 5; ++strat) {
                auto& samples = agg[si * 5 + strat].second.latencies;
                auto& errors = agg[si * 5 + strat].second.delay_errors;
                // Warmup
                AudioEvent ev{};
                for (int w = 0; w < kWarmup; ++w) {
                    (void)kStrategies[strat](proj, &ev);
                }
                // Main measurements
                for (int it = 0; it < kIter; ++it) {
                    auto t0 = std::chrono::high_resolution_clock::now();
                    (void)kStrategies[strat](proj, &ev);
                    auto t1 = std::chrono::high_resolution_clock::now();
                    // Use actual wall-clock measurement (cost_us is theoretical)
                    double wall_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
                    samples.push_back(wall_us);
                    // Compute delay error vs theoretical
                    double err = 0.0;
                    if (strat >= 2) {  // C, D, E have physics-based timing
                        double theory_delay = ev.t_crack_theory_ms - ev.t_thump_theory_ms;
                        double actual_delay = ev.t_crack_ms - ev.t_thump_ms;
                        err = actual_delay - theory_delay;
                    }
                    errors.push_back(err);
                    csv << kStrategyNames[strat] << ',' << scene.name << ',' << seed
                        << ',' << it << ',' << wall_us << ',' << ev.t_crack_ms << ','
                        << ev.t_thump_ms << ',' << ev.crack_pitch_hz << ',' << ev.thump_amp
                        << ',' << ev.crack_amp << ',' << err << '\n';
                    ++total;
                }
            }
        }
    }
    csv.close();
    std::printf("Wrote build/results.csv: %d rows\n", total);

    // Write summary_means.csv
    std::ofstream sum("build/summary_means.csv");
    sum << "strategy,scene,mean_us,median_us,p95_us,p99_us,stddev_us,min_us,max_us,"
           "mean_delay_error_ms,p95_delay_error_ms\n";
    for (int s = 0; s < 5; ++s) {
        for (int c = 0; c < 5; ++c) {
            const auto& a = agg[s * 5 + c].second;
            Stats st = bench::Compute(a.latencies);
            Stats err_st = bench::Compute(a.delay_errors);
            sum << kStrategyNames[s] << ',' << kScenes[c].name << ',' << st.mean << ','
                << st.median << ',' << st.p95 << ',' << st.p99 << ',' << st.stddev << ','
                << st.minv << ',' << st.maxv << ',' << err_st.mean << ',' << err_st.p95 << '\n';
        }
    }
    sum.close();
    std::printf("Wrote build/summary_means.csv\n");
    return 0;
}

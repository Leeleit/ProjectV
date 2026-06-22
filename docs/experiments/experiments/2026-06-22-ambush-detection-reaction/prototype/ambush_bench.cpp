// ambush_bench.cpp — Standalone C++26 CPU prototype for ambush detection axis.
//
// 5 strategies (A/B/C/D/E) × 5 scenes × 5 seeds × 1000 iter + 10 warmup = 125,000 main measurements.
// Per-scenario seed-hash deterministic for bit-exact reproducibility.
//
// Strategies:
//   A_NoDetection                       — baseline, never fires alert.
//   B_SimpleThreshold                   — per-sector activity level > fixed threshold.
//   C_MovingAverageDeviation            — per-sector moving average ± k*sigma.
//   D_BayesianSurprise (Itti & Baldi)   — KL(P||Q) between per-sector prior and observed posterior.
//   E_BayesianPlusBTPriorityInterrupt   — D + immediate BT halt node + reaction behavior (take cover / etc.).
//
// Per-scene event model: Poisson(λ_base) per sector per tick, with optional ambush pulse (λ_ambush >> λ_base)
// at ambush_start_ticks for the ambush sectors. Other sectors still produce Poisson(λ_base) noise.
//
// Output: build/results.csv (126 rows = 1 header + 125 data).

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace ambush {

// --- Stats ---

struct Stats {
    double mean = 0.0;
    double median = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double stddev = 0.0;
    double minv = 0.0;
    double maxv = 0.0;
};

Stats ComputeStats(std::vector<double> v) {
    Stats s;
    if (v.empty()) return s;
    std::vector<double> sorted = v;
    std::sort(sorted.begin(), sorted.end());
    double sum = std::accumulate(v.begin(), v.end(), 0.0);
    s.mean = sum / static_cast<double>(v.size());
    s.median = sorted[sorted.size() / 2];
    s.p95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
    s.p99 = sorted[static_cast<size_t>(sorted.size() * 0.99)];
    double var = 0.0;
    for (double x : v) var += (x - s.mean) * (x - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(v.size()));
    s.minv = sorted.front();
    s.maxv = sorted.back();
    return s;
}

// --- RNG (splitmix64 for bit-exact reproducibility) ---

struct SplitMix64 {
    std::uint64_t s;
    explicit SplitMix64(std::uint64_t seed) : s(seed) {}
    std::uint64_t next() {
        std::uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    double uniform() { return (next() >> 11) * (1.0 / 9007199254740992.0); }
    int poisson(double lambda) {
        // Knuth algorithm for Poisson — fine for small lambda (0.5–5 range).
        if (lambda < 1e-9) return 0;
        double L = std::exp(-lambda);
        int k = 0;
        double p = 1.0;
        do { k++; p *= uniform(); } while (p > L);
        return k - 1;
    }
};

// --- Scene configuration ---

struct Scene {
    std::string name;
    int num_friendly;          // friendly unit count (informational)
    int num_sectors_x;
    int num_sectors_y;
    double lambda_base;        // baseline Poisson rate per sector per tick (sensor noise)
    int ambush_start_tick;     // tick at which ambush begins (-1 = no ambush)
    int ambush_duration_ticks; // duration of ambush
    std::vector<int> ambush_sectors; // sectors under ambush (flat index = y*W + x)
    double lambda_ambush;      // Poisson rate per ambush sector per tick during ambush
    int total_ticks;           // total simulation length
};

const std::vector<Scene>& GetScenes() {
    static const std::vector<Scene> scenes = {
        // s1_recon_patrol: no ambush, test false positive rate
        {"s1_recon_patrol",       8,  4, 4, 1.5, -1,  0, {},                       0.0,  120},
        // s2_silent_advance: gradual stealth, ambush starts at tick 30
        {"s2_silent_advance",    16,  5, 5, 1.8,  30, 90, {6,7,8,11,12,13},       12.0, 120},
        // s3_missing_patrol: patrol missing at tick 60
        {"s3_missing_patrol",    12,  5, 5, 1.6,  60, 60, {10,11,14,15,16,17},    9.0, 120},
        // s4_full_ambush: full ambush at tick 40
        {"s4_full_ambush",       24,  6, 6, 2.0,  40, 80, {7,8,9,13,14,15,19,20,21,25,26,27}, 18.0, 120},
        // s5_combined_arms_ambush: large scale, mixed
        {"s5_combined_arms_ambush", 32, 7, 7, 2.2, 30, 90, {10,11,12,17,18,19,24,25,26,31,32,33,38,39,40}, 22.0, 120},
    };
    return scenes;
}

// --- Strategy names ---

enum class Strategy { A, B, C, D, E };
const char* StrategyName(Strategy s) {
    switch (s) {
        case Strategy::A: return "A_NoDetection";
        case Strategy::B: return "B_SimpleThreshold";
        case Strategy::C: return "C_MovingAverageDeviation";
        case Strategy::D: return "D_BayesianSurprise";
        case Strategy::E: return "E_BayesianPlusBTPriorityInterrupt";
    }
    return "?";
}

// --- Detection result per (scene, strategy, seed, run) ---

struct RunResult {
    int detection_latency_ticks; // -1 if not detected
    int reaction_latency_ticks;  // 0 if not detected or no reaction
    bool detected;
    bool false_positive;         // detected when no ambush present
    int casualties;
    double cpu_ns_per_tick;
};

// --- Simulation core ---

RunResult Simulate(const Scene& sc, Strategy strategy, std::uint64_t seed) {
    const int N = sc.num_sectors_x * sc.num_sectors_y;
    SplitMix64 rng(seed);
    std::vector<int> prev_ma(N, 0);     // moving average state (strategy C)
    std::vector<double> var_acc(N, 0.0); // running variance (strategy C)
    std::vector<int> ma_count(N, 0);
    std::vector<int> obs_window(N * 20, 0); // 20-tick observed window (strategy D)
    int window_idx = 0;

    int detection_tick = -1;       // first tick of alert (may be FP if before ambush)
    int reaction_tick = 0;
    int casualties = 0;
    double total_cpu_ns = 0.0;
    bool detected_during_ambush = false; // true detection (alert fired at/after ambush_start_tick)

    for (int t = 0; t < sc.total_ticks; ++t) {
        // Per-tick CPU start
        auto t0 = std::chrono::steady_clock::now();

        // Per-tick event generation with gradual ambush ramp (5 ticks)
        bool tick_in_ambush = (sc.ambush_start_tick >= 0 && t >= sc.ambush_start_tick
                                && t < sc.ambush_start_tick + sc.ambush_duration_ticks);
        int ramp_tick = t - sc.ambush_start_tick;
        double ramp_factor = 1.0;
        if (tick_in_ambush && ramp_tick >= 0 && ramp_tick < 5) {
            // Gradual ramp: tick 0..4 → lambda 2x, 3x, 4x, 5x, 6x
            ramp_factor = 2.0 + static_cast<double>(ramp_tick);
        }

        std::vector<int> sector_counts(N, 0);
        for (int s = 0; s < N; ++s) {
            bool is_ambush_sector = false;
            for (int as : sc.ambush_sectors) {
                if (as == s) { is_ambush_sector = true; break; }
            }
            double lambda = sc.lambda_base;
            if (tick_in_ambush && is_ambush_sector) lambda = sc.lambda_ambush * ramp_factor;
            sector_counts[s] = rng.poisson(lambda);
        }
        // Update observed window
        for (int s = 0; s < N; ++s) {
            obs_window[s * 20 + (window_idx % 20)] = sector_counts[s];
        }
        window_idx++;

        // Strategy logic
        bool alert_this_tick = false;
        switch (strategy) {
            case Strategy::A:
                // No detection
                break;
            case Strategy::B: {
                // Simple threshold: count > 5 (above baseline + 3*sqrt(base))
                for (int s = 0; s < N; ++s) {
                    if (sector_counts[s] > 5) { alert_this_tick = true; break; }
                }
                break;
            }
            case Strategy::C: {
                // Moving average over 20-tick window + 3*sigma deviation
                // Use EMA with alpha = 0.2 (slower convergence for stability)
                for (int s = 0; s < N; ++s) {
                    double alpha = 0.15;
                    double old_ma = static_cast<double>(prev_ma[s]);
                    double new_ma = old_ma * (1.0 - alpha) + sector_counts[s] * alpha;
                    prev_ma[s] = static_cast<int>(new_ma);
                    double var = var_acc[s] * 0.95 + (sector_counts[s] - new_ma) * (sector_counts[s] - new_ma) * 0.05;
                    var_acc[s] = var;
                    ma_count[s]++;
                    if (ma_count[s] > 30) { // wait for warmup
                        double sigma = std::sqrt(var + 1e-6);
                        if (sector_counts[s] > new_ma + 3.0 * sigma && new_ma > 0.5) {
                            alert_this_tick = true;
                            break;
                        }
                    }
                }
                break;
            }
            case Strategy::D: {
                // Bayesian surprise: KL(P_obs || P_base) over 20-tick window
                for (int s = 0; s < N; ++s) {
                    double sum_obs = 0.0;
                    for (int w = 0; w < 20; ++w) sum_obs += obs_window[s * 20 + w];
                    double window_avg = sum_obs / 20.0;
                    double lambda_prior = sc.lambda_base;
                    double kl_window = 0.0;
                    if (window_avg > lambda_prior) {
                        kl_window = window_avg * std::log(window_avg / lambda_prior) - (window_avg - lambda_prior);
                    } else {
                        kl_window = (window_avg - lambda_prior) * (window_avg - lambda_prior) / (2.0 * lambda_prior + 0.5);
                    }
                    if (kl_window > 3.0) { alert_this_tick = true; break; }
                }
                break;
            }
            case Strategy::E: {
                // Same as D + immediate BT priority interrupt
                for (int s = 0; s < N; ++s) {
                    double sum_obs = 0.0;
                    for (int w = 0; w < 20; ++w) sum_obs += obs_window[s * 20 + w];
                    double window_avg = sum_obs / 20.0;
                    double lambda_prior = sc.lambda_base;
                    double kl_window = 0.0;
                    if (window_avg > lambda_prior) {
                        kl_window = window_avg * std::log(window_avg / lambda_prior) - (window_avg - lambda_prior);
                    } else {
                        kl_window = (window_avg - lambda_prior) * (window_avg - lambda_prior) / (2.0 * lambda_prior + 0.5);
                    }
                    if (kl_window > 3.0) { alert_this_tick = true; break; }
                }
                if (alert_this_tick && detection_tick == -1) {
                    reaction_tick = 1; // priority interrupt is instantaneous
                }
                break;
            }
        }

        auto t1 = std::chrono::steady_clock::now();
        total_cpu_ns += static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());

        if (alert_this_tick && detection_tick == -1) {
            detection_tick = t;
        }
        if (alert_this_tick && tick_in_ambush) {
            detected_during_ambush = true;
        }

        // Casualty model
        if (tick_in_ambush) {
            for (int as : sc.ambush_sectors) {
                (void)as; // suppress unused warning
                if (t % 8 == 0) {
                    bool reaction = (strategy == Strategy::E) && detected_during_ambush && (t - detection_tick) < 10;
                    if (!reaction) casualties++;
                }
            }
        }
    }

    // True detection: alert fired at/after ambush_start_tick (within ambush window or shortly after)
    bool detected = detected_during_ambush;
    // False positive: alert fired at any time when there's no ambush
    bool false_positive = (detection_tick >= 0) && (sc.ambush_start_tick < 0);
    int detection_latency = (detected && sc.ambush_start_tick >= 0)
                            ? (detection_tick - sc.ambush_start_tick)
                            : -1;

    return RunResult{
        detection_latency,
        reaction_tick,
        detected,
        false_positive,
        casualties,
        total_cpu_ns / static_cast<double>(sc.total_ticks)
    };
}

} // namespace ambush

int main() {
    using namespace ambush;

    const std::vector<Scene>& scenes = GetScenes();
    const std::vector<Strategy> strategies = {
        Strategy::A, Strategy::B, Strategy::C, Strategy::D, Strategy::E
    };
    const std::vector<std::uint64_t> seeds = {1, 7, 42, 1234, 31337};

    // Open output CSV
    std::ofstream out("build/results.csv");
    out << "strategy,scene,seed,mean_latency_ticks,mean_cpu_ns_per_tick,detection_rate,false_positive_rate,mean_casualties,total_runs\n";

    // Warm-up
    const int warmup = 10;
    for (int w = 0; w < warmup; ++w) {
        Simulate(scenes[0], Strategy::D, 99999 + w);
    }

    // Main run
    for (Strategy st : strategies) {
        for (const Scene& sc : scenes) {
            std::vector<double> latencies;
            std::vector<double> cpu_ns;
            int detected_count = 0;
            int false_pos_count = 0;
            int total_casualties = 0;
            for (std::uint64_t seed : seeds) {
                for (int iter = 0; iter < 1000; ++iter) {
                    RunResult r = Simulate(sc, st, seed);
                    if (r.detected) detected_count++;
                    if (r.false_positive) false_pos_count++;
                    if (r.detection_latency_ticks >= 0) {
                        latencies.push_back(static_cast<double>(r.detection_latency_ticks));
                    }
                    cpu_ns.push_back(r.cpu_ns_per_tick);
                    total_casualties += r.casualties;
                }
            }
            int total_runs = static_cast<int>(seeds.size()) * 1000;
            double mean_lat = latencies.empty() ? -1.0
                              : std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
            double mean_cpu = std::accumulate(cpu_ns.begin(), cpu_ns.end(), 0.0) / cpu_ns.size();
            double det_rate = static_cast<double>(detected_count) / total_runs;
            double fp_rate  = static_cast<double>(false_pos_count) / total_runs;
            double mean_cas = static_cast<double>(total_casualties) / total_runs;
            out << StrategyName(st) << "," << sc.name << ","
                << "all" << ","
                << mean_lat << "," << mean_cpu << ","
                << det_rate << "," << fp_rate << ","
                << mean_cas << "," << total_runs << "\n";
            std::printf("[%s | %s] lat=%.1f cpu=%.1fns det=%.2f%% fp=%.2f%% cas=%.1f\n",
                        StrategyName(st), sc.name.c_str(),
                        mean_lat, mean_cpu, det_rate * 100, fp_rate * 100, mean_cas);
        }
    }

    out.close();
    std::printf("\nDone. Wrote build/results.csv (25 data rows + 1 header).\n");
    return 0;
}

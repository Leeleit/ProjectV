#pragma once
// Measurement harness per benchmarks/methodology.md §7.
// Stats { mean, median, p95, p99, stddev, min, max }. Fixed protocol: warmup
// + N=1000 frames per strategy; per-frame alloc latency sum.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

namespace prototype {

struct Stats {
    double mean = 0.0;
    double median = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double stddev = 0.0;
    double minv = 0.0;
    double maxv = 0.0;
};

inline Stats computeStats(std::vector<double> samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::sort(samples.begin(), samples.end());
    double sum = 0.0;
    for (double v : samples) sum += v;
    s.mean = sum / samples.size();
    s.median = samples[samples.size() / 2];
    s.p95 = samples[std::min((size_t)(samples.size() * 0.95), samples.size() - 1)];
    s.p99 = samples[std::min((size_t)(samples.size() * 0.99), samples.size() - 1)];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / samples.size());
    s.minv = samples.front();
    s.maxv = samples.back();
    return s;
}

struct FrameSample {
    double allocLatencyUs = 0.0;
    size_t liveAllocations = 0;
    size_t totalAllocationsSoFar = 0;
    size_t freeCount = 0;
    size_t failedCount = 0;
    VkDeviceSize liveVramBytes = 0;
    VkDeviceSize heapBudgetBytes = 0;
    VkDeviceSize heapUsageBytes = 0;
};

// Use steady_clock (monotonic). Per methodology.md §4, the prototype does NOT
// pin cores (single-threaded harness; one core is fine). Governor is whatever
// the host has (typically `powersave` on dev host per hardware-profile.md §1).
class Stopwatch {
public:
    using clock = std::chrono::steady_clock;
    void start() { t0_ = clock::now(); }
    double stopUs() {
        auto t1 = clock::now();
        return std::chrono::duration<double, std::micro>(t1 - t0_).count();
    }
private:
    clock::time_point t0_{};
};

// CSV-friendly summary of all stats. TracyPlot-friendly: columns named for
// easy grep / parsing.
inline void printStatsRow(const char* tag, const Stats& s) {
    printf("  %-22s mean=%9.3f us  median=%9.3f  p95=%9.3f  p99=%9.3f  "
           "std=%8.3f  min=%9.3f  max=%9.3f\n",
           tag, s.mean, s.median, s.p95, s.p99, s.stddev, s.minv, s.maxv);
}

}  // namespace prototype

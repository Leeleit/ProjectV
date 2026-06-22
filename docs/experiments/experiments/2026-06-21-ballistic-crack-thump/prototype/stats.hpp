#pragma once
// Statistics helper for ballistic-crack-thump benchmark.
// Tier-1 per benchmarks/methodology.md §3 (mean, median, p95, p99, std, min, max).

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace bench {

struct Stats {
    double mean{};
    double median{};
    double p95{};
    double p99{};
    double stddev{};
    double minv{};
    double maxv{};
};

inline Stats Compute(const std::vector<double>& samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.0;
    for (double v : samples) sum += v;
    s.mean = sum / static_cast<double>(samples.size());
    s.median = sorted[sorted.size() / 2];
    s.p95 = sorted[static_cast<std::size_t>(sorted.size() * 0.95)];
    s.p99 = sorted[static_cast<std::size_t>(sorted.size() * 0.99)];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
    s.minv = sorted.front();
    s.maxv = sorted.back();
    return s;
}

}  // namespace bench

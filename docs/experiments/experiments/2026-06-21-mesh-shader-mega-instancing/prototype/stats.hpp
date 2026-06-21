// SPDX-License-Identifier: MIT
// Stats helpers per docs/experiments/benchmarks/methodology.md §3, §7
// Standalone, no ProjectV dependencies.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sim {

struct Stats {
    double mean;
    double median;
    double p95;
    double p99;
    double stddev;
    double min;
    double max;
};

inline Stats compute_stats(std::vector<double> samples) {
    Stats s{};
    if (samples.empty()) {
        return s;
    }
    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.0;
    for (double v : samples) {
        sum += v;
    }
    s.mean = sum / static_cast<double>(samples.size());
    s.median = sorted[sorted.size() / 2];
    auto pct = [&](double p) {
        return sorted[std::min(static_cast<std::size_t>(static_cast<double>(sorted.size()) * p),
                               sorted.size() - 1)];
    };
    s.p95 = pct(0.95);
    s.p99 = pct(0.99);
    double var = 0.0;
    for (double v : samples) {
        const double d = v - s.mean;
        var += d * d;
    }
    s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
    s.min = sorted.front();
    s.max = sorted.back();
    return s;
}

}  // namespace sim

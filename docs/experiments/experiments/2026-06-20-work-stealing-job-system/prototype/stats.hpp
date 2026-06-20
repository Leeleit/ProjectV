// stats.hpp - Statistics for benchmark samples (mean / median / p95 / p99 / std / min / max).
//
// Templated for any numeric type. Sorts a copy to compute percentiles (correct even
// for non-uniform distributions, unlike "highest N samples" approach). Used for all
// per-config benchmark reporting.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace stats {

struct Result {
	double mean;
	double median;
	double p95;
	double p99;
	double stddev;
	double min;
	double max;
};

template <typename T>
inline Result Compute(const std::vector<T> &samples)
{
	Result r{};
	if (samples.empty())
		return r;

	std::vector<T> sorted = samples;
	std::sort(sorted.begin(), sorted.end());

	double sum = 0.0;
	for (T v : samples)
		sum += static_cast<double>(v);
	r.mean = sum / static_cast<double>(samples.size());

	r.median = static_cast<double>(sorted[sorted.size() / 2]);
	r.p95 = static_cast<double>(sorted[static_cast<std::size_t>(sorted.size() * 0.95)]);
	r.p99 = static_cast<double>(sorted[static_cast<std::size_t>(sorted.size() * 0.99)]);
	r.min = static_cast<double>(sorted.front());
	r.max = static_cast<double>(sorted.back());

	double var = 0.0;
	for (T v : samples) {
		double d = static_cast<double>(v) - r.mean;
		var += d * d;
	}
	r.stddev = std::sqrt(var / static_cast<double>(samples.size()));

	return r;
}

} // namespace stats

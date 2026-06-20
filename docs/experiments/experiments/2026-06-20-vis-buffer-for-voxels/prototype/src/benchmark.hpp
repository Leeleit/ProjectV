#pragma once
#include "pipeline_baseline.hpp"
#include <string>
#include <vector>

namespace vb {

struct Stats {
	double mean = 0;
	double median = 0;
	double p95 = 0;
	double p99 = 0;
	double stddev = 0;
	double min = 0;
	double max = 0;
};

Stats ComputeStats(std::vector<double> &v);

struct BenchmarkConfig {
	SceneConfig scene;
	VkExtent2D extent{1280, 720};
	uint32_t warmupFrames = 30;
	uint32_t measuredFrames = 200;
};

struct BenchmarkResult {
	uint32_t faceCount = 0;
	uint32_t materialCount = 0;
	Stats baselineStats;
	Stats visStats;
	uint32_t hashBaseline = 0;
	uint32_t hashVis = 0;
};

void RunBenchmark(BenchmarkConfig &cfg, BenchmarkResult &resultOut);
void WriteCSV(const std::string &path, const BenchmarkResult &r);

} // namespace vb

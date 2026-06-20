// Entry point: parse args, run benchmark, print summary, write CSV.

#include "benchmark.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char **argv)
{
	vb::BenchmarkConfig cfg;
	std::string csvPath = "results/benchmark.csv";

	for (int i = 1; i < argc; ++i) {
		if (std::strcmp(argv[i], "--chunks") == 0 && i + 1 < argc)
			cfg.scene.chunksPerSide = uint32_t(std::atoi(argv[++i]));
		else if (std::strcmp(argv[i], "--mats") == 0 && i + 1 < argc)
			cfg.scene.materialCount = uint32_t(std::atoi(argv[++i]));
		else if (std::strcmp(argv[i], "--dim") == 0 && i + 1 < argc)
			cfg.scene.chunkDim = uint32_t(std::atoi(argv[++i]));
		else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
			cfg.measuredFrames = uint32_t(std::atoi(argv[++i]));
		else if (std::strcmp(argv[i], "--warmup") == 0 && i + 1 < argc)
			cfg.warmupFrames = uint32_t(std::atoi(argv[++i]));
		else if (std::strcmp(argv[i], "--resolution") == 0 && i + 1 < argc) {
			int w = std::atoi(argv[i + 1]);
			int slash = -1;
			for (int j = 0; argv[i + 1][j]; ++j)
				if (argv[i + 1][j] == 'x')
					slash = j;
			if (slash > 0) {
				int h = std::atoi(argv[i + 1] + slash + 1);
				cfg.extent = {uint32_t(w), uint32_t(h)};
				++i;
			}
		} else if (std::strcmp(argv[i], "--csv") == 0 && i + 1 < argc)
			csvPath = argv[++i];
		else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
			std::fprintf(stderr,
						 "Usage: %s [--chunks N] [--mats N] [--dim N] [--frames N] [--warmup N] [--resolution WxH] [--csv path]\n",
						 argv[0]);
			return 0;
		}
	}

	std::fprintf(stderr, "Config: chunks=%u (dim=%u) → %u³ chunks\n",
				 cfg.scene.chunksPerSide, cfg.scene.chunkDim, cfg.scene.chunksPerSide);
	std::fprintf(stderr, "        materials=%u, resolution=%ux%u, frames=%u (warmup=%u)\n",
				 cfg.scene.materialCount, cfg.extent.width, cfg.extent.height,
				 cfg.measuredFrames, cfg.warmupFrames);

	vb::BenchmarkResult r;
	auto t0 = std::chrono::steady_clock::now();
	vb::RunBenchmark(cfg, r);
	auto t1 = std::chrono::steady_clock::now();

	double totalSec = std::chrono::duration<double>(t1 - t0).count();
	std::fprintf(stderr, "Wall total: %.1f s\n", totalSec);

	std::fprintf(stderr, "\nFaces generated (after greedy): %u\n", r.faceCount);
	std::fprintf(stderr, "Materials: %u\n\n", r.materialCount);

	auto printStats = [](const char *name, const vb::Stats &s, uint32_t hash) {
		std::fprintf(stderr, "%-12s  mean=%6.3f ms  med=%6.3f  p95=%6.3f  p99=%6.3f  std=%6.3f  min=%6.3f  max=%6.3f  hash=0x%08X\n",
					 name, s.mean, s.median, s.p95, s.p99, s.stddev, s.min, s.max, hash);
	};
	printStats("baseline", r.baselineStats, r.hashBaseline);
	printStats("visbuffer", r.visStats, r.hashVis);

	if (r.baselineStats.mean > 0) {
		double speedup = r.baselineStats.mean / r.visStats.mean;
		std::fprintf(stderr, "\nvisbuffer / baseline = %.3f (visbuffer is %.1f%% the time of baseline)\n",
					 speedup, speedup * 100.0);
	}

	vb::WriteCSV(csvPath, r);
	std::fprintf(stderr, "CSV: %s\n", csvPath.c_str());
	return 0;
}

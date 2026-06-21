// bench.cpp — latency benchmark for audio ray tracer
//
// Per experiments/2026-06-21-audio-raytracing-voxel-sdf/README.md §3 + benchmarks/methodology.md
// 3 scenes × 4 configs × 3 seeds × 1000 iterations + 100 warmup frames.
// CSV output: results.csv (machine-readable) + stdout summary.

#include "audio_raytracer.hpp"
#include "reverb.hpp"
#include "voxel_grid.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <string>
#include <vector>

using namespace audio_rt;

namespace {

struct Config {
	std::string name;
	std::string scene;
	int sources;
	int rays_per_source;
	int reflections;
	bool use_cache;
};

struct Stats {
	double mean = 0;
	double median = 0;
	double p95 = 0;
	double p99 = 0;
	double stddev = 0;
	double min_v = 0;
	double max_v = 0;
};

Stats computeStats(std::vector<double> samples) {
	std::sort(samples.begin(), samples.end());
	double sum = 0;
	for (double s : samples)
		sum += s;
	Stats s{};
	s.mean = sum / samples.size();
	s.median = samples[samples.size() / 2];
	s.p95 = samples[std::min<size_t>(samples.size() - 1, samples.size() * 95 / 100)];
	s.p99 = samples[std::min<size_t>(samples.size() - 1, samples.size() * 99 / 100)];
	double var = 0;
	for (double v : samples)
		var += (v - s.mean) * (v - s.mean);
	s.stddev = std::sqrt(var / samples.size());
	s.min_v = samples.front();
	s.max_v = samples.back();
	return s;
}

VoxelGrid makeScene(const std::string &name, uint64_t seed) {
	if (name == "cave")
		return VoxelGrid::makeCave(16, 16, 16, seed);
	if (name == "open_plains")
		return VoxelGrid::makeOpenPlains(16, 8, 16, seed);
	if (name == "multi_room")
		return VoxelGrid::makeMultiRoom(16, 16, 16, seed);
	std::fprintf(stderr, "Unknown scene %s\n", name.c_str());
	std::exit(1);
}

} // namespace

int main(int /*argc*/, char ** /*argv*/) {
	const std::vector<std::string> scenes = {"cave", "open_plains", "multi_room"};
	const std::vector<uint64_t> seeds = {1, 7, 42};
	const std::vector<Config> configs = {
		{"A_no_geom", "", 64, 0, 0, false},     // scene filled per outer loop
		{"B_occlusion", "", 64, 1, 0, false},   // 1 ray per source, no reflections
		{"C_full_hybrid", "", 64, 32, 4, false}, // 32 rays × 4 reflection orders
		{"D_full_cached", "", 64, 32, 4, true},  // + temporal cache
	};

	constexpr int kIterations = 1000;
	constexpr int kWarmup = 100;
	constexpr float kAudioFrameBudgetMs = 33.3f; // 30 Hz

	std::ofstream csv("results.csv");
	csv << "config,scene,seed,iterations,mean_ms,median_ms,p95_ms,p99_ms,stddev_ms,min_ms,max_ms,"
		<< "rays_traced,voxels_traversed,cache_hits,budget_utilization_pct\n";

	std::printf("%-15s %-12s %-5s | %-8s %-8s %-8s %-8s | %-12s %-12s %-10s\n", "config", "scene", "seed", "mean",
				"median", "p95", "p99", "rays_total", "voxels_total", "budget%");
	std::printf("-----------------------------------------------------------------------------------------------\n");

	int total_runs = static_cast<int>(configs.size() * scenes.size() * seeds.size());
	int run_idx = 0;

	for (const auto &cfg : configs) {
		for (const auto &scene : scenes) {
			for (uint64_t seed : seeds) {
				run_idx += 1;
				VoxelGrid grid = makeScene(scene, seed);
				AudioRaytracer rt(grid);
				RoomEstimate room = estimateRoom(grid);

				// Define sources distributed in scene (cube grid pattern).
				std::vector<AudioSource> sources;
				int sxs = static_cast<int>(std::ceil(std::cbrt(static_cast<double>(cfg.sources))));
				float sx_step = static_cast<float>(grid.worldSizeX()) / (sxs + 1);
				float sy_step = static_cast<float>(grid.worldSizeY()) * 0.5f / (sxs + 1);
				float sz_step = static_cast<float>(grid.worldSizeZ()) / (sxs + 1);
				for (int i = 0; i < cfg.sources; ++i) {
					int ix = i % sxs;
					int iy = (i / sxs) % sxs;
					int iz = i / (sxs * sxs);
					AudioSource s;
					s.x = sx_step * (ix + 1);
					s.y = sy_step * (iy + 1) + 4.0f;
					s.z = sz_step * (iz + 1);
					s.power = 0.8f;
					sources.push_back(s);
				}
				AudioListener lst;
				lst.x = static_cast<float>(grid.worldSizeX()) * 0.5f;
				lst.y = 4.0f;
				lst.z = static_cast<float>(grid.worldSizeZ()) * 0.5f;

				// Reset counters
				uint64_t base_rays = 0;
				uint64_t base_voxels = 0;
				uint64_t base_cache = 0;

				// Warmup
				for (int i = 0; i < kWarmup; ++i) {
					rt.notifySceneChange();
					for (const auto &s : sources) {
						if (cfg.name == "A_no_geom") {
							rt.traceNoGeometry(s, lst);
						} else if (cfg.name == "B_occlusion") {
							rt.traceOcclusionOnly(s, lst);
						} else if (cfg.use_cache) {
							rt.traceReflectionsCached(s, lst, cfg.reflections, cfg.rays_per_source);
						} else {
							rt.traceReflections(s, lst, cfg.reflections, cfg.rays_per_source);
						}
					}
				}

				base_rays = rt.raysTraced();
				base_voxels = rt.voxelsTraversed();
				base_cache = rt.cacheHits();

				// Measurement loop
				std::vector<double> samples;
				samples.reserve(kIterations);
				std::mt19937_64 rng(seed * 1000 + 13);
				std::uniform_real_distribution<float> jitter(-0.05f, 0.05f); // ±5 cm source jitter

				for (int i = 0; i < kIterations; ++i) {
					// For non-cached configs: defeat cache.
					if (!cfg.use_cache)
						rt.notifySceneChange();

					auto t0 = std::chrono::steady_clock::now();
					for (const auto &s : sources) {
						AudioSource ss = s;
						if (cfg.use_cache) {
							// slight jitter — cache hit only if total movement < epsilon (1 cm)
							ss.x += jitter(rng);
							ss.y += jitter(rng);
							ss.z += jitter(rng);
						}
						std::vector<ReflectionPath> paths;
						if (cfg.name == "A_no_geom") {
							rt.traceNoGeometry(ss, lst);
						} else if (cfg.name == "B_occlusion") {
							rt.traceOcclusionOnly(ss, lst);
						} else if (cfg.use_cache) {
							paths = rt.traceReflectionsCached(ss, lst, cfg.reflections, cfg.rays_per_source);
						} else {
							paths = rt.traceReflections(ss, lst, cfg.reflections, cfg.rays_per_source);
						}
						// Generate IR for full configs (realistic workload)
						if (cfg.name == "C_full_hybrid" || cfg.name == "D_full_cached") {
							auto ir = rt.generateIR(paths, 1, 1.5f);
							applyEyringTail(ir, room, 0.08f);
						}
					}
					auto t1 = std::chrono::steady_clock::now();
					double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
					samples.push_back(ms);
				}

				uint64_t rays_total = rt.raysTraced() - base_rays;
				uint64_t voxels_total = rt.voxelsTraversed() - base_voxels;
				uint64_t cache_total = rt.cacheHits() - base_cache;

				Stats s = computeStats(samples);
				double budget_pct = (s.mean / kAudioFrameBudgetMs) * 100.0;

				std::printf("%-15s %-12s %-5llu | %-8.3f %-8.3f %-8.3f %-8.3f | %-12llu %-12llu %-10.2f  [%d/%d]\n",
							cfg.name.c_str(), scene.c_str(), static_cast<unsigned long long>(seed), s.mean, s.median,
							s.p95, s.p99, static_cast<unsigned long long>(rays_total),
							static_cast<unsigned long long>(voxels_total), budget_pct, run_idx, total_runs);

				csv << cfg.name << "," << scene << "," << seed << "," << kIterations << "," << s.mean << "," << s.median
					<< "," << s.p95 << "," << s.p99 << "," << s.stddev << "," << s.min_v << "," << s.max_v << ","
					<< rays_total << "," << voxels_total << "," << cache_total << "," << budget_pct << "\n";
			}
		}
	}

	csv.close();
	std::printf("\nWrote results.csv (%d configs × %zu scenes × %zu seeds = %d runs × %d iter = %d measurements)\n",
				static_cast<int>(configs.size()), scenes.size(), seeds.size(), total_runs, kIterations,
				total_runs * kIterations);
	return 0;
}

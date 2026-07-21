// 2026-06-21-vk-multi-gpu-split-frame — CPU simulation (Phase 3)
// Standalone C++26 — single file, no deps, ad-hoc `clang++` research computation.
// NOT ProjectV mainline. Per `AGENTS.md §1`: agent not building.
//
// Simulates: 4 present modes × 5 synthetic GPU work sizes × 100 iter + 10 warmup = 2000 measurements.
// Synthetic GPU work = small compute-bound loop representing voxel DDA ray-march (CPU proxy for
// GPU dispatch latency on dev host Zen 3 5800X).
// Plus present sync overhead + peer memory copy time (configurable per tier).

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

namespace mgpu_sim {

// ---- Stats compute (per `benchmarks/methodology.md §7`) ----

struct Stats {
	double mean;
	double median;
	double p95;
	double p99;
	double stddev;
	double min;
	double max;
};

Stats compute_stats(std::vector<double> samples)
{
	Stats s{};
	if (samples.empty())
		return s;
	std::sort(samples.begin(), samples.end());
	const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
	s.mean = sum / samples.size();
	s.median = samples[samples.size() / 2];
	s.p95 = samples[static_cast<size_t>(samples.size() * 0.95)];
	s.p99 = samples[static_cast<size_t>(samples.size() * 0.99)];
	double var = 0.0;
	for (double v : samples)
		var += (v - s.mean) * (v - s.mean);
	s.stddev = std::sqrt(var / samples.size());
	s.min = samples.front();
	s.max = samples.back();
	return s;
}

// ---- Synthetic GPU work function ----
// Each "GPU unit of work" = small CPU loop simulating GPU dispatch (DDA ray-march).
// Tuned to give meaningful "frame time" on Zen 3 5800X dev host.
// Uses volatile sink to prevent dead-code elimination; inner loop has sqrt + transcendentals
// (proxy for GPU ALU pipeline latency on real ray-march).

constexpr int kRayMarchIters = 256;			// voxels per ray-march (lighter for ~30s total runtime)
static volatile double g_sink_global = 0.0; // prevent entire loop elimination

double synthetic_gpu_work_us(int ray_march_count)
{
	// Each ray-march = kRayMarchIters iterations of arithmetic (proxy for DDA voxel traversal).
	auto t0 = std::chrono::high_resolution_clock::now();
	double local_sink = 0.0;
	for (int r = 0; r < ray_march_count; ++r) {
		for (int i = 0; i < kRayMarchIters; ++i) {
			// Branchy DDA step with transcendentals (proxy for voxel grid traversal + SDF)
			const int cell = (i * 7919) & 0xFF;
			const double v = static_cast<double>(cell) / 255.0;
			if (cell < 128)
				local_sink += std::sqrt(v + 1.0);
			else if (cell < 192)
				local_sink += std::sin(v * 3.14159);
			else
				local_sink += std::cos(v * 6.28318);
			local_sink *= 1.0000001; // force non-trivial dependency chain
		}
	}
	g_sink_global = local_sink; // force write to volatile
	auto t1 = std::chrono::high_resolution_clock::now();
	return std::chrono::duration<double, std::micro>(t1 - t0).count();
}

// ---- Interconnect simulation parameters ----

struct Interconnect {
	std::string_view name;
	double peer_bw_gb_s;
	double present_overhead_us_per_frame;
	double peer_copy_per_mib_us; // copy time per MiB at this tier
};

// Approximate peer copy time = (MiB / GB/s) * 1e6 / 1e3 = (MiB / GB/s) * 1e3 us/MiB
// e.g., NVLink 4.0 900 GB/s, 4 MiB → 4 / 900 * 1000 = 4.4 us
constexpr std::array<Interconnect, 5> kInterconnects = {{
	{"NVIDIA NVLink 4.0 (Hopper H100, 900 GB/s)", 900.0, 30.0, 1.11}, // 1 MiB / 900 GB/s ≈ 1.11 us
	{"NVIDIA NVLink 4.1 (Blackwell B200, 1800 GB/s)", 1800.0, 25.0, 0.56},
	{"AMD xGMI 2.0 (RDNA 3, 400 GB/s)", 400.0, 60.0, 2.5},
	{"Intel PCIe 4.0 x16 (Arc Battlemage, 32 GB/s)", 32.0, 80.0, 31.25},
	{"NVIDIA PCIe 5.0 x16 (consumer Blackwell, 64 GB/s)", 64.0, 40.0, 15.625},
}};

constexpr int kWarmup = 5;
constexpr int kIters = 30;

// Synthetic GPU work sizes (ray-march counts); each represents ~1-30 ms of "real" GPU work
// Tuned for ~30s total runtime: 5 interconnects × 4 modes × 3 gpu_counts × 5 work_sizes × (5 warmup + 30 iter) = 21000 measurements
constexpr std::array<int, 5> kWorkSizes = {64, 256, 1024, 4096, 16384};

// ---- Simulated frame timing ----

enum class PresentMode { Local,
						 LocalMultiDevice,
						 Sum,
						 Remote };

constexpr std::array<std::string_view, 4> kPresentModeNames = {
	"LOCAL", "LOCAL_MULTI_DEVICE (AFR)", "SUM (SFR)", "REMOTE"};

// Simulate one frame of work for given mode + tier + work size.
// Returns: average frame interval in microseconds.
double simulate_frame_interval_us(PresentMode mode, const Interconnect &ic, int gpu_count, int work_size, int /*iter*/)
{
	const double single_gpu_work_us = synthetic_gpu_work_us(work_size);

	switch (mode) {
	case PresentMode::Local: {
		if (gpu_count == 1)
			return single_gpu_work_us;
		// Multi-GPU LOCAL: 1 GPU renders, others idle (wasted) — total time = single
		return single_gpu_work_us;
	}
	case PresentMode::LocalMultiDevice: {
		// AFR: each GPU renders 1 frame in (work + 5% sync) us; interval = per_gpu / gpu_count
		if (gpu_count < 1)
			return single_gpu_work_us;
		const double per_gpu_work = single_gpu_work_us * (1.0 + 0.05 * (gpu_count - 1));
		const double avg_frame_interval = per_gpu_work / static_cast<double>(gpu_count);
		// Present sync per submitted frame (binary semaphore wait + scanout)
		const double present_sync = ic.present_overhead_us_per_frame * (1.0 + 0.2 * (gpu_count - 1));
		// Peer memory copy for shared resources (4 MiB per frame for AFR, doubled per extra GPU)
		const double peer_copy = 4.0 * ic.peer_copy_per_mib_us / static_cast<double>(gpu_count);
		return avg_frame_interval + present_sync + peer_copy;
	}
	case PresentMode::Sum: {
		// SFR: each GPU renders ~65% of frame (spatial split) + 1.5 ms compositing
		if (gpu_count < 2)
			return single_gpu_work_us;
		const double per_gpu_work = single_gpu_work_us * 0.65;
		const double compositing_us = 1500.0; // 1.5 ms
		const double peer_copy = 4.0 * ic.peer_copy_per_mib_us;
		return per_gpu_work + compositing_us + peer_copy;
	}
	case PresentMode::Remote: {
		// REMOTE: 40% compute on GPU 1, 60% render on GPU 0 — parallel, take max
		if (gpu_count < 2)
			return single_gpu_work_us;
		const double compute_us = single_gpu_work_us * 0.4;
		const double render_us = single_gpu_work_us * 0.6;
		const double peer_copy = 8.0 * ic.peer_copy_per_mib_us; // 2× copy (in + out)
		return std::max(compute_us, render_us) + peer_copy;
	}
	}
	return single_gpu_work_us;
}

} // namespace mgpu_sim

int main(int argc, char **argv)
{
	std::string output_path = "sim_results.csv";
	int warmup = mgpu_sim::kWarmup;
	int iters = mgpu_sim::kIters;
	if (argc > 1) {
		for (int i = 1; i < argc; i += 2) {
			std::string arg = argv[i];
			if (arg == "--output" && i + 1 < argc)
				output_path = argv[i + 1];
			else if (arg == "--warmup" && i + 1 < argc)
				warmup = std::atoi(argv[i + 1]);
			else if (arg == "--iter" && i + 1 < argc)
				iters = std::atoi(argv[i + 1]);
		}
	}

	using namespace mgpu_sim;
	std::vector<std::array<std::string, 12>> rows;
	rows.push_back({"interconnect", "present_mode", "gpu_count", "work_size_rays", "warmup", "iter",
					"mean_us", "median_us", "p95_us", "p99_us", "stddev_us", "scaling_pct_vs_single"});

	constexpr std::array<int, 3> kGpuCounts = {1, 2, 4};

	int total_measurements = 0;
	// Pre-compute baseline (LOCAL single-GPU) per work_size as mean of iters samples (stable)
	std::map<int, double> baseline_mean_us;
	for (int work_size : kWorkSizes) {
		std::vector<double> b_samples;
		b_samples.reserve(iters);
		for (int w = 0; w < warmup; ++w) {
			synthetic_gpu_work_us(work_size);
		}
		for (int it = 0; it < iters; ++it) {
			b_samples.push_back(synthetic_gpu_work_us(work_size));
		}
		baseline_mean_us[work_size] = compute_stats(std::move(b_samples)).mean;
	}
	std::printf("Baseline (LOCAL single-GPU, mean of %d iters):\n", iters);
	for (int work_size : kWorkSizes) {
		std::printf("  work_size=%5d rays: %.2f us\n", work_size, baseline_mean_us[work_size]);
	}

	for (const auto &ic : kInterconnects) {
		for (size_t mi = 0; mi < kPresentModeNames.size(); ++mi) {
			PresentMode mode = static_cast<PresentMode>(mi);
			for (int gpu_count : kGpuCounts) {
				for (int work_size : kWorkSizes) {
					// Run warmup
					for (int w = 0; w < warmup; ++w) {
						simulate_frame_interval_us(mode, ic, gpu_count, work_size, w);
					}
					// Run iters
					std::vector<double> samples;
					samples.reserve(iters);
					for (int it = 0; it < iters; ++it) {
						samples.push_back(simulate_frame_interval_us(mode, ic, gpu_count, work_size, it));
					}
					// Compute baseline (single-GPU LOCAL same work_size) using pre-computed mean
					const double baseline_us = baseline_mean_us[work_size];
					Stats s = compute_stats(std::move(samples));
					const double scaling = (baseline_us / s.mean) * 100.0;
					rows.push_back({
						std::string(ic.name),
						std::string(kPresentModeNames[mi]),
						std::to_string(gpu_count),
						std::to_string(work_size),
						std::to_string(warmup),
						std::to_string(iters),
						std::to_string(s.mean),
						std::to_string(s.median),
						std::to_string(s.p95),
						std::to_string(s.p99),
						std::to_string(s.stddev),
						std::to_string(scaling),
					});
					total_measurements += iters;
				}
			}
		}
	}

	std::ofstream f(output_path);
	if (!f) {
		std::fprintf(stderr, "ERROR: cannot open %s\n", output_path.c_str());
		return 1;
	}
	for (const auto &row : rows) {
		for (size_t i = 0; i < row.size(); ++i) {
			if (i > 0)
				f << ',';
			if (row[i].find(',') != std::string::npos)
				f << '"' << row[i] << '"';
			else
				f << row[i];
		}
		f << '\n';
	}
	f.close();

	std::printf("OK: wrote %zu config-rows (%d total measurements) to %s\n",
				rows.size() - 1, total_measurements, output_path.c_str());
	std::printf("Sample headline (NVLink 4.0, work_size=65536 rays, 2 GPU):\n");
	for (size_t mi = 0; mi < kPresentModeNames.size(); ++mi) {
		PresentMode mode = static_cast<PresentMode>(mi);
		double baseline = simulate_frame_interval_us(PresentMode::Local, kInterconnects[0], 1, 65536, 0);
		double v = simulate_frame_interval_us(mode, kInterconnects[0], 2, 65536, 0);
		std::printf("  %-25s: %.1f us vs baseline %.1f us (%.0f%%)\n",
					std::string(kPresentModeNames[mi]).c_str(), v, baseline, baseline / v * 100.0);
	}
	return 0;
}

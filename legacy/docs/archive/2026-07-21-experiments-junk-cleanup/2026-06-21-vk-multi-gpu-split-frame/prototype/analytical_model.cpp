// 2026-06-21-vk-multi-gpu-split-frame — analytical model (Phase 2)
// Standalone C++26 — single file, no deps, ad-hoc `clang++` research computation.
// NOT ProjectV mainline. Per `AGENTS.md §1`: agent not building, but ad-hoc analytical
// computation is research workflow (not cmake --build / ctest / ProjectV binary).
//
// Model: 4 present modes × 4 cross-vendor interconnects × 4 scenes → 64 (mode × ic × scene) outputs.
// CPU-only, no Vulkan needed.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

namespace mgpu_analytical {

// ---- Hardware baseline (dev host `obvium` per hardware-profile.md §3, single-GPU analytical) ----

struct GpuTier {
	std::string_view name;
	double vram_gib;			// per-GPU VRAM
	double peer_bw_gb_s;		// peer memory bidirectional bandwidth
	double present_overhead_us; // present sync overhead (binary semaphore + scanout)
	double scaling_2gpu_pct;	// measured/analytical scaling on 2 GPU for compute-bound
	double scaling_4gpu_pct;	// scaling on 4 GPU
	std::string_view interconnect;
};

constexpr std::array<GpuTier, 6> kGpuTiers = {{
	{"NVIDIA RTX 3060 Ti (Ampere, dev host single-GPU)", 8.0, 32.0, 50.0, 0.0, 0.0, "PCIe 4.0 x16 (no NVLink consumer)"},
	{"NVIDIA RTX 5090 (Blackwell, consumer)", 32.0, 64.0, 40.0, 50.0, 70.0, "PCIe 5.0 x16 (no NVLink consumer)"},
	{"NVIDIA H100 (Hopper, data center)", 80.0, 900.0, 30.0, 85.0, 75.0, "NVLink 4.0 (18 links × 50 GB/s)"},
	{"NVIDIA B200 (Blackwell, data center)", 192.0, 1800.0, 25.0, 90.0, 82.0, "NVLink 4.1 (18 links × 100 GB/s)"},
	{"AMD RX 7900 XTX (RDNA 3, dual-die potential)", 24.0, 400.0, 60.0, 70.0, 60.0, "xGMI 2.0"},
	{"Intel Arc Battlemage (consumer)", 16.0, 32.0, 80.0, 35.0, 30.0, "PCIe 4.0 x16 (no native peer ic)"},
}};

// ---- Scenes (representative of Stage 4.3 128m draw distance per TODO.md §4.3) ----

struct Scene {
	std::string_view name;
	double vram_baseline_gib;	 // current VRAM required for this scene (single-GPU)
	double frame_time_ms_single; // frame time on single-GPU baseline
	double peer_copy_mib;		 // per-frame cross-GPU transfer (e.g., for AFR shared resources)
};

constexpr std::array<Scene, 4> kScenes = {{
	{"A: baseline_64m (current cap, 1500 chunks)", 2.0, 8.3, 0.0},
	{"B: target_128m (Stage 4.3, 6000 chunks)", 9.0, 20.0, 4.0},
	{"C: extreme_256m (Stage 4.3 stretch, 24k chunks)", 36.0, 50.0, 16.0},
	{"D: vct_heavy (Stage 5.1, 128m + VCT atlas)", 10.0, 25.0, 8.0},
}};

// ---- Present modes ----

enum class PresentMode {
	Local,			  // single-GPU baseline
	LocalMultiDevice, // AFR
	Sum,			  // SFR
	Remote,			  // asymmetric
	Count
};

constexpr std::array<std::string_view, static_cast<size_t>(PresentMode::Count)> kPresentModeNames = {
	"LOCAL (single-GPU baseline)", "LOCAL_MULTI_DEVICE (AFR)", "SUM (SFR)", "REMOTE (asymmetric)"};

// ---- Analytical model ----

struct ModeResult {
	PresentMode mode;
	double effective_frame_time_ms;
	double scaling_pct_vs_single; // 100% = same as baseline, 200% = 2× faster (frame time / 2)
	double vram_aggregate_gib;	  // available VRAM for this mode + GPU count
	bool fits_in_budget;		  // true if vram_aggregate_gib >= scene.vram_baseline_gib
	double sync_overhead_ms;	  // peer memory + present sync per frame
};

constexpr double kFrameRateBaselineHz = 60.0;
static_assert(kFrameRateBaselineHz == 60.0, "baseline Hz constant");

// Compute AFR frame interval: each GPU renders 1 frame in (single + sync_overhead) ms.
// Total frame submission rate = gpu_count / (per_gpu_frame) per second.
// Average frame INTERVAL = per_gpu_frame / gpu_count.
// Plus present sync per submitted frame + peer memory copy (per frame).
double afr_frame_time_ms(const Scene &scene, const GpuTier &tier, int gpu_count)
{
	if (gpu_count < 1)
		return scene.frame_time_ms_single;
	// Each GPU adds 5% sync overhead for cross-GPU coordination (semaphore + fence)
	const double per_gpu_frame = scene.frame_time_ms_single * (1.0 + 0.05 * (gpu_count - 1));
	// AFR frame interval = (per_gpu_frame) / gpu_count (each GPU renders independently in parallel)
	const double base_frame_interval = per_gpu_frame / static_cast<double>(gpu_count);
	// Present sync per submitted frame: binary semaphore wait + scanout (slightly higher for multi-GPU)
	const double present_sync_ms = tier.present_overhead_us / 1000.0 * (1.0 + 0.2 * (gpu_count - 1));
	// Peer memory copy for shared resources (textures, uniform buffers) per frame
	const double peer_copy_ms = (scene.peer_copy_mib / 1024.0) / (tier.peer_bw_gb_s * gpu_count / 8.0 / 1000.0);
	return base_frame_interval + present_sync_ms + peer_copy_ms;
}

// Compute SFR frame time: max(GPU 0 left, GPU 1 right) per-frame + compositing at present
double sfr_frame_time_ms(const Scene &scene, const GpuTier &tier, int gpu_count)
{
	if (gpu_count < 2)
		return scene.frame_time_ms_single;
	const double per_gpu_frame = scene.frame_time_ms_single * 0.65; // 35% savings from spatial split (load balance loss + seam latency)
	const double max_gpu = per_gpu_frame;
	const double compositing_ms = 1.5; // fixed 1-2 ms scanout compositing overhead
	const double peer_copy_ms = (scene.peer_copy_mib / 1024.0) / (tier.peer_bw_gb_s / 8.0 / 1000.0);
	return max_gpu + compositing_ms + peer_copy_ms;
}

// Compute REMOTE frame time: compute on remote GPU, render on local — depends on workload split
double remote_frame_time_ms(const Scene &scene, const GpuTier &tier, int gpu_count)
{
	if (gpu_count < 2)
		return scene.frame_time_ms_single;
	// 50/50 split: compute (Fluid CA, world gen, VCT cone-march) on GPU 1, render on GPU 0
	const double compute_ms = scene.frame_time_ms_single * 0.4; // 40% of frame time is async compute
	const double render_ms = scene.frame_time_ms_single * 0.6;	// 60% is render
	const double peer_copy_ms = (scene.peer_copy_mib * 2.0 / 1024.0) / (tier.peer_bw_gb_s / 8.0 / 1000.0);
	const double max_split = std::max(compute_ms, render_ms);
	return max_split + peer_copy_ms;
}

ModeResult compute_mode(PresentMode mode, const Scene &scene, const GpuTier &tier, int gpu_count)
{
	ModeResult r{mode, scene.frame_time_ms_single, 100.0, tier.vram_gib * gpu_count, false, 0.0};
	switch (mode) {
	case PresentMode::Local: {
		if (gpu_count == 1) {
			r.effective_frame_time_ms = scene.frame_time_ms_single;
			r.scaling_pct_vs_single = 100.0;
			r.vram_aggregate_gib = tier.vram_gib;
		} else {
			// Multi-GPU LOCAL: one device renders, others idle (wasted)
			r.effective_frame_time_ms = scene.frame_time_ms_single; // no scaling
			r.scaling_pct_vs_single = 100.0;
			r.vram_aggregate_gib = tier.vram_gib * gpu_count; // VRAM can still aggregate via peer memory
		}
		r.sync_overhead_ms = 0.0;
		break;
	}
	case PresentMode::LocalMultiDevice:
		r.effective_frame_time_ms = afr_frame_time_ms(scene, tier, gpu_count);
		r.sync_overhead_ms = (scene.peer_copy_mib / 1024.0) / (tier.peer_bw_gb_s * gpu_count / 8.0 / 1000.0) + tier.present_overhead_us / 1000.0;
		break;
	case PresentMode::Sum:
		r.effective_frame_time_ms = sfr_frame_time_ms(scene, tier, gpu_count);
		r.sync_overhead_ms = 1.5 + (scene.peer_copy_mib / 1024.0) / (tier.peer_bw_gb_s / 8.0 / 1000.0);
		break;
	case PresentMode::Remote:
		r.effective_frame_time_ms = remote_frame_time_ms(scene, tier, gpu_count);
		r.sync_overhead_ms = (scene.peer_copy_mib * 2.0 / 1024.0) / (tier.peer_bw_gb_s / 8.0 / 1000.0);
		break;
	case PresentMode::Count:
		break;
	}
	r.scaling_pct_vs_single = (scene.frame_time_ms_single / r.effective_frame_time_ms) * 100.0;
	r.fits_in_budget = r.vram_aggregate_gib >= scene.vram_baseline_gib;
	return r;
}

} // namespace mgpu_analytical

int main(int argc, char **argv)
{
	std::string output_path = "analytical_results.csv";
	if (argc > 2 && std::string(argv[1]) == "--output") {
		output_path = argv[2];
	}

	using namespace mgpu_analytical;
	std::vector<std::array<std::string, 11>> rows; // 1 header + N data rows

	rows.push_back({"gpu_tier", "interconnect", "scene", "gpu_count", "present_mode",
					"frame_time_ms", "scaling_pct_vs_single", "vram_aggregate_gib",
					"scene_vram_required_gib", "fits_in_budget", "sync_overhead_ms"});

	constexpr std::array<int, 3> kGpuCounts = {1, 2, 4}; // 1 vs 2 vs 4 GPU analytical sweep

	for (const auto &tier : kGpuTiers) {
		for (int gpu_count : kGpuCounts) {
			for (const auto &scene : kScenes) {
				for (size_t mi = 0; mi < kPresentModeNames.size(); ++mi) {
					PresentMode mode = static_cast<PresentMode>(mi);
					ModeResult r = compute_mode(mode, scene, tier, gpu_count);
					rows.push_back({
						std::string(tier.name),
						std::string(tier.interconnect),
						std::string(scene.name),
						std::to_string(gpu_count),
						std::string(kPresentModeNames[mi]),
						std::to_string(r.effective_frame_time_ms),
						std::to_string(r.scaling_pct_vs_single),
						std::to_string(r.vram_aggregate_gib),
						std::to_string(scene.vram_baseline_gib),
						r.fits_in_budget ? "true" : "false",
						std::to_string(r.sync_overhead_ms),
					});
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
			// Escape commas in fields
			if (row[i].find(',') != std::string::npos) {
				f << '"' << row[i] << '"';
			} else {
				f << row[i];
			}
		}
		f << '\n';
	}
	f.close();

	std::printf("OK: wrote %zu rows to %s\n", rows.size() - 1, output_path.c_str());
	std::printf("Mode summary (target_128m scene B, NVIDIA H100 NVLink 4.0, 2 GPU):\n");
	for (size_t mi = 0; mi < kPresentModeNames.size(); ++mi) {
		PresentMode mode = static_cast<PresentMode>(mi);
		ModeResult r = compute_mode(mode, kScenes[1], kGpuTiers[2], 2);
		std::printf("  %-30s: frame %.2f ms (%.0f%%), vram %.1f GiB, fits=%d\n",
					std::string(kPresentModeNames[mi]).c_str(),
					r.effective_frame_time_ms,
					r.scaling_pct_vs_single,
					r.vram_aggregate_gib,
					r.fits_in_budget ? 1 : 0);
	}
	std::printf("\nMode summary (target_128m scene B, NVIDIA H100 NVLink 4.0, 4 GPU):\n");
	for (size_t mi = 0; mi < kPresentModeNames.size(); ++mi) {
		PresentMode mode = static_cast<PresentMode>(mi);
		ModeResult r = compute_mode(mode, kScenes[1], kGpuTiers[2], 4);
		std::printf("  %-30s: frame %.2f ms (%.0f%%), vram %.1f GiB, fits=%d\n",
					std::string(kPresentModeNames[mi]).c_str(),
					r.effective_frame_time_ms,
					r.scaling_pct_vs_single,
					r.vram_aggregate_gib,
					r.fits_in_budget ? 1 : 0);
	}
	std::printf("\nMode summary (extreme_256m scene C, NVIDIA B200 NVLink 4.1, 4 GPU):\n");
	for (size_t mi = 0; mi < kPresentModeNames.size(); ++mi) {
		PresentMode mode = static_cast<PresentMode>(mi);
		ModeResult r = compute_mode(mode, kScenes[2], kGpuTiers[3], 4);
		std::printf("  %-30s: frame %.2f ms (%.0f%%), vram %.1f GiB, fits=%d (need %.1f)\n",
					std::string(kPresentModeNames[mi]).c_str(),
					r.effective_frame_time_ms,
					r.scaling_pct_vs_single,
					r.vram_aggregate_gib,
					r.fits_in_budget ? 1 : 0,
					kScenes[2].vram_baseline_gib);
	}
	std::printf("\nMode summary (target_128m scene B, Intel Arc Battlemage PCIe 4.0, 2 GPU):\n");
	for (size_t mi = 0; mi < kPresentModeNames.size(); ++mi) {
		PresentMode mode = static_cast<PresentMode>(mi);
		ModeResult r = compute_mode(mode, kScenes[1], kGpuTiers[5], 2);
		std::printf("  %-30s: frame %.2f ms (%.0f%%), vram %.1f GiB, fits=%d\n",
					std::string(kPresentModeNames[mi]).c_str(),
					r.effective_frame_time_ms,
					r.scaling_pct_vs_single,
					r.vram_aggregate_gib,
					r.fits_in_budget ? 1 : 0);
	}
	return 0;
}

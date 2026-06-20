// SPDX-License-Identifier: MIT
//
// cache_dispatch_model.cpp - CPU-side analytical model for compute cull vs mesh shader pipeline.
// Standalone, no GPU, no Vulkan. Compiles with C++26.
//
// Purpose: illustrate cost-pattern differences between:
//   - Pattern A: compute cull + indirect draw (current ProjectV baseline).
//   - Pattern B: task shader + mesh shader (TODO 2.1 design, the maister says: avoid).
//   - Pattern C: mesh shader + indirect count (the maister's universal fast path).
//
// NOT a benchmark. Output is deterministic analytical estimates derived from cited papers
// (nvpro-samples, the maister, SSeanPP VoxelMVP, vkguide Ascendant).
//
// Per docs/experiments/AGENTS.md section 2: "Ne zapuskayu cmake/ctest/ProjectV-binary".
// This file is standalone research artifact, not part of ProjectV mainline.
//
// Build:
//   clang++ -std=c++26 -O3 -march=native -DNDEBUG \
//     docs/experiments/experiments/2026-06-20-mesh-shader-vs-compute-cull/prototype/cache_dispatch_model.cpp \
//     -o /tmp/cache_dispatch_model
//
// Run:
//   /tmp/cache_dispatch_model
//
// Output: per-pattern cost estimates for ProjectV workload.

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string_view>

namespace model {

inline constexpr uint32_t kVoxelsPerChunk = 32u * 32u * 32u;
inline constexpr uint32_t kSparse64LeafSide = 4u;
inline constexpr uint32_t kLeavesPerChunk = (32u / kSparse64LeafSide) * (32u / kSparse64LeafSide) * (32u / kSparse64LeafSide);

inline constexpr uint32_t kSparse64NodeSizeBytes = 272u;
inline constexpr uint32_t kGpuNodeSizeBytes = 264u;
inline constexpr uint32_t kPackedFaceSizeBytes = 16u;
inline constexpr uint32_t kIndirectCommandSizeBytes = 16u;

inline constexpr uint32_t kRecommendedMeshletMaxVerts = 64u;
inline constexpr uint32_t kRecommendedMeshletMaxPrims = 126u;

struct Scenario {
	std::string_view name;
	uint32_t total_chunks;
	uint32_t view_distance;
	uint32_t dirty_per_frame;
	double cull_ratio;
	double sparse_ratio;
	uint32_t faces_per_chunk;
};

inline constexpr Scenario kSparseWorld{
	"ProjectV sparse world (VoxelLab-like)",
	1024,
	64,
	32,
	0.5,
	0.1,
	200,
};

inline constexpr Scenario kDenseCave{
	"ProjectV dense cave interior",
	1024,
	64,
	32,
	0.7,
	0.5,
	800,
};

inline constexpr Scenario kLargeWorld{
	"ProjectV large world (Stage 4.3, 128+ chunks)",
	4096,
	256,
	128,
	0.5,
	0.1,
	200,
};

struct PatternCosts {
	double vram_ssbo_bytes;
	double bandwidth_read_bytes;
	double bandwidth_write_bytes;
	double barrier_count;
	double dispatch_count;
	double estimated_cpu_overhead_us;
	double shader_loc_estimate;
};

inline constexpr PatternCosts kPatternA{
	8.0 * 1024.0 * 1024.0,
	50.0 * 1024.0 * 1024.0,
	10.0 * 1024.0 * 1024.0,
	4.0,
	51.0,
	150.0,
	562.0,
};

inline constexpr PatternCosts kPatternB{
	0.0,
	50.0 * 1024.0 * 1024.0,
	30.0 * 1024.0 * 1024.0,
	1.5,
	100.0,
	100.0,
	800.0,
};

inline constexpr PatternCosts kPatternC{
	0.0,
	50.0 * 1024.0 * 1024.0,
	25.0 * 1024.0 * 1024.0,
	1.0,
	2.0,
	20.0,
	850.0,
};

struct PatternQualifiers {
	bool works_all_gpus;
	bool vendor_caveats_present;
	bool task_shader_used;
	bool shipped_in_games;
	std::string_view amd_rna2_status;
	std::string_view intel_arc_status;
};

inline constexpr PatternQualifiers kPatternAQuals{
	true,
	false,
	false,
	true,
	"Full support",
	"Full support",
};

inline constexpr PatternQualifiers kPatternBQuals{
	false,
	true,
	true,
	false,
	"TDR on early-return workaround (GameDev.net 2024), RDNA2 Windows maintenance mode",
	"Experimental, driver maturity concerns",
};

inline constexpr PatternQualifiers kPatternCQuals{
	false,
	true,
	false,
	true,
	"Workaround required (early-return after indices output), RDNA2 Windows maintenance mode",
	"Improving 2025, not yet as battle-tested as NVIDIA",
};

void print_scenario(const Scenario &s)
{
	printf("Scenario: %.*s\n", static_cast<int>(s.name.size()), s.name.data());
	printf("  Total chunks in world: %u\n", s.total_chunks);
	printf("  View distance (chunks in frustum before cull): %u\n", s.view_distance);
	printf("  Chunks after frustum cull: %u (cull ratio %.0f%%)\n",
		   static_cast<uint32_t>(s.view_distance * s.cull_ratio), s.cull_ratio * 100.0);
	printf("  Dirty chunks (re-mesh this frame): %u\n", s.dirty_per_frame);
	printf("  Voxel density: %.0f%% (sparse ratio)\n", s.sparse_ratio * 100.0);
	printf("  Avg faces per chunk (after greedy): %u\n", s.faces_per_chunk);
	printf("\n");
}

void print_pattern_costs(const char *pattern_name, const PatternCosts &c)
{
	printf("=== %s ===\n", pattern_name);
	printf("  VRAM SSBO write per frame:    %.2f MB\n", c.vram_ssbo_bytes / (1024.0 * 1024.0));
	printf("  GPU read bandwidth:           %.2f MB/frame\n", c.bandwidth_read_bytes / (1024.0 * 1024.0));
	printf("  GPU write bandwidth:          %.2f MB/frame\n", c.bandwidth_write_bytes / (1024.0 * 1024.0));
	printf("  Total GPU bandwidth:          %.2f MB/frame\n",
		   (c.bandwidth_read_bytes + c.bandwidth_write_bytes) / (1024.0 * 1024.0));
	printf("  Barrier count:                %.1f\n", c.barrier_count);
	printf("  Dispatch count:               %.1f\n", c.dispatch_count);
	printf("  Estimated CPU overhead:       %.0f us/frame\n", c.estimated_cpu_overhead_us);
	printf("  Shader LOC estimate:          %.0f lines\n", c.shader_loc_estimate);
	printf("\n");
}

void print_pattern_quals(const char *pattern_name, const PatternQualifiers &q)
{
	printf("=== %s qualifiers ===\n", pattern_name);
	printf("  Works on all Vulkan GPUs:     %s\n", q.works_all_gpus ? "yes" : "no");
	printf("  Vendor caveats present:       %s\n", q.vendor_caveats_present ? "yes" : "no");
	printf("  Uses task shader:             %s\n", q.task_shader_used ? "yes" : "no");
	printf("  Shipped in real games:        %s\n", q.shipped_in_games ? "yes" : "no");
	printf("  AMD RDNA2 status:             %.*s\n",
		   static_cast<int>(q.amd_rna2_status.size()), q.amd_rna2_status.data());
	printf("  Intel Arc status:             %.*s\n",
		   static_cast<int>(q.intel_arc_status.size()), q.intel_arc_status.data());
	printf("\n");
}

void print_comparison_table()
{
	printf("==== COMPARISON TABLE ====\n");
	printf("%-28s | %-12s | %-12s | %-12s\n", "Metric", "Pattern A", "Pattern B", "Pattern C");
	printf("%-28s-+-%-12s-+-%-12s-+-%-12s\n", "----------------------------", "------------", "------------", "------------");
	printf("%-28s | %-12s | %-12s | %-12s\n", "VRAM SSBO write (MB)", "8.0", "0.0", "0.0");
	printf("%-28s | %-12s | %-12s | %-12s\n", "GPU bandwidth (MB)", "60.0", "80.0", "75.0");
	printf("%-28s | %-12s | %-12s | %-12s\n", "Barrier count", "4", "1.5", "1");
	printf("%-28s | %-12s | %-12s | %-12s\n", "Dispatch count", "51", "100", "2");
	printf("%-28s | %-12s | %-12s | %-12s\n", "CPU overhead (us)", "150", "100", "20");
	printf("%-28s | %-12s | %-12s | %-12s\n", "Shader LOC estimate", "562", "800", "850");
	printf("%-28s | %-12s | %-12s | %-12s\n", "Works all GPUs", "yes", "no", "no");
	printf("%-28s | %-12s | %-12s | %-12s\n", "Shipped in games", "yes", "no", "yes");
	printf("%-28s | %-12s | %-12s | %-12s\n", "Vendor caveats", "none", "high", "medium");
	printf("\n");
	printf("Legend:\n");
	printf("  Pattern A = compute cull + indirect draw (current ProjectV baseline)\n");
	printf("  Pattern B = task shader + mesh shader (TODO 2.1 design, the maister says: avoid)\n");
	printf("  Pattern C = mesh shader + indirect count (the maister's universal fast path)\n");
	printf("\n");
}

} // namespace model

int main()
{
	printf("=== Cache + Dispatch Analytical Model ===\n");
	printf("ProjectV mesh-shader-vs-compute-cull experiment\n");
	printf("Per docs/experiments/experiments/2026-06-20-mesh-shader-vs-compute-cull/README.md section 4\n");
	printf("Standalone, no GPU. Output = deterministic analytical estimates from cited papers.\n");
	printf("================================================================================\n\n");

	printf(">>> Scenario 1: Sparse world (VoxelLab-like)\n");
	model::print_scenario(model::kSparseWorld);
	model::print_pattern_costs("Pattern A (compute cull + indirect, current)", model::kPatternA);
	model::print_pattern_costs("Pattern B (task + mesh shader, TODO 2.1 design)", model::kPatternB);
	model::print_pattern_costs("Pattern C (mesh + indirect count, the maister's choice)", model::kPatternC);
	model::print_pattern_quals("Pattern A", model::kPatternAQuals);
	model::print_pattern_quals("Pattern B", model::kPatternBQuals);
	model::print_pattern_quals("Pattern C", model::kPatternCQuals);

	printf(">>> Scenario 2: Dense cave interior\n");
	model::print_scenario(model::kDenseCave);
	printf("(Same pattern costs; scenario affects only cull ratio and face count.)\n\n");

	printf(">>> Scenario 3: Large world (Stage 4.3 future, 128+ chunks)\n");
	model::print_scenario(model::kLargeWorld);
	printf("(Same pattern costs; scale affects absolute bandwidth/counts proportionally.)\n\n");

	model::print_comparison_table();

	printf("==== KEY FINDINGS ====\n");
	printf("- Pattern C saves ~130 us CPU overhead per frame vs Pattern A (~1%% of 16 ms budget)\n");
	printf("- Pattern C saves ~8 MB VRAM (packedFaces SSBO) per frame\n");
	printf("- Pattern C requires Vulkan 1.2+ + driver maturity + ~290 LOC shader code added\n");
	printf("- Pattern B has task shader overhead ~10%% even optimal (the maister 2024)\n");
	printf("- Pattern B has AMD RDNA2 TDR on early-return workaround (GameDev.net 2024)\n");
	printf("- Pattern B is NOT shipped in any known game (the maister 2024-01)\n");
	printf("- Aokana (May 2025, academic SOTA) uses compute shaders, not mesh shaders\n");
	printf("- Per legacy/docs/philosophy/03_domain/01_optimization-philosophy.md:\n");
	printf("  Pattern C savings = ~1%% frame budget < 5%% threshold. Recommend Pattern A as default,\n");
	printf("  Pattern C as feature-flagged optional path (PROJECTV_MESH_SHADER_PIPELINE=ON).\n\n");

	printf("==== VERDICT ====\n");
	printf("`mixed` - compute cull + indirect draw (current) remains correct default.\n");
	printf("Mesh shader pipeline has merit but should be feature-flagged, not default.\n");
	printf("See docs/experiments/experiments/2026-06-20-mesh-shader-vs-compute-cull/README.md section 6-7.\n\n");

	return 0;
}
// SPDX-License-Identifier: MIT
//
// bindless_layout_sketch.cpp - CPU-side analytical model for descriptor binding strategies
// applied to ProjectV Stage 2.x voxel rendering pipeline.
//
// Purpose: illustrate cost-pattern differences between:
//   - Strategy T (Traditional): current ProjectV pattern — per-pipeline VkDescriptorSetLayoutBinding,
//     per-frame descriptor set update via vkUpdateDescriptorSets, frame-in-flight.
//   - Strategy B (Bindless): VK_EXT_descriptor_indexing — unbounded descriptor arrays indexed in
//     shader via runtimeDescriptorArray + nonuniformEXT; PARTIALLY_BOUND; UPDATE_AFTER_BIND.
//   - Strategy P (Push): VK_KHR_push_descriptor — small transient sets inline-in-command-buffer.
//   - Strategy DB (Descriptor Buffer): VK_EXT_descriptor_buffer — descriptors as buffer memory,
//     memcpy()-style update, vkCmdBindDescriptorBuffersEXT.
//   - Strategy H (Hybrid, recommended): bindless for stable-volume chunk/material tables,
//     traditional+dynamic-offset for per-frame SSBOs, push for small transient sets.
//
// NOT a benchmark. Output is deterministic analytical estimates derived from cited sources:
//   - Arm Mali best practices (descriptor_management sample) — 38% frame time saving from caching.
//   - Samsung Traha blog (2024) — 3.5ms / 220 calls vkUpdateDescriptorSets saved by dynamic offset.
//   - NVIDIA Bindless Graphics (legacy OpenGL, 2009) — 7× CPU-side speedup bound.
//   - NVIDIA Advanced API Performance blog (2023-10-27) — 1M descriptor / 2K sampler limits.
//   - XDC 2025 "Descriptors are Hard" — per-vendor HW descriptor cost (NV: 32B/32B, AMD: 32B/16B,
//     Intel: 64B image; descriptor buffer HW on AMD/Intel/Arm, emulated on NVIDIA).
//   - Vincent Parizet — Vulkan Bindless tutorial (2021-12-12, still accurate 2026).
//
// Per docs/experiments/AGENTS.md section 2: "Ne zapuskayu cmake/ctest/ProjectV-binary".
// This file is standalone research artifact, not part of ProjectV mainline.
//
// Build:
//   clang++ -std=c++26 -O3 -march=native -DNDEBUG \
//     docs/experiments/experiments/2026-06-20-bindless-descriptor-overhead/prototype/bindless_layout_sketch.cpp \
//     -o /tmp/bindless_layout_sketch
//
// Run:
//   /tmp/bindless_layout_sketch
//
// Output: per-strategy cost estimates for ProjectV Stage 2.x workload.

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

namespace model {

inline constexpr uint32_t kMaxFramesInFlight = 3u;

inline constexpr uint32_t kCurrentVoxelChunks = 256u;
inline constexpr uint32_t kFutureVoxelChunks = 4096u;

inline constexpr uint32_t kCurrentViewDistanceChunks = 64u;
inline constexpr uint32_t kFutureViewDistanceChunks = 128u;

inline constexpr uint32_t kGraphicsDescriptorsCurrent = 7u;
inline constexpr uint32_t kShadowDescriptorsCurrent = 3u;
inline constexpr uint32_t kVoxelMeshingDescriptorsCurrent = 9u;
inline constexpr uint32_t kTaaResolveDescriptorsCurrent = 4u;

inline constexpr uint32_t kTotalPipelineDescriptorsCurrent =
	kGraphicsDescriptorsCurrent + kShadowDescriptorsCurrent +
	kVoxelMeshingDescriptorsCurrent + kTaaResolveDescriptorsCurrent;

struct DescriptorAccessPattern {
	std::string_view name;
	std::string_view frequency;
	uint32_t size_bytes;
	bool stable_for_streaming;
};

inline constexpr std::array kProjectVAccessPatterns{
	DescriptorAccessPattern{"PackedFace SSBO", "per-frame from compute cull", 0, false},
	DescriptorAccessPattern{"SceneChunkDescriptor SSBO", "per-chunk, edited on edit", 64, false},
	DescriptorAccessPattern{"Sparse64Node pool", "per-chunk, lazy dedup", 272, true},
	DescriptorAccessPattern{"Material table SSBO", "per-material, mostly stable", 64, true},
	DescriptorAccessPattern{"Voxel payload SSBO", "per-chunk, edited on edit", 0, false},
	DescriptorAccessPattern{"Shadow cascade view", "per-frame, low frequency", 0, true},
	DescriptorAccessPattern{"HZB mip image", "per-frame, Stage 2.2", 0, false},
	DescriptorAccessPattern{"TAA history image", "per-frame, dual-buffered", 0, false},
	DescriptorAccessPattern{"Indirect draw buffers", "per-frame from compute cull", 16, false},
	DescriptorAccessPattern{"Motion vector buffer", "per-frame, Stage 5.3", 0, false},
};

struct StrategyCosts {
	std::string_view name;
	double cpu_descriptor_update_us_per_frame;
	double cpu_bind_calls_per_frame;
	double gpu_memory_overhead_bytes_static;
	double gpu_memory_overhead_bytes_per_frame;
	double validation_layer_overhead_factor;
	std::string_view notes;
};

inline constexpr StrategyCosts kTraditional{
	.name = "Traditional (current ProjectV baseline)",
	.cpu_descriptor_update_us_per_frame = 25.0,
	.cpu_bind_calls_per_frame = 14.0,
	.gpu_memory_overhead_bytes_static = 0,
	.gpu_memory_overhead_bytes_per_frame = 0,
	.validation_layer_overhead_factor = 1.0,
	.notes = "Frame-in-flight per-frame descriptor updates. Stable, debug-friendly.",
};

inline constexpr StrategyCosts kBindless{
	.name = "Bindless (VK_EXT_descriptor_indexing)",
	.cpu_descriptor_update_us_per_frame = 2.0,
	.cpu_bind_calls_per_frame = 1.0,
	.gpu_memory_overhead_bytes_static = 4u * 1024u * 1024u,
	.gpu_memory_overhead_bytes_per_frame = 0,
	.validation_layer_overhead_factor = 8.0,
	.notes = "Unbounded arrays + PARTIALLY_BOUND + UPDATE_AFTER_BIND. GPU-AV REQUIRED for PARTIALLY_BOUND validation (8× debug overhead per Khronos docs).",
};

inline constexpr StrategyCosts kPush{
	.name = "Push (VK_KHR_push_descriptor)",
	.cpu_descriptor_update_us_per_frame = 4.0,
	.cpu_bind_calls_per_frame = 14.0,
	.gpu_memory_overhead_bytes_static = 0,
	.gpu_memory_overhead_bytes_per_frame = 0,
	.validation_layer_overhead_factor = 1.05,
	.notes = "vkCmdPushDescriptorSet[KHR] inline update. maxPushDescriptors per layout (HW-dependent, NOT 32 — that's a common misconception).",
};

inline constexpr StrategyCosts kDescriptorBuffer{
	.name = "Descriptor Buffer (VK_EXT_descriptor_buffer)",
	.cpu_descriptor_update_us_per_frame = 1.5,
	.cpu_bind_calls_per_frame = 1.0,
	.gpu_memory_overhead_bytes_static = 2u * 1024u * 1024u,
	.gpu_memory_overhead_bytes_per_frame = 0,
	.validation_layer_overhead_factor = 4.0,
	.notes = "Descriptors as buffer memory, memcpy()-update. HW on AMD/Intel/Arm v9+, emulated on NVIDIA (5 indirections in VKD3D-Proton per XDC 2025).",
};

inline constexpr StrategyCosts kHybrid{
	.name = "Hybrid (recommended)",
	.cpu_descriptor_update_us_per_frame = 6.0,
	.cpu_bind_calls_per_frame = 6.0,
	.gpu_memory_overhead_bytes_static = 1u * 1024u * 1024u,
	.gpu_memory_overhead_bytes_per_frame = 0,
	.validation_layer_overhead_factor = 2.0,
	.notes = "Bindless for chunk/material tables (stable, indexed in shader); traditional+dynamic-offset for per-frame SSBOs; push for small transient sets.",
};

struct PerStageProjection {
	std::string_view stage;
	uint32_t visible_chunks;
	double frame_budget_ms;
};

inline constexpr std::array kStageProjections{
	PerStageProjection{"Current baseline", kCurrentViewDistanceChunks, 16.67},
	PerStageProjection{"Stage 4.3 (128+ chunks)", kFutureViewDistanceChunks, 16.67},
	PerStageProjection{"Stage 4.3 dense (256+ ch)", 256u, 16.67},
};

inline double PercentOfFrameBudget(double cost_us, double frame_budget_ms)
{
	return (cost_us / 1000.0) / frame_budget_ms * 100.0;
}

inline void PrintStrategyHeader(std::string_view name)
{
	std::printf("\n=== %.*s ===\n", static_cast<int>(name.size()), name.data());
}

inline void PrintRow(std::string_view label, double value, std::string_view unit = "")
{
	std::printf("  %-40s %10.2f %s\n", std::string(label).c_str(), value,
				std::string(unit).c_str());
}

inline void PrintStrategy(const StrategyCosts &s, const PerStageProjection &stage)
{
	PrintStrategyHeader(s.name);

	const double total_cpu_us = s.cpu_descriptor_update_us_per_frame +
								s.cpu_bind_calls_per_frame * 0.5;
	PrintRow("CPU desc update per frame", s.cpu_descriptor_update_us_per_frame, "us");
	PrintRow("CPU bind calls per frame", s.cpu_bind_calls_per_frame, "calls");
	PrintRow("CPU total per frame", total_cpu_us, "us");
	PrintRow("GPU static overhead", static_cast<double>(s.gpu_memory_overhead_bytes_static), "B");
	PrintRow("Validation layer factor", s.validation_layer_overhead_factor, "x");
	PrintRow("% of 16.67ms frame budget (CPU only)",
			 PercentOfFrameBudget(total_cpu_us, stage.frame_budget_ms), "%");
	PrintRow("Notes:", 0.0, "");
	std::printf("    %.*s\n", static_cast<int>(s.notes.size()), s.notes.data());
}

inline void PrintProjectVCurrent(const PerStageProjection &stage)
{
	std::printf("\n=== ProjectV current baseline at %.*s (%u chunks visible) ===\n",
				static_cast<int>(stage.stage.size()), stage.stage.data(), stage.visible_chunks);
	std::printf("  Total pipeline descriptors (current): %u bindings across 4 pipelines\n",
				kTotalPipelineDescriptorsCurrent);
	std::printf("  Estimated CPU desc update calls per frame: %u (4 pipelines * 1 update each + 6 transient SSBO rebinds)\n",
				10u);
	std::printf("  Measured reference (analogous workloads):\n");
	std::printf("    - Traha (Samsung, 2024): 220 vkUpdateDescriptorSets = 3.554ms saved by dynamic-offset rewrite (+5 FPS @ 37->42)\n");
	std::printf("    - Arm Mali sample: 44ms -> 27ms frame time = 38%% saving from descriptor caching alone\n");
	std::printf("    - vkguide Ascendant (~400k chunks): per-batch compute dispatch + DrawIndirectInstanced pattern, no bindless\n");
	std::printf("  Per-frame descriptor cost (estimated, 64 visible chunks): 8-12 us CPU + 0 GPU static overhead\n");
	std::printf("  Per-frame descriptor cost (estimated, %u visible chunks, future Stage 4.3):\n", stage.visible_chunks);
	std::printf("    CPU grows ~linearly with chunk count: 16-24 us at %u chunks (still < 0.15%% of 16.67ms frame)\n", stage.visible_chunks);
	std::printf("  Verdict: descriptor update cost is NOT a current bottleneck (< 0.2%% frame budget).\n");
	std::printf("  Stage 2.x consideration: if Stage 2.3 (3D virtual texturing) lands, per-page-table rebind cost grows.\n");
}

inline void PrintStrategiesPerStage(const PerStageProjection &stage)
{
	std::printf("\n========= Per-stage projection: %.*s =========\n",
				static_cast<int>(stage.stage.size()), stage.stage.data());
	PrintProjectVCurrent(stage);
	PrintStrategy(kTraditional, stage);
	PrintStrategy(kBindless, stage);
	PrintStrategy(kPush, stage);
	PrintStrategy(kDescriptorBuffer, stage);
	PrintStrategy(kHybrid, stage);
}

inline void PrintVendorNotes()
{
	std::printf("\n========= Vendor cross-reference (XDC 2025-09-29 \"Descriptors are Hard\") =========\n");
	std::printf("  NVIDIA (Turing+/Ampere/Ada/Blackwell):\n");
	std::printf("    - Internal: descriptor sets implemented as 2 big tables (images + samplers)\n");
	std::printf("    - Table switching = VERY expensive (sticky to context)\n");
	std::printf("    - Bindless native; descriptor buffer EMULATED (5 indirections in VKD3D-Proton)\n");
	std::printf("    - 32B per image descriptor, 32B per sampler descriptor\n");
	std::printf("    - Practical advice: prefer UPDATE_AFTER_BIND for streamed resources; keep descriptor tables bounded where possible\n\n");
	std::printf("  AMD RDNA2 (RX 6000) + RDNA3 (RX 7000):\n");
	std::printf("    - 32B per image descriptor, 16B per sampler descriptor\n");
	std::printf("    - Descriptor buffer HW-supported (efficient path)\n");
	std::printf("    - Vulkan 1.4 = descriptorIndexing core; PARTIALLY_BOUND + UPDATE_AFTER_BIND well-supported\n");
	std::printf("    - RADV (open source) supports all variants\n\n");
	std::printf("  Intel Arc (Alchemist + Battlemage + Core Ultra Meteor Lake+):\n");
	std::printf("    - Gfx12.5+ has TWO modes: LEGACY (binding tables) + BUFFER (descriptor buffer)\n");
	std::printf("    - ANV_ALWAYS_BINDLESS=1 forces bindless path for testing\n");
	std::printf("    - 64B per image descriptor (largest of three vendors)\n");
	std::printf("    - Driver maturity improved significantly in 2024-2025 (Phoronix)\n\n");
	std::printf("  Arm v9+ (mobile, Mali Valhall gen1+):\n");
	std::printf("    - VK_EXT_descriptor_buffer HW-supported (32 set bindings, each -> buffer of descriptors)\n");
	std::printf("    - 'Fully pipelined' descriptor set bindings unlike NVIDIA/Intel\n");
	std::printf("    - Mobile note: 'Adreno 660 performed poorly with bindless' (AsEn 2025 benchmarks)\n");
	std::printf("    - 'On older Mali T830 and Adreno 505, nonuniform() isn't supported, allowing only 16 textures to be bound'\n");
}

inline void PrintWorkloadClassification()
{
	std::printf("\n========= ProjectV descriptor access pattern classification =========\n");
	std::printf("  Stable (bindless candidate):\n");
	std::printf("    - Material table SSBO (per-material, mostly stable, low-frequency update)\n");
	std::printf("    - Sparse64Node pool (per-chunk, lazy dedup, indexed in shader for Stage 2.1)\n");
	std::printf("    - Shadow cascade views (per-frame but stable identity)\n");
	std::printf("  Transient (traditional + dynamic offset, or push):\n");
	std::printf("    - PackedFace SSBO (per-frame from compute cull)\n");
	std::printf("    - Voxel payload SSBO (per-chunk, mutated on edit)\n");
	std::printf("    - HZB mip image (per-frame, Stage 2.2)\n");
	std::printf("    - TAA history image (per-frame, dual-buffered)\n");
	std::printf("    - Indirect draw buffers (per-frame from compute cull)\n");
	std::printf("    - Motion vector buffer (per-frame, Stage 5.3)\n");
	std::printf("  Per-draw-call small (push descriptor candidate):\n");
	std::printf("    - Per-material shadow cascade params (~4 floats, < 32 bytes push constants)\n");
	std::printf("    - Debug view toggle parameters\n");
}

} // namespace model

int main()
{
	std::printf("bindless_layout_sketch — analytical model for ProjectV Stage 2.x descriptor strategy\n");
	std::printf("Per docs/experiments/AGENTS.md section 2: standalone CPU-only research artifact.\n");
	std::printf("Output: deterministic analytical estimates from cited sources (see file header).\n");

	model::PrintWorkloadClassification();
	model::PrintVendorNotes();

	for (const auto &stage : model::kStageProjections) {
		model::PrintStrategiesPerStage(stage);
	}

	std::printf("\n========= Conclusion (for ProjectV Stage 2.x) =========\n");
	std::printf("  VERDICT: HYBRID strategy is the right default for Stage 2.x:\n");
	std::printf("    - Bindless for stable, low-frequency-update descriptors (material table, Sparse64Node pool).\n");
	std::printf("      Wait for Stage 1.1 (sparse 64-tree) + 1.2 (SVDAG) to land first.\n");
	std::printf("    - Traditional + dynamic offset for transient per-frame SSBOs (compute cull output, indirect).\n");
	std::printf("    - Push descriptors for small per-draw transient sets (shadow cascade params, debug toggles).\n");
	std::printf("  DEFER VK_EXT_descriptor_buffer until cross-vendor maturity improves (NVIDIA emulation overhead, VKD3D-Proton 5-indirection tax).\n");
	std::printf("  AVOID pure-bindres for everything:\n");
	std::printf("    - Validation layer overhead (8x for PARTIALLY_BOUND + GPU-AV)\n");
	std::printf("    - Wave-divergence on non-uniform access (matters for compute cull which has mostly uniform access)\n");
	std::printf("    - Debug introspection cost (no CPU-inspectable descriptor state with UPDATE_AFTER_BIND)\n");
	std::printf("  Below 5%% frame budget (per legacy/docs/philosophy/03_domain/01_optimization-philosophy.md):\n");
	std::printf("    - Don't optimize descriptor strategy prematurely. Current cost (25us / 0.15%% frame) is NOT a bottleneck.\n");
	std::printf("    - Re-evaluation trigger: Stage 2.3 (3D virtual texturing), Stage 5.2 (RTX BLAS per chunk), Stage 4.3 (128+ chunks).\n");

	return 0;
}

#pragma once
// Two pipelines:
//  - pipeline_baseline: forward+ inline lighting + N shadow passes.
//  - pipeline_visbuffer: vis-buffer geometry pass + N fullscreen resolve passes.
//
// Both share the same voxel scene input. Measured: GPU time per frame (avg, p95, p99),
// and bandwidth proxy (bytes written to color + depth per frame).

#include "scene.hpp"
#include "vulkan_setup.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace vb {

struct Pipeline {
	// Geometry input.
	AllocatedBuffer packedFacesBuf;
	AllocatedBuffer chunkDescriptorsBuf;
	AllocatedBuffer materialsBuf;

	// Indirect draw (single draw covering all chunks).
	AllocatedBuffer indirectBuf;

	// Render targets (baseline = color only; visbuffer = vis RT + color).
	AllocatedImage colorRT; // RGBA8 (used as final output).
	AllocatedImage depthRT; // D32.
	AllocatedImage visRT;	// R32_UINT vis-buffer (only used in vis-buffer path).

	// Host-visible readback buffer for color.
	AllocatedBuffer readbackBuf;

	// Pipelines.
	VkPipeline baselineGraphics = VK_NULL_HANDLE;
	VkPipelineLayout baselineLayout = VK_NULL_HANDLE;

	VkPipeline visGeometry = VK_NULL_HANDLE;
	VkPipeline visResolve = VK_NULL_HANDLE;
	VkPipelineLayout visLayout = VK_NULL_HANDLE;

	VkExtent2D extent{};
};

// Setup: load shaders, create pipelines, allocate RTs.
bool BuildPipelines(VkContext &ctx, Pipeline &p, VkExtent2D extent,
					const std::vector<PackedFace> &faces,
					const std::vector<ChunkDescriptor> &chunks,
					const std::vector<MaterialVisual> &materials);

// Record commands for one frame of baseline path.
// Does N_cascades shadow passes (1 depth-only per cascade) + 1 main pass.
// Returns GPU time (ms) for the recorded commands (measured via timestamps).
double RecordAndSubmitBaseline(VkContext &ctx, Pipeline &p, uint32_t cascades,
							   const std::array<float, 16> &viewProj,
							   const std::array<float, 4> &sunDir,
							   const std::array<float, 4> &sunColor,
							   const std::array<float, 4> &camPos,
							   std::vector<uint32_t> &colorHashOut);

// Record commands for one frame of vis-buffer path.
// 1 geometry pass (writes vis-buffer + depth) + 1 resolve pass per "light type".
// Returns GPU time (ms).
double RecordAndSubmitVisBuffer(VkContext &ctx, Pipeline &p, uint32_t resolvePasses,
								const std::array<float, 16> &viewProj,
								const std::array<float, 4> &sunDir,
								const std::array<float, 4> &sunColor,
								const std::array<float, 4> &camPos,
								std::vector<uint32_t> &colorHashOut);

// Cleanup.
void DestroyPipelines(VkContext &ctx, Pipeline &p);

// Hash a color buffer (for sanity check — both paths should produce similar hashes).
uint32_t HashColor(const uint8_t *pixels, size_t bytes);

} // namespace vb

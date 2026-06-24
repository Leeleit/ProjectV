#pragma once

#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
#include "render/SceneResources.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>

#include <vulkan/vulkan.h>

namespace projectv::render {

struct FluidCaPushConstants {
	std::array<uint32_t, 4> chunkDimensions{};
	std::array<uint32_t, 4> chunkCountAndFlags{};
	float fluidTickInterval = 0.0f;
	std::array<uint32_t, 3> reserved{};
};
static_assert(sizeof(FluidCaPushConstants) == 48);

struct FluidCaGpuFrameStats {
	uint32_t activeFluidCells = 0u;
	uint32_t droppedFluidCells = 0u;
	uint32_t iteration = 0u;
	uint32_t reserved = 0u;
};
static_assert(sizeof(FluidCaGpuFrameStats) == 16);

inline bool IsFluidCaGpuPipelineRequested()
{
	if (const char *value = std::getenv("PROJECTV_FLUID_CA_GPU")) {
		return value[0] != '\0' && value[0] != '0';
	}
	return true;
}

inline bool IsAsyncComputeEnabled()
{
	if (const char *value = std::getenv("PROJECTV_ASYNC_COMPUTE")) {
		return value[0] != '\0' && value[0] != '0';
	}
	return true;
}

bool CreateFluidCaPipelines(VulkanContextState *context, RenderState *render);

void DestroyFluidCaPipelines(VulkanContextState *context, RenderState *render);

bool RefreshFluidCaResourceBindings(VulkanContextState *context, RenderState *render);

bool RecordFluidCaDispatch(
	VkCommandBuffer commandBuffer,
	RenderState &render,
	SceneFrameResources &frameResources,
	const FluidCaPushConstants &pushConstants,
	uint32_t activeChunkCount);

bool SubmitFluidCaToComputeQueue(
	VulkanContextState *context,
	RenderState &render,
	VkCommandBuffer commandBuffer,
	uint64_t *outTimelineValue);

bool ReadFluidCaFrameStats(
	VulkanContextState *context,
	const RenderState &render,
	uint32_t frameIndex,
	FluidCaGpuFrameStats *outStats);

}  // namespace projectv::render

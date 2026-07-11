#pragma once

#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include <cstdint>

#include <vulkan/vulkan.h>

namespace projectv::render {

bool IsVctGpuPipelineRequested();

struct VoxelizePushConstants {
	int32_t clipmapOrigin[4];
	int32_t chunkCountAndFlags[4];
	uint32_t chunkGrid[4];
};
static_assert(sizeof(VoxelizePushConstants) == 48);

bool CreateVoxelizePipelines(
	VulkanContextState *context,
	RenderState *render);

bool CreateVctClipmapFallbackSamplerOnly(
	VulkanContextState *context,
	RenderState *render);

void DestroyVoxelizePipelines(
	VulkanContextState *context,
	RenderState *render);

bool RefreshVoxelizeResourceBindings(
	VulkanContextState *context,
	RenderState *render,
	uint32_t frameIndex);

bool RecordVoxelizeDispatch(
	VkCommandBuffer commandBuffer,
	RenderState &render,
	SceneFrameResources &frameResources,
	const VoxelizePushConstants &pushConstants,
	uint32_t activeChunkCount);

bool BuildVctClipmapMipChain(
	VkCommandBuffer commandBuffer,
	RenderState &render);

} // namespace projectv::render

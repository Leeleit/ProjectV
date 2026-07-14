#pragma once

#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include <array>

#include <vulkan/vulkan.h>

namespace projectv::render {

constexpr uint32_t kFacesPerCluster = 64u;

struct MeshCullPushConstants {
	std::array<uint32_t, 4> dispatchParams{};
	std::array<std::array<float, 4>, 6> frustumPlanes{};
};
static_assert(sizeof(MeshCullPushConstants) == 112);

struct MeshClusterizePushConstants {
	std::array<uint32_t, 4> params{}; // x=chunkCount, y=maxClusters, z=facesPerCluster, w=unused
};
static_assert(sizeof(MeshClusterizePushConstants) == 16);

bool IsMeshShaderPipelineRequested();

bool IsMeshShaderIndirectRequested();

MeshCullPushConstants BuildMeshCullPushConstants(
	const ChunkCullingParameters &parameters,
	uint32_t dispatchCapacity);

bool RefreshMeshShaderResourceBindings(
	VulkanContextState *context,
	RenderState *render);

void DestroyMeshShaderPipelines(VulkanContextState *context, RenderState *render);

bool CreateMeshShaderPipelines(VulkanContextState *context, RenderState *render);

bool RecordMeshShaderClusterize(
	VkCommandBuffer commandBuffer,
	VulkanContextState *context,
	RenderState &render,
	SceneFrameResources &frameResources,
	uint32_t chunkCount);

bool RecordMeshShaderPreCull(
	VkCommandBuffer commandBuffer,
	VulkanContextState *context,
	RenderState &render,
	SceneFrameResources &frameResources,
	const MeshCullPushConstants &cullPushConstants);

bool RecordMeshShaderDraw(
	VkCommandBuffer commandBuffer,
	RenderState &render,
	SceneFrameResources &frameResources,
	const GraphicsPushConstants &drawPushConstants,
	uint32_t fallbackTaskCount);

} // namespace projectv::render

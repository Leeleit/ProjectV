#pragma once

#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include <array>

#include <vulkan/vulkan.h>

namespace projectv::render {

struct MeshCullPushConstants {
	std::array<uint32_t, 4> dispatchParams{};
	std::array<std::array<float, 4>, 6> frustumPlanes{};
};
static_assert(sizeof(MeshCullPushConstants) == 112);

struct MeshDrawPushConstants {
	std::array<float, 16> viewProjection{};
	std::array<int32_t, 4> worldMinAndChunkSize{};
	std::array<int32_t, 4> worldMaxExclusiveAndChunkCount{};
	std::array<uint32_t, 4> chunkGridAndTransparentFaceBase{};
	std::array<uint32_t, 4> faceCapacities{};
};
static_assert(sizeof(MeshDrawPushConstants) == 128);

bool IsMeshShaderPipelineRequested();

MeshCullPushConstants BuildMeshCullPushConstants(
	const ChunkCullingParameters &parameters,
	uint32_t chunkDescriptorCount);

bool RefreshMeshShaderResourceBindings(
	VulkanContextState *context,
	RenderState *render);

void DestroyMeshShaderPipelines(VulkanContextState *context, RenderState *render);

bool CreateMeshShaderPipelines(VulkanContextState *context, RenderState *render);

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
	const MeshDrawPushConstants &drawPushConstants,
	uint32_t chunkCount);

} // namespace projectv::render

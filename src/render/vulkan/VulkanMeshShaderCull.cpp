#include "render/vulkan/VulkanMeshShaderPipeline.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/EnvUtils.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <algorithm>
#include <cstring>

namespace projectv::render {
namespace {
std::array<float, 4> MakeFrustumPlane(
	const math::Vec3 &normal,
	const math::Vec3 &cameraPos,
	const float offset)
{
	const float w = -projectv::math::dot(cameraPos, normal) - offset;
	return {normal.x, normal.y, normal.z, w};
}

void CmdBufferBarrier(
	const VkCommandBuffer commandBuffer,
	const VkBuffer buffer,
	const VkPipelineStageFlags2 srcStage,
	const VkAccessFlags2 srcAccess,
	const VkPipelineStageFlags2 dstStage,
	const VkAccessFlags2 dstAccess)
{
	if (buffer == VK_NULL_HANDLE) {
		return;
	}
	VkBufferMemoryBarrier2 barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	barrier.srcStageMask = srcStage;
	barrier.srcAccessMask = srcAccess;
	barrier.dstStageMask = dstStage;
	barrier.dstAccessMask = dstAccess;
	barrier.buffer = buffer;
	barrier.offset = 0;
	barrier.size = VK_WHOLE_SIZE;
	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.bufferMemoryBarrierCount = 1u;
	depInfo.pBufferMemoryBarriers = &barrier;
	vkCmdPipelineBarrier2(commandBuffer, &depInfo);
}
} // namespace

bool IsMeshShaderIndirectRequested()
{
	const char *value = core::GetEnvVar("PROJECTV_MESH_SHADER_INDIRECT");
	if (value == nullptr) {
		return true;
	}
	return value[0] != '\0' && value[0] != '0';
}

MeshCullPushConstants BuildMeshCullPushConstants(
	const ChunkCullingParameters &parameters,
	const uint32_t dispatchCapacity)
{
	MeshCullPushConstants result{};
	result.dispatchParams = {dispatchCapacity, 0u, 0u, 0u};

	const math::Vec3 cameraPos{
		parameters.cameraPositionAndMaxDistance.x,
		parameters.cameraPositionAndMaxDistance.y,
		parameters.cameraPositionAndMaxDistance.z,
		0.0f,
	};
	const math::Vec3 forward{
		parameters.cameraForwardAndTanHalfVerticalFov.x,
		parameters.cameraForwardAndTanHalfVerticalFov.y,
		parameters.cameraForwardAndTanHalfVerticalFov.z,
		0.0f,
	};
	const math::Vec3 right{
		parameters.cameraRightAndTanHalfHorizontalFov.x,
		parameters.cameraRightAndTanHalfHorizontalFov.y,
		parameters.cameraRightAndTanHalfHorizontalFov.z,
		0.0f,
	};
	const math::Vec3 up{
		parameters.cameraUpAndNearPlane.x,
		parameters.cameraUpAndNearPlane.y,
		parameters.cameraUpAndNearPlane.z,
		0.0f,
	};
	const float tanHalfVFov = std::max(parameters.cameraForwardAndTanHalfVerticalFov.w, 0.0f);
	const float tanHalfHFov = std::max(parameters.cameraRightAndTanHalfHorizontalFov.w, 0.0f);
	const float nearPlane = std::max(parameters.cameraUpAndNearPlane.w, 0.0f);
	const float maxDistance = parameters.cameraPositionAndMaxDistance.w;

	const math::Vec3 leftNormal{
		forward.x * tanHalfHFov + right.x,
		forward.y * tanHalfHFov + right.y,
		forward.z * tanHalfHFov + right.z,
		0.0f,
	};
	const math::Vec3 rightNormal{
		forward.x * tanHalfHFov - right.x,
		forward.y * tanHalfHFov - right.y,
		forward.z * tanHalfHFov - right.z,
		0.0f,
	};
	const math::Vec3 bottomNormal{
		forward.x * tanHalfVFov + up.x,
		forward.y * tanHalfVFov + up.y,
		forward.z * tanHalfVFov + up.z,
		0.0f,
	};
	const math::Vec3 topNormal{
		forward.x * tanHalfVFov - up.x,
		forward.y * tanHalfVFov - up.y,
		forward.z * tanHalfVFov - up.z,
		0.0f,
	};

	result.frustumPlanes[0] = MakeFrustumPlane(leftNormal, cameraPos, 0.0f);
	result.frustumPlanes[1] = MakeFrustumPlane(rightNormal, cameraPos, 0.0f);
	result.frustumPlanes[2] = MakeFrustumPlane(bottomNormal, cameraPos, 0.0f);
	result.frustumPlanes[3] = MakeFrustumPlane(topNormal, cameraPos, 0.0f);
	result.frustumPlanes[4] = MakeFrustumPlane(forward, cameraPos, -nearPlane);
	result.frustumPlanes[5] = MakeFrustumPlane(math::Vec3{-forward.x, -forward.y, -forward.z, 0.0f}, cameraPos, -maxDistance);
	return result;
}

bool RecordMeshShaderClusterize(
	const VkCommandBuffer commandBuffer,
	VulkanContextState *context,
	RenderState &render,
	SceneFrameResources &frameResources,
	const uint32_t chunkCount)
{
	if (!render.meshShaderEnabled || commandBuffer == VK_NULL_HANDLE || context == nullptr) {
		return false;
	}
	if (render.meshClusterizePipeline == VK_NULL_HANDLE ||
		render.meshClusterizePipelineLayout == VK_NULL_HANDLE ||
		frameResources.meshClusters.clusterizeDescriptorSet == VK_NULL_HANDLE ||
		frameResources.meshClusters.faceClusterCountMappedData == nullptr) {
		return false;
	}

	std::memset(frameResources.meshClusters.faceClusterCountMappedData, 0, sizeof(uint32_t));

	CmdBufferBarrier(
		commandBuffer,
		frameResources.chunkDescriptorBuffer,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		VK_ACCESS_2_SHADER_STORAGE_READ_BIT);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, render.meshClusterizePipeline);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		render.meshClusterizePipelineLayout,
		0u,
		1u,
		&frameResources.meshClusters.clusterizeDescriptorSet,
		0u,
		nullptr);

	const MeshClusterizePushConstants push{
		.params = {chunkCount, render.faceClusterCapacity, kFacesPerCluster, 0u},
	};
	vkCmdPushConstants(
		commandBuffer,
		render.meshClusterizePipelineLayout,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0u,
		sizeof(MeshClusterizePushConstants),
		&push);

	if (chunkCount > 0u) {
		vkCmdDispatch(commandBuffer, (chunkCount + 63u) / 64u, 1u, 1u);
	}
	return true;
}

bool RecordMeshShaderPreCull(
	const VkCommandBuffer commandBuffer,
	VulkanContextState *context,
	RenderState &render,
	SceneFrameResources &frameResources,
	const MeshCullPushConstants &cullPushConstants)
{
	if (!render.meshShaderEnabled) {
		return false;
	}
	if (commandBuffer == VK_NULL_HANDLE || context == nullptr) {
		return false;
	}
	if (render.meshCullPipeline == VK_NULL_HANDLE ||
		render.meshCullPipelineLayout == VK_NULL_HANDLE) {
		return false;
	}
	if (frameResources.meshShaderDescriptorSet == VK_NULL_HANDLE ||
		frameResources.visibleChunkIdBuffer == VK_NULL_HANDLE ||
		frameResources.meshClusters.meshDrawIndirectMappedData == nullptr) {
		return false;
	}

	VkDrawMeshTasksIndirectCommandEXT zeroIndirect{};
	std::memcpy(frameResources.meshClusters.meshDrawIndirectMappedData, &zeroIndirect, sizeof(zeroIndirect));

	CmdBufferBarrier(
		commandBuffer,
		frameResources.meshClusters.faceClusterBuffer,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
	CmdBufferBarrier(
		commandBuffer,
		frameResources.meshClusters.faceClusterCountBuffer,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		VK_ACCESS_2_SHADER_STORAGE_READ_BIT);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, render.meshCullPipeline);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		render.meshCullPipelineLayout,
		0u,
		1u,
		&frameResources.meshShaderDescriptorSet,
		0u,
		nullptr);
	vkCmdPushConstants(
		commandBuffer,
		render.meshCullPipelineLayout,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0u,
		sizeof(MeshCullPushConstants),
		&cullPushConstants);

	const uint32_t capacity = cullPushConstants.dispatchParams[0];
	if (capacity > 0u) {
		vkCmdDispatch(commandBuffer, (capacity + 63u) / 64u, 1u, 1u);
	}

	CmdBufferBarrier(
		commandBuffer,
		frameResources.visibleChunkIdBuffer,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
		VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
		VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
	CmdBufferBarrier(
		commandBuffer,
		frameResources.meshClusters.meshDrawIndirectBuffer,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
		VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT,
		VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT);

	return true;
}
} // namespace projectv::render

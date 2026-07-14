#include "render/vulkan/VulkanMeshShaderPipeline.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"

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
} // namespace

MeshCullPushConstants BuildMeshCullPushConstants(
	const ChunkCullingParameters &parameters,
	const uint32_t chunkDescriptorCount)
{
	MeshCullPushConstants result{};
	result.dispatchParams = {chunkDescriptorCount, 0u, 0u, 0u};

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
		frameResources.visibilityCounterBuffer == VK_NULL_HANDLE) {
		return false;
	}

	std::memset(frameResources.visibilityCounterMappedData, 0, sizeof(uint32_t));

	VkBufferMemoryBarrier2 fillBarrier{};
	fillBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	fillBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	fillBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	fillBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	fillBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	fillBarrier.buffer = frameResources.visibleChunkIdBuffer;
	fillBarrier.offset = 0;
	fillBarrier.size = sizeof(uint32_t) * static_cast<VkDeviceSize>(std::max(render.visibleChunkIdCapacity, 1u));

	{
		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.bufferMemoryBarrierCount = 1u;
		depInfo.pBufferMemoryBarriers = &fillBarrier;
		vkCmdPipelineBarrier2(commandBuffer, &depInfo);
	}

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

	const uint32_t chunkCount = cullPushConstants.dispatchParams[0];
	if (chunkCount > 0u) {
		const uint32_t workgroupCount = (chunkCount + 63u) / 64u;
		vkCmdDispatch(commandBuffer, workgroupCount, 1u, 1u);
	}

	VkBufferMemoryBarrier2 meshBarrier{};
	meshBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	meshBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	meshBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	meshBarrier.dstStageMask = VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT;
	meshBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	meshBarrier.buffer = frameResources.visibleChunkIdBuffer;
	meshBarrier.offset = 0;
	meshBarrier.size = VK_WHOLE_SIZE;
	{
		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.bufferMemoryBarrierCount = 1u;
		depInfo.pBufferMemoryBarriers = &meshBarrier;
		vkCmdPipelineBarrier2(commandBuffer, &depInfo);
	}

	return true;
}
} // namespace projectv::render

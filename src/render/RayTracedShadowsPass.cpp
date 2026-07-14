#include "volk.h"
#include "render/RayTracedShadows.hpp"
#include "SDL3/SDL_log.h"

#include <array>

namespace projectv::render {

bool RayTracedShadows::RecordRayTracedShadowPass(
	const VkCommandBuffer commandBuffer,
	const VulkanContextState &context,
	const VkPipelineStageFlags waitStage,
	const VkAccessFlags waitAccess) const
{
	if (!m_config.enabled) {
		return false;
	}
	if (commandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	(void)context;
	(void)waitStage;
	(void)waitAccess;
	return true;
}

bool RayTracedShadows::RecordVoxelAwareRtxShadowPass(
	const VkCommandBuffer commandBuffer,
	const VulkanContextState &context,
	const uint32_t frameIndex,
	const VkBuffer chunkDescriptorBuffer,
	const VkBuffer sceneLightingBuffer,
	const VkBuffer chunkVoxelPayloadBuffer,
	const float *inverseViewProjection,
	const float *cameraPosition,
	const float *cameraForward,
	const uint32_t screenWidth,
	const uint32_t screenHeight)
{
	if (!m_config.enabled || !m_voxelAwareRtxActive) {
		return false;
	}
	if (commandBuffer == VK_NULL_HANDLE || m_rtxPipeline.GetPipeline() == VK_NULL_HANDLE || !m_rtxSbt.IsReady()) {
		return false;
	}
	if (frameIndex >= MAX_FRAMES_IN_FLIGHT) {
		return false;
	}
	const auto &[cameraUboBuffer, cameraUboAllocation, cameraUboMappedData, descriptorSet] = m_rtxFrames[frameIndex];
	const bool usePushDescriptors = context.features14.pushDescriptor == VK_TRUE;
	if (cameraUboMappedData == nullptr || (!usePushDescriptors && descriptorSet == VK_NULL_HANDLE)) {
		return false;
	}
	if (inverseViewProjection == nullptr || cameraPosition == nullptr || cameraForward == nullptr) {
		return false;
	}
	if (chunkDescriptorBuffer == VK_NULL_HANDLE || sceneLightingBuffer == VK_NULL_HANDLE || chunkVoxelPayloadBuffer == VK_NULL_HANDLE) {
		return false;
	}

	uint8_t *uboMapped = static_cast<uint8_t *>(cameraUboMappedData);
	std::memcpy(uboMapped + 0, inverseViewProjection, 64u);
	const float positionAndWidth[4] = {cameraPosition[0], cameraPosition[1], cameraPosition[2],
									   static_cast<float>(screenWidth)};
	const float forwardAndHeight[4] = {cameraForward[0], cameraForward[1], cameraForward[2],
									   static_cast<float>(screenHeight)};
	std::memcpy(uboMapped + 64, positionAndWidth, 16u);
	std::memcpy(uboMapped + 80, forwardAndHeight, 16u);
	vmaFlushAllocation(context.allocator, cameraUboAllocation, 0u, 96u);

	const VkDescriptorBufferInfo chunkDescriptorInfo{chunkDescriptorBuffer, 0, VK_WHOLE_SIZE};
	const VkDescriptorBufferInfo sceneLightingInfo{sceneLightingBuffer, 0, VK_WHOLE_SIZE};
	const VkDescriptorBufferInfo chunkVoxelPayloadInfo{chunkVoxelPayloadBuffer, 0, VK_WHOLE_SIZE};
	const VkDescriptorImageInfo shadowMaskImageInfo{VK_NULL_HANDLE, m_shadowMaskImageView, VK_IMAGE_LAYOUT_GENERAL};
	const VkDescriptorBufferInfo cameraUboInfo{cameraUboBuffer, 0, 96u};
	VkWriteDescriptorSetAccelerationStructureKHR tlasInfo{};
	tlasInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	tlasInfo.accelerationStructureCount = 1u;
	tlasInfo.pAccelerationStructures = &m_config.tlas;

	std::array<VkWriteDescriptorSet, 6> writes{};
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = descriptorSet;
	writes[0].dstBinding = 1;
	writes[0].dstArrayElement = 0;
	writes[0].descriptorCount = 1u;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &chunkDescriptorInfo;

	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = descriptorSet;
	writes[1].dstBinding = 3;
	writes[1].dstArrayElement = 0;
	writes[1].descriptorCount = 1u;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[1].pBufferInfo = &sceneLightingInfo;

	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = descriptorSet;
	writes[2].dstBinding = 4;
	writes[2].dstArrayElement = 0;
	writes[2].descriptorCount = 1u;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[2].pBufferInfo = &chunkVoxelPayloadInfo;

	writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[3].dstSet = descriptorSet;
	writes[3].dstBinding = 13;
	writes[3].dstArrayElement = 0;
	writes[3].descriptorCount = 1u;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	writes[3].pNext = &tlasInfo;

	writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[4].dstSet = descriptorSet;
	writes[4].dstBinding = 18;
	writes[4].dstArrayElement = 0;
	writes[4].descriptorCount = 1u;
	writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[4].pImageInfo = &shadowMaskImageInfo;

	writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[5].dstSet = descriptorSet;
	writes[5].dstBinding = 19;
	writes[5].dstArrayElement = 0;
	writes[5].descriptorCount = 1u;
	writes[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writes[5].pBufferInfo = &cameraUboInfo;

	if (usePushDescriptors) {
		vkCmdPushDescriptorSet(
			commandBuffer,
			VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
			m_rtxPipeline.GetPipelineLayout(),
			0u,
			static_cast<uint32_t>(writes.size()),
			writes.data());
	} else {
		vkUpdateDescriptorSets(context.device, static_cast<uint32_t>(writes.size()), writes.data(), 0u, nullptr);
	}

	VkImageMemoryBarrier2 imageBarrier{};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
	imageBarrier.srcAccessMask = VK_ACCESS_2_NONE;
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
	imageBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageBarrier.image = m_shadowMaskImage;
	imageBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	VkDependencyInfo imageDepInfo{};
	imageDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	imageDepInfo.imageMemoryBarrierCount = 1u;
	imageDepInfo.pImageMemoryBarriers = &imageBarrier;
	vkCmdPipelineBarrier2(commandBuffer, &imageDepInfo);

	vkCmdBindPipeline(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		m_rtxPipeline.GetPipeline());
	if (!usePushDescriptors) {
		vkCmdBindDescriptorSets(
			commandBuffer,
			VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
			m_rtxPipeline.GetPipelineLayout(),
			0u,
			1u,
			&descriptorSet,
			0u,
			nullptr);
	}

	vkCmdTraceRaysKHR(
		commandBuffer,
		&m_rtxSbt.GetRaygenRegion(),
		&m_rtxSbt.GetMissRegion(),
		&m_rtxSbt.GetHitRegion(),
		&m_rtxSbt.GetCallableRegion(),
		m_shadowMaskWidth,
		m_shadowMaskHeight,
		1u);

	VkImageMemoryBarrier2 readBarrier{};
	readBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	readBarrier.srcStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
	readBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	readBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	readBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	readBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	readBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	readBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	readBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	readBarrier.image = m_shadowMaskImage;
	readBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	VkDependencyInfo readDepInfo{};
	readDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	readDepInfo.imageMemoryBarrierCount = 1u;
	readDepInfo.pImageMemoryBarriers = &readBarrier;
	vkCmdPipelineBarrier2(commandBuffer, &readDepInfo);

	m_config.shadowRayDispatchCount += 1u;
	return true;
}

void RayTracedShadows::RecordDebugReport() const noexcept
{
#if 0
	if (!m_config.enabled) {
		return;
	}
	SDL_Log(
		"Render: RayTracedShadows: instances=%u blasRebuilds=%u tlasRebuilds=%u dispatch=%u fallback=%u",
		m_config.tlasInstanceCount,
		m_config.blasRebuildCount,
		m_config.tlasRebuildCount,
		m_config.shadowRayDispatchCount,
		m_config.fallbackCount);
#endif
}

bool RecordRayTracedShadowPass(
	const VkCommandBuffer commandBuffer,
	RayTracedShadows *rayTracedShadows,
	const VkPipelineStageFlags waitStage,
	const VkAccessFlags waitAccess)
{
	if (rayTracedShadows == nullptr) {
		return false;
	}
	constexpr VulkanContextState dummyContext{};
	return rayTracedShadows->RecordRayTracedShadowPass(
		commandBuffer,
		dummyContext,
		waitStage,
		waitAccess);
}

} // namespace projectv::render

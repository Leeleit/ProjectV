#include "volk.h"
#include "render/RtxGiProbes.hpp"

#include <array>

namespace projectv::render {

bool RtxGiProbes::RecordUpdatePass(
	const VkCommandBuffer commandBuffer,
	const VulkanContextState &context,
	const uint32_t frameIndex,
	const VkBuffer chunkDescriptorBuffer,
	const VkBuffer sceneLightingBuffer,
	const VkBuffer chunkVoxelPayloadBuffer,
	const VkBuffer materialVisualBuffer,
	VkAccelerationStructureKHR tlas,
	const FrameRenderData &renderData)
{
	if (!m_initialized.load(std::memory_order_acquire)) {
		return false;
	}
	if (frameIndex >= MAX_FRAMES_IN_FLIGHT) {
		return false;
	}

	const VkDescriptorBufferInfo chunkDescInfo{chunkDescriptorBuffer, 0, VK_WHOLE_SIZE};
	const VkDescriptorBufferInfo materialInfo{materialVisualBuffer, 0, VK_WHOLE_SIZE};
	const VkDescriptorBufferInfo sceneLightingInfo{sceneLightingBuffer, 0, VK_WHOLE_SIZE};
	const VkDescriptorBufferInfo voxelPayloadInfo{chunkVoxelPayloadBuffer, 0, VK_WHOLE_SIZE};
	const VkDescriptorBufferInfo volumeDescInfo{m_config.volumeDescBuffer, 0, VK_WHOLE_SIZE};

	const VkDescriptorImageInfo irradianceInfo{
		.sampler = VK_NULL_HANDLE,
		.imageView = m_config.irradianceView,
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL};
	const VkDescriptorImageInfo distanceInfo{
		.sampler = VK_NULL_HANDLE,
		.imageView = m_config.distanceView,
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL};

	const VkWriteDescriptorSetAccelerationStructureKHR tlasInfo{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
		.pNext = nullptr,
		.accelerationStructureCount = 1,
		.pAccelerationStructures = &tlas};

	const std::array writes = {
		VkWriteDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = m_descriptorSets[frameIndex],
			.dstBinding = 1,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pImageInfo = nullptr,
			.pBufferInfo = &chunkDescInfo,
			.pTexelBufferView = nullptr},
		VkWriteDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = m_descriptorSets[frameIndex],
			.dstBinding = 2,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pImageInfo = nullptr,
			.pBufferInfo = &materialInfo,
			.pTexelBufferView = nullptr},
		VkWriteDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = m_descriptorSets[frameIndex],
			.dstBinding = 3,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pImageInfo = nullptr,
			.pBufferInfo = &sceneLightingInfo,
			.pTexelBufferView = nullptr},
		VkWriteDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = m_descriptorSets[frameIndex],
			.dstBinding = 4,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pImageInfo = nullptr,
			.pBufferInfo = &voxelPayloadInfo,
			.pTexelBufferView = nullptr},
		VkWriteDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = &tlasInfo,
			.dstSet = m_descriptorSets[frameIndex],
			.dstBinding = 13,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
			.pImageInfo = nullptr,
			.pBufferInfo = nullptr,
			.pTexelBufferView = nullptr},
		VkWriteDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = m_descriptorSets[frameIndex],
			.dstBinding = 14,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.pImageInfo = &irradianceInfo,
			.pBufferInfo = nullptr,
			.pTexelBufferView = nullptr},
		VkWriteDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = m_descriptorSets[frameIndex],
			.dstBinding = 15,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.pImageInfo = &distanceInfo,
			.pBufferInfo = nullptr,
			.pTexelBufferView = nullptr},
		VkWriteDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = m_descriptorSets[frameIndex],
			.dstBinding = 17,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pImageInfo = nullptr,
			.pBufferInfo = &volumeDescInfo,
			.pTexelBufferView = nullptr}};

	vkUpdateDescriptorSets(context.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

	std::array<VkImageMemoryBarrier, 2> imageBarriers{};
	imageBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageBarriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageBarriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageBarriers[0].image = m_config.irradianceImage;
	imageBarriers[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};

	imageBarriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageBarriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageBarriers[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	imageBarriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageBarriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageBarriers[1].image = m_config.distanceImage;
	imageBarriers[1].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};

	if (m_config.updateDispatchCount > 0u) {
		imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageBarriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		imageBarriers[1].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageBarriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	} else {
		imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarriers[0].srcAccessMask = VK_ACCESS_NONE;
		imageBarriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarriers[1].srcAccessMask = VK_ACCESS_NONE;
	}

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0u,
		0u, nullptr,
		0u, nullptr,
		static_cast<uint32_t>(imageBarriers.size()), imageBarriers.data());

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		m_pipelineLayout,
		0u,
		1u,
		&m_descriptorSets[frameIndex],
		0u,
		nullptr);

	vkCmdPushConstants(
		commandBuffer,
		m_pipelineLayout,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0,
		sizeof(GraphicsPushConstants),
		&renderData.graphicsPushConstants);

	const uint32_t totalProbes = m_config.probeCountAxisX * m_config.probeCountAxisY * m_config.probeCountAxisZ;
	vkCmdDispatch(commandBuffer, 1, 1, totalProbes);

	imageBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	imageBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	imageBarriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	imageBarriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageBarriers[1].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageBarriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0u,
		0u, nullptr,
		0u, nullptr,
		static_cast<uint32_t>(imageBarriers.size()), imageBarriers.data());

	m_config.totalRaysDispatched += totalProbes * m_config.raysPerProbe;
	++m_config.updateDispatchCount;
	return true;
}

bool RecordRtxGiProbeUpdatePass(
	const VkCommandBuffer commandBuffer,
	RtxGiProbes *probes,
	const VulkanContextState &context,
	const uint32_t frameIndex,
	const VkBuffer chunkDescriptorBuffer,
	const VkBuffer sceneLightingBuffer,
	const VkBuffer chunkVoxelPayloadBuffer,
	const VkBuffer materialVisualBuffer,
	const VkAccelerationStructureKHR tlas,
	const FrameRenderData &renderData)
{
	if (probes == nullptr) {
		return true;
	}
	return probes->RecordUpdatePass(
		commandBuffer,
		context,
		frameIndex,
		chunkDescriptorBuffer,
		sceneLightingBuffer,
		chunkVoxelPayloadBuffer,
		materialVisualBuffer,
		tlas,
		renderData);
}

} // namespace projectv::render

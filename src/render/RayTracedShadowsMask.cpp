#include "volk.h"
#include "render/RayTracedShadows.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "SDL3/SDL_log.h"

#include <array>

namespace projectv::render {

bool RayTracedShadows::RecreateShadowMaskForExtent(const VulkanContextState &context, const uint32_t width, const uint32_t height)
{
	if (!m_voxelAwareRtxActive) {
		return false;
	}
	if (width == 0u || height == 0u) {
		return false;
	}
	if (width == m_shadowMaskWidth && height == m_shadowMaskHeight && m_shadowMaskImage != VK_NULL_HANDLE) {
		return true;
	}
	if (context.device == VK_NULL_HANDLE || context.allocator == nullptr) {
		return false;
	}

	if (m_shadowMaskImageView != VK_NULL_HANDLE) {
		vkDestroyImageView(context.device, m_shadowMaskImageView, nullptr);
		m_shadowMaskImageView = VK_NULL_HANDLE;
	}
	if (m_shadowMaskImage != VK_NULL_HANDLE) {
		vmaDestroyImage(context.allocator, m_shadowMaskImage, m_shadowMaskAllocation);
		m_shadowMaskImage = VK_NULL_HANDLE;
		m_shadowMaskAllocation = nullptr;
	}

	m_shadowMaskWidth = width;
	m_shadowMaskHeight = height;

	VkImageCreateInfo imageInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .samples = VK_SAMPLE_COUNT_1_BIT};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = m_shadowMaskFormat;
	imageInfo.extent = {width, height, 1u};
	imageInfo.mipLevels = 1u;
	imageInfo.arrayLayers = 1u;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VmaAllocationCreateInfo imageAllocInfo{};
	imageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	const VkResult createImageResult = vmaCreateImage(
		context.allocator,
		&imageInfo,
		&imageAllocInfo,
		&m_shadowMaskImage,
		&m_shadowMaskAllocation,
		nullptr);
	if (createImageResult != VK_SUCCESS) {
		runtime::LogVkFailure("RayTracedShadows.RecreateShadowMaskForExtent.vmaCreateImage", createImageResult);
		m_shadowMaskWidth = 0u;
		m_shadowMaskHeight = 0u;
		return false;
	}

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = m_shadowMaskImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = m_shadowMaskFormat;
	viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	if (vkCreateImageView(context.device, &viewInfo, nullptr, &m_shadowMaskImageView) != VK_SUCCESS) {
		runtime::LogVkFailure("RayTracedShadows.RecreateShadowMaskForExtent.vkCreateImageView", VK_ERROR_INITIALIZATION_FAILED);
		vmaDestroyImage(context.allocator, m_shadowMaskImage, m_shadowMaskAllocation);
		m_shadowMaskImage = VK_NULL_HANDLE;
		m_shadowMaskAllocation = nullptr;
		m_shadowMaskWidth = 0u;
		m_shadowMaskHeight = 0u;
		return false;
	}

	if (!InitializeShadowMaskClear(context)) {
		SDL_Log("Render: RayTracedShadows: shadow mask clear failed after recreate; first frame may sample undefined data");
	}

	SDL_Log("Render: RayTracedShadows: shadow mask resized to %ux%u", width, height);
	return true;
}

bool RayTracedShadows::CreateVoxelAwareRtxResources(const VulkanContextState &context)
{
	if (!context.rayTracing.rayTracingPipeline) {
		return false;
	}
	if (m_voxelAwareRtxActive) {
		return true;
	}
	const VkDevice device = context.device;
	if (device == VK_NULL_HANDLE || context.allocator == nullptr) {
		return false;
	}

	RtxShadowPipelineConfig pipelineConfig{};
	pipelineConfig.shaderGroupHandleSize = context.rayTracing.shaderGroupHandleSize;
	pipelineConfig.shaderGroupBaseAlignment = context.rayTracing.shaderGroupBaseAlignment;
	pipelineConfig.shaderGroupHandleAlignment = context.rayTracing.shaderGroupHandleAlignment;
	if (!m_rtxPipeline.Initialize(context, pipelineConfig)) {
		SDL_Log("Render: RtxShadowPipeline.Initialize failed; voxel-aware RT shadows disabled");
		return false;
	}
	if (!m_rtxSbt.Initialize(
			context,
			m_rtxPipeline.GetPipeline(),
			RtxShadowPipeline::GetRayGenGroupIndex(),
			RtxShadowPipeline::GetMissGroupIndex(),
			RtxShadowPipeline::GetHitGroupIndex())) {
		m_rtxSbt.Shutdown(context);
		m_rtxPipeline.Shutdown(context);
		return false;
	}

	m_shadowMaskWidth = 1920u;
	m_shadowMaskHeight = 1080u;

	VkImageCreateInfo imageInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .samples = VK_SAMPLE_COUNT_1_BIT};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = m_shadowMaskFormat;
	imageInfo.extent = {m_shadowMaskWidth, m_shadowMaskHeight, 1u};
	imageInfo.mipLevels = 1u;
	imageInfo.arrayLayers = 1u;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VmaAllocationCreateInfo imageAllocInfo{};
	imageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	const VkResult createImageResult = vmaCreateImage(
		context.allocator,
		&imageInfo,
		&imageAllocInfo,
		&m_shadowMaskImage,
		&m_shadowMaskAllocation,
		nullptr);
	if (createImageResult != VK_SUCCESS) {
		runtime::LogVkFailure("RayTracedShadows.CreateVoxelAwareRtxResources.vmaCreateImage", createImageResult);
		ReleaseVoxelAwareRtxResources(context);
		return false;
	}

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = m_shadowMaskImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = m_shadowMaskFormat;
	viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	if (vkCreateImageView(device, &viewInfo, nullptr, &m_shadowMaskImageView) != VK_SUCCESS) {
		runtime::LogVkFailure("RayTracedShadows.CreateVoxelAwareRtxResources.vkCreateImageView", VK_ERROR_INITIALIZATION_FAILED);
		ReleaseVoxelAwareRtxResources(context);
		return false;
	}

	std::array<VkDescriptorPoolSize, 4> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[0].descriptorCount = 2u * MAX_FRAMES_IN_FLIGHT;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[2].descriptorCount = MAX_FRAMES_IN_FLIGHT;
	poolSizes[3].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[3].descriptorCount = MAX_FRAMES_IN_FLIGHT;
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_rtxDescriptorPool) != VK_SUCCESS) {
		runtime::LogVkFailure("RayTracedShadows.CreateVoxelAwareRtxResources.vkCreateDescriptorPool", VK_ERROR_INITIALIZATION_FAILED);
		ReleaseVoxelAwareRtxResources(context);
		return false;
	}

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		auto &[cameraUboBuffer, cameraUboAllocation, cameraUboMappedData, descriptorSet] = m_rtxFrames[i];

		VkBufferCreateInfo uboInfo{};
		uboInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		uboInfo.size = 96u;
		uboInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		uboInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VmaAllocationCreateInfo uboAllocInfo{};
		uboAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		uboAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		if (vmaCreateBuffer(context.allocator, &uboInfo, &uboAllocInfo,
							&cameraUboBuffer, &cameraUboAllocation, nullptr) != VK_SUCCESS) {
			runtime::LogVkFailure("RayTracedShadows.CreateVoxelAwareRtxResources.vmaCreateBuffer.Ubo", VK_ERROR_INITIALIZATION_FAILED);
			ReleaseVoxelAwareRtxResources(context);
			return false;
		}
		VmaAllocationInfo mappedInfo{};
		vmaGetAllocationInfo(context.allocator, cameraUboAllocation, &mappedInfo);
		cameraUboMappedData = mappedInfo.pMappedData;
		std::memset(cameraUboMappedData, 0, 96u);

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_rtxDescriptorPool;
		allocInfo.descriptorSetCount = 1u;
		allocInfo.pSetLayouts = &m_rtxPipeline.GetDescriptorSetLayout();
		if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
			runtime::LogVkFailure("RayTracedShadows.CreateVoxelAwareRtxResources.vkAllocateDescriptorSets", VK_ERROR_INITIALIZATION_FAILED);
			ReleaseVoxelAwareRtxResources(context);
			return false;
		}
	}

	m_voxelAwareRtxActive = true;
	SDL_Log("Render: VoxelAwareRtxShadows: ready (image=%ux%u, frames=%u)", m_shadowMaskWidth, m_shadowMaskHeight, MAX_FRAMES_IN_FLIGHT);
	return true;
}

void RayTracedShadows::ReleaseVoxelAwareRtxResources(const VulkanContextState &context) noexcept
{
	const VkDevice device = context.device;
	for (auto &[cameraUboBuffer, cameraUboAllocation, cameraUboMappedData, descriptorSet] : m_rtxFrames) {
		if (cameraUboBuffer != VK_NULL_HANDLE && context.allocator != nullptr) {
			vmaDestroyBuffer(context.allocator, cameraUboBuffer, cameraUboAllocation);
		}
		cameraUboBuffer = VK_NULL_HANDLE;
		cameraUboAllocation = nullptr;
		cameraUboMappedData = nullptr;
		descriptorSet = VK_NULL_HANDLE;
	}
	if (m_rtxDescriptorPool != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device, m_rtxDescriptorPool, nullptr);
		m_rtxDescriptorPool = VK_NULL_HANDLE;
	}
	if (m_shadowMaskImageView != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
		vkDestroyImageView(device, m_shadowMaskImageView, nullptr);
		m_shadowMaskImageView = VK_NULL_HANDLE;
	}
	if (m_shadowMaskImage != VK_NULL_HANDLE && context.allocator != nullptr) {
		vmaDestroyImage(context.allocator, m_shadowMaskImage, m_shadowMaskAllocation);
		m_shadowMaskImage = VK_NULL_HANDLE;
		m_shadowMaskAllocation = nullptr;
	}
	m_rtxSbt.Shutdown(context);
	m_rtxPipeline.Shutdown(context);
	m_shadowMaskWidth = 0u;
	m_shadowMaskHeight = 0u;
	m_voxelAwareRtxActive = false;
}

bool RayTracedShadows::InitializeShadowMaskClear(const VulkanContextState &context) const
{
	if (m_shadowMaskImage == VK_NULL_HANDLE || context.device == VK_NULL_HANDLE || context.commandPool == VK_NULL_HANDLE || context.queue == VK_NULL_HANDLE) {
		return false;
	}

	VkCommandBufferAllocateInfo cmdInfo{};
	cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdInfo.commandPool = context.commandPool;
	cmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdInfo.commandBufferCount = 1u;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	if (vkAllocateCommandBuffers(context.device, &cmdInfo, &cmd) != VK_SUCCESS) {
		return false;
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
		vkFreeCommandBuffers(context.device, context.commandPool, 1u, &cmd);
		return false;
	}

	VkImageMemoryBarrier2 toTransfer{};
	toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	toTransfer.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
	toTransfer.srcAccessMask = VK_ACCESS_2_NONE;
	toTransfer.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	toTransfer.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toTransfer.image = m_shadowMaskImage;
	toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	VkDependencyInfo toTransferDep{};
	toTransferDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	toTransferDep.imageMemoryBarrierCount = 1u;
	toTransferDep.pImageMemoryBarriers = &toTransfer;
	vkCmdPipelineBarrier2(cmd, &toTransferDep);

	VkClearColorValue clearValue{};
	clearValue.float32[0] = 1.0f;
	clearValue.float32[1] = 1.0f;
	clearValue.float32[2] = 1.0f;
	clearValue.float32[3] = 1.0f;
	VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	vkCmdClearColorImage(cmd, m_shadowMaskImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1u, &range);

	VkImageMemoryBarrier2 toGeneral{};
	toGeneral.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	toGeneral.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	toGeneral.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	toGeneral.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	toGeneral.dstAccessMask = VK_ACCESS_2_NONE;
	toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	toGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toGeneral.image = m_shadowMaskImage;
	toGeneral.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	VkDependencyInfo toGeneralDep{};
	toGeneralDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	toGeneralDep.imageMemoryBarrierCount = 1u;
	toGeneralDep.pImageMemoryBarriers = &toGeneral;
	vkCmdPipelineBarrier2(cmd, &toGeneralDep);

	vkEndCommandBuffer(cmd);

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VkFence fence = VK_NULL_HANDLE;
	if (vkCreateFence(context.device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
		vkFreeCommandBuffers(context.device, context.commandPool, 1u, &cmd);
		return false;
	}

	VkCommandBufferSubmitInfo commandBufferSubmitInfo{};
	commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	commandBufferSubmitInfo.commandBuffer = cmd;
	VkSubmitInfo2 submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.commandBufferInfoCount = 1u;
	submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;
	const VkResult queueSubmitResult = vkQueueSubmit2(context.queue, 1u, &submitInfo, fence);
	if (queueSubmitResult == VK_SUCCESS) {
		vkWaitForFences(context.device, 1u, &fence, VK_TRUE, UINT64_MAX);
	}

	vkDestroyFence(context.device, fence, nullptr);
	vkFreeCommandBuffers(context.device, context.commandPool, 1u, &cmd);
	return queueSubmitResult == VK_SUCCESS;
}

} // namespace projectv::render

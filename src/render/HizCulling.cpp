#include "render/HizCulling.hpp"

#include <algorithm>
#include <cstdlib>

#include "SDL3/SDL_log.h"
#include "core/RuntimeDiagnostics.hpp"
#include "render/vulkan/VulkanDebug.hpp"

namespace projectv::render {

bool IsHzbCullingEnabled()
{
	if (const char *value = std::getenv("PROJECTV_HZB_CULLING")) {
		return value[0] != '\0' && value[0] != '0';
	}
	return false;
}

uint32_t ComputeHzbMipLevelCount(const uint32_t baseWidth, const uint32_t baseHeight)
{
	const uint32_t minExtent = std::max(1u, std::min(baseWidth, baseHeight));
	uint32_t levels = 1u;
	uint32_t currentExtent = minExtent;
	while (currentExtent > 1u) {
		currentExtent = std::max(1u, currentExtent >> 1u);
		++levels;
	}
	return levels;
}

bool CreateHizBuffer(
	VulkanContextState *context,
	const uint32_t baseWidth,
	const uint32_t baseHeight,
	HizBuffer &outBuffer)
{
	if (!context || !context->allocator || context->device == VK_NULL_HANDLE) {
		return false;
	}
	if (baseWidth == 0u || baseHeight == 0u) {
		return false;
	}

	const uint32_t mipLevels = ComputeHzbMipLevelCount(baseWidth, baseHeight);

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R32_SFLOAT;
	imageInfo.extent = {baseWidth, baseHeight, 1u};
	imageInfo.mipLevels = mipLevels;
	imageInfo.arrayLayers = 1u;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

	if (vmaCreateImage(
			context->allocator,
			&imageInfo,
			&allocationInfo,
			&outBuffer.image,
			&outBuffer.allocation,
			nullptr) != VK_SUCCESS) {
		runtime::LogRuntimeFailure(
			"Render",
			"CreateHizBuffer.vmaCreateImage",
			"failed to allocate Hi-Z mip chain image");
		return false;
	}

	VmaAllocationInfo vmaInfo{};
	vmaGetAllocationInfo(context->allocator, outBuffer.allocation, &vmaInfo);
	outBuffer.memory = vmaInfo.deviceMemory;

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = outBuffer.image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R32_SFLOAT;
	viewInfo.subresourceRange = {
		VK_IMAGE_ASPECT_COLOR_BIT,
		0u,
		mipLevels,
		0u,
		1u,
	};
	if (vkCreateImageView(context->device, &viewInfo, nullptr, &outBuffer.imageView) != VK_SUCCESS) {
		vmaDestroyImage(context->allocator, outBuffer.image, outBuffer.allocation);
		outBuffer = {};
		runtime::LogRuntimeFailure(
			"Render",
			"CreateHizBuffer.vkCreateImageView",
			"failed to create Hi-Z image view");
		return false;
	}

	outBuffer.baseWidth = baseWidth;
	outBuffer.baseHeight = baseHeight;
	outBuffer.mipLevelCount = mipLevels;

	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(outBuffer.image),
		VK_OBJECT_TYPE_IMAGE,
		"HizBufferImage");
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(outBuffer.imageView),
		VK_OBJECT_TYPE_IMAGE_VIEW,
		"HizBufferImageView");
	return true;
}

void DestroyHizBuffer(VulkanContextState *context, HizBuffer &buffer)
{
	if (!context || !context->allocator) {
		return;
	}
	if (buffer.imageView != VK_NULL_HANDLE && context->device != VK_NULL_HANDLE) {
		vkDestroyImageView(context->device, buffer.imageView, nullptr);
	}
	if (buffer.image != VK_NULL_HANDLE) {
		vmaDestroyImage(context->allocator, buffer.image, buffer.allocation);
	}
	buffer = {};
}

void BuildHizMipChain(
	VkCommandBuffer commandBuffer,
	VkImage depthImage,
	const VkImageLayout depthImageLayout,
	const HizBuffer &hizBuffer)
{
	if (commandBuffer == VK_NULL_HANDLE ||
		depthImage == VK_NULL_HANDLE ||
		hizBuffer.image == VK_NULL_HANDLE) {
		return;
	}

	const uint32_t mipLevels = hizBuffer.mipLevelCount;
	if (mipLevels == 0u) {
		return;
	}

	VkImageMemoryBarrier barriers[2]{};
	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barriers[0].oldLayout = depthImageLayout;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].image = depthImage;
	barriers[0].subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u};

	barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[1].srcAccessMask = 0u;
	barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].image = hizBuffer.image;
	barriers[1].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0u,
		0u,
		nullptr,
		0u,
		nullptr,
		2u,
		barriers);

	VkImageBlit blit{};
	blit.srcSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
	blit.srcOffsets[0] = {0, 0, 0};
	blit.srcOffsets[1] = {
		static_cast<int32_t>(hizBuffer.baseWidth),
		static_cast<int32_t>(hizBuffer.baseHeight),
		1,
	};
	blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
	blit.dstOffsets[0] = {0, 0, 0};
	blit.dstOffsets[1] = {
		static_cast<int32_t>(hizBuffer.baseWidth),
		static_cast<int32_t>(hizBuffer.baseHeight),
		1,
	};
	vkCmdBlitImage(
		commandBuffer,
		depthImage,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		hizBuffer.image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1u,
		&blit,
		VK_FILTER_NEAREST);

	uint32_t srcWidth = hizBuffer.baseWidth;
	uint32_t srcHeight = hizBuffer.baseHeight;
	for (uint32_t mipLevel = 1u; mipLevel < mipLevels; ++mipLevel) {
		const uint32_t dstWidth = std::max(1u, srcWidth >> 1u);
		const uint32_t dstHeight = std::max(1u, srcHeight >> 1u);

		VkImageMemoryBarrier mipBarrier{};
		mipBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		mipBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		mipBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		mipBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		mipBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		mipBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		mipBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		mipBarrier.image = hizBuffer.image;
		mipBarrier.subresourceRange = {
			VK_IMAGE_ASPECT_COLOR_BIT,
			mipLevel - 1u,
			1u,
			0u,
			1u,
		};
		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0u,
			0u,
			nullptr,
			0u,
			nullptr,
			1u,
			&mipBarrier);

		VkImageBlit mipBlit{};
		mipBlit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevel - 1u, 0u, 1u};
		mipBlit.srcOffsets[0] = {0, 0, 0};
		mipBlit.srcOffsets[1] = {
			static_cast<int32_t>(srcWidth),
			static_cast<int32_t>(srcHeight),
			1,
		};
		mipBlit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 0u, 1u};
		mipBlit.dstOffsets[0] = {0, 0, 0};
		mipBlit.dstOffsets[1] = {
			static_cast<int32_t>(dstWidth),
			static_cast<int32_t>(dstHeight),
			1,
		};
		vkCmdBlitImage(
			commandBuffer,
			hizBuffer.image,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			hizBuffer.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1u,
			&mipBlit,
			VK_FILTER_LINEAR);

		srcWidth = dstWidth;
		srcHeight = dstHeight;
	}

	VkImageMemoryBarrier finalBarrier{};
	finalBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	finalBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	finalBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	finalBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	finalBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	finalBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	finalBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	finalBarrier.image = hizBuffer.image;
	finalBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, mipLevels, 0u, 1u};
	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0u,
		0u,
		nullptr,
		0u,
		nullptr,
		1u,
		&finalBarrier);

	VkImageMemoryBarrier restoreDepth{};
	restoreDepth.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	restoreDepth.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	restoreDepth.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	restoreDepth.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	restoreDepth.newLayout = depthImageLayout;
	restoreDepth.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	restoreDepth.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	restoreDepth.image = depthImage;
	restoreDepth.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u};
	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		0u,
		0u,
		nullptr,
		0u,
		nullptr,
		1u,
		&restoreDepth);
}

}  // namespace projectv::render

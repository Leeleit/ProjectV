#include "render/TaaRenderTargets.hpp"

#include "render/vulkan/VulkanDebug.hpp"

// The header deliberately keeps `VmaAllocation` as a `void*` so it
// does not have to pull in `vk_mem_alloc.h` (which itself is only
// reachable through `core/Types.hpp` *after* the `VulkanContextState`
// forward declaration). Pull the real definition in here, where the
// `vmaCreateImage` / `vmaDestroyImage` calls actually need it.
#include "vk_mem_alloc.h"

#include <algorithm>
#include <array>

namespace projectv::taa {

namespace {

void TransitionImageLayout(
	const VkCommandBuffer cmd,
	const VkImage image,
	const VkImageLayout oldLayout,
	const VkImageLayout newLayout,
	const VkPipelineStageFlags2 srcStageMask,
	const VkAccessFlags2 srcAccessMask,
	const VkPipelineStageFlags2 dstStageMask,
	const VkAccessFlags2 dstAccessMask)
{
	VkImageMemoryBarrier2 imageBarrier{};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageBarrier.srcStageMask = srcStageMask;
	imageBarrier.srcAccessMask = srcAccessMask;
	imageBarrier.dstStageMask = dstStageMask;
	imageBarrier.dstAccessMask = dstAccessMask;
	imageBarrier.oldLayout = oldLayout;
	imageBarrier.newLayout = newLayout;
	imageBarrier.image = image;
	imageBarrier.subresourceRange = {
		VK_IMAGE_ASPECT_COLOR_BIT,
		0, 1, 0, 1,
	};

	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2(cmd, &depInfo);
}

void DestroyTarget(VulkanContextState *context, OffscreenColorTarget &target)
{
	if (target.imageView != VK_NULL_HANDLE) {
		vkDestroyImageView(context->device, target.imageView, nullptr);
		target.imageView = VK_NULL_HANDLE;
	}
	if (target.image != VK_NULL_HANDLE && target.allocation != nullptr) {
		vmaDestroyImage(context->allocator, target.image, static_cast<VmaAllocation>(target.allocation));
		target.image = VK_NULL_HANDLE;
		target.allocation = nullptr;
	}
}

} // namespace

bool CreateOrRecreateTaaRenderTargets(
	VulkanContextState *context,
	const VkExtent2D extent,
	OffscreenColorTarget &sceneColor,
	OffscreenColorTarget &historyColor,
	VkSampler &linearSampler)
{
	if (!context || !context->allocator || context->device == VK_NULL_HANDLE) {
		return false;
	}
	if (extent.width == 0u || extent.height == 0u) {
		return false;
	}

	// Tear down the previous pair before we allocate. The recreate path
	// is the only legal way to resize these targets so any partial state
	// from a failed create is the same as "no allocation yet".
	DestroyTaaRenderTargets(context, sceneColor, historyColor, linearSampler);

	const VkExtent3D imageExtent{extent.width, extent.height, 1u};
	// Source of truth: `kTaaSceneColorFormat` in `TaaRenderTargets.hpp`.
	// The graphics pipeline declaration in `VulkanGraphicsPipeline.cpp`
	// reads the same constant so the two cannot drift.
	const VkFormat targetFormat = kTaaSceneColorFormat;

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = targetFormat;
	imageInfo.extent = imageExtent;
	imageInfo.mipLevels = 1u;
	imageInfo.arrayLayers = 1u;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage =
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT |
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocationInfo.flags = 0;

	auto allocateTarget = [&](OffscreenColorTarget &target, const char *name) -> bool {
		// Cast the `void*` handle back to its real VMA type for the
		// call. The `void*` representation in the public header is
		// just there to avoid leaking `vk_mem_alloc.h` into every
		// translation unit that needs the offscreen-target struct.
		VmaAllocation allocation = nullptr;
		if (vmaCreateImage(
				context->allocator,
				&imageInfo,
				&allocationInfo,
				&target.image,
				&allocation,
				nullptr) != VK_SUCCESS) {
			return false;
		}
		target.allocation = allocation;
		SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(target.image), VK_OBJECT_TYPE_IMAGE, name);

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = target.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = targetFormat;
		viewInfo.components = {
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
		};
		viewInfo.subresourceRange = {
			VK_IMAGE_ASPECT_COLOR_BIT,
			0, 1, 0, 1,
		};
		if (vkCreateImageView(context->device, &viewInfo, nullptr, &target.imageView) != VK_SUCCESS) {
			return false;
		}
		SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(target.imageView), VK_OBJECT_TYPE_IMAGE_VIEW, name);
		return true;
	};

	if (!allocateTarget(sceneColor, "TaaSceneColorImage")) {
		DestroyTaaRenderTargets(context, sceneColor, historyColor, linearSampler);
		return false;
	}
	if (!allocateTarget(historyColor, "TaaHistoryColorImage")) {
		DestroyTaaRenderTargets(context, sceneColor, historyColor, linearSampler);
		return false;
	}

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	if (vkCreateSampler(context->device, &samplerInfo, nullptr, &linearSampler) != VK_SUCCESS) {
		DestroyTaaRenderTargets(context, sceneColor, historyColor, linearSampler);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(linearSampler), VK_OBJECT_TYPE_SAMPLER, "TaaLinearSampler");

	return true;
}

void DestroyTaaRenderTargets(
	VulkanContextState *context,
	OffscreenColorTarget &sceneColor,
	OffscreenColorTarget &historyColor,
	VkSampler &linearSampler)
{
	if (!context) {
		return;
	}
	if (linearSampler != VK_NULL_HANDLE) {
		vkDestroySampler(context->device, linearSampler, nullptr);
		linearSampler = VK_NULL_HANDLE;
	}
	DestroyTarget(context, sceneColor);
	DestroyTarget(context, historyColor);
}

void TransitionTaaSceneColorForWrite(
	const VkCommandBuffer cmd,
	const OffscreenColorTarget &sceneColor)
{
	if (sceneColor.image == VK_NULL_HANDLE) {
		return;
	}
	TransitionImageLayout(
		cmd,
		sceneColor.image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_NONE,
		0,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
}

void TransitionTaaSceneColorForSample(
	const VkCommandBuffer cmd,
	const OffscreenColorTarget &sceneColor)
{
	if (sceneColor.image == VK_NULL_HANDLE) {
		return;
	}
	TransitionImageLayout(
		cmd,
		sceneColor.image,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
}

void TransitionTaaHistoryForSample(
	const VkCommandBuffer cmd,
	const OffscreenColorTarget &historyColor)
{
	if (historyColor.image == VK_NULL_HANDLE) {
		return;
	}
	TransitionImageLayout(
		cmd,
		historyColor.image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_NONE,
		0,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
}

void RecordTaaHistoryCopy(
	const VkCommandBuffer cmd,
	const OffscreenColorTarget &sceneColor,
	const OffscreenColorTarget &historyColor,
	VkExtent2D extent)
{
	if (sceneColor.image == VK_NULL_HANDLE || historyColor.image == VK_NULL_HANDLE) {
		return;
	}

	// Both images must be in `TRANSFER_*` for `vkCmdCopyImage` to read
	// from the source and write to the destination. The caller is
	// expected to have just placed the scene colour in the sample-read
	// state (resolve output). We leave the history image in
	// `TRANSFER_DST_OPTIMAL` after the copy; the next frame's
	// `TransitionTaaHistoryForSample` will move it to
	// `SHADER_READ_ONLY_OPTIMAL` again.
	TransitionImageLayout(
		cmd,
		sceneColor.image,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_READ_BIT);
	TransitionImageLayout(
		cmd,
		historyColor.image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_2_NONE,
		0,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_WRITE_BIT);

	const VkImageCopy copyRegion{};
	// The default-initialised `VkImageCopy` is already correct for a
	// 2D full-color copy of the swapchain-sized area. We set the
	// subresource explicitly to keep the intent obvious in a code
	// review and to make the helper robust to any future change in the
	// struct's zero-initialised field semantics.
	VkImageCopy region{};
	region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
	region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
	region.srcOffset = {0, 0, 0};
	region.dstOffset = {0, 0, 0};
	region.extent = {extent.width, extent.height, 1u};

	vkCmdCopyImage(
		cmd,
		sceneColor.image,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		historyColor.image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region);

	// Move the history image into the read state for the *next* frame's
	// resolve pass.
	TransitionImageLayout(
		cmd,
		historyColor.image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
}

} // namespace projectv::taa

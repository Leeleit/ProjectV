#include "render/TaaRenderTargets.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include "vk_mem_alloc.h"

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
		0,
		1,
		0,
		1,
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

std::expected<void, TaaError> CreateOrRecreateTaaRenderTargets(
	VulkanContextState *context,
	const VkExtent2D extent,
	OffscreenColorTarget &sceneColor,
	OffscreenColorTarget &historyColor,
	OffscreenColorTarget &layerSceneColor,
	OffscreenColorTarget &layerHistoryColor,
	OffscreenColorTarget &motionVectorColor,
	OffscreenColorTarget &motionVectorHistoryColor,
	VkSampler &linearSampler)
{
	const auto fail = [](const TaaError e, const std::string_view step, const std::string_view detail) {
		runtime::LogRuntimeFailure("Taa", step, detail);
		return std::unexpected(e);
	};
	if (!context || !context->allocator || context->device == VK_NULL_HANDLE) {
		return fail(TaaError::PreconditionFailed,
					"CreateOrRecreateTaaRenderTargets.Preconditions", "context/allocator/device null");
	}
	if (extent.width == 0u || extent.height == 0u) {
		return fail(TaaError::PreconditionFailed,
					"CreateOrRecreateTaaRenderTargets.Preconditions", "extent is zero-sized");
	}

	DestroyTaaRenderTargets(
		context,
		sceneColor,
		historyColor,
		layerSceneColor,
		layerHistoryColor,
		motionVectorColor,
		motionVectorHistoryColor,
		linearSampler);

	const VkExtent3D imageExtent{extent.width, extent.height, 1u};

	constexpr VkFormat sceneColorFormat = kTaaSceneColorFormat;
	constexpr VkFormat layerColorFormat = kTaaLayerHistoryColorFormat;
	constexpr VkFormat motionVectorFormat = kTaaMotionVectorFormat;

	const VkImageCreateInfo imageInfoTemplate{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_UNDEFINED,
		.extent = imageExtent,
		.mipLevels = 1u,
		.arrayLayers = 1u,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage =
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
			VK_IMAGE_USAGE_SAMPLED_BIT |
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocationInfo.flags = 0;

	// All targets in this function are single-sample (the multi-sampled render
	// attachment is created separately in `VulkanSwapchain.cpp::RecreateSwapchain`).
	auto allocateTarget = [&](
							  OffscreenColorTarget &target,
							  const VkFormat format,
							  const char *name,
							  const VkImageLayout initialLayout) -> std::expected<void, TaaError> {
		VkImageCreateInfo imageInfo = imageInfoTemplate;
		imageInfo.format = format;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.initialLayout = initialLayout;
		target.samples = 1u;
		VmaAllocation allocation = nullptr;
		if (vmaCreateImage(
				context->allocator,
				&imageInfo,
				&allocationInfo,
				&target.image,
				&allocation,
				nullptr) != VK_SUCCESS) {
			return fail(TaaError::ImageCreateFailed,
						"CreateOrRecreateTaaRenderTargets.vmaCreateImage", name);
		}
		target.allocation = allocation;
		SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(target.image), VK_OBJECT_TYPE_IMAGE, name);

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = target.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.components = {
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
		};
		viewInfo.subresourceRange = {
			VK_IMAGE_ASPECT_COLOR_BIT,
			0,
			1,
			0,
			1,
		};
		if (vkCreateImageView(context->device, &viewInfo, nullptr, &target.imageView) != VK_SUCCESS) {
			return fail(TaaError::ImageViewCreateFailed,
						"CreateOrRecreateTaaRenderTargets.vkCreateImageView", name);
		}
		SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(target.imageView), VK_OBJECT_TYPE_IMAGE_VIEW, name);
		return {};
	};

	if (!allocateTarget(sceneColor, sceneColorFormat, "TaaSceneColorImage", VK_IMAGE_LAYOUT_UNDEFINED).has_value()) {
		DestroyTaaRenderTargets(
			context,
			sceneColor,
			historyColor,
			layerSceneColor,
			layerHistoryColor,
			motionVectorColor,
			motionVectorHistoryColor,
			linearSampler);
		return std::unexpected(TaaError::ImageCreateFailed);
	}
	if (!allocateTarget(historyColor, sceneColorFormat, "TaaHistoryColorImage", VK_IMAGE_LAYOUT_UNDEFINED).has_value()) {
		DestroyTaaRenderTargets(
			context,
			sceneColor,
			historyColor,
			layerSceneColor,
			layerHistoryColor,
			motionVectorColor,
			motionVectorHistoryColor,
			linearSampler);
		return std::unexpected(TaaError::ImageCreateFailed);
	}
	if (!allocateTarget(layerSceneColor, layerColorFormat, "TaaLayerSceneColorImage", VK_IMAGE_LAYOUT_UNDEFINED).has_value()) {
		DestroyTaaRenderTargets(
			context,
			sceneColor,
			historyColor,
			layerSceneColor,
			layerHistoryColor,
			motionVectorColor,
			motionVectorHistoryColor,
			linearSampler);
		return std::unexpected(TaaError::ImageCreateFailed);
	}
	if (!allocateTarget(layerHistoryColor, layerColorFormat, "TaaLayerHistoryColorImage", VK_IMAGE_LAYOUT_UNDEFINED).has_value()) {
		DestroyTaaRenderTargets(
			context,
			sceneColor,
			historyColor,
			layerSceneColor,
			layerHistoryColor,
			motionVectorColor,
			motionVectorHistoryColor,
			linearSampler);
		return std::unexpected(TaaError::ImageCreateFailed);
	}
	if (!allocateTarget(motionVectorColor, motionVectorFormat, "TaaMotionVectorImage", VK_IMAGE_LAYOUT_UNDEFINED).has_value()) {
		DestroyTaaRenderTargets(
			context,
			sceneColor,
			historyColor,
			layerSceneColor,
			layerHistoryColor,
			motionVectorColor,
			motionVectorHistoryColor,
			linearSampler);
		return std::unexpected(TaaError::ImageCreateFailed);
	}
	if (!allocateTarget(motionVectorHistoryColor, motionVectorFormat, "TaaMotionVectorHistoryImage", VK_IMAGE_LAYOUT_UNDEFINED).has_value()) {
		DestroyTaaRenderTargets(
			context,
			sceneColor,
			historyColor,
			layerSceneColor,
			layerHistoryColor,
			motionVectorColor,
			motionVectorHistoryColor,
			linearSampler);
		return std::unexpected(TaaError::ImageCreateFailed);
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
		DestroyTaaRenderTargets(
			context,
			sceneColor,
			historyColor,
			layerSceneColor,
			layerHistoryColor,
			motionVectorColor,
			motionVectorHistoryColor,
			linearSampler);
		return fail(TaaError::SamplerCreateFailed,
					"CreateOrRecreateTaaRenderTargets.vkCreateSampler", "TaaLinearSampler");
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(linearSampler), VK_OBJECT_TYPE_SAMPLER, "TaaLinearSampler");

	return {};
}

void DestroyTaaRenderTargets(
	VulkanContextState *context,
	OffscreenColorTarget &sceneColor,
	OffscreenColorTarget &historyColor,
	OffscreenColorTarget &layerSceneColor,
	OffscreenColorTarget &layerHistoryColor,
	OffscreenColorTarget &motionVectorColor,
	OffscreenColorTarget &motionVectorHistoryColor,
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
	DestroyTarget(context, layerSceneColor);
	DestroyTarget(context, layerHistoryColor);
	DestroyTarget(context, motionVectorColor);
	DestroyTarget(context, motionVectorHistoryColor);
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

void TransitionTaaMotionVectorForSample(
	const VkCommandBuffer cmd,
	const OffscreenColorTarget &motionVectorColor)
{
	if (motionVectorColor.image == VK_NULL_HANDLE) {
		return;
	}
	TransitionImageLayout(
		cmd,
		motionVectorColor.image,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
}

void TransitionTaaMotionVectorForWrite(
	const VkCommandBuffer cmd,
	const OffscreenColorTarget &motionVectorColor,
	const VkImageLayout oldLayout)
{
	if (motionVectorColor.image == VK_NULL_HANDLE) {
		return;
	}
	if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
		return;
	}
	TransitionImageLayout(
		cmd,
		motionVectorColor.image,
		oldLayout,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
			? VK_PIPELINE_STAGE_2_NONE
			: VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ? 0
											   : VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
}

void RecordTaaHistoryCopy(
	const VkCommandBuffer cmd,
	const OffscreenColorTarget &sceneColor,
	const OffscreenColorTarget &historyColor,
	const VkExtent2D extent)
{
	if (sceneColor.image == VK_NULL_HANDLE || historyColor.image == VK_NULL_HANDLE) {
		return;
	}

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

void RecordTaaMotionVectorHistoryCopy(
	const VkCommandBuffer cmd,
	const OffscreenColorTarget &motionVectorColor,
	const OffscreenColorTarget &motionVectorHistoryColor,
	const VkExtent2D extent)
{
	if (motionVectorColor.image == VK_NULL_HANDLE || motionVectorHistoryColor.image == VK_NULL_HANDLE) {
		return;
	}

	TransitionImageLayout(
		cmd,
		motionVectorColor.image,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_READ_BIT);
	TransitionImageLayout(
		cmd,
		motionVectorHistoryColor.image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_2_NONE,
		0,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_WRITE_BIT);

	VkImageCopy region{};
	region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
	region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
	region.srcOffset = {0, 0, 0};
	region.dstOffset = {0, 0, 0};
	region.extent = {extent.width, extent.height, 1u};

	vkCmdCopyImage(
		cmd,
		motionVectorColor.image,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		motionVectorHistoryColor.image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region);

	TransitionImageLayout(
		cmd,
		motionVectorHistoryColor.image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
}

} // namespace projectv::taa

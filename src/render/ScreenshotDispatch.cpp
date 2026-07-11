#include "render/ScreenshotDispatch.hpp"

#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"
#include "render/ScreenshotCapture.hpp"

#include <filesystem>

namespace projectv::render {

namespace {

bool ShouldCaptureScreenshot(const RenderState &render)
{
	return render.screenshotCaptureRequested &&
		   render.screenshotCaptureSupported &&
		   render.screenshotReadbackBuffer != VK_NULL_HANDLE &&
		   render.screenshotReadbackAllocation != VK_NULL_HANDLE &&
		   render.screenshotReadbackMappedData != nullptr;
}

void TransitionImage(
	const VkCommandBuffer cmd,
	const VkImage image,
	const VkImageAspectFlags aspectMask,
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
	imageBarrier.subresourceRange = {aspectMask, 0, 1, 0, 1u};

	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2(cmd, &depInfo);
}

} // namespace

void RecordSwapchainScreenshotCopy(
	const SwapchainState &swapchain,
	RenderState &render,
	const VkCommandBuffer cmd,
	const uint32_t imageIndex)
{
	PV_PROFILE_ZONE_N("RecordSwapchainScreenshotCopy");
	if (!ShouldCaptureScreenshot(render) || imageIndex >= swapchain.images.size()) {
		return;
	}

	TransitionImage(
		cmd,
		swapchain.images[imageIndex],
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_READ_BIT);

	const VkBufferImageCopy copyRegion{
		.bufferOffset = 0u,
		.bufferRowLength = 0u,
		.bufferImageHeight = 0u,
		.imageSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0u,
			.baseArrayLayer = 0u,
			.layerCount = 1u,
		},
		.imageOffset = {0, 0, 0},
		.imageExtent = {swapchain.extent.width, swapchain.extent.height, 1u},
	};
	vkCmdCopyImageToBuffer(
		cmd,
		swapchain.images[imageIndex],
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		render.screenshotReadbackBuffer,
		1,
		&copyRegion);

	TransitionImage(
		cmd,
		swapchain.images[imageIndex],
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_READ_BIT,
		VK_PIPELINE_STAGE_2_NONE,
		0);
}

bool SaveRequestedScreenshot(
	VulkanContextState &context,
	RenderState &render,
	const VkFence inFlightFence,
	const VkExtent2D captureExtent,
	const VkFormat captureFormat)
{
	PV_PROFILE_ZONE_N("SaveRequestedScreenshot");
	if (!ShouldCaptureScreenshot(render)) {
		return true;
	}

	VkResult waitResult = vkWaitForFences(
		context.device, 1, &inFlightFence, VK_TRUE, kVulkanFenceWaitTimeoutNs);
	if (waitResult == VK_TIMEOUT) {
		waitResult = vkWaitForFences(
			context.device, 1, &inFlightFence, VK_TRUE, kVulkanFenceWaitTimeoutUnboundedNs);
	}
	if (waitResult != VK_SUCCESS) {
		runtime::LogVkFailure("DrawFrame.ScreenshotWaitFence", waitResult);
		render.screenshotCaptureRequested = false;
		return false;
	}

	const uint64_t requiredSize =
		static_cast<uint64_t>(captureExtent.width) *
		static_cast<uint64_t>(captureExtent.height) *
		4u;
	if (requiredSize > render.screenshotReadbackBufferSize) {
		runtime::LogRuntimeFailure(
			"Capture",
			"DrawFrame.ScreenshotBufferSize",
			"screenshot readback buffer is smaller than the captured frame");
		render.screenshotCaptureRequested = false;
		return true;
	}
	const VkResult invalidateResult =
		vmaInvalidateAllocation(context.allocator, render.screenshotReadbackAllocation, 0u, requiredSize);
	if (invalidateResult != VK_SUCCESS) {
		runtime::LogVmaFailure("DrawFrame.ScreenshotInvalidate", invalidateResult);
		render.screenshotCaptureRequested = false;
		return true;
	}

	const std::filesystem::path screenshotPath =
		BuildScreenshotCapturePath(render.currentScenePreset, ++render.screenshotCaptureSequence);
	const std::filesystem::path metadataPath = BuildScreenshotCaptureMetadataPath(screenshotPath.string());
	const bool savedImage = SaveScreenshotCaptureBmp(
		render.screenshotReadbackMappedData,
		captureExtent.width,
		captureExtent.height,
		captureFormat,
		screenshotPath.string());
	const bool savedMetadata = savedImage &&
							   SaveScreenshotCaptureMetadata(
								   render,
								   render.currentScenePreset,
								   screenshotPath.string(),
								   metadataPath.string());
	if (savedImage && savedMetadata) {
		SDL_Log(
			"[ProjectV][Capture][SaveRequestedScreenshot] saved image=%s metadata=%s",
			screenshotPath.string().c_str(),
			metadataPath.string().c_str());
	}

	render.screenshotCaptureRequested = false;
	return true;
}

} // namespace projectv::render
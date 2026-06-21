import projectv.math;

#include "render/Renderer.hpp"

#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"
#include "debug/ProfilingGpu.hpp"
#include "render/ScreenshotCapture.hpp"
#include "render/TaaRenderTargets.hpp"
#include "render/vulkan/VulkanInit.hpp"
#include "render/vulkan/VulkanMeshShaderPipeline.hpp"
#include "render/vulkan/VulkanFluidCaPipeline.hpp"
#include "render/vulkan/VulkanWorldGenPipeline.hpp"
#include "render/vulkan/VulkanAsyncCompute.hpp"
#include "render/vulkan/VulkanResult.hpp"
#include "voxel/VoxelMaterials.hpp"

#include "fmt/format.h"

#include <filesystem>

namespace {
class ScopedPassTimer {
  public:
	explicit ScopedPassTimer(float &outMs)
		: outMs_(outMs), start_(SDL_GetPerformanceCounter()) {}

	~ScopedPassTimer()
	{
		const Uint64 end = SDL_GetPerformanceCounter();
		const Uint64 freq = SDL_GetPerformanceFrequency();
		const double seconds = static_cast<double>(end - start_) / static_cast<double>(freq);
		outMs_ = static_cast<float>(seconds * 1000.0);
	}

	ScopedPassTimer(const ScopedPassTimer &) = delete;
	ScopedPassTimer &operator=(const ScopedPassTimer &) = delete;

  private:
	float &outMs_;
	Uint64 start_;
};

DebugOverlayPushConstants BuildBoxOverlayPushConstants(
	const FrameRenderData &frameRenderData,
	const DebugOverlayBox &box)
{
	DebugOverlayPushConstants pushConstants{};
	pushConstants.viewProjection = frameRenderData.graphicsPushConstants.viewProjection;
	pushConstants.overlayData0 = {
		static_cast<float>(box.min.x) - 0.01f,
		static_cast<float>(box.min.y) - 0.01f,
		static_cast<float>(box.min.z) - 0.01f,
		0.0f,
	};
	pushConstants.overlayData1 = {
		static_cast<float>(box.maxExclusive.x) + 0.01f,
		static_cast<float>(box.maxExclusive.y) + 0.01f,
		static_cast<float>(box.maxExclusive.z) + 0.01f,
		0.0f,
	};
	pushConstants.overlayColor = box.color;
	return pushConstants;
}

DebugOverlayPushConstants BuildCrosshairOverlayPushConstants(const SwapchainState &swapchain)
{
	DebugOverlayPushConstants pushConstants{};
	const float halfWidthNdc = 9.0f * 2.0f / static_cast<float>(swapchain.extent.width);
	const float halfHeightNdc = 9.0f * 2.0f / static_cast<float>(swapchain.extent.height);
	const float halfThicknessXNdc = 1.5f * 2.0f / static_cast<float>(swapchain.extent.width);
	const float halfThicknessYNdc = 1.5f * 2.0f / static_cast<float>(swapchain.extent.height);
	pushConstants.overlayData0 = {
		halfWidthNdc,
		halfHeightNdc,
		halfThicknessXNdc,
		1.0f,
	};
	pushConstants.overlayData1 = {halfThicknessYNdc, 0.0f, 0.0f, 0.0f};
	pushConstants.overlayColor = {1.0f, 1.0f, 1.0f, 1.0f};
	return pushConstants;
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
	const VkAccessFlags2 dstAccessMask,
	const uint32_t layerCount = 1u)
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
	imageBarrier.subresourceRange = {aspectMask, 0, 1, 0, layerCount};

	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2(cmd, &depInfo);
}

bool HasShadowCascadeImageViews(const RenderState &render)
{
	for (const VkImageView cascadeImageView : render.shadowCascadeImageViews) {
		if (cascadeImageView == VK_NULL_HANDLE) {
			return false;
		}
	}
	return true;
}

bool ShouldCaptureScreenshot(const RenderState &render)
{
	return render.screenshotCaptureRequested &&
		   render.screenshotCaptureSupported &&
		   render.screenshotReadbackBuffer != VK_NULL_HANDLE &&
		   render.screenshotReadbackAllocation != VK_NULL_HANDLE &&
		   render.screenshotReadbackMappedData != nullptr;
}

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

void RecordShadowCommands(
	RenderState &render,
	const FrameRenderData &frameRenderData,
	const VkCommandBuffer cmd)
{
	ScopedPassTimer passTimer(render.renderPassTimings.shadowMs);
	PV_PROFILE_ZONE_N("RecordShadowCommands");
	PV_PROFILE_GPU_LABEL(cmd, "Shadow Pass");
	if (render.shadowGraphicsPipeline == VK_NULL_HANDLE ||
		render.shadowPipelineLayout == VK_NULL_HANDLE ||
		render.shadowImage == VK_NULL_HANDLE ||
		render.shadowImageView == VK_NULL_HANDLE ||
		!HasShadowCascadeImageViews(render)) {
		return;
	}

	const VkImageLayout oldShadowLayout =
		render.shadowImageNeedsInit ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
	const VkPipelineStageFlags2 oldShadowStage =
		render.shadowImageNeedsInit ? VK_PIPELINE_STAGE_2_NONE : VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	const VkAccessFlags2 oldShadowAccess =
		render.shadowImageNeedsInit ? 0 : VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	TransitionImage(
		cmd,
		render.shadowImage,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		oldShadowLayout,
		VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		oldShadowStage,
		oldShadowAccess,
		VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		kSunShadowCascadeCount);
	render.shadowImageNeedsInit = false;

	constexpr VkClearValue clearDepthValue{.depthStencil = {1.0f, 0}};
	const VkViewport shadowViewport{
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(render.shadowMapExtent.width),
		.height = static_cast<float>(render.shadowMapExtent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	const VkRect2D shadowScissor{
		.offset = {0, 0},
		.extent = render.shadowMapExtent,
	};

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render.shadowGraphicsPipeline);
	if (frameRenderData.shadowDescriptorSet != VK_NULL_HANDLE) {
		vkCmdBindDescriptorSets(
			cmd,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			render.shadowPipelineLayout,
			0,
			1,
			&frameRenderData.shadowDescriptorSet,
			0,
			nullptr);
	}

	for (uint32_t cascadeIndex = 0; cascadeIndex < kSunShadowCascadeCount; ++cascadeIndex) {
		const VkRenderingAttachmentInfo shadowDepthAttachment{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = nullptr,
			.imageView = render.shadowCascadeImageViews[cascadeIndex],
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = clearDepthValue,
		};
		const VkRenderingInfo shadowRenderingInfo{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderArea = {{0, 0}, render.shadowMapExtent},
			.layerCount = 1,
			.viewMask = 0,
			.colorAttachmentCount = 0,
			.pColorAttachments = nullptr,
			.pDepthAttachment = &shadowDepthAttachment,
			.pStencilAttachment = nullptr,
		};

		vkCmdBeginRendering(cmd, &shadowRenderingInfo);
		vkCmdSetViewport(cmd, 0, 1, &shadowViewport);
		vkCmdSetScissor(cmd, 0, 1, &shadowScissor);

		const ShadowPushConstants shadowPushConstants{
			.cascadeIndex = cascadeIndex,
		};
		vkCmdPushConstants(
			cmd,
			render.shadowPipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT,
			0,
			sizeof(shadowPushConstants),
			&shadowPushConstants);

		const bool canSkipEmptyCascadeDraw =
			frameRenderData.dirtyChunkCount == 0 &&
			frameRenderData.shadowCascadeVisibleChunkCounts[cascadeIndex] == 0u;
		if (frameRenderData.shadowDescriptorSet != VK_NULL_HANDLE &&
			frameRenderData.shadowIndirectCommandCount > 0 &&
			!canSkipEmptyCascadeDraw &&
			frameRenderData.shadowIndirectBuffer != VK_NULL_HANDLE &&
			frameRenderData.packedFaceBuffer != VK_NULL_HANDLE) {
			PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Shadow Cascade");

			const VkDeviceSize shadowIndirectOffset =
				static_cast<VkDeviceSize>(cascadeIndex) *
				static_cast<VkDeviceSize>(frameRenderData.shadowIndirectCommandCount) *
				sizeof(VkDrawIndirectCommand);
			vkCmdDrawIndirect(
				cmd,
				frameRenderData.shadowIndirectBuffer,
				shadowIndirectOffset,
				frameRenderData.shadowIndirectCommandCount,
				sizeof(VkDrawIndirectCommand));
		}

		vkCmdEndRendering(cmd);
	}

	TransitionImage(
		cmd,
		render.shadowImage,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
		kSunShadowCascadeCount);
}

void RecordDebugOverlayCommands(
	RenderState &render,
	const SwapchainState &swapchain,
	const FrameRenderData &frameRenderData,
	const VkCommandBuffer cmd)
{
	ScopedPassTimer passTimer(render.renderPassTimings.debugOverlayMs);
	PV_PROFILE_ZONE_N("RecordDebugOverlayCommands");
	PV_PROFILE_GPU_LABEL(cmd, "Debug Overlay");
	if (!frameRenderData.debugUiVisible || render.debugOverlayPipelineLayout == VK_NULL_HANDLE) {
		return;
	}

	if (render.debugOverlayPipeline != VK_NULL_HANDLE &&
		!frameRenderData.debugOverlayBoxes.empty()) {
		PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Debug Overlay");
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render.debugOverlayPipeline);
		for (const DebugOverlayBox &box : frameRenderData.debugOverlayBoxes) {
			const DebugOverlayPushConstants pushConstants = BuildBoxOverlayPushConstants(frameRenderData, box);
			vkCmdPushConstants(
				cmd,
				render.debugOverlayPipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(pushConstants),
				&pushConstants);
			vkCmdDraw(cmd, 24, 1, 0, 0);
		}
	}

	if (render.debugCrosshairPipeline != VK_NULL_HANDLE &&
		swapchain.extent.width > 0 &&
		swapchain.extent.height > 0) {
		PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Crosshair Overlay");
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render.debugCrosshairPipeline);
		const DebugOverlayPushConstants pushConstants = BuildCrosshairOverlayPushConstants(swapchain);
		vkCmdPushConstants(
			cmd,
			render.debugOverlayPipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0,
			sizeof(pushConstants),
			&pushConstants);
		vkCmdDraw(cmd, 18, 1, 0, 0);
	}
}

void RecordDebugHudCommands(
	RenderState &render,
	const FrameRenderData &frameRenderData,
	const VkCommandBuffer cmd)
{
	ScopedPassTimer passTimer(render.renderPassTimings.debugHudMs);
	PV_PROFILE_ZONE_N("RecordDebugHudCommands");
	PV_PROFILE_GPU_LABEL(cmd, "Debug HUD");
	if (render.debugHudPipeline == VK_NULL_HANDLE ||
		frameRenderData.debugHudVertexBuffer == VK_NULL_HANDLE ||
		frameRenderData.debugHudVertexCount == 0) {
		return;
	}

	PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Debug HUD");
	constexpr VkDeviceSize vertexBufferOffset = 0;
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render.debugHudPipeline);
	vkCmdBindVertexBuffers(cmd, 0, 1, &frameRenderData.debugHudVertexBuffer, &vertexBufferOffset);
	vkCmdDraw(cmd, frameRenderData.debugHudVertexCount, 1, 0, 0);
}

void RecordVoxelMeshingCommands(
	RenderState &render,
	const FrameRenderData &frameRenderData,
	const VkCommandBuffer cmd)
{
	ScopedPassTimer passTimer(render.renderPassTimings.meshingMs);

	render.renderPassTimings.dirtyChunkRebuiltCount = frameRenderData.dirtyChunkCount;
	PV_PROFILE_ZONE_N("RecordVoxelMeshingCommands");
	PV_PROFILE_GPU_LABEL(cmd, "Voxel Meshing");
	if (render.voxelMeshingPipeline == VK_NULL_HANDLE ||
		render.voxelMeshingPipelineLayout == VK_NULL_HANDLE ||
		frameRenderData.voxelMeshingDescriptorSet == VK_NULL_HANDLE ||
		frameRenderData.packedFaceBuffer == VK_NULL_HANDLE ||
		frameRenderData.opaqueIndirectBuffer == VK_NULL_HANDLE ||
		frameRenderData.shadowIndirectBuffer == VK_NULL_HANDLE ||
		frameRenderData.transparentIndirectBuffer == VK_NULL_HANDLE ||
		frameRenderData.dirtyChunkCount == 0) {
		return;
	}

	PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Voxel Meshing");

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, render.voxelMeshingPipeline);
	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		render.voxelMeshingPipelineLayout,
		0,
		1,
		&frameRenderData.voxelMeshingDescriptorSet,
		0,
		nullptr);
	vkCmdPushConstants(
		cmd,
		render.voxelMeshingPipelineLayout,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0,
		sizeof(frameRenderData.voxelMeshingPushConstants),
		&frameRenderData.voxelMeshingPushConstants);
	vkCmdDispatch(cmd, frameRenderData.dirtyChunkCount, 1, 1);

	VkBufferMemoryBarrier2 bufferBarriers[4]{};
	bufferBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	bufferBarriers[0].srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	bufferBarriers[0].srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	bufferBarriers[0].dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
	bufferBarriers[0].dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	bufferBarriers[0].buffer = frameRenderData.packedFaceBuffer;
	bufferBarriers[0].offset = 0;
	bufferBarriers[0].size = VK_WHOLE_SIZE;

	bufferBarriers[1] = bufferBarriers[0];
	bufferBarriers[1].dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
	bufferBarriers[1].dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
	bufferBarriers[1].buffer = frameRenderData.opaqueIndirectBuffer;

	bufferBarriers[2] = bufferBarriers[0];
	bufferBarriers[2].dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
	bufferBarriers[2].dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
	bufferBarriers[2].buffer = frameRenderData.shadowIndirectBuffer;

	bufferBarriers[3] = bufferBarriers[0];
	bufferBarriers[3].dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
	bufferBarriers[3].dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
	bufferBarriers[3].buffer = frameRenderData.transparentIndirectBuffer;

	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.bufferMemoryBarrierCount = 4;
	depInfo.pBufferMemoryBarriers = bufferBarriers;
	vkCmdPipelineBarrier2(cmd, &depInfo);
}

void RecordGraphicsCommands(
	RenderState &render,
	const SwapchainState &swapchain,
	const FrameRenderData &frameRenderData,
	const VkCommandBuffer cmd,
	const uint32_t imageIndex)
{
	ScopedPassTimer passTimer(render.renderPassTimings.graphicsMs);
	PV_PROFILE_ZONE_N("RecordGraphicsCommands");
	PV_PROFILE_GPU_LABEL(cmd, "Graphics Pass");
	{
		PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Graphics Frame");

		RecordVoxelMeshingCommands(render, frameRenderData, cmd);
		RecordShadowCommands(render, frameRenderData, cmd);

		const bool taaOn = render.taaEnabled &&
						   render.taaSceneColorTarget != nullptr && render.taaHistoryColorTarget != nullptr &&
						   render.taaResolvePipeline != VK_NULL_HANDLE && render.taaResolvePipelineLayout != VK_NULL_HANDLE;

		if (!taaOn) {
			TransitionImage(
				cmd,
				swapchain.images[imageIndex],
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_2_NONE,
				0,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
		} else {
			const VkImageLayout oldSceneLayout = render.taaSceneColorCurrentLayout;
			const VkPipelineStageFlags2 oldSceneStage =
				oldSceneLayout == VK_IMAGE_LAYOUT_UNDEFINED
					? VK_PIPELINE_STAGE_2_NONE
					: VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
			const VkAccessFlags2 oldSceneAccess =
				oldSceneLayout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
			TransitionImage(
				cmd,
				render.taaSceneColorTarget->image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				oldSceneLayout,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				oldSceneStage,
				oldSceneAccess,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
			render.taaSceneColorCurrentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			render.taaSceneColorNeedsInit = false;
		}

		{
			const VkImageLayout oldLayerSceneLayout = render.taaLayerSceneColorCurrentLayout;
			const VkPipelineStageFlags2 oldLayerSceneStage =
				oldLayerSceneLayout == VK_IMAGE_LAYOUT_UNDEFINED
					? VK_PIPELINE_STAGE_2_NONE
					: VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
			const VkAccessFlags2 oldLayerSceneAccess =
				oldLayerSceneLayout == VK_IMAGE_LAYOUT_UNDEFINED
					? 0
					: VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
			TransitionImage(
				cmd,
				render.taaLayerSceneColorTarget->image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				oldLayerSceneLayout,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				oldLayerSceneStage,
				oldLayerSceneAccess,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
			render.taaLayerSceneColorCurrentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		{
			const VkImageLayout oldLayerHistoryLayout = render.taaLayerHistoryColorCurrentLayout;
			const VkPipelineStageFlags2 oldLayerHistoryStage =
				oldLayerHistoryLayout == VK_IMAGE_LAYOUT_UNDEFINED
					? VK_PIPELINE_STAGE_2_NONE
					: VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
			const VkAccessFlags2 oldLayerHistoryAccess =
				oldLayerHistoryLayout == VK_IMAGE_LAYOUT_UNDEFINED
					? 0
					: VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
			TransitionImage(
				cmd,
				render.taaLayerHistoryColorTarget->image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				oldLayerHistoryLayout,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				oldLayerHistoryStage,
				oldLayerHistoryAccess,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
			render.taaLayerHistoryColorCurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		const VkImageLayout oldDepthLayout = render.depthImageCurrentLayout;
		const VkPipelineStageFlags2 oldDepthStage =
			oldDepthLayout == VK_IMAGE_LAYOUT_UNDEFINED
				? VK_PIPELINE_STAGE_2_NONE
			: oldDepthLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
				? VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
				: VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		const VkAccessFlags2 oldDepthAccess =
			oldDepthLayout == VK_IMAGE_LAYOUT_UNDEFINED
				? 0
			: oldDepthLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
				? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
				: VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
		TransitionImage(
			cmd,
			render.depthImage,
			VK_IMAGE_ASPECT_DEPTH_BIT,
			oldDepthLayout,
			VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			oldDepthStage,
			oldDepthAccess,
			VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
			VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
		render.depthImageCurrentLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		render.depthImageNeedsInit = false;

		const std::array<float, 4> sceneClearColor = GetVoxelSceneClearColor(render.currentSceneLighting);
		VkClearValue clearColorValue{};
		clearColorValue.color = {
			{
				sceneClearColor[0],
				sceneClearColor[1],
				sceneClearColor[2],
				sceneClearColor[3],
			},
		};
		constexpr VkClearValue clearDepthValue{.depthStencil = {1.0f, 0}};

		const VkImageView mainColor0View = taaOn ? VK_NULL_HANDLE : swapchain.imageViews[imageIndex];
		const VkImageView mainColor1View = taaOn ? render.taaSceneColorTarget->imageView : VK_NULL_HANDLE;
		const VkImageView mainColor2View = render.taaLayerSceneColorTarget != nullptr
											   ? render.taaLayerSceneColorTarget->imageView
											   : VK_NULL_HANDLE;
		const VkImageView mainColor3View = render.taaMotionVectorTarget != nullptr
											   ? render.taaMotionVectorTarget->imageView
											   : VK_NULL_HANDLE;

		const VkRenderingAttachmentInfo colorAttachment0{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = nullptr,
			.imageView = mainColor0View,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = mainColor0View != VK_NULL_HANDLE ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.clearValue = clearColorValue,
		};
		const VkRenderingAttachmentInfo colorAttachment1{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = nullptr,
			.imageView = mainColor1View,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = mainColor1View != VK_NULL_HANDLE ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.clearValue = clearColorValue,
		};
		const VkRenderingAttachmentInfo colorAttachment2{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = nullptr,
			.imageView = mainColor2View,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = mainColor2View != VK_NULL_HANDLE ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.clearValue = clearColorValue,
		};
		const VkRenderingAttachmentInfo colorAttachment3{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = nullptr,
			.imageView = mainColor3View,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = mainColor3View != VK_NULL_HANDLE ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.clearValue = clearColorValue,
		};
		const VkRenderingAttachmentInfo colorAttachments[4] = {colorAttachment0, colorAttachment1, colorAttachment2, colorAttachment3};
		const VkRenderingAttachmentInfo depthAttachment{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = nullptr,
			.imageView = render.depthImageView,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.clearValue = clearDepthValue,
		};
		const VkRenderingInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderArea = {{0, 0}, swapchain.extent},
			.layerCount = 1,
			.viewMask = 0,
			.colorAttachmentCount = 4,
			.pColorAttachments = colorAttachments,
			.pDepthAttachment = &depthAttachment,
			.pStencilAttachment = nullptr,
		};

		if (render.taaMotionVectorTarget != nullptr && render.taaMotionVectorCurrentLayout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
			projectv::taa::TransitionTaaMotionVectorForWrite(
				cmd,
				*render.taaMotionVectorTarget,
				render.taaMotionVectorCurrentLayout);
			render.taaMotionVectorCurrentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		vkCmdBeginRendering(cmd, &renderingInfo);

		const VkViewport viewport{
			.x = 0.0f,
			.y = 0.0f,
			.width = static_cast<float>(swapchain.extent.width),
			.height = static_cast<float>(swapchain.extent.height),
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		};
		const VkRect2D scissor{
			.offset = {0, 0},
			.extent = swapchain.extent,
		};
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		if (frameRenderData.graphicsDescriptorSet != VK_NULL_HANDLE) {
			vkCmdBindDescriptorSets(
				cmd,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				render.graphicsPipelineLayout,
				0,
				1,
				&frameRenderData.graphicsDescriptorSet,
				0,
				nullptr);
		}

		if (frameRenderData.graphicsDescriptorSet != VK_NULL_HANDLE &&
			frameRenderData.chunkDescriptorCount > 0 &&
			frameRenderData.opaqueIndirectBuffer != VK_NULL_HANDLE &&
			frameRenderData.packedFaceBuffer != VK_NULL_HANDLE) {
			PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Opaque Pass");
			if (render.meshShaderEnabled) {
				projectv::render::MeshDrawPushConstants meshDrawPush{};
				const auto &viewProj = frameRenderData.graphicsPushConstants.viewProjection;
				for (size_t i = 0; i < 16; ++i) {
					meshDrawPush.viewProjection[i] = viewProj.data()[i];
				}
				meshDrawPush.worldMinAndChunkSize = frameRenderData.voxelMeshingPushConstants.worldMinAndChunkSize;
				meshDrawPush.worldMaxExclusiveAndChunkCount = frameRenderData.voxelMeshingPushConstants.worldMaxExclusiveAndChunkCount;
				meshDrawPush.chunkGridAndTransparentFaceBase = frameRenderData.voxelMeshingPushConstants.chunkGridAndTransparentFaceBase;
				meshDrawPush.faceCapacities = frameRenderData.voxelMeshingPushConstants.faceCapacities;
				projectv::render::RecordMeshShaderDraw(
					cmd,
					render,
					render.sceneFrameResources[frameRenderData.frameIndex],
					meshDrawPush,
					frameRenderData.chunkDescriptorCount);
			} else {
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, taaOn ? render.graphicsPipelineTaaOn : render.graphicsPipeline);
				vkCmdPushConstants(
					cmd,
					render.graphicsPipelineLayout,
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
					0,
					sizeof(frameRenderData.graphicsPushConstants),
					&frameRenderData.graphicsPushConstants);
				const bool hzbCullingActive =
					projectv::render::IsHzbCullingEnabled() &&
					frameRenderData.hzbVisibleCountBuffer != VK_NULL_HANDLE;
				if (hzbCullingActive) {
					VkBufferMemoryBarrier2 indirectBarrier{};
					indirectBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
					indirectBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
					indirectBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
					indirectBarrier.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
					indirectBarrier.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
					indirectBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					indirectBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					indirectBarrier.buffer = frameRenderData.hzbVisibleCountBuffer;
					indirectBarrier.offset = 0u;
					indirectBarrier.size = sizeof(uint32_t);

					VkDependencyInfo indirectDepInfo{};
					indirectDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
					indirectDepInfo.bufferMemoryBarrierCount = 1u;
					indirectDepInfo.pBufferMemoryBarriers = &indirectBarrier;
					vkCmdPipelineBarrier2(cmd, &indirectDepInfo);

					vkCmdDrawIndirectCountKHR(
						cmd,
						frameRenderData.opaqueIndirectBuffer,
						0u,
						frameRenderData.hzbVisibleCountBuffer,
						0u,
						frameRenderData.chunkDescriptorCount,
						sizeof(VkDrawIndirectCommand));
				} else {
					vkCmdDrawIndirect(
						cmd,
						frameRenderData.opaqueIndirectBuffer,
						0,
						frameRenderData.chunkDescriptorCount,
						sizeof(VkDrawIndirectCommand));
				}
			}
		}

		if (render.modelPipeline != VK_NULL_HANDLE && !render.visibleModelInstances.empty()) {
			PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Model Pass");
			vkCmdBindPipeline(
				cmd,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				taaOn && render.modelPipelineTaaOn != VK_NULL_HANDLE
					? render.modelPipelineTaaOn
					: render.modelPipeline);
			struct ModelPush {
				std::array<float, 16> viewProjection{};
				std::array<float, 16> modelTransform{};
			};
			ModelPush push{};
			std::memcpy(
				push.viewProjection.data(),
				frameRenderData.graphicsPushConstants.viewProjection.data(),
				sizeof(float) * 16);
			for (const ModelInstanceData &instance : render.visibleModelInstances) {
				if (instance.indexCount == 0 || instance.vertexBuffer == VK_NULL_HANDLE || instance.indexBuffer == VK_NULL_HANDLE) {
					continue;
				}
				std::memcpy(
					push.modelTransform.data(),
					instance.modelTransform.data(),
					sizeof(float) * 16);
				vkCmdPushConstants(
					cmd,
					render.graphicsPipelineLayout,
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
					0,
					sizeof(ModelPush),
					&push);
				constexpr VkDeviceSize vertexOffset = 0;
				vkCmdBindVertexBuffers(cmd, 0, 1, &instance.vertexBuffer, &vertexOffset);
				vkCmdBindIndexBuffer(
					cmd,
					instance.indexBuffer,
					0,
					VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(cmd, instance.indexCount, 1, 0, 0, 0);
			}
		}

		if ((taaOn ? render.transparentGraphicsPipelineTaaOn : render.transparentGraphicsPipeline) &&
			frameRenderData.graphicsDescriptorSet != VK_NULL_HANDLE &&
			frameRenderData.chunkDescriptorCount > 0 &&
			frameRenderData.transparentIndirectBuffer != VK_NULL_HANDLE &&
			frameRenderData.packedFaceBuffer != VK_NULL_HANDLE) {
			PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Transparent Pass");
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, taaOn ? render.transparentGraphicsPipelineTaaOn : render.transparentGraphicsPipeline);
			vkCmdPushConstants(
				cmd,
				render.graphicsPipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(frameRenderData.graphicsPushConstants),
				&frameRenderData.graphicsPushConstants);
			vkCmdDrawIndirect(
				cmd,
				frameRenderData.transparentIndirectBuffer,
				0,
				frameRenderData.chunkDescriptorCount,
				sizeof(VkDrawIndirectCommand));
		}

		if (!taaOn) {
			RecordDebugOverlayCommands(render, swapchain, frameRenderData, cmd);
			RecordDebugHudCommands(render, frameRenderData, cmd);
		}

		vkCmdEndRendering(cmd);

		render.taaLayerSceneColorCurrentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		if (taaOn) {

			TransitionImage(
				cmd,
				render.taaSceneColorTarget->image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
			render.taaSceneColorCurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			TransitionImage(
				cmd,
				render.depthImage,
				VK_IMAGE_ASPECT_DEPTH_BIT,
				VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
				VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
			render.depthImageCurrentLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;

			TransitionImage(
				cmd,
				render.taaHistoryColorTarget->image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				render.taaHistoryColorCurrentLayout,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				render.taaHistoryColorCurrentLayout == VK_IMAGE_LAYOUT_UNDEFINED
					? VK_PIPELINE_STAGE_2_NONE
					: VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				render.taaHistoryColorCurrentLayout == VK_IMAGE_LAYOUT_UNDEFINED ? 0
																			 : VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
			render.taaHistoryColorCurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			render.taaHistoryNeedsInit = false;

			projectv::taa::TransitionTaaMotionVectorForSample(cmd, *render.taaMotionVectorTarget);
			render.taaMotionVectorCurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			TransitionImage(
				cmd,
				swapchain.images[imageIndex],
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_2_NONE,
				0,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

			const VkRenderingAttachmentInfo resolveColorAttachment{
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.pNext = nullptr,
				.imageView = swapchain.imageViews[imageIndex],
				.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.resolveMode = VK_RESOLVE_MODE_NONE,
				.resolveImageView = VK_NULL_HANDLE,
				.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.clearValue = clearColorValue,
			};
			const VkRenderingInfo resolveRenderingInfo{
				.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
				.pNext = nullptr,
				.flags = 0,
				.renderArea = {{0, 0}, swapchain.extent},
				.layerCount = 1,
				.viewMask = 0,
				.colorAttachmentCount = 1,
				.pColorAttachments = &resolveColorAttachment,
				.pDepthAttachment = nullptr,
				.pStencilAttachment = nullptr,
			};
			vkCmdBeginRendering(cmd, &resolveRenderingInfo);
			vkCmdSetViewport(cmd, 0, 1, &viewport);
			vkCmdSetScissor(cmd, 0, 1, &scissor);

			PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "TAA Resolve");
			PV_PROFILE_GPU_LABEL_COLOR(cmd, "TAA Resolve", 0.20f, 0.65f, 1.00f, 1.0f);

			const Uint64 taaResolveStartCounter = SDL_GetPerformanceCounter();

			const projectv::math::Mat4 currentViewProj = frameRenderData.graphicsPushConstants.viewProjection;
			ResolvePushConstants resolvePushConstants{};
			resolvePushConstants.inverseCurrentViewProjection = projectv::math::inverse(currentViewProj);
			resolvePushConstants.currentViewProjection = currentViewProj;
			resolvePushConstants.renderExtentInverse = {
				1.0f / static_cast<float>(swapchain.extent.width),
				1.0f / static_cast<float>(swapchain.extent.height),
			};

			resolvePushConstants.taaBlend = render.taaEnabled ? render.taaBlend : 0.0f;
			resolvePushConstants.taaCasSharpnessMax = render.taaCasSharpnessMax;

			if (frameRenderData.taaResolveDescriptorSet != VK_NULL_HANDLE) {
				vkCmdBindDescriptorSets(
					cmd,
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					render.taaResolvePipelineLayout,
					0,
					1,
					&frameRenderData.taaResolveDescriptorSet,
					0,
					nullptr);
			}
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render.taaResolvePipeline);
			vkCmdPushConstants(
				cmd,
				render.taaResolvePipelineLayout,
				VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(resolvePushConstants),
				&resolvePushConstants);

			vkCmdDraw(cmd, 3, 1, 0, 0);
			{
				const Uint64 taaResolveEndCounter = SDL_GetPerformanceCounter();
				const double seconds = static_cast<double>(taaResolveEndCounter - taaResolveStartCounter) /
									   static_cast<double>(SDL_GetPerformanceFrequency());
				render.renderPassTimings.taaResolveMs = static_cast<float>(seconds * 1000.0);
			}

			RecordDebugOverlayCommands(render, swapchain, frameRenderData, cmd);
			RecordDebugHudCommands(render, frameRenderData, cmd);

			vkCmdEndRendering(cmd);

			if (render.taaHistoryValid) {
				TransitionImage(
					cmd,
					render.taaSceneColorTarget->image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_READ_BIT);
				render.taaSceneColorCurrentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				TransitionImage(
					cmd,
					render.taaHistoryColorTarget->image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_WRITE_BIT);
				render.taaHistoryColorCurrentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

				const VkImageCopy historyCopyRegion{
					.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u},
					.srcOffset = {0, 0, 0},
					.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u},
					.dstOffset = {0, 0, 0},
					.extent = {swapchain.extent.width, swapchain.extent.height, 1u},
				};
				vkCmdCopyImage(
					cmd,
					render.taaSceneColorTarget->image,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					render.taaHistoryColorTarget->image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&historyCopyRegion);

				TransitionImage(
					cmd,
					render.taaSceneColorTarget->image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_READ_BIT,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
				render.taaSceneColorCurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				TransitionImage(
					cmd,
					render.taaHistoryColorTarget->image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_WRITE_BIT,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
				render.taaHistoryColorCurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			} else {

				render.taaHistoryValid = true;
			}
		}

		if (render.taaLayerSceneColorTarget != nullptr && render.taaLayerHistoryColorTarget != nullptr && render.taaLayerSceneColorTarget->image != VK_NULL_HANDLE && render.taaLayerHistoryColorTarget->image != VK_NULL_HANDLE) {
			if (render.taaLayerHistoryValid) {
				TransitionImage(
					cmd,
					render.taaLayerSceneColorTarget->image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					render.taaLayerSceneColorCurrentLayout,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_READ_BIT);
				render.taaLayerSceneColorCurrentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				TransitionImage(
					cmd,
					render.taaLayerHistoryColorTarget->image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					render.taaLayerHistoryColorCurrentLayout,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_WRITE_BIT);
				render.taaLayerHistoryColorCurrentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

				const VkImageCopy layerHistoryCopyRegion{
					.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u},
					.srcOffset = {0, 0, 0},
					.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u},
					.dstOffset = {0, 0, 0},
					.extent = {swapchain.extent.width, swapchain.extent.height, 1u},
				};
				vkCmdCopyImage(
					cmd,
					render.taaLayerSceneColorTarget->image,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					render.taaLayerHistoryColorTarget->image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&layerHistoryCopyRegion);

				TransitionImage(
					cmd,
					render.taaLayerSceneColorTarget->image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					render.taaLayerSceneColorCurrentLayout,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_READ_BIT,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
				render.taaLayerSceneColorCurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				TransitionImage(
					cmd,
					render.taaLayerHistoryColorTarget->image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					render.taaLayerHistoryColorCurrentLayout,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_2_COPY_BIT,
					VK_ACCESS_2_TRANSFER_WRITE_BIT,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
				render.taaLayerHistoryColorCurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			} else {

				render.taaLayerHistoryValid = true;
			}
		}

		if (ShouldCaptureScreenshot(render)) {
			RecordSwapchainScreenshotCopy(swapchain, render, cmd, imageIndex);
		} else {
			TransitionImage(
				cmd,
				swapchain.images[imageIndex],
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_PIPELINE_STAGE_2_NONE,
				0);
		}
	}

	profiling::CollectVulkanGpu(render.tracyGraphicsContext, cmd);
}
} // namespace

SDL_AppResult DrawFrame(
	AppState *state,
	PlatformState *platform,
	VulkanContextState *context,
	SwapchainState *swapchain,
	RenderState *render,
	FrameState *frame)
{
	PV_PROFILE_ZONE_N("DrawFrame");
	if (!platform || !context || !swapchain || !render || !frame) {
		return SDL_APP_CONTINUE;
	}

	if (swapchain->extent.width == 0 || swapchain->extent.height == 0) {
		if (!RecreateSwapchain(platform, context, swapchain, render)) {
			runtime::LogRuntimeFailure(
				"Render",
				"DrawFrame.RecreateSwapchainZeroExtent",
				"RecreateSwapchain returned false while swapchain extent is zero");
			return SDL_APP_FAILURE;
		}
		platform->windowResized = false;
		return SDL_APP_CONTINUE;
	}
	if (swapchain->handle == VK_NULL_HANDLE) {
		runtime::LogRuntimeFailure("Render", "DrawFrame.SwapchainHandle", "swapchain handle is null");
		return SDL_APP_FAILURE;
	}
	if (render->screenshotCaptureRequested && !render->screenshotCaptureSupported) {
		runtime::LogRuntimeFailure(
			"Capture",
			"DrawFrame.ScreenshotSupport",
			"screenshot capture is unavailable for the current swapchain");
		render->screenshotCaptureRequested = false;
	}

	const uint32_t currentFrame = frame->currentFrame;
	const size_t currentFrameIndex = currentFrame;
	if (currentFrameIndex >= frame->commandBuffers.size() ||
		currentFrameIndex >= frame->inFlightFences.size() ||
		currentFrameIndex >= frame->imageAvailableSemaphores.size() ||
		currentFrameIndex >= frame->renderFinishedSemaphores.size()) {
		runtime::LogRuntimeFailure("Render", "DrawFrame.FrameState", "FrameState is incomplete");
		return SDL_APP_FAILURE;
	}
	if (frame->renderData.frameIndex != currentFrame) {
		runtime::LogRuntimeFailure(
			"Render",
			"DrawFrame.FrameRenderData",
			fmt::format("FrameRenderData is not prepared for frame {}", currentFrame));
		return SDL_APP_FAILURE;
	}

	const VkCommandBuffer cmd = frame->commandBuffers[currentFrameIndex];
	const VkFence inFlightFence = frame->inFlightFences[currentFrameIndex];
	const VkSemaphore imageAvailableSemaphore = frame->imageAvailableSemaphores[currentFrameIndex];
	const VkExtent2D captureExtent = swapchain->extent;
	const VkFormat captureFormat = swapchain->format;

	uint32_t imageIndex = 0;

	const VkResult acquireRes = vkAcquireNextImageKHR(
		context->device,
		swapchain->handle,
		UINT64_MAX,
		imageAvailableSemaphore,
		VK_NULL_HANDLE,
		&imageIndex);
	if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR) {
		if (!RecreateSwapchain(platform, context, swapchain, render)) {
			runtime::LogRuntimeFailure(
				"Render",
				"DrawFrame.RecreateSwapchainAfterAcquire",
				fmt::format(
					"RecreateSwapchain returned false after vkAcquireNextImageKHR returned {} ({})",
					VkResultToString(acquireRes),
					static_cast<int>(acquireRes)));
			return SDL_APP_FAILURE;
		}
		platform->windowResized = false;
		return SDL_APP_CONTINUE;
	}
	if (acquireRes != VK_SUCCESS && acquireRes != VK_SUBOPTIMAL_KHR) {
		runtime::LogVkFailure("DrawFrame.vkAcquireNextImageKHR", acquireRes);
		return SDL_APP_FAILURE;
	}

	const VkResult resetFenceResult = vkResetFences(context->device, 1, &inFlightFence);
	if (resetFenceResult != VK_SUCCESS) {
		runtime::LogVkFailure("DrawFrame.vkResetFences", resetFenceResult);
		return SDL_APP_FAILURE;
	}
	const VkResult resetCommandBufferResult = vkResetCommandBuffer(cmd, 0);
	if (resetCommandBufferResult != VK_SUCCESS) {
		runtime::LogVkFailure("DrawFrame.vkResetCommandBuffer", resetCommandBufferResult);
		return SDL_APP_FAILURE;
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	const VkResult beginCommandBufferResult = vkBeginCommandBuffer(cmd, &beginInfo);
	if (beginCommandBufferResult != VK_SUCCESS) {
		runtime::LogVkFailure("DrawFrame.vkBeginCommandBuffer", beginCommandBufferResult);
		return SDL_APP_FAILURE;
	}

	if (render->meshShaderEnabled && frame->renderData.chunkDescriptorCount > 0) {
		const projectv::render::MeshCullPushConstants cullPush =
			projectv::render::BuildMeshCullPushConstants(
				frame->renderData.chunkCullingParameters,
				frame->renderData.chunkDescriptorCount);
		projectv::render::RecordMeshShaderPreCull(
			cmd,
			context,
			*render,
			render->sceneFrameResources[frame->currentFrame],
			cullPush);
	}

	RecordGraphicsCommands(*render, *swapchain, frame->renderData, cmd, imageIndex);

	const bool asyncComputeHzbPathActive =
		projectv::render::IsAsyncComputeEnabled() &&
		projectv::render::IsAsyncComputeResourcesAllocated(*context) &&
		projectv::render::IsHzbCullingEnabled() &&
		render->hizBuffer.image != VK_NULL_HANDLE &&
		frame->renderData.hizCullingDescriptorSet != VK_NULL_HANDLE;

	if (projectv::render::IsHzbCullingEnabled() &&
		render->hizBuffer.image != VK_NULL_HANDLE) {
		projectv::render::BuildHizMipChain(
			cmd,
			render->depthImage,
			render->depthImageCurrentLayout,
			render->hizBuffer);
		if (render->hizBuffer.imageView != VK_NULL_HANDLE &&
			render->hizBuffer.sampler != VK_NULL_HANDLE &&
			frame->renderData.hizCullingDescriptorSet != VK_NULL_HANDLE &&
			!asyncComputeHzbPathActive) {
			projectv::math::Mat4 inverseViewProjection =
				projectv::math::inverse(frame->renderData.graphicsPushConstants.viewProjection);
			std::array<float, 16> inverseViewProjectionFlat{};
			for (uint32_t i = 0; i < 16u; ++i) {
				inverseViewProjectionFlat[i] = inverseViewProjection.data()[i];
			}
			projectv::render::RecordHzbCullingDispatch(
				cmd,
				context,
				*render,
				render->sceneFrameResources[frame->currentFrame],
				*reinterpret_cast<const float (*)[16]>(inverseViewProjectionFlat.data()),
				frame->renderData.chunkDescriptorCount);
		}
	}

	const bool asyncComputePathActive =
		asyncComputeHzbPathActive ||
		(projectv::render::IsAsyncComputeEnabled() &&
			projectv::render::IsAsyncComputeResourcesAllocated(*context) &&
			(render->fluidCaPipelineEnabled || render->worldGenPipelineEnabled));

	if (!asyncComputePathActive && render->fluidCaPipelineEnabled && state->simulation().fluidGpuTicksPending > 0u) {
		PV_PROFILE_ZONE_N("RecordFluidCaCommands");
		VoxelWorld *voxelWorld = state->world().voxelWorld.get();
		if (voxelWorld != nullptr) {
			const std::vector<uint32_t> activeChunkIds = BuildActiveChunkIdsForFluidCa(*voxelWorld);
			SceneFrameResources &frameResources = render->sceneFrameResources[frame->currentFrame];
			if (frameResources.fluidCaActiveChunkIdMappedData != nullptr && !activeChunkIds.empty()) {
				std::memcpy(
					frameResources.fluidCaActiveChunkIdMappedData,
					activeChunkIds.data(),
					activeChunkIds.size() * sizeof(uint32_t));
			}
			projectv::render::FluidCaPushConstants fluidCaPush{};
			fluidCaPush.chunkDimensions = {
				static_cast<uint32_t>(voxelWorld->chunkSize),
				static_cast<uint32_t>(voxelWorld->chunkSize),
				static_cast<uint32_t>(voxelWorld->chunkSize),
				0u,
			};
			fluidCaPush.chunkCountAndFlags = {
				static_cast<uint32_t>(activeChunkIds.size()),
				0u,
				0u,
				0u,
			};
			fluidCaPush.fluidTickInterval = 1.0f / std::max(state->simulation().fluidTickRateHz, 1.0f);
			for (uint32_t tickIndex = 0; tickIndex < state->simulation().fluidGpuTicksPending; ++tickIndex) {
				projectv::render::RecordFluidCaDispatch(
					cmd,
					*render,
					frameResources,
					fluidCaPush,
					static_cast<uint32_t>(activeChunkIds.size()));
			}
			state->simulation().fluidGpuTicksPending = 0u;
		}
	}

	if (!asyncComputePathActive && render->worldGenPipelineEnabled && state->world().voxelWorld != nullptr) {
		PV_PROFILE_ZONE_N("RecordWorldGenCommands");
		VoxelWorld *voxelWorld = state->world().voxelWorld.get();
		std::vector<uint32_t> activeWorldGenChunkIds;
		const uint32_t worldGenChunkCount = projectv::render::BuildActiveChunkIdsForWorldGen(
			*voxelWorld,
			activeWorldGenChunkIds);
		SceneFrameResources &worldGenFrameResources = render->sceneFrameResources[frame->currentFrame];
		if (worldGenChunkCount > 0u && worldGenFrameResources.worldGenVoxelBuffer != VK_NULL_HANDLE) {
			if (worldGenFrameResources.worldGenVoxelMappedData != nullptr) {
				std::memset(
					worldGenFrameResources.worldGenVoxelMappedData,
					0,
					static_cast<size_t>(worldGenChunkCount) *
						static_cast<size_t>(projectv::render::kWorldGenVoxelBufferBytesPerChunk));
			}
			projectv::render::WorldGenPushConstants worldGenPush{};
			worldGenPush.chunkOriginAndChunkSize = {
				0,
				0,
				0,
				static_cast<int32_t>(voxelWorld->chunkSize),
			};
			worldGenPush.chunkCountAndFlags = {
				worldGenChunkCount,
				0u,
				0u,
				0u,
			};
			worldGenPush.noiseParams = {
				0.5f,
				0.5f,
				4u,
				2.0f,
			};
			worldGenPush.seed = static_cast<uint32_t>(state->simulation().simulationTick);
			projectv::render::RecordWorldGenDispatch(
				cmd,
				*render,
				worldGenFrameResources,
				worldGenPush,
				worldGenChunkCount);
		}
	}

	const VkResult endCommandBufferResult = vkEndCommandBuffer(cmd);
	if (endCommandBufferResult != VK_SUCCESS) {
		runtime::LogVkFailure("DrawFrame.vkEndCommandBuffer", endCommandBufferResult);
		return SDL_APP_FAILURE;
	}

	if (asyncComputePathActive) {
		PV_PROFILE_ZONE_N("AsyncCompute.Submit");
		if (projectv::render::RecordAsyncComputePass(
				context->asyncComputeCommandBuffer,
				*context,
				*render,
				state,
				frame)) {
			uint64_t newTimelineValue = 0u;
			if (projectv::render::SubmitToComputeQueue(context, context->asyncComputeCommandBuffer, &newTimelineValue)) {
				context->asyncComputeLastTimelineValue = newTimelineValue;
			}
		}
	}

	VkSemaphoreSubmitInfo waitSemaphoreInfo{};
	waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;

	waitSemaphoreInfo.semaphore = imageAvailableSemaphore;
	waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSemaphoreSubmitInfo computeWaitSemaphoreInfo{};
	std::array<VkSemaphoreSubmitInfo, 2> allWaitSemaphoreInfos{};
	uint32_t waitSemaphoreInfoCount = 1u;
	allWaitSemaphoreInfos[0] = waitSemaphoreInfo;
	if (asyncComputePathActive && context->asyncComputeLastTimelineValue > 0u) {
		computeWaitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		computeWaitSemaphoreInfo.semaphore = context->renderTimelineSemaphore;
		computeWaitSemaphoreInfo.value = context->asyncComputeLastTimelineValue;
		computeWaitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		allWaitSemaphoreInfos[1] = computeWaitSemaphoreInfo;
		waitSemaphoreInfoCount = 2u;
	}

	const VkSemaphore submitSemaphore = swapchain->submitSemaphores[imageIndex];
	VkSemaphoreSubmitInfo signalSemaphoreInfo{};
	signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalSemaphoreInfo.semaphore = submitSemaphore;

	signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

	std::array<VkSemaphoreSubmitInfo, 2> allSignalSemaphoreInfos{};
	allSignalSemaphoreInfos[0] = signalSemaphoreInfo;
	uint32_t signalSemaphoreInfoCount = 1u;

	VkSemaphoreSubmitInfo hzbSignalSemaphoreInfo{};
	if (asyncComputeHzbPathActive && context->hzbBuildTimelineSemaphore != VK_NULL_HANDLE) {
		context->hzbBuildLastTimelineValue += 1u;
		hzbSignalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		hzbSignalSemaphoreInfo.semaphore = context->hzbBuildTimelineSemaphore;
		hzbSignalSemaphoreInfo.value = context->hzbBuildLastTimelineValue;
		hzbSignalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		allSignalSemaphoreInfos[1] = hzbSignalSemaphoreInfo;
		signalSemaphoreInfoCount = 2u;
	}

	VkCommandBufferSubmitInfo cmdBufferInfo{};
	cmdBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmdBufferInfo.commandBuffer = cmd;

	VkSubmitInfo2 submitInfo2{};
	submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo2.waitSemaphoreInfoCount = waitSemaphoreInfoCount;
	submitInfo2.pWaitSemaphoreInfos = allWaitSemaphoreInfos.data();
	submitInfo2.commandBufferInfoCount = 1;
	submitInfo2.pCommandBufferInfos = &cmdBufferInfo;
	submitInfo2.signalSemaphoreInfoCount = signalSemaphoreInfoCount;
	submitInfo2.pSignalSemaphoreInfos = allSignalSemaphoreInfos.data();
	const VkResult submitResult = vkQueueSubmit2(context->queue, 1, &submitInfo2, inFlightFence);
	if (submitResult != VK_SUCCESS) {
		runtime::LogVkFailure("DrawFrame.vkQueueSubmit2", submitResult);
		return SDL_APP_FAILURE;
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &submitSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain->handle;
	presentInfo.pImageIndices = &imageIndex;

	const VkResult presentRes = vkQueuePresentKHR(context->queue, &presentInfo);
	if (!SaveRequestedScreenshot(*context, *render, inFlightFence, captureExtent, captureFormat)) {
		return SDL_APP_FAILURE;
	}
	if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR || platform->windowResized) {
		if (!RecreateSwapchain(platform, context, swapchain, render)) {
			runtime::LogRuntimeFailure(
				"Render",
				"DrawFrame.RecreateSwapchainAfterPresent",
				"RecreateSwapchain returned false after vkQueuePresentKHR/window lifecycle refresh");
			return SDL_APP_FAILURE;
		}
		platform->windowResized = false;
	} else if (presentRes != VK_SUCCESS) {
		runtime::LogVkFailure("DrawFrame.vkQueuePresentKHR", presentRes);
		return SDL_APP_FAILURE;
	}

	frame->currentFrame = (frame->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	return SDL_APP_CONTINUE;
}

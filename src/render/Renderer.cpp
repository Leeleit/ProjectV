#include "render/Renderer.hpp"

#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"
#include "debug/ProfilingGpu.hpp"
#include "render/ScreenshotCapture.hpp"
#include "render/vulkan/VulkanInit.hpp"
#include "render/vulkan/VulkanResult.hpp"
#include "voxel/VoxelMaterials.hpp"

#include "fmt/format.h"

#include <filesystem>

namespace {
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

	const VkResult waitResult = vkWaitForFences(context.device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
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
	PV_PROFILE_ZONE_N("RecordShadowCommands");
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
			// Shadows follow sparse per-chunk opaque face ranges. Transparent
			// casters follow the current material policy encoded by the shadow shader.
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
	PV_PROFILE_ZONE_N("RecordDebugOverlayCommands");
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
	PV_PROFILE_ZONE_N("RecordDebugHudCommands");
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
	PV_PROFILE_ZONE_N("RecordVoxelMeshingCommands");
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
	PV_PROFILE_ZONE_N("RecordGraphicsCommands");
	{
		PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Graphics Frame");

		RecordVoxelMeshingCommands(render, frameRenderData, cmd);
		RecordShadowCommands(render, frameRenderData, cmd);

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

		const VkImageLayout oldDepthLayout =
			render.depthImageNeedsInit ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		const VkPipelineStageFlags2 oldDepthStage =
			render.depthImageNeedsInit ? VK_PIPELINE_STAGE_2_NONE : VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
		const VkAccessFlags2 oldDepthAccess =
			render.depthImageNeedsInit ? 0 : VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
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
		const VkRenderingAttachmentInfo colorAttachment{
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
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachment,
			.pDepthAttachment = &depthAttachment,
			.pStencilAttachment = nullptr,
		};

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
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render.graphicsPipeline);
			vkCmdPushConstants(
				cmd,
				render.graphicsPipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(frameRenderData.graphicsPushConstants),
				&frameRenderData.graphicsPushConstants);
			vkCmdDrawIndirect(
				cmd,
				frameRenderData.opaqueIndirectBuffer,
				0,
				frameRenderData.chunkDescriptorCount,
				sizeof(VkDrawIndirectCommand));
		}

		if (render.transparentGraphicsPipeline &&
			frameRenderData.graphicsDescriptorSet != VK_NULL_HANDLE &&
			frameRenderData.chunkDescriptorCount > 0 &&
			frameRenderData.transparentIndirectBuffer != VK_NULL_HANDLE &&
			frameRenderData.packedFaceBuffer != VK_NULL_HANDLE) {
			PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, "Transparent Pass");
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render.transparentGraphicsPipeline);
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

		RecordDebugOverlayCommands(render, swapchain, frameRenderData, cmd);
		RecordDebugHudCommands(render, frameRenderData, cmd);

		vkCmdEndRendering(cmd);

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
	// `vkAcquireNextImageKHR` is given the per-in-flight-frame
	// `imageAvailableSemaphore`. The driver signals it when the
	// returned `imageIndex` becomes writable. The submit pipeline (below)
	// waits on this same handle, so the command buffer does not start
	// rendering before the swapchain image is actually writable. The
	// guide `swapchain_semaphore_reuse.html` uses *exactly* this pattern:
	// per-frame acquire-semaphore, per-image submit-semaphore, no
	// per-image acquire fence (the fence pattern requires
	// `VK_KHR_swapchain_maintenance1`).
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

	RecordGraphicsCommands(*render, *swapchain, frame->renderData, cmd, imageIndex);

	const VkResult endCommandBufferResult = vkEndCommandBuffer(cmd);
	if (endCommandBufferResult != VK_SUCCESS) {
		runtime::LogVkFailure("DrawFrame.vkEndCommandBuffer", endCommandBufferResult);
		return SDL_APP_FAILURE;
	}

	VkSemaphoreSubmitInfo waitSemaphoreInfo{};
	waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	// `pWaitSemaphores[0]` is the per-in-flight-frame `imageAvailableSemaphore`,
	// signaled by the `vkAcquireNextImageKHR` call above when the swapchain
	// image became available. The submit pipeline waits on it so the
	// command buffer does not start rendering before the swapchain image
	// is actually writable.
	waitSemaphoreInfo.semaphore = imageAvailableSemaphore;
	waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

	// `pSignalSemaphores[0]` is the per-swapchain-image `submitSemaphore`,
	// *not* the per-in-flight-frame `renderFinishedSemaphore`. The
	// canonical Vulkan pattern (per the SDK 1.4 guide
	// `swapchain_semaphore_reuse.html`) is to index the submit-finished
	// semaphore by `imageIndex` rather than by frame counter, because
	// two consecutive in-flight frames can be handed the same
	// `imageIndex` before the first one's present has retired its
	// `pWaitSemaphores`. We then present the same `submitSemaphore`
	// below in `presentInfo.pWaitSemaphores`.
	const VkSemaphore submitSemaphore = swapchain->submitSemaphores[imageIndex];
	VkSemaphoreSubmitInfo signalSemaphoreInfo{};
	signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalSemaphoreInfo.semaphore = submitSemaphore;
	// Screenshot capture records a transfer copy after color rendering; present
	// must not observe the swapchain image before that copy and layout transition finish.
	signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

	VkCommandBufferSubmitInfo cmdBufferInfo{};
	cmdBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmdBufferInfo.commandBuffer = cmd;

	VkSubmitInfo2 submitInfo2{};
	submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo2.waitSemaphoreInfoCount = 1;
	submitInfo2.pWaitSemaphoreInfos = &waitSemaphoreInfo;
	submitInfo2.commandBufferInfoCount = 1;
	submitInfo2.pCommandBufferInfos = &cmdBufferInfo;
	submitInfo2.signalSemaphoreInfoCount = 1;
	submitInfo2.pSignalSemaphoreInfos = &signalSemaphoreInfo;
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

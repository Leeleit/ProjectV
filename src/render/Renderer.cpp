#include "render/Renderer.hpp"

#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"
#include "debug/ProfilingGpu.hpp"
#include "render/vulkan/VulkanInit.hpp"
#include "render/vulkan/VulkanResult.hpp"

#include "fmt/format.h"

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
	imageBarrier.subresourceRange = {aspectMask, 0, 1, 0, 1};

	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2(cmd, &depInfo);
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

	VkBufferMemoryBarrier2 bufferBarriers[3]{};
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
	bufferBarriers[2].buffer = frameRenderData.transparentIndirectBuffer;

	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.bufferMemoryBarrierCount = 3;
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

		constexpr VkClearValue clearColorValue{.color = {{0.73f, 0.84f, 0.96f, 1.0f}}};
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
	const VkSemaphore renderFinishedSemaphore = frame->renderFinishedSemaphores[currentFrameIndex];

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

	RecordGraphicsCommands(*render, *swapchain, frame->renderData, cmd, imageIndex);

	const VkResult endCommandBufferResult = vkEndCommandBuffer(cmd);
	if (endCommandBufferResult != VK_SUCCESS) {
		runtime::LogVkFailure("DrawFrame.vkEndCommandBuffer", endCommandBufferResult);
		return SDL_APP_FAILURE;
	}

	VkSemaphoreSubmitInfo waitSemaphoreInfo{};
	waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	waitSemaphoreInfo.semaphore = imageAvailableSemaphore;
	waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSemaphoreSubmitInfo signalSemaphoreInfo{};
	signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalSemaphoreInfo.semaphore = renderFinishedSemaphore;
	signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

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
	presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain->handle;
	presentInfo.pImageIndices = &imageIndex;

	const VkResult presentRes = vkQueuePresentKHR(context->queue, &presentInfo);
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

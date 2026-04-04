#include "Renderer.hpp"
#include "Camera.hpp"
#include "SceneResources.hpp"
#include "VulkanInit.hpp"

namespace {
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

void RecordGraphicsCommands(AppState *state, const VkCommandBuffer cmd, const uint32_t imageIndex)
{
	RenderState &render = state->render;
	const SwapchainState &swapchain = state->swapchain;
	const SceneFrameResources &frameResources = render.sceneFrameResources[state->frame.currentFrame];

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

	const GraphicsPushConstants pushConstants = BuildGraphicsPushConstants(state->camera, swapchain.extent);
	const VkBuffer vertexBuffers[] = {frameResources.vertexBuffer};
	constexpr VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render.graphicsPipeline);
	vkCmdPushConstants(
		cmd,
		render.graphicsPipelineLayout,
		VK_SHADER_STAGE_VERTEX_BIT,
		0,
		sizeof(pushConstants),
		&pushConstants);
	if (frameResources.opaqueVertexCount > 0) {
		vkCmdDraw(cmd, frameResources.opaqueVertexCount, 1, 0, 0);
	}

	if (render.transparentGraphicsPipeline && frameResources.transparentVertexCount > 0) {
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render.transparentGraphicsPipeline);
		vkCmdPushConstants(
			cmd,
			render.graphicsPipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT,
			0,
			sizeof(pushConstants),
			&pushConstants);
		vkCmdDraw(cmd, frameResources.transparentVertexCount, 1, frameResources.opaqueVertexCount, 0);
	}

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
} // namespace

SDL_AppResult DrawFrame(AppState *state)
{
	if (!state) {
		return SDL_APP_CONTINUE;
	}

	if (state->swapchain.extent.width == 0 || state->swapchain.extent.height == 0) {
		if (!RecreateSwapchain(&state->platform, &state->context, &state->swapchain, &state->render)) {
			SDL_Log("RecreateSwapchain failed");
			return SDL_APP_FAILURE;
		}
		return SDL_APP_CONTINUE;
	}

	const uint32_t currentFrame = state->frame.currentFrame;
	const VkCommandBuffer cmd = state->frame.commandBuffers[currentFrame];
	const VkFence inFlightFence = state->frame.inFlightFences[currentFrame];
	const VkSemaphore imageAvailableSemaphore = state->frame.imageAvailableSemaphores[currentFrame];
	const VkSemaphore renderFinishedSemaphore = state->frame.renderFinishedSemaphores[currentFrame];

	vkWaitForFences(state->context.device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

	if (!UploadSceneFrameResources(&state->world, &state->render, currentFrame)) {
		SDL_Log("UploadSceneFrameResources failed");
		return SDL_APP_FAILURE;
	}

	uint32_t imageIndex = 0;
	const VkResult acquireRes = vkAcquireNextImageKHR(
		state->context.device,
		state->swapchain.handle,
		UINT64_MAX,
		imageAvailableSemaphore,
		VK_NULL_HANDLE,
		&imageIndex);
	if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR) {
		if (!RecreateSwapchain(&state->platform, &state->context, &state->swapchain, &state->render)) {
			return SDL_APP_FAILURE;
		}
		return SDL_APP_CONTINUE;
	}
	if (acquireRes != VK_SUCCESS && acquireRes != VK_SUBOPTIMAL_KHR) {
		return SDL_APP_CONTINUE;
	}

	vkResetFences(state->context.device, 1, &inFlightFence);
	vkResetCommandBuffer(cmd, 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
		SDL_Log("vkBeginCommandBuffer failed");
		return SDL_APP_FAILURE;
	}

	RecordGraphicsCommands(state, cmd, imageIndex);

	if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
		SDL_Log("vkEndCommandBuffer failed");
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
	if (vkQueueSubmit2(state->context.queue, 1, &submitInfo2, inFlightFence) != VK_SUCCESS) {
		SDL_Log("vkQueueSubmit2 failed");
		return SDL_APP_FAILURE;
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &state->swapchain.handle;
	presentInfo.pImageIndices = &imageIndex;

	const VkResult presentRes = vkQueuePresentKHR(state->context.queue, &presentInfo);
	if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR || state->platform.windowResized) {
		state->platform.windowResized = false;
		if (!RecreateSwapchain(&state->platform, &state->context, &state->swapchain, &state->render)) {
			SDL_Log("RecreateSwapchain failed");
			return SDL_APP_FAILURE;
		}
	} else if (presentRes != VK_SUCCESS) {
		SDL_Log("vkQueuePresentKHR failed");
		return SDL_APP_FAILURE;
	}

	state->frame.currentFrame = (state->frame.currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	return SDL_APP_CONTINUE;
}

#include "Renderer.hpp"
#include "Camera.hpp"
#include "SceneResources.hpp"
#include "VulkanInit.hpp"

namespace {
void TransitionImage(
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
	imageBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2(cmd, &depInfo);
}

void PushComputeConstants(AppState *state, const VkCommandBuffer cmd)
{
	ComputePushConstants pushConstants{};
	pushConstants.clearColor = {0.08f, 0.10f, 0.14f, 1.0f};
	pushConstants.triangleCount = state->sceneTriangleCount;

	vkCmdPushConstants(
		cmd,
		state->computePipelineLayout,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0,
		sizeof(pushConstants),
		&pushConstants);
}
} // namespace

SDL_AppResult DrawFrame(AppState *state)
{
	if (!state) {
		return SDL_APP_CONTINUE;
	}

	if (state->extent.width == 0 || state->extent.height == 0) {
		if (!RecreateSwapchain(state)) {
			SDL_Log("RecreateSwapchain failed");
			return SDL_APP_FAILURE;
		}
		return SDL_APP_CONTINUE;
	}

	const uint32_t currentFrame = state->currentFrame;
	const VkCommandBuffer cmd = state->commandBuffers[currentFrame];
	const VkFence inFlightFence = state->inFlightFences[currentFrame];
	const VkSemaphore imageAvailableSemaphore = state->imageAvailableSemaphores[currentFrame];
	const VkSemaphore renderFinishedSemaphore = state->renderFinishedSemaphores[currentFrame];

	vkWaitForFences(
		state->device,
		static_cast<uint32_t>(state->inFlightFences.size()),
		state->inFlightFences.data(),
		VK_TRUE,
		UINT64_MAX);

	UpdateCamera(state);
	if (!UpdateSceneResources(state)) {
		SDL_Log("UpdateSceneResources failed");
		return SDL_APP_FAILURE;
	}

	uint32_t imageIndex = 0;
	const VkResult acquireRes = vkAcquireNextImageKHR(
		state->device,
		state->swapchain,
		UINT64_MAX,
		imageAvailableSemaphore,
		VK_NULL_HANDLE,
		&imageIndex);
	if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR) {
		if (!RecreateSwapchain(state)) {
			return SDL_APP_FAILURE;
		}
		return SDL_APP_CONTINUE;
	}
	if (acquireRes != VK_SUCCESS && acquireRes != VK_SUBOPTIMAL_KHR) {
		return SDL_APP_CONTINUE;
	}

	vkResetFences(state->device, 1, &inFlightFence);
	vkResetCommandBuffer(cmd, 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
		SDL_Log("vkBeginCommandBuffer failed");
		return SDL_APP_FAILURE;
	}

	TransitionImage(
		cmd,
		state->swapchainImages[imageIndex],
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_2_NONE,
		0,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);

	if (state->computeDepthImageNeedsInit) {
		TransitionImage(
			cmd,
			state->computeDepthImage,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_PIPELINE_STAGE_2_NONE,
			0,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
		state->computeDepthImageNeedsInit = false;
	}

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, state->computePipeline);
	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		state->computePipelineLayout,
		0,
		1,
		&state->computeDescriptorSets[imageIndex],
		0,
		nullptr);
	PushComputeConstants(state, cmd);

	const uint32_t groupCountX = (state->extent.width + 7) / 8;
	const uint32_t groupCountY = (state->extent.height + 7) / 8;
	vkCmdDispatch(cmd, groupCountX, groupCountY, 1);

	TransitionImage(
		cmd,
		state->swapchainImages[imageIndex],
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
		VK_PIPELINE_STAGE_2_NONE,
		0);

	if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
		SDL_Log("vkEndCommandBuffer failed");
		return SDL_APP_FAILURE;
	}

	VkSemaphoreSubmitInfo waitSemaphoreInfo{};
	waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	waitSemaphoreInfo.semaphore = imageAvailableSemaphore;
	waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

	VkSemaphoreSubmitInfo signalSemaphoreInfo{};
	signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalSemaphoreInfo.semaphore = renderFinishedSemaphore;
	signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

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

	if (vkQueueSubmit2(state->queue, 1, &submitInfo2, inFlightFence) != VK_SUCCESS) {
		SDL_Log("vkQueueSubmit2 failed");
		return SDL_APP_FAILURE;
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &state->swapchain;
	presentInfo.pImageIndices = &imageIndex;

	const VkResult presentRes = vkQueuePresentKHR(state->queue, &presentInfo);
	if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR || state->windowResized) {
		state->windowResized = false;
		if (!RecreateSwapchain(state)) {
			SDL_Log("RecreateSwapchain failed");
			return SDL_APP_FAILURE;
		}
	} else if (presentRes != VK_SUCCESS) {
		SDL_Log("vkQueuePresentKHR failed");
		return SDL_APP_FAILURE;
	}

	state->currentFrame = (state->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	return SDL_APP_CONTINUE;
}

#include "Camera.hpp"
#include "Profiling.hpp"
#include "ProfilingGpu.hpp"
#include "SceneResources.hpp"
#include "VoxelWorld.hpp"
#include "VulkanBootstrap.hpp"
#include "VulkanGraphicsPipeline.hpp"
#include "VulkanSwapchain.hpp"
#include "VulkanVoxelMeshingPipeline.hpp"

namespace {
bool TryCreateCalibratedTracyGpuContext(
	VulkanContextState &context,
	RenderState &render,
	const VkCommandBuffer temporaryCommandBuffer)
{
	PV_PROFILE_ZONE_N("TryCreateCalibratedTracyGpuContext");
	if (temporaryCommandBuffer == VK_NULL_HANDLE) {
		return false;
	}

	if (!vkGetPhysicalDeviceCalibrateableTimeDomainsEXT || !vkGetCalibratedTimestampsEXT) {
		return false;
	}

	render.tracyGraphicsContext = profiling::CreateVulkanGpuContextCalibrated(
		context.physicalDevice,
		context.device,
		context.queue,
		temporaryCommandBuffer,
		vkGetPhysicalDeviceCalibrateableTimeDomainsEXT,
		vkGetCalibratedTimestampsEXT);
	render.tracyGraphicsContextCalibrated = render.tracyGraphicsContext != nullptr;
	return render.tracyGraphicsContext != nullptr;
}

bool CreateTracyGpuContext(
	VulkanContextState *context,
	RenderState *render)
{
	PV_PROFILE_ZONE_N("CreateTracyGpuContext");
#if !defined(PROJECTV_ENABLE_TRACY)
	(void)context;
	(void)render;
	return true;
#else
	if (!context || !render || !context->device || !context->queue || !context->commandPool) {
		return false;
	}

	VkCommandBuffer temporaryCommandBuffer = VK_NULL_HANDLE;
	VkCommandBufferAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = context->commandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(context->device, &allocateInfo, &temporaryCommandBuffer) != VK_SUCCESS) {
		SDL_Log("vkAllocateCommandBuffers failed for Tracy GPU context");
		return false;
	}

	render->tracyGraphicsContext = nullptr;
	render->tracyGraphicsContextCalibrated = false;

	if (!TryCreateCalibratedTracyGpuContext(*context, *render, temporaryCommandBuffer)) {
		render->tracyGraphicsContext = profiling::CreateVulkanGpuContext(
			context->physicalDevice,
			context->device,
			context->queue,
			temporaryCommandBuffer);
	}

	vkFreeCommandBuffers(context->device, context->commandPool, 1, &temporaryCommandBuffer);

	if (!render->tracyGraphicsContext) {
		SDL_Log("CreateVulkanGpuContext failed");
		return false;
	}

	SDL_Log(
		"Tracy GPU context created (%s)",
		render->tracyGraphicsContextCalibrated ? "calibrated timestamps" : "uncalibrated fallback");
	profiling::NameVulkanGpuContext(render->tracyGraphicsContext, "Graphics Queue");
	return true;
#endif
}
} // namespace

bool InitVulkan(AppState *state)
{
	PV_PROFILE_ZONE_N("InitVulkan");
	if (!InitializeVulkanBase(&state->platform, &state->context, &state->frame)) {
		return false;
	}

	if (!CreateTracyGpuContext(&state->context, &state->render)) {
		SDL_Log("CreateTracyGpuContext failed");
		return false;
	}

	if (!RecreateSwapchain(&state->platform, &state->context, &state->swapchain, &state->render)) {
		return false;
	}

	if (!CreateVoxelLabWorld(state)) {
		SDL_Log("CreateVoxelLabWorld failed");
		return false;
	}

	InitializeCamera(&state->camera, &state->simulation, &state->input);

	if (!CreateSceneResources(&state->context, &state->world, &state->render)) {
		SDL_Log("CreateSceneResources failed");
		return false;
	}

	if (!CreateGraphicsPipeline(&state->context, &state->swapchain, &state->render)) {
		SDL_Log("CreateGraphicsPipeline failed");
		return false;
	}
	if (!CreateVoxelMeshingPipeline(&state->context, &state->render)) {
		SDL_Log("CreateVoxelMeshingPipeline failed");
		return false;
	}

	return true;
}

#include "app/Camera.hpp"
#include "debug/Profiling.hpp"
#include "debug/ProfilingGpu.hpp"
#include "render/SceneResources.hpp"
#include "voxel/VoxelWorld.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "core/RuntimeProbe.hpp"
#include "render/vulkan/VulkanBootstrap.hpp"
#include "render/vulkan/VulkanGraphicsPipeline.hpp"
#include "render/vulkan/VulkanSwapchain.hpp"
#include "render/vulkan/VulkanVoxelMeshingPipeline.hpp"

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
	PV_CHECK_OR_RETURN(
		context && render && context->device && context->queue && context->commandPool,
		"Init",
		"CreateTracyGpuContext.Preconditions",
		"context/render/device/queue/commandPool is incomplete");

	VkCommandBuffer temporaryCommandBuffer = VK_NULL_HANDLE;
	VkCommandBufferAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = context->commandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = 1;

	const VkResult allocateCommandBuffersResult =
		vkAllocateCommandBuffers(context->device, &allocateInfo, &temporaryCommandBuffer);
	if (allocateCommandBuffersResult != VK_SUCCESS) {
		return runtime::LogVkFailure(
			"CreateTracyGpuContext.vkAllocateCommandBuffers",
			allocateCommandBuffersResult);
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
		return runtime::LogRuntimeFailure(
			"Init",
			"CreateTracyGpuContext.CreateVulkanGpuContext",
			"CreateVulkanGpuContext returned null");
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
	PV_CHECK_OR_RETURN(state != nullptr, "Init", "InitVulkan.Preconditions", "AppState is null");
	if (!InitializeVulkanBase(&state->platform, &state->context, &state->frame)) {
		return false;
	}
	if (IsInitFailureStageRequested(InitFailureStage::AfterBootstrap)) {
		return runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan",
			"intentional failure probe requested after bootstrap");
	}

	if (!CreateTracyGpuContext(&state->context, &state->render)) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan.CreateTracyGpuContext",
			"CreateTracyGpuContext returned false");
		return false;
	}

	if (!RecreateSwapchain(&state->platform, &state->context, &state->swapchain, &state->render)) {
		return false;
	}

	if (!CreateVoxelLabWorld(state)) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan.CreateVoxelLabWorld",
			"CreateVoxelLabWorld returned false");
		return false;
	}
	if (IsInitFailureStageRequested(InitFailureStage::AfterWorld)) {
		return runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan",
			"intentional failure probe requested after world creation");
	}

	InitializeCamera(&state->camera, &state->simulation, &state->input);

	if (!CreateSceneResources(&state->context, &state->world, &state->render)) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan.CreateSceneResources",
			"CreateSceneResources returned false");
		return false;
	}
	if (IsInitFailureStageRequested(InitFailureStage::AfterSceneResources)) {
		return runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan",
			"intentional failure probe requested after scene resources");
	}
	if (IsInitFailureStageRequested(InitFailureStage::BeforeGraphicsPipeline)) {
		return runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan",
			"intentional failure probe requested before graphics pipeline creation");
	}

	if (!CreateGraphicsPipeline(&state->context, &state->swapchain, &state->render)) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan.CreateGraphicsPipeline",
			"CreateGraphicsPipeline returned false");
		return false;
	}
	if (IsInitFailureStageRequested(InitFailureStage::BeforeVoxelMeshingPipeline)) {
		return runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan",
			"intentional failure probe requested before voxel meshing pipeline creation");
	}
	if (!CreateVoxelMeshingPipeline(&state->context, &state->render)) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan.CreateVoxelMeshingPipeline",
			"CreateVoxelMeshingPipeline returned false");
		return false;
	}

	return true;
}

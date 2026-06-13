#include "app/Camera.hpp"
#include "app/LookDevCaptureAutomation.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "asset/ModelManifestLoader.hpp"
#include "asset/ModelPass.hpp"
#include "core/RuntimeProbe.hpp"
#include "debug/Profiling.hpp"
#include "debug/ProfilingGpu.hpp"
#include "ecs/EcsWorld.hpp"
#include "physics/PhysicsWorld.hpp"
#include "render/SceneResources.hpp"
#include "render/vulkan/VulkanBootstrap.hpp"
#include "render/vulkan/VulkanGraphicsPipeline.hpp"
#include "render/vulkan/VulkanSwapchain.hpp"
#include "render/vulkan/VulkanVoxelMeshingPipeline.hpp"
#include "voxel/VoxelWorld.hpp"

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
		runtime::LogVkFailure(
			"CreateTracyGpuContext.vkAllocateCommandBuffers",
			allocateCommandBuffersResult);
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
		runtime::LogRuntimeFailure(
			"Init",
			"CreateTracyGpuContext.CreateVulkanGpuContext",
			"CreateVulkanGpuContext returned null");
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

namespace {
VkFormat ChooseModelDepthFormat(const VkPhysicalDevice physicalDevice)
{
	constexpr std::array candidates{
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D16_UNORM,
	};
	for (const VkFormat format : candidates) {
		VkFormatProperties props{};
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
		if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
			return format;
		}
	}
	return VK_FORMAT_UNDEFINED;
}
} // namespace

bool InitVulkan(AppState *state)
{
	PV_PROFILE_ZONE_N("InitVulkan");
	PV_CHECK_OR_RETURN(state != nullptr, "Init", "InitVulkan.Preconditions", "AppState is null");
	CameraState *camera = GetPrimaryCameraState(state->ecs.get());
	WorldState *world = GetWorldState(state->ecs.get());
	PV_CHECK_OR_RETURN(
		camera && world,
		"Init",
		"InitVulkan.EcsAccess",
		"primary camera or world singleton is unavailable");
	if (!InitializeVulkanBase(&state->platform, &state->context, &state->frame)) {
		return false;
	}
	if (IsInitFailureStageRequested(InitFailureStage::AfterBootstrap)) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan",
			"intentional failure probe requested after bootstrap");
		return false;
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

	if (!CreateVoxelSceneWorld(state)) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan.CreateVoxelSceneWorld",
			"CreateVoxelSceneWorld returned false");
		return false;
	}
	if (IsInitFailureStageRequested(InitFailureStage::AfterWorld)) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan",
			"intentional failure probe requested after world creation");
		return false;
	}

	InitializeCamera(camera, &state->simulation, &state->input);
	ApplyStartupCameraOverrideFromEnvironment(camera);
	if (!SyncEcsWorldState(state->ecs.get())) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan.SyncEcsWorldState",
			"SyncEcsWorldState returned false after world creation");
		return false;
	}
	state->physics.reset(CreatePhysicsState());
	if (!state->physics ||
		!SyncPhysicsWorld(state->physics.get(), world->voxelWorld.get())) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan.CreatePhysicsState",
			"failed to create or sync physics state");
		return false;
	}

	if (!CreateSceneResources(&state->context, world, &state->render)) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan.CreateSceneResources",
			"CreateSceneResources returned false");
		return false;
	}
	if (IsInitFailureStageRequested(InitFailureStage::AfterSceneResources)) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan",
			"intentional failure probe requested after scene resources");
		return false;
	}
	if (IsInitFailureStageRequested(InitFailureStage::BeforeGraphicsPipeline)) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan",
			"intentional failure probe requested before graphics pipeline creation");
		return false;
	}

	if (!CreateGraphicsPipeline(&state->context, &state->swapchain, &state->render)) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan.CreateGraphicsPipeline",
			"CreateGraphicsPipeline returned false");
		return false;
	}
	if (IsInitFailureStageRequested(InitFailureStage::BeforeVoxelMeshingPipeline)) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan",
			"intentional failure probe requested before voxel meshing pipeline creation");
		return false;
	}
	if (!CreateVoxelMeshingPipeline(&state->context, &state->render)) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan.CreateVoxelMeshingPipeline",
			"CreateVoxelMeshingPipeline returned false");
		return false;
	}

	if (!projectv::asset::CreateModelPipeline(
			&state->context,
			state->render.graphicsPipelineLayout,
			state->swapchain.format,
			ChooseModelDepthFormat(state->context.physicalDevice),
			&state->render)) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan.CreateModelPipeline",
			"CreateModelPipeline returned false");
		return false;
	}

	if (!projectv::asset::LoadAndRegisterModelsFromManifest(
			&state->context,
			state->context.commandPool,
			state->context.queue,
			&state->render)) {
		runtime::LogRuntimeFailure(
			"Init",
			"InitVulkan.LoadAndRegisterModelsFromManifest",
			"LoadAndRegisterModelsFromManifest returned false");
		return false;
	}

	// M5.1b follow-up: lift the loaded `modelInstances` to sit
	// cleanly on top of the voxel floor instead of half-submerged
	// in it. The same call is also wired into
	// `FinalizeActiveVoxelWorldReload` for F5 / F6 / replay; this
	// startup branch covers the initial VoxelLab default scene
	// (which doesn't go through `FinalizeActiveVoxelWorldReload`).
	//
	// M5.1d: routes through the dispatch wrapper, which honours
	// the `PROJECTV_MODEL_SNAP=centre` env var for the optional
	// centre-anchored snap (see `ModelManifestLoader.hpp` for the
	// contract). The default branch (no env var, or any other
	// value) is bottom-anchored.
	projectv::asset::SnapModelInstancesAboveGroundDispatch(*state->world.voxelWorld, &state->render);

	return true;
}

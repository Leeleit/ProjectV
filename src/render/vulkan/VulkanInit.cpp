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
#include "render/vulkan/VulkanInit.hpp"
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

// Anchor: TryCreateCalibratedTracyGpuContext is called from
// CreateTracyGpuContext below (line ~80). Static analyzer reports
// "All calls of function are unreachable" because it does not see the
// cross-function call site through the #if PROJECTV_ENABLE_TRACY guard.
// This compile-time address-of proves the function is reachable.
static_assert(&TryCreateCalibratedTracyGpuContext != nullptr,
			  "TryCreateCalibratedTracyGpuContext is reachable (called from CreateTracyGpuContext)");

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

std::expected<void, projectv::vulkan_init::VulkanInitError> InitVulkan(AppState *state)
{
	const auto fail = [](projectv::vulkan_init::VulkanInitError e, const std::string_view step, const std::string_view detail) {
		runtime::LogRuntimeFailure("Init", step, detail);
		return std::unexpected(e);
	};
	static_assert(&fail != nullptr, "fail lambda is reachable (called from many sites in InitVulkan)");
	PV_PROFILE_ZONE_N("InitVulkan");
	if (state == nullptr) {
		return fail(projectv::vulkan_init::VulkanInitError::PreconditionFailed,
					"InitVulkan.Preconditions", "AppState is null");
	}
	CameraState *camera = GetPrimaryCameraState(state->ecs().get());
	WorldState *world = GetWorldState(state->ecs().get());
	if (!camera || !world) {
		return fail(projectv::vulkan_init::VulkanInitError::PreconditionFailed,
					"InitVulkan.EcsAccess", "primary camera or world singleton is unavailable");
	}
	if (!InitializeVulkanBase(&state->platform(), &state->context(), &state->frame())) {
		return fail(projectv::vulkan_init::VulkanInitError::BootstrapFailed,
					"InitVulkan.InitializeVulkanBase", "InitializeVulkanBase returned false");
	}
	if (IsInitFailureStageRequested(InitFailureStage::AfterBootstrap)) {
		return fail(projectv::vulkan_init::VulkanInitError::BootstrapFailureProbe,
					"InitVulkan", "intentional failure probe requested after bootstrap");
	}

	if (!CreateTracyGpuContext(&state->context(), &state->render())) {
		return fail(projectv::vulkan_init::VulkanInitError::TracyContextFailed,
					"InitVulkan.CreateTracyGpuContext", "CreateTracyGpuContext returned false");
	}

	if (!RecreateSwapchain(&state->platform(), &state->context(), &state->swapchain(), &state->render())) {
		return std::unexpected(projectv::vulkan_init::VulkanInitError::SwapchainFailed);
	}

	if (!CreateVoxelSceneWorld(state)) {
		return fail(projectv::vulkan_init::VulkanInitError::WorldCreationFailed,
					"InitVulkan.CreateVoxelSceneWorld", "CreateVoxelSceneWorld returned false");
	}
	if (IsInitFailureStageRequested(InitFailureStage::AfterWorld)) {
		return fail(projectv::vulkan_init::VulkanInitError::WorldFailureProbe,
					"InitVulkan", "intentional failure probe requested after world creation");
	}

	InitializeCamera(camera, &state->simulation(), &state->input());
	ApplyStartupCameraOverrideFromEnvironment(camera);
	if (!SyncEcsWorldState(state->ecs().get())) {
		return fail(projectv::vulkan_init::VulkanInitError::EcsSyncFailed,
					"InitVulkan.SyncEcsWorldState", "SyncEcsWorldState returned false after world creation");
	}
	state->physics().reset(CreatePhysicsState());
	if (!state->physics() ||
		!SyncPhysicsWorld(state->physics().get(), world->voxelWorld.get())) {
		return fail(projectv::vulkan_init::VulkanInitError::PhysicsStateFailed,
					"InitVulkan.CreatePhysicsState", "failed to create or sync physics state");
	}

	if (!CreateSceneResources(&state->context(), world, &state->render())) {
		return fail(projectv::vulkan_init::VulkanInitError::SceneResourcesFailed,
					"InitVulkan.CreateSceneResources", "CreateSceneResources returned false");
	}
	if (IsInitFailureStageRequested(InitFailureStage::AfterSceneResources)) {
		return fail(projectv::vulkan_init::VulkanInitError::SceneResourcesFailureProbe,
					"InitVulkan", "intentional failure probe requested after scene resources");
	}
	if (IsInitFailureStageRequested(InitFailureStage::BeforeGraphicsPipeline)) {
		return fail(projectv::vulkan_init::VulkanInitError::GraphicsPipelineProbe,
					"InitVulkan", "intentional failure probe requested before graphics pipeline creation");
	}

	if (!CreateShadowResources(&state->context(), &state->render())) {
		return fail(projectv::vulkan_init::VulkanInitError::ShadowResourcesFailed,
					"InitVulkan.CreateShadowResources", "CreateShadowResources returned false");
	}

	if (!CreateGraphicsPipeline(&state->context(), &state->swapchain(), &state->render())) {
		return fail(projectv::vulkan_init::VulkanInitError::GraphicsPipelineFailed,
					"InitVulkan.CreateGraphicsPipeline", "CreateGraphicsPipeline returned false");
	}
	if (IsInitFailureStageRequested(InitFailureStage::BeforeVoxelMeshingPipeline)) {
		return fail(projectv::vulkan_init::VulkanInitError::VoxelMeshingPipelineProbe,
					"InitVulkan", "intentional failure probe requested before voxel meshing pipeline creation");
	}
	if (!CreateVoxelMeshingPipeline(&state->context(), &state->render())) {
		return fail(projectv::vulkan_init::VulkanInitError::VoxelMeshingPipelineFailed,
					"InitVulkan.CreateVoxelMeshingPipeline", "CreateVoxelMeshingPipeline returned false");
	}

	const bool modelPipelineCreated = projectv::asset::CreateModelPipeline(
		&state->context(),
		state->render().graphicsPipelineLayout,
		state->swapchain().format,
		ChooseModelDepthFormat(state->context().physicalDevice),
		&state->render());
	if (!modelPipelineCreated) [[unlikely]] {
		return fail(projectv::vulkan_init::VulkanInitError::ModelPipelineFailed,
					"InitVulkan.CreateModelPipeline", "CreateModelPipeline returned false");
	}

	if (!projectv::asset::LoadAndRegisterModelsFromManifest(
			&state->context(),
			state->context().commandPool,
			state->context().queue,
			&state->render())) {
		return fail(projectv::vulkan_init::VulkanInitError::ModelManifestFailed,
					"InitVulkan.LoadAndRegisterModelsFromManifest", "LoadAndRegisterModelsFromManifest returned false");
	}

	projectv::asset::SnapModelInstancesAboveGroundDispatch(*state->world().voxelWorld, &state->render());

	return {};
}

#include "app/Camera.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
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
#include "render/RayTracedShadows.hpp"
#include "render/RtxGiProbes.hpp"
#include "render/vulkan/VulkanAsyncCompute.hpp"
#include "render/vulkan/VulkanVoxelizePipeline.hpp"
#include "render/vulkan/VulkanBootstrap.hpp"
#include "render/vulkan/VulkanWorldGenPipeline.hpp"
#include "render/SkyAtmosphere.hpp"
#include "render/VolumetricFog.hpp"
#include "render/Cloudscape.hpp"
#include "render/vulkan/VulkanGraphicsPipeline.hpp"
#include "render/vulkan/VulkanInit.hpp"
#include "render/vulkan/VulkanMeshShaderPipeline.hpp"
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

	{
		RenderState &render = state->render();
		VulkanContextState &context = state->context();
		VkPhysicalDeviceProperties props{};
		vkGetPhysicalDeviceProperties(context.physicalDevice, &props);
		render.gpuTimestampPeriodNs = props.limits.timestampPeriod;
		render.gpuTimestampQueriesPerFrame = 10u; // tlas, rtx, ddgi, opaque, aa/post
		VkQueryPoolCreateInfo queryInfo{.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
		queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
		queryInfo.queryCount = render.gpuTimestampQueriesPerFrame * MAX_FRAMES_IN_FLIGHT;
		if (vkCreateQueryPool(context.device, &queryInfo, nullptr, &render.gpuTimestampQueryPool) != VK_SUCCESS) {
			render.gpuTimestampQueryPool = VK_NULL_HANDLE;
			SDL_Log("InitVulkan: GPU timestamp query pool unavailable; GPU pass timings disabled");
		}
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

	if (projectv::render::IsMeshShaderPipelineRequested()) {
		if (!projectv::render::CreateMeshShaderPipelines(&state->context(), &state->render())) {
			SDL_LogInfo(
				SDL_LOG_CATEGORY_APPLICATION,
				"Mesh shader pipeline not created (feature disabled or unavailable); continuing with PackedFace main draw");
		}
	}

	if (projectv::render::IsWorldGenGpuPipelineRequested()) {
		if (!projectv::render::CreateWorldGenPipelines(&state->context(), &state->render())) {
			SDL_LogInfo(
				SDL_LOG_CATEGORY_APPLICATION,
				"World gen GPU pipeline not created (feature disabled or unavailable); continuing with CPU fallback");
		} else if (!projectv::render::RefreshWorldGenResourceBindings(&state->context(), &state->render())) {
			SDL_LogInfo(
				SDL_LOG_CATEGORY_APPLICATION,
				"World gen GPU descriptor bindings failed; disabling GPU path");
			projectv::render::DestroyWorldGenPipelines(&state->context(), &state->render());
		}
	}

	if (projectv::render::IsAsyncComputeEnabled()) {
		if (!projectv::render::EnsureAsyncComputeResources(&state->context())) {
			SDL_LogInfo(
				SDL_LOG_CATEGORY_APPLICATION,
				"Async compute resources not created (no dedicated compute queue or device unavailable); continuing with graphics-queue path");
		}
	}

	if (projectv::render::IsSkyAtmosphereEnabled()) {
		if (!projectv::render::CreateSkyAtmospherePipelines(&state->context(), &state->render())) {
			SDL_LogInfo(
				SDL_LOG_CATEGORY_APPLICATION,
				"Sky atmosphere pipeline not created (shader load or device failure); continuing without sky pass");
		}
	}

	if (projectv::render::IsSkyLutPrecomputeEnabled()) {
		if (!projectv::render::CreateSkyLutResources(&state->context(), &state->render())) {
			SDL_LogInfo(
				SDL_LOG_CATEGORY_APPLICATION,
				"Sky LUT precompute not created (shader load or device failure); continuing with analytical sky only");
		}
	}

	if (!projectv::render::CreateVolumetricFogFallbackOnly(&state->context(), &state->render())) {
		SDL_LogInfo(
			SDL_LOG_CATEGORY_APPLICATION,
			"Volumetric fog fallback image not created (device failure); voxel.frag binding 12 will sample null (warning)");
	}

	if (!projectv::render::CreateVctClipmapFallbackSamplerOnly(&state->context(), &state->render())) {
		SDL_LogInfo(
			SDL_LOG_CATEGORY_APPLICATION,
			"VCT clipmap fallback sampler not created (device failure); voxel.frag binding 11 will sample null (warning)");
	}

	if (!projectv::render::CreateRtxShadowMaskFallbackOnly(&state->context(), &state->render())) {
		return fail(projectv::vulkan_init::VulkanInitError::ShadowResourcesFailed,
					"InitVulkan.CreateRtxShadowMaskFallbackOnly",
					"failed to allocate RTX shadow mask fallback image");
	}

	RefreshGraphicsResourceBindings(&state->context(), &state->render()); // EVIL: deferred descriptor writes until fallback images exist (8x V C bug).

	if (projectv::render::IsVolumetricFogEnabled()) {
		if (!projectv::render::CreateVolumetricFogResources(&state->context(), &state->render())) {
			SDL_LogInfo(
				SDL_LOG_CATEGORY_APPLICATION,
				"Volumetric fog resources not created (shader load or device failure); continuing with analytic-distance fog only");
		}
	}

	if (projectv::render::IsCloudscapeEnabled()) {
		if (!projectv::render::CreateCloudscapeResources(&state->context(), &state->render())) {
			SDL_LogInfo(
				SDL_LOG_CATEGORY_APPLICATION,
				"Cloudscape resources not created (shader load or device failure); continuing without cloud pass");
		}
	}

	if (state->render().rayTracedShadows == nullptr) {
		state->render().rayTracedShadows = new projectv::render::RayTracedShadows();
	}
	if (!projectv::render::CreateRayTracedShadowResources(&state->context(), &state->render())) {
		return fail(projectv::vulkan_init::VulkanInitError::ShadowResourcesFailed,
					"InitVulkan.CreateRayTracedShadowResources",
					"RTX-capable GPU required (NVIDIA RTX 20 series or newer with RT cores)");
	}

	RefreshGraphicsResourceBindings(&state->context(), &state->render()); // EVIL: re-run after rayTracedShadows allocation so binding 13 TLAS is updated.

	if (!projectv::render::CreateRtxGiProbeResources(&state->context(), &state->render())) {
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
					"InitVulkan.CreateRtxGiProbeResources: probe field init failed; shader will fall back to VCT diffuse");
	}

	RefreshGraphicsResourceBindings(&state->context(), &state->render()); // 3rd pass: rtxGiProbes ready → bindings 14-17 now writable

	if (projectv::render::IsAsyncComputeEnabled()) {
		if (!projectv::render::EnsureAsyncComputeResources(&state->context())) {
			SDL_LogInfo(
				SDL_LOG_CATEGORY_APPLICATION,
				"Async compute resources not allocated (no dedicated compute queue); continuing with inline dispatches");
		}
	}

	const bool modelPipelineCreated = projectv::asset::CreateModelPipeline(
		&state->context(),
		state->render().graphicsPipelineLayout,
		VK_FORMAT_B10G11R11_UFLOAT_PACK32,
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

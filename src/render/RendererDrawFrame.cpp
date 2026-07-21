#include "render/RendererInternal.hpp"
#include "render/SceneResources.hpp"

#include "core/EnvUtils.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"
#include "debug/ProfilingGpu.hpp"
#include "fmt/format.h"
#include "render/HizCulling.hpp"
#include "render/RayTracedShadows.hpp"
#include "render/RtxGiProbes.hpp"
#include "render/vulkan/VulkanAsyncCompute.hpp"
#include "render/vulkan/VulkanGraphicsPipeline.hpp"
#include "render/vulkan/VulkanInit.hpp"
#include "render/vulkan/VulkanResult.hpp"
#include "render/vulkan/VulkanWorldGenPipeline.hpp"
#include "voxel/VoxelWorld.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <optional>

namespace {

uint32_t GetPresentWaitMaxLatencyFrames()
{
	const char *const value = projectv::core::GetEnvVar("PROJECTV_PRESENT_WAIT");
	if (value == nullptr || value[0] == '\0' || value[0] == '-') {
		return 0u;
	}
	char *end = nullptr;
	const unsigned long parsed = std::strtoul(value, &end, 10);
	if (end == value || *end != '\0' || parsed == 0u) {
		return 0u;
	}
	return parsed > std::numeric_limits<uint32_t>::max()
			   ? std::numeric_limits<uint32_t>::max()
			   : static_cast<uint32_t>(parsed);
}

VkExtent2D ScaleShadowMaskExtent(const VkExtent2D full)
{
	return projectv::render::RayTracedShadows::ResolveShadowMaskExtent(full);
}

bool IsRtxShadowPassEnabled()
{
	const char *const value = projectv::core::GetEnvVar("PROJECTV_RTX_SHADOW_PASS");
	if (value == nullptr || value[0] == '\0') {
		return true;
	}
	return !(value[0] == '0' || value[0] == 'n' || value[0] == 'N' || value[0] == 'f' || value[0] == 'F');
}

bool IsRtxTightAabbEnabled()
{
	const char *const value = projectv::core::GetEnvVar("PROJECTV_RTX_TIGHT_AABB");
	if (value == nullptr || value[0] == '\0') {
		return true;
	}
	return !(value[0] == '0' || value[0] == 'n' || value[0] == 'N' || value[0] == 'f' || value[0] == 'F');
}

uint32_t GetDdgiUpdatePeriod()
{
	const char *const value = projectv::core::GetEnvVar("PROJECTV_DDGI_UPDATE_PERIOD");
	if (value == nullptr || value[0] == '\0') {
		return 1u; // every frame (look default); N>1 amortizes; 0 disables update
	}
	char *end = nullptr;
	const unsigned long parsed = std::strtoul(value, &end, 10);
	if (end == value || *end != '\0') {
		return 1u;
	}
	return static_cast<uint32_t>(parsed);
}

uint32_t GetMaxBlasBuildsPerFrame()
{
	const char *const value = projectv::core::GetEnvVar("PROJECTV_BLAS_BUILDS_PER_FRAME");
	if (value == nullptr || value[0] == '\0') {
		return 8u;
	}
	char *end = nullptr;
	const unsigned long parsed = std::strtoul(value, &end, 10);
	if (end == value || *end != '\0' || parsed == 0u) {
		return 8u;
	}
	return static_cast<uint32_t>(std::min<unsigned long>(parsed, 256ul));
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

	DrainDeferredNanoVdbDestroysForFrame(context, *render, currentFrameIndex);

	const VkCommandBuffer cmd = frame->commandBuffers[currentFrameIndex];
	const VkFence inFlightFence = frame->inFlightFences[currentFrameIndex];
	const VkSemaphore imageAvailableSemaphore = frame->imageAvailableSemaphores[currentFrameIndex];

	// Wait BEFORE acquire: with minImageCount==MAX_FRAMES_IN_FLIGHT, acquire-first deadlocks on resize.
	const VkResult waitFencesResult = vkWaitForFences(context->device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
	if (waitFencesResult != VK_SUCCESS) {
		runtime::LogVkFailure("DrawFrame.vkWaitForFences", waitFencesResult);
		return SDL_APP_FAILURE;
	}
	projectv::render::SyncHzbUnifiedVisibilityAfterFence(context, *render, currentFrameIndex);
	if (render->rayTracedShadows != nullptr) {
		render->rayTracedShadows->SyncTraversalCountersAfterFence(*context, currentFrame);
	}
	if (render->gpuTimestampQueryPool != VK_NULL_HANDLE &&
		render->gpuTimestampQueriesReady[currentFrameIndex]) {
		const uint32_t base = static_cast<uint32_t>(currentFrameIndex) * render->gpuTimestampQueriesPerFrame;
		std::array<uint64_t, 10> stamps{};
		if (vkGetQueryPoolResults(
				context->device,
				render->gpuTimestampQueryPool,
				base,
				render->gpuTimestampQueriesPerFrame,
				sizeof(stamps),
				stamps.data(),
				sizeof(uint64_t),
				VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) {
			const auto toMs = [period = render->gpuTimestampPeriodNs](const uint64_t a, const uint64_t b) {
				return static_cast<float>((static_cast<double>(b - a) * static_cast<double>(period)) * 1.0e-6);
			};
			render->renderPassTimings.gpuTlasMs = toMs(stamps[0], stamps[1]);
			render->renderPassTimings.gpuRtxShadowMs = toMs(stamps[2], stamps[3]);
			render->renderPassTimings.gpuDdgiMs = toMs(stamps[4], stamps[5]);
			render->renderPassTimings.gpuOpaqueMs = toMs(stamps[6], stamps[7]);
			render->renderPassTimings.gpuAaPostMs = toMs(stamps[8], stamps[9]);
			render->renderPassTimings.gpuGraphicsMs =
				render->renderPassTimings.gpuOpaqueMs + render->renderPassTimings.gpuAaPostMs;
		}
	}

	if (platform->windowResized) {
		if (!RecreateSwapchain(platform, context, swapchain, render)) {
			runtime::LogRuntimeFailure(
				"Render",
				"DrawFrame.RecreateSwapchainBeforeAcquire",
				"RecreateSwapchain returned false for windowResized");
			return SDL_APP_FAILURE;
		}
		platform->windowResized = false;
		// PrepareFrame already baked descriptor sets; next iterate will re-prepare. Do not draw.
		return SDL_APP_CONTINUE;
	}

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
		return SDL_APP_CONTINUE; // fence still signaled — do not reset without a submit
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
	const uint32_t gpuTsBase =
		static_cast<uint32_t>(currentFrameIndex) * render->gpuTimestampQueriesPerFrame;
	if (render->gpuTimestampQueryPool != VK_NULL_HANDLE) {
		vkCmdResetQueryPool(cmd, render->gpuTimestampQueryPool, gpuTsBase, render->gpuTimestampQueriesPerFrame);
	}
	const auto writeGpuTs = [&](const uint32_t slot) {
		if (render->gpuTimestampQueryPool != VK_NULL_HANDLE) {
			vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, render->gpuTimestampQueryPool, gpuTsBase + slot);
		}
	};

	if (render->rayTracedShadows != nullptr) {
		const bool rtxFinishWasPending = render->rayTracedShadows->IsVoxelAwareRtxPending();
		if (rtxFinishWasPending) {
			PV_PROFILE_ZONE_N("TryFinishVoxelAwareRtxResources");
			const bool finished = render->rayTracedShadows->TryFinishVoxelAwareRtxResources(*context);
			if (finished && render->rayTracedShadows->IsVoxelAwareRtxActive()) {
				const VkExtent2D fullExtent = render->internalRenderExtent.width > 0u &&
													  render->internalRenderExtent.height > 0u
												  ? render->internalRenderExtent
												  : swapchain->extent;
				const VkExtent2D shadowExtent = ScaleShadowMaskExtent(fullExtent);
				if (!render->rayTracedShadows->RecreateShadowMaskForExtent(
						*context, shadowExtent.width, shadowExtent.height)) {
					runtime::LogRuntimeFailure(
						"Render",
						"DrawFrame.RecreateShadowMaskForExtentAfterRtxFinish",
						"RecreateShadowMaskForExtent returned false after deferred RTX ready");
					return SDL_APP_FAILURE;
				}
				if (!RefreshGraphicsResourceBindings(context, render)) {
					runtime::LogRuntimeFailure(
						"Render",
						"DrawFrame.RefreshGraphicsResourceBindingsAfterRtxFinish",
						"RefreshGraphicsResourceBindings returned false after deferred RTX ready");
					return SDL_APP_FAILURE;
				}
				// PrepareFrame cached the pre-refresh set — rebind after pool reset.
				frame->renderData.graphicsDescriptorSet =
					render->sceneFrameResources[currentFrameIndex].graphicsDescriptorSet;
				frame->renderData.meshShaderDescriptorSet =
					render->sceneFrameResources[currentFrameIndex].meshShaderDescriptorSet;
				frame->renderData.voxelMeshingDescriptorSet =
					render->sceneFrameResources[currentFrameIndex].voxelMeshingDescriptorSet;
			}
		}
		PV_PROFILE_ZONE_N("CollectAndBuildBlasChunks");
		if (state->world().voxelWorld != nullptr) {
			const VoxelWorld &world = *state->world().voxelWorld;
			const auto &rtxConfig = render->rayTracedShadows->GetConfig();
			const SceneFrameResources &aabbFrameResources = render->sceneFrameResources[currentFrameIndex];
			const auto *const packedChunkAabbs =
				static_cast<const PackedSceneChunkAabb *>(aabbFrameResources.chunkAabbMappedData);
			const bool useTightAabbs = IsRtxTightAabbEnabled();

			// Combine dirty (pendingBlasRebuildIndices) + initial (non-empty without BLAS).
			// The initial path covers scene-load chunks that never received a voxel edit
			// and therefore never landed in pendingBlasRebuildIndices.
			std::vector<uint32_t> blasChunks{};
			CollectDirtyVoxelChunkBlasRebuildRequests(
				*state->world().voxelWorld,
				&blasChunks);
			std::vector<uint32_t> initialChunks{};
			projectv::render::CollectNonBuiltBlasChunksForRayTracing(
				world,
				rtxConfig.blasDeviceAddresses,
				&initialChunks);
			for (const uint32_t index : initialChunks) {
				if (std::ranges::find(blasChunks, index) == blasChunks.end()) {
					blasChunks.push_back(index);
				}
			}

			if (!blasChunks.empty()) {
				std::vector<projectv::render::DirtyChunkRebuild> dirtyRebuilds{};
				dirtyRebuilds.reserve(blasChunks.size());
				for (const uint32_t chunkIndex : blasChunks) {
					if (chunkIndex >= world.chunks.size()) {
						continue;
					}
					const VoxelChunk &chunk = world.chunks[chunkIndex];
					projectv::render::DirtyChunkRebuild entry{};
					entry.chunkIndex = chunkIndex;
					const std::optional<VkAabbPositionsKHR> tightAabb =
						useTightAabbs && packedChunkAabbs != nullptr &&
								chunkIndex < aabbFrameResources.chunkDescriptorCount
							? projectv::render::TryBuildTightChunkAabb(packedChunkAabbs[chunkIndex])
							: std::nullopt;
					if (tightAabb.has_value()) {
						entry.aabb = *tightAabb;
					} else {
						entry.aabb.minX = static_cast<float>(chunk.min.x);
						entry.aabb.minY = static_cast<float>(chunk.min.y);
						entry.aabb.minZ = static_cast<float>(chunk.min.z);
						entry.aabb.maxX = static_cast<float>(chunk.maxExclusive.x);
						entry.aabb.maxY = static_cast<float>(chunk.maxExclusive.y);
						entry.aabb.maxZ = static_cast<float>(chunk.maxExclusive.z);
					}
					dirtyRebuilds.push_back(entry);
				}
				if (!dirtyRebuilds.empty()) {
					render->rayTracedShadows->SetBlasDirtyQueue(std::move(dirtyRebuilds));
				}
			}
		}
		uint32_t blasBuilt = 0u;
		{
			PV_PROFILE_GPU_LABEL(cmd, "RTX BLAS Build");
			PV_PROFILE_GPU_ZONE(render->tracyGraphicsContext, cmd, "RTX BLAS Build");
			blasBuilt = render->rayTracedShadows->RecordDirtyBlasBuilds(
				cmd, *context, GetMaxBlasBuildsPerFrame());
		}
		(void)blasBuilt;

		bool tlasNeedsBuild = false;
		if (state->world().voxelWorld != nullptr) { // EVIL: visible chunk list assembled from non-empty chunks with valid BLAS for TLAS population.
			std::vector<uint32_t> visibleChunkIndices{};
			std::vector<VkTransformMatrixKHR> visibleChunkTransforms{};
			const VoxelWorld &world = *state->world().voxelWorld;
			const auto &rtxConfig = render->rayTracedShadows->GetConfig();
			VkTransformMatrixKHR identityMatrix{};
			identityMatrix.matrix[0][0] = 1.0f;
			identityMatrix.matrix[1][1] = 1.0f;
			identityMatrix.matrix[2][2] = 1.0f;
			visibleChunkIndices.reserve(world.chunks.size());
			visibleChunkTransforms.reserve(world.chunks.size());
			for (size_t i = 0; i < world.chunks.size(); ++i) {
				const VoxelChunk &chunk = world.chunks[i];
				if (chunk.nonAirVoxelCount == 0u) {
					continue;
				}
				if (i >= rtxConfig.blasDeviceAddresses.size() || rtxConfig.blasDeviceAddresses[i] == 0u) {
					continue;
				}
				visibleChunkIndices.push_back(static_cast<uint32_t>(i));
				visibleChunkTransforms.push_back(identityMatrix);
			}
			if (!visibleChunkIndices.empty()) {
				tlasNeedsBuild = render->rayTracedShadows->UpdateTlas(
					*context,
					visibleChunkIndices,
					visibleChunkTransforms);
			}
		}

		if (render->rayTracedShadows->IsEnabled() && tlasNeedsBuild) {
			PV_PROFILE_GPU_ZONE(render->tracyGraphicsContext, cmd, "TLAS Build");
			ScopedPassTimer tlasTimer(render->renderPassTimings.shadowMs);
			writeGpuTs(0u);
			render->rayTracedShadows->RecordTlasBuild(cmd, *context); // EVIL: build TLAS only when instances/BLAS changed (dirty-only).
			writeGpuTs(1u);
		} else {
			writeGpuTs(0u);
			writeGpuTs(1u);
		}
		if (IsRtxShadowPassEnabled()) {
			PV_PROFILE_ZONE_N("RecordVoxelAwareRtxShadowPass");
			writeGpuTs(2u);
			const SceneFrameResources &shadowFrameResources = render->sceneFrameResources[frame->currentFrame];
			projectv::math::Mat4 inverseViewProjection =
				projectv::math::inverse(frame->renderData.graphicsPushConstants.viewProjection);
			std::array<float, 16> inverseViewProjectionFlat{};
			for (uint32_t i = 0; i < 16u; ++i) {
				inverseViewProjectionFlat[i] = inverseViewProjection.data()[i];
			}
			const float cameraPosition[3] = {
				frame->renderData.graphicsPushConstants.cameraPosition.x,
				frame->renderData.graphicsPushConstants.cameraPosition.y,
				frame->renderData.graphicsPushConstants.cameraPosition.z};
			const float cameraForward[3] = {
				frame->renderData.graphicsPushConstants.cameraForward.x,
				frame->renderData.graphicsPushConstants.cameraForward.y,
				frame->renderData.graphicsPushConstants.cameraForward.z};
			const VkExtent2D fullExtent = render->internalRenderExtent.width > 0u &&
												  render->internalRenderExtent.height > 0u
											  ? render->internalRenderExtent
											  : swapchain->extent;
			const VkExtent2D maskExtent = ScaleShadowMaskExtent(fullExtent);
			render->rayTracedShadows->RecordVoxelAwareRtxShadowPass(
				cmd,
				render->tracyGraphicsContext,
				*context,
				frame->currentFrame,
				frame->renderData.chunkDescriptorBuffer,
				shadowFrameResources.sceneLightingBuffer,
				frame->renderData.chunkVoxelPayloadBuffer,
				inverseViewProjectionFlat.data(),
				cameraPosition,
				cameraForward,
				maskExtent.width,
				maskExtent.height);
			writeGpuTs(3u);
		} else {
			writeGpuTs(2u);
			writeGpuTs(3u);
		}
	} else {
		writeGpuTs(0u);
		writeGpuTs(1u);
		writeGpuTs(2u);
		writeGpuTs(3u);
	}

	if (render->rtxGiProbes != nullptr && render->rtxGiProbes->IsEnabled()) {
		const uint32_t ddgiPeriod = GetDdgiUpdatePeriod();
		static uint32_t ddgiFrameCounter = 0u;
		const bool runDdgi = ddgiPeriod > 0u && (ddgiFrameCounter++ % ddgiPeriod) == 0u;
		VkAccelerationStructureKHR tlas = render->rayTracedShadows != nullptr ? render->rayTracedShadows->GetTlas() : VK_NULL_HANDLE;
		if (runDdgi && tlas != VK_NULL_HANDLE) {
			PV_PROFILE_ZONE_N("RecordRtxGiProbeUpdatePass");
			writeGpuTs(4u);
			const SceneFrameResources &frameResources = render->sceneFrameResources[frame->currentFrame];
			projectv::render::RecordRtxGiProbeUpdatePass(
				cmd,
				render->rtxGiProbes,
				*context,
				frame->currentFrame,
				frame->renderData.chunkDescriptorBuffer,
				frameResources.sceneLightingBuffer,
				frame->renderData.chunkVoxelPayloadBuffer,
				render->materialVisualBuffer,
				tlas,
				frame->renderData);
			writeGpuTs(5u);
		} else {
			writeGpuTs(4u);
			writeGpuTs(5u);
		}
	} else {
		writeGpuTs(4u);
		writeGpuTs(5u);
	}

	writeGpuTs(6u);
	RecordGraphicsCommands(
		*render,
		*swapchain,
		frame->renderData,
		*context,
		cmd,
		imageIndex,
		[&] {
			writeGpuTs(7u); // end opaque
			writeGpuTs(8u); // start AA/post
		});
	writeGpuTs(9u);

	if (render->rayTracedShadows != nullptr) {
		render->rayTracedShadows->RecordDebugReport();
	}

	const bool asyncComputePathActive =
		projectv::render::IsAsyncComputeEnabled() &&
		projectv::render::IsAsyncComputeResourcesAllocated(*context) &&
		render->worldGenPipelineEnabled;

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
				voxelWorld->chunkSize,
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

	std::array<VkSemaphoreSubmitInfo, 1u> allSignalSemaphoreInfos{};
	allSignalSemaphoreInfos[0] = signalSemaphoreInfo;
	constexpr uint32_t signalSemaphoreInfoCount = 1u;

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
	render->gpuTimestampQueriesReady[currentFrameIndex] = true;

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &submitSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain->handle;
	presentInfo.pImageIndices = &imageIndex;

	const uint32_t presentWaitMaxLatencyFrames = GetPresentWaitMaxLatencyFrames();
	const bool presentIdActive = context->supportsPresentId;
	const bool presentWaitActive = context->supportsPresentWait && presentWaitMaxLatencyFrames > 0u;
	const uint64_t presentId = presentIdActive ? swapchain->nextPresentId++ : 0u;
	VkPresentIdKHR presentIdInfo{};
	if (presentIdActive) {
		presentIdInfo.sType = VK_STRUCTURE_TYPE_PRESENT_ID_KHR;
		presentIdInfo.swapchainCount = 1u;
		presentIdInfo.pPresentIds = &presentId;
		presentInfo.pNext = &presentIdInfo;
	}

	VkResult presentRes = VK_SUCCESS;
	{
		ScopedPassTimer presentTimer(render->renderPassTimings.presentMs);
		presentRes = vkQueuePresentKHR(context->queue, &presentInfo);
		if (presentWaitActive && presentRes == VK_SUCCESS && presentId > presentWaitMaxLatencyFrames) {
			if (!swapchain->presentWaitLogged) {
				SDL_Log("Render: present_wait active with max latency %u frame(s)", presentWaitMaxLatencyFrames);
				swapchain->presentWaitLogged = true;
			}
			const VkResult waitPresentResult = vkWaitForPresentKHR(
				context->device,
				swapchain->handle,
				presentId - presentWaitMaxLatencyFrames,
				UINT64_MAX);
			if (waitPresentResult == VK_ERROR_OUT_OF_DATE_KHR) {
				presentRes = waitPresentResult;
			} else if (waitPresentResult != VK_SUCCESS) {
				runtime::LogVkFailure("DrawFrame.vkWaitForPresentKHR", waitPresentResult);
				return SDL_APP_FAILURE;
			}
		}
	}
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

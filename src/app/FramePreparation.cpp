import projectv.math; // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "app/FramePreparation.hpp"

#include "app/Camera.hpp"
#include "app/ModelGravigun.hpp"
#include "c_kernels/FrustumCulling.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "debug/DebugHud.hpp"
#include "debug/DebugOverlays.hpp"
#include "debug/Profiling.hpp"
#include "render/SceneResources.hpp"
#include "render/LodDownsampleGpuConsume.hpp"
#include "render/Taa.hpp"
#include "voxel/ChunkStreamer.hpp"
#include "voxel/VoxelWorld.hpp"

void BuildVisibleModelInstanceList(
	const ChunkCullingParameters &parameters,
	RenderState *render)
{
	if (!render) {
		return;
	}
	if (render->modelInstances.empty()) {

		render->visibleModelInstances.clear();
		return;
	}

	std::vector<ModelInstanceData> cullCandidates;
	cullCandidates.reserve(render->modelInstances.size());
	for (const ModelInstanceData &instance : render->modelInstances) {
		if (instance.indexCount == 0 ||
			instance.vertexBuffer == VK_NULL_HANDLE ||
			instance.indexBuffer == VK_NULL_HANDLE) {
			continue;
		}
		cullCandidates.push_back(instance);
	}

	const std::vector<ModelInstanceData> visible = projectv::c_kernels::FilterVisibleInstances(
		std::span<const ModelInstanceData>(cullCandidates.data(), cullCandidates.size()),
		parameters);

	render->visibleModelInstances.clear();
	render->visibleModelInstances.reserve(visible.size());
	for (const ModelInstanceData &instance : visible) {
		render->visibleModelInstances.push_back(instance);
	}
}

bool PrepareFrameRenderData(
	VulkanContextState *context,
	const SwapchainState *swapchain,
	const CameraState *camera,
	const InteractionState *interaction,
	const DebugState *debug,
	WorldState *world,
	RenderState *render,
	FrameState *frame,
	InputState *input)
{
	PV_PROFILE_ZONE_N("PrepareFrameRenderData");
	PV_CHECK_OR_RETURN(
		context && swapchain && camera && interaction && debug && world && render && frame,
		"Frame",
		"PrepareFrameRenderData.Preconditions",
		"context/swapchain/camera/interaction/debug/world/render/frame is incomplete");
	const size_t frameIndex = frame->currentFrame;
	PV_CHECK_OR_RETURN(
		context->device && frameIndex < frame->inFlightFences.size(),
		"Frame",
		"PrepareFrameRenderData.FrameFence",
		"device is null or frame fence index is out of range");
	PV_CHECK_OR_RETURN(
		frameIndex < render->sceneFrameResources.size(),
		"Frame",
		"PrepareFrameRenderData.SceneFrameResources",
		"frame index is out of range for scene frame resources");

	if (world->voxelWorld) {
		CollectDirtyVoxelChunkRebuildRequests(*world->voxelWorld, &render->pendingChunkRebuildIndices);
	}

	const ChunkCullingParameters chunkCullingParameters = BuildChunkCullingParameters(
		*camera,
		swapchain->extent,
		GetCameraVisibleSceneMaxDistance(*camera));

	const VkFence inFlightFence = frame->inFlightFences[frameIndex];
	VkResult waitResult = vkWaitForFences(
		context->device, 1, &inFlightFence, VK_TRUE, kVulkanFenceWaitTimeoutNs);
	if (waitResult == VK_TIMEOUT) {

		PV_PROFILE_ZONE_N("PrepareFrameRenderData.GPUStallFallback");
		waitResult = vkWaitForFences(
			context->device, 1, &inFlightFence, VK_TRUE, kVulkanFenceWaitTimeoutUnboundedNs);
	}
	if (waitResult != VK_SUCCESS) {
		runtime::LogVkFailure("PrepareFrameRenderData.vkWaitForFences", waitResult);
		return false;
	}

	if (!UpdateSceneResources(world, render, chunkCullingParameters, swapchain->extent)) {
		runtime::LogRuntimeFailure(
			"Frame",
			"PrepareFrameRenderData.UpdateSceneResources",
			"UpdateSceneResources returned false");
		return false;
	}

	if (world->voxelWorld && IsLodDownsampleEnabled()) {
		AssignLodLevels(
			*world->voxelWorld,
			camera->position.x,
			camera->position.y,
			camera->position.z);
		const uint32_t lodJobsProcessed = RunLodDownsampleJobs(*world->voxelWorld);
		profiling::PlotValue("LOD Downsample Chunks", static_cast<int64_t>(lodJobsProcessed));
		profiling::PlotValue("LOD Active Chunks", static_cast<int64_t>(world->voxelWorld->stats.activeChunkCount));
		if (projectv::render::IsLodDownsampledGpuConsumeEnabled()) {
			projectv::render::RefreshLodDownsampledBuffers(
				context,
				render,
				*world->voxelWorld);
		}
	}

	if (projectv::voxel::IsChunkStreamingEnabled() && world->voxelWorld) {
		constexpr uint32_t kMaxChunksPerFrame = 8u;
		uint32_t chunksDrained = 0u;
		for (uint32_t i = 0; i < kMaxChunksPerFrame; ++i) {
			std::expected<projectv::voxel::ChunkData, projectv::voxel::ChunkStreamError> dequeueResult =
				projectv::voxel::TryDequeueChunkData();
			if (!dequeueResult.has_value()) {
				if (dequeueResult.error() == projectv::voxel::ChunkStreamError::QueueFull) {
					break;
				}
				break;
			}
			++chunksDrained;
		}
		profiling::PlotValue("Chunk Stream Drained", static_cast<int64_t>(chunksDrained));
		profiling::PlotValue("Chunk Stream Pending", static_cast<int64_t>(projectv::voxel::DrainChunkStreamQueueSize()));

		if (projectv::voxel::IsChunkStreamerPrebakeReady()) {
			const uint32_t kPreloadRadiusChunks = 8u;
			const uint32_t enqueued = projectv::voxel::PreloadChunksAroundCamera(
				*world->voxelWorld,
				camera->position.x,
				camera->position.y,
				camera->position.z,
				kPreloadRadiusChunks);
			profiling::PlotValue("Chunk Streamer Preload Queue Depth", static_cast<int64_t>(enqueued));
		}
	}

	if (world->voxelWorld && !render->completedChunkRebuildIndices.empty()) {
		CommitDirtyVoxelChunkRebuildRequests(*world->voxelWorld, render->completedChunkRebuildIndices);
		render->completedChunkRebuildIndices.clear();
	}

	if (!UploadSceneFrameResources(context, *render, frame->currentFrame)) {
		runtime::LogRuntimeFailure(
			"Frame",
			"PrepareFrameRenderData.UploadSceneFrameResources",
			"UploadSceneFrameResources returned false");
		return false;
	}

	if (!UpdateSceneFrameChunkVisibility(*render, frame->currentFrame, chunkCullingParameters)) {
		runtime::LogRuntimeFailure(
			"Frame",
			"PrepareFrameRenderData.UpdateSceneFrameChunkVisibility",
			"UpdateSceneFrameChunkVisibility returned false");
		return false;
	}

	BuildVisibleModelInstanceList(chunkCullingParameters, render);

	if (world->voxelWorld) {
		static projectv::app::ModelGravigunState gravigunState;
		TickModelGravigun(
			&gravigunState,
			*world->voxelWorld,
			*camera,
			swapchain->extent,
			render,
			input);
	}

	SceneFrameResources &sceneFrameResources = render->sceneFrameResources[frameIndex];
	sceneFrameResources.debugHudVertexCount = 0;
	if (sceneFrameResources.debugHudVertexMappedData) {
		sceneFrameResources.debugHudVertexCount = BuildDebugHudVertices(
			debug->stats,
			*camera,
			*interaction,
			debug->hudVisible,
			swapchain->extent,
			static_cast<DebugHudVertex *>(sceneFrameResources.debugHudVertexMappedData),
			DEBUG_HUD_MAX_VERTEX_COUNT);
	}
	if (world->voxelWorld && sceneFrameResources.chunkAabbMappedData) {
		RefreshChunkAabbBuffer(
			std::span<const VoxelChunk>(world->voxelWorld->chunks.data(), world->voxelWorld->chunks.size()),
			std::span<const PackedSceneChunkDescriptor>(render->sceneChunkDescriptors.data(), render->sceneChunkDescriptors.size()),
			sceneFrameResources);
	}
	frame->renderData.frameIndex = frame->currentFrame;
	frame->renderData.packedFaceBuffer = sceneFrameResources.packedFaceBuffer;
	frame->renderData.chunkDescriptorBuffer = sceneFrameResources.chunkDescriptorBuffer;
	frame->renderData.chunkVoxelPayloadBuffer = sceneFrameResources.chunkVoxelPayloadBuffer;
	frame->renderData.debugHudVertexBuffer = sceneFrameResources.debugHudVertexBuffer;
	frame->renderData.chunkAabbBuffer = sceneFrameResources.chunkAabbBuffer;
	frame->renderData.visibilityMaskBuffer = sceneFrameResources.visibilityMaskBuffer;
	frame->renderData.hzbVisibleCountBuffer = sceneFrameResources.hzbVisibleCountBuffer;
	frame->renderData.graphicsDescriptorSet = sceneFrameResources.graphicsDescriptorSet;
	frame->renderData.voxelMeshingDescriptorSet = sceneFrameResources.voxelMeshingDescriptorSet;
	frame->renderData.hizCullingDescriptorSet = sceneFrameResources.hizCullingDescriptorSet;
	frame->renderData.meshShaderDescriptorSet = sceneFrameResources.meshShaderDescriptorSet;
	frame->renderData.taaResolveDescriptorSet = render->taaResolveDescriptorSets[frameIndex];
	frame->renderData.opaqueIndirectBuffer = sceneFrameResources.opaqueIndirectBuffer;
	frame->renderData.transparentIndirectBuffer = sceneFrameResources.transparentIndirectBuffer;
	frame->renderData.chunkDescriptorCount = sceneFrameResources.chunkDescriptorCount;
	frame->renderData.chunkCullingParameters = chunkCullingParameters;
	frame->renderData.dirtyChunkCount = sceneFrameResources.dirtyChunkCount;
	frame->renderData.opaqueFaceCount = sceneFrameResources.opaqueFaceCount;
	frame->renderData.transparentFaceCount = sceneFrameResources.transparentFaceCount;
	frame->renderData.debugHudVertexCount = sceneFrameResources.debugHudVertexCount;
	frame->renderData.debugUiVisible = debug->hudVisible;
	frame->renderData.interactionSelection = debug->hudVisible ? interaction->selection : InteractionSelectionState{};
	BuildDebugOverlayBoxes(
		world->voxelWorld.get(),
		*interaction,
		*debug,
		&frame->renderData.debugOverlayBoxes,
		*camera,
		*render);
	const std::array taaPixelJitter = render->taaEnabled
										  ? projectv::taa::AdvanceTaaPixelJitter(&render->taaFrameCounter)
										  : std::array{0.0f, 0.0f};

	render->taaJitterX = taaPixelJitter[0] * render->taaJitterScale;
	render->taaJitterY = taaPixelJitter[1] * render->taaJitterScale;
	frame->renderData.graphicsPushConstants = {};
	if (swapchain->extent.width > 0 && swapchain->extent.height > 0) {
		frame->renderData.graphicsPushConstants = BuildGraphicsPushConstants(
			*camera,
			swapchain->extent,
			render->taaJitterX,
			render->taaJitterY);
	}

	constexpr float kTaaCameraCutThreshold = 0.10f;

	if (render->taaEnabled && render->taaPrevViewProjectionMatrixInitialized) {
		const auto &currentVP = frame->renderData.graphicsPushConstants.viewProjection;
		const auto &prevVP = render->taaPrevViewProjectionMatrix;
		float maxDelta = 0.0f;
		const float *currentData = currentVP.data();
		const float *prevData = prevVP.data();
		for (size_t i = 0; i < 16; ++i) {
			const float delta = std::abs(currentData[i] - prevData[i]);
			if (delta > maxDelta) {
				maxDelta = delta;
			}
		}
		if (maxDelta > kTaaCameraCutThreshold) {
			render->taaHistoryValid = false;
			++render->taaCameraCutCount;
		}
		if (maxDelta > render->taaCameraCutMaxDelta) {
			render->taaCameraCutMaxDelta = maxDelta;
		}
	}

	if (render->taaEnabled) {
		render->taaPrevViewProjectionMatrix = frame->renderData.graphicsPushConstants.viewProjection;
		render->taaPrevViewProjectionMatrixInitialized = true;
	}
	if (world->voxelWorld) {
		frame->renderData.graphicsPushConstants.worldMinAndChunkSize = {
			world->voxelWorld->min.x,
			world->voxelWorld->min.y,
			world->voxelWorld->min.z,
			world->voxelWorld->chunkSize,
		};
		frame->renderData.graphicsPushConstants.chunkGridAndFlags = {
			static_cast<uint32_t>(world->voxelWorld->chunkCountX),
			static_cast<uint32_t>(world->voxelWorld->chunkCountY),
			static_cast<uint32_t>(world->voxelWorld->chunkCountZ),
			0u,
		};
	}
	frame->renderData.voxelMeshingPushConstants = {};
	if (world->voxelWorld) {
		frame->renderData.voxelMeshingPushConstants.worldMinAndChunkSize = {
			world->voxelWorld->min.x,
			world->voxelWorld->min.y,
			world->voxelWorld->min.z,
			world->voxelWorld->chunkSize,
		};
		frame->renderData.voxelMeshingPushConstants.worldMaxExclusiveAndChunkCount = {
			world->voxelWorld->maxExclusive.x,
			world->voxelWorld->maxExclusive.y,
			world->voxelWorld->maxExclusive.z,
			static_cast<int32_t>(sceneFrameResources.chunkDescriptorCount),
		};
		frame->renderData.voxelMeshingPushConstants.chunkGridAndTransparentFaceBase = {
			static_cast<uint32_t>(world->voxelWorld->chunkCountX),
			static_cast<uint32_t>(world->voxelWorld->chunkCountY),
			static_cast<uint32_t>(world->voxelWorld->chunkCountZ),
			render->sceneTransparentFaceBase,
		};
		frame->renderData.voxelMeshingPushConstants.faceCapacities = {
			render->sceneFaceCapacity,
			render->sceneFaceCapacity,
			sceneFrameResources.dirtyChunkCount,
			0u,
		};
	}

	return true;
}

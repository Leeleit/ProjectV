#include "app/FramePreparation.hpp"

#include "app/Camera.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "debug/DebugHud.hpp"
#include "debug/DebugOverlays.hpp"
#include "debug/Profiling.hpp"
#include "render/SceneResources.hpp"
#include "render/Taa.hpp"
#include "voxel/VoxelWorld.hpp"

void BuildVisibleModelInstanceList(
	const ChunkCullingParameters &parameters,
	RenderState *render)
{
	if (!render) {
		return;
	}
	if (render->modelInstances.empty()) {
		// No models loaded (manifest empty or never set) — keep
		// the culled list empty and skip the per-instance loop
		// entirely. Cheap, and the renderer already short-circuits
		// on an empty `visibleModelInstances`.
		render->visibleModelInstances.clear();
		return;
	}
	render->visibleModelInstances.clear();
	render->visibleModelInstances.reserve(render->modelInstances.size());
	for (const ModelInstanceData &instance : render->modelInstances) {
		// Skip empty registrations: the registry/loader can
		// produce entries with `indexCount == 0` or null buffers
		// (e.g. a primitive that failed to upload) and those
		// would have been skipped by the renderer anyway — no
		// point culling them through the frustum math first.
		if (instance.indexCount == 0 ||
			instance.vertexBuffer == VK_NULL_HANDLE ||
			instance.indexBuffer == VK_NULL_HANDLE) {
			continue;
		}
		if (IsAabbVisibleAgainstCameraFrustum(
				instance.worldAabbMin,
				instance.worldAabbMax,
				parameters)) {
			render->visibleModelInstances.push_back(instance);
		}
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
	FrameState *frame)
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
	const VkResult waitResult = vkWaitForFences(context->device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
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

	if (world->voxelWorld && !render->completedChunkRebuildIndices.empty()) {
		CommitDirtyVoxelChunkRebuildRequests(*world->voxelWorld, render->completedChunkRebuildIndices);
		render->completedChunkRebuildIndices.clear();
	}

	if (!UploadSceneFrameResources(*render, frame->currentFrame)) {
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

	// M5: per-frame model-instance frustum cull. The same camera
	// basis that drives chunk visibility (`chunkCullingParameters`,
	// built above) is reused so chunk cull + model cull share a
	// consistent view of the frustum. Off-screen and max-distance
	// instances never reach `RecordModelCommands`. Empty manifests
	// short-circuit to a clear; non-empty manifests re-use
	// `visibleModelInstances` capacity to avoid per-frame
	// allocations.
	BuildVisibleModelInstanceList(chunkCullingParameters, render);

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
	frame->renderData.frameIndex = frame->currentFrame;
	frame->renderData.packedFaceBuffer = sceneFrameResources.packedFaceBuffer;
	frame->renderData.chunkDescriptorBuffer = sceneFrameResources.chunkDescriptorBuffer;
	frame->renderData.chunkVoxelPayloadBuffer = sceneFrameResources.chunkVoxelPayloadBuffer;
	frame->renderData.debugHudVertexBuffer = sceneFrameResources.debugHudVertexBuffer;
	frame->renderData.graphicsDescriptorSet = sceneFrameResources.graphicsDescriptorSet;
	frame->renderData.shadowDescriptorSet = sceneFrameResources.shadowDescriptorSet;
	frame->renderData.voxelMeshingDescriptorSet = sceneFrameResources.voxelMeshingDescriptorSet;
	frame->renderData.taaResolveDescriptorSet = render->taaResolveDescriptorSets[frameIndex];
	frame->renderData.opaqueIndirectBuffer = sceneFrameResources.opaqueIndirectBuffer;
	frame->renderData.shadowIndirectBuffer = sceneFrameResources.shadowIndirectBuffer;
	frame->renderData.transparentIndirectBuffer = sceneFrameResources.transparentIndirectBuffer;
	frame->renderData.chunkDescriptorCount = sceneFrameResources.chunkDescriptorCount;
	frame->renderData.shadowIndirectCommandCount = sceneFrameResources.shadowIndirectCommandCount;
	frame->renderData.shadowCascadeVisibleChunkCounts = sceneFrameResources.shadowCascadeVisibleChunkCounts;
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
		&frame->renderData.debugOverlayBoxes);
	// TAA jitter: advance the 8-tap Halton(2,3) sub-pixel sequence and stash
	// the offset so the next frame can reproject through it. The jitter sits
	// in pixel units here; `BuildGraphicsPushConstants` converts it to NDC
	// when it writes the projection matrix. TAA is the source of the
	// anti-jitter that closed the user-reported 2026-06-11 jitter bug, so
	// even at this hook we treat `taaEnabled` as a master gate and force
	// the jitter to zero when it is off.
	const std::array<float, 2> taaPixelJitter = render->taaEnabled
		? projectv::taa::AdvanceTaaPixelJitter(&render->taaFrameCounter)
		: std::array<float, 2>{0.0f, 0.0f};
	// `taaJitterScale` is the per-pass TAA tuning-ladder multiplier
	// (live `;`/`'` keys, see `InputAction::*TaaJitterScale`). At 1.0 the
	// behaviour matches the pre-ladder Halton output; 0.0 freezes the
	// projection jitter entirely, 2.0 lets it wander across a full pixel.
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
	// Stash the current frame's viewProjection as the next frame's `prev`.
	// The TAA resolve pass consumes `prevViewProjectionMatrix` from
	// `VoxelSceneLighting`; on the *first* frame `taaPrevViewProjectionMatrix`
	// is zero-initialised, so the first resolve correctly treats the history
	// as invalid and falls back to the current sample only.
	if (render->taaEnabled) {
		render->taaPrevViewProjectionMatrix = frame->renderData.graphicsPushConstants.viewProjection;
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

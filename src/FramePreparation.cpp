#include "FramePreparation.hpp"

#include "Camera.hpp"
#include "Profiling.hpp"
#include "SceneResources.hpp"
#include "VoxelWorld.hpp"

bool PrepareFrameRenderData(
	VulkanContextState *context,
	const SwapchainState *swapchain,
	const CameraState *camera,
	WorldState *world,
	RenderState *render,
	FrameState *frame)
{
	PV_PROFILE_ZONE_N("PrepareFrameRenderData");
	if (!context || !swapchain || !camera || !world || !render || !frame) {
		return false;
	}
	const size_t frameIndex = frame->currentFrame;
	if (!context->device || frameIndex >= frame->inFlightFences.size()) {
		return false;
	}
	if (frameIndex >= render->sceneFrameResources.size()) {
		return false;
	}

	if (world->voxelWorld) {
		CollectDirtyVoxelChunkRebuildRequests(*world->voxelWorld, &render->pendingChunkRebuildIndices);
	}

	if (!UpdateSceneResources(world, render)) {
		return false;
	}

	if (world->voxelWorld && !render->completedChunkRebuildIndices.empty()) {
		CommitDirtyVoxelChunkRebuildRequests(*world->voxelWorld, render->completedChunkRebuildIndices);
		render->completedChunkRebuildIndices.clear();
	}

	const VkFence inFlightFence = frame->inFlightFences[frameIndex];
	if (vkWaitForFences(context->device, 1, &inFlightFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
		SDL_Log("vkWaitForFences failed");
		return false;
	}

	if (!UploadSceneFrameResources(*render, frame->currentFrame)) {
		return false;
	}

	const SceneFrameResources &sceneFrameResources = render->sceneFrameResources[frameIndex];
	frame->renderData.frameIndex = frame->currentFrame;
	frame->renderData.packedFaceBuffer = sceneFrameResources.packedFaceBuffer;
	frame->renderData.chunkDescriptorBuffer = sceneFrameResources.chunkDescriptorBuffer;
	frame->renderData.chunkVoxelPayloadBuffer = sceneFrameResources.chunkVoxelPayloadBuffer;
	frame->renderData.graphicsDescriptorSet = sceneFrameResources.graphicsDescriptorSet;
	frame->renderData.voxelMeshingDescriptorSet = sceneFrameResources.voxelMeshingDescriptorSet;
	frame->renderData.opaqueIndirectBuffer = sceneFrameResources.opaqueIndirectBuffer;
	frame->renderData.transparentIndirectBuffer = sceneFrameResources.transparentIndirectBuffer;
	frame->renderData.chunkDescriptorCount = sceneFrameResources.chunkDescriptorCount;
	frame->renderData.dirtyChunkCount = sceneFrameResources.dirtyChunkCount;
	frame->renderData.opaqueFaceCount = sceneFrameResources.opaqueFaceCount;
	frame->renderData.transparentFaceCount = sceneFrameResources.transparentFaceCount;
	frame->renderData.graphicsPushConstants = {};
	if (swapchain->extent.width > 0 && swapchain->extent.height > 0) {
		frame->renderData.graphicsPushConstants = BuildGraphicsPushConstants(*camera, swapchain->extent);
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

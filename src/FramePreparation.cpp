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
	frame->renderData.packedVertexBuffer = sceneFrameResources.packedVertexBuffer;
	frame->renderData.chunkDescriptorBuffer = sceneFrameResources.chunkDescriptorBuffer;
	frame->renderData.graphicsDescriptorSet = sceneFrameResources.graphicsDescriptorSet;
	frame->renderData.chunkDescriptorCount = sceneFrameResources.chunkDescriptorCount;
	frame->renderData.vertexCount = sceneFrameResources.vertexCount;
	frame->renderData.opaqueVertexCount = sceneFrameResources.opaqueVertexCount;
	frame->renderData.transparentVertexCount = sceneFrameResources.transparentVertexCount;
	frame->renderData.opaqueChunkDrawRanges =
		render->opaqueChunkDrawRanges.empty() ? nullptr : render->opaqueChunkDrawRanges.data();
	frame->renderData.opaqueChunkDrawRangeCount = static_cast<uint32_t>(render->opaqueChunkDrawRanges.size());
	frame->renderData.transparentChunkDrawRanges =
		render->transparentChunkDrawRanges.empty() ? nullptr : render->transparentChunkDrawRanges.data();
	frame->renderData.transparentChunkDrawRangeCount =
		static_cast<uint32_t>(render->transparentChunkDrawRanges.size());
	frame->renderData.graphicsPushConstants = {};
	if (swapchain->extent.width > 0 && swapchain->extent.height > 0) {
		frame->renderData.graphicsPushConstants = BuildGraphicsPushConstants(*camera, swapchain->extent);
	}

	return true;
}

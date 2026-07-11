#include "render/SceneResourcesInternal.hpp"

void DestroySceneResources(
	VulkanContextState *context,
	RenderState *render)
{
	if (!context || !render || !context->allocator) {
		return;
	}

	DrainAllDeferredNanoVdbDestroys(context, *render);

	if (context->device != VK_NULL_HANDLE) {
		const VkResult idleResult = vkDeviceWaitIdle(context->device);
		if (idleResult != VK_SUCCESS) {
			runtime::LogVkFailure(
				"DestroySceneResources.vkDeviceWaitIdle",
				idleResult);
		}
	}

	for (auto &[packedFaceMappedData, packedFaceBuffer, packedFaceAllocation, debugHudVertexMappedData, debugHudVertexBuffer, debugHudVertexAllocation, chunkDescriptorMappedData, chunkDescriptorBuffer, chunkDescriptorAllocation, chunkVoxelPayloadMappedData, chunkVoxelPayloadBuffer, chunkVoxelPayloadAllocation, opaqueIndirectMappedData, opaqueIndirectBuffer, opaqueIndirectAllocation, transparentIndirectMappedData, transparentIndirectBuffer, transparentIndirectAllocation, dirtyChunkIndexMappedData, dirtyChunkIndexBuffer, dirtyChunkIndexAllocation, chunkCullingMappedData, chunkCullingBuffer, chunkCullingAllocation, sceneLightingMappedData, sceneLightingBuffer, sceneLightingAllocation, chunkAabbMappedData, chunkAabbBuffer, chunkAabbAllocation, visibilityMaskMappedData, visibilityMaskBuffer, visibilityMaskAllocation, hzbVisibleCountMappedData, hzbVisibleCountBuffer, hzbVisibleCountAllocation, hzbPerChunkMipMappedData, hzbPerChunkMipBuffer, hzbPerChunkMipAllocation, hzbPerChunkMipCapacityBytes, lodDownsampledVoxelPayloadMappedData, lodDownsampledVoxelPayloadBuffer, lodDownsampledVoxelPayloadAllocation, lodDownsampledVoxelPayloadCapacityBytes, chunkLodLevelsMappedData, chunkLodLevelsBuffer, chunkLodLevelsAllocation, chunkLodLevelsCapacity, visibleChunkIdMappedData, visibleChunkIdBuffer, visibleChunkIdAllocation, visibilityCounterMappedData, visibilityCounterBuffer, visibilityCounterAllocation, fluidCaSourceMappedData, fluidCaSourceBuffer, fluidCaSourceAllocation, fluidCaDestinationMappedData, fluidCaDestinationBuffer, fluidCaDestinationAllocation, fluidCaActiveChunkIdMappedData, fluidCaActiveChunkIdBuffer, fluidCaActiveChunkIdAllocation, fluidCaStatsMappedData, fluidCaStatsBuffer, fluidCaStatsAllocation, nanovdbUpperMappedData, nanovdbUpperBuffer, nanovdbUpperAllocation, nanovdbUpperCapacityBytes, nanovdbLowerMappedData, nanovdbLowerBuffer, nanovdbLowerAllocation, nanovdbLowerCapacityBytes, nanovdbLeafMappedData, nanovdbLeafBuffer, nanovdbLeafAllocation, nanovdbLeafCapacityBytes, nanovdbMaterialMappedData, nanovdbMaterialBuffer, nanovdbMaterialAllocation, nanovdbMaterialCapacityBytes, worldGenVoxelMappedData, worldGenVoxelBuffer, worldGenVoxelAllocation, worldGenVoxelCapacityBytes, worldGenDescriptorSet, graphicsDescriptorSet, meshShaderDescriptorSet, voxelMeshingDescriptorSet, hizCullingDescriptorSet, fluidCaDescriptorSet, vctVoxelizeDescriptorSet, uploadedSceneVersion, uploadedVoxelPayloadVersion, meshedSceneVersion, uploadedNanoVdbVersion, chunkDescriptorCount, dirtyChunkCount, opaqueFaceCount, transparentFaceCount, debugHudVertexCount] : render->sceneFrameResources) {
		if (packedFaceBuffer && packedFaceAllocation) {
			profiling::RecordFree(packedFaceAllocation, "ScenePackedFaceBufferAllocation");
			vmaDestroyBuffer(context->allocator, packedFaceBuffer, packedFaceAllocation);
		}
		if (debugHudVertexBuffer && debugHudVertexAllocation) {
			profiling::RecordFree(debugHudVertexAllocation, "SceneDebugHudVertexBufferAllocation");
			vmaDestroyBuffer(context->allocator, debugHudVertexBuffer, debugHudVertexAllocation);
		}
		if (chunkDescriptorBuffer && chunkDescriptorAllocation) {
			profiling::RecordFree(chunkDescriptorAllocation, "SceneChunkDescriptorBufferAllocation");
			vmaDestroyBuffer(context->allocator, chunkDescriptorBuffer, chunkDescriptorAllocation);
		}
		if (chunkVoxelPayloadBuffer && chunkVoxelPayloadAllocation) {
			profiling::RecordFree(chunkVoxelPayloadAllocation, "SceneChunkVoxelPayloadBufferAllocation");
			vmaDestroyBuffer(context->allocator, chunkVoxelPayloadBuffer, chunkVoxelPayloadAllocation);
		}
		if (opaqueIndirectBuffer && opaqueIndirectAllocation) {
			profiling::RecordFree(opaqueIndirectAllocation, "SceneOpaqueIndirectBufferAllocation");
			vmaDestroyBuffer(context->allocator, opaqueIndirectBuffer, opaqueIndirectAllocation);
		}
		if (transparentIndirectBuffer && transparentIndirectAllocation) {
			profiling::RecordFree(transparentIndirectAllocation, "SceneTransparentIndirectBufferAllocation");
			vmaDestroyBuffer(context->allocator, transparentIndirectBuffer, transparentIndirectAllocation);
		}
		if (dirtyChunkIndexBuffer && dirtyChunkIndexAllocation) {
			profiling::RecordFree(dirtyChunkIndexAllocation, "SceneDirtyChunkIndexBufferAllocation");
			vmaDestroyBuffer(context->allocator, dirtyChunkIndexBuffer, dirtyChunkIndexAllocation);
		}
		if (chunkCullingBuffer && chunkCullingAllocation) {
			profiling::RecordFree(chunkCullingAllocation, "SceneChunkCullingBufferAllocation");
			vmaDestroyBuffer(context->allocator, chunkCullingBuffer, chunkCullingAllocation);
		}
		if (sceneLightingBuffer && sceneLightingAllocation) {
			profiling::RecordFree(sceneLightingAllocation, "VoxelSceneLightingBufferAllocation");
			vmaDestroyBuffer(context->allocator, sceneLightingBuffer, sceneLightingAllocation);
		}
		if (chunkAabbBuffer && chunkAabbAllocation) {
			profiling::RecordFree(chunkAabbAllocation, "SceneChunkAabbBufferAllocation");
			vmaDestroyBuffer(context->allocator, chunkAabbBuffer, chunkAabbAllocation);
		}
		if (visibilityMaskBuffer && visibilityMaskAllocation) {
			profiling::RecordFree(visibilityMaskAllocation, "SceneVisibilityMaskBufferAllocation");
			vmaDestroyBuffer(context->allocator, visibilityMaskBuffer, visibilityMaskAllocation);
		}
		if (visibleChunkIdBuffer && visibleChunkIdAllocation) {
			profiling::RecordFree(visibleChunkIdAllocation, "SceneVisibleChunkIdBufferAllocation");
			vmaDestroyBuffer(context->allocator, visibleChunkIdBuffer, visibleChunkIdAllocation);
		}
		if (visibilityCounterBuffer && visibilityCounterAllocation) {
			profiling::RecordFree(visibilityCounterAllocation, "SceneVisibilityCounterBufferAllocation");
			vmaDestroyBuffer(context->allocator, visibilityCounterBuffer, visibilityCounterAllocation);
		}
		if (hzbVisibleCountBuffer && hzbVisibleCountAllocation) {
			profiling::RecordFree(hzbVisibleCountAllocation, "SceneHzbVisibleCountBufferAllocation");
			vmaDestroyBuffer(context->allocator, hzbVisibleCountBuffer, hzbVisibleCountAllocation);
		}
		if (hzbPerChunkMipBuffer && hzbPerChunkMipAllocation) {
			profiling::RecordFree(hzbPerChunkMipAllocation, "SceneHzbPerChunkMipBufferAllocation");
			vmaDestroyBuffer(context->allocator, hzbPerChunkMipBuffer, hzbPerChunkMipAllocation);
		}
		if (lodDownsampledVoxelPayloadBuffer && lodDownsampledVoxelPayloadAllocation) {
			profiling::RecordFree(lodDownsampledVoxelPayloadAllocation, "SceneLodDownsampledVoxelPayloadBufferAllocation");
			vmaDestroyBuffer(context->allocator, lodDownsampledVoxelPayloadBuffer, lodDownsampledVoxelPayloadAllocation);
		}
		if (chunkLodLevelsBuffer && chunkLodLevelsAllocation) {
			profiling::RecordFree(chunkLodLevelsAllocation, "SceneChunkLodLevelsBufferAllocation");
			vmaDestroyBuffer(context->allocator, chunkLodLevelsBuffer, chunkLodLevelsAllocation);
		}
		if (fluidCaSourceBuffer && fluidCaSourceAllocation) {
			profiling::RecordFree(fluidCaSourceAllocation, "SceneFluidCaSourceBufferAllocation");
			vmaDestroyBuffer(context->allocator, fluidCaSourceBuffer, fluidCaSourceAllocation);
		}
		if (fluidCaDestinationBuffer && fluidCaDestinationAllocation) {
			profiling::RecordFree(fluidCaDestinationAllocation, "SceneFluidCaDestinationBufferAllocation");
			vmaDestroyBuffer(context->allocator, fluidCaDestinationBuffer, fluidCaDestinationAllocation);
		}
		if (fluidCaActiveChunkIdBuffer && fluidCaActiveChunkIdAllocation) {
			profiling::RecordFree(fluidCaActiveChunkIdAllocation, "SceneFluidCaActiveChunkIdBufferAllocation");
			vmaDestroyBuffer(context->allocator, fluidCaActiveChunkIdBuffer, fluidCaActiveChunkIdAllocation);
		}
		if (fluidCaStatsBuffer && fluidCaStatsAllocation) {
			profiling::RecordFree(fluidCaStatsAllocation, "SceneFluidCaStatsBufferAllocation");
			vmaDestroyBuffer(context->allocator, fluidCaStatsBuffer, fluidCaStatsAllocation);
		}
		if (nanovdbUpperBuffer && nanovdbUpperAllocation) {
			profiling::RecordFree(nanovdbUpperAllocation, "SceneNanoVdbUpperBufferAllocation");
			vmaDestroyBuffer(context->allocator, nanovdbUpperBuffer, nanovdbUpperAllocation);
		}
		if (nanovdbLowerBuffer && nanovdbLowerAllocation) {
			profiling::RecordFree(nanovdbLowerAllocation, "SceneNanoVdbLowerBufferAllocation");
			vmaDestroyBuffer(context->allocator, nanovdbLowerBuffer, nanovdbLowerAllocation);
		}
		if (nanovdbLeafBuffer && nanovdbLeafAllocation) {
			profiling::RecordFree(nanovdbLeafAllocation, "SceneNanoVdbLeafBufferAllocation");
			vmaDestroyBuffer(context->allocator, nanovdbLeafBuffer, nanovdbLeafAllocation);
		}
		if (nanovdbMaterialBuffer && nanovdbMaterialAllocation) {
			profiling::RecordFree(nanovdbMaterialAllocation, "SceneNanoVdbMaterialBufferAllocation");
			vmaDestroyBuffer(context->allocator, nanovdbMaterialBuffer, nanovdbMaterialAllocation);
		}
		if (worldGenVoxelBuffer && worldGenVoxelAllocation) {
			profiling::RecordFree(worldGenVoxelAllocation, "SceneWorldGenVoxelBufferAllocation");
			vmaDestroyBuffer(context->allocator, worldGenVoxelBuffer, worldGenVoxelAllocation);
		}

		packedFaceMappedData = nullptr;
		packedFaceBuffer = VK_NULL_HANDLE;
		packedFaceAllocation = VK_NULL_HANDLE;
		debugHudVertexMappedData = nullptr;
		debugHudVertexBuffer = VK_NULL_HANDLE;
		debugHudVertexAllocation = VK_NULL_HANDLE;
		chunkDescriptorMappedData = nullptr;
		chunkDescriptorBuffer = VK_NULL_HANDLE;
		chunkDescriptorAllocation = VK_NULL_HANDLE;
		chunkVoxelPayloadMappedData = nullptr;
		chunkVoxelPayloadBuffer = VK_NULL_HANDLE;
		chunkVoxelPayloadAllocation = VK_NULL_HANDLE;
		opaqueIndirectMappedData = nullptr;
		opaqueIndirectBuffer = VK_NULL_HANDLE;
		opaqueIndirectAllocation = nullptr;
		transparentIndirectMappedData = nullptr;
		transparentIndirectBuffer = VK_NULL_HANDLE;
		transparentIndirectAllocation = VK_NULL_HANDLE;
		dirtyChunkIndexMappedData = nullptr;
		dirtyChunkIndexBuffer = VK_NULL_HANDLE;
		dirtyChunkIndexAllocation = VK_NULL_HANDLE;
		chunkCullingMappedData = nullptr;
		chunkCullingBuffer = VK_NULL_HANDLE;
		chunkCullingAllocation = VK_NULL_HANDLE;
		sceneLightingMappedData = nullptr;
		sceneLightingBuffer = VK_NULL_HANDLE;
		sceneLightingAllocation = nullptr;
		chunkAabbMappedData = nullptr;
		chunkAabbBuffer = VK_NULL_HANDLE;
		chunkAabbAllocation = nullptr;
		visibilityMaskMappedData = nullptr;
		visibilityMaskBuffer = VK_NULL_HANDLE;
		visibilityMaskAllocation = nullptr;
		visibleChunkIdMappedData = nullptr;
		visibleChunkIdBuffer = VK_NULL_HANDLE;
		visibleChunkIdAllocation = nullptr;
		visibilityCounterMappedData = nullptr;
		visibilityCounterBuffer = VK_NULL_HANDLE;
		visibilityCounterAllocation = nullptr;
		hzbVisibleCountMappedData = nullptr;
		hzbVisibleCountBuffer = VK_NULL_HANDLE;
		hzbVisibleCountAllocation = nullptr;
		hzbPerChunkMipMappedData = nullptr;
		hzbPerChunkMipBuffer = VK_NULL_HANDLE;
		hzbPerChunkMipAllocation = nullptr;
		hzbPerChunkMipCapacityBytes = 0u;
		lodDownsampledVoxelPayloadMappedData = nullptr;
		lodDownsampledVoxelPayloadBuffer = VK_NULL_HANDLE;
		lodDownsampledVoxelPayloadAllocation = nullptr;
		lodDownsampledVoxelPayloadCapacityBytes = 0u;
		chunkLodLevelsMappedData = nullptr;
		chunkLodLevelsBuffer = VK_NULL_HANDLE;
		chunkLodLevelsAllocation = nullptr;
		chunkLodLevelsCapacity = 0u;
		fluidCaSourceMappedData = nullptr;
		fluidCaSourceBuffer = VK_NULL_HANDLE;
		fluidCaSourceAllocation = nullptr;
		fluidCaDestinationMappedData = nullptr;
		fluidCaDestinationBuffer = VK_NULL_HANDLE;
		fluidCaDestinationAllocation = nullptr;
		fluidCaActiveChunkIdMappedData = nullptr;
		fluidCaActiveChunkIdBuffer = VK_NULL_HANDLE;
		fluidCaActiveChunkIdAllocation = nullptr;
		fluidCaStatsMappedData = nullptr;
		fluidCaStatsBuffer = VK_NULL_HANDLE;
		fluidCaStatsAllocation = nullptr;
		nanovdbUpperMappedData = nullptr;
		nanovdbUpperBuffer = VK_NULL_HANDLE;
		nanovdbUpperAllocation = nullptr;
		nanovdbLowerMappedData = nullptr;
		nanovdbLowerBuffer = VK_NULL_HANDLE;
		nanovdbLowerAllocation = nullptr;
		nanovdbLeafMappedData = nullptr;
		nanovdbLeafBuffer = VK_NULL_HANDLE;
		nanovdbLeafAllocation = nullptr;
		nanovdbMaterialMappedData = nullptr;
		nanovdbMaterialBuffer = VK_NULL_HANDLE;
		nanovdbMaterialAllocation = nullptr;
		worldGenVoxelMappedData = nullptr;
		worldGenVoxelBuffer = VK_NULL_HANDLE;
		worldGenVoxelAllocation = nullptr;
		worldGenDescriptorSet = VK_NULL_HANDLE;
		graphicsDescriptorSet = VK_NULL_HANDLE;
		meshShaderDescriptorSet = VK_NULL_HANDLE;
		voxelMeshingDescriptorSet = VK_NULL_HANDLE;
		hizCullingDescriptorSet = VK_NULL_HANDLE;
		fluidCaDescriptorSet = VK_NULL_HANDLE;
		uploadedSceneVersion = 0;
		uploadedVoxelPayloadVersion = 0;
		meshedSceneVersion = 0;
		uploadedNanoVdbVersion = 0;
		chunkDescriptorCount = 0;
		dirtyChunkCount = 0;
		opaqueFaceCount = 0;
		transparentFaceCount = 0;
		debugHudVertexCount = 0;
	}

	if (render->materialVisualBuffer && render->materialVisualAllocation) {
		profiling::RecordFree(render->materialVisualAllocation, "VoxelMaterialVisualBufferAllocation");
		vmaDestroyBuffer(context->allocator, render->materialVisualBuffer, render->materialVisualAllocation);
	}
	render->materialVisualMappedData = nullptr;
	render->materialVisualBuffer = VK_NULL_HANDLE;
	render->materialVisualAllocation = VK_NULL_HANDLE;
	render->currentSceneLighting = {};

	render->sceneFaceCapacity = 0;
	render->sceneTransparentFaceBase = 0;
	render->sceneOpaqueFaceCount = 0;
	render->sceneTransparentFaceCount = 0;
	render->sceneChunkVoxelPayloadWordCount = 0;
	render->sceneUploadVersion = 0;
	render->sceneVoxelPayloadVersion = 0;
	render->sceneTriangleCount = 0;
	render->sceneMemoryBytes = 0;
	render->sceneChunkDescriptors.clear();
	render->sceneChunkVoxelPayloadRanges.clear();
	render->sceneChunkVoxelPayloadWords.clear();
	render->latestVoxelPayloadChunkIndices.clear();
	render->pendingChunkRebuildIndices.clear();
	render->completedChunkRebuildIndices.clear();
}

void EnqueueDeferredNanoVdbDestroy(
	RenderState &render,
	const uint32_t frameIndex,
	const VkBuffer buffer,
	const VmaAllocation allocation)
{
	if (frameIndex >= MAX_FRAMES_IN_FLIGHT) {
		return;
	}
	render.deferredNanoVdbDestroys[frameIndex].push_back({buffer, allocation});
}

void DrainDeferredNanoVdbDestroysForFrame(
	VulkanContextState *context,
	RenderState &render,
	const uint32_t frameIndex)
{
	if (frameIndex >= MAX_FRAMES_IN_FLIGHT || context == nullptr) {
		return;
	}
	auto &queue = render.deferredNanoVdbDestroys[frameIndex];
	if (queue.empty()) {
		return;
	}
	for (auto &[buffer, allocation] : queue) {
		if (buffer != VK_NULL_HANDLE && allocation != nullptr) {
			vmaDestroyBuffer(context->allocator, buffer, allocation);
		}
	}
	queue.clear();
}

void DrainAllDeferredNanoVdbDestroys(
	VulkanContextState *context,
	RenderState &render)
{
	if (context == nullptr) {
		return;
	}
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		DrainDeferredNanoVdbDestroysForFrame(context, render, i);
	}
}

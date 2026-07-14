import projectv.math; // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "render/SceneResources.hpp"
#include "render/SceneResourcesInternal.hpp"

#include "render/LodDownsampleGpuConsume.hpp"

#include "debug/Profiling.hpp"
#include "render/vulkan/VulkanBootstrap.hpp"
#include "render/vulkan/VulkanDebug.hpp"
#include "render/vulkan/VulkanMeshShaderPipeline.hpp"
#include "voxel/NanoVdb.hpp"
#include "voxel/VoxelMaterials.hpp"
#include "voxel/VoxelWorld.hpp"

#include <algorithm>
#include <cstring>

bool CreateSceneFrameGeometryBuffers(
	VulkanContextState *context,
	WorldState *world,
	RenderState *render,
	SceneFrameResources &frameResources,
	size_t frameResourceIndex,
	VkSharingMode asyncComputeSharingMode,
	const uint32_t *asyncComputeQueueFamilyIndices,
	uint32_t asyncComputeQueueFamilyIndexCount)
{
	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.flags =
		VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
		VMA_ALLOCATION_CREATE_MAPPED_BIT;
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

	{
		VmaAllocationInfo sceneLightingAllocationInfo{};
		if (!CreateBuffer(
				context,
				sizeof(VoxelSceneLighting),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.sceneLightingBuffer,
				&frameResources.sceneLightingAllocation,
				&sceneLightingAllocationInfo)) {
			return false;
		}
		frameResources.sceneLightingMappedData = sceneLightingAllocationInfo.pMappedData;
		profiling::RecordAllocation(
			frameResources.sceneLightingAllocation,
			sceneLightingAllocationInfo.size,
			"VoxelSceneLightingBufferAllocation");
		render->sceneMemoryBytes += sceneLightingAllocationInfo.size;
	}
	std::memcpy(
		frameResources.sceneLightingMappedData,
		&render->currentSceneLighting,
		sizeof(render->currentSceneLighting));

	VmaAllocationInfo packedFaceAllocationInfo{};
	if (!CreateBuffer(
			context,
			sizeof(PackedSceneVoxelFace) *
				static_cast<VkDeviceSize>(render->sceneFaceCapacity * 2u),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			allocationInfo,
			&frameResources.packedFaceBuffer,
			&frameResources.packedFaceAllocation,
			&packedFaceAllocationInfo)) {
		return false;
	}
	frameResources.packedFaceMappedData = packedFaceAllocationInfo.pMappedData;
	profiling::RecordAllocation(
		frameResources.packedFaceAllocation,
		packedFaceAllocationInfo.size,
		"ScenePackedFaceBufferAllocation");
	render->sceneMemoryBytes += packedFaceAllocationInfo.size;

	VmaAllocationInfo debugHudVertexAllocationInfo{};
	if (!CreateBuffer(
			context,
			sizeof(DebugHudVertex) * static_cast<VkDeviceSize>(DEBUG_HUD_MAX_VERTEX_COUNT),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			allocationInfo,
			&frameResources.debugHudVertexBuffer,
			&frameResources.debugHudVertexAllocation,
			&debugHudVertexAllocationInfo)) {
		return false;
	}
	frameResources.debugHudVertexMappedData = debugHudVertexAllocationInfo.pMappedData;
	profiling::RecordAllocation(
		frameResources.debugHudVertexAllocation,
		debugHudVertexAllocationInfo.size,
		"SceneDebugHudVertexBufferAllocation");
	render->sceneMemoryBytes += debugHudVertexAllocationInfo.size;

	VmaAllocationInfo chunkDescriptorAllocationInfo{};
	if (!CreateBuffer(
			context,
			sizeof(PackedSceneChunkDescriptor) * world->voxelWorld->chunks.size(),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			allocationInfo,
			&frameResources.chunkDescriptorBuffer,
			&frameResources.chunkDescriptorAllocation,
			&chunkDescriptorAllocationInfo,
			asyncComputeSharingMode,
			asyncComputeQueueFamilyIndices,
			asyncComputeQueueFamilyIndexCount)) {
		return false;
	}
	frameResources.chunkDescriptorMappedData = chunkDescriptorAllocationInfo.pMappedData;
	profiling::RecordAllocation(
		frameResources.chunkDescriptorAllocation,
		chunkDescriptorAllocationInfo.size,
		"SceneChunkDescriptorBufferAllocation");
	render->sceneMemoryBytes += chunkDescriptorAllocationInfo.size;

	VmaAllocationInfo chunkVoxelPayloadAllocationInfo{};
	if (!CreateBuffer(
			context,
			sizeof(uint32_t) * static_cast<VkDeviceSize>(render->sceneChunkVoxelPayloadWordCount),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			allocationInfo,
			&frameResources.chunkVoxelPayloadBuffer,
			&frameResources.chunkVoxelPayloadAllocation,
			&chunkVoxelPayloadAllocationInfo)) {
		return false;
	}
	frameResources.chunkVoxelPayloadMappedData = chunkVoxelPayloadAllocationInfo.pMappedData;
	profiling::RecordAllocation(
		frameResources.chunkVoxelPayloadAllocation,
		chunkVoxelPayloadAllocationInfo.size,
		"SceneChunkVoxelPayloadBufferAllocation");
	render->sceneMemoryBytes += chunkVoxelPayloadAllocationInfo.size;

	const uint32_t chunkCount = static_cast<uint32_t>(world->voxelWorld->chunks.size());
	const uint32_t faceClusterCapacity = std::max(
		chunkCount,
		(render->sceneFaceCapacity + projectv::render::kFacesPerCluster - 1u) / projectv::render::kFacesPerCluster);
	if (frameResourceIndex == 0) {
		render->visibleChunkIdCapacity = faceClusterCapacity;
		render->faceClusterCapacity = faceClusterCapacity;
	}
	VmaAllocationInfo visibleChunkIdAllocationInfo{};
	if (!CreateBuffer(
			context,
			sizeof(uint32_t) * static_cast<VkDeviceSize>(std::max(faceClusterCapacity, 1u)),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			allocationInfo,
			&frameResources.visibleChunkIdBuffer,
			&frameResources.visibleChunkIdAllocation,
			&visibleChunkIdAllocationInfo)) {
		return false;
	}
	frameResources.visibleChunkIdMappedData = visibleChunkIdAllocationInfo.pMappedData;
	profiling::RecordAllocation(
		frameResources.visibleChunkIdAllocation,
		visibleChunkIdAllocationInfo.size,
		"SceneVisibleChunkIdBufferAllocation");
	render->sceneMemoryBytes += visibleChunkIdAllocationInfo.size;

	constexpr VkDeviceSize kFaceClusterBytes = 32u; // FaceCluster: 4x uint + vec4
	VmaAllocationInfo faceClusterAllocationInfo{};
	if (!CreateBuffer(
			context,
			kFaceClusterBytes * static_cast<VkDeviceSize>(std::max(faceClusterCapacity, 1u)),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			allocationInfo,
			&frameResources.meshClusters.faceClusterBuffer,
			&frameResources.meshClusters.faceClusterAllocation,
			&faceClusterAllocationInfo)) {
		return false;
	}
	frameResources.meshClusters.faceClusterMappedData = faceClusterAllocationInfo.pMappedData;
	profiling::RecordAllocation(
		frameResources.meshClusters.faceClusterAllocation,
		faceClusterAllocationInfo.size,
		"SceneFaceClusterBufferAllocation");
	render->sceneMemoryBytes += faceClusterAllocationInfo.size;

	VmaAllocationInfo faceClusterCountAllocationInfo{};
	if (!CreateBuffer(
			context,
			sizeof(uint32_t),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			allocationInfo,
			&frameResources.meshClusters.faceClusterCountBuffer,
			&frameResources.meshClusters.faceClusterCountAllocation,
			&faceClusterCountAllocationInfo)) {
		return false;
	}
	frameResources.meshClusters.faceClusterCountMappedData = faceClusterCountAllocationInfo.pMappedData;
	profiling::RecordAllocation(
		frameResources.meshClusters.faceClusterCountAllocation,
		faceClusterCountAllocationInfo.size,
		"SceneFaceClusterCountBufferAllocation");
	render->sceneMemoryBytes += faceClusterCountAllocationInfo.size;
	std::memset(frameResources.meshClusters.faceClusterCountMappedData, 0, sizeof(uint32_t));

	VmaAllocationInfo meshDrawIndirectAllocationInfo{};
	if (!CreateBuffer(
			context,
			sizeof(VkDrawMeshTasksIndirectCommandEXT),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
			allocationInfo,
			&frameResources.meshClusters.meshDrawIndirectBuffer,
			&frameResources.meshClusters.meshDrawIndirectAllocation,
			&meshDrawIndirectAllocationInfo)) {
		return false;
	}
	frameResources.meshClusters.meshDrawIndirectMappedData = meshDrawIndirectAllocationInfo.pMappedData;
	profiling::RecordAllocation(
		frameResources.meshClusters.meshDrawIndirectAllocation,
		meshDrawIndirectAllocationInfo.size,
		"SceneMeshDrawIndirectBufferAllocation");
	render->sceneMemoryBytes += meshDrawIndirectAllocationInfo.size;
	std::memset(frameResources.meshClusters.meshDrawIndirectMappedData, 0, sizeof(VkDrawMeshTasksIndirectCommandEXT));

	VmaAllocationInfo visibilityCounterAllocationInfo{};
	if (!CreateBuffer(
			context,
			sizeof(uint32_t),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			allocationInfo,
			&frameResources.visibilityCounterBuffer,
			&frameResources.visibilityCounterAllocation,
			&visibilityCounterAllocationInfo)) {
		return false;
	}
	frameResources.visibilityCounterMappedData = visibilityCounterAllocationInfo.pMappedData;
	profiling::RecordAllocation(
		frameResources.visibilityCounterAllocation,
		visibilityCounterAllocationInfo.size,
		"SceneVisibilityCounterBufferAllocation");
	render->sceneMemoryBytes += visibilityCounterAllocationInfo.size;
	std::memset(frameResources.visibilityCounterMappedData, 0, sizeof(uint32_t));

	VmaAllocationInfo opaqueIndirectAllocationInfo{};
	if (!CreateBuffer(
			context,
			sizeof(VkDrawIndirectCommand) * world->voxelWorld->chunks.size(),
			VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			allocationInfo,
			&frameResources.opaqueIndirectBuffer,
			&frameResources.opaqueIndirectAllocation,
			&opaqueIndirectAllocationInfo)) {
		return false;
	}
	frameResources.opaqueIndirectMappedData = opaqueIndirectAllocationInfo.pMappedData;
	profiling::RecordAllocation(
		frameResources.opaqueIndirectAllocation,
		opaqueIndirectAllocationInfo.size,
		"SceneOpaqueIndirectBufferAllocation");
	render->sceneMemoryBytes += opaqueIndirectAllocationInfo.size;
	std::memset(
		frameResources.opaqueIndirectMappedData,
		0,
		sizeof(VkDrawIndirectCommand) * world->voxelWorld->chunks.size());

	VmaAllocationInfo transparentIndirectAllocationInfo{};
	if (!CreateBuffer(
			context,
			sizeof(VkDrawIndirectCommand) * world->voxelWorld->chunks.size(),
			VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			allocationInfo,
			&frameResources.transparentIndirectBuffer,
			&frameResources.transparentIndirectAllocation,
			&transparentIndirectAllocationInfo)) {
		return false;
	}
	frameResources.transparentIndirectMappedData = transparentIndirectAllocationInfo.pMappedData;
	profiling::RecordAllocation(
		frameResources.transparentIndirectAllocation,
		transparentIndirectAllocationInfo.size,
		"SceneTransparentIndirectBufferAllocation");
	render->sceneMemoryBytes += transparentIndirectAllocationInfo.size;
	std::memset(
		frameResources.transparentIndirectMappedData,
		0,
		sizeof(VkDrawIndirectCommand) * world->voxelWorld->chunks.size());

	VmaAllocationInfo dirtyChunkIndexAllocationInfo{};
	if (!CreateBuffer(
			context,
			sizeof(uint32_t) * world->voxelWorld->chunks.size(),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			allocationInfo,
			&frameResources.dirtyChunkIndexBuffer,
			&frameResources.dirtyChunkIndexAllocation,
			&dirtyChunkIndexAllocationInfo)) {
		return false;
	}
	frameResources.dirtyChunkIndexMappedData = dirtyChunkIndexAllocationInfo.pMappedData;
	profiling::RecordAllocation(
		frameResources.dirtyChunkIndexAllocation,
		dirtyChunkIndexAllocationInfo.size,
		"SceneDirtyChunkIndexBufferAllocation");
	render->sceneMemoryBytes += dirtyChunkIndexAllocationInfo.size;
	std::memset(
		frameResources.dirtyChunkIndexMappedData,
		0,
		sizeof(uint32_t) * world->voxelWorld->chunks.size());

	VmaAllocationInfo chunkCullingAllocationInfo{};
	if (!CreateBuffer(
			context,
			sizeof(ChunkCullingParameters),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			allocationInfo,
			&frameResources.chunkCullingBuffer,
			&frameResources.chunkCullingAllocation,
			&chunkCullingAllocationInfo)) {
		return false;
	}
	frameResources.chunkCullingMappedData = chunkCullingAllocationInfo.pMappedData;
	profiling::RecordAllocation(
		frameResources.chunkCullingAllocation,
		chunkCullingAllocationInfo.size,
		"SceneChunkCullingBufferAllocation");
	render->sceneMemoryBytes += chunkCullingAllocationInfo.size;
	std::memset(frameResources.chunkCullingMappedData, 0, sizeof(ChunkCullingParameters));

	constexpr VkDeviceSize chunkAabbStrideBytes = sizeof(PackedSceneChunkAabb);
	const VkDeviceSize chunkAabbBufferBytes = chunkAabbStrideBytes *
											  world->voxelWorld->chunks.size();
	if (chunkAabbBufferBytes > 0) {
		VmaAllocationInfo chunkAabbAllocationInfo{};
		if (!CreateBuffer(
				context,
				chunkAabbBufferBytes,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.chunkAabbBuffer,
				&frameResources.chunkAabbAllocation,
				&chunkAabbAllocationInfo)) {
			return false;
		}
		frameResources.chunkAabbMappedData = chunkAabbAllocationInfo.pMappedData;
		profiling::RecordAllocation(
			frameResources.chunkAabbAllocation,
			chunkAabbAllocationInfo.size,
			"SceneChunkAabbBufferAllocation");
		render->sceneMemoryBytes += chunkAabbAllocationInfo.size;
		std::memset(frameResources.chunkAabbMappedData, 0, chunkAabbBufferBytes);
	}

	const uint32_t visibilityMaskWordCount = (static_cast<uint32_t>(
												  world->voxelWorld->chunks.size()) +
											  31u) /
											 32u;
	if (visibilityMaskWordCount > 0u) {
		VmaAllocationInfo visibilityMaskAllocationInfo{};
		if (!CreateBuffer(
				context,
				sizeof(uint32_t) * visibilityMaskWordCount,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.visibilityMaskBuffer,
				&frameResources.visibilityMaskAllocation,
				&visibilityMaskAllocationInfo)) {
			return false;
		}
		frameResources.visibilityMaskMappedData = visibilityMaskAllocationInfo.pMappedData;
		profiling::RecordAllocation(
			frameResources.visibilityMaskAllocation,
			visibilityMaskAllocationInfo.size,
			"SceneVisibilityMaskBufferAllocation");
		render->sceneMemoryBytes += visibilityMaskAllocationInfo.size;
		std::memset(
			frameResources.visibilityMaskMappedData,
			0,
			sizeof(uint32_t) * visibilityMaskWordCount);
	}

	VmaAllocationInfo hzbVisibleCountAllocationInfo{};
	if (!CreateBuffer(
			context,
			sizeof(uint32_t),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
			allocationInfo,
			&frameResources.hzbVisibleCountBuffer,
			&frameResources.hzbVisibleCountAllocation,
			&hzbVisibleCountAllocationInfo)) {
		return false;
	}
	frameResources.hzbVisibleCountMappedData = hzbVisibleCountAllocationInfo.pMappedData;
	profiling::RecordAllocation(
		frameResources.hzbVisibleCountAllocation,
		hzbVisibleCountAllocationInfo.size,
		"SceneHzbVisibleCountBufferAllocation");
	render->sceneMemoryBytes += hzbVisibleCountAllocationInfo.size;
	const uint32_t initialHzbVisibleCount =
		static_cast<uint32_t>(world->voxelWorld->chunks.size());
	std::memcpy(
		frameResources.hzbVisibleCountMappedData,
		&initialHzbVisibleCount,
		sizeof(uint32_t));

	const uint32_t hzbPerChunkMipCount =
		std::max(static_cast<uint32_t>(world->voxelWorld->chunks.size()), 1u);
	const VkDeviceSize hzbPerChunkMipBytes = sizeof(uint32_t) * 2u * hzbPerChunkMipCount;
	VmaAllocationInfo hzbPerChunkMipAllocationInfo{};
	if (!CreateBuffer(
			context,
			hzbPerChunkMipBytes,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			allocationInfo,
			&frameResources.hzbPerChunkMipBuffer,
			&frameResources.hzbPerChunkMipAllocation,
			&hzbPerChunkMipAllocationInfo)) {
		return false;
	}
	frameResources.hzbPerChunkMipMappedData = hzbPerChunkMipAllocationInfo.pMappedData;
	frameResources.hzbPerChunkMipCapacityBytes = hzbPerChunkMipAllocationInfo.size;
	profiling::RecordAllocation(
		frameResources.hzbPerChunkMipAllocation,
		hzbPerChunkMipAllocationInfo.size,
		"SceneHzbPerChunkMipBufferAllocation");
	render->sceneMemoryBytes += hzbPerChunkMipAllocationInfo.size;

	const uint32_t chunkLodLevelsCount =
		std::max(static_cast<uint32_t>(world->voxelWorld->chunks.size()), 1u);
	const VkDeviceSize chunkLodLevelsBytes = sizeof(uint32_t) * chunkLodLevelsCount;
	VmaAllocationInfo chunkLodLevelsAllocationInfo{};
	if (!CreateBuffer(
			context,
			chunkLodLevelsBytes,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			allocationInfo,
			&frameResources.chunkLodLevelsBuffer,
			&frameResources.chunkLodLevelsAllocation,
			&chunkLodLevelsAllocationInfo)) {
		return false;
	}
	frameResources.chunkLodLevelsMappedData = chunkLodLevelsAllocationInfo.pMappedData;
	frameResources.chunkLodLevelsCapacity = static_cast<uint32_t>(chunkLodLevelsCount);
	profiling::RecordAllocation(
		frameResources.chunkLodLevelsAllocation,
		chunkLodLevelsAllocationInfo.size,
		"SceneChunkLodLevelsBufferAllocation");
	render->sceneMemoryBytes += chunkLodLevelsAllocationInfo.size;
	std::memset(frameResources.chunkLodLevelsMappedData, 0, chunkLodLevelsBytes);

	const uint32_t chunkSizeForLodPayload = static_cast<uint32_t>(world->voxelWorld->chunkSize);
	const uint32_t lodPayloadBytesNeeded =
		projectv::render::ComputeLodDownsampledVoxelPayloadBytes(
			static_cast<uint32_t>(world->voxelWorld->chunks.size()),
			chunkSizeForLodPayload);
	const VkDeviceSize lodPayloadBytes = std::max<VkDeviceSize>(lodPayloadBytesNeeded, 1u);
	VmaAllocationInfo lodPayloadAllocationInfo{};
	if (!CreateBuffer(
			context,
			lodPayloadBytes,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			allocationInfo,
			&frameResources.lodDownsampledVoxelPayloadBuffer,
			&frameResources.lodDownsampledVoxelPayloadAllocation,
			&lodPayloadAllocationInfo)) {
		return false;
	}
	frameResources.lodDownsampledVoxelPayloadMappedData = lodPayloadAllocationInfo.pMappedData;
	frameResources.lodDownsampledVoxelPayloadCapacityBytes = lodPayloadAllocationInfo.size;
	profiling::RecordAllocation(
		frameResources.lodDownsampledVoxelPayloadAllocation,
		lodPayloadAllocationInfo.size,
		"SceneLodDownsampledVoxelPayloadBufferAllocation");
	render->sceneMemoryBytes += lodPayloadAllocationInfo.size;
	std::memset(frameResources.lodDownsampledVoxelPayloadMappedData, 0, lodPayloadBytes);

	char bufferName[64]{};

	std::snprintf(bufferName, sizeof(bufferName), "ScenePackedFaceBuffer[%zu]", frameResourceIndex);
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(frameResources.packedFaceBuffer),
		VK_OBJECT_TYPE_BUFFER,
		bufferName);
	std::snprintf(bufferName, sizeof(bufferName), "SceneDebugHudVertexBuffer[%zu]", frameResourceIndex);
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(frameResources.debugHudVertexBuffer),
		VK_OBJECT_TYPE_BUFFER,
		bufferName);
	std::snprintf(bufferName, sizeof(bufferName), "SceneChunkDescriptorBuffer[%zu]", frameResourceIndex);
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(frameResources.chunkDescriptorBuffer),
		VK_OBJECT_TYPE_BUFFER,
		bufferName);
	std::snprintf(bufferName, sizeof(bufferName), "SceneChunkVoxelPayloadBuffer[%zu]", frameResourceIndex);
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(frameResources.chunkVoxelPayloadBuffer),
		VK_OBJECT_TYPE_BUFFER,
		bufferName);
	std::snprintf(bufferName, sizeof(bufferName), "SceneOpaqueIndirectBuffer[%zu]", frameResourceIndex);
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(frameResources.opaqueIndirectBuffer),
		VK_OBJECT_TYPE_BUFFER,
		bufferName);
	std::snprintf(bufferName, sizeof(bufferName), "SceneTransparentIndirectBuffer[%zu]", frameResourceIndex);
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(frameResources.transparentIndirectBuffer),
		VK_OBJECT_TYPE_BUFFER,
		bufferName);
	std::snprintf(bufferName, sizeof(bufferName), "SceneDirtyChunkIndexBuffer[%zu]", frameResourceIndex);
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(frameResources.dirtyChunkIndexBuffer),
		VK_OBJECT_TYPE_BUFFER,
		bufferName);
	std::snprintf(bufferName, sizeof(bufferName), "SceneChunkCullingBuffer[%zu]", frameResourceIndex);
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(frameResources.chunkCullingBuffer),
		VK_OBJECT_TYPE_BUFFER,
		bufferName);

	frameResources.debugHudVertexCount = 0;
	return true;
}

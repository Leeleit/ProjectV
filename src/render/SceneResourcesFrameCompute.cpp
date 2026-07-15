#include "core/Math.hpp"

#include "render/SceneResources.hpp"
#include "render/SceneResourcesInternal.hpp"

#include "debug/Profiling.hpp"
#include "render/vulkan/VulkanBootstrap.hpp"
#include "render/vulkan/VulkanDebug.hpp"
#include "voxel/NanoVdb.hpp"
#include "voxel/VoxelMaterials.hpp"
#include "voxel/VoxelWorld.hpp"

#include <algorithm>
#include <cstring>

bool CreateSceneFrameComputeBuffers(
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

	const uint32_t fluidCaMaxActiveChunks = static_cast<uint32_t>(world->voxelWorld->chunks.size());
	const VkDeviceSize fluidCaActiveChunkIdBytes = sizeof(uint32_t) * std::max(fluidCaMaxActiveChunks, 1u);
	VmaAllocationInfo fluidCaActiveChunkIdAllocationInfo{};
	if (!CreateBuffer(
			context,
			fluidCaActiveChunkIdBytes,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			allocationInfo,
			&frameResources.fluidCaActiveChunkIdBuffer,
			&frameResources.fluidCaActiveChunkIdAllocation,
			&fluidCaActiveChunkIdAllocationInfo,
			asyncComputeSharingMode,
			asyncComputeQueueFamilyIndices,
			asyncComputeQueueFamilyIndexCount)) {
		return false;
	}
	frameResources.fluidCaActiveChunkIdMappedData = fluidCaActiveChunkIdAllocationInfo.pMappedData;
	profiling::RecordAllocation(
		frameResources.fluidCaActiveChunkIdAllocation,
		fluidCaActiveChunkIdAllocationInfo.size,
		"SceneFluidCaActiveChunkIdBufferAllocation");
	render->sceneMemoryBytes += fluidCaActiveChunkIdAllocationInfo.size;
	std::memset(frameResources.fluidCaActiveChunkIdMappedData, 0, fluidCaActiveChunkIdBytes);

	VmaAllocationInfo fluidCaStatsAllocationInfo{};
	if (!CreateBuffer(
			context,
			sizeof(uint32_t) * 4u,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			allocationInfo,
			&frameResources.fluidCaStatsBuffer,
			&frameResources.fluidCaStatsAllocation,
			&fluidCaStatsAllocationInfo,
			asyncComputeSharingMode,
			asyncComputeQueueFamilyIndices,
			asyncComputeQueueFamilyIndexCount)) {
		return false;
	}
	frameResources.fluidCaStatsMappedData = fluidCaStatsAllocationInfo.pMappedData;
	profiling::RecordAllocation(
		frameResources.fluidCaStatsAllocation,
		fluidCaStatsAllocationInfo.size,
		"SceneFluidCaStatsBufferAllocation");
	render->sceneMemoryBytes += fluidCaStatsAllocationInfo.size;
	std::memset(frameResources.fluidCaStatsMappedData, 0, sizeof(uint32_t) * 4u);

	if (frameResourceIndex == 0) {
		render->fluidCaMaxActiveChunks = fluidCaMaxActiveChunks;
	}

	const uint32_t chunkVoxelCount = static_cast<uint32_t>(world->voxelWorld->chunkSize) *
									 static_cast<uint32_t>(world->voxelWorld->chunkSize) *
									 static_cast<uint32_t>(world->voxelWorld->chunkSize);
	const VkDeviceSize fluidCaPingPongBytes = std::max<VkDeviceSize>(
		sizeof(uint32_t) * 4u * std::max(chunkVoxelCount, 1u) * std::max(fluidCaMaxActiveChunks, 1u),
		sizeof(uint32_t) * 4u);
	if (frameResourceIndex == 0) {
		render->fluidCaPingPongBufferBytes = static_cast<uint32_t>(fluidCaPingPongBytes);
	}

	for (uint32_t pingPong = 0u; pingPong < 2u; ++pingPong) {
		VkBuffer &targetBuffer = pingPong == 0u ? frameResources.fluidCaSourceBuffer
												: frameResources.fluidCaDestinationBuffer;
		VmaAllocation &targetAllocation = pingPong == 0u ? frameResources.fluidCaSourceAllocation
														 : frameResources.fluidCaDestinationAllocation;
		void *&targetMappedData = pingPong == 0u ? frameResources.fluidCaSourceMappedData
												 : frameResources.fluidCaDestinationMappedData;
		VmaAllocationInfo pingPongAllocationInfo{};
		if (!CreateBuffer(
				context,
				fluidCaPingPongBytes,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&targetBuffer,
				&targetAllocation,
				&pingPongAllocationInfo,
				asyncComputeSharingMode,
				asyncComputeQueueFamilyIndices,
				asyncComputeQueueFamilyIndexCount)) {
			return false;
		}
		targetMappedData = pingPongAllocationInfo.pMappedData;
		profiling::RecordAllocation(
			targetAllocation,
			pingPongAllocationInfo.size,
			pingPong == 0u ? "SceneFluidCaSourceBufferAllocation" : "SceneFluidCaDestinationBufferAllocation");
		render->sceneMemoryBytes += pingPongAllocationInfo.size;
		std::memset(targetMappedData, 0, fluidCaPingPongBytes);
	}

	constexpr VkDeviceSize kNanoVdbInitialUpperCapacityBytes = sizeof(projectv::voxel::nanovdb::NanoVdbUpper) * 1u;
	constexpr VkDeviceSize kNanoVdbInitialLowerCapacityBytes = sizeof(projectv::voxel::nanovdb::NanoVdbLower) * 64u;
	constexpr VkDeviceSize kNanoVdbInitialLeafCapacityBytes = sizeof(projectv::voxel::nanovdb::NanoVdbLeaf) * 64u;
	constexpr VkDeviceSize kNanoVdbInitialMaterialCapacityBytes = sizeof(uint8_t) * 64u;

	{
		VmaAllocationInfo nanovdbUpperAllocationInfo{};
		if (!CreateBuffer(
				context,
				kNanoVdbInitialUpperCapacityBytes,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.nanovdbUpperBuffer,
				&frameResources.nanovdbUpperAllocation,
				&nanovdbUpperAllocationInfo)) {
			return false;
		}
		frameResources.nanovdbUpperMappedData = nanovdbUpperAllocationInfo.pMappedData;
		frameResources.nanovdbUpperCapacityBytes = nanovdbUpperAllocationInfo.size;
		profiling::RecordAllocation(
			frameResources.nanovdbUpperAllocation,
			nanovdbUpperAllocationInfo.size,
			"SceneNanoVdbUpperBufferAllocation");
		render->sceneMemoryBytes += nanovdbUpperAllocationInfo.size;
	}

	{
		VmaAllocationInfo nanovdbLowerAllocationInfo{};
		if (!CreateBuffer(
				context,
				kNanoVdbInitialLowerCapacityBytes,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.nanovdbLowerBuffer,
				&frameResources.nanovdbLowerAllocation,
				&nanovdbLowerAllocationInfo)) {
			return false;
		}
		frameResources.nanovdbLowerMappedData = nanovdbLowerAllocationInfo.pMappedData;
		frameResources.nanovdbLowerCapacityBytes = nanovdbLowerAllocationInfo.size;
		profiling::RecordAllocation(
			frameResources.nanovdbLowerAllocation,
			nanovdbLowerAllocationInfo.size,
			"SceneNanoVdbLowerBufferAllocation");
		render->sceneMemoryBytes += nanovdbLowerAllocationInfo.size;
	}

	{
		VmaAllocationInfo nanovdbLeafAllocationInfo{};
		if (!CreateBuffer(
				context,
				kNanoVdbInitialLeafCapacityBytes,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.nanovdbLeafBuffer,
				&frameResources.nanovdbLeafAllocation,
				&nanovdbLeafAllocationInfo)) {
			return false;
		}
		frameResources.nanovdbLeafMappedData = nanovdbLeafAllocationInfo.pMappedData;
		frameResources.nanovdbLeafCapacityBytes = nanovdbLeafAllocationInfo.size;
		profiling::RecordAllocation(
			frameResources.nanovdbLeafAllocation,
			nanovdbLeafAllocationInfo.size,
			"SceneNanoVdbLeafBufferAllocation");
		render->sceneMemoryBytes += nanovdbLeafAllocationInfo.size;
	}

	{
		VmaAllocationInfo nanovdbMaterialAllocationInfo{};
		if (!CreateBuffer(
				context,
				kNanoVdbInitialMaterialCapacityBytes,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.nanovdbMaterialBuffer,
				&frameResources.nanovdbMaterialAllocation,
				&nanovdbMaterialAllocationInfo)) {
			return false;
		}
		frameResources.nanovdbMaterialMappedData = nanovdbMaterialAllocationInfo.pMappedData;
		frameResources.nanovdbMaterialCapacityBytes = nanovdbMaterialAllocationInfo.size;
		profiling::RecordAllocation(
			frameResources.nanovdbMaterialAllocation,
			nanovdbMaterialAllocationInfo.size,
			"SceneNanoVdbMaterialBufferAllocation");
		render->sceneMemoryBytes += nanovdbMaterialAllocationInfo.size;
	}

	const VkDeviceSize kWorldGenVoxelBufferBytes = sizeof(uint32_t) * 8u * 8u * 8u *
												   std::max(world->voxelWorld->chunks.size(), static_cast<size_t>(1u));
	{
		VmaAllocationInfo worldGenVoxelAllocationInfo{};
		if (!CreateBuffer(
				context,
				kWorldGenVoxelBufferBytes,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.worldGenVoxelBuffer,
				&frameResources.worldGenVoxelAllocation,
				&worldGenVoxelAllocationInfo,
				asyncComputeSharingMode,
				asyncComputeQueueFamilyIndices,
				asyncComputeQueueFamilyIndexCount)) {
			return false;
		}
		frameResources.worldGenVoxelMappedData = worldGenVoxelAllocationInfo.pMappedData;
		frameResources.worldGenVoxelCapacityBytes = worldGenVoxelAllocationInfo.size;
		profiling::RecordAllocation(
			frameResources.worldGenVoxelAllocation,
			worldGenVoxelAllocationInfo.size,
			"SceneWorldGenVoxelBufferAllocation");
		render->sceneMemoryBytes += worldGenVoxelAllocationInfo.size;
	}

	char bufferName[64]{};

	std::snprintf(bufferName, sizeof(bufferName), "SceneFluidCaActiveChunkIdBuffer[%zu]", frameResourceIndex);
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(frameResources.fluidCaActiveChunkIdBuffer),
		VK_OBJECT_TYPE_BUFFER,
		bufferName);
	std::snprintf(bufferName, sizeof(bufferName), "SceneFluidCaStatsBuffer[%zu]", frameResourceIndex);
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(frameResources.fluidCaStatsBuffer),
		VK_OBJECT_TYPE_BUFFER,
		bufferName);
	std::snprintf(bufferName, sizeof(bufferName), "SceneFluidCaSourceBuffer[%zu]", frameResourceIndex);
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(frameResources.fluidCaSourceBuffer),
		VK_OBJECT_TYPE_BUFFER,
		bufferName);
	std::snprintf(bufferName, sizeof(bufferName), "SceneFluidCaDestinationBuffer[%zu]", frameResourceIndex);
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(frameResources.fluidCaDestinationBuffer),
		VK_OBJECT_TYPE_BUFFER,
		bufferName);
	std::snprintf(bufferName, sizeof(bufferName), "SceneNanoVdbUpperBuffer[%zu]", frameResourceIndex);
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(frameResources.nanovdbUpperBuffer),
		VK_OBJECT_TYPE_BUFFER,
		bufferName);
	std::snprintf(bufferName, sizeof(bufferName), "SceneNanoVdbLowerBuffer[%zu]", frameResourceIndex);
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(frameResources.nanovdbLowerBuffer),
		VK_OBJECT_TYPE_BUFFER,
		bufferName);
	std::snprintf(bufferName, sizeof(bufferName), "SceneNanoVdbLeafBuffer[%zu]", frameResourceIndex);
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(frameResources.nanovdbLeafBuffer),
		VK_OBJECT_TYPE_BUFFER,
		bufferName);
	std::snprintf(bufferName, sizeof(bufferName), "SceneNanoVdbMaterialBuffer[%zu]", frameResourceIndex);
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(frameResources.nanovdbMaterialBuffer),
		VK_OBJECT_TYPE_BUFFER,
		bufferName);

	return true;
}

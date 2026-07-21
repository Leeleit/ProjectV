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

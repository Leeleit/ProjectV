#include "core/Math.hpp"

#include "render/SceneResources.hpp"
#include "render/SceneResourcesInternal.hpp"

#include "debug/Profiling.hpp"
#include "render/vulkan/VulkanBootstrap.hpp"
#include "render/vulkan/VulkanDebug.hpp"
#include "render/vulkan/VulkanGraphicsPipeline.hpp"
#include "render/vulkan/VulkanVoxelMeshingPipeline.hpp"
#include "voxel/VoxelMaterials.hpp"
#include "voxel/VoxelWorld.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <span>

namespace {
std::span<const VoxelMaterialVisual> BuildMaterialVisualTable()
{
	static const std::array visuals{
		GetVoxelMaterialVisual(VoxelMaterial::Air),
		GetVoxelMaterialVisual(VoxelMaterial::Glass),
		GetVoxelMaterialVisual(VoxelMaterial::Fluid),
		GetVoxelMaterialVisual(VoxelMaterial::FloorWhite),
		GetVoxelMaterialVisual(VoxelMaterial::FloorGray),
	};
	return visuals;
}
} // namespace

bool CreateSceneResources(
	VulkanContextState *context,
	WorldState *world,
	RenderState *render)
{
	if (!context || !world || !render || !context->allocator || !world->voxelWorld) {
		return false;
	}
	if (world->voxelWorld->chunkSize > 255) {
		SDL_Log("Chunk size %d exceeds packed scene face payload limit", world->voxelWorld->chunkSize);
		return false;
	}

	DestroySceneResources(context, render);
	render->sceneFaceCapacity = GetMaxSceneFaceCount(*world->voxelWorld);
	render->sceneTransparentFaceBase = render->sceneFaceCapacity;
	InitializeSceneChunkDescriptorsAndVoxelPayloadLayout(*world->voxelWorld, *render);
	render->sceneMemoryBytes = 0;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.flags =
		VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
		VMA_ALLOCATION_CREATE_MAPPED_BIT;
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

	std::array<uint32_t, 2> asyncComputeQueueFamilies{};
	const VkSharingMode asyncComputeSharingMode = projectv::render::ChooseSharingMode(*context, asyncComputeQueueFamilies);
	const uint32_t *asyncComputeQueueFamilyIndices = asyncComputeSharingMode == VK_SHARING_MODE_CONCURRENT ? asyncComputeQueueFamilies.data() : nullptr;
	const uint32_t asyncComputeQueueFamilyIndexCount = asyncComputeSharingMode == VK_SHARING_MODE_CONCURRENT ? static_cast<uint32_t>(asyncComputeQueueFamilies.size()) : 0u;

	{
		VmaAllocationInfo materialVisualAllocationInfo{};
		if (!CreateBuffer(
				context,
				sizeof(VoxelMaterialVisual) * kVoxelMaterialCount,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&render->materialVisualBuffer,
				&render->materialVisualAllocation,
				&materialVisualAllocationInfo)) {
			DestroySceneResources(context, render);
			return false;
		}
		render->materialVisualMappedData = materialVisualAllocationInfo.pMappedData;
		profiling::RecordAllocation(
			render->materialVisualAllocation,
			materialVisualAllocationInfo.size,
			"VoxelMaterialVisualBufferAllocation");
		render->sceneMemoryBytes += materialVisualAllocationInfo.size;
		const std::span<const VoxelMaterialVisual> materialVisuals = BuildMaterialVisualTable();
		std::memcpy(
			render->materialVisualMappedData,
			materialVisuals.data(),
			materialVisuals.size_bytes());
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(render->materialVisualBuffer),
			VK_OBJECT_TYPE_BUFFER,
			"VoxelMaterialVisualBuffer");
	}

	render->currentSceneLighting = BuildSceneLighting(*world->voxelWorld, *render);

	for (SceneFrameResources &frameResources : render->sceneFrameResources) {
		const size_t frameResourceIndex = static_cast<size_t>(&frameResources - render->sceneFrameResources.data());
		if (!CreateSceneFrameGeometryBuffers(
				context,
				world,
				render,
				frameResources,
				frameResourceIndex,
				asyncComputeSharingMode,
				asyncComputeQueueFamilyIndices,
				asyncComputeQueueFamilyIndexCount)) {
			DestroySceneResources(context, render);
			return false;
		}
		if (!CreateSceneFrameComputeBuffers(
				context,
				world,
				render,
				frameResources,
				frameResourceIndex,
				asyncComputeSharingMode,
				asyncComputeQueueFamilyIndices,
				asyncComputeQueueFamilyIndexCount)) {
			DestroySceneResources(context, render);
			return false;
		}
	}

	render->sceneOpaqueFaceCount = 0;
	render->sceneTransparentFaceCount = 0;
	render->sceneTriangleCount = 0;
	render->sceneUploadVersion = 1;
	render->sceneVoxelPayloadVersion = 0;
	render->latestVoxelPayloadChunkIndices.clear();
	render->latestVoxelPayloadChunkIndices.reserve(world->voxelWorld->chunks.size());
	for (SceneVoxelPayloadSyncState &sync : render->sceneVoxelPayloadSync) {
		sync = {};
		sync.pendingChunkIndices.reserve(world->voxelWorld->chunks.size());
	}
	render->pendingChunkRebuildIndices.clear();
	render->pendingChunkRebuildIndices.reserve(world->voxelWorld->chunks.size());
	render->completedChunkRebuildIndices.clear();
	render->completedChunkRebuildIndices.reserve(world->voxelWorld->chunks.size());

	if (!RefreshGraphicsResourceBindings(context, render)) {
		DestroySceneResources(context, render);
		return false;
	}
	if (!RefreshVoxelMeshingResourceBindings(context, render)) {
		DestroySceneResources(context, render);
		return false;
	}
	return true;
}

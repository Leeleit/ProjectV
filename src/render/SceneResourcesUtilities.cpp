#include "render/SceneResourcesInternal.hpp"

VoxelSceneLighting BuildSceneLighting(
	const VoxelWorld &world,
	const RenderState &render)
{
	return BuildVoxelSceneLighting(world.scenePreset, render.lightingDebugControls);
}
void RefreshSceneLightingBuffer(
	const VoxelWorld &world,
	RenderState &render,
	const VkExtent2D renderExtent)
{
	(void)renderExtent;
	render.currentScenePreset = world.scenePreset;
	render.currentSceneLighting = BuildSceneLighting(world, render);
}

bool CreateBuffer(
	VulkanContextState *context,
	const VkDeviceSize size,
	const VkBufferUsageFlags usage,
	const VmaAllocationCreateInfo &allocationInfo,
	VkBuffer *outBuffer,
	VmaAllocation *outAllocation,
	VmaAllocationInfo *outAllocationInfo,
	const VkSharingMode sharingMode,
	const uint32_t *queueFamilyIndices,
	const uint32_t queueFamilyIndexCount)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = sharingMode;
	if (sharingMode == VK_SHARING_MODE_CONCURRENT && queueFamilyIndices != nullptr && queueFamilyIndexCount > 0) {
		bufferInfo.queueFamilyIndexCount = queueFamilyIndexCount;
		bufferInfo.pQueueFamilyIndices = queueFamilyIndices;
	}

	return vmaCreateBuffer(
			   context->allocator,
			   &bufferInfo,
			   &allocationInfo,
			   outBuffer,
			   outAllocation,
			   outAllocationInfo) == VK_SUCCESS;
}

uint32_t GetChunkVoxelCount(const VoxelChunk &chunk)
{
	const uint32_t extentX = static_cast<uint32_t>(chunk.maxExclusive.x - chunk.min.x);
	const uint32_t extentY = static_cast<uint32_t>(chunk.maxExclusive.y - chunk.min.y);
	const uint32_t extentZ = static_cast<uint32_t>(chunk.maxExclusive.z - chunk.min.z);
	return extentX * extentY * extentZ;
}

uint32_t GetChunkVoxelWordCount(const VoxelChunk &chunk)
{
	const uint32_t voxelCount = GetChunkVoxelCount(chunk);
	return (voxelCount + kVoxelMaterialsPerWord - 1u) / kVoxelMaterialsPerWord;
}

VkDrawIndirectCommand BuildChunkIndirectCommand(
	const uint32_t firstInstance,
	const uint32_t faceCount,
	const bool visible)
{
	return {
		.vertexCount = 6u,
		.instanceCount = visible ? faceCount : 0u,
		.firstVertex = 0u,
		.firstInstance = firstInstance,
	};
}

uint32_t GetShadowIndirectCommandCount(const uint32_t chunkDescriptorCount)
{
	return chunkDescriptorCount;
}

uint32_t GetMaxSceneFaceCount(const VoxelWorld &world)
{
	const size_t totalCells = static_cast<size_t>(world.width) * world.height * world.depth;
	return static_cast<uint32_t>(totalCells) * 6u;
}


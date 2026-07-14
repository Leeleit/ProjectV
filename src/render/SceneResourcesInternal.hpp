#pragma once

#include "render/SceneResources.hpp"
#include "render/LodDownsampleGpuConsume.hpp"

#include "voxel/VoxelMaterials.hpp"
#include "voxel/VoxelWorld.hpp"

constexpr uint32_t kVoxelMaterialsPerWord = 4u;

VoxelSceneLighting BuildSceneLighting(const VoxelWorld &world, const RenderState &render);
void RefreshSceneLightingBuffer(const VoxelWorld &world, RenderState &render, VkExtent2D renderExtent);
bool CreateBuffer(VulkanContextState *context, VkDeviceSize size, VkBufferUsageFlags usage, const VmaAllocationCreateInfo &allocationInfo, VkBuffer *outBuffer, VmaAllocation *outAllocation, VmaAllocationInfo *outAllocationInfo = nullptr, VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE, const uint32_t *queueFamilyIndices = nullptr, uint32_t queueFamilyIndexCount = 0);
uint32_t GetChunkVoxelCount(const VoxelChunk &chunk);
uint32_t GetChunkVoxelWordCount(const VoxelChunk &chunk);
VkDrawIndirectCommand BuildChunkIndirectCommand(uint32_t firstInstance, uint32_t faceCount, bool visible);
uint32_t GetShadowIndirectCommandCount(uint32_t chunkDescriptorCount);
uint32_t GetMaxSceneFaceCount(const VoxelWorld &world);

void InitializeSceneChunkDescriptorsAndVoxelPayloadLayout(const VoxelWorld &world, RenderState &render);
void RepackChunkVoxelPayload(const VoxelWorld &world, size_t chunkIndex, RenderState &render);
void UpdateGeneratedFaceStatsFromFrameResources(RenderState &render, const SceneFrameResources &frameResources);
uint32_t PrepareDirtyChunkMeshingList(const RenderState &render, SceneFrameResources &frameResources);

bool CreateSceneFrameGeometryBuffers(
	VulkanContextState *context,
	WorldState *world,
	RenderState *render,
	SceneFrameResources &frameResources,
	size_t frameResourceIndex,
	VkSharingMode asyncComputeSharingMode,
	const uint32_t *asyncComputeQueueFamilyIndices,
	uint32_t asyncComputeQueueFamilyIndexCount);
bool CreateSceneFrameComputeBuffers(
	VulkanContextState *context,
	WorldState *world,
	RenderState *render,
	SceneFrameResources &frameResources,
	size_t frameResourceIndex,
	VkSharingMode asyncComputeSharingMode,
	const uint32_t *asyncComputeQueueFamilyIndices,
	uint32_t asyncComputeQueueFamilyIndexCount);

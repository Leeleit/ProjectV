#pragma once

#include "render/SceneResources.hpp"
#include "render/LodDownsampleGpuConsume.hpp"
#include "render/VoxelMeshingPushConstants.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"
#include "render/vulkan/VulkanDebug.hpp"
#include "render/vulkan/VulkanGraphicsPipeline.hpp"
#include "render/vulkan/VulkanVoxelMeshingPipeline.hpp"
#include "voxel/NanoVdb.hpp"
#include "voxel/VoxelMaterials.hpp"
#include "voxel/VoxelWorld.hpp"
#include <algorithm>
#include <array>
#include <span>

constexpr uint32_t kVoxelMaterialsPerWord = 4u;

VoxelSceneLighting BuildSceneLighting(const VoxelWorld &world, const RenderState &render);
void RefreshSceneLightingBuffer(const VoxelWorld &world, RenderState &render, const VkExtent2D renderExtent);
bool CreateBuffer(VulkanContextState *context, VkDeviceSize size, VkBufferUsageFlags usage, const VmaAllocationCreateInfo &allocationInfo, VkBuffer *outBuffer, VmaAllocation *outAllocation, VmaAllocationInfo *outAllocationInfo);
uint32_t GetChunkVoxelCount(const VoxelChunk &chunk);
uint32_t GetChunkVoxelWordCount(const VoxelChunk &chunk);
VkDrawIndirectCommand BuildChunkIndirectCommand(uint32_t firstInstance, uint32_t faceCount, bool visible);
uint32_t GetShadowIndirectCommandCount(uint32_t chunkDescriptorCount);
uint32_t GetMaxSceneFaceCount(const VoxelWorld &world);

void InitializeSceneChunkDescriptorsAndVoxelPayloadLayout(const VoxelWorld &world, RenderState &render);
void RepackChunkVoxelPayload(const VoxelWorld &world, size_t chunkIndex, RenderState &render);
void UpdateGeneratedFaceStatsFromFrameResources(RenderState &render, const SceneFrameResources &frameResources);
uint32_t PrepareDirtyChunkMeshingList(const RenderState &render, SceneFrameResources &frameResources);

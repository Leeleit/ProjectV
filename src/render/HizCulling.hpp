#pragma once

#include <array> // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

struct VulkanContextState;
struct RenderState;
struct SceneFrameResources;

#include "vk_mem_alloc.h"

namespace projectv::render {

struct HizBuffer {
	VkImage image = VK_NULL_HANDLE;
	VkImageView imageView = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	VmaAllocation allocation = nullptr;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	uint32_t baseWidth = 0u;
	uint32_t baseHeight = 0u;
	uint32_t mipLevelCount = 0u;
};

bool IsHzbCullingEnabled();

bool IsMeshShaderPipelineEnabled();

bool IsHzbSmartMipEnabled();

bool IsHzbSmartBlendWidthEnabled();

uint32_t ComputeHzbMipLevelCount(const uint32_t baseWidth, const uint32_t baseHeight);

uint32_t ComputeBlendWidthForChunkMip(
	uint32_t projectedExtentXTexels,
	uint32_t projectedExtentYTexels,
	uint32_t mipLevel,
	uint32_t maxBlendWidth);

bool CreateHizBuffer(
	VulkanContextState *context,
	uint32_t baseWidth,
	uint32_t baseHeight,
	HizBuffer &outBuffer);

void DestroyHizBuffer(VulkanContextState *context, HizBuffer &buffer);

void BuildHizMipChain(
	VkCommandBuffer commandBuffer,
	VkImage depthImage,
	VkImageLayout depthImageLayout,
	const HizBuffer &hizBuffer);

struct HizCullingPushConstants {
	std::array<float, 16> inverseViewProjection{};
	std::array<uint32_t, 4> hizExtentAndMipCount{};
	std::array<float, 4> depthUnpackParams{};
};
static_assert(sizeof(HizCullingPushConstants) == 96);

bool CreateHizCullingPipeline(
	VulkanContextState *context,
	RenderState *render);

void DestroyHizCullingPipeline(
	VulkanContextState *context,
	RenderState *render);

bool RecordHzbCullingDispatch(
	VkCommandBuffer commandBuffer,
	VulkanContextState *context,
	RenderState &render,
	SceneFrameResources &frameResources,
	const float (&inverseViewProjection)[16],
	uint32_t chunkDescriptorCount);

uint32_t ComputePerChunkMipLevelCpu(
	float projectedExtentXTexels,
	float projectedExtentYTexels,
	uint32_t maxMipLevel);

uint32_t ComputePerChunkMipLevelsFromAabbs(
	const std::vector<std::array<float, 4>> &chunkCenters,
	const std::vector<std::array<float, 4>> &chunkHalfExtents,
	const std::array<float, 16> &viewProjection,
	uint32_t baseWidth,
	uint32_t baseHeight,
	uint32_t maxMipLevel,
	std::vector<uint32_t> &outMipLevels);

uint32_t ComputePerChunkMipAndBlendWidthsFromAabbs(
	const std::vector<std::array<float, 4>> &chunkCenters,
	const std::vector<std::array<float, 4>> &chunkHalfExtents,
	const std::array<float, 16> &viewProjection,
	uint32_t baseWidth,
	uint32_t baseHeight,
	uint32_t maxMipLevel,
	uint32_t maxBlendWidth,
	std::vector<uint32_t> &outMipAndBlendWidths);

// EVIL: kHizMipAndBlendWidthWordsPerChunk = 2 (mip + blendWidth packed).
// Must match shader's `perChunkMipAndBlendWidth[i*2]` / `perChunkMipAndBlendWidth[i*2+1]`
// access pattern in hzb_cull.comp. Stride is 2 uint32 per chunk, total = chunkCount * 8 bytes.
inline constexpr uint32_t kHizMipAndBlendWidthWordsPerChunk = 2u;

void WritePerChunkMipAndBlendWidthsToBuffer(
	void *mappedData,
	const uint32_t *mipAndBlendWidths,
	uint32_t chunkCount);

}  // namespace projectv::render

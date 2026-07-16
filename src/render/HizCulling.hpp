#pragma once

#include <array> // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
#include <span>
#include <vector>

#include <vulkan/vulkan.h>

#include "core/Math.hpp"

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
	std::array<VkImageView, 16> mipStorageViews{}; // Per-mip storage views for depth reduction.
	uint32_t mipStorageViewCount = 0u;
};

bool IsHzbCullingEnabled();

bool IsMeshShaderPipelineEnabled();

bool IsHzbSmartMipEnabled();

bool IsHzbSmartBlendWidthEnabled();

bool IsHzbMinMipEnabled();

uint32_t CountHzbVisibleChunks(
	std::span<const uint32_t> visibilityWords,
	uint32_t chunkCount);

uint32_t ComputeHzbMipLevelCount(uint32_t baseWidth, uint32_t baseHeight);

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
	const HizBuffer &hizBuffer,
	RenderState *render = nullptr,
	VulkanContextState *context = nullptr);

struct HizCullingPushConstants {
	std::array<float, 16> viewProjection{};
	std::array<uint32_t, 4> hizExtentAndMipCount{};
	std::array<float, 4> depthUnpackParams{};
};
static_assert(sizeof(HizCullingPushConstants) == 96);

struct HizMinifyPushConstants {
	std::array<uint32_t, 4> copySourceAndPadding{};
};
static_assert(sizeof(HizMinifyPushConstants) == 16);

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
	const float (&viewProjection)[16],
	uint32_t chunkDescriptorCount);

enum class HzbApplyMode : uint32_t {
	PassA = 0u,	   // draw if visibility bit set
	PassB = 1u,	   // draw if newly visible (prev clear, now set)
	ForceAll = 2u, // camera cut / first frame
};

struct HzbApplyPushConstants {
	std::array<uint32_t, 4> chunkCountAndMode{};
};
static_assert(sizeof(HzbApplyPushConstants) == 16);

bool RecordHzbApplyVisibility(
	VkCommandBuffer commandBuffer,
	VulkanContextState *context,
	RenderState &render,
	SceneFrameResources &frameResources,
	uint32_t chunkDescriptorCount,
	HzbApplyMode mode);

bool DetectHzbCameraCut(RenderState &render, const projectv::math::Vec3 &cameraForward);

// After vkWaitForFences on frameIndex: fold that slot's completed cull into host unified mask.
void SyncHzbUnifiedVisibilityAfterFence(
	VulkanContextState *context,
	RenderState &render,
	uint32_t frameIndex);

// Before Pass A: write unified mask into this slot so all FIF slots Pass-A the same history.
void SeedHzbSlotVisibilityFromUnified(
	VulkanContextState *context,
	RenderState &render,
	uint32_t frameIndex);

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

inline constexpr uint32_t kHizMipAndBlendWidthWordsPerChunk = 2u; // mip + blendWidth packed; matches shader's perChunkMipAndBlendWidth stride.

void WritePerChunkMipAndBlendWidthsToBuffer(
	void *mappedData,
	const uint32_t *mipAndBlendWidths,
	uint32_t chunkCount);

} // namespace projectv::render

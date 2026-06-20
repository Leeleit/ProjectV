#pragma once

#include <array>
#include <cstdint>

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

uint32_t ComputeHzbMipLevelCount(const uint32_t baseWidth, const uint32_t baseHeight);

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

}  // namespace projectv::render

#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "core/Types.hpp"
#include "vk_mem_alloc.h"

namespace projectv::render {

struct HizBuffer {
	VkImage image = VK_NULL_HANDLE;
	VkImageView imageView = VK_NULL_HANDLE;
	VmaAllocation allocation = nullptr;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	uint32_t baseWidth = 0u;
	uint32_t baseHeight = 0u;
	uint32_t mipLevelCount = 0u;
};

bool IsHzbCullingEnabled();

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

}  // namespace projectv::render

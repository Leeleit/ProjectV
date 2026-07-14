#pragma once

#include "core/Types.hpp"

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

namespace projectv::render {

constexpr uint32_t kBindlessHeapCapacity = 256u;
constexpr uint32_t kBindlessInvalidIndex = 0xffffffffu;

// Shared UPDATE_AFTER_BIND sampled-image heap for descriptor-indexing bindless.
struct BindlessHeap {
	VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
	VkDescriptorPool pool = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;
	uint32_t capacity = 0u;
	uint32_t nextSlot = 0u;
	std::vector<VkDescriptorImageInfo> slots;
};

bool CreateBindlessHeap(
	VulkanContextState &context,
	BindlessHeap &heap,
	uint32_t capacity = kBindlessHeapCapacity);

void DestroyBindlessHeap(VulkanContextState &context, BindlessHeap &heap);

// Registers a combined image sampler; returns slot index or kBindlessInvalidIndex on failure.
uint32_t BindlessHeapRegister(
	VulkanContextState &context,
	BindlessHeap &heap,
	VkSampler sampler,
	VkImageView imageView,
	VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

void BindlessHeapUpdateSlot(
	VulkanContextState &context,
	BindlessHeap &heap,
	uint32_t slot,
	VkSampler sampler,
	VkImageView imageView,
	VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

} // namespace projectv::render

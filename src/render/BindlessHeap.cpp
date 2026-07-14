#include "render/BindlessHeap.hpp"

#include "core/RuntimeDiagnostics.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <cstring>

namespace projectv::render {

bool CreateBindlessHeap(
	VulkanContextState &context,
	BindlessHeap &heap,
	const uint32_t capacity)
{
	DestroyBindlessHeap(context, heap);
	if (!context.bindless || context.device == VK_NULL_HANDLE || capacity == 0u) {
		return false;
	}

	VkDescriptorSetLayoutBinding binding{};
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding.descriptorCount = capacity;
	binding.stageFlags = VK_SHADER_STAGE_ALL;

	const VkDescriptorBindingFlags bindingFlags =
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
		VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
		VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

	VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
	flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	flagsInfo.bindingCount = 1u;
	flagsInfo.pBindingFlags = &bindingFlags;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = &flagsInfo;
	layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	layoutInfo.bindingCount = 1u;
	layoutInfo.pBindings = &binding;
	if (vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr, &heap.setLayout) != VK_SUCCESS) {
		DestroyBindlessHeap(context, heap);
		return false;
	}

	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = capacity;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	poolInfo.maxSets = 1u;
	poolInfo.poolSizeCount = 1u;
	poolInfo.pPoolSizes = &poolSize;
	if (vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &heap.pool) != VK_SUCCESS) {
		DestroyBindlessHeap(context, heap);
		return false;
	}

	VkDescriptorSetVariableDescriptorCountAllocateInfo variableAlloc{};
	variableAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
	variableAlloc.descriptorSetCount = 1u;
	variableAlloc.pDescriptorCounts = &capacity;

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = &variableAlloc;
	allocInfo.descriptorPool = heap.pool;
	allocInfo.descriptorSetCount = 1u;
	allocInfo.pSetLayouts = &heap.setLayout;
	if (vkAllocateDescriptorSets(context.device, &allocInfo, &heap.set) != VK_SUCCESS) {
		DestroyBindlessHeap(context, heap);
		return false;
	}

	heap.capacity = capacity;
	heap.nextSlot = 0u;
	heap.slots.assign(capacity, {});
	SetVulkanObjectName(context, reinterpret_cast<uint64_t>(heap.setLayout), VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "BindlessHeapLayout");
	return true;
}

void DestroyBindlessHeap(VulkanContextState &context, BindlessHeap &heap)
{
	if (context.device != VK_NULL_HANDLE) {
		if (heap.pool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(context.device, heap.pool, nullptr);
		}
		if (heap.setLayout != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(context.device, heap.setLayout, nullptr);
		}
	}
	heap = {};
}

void BindlessHeapUpdateSlot(
	VulkanContextState &context,
	BindlessHeap &heap,
	const uint32_t slot,
	const VkSampler sampler,
	const VkImageView imageView,
	const VkImageLayout layout)
{
	if (heap.set == VK_NULL_HANDLE || slot >= heap.capacity || sampler == VK_NULL_HANDLE || imageView == VK_NULL_HANDLE) {
		return;
	}
	VkDescriptorImageInfo imageInfo{};
	imageInfo.sampler = sampler;
	imageInfo.imageView = imageView;
	imageInfo.imageLayout = layout;
	heap.slots[slot] = imageInfo;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = heap.set;
	write.dstBinding = 0;
	write.dstArrayElement = slot;
	write.descriptorCount = 1u;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &heap.slots[slot];
	vkUpdateDescriptorSets(context.device, 1u, &write, 0u, nullptr);
}

uint32_t BindlessHeapRegister(
	VulkanContextState &context,
	BindlessHeap &heap,
	const VkSampler sampler,
	const VkImageView imageView,
	const VkImageLayout layout)
{
	if (heap.nextSlot >= heap.capacity) {
		return kBindlessInvalidIndex;
	}
	const uint32_t slot = heap.nextSlot++;
	BindlessHeapUpdateSlot(context, heap, slot, sampler, imageView, layout);
	return slot;
}

} // namespace projectv::render

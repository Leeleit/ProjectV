#pragma once

#include <string>

#include "asset/MeshBaker.hpp"
#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

namespace projectv::asset {

struct MeshGpuResources {
	VkBuffer vertexBuffer = VK_NULL_HANDLE;
	VmaAllocation vertexAllocation = VK_NULL_HANDLE;
	VkBuffer indexBuffer = VK_NULL_HANDLE;
	VmaAllocation indexAllocation = VK_NULL_HANDLE;
	uint32_t vertexCount = 0;
	uint32_t indexCount = 0;
	VkDeviceSize vertexBytes = 0;
	VkDeviceSize indexBytes = 0;
};

bool UploadBakedPrimitiveToGpu(
	VkDevice device,
	VmaAllocator allocator,
	VkCommandPool commandPool,
	VkQueue queue,
	const BakedPrimitive &baked,
	MeshGpuResources &outResources,
	std::string *outError = nullptr);

void DestroyMeshGpuResources(VmaAllocator allocator, MeshGpuResources &resources);

} // namespace projectv::asset


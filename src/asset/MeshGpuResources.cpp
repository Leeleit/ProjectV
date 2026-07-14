#include "asset/MeshGpuResources.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace projectv::asset {

namespace {

bool CreateStagingBuffer(
	const VmaAllocator allocator,
	const VkDeviceSize size,
	VkBuffer &outBuffer,
	VmaAllocation &outAllocation,
	VmaAllocationInfo &outAllocationInfo)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

	return vmaCreateBuffer(
			   allocator,
			   &bufferInfo,
			   &allocationInfo,
			   &outBuffer,
			   &outAllocation,
			   &outAllocationInfo) == VK_SUCCESS;
}

bool CreateDeviceBuffer(
	const VmaAllocator allocator,
	const VkDeviceSize size,
	const VkBufferUsageFlags usage,
	VkBuffer &outBuffer,
	VmaAllocation &outAllocation)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocationInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	return vmaCreateBuffer(allocator, &bufferInfo, &allocationInfo, &outBuffer, &outAllocation, nullptr) == VK_SUCCESS;
}

bool CopyBufferViaStaging(
	const VkDevice device,
	const VmaAllocator allocator,
	const VkCommandPool commandPool,
	const VkQueue queue,
	const VkBuffer staging,
	const VkBuffer destination,
	const VkDeviceSize size)
{

	(void)allocator;

	VkCommandBufferAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = commandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	if (vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer) != VK_SUCCESS) {
		return false;
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
		return false;
	}

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, staging, destination, 1, &copyRegion);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
		return false;
	}

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	const VkResult submitResult = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
	if (submitResult != VK_SUCCESS) {
		runtime::LogVkFailure("MeshGpuResources.vkQueueSubmit", submitResult);
		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
		return false;
	}

	if (vkQueueWaitIdle(queue) != VK_SUCCESS) {
		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
		return false;
	}
	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
	return true;
}

} // namespace

bool UploadBakedPrimitiveToGpu(
	const VkDevice device,
	const VmaAllocator allocator,
	const VkCommandPool commandPool,
	const VkQueue queue,
	const bool indexTypeUint8Enabled,
	const BakedPrimitive &baked,
	MeshGpuResources &outResources,
	std::string *outError)
{
	if (outError) {
		outError->clear();
	}
	if (device == VK_NULL_HANDLE || allocator == VK_NULL_HANDLE || commandPool == VK_NULL_HANDLE || queue == VK_NULL_HANDLE) {
		if (outError) {
			*outError = "UploadBakedPrimitiveToGpu: null device / allocator / pool / queue";
		}
		return false;
	}
	if (baked.vertexBuffer.empty() || baked.indices.empty()) {
		if (outError) {
			*outError = "UploadBakedPrimitiveToGpu: empty vertex or index data";
		}
		return false;
	}

	const VkDeviceSize vertexBytes = baked.vertexBuffer.size();
	uint32_t maxIndex = 0u;
	for (const uint32_t index : baked.indices) {
		maxIndex = std::max(maxIndex, index);
	}
	VkIndexType indexType = VK_INDEX_TYPE_UINT32;
	std::vector<uint8_t> indices8;
	std::vector<uint16_t> indices16;
	const void *indexData = baked.indices.data();
	VkDeviceSize indexBytes = baked.indices.size() * sizeof(uint32_t);
	if (indexTypeUint8Enabled && maxIndex <= UINT8_MAX) {
		indices8.reserve(baked.indices.size());
		for (const uint32_t index : baked.indices) {
			indices8.push_back(static_cast<uint8_t>(index));
		}
		indexData = indices8.data();
		indexBytes = indices8.size() * sizeof(uint8_t);
		indexType = VK_INDEX_TYPE_UINT8;
	} else if (maxIndex <= UINT16_MAX) {
		indices16.reserve(baked.indices.size());
		for (const uint32_t index : baked.indices) {
			indices16.push_back(static_cast<uint16_t>(index));
		}
		indexData = indices16.data();
		indexBytes = indices16.size() * sizeof(uint16_t);
		indexType = VK_INDEX_TYPE_UINT16;
	}

	VkBuffer vertexStaging = VK_NULL_HANDLE;
	VmaAllocation vertexStagingAllocation = VK_NULL_HANDLE;
	VmaAllocationInfo vertexStagingInfo{};
	if (!CreateStagingBuffer(allocator, vertexBytes, vertexStaging, vertexStagingAllocation, vertexStagingInfo)) {
		if (outError) {
			*outError = "failed to create vertex staging buffer";
		}
		return false;
	}
	std::memcpy(vertexStagingInfo.pMappedData, baked.vertexBuffer.data(), vertexBytes);

	VkBuffer indexStaging = VK_NULL_HANDLE;
	VmaAllocation indexStagingAllocation = VK_NULL_HANDLE;
	VmaAllocationInfo indexStagingInfo{};
	if (!CreateStagingBuffer(allocator, indexBytes, indexStaging, indexStagingAllocation, indexStagingInfo)) {
		if (outError) {
			*outError = "failed to create index staging buffer";
		}
		vmaDestroyBuffer(allocator, vertexStaging, vertexStagingAllocation);
		return false;
	}
	std::memcpy(indexStagingInfo.pMappedData, indexData, indexBytes);

	MeshGpuResources result;
	if (!CreateDeviceBuffer(
			allocator,
			vertexBytes,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			result.vertexBuffer,
			result.vertexAllocation)) {
		if (outError) {
			*outError = "failed to create device-local vertex buffer";
		}
		vmaDestroyBuffer(allocator, vertexStaging, vertexStagingAllocation);
		vmaDestroyBuffer(allocator, indexStaging, indexStagingAllocation);
		return false;
	}
	if (!CreateDeviceBuffer(
			allocator,
			indexBytes,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			result.indexBuffer,
			result.indexAllocation)) {
		if (outError) {
			*outError = "failed to create device-local index buffer";
		}
		vmaDestroyBuffer(allocator, vertexStaging, vertexStagingAllocation);
		vmaDestroyBuffer(allocator, indexStaging, indexStagingAllocation);
		vmaDestroyBuffer(allocator, result.vertexBuffer, result.vertexAllocation);
		return false;
	}

	if (!CopyBufferViaStaging(device, allocator, commandPool, queue, vertexStaging, result.vertexBuffer, vertexBytes)) {
		if (outError) {
			*outError = "vertex copy command failed";
		}
		DestroyMeshGpuResources(allocator, result);
		vmaDestroyBuffer(allocator, vertexStaging, vertexStagingAllocation);
		vmaDestroyBuffer(allocator, indexStaging, indexStagingAllocation);
		return false;
	}
	if (!CopyBufferViaStaging(device, allocator, commandPool, queue, indexStaging, result.indexBuffer, indexBytes)) {
		if (outError) {
			*outError = "index copy command failed";
		}
		DestroyMeshGpuResources(allocator, result);
		vmaDestroyBuffer(allocator, vertexStaging, vertexStagingAllocation);
		vmaDestroyBuffer(allocator, indexStaging, indexStagingAllocation);
		return false;
	}

	vmaDestroyBuffer(allocator, vertexStaging, vertexStagingAllocation);
	vmaDestroyBuffer(allocator, indexStaging, indexStagingAllocation);

	result.vertexCount = baked.vertexCount;
	result.indexCount = baked.indexCount;
	result.indexType = indexType;
	result.vertexBytes = vertexBytes;
	result.indexBytes = indexBytes;
	outResources = result;
	return true;
}

void DestroyMeshGpuResources(const VmaAllocator allocator, MeshGpuResources &resources)
{
	if (resources.vertexBuffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(allocator, resources.vertexBuffer, resources.vertexAllocation);
		resources.vertexBuffer = VK_NULL_HANDLE;
		resources.vertexAllocation = VK_NULL_HANDLE;
	}
	if (resources.indexBuffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(allocator, resources.indexBuffer, resources.indexAllocation);
		resources.indexBuffer = VK_NULL_HANDLE;
		resources.indexAllocation = VK_NULL_HANDLE;
	}
	resources.vertexCount = 0;
	resources.indexCount = 0;
	resources.indexType = VK_INDEX_TYPE_UINT32;
	resources.vertexBytes = 0;
	resources.indexBytes = 0;
}

} // namespace projectv::asset

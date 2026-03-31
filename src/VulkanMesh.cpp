#include "VulkanMesh.hpp"

#include <cstddef>
#include <cstring>

namespace {
bool CreateBuffer(
	AppState *state,
	const VkDeviceSize size,
	const VkBufferUsageFlags usage,
	const VmaMemoryUsage memoryUsage,
	const VmaAllocationCreateFlags allocationFlags,
	VkBuffer *outBuffer,
	VmaAllocation *outAllocation)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = memoryUsage;
	allocInfo.flags = allocationFlags;

	return vmaCreateBuffer(
			   state->allocator,
			   &bufferInfo,
			   &allocInfo,
			   outBuffer,
			   outAllocation,
			   nullptr) == VK_SUCCESS;
}

VkCommandBuffer BeginSingleTimeCommands(AppState *state)
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = state->commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer cmd = VK_NULL_HANDLE;
	if (vkAllocateCommandBuffers(state->device, &allocInfo, &cmd) != VK_SUCCESS) {
		return VK_NULL_HANDLE;
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
		vkFreeCommandBuffers(state->device, state->commandPool, 1, &cmd);
		return VK_NULL_HANDLE;
	}

	return cmd;
}

bool EndSingleTimeCommands(AppState *state, const VkCommandBuffer cmd)
{
	if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
		vkFreeCommandBuffers(state->device, state->commandPool, 1, &cmd);
		return false;
	}

	VkCommandBufferSubmitInfo cmdInfo{};
	cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmdInfo.commandBuffer = cmd;

	VkSubmitInfo2 submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.pCommandBufferInfos = &cmdInfo;

	if (vkQueueSubmit2(state->queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
		vkFreeCommandBuffers(state->device, state->commandPool, 1, &cmd);
		return false;
	}

	if (vkQueueWaitIdle(state->queue) != VK_SUCCESS) {
		vkFreeCommandBuffers(state->device, state->commandPool, 1, &cmd);
		return false;
	}

	vkFreeCommandBuffers(state->device, state->commandPool, 1, &cmd);
	return true;
}
} // namespace

bool UploadMeshToGpu(AppState *state, const MeshCpu &cpuMesh, MeshGpu *outMesh)
{
	const uint32_t vertexCount = static_cast<uint32_t>(cpuMesh.vertices.size());
	const uint32_t indexCount = static_cast<uint32_t>(cpuMesh.indices.size());
	if (vertexCount == 0 || indexCount == 0) {
		SDL_Log("Mesh is empty");
		return false;
	}

	const VkDeviceSize vertexBufferSize = sizeof(Vertex) * vertexCount;
	const VkDeviceSize indexBufferSize = sizeof(uint32_t) * indexCount;
	const VkDeviceSize totalSize = vertexBufferSize + indexBufferSize;

	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VmaAllocation stagingAllocation = VK_NULL_HANDLE;
	if (!CreateBuffer(
			state,
			totalSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
				VMA_ALLOCATION_CREATE_MAPPED_BIT,
			&stagingBuffer,
			&stagingAllocation)) {
		SDL_Log("Failed to create mesh staging buffer");
		return false;
	}

	void *mappedData = nullptr;
	if (vmaMapMemory(state->allocator, stagingAllocation, &mappedData) != VK_SUCCESS) {
		vmaDestroyBuffer(state->allocator, stagingBuffer, stagingAllocation);
		SDL_Log("vmaMapMemory failed");
		return false;
	}

	auto *bytes = static_cast<std::byte *>(mappedData);
	std::memcpy(bytes, cpuMesh.vertices.data(), static_cast<size_t>(vertexBufferSize));
	std::memcpy(bytes + vertexBufferSize, cpuMesh.indices.data(), static_cast<size_t>(indexBufferSize));
	vmaUnmapMemory(state->allocator, stagingAllocation);

	MeshGpu mesh{};
	if (!CreateBuffer(
			state,
			vertexBufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
			0,
			&mesh.vertexBuffer,
			&mesh.vertexAllocation)) {
		vmaDestroyBuffer(state->allocator, stagingBuffer, stagingAllocation);
		SDL_Log("Failed to create mesh vertex buffer");
		return false;
	}

	if (!CreateBuffer(
			state,
			indexBufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
			0,
			&mesh.indexBuffer,
			&mesh.indexAllocation)) {
		vmaDestroyBuffer(state->allocator, mesh.vertexBuffer, mesh.vertexAllocation);
		vmaDestroyBuffer(state->allocator, stagingBuffer, stagingAllocation);
		SDL_Log("Failed to create mesh index buffer");
		return false;
	}

	const VkCommandBuffer cmd = BeginSingleTimeCommands(state);
	if (cmd == VK_NULL_HANDLE) {
		vmaDestroyBuffer(state->allocator, mesh.indexBuffer, mesh.indexAllocation);
		vmaDestroyBuffer(state->allocator, mesh.vertexBuffer, mesh.vertexAllocation);
		vmaDestroyBuffer(state->allocator, stagingBuffer, stagingAllocation);
		SDL_Log("BeginSingleTimeCommands failed");
		return false;
	}

	VkBufferCopy vertexCopy{};
	vertexCopy.size = vertexBufferSize;
	vkCmdCopyBuffer(cmd, stagingBuffer, mesh.vertexBuffer, 1, &vertexCopy);

	VkBufferCopy indexCopy{};
	indexCopy.srcOffset = vertexBufferSize;
	indexCopy.size = indexBufferSize;
	vkCmdCopyBuffer(cmd, stagingBuffer, mesh.indexBuffer, 1, &indexCopy);

	if (!EndSingleTimeCommands(state, cmd)) {
		vmaDestroyBuffer(state->allocator, mesh.indexBuffer, mesh.indexAllocation);
		vmaDestroyBuffer(state->allocator, mesh.vertexBuffer, mesh.vertexAllocation);
		vmaDestroyBuffer(state->allocator, stagingBuffer, stagingAllocation);
		SDL_Log("EndSingleTimeCommands failed");
		return false;
	}

	vmaDestroyBuffer(state->allocator, stagingBuffer, stagingAllocation);

	mesh.vertexCount = vertexCount;
	mesh.indexCount = indexCount;
	*outMesh = mesh;
	return true;
}

MeshCpu CreateTriangleMeshCpu()
{
	MeshCpu mesh;
	mesh.vertices = {
		{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
		{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
	};
	mesh.indices = {0, 1, 2};
	return mesh;
}

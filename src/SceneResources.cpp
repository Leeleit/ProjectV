#include "SceneResources.hpp"

#include <array>
#include <cstring>

namespace {
bool CreateBuffer(
	AppState *state,
	// ReSharper disable once CppDFAConstantParameter
	const VkDeviceSize size,
	// ReSharper disable once CppDFAConstantParameter
	const VkBufferUsageFlags usage,
	const VmaAllocationCreateInfo &allocationInfo,
	VkBuffer *outBuffer,
	VmaAllocation *outAllocation,
	VmaAllocationInfo *outAllocationInfo = nullptr)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	return vmaCreateBuffer(
			   state->allocator,
			   &bufferInfo,
			   &allocationInfo,
			   outBuffer,
			   outAllocation,
			   outAllocationInfo) == VK_SUCCESS;
}
} // namespace

void DestroySceneResources(AppState *state)
{
	if (!state || !state->allocator) {
		return;
	}

	if (state->sceneVertexBuffer && state->sceneVertexAllocation) {
		vmaDestroyBuffer(state->allocator, state->sceneVertexBuffer, state->sceneVertexAllocation);
		state->sceneVertexBuffer = VK_NULL_HANDLE;
		state->sceneVertexAllocation = VK_NULL_HANDLE;
	}

	state->sceneTriangleCount = 0;
}

bool CreateSceneResources(AppState *state)
{
	if (!state || !state->allocator) {
		return false;
	}

	DestroySceneResources(state);

	constexpr std::array<ComputeVertex, 9> vertices{{
		{{-0.85f, -0.75f, 0.90f, 1.0f}, {0.14f, 0.25f, 0.95f, 1.0f}},
		{{0.85f, -0.75f, 0.90f, 1.0f}, {0.08f, 0.70f, 0.98f, 1.0f}},
		{{0.00f, 0.90f, 0.90f, 1.0f}, {0.44f, 0.90f, 1.00f, 1.0f}},
		{{-0.55f, -0.35f, 0.55f, 1.0f}, {1.00f, 0.35f, 0.20f, 1.0f}},
		{{0.15f, -0.70f, 0.55f, 1.0f}, {0.95f, 0.80f, 0.18f, 1.0f}},
		{{0.55f, 0.35f, 0.55f, 1.0f}, {1.00f, 0.15f, 0.48f, 1.0f}},
		{{-0.20f, -0.15f, 0.20f, 1.0f}, {0.95f, 0.10f, 0.28f, 1.0f}},
		{{0.68f, -0.12f, 0.20f, 1.0f}, {1.00f, 0.92f, 0.30f, 1.0f}},
		{{0.20f, 0.72f, 0.20f, 1.0f}, {0.28f, 0.98f, 0.56f, 1.0f}},
	}};

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.flags =
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
		VMA_ALLOCATION_CREATE_MAPPED_BIT;
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

	VmaAllocationInfo allocationResultInfo{};
	if (!CreateBuffer(
			state,
			sizeof(vertices),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			allocationInfo,
			&state->sceneVertexBuffer,
			&state->sceneVertexAllocation,
			&allocationResultInfo)) {
		return false;
	}

	std::memcpy(allocationResultInfo.pMappedData, vertices.data(), sizeof(vertices));
	state->sceneTriangleCount = static_cast<uint32_t>(vertices.size() / 3);
	return true;
}

#include "render/NanoVdbResources.hpp"

#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"
#include "render/vulkan/VulkanInit.hpp"

namespace projectv::render {

bool RefreshNanoVdbFlattenBuffers(
	const projectv::voxel::nanovdb::NanoVdbFlattenResult &flatten,
	SceneFrameResources &frameResources)
{
	if (!flatten.uppers.empty() && !frameResources.nanovdbUpperMappedData) {
		return false;
	}
	if (!flatten.lowers.empty() && !frameResources.nanovdbLowerMappedData) {
		return false;
	}
	if (!flatten.leaves.empty() && !frameResources.nanovdbLeafMappedData) {
		return false;
	}
	if (!flatten.materials.empty() && !frameResources.nanovdbMaterialMappedData) {
		return false;
	}

	projectv::voxel::nanovdb::PackNanoVdbFlattenData(
		flatten,
		frameResources.nanovdbUpperMappedData,
		frameResources.nanovdbLowerMappedData,
		frameResources.nanovdbLeafMappedData,
		frameResources.nanovdbMaterialMappedData);
	return true;
}

uint64_t ComputeGrownNanoVdbCapacity(const uint64_t currentCapacityBytes, const uint64_t requiredCapacityBytes)
{
	if (currentCapacityBytes == 0u) {
		return std::max<uint64_t>(requiredCapacityBytes, 1u);
	}
	if (requiredCapacityBytes <= currentCapacityBytes) {
		return currentCapacityBytes;
	}
	const uint64_t grown = currentCapacityBytes + currentCapacityBytes / 2u;
	return std::max<uint64_t>(grown, requiredCapacityBytes);
}

bool GrowNanoVdbBuffer(
	VulkanContextState *context,
	RenderState &render,
	uint32_t currentFrameIndex,
	VkBuffer &buffer,
	VmaAllocation &allocation,
	void *&mappedData,
	uint64_t &capacityBytes,
	const uint64_t newCapacityBytes,
	const char *profilingTag)
{
	if (context == nullptr || context->device == VK_NULL_HANDLE || context->allocator == VK_NULL_HANDLE) {
		return false;
	}
	if (newCapacityBytes == 0u) {
		return false;
	}
	if (buffer != VK_NULL_HANDLE && allocation != nullptr) {
		profiling::RecordFree(allocation, profilingTag);
		EnqueueDeferredNanoVdbDestroy(render, currentFrameIndex, buffer, allocation);
		buffer = VK_NULL_HANDLE;
		allocation = nullptr;
		mappedData = nullptr;
		capacityBytes = 0u;
	}
	VmaAllocationCreateInfo allocationCreateInfo{};
	VkBufferCreateInfo bufferCreateInfo{};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = newCapacityBytes;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	const VkResult createResult = vmaCreateBuffer(
		context->allocator,
		&bufferCreateInfo,
		&allocationCreateInfo,
		&buffer,
		&allocation,
		nullptr);
	if (createResult != VK_SUCCESS) {
		runtime::LogVkFailure("GrowNanoVdbBuffer.vmaCreateBuffer", createResult);
		buffer = VK_NULL_HANDLE;
		allocation = nullptr;
		return false;
	}
	VmaAllocationInfo allocInfo{};
	vmaGetAllocationInfo(context->allocator, allocation, &allocInfo);
	mappedData = allocInfo.pMappedData;
	capacityBytes = allocInfo.size;
	profiling::RecordAllocation(allocation, allocInfo.size, profilingTag);
	return true;
}

void EnqueueDeferredNanoVdbDestroy(
	RenderState &render,
	uint32_t frameIndex,
	VkBuffer buffer,
	VmaAllocation allocation)
{
	if (frameIndex >= MAX_FRAMES_IN_FLIGHT) {
		return;
	}
	render.deferredNanoVdbDestroys[frameIndex].push_back({buffer, allocation});
}

void DrainDeferredNanoVdbDestroysForFrame(
	VulkanContextState *context,
	RenderState &render,
	uint32_t frameIndex)
{
	if (frameIndex >= MAX_FRAMES_IN_FLIGHT || context == nullptr) {
		return;
	}
	auto &queue = render.deferredNanoVdbDestroys[frameIndex];
	if (queue.empty()) {
		return;
	}
	for (auto &entry : queue) {
		if (entry.buffer != VK_NULL_HANDLE && entry.allocation != nullptr) {
			vmaDestroyBuffer(context->allocator, entry.buffer, entry.allocation);
		}
	}
	queue.clear();
}

void DrainAllDeferredNanoVdbDestroys(
	VulkanContextState *context,
	RenderState &render)
{
	if (context == nullptr) {
		return;
	}
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		DrainDeferredNanoVdbDestroysForFrame(context, render, i);
	}
}

} // namespace projectv::render
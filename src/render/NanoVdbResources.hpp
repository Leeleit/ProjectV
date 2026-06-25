#pragma once

#include "core/Types.hpp"
#include "voxel/NanoVdb.hpp"

namespace projectv::render {

bool RefreshNanoVdbFlattenBuffers(
	const projectv::voxel::nanovdb::NanoVdbFlattenResult &flatten,
	SceneFrameResources &frameResources);

uint64_t ComputeGrownNanoVdbCapacity(
	const uint64_t currentCapacityBytes,
	const uint64_t requiredCapacityBytes);

bool GrowNanoVdbBuffer(
	VulkanContextState *context,
	RenderState &render,
	uint32_t currentFrameIndex,
	VkBuffer &buffer,
	VmaAllocation &allocation,
	void *&mappedData,
	uint64_t &capacityBytes,
	const uint64_t newCapacityBytes,
	const char *profilingTag);

void EnqueueDeferredNanoVdbDestroy(
	RenderState &render,
	uint32_t frameIndex,
	VkBuffer buffer,
	VmaAllocation allocation);

void DrainDeferredNanoVdbDestroysForFrame(
	VulkanContextState *context,
	RenderState &render,
	uint32_t frameIndex);

void DrainAllDeferredNanoVdbDestroys(
	VulkanContextState *context,
	RenderState &render);

} // namespace projectv::render
#pragma once

#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include <cstdint>

#include <vulkan/vulkan.h>

namespace projectv::render {

bool IsAsyncComputeResourcesAllocated(const VulkanContextState &context);

bool EnsureAsyncComputeResources(VulkanContextState *context);

void DestroyAsyncComputeResources(VulkanContextState *context);

bool RecordAsyncComputePass(
	VkCommandBuffer asyncCommandBuffer,
	VulkanContextState &context,
	RenderState &render,
	AppState *state,
	FrameState *frame);

bool SubmitToComputeQueue(
	VulkanContextState *context,
	VkCommandBuffer commandBuffer,
	uint64_t *outTimelineValue);

bool RecordHzbAsyncCullPass(
	VkCommandBuffer asyncCommandBuffer,
	VulkanContextState &context,
	RenderState &render,
	const float (&inverseViewProjection)[16],
	uint32_t chunkDescriptorCount);

bool SubmitHzbAsyncCullToComputeQueue(
	VulkanContextState *context,
	VkCommandBuffer asyncCommandBuffer,
	uint64_t *outTimelineValue);

}  // namespace projectv::render

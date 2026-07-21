#pragma once

#include "core/EnvUtils.hpp"
#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include <vulkan/vulkan.h>

namespace projectv::render {

inline bool IsAsyncComputeEnabled()
{
	if (const char *value = core::GetEnvVar("PROJECTV_ASYNC_COMPUTE")) {
		return value[0] != '\0' && value[0] != '0';
	}
	return true;
}

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

bool SubmitToComputeQueue(
	VulkanContextState *context,
	VkCommandBuffer commandBuffer,
	uint64_t *outTimelineValue);

} // namespace projectv::render

#pragma once

#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
#include <array>
#include <vulkan/vulkan.h>

namespace projectv::render {

uint32_t GetMinVulkanApiVersion();

inline VkSharingMode ChooseSharingMode(
	const VulkanContextState &context,
	std::array<uint32_t, 2> &outQueueFamilies) noexcept
{
	if (context.hasDedicatedComputeQueue &&
		context.queueFamilyIndex != context.dedicatedComputeQueueFamilyIndex) {
		outQueueFamilies[0] = context.queueFamilyIndex;
		outQueueFamilies[1] = context.dedicatedComputeQueueFamilyIndex;
		return VK_SHARING_MODE_CONCURRENT;
	}
	return VK_SHARING_MODE_EXCLUSIVE;
}

} // namespace projectv::render

bool InitializeVulkanBase(
	PlatformState *platform,
	VulkanContextState *context,
	FrameState *frame);

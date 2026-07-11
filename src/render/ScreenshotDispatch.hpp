#pragma once

#include "core/Types.hpp"

struct SwapchainState;
struct VulkanContextState;

namespace projectv::render {

void RecordSwapchainScreenshotCopy(
	const SwapchainState &swapchain,
	RenderState &render,
	VkCommandBuffer cmd,
	uint32_t imageIndex);

bool SaveRequestedScreenshot(
	VulkanContextState &context,
	RenderState &render,
	VkFence inFlightFence,
	VkExtent2D captureExtent,
	VkFormat captureFormat);

} // namespace projectv::render
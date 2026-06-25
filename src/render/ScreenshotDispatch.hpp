#pragma once

#include "core/Types.hpp"

struct SwapchainState;
struct VulkanContextState;

namespace projectv::render {

void RecordSwapchainScreenshotCopy(
	const SwapchainState &swapchain,
	RenderState &render,
	const VkCommandBuffer cmd,
	const uint32_t imageIndex);

bool SaveRequestedScreenshot(
	VulkanContextState &context,
	RenderState &render,
	const VkFence inFlightFence,
	const VkExtent2D captureExtent,
	const VkFormat captureFormat);

} // namespace projectv::render
#ifndef VULKAN_GRAPHICS_PIPELINE_HPP
#define VULKAN_GRAPHICS_PIPELINE_HPP

#include "Types.hpp"

bool CreateGraphicsPipeline(
	VulkanContextState *context,
	const SwapchainState *swapchain,
	RenderState *render);
void DestroyGraphicsPipeline(
	VulkanContextState *context,
	RenderState *render);

#endif

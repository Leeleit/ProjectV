#pragma once

#include "core/Types.hpp"

bool CreateGraphicsPipeline(
	VulkanContextState *context,
	const SwapchainState *swapchain,
	RenderState *render);
bool RefreshGraphicsResourceBindings(
	VulkanContextState *context,
	RenderState *render);
void DestroyGraphicsPipeline(
	VulkanContextState *context,
	RenderState *render);


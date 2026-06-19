#pragma once



#include "core/Types.hpp"

struct VulkanContextState;
struct SwapchainState;

bool CreateTaaResolvePipeline(
	VulkanContextState *context,
	const SwapchainState *swapchain,
	RenderState *render);

bool RefreshTaaResolveResourceBindings(
	VulkanContextState *context,
	RenderState *render);

void DestroyTaaResolvePipeline(
	VulkanContextState *context,
	RenderState *render);


#pragma once



#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

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


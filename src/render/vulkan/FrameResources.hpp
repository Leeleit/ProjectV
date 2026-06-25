#pragma once

#include "core/Types.hpp"

struct SwapchainState;
struct VulkanContextState;

namespace projectv::render {

bool CreateDepthResources(
	VulkanContextState *context,
	const SwapchainState *swapchain,
	RenderState *render);

void DestroyDepthResources(
	VulkanContextState *context,
	RenderState *render);

bool CreateShadowResources(
	VulkanContextState *context,
	RenderState *render);

void DestroyShadowResources(
	VulkanContextState *context,
	RenderState *render);

bool CreateScreenshotReadbackResources(
	VulkanContextState *context,
	const SwapchainState *swapchain,
	RenderState *render);

void DestroyScreenshotReadbackResources(
	VulkanContextState *context,
	RenderState *render);

} // namespace projectv::render
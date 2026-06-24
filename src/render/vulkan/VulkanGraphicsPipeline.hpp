#pragma once

#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

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


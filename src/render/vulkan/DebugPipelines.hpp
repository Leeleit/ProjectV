#pragma once

#include "core/Types.hpp"

namespace projectv::render {

bool CreateDebugOverlayPipeline(
	VulkanContextState &context,
	const SwapchainState &swapchain,
	RenderState &render);

void DestroyDebugOverlayPipeline(
	VulkanContextState &context,
	RenderState &render);

bool CreateDebugHudPipeline(
	VulkanContextState &context,
	RenderState &render);

void DestroyDebugHudPipeline(
	VulkanContextState &context,
	RenderState &render);

} // namespace projectv::render
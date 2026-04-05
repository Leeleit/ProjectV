#ifndef FRAME_PREPARATION_HPP
#define FRAME_PREPARATION_HPP

#include "Types.hpp"

bool PrepareFrameRenderData(
	VulkanContextState *context,
	const SwapchainState *swapchain,
	const CameraState *camera,
	WorldState *world,
	RenderState *render,
	FrameState *frame);

#endif

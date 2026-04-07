#ifndef FRAME_PREPARATION_HPP
#define FRAME_PREPARATION_HPP

#include "Types.hpp"

bool PrepareFrameRenderData(
	VulkanContextState *context,
	const SwapchainState *swapchain,
	const CameraState *camera,
	const InteractionState *interaction,
	const DebugState *debug,
	WorldState *world,
	RenderState *render,
	FrameState *frame);

#endif

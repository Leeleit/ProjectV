#pragma once

#include "core/Types.hpp"

SDL_AppResult DrawFrame(
	PlatformState *platform,
	VulkanContextState *context,
	SwapchainState *swapchain,
	RenderState *render,
	FrameState *frame);


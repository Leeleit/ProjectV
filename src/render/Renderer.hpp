#pragma once

#include "core/Types.hpp"

SDL_AppResult DrawFrame(
	AppState *state,
	PlatformState *platform,
	VulkanContextState *context,
	SwapchainState *swapchain,
	RenderState *render,
	FrameState *frame);


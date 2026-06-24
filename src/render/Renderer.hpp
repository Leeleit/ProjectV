#pragma once

#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

SDL_AppResult DrawFrame(
	AppState *state,
	PlatformState *platform,
	VulkanContextState *context,
	SwapchainState *swapchain,
	RenderState *render,
	FrameState *frame);


#pragma once

#include "core/Types.hpp"

bool PrepareFrameRenderData(
	VulkanContextState *context,
	const SwapchainState *swapchain,
	const CameraState *camera,
	const InteractionState *interaction,
	const DebugState *debug,
	WorldState *world,
	RenderState *render,
	FrameState *frame,
	InputState *input);


void BuildVisibleModelInstanceList(
	const ChunkCullingParameters &parameters,
	RenderState *render);


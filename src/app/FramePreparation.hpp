#pragma once

#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

[[nodiscard]] bool PrepareFrameRenderData(
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


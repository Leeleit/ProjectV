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

// M5: build the per-frame frustum-culled model draw list. Walks
// `render->modelInstances` and copies the ones whose world AABB
// intersects the camera frustum (constructed from the same
// `ChunkCullingParameters` that drives chunk visibility) into
// `render->visibleModelInstances`. Capacity of the destination is
// preserved across calls. This is the per-frame cull that
// `Renderer::RecordModelCommands` then iterates.
void BuildVisibleModelInstanceList(
	const ChunkCullingParameters &parameters,
	RenderState *render);


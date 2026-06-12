#ifndef DEBUG_OVERLAYS_HPP
#define DEBUG_OVERLAYS_HPP

#include "core/Types.hpp"

void BuildDebugOverlayBoxes(
	const VoxelWorld *world,
	const InteractionState &interaction,
	const DebugState &debug,
	std::vector<DebugOverlayBox> *outBoxes,
	const CameraState &camera = CameraState{},
	const RenderState &render = RenderState{});

#endif

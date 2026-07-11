#pragma once

#include "core/Types.hpp"

void BuildDebugOverlayBoxes(
	const VoxelWorld *world,
	const InteractionState &interaction,
	const DebugState &debug,
	std::vector<DebugOverlayBox> *outBoxes);


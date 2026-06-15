#pragma once

#include "core/Types.hpp"

struct PhysicsState;

void HandleInteractionEvent(
	InputState *input,
	const SDL_Event *event);
void UpdateVoxelInteraction(
	const CameraState &camera,
	InputState *input,
	VoxelWorld *world,
	InteractionState *interaction,
	bool allowEditing,
	const PhysicsState *physics);


#ifndef INPUT_ACTIONS_HPP
#define INPUT_ACTIONS_HPP

#include "core/Types.hpp"

void InitializeInputState(InputState *input);
void HandleInputActionEvent(
	InputState *input,
	const SDL_Event *event);
bool IsInputActionDown(
	const InputState &input,
	InputAction action);
bool ConsumeInputActionPressed(
	InputState *input,
	InputAction action);
VoxelMaterial GetNextPlacementMaterial(VoxelMaterial currentMaterial);

#endif

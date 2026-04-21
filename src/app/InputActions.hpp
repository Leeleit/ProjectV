#ifndef INPUT_ACTIONS_HPP
#define INPUT_ACTIONS_HPP

#include "core/Types.hpp"

void InitializeInputState(InputState &input);
void HandleInputActionEvent(
	InputState &input,
	const SDL_Event *event);
bool IsInputActionDown(
	const InputState &input,
	InputAction action);
bool ConsumeInputActionPressed(
	InputState &input,
	InputAction action);
uint32_t GetInputActionDownMask(const InputState &input);
uint32_t GetInputActionPressedMask(const InputState &input);
void ApplyInputActionSnapshot(
	InputState &input,
	uint32_t downMask,
	uint32_t pressedMask);
VoxelMaterial GetNextPlacementMaterial(VoxelMaterial currentMaterial);

#endif

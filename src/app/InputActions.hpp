#pragma once

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
uint64_t GetInputActionDownMask(const InputState &input);
uint64_t GetInputActionPressedMask(const InputState &input);
void ApplyInputActionSnapshot(
	InputState &input,
	uint64_t downMask,
	uint64_t pressedMask);
VoxelMaterial GetNextPlacementMaterial(VoxelMaterial currentMaterial);


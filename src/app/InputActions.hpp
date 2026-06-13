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
// **Tier 5 (`2026-06-13`).** `uint64_t` mask type.
// The 58-action `InputAction` inventory exceeds 32
// bits; the previous `uint32_t` return type silently
// lost high bits and invoked UB on the
// `1u << actionIndex` shift. See the matching note
// in `core/Types.hpp::InputReplayFrame`.
uint64_t GetInputActionDownMask(const InputState &input);
uint64_t GetInputActionPressedMask(const InputState &input);
void ApplyInputActionSnapshot(
	InputState &input,
	uint64_t downMask,
	uint64_t pressedMask);
VoxelMaterial GetNextPlacementMaterial(VoxelMaterial currentMaterial);

#endif

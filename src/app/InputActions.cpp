#include "app/InputActions.hpp"

#include <array>

namespace {
constexpr size_t GetInputActionIndex(const InputAction action)
{
	return static_cast<size_t>(action);
}

void BindAction(
	InputState *input,
	const InputAction action,
	const SDL_Scancode primaryScancode,
	const SDL_Scancode secondaryScancode = SDL_SCANCODE_UNKNOWN)
{
	if (!input) {
		return;
	}

	input->bindings[GetInputActionIndex(action)].scancodes = {
		primaryScancode,
		secondaryScancode,
	};
}

void SetActionState(
	InputState *input,
	const InputAction action,
	const size_t bindingSlot,
	const bool isDown,
	const bool isRepeat)
{
	if (!input) {
		return;
	}

	InputActionBinding &binding = input->bindings[GetInputActionIndex(action)];
	binding.downStates[bindingSlot] = isDown;

	InputActionButtonState &buttonState = input->actions[GetInputActionIndex(action)];
	buttonState.down = false;
	for (const bool slotDown : binding.downStates) {
		buttonState.down = buttonState.down || slotDown;
	}
	if (isDown && !isRepeat) {
		buttonState.pressed = true;
	}
}

void ResetBindingStates(InputState *input)
{
	if (!input) {
		return;
	}

	for (InputActionBinding &binding : input->bindings) {
		binding.downStates = {};
	}
}

void ResetActionStates(InputState *input)
{
	if (!input) {
		return;
	}

	for (InputActionButtonState &buttonState : input->actions) {
		buttonState = {};
	}
}
} // namespace

void InitializeInputState(InputState *input)
{
	if (!input) {
		return;
	}

	input->mouseDeltaX = 0.0f;
	input->mouseDeltaY = 0.0f;
	input->removePressed = false;
	input->placePressed = false;
	input->relativeMouseModeEnabled = true;
	ResetActionStates(input);
	ResetBindingStates(input);
	for (InputActionBinding &binding : input->bindings) {
		binding.scancodes = {
			SDL_SCANCODE_UNKNOWN,
			SDL_SCANCODE_UNKNOWN,
		};
	}

	BindAction(input, InputAction::MoveForward, SDL_SCANCODE_W);
	BindAction(input, InputAction::MoveBackward, SDL_SCANCODE_S);
	BindAction(input, InputAction::MoveLeft, SDL_SCANCODE_A);
	BindAction(input, InputAction::MoveRight, SDL_SCANCODE_D);
	BindAction(input, InputAction::MoveUp, SDL_SCANCODE_SPACE);
	BindAction(input, InputAction::MoveDown, SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT);
	BindAction(input, InputAction::SpeedBoost, SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL);
	BindAction(input, InputAction::SpeedSlow, SDL_SCANCODE_LALT, SDL_SCANCODE_RALT);
	BindAction(input, InputAction::ToggleHud, SDL_SCANCODE_F1);
	BindAction(input, InputAction::ToggleRelativeMouseMode, SDL_SCANCODE_TAB);
	BindAction(input, InputAction::CyclePlacementMaterial, SDL_SCANCODE_F2);
	BindAction(input, InputAction::ResetCamera, SDL_SCANCODE_F3);
	BindAction(input, InputAction::TogglePause, SDL_SCANCODE_P);
	BindAction(input, InputAction::ToggleControlMode, SDL_SCANCODE_F4);
}

void HandleInputActionEvent(
	InputState *input,
	const SDL_Event *event)
{
	if (!input || !event ||
		(event->type != SDL_EVENT_KEY_DOWN && event->type != SDL_EVENT_KEY_UP)) {
		return;
	}

	const SDL_Scancode scancode = event->key.scancode;
	const bool isDown = event->type == SDL_EVENT_KEY_DOWN;
	for (size_t actionIndex = 0; actionIndex < input->bindings.size(); ++actionIndex) {
		const InputAction action = static_cast<InputAction>(actionIndex);
		for (size_t slotIndex = 0; slotIndex < input->bindings[actionIndex].scancodes.size(); ++slotIndex) {
			const SDL_Scancode boundScancode = input->bindings[actionIndex].scancodes[slotIndex];
			if (boundScancode == SDL_SCANCODE_UNKNOWN || boundScancode != scancode) {
				continue;
			}

			SetActionState(input, action, slotIndex, isDown, event->key.repeat);
			break;
		}
	}
}

bool IsInputActionDown(
	const InputState &input,
	const InputAction action)
{
	return input.actions[GetInputActionIndex(action)].down;
}

bool ConsumeInputActionPressed(
	InputState *input,
	const InputAction action)
{
	if (!input) {
		return false;
	}

	InputActionButtonState &buttonState = input->actions[GetInputActionIndex(action)];
	const bool wasPressed = buttonState.pressed;
	buttonState.pressed = false;
	return wasPressed;
}

VoxelMaterial GetNextPlacementMaterial(const VoxelMaterial currentMaterial)
{
	switch (currentMaterial) {
	case VoxelMaterial::FloorWhite:
		return VoxelMaterial::FloorGray;
	case VoxelMaterial::FloorGray:
		return VoxelMaterial::Glass;
	case VoxelMaterial::Glass:
		return VoxelMaterial::Fluid;
	case VoxelMaterial::Fluid:
	case VoxelMaterial::Air:
		return VoxelMaterial::FloorWhite;
	}

	return VoxelMaterial::FloorWhite;
}

#include "app/InputActions.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include <array>

namespace {
constexpr Uint64 kMoveUpDoubleTapWindowNs = SDL_MS_TO_NS(300);

constexpr size_t GetInputActionIndex(const InputAction action)
{
	return static_cast<size_t>(action);
}

constexpr bool IsInputActionReplayRecordable(const InputAction action)
{
	switch (action) {
	case InputAction::DecreaseLightingExposure:
	case InputAction::IncreaseLightingExposure:
	case InputAction::CycleToneMapOperator:
	case InputAction::CycleLightingDebugView:
	case InputAction::ResetLightingDebugControls:
	case InputAction::CaptureScreenshot:
	case InputAction::ToggleInputReplayRecording:
	case InputAction::PlayLastInputReplay:
	case InputAction::Count:
		return false;
	default:
		return true;
	}
}

void BindAction(
	InputState &input,
	const InputAction action,
	const SDL_Scancode primaryScancode,
	const SDL_Scancode secondaryScancode = SDL_SCANCODE_UNKNOWN)
{
	input.bindings[GetInputActionIndex(action)].scancodes = {
		primaryScancode,
		secondaryScancode,
	};
}

void SetActionState(
	InputState &input,
	const InputAction action,
	const size_t bindingSlot,
	const bool isDown,
	const bool isRepeat)
{
	auto &[scancodes, downStates] = input.bindings[GetInputActionIndex(action)];
	downStates[bindingSlot] = isDown;

	auto &[down, pressed] = input.actions[GetInputActionIndex(action)];
	down = false;
	for (const bool slotDown : downStates) {
		down = down || slotDown;
	}
	if (isDown && !isRepeat) {
		pressed = true;
	}
}

Uint64 GetEventTimestampNs(const SDL_Event &event)
{
	return event.key.timestamp != 0 ? event.key.timestamp : SDL_GetTicksNS();
}

bool DetectMoveUpDoubleTap(
	InputState &input,
	const Uint64 pressedTimestampNs)
{
	const Uint64 previousPressedTimestampNs = input.lastMoveUpPressedTimestampNs;
	input.lastMoveUpPressedTimestampNs = pressedTimestampNs;
	if (previousPressedTimestampNs != 0 &&
		pressedTimestampNs >= previousPressedTimestampNs &&
		pressedTimestampNs - previousPressedTimestampNs <= kMoveUpDoubleTapWindowNs) {
		input.lastMoveUpPressedTimestampNs = 0;
		return true;
	}

	return false;
}

void ResetBindingStates(InputState &input)
{
	for (auto &[scancodes, downStates] : input.bindings) {
		downStates = {};
	}
}

void ResetActionStates(InputState &input)
{
	for (InputActionButtonState &buttonState : input.actions) {
		buttonState = {};
	}
}
} // namespace

void InitializeInputState(InputState &input)
{
	input.mouseDeltaX = 0.0f;
	input.mouseDeltaY = 0.0f;
	input.removePressed = false;
	input.placePressed = false;
	input.relativeMouseModeEnabled = true;
	input.lastMoveUpPressedTimestampNs = 0;
	ResetActionStates(input);
	ResetBindingStates(input);
	for (auto &[scancodes, downStates] : input.bindings) {
		scancodes = {
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
	BindAction(input, InputAction::ToggleDetailedHud, SDL_SCANCODE_G);
	BindAction(input, InputAction::ToggleRelativeMouseMode, SDL_SCANCODE_TAB);
	BindAction(input, InputAction::CyclePlacementMaterial, SDL_SCANCODE_F2);
	BindAction(input, InputAction::ResetCamera, SDL_SCANCODE_F3);
	BindAction(input, InputAction::TogglePause, SDL_SCANCODE_P);
	BindAction(input, InputAction::ToggleControlMode, SDL_SCANCODE_F4);
	BindAction(input, InputAction::CycleScenePreset, SDL_SCANCODE_F5);
	BindAction(input, InputAction::SaveWorldSnapshot, SDL_SCANCODE_F6);
	BindAction(input, InputAction::LoadWorldSnapshot, SDL_SCANCODE_F7);
	BindAction(input, InputAction::CycleEditorTool, SDL_SCANCODE_F8);
	BindAction(input, InputAction::ToggleChunkBounds, SDL_SCANCODE_F9);
	BindAction(input, InputAction::ToggleDirtyChunkOverlay, SDL_SCANCODE_F10);
	BindAction(input, InputAction::ToggleWalkAirControlMode, SDL_SCANCODE_F11);
	BindAction(input, InputAction::ToggleWalkAutoJump, SDL_SCANCODE_J);
	BindAction(input, InputAction::ToggleWalkAutoJumpDelay, SDL_SCANCODE_F12);
	BindAction(input, InputAction::CaptureScreenshot, SDL_SCANCODE_C);
	BindAction(input, InputAction::DecreaseLightingExposure, SDL_SCANCODE_H);
	BindAction(input, InputAction::IncreaseLightingExposure, SDL_SCANCODE_K);
	BindAction(input, InputAction::CycleToneMapOperator, SDL_SCANCODE_N);
	BindAction(input, InputAction::CycleLightingDebugView, SDL_SCANCODE_B);
	BindAction(input, InputAction::ResetLightingDebugControls, SDL_SCANCODE_V);
	BindAction(input, InputAction::ToggleInputReplayRecording, SDL_SCANCODE_R);
	BindAction(input, InputAction::PlayLastInputReplay, SDL_SCANCODE_Y);
	BindAction(input, InputAction::ToggleMutationAnchor, SDL_SCANCODE_X);
	BindAction(input, InputAction::PickTargetMaterial, SDL_SCANCODE_M);

	BindAction(input, InputAction::PickModel, SDL_SCANCODE_F);

	BindAction(input, InputAction::ToggleCursorHitNormal, SDL_SCANCODE_Z);
	BindAction(input, InputAction::DecreaseTimeScale, SDL_SCANCODE_LEFTBRACKET);
	BindAction(input, InputAction::IncreaseTimeScale, SDL_SCANCODE_RIGHTBRACKET);
	BindAction(input, InputAction::StepSingleFrame, SDL_SCANCODE_BACKSLASH);
	BindAction(input, InputAction::ResetTimeScale, SDL_SCANCODE_GRAVE);
	BindAction(input, InputAction::ToggleMusicPlayPause, SDL_SCANCODE_Q);
	BindAction(input, InputAction::StopMusic, SDL_SCANCODE_E);
	BindAction(input, InputAction::MusicVolumeDown, SDL_SCANCODE_7);
	BindAction(input, InputAction::MusicVolumeUp, SDL_SCANCODE_8);
	BindAction(input, InputAction::NextMusicTrack, SDL_SCANCODE_9);
	BindAction(input, InputAction::PreviousMusicTrack, SDL_SCANCODE_0);
}

void HandleInputActionEvent(
	InputState &input,
	const SDL_Event *event)
{
	if (!event ||
		(event->type != SDL_EVENT_KEY_DOWN && event->type != SDL_EVENT_KEY_UP)) {
		return;
	}

	const SDL_Scancode scancode = event->key.scancode;
	const bool isDown = event->type == SDL_EVENT_KEY_DOWN;
	for (size_t actionIndex = 0; actionIndex < input.bindings.size(); ++actionIndex) {
		const InputAction action = static_cast<InputAction>(actionIndex);
		for (size_t slotIndex = 0; slotIndex < input.bindings[actionIndex].scancodes.size(); ++slotIndex) {
			const SDL_Scancode boundScancode = input.bindings[actionIndex].scancodes[slotIndex];
			if (boundScancode == SDL_SCANCODE_UNKNOWN || boundScancode != scancode) {
				continue;
			}

			SetActionState(input, action, slotIndex, isDown, event->key.repeat);
			if (action == InputAction::MoveUp &&
				isDown &&
				!event->key.repeat &&
				DetectMoveUpDoubleTap(input, GetEventTimestampNs(*event))) {
				input.actions[GetInputActionIndex(InputAction::ToggleWalkCreativeMode)].pressed = true;
			}
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
	InputState &input,
	const InputAction action)
{
	auto &[down, pressed] = input.actions[GetInputActionIndex(action)];
	const bool wasPressed = pressed;
	pressed = false;
	return wasPressed;
}

uint64_t GetInputActionDownMask(const InputState &input)
{
	uint64_t mask = 0;
	for (size_t actionIndex = 0; actionIndex < input.actions.size(); ++actionIndex) {
		const InputAction action = static_cast<InputAction>(actionIndex);
		if (!IsInputActionReplayRecordable(action) || !input.actions[actionIndex].down) {
			continue;
		}

		mask |= 1ull << actionIndex;
	}

	return mask;
}

uint64_t GetInputActionPressedMask(const InputState &input)
{
	uint64_t mask = 0;
	for (size_t actionIndex = 0; actionIndex < input.actions.size(); ++actionIndex) {
		const InputAction action = static_cast<InputAction>(actionIndex);
		if (!IsInputActionReplayRecordable(action) || !input.actions[actionIndex].pressed) {
			continue;
		}

		mask |= 1ull << actionIndex;
	}

	return mask;
}

void ApplyInputActionSnapshot(
	InputState &input,
	const uint64_t downMask,
	const uint64_t pressedMask)
{
	input.lastMoveUpPressedTimestampNs = 0;
	for (size_t actionIndex = 0; actionIndex < input.actions.size(); ++actionIndex) {
		const InputAction action = static_cast<InputAction>(actionIndex);
		const bool isRecordable = IsInputActionReplayRecordable(action);
		const bool isDown = isRecordable && (downMask & 1ull << actionIndex) != 0u;
		const bool isPressed = isRecordable && (pressedMask & 1ull << actionIndex) != 0u;
		input.actions[actionIndex].down = isDown;
		input.actions[actionIndex].pressed = isPressed;
		input.bindings[actionIndex].downStates = {};
		if (isDown && input.bindings[actionIndex].scancodes[0] != SDL_SCANCODE_UNKNOWN) {
			input.bindings[actionIndex].downStates[0] = true;
		}
	}
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

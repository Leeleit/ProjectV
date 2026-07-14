#pragma once

#if defined(__clang__) && defined(_MSC_VER)

#else
import projectv.math;
#endif

#include "SDL3/SDL.h"
import projectv.math;
#include "voxel/VoxelMaterials.hpp"

#include <array>
#include <filesystem>
#include <vector>

enum class InputAction : uint8_t {
	MoveForward,
	MoveBackward,
	MoveLeft,
	MoveRight,
	MoveUp,
	MoveDown,
	SpeedBoost,
	SpeedSlow,
	ToggleHud,
	ToggleDetailedHud,
	ToggleRelativeMouseMode,
	CyclePlacementMaterial,
	ResetCamera,
	TogglePause,
	ToggleControlMode,
	ToggleWalkCreativeMode,
	CycleScenePreset,
	SaveWorldSnapshot,
	LoadWorldSnapshot,
	CycleEditorTool,
	ToggleChunkBounds,
	ToggleDirtyChunkOverlay,
	ToggleWalkAirControlMode,
	ToggleWalkAutoJump,
	ToggleWalkAutoJumpDelay, // reserved: bit-index stable; delay removed (always 0)
	DecreaseLightingExposure,
	IncreaseLightingExposure,
	CycleToneMapOperator,
	CycleLightingDebugView,
	ResetLightingDebugControls,
	ToggleInputReplayRecording,
	PlayLastInputReplay,
	ToggleMutationAnchor,
	PickTargetMaterial,
	CaptureScreenshot,
	PickModel,
	ToggleCursorHitNormal,
	DecreaseTimeScale,
	IncreaseTimeScale,
	StepSingleFrame,
	ResetTimeScale,
	ToggleMusicPlayPause,
	StopMusic,
	MusicVolumeDown,
	MusicVolumeUp,
	NextMusicTrack,
	PreviousMusicTrack,
	CycleMsaaMode,
	ToggleSmaa,
	CycleRenderScale,
	OpenHudSettings,
	Count,
};

constexpr size_t kInputActionCount = static_cast<size_t>(InputAction::Count);
constexpr size_t kInputActionBindingSlotCount = 2;

static_assert(
	kInputActionCount <= 64,
	"InputAction bit-mask overflow: InputReplayFrame::actionDownMask / "
	"actionPressedMask are uint64_t (64 bits); >64 actions require "
	"either a multi-mask representation or a wider type.");

struct InputActionButtonState {
	bool down = false;
	bool pressed = false;
};

struct InputActionBinding {
	std::array<SDL_Scancode, kInputActionBindingSlotCount> scancodes{
		SDL_SCANCODE_UNKNOWN,
		SDL_SCANCODE_UNKNOWN,
	};
	std::array<bool, kInputActionBindingSlotCount> downStates{};
};

struct CameraState {
	projectv::math::Vec3 position{0.0f, 8.0f, 24.0f};
	float yawRadians = 0.0f;
	float pitchRadians = -0.2f;
	float moveSpeed = 10.0f;
	float mouseSensitivity = 0.0025f;
	float verticalFovRadians = 1.0471976f;
	float nearPlane = 0.1f;
	float farPlane = 128.0f;
	enum class ControlMode : uint8_t {
		Creative,
		Spectator,
		Walk,
	} controlMode = ControlMode::Creative;
};

enum class WalkAirControlMode : uint8_t {
	MinecraftLike = 0,
	Realistic,
};

enum class DebugEditorTool : uint8_t {
	Classic = 0,
	Paint,
	Erase,
	Fill,
	Inspect,
};

struct InteractionSelectionState {
	bool hasHit = false;
	bool hasPlacementVoxel = false;
	bool hasTargetChunk = false;
	bool hasPlacementChunk = false;
	bool targetSolid = false;
	Int3 targetVoxel{};
	Int3 placementVoxel{};
	Int3 hitNormal{};
	Int3 targetVoxelInChunk{};
	Int3 placementVoxelInChunk{};
	Int3 targetChunkCoord{};
	Int3 targetChunkMin{};
	Int3 targetChunkMaxExclusive{};
	Int3 placementChunkCoord{};
	Int3 placementChunkMin{};
	Int3 placementChunkMaxExclusive{};
	VoxelMaterial targetMaterial = VoxelMaterial::Air;
	float hitDistance = 0.0f;
	bool targetChunkDirty = false;
	bool targetChunkActive = false;
	bool placementChunkDirty = false;
	bool placementChunkActive = false;
	uint32_t targetChunkIndex = 0;
	uint32_t placementChunkIndex = 0;
	uint32_t targetChunkNonAirVoxelCount = 0;
	uint32_t placementChunkNonAirVoxelCount = 0;
};

struct InteractionState {
	InteractionSelectionState selection{};
	VoxelMaterial placementMaterial = VoxelMaterial::FloorWhite;
	float maxInteractionDistance = 12.0f;
	DebugEditorTool editorTool = DebugEditorTool::Classic;
	bool mutationAnchorValid = false;
	Int3 mutationAnchorVoxel{};
	bool mutationAnchorUsesPlacementVoxel = false;
};

struct InputReplayFrame {
	float deltaSeconds = 0.0f;
	float mouseDeltaX = 0.0f;
	float mouseDeltaY = 0.0f;
	uint64_t actionDownMask = 0;
	uint64_t actionPressedMask = 0;
	bool removePressed = false;
	bool placePressed = false;
};

struct InputReplayCapture {
	std::filesystem::path snapshotPath;
	CameraState initialCamera{};
	InteractionState initialInteraction{};
	WalkAirControlMode walkAirControlMode = WalkAirControlMode::MinecraftLike;
	bool walkAutoJumpEnabled = false;
	bool walkAutoJumpDelayEnabled = false; // file-format field; always false (delay removed)
	std::vector<InputReplayFrame> frames;
};

struct InputReplayState {
	InputReplayCapture capture{};
	std::filesystem::path replayPath;
	bool recording = false;
	bool playbackRequested = false;
	bool playbackActive = false;
	bool captureAvailable = false;
	size_t playbackFrameIndex = 0;
};

struct InputState {
	float mouseDeltaX = 0.0f;
	float mouseDeltaY = 0.0f;
	bool removePressed = false;
	bool placePressed = false;
	bool relativeMouseModeEnabled = true;

	bool skipFirstMouseMotion = true;
	int mouseMotionFreezeCount = 0;
	Uint64 lastMoveUpPressedTimestampNs = 0;
	std::array<InputActionButtonState, kInputActionCount> actions{};
	std::array<InputActionBinding, kInputActionCount> bindings{};
	InputReplayState replay{};
};

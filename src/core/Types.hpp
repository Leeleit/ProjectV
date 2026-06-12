#ifndef TYPES_HPP
#define TYPES_HPP

// `volk.h` must come before any header that pulls in `vk_mem_alloc.h`
// (transitively: `asset/MeshGpuResources.hpp`, `render/ShadowTypes.hpp`,
// `render/TaaRenderTargets.hpp`, `voxel/VoxelMaterials.hpp` all carry
// VMA-related types). VMA's volk-aware import helper
// (`vmaImportVulkanFunctionsFromVolk`) is declared only when
// `VOLK_HEADER_VERSION` is defined at the time VMA's header is processed;
// putting `volk.h` first here keeps the helper visible project-wide.
// ReSharper disable once CppUnusedIncludeDirective
#include "volk.h"

#include "SDL3/SDL.h"
#include "asset/MeshGpuResources.hpp"
#include "render/ShadowTypes.hpp"
#include "render/TaaRenderTargets.hpp"
#include "voxel/VoxelMaterials.hpp"

namespace projectv::taa {
struct OffscreenColorTarget;
} // namespace projectv::taa
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "vk_mem_alloc.h"
#pragma clang diagnostic pop

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <string>
#include <type_traits>
#include <vector>

constexpr int MAX_FRAMES_IN_FLIGHT = 2;
constexpr uint32_t MAX_LOOK_DEV_CAPTURE_VIEW_COUNT = 8;

struct VoxelWorld;
struct EcsState;
struct PhysicsState;
namespace projectv::audio {
class AudioEngine;
} // namespace projectv::audio
void DestroyEcsState(EcsState *ecs);
void DestroyPhysicsState(PhysicsState *physics);
void DestroyAudioEngine(projectv::audio::AudioEngine *engine);
using EcsStatePtr = std::unique_ptr<EcsState, void (*)(EcsState *)>;
using PhysicsStatePtr = std::unique_ptr<PhysicsState, void (*)(PhysicsState *)>;
using AudioEnginePtr = std::unique_ptr<projectv::audio::AudioEngine,
	void (*)(projectv::audio::AudioEngine *)>;

struct PackedSceneVoxelFace {
	// A1 (4.1 greedy meshing): this struct now covers both 1×1 voxels and
	// greedy W×H merged quads. `localVoxelFace` still packs the START voxel
	// coord + face index (same as before — the minimum-corner anchor);
	// `packedExtents` carries the in-plane (width, height) in 8 bits each so
	// the vertex shader can stretch the 4-corner unit offset to the merged
	// quad size. Lighting stays on `lightingData` as a no-op for now
	// (per-vertex AO disabled per `decisions.md §14` v2, AO no longer blocks
	// greedy merge).
	uint32_t localVoxelFace = 0;
	uint32_t chunkIndexMaterial = 0;
	uint32_t lightingData = 0;
	uint32_t packedExtents = 0;
};
static_assert(std::is_standard_layout_v<PackedSceneVoxelFace>);
static_assert(std::is_trivially_copyable_v<PackedSceneVoxelFace>);
static_assert(sizeof(PackedSceneVoxelFace) == 16);
static_assert(offsetof(PackedSceneVoxelFace, localVoxelFace) == 0);
static_assert(offsetof(PackedSceneVoxelFace, chunkIndexMaterial) == 4);
static_assert(offsetof(PackedSceneVoxelFace, lightingData) == 8);
static_assert(offsetof(PackedSceneVoxelFace, packedExtents) == 12);

struct PackedSceneChunkDescriptor {
	std::array<int32_t, 4> chunkOrigin{};
	std::array<uint32_t, 4> chunkExtentAndNonAir{};
	std::array<uint32_t, 4> voxelDataInfo{};
	std::array<uint32_t, 4> drawRanges{};
};
static_assert(std::is_standard_layout_v<PackedSceneChunkDescriptor>);
static_assert(std::is_trivially_copyable_v<PackedSceneChunkDescriptor>);
static_assert(sizeof(PackedSceneChunkDescriptor) == 64);
static_assert(offsetof(PackedSceneChunkDescriptor, chunkOrigin) == 0);
static_assert(offsetof(PackedSceneChunkDescriptor, chunkExtentAndNonAir) == 16);
static_assert(offsetof(PackedSceneChunkDescriptor, voxelDataInfo) == 32);
static_assert(offsetof(PackedSceneChunkDescriptor, drawRanges) == 48);

struct SceneChunkVoxelPayloadRange {
	uint32_t wordOffset = 0;
	uint32_t voxelCount = 0;
	uint32_t wordCount = 0;
	uint32_t reserved = 0;
};
static_assert(std::is_standard_layout_v<SceneChunkVoxelPayloadRange>);
static_assert(std::is_trivially_copyable_v<SceneChunkVoxelPayloadRange>);
static_assert(sizeof(SceneChunkVoxelPayloadRange) == 16);
static_assert(offsetof(SceneChunkVoxelPayloadRange, wordOffset) == 0);
static_assert(offsetof(SceneChunkVoxelPayloadRange, voxelCount) == 4);
static_assert(offsetof(SceneChunkVoxelPayloadRange, wordCount) == 8);

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
	ToggleWalkAutoJumpDelay,
	DecreaseLightingExposure,
	IncreaseLightingExposure,
	CycleToneMapOperator,
	CycleLightingDebugView,
	ResetLightingDebugControls,
	CycleShadowTuningTarget,
	DecreaseShadowTuningValue,
	IncreaseShadowTuningValue,
	ToggleInputReplayRecording,
	PlayLastInputReplay,
	ToggleMutationAnchor,
	PickTargetMaterial,
	CaptureScreenshot,
	ToggleTaa,
	DecreaseTaaJitterScale,
	IncreaseTaaJitterScale,
	DecreaseTaaBlend,
	IncreaseTaaBlend,
	CycleTaaNeighbourhoodRadius,
	InvalidateTaaHistory,
	// **M5.1d debug tool, 2026-06-12:** Half-Life 2 / Garry's Mod
	// style physics-gun for interactively positioning loaded
	// models. Hold F to "pick" the closest model in front of
	// the camera; the picked model then snaps to integer voxel
	// coords under the crosshair on a horizontal plane (default
	// Y=1, the top of the VoxelLab floor voxel). Release F to
	// drop. Prints "PICKED: <id> aabbMin=(...)" on grab and
	// "DROPPED: <id> final_aabbMin=(...)" on release, so the
	// operator can read the integer voxel coordinates out of
	// the smoke log and hard-code the manifest position
	// (or verify the snap math). See `app/ModelGravigun.hpp`
	// for the contract.
	PickModel,
	// **5.2 Debug gizmos, 2026-06-12:** `L` cycles a world-aligned
	// "split plane" overlay at each of the four `viewDepthSplits`
	// distances along the camera forward vector. Useful when tuning
	// cascade split lambda / split distribution by eye against a
	// static scene. The boxes are intentionally axis-aligned
	// (`DebugOverlayBox` is `Int3 min/maxExclusive`) so they read
	// as a coarse "where does each cascade start" cue rather
	// than a precise world frustum — see
	// `debug/DebugOverlays.cpp::AppendCascadeSplitPlaneOverlayBoxes`
	// for the math.
	ToggleCascadeSplitPlanes,
	// **5.2 Debug gizmos, 2026-06-12:** `Z` toggles a 1-voxel-wide
	// shaft along the cursor hit normal (`selection.hitNormal`,
	// ±1 in one axis) for 2 voxels. Helps visualise face selection
	// at extreme angles (top-down, side-on) where the yellow
	// selection box alone is ambiguous.
	ToggleCursorHitNormal,
	// **Frame-step / slow-motion debug, 2026-06-12:** live time-scale
	// control for visual debugging. 4 hotkeys: `[` halves the current
	// `SimulationState::timeScale` (snapping to 0 below 0.01 for a
	// discrete "pause" stop), `]` doubles it (clamped to 4.0 max),
	// `\` queues exactly one fixed-step physics tick regardless of the
	// current pause state (consumed in `UpdateApp` after the input
	// action block), and `` ` `` resets `timeScale` to 1.0. These are
	// independent of the existing `TogglePause` (`P`) action — pause
	// and slow-motion are distinct runtime debug paths so the operator
	// can keep the time-scale axis at, e.g., 0.25x while still being
	// free to single-step one frame for screenshot alignment. See
	// `agent/decisions.md §26` for the per-field contract.
	DecreaseTimeScale,
	IncreaseTimeScale,
	StepSingleFrame,
	ResetTimeScale,
	// **Audio engine, 2026-06-12.** Music player
	// controls. v1 layout uses free letters/digits
	// per the operator's request ("надо
	// переназначить все кнопки ... но это потом.
	// Сейчас назначай там, где свободно"); the full
	// hotkey rebind is a follow-up slice. See
	// `decisions.md §28` and `InputActions.cpp` for
	// the current bindings (Q / E / 7 / 8). The
	// `MusicState` enum (Stopped / Playing / Paused)
	// lives in `src/audio/AudioEngine.hpp`.
	ToggleMusicPlayPause,
	StopMusic,
	MusicVolumeDown,
	MusicVolumeUp,
	// **Track switching, 2026-06-12.** Next /
	// previous track through the 5-sec-refresh
	// playlist. Hotkeys `9` (next) and `0`
	// (previous) are the only adjacent free
	// digits in the existing `InputAction` table
	// (1-6 are also free, but 9/0 are the
	// conventional "skip ahead / rewind" pair in
	// media-player UX). v1 layout is placeholder;
	// the full hotkey rebind (per the operator's
	// note in the audio-engine session) remains
	// the follow-up slice. `nextTrack()` and
	// `previousTrack()` wrap around the playlist
	// (next from last → 0; prev from 0 → last);
	// empty playlist = no-op.
	NextMusicTrack,
	PreviousMusicTrack,
	Count,
};

constexpr size_t kInputActionCount = static_cast<size_t>(InputAction::Count);
constexpr size_t kInputActionBindingSlotCount = 2;

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
	std::array<float, 3> position{0.0f, 8.0f, 24.0f};
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

struct GraphicsPushConstants {
	std::array<float, 16> viewProjection{};
	std::array<float, 4> cameraPosition{};
	std::array<float, 4> cameraForward{};
	std::array<int32_t, 4> worldMinAndChunkSize{};
	std::array<uint32_t, 4> chunkGridAndFlags{};
};
static_assert(std::is_standard_layout_v<GraphicsPushConstants>);
static_assert(std::is_trivially_copyable_v<GraphicsPushConstants>);
static_assert(sizeof(GraphicsPushConstants) == 128);
static_assert(offsetof(GraphicsPushConstants, viewProjection) == 0);
static_assert(offsetof(GraphicsPushConstants, cameraPosition) == 64);
static_assert(offsetof(GraphicsPushConstants, cameraForward) == 80);
static_assert(offsetof(GraphicsPushConstants, worldMinAndChunkSize) == 96);
static_assert(offsetof(GraphicsPushConstants, chunkGridAndFlags) == 112);

struct ShadowPushConstants {
	uint32_t cascadeIndex = 0;
};
static_assert(std::is_standard_layout_v<ShadowPushConstants>);
static_assert(std::is_trivially_copyable_v<ShadowPushConstants>);
static_assert(sizeof(ShadowPushConstants) == 4);

struct ResolvePushConstants {
	// Matches the `ResolvePushConstants` struct declared in
	// `src/shaders/taa_resolve.frag` (lines 38-43). Byte layout is enforced
	// with `static_assert` so a future re-ordering of either side fails to
	// compile. The TAA resolve pass is a fullscreen triangle that needs the
	// inverse of the current viewProjection (for depth-to-world
	// reprojection), the current viewProjection (uploaded alongside for
	// potential future motion-vector work), the inverse render extent in
	// pixels (to convert `gl_FragCoord` to UV), and the CAS sharpening
	// inputs at the end. Total: 144 B. The trailing `taaBlend` /
	// `taaCasSharpnessMax` pair (1.3) replaced the original
	// `vec2 reservedPadding` slot with the same 8 B total; byte layout
	// is unchanged so existing SPIR-V / push-constant expectations stay
	// intact.
	std::array<float, 16> inverseCurrentViewProjection{};
	std::array<float, 16> currentViewProjection{};
	std::array<float, 2> renderExtentInverse{};
	float taaBlend = 0.0f;
	float taaCasSharpnessMax = 0.0f;
};
static_assert(std::is_standard_layout_v<ResolvePushConstants>);
static_assert(std::is_trivially_copyable_v<ResolvePushConstants>);
static_assert(sizeof(ResolvePushConstants) == 144);
static_assert(offsetof(ResolvePushConstants, inverseCurrentViewProjection) == 0);
static_assert(offsetof(ResolvePushConstants, currentViewProjection) == 64);
static_assert(offsetof(ResolvePushConstants, renderExtentInverse) == 128);
static_assert(offsetof(ResolvePushConstants, taaBlend) == 136);
static_assert(offsetof(ResolvePushConstants, taaCasSharpnessMax) == 140);

struct DebugOverlayPushConstants {
	std::array<float, 16> viewProjection{};
	std::array<float, 4> overlayData0{};
	std::array<float, 4> overlayData1{};
	std::array<float, 4> overlayColor{};
};
static_assert(std::is_standard_layout_v<DebugOverlayPushConstants>);
static_assert(std::is_trivially_copyable_v<DebugOverlayPushConstants>);
static_assert(sizeof(DebugOverlayPushConstants) == 112);
static_assert(offsetof(DebugOverlayPushConstants, viewProjection) == 0);
static_assert(offsetof(DebugOverlayPushConstants, overlayData0) == 64);
static_assert(offsetof(DebugOverlayPushConstants, overlayData1) == 80);
static_assert(offsetof(DebugOverlayPushConstants, overlayColor) == 96);

struct DebugHudVertex {
	std::array<float, 2> positionNdc{};
	std::array<float, 4> color{};
};
static_assert(std::is_standard_layout_v<DebugHudVertex>);
static_assert(std::is_trivially_copyable_v<DebugHudVertex>);
static_assert(sizeof(DebugHudVertex) == 24);
static_assert(offsetof(DebugHudVertex, positionNdc) == 0);
static_assert(offsetof(DebugHudVertex, color) == 8);

constexpr uint32_t DEBUG_HUD_MAX_VERTEX_COUNT = 262144;

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
	uint32_t actionDownMask = 0;
	uint32_t actionPressedMask = 0;
	bool removePressed = false;
	bool placePressed = false;
};

struct InputReplayCapture {
	std::string snapshotPath;
	CameraState initialCamera{};
	InteractionState initialInteraction{};
	WalkAirControlMode walkAirControlMode = WalkAirControlMode::MinecraftLike;
	bool walkAutoJumpEnabled = false;
	bool walkAutoJumpDelayEnabled = true;
	std::vector<InputReplayFrame> frames;
};

struct InputReplayState {
	InputReplayCapture capture{};
	std::string replayPath{};
	bool recording = false;
	bool playbackRequested = false;
	bool playbackActive = false;
	bool captureAvailable = false;
	size_t playbackFrameIndex = 0;
};

struct DebugOverlayBox {
	Int3 min{};
	Int3 maxExclusive{};
	std::array<float, 4> color{};
};

struct VoxelMeshingPushConstants {
	std::array<int32_t, 4> worldMinAndChunkSize{};
	std::array<int32_t, 4> worldMaxExclusiveAndChunkCount{};
	std::array<uint32_t, 4> chunkGridAndTransparentFaceBase{};
	std::array<uint32_t, 4> faceCapacities{};
};

// **Two-level chunk visibility cache (2026-06-12).** When the
// CPU-side cull pass in `UpdateChunkVisibilityAndIndirectCommands`
// runs every frame, it re-iterates every chunk descriptor and
// re-evaluates the camera frustum + 4 shadow cascade clip volumes,
// even on a perfectly static camera + static world. That's
// measurable on a mainline look-dev scene (300+ chunks × 5
// visibility tests = 1500+ dot products / frame) and pure waste
// when nothing moved. This struct caches the resulting
// `VkDrawIndirectCommand` arrays + visible-chunk counts keyed
// on a hash of `(sceneVoxelPayloadVersion, chunkDescriptorCount,
// quantized camera position, quantized camera forward)`. On
// cache hit, the per-chunk loop is skipped entirely and the
// cached command buffers are `memcpy`'d straight into the
// per-frame mapped GPU indirect buffers. The thresholds are
// chosen so a static-camera replay / capture run keeps the
// entire cull pass off the per-frame critical path.
struct ChunkVisibilityCache {
	bool valid = false;
	uint64_t sceneVoxelPayloadVersion = 0;
	uint32_t chunkDescriptorCount = 0;
	// Quantized camera state: position to 0.25 voxel (so
	// 1-voxel camera moves always invalidate), forward to
	// 0.005 (~0.3° steps) so sub-1° rotations also
	// invalidate. Forward is stored as fixed-point
	// (forward * 1000, clamped + rounded) to keep the
	// cache key as a small integer tuple.
	int32_t quantizedCameraX = 0;
	int32_t quantizedCameraY = 0;
	int32_t quantizedCameraZ = 0;
	int32_t quantizedForwardX1000 = 0;
	int32_t quantizedForwardY1000 = 0;
	int32_t quantizedForwardZ1000 = 0;
	uint64_t hash = 0;
	uint32_t visibleChunkCount = 0;
	std::array<uint32_t, kSunShadowCascadeCount> shadowCascadeVisibleChunkCounts{};
	std::vector<VkDrawIndirectCommand> opaqueCommands;
	std::vector<VkDrawIndirectCommand> shadowCommands;
	std::vector<VkDrawIndirectCommand> transparentCommands;
	// Cached values for the chunk-culling profiler plots. Mirrors
	// the "Visible Chunks" / "Culled Chunks" values written by
	// `UpdateSceneFrameChunkVisibility` so the plot stays
	// populated even on cache-hit frames.
	uint32_t culledChunkCount = 0;
	// 0 = miss (rebuild), 1+ = Nth consecutive hit. Useful
	// when correlating profiler traces with cache behaviour.
	uint64_t consecutiveHitCount = 0;
};
static_assert(std::is_standard_layout_v<VoxelMeshingPushConstants>);
static_assert(std::is_trivially_copyable_v<VoxelMeshingPushConstants>);
static_assert(sizeof(VoxelMeshingPushConstants) == 64);
static_assert(offsetof(VoxelMeshingPushConstants, worldMinAndChunkSize) == 0);
static_assert(offsetof(VoxelMeshingPushConstants, worldMaxExclusiveAndChunkCount) == 16);
static_assert(offsetof(VoxelMeshingPushConstants, chunkGridAndTransparentFaceBase) == 32);
static_assert(offsetof(VoxelMeshingPushConstants, faceCapacities) == 48);

struct ChunkCullingParameters {
	std::array<float, 4> cameraPositionAndMaxDistance{};
	std::array<float, 4> cameraForwardAndTanHalfVerticalFov{};
	std::array<float, 4> cameraRightAndTanHalfHorizontalFov{};
	std::array<float, 4> cameraUpAndNearPlane{};
};
static_assert(std::is_standard_layout_v<ChunkCullingParameters>);
static_assert(std::is_trivially_copyable_v<ChunkCullingParameters>);
static_assert(sizeof(ChunkCullingParameters) == 64);
static_assert(offsetof(ChunkCullingParameters, cameraPositionAndMaxDistance) == 0);
static_assert(offsetof(ChunkCullingParameters, cameraForwardAndTanHalfVerticalFov) == 16);
static_assert(offsetof(ChunkCullingParameters, cameraRightAndTanHalfHorizontalFov) == 32);
static_assert(offsetof(ChunkCullingParameters, cameraUpAndNearPlane) == 48);

struct FrameRenderData {
	uint32_t frameIndex = 0;
	VkBuffer packedFaceBuffer = VK_NULL_HANDLE;
	VkBuffer chunkDescriptorBuffer = VK_NULL_HANDLE;
	VkBuffer chunkVoxelPayloadBuffer = VK_NULL_HANDLE;
	VkBuffer debugHudVertexBuffer = VK_NULL_HANDLE;
	VkDescriptorSet graphicsDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSet shadowDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSet voxelMeshingDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSet taaResolveDescriptorSet = VK_NULL_HANDLE;
	VkBuffer opaqueIndirectBuffer = VK_NULL_HANDLE;
	VkBuffer shadowIndirectBuffer = VK_NULL_HANDLE;
	VkBuffer transparentIndirectBuffer = VK_NULL_HANDLE;
	uint32_t chunkDescriptorCount = 0;
	uint32_t shadowIndirectCommandCount = 0;
	std::array<uint32_t, kSunShadowCascadeCount> shadowCascadeVisibleChunkCounts{};
	uint32_t dirtyChunkCount = 0;
	uint32_t opaqueFaceCount = 0;
	uint32_t transparentFaceCount = 0;
	uint32_t debugHudVertexCount = 0;
	bool debugUiVisible = true;
	GraphicsPushConstants graphicsPushConstants{};
	VoxelMeshingPushConstants voxelMeshingPushConstants{};
	InteractionSelectionState interactionSelection{};
	std::vector<DebugOverlayBox> debugOverlayBoxes;
};

struct DebugStats {
	float framesPerSecond = 0.0f;
	float frameTimeMilliseconds = 0.0f;
	uint32_t simulationStepsLastFrame = 0;
	uint32_t dirtyChunkCount = 0;
	uint32_t activeChunkCount = 0;
	uint32_t nonAirVoxelCount = 0;
	uint32_t glassVoxelCount = 0;
	uint32_t fluidVoxelCount = 0;
	uint32_t floorVoxelCount = 0;
	uint32_t sceneTriangleCount = 0;
	uint64_t sceneMemoryBytes = 0;
	uint64_t worldEditVersion = 0;
	VoxelScenePreset scenePreset = VoxelScenePreset::VoxelLab;
	CameraState::ControlMode controlMode = CameraState::ControlMode::Creative;
	WalkAirControlMode walkAirControlMode = WalkAirControlMode::MinecraftLike;
	bool detailedHudVisible = false;
	bool walkAutoJumpEnabled = false;
	bool walkAutoJumpDelayEnabled = true;
	bool simulationPaused = false;
	// **Frame-step / slow-motion mirrors, 2026-06-12.** See
	// `SimulationState::timeScale` and `::frameStepRequested`
	// for the per-field contract. The mirror exists so the HUD
	// and the runtime capture sidecar (added by the same
	// change) can read the current time-scale without poking
	// into `SimulationState` directly. `simulationFrameStepPending`
	// is set true on the same frame the user pressed
	// `StepSingleFrame` and is cleared at the top of `UpdateApp`,
	// so it is a one-frame indicator for "the next tick is a
	// forced step" — not a sticky latched flag.
	float simulationTimeScale = 1.0f;
	bool simulationFrameStepPending = false;
	// Per-pass CPU timing mirrors (2026-06-12). Source fields
	// live in `RenderState::renderPassTimings`; the mirror
	// exists so the HUD and runtime capture sidecar can read
	// the per-pass breakdown without poking into
	// `RenderState` directly. `renderPassOtherMs` is derived
	// in `AppUpdate.cpp` as
	// `frameTimeMs - sum(measured passes)` so the dashboard
	// shows what the unaccounted-for slice of the frame is.
	float renderPassShadowMs = 0.0f;
	float renderPassMeshingMs = 0.0f;
	float renderPassGraphicsMs = 0.0f;
	float renderPassTaaResolveMs = 0.0f;
	float renderPassDebugOverlayMs = 0.0f;
	float renderPassDebugHudMs = 0.0f;
	float renderPassOtherMs = 0.0f;
	uint32_t renderPassDirtyChunkRebuiltCount = 0;
	// **Audio engine mirrors, 2026-06-12.** Source of
	// truth lives in `AudioEngine` (singleton on
	// `AppState`); these mirrors feed the HUD line
	// and the capture sidecar without poking into
	// `AppState` directly. `audioMusicState` is
	// `uint8_t` (the underlying `MusicState` is
	// 1-byte too) for ABI stability across the
	// `DebugStats` mirror boundary. The string
	// mirrors are populated on every frame the
	// playlist rescan or current-index change
	// (see `AudioEngine::scanPlaylist`).
	uint8_t audioMusicState = 0; // 0=Stopped, 1=Playing, 2=Paused (matches MusicState enum)
	float audioMusicVolume = 0.8f;
	bool audioMusicInitialized = false;
	uint32_t audioMusicPlaylistSize = 0;
	uint32_t audioMusicCurrentIndex = 0;
	std::array<char, 128> audioMusicTrackName{};
	// **Music HUD mirrors, 2026-06-13.** Parsed
	// from `audioMusicTrackName` on the C++ side
	// (see `audio::ParseArtistTitle`); the engine
	// re-parses only when the underlying track
	// changes, so the mirror update in
	// `AppUpdate` is a cheap per-frame copy.
	// `audioMusicArtist` is `"-"` (em-dash) when
	// the filename has no ` - ` separator;
	// `audioMusicTitle` is the stem with `.mp3`
	// stripped. Both are empty when the playlist
	// is empty. Position / duration are in
	// seconds; 0.0f means "not loaded" (position)
	// or "decoder did not expose length"
	// (duration). 96 / 128 are the same char
	// budgets as the existing track name —
	// the artist line and title line fit
	// comfortably in the 96-char HUD buffer
	// (`kHudLineBufferSize`) because each has
	// a 6-7 char label prefix.
	std::array<char, 96> audioMusicArtist{};
	std::array<char, 128> audioMusicTitle{};
	float audioMusicPositionSec = 0.0f;
	float audioMusicDurationSec = 0.0f;
	bool showChunkBounds = false;
	bool showDirtyChunkOverlay = false;
	bool walkDebugValid = false;
	uint8_t walkSupportState = 0;
	std::array<float, 3> walkFeetPosition{};
	float walkFootSupportScore = 0.0f;
	uint32_t walkFootSupportHitSamples = 0;
	uint32_t walkFootSupportTotalSamples = 0;
	uint32_t walkEdgeGraceFramesRemaining = 0;
	uint32_t walkGroundTakeoffGraceFramesRemaining = 0;
	uint32_t walkSneakSupportGraceFramesRemaining = 0;
	uint32_t walkLedgeReleaseGraceFramesRemaining = 0;
	uint32_t walkAutoJumpDelayFramesRemaining = 0;
	bool walkGroundTakeoffCached = false;
	bool walkSneakActive = false;
	bool walkJumpLockActive = false;
	bool walkSuppressPassiveSlide = false;
	float sceneExposure = 1.0f;
	float sceneEnvironmentIntensity = 1.0f;
	float sceneColorGradeWhitePoint = 1.0f;
	float sceneColorGradeContrast = 1.0f;
	float sceneColorGradeSaturation = 1.0f;
	float sceneColorGradeLift = 0.0f;
	ExposureMeteringMode sceneExposureMeteringMode = ExposureMeteringMode::SceneKey;
	float sceneExposureKey = 1.0f;
	float sceneExposureTargetKey = 1.0f;
	float sceneMinExposure = 0.05f;
	float sceneMaxExposure = 4.0f;
	ToneMapOperator toneMapOperator = ToneMapOperator::AcesApprox;
	LightingDebugView lightingDebugView = LightingDebugView::Final;
	// TAA runtime state mirrored in the public debug HUD. `taaEnabled` is
	// the master gate; the active blend factor, current frame counter and
	// history-valid flag are also queryable here for HUD lines and the
	// scripted capture sidecar. The per-frame jitter values
	// (`taaJitterX/Y`) and the previous-frame view-projection matrix live
	// only in `RenderState` because they feed the push-constant uploads
	// and the scene-lighting buffer, not the debug HUD.
	bool taaEnabled = false;
	float taaBlend = 0.10f;
	uint32_t taaFrameCounter = 0u;
	bool taaHistoryValid = false;
	float taaJitterX = 0.0f;
	float taaJitterY = 0.0f;
	float taaJitterScale = 1.0f;
	int32_t taaNeighbourhoodRadius = 1;
	// CAS (1.3) ceiling mirror; see `RenderState` for the contract.
	float taaCasSharpnessMax = 0.5f;
	// 1.5 — layer history validity mirror. See `RenderState` for the
	// contract; the HUD exposes a single boolean.
	bool taaLayerHistoryValid = false;
	// 1.5 — layer blend factor mirror; see `RenderState` for the
	// contract. Not currently surfaced in the HUD (the value is
	// authored at startup and only changed via env var), but
	// exposed through this mirror so sidecar captures can verify
	// it.
	float taaLayerBlendFactor = 0.4f;
	// Camera-cut detection mirror (1.2). Same source fields as in
	// `RenderState`; see that block for the contract.
	uint32_t taaCameraCutCount = 0;
	float taaCameraCutMaxDelta = 0.0f;
	std::array<float, 3> sunDirection{};
	float sunIntensity = 0.0f;
	float sunShadowStrength = 0.0f;
	float sunShadowDepthBias = 0.0f;
	float sunShadowNormalBias = 0.0f;
	float sunShadowFilterRadius = 0.0f;
	float sunShadowCoverageScale = 1.0f;
	float sunShadowCascadeBlend = 0.0f;
	float sunShadowCascadeSplitLambda = 0.80f;
	std::array<float, kSunShadowCascadeCount> sunShadowCascadeDepthSplits{};
	SunShadowCascadeDiagnostics sunShadowCascadeDiagnostics{};
	uint32_t shadowMapResolution = 0;
	TransparentShadowPolicy transparentShadowPolicy = TransparentShadowPolicy::GlassIgnoredFluidCasts;
	ShadowTuningTarget shadowTuningTarget = ShadowTuningTarget::Strength;
	float sunContactShadowStrength = 0.0f;
	float sunContactShadowDistance = 0.0f;
	float ambientOcclusionStrength = 0.0f;
	float ambientOcclusionRadius = 0.0f;
	float ambientOcclusionMinVisibility = 0.0f;
	std::array<float, 3> localPointLightPosition{};
	std::array<float, 3> localPointLightColor{};
	float localPointLightRadius = 0.0f;
	float localPointLightIntensity = 0.0f;
	bool localPointLightEnabled = false;
	float localPointLightSourceRadius = 0.0f;
	float localPointLightShadowStrength = 0.0f;
	float localPointLightShadowBias = 0.0f;
	bool inputReplayRecording = false;
	bool inputReplayPlaybackActive = false;
	bool inputReplayReady = false;
	uint32_t inputReplayFrameCount = 0;
	uint32_t inputReplayPlaybackFrameIndex = 0;
};

struct SceneFrameResources {
	void *packedFaceMappedData = nullptr;
	VkBuffer packedFaceBuffer = VK_NULL_HANDLE;
	VmaAllocation packedFaceAllocation = VK_NULL_HANDLE;
	void *debugHudVertexMappedData = nullptr;
	VkBuffer debugHudVertexBuffer = VK_NULL_HANDLE;
	VmaAllocation debugHudVertexAllocation = VK_NULL_HANDLE;
	void *chunkDescriptorMappedData = nullptr;
	VkBuffer chunkDescriptorBuffer = VK_NULL_HANDLE;
	VmaAllocation chunkDescriptorAllocation = VK_NULL_HANDLE;
	void *chunkVoxelPayloadMappedData = nullptr;
	VkBuffer chunkVoxelPayloadBuffer = VK_NULL_HANDLE;
	VmaAllocation chunkVoxelPayloadAllocation = VK_NULL_HANDLE;
	void *opaqueIndirectMappedData = nullptr;
	VkBuffer opaqueIndirectBuffer = VK_NULL_HANDLE;
	VmaAllocation opaqueIndirectAllocation = VK_NULL_HANDLE;
	void *shadowIndirectMappedData = nullptr;
	VkBuffer shadowIndirectBuffer = VK_NULL_HANDLE;
	VmaAllocation shadowIndirectAllocation = VK_NULL_HANDLE;
	void *transparentIndirectMappedData = nullptr;
	VkBuffer transparentIndirectBuffer = VK_NULL_HANDLE;
	VmaAllocation transparentIndirectAllocation = VK_NULL_HANDLE;
	void *dirtyChunkIndexMappedData = nullptr;
	VkBuffer dirtyChunkIndexBuffer = VK_NULL_HANDLE;
	VmaAllocation dirtyChunkIndexAllocation = VK_NULL_HANDLE;
	void *chunkCullingMappedData = nullptr;
	VkBuffer chunkCullingBuffer = VK_NULL_HANDLE;
	VmaAllocation chunkCullingAllocation = VK_NULL_HANDLE;
	void *sceneLightingMappedData = nullptr;
	VkBuffer sceneLightingBuffer = VK_NULL_HANDLE;
	VmaAllocation sceneLightingAllocation = VK_NULL_HANDLE;
	VkDescriptorSet graphicsDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSet shadowDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSet voxelMeshingDescriptorSet = VK_NULL_HANDLE;
	uint64_t uploadedSceneVersion = 0;
	uint64_t uploadedVoxelPayloadVersion = 0;
	uint64_t meshedSceneVersion = 0;
	uint32_t chunkDescriptorCount = 0;
	uint32_t shadowIndirectCommandCount = 0;
	std::array<uint32_t, kSunShadowCascadeCount> shadowCascadeVisibleChunkCounts{};
	uint32_t dirtyChunkCount = 0;
	uint32_t opaqueFaceCount = 0;
	uint32_t transparentFaceCount = 0;
	uint32_t debugHudVertexCount = 0;
};

struct WorldState {
	std::unique_ptr<VoxelWorld> voxelWorld;
	bool scenePresetReloadRequested = false;
	VoxelScenePreset requestedScenePreset = VoxelScenePreset::VoxelLab;
	bool snapshotSaveRequested = false;
	bool snapshotLoadRequested = false;
};

struct ModelInstanceData {
	std::array<float, 16> modelTransform{};
	std::array<float, 3> worldAabbMin{};
	std::array<float, 3> worldAabbMax{};
	// Source (asset-local) AABB min in model-local space. The
	// load path's `aabbMinOffset = T(-srcMin*scale)` shifts the
	// model basis so the translation column of `modelTransform`
	// ends up at `pos - srcAabbMin` (NOT `pos`); the renderer
	// then adds `srcAabbMin` back when transforming each vertex.
	// Snap / drag paths that mutate the AABB min must also update
	// `modelTransform[12..14]` to `newMin - sourceAabbMin` — see
	// `ModelManifestLoader.cpp` and `ModelGravigun.cpp`. The
	// field is captured at load time from
	// `ModelRegistryEntry::aabbMin` and never changes after that.
	std::array<float, 3> sourceAabbMin{};
	VkBuffer vertexBuffer = VK_NULL_HANDLE;
	VkBuffer indexBuffer = VK_NULL_HANDLE;
	uint32_t indexCount = 0;
};

struct ModelRegistryEntry {
	std::string id;
	projectv::asset::MeshGpuResources gpu;
	std::array<float, 3> aabbMin{0.0f};
	std::array<float, 3> aabbMax{0.0f};
};

// **Per-pass CPU timing, 2026-06-12.** Wall-clock
// microsecond-precision time spent inside each
// `Record*Commands` function in `Renderer.cpp`, plus the
// inlined TAA resolve block. Measured with
// `SDL_GetPerformanceCounter` (same primitive as
// `ComputeFrameDeltaSeconds` in `AppUpdate.cpp`). All 6
// fields are CPU-side; for GPU-accurate numbers (e.g. shadow
// GPU time vs shadow CPU time on different drivers) the
// follow-up would be `vkCmdWriteTimestamp` queries on each
// pass, but for the "where is my frame budget going" use
// case CPU is sufficient and avoids the per-frame query
// pool reset overhead. `*Ms` fields default to `0.0f` so
// the HUD line never shows garbage on the first frame.
// `dirtyChunkRebuiltCount` mirrors `FrameRenderData::dirtyChunkCount`
// at the time of `RecordVoxelMeshingCommands` — that count
// is already tracked; this struct just snapshots it for
// the HUD/sidecar.
struct RenderPassTimings {
	float shadowMs = 0.0f;
	float meshingMs = 0.0f;
	float graphicsMs = 0.0f;
	float taaResolveMs = 0.0f;
	float debugOverlayMs = 0.0f;
	float debugHudMs = 0.0f;
	// Derived: `frameTimeMs - (shadow + meshing + graphics +
	// taaResolve + debugOverlay + debugHud)`. Computed at
	// the stats-mirror site in `AppUpdate.cpp` so the
	// caller can keep it consistent with whatever total
	// the rest of the dashboard reads.
	float otherMs = 0.0f;
	uint32_t dirtyChunkRebuiltCount = 0;
};

struct RenderState {
	std::vector<PackedSceneChunkDescriptor> sceneChunkDescriptors;
	std::vector<SceneChunkVoxelPayloadRange> sceneChunkVoxelPayloadRanges;
	std::vector<uint32_t> sceneChunkVoxelPayloadWords;
	std::vector<size_t> latestVoxelPayloadChunkIndices;
	std::vector<size_t> pendingChunkRebuildIndices;
	std::vector<size_t> completedChunkRebuildIndices;
	uint32_t sceneFaceCapacity = 0;
	uint32_t sceneTransparentFaceBase = 0;
	uint32_t sceneOpaqueFaceCount = 0;
	uint32_t sceneTransparentFaceCount = 0;
	uint32_t sceneChunkVoxelPayloadWordCount = 0;
	uint64_t sceneUploadVersion = 0;
	uint64_t sceneVoxelPayloadVersion = 0;
	uint32_t sceneTriangleCount = 0;
	uint64_t sceneMemoryBytes = 0;
	void *tracyGraphicsContext = nullptr;
	bool tracyGraphicsContextCalibrated = false;
	VoxelLightingDebugControls lightingDebugControls{};
	VoxelSceneLighting currentSceneLighting{};
	SunShadowCascadeSplits currentSunShadowCascadeSplits{};
	SunShadowCascadeDiagnostics currentSunShadowCascadeDiagnostics{};
	float sunShadowCascadeSplitLambda = 0.80f;
	TransparentShadowPolicy transparentShadowPolicy = TransparentShadowPolicy::GlassIgnoredFluidCasts;
	VoxelScenePreset currentScenePreset = VoxelScenePreset::VoxelLab;
	bool screenshotCaptureRequested = false;
	bool screenshotCaptureSupported = false;
	uint64_t screenshotCaptureSequence = 0;
	void *screenshotReadbackMappedData = nullptr;
	VkBuffer screenshotReadbackBuffer = VK_NULL_HANDLE;
	VmaAllocation screenshotReadbackAllocation = VK_NULL_HANDLE;
	uint64_t screenshotReadbackBufferSize = 0;
	void *materialVisualMappedData = nullptr;
	VkBuffer materialVisualBuffer = VK_NULL_HANDLE;
	VmaAllocation materialVisualAllocation = VK_NULL_HANDLE;
	VkDescriptorSetLayout graphicsDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool graphicsDescriptorPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout shadowDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool shadowDescriptorPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout voxelMeshingDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool voxelMeshingDescriptorPool = VK_NULL_HANDLE;
	std::array<SceneFrameResources, MAX_FRAMES_IN_FLIGHT> sceneFrameResources{};
	// Two-level chunk visibility cache. Lives on RenderState (not
	// SceneFrameResources) because the cached commands are
	// frame-independent — both `sceneFrameResources[0]` and
	// `[1]` get the same `memcpy`'d commands from this single
	// cache on a hit. The cache is invalidated by
	// `UpdateSceneFrameChunkVisibility` whenever the camera
	// moved past the quantization threshold or the world
	// version changed. See `ChunkVisibilityCache` for the
	// per-field contract.
	ChunkVisibilityCache chunkVisibilityCache{};
	VkImage depthImage = VK_NULL_HANDLE;
	VkImageView depthImageView = VK_NULL_HANDLE;
	VmaAllocation depthAllocation = VK_NULL_HANDLE;
	VkFormat shadowDepthFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D shadowMapExtent{2048u, 2048u};
	VkImage shadowImage = VK_NULL_HANDLE;
	VkImageView shadowImageView = VK_NULL_HANDLE;
	std::array<VkImageView, kSunShadowCascadeCount> shadowCascadeImageViews{};
	VmaAllocation shadowAllocation = VK_NULL_HANDLE;
	VkSampler shadowSampler = VK_NULL_HANDLE;
	bool depthImageNeedsInit = false;
	bool shadowImageNeedsInit = false;
	// Per-image layout trackers. The `*NeedsInit` flags above only
	// cover the very first transition (UNDEFINED → first layout);
	// once cleared, the renderer is responsible for keeping the
	// per-frame layout transitions consistent. The trackers below let
	// `Renderer.cpp::RecordGraphicsCommands` pick the right `oldLayout`
	// for each transition (depth lands in `DEPTH_ATTACHMENT_OPTIMAL` at
	// the end of a TAA-off frame and `DEPTH_READ_ONLY_OPTIMAL` at the
	// end of a TAA-on frame, so the next frame's start-of-frame
	// transition needs to know which one it is). Same story for the
	// TAA offscreen pair, which lands in `SHADER_READ_ONLY_OPTIMAL` at
	// the end of every frame so the next frame's resolve pass can sample
	// it without an extra barrier.
	VkImageLayout depthImageCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout taaSceneColorCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	// Vulkan spec VUID-VkImageCreateInfo-initialLayout-00993
	// requires `initialLayout` to be `UNDEFINED` /
	// `PREINITIALIZED` / `ZERO_INITIALIZED`, so both the
	// colour and layer history images are created with
	// `initialLayout = UNDEFINED` and the first-frame
	// per-frame transition in `Renderer.cpp` moves them to
	// `SHADER_READ_ONLY_OPTIMAL` for the resolve pass / the
	// voxel pass's binding-6 sample. The trackers here mirror
	// that initial state.
	VkImageLayout taaHistoryColorCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	// 1.5 anti-flicker: per-layer (CTSH/AOCC/LOCL) image
	// layout trackers. Same shape and lifecycle as the colour
	// history layout trackers above — both go through the
	// per-frame transition + copy block in `Renderer.cpp` and
	// are reset on swapchain recreate. The scene target is
	// born in `UNDEFINED` (the voxel pass writes to it via
	// `vkCmdBeginRendering`'s `imageLayout`); the history
	// target is also born in `UNDEFINED` for the same
	// Vulkan-spec reason as the colour history.
	VkImageLayout taaLayerSceneColorCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout taaLayerHistoryColorCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkPipelineLayout graphicsPipelineLayout = VK_NULL_HANDLE;
	VkPipelineLayout shadowPipelineLayout = VK_NULL_HANDLE;
	VkPipeline graphicsPipeline = VK_NULL_HANDLE;
	VkPipeline graphicsPipelineTaaOn = VK_NULL_HANDLE;
	VkPipeline transparentGraphicsPipeline = VK_NULL_HANDLE;
	VkPipeline transparentGraphicsPipelineTaaOn = VK_NULL_HANDLE;
	VkPipeline shadowGraphicsPipeline = VK_NULL_HANDLE;
	VkPipelineLayout debugOverlayPipelineLayout = VK_NULL_HANDLE;
	VkPipeline debugOverlayPipeline = VK_NULL_HANDLE;
	VkPipeline debugCrosshairPipeline = VK_NULL_HANDLE;
	VkPipelineLayout debugHudPipelineLayout = VK_NULL_HANDLE;
	VkPipeline debugHudPipeline = VK_NULL_HANDLE;
	VkPipelineLayout voxelMeshingPipelineLayout = VK_NULL_HANDLE;
	VkPipeline voxelMeshingPipeline = VK_NULL_HANDLE;
	// TAA (Temporal Anti-Aliasing) runtime state. `taaEnabled` is the master
	// gate; when false, the main pass writes straight to the swapchain and
	// no TAA resolve runs. `taaBlend` is the per-frame history blend factor
	// (lower = more ghosting on moving silhouettes, higher = less stable on
	// stationary detail). `taaFrameCounter` advances an 8-tap Halton(2,3)
	// sequence so the projection-matrix jitter spans the full sub-pixel
	// neighbourhood over eight frames before repeating. `taaHistoryValid`
	// drops to false for one frame after resize / world reload / preset
	// change / pause toggle / Taa toggle so the next resolve takes the
	// current scene as the only sample instead of the stale history.
	// `taaPrevViewProjectionMatrix` is the previous frame's viewProjection,
	// uploaded as the `prevViewProjectionMatrix` field of `VoxelSceneLighting`
	// and consumed by the TAA resolve pass for depth-based reprojection.
	// `taaJitterX/Y` are the current frame's NDC sub-pixel offsets and are
	// written into `VoxelSceneLighting.taaParams` for the same resolve pass.
	bool taaEnabled = true;
	float taaBlend = 0.10f;
	uint32_t taaFrameCounter = 0u;
	bool taaHistoryValid = false;
	bool taaSceneColorNeedsInit = true;
	bool taaHistoryNeedsInit = true;
	std::array<float, 16> taaPrevViewProjectionMatrix{};
	float taaJitterX = 0.0f;
	float taaJitterY = 0.0f;
	// Per-pass TAA tuning ladder (live `;`/`'`/`-`/`=`/`,`/`.` ladder, see
	// `InputAction::*Taa*`). `taaJitterScale` multiplies the Halton(2,3) output
	// before it lands in `taaParams.xy`; range [0, 2], step 0.25. Default 1.0
	// matches the pre-ladder behaviour. `taaNeighbourhoodRadius` drives the
	// 3x3 / 5x5 / 7x7 history clamp in `taa_resolve.frag`; allowed values are
	// 1 / 3 / 5 / 7 (the shader treats it as the per-axis loop bound), default
	// 1 keeps the original 3x3 clamp (`-1, 0, +1`).
	float taaJitterScale = 1.0f;
	int32_t taaNeighbourhoodRadius = 1;
	// CAS (Contrast Adaptive Sharpening) post-TAA (1.3). The shader
	// derives the effective sharpening amount as
	// `(1.0 - taaBlend) * taaCasSharpnessMax`, so this field is the
	// *ceiling* on top of the per-frame blend-driven attenuation;
	// high-blend frames (more history, already stable) get less
	// sharpening, low-blend frames (less history, more noise) get
	// more. 0.5 matches the AMD CAS reference for a default 0.10
	// TAA blend, i.e. `(1 - 0.10) * 0.5 = 0.45` effective sharpening
	// for the first-frame-after-invalidation sample. Range [0, 1];
	// 0 disables the CAS step entirely (`ApplyCasLinear` short-
	// circuits). The value is uploaded as part of `ResolvePushConstants`
	// and consumed by `taa_resolve.frag::ApplyCasLinear` (see
	// `decisions.md` §19).
	float taaCasSharpnessMax = 0.5f;
	// Camera-cut detection (1.2). The Chebyshev (max-abs-element) distance
	// between the previous and current frame's `viewProjection` is computed
	// each frame in `FramePreparation::BuildFrameData`; if it exceeds
	// `kTaaCameraCutThreshold` (0.10) the camera has moved far enough in
	// one frame that motion vectors can't sensibly reproject the history,
	// and `taaHistoryValid` is dropped the same way swapchain resize /
	// world reload / Taa toggle already do. `taaCameraCutCount` accumulates
	// across the session (resets only on world reload); `taaCameraCutMaxDelta`
	// records the worst Chebyshev distance seen since startup so the operator
	// can compare a live repro against `decisions.md` §19's expected delta
	// ranges. The first frame is skipped because the zero-initialised
	// `taaPrevViewProjectionMatrix` would always register as a "cut".
	uint32_t taaCameraCutCount = 0;
	float taaCameraCutMaxDelta = 0.0f;
	// 1.2 — companion flag to `taaPrevViewProjectionMatrix`. Set
	// after the first successful stash so the camera-cut detector
	// knows the previous-frame matrix is real (rather than the
	// zero-initialised default). Reset by `VulkanSwapchain.cpp`
	// on swapchain recreate, since that path also zeroes
	// `taaPrevViewProjectionMatrix`; without this flag the very
	// first frame after (re)init would always register as a
	// "cut" with `maxDelta` equal to the largest |viewProj| entry.
	bool taaPrevViewProjectionMatrixInitialized = false;
	// TAA offscreen render targets + linear sampler + resolve pipeline.
	// Allocated by `projectv::taa::CreateOrRecreateTaaRenderTargets` from
	// `VulkanSwapchain::CreateOrRecreateSwapchain` so the size stays in
	// lockstep with the swapchain. The TAA resolve pipeline reads from the
	// `historyColor` target while writing the resolved scene to the
	// swapchain image; the freshly-resolved frame is then
	// `vkCmdCopyImage`'d back into `historyColor` for the next frame.
	// The fields are heap-allocated pointers (rather than raw `VkImage`
	// handles) so `TaaRenderTargets.{hpp,cpp}` owns the full lifecycle
	// and `RenderState` just keeps a borrowed reference.
	// `projectv::taa::OffscreenColorTarget` is forward-declared at the
	// top of this header; the full definition is only visible in
	// `TaaRenderTargets.hpp` and the .cpp that actually performs the
	// allocation.
	projectv::taa::OffscreenColorTarget *taaSceneColorTarget = nullptr;
	projectv::taa::OffscreenColorTarget *taaHistoryColorTarget = nullptr;
	// 1.5 anti-flicker: per-layer (CTSH, AOCC, LOCL) temporal history
	// attachments. R8G8B8A8_UNORM (4 B/pixel) for the 3 layer values
	// in RGB + alpha=1.0. The voxel pass reads from
	// `taaLayerHistoryColorTarget` and writes the freshly-computed raw
	// values to `taaLayerSceneColorTarget`; the per-frame
	// `vkCmdCopyImage` in `Renderer.cpp` makes the current frame's
	// values the next frame's history input. See
	// `TaaRenderTargets.hpp::kTaaLayerHistoryColorFormat` for the
	// format choice rationale.
	projectv::taa::OffscreenColorTarget *taaLayerSceneColorTarget = nullptr;
	projectv::taa::OffscreenColorTarget *taaLayerHistoryColorTarget = nullptr;
	// 1.5 — companion init flag for the layer history. Mirrors the
	// 1.2 `taaPrevViewProjectionMatrixInitialized` pattern: the voxel
	// pass should not sample the layer history on the very first
	// frame after (re)init because the zero-initialised content is
	// not a real previous-frame value (it would still blend cleanly
	// because layer values are 0-1 floats, but the explicit gate
	// matches the existing init-flag contract and keeps the
	// history-invalidation semantics consistent). Reset by
	// `VulkanSwapchain.cpp::CreateOrRecreateSwapchain` next to the
	// `taaHistoryColorTarget` reset.
	bool taaLayerHistoryValid = false;
	// 1.5 anti-flicker layer blend factor. 0 = no temporal
	// smoothing (raw current value used in lighting), 1 = full
	// reliance on history (previous-frame value used in lighting,
	// current frame is just written to history for next frame's
	// read). Default 0.4 matches a 1-frame temporal filter
	// (`final = 0.6 * raw_current + 0.4 * raw_previous`) — enough
	// to kill per-frame flicker on AOCC and local-light DDA, not
	// so high that legitimate one-frame lighting changes
	// (e.g. toggling a light on/off) are smeared across frames.
	float taaLayerBlendFactor = 0.4f;
	VkSampler taaLinearSampler = VK_NULL_HANDLE;
	VkImageView taaResolveAttachmentImageView = VK_NULL_HANDLE;
	VkPipelineLayout taaResolvePipelineLayout = VK_NULL_HANDLE;
	VkPipeline taaResolvePipeline = VK_NULL_HANDLE;
	VkDescriptorSetLayout taaResolveDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool taaResolveDescriptorPool = VK_NULL_HANDLE;
	std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> taaResolveDescriptorSets{};
	// Polygon-model import pipeline (M4). Each `ModelRegistryEntry`
	// owns the device-local VBO/IBO for one baked .glb / .gltf asset.
	// `modelInstances` is the per-frame list of draw commands, built
	// by `FramePreparation::BuildModelDrawList` from the current ECS
	// snapshot + the camera frustum. The model pass reuses the main
	// `graphicsDescriptorSet` (binding 3 = SceneLightingBuffer is the
	// only descriptor it needs), so no new descriptor set
	// allocation is required for M4.
	std::vector<ModelRegistryEntry> modelRegistry;
	std::vector<ModelInstanceData> modelInstances;
	// M5: per-frame frustum-culled subset of `modelInstances`. Built
	// each frame in `FramePreparation::BuildVisibleModelInstanceList`
	// from the same `ChunkCullingParameters` that drives chunk
	// visibility. The render command buffer iterates this filtered
	// list rather than `modelInstances` so off-screen / max-distance
	// instances do not generate draw calls. The allocation is
	// reused across frames; capacity tracks the worst-case
	// `modelInstances.size()`.
	std::vector<ModelInstanceData> visibleModelInstances;
	VkPipelineLayout modelPipelineLayout = VK_NULL_HANDLE;
	VkPipeline modelPipeline = VK_NULL_HANDLE;
	VkPipeline modelPipelineTaaOn = VK_NULL_HANDLE;
	// The model pipeline reuses the main `graphicsPipelineLayout`
	// (`modelPipelineLayout` stays null); the TAA-on variant is a
	// separate VkPipeline object that binds the TAA-on shader variant.
	// Per-pass CPU timings (2026-06-12). Each `Record*Commands`
	// function in `Renderer.cpp` writes its own field at exit;
	// `AppUpdate.cpp` computes `otherMs` from
	// `frameTimeMs - sum(measured passes)` and mirrors the
	// whole struct into `DebugStats` for HUD/sidecar
	// consumption. Field semantics per `RenderPassTimings`.
	RenderPassTimings renderPassTimings{};
};

struct LookDevCaptureAutomationState {
	bool active = false;
	bool quitWhenDone = false;
	bool completed = false;
	uint32_t warmupFramesRemaining = 0;
	uint32_t intervalFrames = 2;
	uint32_t intervalFramesRemaining = 0;
	std::array<LightingDebugView, MAX_LOOK_DEV_CAPTURE_VIEW_COUNT> views{};
	uint32_t viewCount = 0;
	uint32_t nextViewIndex = 0;
};

// **5.3 Benchmark automation, 2026-06-12.** Tracks per-frame timings
// over a fixed-frame target driven by `PROJECTV_BENCHMARK_FRAMES` so the
// operator can capture stable FPS / min-ms / max-ms numbers from a
// CI-style headless run without writing a one-off test harness. The
// state mirrors `LookDevCaptureAutomationState`'s shape (active /
// quitWhenDone / completed) so the wiring in `main.cpp` is
// symmetrical. Warmup frames are discarded (the first ~30 frames of
// any ProjectV run include Vulkan pipeline compile, VMA pool warmup,
// shader SPIR-V load, and the first chunk meshing dispatch — none of
// which represent steady-state cost). `minFrameSeconds` /
// `maxFrameSeconds` use a "first-sample" sentinel (`1e30f` / `0.0f`)
// so the first valid frame always wins. The cumulative
// `totalFrameSeconds` is what `LogBenchmarkSummary` divides by
// `framesRendered` to compute the mean.
struct BenchmarkAutomationState {
	bool active = false;
	bool quitWhenDone = false;
	bool completed = false;
	uint32_t warmupFramesRemaining = 0;
	uint32_t targetFrameCount = 0;
	uint32_t framesRendered = 0;
	uint32_t logEveryFrames = 60;
	Uint64 startCounter = 0;
	Uint64 firstFrameCounter = 0;
	Uint64 lastFrameCounter = 0;
	float totalFrameSeconds = 0.0f;
	float minFrameSeconds = 1e30f;
	float maxFrameSeconds = 0.0f;
};

struct FrameState {
	uint32_t currentFrame = 0;
	std::vector<VkCommandBuffer> commandBuffers;
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	FrameRenderData renderData{};
};

struct SimulationState {
	Uint64 lastFrameCounter = 0;
	float frameDeltaSeconds = 0.0f;
	float simulationAccumulatorSeconds = 0.0f;
	float fixedSimulationDeltaSeconds = 1.0f / 60.0f;
	uint32_t simulationStepsLastFrame = 0;
	uint64_t simulationTick = 0;
	bool paused = false;
	// **Frame-step / slow-motion debug, 2026-06-12.** Live
	// runtime multiplier on the per-frame delta. 1.0 = realtime,
	// 0.5 = half speed, 0 = effectively paused (same end-state as
	// `TogglePause` but on a continuous axis, not a discrete
	// toggle). Applied to `frameDeltaSeconds` after
	// `ComputeFrameDeltaSeconds` in `UpdateApp` so the
	// fixed-step physics accumulator, look-dev capture
	// automation, and input replay recording all see the
	// scaled delta. Clamped to [0, 4] by the action handlers;
	// the `[` key halves and snaps to 0 below 0.01 for a
	// discrete "pause" stop, the `]` key doubles and clamps
	// at 4.0, the `` ` `` key resets to 1.0.
	float timeScale = 1.0f;
	// **Frame-step request, 2026-06-12.** One-shot flag. Set by
	// the `StepSingleFrame` action handler, consumed at the
	// top of the `UpdateApp` accumulator block. The consumer
	// forces `simulationAccumulatorSeconds = fixedSimulationDeltaSeconds`
	// and computes `effectivePaused = paused && !frameStepRequestedNow`
	// so exactly one fixed tick runs that frame even when
	// `paused == true`. The flag is read-then-cleared, so a
	// second press while the first tick is still in the
	// accumulator queue is impossible to lose — but the queue
	// only ever holds one step at a time, so back-to-back
	// presses translate to "one tick per press".
	bool frameStepRequested = false;
};

struct InputState {
	float mouseDeltaX = 0.0f;
	float mouseDeltaY = 0.0f;
	bool removePressed = false;
	bool placePressed = false;
	bool relativeMouseModeEnabled = true;
	// When `SDL_SetWindowRelativeMouseMode(true)` is first enabled (or
	// re-enabled after a tab-toggle) the first SDL_EVENT_MOUSE_MOTION can carry
	// a very large `xrel` / `yrel` because the cursor was still at its
	// pre-capture screen position. Without this flag, that one event
	// yanks the camera sharply (typically pitching the look down to the
	// floor) the moment the program starts. Defaulted to true so the first
	// capture-mode motion on launch is silently dropped; reset to true from
	// `SetRelativeMouseMode` whenever the user re-toggles relative mode.
	bool skipFirstMouseMotion = true;
	Uint64 lastMoveUpPressedTimestampNs = 0;
	std::array<InputActionButtonState, kInputActionCount> actions{};
	std::array<InputActionBinding, kInputActionCount> bindings{};
	InputReplayState replay{};
};

struct DebugState {
	DebugStats stats{};
	float titleUpdateAccumulatorSeconds = 0.0f;
	bool hudVisible = true;
	bool detailedHudVisible = false;
	bool showChunkBounds = false;
	bool showDirtyChunkOverlay = false;
	// 5.2 debug gizmos. See `InputAction::ToggleCascadeSplitPlanes`
	// and `InputAction::ToggleCursorHitNormal` for the hotkey
	// contracts. Both overlays only emit when `hudVisible` is also
	// true (matches the existing `showChunkBounds` /
	// `showDirtyChunkOverlay` contract — the boxes are a
	// diagnostic aid, not a primary UI element).
	bool showCascadeSplitPlanes = false;
	bool showCursorHitNormal = false;
};

struct PlatformState {
	SDL_Window *window = nullptr;
	bool windowResized = false;
};

struct VulkanContextState {
	VkInstance instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue queue = VK_NULL_HANDLE;
	uint32_t queueFamilyIndex = 0;
	VmaAllocator allocator = VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	// `dynamicRenderingUnusedAttachments` is the runtime gate that lets the
	// main voxel graphics pipeline declare two color attachment formats
	// (`swapchain_format` + `R16G16B16A16_SFLOAT`) and still bind it with
	// `imageView = VK_NULL_HANDLE` on the unused slot when TAA is off. The
	// pipeline would otherwise trip
	// VUID-VkRenderingAttachmentInfo-imageView-06135 (`imageView` must be
	// non-NULL when the pipeline declared a non-`VK_FORMAT_UNDEFINED`
	// format for that slot). Set during `InitializeVulkanBase` from the
	// result of `TryPickPhysicalDevice`; read by
	// `VulkanGraphicsPipeline::CreateGraphicsPipeline` to fail fast on
	// devices that lack the extension. The TAA-off path itself works
	// without this feature, but the dual-format pipeline declaration does
	// not.
	bool supportsDynamicRenderingUnusedAttachments = false;
};

struct SwapchainState {
	VkSwapchainKHR handle = VK_NULL_HANDLE;
	VkFormat format = VK_FORMAT_UNDEFINED;
	VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	VkExtent2D extent = {};
	bool supportsTransferSrc = false;
	std::vector<VkImage> images;
	std::vector<VkImageView> imageViews;
	// Per-swapchain-image "submit finished" semaphores. Indexed by the
	// `imageIndex` returned by `vkAcquireNextImageKHR`, *not* by the
	// in-flight frame counter. The submit pipeline signals
	// `submitSemaphores[imageIndex]` and the present pipeline waits on
	// the same handle. The canonical Vulkan pattern (per the SDK 1.4
	// guide `swapchain_semaphore_reuse.html`) requires this indexing —
	// a per-in-flight-frame array is what triggers the validation
	// layer's "semaphore may still be in use by VkSwapchainKHR" warning
	// because two consecutive in-flight frames can be handed the same
	// `imageIndex` before the first one's present has retired its
	// `pWaitSemaphores`. Created in `CreateOrRecreateSwapchain` so the
	// size always matches the current swapchain image count.
	//
	// Note: the per-in-flight-frame `imageAvailableSemaphore` in
	// `FrameState` (the `acquire_semaphore` in the guide's pseudocode)
	// stays per-frame. The guide uses a per-frame *semaphore* for
	// acquire and a per-image *semaphore* for submit; we do the same.
	std::vector<VkSemaphore> submitSemaphores;
};

struct AppState {
	PlatformState platform{};
	VulkanContextState context{};
	SwapchainState swapchain{};
	WorldState world{};
	RenderState render{};
	FrameState frame{};
	SimulationState simulation{};
	InputState input{};
	InteractionState interaction{};
	LookDevCaptureAutomationState lookDevCapture{};
	// 5.3 benchmark automation. See `BenchmarkAutomationState` for the
	// per-frame field contract; the env var reader is in
	// `app/BenchmarkAutomation.cpp` and the per-frame tick is wired in
	// `main.cpp::SDL_AppIterate` right after the look-dev capture
	// automation. Inactive when `PROJECTV_BENCHMARK_FRAMES` is unset.
	BenchmarkAutomationState benchmark{};
	EcsStatePtr ecs{nullptr, DestroyEcsState};
	PhysicsStatePtr physics{nullptr, DestroyPhysicsState};
	// **Audio engine, 2026-06-12.** Single
	// `AudioEngine` instance mirroring the
	// `physics` / `ecs` / `render` singletons.
	// Stays `nullptr` if `init` failed (logs and
	// returns false) so the rest of the program
	// keeps running without audio. The
	// destructor is the engine's own
	// `~AudioEngine`, which calls `shutdown`
	// unconditionally.
	AudioEnginePtr audio{nullptr, DestroyAudioEngine};

	bool shutdownDone = false;

	~AppState() = default;
};

void ShutdownVulkan(AppState *state);

#endif

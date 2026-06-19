#pragma once

#if defined(__clang__) && defined(_MSC_VER)

#else
import projectv.math;
import projectv.string_id;
#endif

#include "SDL3/SDL.h"
#include "asset/MeshGpuResources.hpp"
#include "core/Math.hpp"
#include "core/StringId.hpp"
#include "render/ShadowTypes.hpp"
#include "render/TaaRenderTargets.hpp"
#include "render/VoxelMeshingPushConstants.hpp"
#include "voxel/VoxelMaterials.hpp"

namespace projectv::taa {
struct OffscreenColorTarget;
} // namespace projectv::taa
#include "vk_mem_alloc.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>
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
	PickModel,
	ToggleCascadeSplitPlanes,
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

struct GraphicsPushConstants {
	projectv::math::Mat4 viewProjection{};
	projectv::math::Vec4 cameraPosition{};
	projectv::math::Vec4 cameraForward{};
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
	projectv::math::Mat4 inverseCurrentViewProjection{};
	projectv::math::Mat4 currentViewProjection{};
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
	projectv::math::Mat4 viewProjection{};
	projectv::math::Vec4 overlayData0{};
	projectv::math::Vec4 overlayData1{};
	projectv::math::Vec4 overlayColor{};
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
	projectv::math::Vec4 color{};
};
static_assert(std::is_standard_layout_v<DebugHudVertex>);
static_assert(std::is_trivially_copyable_v<DebugHudVertex>);
static_assert(sizeof(DebugHudVertex) == 32);
static_assert(offsetof(DebugHudVertex, positionNdc) == 0);
static_assert(offsetof(DebugHudVertex, color) == 16);

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
	bool walkAutoJumpDelayEnabled = true;
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

struct DebugOverlayBox {
	Int3 min{};
	Int3 maxExclusive{};
	projectv::math::Vec4 color{};
};

struct ChunkCullingParameters {
	projectv::math::Vec4 cameraPositionAndMaxDistance{};
	projectv::math::Vec4 cameraForwardAndTanHalfVerticalFov{};
	projectv::math::Vec4 cameraRightAndTanHalfHorizontalFov{};
	projectv::math::Vec4 cameraUpAndNearPlane{};
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
	float simulationTimeScale = 1.0f;
	bool simulationFrameStepPending = false;
	float renderPassShadowMs = 0.0f;
	float renderPassMeshingMs = 0.0f;
	float renderPassGraphicsMs = 0.0f;
	float renderPassTaaResolveMs = 0.0f;
	float renderPassDebugOverlayMs = 0.0f;
	float renderPassDebugHudMs = 0.0f;
	float renderPassOtherMs = 0.0f;
	uint32_t renderPassDirtyChunkRebuiltCount = 0;
	uint8_t audioMusicState = 0;
	float audioMusicVolume = 0.8f;
	bool audioMusicInitialized = false;
	uint32_t audioMusicPlaylistSize = 0;
	uint32_t audioMusicCurrentIndex = 0;
	std::array<char, 128> audioMusicTrackName{};
	std::array<char, 96> audioMusicArtist{};
	std::array<char, 128> audioMusicTitle{};
	float audioMusicPositionSec = 0.0f;
	float audioMusicDurationSec = 0.0f;
	bool showChunkBounds = false;
	bool showDirtyChunkOverlay = false;
	bool walkDebugValid = false;
	uint8_t walkSupportState = 0;
	projectv::math::Vec3 walkFeetPosition{};
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

	bool taaEnabled = false;
	float taaBlend = 0.0f;
	uint32_t taaFrameCounter = 0u;
	bool taaHistoryValid = false;
	float taaJitterX = 0.0f;
	float taaJitterY = 0.0f;

	float taaJitterScale = 1.0f;
	int32_t taaNeighbourhoodRadius = 1;
	float taaCasSharpnessMax = 0.5f;

	bool taaLayerHistoryValid = false;

	float taaLayerBlendFactor = 0.4f;

	uint32_t taaCameraCutCount = 0;
	float taaCameraCutMaxDelta = 0.0f;
	projectv::math::Vec3 sunDirection{};
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
	projectv::math::Vec3 localPointLightPosition{};
	projectv::math::Vec3 localPointLightColor{};
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
	projectv::math::Mat4 modelTransform{};
	projectv::math::Vec3 worldAabbMin{};
	projectv::math::Vec3 worldAabbMax{};

	projectv::math::Vec3 sourceAabbMin{};
	VkBuffer vertexBuffer = VK_NULL_HANDLE;
	VkBuffer indexBuffer = VK_NULL_HANDLE;
	uint32_t indexCount = 0;
};

struct ModelRegistryEntry {
	projectv::core::StringID id;
	projectv::asset::MeshGpuResources gpu;
	projectv::math::Vec3 aabbMin{0.0f, 0.0f, 0.0f};
	projectv::math::Vec3 aabbMax{0.0f, 0.0f, 0.0f};
};

struct RenderPassTimings {
	float shadowMs = 0.0f;
	float meshingMs = 0.0f;
	float graphicsMs = 0.0f;
	float taaResolveMs = 0.0f;
	float debugOverlayMs = 0.0f;
	float debugHudMs = 0.0f;

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

	VkImageLayout depthImageCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout taaSceneColorCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkImageLayout taaHistoryColorCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

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

	bool taaEnabled = true;
	float taaBlend = 0.10f;
	uint32_t taaFrameCounter = 0u;
	bool taaHistoryValid = false;
	bool taaSceneColorNeedsInit = true;
	bool taaHistoryNeedsInit = true;
	projectv::math::Mat4 taaPrevViewProjectionMatrix{};
	float taaJitterX = 0.0f;
	float taaJitterY = 0.0f;
	float taaJitterScale = 0.0f;
	int32_t taaNeighbourhoodRadius = 1;

	float taaCasSharpnessMax = 0.5f;

	uint32_t taaCameraCutCount = 0;
	float taaCameraCutMaxDelta = 0.0f;

	bool taaPrevViewProjectionMatrixInitialized = false;

	projectv::taa::OffscreenColorTarget *taaSceneColorTarget = nullptr;
	projectv::taa::OffscreenColorTarget *taaHistoryColorTarget = nullptr;

	projectv::taa::OffscreenColorTarget *taaLayerSceneColorTarget = nullptr;
	projectv::taa::OffscreenColorTarget *taaLayerHistoryColorTarget = nullptr;

	bool taaLayerHistoryValid = false;

	float taaLayerBlendFactor = 0.4f;
	VkSampler taaLinearSampler = VK_NULL_HANDLE;
	VkImageView taaResolveAttachmentImageView = VK_NULL_HANDLE;
	VkPipelineLayout taaResolvePipelineLayout = VK_NULL_HANDLE;
	VkPipeline taaResolvePipeline = VK_NULL_HANDLE;
	VkDescriptorSetLayout taaResolveDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool taaResolveDescriptorPool = VK_NULL_HANDLE;
	std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> taaResolveDescriptorSets{};

	std::vector<ModelRegistryEntry> modelRegistry;
	std::vector<ModelInstanceData> modelInstances;

	std::vector<ModelInstanceData> visibleModelInstances;
	VkPipelineLayout modelPipelineLayout = VK_NULL_HANDLE;
	VkPipeline modelPipeline = VK_NULL_HANDLE;
	VkPipeline modelPipelineTaaOn = VK_NULL_HANDLE;
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
	float timeScale = 1.0f;
	bool frameStepRequested = false;
	float fluidTickRateHz = 20.0f;
	float fluidAccumulatorSeconds = 0.0f;
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

struct DebugState {
	DebugStats stats{};
	float titleUpdateAccumulatorSeconds = 0.0f;
	bool hudVisible = true;
	bool detailedHudVisible = false;
	bool showChunkBounds = false;
	bool showDirtyChunkOverlay = false;

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

	BenchmarkAutomationState benchmark{};
	EcsStatePtr ecs{nullptr, DestroyEcsState};
	PhysicsStatePtr physics{nullptr, DestroyPhysicsState};
	AudioEnginePtr audio{nullptr, DestroyAudioEngine};

	bool shutdownDone = false;

	~AppState() = default;
};

void ShutdownVulkan(AppState *state);

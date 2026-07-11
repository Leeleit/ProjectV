#pragma once

#if defined(__clang__) && defined(_MSC_VER) // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#else
import projectv.math;
import projectv.string_id;
#endif

#include "SDL3/SDL.h"
#include "asset/MeshGpuResources.hpp"
import projectv.math;
import projectv.string_id;
#include "render/ShadowTypes.hpp"
#include "render/VoxelMeshingPushConstants.hpp"
#include "render/HizCulling.hpp"
#include "voxel/NanoVdb.hpp"
#include "voxel/VoxelMaterials.hpp"
#include "render/vulkan/HardwareRayTracingProbe.hpp"
#include "core/InputTypes.hpp"

namespace projectv::render {
class RayTracedShadows;
class RtxGiProbes;
} // namespace projectv::render

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
constexpr uint32_t DEBUG_HUD_MAX_VERTEX_COUNT = 262144;

struct VoxelWorld;
struct EcsState;
struct PhysicsState;
namespace projectv::audio {
class AudioEngine;
} // namespace projectv::audio
void DestroyEcsState(EcsState *ecs);
void DestroyPhysicsState(PhysicsState *physics); // NOLINT(readability-redundant-declaration): shared with PhysicsWorld.hpp
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

struct PackedSceneChunkAabb {
	std::array<float, 4> centerAndHalfExtent{};
	std::array<float, 4> originAndPadding{};
};
static_assert(std::is_standard_layout_v<PackedSceneChunkAabb>);
static_assert(std::is_trivially_copyable_v<PackedSceneChunkAabb>);
static_assert(sizeof(PackedSceneChunkAabb) == 32);
static_assert(offsetof(PackedSceneChunkAabb, centerAndHalfExtent) == 0);
static_assert(offsetof(PackedSceneChunkAabb, originAndPadding) == 16);

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
	VkBuffer chunkAabbBuffer = VK_NULL_HANDLE;
	VkBuffer visibilityMaskBuffer = VK_NULL_HANDLE;
	VkBuffer hzbVisibleCountBuffer = VK_NULL_HANDLE;
	VkDescriptorSet graphicsDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSet voxelMeshingDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSet hizCullingDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSet meshShaderDescriptorSet = VK_NULL_HANDLE;
	VkBuffer opaqueIndirectBuffer = VK_NULL_HANDLE;
	VkBuffer transparentIndirectBuffer = VK_NULL_HANDLE;
	uint32_t chunkDescriptorCount = 0;
	uint32_t dirtyChunkCount = 0;
	uint32_t opaqueFaceCount = 0;
	uint32_t transparentFaceCount = 0;
	uint32_t debugHudVertexCount = 0;
	bool debugUiVisible = true;
	GraphicsPushConstants graphicsPushConstants{};
	VoxelMeshingPushConstants voxelMeshingPushConstants{};
	ChunkCullingParameters chunkCullingParameters{};
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

	projectv::math::Vec3 sunDirection{};
	float sunIntensity = 0.0f;
	TransparentShadowPolicy transparentShadowPolicy = TransparentShadowPolicy::GlassIgnoredFluidCasts;
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
	VmaAllocation packedFaceAllocation = nullptr;
	void *debugHudVertexMappedData = nullptr;
	VkBuffer debugHudVertexBuffer = VK_NULL_HANDLE;
	VmaAllocation debugHudVertexAllocation = nullptr;
	void *chunkDescriptorMappedData = nullptr;
	VkBuffer chunkDescriptorBuffer = VK_NULL_HANDLE;
	VmaAllocation chunkDescriptorAllocation = nullptr;
	void *chunkVoxelPayloadMappedData = nullptr;
	VkBuffer chunkVoxelPayloadBuffer = VK_NULL_HANDLE;
	VmaAllocation chunkVoxelPayloadAllocation = nullptr;
	void *opaqueIndirectMappedData = nullptr;
	VkBuffer opaqueIndirectBuffer = VK_NULL_HANDLE;
	VmaAllocation opaqueIndirectAllocation = nullptr;
	void *transparentIndirectMappedData = nullptr;
	VkBuffer transparentIndirectBuffer = VK_NULL_HANDLE;
	VmaAllocation transparentIndirectAllocation = nullptr;
	void *dirtyChunkIndexMappedData = nullptr;
	VkBuffer dirtyChunkIndexBuffer = VK_NULL_HANDLE;
	VmaAllocation dirtyChunkIndexAllocation = nullptr;
	void *chunkCullingMappedData = nullptr;
	VkBuffer chunkCullingBuffer = VK_NULL_HANDLE;
	VmaAllocation chunkCullingAllocation = nullptr;
	void *sceneLightingMappedData = nullptr;
	VkBuffer sceneLightingBuffer = VK_NULL_HANDLE;
	VmaAllocation sceneLightingAllocation = nullptr;
	void *chunkAabbMappedData = nullptr;
	VkBuffer chunkAabbBuffer = VK_NULL_HANDLE;
	VmaAllocation chunkAabbAllocation = nullptr;
	void *visibilityMaskMappedData = nullptr;
	VkBuffer visibilityMaskBuffer = VK_NULL_HANDLE;
	VmaAllocation visibilityMaskAllocation = nullptr;
	void *hzbVisibleCountMappedData = nullptr;
	VkBuffer hzbVisibleCountBuffer = VK_NULL_HANDLE;
	VmaAllocation hzbVisibleCountAllocation = nullptr;
	void *hzbPerChunkMipMappedData = nullptr;
	VkBuffer hzbPerChunkMipBuffer = VK_NULL_HANDLE;
	VmaAllocation hzbPerChunkMipAllocation = nullptr;
	VkDeviceSize hzbPerChunkMipCapacityBytes = 0u;
	void *lodDownsampledVoxelPayloadMappedData = nullptr;
	VkBuffer lodDownsampledVoxelPayloadBuffer = VK_NULL_HANDLE;
	VmaAllocation lodDownsampledVoxelPayloadAllocation = nullptr;
	VkDeviceSize lodDownsampledVoxelPayloadCapacityBytes = 0u;
	void *chunkLodLevelsMappedData = nullptr;
	VkBuffer chunkLodLevelsBuffer = VK_NULL_HANDLE;
	VmaAllocation chunkLodLevelsAllocation = nullptr;
	uint32_t chunkLodLevelsCapacity = 0u;
	void *visibleChunkIdMappedData = nullptr;
	VkBuffer visibleChunkIdBuffer = VK_NULL_HANDLE;
	VmaAllocation visibleChunkIdAllocation = nullptr;
	void *visibilityCounterMappedData = nullptr;
	VkBuffer visibilityCounterBuffer = VK_NULL_HANDLE;
	VmaAllocation visibilityCounterAllocation = nullptr;
	void *fluidCaSourceMappedData = nullptr;
	VkBuffer fluidCaSourceBuffer = VK_NULL_HANDLE;
	VmaAllocation fluidCaSourceAllocation = nullptr;
	void *fluidCaDestinationMappedData = nullptr;
	VkBuffer fluidCaDestinationBuffer = VK_NULL_HANDLE;
	VmaAllocation fluidCaDestinationAllocation = nullptr;
	void *fluidCaActiveChunkIdMappedData = nullptr;
	VkBuffer fluidCaActiveChunkIdBuffer = VK_NULL_HANDLE;
	VmaAllocation fluidCaActiveChunkIdAllocation = nullptr;
	void *fluidCaStatsMappedData = nullptr;
	VkBuffer fluidCaStatsBuffer = VK_NULL_HANDLE;
	VmaAllocation fluidCaStatsAllocation = nullptr;
	void *nanovdbUpperMappedData = nullptr;
	VkBuffer nanovdbUpperBuffer = VK_NULL_HANDLE;
	VmaAllocation nanovdbUpperAllocation = nullptr;
	VkDeviceSize nanovdbUpperCapacityBytes = 0u;
	void *nanovdbLowerMappedData = nullptr;
	VkBuffer nanovdbLowerBuffer = VK_NULL_HANDLE;
	VmaAllocation nanovdbLowerAllocation = nullptr;
	VkDeviceSize nanovdbLowerCapacityBytes = 0u;
	void *nanovdbLeafMappedData = nullptr;
	VkBuffer nanovdbLeafBuffer = VK_NULL_HANDLE;
	VmaAllocation nanovdbLeafAllocation = nullptr;
	VkDeviceSize nanovdbLeafCapacityBytes = 0u;
	void *nanovdbMaterialMappedData = nullptr;
	VkBuffer nanovdbMaterialBuffer = VK_NULL_HANDLE;
	VmaAllocation nanovdbMaterialAllocation = nullptr;
	VkDeviceSize nanovdbMaterialCapacityBytes = 0u;
	void *worldGenVoxelMappedData = nullptr;
	VkBuffer worldGenVoxelBuffer = VK_NULL_HANDLE;
	VmaAllocation worldGenVoxelAllocation = nullptr;
	VkDeviceSize worldGenVoxelCapacityBytes = 0u;
	VkDescriptorSet worldGenDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSet graphicsDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSet meshShaderDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSet voxelMeshingDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSet hizCullingDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSet fluidCaDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSet vctVoxelizeDescriptorSet = VK_NULL_HANDLE;
	uint64_t uploadedSceneVersion = 0;
	uint64_t uploadedVoxelPayloadVersion = 0;
	uint64_t meshedSceneVersion = 0;
	uint64_t uploadedNanoVdbVersion = 0;
	uint32_t chunkDescriptorCount = 0;
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
	bool allowWorldEditing = false;
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
	float debugOverlayMs = 0.0f;
	float debugHudMs = 0.0f;

	float otherMs = 0.0f;
	uint32_t dirtyChunkRebuiltCount = 0;
};

struct RenderState { // ownership: Create*/Destroy* pair per VkBuffer+VmaAllocation, VkImage, VkPipeline, VkShaderModule, VkDescriptorSetLayout field
	std::vector<PackedSceneChunkDescriptor> sceneChunkDescriptors;
	std::vector<SceneChunkVoxelPayloadRange> sceneChunkVoxelPayloadRanges;
	std::vector<uint32_t> sceneChunkVoxelPayloadWords;
	std::vector<size_t> latestVoxelPayloadChunkIndices;
	std::vector<size_t> pendingChunkRebuildIndices;
	std::vector<size_t> completedChunkRebuildIndices;
	projectv::voxel::nanovdb::NanoVdbFlattenResult sceneNanoVdbFlatten;
	uint64_t sceneNanoVdbVersion = 0;
	uint64_t lastNanoVdbSyncedEditVersion = 0;
	uint32_t sceneFaceCapacity = 0;
	uint32_t sceneTransparentFaceBase = 0;
	uint32_t sceneOpaqueFaceCount = 0;
	uint32_t sceneTransparentFaceCount = 0;
	uint32_t sceneChunkVoxelPayloadWordCount = 0;
	uint64_t sceneUploadVersion = 0;
	uint64_t sceneVoxelPayloadVersion = 0;
	uint64_t lodDownsampledPayloadVersion = 0;
	uint32_t sceneTriangleCount = 0;
	uint64_t sceneMemoryBytes = 0;
	void *tracyGraphicsContext = nullptr;
	bool tracyGraphicsContextCalibrated = false;
	VoxelLightingDebugControls lightingDebugControls{};
	VoxelSceneLighting currentSceneLighting{};
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
	VkDescriptorSetLayout voxelMeshingDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool voxelMeshingDescriptorPool = VK_NULL_HANDLE;
	std::array<SceneFrameResources, MAX_FRAMES_IN_FLIGHT> sceneFrameResources{};

	struct DeferredNanoVdbDestroyEntry {
		VkBuffer buffer = VK_NULL_HANDLE;
		VmaAllocation allocation = nullptr;
	};
	std::array<std::vector<DeferredNanoVdbDestroyEntry>, MAX_FRAMES_IN_FLIGHT>
		deferredNanoVdbDestroys{};

	ChunkVisibilityCache chunkVisibilityCache{};
	projectv::render::RayTracedShadows *rayTracedShadows = nullptr;
	projectv::render::RtxGiProbes *rtxGiProbes = nullptr;
	VkImage depthImage = VK_NULL_HANDLE;
	VkImageView depthImageView = VK_NULL_HANDLE;
	VmaAllocation depthAllocation = nullptr;
	VkImage sceneColorImage = VK_NULL_HANDLE;
	VkImageView sceneColorImageView = VK_NULL_HANDLE;
	VmaAllocation sceneColorAllocation = nullptr;
	VkImageLayout sceneColorCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	bool sceneColorNeedsInit = false;
	VkImage vctClipmapImage = VK_NULL_HANDLE;
	VkImageView vctClipmapView = VK_NULL_HANDLE;
	VmaAllocation vctClipmapAllocation = nullptr;
	VkDeviceMemory vctClipmapMemory = VK_NULL_HANDLE;
	uint32_t vctClipmapResolution = 256u;
	uint32_t vctClipmapMipLevelCount = 4u;
	bool vctClipmapEnabled = false;
	VkSampler vctClipmapSampler = VK_NULL_HANDLE;
	VkDescriptorSetLayout vctVoxelizeDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool vctVoxelizeDescriptorPool = VK_NULL_HANDLE;
	std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> vctVoxelizeDescriptorSets{};
	VkShaderModule vctVoxelizeShaderModule = VK_NULL_HANDLE;
	VkPipelineLayout vctVoxelizePipelineLayout = VK_NULL_HANDLE;
	VkPipeline vctVoxelizePipeline = VK_NULL_HANDLE;
	bool depthImageNeedsInit = false;
	projectv::render::HizBuffer hizBuffer{};
	VkPipelineLayout hizCullingPipelineLayout = VK_NULL_HANDLE;
	VkPipeline hizCullingPipeline = VK_NULL_HANDLE;
	VkShaderModule hizCullingShaderModule = VK_NULL_HANDLE;
	VkDescriptorSetLayout hizCullingDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool hizCullingDescriptorPool = VK_NULL_HANDLE;
	bool hizBufferNeedsInit = false;
	bool hizCullingEnabled = false;

	VkImageLayout depthImageCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkPipeline skyAtmospherePipeline = VK_NULL_HANDLE;
	VkPipelineLayout skyAtmospherePipelineLayout = VK_NULL_HANDLE;
	VkShaderModule skyAtmosphereVertexShaderModule = VK_NULL_HANDLE;
	VkShaderModule skyAtmosphereFragmentShaderModule = VK_NULL_HANDLE;
	VkDescriptorSetLayout skyAtmosphereDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool skyAtmosphereDescriptorPool = VK_NULL_HANDLE;
	std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> skyAtmosphereDescriptorSets{};
	bool skyAtmospherePipelineEnabled = false;
	VkImage skyViewLutImage = VK_NULL_HANDLE;
	VkImageView skyViewLutView = VK_NULL_HANDLE;
	VmaAllocation skyViewLutAllocation = nullptr;
	VkImage multiScatteringLutImage = VK_NULL_HANDLE;
	VkImageView multiScatteringLutView = VK_NULL_HANDLE;
	VmaAllocation multiScatteringLutAllocation = nullptr;
	VkSampler skyLutLinearSampler = VK_NULL_HANDLE;
	bool skyLutPrecomputeEnabled = false;
	VkPipeline volumetricFogPipeline = VK_NULL_HANDLE;
	VkPipelineLayout volumetricFogPipelineLayout = VK_NULL_HANDLE;
	VkShaderModule volumetricFogShaderModule = VK_NULL_HANDLE;
	VkDescriptorSetLayout volumetricFogDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool volumetricFogDescriptorPool = VK_NULL_HANDLE;
	std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> volumetricFogDescriptorSets{};
	VkImage volumetricFogFroxelImage = VK_NULL_HANDLE;
	VkImageView volumetricFogFroxelView = VK_NULL_HANDLE;
	VmaAllocation volumetricFogFroxelAllocation = nullptr;
	VkSampler volumetricFogLinearSampler = VK_NULL_HANDLE;
	bool volumetricFogPipelineEnabled = false;
	VkImage volumetricFogFallbackImage = VK_NULL_HANDLE;
	VkImageView volumetricFogFallbackView = VK_NULL_HANDLE;
	VmaAllocation volumetricFogFallbackAllocation = nullptr;
	VkDeviceMemory volumetricFogFallbackMemory = VK_NULL_HANDLE;
	VkImage rtxShadowMaskFallbackImage = VK_NULL_HANDLE;
	VkImageView rtxShadowMaskFallbackView = VK_NULL_HANDLE;
	VmaAllocation rtxShadowMaskFallbackAllocation = nullptr;
	VkDeviceMemory rtxShadowMaskFallbackMemory = VK_NULL_HANDLE;
	VkPipeline cloudscapePipeline = VK_NULL_HANDLE;
	VkPipelineLayout cloudscapePipelineLayout = VK_NULL_HANDLE;
	VkShaderModule cloudscapeVertexShaderModule = VK_NULL_HANDLE;
	VkShaderModule cloudscapeFragmentShaderModule = VK_NULL_HANDLE;
	VkDescriptorSetLayout cloudscapeDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool cloudscapeDescriptorPool = VK_NULL_HANDLE;
	std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> cloudscapeDescriptorSets{};
	VkImage cloudscapeNoiseImage = VK_NULL_HANDLE;
	VkImageView cloudscapeNoiseView = VK_NULL_HANDLE;
	VmaAllocation cloudscapeNoiseAllocation = nullptr;
	VkSampler cloudscapeLinearSampler = VK_NULL_HANDLE;
	bool cloudscapePipelineEnabled = false;
	VkPipelineLayout graphicsPipelineLayout = VK_NULL_HANDLE;
	VkPipelineLayout shadowPipelineLayout = VK_NULL_HANDLE;
	VkPipeline graphicsPipeline = VK_NULL_HANDLE;
	VkPipeline graphicsPipelineRtx = VK_NULL_HANDLE;
	VkPipeline transparentGraphicsPipeline = VK_NULL_HANDLE;
	VkPipeline transparentDebugGraphicsPipeline = VK_NULL_HANDLE;
	VkPipeline shadowGraphicsPipeline = VK_NULL_HANDLE;
	VkPipelineLayout debugOverlayPipelineLayout = VK_NULL_HANDLE;
	VkPipeline debugOverlayPipeline = VK_NULL_HANDLE;
	VkPipeline debugCrosshairPipeline = VK_NULL_HANDLE;
	VkPipelineLayout debugHudPipelineLayout = VK_NULL_HANDLE;
	VkPipeline debugHudPipeline = VK_NULL_HANDLE;
	VkPipelineLayout voxelMeshingPipelineLayout = VK_NULL_HANDLE;
	VkPipeline voxelMeshingPipeline = VK_NULL_HANDLE;
	VkShaderModule meshCullShaderModule = VK_NULL_HANDLE;
	VkShaderModule meshShaderModule = VK_NULL_HANDLE;
	VkPipelineLayout meshShaderPipelineLayout = VK_NULL_HANDLE;
	VkPipeline meshShaderPipeline = VK_NULL_HANDLE;
	VkPipelineLayout meshCullPipelineLayout = VK_NULL_HANDLE;
	VkPipeline meshCullPipeline = VK_NULL_HANDLE;
	VkDescriptorSetLayout meshShaderDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool meshShaderDescriptorPool = VK_NULL_HANDLE;
	bool meshShaderEnabled = false;
	uint32_t visibleChunkIdCapacity = 0u;
	uint32_t meshShaderMaxOutputVertices = 0u;
	uint32_t meshShaderMaxOutputPrimitives = 0u;
	bool fluidCaPipelineEnabled = false;
	uint32_t fluidCaPingPongBufferBytes = 0u;
	uint32_t fluidCaMaxActiveChunks = 0u;
	VkShaderModule fluidCaShaderModule = VK_NULL_HANDLE;
	VkPipelineLayout fluidCaPipelineLayout = VK_NULL_HANDLE;
	VkPipeline fluidCaPipeline = VK_NULL_HANDLE;
	VkDescriptorSetLayout fluidCaDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool fluidCaDescriptorPool = VK_NULL_HANDLE;

	bool worldGenPipelineEnabled = false;
	VkShaderModule worldGenShaderModule = VK_NULL_HANDLE;
	VkPipelineLayout worldGenPipelineLayout = VK_NULL_HANDLE;
	VkPipeline worldGenPipeline = VK_NULL_HANDLE;
	VkDescriptorSetLayout worldGenDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool worldGenDescriptorPool = VK_NULL_HANDLE;

	std::vector<ModelRegistryEntry> modelRegistry;
	std::vector<ModelInstanceData> modelInstances;

	std::vector<ModelInstanceData> visibleModelInstances;
	VkPipelineLayout modelPipelineLayout = VK_NULL_HANDLE;
	VkPipeline modelPipeline = VK_NULL_HANDLE;
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
	bool effectivePaused = false;
	float fluidTickRateHz = 5.0f;
	float fluidAccumulatorSeconds = 0.0f;
	uint32_t fluidGpuTicksPending = 0u;
};

struct DebugState {
	DebugStats stats{};
	float titleUpdateAccumulatorSeconds = 0.0f;
	bool hudVisible = true;
	bool detailedHudVisible = false;
	bool showChunkBounds = false;
	bool showDirtyChunkOverlay = false;

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
	VkQueue dedicatedComputeQueue = VK_NULL_HANDLE;
	uint32_t dedicatedComputeQueueFamilyIndex = UINT32_MAX;
	bool hasDedicatedComputeQueue = false;
	VkSemaphore renderTimelineSemaphore = VK_NULL_HANDLE;
	uint64_t renderTimelineValue = 0;
	VmaAllocator allocator = VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkCommandPool asyncComputeCommandPool = VK_NULL_HANDLE;
	VkCommandBuffer asyncComputeCommandBuffer = VK_NULL_HANDLE;
	uint64_t asyncComputeLastTimelineValue = 0;
	VkSemaphore hzbBuildTimelineSemaphore = VK_NULL_HANDLE;
	uint64_t hzbBuildLastTimelineValue = 0;

	bool supportsDynamicRenderingUnusedAttachments = false;
	projectv::render::HardwareRayTracingSupport rayTracing{};
};

inline constexpr uint64_t kVulkanFenceWaitTimeoutNs = 10'000'000;
inline constexpr uint64_t kVulkanFenceWaitTimeoutUnboundedNs = UINT64_MAX;

inline constexpr float kVctCutoffRoughness = 0.3f;
inline constexpr float kRtxCutoffRoughness = 0.3f;

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

struct AppStateImpl {
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
};

struct AppState {
	std::unique_ptr<AppStateImpl> impl{std::make_unique<AppStateImpl>()};

	PlatformState &platform() noexcept { return impl->platform; } // NOLINT(readability-make-member-function-const): deliberate mutable accessor
	[[nodiscard]] const PlatformState &platform() const noexcept { return impl->platform; }
	VulkanContextState &context() noexcept { return impl->context; } // NOLINT(readability-make-member-function-const): deliberate mutable accessor
	[[nodiscard]] const VulkanContextState &context() const noexcept { return impl->context; }
	SwapchainState &swapchain() noexcept { return impl->swapchain; } // NOLINT(readability-make-member-function-const): deliberate mutable accessor
	[[nodiscard]] const SwapchainState &swapchain() const noexcept { return impl->swapchain; }
	WorldState &world() noexcept { return impl->world; } // NOLINT(readability-make-member-function-const): deliberate mutable accessor
	[[nodiscard]] const WorldState &world() const noexcept { return impl->world; }
	RenderState &render() noexcept { return impl->render; } // NOLINT(readability-make-member-function-const): deliberate mutable accessor
	[[nodiscard]] const RenderState &render() const noexcept { return impl->render; }
	FrameState &frame() noexcept { return impl->frame; } // NOLINT(readability-make-member-function-const): deliberate mutable accessor
	[[nodiscard]] const FrameState &frame() const noexcept { return impl->frame; }
	SimulationState &simulation() noexcept { return impl->simulation; } // NOLINT(readability-make-member-function-const): deliberate mutable accessor
	[[nodiscard]] const SimulationState &simulation() const noexcept { return impl->simulation; }
	InputState &input() noexcept { return impl->input; } // NOLINT(readability-make-member-function-const): deliberate mutable accessor
	[[nodiscard]] const InputState &input() const noexcept { return impl->input; }
	InteractionState &interaction() noexcept { return impl->interaction; } // NOLINT(readability-make-member-function-const): deliberate mutable accessor
	[[nodiscard]] const InteractionState &interaction() const noexcept { return impl->interaction; }
	LookDevCaptureAutomationState &lookDevCapture() noexcept { return impl->lookDevCapture; } // NOLINT(readability-make-member-function-const): deliberate mutable accessor
	[[nodiscard]] const LookDevCaptureAutomationState &lookDevCapture() const noexcept { return impl->lookDevCapture; }
	BenchmarkAutomationState &benchmark() noexcept { return impl->benchmark; } // NOLINT(readability-make-member-function-const): deliberate mutable accessor
	[[nodiscard]] const BenchmarkAutomationState &benchmark() const noexcept { return impl->benchmark; }
	EcsStatePtr &ecs() noexcept { return impl->ecs; } // NOLINT(readability-make-member-function-const): deliberate mutable accessor
	[[nodiscard]] const EcsStatePtr &ecs() const noexcept { return impl->ecs; }
	PhysicsStatePtr &physics() noexcept { return impl->physics; } // NOLINT(readability-make-member-function-const): deliberate mutable accessor
	[[nodiscard]] const PhysicsStatePtr &physics() const noexcept { return impl->physics; }
	AudioEnginePtr &audio() noexcept { return impl->audio; } // NOLINT(readability-make-member-function-const): deliberate mutable accessor
	[[nodiscard]] const AudioEnginePtr &audio() const noexcept { return impl->audio; }
	bool &shutdownDone() noexcept { return impl->shutdownDone; } // NOLINT(readability-make-member-function-const): deliberate mutable accessor
	[[nodiscard]] const bool &shutdownDone() const noexcept { return impl->shutdownDone; }
};

void ShutdownVulkan(AppState *state);

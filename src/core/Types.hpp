#ifndef TYPES_HPP
#define TYPES_HPP

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "voxel/VoxelMaterials.hpp"
// ReSharper disable once CppUnusedIncludeDirective
#include "volk.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "vma/vk_mem_alloc.h"
#pragma clang diagnostic pop

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

struct VoxelWorld;

struct PackedSceneVoxelFace {
	uint32_t localVoxelFace = 0;
	uint32_t chunkIndexMaterial = 0;
};
static_assert(std::is_standard_layout_v<PackedSceneVoxelFace>);
static_assert(std::is_trivially_copyable_v<PackedSceneVoxelFace>);
static_assert(sizeof(PackedSceneVoxelFace) == 8);
static_assert(offsetof(PackedSceneVoxelFace, localVoxelFace) == 0);
static_assert(offsetof(PackedSceneVoxelFace, chunkIndexMaterial) == 4);

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
	ToggleRelativeMouseMode,
	CyclePlacementMaterial,
	ResetCamera,
	TogglePause,
	ToggleControlMode,
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
		FreeFly,
		Spectator,
	} controlMode = ControlMode::FreeFly;
};

struct GraphicsPushConstants {
	std::array<float, 16> viewProjection{};
	std::array<float, 4> cameraPosition{};
};
static_assert(std::is_standard_layout_v<GraphicsPushConstants>);
static_assert(std::is_trivially_copyable_v<GraphicsPushConstants>);
static_assert(sizeof(GraphicsPushConstants) == 80);
static_assert(offsetof(GraphicsPushConstants, viewProjection) == 0);
static_assert(offsetof(GraphicsPushConstants, cameraPosition) == 64);

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

constexpr uint32_t DEBUG_HUD_MAX_VERTEX_COUNT = 65536;

struct InteractionSelectionState {
	bool hasHit = false;
	bool hasPlacementVoxel = false;
	Int3 targetVoxel{};
	Int3 placementVoxel{};
	Int3 hitNormal{};
	VoxelMaterial targetMaterial = VoxelMaterial::Air;
	float hitDistance = 0.0f;
};

struct InteractionState {
	InteractionSelectionState selection{};
	VoxelMaterial placementMaterial = VoxelMaterial::FloorWhite;
	float maxInteractionDistance = 12.0f;
};

struct VoxelMeshingPushConstants {
	std::array<int32_t, 4> worldMinAndChunkSize{};
	std::array<int32_t, 4> worldMaxExclusiveAndChunkCount{};
	std::array<uint32_t, 4> chunkGridAndTransparentFaceBase{};
	std::array<uint32_t, 4> faceCapacities{};
};
static_assert(std::is_standard_layout_v<VoxelMeshingPushConstants>);
static_assert(std::is_trivially_copyable_v<VoxelMeshingPushConstants>);
static_assert(sizeof(VoxelMeshingPushConstants) == 64);
static_assert(offsetof(VoxelMeshingPushConstants, worldMinAndChunkSize) == 0);
static_assert(offsetof(VoxelMeshingPushConstants, worldMaxExclusiveAndChunkCount) == 16);
static_assert(offsetof(VoxelMeshingPushConstants, chunkGridAndTransparentFaceBase) == 32);
static_assert(offsetof(VoxelMeshingPushConstants, faceCapacities) == 48);

struct FrameRenderData {
	uint32_t frameIndex = 0;
	VkBuffer packedFaceBuffer = VK_NULL_HANDLE;
	VkBuffer chunkDescriptorBuffer = VK_NULL_HANDLE;
	VkBuffer chunkVoxelPayloadBuffer = VK_NULL_HANDLE;
	VkBuffer debugHudVertexBuffer = VK_NULL_HANDLE;
	VkDescriptorSet graphicsDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSet voxelMeshingDescriptorSet = VK_NULL_HANDLE;
	VkBuffer opaqueIndirectBuffer = VK_NULL_HANDLE;
	VkBuffer transparentIndirectBuffer = VK_NULL_HANDLE;
	uint32_t chunkDescriptorCount = 0;
	uint32_t dirtyChunkCount = 0;
	uint32_t opaqueFaceCount = 0;
	uint32_t transparentFaceCount = 0;
	uint32_t debugHudVertexCount = 0;
	GraphicsPushConstants graphicsPushConstants{};
	VoxelMeshingPushConstants voxelMeshingPushConstants{};
	InteractionSelectionState interactionSelection{};
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
	CameraState::ControlMode controlMode = CameraState::ControlMode::FreeFly;
	bool simulationPaused = false;
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
	void *transparentIndirectMappedData = nullptr;
	VkBuffer transparentIndirectBuffer = VK_NULL_HANDLE;
	VmaAllocation transparentIndirectAllocation = VK_NULL_HANDLE;
	void *dirtyChunkIndexMappedData = nullptr;
	VkBuffer dirtyChunkIndexBuffer = VK_NULL_HANDLE;
	VmaAllocation dirtyChunkIndexAllocation = VK_NULL_HANDLE;
	VkDescriptorSet graphicsDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSet voxelMeshingDescriptorSet = VK_NULL_HANDLE;
	uint64_t uploadedSceneVersion = 0;
	uint64_t uploadedVoxelPayloadVersion = 0;
	uint64_t meshedSceneVersion = 0;
	uint32_t chunkDescriptorCount = 0;
	uint32_t dirtyChunkCount = 0;
	uint32_t opaqueFaceCount = 0;
	uint32_t transparentFaceCount = 0;
	uint32_t debugHudVertexCount = 0;
};

struct WorldState {
	std::unique_ptr<VoxelWorld> voxelWorld;
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
	void *materialVisualMappedData = nullptr;
	VkBuffer materialVisualBuffer = VK_NULL_HANDLE;
	VmaAllocation materialVisualAllocation = VK_NULL_HANDLE;
	VkDescriptorSetLayout graphicsDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool graphicsDescriptorPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout voxelMeshingDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool voxelMeshingDescriptorPool = VK_NULL_HANDLE;
	std::array<SceneFrameResources, MAX_FRAMES_IN_FLIGHT> sceneFrameResources{};
	VkImage depthImage = VK_NULL_HANDLE;
	VkImageView depthImageView = VK_NULL_HANDLE;
	VmaAllocation depthAllocation = VK_NULL_HANDLE;
	bool depthImageNeedsInit = false;
	VkPipelineLayout graphicsPipelineLayout = VK_NULL_HANDLE;
	VkPipeline graphicsPipeline = VK_NULL_HANDLE;
	VkPipeline transparentGraphicsPipeline = VK_NULL_HANDLE;
	VkPipelineLayout debugOverlayPipelineLayout = VK_NULL_HANDLE;
	VkPipeline debugOverlayPipeline = VK_NULL_HANDLE;
	VkPipelineLayout debugHudPipelineLayout = VK_NULL_HANDLE;
	VkPipeline debugHudPipeline = VK_NULL_HANDLE;
	VkPipelineLayout voxelMeshingPipelineLayout = VK_NULL_HANDLE;
	VkPipeline voxelMeshingPipeline = VK_NULL_HANDLE;
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
};

struct InputState {
	float mouseDeltaX = 0.0f;
	float mouseDeltaY = 0.0f;
	bool removePressed = false;
	bool placePressed = false;
	bool relativeMouseModeEnabled = true;
	std::array<InputActionButtonState, kInputActionCount> actions{};
	std::array<InputActionBinding, kInputActionCount> bindings{};
};

struct DebugState {
	DebugStats stats{};
	float titleUpdateAccumulatorSeconds = 0.0f;
	bool hudVisible = true;
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
};

struct SwapchainState {
	VkSwapchainKHR handle = VK_NULL_HANDLE;
	VkFormat format = VK_FORMAT_UNDEFINED;
	VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	VkExtent2D extent = {};
	std::vector<VkImage> images;
	std::vector<VkImageView> imageViews;
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
	DebugState debug{};
	CameraState camera{};

	bool shutdownDone = false;

	~AppState();
};

void ShutdownVulkan(AppState *state);

#endif

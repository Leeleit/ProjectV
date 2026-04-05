#ifndef TYPES_HPP
#define TYPES_HPP

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "VoxelWorld.hpp"
// ReSharper disable once CppUnusedIncludeDirective
#include "volk.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "vma/vk_mem_alloc.h"
#pragma clang diagnostic pop

#include <array>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <vector>

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

struct SceneChunkMeshVertex {
	uint32_t localPositionMaterial = 0;
	uint32_t faceIndex = 0;
};
static_assert(std::is_standard_layout_v<SceneChunkMeshVertex>);
static_assert(std::is_trivially_copyable_v<SceneChunkMeshVertex>);
static_assert(sizeof(SceneChunkMeshVertex) == 8);
static_assert(offsetof(SceneChunkMeshVertex, localPositionMaterial) == 0);
static_assert(offsetof(SceneChunkMeshVertex, faceIndex) == 4);

struct SceneChunkRenderCache {
	std::vector<SceneChunkMeshVertex> meshVertices;
};

struct PackedSceneChunkDescriptor {
	std::array<int32_t, 4> chunkOrigin{};
	std::array<uint32_t, 4> drawRanges{};
};
static_assert(std::is_standard_layout_v<PackedSceneChunkDescriptor>);
static_assert(std::is_trivially_copyable_v<PackedSceneChunkDescriptor>);
static_assert(sizeof(PackedSceneChunkDescriptor) == 32);
static_assert(offsetof(PackedSceneChunkDescriptor, chunkOrigin) == 0);
static_assert(offsetof(PackedSceneChunkDescriptor, drawRanges) == 16);

struct SceneChunkDrawRange {
	uint32_t firstVertex = 0;
	uint32_t vertexCount = 0;
	uint32_t chunkIndex = 0;
};
static_assert(std::is_standard_layout_v<SceneChunkDrawRange>);
static_assert(std::is_trivially_copyable_v<SceneChunkDrawRange>);
static_assert(sizeof(SceneChunkDrawRange) == 12);
static_assert(offsetof(SceneChunkDrawRange, firstVertex) == 0);
static_assert(offsetof(SceneChunkDrawRange, vertexCount) == 4);
static_assert(offsetof(SceneChunkDrawRange, chunkIndex) == 8);

struct CameraState {
	std::array<float, 3> position{0.0f, 8.0f, 24.0f};
	float yawRadians = 0.0f;
	float pitchRadians = -0.2f;
	float moveSpeed = 10.0f;
	float mouseSensitivity = 0.0025f;
	float verticalFovRadians = 1.0471976f;
	float nearPlane = 0.1f;
	float farPlane = 128.0f;
};

struct GraphicsPushConstants {
	std::array<float, 16> viewProjection{};
};

struct FrameRenderData {
	uint32_t frameIndex = 0;
	VkBuffer packedVertexBuffer = VK_NULL_HANDLE;
	VkBuffer chunkDescriptorBuffer = VK_NULL_HANDLE;
	VkDescriptorSet graphicsDescriptorSet = VK_NULL_HANDLE;
	uint32_t chunkDescriptorCount = 0;
	uint32_t vertexCount = 0;
	uint32_t opaqueVertexCount = 0;
	uint32_t transparentVertexCount = 0;
	const SceneChunkDrawRange *opaqueChunkDrawRanges = nullptr;
	uint32_t opaqueChunkDrawRangeCount = 0;
	const SceneChunkDrawRange *transparentChunkDrawRanges = nullptr;
	uint32_t transparentChunkDrawRangeCount = 0;
	GraphicsPushConstants graphicsPushConstants{};
};

struct DebugStats {
	uint32_t simulationStepsLastFrame = 0;
	uint32_t dirtyChunkCount = 0;
	uint32_t activeChunkCount = 0;
	uint32_t nonAirVoxelCount = 0;
	uint32_t glassVoxelCount = 0;
	uint32_t fluidVoxelCount = 0;
	uint32_t floorVoxelCount = 0;
	uint32_t sceneTriangleCount = 0;
};

struct SceneFrameResources {
	void *packedVertexMappedData = nullptr;
	VkBuffer packedVertexBuffer = VK_NULL_HANDLE;
	VmaAllocation packedVertexAllocation = VK_NULL_HANDLE;
	void *chunkDescriptorMappedData = nullptr;
	VkBuffer chunkDescriptorBuffer = VK_NULL_HANDLE;
	VmaAllocation chunkDescriptorAllocation = VK_NULL_HANDLE;
	VkDescriptorSet graphicsDescriptorSet = VK_NULL_HANDLE;
	uint64_t uploadedSceneVersion = 0;
	uint32_t chunkDescriptorCount = 0;
	uint32_t vertexCount = 0;
	uint32_t opaqueVertexCount = 0;
	uint32_t transparentVertexCount = 0;
};

struct WorldState {
	std::unique_ptr<VoxelWorld> voxelWorld;
};

struct RenderState {
	std::vector<SceneChunkRenderCache> sceneChunkRenderCaches;
	std::vector<SceneChunkMeshVertex> scenePackedVertexPayload;
	std::vector<SceneChunkMeshVertex> transparentPackedVertexScratch;
	std::vector<PackedSceneChunkDescriptor> sceneChunkDescriptors;
	std::vector<SceneChunkDrawRange> opaqueChunkDrawRanges;
	std::vector<SceneChunkDrawRange> transparentChunkDrawRanges;
	std::vector<size_t> pendingChunkRebuildIndices;
	std::vector<size_t> completedChunkRebuildIndices;
	uint32_t sceneVertexCapacity = 0;
	uint32_t sceneUploadVertexCount = 0;
	uint32_t sceneUploadOpaqueVertexCount = 0;
	uint32_t sceneUploadTransparentVertexCount = 0;
	uint64_t sceneUploadVersion = 0;
	bool sceneUploadCacheDirty = true;
	uint32_t sceneTriangleCount = 0;
	void *tracyGraphicsContext = nullptr;
	bool tracyGraphicsContextCalibrated = false;
	VkDescriptorSetLayout graphicsDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool graphicsDescriptorPool = VK_NULL_HANDLE;
	std::array<SceneFrameResources, MAX_FRAMES_IN_FLIGHT> sceneFrameResources{};
	VkImage depthImage = VK_NULL_HANDLE;
	VkImageView depthImageView = VK_NULL_HANDLE;
	VmaAllocation depthAllocation = VK_NULL_HANDLE;
	bool depthImageNeedsInit = false;
	VkPipelineLayout graphicsPipelineLayout = VK_NULL_HANDLE;
	VkPipeline graphicsPipeline = VK_NULL_HANDLE;
	VkPipeline transparentGraphicsPipeline = VK_NULL_HANDLE;
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
};

struct InputState {
	float mouseDeltaX = 0.0f;
	float mouseDeltaY = 0.0f;
};

struct DebugState {
	DebugStats stats{};
	float titleUpdateAccumulatorSeconds = 0.0f;
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
	DebugState debug{};
	CameraState camera{};

	bool shutdownDone = false;

	~AppState();
};

void ShutdownVulkan(AppState *state);

#endif

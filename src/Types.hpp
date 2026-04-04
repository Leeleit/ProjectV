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
#include <memory>
#include <vector>

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

struct ComputeVertex {
	std::array<float, 4> position{};
	std::array<float, 4> color{};
};

struct RenderVertex {
	std::array<float, 3> position{};
	std::array<float, 3> normal{};
	std::array<float, 4> color{};
	float materialKind = 0.0f;
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
};

struct GraphicsPushConstants {
	std::array<float, 16> viewProjection{};
};

struct ComputePushConstants {
	std::array<float, 4> clearColor{0.08f, 0.10f, 0.14f, 1.0f};
	uint32_t triangleCount = 0;
	std::array<uint32_t, 3> padding{};
};

struct AppState {
	SDL_Window *window = nullptr;
	bool windowResized = false;

	VkInstance instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue queue = VK_NULL_HANDLE;
	uint32_t queueFamilyIndex = 0;
	VmaAllocator allocator = VK_NULL_HANDLE;

	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
	VkColorSpaceKHR swapchainColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	VkExtent2D extent = {};
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;

	VkCommandPool commandPool = VK_NULL_HANDLE;

	VkDescriptorPool computeDescriptorPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout computeDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> computeDescriptorSets;
	std::unique_ptr<VoxelWorld> voxelWorld;
	CameraState camera{};
	void *sceneVertexMappedData = nullptr;
	uint32_t sceneVertexCapacity = 0;
	uint32_t sceneVertexCount = 0;
	uint32_t sceneOpaqueVertexCount = 0;
	uint32_t sceneTransparentVertexCount = 0;
	bool sceneVertexBufferDirty = true;
	VkBuffer sceneVertexBuffer = VK_NULL_HANDLE;
	VmaAllocation sceneVertexAllocation = VK_NULL_HANDLE;
	uint32_t sceneTriangleCount = 0;
	VkImage depthImage = VK_NULL_HANDLE;
	VkImageView depthImageView = VK_NULL_HANDLE;
	VmaAllocation depthAllocation = VK_NULL_HANDLE;
	bool depthImageNeedsInit = false;
	VkPipelineLayout graphicsPipelineLayout = VK_NULL_HANDLE;
	VkPipeline graphicsPipeline = VK_NULL_HANDLE;
	VkPipeline transparentGraphicsPipeline = VK_NULL_HANDLE;
	VkImage computeDepthImage = VK_NULL_HANDLE;
	VkImageView computeDepthImageView = VK_NULL_HANDLE;
	VmaAllocation computeDepthAllocation = VK_NULL_HANDLE;
	bool computeDepthImageNeedsInit = false;
	VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
	VkPipeline computePipeline = VK_NULL_HANDLE;

	uint32_t currentFrame = 0;
	std::vector<VkCommandBuffer> commandBuffers;
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	Uint64 lastFrameCounter = 0;
	float deltaTimeSeconds = 0.0f;
	float mouseDeltaX = 0.0f;
	float mouseDeltaY = 0.0f;

	bool shutdownDone = false;

	~AppState();
};

void ShutdownVulkan(AppState *state);

#endif

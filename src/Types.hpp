#ifndef TYPES_HPP
#define TYPES_HPP

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
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
	VkBuffer sceneVertexBuffer = VK_NULL_HANDLE;
	VmaAllocation sceneVertexAllocation = VK_NULL_HANDLE;
	uint32_t sceneTriangleCount = 0;
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

	bool shutdownDone = false;

	~AppState();
};

void ShutdownVulkan(AppState *state);

#endif

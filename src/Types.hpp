#ifndef TYPES_HPP
#define TYPES_HPP

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "volk.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "vma/vk_mem_alloc.h"
#pragma clang diagnostic pop

#include <memory>
#include <vector>

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

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

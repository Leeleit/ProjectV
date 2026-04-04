#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "Types.hpp"
#include "SceneResources.hpp"
#include "VulkanComputePipeline.hpp"

void ShutdownVulkan(AppState *state)
{
	if (!state || state->shutdownDone) {
		return;
	}
	state->shutdownDone = true;

	if (state->device) {
		vkDeviceWaitIdle(state->device);
		DestroyComputePipeline(state);
		DestroySceneResources(state);
	}

	if (state->device) {
		for (const auto iv : state->swapchainImageViews) {
			vkDestroyImageView(state->device, iv, nullptr);
		}
		if (state->swapchain) {
			vkDestroySwapchainKHR(state->device, state->swapchain, nullptr);
		}
	}

	if (state->device) {
		for (const auto sem : state->imageAvailableSemaphores) {
			vkDestroySemaphore(state->device, sem, nullptr);
		}
		for (const auto sem : state->renderFinishedSemaphores) {
			vkDestroySemaphore(state->device, sem, nullptr);
		}
		for (const auto fence : state->inFlightFences) {
			vkDestroyFence(state->device, fence, nullptr);
		}

		if (state->commandPool) {
			vkDestroyCommandPool(state->device, state->commandPool, nullptr);
		}
	}

	if (state->allocator) {
		vmaDestroyAllocator(state->allocator);
		state->allocator = VK_NULL_HANDLE;
	}
	if (state->device) {
		vkDestroyDevice(state->device, nullptr);
		state->device = VK_NULL_HANDLE;
	}
	if (state->surface && state->instance) {
		vkDestroySurfaceKHR(state->instance, state->surface, nullptr);
		state->surface = VK_NULL_HANDLE;
	}

	if (state->debugMessenger && state->instance) {
		vkDestroyDebugUtilsMessengerEXT(state->instance, state->debugMessenger, nullptr);
		state->debugMessenger = VK_NULL_HANDLE;
	}

	if (state->window) {
		SDL_DestroyWindow(state->window);
		state->window = nullptr;
	}
	if (state->instance) {
		vkDestroyInstance(state->instance, nullptr);
		state->instance = VK_NULL_HANDLE;
	}
}

AppState::~AppState()
{
	ShutdownVulkan(this);
}

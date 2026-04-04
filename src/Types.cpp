#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "Types.hpp"
#include "SceneResources.hpp"
#include "VoxelWorld.hpp"
#include "VulkanGraphicsPipeline.hpp"

void ShutdownVulkan(AppState *state)
{
	if (!state || state->shutdownDone) {
		return;
	}
	state->shutdownDone = true;

	if (state->context.device) {
		vkDeviceWaitIdle(state->context.device);
		DestroyGraphicsPipeline(&state->context, &state->render);
		DestroySceneResources(&state->context, &state->render);
	}

	DestroyVoxelLabWorld(state);

	if (state->context.device) {
		for (const auto iv : state->swapchain.imageViews) {
			vkDestroyImageView(state->context.device, iv, nullptr);
		}
		if (state->swapchain.handle) {
			vkDestroySwapchainKHR(state->context.device, state->swapchain.handle, nullptr);
		}
	}

	if (state->context.device) {
		for (const auto sem : state->frame.imageAvailableSemaphores) {
			vkDestroySemaphore(state->context.device, sem, nullptr);
		}
		for (const auto sem : state->frame.renderFinishedSemaphores) {
			vkDestroySemaphore(state->context.device, sem, nullptr);
		}
		for (const auto fence : state->frame.inFlightFences) {
			vkDestroyFence(state->context.device, fence, nullptr);
		}

		if (state->context.commandPool) {
			vkDestroyCommandPool(state->context.device, state->context.commandPool, nullptr);
		}
	}

	if (state->context.allocator) {
		vmaDestroyAllocator(state->context.allocator);
		state->context.allocator = VK_NULL_HANDLE;
	}
	if (state->context.device) {
		vkDestroyDevice(state->context.device, nullptr);
		state->context.device = VK_NULL_HANDLE;
	}
	if (state->context.surface && state->context.instance) {
		vkDestroySurfaceKHR(state->context.instance, state->context.surface, nullptr);
		state->context.surface = VK_NULL_HANDLE;
	}

	if (state->context.debugMessenger && state->context.instance) {
		vkDestroyDebugUtilsMessengerEXT(state->context.instance, state->context.debugMessenger, nullptr);
		state->context.debugMessenger = VK_NULL_HANDLE;
	}

	if (state->platform.window) {
		SDL_DestroyWindow(state->platform.window);
		state->platform.window = nullptr;
	}
	if (state->context.instance) {
		vkDestroyInstance(state->context.instance, nullptr);
		state->context.instance = VK_NULL_HANDLE;
	}
}

AppState::~AppState()
{
	ShutdownVulkan(this);
}

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "Types.hpp"
#include "VulkanPipeline.hpp"

static void DestroyMeshGpu(const VmaAllocator allocator, MeshGpu &mesh)
{
	if (allocator && mesh.vertexBuffer) {
		vmaDestroyBuffer(allocator, mesh.vertexBuffer, mesh.vertexAllocation);
		mesh.vertexBuffer = VK_NULL_HANDLE;
		mesh.vertexAllocation = VK_NULL_HANDLE;
		mesh.vertexCount = 0;
	}

	if (allocator && mesh.indexBuffer) {
		vmaDestroyBuffer(allocator, mesh.indexBuffer, mesh.indexAllocation);
		mesh.indexBuffer = VK_NULL_HANDLE;
		mesh.indexAllocation = VK_NULL_HANDLE;
		mesh.indexCount = 0;
	}
}

void ShutdownVulkan(AppState *state)
{
	if (!state || state->shutdownDone) {
		return;
	}
	state->shutdownDone = true;

	if (state->device) {
		vkDeviceWaitIdle(state->device);
		DestroyGraphicsPipeline(state);
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

	DestroyMeshGpu(state->allocator, state->sceneMesh);

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

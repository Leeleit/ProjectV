#define VMA_IMPLEMENTATION // Генерация определений для VMA
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "Types.hpp"

AppState::~AppState()
{ // Деструктор (Замена функции cleanupAppState)
	if (device)
		vkDeviceWaitIdle(device);

	// Очищаем swapchain
	if (device) {
		for (const auto iv : swapchainImageViews)
			vkDestroyImageView(device, iv, nullptr);
		if (swapchain)
			vkDestroySwapchainKHR(device, swapchain, nullptr);
	}

	// Удаляем синхронизацию в цикле
	if (device) {
		for (const auto sem : imageAvailableSemaphores)
			vkDestroySemaphore(device, sem, nullptr);
		for (const auto sem : renderFinishedSemaphores)
			vkDestroySemaphore(device, sem, nullptr);
		for (const auto fence : inFlightFences)
			vkDestroyFence(device, fence, nullptr);

		if (commandPool)
			vkDestroyCommandPool(device, commandPool, nullptr);
	}

	if (allocator)
		vmaDestroyAllocator(allocator);
	if (device)
		vkDestroyDevice(device, nullptr);
	if (surface && instance)
		vkDestroySurfaceKHR(instance, surface, nullptr);

	if (debugMessenger && instance)
		vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

	if (window)
		SDL_DestroyWindow(window);
	if (instance)
		vkDestroyInstance(instance, nullptr);
}

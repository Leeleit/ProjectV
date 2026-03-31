#include "VulkanSwapchain.hpp"

#include "VulkanPipeline.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace {
struct SwapchainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

bool QuerySwapchainSupport(
	const VkPhysicalDevice physicalDevice,
	const VkSurfaceKHR surface,
	SwapchainSupportDetails *outDetails)
{
	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
			physicalDevice, surface, &outDetails->capabilities) != VK_SUCCESS) {
		return false;
	}

	uint32_t formatCount = 0;
	if (vkGetPhysicalDeviceSurfaceFormatsKHR(
			physicalDevice, surface, &formatCount, nullptr) != VK_SUCCESS) {
		return false;
	}
	outDetails->formats.resize(formatCount);
	if (formatCount > 0 &&
		vkGetPhysicalDeviceSurfaceFormatsKHR(
			physicalDevice, surface, &formatCount, outDetails->formats.data()) != VK_SUCCESS) {
		return false;
	}

	uint32_t presentModeCount = 0;
	if (vkGetPhysicalDeviceSurfacePresentModesKHR(
			physicalDevice, surface, &presentModeCount, nullptr) != VK_SUCCESS) {
		return false;
	}
	outDetails->presentModes.resize(presentModeCount);
	if (presentModeCount > 0 &&
		vkGetPhysicalDeviceSurfacePresentModesKHR(
			physicalDevice, surface, &presentModeCount, outDetails->presentModes.data()) != VK_SUCCESS) {
		return false;
	}

	return !outDetails->formats.empty() && !outDetails->presentModes.empty();
}

VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats)
{
	for (const auto &fmt : formats) {
		if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
			fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return fmt;
		}
	}
	return formats.front();
}

VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR> &presentModes)
{
	for (const VkPresentModeKHR mode : presentModes) {
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return mode;
		}
	}
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR &caps, SDL_Window *window)
{
	if (caps.currentExtent.width != UINT32_MAX) {
		return caps.currentExtent;
	}

	int w = 0;
	int h = 0;
	SDL_GetWindowSizeInPixels(window, &w, &h);

	VkExtent2D extent{
		static_cast<uint32_t>(w),
		static_cast<uint32_t>(h)};

	extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
	extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
	return extent;
}

bool CreateOrRecreateSwapchain(AppState *state)
{
	SwapchainSupportDetails support;
	if (!QuerySwapchainSupport(state->physicalDevice, state->surface, &support)) {
		SDL_Log("QuerySwapchainSupport failed");
		return false;
	}

	const auto [format, colorSpace] = ChooseSurfaceFormat(support.formats);
	const VkPresentModeKHR chosenPresentMode = ChoosePresentMode(support.presentModes);
	const VkExtent2D chosenExtent = ChooseExtent(support.capabilities, state->window);

	if (chosenExtent.width == 0 || chosenExtent.height == 0) {
		state->extent = chosenExtent;
		return true;
	}

	uint32_t imageCount = std::max(2u, support.capabilities.minImageCount);
	if (support.capabilities.maxImageCount > 0 &&
		imageCount > support.capabilities.maxImageCount) {
		imageCount = support.capabilities.maxImageCount;
	}

	VkSwapchainKHR oldSwapchain = state->swapchain;
	VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = state->surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = format;
	createInfo.imageColorSpace = colorSpace;
	createInfo.imageExtent = chosenExtent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.preTransform = support.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = chosenPresentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = oldSwapchain;

	if (vkCreateSwapchainKHR(state->device, &createInfo, nullptr, &newSwapchain) != VK_SUCCESS) {
		SDL_Log("vkCreateSwapchainKHR failed");
		return false;
	}

	uint32_t actualImageCount = 0;
	if (vkGetSwapchainImagesKHR(state->device, newSwapchain, &actualImageCount, nullptr) != VK_SUCCESS ||
		actualImageCount == 0) {
		vkDestroySwapchainKHR(state->device, newSwapchain, nullptr);
		SDL_Log("vkGetSwapchainImagesKHR failed");
		return false;
	}

	std::vector<VkImage> newImages(actualImageCount);
	if (vkGetSwapchainImagesKHR(state->device, newSwapchain, &actualImageCount, newImages.data()) != VK_SUCCESS) {
		vkDestroySwapchainKHR(state->device, newSwapchain, nullptr);
		SDL_Log("vkGetSwapchainImagesKHR failed");
		return false;
	}

	std::vector<VkImageView> newViews(actualImageCount, VK_NULL_HANDLE);
	for (uint32_t i = 0; i < actualImageCount; ++i) {
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = newImages[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(state->device, &viewInfo, nullptr, &newViews[i]) != VK_SUCCESS) {
			for (VkImageView iv : newViews) {
				if (iv) {
					vkDestroyImageView(state->device, iv, nullptr);
				}
			}
			vkDestroySwapchainKHR(state->device, newSwapchain, nullptr);
			SDL_Log("vkCreateImageView failed");
			return false;
		}
	}

	for (VkImageView iv : state->swapchainImageViews) {
		if (iv) {
			vkDestroyImageView(state->device, iv, nullptr);
		}
	}
	state->swapchainImageViews.clear();
	state->swapchainImages.clear();

	if (oldSwapchain) {
		vkDestroySwapchainKHR(state->device, oldSwapchain, nullptr);
	}

	state->swapchain = newSwapchain;
	state->swapchainFormat = format;
	state->swapchainColorSpace = colorSpace;
	state->extent = chosenExtent;
	state->swapchainImages = std::move(newImages);
	state->swapchainImageViews = std::move(newViews);

	return true;
}
} // namespace

bool RecreateSwapchain(AppState *state)
{
	// Если окно свернуто, real size становится нулевым, и пересоздавать swapchain бессмысленно.
	int w = 0;
	int h = 0;
	SDL_GetWindowSizeInPixels(state->window, &w, &h);

	if (w == 0 || h == 0) {
		// Помечаем swapchain как "пауза", а реальное пересоздание отложим до восстановления окна.
		state->extent = {0, 0};
		return true;
	}

	// Перед пересозданием ждем, пока GPU закончит работу со старой цепочкой изображений.
	vkDeviceWaitIdle(state->device);

	// Запоминаем старый формат, чтобы понять, нужно ли перестраивать pipeline.
	const VkFormat oldFormat = state->swapchainFormat;
	const bool hadPipeline =
		state->graphicsPipeline != VK_NULL_HANDLE ||
		state->pipelineLayout != VK_NULL_HANDLE;

	if (!CreateOrRecreateSwapchain(state)) {
		return false;
	}

	// На первом старте формат часто меняется с UNDEFINED, но pipeline ещё не существует.
	// Пересобирать его имеет смысл только если он уже был создан ранее.
	if (hadPipeline && state->swapchainFormat != oldFormat) {
		DestroyGraphicsPipeline(state);
		if (!CreateGraphicsPipeline(state)) {
			SDL_Log("CreateGraphicsPipeline failed after format change");
			return false;
		}
	}

	return true;
}

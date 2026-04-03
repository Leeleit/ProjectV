#include "VulkanSwapchain.hpp"

#include "VulkanComputePipeline.hpp"

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

bool SupportsStorageImage(const VkPhysicalDevice physicalDevice, const VkFormat format)
{
	VkFormatProperties props{};
	vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
	return (props.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0;
}

VkSurfaceFormatKHR ChooseSurfaceFormat(
	const VkPhysicalDevice physicalDevice,
	const std::vector<VkSurfaceFormatKHR> &formats)
{
	for (const auto &fmt : formats) {
		if (fmt.format == VK_FORMAT_R8G8B8A8_UNORM &&
			fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
			SupportsStorageImage(physicalDevice, fmt.format)) {
			return fmt;
		}
	}

	for (const auto &fmt : formats) {
		if (fmt.format == VK_FORMAT_R8G8B8A8_UNORM &&
			SupportsStorageImage(physicalDevice, fmt.format)) {
			return fmt;
		}
	}

	return {};
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

	if ((support.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT) == 0) {
		SDL_Log("Surface does not support VK_IMAGE_USAGE_STORAGE_BIT for swapchain images");
		return false;
	}

	const auto [format, colorSpace] = ChooseSurfaceFormat(state->physicalDevice, support.formats);
	if (format == VK_FORMAT_UNDEFINED) {
		SDL_Log("No swapchain surface format supports storage image usage");
		return false;
	}

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

	VkSwapchainCreateInfoKHR createInfo{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext = nullptr,
		.flags = 0,
		.surface = state->surface,
		.minImageCount = imageCount,
		.imageFormat = format,
		.imageColorSpace = colorSpace,
		.imageExtent = chosenExtent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_STORAGE_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.preTransform = support.capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = chosenPresentMode,
		.clipped = VK_TRUE,
		.oldSwapchain = oldSwapchain,
	};

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
	int w = 0;
	int h = 0;
	SDL_GetWindowSizeInPixels(state->window, &w, &h);

	if (w == 0 || h == 0) {
		state->extent = {0, 0};
		return true;
	}

	vkDeviceWaitIdle(state->device);

	const bool hadComputePipeline =
		state->computePipeline != VK_NULL_HANDLE ||
		state->computePipelineLayout != VK_NULL_HANDLE ||
		!state->computeDescriptorSets.empty();

	if (hadComputePipeline) {
		DestroyComputePipeline(state);
	}

	if (!CreateOrRecreateSwapchain(state)) {
		return false;
	}

	if (hadComputePipeline) {
		if (!CreateComputePipeline(state)) {
			SDL_Log("CreateComputePipeline failed after swapchain recreation");
			return false;
		}
	}

	return true;
}

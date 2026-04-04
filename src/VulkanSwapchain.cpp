#include "VulkanSwapchain.hpp"

#include "VulkanDebug.hpp"
#include "VulkanGraphicsPipeline.hpp"

#include <algorithm>
#include <cstdio>
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
		if (fmt.format == VK_FORMAT_B8G8R8A8_UNORM &&
			fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return fmt;
		}
	}

	for (const auto &fmt : formats) {
		if (fmt.format == VK_FORMAT_R8G8B8A8_UNORM &&
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
		static_cast<uint32_t>(h),
	};

	extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
	extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
	return extent;
}

bool CreateOrRecreateSwapchain(
	PlatformState *platform,
	VulkanContextState *context,
	SwapchainState *swapchain)
{
	SwapchainSupportDetails support;
	if (!QuerySwapchainSupport(context->physicalDevice, context->surface, &support)) {
		SDL_Log("QuerySwapchainSupport failed");
		return false;
	}

	if ((support.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0) {
		SDL_Log("Surface does not support VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT for swapchain images");
		return false;
	}

	const auto [format, colorSpace] = ChooseSurfaceFormat(support.formats);
	const VkPresentModeKHR chosenPresentMode = ChoosePresentMode(support.presentModes);
	const VkExtent2D chosenExtent = ChooseExtent(support.capabilities, platform->window);

	if (chosenExtent.width == 0 || chosenExtent.height == 0) {
		swapchain->extent = chosenExtent;
		return true;
	}

	uint32_t imageCount = std::max(2u, support.capabilities.minImageCount);
	if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
		imageCount = support.capabilities.maxImageCount;
	}

	VkSwapchainKHR oldSwapchain = swapchain->handle;
	VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
	const VkSwapchainCreateInfoKHR createInfo{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext = nullptr,
		.flags = 0,
		.surface = context->surface,
		.minImageCount = imageCount,
		.imageFormat = format,
		.imageColorSpace = colorSpace,
		.imageExtent = chosenExtent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.preTransform = support.capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = chosenPresentMode,
		.clipped = VK_TRUE,
		.oldSwapchain = oldSwapchain,
	};

	if (vkCreateSwapchainKHR(context->device, &createInfo, nullptr, &newSwapchain) != VK_SUCCESS) {
		SDL_Log("vkCreateSwapchainKHR failed");
		return false;
	}

	uint32_t actualImageCount = 0;
	if (vkGetSwapchainImagesKHR(context->device, newSwapchain, &actualImageCount, nullptr) != VK_SUCCESS ||
		actualImageCount == 0) {
		vkDestroySwapchainKHR(context->device, newSwapchain, nullptr);
		SDL_Log("vkGetSwapchainImagesKHR failed");
		return false;
	}

	std::vector<VkImage> newImages(actualImageCount);
	if (vkGetSwapchainImagesKHR(context->device, newSwapchain, &actualImageCount, newImages.data()) != VK_SUCCESS) {
		vkDestroySwapchainKHR(context->device, newSwapchain, nullptr);
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

		if (vkCreateImageView(context->device, &viewInfo, nullptr, &newViews[i]) != VK_SUCCESS) {
			for (VkImageView imageView : newViews) {
				if (imageView) {
					vkDestroyImageView(context->device, imageView, nullptr);
				}
			}
			vkDestroySwapchainKHR(context->device, newSwapchain, nullptr);
			SDL_Log("vkCreateImageView failed");
			return false;
		}
	}

	for (VkImageView imageView : swapchain->imageViews) {
		if (imageView) {
			vkDestroyImageView(context->device, imageView, nullptr);
		}
	}
	swapchain->imageViews.clear();
	swapchain->images.clear();

	if (oldSwapchain) {
		vkDestroySwapchainKHR(context->device, oldSwapchain, nullptr);
	}

	swapchain->handle = newSwapchain;
	swapchain->format = format;
	swapchain->colorSpace = colorSpace;
	swapchain->extent = chosenExtent;
	swapchain->images = std::move(newImages);
	swapchain->imageViews = std::move(newViews);

	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(swapchain->handle),
		VK_OBJECT_TYPE_SWAPCHAIN_KHR,
		"MainSwapchain");
	for (size_t i = 0; i < swapchain->imageViews.size(); ++i) {
		char imageName[64]{};
		std::snprintf(imageName, sizeof(imageName), "SwapchainImage[%zu]", i);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(swapchain->images[i]),
			VK_OBJECT_TYPE_IMAGE,
			imageName);

		char viewName[64]{};
		std::snprintf(viewName, sizeof(viewName), "SwapchainImageView[%zu]", i);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(swapchain->imageViews[i]),
			VK_OBJECT_TYPE_IMAGE_VIEW,
			viewName);
	}
	return true;
}
} // namespace

bool RecreateSwapchain(
	PlatformState *platform,
	VulkanContextState *context,
	SwapchainState *swapchain,
	RenderState *render)
{
	int w = 0;
	int h = 0;
	SDL_GetWindowSizeInPixels(platform->window, &w, &h);

	if (w == 0 || h == 0) {
		swapchain->extent = {0, 0};
		return true;
	}

	vkDeviceWaitIdle(context->device);

	const bool hadGraphicsPipeline =
		render->graphicsPipeline != VK_NULL_HANDLE ||
		render->graphicsPipelineLayout != VK_NULL_HANDLE;
	if (hadGraphicsPipeline) {
		DestroyGraphicsPipeline(context, render);
	}

	if (!CreateOrRecreateSwapchain(platform, context, swapchain)) {
		return false;
	}

	if (hadGraphicsPipeline) {
		if (!CreateGraphicsPipeline(context, swapchain, render)) {
			SDL_Log("CreateGraphicsPipeline failed after swapchain recreation");
			return false;
		}
	}

	return true;
}

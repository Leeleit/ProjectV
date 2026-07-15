#include "render/vulkan/VulkanSwapchain.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/EnvUtils.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"
#include "render/AaPass.hpp"
#include "render/AntialiasingSettings.hpp"
#include "render/PostFx.hpp"
#include "render/RayTracedShadows.hpp"
#include "render/vulkan/VulkanDebug.hpp"
#include "render/vulkan/VulkanGraphicsPipeline.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
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
	PV_PROFILE_ZONE_N("QuerySwapchainSupport");
	const VkResult capabilitiesResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		physicalDevice, surface, &outDetails->capabilities);
	if (capabilitiesResult != VK_SUCCESS) {
		runtime::LogVkFailure("QuerySwapchainSupport.vkGetPhysicalDeviceSurfaceCapabilitiesKHR", capabilitiesResult);
		return false;
	}

	uint32_t formatCount = 0;
	const VkResult formatCountResult =
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
	if (formatCountResult != VK_SUCCESS) {
		runtime::LogVkFailure("QuerySwapchainSupport.vkGetPhysicalDeviceSurfaceFormatsKHR(count)", formatCountResult);
		return false;
	}
	outDetails->formats.resize(formatCount);
	if (formatCount > 0) {
		const VkResult formatsResult =
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, outDetails->formats.data());
		if (formatsResult != VK_SUCCESS) {
			runtime::LogVkFailure("QuerySwapchainSupport.vkGetPhysicalDeviceSurfaceFormatsKHR(data)", formatsResult);
			return false;
		}
	}

	uint32_t presentModeCount = 0;
	const VkResult presentModeCountResult =
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
	if (presentModeCountResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"QuerySwapchainSupport.vkGetPhysicalDeviceSurfacePresentModesKHR(count)",
			presentModeCountResult);
		return false;
	}
	outDetails->presentModes.resize(presentModeCount);
	if (presentModeCount > 0) {
		const VkResult presentModesResult =
			vkGetPhysicalDeviceSurfacePresentModesKHR(
				physicalDevice,
				surface,
				&presentModeCount,
				outDetails->presentModes.data());
		if (presentModesResult != VK_SUCCESS) {
			runtime::LogVkFailure(
				"QuerySwapchainSupport.vkGetPhysicalDeviceSurfacePresentModesKHR(data)",
				presentModesResult);
			return false;
		}
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

using projectv::present_mode::g_active;

const char *PresentModeName(const VkPresentModeKHR mode)
{
	switch (mode) {
	case VK_PRESENT_MODE_IMMEDIATE_KHR:
		return "IMMEDIATE";
	case VK_PRESENT_MODE_MAILBOX_KHR:
		return "MAILBOX";
	case VK_PRESENT_MODE_FIFO_KHR:
		return "FIFO";
	default:
		return "UNKNOWN";
	}
}

VkPresentModeKHR PickBestAvailablePresentMode(
	const std::vector<VkPresentModeKHR> &presentModes,
	const VkPresentModeKHR preferred)
{
	const std::array priority{
		preferred,
		VK_PRESENT_MODE_MAILBOX_KHR,
		VK_PRESENT_MODE_FIFO_KHR,
	};
	for (const VkPresentModeKHR candidate : priority) {
		if (std::ranges::find(presentModes, candidate) != presentModes.end()) {
			return candidate;
		}
	}
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR> &presentModes)
{
	if (g_active == VK_PRESENT_MODE_IMMEDIATE_KHR ||
		g_active == VK_PRESENT_MODE_MAILBOX_KHR) {
		return PickBestAvailablePresentMode(presentModes, g_active);
	}

	for (const VkPresentModeKHR mode : presentModes) {
		if (mode == VK_PRESENT_MODE_FIFO_KHR) {
			return mode;
		}
	}
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
} // namespace

void ApplyPreferredPresentModeFromEnvironment()
{
	static bool applied = false;
	if (applied) {
		return;
	}
	applied = true;
	const char *const value = projectv::core::GetEnvVar("PROJECTV_PRESENT_MODE");
	if (value == nullptr || value[0] == '\0') {
		return;
	}
	using projectv::present_mode::g_active;
	VkPresentModeKHR preferred = VK_PRESENT_MODE_FIFO_KHR;
	const char *label = "FIFO";
	auto equals = [](const char *lhs, const char *rhs) {
		if (lhs == nullptr || rhs == nullptr) {
			return false;
		}
		while (*lhs != '\0' && *rhs != '\0') {
			const unsigned char left = static_cast<unsigned char>(*lhs);
			const unsigned char right = static_cast<unsigned char>(*rhs);
			if (std::tolower(left) != std::tolower(right)) {
				return false;
			}
			++lhs;
			++rhs;
		}
		return *lhs == '\0' && *rhs == '\0';
	};
	if (equals(value, "MAILBOX")) {
		preferred = VK_PRESENT_MODE_MAILBOX_KHR;
		label = "MAILBOX";
	} else if (equals(value, "IMMEDIATE")) {
		preferred = VK_PRESENT_MODE_IMMEDIATE_KHR;
		label = "IMMEDIATE";
	} else if (equals(value, "FIFO")) {
		preferred = VK_PRESENT_MODE_FIFO_KHR;
		label = "FIFO";
	} else {
		std::fprintf(
			stderr,
			"[ProjectV][Present] ignoring unknown PROJECTV_PRESENT_MODE='%s' (use FIFO|MAILBOX|IMMEDIATE)\n",
			value);
		return;
	}
	g_active = preferred;
	std::fprintf(stderr, "[ProjectV][Present] preferred mode from env: %s\n", label);
}

bool CreateOrRecreateSwapchain(
	PlatformState *platform,
	VulkanContextState *context,
	SwapchainState *swapchain)
{
	PV_PROFILE_ZONE_N("CreateOrRecreateSwapchain");
	SwapchainSupportDetails support;
	if (!QuerySwapchainSupport(context->physicalDevice, context->surface, &support)) {
		runtime::LogRuntimeFailure(
			"Swapchain",
			"CreateOrRecreateSwapchain.QuerySwapchainSupport",
			"QuerySwapchainSupport returned false");
		return false;
	}

	if ((support.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0) {
		runtime::LogRuntimeFailure(
			"Swapchain",
			"CreateOrRecreateSwapchain.SurfaceUsage",
			"surface does not support VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT for swapchain images");
		return false;
	}
	const bool supportsTransferSrc =
		(support.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;

	const auto [format, colorSpace] = ChooseSurfaceFormat(support.formats);
	ApplyPreferredPresentModeFromEnvironment();
	(void)BuildPresentModeCycle(support.presentModes);
	const VkPresentModeKHR chosenPresentMode = ChoosePresentMode(support.presentModes);
	std::fprintf(
		stderr,
		"[ProjectV][Present] swapchain presentMode=%s (preferred=%s)\n",
		PresentModeName(chosenPresentMode),
		PresentModeName(g_active));
	const VkExtent2D chosenExtent = ChooseExtent(support.capabilities, platform->window);

	if (chosenExtent.width == 0 || chosenExtent.height == 0) {
		swapchain->format = format;
		swapchain->extent = chosenExtent;
		swapchain->supportsTransferSrc = false;
		return true;
	}

	uint32_t imageCount = std::max(2u, support.capabilities.minImageCount);
	if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
		imageCount = support.capabilities.maxImageCount;
	}

	VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (supportsTransferSrc) {
		imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}
	if ((support.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0) {
		imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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
		.imageUsage = imageUsage,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.preTransform = support.capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = chosenPresentMode,
		.clipped = VK_TRUE,
		.oldSwapchain = oldSwapchain,
	};

	{
		PV_PROFILE_ZONE_N("CreateOrRecreateSwapchain.CreateSwapchain");
		const VkResult createSwapchainResult = vkCreateSwapchainKHR(context->device, &createInfo, nullptr, &newSwapchain);
		if (createSwapchainResult != VK_SUCCESS) {
			runtime::LogVkFailure("CreateOrRecreateSwapchain.vkCreateSwapchainKHR", createSwapchainResult);
			return false;
		}
	}

	uint32_t actualImageCount = 0;
	{
		PV_PROFILE_ZONE_N("CreateOrRecreateSwapchain.QueryImages");
		const VkResult imageCountResult = vkGetSwapchainImagesKHR(context->device, newSwapchain, &actualImageCount, nullptr);
		if (imageCountResult != VK_SUCCESS || actualImageCount == 0) {
			vkDestroySwapchainKHR(context->device, newSwapchain, nullptr);
			if (imageCountResult != VK_SUCCESS) {
				runtime::LogVkFailure("CreateOrRecreateSwapchain.vkGetSwapchainImagesKHR(count)", imageCountResult);
				return false;
			}
			return false;
		}
	}

	std::vector<VkImage> newImages(actualImageCount);
	{
		PV_PROFILE_ZONE_N("CreateOrRecreateSwapchain.FetchImages");
		const VkResult fetchImagesResult =
			vkGetSwapchainImagesKHR(context->device, newSwapchain, &actualImageCount, newImages.data());
		if (fetchImagesResult != VK_SUCCESS) {
			vkDestroySwapchainKHR(context->device, newSwapchain, nullptr);
			runtime::LogVkFailure("CreateOrRecreateSwapchain.vkGetSwapchainImagesKHR(data)", fetchImagesResult);
			return false;
		}
	}

	std::vector<VkImageView> newViews(actualImageCount, VK_NULL_HANDLE);
	{
		PV_PROFILE_ZONE_N("CreateOrRecreateSwapchain.CreateImageViews");
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

			const VkResult createImageViewResult = vkCreateImageView(context->device, &viewInfo, nullptr, &newViews[i]);
			if (createImageViewResult != VK_SUCCESS) {
				for (VkImageView imageView : newViews) {
					if (imageView) {
						vkDestroyImageView(context->device, imageView, nullptr);
					}
				}
				vkDestroySwapchainKHR(context->device, newSwapchain, nullptr);
				runtime::LogVkFailure("CreateOrRecreateSwapchain.vkCreateImageView", createImageViewResult);
				return false;
			}
		}
	}

	{
		PV_PROFILE_ZONE_N("CreateOrRecreateSwapchain.DestroyOldViews");
		for (VkImageView imageView : swapchain->imageViews) {
			if (imageView) {
				vkDestroyImageView(context->device, imageView, nullptr);
			}
		}
		swapchain->imageViews.clear();
		swapchain->images.clear();
	}

	if (oldSwapchain) {
		PV_PROFILE_ZONE_N("CreateOrRecreateSwapchain.DestroyOldSwapchain");
		vkDestroySwapchainKHR(context->device, oldSwapchain, nullptr);
	}

	swapchain->handle = newSwapchain;
	swapchain->format = format;
	swapchain->colorSpace = colorSpace;
	swapchain->extent = chosenExtent;
	swapchain->supportsTransferSrc = supportsTransferSrc;
	swapchain->images = std::move(newImages);
	swapchain->imageViews = std::move(newViews);
	swapchain->nextPresentId = 1u;
	swapchain->presentWaitLogged = false;

	{
		PV_PROFILE_ZONE_N("CreateOrRecreateSwapchain.CreatePerImageSemaphores");
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		std::vector<VkSemaphore> newSubmitSemaphores(actualImageCount, VK_NULL_HANDLE);
		bool perImageSemaphoresOk = true;
		for (uint32_t i = 0; i < actualImageCount; ++i) {
			const VkResult createSemaphoreResult = vkCreateSemaphore(
				context->device, &semaphoreInfo, nullptr, &newSubmitSemaphores[i]);
			if (createSemaphoreResult != VK_SUCCESS) {
				runtime::LogVkFailure(
					"CreateOrRecreateSwapchain.vkCreateSemaphore(submitSemaphore)",
					createSemaphoreResult);
				perImageSemaphoresOk = false;
				break;
			}
		}
		if (!perImageSemaphoresOk) {
			for (VkSemaphore semaphore : newSubmitSemaphores) {
				if (semaphore != VK_NULL_HANDLE) {
					vkDestroySemaphore(context->device, semaphore, nullptr);
				}
			}

			runtime::LogRuntimeFailure(
				"Swapchain",
				"CreateOrRecreateSwapchain.PerImageSemaphores",
				"failed to create per-swapchain-image submit semaphores");
			return false;
		}

		for (VkSemaphore semaphore : swapchain->submitSemaphores) {
			if (semaphore != VK_NULL_HANDLE) {
				vkDestroySemaphore(context->device, semaphore, nullptr);
			}
		}
		swapchain->submitSemaphores = std::move(newSubmitSemaphores);
	}

	{
		PV_PROFILE_ZONE_N("CreateOrRecreateSwapchain.NameObjects");
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
	}
	return true;
}

bool RecreateSwapchain(
	PlatformState *platform,
	VulkanContextState *context,
	SwapchainState *swapchain,
	RenderState *render)
{
	PV_PROFILE_ZONE_N("RecreateSwapchain");
	int w = 0;
	int h = 0;
	SDL_GetWindowSizeInPixels(platform->window, &w, &h);

	if (w == 0 || h == 0) {
		swapchain->extent = {0, 0};
		swapchain->supportsTransferSrc = false;
		return true;
	}

	{
		PV_PROFILE_ZONE_N("RecreateSwapchain.WaitDeviceIdle");
		const VkResult waitIdleResult = vkDeviceWaitIdle(context->device);
		if (waitIdleResult != VK_SUCCESS) {
			runtime::LogVkFailure("RecreateSwapchain.vkDeviceWaitIdle", waitIdleResult);
			return false;
		}
	}

	const bool hadSwapchainResources =
		render->depthImage != VK_NULL_HANDLE ||
		render->screenshotReadbackBuffer != VK_NULL_HANDLE;
	{
		PV_PROFILE_ZONE_N("RecreateSwapchain.CreateSwapchainResources");
		if (!CreateOrRecreateSwapchain(platform, context, swapchain)) {
			return false;
		}
	}

	if (swapchain->extent.width == 0 || swapchain->extent.height == 0) {
		return true;
	}

	if (hadSwapchainResources) {
		PV_PROFILE_ZONE_N("RecreateSwapchain.DestroySwapchainResources");
		DestroyDepthResources(context, render);
		DestroyScreenshotReadbackResources(context, render);
		projectv::render::DestroyAaPassResources(context, render);
		projectv::render::DestroyAaSceneTargets(context, render);
		projectv::render::DestroyPostFxResources(context, render);
		projectv::render::DestroyHizBuffer(context, render->hizBuffer);
		projectv::render::DestroyHizCullingPipeline(context, render);
		render->hizBufferNeedsInit = true;
		render->hzbMaskValid = false; // ForceAll next frame — old mask vs new Hi-Z = chunk pop/flicker
		render->hzbPrevCameraForwardValid = false;
		render->hzbUnifiedVisibilityWords.clear();
		render->hzbUnifiedCullSerial = 0u;
		render->hzbCullSerialCounter = 0u;
		render->hzbSlotCullSerial = {};
	}

	// Must set before CreateDepthResources — it otherwise keeps the previous internal
	// extent and depth/color diverge on render-scale change (DEVICE_LOST).
	const VkExtent2D internalExtent =
		projectv::render::ComputeInternalRenderExtent(swapchain->extent, render->renderScaleMode);
	render->internalRenderExtent = internalExtent;
	projectv::render::ResolveMsaaSampleCount(context, render);

	if (!CreateDepthResources(context, swapchain, render)) {
		runtime::LogRuntimeFailure(
			"Swapchain",
			"RecreateSwapchain.CreateDepthResources",
			"CreateDepthResources returned false after swapchain recreation");
		return false;
	}

	if (projectv::render::IsHzbCullingEnabled() &&
		!projectv::render::CreateHizBuffer(
			context,
			internalExtent.width,
			internalExtent.height,
			render->hizBuffer)) {
		runtime::LogRuntimeFailure(
			"Swapchain",
			"RecreateSwapchain.CreateHizBuffer",
			"CreateHizBuffer returned false after swapchain recreation");
		return false;
	}
	if (projectv::render::IsHzbCullingEnabled()) {
		render->hizBufferNeedsInit = true;
	}

	if (projectv::render::IsHzbCullingEnabled() &&
		!projectv::render::CreateHizCullingPipeline(context, render)) {
		runtime::LogRuntimeFailure(
			"Swapchain",
			"RecreateSwapchain.CreateHizCullingPipeline",
			"CreateHizCullingPipeline returned false after swapchain recreation");
		return false;
	}
	if (!CreateScreenshotReadbackResources(context, swapchain, render)) {
		runtime::LogRuntimeFailure(
			"Swapchain",
			"RecreateSwapchain.CreateScreenshotReadbackResources",
			"CreateScreenshotReadbackResources returned false after swapchain recreation");
		return false;
	}

	if (!projectv::render::CreateAaSceneTargets(context, render, internalExtent) ||
		!projectv::render::CreateAaPassResources(context, render, internalExtent)) {
		return false;
	}
	if (projectv::render::IsPostFxEnabled() &&
		!projectv::render::CreatePostFxResources(context, render, internalExtent)) {
		return false;
	}
	render->depthImageCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	if (render->aaPipelinesNeedRecreate &&
		!projectv::render::RecreateAaDependentPipelines(context, swapchain, render)) {
		return false;
	}

	if (render->graphicsPipeline != VK_NULL_HANDLE) {
		PV_PROFILE_ZONE_N("RecreateSwapchain.RefreshBindings");
		if (!RefreshGraphicsResourceBindings(context, render)) {
			runtime::LogRuntimeFailure(
				"Graphics",
				"RecreateSwapchain.RefreshGraphicsResourceBindings",
				"RefreshGraphicsResourceBindings returned false after swapchain recreation");
		}

		if (render->rayTracedShadows != nullptr && render->rayTracedShadows->IsVoxelAwareRtxActive()) {
			const VkExtent2D maskExtent =
				projectv::render::RayTracedShadows::ResolveShadowMaskExtent(internalExtent);
			if (!render->rayTracedShadows->RecreateShadowMaskForExtent(
					*context,
					maskExtent.width,
					maskExtent.height)) {
				runtime::LogRuntimeFailure(
					"Graphics",
					"RecreateSwapchain.RecreateShadowMaskForExtent",
					"RecreateShadowMaskForExtent returned false after swapchain recreation");
				return false;
			}
			if (!RefreshGraphicsResourceBindings(context, render)) {
				runtime::LogRuntimeFailure(
					"Graphics",
					"RecreateSwapchain.RefreshGraphicsResourceBindingsAfterShadowMaskResize",
					"RefreshGraphicsResourceBindings returned false after shadow mask resize");
				return false;
			}
		}
	}

	return true;
}

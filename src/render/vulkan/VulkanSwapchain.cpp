#include "render/vulkan/VulkanSwapchain.hpp"

#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"
#include "render/TaaRenderTargets.hpp"
#include "render/vulkan/VulkanDebug.hpp"
#include "render/vulkan/VulkanGraphicsPipeline.hpp"

#include <algorithm>
#include <array>
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

// **V-sync toggle (`2026-06-13`).** Live-cycled by the
// `V` hotkey in `app/main.cpp`. Three modes cycle:
//   0. `VK_PRESENT_MODE_IMMEDIATE_KHR` — vsync off,
//      no FPS cap, tearing possible (max throughput;
//      the only option for benchmarking the GPU ceiling).
//   1. `VK_PRESENT_MODE_MAILBOX_KHR` — vsync off
//      semantically, but the surface manager keeps a
//      single "latest image" and only swaps it on
//      vblank. Triple-buffered; no FPS cap. With a
//      VRR display (G-Sync / FreeSync) this is
//      *tear-free AND uncapped*. On a fixed-refresh
//      display, the swap-on-vblank means MAILBOX is
//      effectively capped at the display rate (the
//      display only updates on vblank; an image
//      submitted just after vblank waits almost a
//      full frame, so 144 FPS output on a 60 Hz
//      panel still shows ~60 Hz but with no tearing
//      and a one-frame latency). This is what NVIDIA
//      "Fast Sync" maps to under the hood.
//   2. `VK_PRESENT_MODE_FIFO_KHR` — vsync on, FPS
//      capped at the panel refresh rate (default
//      pre-toggle behaviour).
//
// We only return a mode the surface actually
// supports (`IMMEDIATE` is optional per the Vulkan
// spec; some Wayland / headless surfaces omit it).
// If the operator-requested mode isn't supported, the
// function falls back to the next one down the
// priority list so the engine always picks the
// best *available* tear-resistance for the current
// display rather than failing the swapchain create.
//
// **2026-06-14:** `g_preferredPresentMode` and
// `g_presentModeCycle` are now `inline` variables in
// `VulkanSwapchain.hpp` (so the HUD's read-only
// accessors can be header-only inlines without pulling
// this TU into test targets). The `ChoosePresentMode`
// / `CyclePreferredPresentMode` / `BuildPresentModeCycle`
// functions below reference the inline variables
// directly.
using projectv::present_mode::g_active;
using projectv::present_mode::g_cycle;

static VkPresentModeKHR PickBestAvailablePresentMode(
	const std::vector<VkPresentModeKHR> &presentModes,
	VkPresentModeKHR preferred)
{
	const std::array<VkPresentModeKHR, 3> priority{
		preferred,
		VK_PRESENT_MODE_MAILBOX_KHR,
		VK_PRESENT_MODE_FIFO_KHR,
	};
	for (const VkPresentModeKHR candidate : priority) {
		if (std::find(presentModes.begin(), presentModes.end(), candidate) != presentModes.end()) {
			return candidate;
		}
	}
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR> &presentModes)
{
	// **2026-06-14 fix.** Honour the operator's `V`-hotkey
	// override for ALL three modes, not just the non-FIFO
	// ones. The previous `if (g_preferredPresentMode !=
	// FIFO)` branch silently fell through to a MAILBOX-
	// first default chain whenever the operator cycled
	// the mode back to FIFO, which on Linux/Wayland
	// surfaces (or any surface that supports MAILBOX) meant
	// V → "FIFO" actually returned MAILBOX — vsync never
	// turned back on. The user reported this as "vsync
	// слетает при постановке блока" because the swapchain
	// recreates on block-induced re-renders re-entered
	// `ChoosePresentMode` and re-picked MAILBOX. See
	// `agent/decisions.md §30` and `agent/memory.md §12`
	// (V-sync bug history).
	if (g_active == VK_PRESENT_MODE_IMMEDIATE_KHR ||
		g_active == VK_PRESENT_MODE_MAILBOX_KHR) {
		return PickBestAvailablePresentMode(presentModes, g_active);
	}
	// Explicit FIFO request (operator chose V → "vsync on").
	// Honour it even if the surface supports MAILBOX.
	// FIFO is the only V-sync-capable present mode; if the
	// surface doesn't expose it (Vulkan 1.4 spec: always
	// supported per `VkPresentModeKHR` table), we fall back
	// to MAILBOX so the engine never fails the swapchain
	// create.
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

// **Tier 1.B (`2026-06-13`).** Moved `CreateOrRecreateSwapchain` to
// file scope so the header declaration (also file scope) doesn't
// conflict with an anonymous-namespace definition. The function
// still uses the file-scope helpers from the anonymous namespace
// above (anonymous-namespace contents are visible at file scope).
std::expected<VkFormat, projectv::swapchain::SwapchainError> CreateOrRecreateSwapchain(
	PlatformState *platform,
	VulkanContextState *context,
	SwapchainState *swapchain)
{
	// **Tier 1.B (`2026-06-13`).** Returns
	// `std::expected<VkFormat, SwapchainError>`. The chosen
	// `VkFormat` is returned on success and also written to
	// `swapchain->format` so callers that read the format through
	// the struct keep working. Each `return false;` is replaced
	// with `return std::unexpected(SwapchainError::Variant);` —
	// the per-step `runtime::LogRuntimeFailure` / `LogVkFailure`
	// calls are preserved inside this function so the
	// diagnostic log line carries the same detail as before.
	const auto fail = [](projectv::swapchain::SwapchainError e, std::string_view step, std::string_view detail) {
		runtime::LogRuntimeFailure("Swapchain", step, detail);
		return std::unexpected(e);
	};
	PV_PROFILE_ZONE_N("CreateOrRecreateSwapchain");
	SwapchainSupportDetails support;
	if (!QuerySwapchainSupport(context->physicalDevice, context->surface, &support)) {
		return fail(projectv::swapchain::SwapchainError::QuerySupportFailed,
			"CreateOrRecreateSwapchain.QuerySwapchainSupport",
			"QuerySwapchainSupport returned false");
	}

	if ((support.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0) {
		return fail(projectv::swapchain::SwapchainError::SurfaceUsageUnsupported,
			"CreateOrRecreateSwapchain.SurfaceUsage",
			"surface does not support VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT for swapchain images");
	}
	const bool supportsTransferSrc =
		(support.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;

	const auto [format, colorSpace] = ChooseSurfaceFormat(support.formats);
	// **2026-06-14: build the runtime present-mode cycle
	// here, before `ChoosePresentMode`.** The cycle is the
	// data `CyclePreferredPresentMode` walks (instead of a
	// hard-coded 3-state switch). Building it here means
	// the cycle is always **fresh** — if the surface's
	// support set changes (rare: only on display hot-swap
	// or driver upgrade), the next swapchain create
	// re-queries and rebuilds. `ChoosePresentMode` still
	// reads `g_preferredPresentMode` directly (a single
	// `VkPresentModeKHR`, set to the cycle's first element
	// by `BuildPresentModeCycle`).
	(void)BuildPresentModeCycle(support.presentModes);
	const VkPresentModeKHR chosenPresentMode = ChoosePresentMode(support.presentModes);
	const VkExtent2D chosenExtent = ChooseExtent(support.capabilities, platform->window);

	if (chosenExtent.width == 0 || chosenExtent.height == 0) {
		swapchain->format = format;
		swapchain->extent = chosenExtent;
		swapchain->supportsTransferSrc = false;
		return format;
	}

	uint32_t imageCount = std::max(2u, support.capabilities.minImageCount);
	if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
		imageCount = support.capabilities.maxImageCount;
	}

	VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (supportsTransferSrc) {
		imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
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
			return std::unexpected(projectv::swapchain::SwapchainError::CreateSwapchainFailed);
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
				return std::unexpected(projectv::swapchain::SwapchainError::GetImageCountFailed);
			}
			return fail(projectv::swapchain::SwapchainError::ZeroImages,
				"CreateOrRecreateSwapchain.vkGetSwapchainImagesKHR(count)",
				"swapchain returned zero images");
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
			return std::unexpected(projectv::swapchain::SwapchainError::GetImageListFailed);
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
				return std::unexpected(projectv::swapchain::SwapchainError::CreateImageViewFailed);
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

	// Per-swapchain-image submit-finished semaphores, created here so
	// the size always matches the current swapchain image count. See
	// `SwapchainState::submitSemaphores` for the contract; the
	// per-swapchain-image indexing is the canonical fix for the
	// "semaphore may still be in use by VkSwapchainKHR" validation
	// warning (per the Vulkan SDK 1.4
	// `swapchain_semaphore_reuse.html` guide, chapter "Swapchain
	// Semaphore Reuse").
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
			// The imageViews and swapchain itself were already created;
			// we cannot roll them back here, so just return false and
			// let the caller handle the failure.
			runtime::LogRuntimeFailure(
				"Swapchain",
				"CreateOrRecreateSwapchain.PerImageSemaphores",
				"failed to create per-swapchain-image submit semaphores");
			return std::unexpected(projectv::swapchain::SwapchainError::CreateSemaphoreFailed);
		}
		// Destroy the previous per-image semaphores before overwriting
		// the vector. The earlier `vkDeviceWaitIdle` in
		// `RecreateSwapchain` should have retired any in-flight work
		// that referenced them.
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
	return format;
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

	const bool hadGraphicsPipeline =
		render->graphicsPipeline != VK_NULL_HANDLE ||
		render->graphicsPipelineTaaOn != VK_NULL_HANDLE ||
		render->graphicsPipelineLayout != VK_NULL_HANDLE;
	{
		PV_PROFILE_ZONE_N("RecreateSwapchain.CreateSwapchainResources");
		// **Tier 1.B (`2026-06-13`).** `CreateOrRecreateSwapchain`
		// now returns `std::expected<VkFormat, SwapchainError>`.
		// `RecreateSwapchain` keeps its `bool` contract (it's the
		// per-frame hot path that wraps the cold init); the
		// variant is logged at high level so the operator still
		// gets the specific failure reason.
		const auto swapResult = CreateOrRecreateSwapchain(platform, context, swapchain);
		if (!swapResult.has_value()) {
			runtime::LogRuntimeFailure(
				"Swapchain",
				"RecreateSwapchain.CreateOrRecreateSwapchain",
				std::string{"CreateOrRecreateSwapchain returned: "} + std::string{projectv::swapchain::toString(swapResult.error())});
			return false;
		}
	}

	if (swapchain->extent.width == 0 || swapchain->extent.height == 0) {
		return true;
	}

	// TAA offscreen colour targets stay in lockstep with the swapchain
	// extent. The pair is intentionally allocated even when TAA is
	// currently disabled — the targets are only ~24 MiB at 1440p with
	// R16G16B16A16_SFLOAT, and pre-allocating them means toggling TAA on
	// at runtime does not have to wait for a swapchain resize to reattach.
	// The just-resolved history from the *previous* swapchain is no
	// longer valid; the next frame's resolve pass therefore starts with
	// `taaHistoryValid = false` and uses the current scene as the only
	// sample until a fresh history has been resolved at least once.
	if (render->taaSceneColorTarget == nullptr) {
		render->taaSceneColorTarget = new projectv::taa::OffscreenColorTarget();
	}
	if (render->taaHistoryColorTarget == nullptr) {
		render->taaHistoryColorTarget = new projectv::taa::OffscreenColorTarget();
	}
	// 1.5 anti-flicker: also lazy-allocate the per-layer history
	// pair. Same lifecycle as the colour history — both pairs go
	// through `CreateOrRecreateTaaRenderTargets` and re-allocate
	// together on swapchain resize. The pair is intentionally
	// allocated even when anti-flicker is conceptually disabled
	// (TAA off) so toggling it on at runtime does not have to
	// wait for a swapchain resize.
	if (render->taaLayerSceneColorTarget == nullptr) {
		render->taaLayerSceneColorTarget = new projectv::taa::OffscreenColorTarget();
	}
	if (render->taaLayerHistoryColorTarget == nullptr) {
		render->taaLayerHistoryColorTarget = new projectv::taa::OffscreenColorTarget();
	}
	// **Tier 1.B (`2026-06-13`).** `std::expected<void, TaaError>`
	// returns the specific failure variant (image / image-view /
	// sampler create). The per-step detail is logged inside
	// `CreateOrRecreateTaaRenderTargets`; the high-level caller
	// logs the variant name and tears down.
	const auto taaResult = projectv::taa::CreateOrRecreateTaaRenderTargets(
		context,
		swapchain->extent,
		*render->taaSceneColorTarget,
		*render->taaHistoryColorTarget,
		*render->taaLayerSceneColorTarget,
		*render->taaLayerHistoryColorTarget,
		render->taaLinearSampler);
	if (!taaResult.has_value()) {
		runtime::LogRuntimeFailure(
			"TaaRenderTargets",
			"RecreateSwapchain.CreateOrRecreateTaaRenderTargets",
			std::string{"TAA render target allocation failed: "} + std::string{projectv::taa::toString(taaResult.error())});
		return false;
	}
	render->taaHistoryValid = false;
	render->taaSceneColorNeedsInit = true;
	render->taaHistoryNeedsInit = true;
	render->taaSceneColorCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	// Both history images are reset to `UNDEFINED` (matching
	// their `initialLayout` in `TaaRenderTargets.cpp`); the
	// first-frame per-frame transition in `Renderer.cpp` will
	// move them to `SHADER_READ_ONLY_OPTIMAL` for the next
	// resolve pass / the next voxel pass's binding-6 sample.
	// The depth and the layer scene trackers also go back to
	// `UNDEFINED` so the first frame after recreate starts
	// from a clean state.
	render->taaHistoryColorCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	render->taaLayerSceneColorCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	render->taaLayerHistoryColorCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	render->depthImageCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	render->taaFrameCounter = 0u;
	render->taaPrevViewProjectionMatrix = {};
	// 1.2 — companion reset for the camera-cut detector. The
	// swapchain recreate path zeroes `taaPrevViewProjectionMatrix`,
	// so the next frame's cut check would otherwise see the real
	// current matrix next to a zero `prev` and register a false
	// "cut" with `maxDelta` equal to the largest |viewProj| entry.
	// Clear both the init flag and the cut accumulator so the
	// post-recreate counter is a clean baseline again.
	render->taaPrevViewProjectionMatrixInitialized = false;
	render->taaCameraCutCount = 0u;
	render->taaCameraCutMaxDelta = 0.0f;
	// 1.5 — same companion reset for the layer history. The
	// swapchain recreate path zero-initialises the layer history
	// pair (same as the colour history above), so the next
	// frame's layer history sample would otherwise read garbage.
	// Reset the valid flag so the voxel pass falls through to
	// the raw current value (without sampling history) for the
	// first frame after recreate.
	render->taaLayerHistoryValid = false;

	if (hadGraphicsPipeline) {
		PV_PROFILE_ZONE_N("RecreateSwapchain.DestroyGraphicsPipeline");
		DestroyGraphicsPipeline(context, render);
	}

	if (hadGraphicsPipeline) {
		PV_PROFILE_ZONE_N("RecreateSwapchain.CreateGraphicsPipeline");
		if (!CreateGraphicsPipeline(context, swapchain, render)) {
			runtime::LogRuntimeFailure(
				"Swapchain",
				"RecreateSwapchain.CreateGraphicsPipeline",
				"CreateGraphicsPipeline returned false after swapchain recreation");
			return false;
		}
	}

	return true;
}


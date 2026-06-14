#ifndef VULKAN_SWAPCHAIN_HPP
#define VULKAN_SWAPCHAIN_HPP

#include <cstdint>
#include <expected>
#include <string_view>

#include "core/Types.hpp"

namespace projectv::swapchain {

// **Tier 1.B (`2026-06-13`).** Strongly-typed error enum for
// `CreateOrRecreateSwapchain`. Replaces the per-step
// `runtime::LogRuntimeFailure` + `return false` pattern with
// `std::expected<VkFormat, SwapchainError>`. Cold path (1× per
// swapchain recreate / window resize), so the `std::expected`
// cost is irrelevant. The error variant is the machine-readable
// signal; the per-step log line is preserved inside the
// implementation.
enum class SwapchainError : std::uint8_t {
	PreconditionFailed = 0,
	QuerySupportFailed,
	SurfaceUsageUnsupported,
	CreateSwapchainFailed,
	GetImageCountFailed,
	ZeroImages,
	GetImageListFailed,
	CreateImageViewFailed,
	CreateSemaphoreFailed,
};

constexpr std::string_view toString(SwapchainError e) noexcept {
	switch (e) {
	case SwapchainError::PreconditionFailed: return "PreconditionFailed";
	case SwapchainError::QuerySupportFailed: return "QuerySupportFailed";
	case SwapchainError::SurfaceUsageUnsupported: return "SurfaceUsageUnsupported";
	case SwapchainError::CreateSwapchainFailed: return "CreateSwapchainFailed";
	case SwapchainError::GetImageCountFailed: return "GetImageCountFailed";
	case SwapchainError::ZeroImages: return "ZeroImages";
	case SwapchainError::GetImageListFailed: return "GetImageListFailed";
	case SwapchainError::CreateImageViewFailed: return "CreateImageViewFailed";
	case SwapchainError::CreateSemaphoreFailed: return "CreateSemaphoreFailed";
	}
	return "Unknown";
}
} // namespace projectv::swapchain

// **Tier 1.B (`2026-06-13`).** Returns
// `std::expected<VkFormat, projectv::swapchain::SwapchainError>`.
// The chosen `VkFormat` is the new return value (was
// `swapchain->imageFormat` side effect). On success the format
// is BOTH returned and written to `swapchain->imageFormat` so
// existing call sites that read the format via the struct keep
// working. The higher-level `RecreateSwapchain` wrapper keeps
// its `bool` return — it adds TAA render-target recreation on
// top and is a different (per-frame) hot path that the
// std::expected refactor doesn't fit cleanly.
std::expected<VkFormat, projectv::swapchain::SwapchainError> CreateOrRecreateSwapchain(
	PlatformState *platform,
	VulkanContextState *context,
	SwapchainState *swapchain);

bool RecreateSwapchain(
	PlatformState *platform,
	VulkanContextState *context,
	SwapchainState *swapchain,
	RenderState *render);

// **V-sync toggle hotkey (`2026-06-13`).** Cycles the
// `ChoosePresentMode` preference through
// IMMEDIATE → MAILBOX → FIFO. Returns the *new*
// preferred mode so the caller can log it. The
// swapchain is *not* recreated from this call —
// the next frame's `ChoosePresentMode` picks up the
// new preference when the swapchain is rebuilt
// (which happens automatically on window resize /
// device lost / explicit `RecreateSwapchain` call,
// or on the *next* swapchain create if the surface
// is recreated). For an immediate mode change, the
// caller should call `RecreateSwapchain` after
// invoking this; for the operator's `V` hotkey, the
// mode is just stashed here and picked up on the
// next natural recreate (next frame after
// `SDL_AppIterate` returns `SDL_APP_CONTINUE`).
VkPresentModeKHR CyclePreferredPresentMode();

#endif

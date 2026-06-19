#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>
#include <vector>

#include "core/Types.hpp"

namespace projectv::swapchain {

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

std::expected<VkFormat, projectv::swapchain::SwapchainError> CreateOrRecreateSwapchain(
	PlatformState *platform,
	VulkanContextState *context,
	SwapchainState *swapchain);

bool RecreateSwapchain(
	PlatformState *platform,
	VulkanContextState *context,
	SwapchainState *swapchain,
	RenderState *render);

VkPresentModeKHR CyclePreferredPresentMode();

std::vector<VkPresentModeKHR> BuildPresentModeCycle(
	const std::vector<VkPresentModeKHR> &surfacePresentModes);

namespace projectv::present_mode {
inline VkPresentModeKHR g_active = VK_PRESENT_MODE_FIFO_KHR;
inline std::vector<VkPresentModeKHR> g_cycle = {VK_PRESENT_MODE_FIFO_KHR};
} // namespace projectv::present_mode

inline VkPresentModeKHR GetActivePresentMode()
{
	return projectv::present_mode::g_active;
}

inline std::size_t GetPresentModeCycleSize()
{
	return projectv::present_mode::g_cycle.size();
}

inline std::size_t GetPresentModeCycleIndex(const VkPresentModeKHR mode)
{
	const auto it = std::find(
		projectv::present_mode::g_cycle.begin(),
		projectv::present_mode::g_cycle.end(),
		mode);
	if (it == projectv::present_mode::g_cycle.end()) {
		return 0u;
	}
	return static_cast<std::size_t>(it - projectv::present_mode::g_cycle.begin());
}

inline VkPresentModeKHR CyclePreferredPresentMode()
{
	if (projectv::present_mode::g_cycle.size() <= 1) {
		return projectv::present_mode::g_active;
	}
	const auto it = std::find(
		projectv::present_mode::g_cycle.begin(),
		projectv::present_mode::g_cycle.end(),
		projectv::present_mode::g_active);
	const std::size_t currentIndex = (it == projectv::present_mode::g_cycle.end())
		? 0u
		: static_cast<std::size_t>(it - projectv::present_mode::g_cycle.begin());
	const std::size_t nextIndex = (currentIndex + 1u) % projectv::present_mode::g_cycle.size();
	projectv::present_mode::g_active = projectv::present_mode::g_cycle[nextIndex];
	return projectv::present_mode::g_active;
}

inline std::vector<VkPresentModeKHR> BuildPresentModeCycle(
	const std::vector<VkPresentModeKHR> &surfacePresentModes)
{
	static constexpr std::array<VkPresentModeKHR, 3> kPriority{
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_MAILBOX_KHR,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
	};
	const VkPresentModeKHR previousActive = projectv::present_mode::g_active;
	projectv::present_mode::g_cycle.clear();
	projectv::present_mode::g_cycle.reserve(kPriority.size());
	for (const VkPresentModeKHR mode : kPriority) {
		if (std::find(surfacePresentModes.begin(), surfacePresentModes.end(), mode)
			!= surfacePresentModes.end()) {
			projectv::present_mode::g_cycle.push_back(mode);
		}
	}
	if (projectv::present_mode::g_cycle.empty()) {
		// Vulkan 1.4 spec: `FIFO_KHR` is mandatory. If the
		// surface didn't report it, fall back to a single-
		// element cycle of FIFO so the engine still creates
		// a swapchain.
		projectv::present_mode::g_cycle.push_back(VK_PRESENT_MODE_FIFO_KHR);
	}
	// Preserve the previous `g_active` if it's still
	// supported; otherwise fall back to highest-priority.
	if (std::find(
			projectv::present_mode::g_cycle.begin(),
			projectv::present_mode::g_cycle.end(),
			previousActive)
		!= projectv::present_mode::g_cycle.end()) {
		projectv::present_mode::g_active = previousActive;
	} else {
		// Display hot-swap dropped the current mode
		// (e.g. external monitor unplugged and the new
		// surface doesn't expose IMMEDIATE). Fall back
		// to highest-priority supported mode, which is
		// FIFO on every conformant Vulkan 1.4 surface.
		projectv::present_mode::g_active = projectv::present_mode::g_cycle.front();
	}
	return projectv::present_mode::g_cycle;
}


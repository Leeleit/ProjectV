#pragma once

#include <cstddef> // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
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

constexpr std::string_view toString(SwapchainError const e) noexcept {
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

bool CreateOrRecreateSwapchain(
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
} // namespace projectv::present_mode

inline VkPresentModeKHR GetActivePresentMode()
{
	return projectv::present_mode::g_active;
}

namespace projectv::present_mode {
inline std::vector<VkPresentModeKHR> &MutableCycle() noexcept // NOLINT(bugprone-exception-escape): static cycle is small and fixed
{
	static std::vector g_cycle = {VK_PRESENT_MODE_FIFO_KHR};
	return g_cycle;
}
} // namespace projectv::present_mode

inline std::size_t GetPresentModeCycleSize()
{
	return projectv::present_mode::MutableCycle().size();
}

inline std::size_t GetPresentModeCycleIndex(const VkPresentModeKHR mode)
{
	const auto &cycle = projectv::present_mode::MutableCycle();
	const auto it = std::find(cycle.begin(), cycle.end(), mode);
	if (it == cycle.end()) {
		return 0u;
	}
	return static_cast<std::size_t>(it - cycle.begin());
}

inline VkPresentModeKHR CyclePreferredPresentMode()
{
	auto &cycle = projectv::present_mode::MutableCycle();
	if (cycle.size() <= 1) {
		return projectv::present_mode::g_active;
	}
	const auto it = std::find(cycle.begin(), cycle.end(), projectv::present_mode::g_active);
	const std::size_t currentIndex = it == cycle.end()
		? 0u
		: static_cast<std::size_t>(it - cycle.begin());
	const std::size_t nextIndex = (currentIndex + 1u) % cycle.size();
	projectv::present_mode::g_active = cycle[nextIndex];
	return projectv::present_mode::g_active;
}

inline std::vector<VkPresentModeKHR> BuildPresentModeCycle(
	const std::vector<VkPresentModeKHR> &surfacePresentModes)
{
	static constexpr std::array kPriority{
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_MAILBOX_KHR,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
	};
	const VkPresentModeKHR previousActive = projectv::present_mode::g_active;
	auto &cycle = projectv::present_mode::MutableCycle();
	cycle.clear();
	cycle.reserve(kPriority.size());
	for (const VkPresentModeKHR mode : kPriority) {
		if (std::find(surfacePresentModes.begin(), surfacePresentModes.end(), mode)
			!= surfacePresentModes.end()) {
			cycle.push_back(mode);
		}
	}
	if (cycle.empty()) {

		cycle.push_back(VK_PRESENT_MODE_FIFO_KHR);
	}

	if (std::find(cycle.begin(), cycle.end(), previousActive) != cycle.end()) {
		projectv::present_mode::g_active = previousActive;
	} else {

		projectv::present_mode::g_active = cycle.front();
	}
	return cycle;
}


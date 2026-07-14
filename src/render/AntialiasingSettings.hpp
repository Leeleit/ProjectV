#pragma once

#include <cstdint>
#include <string_view>

#include <vulkan/vulkan.h>

namespace projectv::render {

enum class MsaaMode : std::uint8_t {
	Off = 0,
	X2,
	X4,
	Count,
};

enum class RenderScaleMode : std::uint8_t {
	Native = 0,
	X125,
	X150,
	Count,
};

inline constexpr std::string_view ToString(const MsaaMode mode) noexcept
{
	switch (mode) {
	case MsaaMode::Off:
		return "Off";
	case MsaaMode::X2:
		return "MSAA2";
	case MsaaMode::X4:
		return "MSAA4";
	case MsaaMode::Count:
		return "Count";
	}
	return "Unknown";
}

inline constexpr std::string_view ToString(const RenderScaleMode mode) noexcept
{
	switch (mode) {
	case RenderScaleMode::Native:
		return "1.00";
	case RenderScaleMode::X125:
		return "1.25";
	case RenderScaleMode::X150:
		return "1.50";
	case RenderScaleMode::Count:
		return "Count";
	}
	return "Unknown";
}

inline constexpr std::uint32_t MsaaSampleCount(const MsaaMode mode) noexcept
{
	switch (mode) {
	case MsaaMode::X2:
		return 2u;
	case MsaaMode::X4:
		return 4u;
	case MsaaMode::Off:
	case MsaaMode::Count:
	default:
		return 1u;
	}
}

inline constexpr float RenderScaleFactor(const RenderScaleMode mode) noexcept
{
	switch (mode) {
	case RenderScaleMode::X125:
		return 1.25f;
	case RenderScaleMode::X150:
		return 1.5f;
	case RenderScaleMode::Native:
	case RenderScaleMode::Count:
	default:
		return 1.0f;
	}
}

inline constexpr MsaaMode CycleMsaaMode(const MsaaMode mode) noexcept
{
	const auto next = static_cast<std::uint8_t>(mode) + 1u;
	return static_cast<MsaaMode>(next % static_cast<std::uint8_t>(MsaaMode::Count));
}

inline constexpr RenderScaleMode CycleRenderScaleMode(const RenderScaleMode mode) noexcept
{
	const auto next = static_cast<std::uint8_t>(mode) + 1u;
	return static_cast<RenderScaleMode>(next % static_cast<std::uint8_t>(RenderScaleMode::Count));
}

inline constexpr VkSampleCountFlagBits ToVkSampleCount(const std::uint32_t samples) noexcept
{
	switch (samples) {
	case 2u:
		return VK_SAMPLE_COUNT_2_BIT;
	case 4u:
		return VK_SAMPLE_COUNT_4_BIT;
	case 8u:
		return VK_SAMPLE_COUNT_8_BIT;
	default:
		return VK_SAMPLE_COUNT_1_BIT;
	}
}

// Clamp requested MSAA to device framebuffer sample support (color ∩ depth).
inline std::uint32_t ClampMsaaSampleCount(
	const VkSampleCountFlags colorCounts,
	const VkSampleCountFlags depthCounts,
	const std::uint32_t requested) noexcept
{
	const VkSampleCountFlags supported = colorCounts & depthCounts;
	if (requested >= 4u && (supported & VK_SAMPLE_COUNT_4_BIT) != 0) {
		return 4u;
	}
	if (requested >= 2u && (supported & VK_SAMPLE_COUNT_2_BIT) != 0) {
		return 2u;
	}
	return 1u;
}

inline constexpr std::uint32_t kProgressiveAccumMaxFrames = 16u;

inline VkExtent2D ComputeInternalRenderExtent(const VkExtent2D swapchainExtent, const RenderScaleMode scaleMode) noexcept
{
	const float scale = RenderScaleFactor(scaleMode);
	const std::uint32_t width = static_cast<std::uint32_t>(
		static_cast<float>(swapchainExtent.width) * scale + 0.999f);
	const std::uint32_t height = static_cast<std::uint32_t>(
		static_cast<float>(swapchainExtent.height) * scale + 0.999f);
	return VkExtent2D{
		.width = width == 0u ? 1u : width,
		.height = height == 0u ? 1u : height,
	};
}

} // namespace projectv::render

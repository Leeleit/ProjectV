#pragma once

#include <cstdint>
#include <string_view>

namespace projectv::render {

// Antialiasing mode for the main color pass. Combines MSAA (geometry AA) and
// TAA (shader / temporal AA). Selected via the `T` hotkey (cycles through) and
// persisted in runtime/scene.json as `"aaMode"`.
enum class AntialiasingMode : std::uint8_t {
	Off = 0,	 // no AA, single-sample, no temporal accumulation
	MSAA_2X,	 // 2x MSAA, no TAA, no jitter, no history
	TAA,		 // single-sample + TAA, jitter ON, history buffer active
	MSAA_2X_TAA, // 2x MSAA + TAA (default; geometry + shader AA)
	Count,
};

inline constexpr std::string_view ToString(AntialiasingMode mode) noexcept
{
	switch (mode) {
	case AntialiasingMode::Off:
		return "Off";
	case AntialiasingMode::MSAA_2X:
		return "MSAA_2X";
	case AntialiasingMode::TAA:
		return "TAA";
	case AntialiasingMode::MSAA_2X_TAA:
		return "MSAA_2X_TAA";
	case AntialiasingMode::Count:
		return "Count";
	}
	return "Unknown";
}

// Returns the Vulkan sample count for the given AA mode. 1 means single-sample
// (no MSAA). 2 means 2x MSAA. Other values would require hardware support check.
inline constexpr std::uint32_t MsaaSamplesForMode(AntialiasingMode mode) noexcept
{
	switch (mode) {
	case AntialiasingMode::MSAA_2X:
	case AntialiasingMode::MSAA_2X_TAA:
		return 2u;
	case AntialiasingMode::Off:
	case AntialiasingMode::TAA:
	default:
		return 1u;
	}
}

// Whether TAA (temporal reprojection + history blending) is active in the given
// mode. Off and MSAA_2X have no TAA; TAA and MSAA_2X_TAA do.
inline constexpr bool IsTaaEnabledForMode(AntialiasingMode mode) noexcept
{
	return mode == AntialiasingMode::TAA || mode == AntialiasingMode::MSAA_2X_TAA;
}

// Cycles to the next AA mode (used by the T hotkey). Order: Off -> MSAA_2X -> TAA
// -> MSAA_2X_TAA -> Off.
inline constexpr AntialiasingMode CycleAntialiasingMode(AntialiasingMode mode) noexcept
{
	const auto next = static_cast<std::uint8_t>(mode) + 1u;
	return static_cast<AntialiasingMode>(next % static_cast<std::uint8_t>(AntialiasingMode::Count));
}

} // namespace projectv::render

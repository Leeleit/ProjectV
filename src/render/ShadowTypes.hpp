#pragma once

#include <array>
#include <cstddef> // IWYU pragma: keep — size_t at L8 needs <cstddef>
#include <cstdint> // IWYU pragma: keep <cstdint>

constexpr uint32_t kSunShadowCascadeCount = 4u;
constexpr size_t kSunShadowMatrixElementCount = 16u * static_cast<size_t>(kSunShadowCascadeCount);

enum class TransparentShadowPolicy : uint8_t {
	GlassIgnoredFluidCasts = 0,
};

struct SunShadowCascadeSplits {
	std::array<float, kSunShadowCascadeCount> normalizedSplits{};
	std::array<float, kSunShadowCascadeCount> viewDepthSplits{};
	float splitLambda = 0.80f;
	float nearPlane = 0.1f;
	float farPlane = 128.0f;
};

struct SunShadowCascadeDiagnostics {
	std::array<float, kSunShadowCascadeCount> viewNearDepths{};
	std::array<float, kSunShadowCascadeCount> viewFarDepths{};
	std::array<float, kSunShadowCascadeCount> orthoWidths{};
	std::array<float, kSunShadowCascadeCount> orthoHeights{};
	std::array<float, kSunShadowCascadeCount> texelWorldSizes{};
	std::array<float, kSunShadowCascadeCount> casterLightNearDepths{};
	std::array<float, kSunShadowCascadeCount> casterLightFarDepths{};
};

inline const char *TransparentShadowPolicyToString(const TransparentShadowPolicy policy)
{
	switch (policy) {
	case TransparentShadowPolicy::GlassIgnoredFluidCasts:
	default:
		return "GLASS_IGNORED_FLUID_CASTS";
	}
}


#include "render/ShadowProjection.hpp"
#include "render/ShadowTypes.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {

void Expect(const bool condition, const char *label)
{
	if (!condition) {
		std::fprintf(stderr, "FAIL: %s\n", label);
		std::abort();
	}
}

void ExpectMonotonic(const SunShadowCascadeSplits &splits, const char *label)
{
	for (uint32_t i = 1; i < kSunShadowCascadeCount; ++i) {
		Expect(
			splits.viewDepthSplits[i] > splits.viewDepthSplits[i - 1],
			label);
		Expect(
			splits.normalizedSplits[i] > splits.normalizedSplits[i - 1],
			label);
	}
}

void ExpectInRange(
	const SunShadowCascadeSplits &splits,
	const char *label)
{
	for (uint32_t i = 0; i < kSunShadowCascadeCount; ++i) {
		Expect(
			splits.viewDepthSplits[i] >= splits.nearPlane &&
				splits.viewDepthSplits[i] <= splits.farPlane,
			label);
		Expect(
			splits.normalizedSplits[i] >= 0.0f &&
				splits.normalizedSplits[i] <= 1.0f,
			label);
	}
}

} // namespace

int main() {

	{
		const SunShadowCascadeSplits splits =
			BuildSunShadowCascadeSplits(0.1f, 128.0f);
		Expect(
			splits.splitLambda == 0.80f, "happy: default lambda");
		Expect(
			splits.nearPlane == 0.1f, "happy: nearPlane echoed");
		Expect(
			splits.farPlane == 128.0f, "happy: farPlane echoed");
		Expect(
			splits.viewDepthSplits.back() == 128.0f,
			"happy: last viewDepthSplit == farPlane");
		Expect(
			splits.normalizedSplits.back() == 1.0f,
			"happy: last normalizedSplit == 1.0f");
		ExpectMonotonic(splits, "happy: monotonic");
		ExpectInRange(splits, "happy: in-range");
	}
	std::printf("[OK] happy path: 0.1→128, default lambda\n");


	{
		const SunShadowCascadeSplits splits =
			BuildSunShadowCascadeSplits(0.01f, 1.0f);
		ExpectMonotonic(splits, "minStep: monotonic");
		ExpectInRange(splits, "minStep: in-range");
		Expect(
			splits.viewDepthSplits.back() == 1.0f,
			"minStep: last == farPlane");
	}
	std::printf("[OK] minDepthStep clamp: 0.01→1.0\n");


	{
		const SunShadowCascadeSplits splits =
			BuildSunShadowCascadeSplits(1.0f, 100.0f, 0.0f);
		const float depthRange = splits.farPlane - splits.nearPlane;
		for (uint32_t i = 0; i < kSunShadowCascadeCount; ++i) {
			const float expected = splits.nearPlane +
				depthRange * static_cast<float>(i + 1u) /
				static_cast<float>(kSunShadowCascadeCount);

			if (i + 1u < kSunShadowCascadeCount) {
				Expect(
					std::abs(splits.viewDepthSplits[i] - expected) < 1e-4f,
					"uniform: split matches");
			}
		}
	}
	std::printf("[OK] splitLambda=0 → pure uniform\n");


	{
		const SunShadowCascadeSplits uniformSplits =
			BuildSunShadowCascadeSplits(1.0f, 100.0f, 0.0f);
		const SunShadowCascadeSplits logSplits =
			BuildSunShadowCascadeSplits(1.0f, 100.0f, 1.0f);

		for (uint32_t i = 0; i + 1u < kSunShadowCascadeCount; ++i) {
			Expect(
				logSplits.viewDepthSplits[i] < uniformSplits.viewDepthSplits[i],
				"logarithmic: split shallower than uniform");
		}
		Expect(
			logSplits.viewDepthSplits.back() == 100.0f,
			"logarithmic: last == farPlane");
		ExpectMonotonic(logSplits, "logarithmic: monotonic");
	}
	std::printf("[OK] splitLambda=1 → pure logarithmic\n");


	{
		const SunShadowCascadeSplits nanSplits = BuildSunShadowCascadeSplits(
			std::numeric_limits<float>::quiet_NaN(),
			std::numeric_limits<float>::quiet_NaN());
		Expect(
			nanSplits.nearPlane == 0.1f, "NaN: nearPlane fallback");
		Expect(
			nanSplits.farPlane >= 1.0f, "NaN: farPlane fallback");
		ExpectMonotonic(nanSplits, "NaN: monotonic");
	}
	std::printf("[OK] defensive defaults: NaN inputs\n");


	{
		const SunShadowCascadeSplits lowLambda =
			BuildSunShadowCascadeSplits(1.0f, 100.0f, -0.5f);
		Expect(
			lowLambda.splitLambda == 0.0f, "lambda < 0: clamped to 0");
		const SunShadowCascadeSplits highLambda =
			BuildSunShadowCascadeSplits(1.0f, 100.0f, 2.0f);
		Expect(
			highLambda.splitLambda == 1.0f, "lambda > 1: clamped to 1");
	}
	std::printf("[OK] lambda clamp: out-of-range inputs\n");


	{
		const SunShadowCascadeSplits splits =
			BuildSunShadowCascadeSplits(0.1f, 0.05f);
		Expect(
			splits.farPlane >= 1.0f,
			"degenerate: farPlane bumped to default");
		ExpectMonotonic(splits, "degenerate: monotonic");
	}
	std::printf("[OK] degenerate farPlane <= nearPlane\n");

	std::printf("ProjectVSunShadowCascadeSplitsTests: 7/7 passed\n");
	return EXIT_SUCCESS;
}

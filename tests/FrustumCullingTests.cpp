
#include "core/Math.hpp"
#include "core/Types.hpp"
#include "render/SceneResources.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <string_view>

namespace {
struct TestContext {
	int failures = 0;

	void Fail(const int line, const std::string_view message)
	{
		std::fprintf(stderr, "Test failure at line %d: %.*s\n", line, static_cast<int>(message.size()), message.data());
		++failures;
	}
};

ChunkCullingParameters MakeForwardLookingCamera(
	const float verticalFovRadians = static_cast<float>(3.14159265358979323846L) / 3.0f,
	const float aspect = 16.0f / 9.0f,
	const float nearPlane = 0.1f,
	const float maxDistance = 0.0f)
{
	ChunkCullingParameters parameters{};
	const float tanHalfVerticalFov = std::tan(verticalFovRadians * 0.5f);
	const float tanHalfHorizontalFov = tanHalfVerticalFov * aspect;
	parameters.cameraPositionAndMaxDistance = {0.0f, 0.0f, 0.0f, maxDistance};
	parameters.cameraForwardAndTanHalfVerticalFov = {0.0f, 0.0f, -1.0f, tanHalfVerticalFov};
	parameters.cameraRightAndTanHalfHorizontalFov = {1.0f, 0.0f, 0.0f, tanHalfHorizontalFov};
	parameters.cameraUpAndNearPlane = {0.0f, 1.0f, 0.0f, nearPlane};
	return parameters;
}

#define PV_EXPECT_TRUE(ctx, cond, msg)   \
	do {                                 \
		if (!(cond)) {                   \
			(ctx).Fail(__LINE__, (msg)); \
		}                                \
	} while (0)

void TestAabbInsideFrustumVisible(TestContext &ctx)
{

	const ChunkCullingParameters camera = MakeForwardLookingCamera();
	constexpr projectv::math::Vec3 aabbMin{-0.5f, -0.5f, -2.5f, 0.0f};
	constexpr projectv::math::Vec3 aabbMax{0.5f, 0.5f, -1.5f, 0.0f};
	PV_EXPECT_TRUE(
		ctx,
		IsAabbVisibleAgainstCameraFrustum(aabbMin, aabbMax, camera),
		"AABB inside the frustum should be visible");
}

void TestAabbBehindCameraCulled(TestContext &ctx)
{

	const ChunkCullingParameters camera = MakeForwardLookingCamera();
	constexpr projectv::math::Vec3 aabbMin{-0.5f, -0.5f, 0.5f, 0.0f};
	constexpr projectv::math::Vec3 aabbMax{0.5f, 0.5f, 1.5f, 0.0f};
	PV_EXPECT_TRUE(
		ctx,
		!IsAabbVisibleAgainstCameraFrustum(aabbMin, aabbMax, camera),
		"AABB behind the camera (positive Z) should be culled");
}

void TestAabbToTheLeftCulled(TestContext &ctx)
{

	const ChunkCullingParameters camera = MakeForwardLookingCamera();
	constexpr projectv::math::Vec3 aabbMin{-100.5f, -0.5f, -10.5f, 0.0f};
	constexpr projectv::math::Vec3 aabbMax{-99.5f, 0.5f, -9.5f, 0.0f};
	PV_EXPECT_TRUE(
		ctx,
		!IsAabbVisibleAgainstCameraFrustum(aabbMin, aabbMax, camera),
		"AABB far to the left of the frustum should be culled");
}

void TestAabbStraddlingNearPlaneVisible(TestContext &ctx)
{

	const ChunkCullingParameters camera = MakeForwardLookingCamera();
	constexpr projectv::math::Vec3 aabbMin{-0.5f, -0.5f, -0.6f, 0.0f};
	constexpr projectv::math::Vec3 aabbMax{0.5f, 0.5f, 0.4f, 0.0f};
	PV_EXPECT_TRUE(
		ctx,
		IsAabbVisibleAgainstCameraFrustum(aabbMin, aabbMax, camera),
		"AABB straddling the near plane should remain visible");
}

void TestAabbBeyondMaxDistanceCulled(TestContext &ctx)
{

	const ChunkCullingParameters camera = MakeForwardLookingCamera(
		static_cast<float>(3.14159265358979323846L) / 3.0f,
		16.0f / 9.0f,
		0.1f,
		5.0f);
	constexpr projectv::math::Vec3 aabbMin{-0.5f, -0.5f, -100.5f, 0.0f};
	constexpr projectv::math::Vec3 aabbMax{0.5f, 0.5f, -99.5f, 0.0f};
	PV_EXPECT_TRUE(
		ctx,
		!IsAabbVisibleAgainstCameraFrustum(aabbMin, aabbMax, camera),
		"AABB beyond maxDistance should be culled");
}
} // namespace

int main()
{
	TestContext ctx;
	TestAabbInsideFrustumVisible(ctx);
	TestAabbBehindCameraCulled(ctx);
	TestAabbToTheLeftCulled(ctx);
	TestAabbStraddlingNearPlaneVisible(ctx);
	TestAabbBeyondMaxDistanceCulled(ctx);
	if (ctx.failures == 0) {
		std::printf("ProjectVFrustumCullingTests: 5/5 passed\n");
		return 0;
	}
	std::fprintf(stderr, "ProjectVFrustumCullingTests: %d failure(s)\n", ctx.failures);
	return 1;
}

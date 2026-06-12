// M5: unit tests for `IsAabbVisibleAgainstCameraFrustum` (declared in
// `src/render/SceneResources.hpp`). CPU-only: the helper is an inline
// camera-frustum test that takes the same `ChunkCullingParameters`
// struct the chunk cull path consumes. The math is independent of
// Vulkan / mesh data, so no fixture file is needed — every test
// constructs the camera basis and the AABB by hand.

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

// A camera sitting at the origin looking down -Z with a 60° vertical
// FOV. `extent` carries the aspect ratio through the helper
// (`BuildChunkCullingParameters` multiplies tanHalfVerticalFov by
// width/height). The frustum planes that matter for these unit tests
// are: forward = (0,0,-1), near = 0.1, maxDistance = 0 means
// "unbounded" in the helper.
ChunkCullingParameters MakeForwardLookingCamera(
	const float verticalFovRadians = static_cast<float>(M_PI) / 3.0f,
	const float aspect = 16.0f / 9.0f,
	const float nearPlane = 0.1f,
	const float maxDistance = 0.0f)
{
	ChunkCullingParameters parameters{};
	const float tanHalfVerticalFov = std::tan(verticalFovRadians * 0.5f);
	const float tanHalfHorizontalFov = tanHalfVerticalFov * aspect;
	// Camera basis: position at origin, forward = -Z, right = +X, up = +Y.
	parameters.cameraPositionAndMaxDistance = { 0.0f, 0.0f, 0.0f, maxDistance };
	parameters.cameraForwardAndTanHalfVerticalFov = { 0.0f, 0.0f, -1.0f, tanHalfVerticalFov };
	parameters.cameraRightAndTanHalfHorizontalFov = { 1.0f, 0.0f, 0.0f, tanHalfHorizontalFov };
	parameters.cameraUpAndNearPlane = { 0.0f, 1.0f, 0.0f, nearPlane };
	return parameters;
}

#define PV_EXPECT_TRUE(ctx, cond, msg) \
	do { \
		if (!(cond)) { \
			(ctx).Fail(__LINE__, (msg)); \
		} \
	} while (0)

void TestAabbInsideFrustumVisible(TestContext &ctx)
{
	// 1x1x1 cube centred on the camera axis, 2 units in front of it.
	// Fully inside all six planes; maxDistance = 0 disables the far
	// sphere test.
	const ChunkCullingParameters camera = MakeForwardLookingCamera();
	const std::array<float, 3> aabbMin{ -0.5f, -0.5f, -2.5f };
	const std::array<float, 3> aabbMax{ 0.5f, 0.5f, -1.5f };
	PV_EXPECT_TRUE(
		ctx,
		IsAabbVisibleAgainstCameraFrustum(aabbMin, aabbMax, camera),
		"AABB inside the frustum should be visible");
}

void TestAabbBehindCameraCulled(TestContext &ctx)
{
	// 1x1x1 cube behind the camera (positive Z). Forward = (0,0,-1),
	// so the near plane test must reject it.
	const ChunkCullingParameters camera = MakeForwardLookingCamera();
	const std::array<float, 3> aabbMin{ -0.5f, -0.5f, 0.5f };
	const std::array<float, 3> aabbMax{ 0.5f, 0.5f, 1.5f };
	PV_EXPECT_TRUE(
		ctx,
		!IsAabbVisibleAgainstCameraFrustum(aabbMin, aabbMax, camera),
		"AABB behind the camera (positive Z) should be culled");
}

void TestAabbToTheLeftCulled(TestContext &ctx)
{
	// AABB far to the left of the camera axis. With FOV 60° and
	// aspect 16/9, the horizontal half-angle's tan ≈ 0.577, so an
	// AABB centred at x = -100 at z = -10 has a projected radius of
	// 0.5 against the left-plane normal — center distance is ~100
	// along (forward * tanHalfH + right), which dwarfs the radius
	// and culls the box.
	const ChunkCullingParameters camera = MakeForwardLookingCamera();
	const std::array<float, 3> aabbMin{ -100.5f, -0.5f, -10.5f };
	const std::array<float, 3> aabbMax{ -99.5f, 0.5f, -9.5f };
	PV_EXPECT_TRUE(
		ctx,
		!IsAabbVisibleAgainstCameraFrustum(aabbMin, aabbMax, camera),
		"AABB far to the left of the frustum should be culled");
}

void TestAabbStraddlingNearPlaneVisible(TestContext &ctx)
{
	// 1x1x1 cube straddling the near plane (z = -0.1, near = 0.1).
	// The center is exactly on the near plane: centerDistance to
	// forward is 0, projected radius onto (0,0,-1) is 0.5, so
	// `0 + 0.5 >= 0` passes. This is the desired behaviour — a
	// model placed right at the camera origin must not flicker as
	// the near plane tightens.
	const ChunkCullingParameters camera = MakeForwardLookingCamera();
	const std::array<float, 3> aabbMin{ -0.5f, -0.5f, -0.6f };
	const std::array<float, 3> aabbMax{ 0.5f, 0.5f, 0.4f };
	PV_EXPECT_TRUE(
		ctx,
		IsAabbVisibleAgainstCameraFrustum(aabbMin, aabbMax, camera),
		"AABB straddling the near plane should remain visible");
}

void TestAabbBeyondMaxDistanceCulled(TestContext &ctx)
{
	// AABB in front of the camera, but past the configured
	// maxDistance = 5. The helper's far test is a sphere test on
	// `length(toAabbCenter)` vs `maxDistance + aabbRadius`, so
	// 100 units of distance with a 0.5-unit half-extent must reject
	// the box.
	const ChunkCullingParameters camera = MakeForwardLookingCamera(
		static_cast<float>(M_PI) / 3.0f,
		16.0f / 9.0f,
		0.1f,
		5.0f);
	const std::array<float, 3> aabbMin{ -0.5f, -0.5f, -100.5f };
	const std::array<float, 3> aabbMax{ 0.5f, 0.5f, -99.5f };
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

#include "voxel/CpuGreedyMeshing.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {

struct TestContext {
	int failures = 0;
	void Fail(const int line, const std::string_view msg)
	{
		std::fprintf(stderr, "FAIL line %d: %.*s\n", line, static_cast<int>(msg.size()), msg.data());
		++failures;
	}
};

using namespace projectv::voxel;

CpuGreedyChunkDesc MakeChunk(const int ox, const int oy, const int ex, const int ey, const int ez, const uint32_t nonAir = 1u)
{
	CpuGreedyChunkDesc c{};
	c.chunkOrigin[0] = ox;
	c.chunkOrigin[1] = oy;
	c.chunkOrigin[2] = 0;
	c.extent[0] = static_cast<uint32_t>(ex);
	c.extent[1] = static_cast<uint32_t>(ey);
	c.extent[2] = static_cast<uint32_t>(ez);
	c.nonAirCount = nonAir;
	return c;
}

CpuChunkCullingParams MakeCameraLookingForward(const float camX, const float camY, const float camZ)
{
	CpuChunkCullingParams c{};
	c.cameraX = camX;
	c.cameraY = camY;
	c.cameraZ = camZ;
	c.maxDistance = 1000.0f;
	c.cameraForwardX = 0.0f;
	c.cameraForwardY = 0.0f;
	c.cameraForwardZ = 1.0f;
	c.cameraRightX = 1.0f;
	c.cameraRightY = 0.0f;
	c.cameraRightZ = 0.0f;
	c.cameraUpX = 0.0f;
	c.cameraUpY = 1.0f;
	c.cameraUpZ = 0.0f;
	c.tanHalfVerticalFov = 1.0f;
	c.tanHalfHorizontalFov = 1.0f;
	c.nearPlane = 0.1f;
	return c;
}

void TestZeroNonAirInvisible(TestContext &ctx)
{
	const auto chunk = MakeChunk(0, 0, 8, 8, 8, 0u);
	const auto cam = MakeCameraLookingForward(0, 0, -10);
	if (IsChunkVisibleCPU(chunk, cam)) {
		ctx.Fail(__LINE__, "chunk with nonAirCount=0 must be invisible");
	}
}

void TestChunkInFrontOfCameraVisible(TestContext &ctx)
{
	const auto chunk = MakeChunk(0, 0, 8, 8, 8, 1u);
	const auto cam = MakeCameraLookingForward(-4, -4, -10);
	if (!IsChunkVisibleCPU(chunk, cam)) {
		ctx.Fail(__LINE__, "chunk directly in front of camera must be visible");
	}
}

void TestChunkBehindCameraInvisible(TestContext &ctx)
{
	const auto chunk = MakeChunk(0, 0, 8, 8, 8, 1u);
	auto cam = MakeCameraLookingForward(-4, -4, 100);
	cam.cameraForwardZ = 1.0f;
	cam.nearPlane = 20.0f;
	if (IsChunkVisibleCPU(chunk, cam)) {
		ctx.Fail(__LINE__, "chunk behind near plane must be invisible");
	}
}

void TestChunkBeyondMaxDistanceInvisible(TestContext &ctx)
{
	const auto chunk = MakeChunk(0, 0, 2, 2, 2, 1u);
	auto cam = MakeCameraLookingForward(-1, -1, 1000);
	cam.maxDistance = 5.0f;
	if (IsChunkVisibleCPU(chunk, cam)) {
		ctx.Fail(__LINE__, "chunk beyond maxDistance must be invisible");
	}
}

void TestChunkWithinMaxDistanceVisible(TestContext &ctx)
{
	const auto chunk = MakeChunk(0, 0, 8, 8, 8, 1u);
	auto cam = MakeCameraLookingForward(-4, -4, -10);
	cam.maxDistance = 100.0f;
	if (!IsChunkVisibleCPU(chunk, cam)) {
		ctx.Fail(__LINE__, "chunk within maxDistance must be visible");
	}
}

void TestChunkOutsideFrustumLeft(TestContext &ctx)
{
	const auto chunk = MakeChunk(-100, 0, 2, 2, 2, 1u);
	const auto cam = MakeCameraLookingForward(0, 0, -10);
	if (IsChunkVisibleCPU(chunk, cam)) {
		ctx.Fail(__LINE__, "chunk far to the left must be invisible");
	}
}

void TestChunkOutsideFrustumRight(TestContext &ctx)
{
	const auto chunk = MakeChunk(100, 0, 2, 2, 2, 1u);
	const auto cam = MakeCameraLookingForward(0, 0, -10);
	if (IsChunkVisibleCPU(chunk, cam)) {
		ctx.Fail(__LINE__, "chunk far to the right must be invisible");
	}
}

void TestChunkOutsideFrustumTop(TestContext &ctx)
{
	const auto chunk = MakeChunk(0, 100, 2, 2, 2, 1u);
	const auto cam = MakeCameraLookingForward(0, 0, -10);
	if (IsChunkVisibleCPU(chunk, cam)) {
		ctx.Fail(__LINE__, "chunk far above must be invisible");
	}
}

void TestChunkOutsideFrustumBottom(TestContext &ctx)
{
	const auto chunk = MakeChunk(0, -100, 2, 2, 2, 1u);
	const auto cam = MakeCameraLookingForward(0, 0, -10);
	if (IsChunkVisibleCPU(chunk, cam)) {
		ctx.Fail(__LINE__, "chunk far below must be invisible");
	}
}

void TestDegenerateFovNoCrash(TestContext &ctx)
{
	(void)ctx;
	const auto chunk = MakeChunk(0, 0, 8, 8, 8, 1u);
	CpuChunkCullingParams cam{};
	cam.cameraForwardZ = 1.0f;
	cam.tanHalfVerticalFov = 0.0f;
	cam.tanHalfHorizontalFov = 0.0f;
	cam.maxDistance = 0.0f;
	cam.nearPlane = 0.0f;
	(void)IsChunkVisibleCPU(chunk, cam);
}

void TestMaxDistanceZeroMeansNoDistanceCulling(TestContext &ctx)
{
	const auto chunk = MakeChunk(0, 0, 8, 8, 8, 1u);
	auto cam = MakeCameraLookingForward(-4, -4, -100000);
	cam.maxDistance = 0.0f;
	if (!IsChunkVisibleCPU(chunk, cam)) {
		ctx.Fail(__LINE__, "maxDistance=0 must disable distance culling");
	}
}

void TestChunkAtOriginVisibleFromOrigin(TestContext &ctx)
{
	const auto chunk = MakeChunk(0, 0, 8, 8, 8, 1u);
	const auto cam = MakeCameraLookingForward(0, 0, 0);
	if (!IsChunkVisibleCPU(chunk, cam)) {
		ctx.Fail(__LINE__, "chunk at camera position must be visible");
	}
}

void TestLargeChunkPartiallyInFrustumVisible(TestContext &ctx)
{
	const auto chunk = MakeChunk(-4, -4, 64, 64, 64, 1u);
	const auto cam = MakeCameraLookingForward(0, 0, -30);
	if (!IsChunkVisibleCPU(chunk, cam)) {
		ctx.Fail(__LINE__, "large chunk partially in frustum must be visible");
	}
}

} // namespace

int main()
{
	TestContext ctx{};
	TestZeroNonAirInvisible(ctx);
	TestChunkInFrontOfCameraVisible(ctx);
	TestChunkBehindCameraInvisible(ctx);
	TestChunkBeyondMaxDistanceInvisible(ctx);
	TestChunkWithinMaxDistanceVisible(ctx);
	TestChunkOutsideFrustumLeft(ctx);
	TestChunkOutsideFrustumRight(ctx);
	TestChunkOutsideFrustumTop(ctx);
	TestChunkOutsideFrustumBottom(ctx);
	TestDegenerateFovNoCrash(ctx);
	TestMaxDistanceZeroMeansNoDistanceCulling(ctx);
	TestChunkAtOriginVisibleFromOrigin(ctx);
	TestLargeChunkPartiallyInFrustumVisible(ctx);
	if (ctx.failures > 0) {
		std::fprintf(stderr, "ProjectVChunkVisibilityTests: %d failure(s)\n", ctx.failures);
		return EXIT_FAILURE;
	}
	std::puts("ProjectVChunkVisibilityTests passed");
	return EXIT_SUCCESS;
}

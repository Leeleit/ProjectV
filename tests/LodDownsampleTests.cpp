#include "voxel/VoxelLodDownsample.hpp"
#include "voxel/VoxelWorld.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct TestContext {
	int failures = 0;
	void Fail(const int line, const std::string_view message)
	{
		std::fprintf(stderr, "Test failure at line %d: %.*s\n", line, static_cast<int>(message.size()), message.data());
		++failures;
	}
};

void ExpectEqualUInt(TestContext &context, const uint32_t expected, const uint32_t actual, const int line, const std::string_view expr)
{
	if (expected != actual) {
		std::fprintf(stderr, "Test failure at line %d: %.*s (expected %u, got %u)\n", line, static_cast<int>(expr.size()), expr.data(), expected, actual);
		++context.failures;
	}
}

void ExpectEqualU8(TestContext &context, const uint8_t expected, const uint8_t actual, const int line, const std::string_view expr)
{
	if (expected != actual) {
		std::fprintf(stderr, "Test failure at line %d: %.*s (expected %u, got %u)\n", line, static_cast<int>(expr.size()), expr.data(), expected, actual);
		++context.failures;
	}
}

VoxelWorld BuildUniformWorld(const uint8_t material)
{
	// Build a minimal 8x8x8 chunked world with `material` filling every voxel.
	// We use CreateVoxelSceneWorld indirectly by constructing a VoxelWorld from scratch.
	VoxelWorld world{};
	world.scenePreset = VoxelScenePreset::VoxelLab;
	world.config.chunkSize = 8;
	world.config.floorSize = 1;
	world.config.worldTopY = 1;
	world.config.padding = 0;
	world.min = {0, 0, 0};
	world.maxExclusive = {8, 8, 8};
	world.width = 8;
	world.height = 8;
	world.depth = 8;
	world.chunkSize = 8;
	world.chunkCountX = 1;
	world.chunkCountY = 1;
	world.chunkCountZ = 1;
	world.editVersion = 0;
	world.chunks.clear();
	VoxelChunk chunk{};
	chunk.min = {0, 0, 0};
	chunk.maxExclusive = {8, 8, 8};
	chunk.rebuildQueued = true;
	chunk.isStatic = false;
	chunk.nonAirVoxelCount = material == 0u ? 0u : 8u * 8u * 8u;
	chunk.lodLevel = 0;
	world.chunks.push_back(chunk);

	world.sparseStorage.Reset(8, 8, 8);
	for (int z = 0; z < 8; ++z) {
		for (int y = 0; y < 8; ++y) {
			for (int x = 0; x < 8; ++x) {
				world.sparseStorage.SetCell(x, y, z, material);
			}
		}
	}
	return world;
}

void TestLodDownsampleStepForLod(TestContext &context)
{
	ExpectEqualUInt(context, 1u, projectv::voxel::LodDownsampleStepForLod(0u), __LINE__, "LOD 0 -> step 1");
	ExpectEqualUInt(context, 2u, projectv::voxel::LodDownsampleStepForLod(1u), __LINE__, "LOD 1 -> step 2");
	ExpectEqualUInt(context, 4u, projectv::voxel::LodDownsampleStepForLod(2u), __LINE__, "LOD 2 -> step 4");
	ExpectEqualUInt(context, 8u, projectv::voxel::LodDownsampleStepForLod(3u), __LINE__, "LOD 3 -> step 8");
}

void TestLodDownsampledExtentForLod(TestContext &context)
{
	ExpectEqualUInt(context, 8u, projectv::voxel::LodDownsampledExtentForLod(0u, 8u), __LINE__, "LOD 0 @ chunkSize 8 -> 8");
	ExpectEqualUInt(context, 4u, projectv::voxel::LodDownsampledExtentForLod(1u, 8u), __LINE__, "LOD 1 @ chunkSize 8 -> 4");
	ExpectEqualUInt(context, 2u, projectv::voxel::LodDownsampledExtentForLod(2u, 8u), __LINE__, "LOD 2 @ chunkSize 8 -> 2");
	ExpectEqualUInt(context, 1u, projectv::voxel::LodDownsampledExtentForLod(3u, 8u), __LINE__, "LOD 3 @ chunkSize 8 -> 1");
}

void TestSurfacePreserveUniformAir(TestContext &context)
{
	const VoxelWorld world = BuildUniformWorld(0u);
	std::vector<uint8_t> downsampled;
	projectv::voxel::DownsampleChunkForLodSurfacePreserve(world, 0u, 1u, downsampled);
	if (downsampled.size() != 64u) {
		std::fprintf(stderr, "Test failure at line %d: downsampled size expected 64 (got %zu)\n", __LINE__, downsampled.size());
		++context.failures;
		return;
	}
	for (const uint8_t material : downsampled) {
		ExpectEqualU8(context, 0u, material, __LINE__, "uniform Air -> Air everywhere");
	}
}

void TestSurfacePreserveUniformSolid(TestContext &context)
{
	const VoxelWorld world = BuildUniformWorld(3u);
	std::vector<uint8_t> downsampled;
	projectv::voxel::DownsampleChunkForLodSurfacePreserve(world, 0u, 1u, downsampled);
	if (downsampled.size() != 64u) {
		std::fprintf(stderr, "Test failure at line %d: downsampled size expected 64 (got %zu)\n", __LINE__, downsampled.size());
		++context.failures;
		return;
	}
	for (const uint8_t material : downsampled) {
		ExpectEqualU8(context, 3u, material, __LINE__, "uniform solid -> solid everywhere");
	}
}

void TestSurfacePreserveMixedHalf(TestContext &context)
{
	VoxelWorld world = BuildUniformWorld(0u);
	for (int z = 0; z < 8; ++z) {
		for (int y = 0; y < 8; ++y) {
			for (int x = 0; x < 4; ++x) {
				world.sparseStorage.SetCell(x, y, z, 3u);
			}
		}
	}
	std::vector<uint8_t> downsampled;
	projectv::voxel::DownsampleChunkForLodSurfacePreserve(world, 0u, 1u, downsampled);
	if (downsampled.size() != 64u) {
		std::fprintf(stderr, "Test failure at line %d: downsampled size expected 64 (got %zu)\n", __LINE__, downsampled.size());
		++context.failures;
		return;
	}
	constexpr uint8_t expected[4] = {3u, 3u, 0u, 0u};
	for (uint32_t ox = 0; ox < 4u; ++ox) {
		const uint8_t material = downsampled[ox];
		ExpectEqualU8(context, expected[ox], material, __LINE__, "mixed half: solid + Air per output");
	}
}

void TestSurfacePreserveLod0NoOp(TestContext &context)
{
	const VoxelWorld world = BuildUniformWorld(0u);
	std::vector<uint8_t> downsampled;
	projectv::voxel::DownsampleChunkForLodSurfacePreserve(world, 0u, 0u, downsampled);
	if (!downsampled.empty()) {
		std::fprintf(stderr, "Test failure at line %d: LOD 0 should not produce downsampled data (got %zu)\n", __LINE__, downsampled.size());
		++context.failures;
	}
}

void TestRunLodDownsampleJobsAir(TestContext &context)
{
	unsetenv("PROJECTV_LOD_DOWNSAMPLE");
	VoxelWorld world = BuildUniformWorld(0u);
	world.chunks[0].lodLevel = 1u;
	const uint32_t processed = projectv::voxel::RunLodDownsampleJobs(world);
	if (processed != 1u) {
		std::fprintf(stderr, "Test failure at line %d: RunLodDownsampleJobs expected 1 processed (got %u)\n", __LINE__, processed);
		++context.failures;
	}
	if (world.chunks[0].lodDownsampledNonAirCount != 0u) {
		std::fprintf(stderr, "Test failure at line %d: uniform Air -> lodDownsampledNonAirCount expected 0 (got %u)\n", __LINE__, world.chunks[0].lodDownsampledNonAirCount);
		++context.failures;
	}
}

void TestRunLodDownsampleJobsSolid(TestContext &context)
{
	unsetenv("PROJECTV_LOD_DOWNSAMPLE");
	VoxelWorld world = BuildUniformWorld(3u);
	world.chunks[0].lodLevel = 1u;
	const uint32_t processed = projectv::voxel::RunLodDownsampleJobs(world);
	if (processed != 1u) {
		std::fprintf(stderr, "Test failure at line %d: RunLodDownsampleJobs expected 1 processed (got %u)\n", __LINE__, processed);
		++context.failures;
	}
	if (world.chunks[0].lodDownsampledNonAirCount != 64u) {
		std::fprintf(stderr, "Test failure at line %d: uniform solid at LOD 1 -> lodDownsampledNonAirCount expected 64 (got %u)\n", __LINE__, world.chunks[0].lodDownsampledNonAirCount);
		++context.failures;
	}
}

void TestIsLodDownsampleEnabledEnv(TestContext &context)
{
	unsetenv("PROJECTV_LOD_DOWNSAMPLE");
	if (projectv::voxel::IsLodDownsampleEnabled()) {
		context.Fail(__LINE__, "PROJECTV_LOD_DOWNSAMPLE unset -> false");
	}
	setenv("PROJECTV_LOD_DOWNSAMPLE", "1", 1);
	if (!projectv::voxel::IsLodDownsampleEnabled()) {
		context.Fail(__LINE__, "PROJECTV_LOD_DOWNSAMPLE=1 -> true");
	}
	unsetenv("PROJECTV_LOD_DOWNSAMPLE");
}

}  // namespace

int main()
{
	TestContext context{};
	TestLodDownsampleStepForLod(context);
	TestLodDownsampledExtentForLod(context);
	TestSurfacePreserveUniformAir(context);
	TestSurfacePreserveUniformSolid(context);
	TestSurfacePreserveMixedHalf(context);
	TestSurfacePreserveLod0NoOp(context);
	TestRunLodDownsampleJobsAir(context);
	TestRunLodDownsampleJobsSolid(context);
	TestIsLodDownsampleEnabledEnv(context);

	if (context.failures > 0) {
		std::fprintf(stderr, "ProjectVLodDownsampleTests: %d failure(s)\n", context.failures);
		return 1;
	}
	std::puts("ProjectVLodDownsampleTests passed");
	return 0;
}

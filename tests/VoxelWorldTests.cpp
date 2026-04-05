#include "Types.hpp"
#include "VoxelMaterials.hpp"
#include "VoxelWorld.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
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

template <typename T>
void ExpectEqual(TestContext &context, const T &expected, const T &actual, const int line, const std::string_view expr)
{
	if (!(expected == actual)) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"%.*s (expected %lld, got %lld)",
			static_cast<int>(expr.size()),
			expr.data(),
			static_cast<long long>(expected),
			static_cast<long long>(actual));
		context.Fail(line, buffer);
	}
}

void ExpectTrue(TestContext &context, const bool condition, const int line, const std::string_view expr)
{
	if (!condition) {
		context.Fail(line, expr);
	}
}

#define EXPECT_TRUE(context, expr) ExpectTrue(context, (expr), __LINE__, #expr)
#define EXPECT_EQ(context, expected, actual) ExpectEqual(context, (expected), (actual), __LINE__, #actual)

VoxelWorld MakeTestWorld(const Int3 min, const Int3 maxExclusive, const int chunkSize)
{
	VoxelWorld world{};
	world.min = min;
	world.maxExclusive = maxExclusive;
	world.width = maxExclusive.x - min.x;
	world.height = maxExclusive.y - min.y;
	world.depth = maxExclusive.z - min.z;
	world.chunkSize = chunkSize;
	world.chunkCountX = (world.width + chunkSize - 1) / chunkSize;
	world.chunkCountY = (world.height + chunkSize - 1) / chunkSize;
	world.chunkCountZ = (world.depth + chunkSize - 1) / chunkSize;
	world.voxels.resize(
		static_cast<size_t>(world.width) *
			static_cast<size_t>(world.height) *
			static_cast<size_t>(world.depth),
		static_cast<uint8_t>(VoxelMaterial::Air));
	world.chunks.resize(
		static_cast<size_t>(world.chunkCountX) *
		static_cast<size_t>(world.chunkCountY) *
		static_cast<size_t>(world.chunkCountZ));

	for (int chunkZ = 0; chunkZ < world.chunkCountZ; ++chunkZ) {
		for (int chunkY = 0; chunkY < world.chunkCountY; ++chunkY) {
			for (int chunkX = 0; chunkX < world.chunkCountX; ++chunkX) {
				const size_t chunkIndex = GetVoxelChunkIndex(world, {chunkX, chunkY, chunkZ});
				VoxelChunk &chunk = world.chunks[chunkIndex];
				chunk.min = {
					world.min.x + chunkX * world.chunkSize,
					world.min.y + chunkY * world.chunkSize,
					world.min.z + chunkZ * world.chunkSize,
				};
				chunk.maxExclusive = {
					chunk.min.x + std::min(world.chunkSize, world.maxExclusive.x - chunk.min.x),
					chunk.min.y + std::min(world.chunkSize, world.maxExclusive.y - chunk.min.y),
					chunk.min.z + std::min(world.chunkSize, world.maxExclusive.z - chunk.min.z),
				};
				chunk.rebuildQueued = false;
				chunk.nonAirVoxelCount = 0;
			}
		}
	}

	return world;
}

void ExpectChunkBoundsAreValid(TestContext &context, const VoxelWorld &world)
{
	EXPECT_TRUE(context, world.chunkSize > 0);
	EXPECT_EQ(
		context,
		static_cast<size_t>(world.chunkCountX) * static_cast<size_t>(world.chunkCountY) * static_cast<size_t>(world.chunkCountZ),
		world.chunks.size());

	for (const VoxelChunk &chunk : world.chunks) {
		EXPECT_TRUE(context, chunk.min.x >= world.min.x);
		EXPECT_TRUE(context, chunk.min.y >= world.min.y);
		EXPECT_TRUE(context, chunk.min.z >= world.min.z);
		EXPECT_TRUE(context, chunk.maxExclusive.x <= world.maxExclusive.x);
		EXPECT_TRUE(context, chunk.maxExclusive.y <= world.maxExclusive.y);
		EXPECT_TRUE(context, chunk.maxExclusive.z <= world.maxExclusive.z);
		EXPECT_TRUE(context, chunk.min.x < chunk.maxExclusive.x);
		EXPECT_TRUE(context, chunk.min.y < chunk.maxExclusive.y);
		EXPECT_TRUE(context, chunk.min.z < chunk.maxExclusive.z);
		EXPECT_TRUE(context, chunk.maxExclusive.x - chunk.min.x <= world.chunkSize);
		EXPECT_TRUE(context, chunk.maxExclusive.y - chunk.min.y <= world.chunkSize);
		EXPECT_TRUE(context, chunk.maxExclusive.z - chunk.min.z <= world.chunkSize);
	}
}

void TestWorldBoundsAndChunkIndexing(TestContext &context)
{
	const VoxelWorld world = MakeTestWorld({-4, 2, -4}, {12, 10, 12}, 8);
	ExpectChunkBoundsAreValid(context, world);

	EXPECT_TRUE(context, IsInsideVoxelWorld(world, {-4, 2, -4}));
	EXPECT_TRUE(context, IsInsideVoxelWorld(world, {11, 9, 11}));
	EXPECT_TRUE(context, !IsInsideVoxelWorld(world, {12, 9, 11}));
	EXPECT_TRUE(context, !IsInsideVoxelWorld(world, {11, 10, 11}));
	EXPECT_TRUE(context, !IsInsideVoxelWorld(world, {11, 9, 12}));
	EXPECT_TRUE(context, !IsInsideVoxelWorld(world, {-5, 2, -4}));

	EXPECT_EQ(context, static_cast<size_t>(0), GetVoxelChunkIndex(world, {0, 0, 0}));
	EXPECT_EQ(context, static_cast<size_t>(1), GetVoxelChunkIndex(world, {1, 0, 0}));
	EXPECT_EQ(context, static_cast<size_t>(2), GetVoxelChunkIndex(world, {0, 0, 1}));
	EXPECT_EQ(context, static_cast<size_t>(3), GetVoxelChunkIndex(world, {1, 0, 1}));
}

void TestMarkVoxelRegionDirtyQueuesExpectedChunks(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {16, 16, 16}, 8);
	MarkVoxelRegionDirty(world, {7, 7, 7}, {9, 9, 9});

	EXPECT_EQ(context, static_cast<uint32_t>(8), CountDirtyVoxelChunks(world));
	EXPECT_EQ(context, static_cast<size_t>(8), world.pendingChunkRebuildIndices.size());

	MarkVoxelRegionDirty(world, {7, 7, 7}, {9, 9, 9});
	EXPECT_EQ(context, static_cast<uint32_t>(8), CountDirtyVoxelChunks(world));
	EXPECT_EQ(context, static_cast<size_t>(8), world.pendingChunkRebuildIndices.size());

	std::vector<size_t> rebuildRequests;
	CollectDirtyVoxelChunkRebuildRequests(world, &rebuildRequests);
	EXPECT_EQ(context, static_cast<size_t>(8), rebuildRequests.size());
	EXPECT_EQ(context, static_cast<size_t>(0), world.pendingChunkRebuildIndices.size());
	EXPECT_EQ(context, static_cast<uint32_t>(8), CountDirtyVoxelChunks(world));

	CommitDirtyVoxelChunkRebuildRequests(world, rebuildRequests);
	EXPECT_EQ(context, static_cast<uint32_t>(0), CountDirtyVoxelChunks(world));
	for (const VoxelChunk &chunk : world.chunks) {
		EXPECT_TRUE(context, !chunk.rebuildQueued);
	}
}

void TestSetVoxelMaterialTracksCountsAndQueuesRebuild(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Fluid);

	EXPECT_EQ(context, static_cast<uint32_t>(1), CountVoxelsByMaterial(world, VoxelMaterial::Fluid));
	EXPECT_EQ(context, static_cast<uint32_t>(1), world.stats.nonAirVoxelCount);
	EXPECT_EQ(context, static_cast<uint32_t>(1), CountActiveVoxelChunks(world));
	EXPECT_EQ(context, static_cast<uint32_t>(1), CountDirtyVoxelChunks(world));
	EXPECT_EQ(context, static_cast<size_t>(1), world.pendingChunkRebuildIndices.size());

	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Fluid);
	EXPECT_EQ(context, static_cast<uint32_t>(1), CountDirtyVoxelChunks(world));
	EXPECT_EQ(context, static_cast<size_t>(1), world.pendingChunkRebuildIndices.size());

	std::vector<size_t> rebuildRequests;
	CollectDirtyVoxelChunkRebuildRequests(world, &rebuildRequests);
	CommitDirtyVoxelChunkRebuildRequests(world, rebuildRequests);
	EXPECT_EQ(context, static_cast<uint32_t>(0), CountDirtyVoxelChunks(world));

	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Air);
	EXPECT_EQ(context, static_cast<uint32_t>(0), CountVoxelsByMaterial(world, VoxelMaterial::Fluid));
	EXPECT_EQ(context, static_cast<uint32_t>(0), world.stats.nonAirVoxelCount);
	EXPECT_EQ(context, static_cast<uint32_t>(0), CountActiveVoxelChunks(world));
	EXPECT_EQ(context, static_cast<uint32_t>(1), CountDirtyVoxelChunks(world));
	EXPECT_EQ(context, static_cast<size_t>(1), world.pendingChunkRebuildIndices.size());
}

void TestMarkAllVoxelChunksDirtyResetsQueue(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {16, 8, 16}, 8);
	MarkVoxelChunkDirty(world, {1, 1, 1});
	EXPECT_EQ(context, static_cast<uint32_t>(1), CountDirtyVoxelChunks(world));

	MarkAllVoxelChunksDirty(&world);
	EXPECT_EQ(context, static_cast<uint32_t>(4), CountDirtyVoxelChunks(world));
	EXPECT_EQ(context, static_cast<size_t>(4), world.pendingChunkRebuildIndices.size());
	for (const VoxelChunk &chunk : world.chunks) {
		EXPECT_TRUE(context, chunk.rebuildQueued);
	}
}

void TestVoxelMaterialVisuals(TestContext &context)
{
	const VoxelMaterialVisual air = GetVoxelMaterialVisual(VoxelMaterial::Air);
	const VoxelMaterialVisual glass = GetVoxelMaterialVisual(VoxelMaterial::Glass);

	EXPECT_TRUE(context, air.baseColor[3] == 0.0f);
	EXPECT_TRUE(context, glass.baseColor[3] < 1.0f);
	EXPECT_TRUE(context, glass.specular > 0.0f);
}
} // namespace

int main()
{
	TestContext context{};

	TestWorldBoundsAndChunkIndexing(context);
	TestMarkVoxelRegionDirtyQueuesExpectedChunks(context);
	TestSetVoxelMaterialTracksCountsAndQueuesRebuild(context);
	TestMarkAllVoxelChunksDirtyResetsQueue(context);
	TestVoxelMaterialVisuals(context);

	if (context.failures != 0) {
		return EXIT_FAILURE;
	}

	std::puts("ProjectVTests passed");
	return EXIT_SUCCESS;
}

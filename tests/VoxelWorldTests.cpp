#include "DebugHud.hpp"
#include "Types.hpp"
#include "VoxelInteraction.hpp"
#include "VoxelMaterials.hpp"
#include "VoxelRaycast.hpp"
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
				// ReSharper disable once CppUseStructuredBinding
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

void TestSetVoxelMaterialMarksNeighborChunksDirtyAtBoundaries(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {16, 16, 16}, 8);

	SetVoxelMaterial(world, {7, 7, 7}, VoxelMaterial::Glass);

	EXPECT_EQ(context, static_cast<uint32_t>(8), CountDirtyVoxelChunks(world));
	EXPECT_EQ(context, static_cast<size_t>(8), world.pendingChunkRebuildIndices.size());
	std::ranges::sort(world.pendingChunkRebuildIndices);
	for (size_t chunkIndex = 0; chunkIndex < 8; ++chunkIndex) {
		EXPECT_EQ(context, chunkIndex, world.pendingChunkRebuildIndices[chunkIndex]);
	}
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

void ExpectInt3Equal(
	TestContext &context,
	const Int3 expected,
	const Int3 actual,
	const int line,
	const std::string_view expr)
{
	if (expected.x == actual.x && expected.y == actual.y && expected.z == actual.z) {
		return;
	}

	char buffer[256]{};
	std::snprintf(
		buffer,
		sizeof(buffer),
		"%.*s (expected {%d, %d, %d}, got {%d, %d, %d})",
		static_cast<int>(expr.size()),
		expr.data(),
		expected.x,
		expected.y,
		expected.z,
		actual.x,
		actual.y,
		actual.z);
	context.Fail(line, buffer);
}

#undef EXPECT_EQ
#define EXPECT_EQ(context, expected, actual) ExpectEqual(context, (expected), (actual), __LINE__, #actual)
#define EXPECT_INT3_EQ(context, expected, actual) ExpectInt3Equal(context, (expected), (actual), __LINE__, #actual)

void ExpectNear(
	TestContext &context,
	const float expected,
	const float actual,
	const int line,
	const std::string_view expr)
{
	constexpr float kFloatTolerance = 0.001f;
	if (std::abs(expected - actual) <= kFloatTolerance) {
		return;
	}

	char buffer[256]{};
	std::snprintf(
		buffer,
		sizeof(buffer),
		"%.*s (expected %.3f, got %.3f)",
		static_cast<int>(expr.size()),
		expr.data(),
		expected,
		actual);
	context.Fail(line, buffer);
}

#define EXPECT_NEAR(context, expected, actual) ExpectNear(context, (expected), (actual), __LINE__, #actual)

void TestVoxelRaycastHitsSolidVoxelAndReturnsPlacementCell(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Glass);
	world.pendingChunkRebuildIndices.clear();
	world.stats.dirtyChunkCount = 0;
	for (VoxelChunk &chunk : world.chunks) {
		chunk.rebuildQueued = false;
	}

	const auto [hasHit, hasPlacementVoxel, voxel, placementVoxel, hitNormal, material, distance] = RaycastVoxelWorld(
		world,
		{1.5f, 1.5f, 4.5f},
		{0.0f, 0.0f, -1.0f},
		10.0f);

	EXPECT_TRUE(context, hasHit);
	EXPECT_TRUE(context, hasPlacementVoxel);
	EXPECT_EQ(context, VoxelMaterial::Glass, material);
	EXPECT_INT3_EQ(context, (Int3{1, 1, 1}), voxel);
	EXPECT_INT3_EQ(context, (Int3{1, 1, 2}), placementVoxel);
	EXPECT_INT3_EQ(context, (Int3{0, 0, 1}), hitNormal);
	EXPECT_NEAR(context, 2.5f, distance);
}

void TestVoxelRaycastStopsAtWorldBoundaryWithoutPlacementCell(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {4, 4, 4}, 4);
	SetVoxelMaterial(world, {0, 1, 1}, VoxelMaterial::Fluid);
	world.pendingChunkRebuildIndices.clear();
	world.stats.dirtyChunkCount = 0;
	for (VoxelChunk &chunk : world.chunks) {
		chunk.rebuildQueued = false;
	}

	const auto [hasHit, hasPlacementVoxel, voxel, placementVoxel, hitNormal, material, distance] = RaycastVoxelWorld(
		world,
		{-2.0f, 1.5f, 1.5f},
		{1.0f, 0.0f, 0.0f},
		10.0f);

	EXPECT_TRUE(context, hasHit);
	EXPECT_TRUE(context, !hasPlacementVoxel);
	EXPECT_EQ(context, VoxelMaterial::Fluid, material);
	EXPECT_INT3_EQ(context, (Int3{0, 1, 1}), voxel);
	EXPECT_INT3_EQ(context, (Int3{0, 0, 0}), placementVoxel);
	EXPECT_INT3_EQ(context, (Int3{0, 0, 0}), hitNormal);
	EXPECT_NEAR(context, 2.0f, distance);
}

void TestVoxelRaycastMissesWhenNoSolidVoxelIsReached(TestContext &context)
{
	const VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	const VoxelRaycastHit hit = RaycastVoxelWorld(
		world,
		{1.5f, 1.5f, 1.5f},
		{1.0f, 0.0f, 0.0f},
		4.0f);

	EXPECT_TRUE(context, !hit.hasHit);
	EXPECT_TRUE(context, !hit.hasPlacementVoxel);
	EXPECT_EQ(context, VoxelMaterial::Air, hit.material);
}

CameraState MakeTestCamera(const std::array<float, 3> &position)
{
	CameraState camera{};
	camera.position = position;
	camera.yawRadians = 0.0f;
	camera.pitchRadians = 0.0f;
	return camera;
}

void ResetDirtyFlags(VoxelWorld &world)
{
	world.pendingChunkRebuildIndices.clear();
	world.stats.dirtyChunkCount = 0;
	for (VoxelChunk &chunk : world.chunks) {
		chunk.rebuildQueued = false;
	}
}

void TestUpdateVoxelInteractionRemovesTargetedBlock(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Glass);
	ResetDirtyFlags(world);

	InputState input{};
	input.removePressed = true;
	InteractionState interaction{};

	UpdateVoxelInteraction(
		MakeTestCamera({1.5f, 1.5f, 4.5f}),
		&input,
		&world,
		&interaction);

	EXPECT_EQ(context, VoxelMaterial::Air, GetVoxelMaterial(world, {1, 1, 1}));
	EXPECT_TRUE(context, !input.removePressed);
	EXPECT_TRUE(context, !interaction.selection.hasHit);
	EXPECT_EQ(context, static_cast<uint32_t>(1), CountDirtyVoxelChunks(world));
}

void TestUpdateVoxelInteractionPlacesConfiguredBlock(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Glass);
	ResetDirtyFlags(world);

	InputState input{};
	input.placePressed = true;
	InteractionState interaction{};
	interaction.placementMaterial = VoxelMaterial::FloorWhite;

	UpdateVoxelInteraction(
		MakeTestCamera({1.5f, 1.5f, 4.5f}),
		&input,
		&world,
		&interaction);

	EXPECT_EQ(context, VoxelMaterial::FloorWhite, GetVoxelMaterial(world, {1, 1, 2}));
	EXPECT_TRUE(context, !input.placePressed);
	EXPECT_TRUE(context, interaction.selection.hasHit);
	EXPECT_EQ(context, VoxelMaterial::FloorWhite, interaction.selection.targetMaterial);
	EXPECT_INT3_EQ(context, (Int3{1, 1, 2}), interaction.selection.targetVoxel);
	EXPECT_EQ(context, static_cast<uint32_t>(1), CountDirtyVoxelChunks(world));
}

void TestBuildDebugHudVerticesReturnsZeroWhenHidden(TestContext &context)
{
	std::vector<DebugHudVertex> vertices(64);
	const uint32_t vertexCount = BuildDebugHudVertices(
		DebugStats{},
		CameraState{},
		InteractionState{},
		false,
		VkExtent2D{1280, 720},
		vertices.data(),
		static_cast<uint32_t>(vertices.size()));

	EXPECT_EQ(context, 0u, vertexCount);
}

void TestBuildDebugHudVerticesProducesGeometryWhenVisible(TestContext &context)
{
	std::vector<DebugHudVertex> vertices(4096);
	DebugStats stats{};
	stats.framesPerSecond = 120.0f;
	stats.frameTimeMilliseconds = 8.33f;
	stats.simulationStepsLastFrame = 1;
	stats.sceneTriangleCount = 42;
	stats.dirtyChunkCount = 2;
	stats.activeChunkCount = 4;
	stats.nonAirVoxelCount = 128;
	stats.glassVoxelCount = 16;
	stats.fluidVoxelCount = 8;
	stats.floorVoxelCount = 104;
	stats.sceneMemoryBytes = 4096;

	InteractionState interaction{};
	interaction.selection.hasHit = true;
	interaction.selection.hasPlacementVoxel = true;
	interaction.selection.targetVoxel = {1, 2, 3};
	interaction.selection.placementVoxel = {1, 2, 4};
	interaction.selection.targetMaterial = VoxelMaterial::Glass;
	interaction.selection.hitDistance = 3.5f;

	const uint32_t vertexCount = BuildDebugHudVertices(
		stats,
		MakeTestCamera({2.0f, 3.0f, 4.0f}),
		interaction,
		true,
		VkExtent2D{1280, 720},
		vertices.data(),
		static_cast<uint32_t>(vertices.size()));

	EXPECT_TRUE(context, vertexCount > 6u);
	EXPECT_TRUE(context, vertexCount <= static_cast<uint32_t>(vertices.size()));
	EXPECT_TRUE(context, vertices[0].positionNdc[0] >= -1.0f);
	EXPECT_TRUE(context, vertices[0].positionNdc[0] <= 1.0f);
	EXPECT_TRUE(context, vertices[0].positionNdc[1] >= -1.0f);
	EXPECT_TRUE(context, vertices[0].positionNdc[1] <= 1.0f);
	EXPECT_TRUE(context, vertices[0].positionNdc[0] < 0.0f);
	EXPECT_TRUE(context, vertices[0].positionNdc[1] < 0.0f);
}
} // namespace

int main()
{
	TestContext context{};

	TestWorldBoundsAndChunkIndexing(context);
	TestMarkVoxelRegionDirtyQueuesExpectedChunks(context);
	TestSetVoxelMaterialTracksCountsAndQueuesRebuild(context);
	TestSetVoxelMaterialMarksNeighborChunksDirtyAtBoundaries(context);
	TestMarkAllVoxelChunksDirtyResetsQueue(context);
	TestVoxelMaterialVisuals(context);
	TestVoxelRaycastHitsSolidVoxelAndReturnsPlacementCell(context);
	TestVoxelRaycastStopsAtWorldBoundaryWithoutPlacementCell(context);
	TestVoxelRaycastMissesWhenNoSolidVoxelIsReached(context);
	TestUpdateVoxelInteractionRemovesTargetedBlock(context);
	TestUpdateVoxelInteractionPlacesConfiguredBlock(context);
	TestBuildDebugHudVerticesReturnsZeroWhenHidden(context);
	TestBuildDebugHudVerticesProducesGeometryWhenVisible(context);

	if (context.failures != 0) {
		return EXIT_FAILURE;
	}

	std::puts("ProjectVTests passed");
	return EXIT_SUCCESS;
}

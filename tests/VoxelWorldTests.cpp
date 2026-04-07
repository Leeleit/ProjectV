#include "app/AppUpdate.hpp"
#include "app/Camera.hpp"
#include "app/InputActions.hpp"
#include "core/RuntimeProbe.hpp"
#include "core/Types.hpp"
#include "debug/DebugHud.hpp"
#include "ecs/EcsWorld.hpp"
#include "physics/PhysicsWorld.hpp"
#include "platform/PlatformEvents.hpp"
#include "render/SceneResources.hpp"
#include "render/vulkan/VulkanResult.hpp"
#include "voxel/VoxelInteraction.hpp"
#include "voxel/VoxelMaterials.hpp"
#include "voxel/VoxelRaycast.hpp"
#include "voxel/VoxelWorld.hpp"

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

bool ColorsMatch(
	const std::array<float, 4> &expected,
	const std::array<float, 4> &actual)
{
	constexpr float kColorEpsilon = 0.0001f;
	for (size_t index = 0; index < expected.size(); ++index) {
		if (std::abs(expected[index] - actual[index]) > kColorEpsilon) {
			return false;
		}
	}

	return true;
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

PackedSceneChunkDescriptor MakePackedSceneChunkDescriptor(
	const Int3 min,
	const Int3 maxExclusive,
	const uint32_t nonAirVoxelCount = 1u)
{
	const uint32_t extentX = static_cast<uint32_t>(maxExclusive.x - min.x);
	const uint32_t extentY = static_cast<uint32_t>(maxExclusive.y - min.y);
	const uint32_t extentZ = static_cast<uint32_t>(maxExclusive.z - min.z);
	const uint32_t voxelCount = extentX * extentY * extentZ;
	return {
		.chunkOrigin = {min.x, min.y, min.z, 0},
		.chunkExtentAndNonAir = {extentX, extentY, extentZ, nonAirVoxelCount},
		.voxelDataInfo = {0u, voxelCount, (voxelCount + 3u) / 4u, 0u},
		.drawRanges = {0u, 0u, 0u, 0u},
	};
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

void TestVoxelScenePresetParsingAcceptsCanonicalAndFlexibleNames(TestContext &context)
{
	VoxelScenePreset preset = VoxelScenePreset::VoxelLab;
	EXPECT_TRUE(context, TryParseVoxelScenePreset("VoxelLab", &preset));
	EXPECT_EQ(context, VoxelScenePreset::VoxelLab, preset);
	EXPECT_TRUE(context, TryParseVoxelScenePreset("flat_benchmark", &preset));
	EXPECT_EQ(context, VoxelScenePreset::FlatBenchmark, preset);
	EXPECT_TRUE(context, TryParseVoxelScenePreset("TRANSPARENCY-STRESS", &preset));
	EXPECT_EQ(context, VoxelScenePreset::TransparencyStress, preset);
	EXPECT_TRUE(context, TryParseVoxelScenePreset("chunk grid", &preset));
	EXPECT_EQ(context, VoxelScenePreset::ChunkGrid, preset);
	EXPECT_TRUE(context, TryParseVoxelScenePreset("meshingstress", &preset));
	EXPECT_EQ(context, VoxelScenePreset::MeshingStress, preset);
	EXPECT_TRUE(context, !TryParseVoxelScenePreset("not_a_scene", &preset));
	EXPECT_TRUE(context, std::string_view(VoxelScenePresetToString(VoxelScenePreset::MeshingStress)) == "MeshingStress");
}

void TestCreateVoxelSceneWorldBuildsExpectedBaselineScenes(TestContext &context)
{
	AppState state{};

	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::VoxelLab));
	EXPECT_TRUE(context, state.world.voxelWorld != nullptr);
	EXPECT_EQ(context, VoxelScenePreset::VoxelLab, state.world.voxelWorld->scenePreset);
	EXPECT_TRUE(context, state.world.voxelWorld->config.worldTopY >= 14);
	EXPECT_TRUE(context, state.world.voxelWorld->stats.glassVoxelCount > 0);
	EXPECT_TRUE(context, state.world.voxelWorld->stats.fluidVoxelCount > 0);

	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::FlatBenchmark));
	EXPECT_TRUE(context, state.world.voxelWorld != nullptr);
	EXPECT_EQ(context, VoxelScenePreset::FlatBenchmark, state.world.voxelWorld->scenePreset);
	EXPECT_TRUE(context, state.world.voxelWorld->stats.floorWhiteVoxelCount > 0);
	EXPECT_EQ(context, 0u, state.world.voxelWorld->stats.glassVoxelCount);
	EXPECT_EQ(context, 0u, state.world.voxelWorld->stats.fluidVoxelCount);

	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::TransparencyStress));
	EXPECT_EQ(context, VoxelScenePreset::TransparencyStress, state.world.voxelWorld->scenePreset);
	EXPECT_TRUE(context, state.world.voxelWorld->stats.glassVoxelCount > 0);
	EXPECT_EQ(context, 0u, state.world.voxelWorld->stats.fluidVoxelCount);

	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::MeshingStress));
	EXPECT_EQ(context, VoxelScenePreset::MeshingStress, state.world.voxelWorld->scenePreset);
	EXPECT_TRUE(context, state.world.voxelWorld->stats.nonAirVoxelCount > state.world.voxelWorld->stats.floorWhiteVoxelCount);
}

void TestCreateVoxelSceneWorldReadsEnvironmentPreset(TestContext &context)
{
	AppState state{};
	SDL_setenv_unsafe("PROJECTV_SCENE_PRESET", "ChunkGrid", 1);

	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state));
	EXPECT_TRUE(context, state.world.voxelWorld != nullptr);
	EXPECT_EQ(context, VoxelScenePreset::ChunkGrid, state.world.voxelWorld->scenePreset);

	SDL_unsetenv_unsafe("PROJECTV_SCENE_PRESET");
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

void TestSetVoxelMaterialMarksOnlyFaceSharingNeighborChunksDirty(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {16, 16, 16}, 8);

	SetVoxelMaterial(world, {7, 4, 4}, VoxelMaterial::Glass);

	EXPECT_EQ(context, static_cast<uint32_t>(2), CountDirtyVoxelChunks(world));
	EXPECT_EQ(context, static_cast<size_t>(2), world.pendingChunkRebuildIndices.size());
	std::ranges::sort(world.pendingChunkRebuildIndices);
	EXPECT_EQ(context, static_cast<size_t>(0), world.pendingChunkRebuildIndices[0]);
	EXPECT_EQ(context, static_cast<size_t>(1), world.pendingChunkRebuildIndices[1]);
}

void TestSetVoxelMaterialDoesNotQueueMissingWorldNeighbors(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {16, 16, 16}, 8);

	SetVoxelMaterial(world, {0, 0, 0}, VoxelMaterial::Glass);

	EXPECT_EQ(context, static_cast<uint32_t>(1), CountDirtyVoxelChunks(world));
	EXPECT_EQ(context, static_cast<size_t>(1), world.pendingChunkRebuildIndices.size());
	EXPECT_EQ(context, static_cast<size_t>(0), world.pendingChunkRebuildIndices[0]);
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

void TestSwapchainRefreshWindowEventClassification(TestContext &context)
{
	EXPECT_TRUE(context, ShouldRequestSwapchainRefreshForWindowEvent(SDL_EVENT_WINDOW_RESIZED));
	EXPECT_TRUE(context, ShouldRequestSwapchainRefreshForWindowEvent(SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED));
	EXPECT_TRUE(context, ShouldRequestSwapchainRefreshForWindowEvent(SDL_EVENT_WINDOW_MINIMIZED));
	EXPECT_TRUE(context, ShouldRequestSwapchainRefreshForWindowEvent(SDL_EVENT_WINDOW_MAXIMIZED));
	EXPECT_TRUE(context, ShouldRequestSwapchainRefreshForWindowEvent(SDL_EVENT_WINDOW_RESTORED));
	EXPECT_TRUE(context, ShouldRequestSwapchainRefreshForWindowEvent(SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED));
	EXPECT_TRUE(context, ShouldRequestSwapchainRefreshForWindowEvent(SDL_EVENT_WINDOW_ENTER_FULLSCREEN));
	EXPECT_TRUE(context, ShouldRequestSwapchainRefreshForWindowEvent(SDL_EVENT_WINDOW_LEAVE_FULLSCREEN));
	EXPECT_TRUE(context, !ShouldRequestSwapchainRefreshForWindowEvent(SDL_EVENT_WINDOW_EXPOSED));
	EXPECT_TRUE(context, !ShouldRequestSwapchainRefreshForWindowEvent(SDL_EVENT_WINDOW_HIDDEN));
	EXPECT_TRUE(context, !ShouldRequestSwapchainRefreshForWindowEvent(SDL_EVENT_KEY_DOWN));
}

void TestVkResultToStringCoversCommonRuntimeResults(TestContext &context)
{
	EXPECT_TRUE(context, std::string_view(VkResultToString(VK_SUCCESS)) == "VK_SUCCESS");
	EXPECT_TRUE(context, std::string_view(VkResultToString(VK_ERROR_OUT_OF_DATE_KHR)) == "VK_ERROR_OUT_OF_DATE_KHR");
	EXPECT_TRUE(context, std::string_view(VkResultToString(VK_SUBOPTIMAL_KHR)) == "VK_SUBOPTIMAL_KHR");
	EXPECT_TRUE(context, std::string_view(VkResultToString(static_cast<VkResult>(0x7fffffff))) == "VK_RESULT_UNKNOWN");
}

void TestInitFailureStageParsing(TestContext &context)
{
	InitFailureStage stage = InitFailureStage::None;
	EXPECT_TRUE(context, TryParseInitFailureStage("after_bootstrap", &stage));
	EXPECT_EQ(context, InitFailureStage::AfterBootstrap, stage);
	EXPECT_TRUE(context, TryParseInitFailureStage("after_scene_resources", &stage));
	EXPECT_EQ(context, InitFailureStage::AfterSceneResources, stage);
	EXPECT_TRUE(context, TryParseInitFailureStage("before_voxel_meshing_pipeline", &stage));
	EXPECT_EQ(context, InitFailureStage::BeforeVoxelMeshingPipeline, stage);
	EXPECT_TRUE(context, !TryParseInitFailureStage("unknown", &stage));
	EXPECT_EQ(context, InitFailureStage::None, stage);
	EXPECT_TRUE(context, std::string_view(InitFailureStageToString(InitFailureStage::AfterWorld)) == "after_world");
	EXPECT_TRUE(context, std::string_view(InitFailureStageToString(InitFailureStage::None)) == "none");
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

void SendKeyEvent(
	InputState *input,
	const Uint32 eventType,
	const SDL_Scancode scancode,
	const bool repeat = false,
	const Uint64 timestamp = 0)
{
	SDL_Event event{};
	event.type = eventType;
	event.key.scancode = scancode;
	event.key.repeat = repeat;
	event.key.timestamp = timestamp;
	HandleInputActionEvent(*input, &event);
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

VoxelWorld MakeWalkTestWorld()
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	for (int z = world.min.z; z < world.maxExclusive.z; ++z) {
		for (int x = world.min.x; x < world.maxExclusive.x; ++x) {
			SetVoxelMaterial(world, {x, 0, z}, VoxelMaterial::FloorWhite);
		}
	}
	ResetDirtyFlags(world);
	return world;
}

void TestInputActionBindingsTrackPressedAndReleasedKeys(TestContext &context)
{
	InputState input{};
	InitializeInputState(input);

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	EXPECT_TRUE(context, IsInputActionDown(input, InputAction::MoveForward));
	EXPECT_TRUE(context, ConsumeInputActionPressed(input, InputAction::MoveForward));
	EXPECT_TRUE(context, !ConsumeInputActionPressed(input, InputAction::MoveForward));

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W, true);
	EXPECT_TRUE(context, IsInputActionDown(input, InputAction::MoveForward));
	EXPECT_TRUE(context, !ConsumeInputActionPressed(input, InputAction::MoveForward));

	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_W);
	EXPECT_TRUE(context, !IsInputActionDown(input, InputAction::MoveForward));

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	EXPECT_TRUE(context, IsInputActionDown(input, InputAction::MoveDown));
	EXPECT_TRUE(context, ConsumeInputActionPressed(input, InputAction::MoveDown));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_RSHIFT);
	EXPECT_TRUE(context, !IsInputActionDown(input, InputAction::MoveDown));

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_F4);
	EXPECT_TRUE(context, ConsumeInputActionPressed(input, InputAction::ToggleControlMode));

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_F5);
	EXPECT_TRUE(context, ConsumeInputActionPressed(input, InputAction::CycleScenePreset));

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE, false, SDL_MS_TO_NS(100));
	EXPECT_TRUE(context, ConsumeInputActionPressed(input, InputAction::MoveUp));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE, false, SDL_MS_TO_NS(160));
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE, false, SDL_MS_TO_NS(320));
	EXPECT_TRUE(context, ConsumeInputActionPressed(input, InputAction::ToggleWalkCreativeMode));
	EXPECT_TRUE(context, ConsumeInputActionPressed(input, InputAction::MoveUp));
}

void TestConsumeCameraLookInputAllowsNearVerticalPitch(TestContext &context)
{
	CameraState camera{};
	InputState input{};

	input.mouseDeltaY = -10000.0f;
	ConsumeCameraLookInput(&camera, &input);
	EXPECT_NEAR(context, 1.553343f, camera.pitchRadians);

	input.mouseDeltaY = 10000.0f;
	ConsumeCameraLookInput(&camera, &input);
	EXPECT_NEAR(context, -1.553343f, camera.pitchRadians);
}

void TestTickCameraUsesActionStateAndSpeedModifiers(TestContext &context)
{
	{
		InputState input{};
		InitializeInputState(input);
		SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);

		CameraState camera = MakeTestCamera({0.0f, 0.0f, 0.0f});
		TickCamera(&camera, input, 1.0f);
		EXPECT_NEAR(context, -10.0f, camera.position[2]);
	}

	{
		InputState input{};
		InitializeInputState(input);
		SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);

		CameraState camera = MakeTestCamera({0.0f, 5.0f, 0.0f});
		camera.pitchRadians = 0.9f;
		TickCamera(&camera, input, 1.0f);
		EXPECT_NEAR(context, 5.0f, camera.position[1]);
		EXPECT_TRUE(context, camera.position[2] < 0.0f);
	}

	{
		InputState input{};
		InitializeInputState(input);
		SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
		SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_LCTRL);

		CameraState camera = MakeTestCamera({0.0f, 0.0f, 0.0f});
		TickCamera(&camera, input, 1.0f);
		EXPECT_NEAR(context, -30.0f, camera.position[2]);
	}

	{
		InputState input{};
		InitializeInputState(input);
		SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
		SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_LALT);

		CameraState camera = MakeTestCamera({0.0f, 0.0f, 0.0f});
		TickCamera(&camera, input, 1.0f);
		EXPECT_NEAR(context, -2.5f, camera.position[2]);
	}

	{
		InputState input{};
		InitializeInputState(input);
		SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);

		CameraState camera = MakeTestCamera({0.0f, 0.0f, 0.0f});
		TickCamera(&camera, input, 1.0f);
		EXPECT_NEAR(context, -10.0f, camera.position[1]);
	}
}

void TestHandleCameraEventIgnoresLookInputWithoutRelativeMouseMode(TestContext &context)
{
	CameraState camera{};
	InputState input{};
	InitializeInputState(input);
	input.relativeMouseModeEnabled = false;

	SDL_Event event{};
	event.type = SDL_EVENT_MOUSE_MOTION;
	event.motion.xrel = 12.0f;
	event.motion.yrel = -8.0f;
	HandleCameraEvent(&camera, &input, &event);

	EXPECT_NEAR(context, 0.0f, input.mouseDeltaX);
	EXPECT_NEAR(context, 0.0f, input.mouseDeltaY);
}

void TestSceneChunkVisibilityUsesFrustumAndDistanceCulling(TestContext &context)
{
	const CameraState camera = MakeTestCamera({4.0f, 4.0f, 20.0f});
	const ChunkCullingParameters parameters = BuildChunkCullingParameters(camera, {1280u, 720u}, 64.0f);

	EXPECT_TRUE(context, IsSceneChunkVisible(
							 MakePackedSceneChunkDescriptor({0, 0, 0}, {8, 8, 8}),
							 parameters));
	EXPECT_TRUE(context, !IsSceneChunkVisible(
							 MakePackedSceneChunkDescriptor({0, 0, 28}, {8, 8, 36}),
							 parameters));
	EXPECT_TRUE(context, !IsSceneChunkVisible(
							 MakePackedSceneChunkDescriptor({0, 0, -84}, {8, 8, -76}),
							 parameters));
	EXPECT_TRUE(context, !IsSceneChunkVisible(
							 MakePackedSceneChunkDescriptor({80, 0, 0}, {88, 8, 8}),
							 parameters));
}

void TestSceneChunkVisibilityKeepsChunksVisibleAtFrustumEdges(TestContext &context)
{
	CameraState camera = MakeTestCamera({20.0f, 18.0f, 20.0f});
	camera.yawRadians = -0.45f;
	camera.pitchRadians = -1.15f;
	const ChunkCullingParameters parameters = BuildChunkCullingParameters(camera, {1280u, 720u}, 64.0f);

	EXPECT_TRUE(context, IsSceneChunkVisible(
							 MakePackedSceneChunkDescriptor({34, 0, -8}, {42, 8, 0}),
							 parameters));
	EXPECT_TRUE(context, !IsSceneChunkVisible(
							 MakePackedSceneChunkDescriptor({42, 0, -8}, {50, 8, 0}),
							 parameters));
}

void TestMakeUploadedSceneChunkDescriptorPreservesGeneratedFaceCounts(TestContext &context)
{
	PackedSceneChunkDescriptor sourceDescriptor = MakePackedSceneChunkDescriptor({0, 0, 0}, {8, 8, 8}, 11u);
	sourceDescriptor.drawRanges = {32u, 0u, 96u, 0u};
	PackedSceneChunkDescriptor existingDescriptor = sourceDescriptor;
	existingDescriptor.drawRanges = {32u, 17u, 96u, 9u};

	const PackedSceneChunkDescriptor uploadedDescriptor =
		MakeUploadedSceneChunkDescriptor(sourceDescriptor, &existingDescriptor);

	EXPECT_EQ(context, sourceDescriptor.chunkExtentAndNonAir[3], uploadedDescriptor.chunkExtentAndNonAir[3]);
	EXPECT_EQ(context, sourceDescriptor.drawRanges[0], uploadedDescriptor.drawRanges[0]);
	EXPECT_EQ(context, static_cast<uint32_t>(17), uploadedDescriptor.drawRanges[1]);
	EXPECT_EQ(context, sourceDescriptor.drawRanges[2], uploadedDescriptor.drawRanges[2]);
	EXPECT_EQ(context, static_cast<uint32_t>(9), uploadedDescriptor.drawRanges[3]);
}

void TestUpdateAppConsumesDebugInputActions(TestContext &context)
{
	PlatformState platform{};
	SimulationState simulation{};
	CameraState camera = MakeTestCamera({2.0f, 3.0f, 4.0f});
	camera.moveSpeed = 17.0f;
	InputState input{};
	InitializeInputState(input);
	InteractionState interaction{};
	interaction.placementMaterial = VoxelMaterial::FloorWhite;
	WorldState world{};
	RenderState render{};
	DebugState debug{};

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_F1);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_F2);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_F3);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_P);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_F4);

	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, nullptr, &render, &debug));
	EXPECT_TRUE(context, !debug.hudVisible);
	EXPECT_EQ(context, VoxelMaterial::FloorGray, interaction.placementMaterial);
	EXPECT_TRUE(context, simulation.paused);
	EXPECT_EQ(context, CameraState::ControlMode::Spectator, camera.controlMode);
	EXPECT_EQ(context, CameraState::ControlMode::Spectator, debug.stats.controlMode);
	EXPECT_TRUE(context, debug.stats.simulationPaused);
	EXPECT_NEAR(context, 0.0f, simulation.simulationAccumulatorSeconds);
	EXPECT_NEAR(context, 0.0f, camera.position[0]);
	EXPECT_NEAR(context, 8.0f, camera.position[1]);
	EXPECT_NEAR(context, 24.0f, camera.position[2]);
	EXPECT_NEAR(context, 10.0f, camera.moveSpeed);
	EXPECT_NEAR(context, 0.0f, camera.yawRadians);
	EXPECT_NEAR(context, -0.2f, camera.pitchRadians);
}

void TestPlacementMaterialCycleCoversAllDebugMaterials(TestContext &context)
{
	EXPECT_EQ(context, VoxelMaterial::FloorGray, GetNextPlacementMaterial(VoxelMaterial::FloorWhite));
	EXPECT_EQ(context, VoxelMaterial::Glass, GetNextPlacementMaterial(VoxelMaterial::FloorGray));
	EXPECT_EQ(context, VoxelMaterial::Fluid, GetNextPlacementMaterial(VoxelMaterial::Glass));
	EXPECT_EQ(context, VoxelMaterial::FloorWhite, GetNextPlacementMaterial(VoxelMaterial::Fluid));
	EXPECT_EQ(context, VoxelMaterial::FloorWhite, GetNextPlacementMaterial(VoxelMaterial::Air));
}

void TestResetCameraPreservesControlMode(TestContext &context)
{
	PlatformState platform{};
	SimulationState simulation{};
	CameraState camera = MakeTestCamera({5.0f, 6.0f, 7.0f});
	camera.controlMode = CameraState::ControlMode::Spectator;
	camera.moveSpeed = 22.0f;
	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_F3);
	InteractionState interaction{};
	WorldState world{};
	RenderState render{};
	DebugState debug{};

	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, nullptr, &render, &debug));
	EXPECT_EQ(context, CameraState::ControlMode::Spectator, camera.controlMode);
	EXPECT_EQ(context, CameraState::ControlMode::Spectator, debug.stats.controlMode);
	EXPECT_NEAR(context, 0.0f, camera.position[0]);
	EXPECT_NEAR(context, 8.0f, camera.position[1]);
	EXPECT_NEAR(context, 24.0f, camera.position[2]);
	EXPECT_NEAR(context, 10.0f, camera.moveSpeed);
}

void TestCreativeModeBlocksPausedMovement(TestContext &context)
{
	VoxelWorld voxelWorld = MakeWalkTestWorld();
	WorldState world{};
	world.voxelWorld = std::make_unique<VoxelWorld>(std::move(voxelWorld));
	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);

	PlatformState platform{};
	SimulationState simulation{};
	simulation.paused = true;
	const Uint64 frequency = SDL_GetPerformanceFrequency();
	simulation.lastFrameCounter = SDL_GetPerformanceCounter() - frequency / 10;
	CameraState camera = MakeTestCamera({2.5f, 3.0f, 4.5f});
	camera.controlMode = CameraState::ControlMode::Creative;
	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	InteractionState interaction{};
	RenderState render{};
	DebugState debug{};

	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), world.voxelWorld.get()));
	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, physics.get(), &render, &debug));
	EXPECT_NEAR(context, 4.5f, camera.position[2]);
	EXPECT_NEAR(context, 3.0f, camera.position[1]);
	EXPECT_TRUE(context, debug.stats.simulationPaused);
	EXPECT_EQ(context, CameraState::ControlMode::Creative, debug.stats.controlMode);
}

void TestSpectatorModeAllowsPausedMovementButBlocksEdits(TestContext &context)
{
	VoxelWorld voxelWorld = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	SetVoxelMaterial(voxelWorld, {1, 1, 1}, VoxelMaterial::Glass);
	ResetDirtyFlags(voxelWorld);

	PlatformState platform{};
	SimulationState simulation{};
	simulation.paused = true;
	const Uint64 frequency = SDL_GetPerformanceFrequency();
	simulation.lastFrameCounter = SDL_GetPerformanceCounter() - frequency / 10;
	CameraState camera = MakeTestCamera({1.5f, 1.5f, 4.5f});
	camera.controlMode = CameraState::ControlMode::Spectator;
	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	input.removePressed = true;
	InteractionState interaction{};
	WorldState world{};
	world.voxelWorld = std::make_unique<VoxelWorld>(std::move(voxelWorld));
	RenderState render{};
	DebugState debug{};

	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, nullptr, &render, &debug));
	EXPECT_TRUE(context, camera.position[2] < 4.0f);
	EXPECT_TRUE(context, camera.position[2] > 3.0f);
	EXPECT_EQ(context, VoxelMaterial::Glass, GetVoxelMaterial(*world.voxelWorld, {1, 1, 1}));
	EXPECT_TRUE(context, interaction.selection.hasHit);
	EXPECT_EQ(context, VoxelMaterial::Glass, interaction.selection.targetMaterial);
	EXPECT_TRUE(context, !input.removePressed);
	EXPECT_EQ(context, CameraState::ControlMode::Spectator, debug.stats.controlMode);
}

void TestPhysicsRaycastHitsStaticVoxelCollision(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Glass);

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	const PhysicsRaycastHit hit = RaycastPhysicsWorld(
		physics.get(),
		{1.5f, 4.5f, 1.5f},
		{0.0f, -1.0f, 0.0f},
		8.0f);

	EXPECT_TRUE(context, hit.hasHit);
	EXPECT_INT3_EQ(context, (Int3{1, 1, 1}), hit.voxel);
	EXPECT_NEAR(context, 2.5f, hit.distance);
	EXPECT_NEAR(context, 1.0f, hit.normal[1]);
}

void TestPhysicsWorldSyncTracksVoxelEdits(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	PhysicsRaycastHit hit = RaycastPhysicsWorld(
		physics.get(),
		{1.5f, 4.5f, 1.5f},
		{0.0f, -1.0f, 0.0f},
		8.0f);
	EXPECT_TRUE(context, !hit.hasHit);

	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Glass);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));
	EXPECT_EQ(context, world.editVersion, GetPhysicsWorldSyncVersion(physics.get()));

	hit = RaycastPhysicsWorld(
		physics.get(),
		{1.5f, 4.5f, 1.5f},
		{0.0f, -1.0f, 0.0f},
		8.0f);
	EXPECT_TRUE(context, hit.hasHit);
	EXPECT_INT3_EQ(context, (Int3{1, 1, 1}), hit.voxel);

	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Air);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	hit = RaycastPhysicsWorld(
		physics.get(),
		{1.5f, 4.5f, 1.5f},
		{0.0f, -1.0f, 0.0f},
		8.0f);
	EXPECT_TRUE(context, !hit.hasHit);
}

void TestUpdateAppCyclesCreativeSpectatorAndWalkModes(TestContext &context)
{
	PlatformState platform{};
	SimulationState simulation{};
	CameraState camera = MakeTestCamera({2.5f, 6.0f, 4.5f});
	InputState input{};
	InitializeInputState(input);
	InteractionState interaction{};
	WorldState world{};
	world.voxelWorld = std::make_unique<VoxelWorld>(MakeWalkTestWorld());
	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	RenderState render{};
	DebugState debug{};

	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), world.voxelWorld.get()));

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_F4);
	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, physics.get(), &render, &debug));
	EXPECT_EQ(context, CameraState::ControlMode::Spectator, camera.controlMode);

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_F4);
	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, physics.get(), &render, &debug));
	EXPECT_EQ(context, CameraState::ControlMode::Walk, camera.controlMode);
	EXPECT_EQ(context, CameraState::ControlMode::Walk, debug.stats.controlMode);
	EXPECT_NEAR(context, 6.0f, camera.position[1]);

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_F4);
	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, physics.get(), &render, &debug));
	EXPECT_EQ(context, CameraState::ControlMode::Creative, camera.controlMode);
	EXPECT_EQ(context, CameraState::ControlMode::Creative, debug.stats.controlMode);
	EXPECT_NEAR(context, 6.0f, camera.position[1]);
}

void TestUpdateAppDoubleSpaceTogglesCreativeAndWalk(TestContext &context)
{
	PlatformState platform{};
	SimulationState simulation{};
	CameraState camera = MakeTestCamera({2.5f, 6.0f, 4.5f});
	InputState input{};
	InitializeInputState(input);
	InteractionState interaction{};
	WorldState world{};
	world.voxelWorld = std::make_unique<VoxelWorld>(MakeWalkTestWorld());
	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	RenderState render{};
	DebugState debug{};

	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), world.voxelWorld.get()));

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE, false, SDL_MS_TO_NS(100));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE, false, SDL_MS_TO_NS(160));
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE, false, SDL_MS_TO_NS(260));
	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, physics.get(), &render, &debug));
	EXPECT_EQ(context, CameraState::ControlMode::Walk, camera.controlMode);
	EXPECT_EQ(context, CameraState::ControlMode::Walk, debug.stats.controlMode);
	EXPECT_NEAR(context, 6.0f, camera.position[1]);

	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE, false, SDL_MS_TO_NS(320));
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE, false, SDL_MS_TO_NS(500));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE, false, SDL_MS_TO_NS(560));
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE, false, SDL_MS_TO_NS(680));
	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, physics.get(), &render, &debug));
	EXPECT_EQ(context, CameraState::ControlMode::Creative, camera.controlMode);
	EXPECT_EQ(context, CameraState::ControlMode::Creative, debug.stats.controlMode);
	EXPECT_NEAR(context, 6.0f, camera.position[1]);
}

void TestWalkCharacterCollidesWithVoxelWall(TestContext &context)
{
	VoxelWorld world = MakeWalkTestWorld();
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Glass);
	SetVoxelMaterial(world, {2, 2, 2}, VoxelMaterial::Glass);

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({2.5f, 2.65f, 4.5f});
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 120; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	EXPECT_TRUE(context, camera.position[2] > 3.2f);
	EXPECT_TRUE(context, camera.position[2] < 4.5f);
	EXPECT_TRUE(context, camera.position[1] > 2.4f);
	EXPECT_TRUE(context, camera.position[1] < 2.9f);
}

void TestCreativeCharacterCollidesWithVoxelWall(TestContext &context)
{
	VoxelWorld world = MakeWalkTestWorld();
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Glass);
	SetVoxelMaterial(world, {2, 2, 2}, VoxelMaterial::Glass);

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({2.5f, 3.0f, 4.5f});
	camera.controlMode = CameraState::ControlMode::Creative;
	EXPECT_TRUE(context, SnapCreativeCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 120; ++step) {
		EXPECT_TRUE(context, TickCreativeCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	EXPECT_TRUE(context, camera.position[2] > 3.2f);
	EXPECT_TRUE(context, camera.position[2] < 4.5f);
	EXPECT_NEAR(context, 3.0f, camera.position[1]);
}

void TestGetNextVoxelScenePresetCyclesAllBuiltinPresets(TestContext &context)
{
	EXPECT_EQ(context, VoxelScenePreset::FlatBenchmark, GetNextVoxelScenePreset(VoxelScenePreset::VoxelLab));
	EXPECT_EQ(context, VoxelScenePreset::TransparencyStress, GetNextVoxelScenePreset(VoxelScenePreset::FlatBenchmark));
	EXPECT_EQ(context, VoxelScenePreset::ChunkGrid, GetNextVoxelScenePreset(VoxelScenePreset::TransparencyStress));
	EXPECT_EQ(context, VoxelScenePreset::MeshingStress, GetNextVoxelScenePreset(VoxelScenePreset::ChunkGrid));
	EXPECT_EQ(context, VoxelScenePreset::VoxelLab, GetNextVoxelScenePreset(VoxelScenePreset::MeshingStress));
}

void TestUpdateAppRequestsScenePresetReload(TestContext &context)
{
	PlatformState platform{};
	SimulationState simulation{};
	CameraState camera = MakeTestCamera({2.0f, 3.0f, 4.0f});
	InputState input{};
	InitializeInputState(input);
	InteractionState interaction{};
	WorldState world{};
	world.voxelWorld = std::make_unique<VoxelWorld>(MakeWalkTestWorld());
	world.voxelWorld->scenePreset = VoxelScenePreset::ChunkGrid;
	RenderState render{};
	DebugState debug{};

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_F5);
	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, nullptr, &render, &debug));
	EXPECT_TRUE(context, world.scenePresetReloadRequested);
	EXPECT_EQ(context, VoxelScenePreset::MeshingStress, world.requestedScenePreset);
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
		&interaction,
		true);

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
		&interaction,
		true);

	EXPECT_EQ(context, VoxelMaterial::FloorWhite, GetVoxelMaterial(world, {1, 1, 2}));
	EXPECT_TRUE(context, !input.placePressed);
	EXPECT_TRUE(context, interaction.selection.hasHit);
	EXPECT_EQ(context, VoxelMaterial::FloorWhite, interaction.selection.targetMaterial);
	EXPECT_INT3_EQ(context, (Int3{1, 1, 2}), interaction.selection.targetVoxel);
	EXPECT_EQ(context, static_cast<uint32_t>(1), CountDirtyVoxelChunks(world));
}

void TestUpdateVoxelInteractionSkipsEditingWhenDisabled(TestContext &context)
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
		&interaction,
		false);

	EXPECT_EQ(context, VoxelMaterial::Glass, GetVoxelMaterial(world, {1, 1, 1}));
	EXPECT_TRUE(context, interaction.selection.hasHit);
	EXPECT_TRUE(context, !input.removePressed);
	EXPECT_EQ(context, static_cast<uint32_t>(0), CountDirtyVoxelChunks(world));
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
	std::vector<DebugHudVertex> vertices(65536);
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
	stats.scenePreset = VoxelScenePreset::MeshingStress;

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

	constexpr std::array helperPanelColor{0.07f, 0.09f, 0.12f, 0.76f};
	constexpr std::array statsPanelColor{0.05f, 0.07f, 0.10f, 0.80f};
	constexpr std::array helperTextColor{0.77f, 0.84f, 0.90f, 0.94f};
	float statsPanelMaxX = -1.0f;
	float helperPanelMaxX = -1.0f;
	float helperTextMaxX = -1.0f;
	for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
		const auto &[positionNdc, color] = vertices[vertexIndex];
		if (ColorsMatch(statsPanelColor, color)) {
			statsPanelMaxX = std::max(statsPanelMaxX, positionNdc[0]);
		}
		if (ColorsMatch(helperPanelColor, color)) {
			helperPanelMaxX = std::max(helperPanelMaxX, positionNdc[0]);
		}
		if (ColorsMatch(helperTextColor, color)) {
			helperTextMaxX = std::max(helperTextMaxX, positionNdc[0]);
		}
	}

	EXPECT_TRUE(context, statsPanelMaxX > -1.0f);
	EXPECT_TRUE(context, helperPanelMaxX > -1.0f);
	EXPECT_TRUE(context, helperTextMaxX > -1.0f);
	EXPECT_TRUE(context, std::abs(statsPanelMaxX - helperPanelMaxX) <= 0.0001f);
	EXPECT_TRUE(context, helperTextMaxX <= helperPanelMaxX);
}

void TestInitializeAppEcsCreatesPrimaryCameraPlayerAndSingletons(TestContext &context)
{
	AppState state{};

	EXPECT_TRUE(context, InitializeAppEcs(&state));
	EXPECT_TRUE(context, state.ecs != nullptr);
	EXPECT_TRUE(context, GetPrimaryCameraEntityId(state.ecs.get()) != 0u);
	EXPECT_TRUE(context, GetPrimaryPlayerEntityId(state.ecs.get()) != 0u);
	EXPECT_EQ(context, GetPrimaryCameraEntityId(state.ecs.get()), GetPlayerControlledCameraEntityId(state.ecs.get()));

	CameraState *camera = GetPrimaryCameraState(state.ecs.get());
	const DebugState *debug = GetDebugState(state.ecs.get());
	const WorldState *world = GetWorldState(state.ecs.get());

	EXPECT_TRUE(context, camera != nullptr);
	EXPECT_TRUE(context, debug != nullptr);
	EXPECT_TRUE(context, world == &state.world);

	camera->moveSpeed = 17.0f;
	EXPECT_NEAR(context, 17.0f, GetPrimaryCameraState(state.ecs.get())->moveSpeed);
	EXPECT_TRUE(context, debug->hudVisible);
}

void TestSyncEcsWorldStateMirrorsVoxelChunksAndWorldSummary(TestContext &context)
{
	AppState state{};
	EXPECT_TRUE(context, InitializeAppEcs(&state));

	state.world.voxelWorld = std::make_unique<VoxelWorld>(MakeTestWorld({0, 0, 0}, {16, 8, 16}, 8));
	SetVoxelMaterial(*state.world.voxelWorld, {1, 1, 1}, VoxelMaterial::Glass);
	SetVoxelMaterial(*state.world.voxelWorld, {9, 1, 1}, VoxelMaterial::Fluid);

	DebugState *debug = GetDebugState(state.ecs.get());
	EXPECT_TRUE(context, debug != nullptr);
	debug->stats.dirtyChunkCount = 99;
	debug->stats.activeChunkCount = 99;
	debug->stats.nonAirVoxelCount = 99;

	EXPECT_TRUE(context, SyncEcsWorldState(state.ecs.get()));

	VoxelWorldStats summary{};
	size_t chunkEntityCount = 0;
	EXPECT_TRUE(context, GetEcsWorldChunkSummary(state.ecs.get(), &summary, &chunkEntityCount));

	EXPECT_EQ(context, state.world.voxelWorld->chunks.size(), chunkEntityCount);
	EXPECT_EQ(context, state.world.voxelWorld->stats.dirtyChunkCount, summary.dirtyChunkCount);
	EXPECT_EQ(context, state.world.voxelWorld->stats.activeChunkCount, summary.activeChunkCount);
	EXPECT_EQ(context, state.world.voxelWorld->stats.nonAirVoxelCount, summary.nonAirVoxelCount);
	EXPECT_EQ(context, summary.dirtyChunkCount, debug->stats.dirtyChunkCount);
	EXPECT_EQ(context, summary.activeChunkCount, debug->stats.activeChunkCount);
	EXPECT_EQ(context, summary.nonAirVoxelCount, debug->stats.nonAirVoxelCount);
}
} // namespace

int main()
{
	TestContext context{};

	TestWorldBoundsAndChunkIndexing(context);
	TestVoxelScenePresetParsingAcceptsCanonicalAndFlexibleNames(context);
	TestCreateVoxelSceneWorldBuildsExpectedBaselineScenes(context);
	TestCreateVoxelSceneWorldReadsEnvironmentPreset(context);
	TestMarkVoxelRegionDirtyQueuesExpectedChunks(context);
	TestSetVoxelMaterialTracksCountsAndQueuesRebuild(context);
	TestSetVoxelMaterialMarksNeighborChunksDirtyAtBoundaries(context);
	TestSetVoxelMaterialMarksOnlyFaceSharingNeighborChunksDirty(context);
	TestSetVoxelMaterialDoesNotQueueMissingWorldNeighbors(context);
	TestMarkAllVoxelChunksDirtyResetsQueue(context);
	TestSwapchainRefreshWindowEventClassification(context);
	TestVkResultToStringCoversCommonRuntimeResults(context);
	TestInitFailureStageParsing(context);
	TestVoxelMaterialVisuals(context);
	TestInputActionBindingsTrackPressedAndReleasedKeys(context);
	TestConsumeCameraLookInputAllowsNearVerticalPitch(context);
	TestTickCameraUsesActionStateAndSpeedModifiers(context);
	TestHandleCameraEventIgnoresLookInputWithoutRelativeMouseMode(context);
	TestSceneChunkVisibilityUsesFrustumAndDistanceCulling(context);
	TestSceneChunkVisibilityKeepsChunksVisibleAtFrustumEdges(context);
	TestMakeUploadedSceneChunkDescriptorPreservesGeneratedFaceCounts(context);
	TestUpdateAppConsumesDebugInputActions(context);
	TestPlacementMaterialCycleCoversAllDebugMaterials(context);
	TestResetCameraPreservesControlMode(context);
	TestCreativeModeBlocksPausedMovement(context);
	TestSpectatorModeAllowsPausedMovementButBlocksEdits(context);
	TestPhysicsRaycastHitsStaticVoxelCollision(context);
	TestPhysicsWorldSyncTracksVoxelEdits(context);
	TestUpdateAppCyclesCreativeSpectatorAndWalkModes(context);
	TestUpdateAppDoubleSpaceTogglesCreativeAndWalk(context);
	TestWalkCharacterCollidesWithVoxelWall(context);
	TestCreativeCharacterCollidesWithVoxelWall(context);
	TestGetNextVoxelScenePresetCyclesAllBuiltinPresets(context);
	TestUpdateAppRequestsScenePresetReload(context);
	TestVoxelRaycastHitsSolidVoxelAndReturnsPlacementCell(context);
	TestVoxelRaycastStopsAtWorldBoundaryWithoutPlacementCell(context);
	TestVoxelRaycastMissesWhenNoSolidVoxelIsReached(context);
	TestUpdateVoxelInteractionRemovesTargetedBlock(context);
	TestUpdateVoxelInteractionPlacesConfiguredBlock(context);
	TestUpdateVoxelInteractionSkipsEditingWhenDisabled(context);
	TestBuildDebugHudVerticesReturnsZeroWhenHidden(context);
	TestBuildDebugHudVerticesProducesGeometryWhenVisible(context);
	TestInitializeAppEcsCreatesPrimaryCameraPlayerAndSingletons(context);
	TestSyncEcsWorldStateMirrorsVoxelChunksAndWorldSummary(context);

	if (context.failures != 0) {
		return EXIT_FAILURE;
	}

	std::puts("ProjectVTests passed");
	return EXIT_SUCCESS;
}

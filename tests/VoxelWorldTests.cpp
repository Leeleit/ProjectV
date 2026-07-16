#include "app/AppUpdate.hpp"
#include "app/Camera.hpp"
#include "app/InputActions.hpp"
#include "app/InputReplay.hpp"
#include "app/LookDevCaptureAutomation.hpp"
#include "core/RuntimeProbe.hpp"
#include "core/Types.hpp"
#include "debug/DebugOverlays.hpp"
#include "ecs/EcsWorld.hpp"
#include "physics/PhysicsWorld.hpp"
#include "platform/PlatformEvents.hpp"
#include "render/SceneResources.hpp"
#include "render/ScreenshotCapture.hpp"
// CSM removed per TODO.md §5.2.D (session 20x). Tests that referenced
// ShadowProjection helpers have been retired; BuildSunShadowProjection
// callers below are stubs (kept for test enumeration compatibility).
#include "render/vulkan/VulkanResult.hpp"
#include "voxel/VoxelInteraction.hpp"
#include "voxel/VoxelMaterials.hpp"
#include "voxel/VoxelRaycast.hpp"
#include "voxel/VoxelWorld.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>
#include <vector>

// ReSharper disable CppDFAUnreachableFunctionCall

#include "projectv_test_utils.hpp"

namespace {
#ifndef PROJECTV_TESTS_SOURCE_DIR
#define PROJECTV_TESTS_SOURCE_DIR "."
#endif

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

int SimulationFrameCount(const int frames)
{
	return std::max(frames, 0);
}

bool ColorsMatch(
	const projectv::math::Vec4 &expected,
	const projectv::math::Vec4 &actual)
{
	constexpr float kColorEpsilon = 0.0001f;
	if (std::abs(expected[0] - actual[0]) > kColorEpsilon) {
		return false;
	}
	if (std::abs(expected[1] - actual[1]) > kColorEpsilon) {
		return false;
	}
	if (std::abs(expected[2] - actual[2]) > kColorEpsilon) {
		return false;
	}
	if (std::abs(expected[3] - actual[3]) > kColorEpsilon) {
		return false;
	}
	return true;
}

uint32_t ReadLe32(const std::vector<uint8_t> &bytes, const size_t offset)
{
	const uint32_t byte0 = bytes[offset];
	const uint32_t byte1 = bytes[offset + 1];
	const uint32_t byte2 = bytes[offset + 2];
	const uint32_t byte3 = bytes[offset + 3];
	return byte0 | byte1 << 8u | byte2 << 16u | byte3 << 24u;
}

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
	world.sparseStorage.Reset(world.width, world.height, world.depth);
	for (int z = 0; z < world.depth; ++z) {
		for (int y = 0; y < world.height; ++y) {
			for (int x = 0; x < world.width; ++x) {
				SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::Air, nullptr);
			}
		}
	}
	world.chunks.resize(
		static_cast<size_t>(world.chunkCountX) *
		static_cast<size_t>(world.chunkCountY) *
		static_cast<size_t>(world.chunkCountZ));

	for (int chunkZ = 0; chunkZ < world.chunkCountZ; ++chunkZ) {
		for (int chunkY = 0; chunkY < world.chunkCountY; ++chunkY) {
			for (int chunkX = 0; chunkX < world.chunkCountX; ++chunkX) {
				const size_t chunkIndex = GetVoxelChunkIndex(world, {chunkX, chunkY, chunkZ});
				auto &[min, maxExclusive, rebuildQueued, isStatic, nonAirVoxelCount, ticksSinceLastEdit, lodLevel, reserved0, reserved1, reserved2] = world.chunks[chunkIndex];
				min = {
					world.min.x + chunkX * world.chunkSize,
					world.min.y + chunkY * world.chunkSize,
					world.min.z + chunkZ * world.chunkSize,
				};
				maxExclusive = {
					min.x + std::min(world.chunkSize, world.maxExclusive.x - min.x),
					min.y + std::min(world.chunkSize, world.maxExclusive.y - min.y),
					min.z + std::min(world.chunkSize, world.maxExclusive.z - min.z),
				};
				rebuildQueued = false;
				nonAirVoxelCount = 0;
			}
		}
	}

	return world;
}

std::filesystem::path GetTestVoxelWorldSnapshotPath()
{
	std::error_code error;
	const std::filesystem::path tempDirectory = std::filesystem::temp_directory_path(error);
	if (error) {
		return std::filesystem::path("ProjectV-VoxelWorldSnapshotTest.bin");
	}

	return tempDirectory / "ProjectV-VoxelWorldSnapshotTest.bin";
}

std::filesystem::path GetTestInputReplayPath()
{
	std::error_code error;
	const std::filesystem::path tempDirectory = std::filesystem::temp_directory_path(error);
	if (error) {
		return std::filesystem::path("ProjectV-InputReplayTest.replay");
	}

	return tempDirectory / "ProjectV-InputReplayTest.replay";
}

std::filesystem::path GetTestInputReplaySnapshotPath()
{
	std::error_code error;
	const std::filesystem::path tempDirectory = std::filesystem::temp_directory_path(error);
	if (error) {
		return std::filesystem::path("ProjectV-InputReplayTest.snapshot.bin");
	}

	return tempDirectory / "ProjectV-InputReplayTest.snapshot.bin";
}

std::filesystem::path GetTestScreenshotPath()
{
	std::error_code error;
	const std::filesystem::path tempDirectory = std::filesystem::temp_directory_path(error);
	if (error) {
		return std::filesystem::path("ProjectV-ScreenshotTest.bmp");
	}

	return tempDirectory / "ProjectV-ScreenshotTest.bmp";
}

void ResetDirtyFlags(VoxelWorld &world)
{
	world.pendingChunkRebuildIndices.clear();
	world.stats.dirtyChunkCount = 0;
	for (VoxelChunk &chunk : world.chunks) {
		chunk.rebuildQueued = false;
	}
}

PackedSceneChunkDescriptor MakePackedSceneChunkDescriptor(
	const Int3 min,
	const Int3 maxExclusive)
{
	const uint32_t extentX = static_cast<uint32_t>(maxExclusive.x - min.x);
	const uint32_t extentY = static_cast<uint32_t>(maxExclusive.y - min.y);
	const uint32_t extentZ = static_cast<uint32_t>(maxExclusive.z - min.z);
	const uint32_t voxelCount = extentX * extentY * extentZ;
	return {
		.chunkOrigin = {min.x, min.y, min.z, 0},
		.chunkExtentAndNonAir = {extentX, extentY, extentZ, 1u},
		.voxelDataInfo = {0u, voxelCount, (voxelCount + 3u) / 4u, 0u},
		.drawRanges = {0u, 0u, 0u, 0u},
	};
}

Int3 AddInt3(const Int3 a, const Int3 b)
{
	return {a.x + b.x, a.y + b.y, a.z + b.z};
}

bool IsAmbientOccluderMaterial(const VoxelMaterial material)
{
	return material != VoxelMaterial::Air && material != VoxelMaterial::Glass;
}

void GetFaceCornerAmbientOffsets(
	const uint32_t faceIndex,
	const uint32_t cornerIndex,
	Int3 &outSideOffsetA,
	Int3 &outSideOffsetB,
	Int3 &outDiagonalOffset)
{
	switch (faceIndex) {
	case 0u:
		outSideOffsetA = cornerIndex == 1u || cornerIndex == 2u ? Int3{0, 1, 0} : Int3{0, -1, 0};
		outSideOffsetB = cornerIndex >= 2u ? Int3{0, 0, 1} : Int3{0, 0, -1};
		break;
	case 1u:
		outSideOffsetA = cornerIndex == 1u || cornerIndex == 2u ? Int3{0, 1, 0} : Int3{0, -1, 0};
		outSideOffsetB = cornerIndex == 0u || cornerIndex == 1u ? Int3{0, 0, 1} : Int3{0, 0, -1};
		break;
	case 2u:
		outSideOffsetA = cornerIndex >= 2u ? Int3{1, 0, 0} : Int3{-1, 0, 0};
		outSideOffsetB = cornerIndex == 1u || cornerIndex == 2u ? Int3{0, 0, 1} : Int3{0, 0, -1};
		break;
	case 3u:
		outSideOffsetA = cornerIndex >= 2u ? Int3{1, 0, 0} : Int3{-1, 0, 0};
		outSideOffsetB = cornerIndex == 0u || cornerIndex == 3u ? Int3{0, 0, 1} : Int3{0, 0, -1};
		break;
	case 4u:
		outSideOffsetA = cornerIndex <= 1u ? Int3{1, 0, 0} : Int3{-1, 0, 0};
		outSideOffsetB = cornerIndex == 1u || cornerIndex == 2u ? Int3{0, 1, 0} : Int3{0, -1, 0};
		break;
	default:
		outSideOffsetA = cornerIndex >= 2u ? Int3{1, 0, 0} : Int3{-1, 0, 0};
		outSideOffsetB = cornerIndex == 1u || cornerIndex == 2u ? Int3{0, 1, 0} : Int3{0, -1, 0};
		break;
	}

	outDiagonalOffset = AddInt3(outSideOffsetA, outSideOffsetB);
}

uint32_t ReadAmbientOccluder(const VoxelWorld &world, const Int3 position)
{
	return IsAmbientOccluderMaterial(GetVoxelMaterial(world, position)) ? 1u : 0u;
}

uint32_t ComputeVoxelFaceAmbientVisibilityByte(
	const VoxelWorld &world,
	const Int3 voxelPosition,
	const uint32_t faceIndex)
{
	uint32_t visibilityLevels = 0u;
	for (uint32_t cornerIndex = 0u; cornerIndex < 4u; ++cornerIndex) {
		Int3 sideOffsetA{};
		Int3 sideOffsetB{};
		Int3 diagonalOffset{};
		GetFaceCornerAmbientOffsets(faceIndex, cornerIndex, sideOffsetA, sideOffsetB, diagonalOffset);

		const uint32_t sideA = ReadAmbientOccluder(world, AddInt3(voxelPosition, sideOffsetA));
		const uint32_t sideB = ReadAmbientOccluder(world, AddInt3(voxelPosition, sideOffsetB));
		if (sideA != 0u && sideB != 0u) {
			continue;
		}

		const uint32_t diagonal = ReadAmbientOccluder(world, AddInt3(voxelPosition, diagonalOffset));
		visibilityLevels += 3u - std::min(sideA + sideB + diagonal, 3u);
	}

	return (visibilityLevels * 255u + 6u) / 12u;
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
	{
		const auto parsed = ParseVoxelScenePreset("VoxelLab");
		EXPECT_TRUE(context, parsed.has_value());
		if (parsed.has_value()) {
			preset = *parsed;
		}
		EXPECT_EQ(context, VoxelScenePreset::VoxelLab, preset);
	}
	{
		const auto parsed = ParseVoxelScenePreset("flat_benchmark");
		EXPECT_TRUE(context, parsed.has_value());
		if (parsed.has_value()) {
			preset = *parsed;
		}
		EXPECT_EQ(context, VoxelScenePreset::FlatBenchmark, preset);
	}
	{
		const auto parsed = ParseVoxelScenePreset("TRANSPARENCY-STRESS");
		EXPECT_TRUE(context, parsed.has_value());
		if (parsed.has_value()) {
			preset = *parsed;
		}
		EXPECT_EQ(context, VoxelScenePreset::TransparencyStress, preset);
	}
	{
		const auto parsed = ParseVoxelScenePreset("chunk grid");
		EXPECT_TRUE(context, parsed.has_value());
		if (parsed.has_value()) {
			preset = *parsed;
		}
		EXPECT_EQ(context, VoxelScenePreset::ChunkGrid, preset);
	}
	{
		const auto parsed = ParseVoxelScenePreset("meshingstress");
		EXPECT_TRUE(context, parsed.has_value());
		if (parsed.has_value()) {
			preset = *parsed;
		}
		EXPECT_EQ(context, VoxelScenePreset::MeshingStress, preset);
	}
	EXPECT_TRUE(context, !ParseVoxelScenePreset("not_a_scene").has_value());
	EXPECT_TRUE(context, VoxelScenePresetToString(VoxelScenePreset::MeshingStress) == "MeshingStress");
}

void TestCreateVoxelSceneWorldBuildsExpectedBaselineScenes(TestContext &context)
{
	AppState state{};

	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::VoxelLab));
	EXPECT_TRUE(context, state.world().voxelWorld != nullptr);
	EXPECT_EQ(context, VoxelScenePreset::VoxelLab, state.world().voxelWorld->scenePreset);
	EXPECT_TRUE(context, state.world().voxelWorld->config.worldTopY >= 14);
	EXPECT_TRUE(context, state.world().voxelWorld->stats.glassVoxelCount > 0);
	EXPECT_TRUE(context, state.world().voxelWorld->stats.fluidVoxelCount > 0);
	EXPECT_EQ(context, VoxelMaterial::FloorGray, GetVoxelMaterial(*state.world().voxelWorld, {8, 1, 4}));
	EXPECT_EQ(context, VoxelMaterial::FloorWhite, GetVoxelMaterial(*state.world().voxelWorld, {7, 6, 4}));
	EXPECT_EQ(context, VoxelMaterial::Air, GetVoxelMaterial(*state.world().voxelWorld, {5, 5, 6}));

	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::FlatBenchmark));
	EXPECT_TRUE(context, state.world().voxelWorld != nullptr);
	EXPECT_EQ(context, VoxelScenePreset::FlatBenchmark, state.world().voxelWorld->scenePreset);
	EXPECT_TRUE(context, state.world().voxelWorld->stats.floorWhiteVoxelCount > 0);
	EXPECT_EQ(context, 0u, state.world().voxelWorld->stats.glassVoxelCount);
	EXPECT_EQ(context, 0u, state.world().voxelWorld->stats.fluidVoxelCount);

	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::TransparencyStress));
	EXPECT_EQ(context, VoxelScenePreset::TransparencyStress, state.world().voxelWorld->scenePreset);
	EXPECT_TRUE(context, state.world().voxelWorld->stats.glassVoxelCount > 0);
	EXPECT_EQ(context, 0u, state.world().voxelWorld->stats.fluidVoxelCount);

	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::MeshingStress));
	EXPECT_EQ(context, VoxelScenePreset::MeshingStress, state.world().voxelWorld->scenePreset);
	EXPECT_TRUE(context, state.world().voxelWorld->stats.nonAirVoxelCount > state.world().voxelWorld->stats.floorWhiteVoxelCount);
}

void TestCreateVoxelSceneWorldReadsEnvironmentPreset(TestContext &context)
{
	AppState state{};
	SDL_setenv_unsafe("PROJECTV_SCENE_PRESET", "ChunkGrid", 1);

	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state));
	EXPECT_TRUE(context, state.world().voxelWorld != nullptr);
	EXPECT_EQ(context, VoxelScenePreset::ChunkGrid, state.world().voxelWorld->scenePreset);

	SDL_unsetenv_unsafe("PROJECTV_SCENE_PRESET");
}

void TestVoxelWorldSnapshotPathUsesEnvironmentOverride(TestContext &context)
{
	constexpr const char *snapshotPath = "C:/ProjectVTests/ProjectV.snapshot.bin";
	SDL_setenv_unsafe("PROJECTV_SNAPSHOT_PATH", snapshotPath, 1);
	EXPECT_TRUE(context, GetVoxelWorldSnapshotPath() == snapshotPath);
	SDL_unsetenv_unsafe("PROJECTV_SNAPSHOT_PATH");
}

void TestScreenshotCapturePathUsesEnvironmentOverride(TestContext &context)
{
	constexpr const char *screenshotDirectory = "C:/ProjectVTests/Captures";
	SDL_setenv_unsafe("PROJECTV_SCREENSHOT_DIR", screenshotDirectory, 1);

	const std::filesystem::path screenshotPath = BuildScreenshotCapturePath(VoxelScenePreset::ChunkGrid, 7u);
	EXPECT_TRUE(context, screenshotPath.parent_path() == std::filesystem::path(screenshotDirectory));
	EXPECT_TRUE(context, screenshotPath.extension() == ".bmp");
	EXPECT_TRUE(context, screenshotPath.filename().string().find("ChunkGrid") != std::string::npos);

	const std::filesystem::path metadataPath = BuildScreenshotCaptureMetadataPath(screenshotPath.string());
	EXPECT_TRUE(context, metadataPath.parent_path() == std::filesystem::path(screenshotDirectory));
	EXPECT_TRUE(context, metadataPath.extension() == ".txt");
	EXPECT_TRUE(context, metadataPath.stem() == screenshotPath.stem());

	SDL_unsetenv_unsafe("PROJECTV_SCREENSHOT_DIR");
}

void TestSaveScreenshotCaptureBmpWritesExpectedBmp(TestContext &context)
{
	const std::filesystem::path screenshotPath = GetTestScreenshotPath();
	std::error_code removeError;
	std::filesystem::remove(screenshotPath, removeError);

	constexpr std::array<uint8_t, 16> pixels{
		255u,
		0u,
		0u,
		255u,
		0u,
		255u,
		0u,
		255u,
		0u,
		0u,
		255u,
		255u,
		255u,
		255u,
		255u,
		255u,
	};

	EXPECT_TRUE(context, SaveScreenshotCaptureBmp(
							 pixels.data(),
							 2u,
							 2u,
							 VK_FORMAT_R8G8B8A8_UNORM,
							 screenshotPath.string()));
	EXPECT_TRUE(context, std::filesystem::exists(screenshotPath));

	std::ifstream stream(screenshotPath, std::ios::binary);
	const std::vector<uint8_t> bytes(std::istreambuf_iterator(stream), {});
	EXPECT_TRUE(context, bytes.size() == 70u);
	if (bytes.size() >= 70u) {
		EXPECT_TRUE(context, bytes[0] == static_cast<uint8_t>('B'));
		EXPECT_TRUE(context, bytes[1] == static_cast<uint8_t>('M'));
		EXPECT_EQ(context, 70u, ReadLe32(bytes, 2u));
		EXPECT_EQ(context, 2u, ReadLe32(bytes, 18u));
		EXPECT_EQ(context, 2u, ReadLe32(bytes, 22u));
		EXPECT_TRUE(context, bytes[54] == 255u);
		EXPECT_TRUE(context, bytes[55] == 0u);
		EXPECT_TRUE(context, bytes[56] == 0u);
	}

	std::filesystem::remove(screenshotPath, removeError);
}

void TestSaveScreenshotCaptureMetadataWritesLookDevState(TestContext &context)
{
	RenderState render{};
	render.currentSceneLighting.postProcess[0] = 1.25f;
	render.currentSceneLighting.postProcess[1] = 0.85f;
	render.currentSceneLighting.colorGrading = {1.10f, 1.05f, 0.95f, -0.02f};
	render.currentSceneLighting.exposureControl = {
		static_cast<float>(ExposureMeteringMode::SceneKey),
		0.75f,
		0.40f,
		2.50f};
	render.currentSceneLighting.sunDirectionAndWrap = {-0.58f, 0.62f, -0.31f, 0.10f};
	render.currentSceneLighting.sunColorAndIntensity = {0.95f, 0.90f, 0.82f, 1.30f};
	render.currentSceneLighting.sunContactShadowParams = {0.46f, 3.25f, 0.0f, 0.0f};
	render.currentSceneLighting.ambientOcclusionParams = {0.37f, 2.75f, 0.55f, 0.0f};
	render.currentSceneLighting.localPointLightPositionAndRadius = {1.0f, 2.0f, 3.0f, 9.0f};
	render.currentSceneLighting.localPointLightColorAndIntensity = {0.70f, 0.80f, 0.90f, 12.0f};
	render.currentSceneLighting.localPointLightParams = {1.0f, 1.50f, 0.85f, 0.08f};
	render.lightingDebugControls.exposureBiasStops = 0.25f;
	render.lightingDebugControls.toneMapOperator = ToneMapOperator::AcesApprox;
	render.lightingDebugControls.debugView = LightingDebugView::Shadow;

	const std::filesystem::path metadataPath = GetTestScreenshotPath().replace_extension(".txt");
	std::error_code removeError;
	std::filesystem::remove(metadataPath, removeError);

	EXPECT_TRUE(context, SaveScreenshotCaptureMetadata(
							 render,
							 VoxelScenePreset::MeshingStress,
							 "C:/ProjectVTests/Captures/sample.bmp",
							 metadataPath.string()));
	EXPECT_TRUE(context, std::filesystem::exists(metadataPath));

	std::ifstream stream(metadataPath);
	const std::string text(std::istreambuf_iterator(stream), {});
	EXPECT_TRUE(context, text.find("scene_preset=MeshingStress") != std::string::npos);
	EXPECT_TRUE(context, text.find("debug_view=SHDW") != std::string::npos);
	EXPECT_TRUE(context, text.find("environment_intensity=0.850000") != std::string::npos);
	EXPECT_TRUE(context, text.find("grading_white_point=1.100000") != std::string::npos);
	EXPECT_TRUE(context, text.find("grading_contrast=1.050000") != std::string::npos);
	EXPECT_TRUE(context, text.find("grading_saturation=0.950000") != std::string::npos);
	EXPECT_TRUE(context, text.find("grading_lift=-0.020000") != std::string::npos);
	EXPECT_TRUE(context, text.find("exposure_metering=SCENEKEY") != std::string::npos);
	EXPECT_TRUE(context, text.find("exposure_target_key=0.750000") != std::string::npos);
	EXPECT_TRUE(context, text.find("exposure_min=0.400000") != std::string::npos);
	EXPECT_TRUE(context, text.find("exposure_max=2.500000") != std::string::npos);
	EXPECT_TRUE(context, text.find("contact_shadow_strength=0.460000") != std::string::npos);
	EXPECT_TRUE(context, text.find("contact_shadow_distance=3.250000") != std::string::npos);
	EXPECT_TRUE(context, text.find("ambient_occlusion_strength=0.370000") != std::string::npos);
	EXPECT_TRUE(context, text.find("ambient_occlusion_radius=2.750000") != std::string::npos);
	EXPECT_TRUE(context, text.find("ambient_occlusion_min_visibility=0.550000") != std::string::npos);
	EXPECT_TRUE(context, text.find("local_point_light_position=1.000000 2.000000 3.000000") != std::string::npos);
	EXPECT_TRUE(context, text.find("local_point_light_radius=9.000000") != std::string::npos);
	EXPECT_TRUE(context, text.find("local_point_light_color=0.700000 0.800000 0.900000") != std::string::npos);
	EXPECT_TRUE(context, text.find("local_point_light_intensity=12.000000") != std::string::npos);
	EXPECT_TRUE(context, text.find("local_point_light_enabled=1.000000") != std::string::npos);
	EXPECT_TRUE(context, text.find("local_point_light_source_radius=1.500000") != std::string::npos);
	EXPECT_TRUE(context, text.find("local_point_light_shadow_strength=0.850000") != std::string::npos);
	EXPECT_TRUE(context, text.find("local_point_light_shadow_bias=0.080000") != std::string::npos);
	EXPECT_TRUE(context, text.find("transparent_shadow_policy=GLASS_IGNORED_FLUID_CASTS") != std::string::npos);
	// CSM removed per TODO.md §5.2.D (session 20x). shadow_tuning_target,
	// shadow_coverage_scale, shadow_cascade_* metadata fields are retired;
	// RTX shadows are the canonical sun shadow path.

	std::filesystem::remove(metadataPath, removeError);
}

void TestVoxelFaceAmbientVisibilityStaysOpenForExposedTopFace(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {4, 4, 4}, 4);
	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::FloorWhite, nullptr);

	EXPECT_EQ(context, 255u, ComputeVoxelFaceAmbientVisibilityByte(world, {1, 1, 1}, 2u));
}

void TestVoxelFaceAmbientVisibilityDarkensEnclosedTopFace(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {5, 5, 5}, 5);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::FloorWhite, nullptr);
	SetVoxelMaterial(world, {1, 1, 2}, VoxelMaterial::FloorGray, nullptr);
	SetVoxelMaterial(world, {3, 1, 2}, VoxelMaterial::FloorGray, nullptr);
	SetVoxelMaterial(world, {2, 1, 1}, VoxelMaterial::FloorGray, nullptr);
	SetVoxelMaterial(world, {2, 1, 3}, VoxelMaterial::FloorGray, nullptr);

	EXPECT_EQ(context, 0u, ComputeVoxelFaceAmbientVisibilityByte(world, {2, 1, 2}, 2u));
}

void TestVoxelFaceAmbientVisibilityTreatsGlassAsNonOccluder(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {5, 5, 5}, 5);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::FloorWhite, nullptr);
	SetVoxelMaterial(world, {1, 1, 2}, VoxelMaterial::Glass, nullptr);
	SetVoxelMaterial(world, {3, 1, 2}, VoxelMaterial::Glass, nullptr);
	SetVoxelMaterial(world, {2, 1, 1}, VoxelMaterial::Glass, nullptr);
	SetVoxelMaterial(world, {2, 1, 3}, VoxelMaterial::Glass, nullptr);

	EXPECT_EQ(context, 255u, ComputeVoxelFaceAmbientVisibilityByte(world, {2, 1, 2}, 2u));
}

void TestVoxelWorldSnapshotRoundTripsWorldState(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({-4, 1, -4}, {12, 9, 12}, 4);
	world.scenePreset = VoxelScenePreset::TransparencyStress;
	world.config.floorSize = 20;
	world.config.floorY = 1;
	world.config.worldTopY = 12;
	world.config.padding = 5;
	world.config.chunkSize = 4;
	SetVoxelMaterial(world, {-4, 1, -4}, VoxelMaterial::FloorWhite, nullptr);
	SetVoxelMaterial(world, {-1, 2, 3}, VoxelMaterial::Glass, nullptr);
	SetVoxelMaterial(world, {5, 4, 6}, VoxelMaterial::Fluid, nullptr);
	SetVoxelMaterial(world, {11, 8, 11}, VoxelMaterial::FloorGray, nullptr);
	world.editVersion = 77;
	ResetDirtyFlags(world);

	const std::filesystem::path snapshotPath = GetTestVoxelWorldSnapshotPath();
	std::error_code removeError;
	std::filesystem::remove(snapshotPath, removeError);

	EXPECT_TRUE(context, SaveVoxelWorldSnapshot(world, snapshotPath.string()).has_value());
	const std::unique_ptr<VoxelWorld> loadedWorld = LoadVoxelWorldSnapshot(snapshotPath.string()).value();
	EXPECT_TRUE(context, loadedWorld != nullptr);
	if (!loadedWorld) {
		return;
	}

	EXPECT_EQ(context, world.scenePreset, loadedWorld->scenePreset);
	EXPECT_EQ(context, world.config.floorSize, loadedWorld->config.floorSize);
	EXPECT_EQ(context, world.config.floorY, loadedWorld->config.floorY);
	EXPECT_EQ(context, world.config.worldTopY, loadedWorld->config.worldTopY);
	EXPECT_EQ(context, world.config.padding, loadedWorld->config.padding);
	EXPECT_EQ(context, world.config.chunkSize, loadedWorld->config.chunkSize);
	EXPECT_EQ(context, world.min.x, loadedWorld->min.x);
	EXPECT_EQ(context, world.min.y, loadedWorld->min.y);
	EXPECT_EQ(context, world.min.z, loadedWorld->min.z);
	EXPECT_EQ(context, world.maxExclusive.x, loadedWorld->maxExclusive.x);
	EXPECT_EQ(context, world.maxExclusive.y, loadedWorld->maxExclusive.y);
	EXPECT_EQ(context, world.maxExclusive.z, loadedWorld->maxExclusive.z);
	EXPECT_EQ(context, world.editVersion, loadedWorld->editVersion);
	EXPECT_TRUE(context, BuildFlatVoxelSnapshot(world) == BuildFlatVoxelSnapshot(*loadedWorld));
	EXPECT_EQ(context, world.stats.nonAirVoxelCount, loadedWorld->stats.nonAirVoxelCount);
	EXPECT_EQ(context, world.stats.glassVoxelCount, loadedWorld->stats.glassVoxelCount);
	EXPECT_EQ(context, world.stats.fluidVoxelCount, loadedWorld->stats.fluidVoxelCount);
	EXPECT_EQ(context, world.stats.floorWhiteVoxelCount, loadedWorld->stats.floorWhiteVoxelCount);
	EXPECT_EQ(context, world.stats.floorGrayVoxelCount, loadedWorld->stats.floorGrayVoxelCount);
	EXPECT_EQ(context, world.stats.activeChunkCount, loadedWorld->stats.activeChunkCount);
	EXPECT_EQ(context, static_cast<uint32_t>(loadedWorld->chunks.size()), CountDirtyVoxelChunks(*loadedWorld));
	EXPECT_EQ(context, loadedWorld->chunks.size(), loadedWorld->pendingChunkRebuildIndices.size());
	EXPECT_EQ(context, loadedWorld->chunks.size(), loadedWorld->pendingBlasRebuildIndices.size()); // Verify all chunks queued for BLAS rebuild on load
	for (size_t chunkIndex = 0; chunkIndex < loadedWorld->chunks.size(); ++chunkIndex) {
		EXPECT_TRUE(context, loadedWorld->chunks[chunkIndex].rebuildQueued);
		EXPECT_EQ(context, world.chunks[chunkIndex].nonAirVoxelCount, loadedWorld->chunks[chunkIndex].nonAirVoxelCount);
	}

	std::filesystem::remove(snapshotPath, removeError);
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
	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Fluid, nullptr);

	EXPECT_EQ(context, static_cast<uint32_t>(1), CountVoxelsByMaterial(world, VoxelMaterial::Fluid));
	EXPECT_EQ(context, static_cast<uint32_t>(1), world.stats.nonAirVoxelCount);
	EXPECT_EQ(context, static_cast<uint32_t>(1), CountActiveVoxelChunks(world));
	EXPECT_EQ(context, static_cast<uint32_t>(1), CountDirtyVoxelChunks(world));
	EXPECT_EQ(context, static_cast<size_t>(1), world.pendingChunkRebuildIndices.size());

	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Fluid, nullptr);
	EXPECT_EQ(context, static_cast<uint32_t>(1), CountDirtyVoxelChunks(world));
	EXPECT_EQ(context, static_cast<size_t>(1), world.pendingChunkRebuildIndices.size());

	std::vector<size_t> rebuildRequests;
	CollectDirtyVoxelChunkRebuildRequests(world, &rebuildRequests);
	CommitDirtyVoxelChunkRebuildRequests(world, rebuildRequests);
	EXPECT_EQ(context, static_cast<uint32_t>(0), CountDirtyVoxelChunks(world));

	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Air, nullptr);
	EXPECT_EQ(context, static_cast<uint32_t>(0), CountVoxelsByMaterial(world, VoxelMaterial::Fluid));
	EXPECT_EQ(context, static_cast<uint32_t>(0), world.stats.nonAirVoxelCount);
	EXPECT_EQ(context, static_cast<uint32_t>(0), CountActiveVoxelChunks(world));
	EXPECT_EQ(context, static_cast<uint32_t>(1), CountDirtyVoxelChunks(world));
	EXPECT_EQ(context, static_cast<size_t>(1), world.pendingChunkRebuildIndices.size());
}

void TestSetVoxelMaterialMarksNeighborChunksDirtyAtBoundaries(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {16, 16, 16}, 8);

	SetVoxelMaterial(world, {7, 7, 7}, VoxelMaterial::Glass, nullptr);

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

	SetVoxelMaterial(world, {7, 4, 4}, VoxelMaterial::Glass, nullptr);

	EXPECT_EQ(context, static_cast<uint32_t>(2), CountDirtyVoxelChunks(world));
	EXPECT_EQ(context, static_cast<size_t>(2), world.pendingChunkRebuildIndices.size());
	std::ranges::sort(world.pendingChunkRebuildIndices);
	EXPECT_EQ(context, static_cast<size_t>(0), world.pendingChunkRebuildIndices[0]);
	EXPECT_EQ(context, static_cast<size_t>(1), world.pendingChunkRebuildIndices[1]);
}

void TestSetVoxelMaterialDoesNotQueueMissingWorldNeighbors(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {16, 16, 16}, 8);

	SetVoxelMaterial(world, {0, 0, 0}, VoxelMaterial::Glass, nullptr);

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
	const VoxelMaterialVisual fluid = GetVoxelMaterialVisual(VoxelMaterial::Fluid);

	EXPECT_TRUE(context, air.baseColor[3] == 0.0f);
	EXPECT_TRUE(context, glass.baseColor[3] < 1.0f);
	EXPECT_TRUE(context, glass.surface[1] < 0.2f);
	EXPECT_TRUE(context, glass.medium[3] > 0.0f);
	EXPECT_TRUE(context, fluid.shading[1] > 0.0f);
	EXPECT_TRUE(context, glass.shading[2] > 0.0f);
	EXPECT_TRUE(context, fluid.shading[3] > 0.0f);
	EXPECT_TRUE(context, fluid.surface[2] == 0.0f);
	EXPECT_TRUE(context, glass.bindlessIndices[0] == 0xffffffffu);
}

void TestVoxelSceneLightingPresetsProvideDistinctLooks(TestContext &context)
{
	const VoxelSceneLighting voxelLab = GetVoxelSceneLighting(VoxelScenePreset::VoxelLab);
	const VoxelSceneLighting chunkGrid = GetVoxelSceneLighting(VoxelScenePreset::ChunkGrid);

	EXPECT_TRUE(context, !ColorsMatch(voxelLab.skyColorAndFogDensity, chunkGrid.skyColorAndFogDensity));
	EXPECT_TRUE(context, voxelLab.sunColorAndIntensity[3] > 0.0f);
	EXPECT_TRUE(context, chunkGrid.horizonColorAndFogStart[3] > 0.0f);
	EXPECT_TRUE(context, chunkGrid.groundColorAndFogMax[3] > 0.0f);
	EXPECT_TRUE(context, voxelLab.postProcess[0] > 0.0f);
	EXPECT_TRUE(context, voxelLab.postProcess[1] > 0.0f);
	EXPECT_TRUE(context, chunkGrid.postProcess[0] > 0.0f);
	EXPECT_TRUE(context, chunkGrid.postProcess[1] > 0.0f);
	EXPECT_TRUE(context, std::abs(chunkGrid.postProcess[1] - voxelLab.postProcess[1]) > 0.001f);
	EXPECT_TRUE(context, voxelLab.postProcess[2] == static_cast<float>(ToneMapOperator::AcesApprox));
	EXPECT_TRUE(context, voxelLab.colorGrading[0] > 0.0f);
	EXPECT_TRUE(context, voxelLab.colorGrading[1] > 0.0f);
	EXPECT_TRUE(context, voxelLab.colorGrading[2] > 0.0f);
	EXPECT_TRUE(context, chunkGrid.colorGrading[1] >= voxelLab.colorGrading[1]);
	EXPECT_TRUE(context, voxelLab.exposureControl[0] == static_cast<float>(ExposureMeteringMode::SceneKey));
	EXPECT_TRUE(context, voxelLab.exposureControl[1] > 0.0f);
	EXPECT_TRUE(context, voxelLab.exposureControl[2] > 0.0f);
	EXPECT_TRUE(context, voxelLab.exposureControl[3] > voxelLab.exposureControl[2]);
	EXPECT_TRUE(context, voxelLab.sunContactShadowParams[0] > 0.0f);
	EXPECT_TRUE(context, chunkGrid.sunContactShadowParams[1] > 0.0f);
	EXPECT_TRUE(context, voxelLab.ambientOcclusionParams[0] > 0.0f);
	EXPECT_TRUE(context, chunkGrid.ambientOcclusionParams[1] > 0.0f);
	EXPECT_TRUE(context, voxelLab.ambientOcclusionParams[2] > 0.0f);
	EXPECT_TRUE(context, voxelLab.localPointLightParams[0] == 0.0f);
	EXPECT_TRUE(context, voxelLab.localPointLightPositionAndRadius[3] > 0.0f);
	EXPECT_TRUE(context, chunkGrid.localPointLightColorAndIntensity[3] > 0.0f);
	EXPECT_TRUE(context, voxelLab.localPointLightParams[2] > 0.0f);
	EXPECT_TRUE(context, chunkGrid.localPointLightParams[3] > 0.0f);
	EXPECT_TRUE(context, !ColorsMatch(voxelLab.localPointLightColorAndIntensity, chunkGrid.localPointLightColorAndIntensity));
}

void TestBuildVoxelSceneLightingAppliesLookDevControls(TestContext &context)
{
	const VoxelSceneLighting authoredLighting = GetVoxelSceneLighting(VoxelScenePreset::VoxelLab);
	const VoxelSceneLighting baseLighting = BuildVoxelSceneLighting(VoxelScenePreset::VoxelLab, {});
	VoxelLightingDebugControls controls{};
	controls.exposureBiasStops = 1.0f;
	controls.toneMapOperator = ToneMapOperator::Reinhard;
	controls.debugView = LightingDebugView::Direct;

	const VoxelSceneLighting tunedLighting = BuildVoxelSceneLighting(VoxelScenePreset::VoxelLab, controls);
	EXPECT_TRUE(context, tunedLighting.postProcess[0] > authoredLighting.postProcess[0]);
	EXPECT_TRUE(context, EstimateVoxelSceneExposureKey(tunedLighting) > 0.0f);
	EXPECT_TRUE(
		context,
		std::string_view(ExposureMeteringModeToString(static_cast<ExposureMeteringMode>(
			std::lround(tunedLighting.exposureControl[0])))) == "SCENEKEY");
	EXPECT_TRUE(context, tunedLighting.postProcess[2] == static_cast<float>(ToneMapOperator::Reinhard));
	EXPECT_TRUE(context, tunedLighting.postProcess[3] == static_cast<float>(LightingDebugView::Direct));
	EXPECT_TRUE(context, tunedLighting.sunContactShadowParams[0] == baseLighting.sunContactShadowParams[0]);
	EXPECT_TRUE(context, tunedLighting.sunContactShadowParams[1] == baseLighting.sunContactShadowParams[1]);
	EXPECT_TRUE(context, tunedLighting.ambientOcclusionParams[0] == baseLighting.ambientOcclusionParams[0]);
	EXPECT_TRUE(context, tunedLighting.ambientOcclusionParams[1] == baseLighting.ambientOcclusionParams[1]);
	EXPECT_TRUE(context, tunedLighting.ambientOcclusionParams[2] == baseLighting.ambientOcclusionParams[2]);
	EXPECT_TRUE(context, tunedLighting.localPointLightParams[0] == baseLighting.localPointLightParams[0]);
	EXPECT_TRUE(context, tunedLighting.localPointLightParams[1] == baseLighting.localPointLightParams[1]);
	EXPECT_TRUE(context, tunedLighting.localPointLightParams[2] == baseLighting.localPointLightParams[2]);
	EXPECT_TRUE(context, tunedLighting.localPointLightParams[3] == baseLighting.localPointLightParams[3]);
	EXPECT_TRUE(context, tunedLighting.localPointLightPositionAndRadius[3] == baseLighting.localPointLightPositionAndRadius[3]);
	EXPECT_TRUE(context, tunedLighting.localPointLightColorAndIntensity[3] == baseLighting.localPointLightColorAndIntensity[3]);

	const std::array<float, 4> clearColor = GetVoxelSceneClearColor(tunedLighting);
	EXPECT_TRUE(context, clearColor[0] >= 0.0f && clearColor[0] <= 1.0f);
	EXPECT_TRUE(context, clearColor[1] >= 0.0f && clearColor[1] <= 1.0f);
	EXPECT_TRUE(context, clearColor[2] >= 0.0f && clearColor[2] <= 1.0f);
	EXPECT_TRUE(context, clearColor[3] == 1.0f);
}

void TestLightingDebugViewCycleIncludesShadow(TestContext &context)
{
	EXPECT_TRUE(context, std::string_view(LightingDebugViewToString(LightingDebugView::Shadow)) == "SHDW");
	EXPECT_TRUE(context, std::string_view(LightingDebugViewToString(LightingDebugView::Local)) == "LOCL");
	EXPECT_TRUE(context, std::string_view(LightingDebugViewToString(LightingDebugView::Contact)) == "CTSH");
	EXPECT_TRUE(context, std::string_view(LightingDebugViewToString(LightingDebugView::Occlusion)) == "AOCC");
	EXPECT_TRUE(context, std::string_view(LightingDebugViewToString(LightingDebugView::DiffuseGI)) == "GI_DIF");
	EXPECT_TRUE(context, std::string_view(LightingDebugViewToString(LightingDebugView::SpecularGI)) == "GI_SPC");
	EXPECT_TRUE(context, std::string_view(LightingDebugViewToString(LightingDebugView::RtxSpecular)) == "RTX_SPC");
	EXPECT_TRUE(context, std::string_view(LightingDebugViewToString(LightingDebugView::VolumetricFog)) == "VOL_FOG");
	EXPECT_TRUE(context, std::string_view(LightingDebugViewToString(LightingDebugView::VolumetricTransmittance)) == "VOL_TRN");
	EXPECT_TRUE(context, std::string_view(LightingDebugViewToString(LightingDebugView::GreedyMeshing)) == "MESH");
	EXPECT_TRUE(context, std::string_view(LightingDebugViewToString(LightingDebugView::ToneMapOutput)) == "TONEMAP");
	EXPECT_TRUE(context, std::string_view(LightingDebugViewToString(LightingDebugView::ColorGradingOutput)) == "GRADING");
	EXPECT_TRUE(context, std::string_view(LightingDebugViewToString(LightingDebugView::ExposureCurve)) == "EXPOSURE");
	EXPECT_TRUE(
		context,
		GetNextLightingDebugView(LightingDebugView::Direct) == LightingDebugView::Local);
	EXPECT_TRUE(
		context,
		GetNextLightingDebugView(LightingDebugView::Local) == LightingDebugView::Shadow);
	EXPECT_TRUE(
		context,
		GetNextLightingDebugView(LightingDebugView::Shadow) == LightingDebugView::Contact);
	EXPECT_TRUE(
		context,
		GetNextLightingDebugView(LightingDebugView::Contact) == LightingDebugView::Occlusion);
	EXPECT_TRUE(
		context,
		GetNextLightingDebugView(LightingDebugView::Occlusion) == LightingDebugView::Fog);
	EXPECT_TRUE(
		context,
		GetNextLightingDebugView(LightingDebugView::Fog) == LightingDebugView::DiffuseGI);
	EXPECT_TRUE(
		context,
		GetNextLightingDebugView(LightingDebugView::DiffuseGI) == LightingDebugView::SpecularGI);
	EXPECT_TRUE(
		context,
		GetNextLightingDebugView(LightingDebugView::SpecularGI) == LightingDebugView::RtxSpecular);
	EXPECT_TRUE(
		context,
		GetNextLightingDebugView(LightingDebugView::RtxSpecular) == LightingDebugView::VolumetricFog);
	EXPECT_TRUE(
		context,
		GetNextLightingDebugView(LightingDebugView::VolumetricFog) == LightingDebugView::VolumetricTransmittance);
	EXPECT_TRUE(
		context,
		GetNextLightingDebugView(LightingDebugView::VolumetricTransmittance) == LightingDebugView::GreedyMeshing);
	EXPECT_TRUE(
		context,
		GetNextLightingDebugView(LightingDebugView::GreedyMeshing) == LightingDebugView::ToneMapOutput);
	EXPECT_TRUE(
		context,
		GetNextLightingDebugView(LightingDebugView::ToneMapOutput) == LightingDebugView::ColorGradingOutput);
	EXPECT_TRUE(
		context,
		GetNextLightingDebugView(LightingDebugView::ColorGradingOutput) == LightingDebugView::ExposureCurve);
	EXPECT_TRUE(
		context,
		GetNextLightingDebugView(LightingDebugView::ExposureCurve) == LightingDebugView::VctConeCount);
	EXPECT_TRUE(
		context,
		GetNextLightingDebugView(LightingDebugView::VctConeCount) == LightingDebugView::VctConeDirections);
	EXPECT_TRUE(
		context,
		GetNextLightingDebugView(LightingDebugView::VctConeDirections) == LightingDebugView::Final);
}

void TestShadowTuningTargetCycleAndLabels(TestContext &context)
{
	(void)context;
	// CSM removed per TODO.md §5.2.D (session 20x). The shadow tuning
	// target enum and its cycle/labels are retired; ShadowTuningTargetToString
	// returns a no-op string and GetNextShadowTuningTarget is a no-op cycle.
}

void TestBuildSunShadowProjectionFitsSceneBounds(TestContext &context)
{
	(void)context;
	// CSM removed per TODO.md §5.2.D (session 20x). RTX shadows are the
	// canonical sun shadow path; the BuildSunShadowProjection helper
	// and its tests are retired.
}

void TestBuildSunShadowProjectionUsesActiveChunkBoundsInsteadOfEmptyPadding(TestContext &context)
{
	(void)context;
	// CSM removed per TODO.md §5.2.D (session 20x).
}

void TestBuildSunShadowProjectionInterpretsSunDirectionAsTowardsSun(TestContext &context)
{
	(void)context;
	// CSM removed per TODO.md §5.2.D (session 20x).
}

void TestBuildSunShadowCascadeSplitsUsesStablePracticalSplitScheme(TestContext &context)
{
	(void)context;
	// CSM removed per TODO.md §5.2.D (session 20x).
}

void TestBuildSunShadowCascadeSplitsHonorsUniformAndLogarithmicLimits(TestContext &context)
{
	(void)context;
	// CSM removed per TODO.md §5.2.D (session 20x).
}

void TestBuildSunShadowCascadeSplitsClampsInvalidInputs(TestContext &context)
{
	(void)context;
	// CSM removed per TODO.md §5.2.D (session 20x).
}

void TestBuildSunShadowCascadeProjectionsFitEachViewDepthSlice(TestContext &context)
{
	(void)context;
	// CSM removed per TODO.md §5.2.D (session 20x).
}

void TestBuildSunShadowCascadeProjectionsSnapToShadowTexelGrid(TestContext &context)
{
	(void)context;
	// CSM removed per TODO.md §5.2.D (session 20x).
}

void TestBuildSunShadowCascadeProjectionsUseCascadeSpecificCasterCoverage(TestContext &context)
{
	(void)context;
	// CSM removed per TODO.md §5.2.D (session 20x).
}

void TestBuildSunShadowCascadeProjectionsExpandOrthoExtentForTallCasters(TestContext &context)
{
	(void)context;
	// CSM removed per TODO.md §5.2.D (session 20x).
}

void TestBuildSunShadowCascadeProjectionsKeepExpandedCastersAheadOfNearPlane(TestContext &context)
{
	(void)context;
	// CSM removed per TODO.md §5.2.D (session 20x).
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

void TestStartupCameraOverrideReadsEnvironment(TestContext &context)
{
	CameraState camera{};
	SDL_setenv_unsafe("PROJECTV_START_CAMERA_POSITION", "-25 19 25", 1);
	SDL_setenv_unsafe("PROJECTV_START_CAMERA_LOOK", "0.62 -0.48 -0.62", 1);

	ApplyStartupCameraOverrideFromEnvironment(&camera);

	EXPECT_NEAR(context, -25.0f, camera.position[0]);
	EXPECT_NEAR(context, 19.0f, camera.position[1]);
	EXPECT_NEAR(context, 25.0f, camera.position[2]);

	const std::array<float, 3> forward = GetCameraForwardVector(camera);
	const float lookLength = std::sqrt(0.62f * 0.62f + -0.48f * -0.48f + -0.62f * -0.62f);
	EXPECT_NEAR(context, 0.62f / lookLength, forward[0]);
	EXPECT_NEAR(context, -0.48f / lookLength, forward[1]);
	EXPECT_NEAR(context, -0.62f / lookLength, forward[2]);

	SDL_unsetenv_unsafe("PROJECTV_START_CAMERA_POSITION");
	SDL_unsetenv_unsafe("PROJECTV_START_CAMERA_LOOK");
}

void TestLookDevCaptureAutomationRequestsConfiguredViews(TestContext &context)
{
	SDL_setenv_unsafe("PROJECTV_LOOKDEV_CAPTURE_VIEWS", "FINAL,LOCL,CTSH,AOCC,SHDW", 1);
	SDL_setenv_unsafe("PROJECTV_LOOKDEV_CAPTURE_WARMUP_FRAMES", "1", 1);
	SDL_setenv_unsafe("PROJECTV_LOOKDEV_CAPTURE_INTERVAL_FRAMES", "1", 1);
	SDL_setenv_unsafe("PROJECTV_LOOKDEV_CAPTURE_QUIT", "1", 1);

	LookDevCaptureAutomationState automation{};
	ConfigureLookDevCaptureAutomationFromEnvironment(&automation);
	EXPECT_TRUE(context, automation.active);
	EXPECT_EQ(context, 5u, automation.viewCount);

	RenderState render{};
	EXPECT_TRUE(context, !UpdateLookDevCaptureAutomation(&automation, &render));
	EXPECT_TRUE(context, !render.screenshotCaptureRequested);

	EXPECT_TRUE(context, !UpdateLookDevCaptureAutomation(&automation, &render));
	EXPECT_EQ(context, LightingDebugView::Final, render.lightingDebugControls.debugView);
	EXPECT_TRUE(context, render.screenshotCaptureRequested);
	render.screenshotCaptureRequested = false;

	EXPECT_TRUE(context, !UpdateLookDevCaptureAutomation(&automation, &render));
	EXPECT_TRUE(context, !render.screenshotCaptureRequested);

	EXPECT_TRUE(context, !UpdateLookDevCaptureAutomation(&automation, &render));
	EXPECT_EQ(context, LightingDebugView::Local, render.lightingDebugControls.debugView);
	EXPECT_TRUE(context, render.screenshotCaptureRequested);
	render.screenshotCaptureRequested = false;

	EXPECT_TRUE(context, !UpdateLookDevCaptureAutomation(&automation, &render));
	EXPECT_TRUE(context, !render.screenshotCaptureRequested);

	EXPECT_TRUE(context, !UpdateLookDevCaptureAutomation(&automation, &render));
	EXPECT_EQ(context, LightingDebugView::Contact, render.lightingDebugControls.debugView);
	EXPECT_TRUE(context, render.screenshotCaptureRequested);
	render.screenshotCaptureRequested = false;

	EXPECT_TRUE(context, !UpdateLookDevCaptureAutomation(&automation, &render));
	EXPECT_TRUE(context, !render.screenshotCaptureRequested);

	EXPECT_TRUE(context, !UpdateLookDevCaptureAutomation(&automation, &render));
	EXPECT_EQ(context, LightingDebugView::Occlusion, render.lightingDebugControls.debugView);
	EXPECT_TRUE(context, render.screenshotCaptureRequested);
	render.screenshotCaptureRequested = false;

	EXPECT_TRUE(context, !UpdateLookDevCaptureAutomation(&automation, &render));
	EXPECT_TRUE(context, !render.screenshotCaptureRequested);

	EXPECT_TRUE(context, UpdateLookDevCaptureAutomation(&automation, &render));
	EXPECT_EQ(context, LightingDebugView::Shadow, render.lightingDebugControls.debugView);
	EXPECT_TRUE(context, render.screenshotCaptureRequested);
	EXPECT_TRUE(context, automation.completed);

	SDL_unsetenv_unsafe("PROJECTV_LOOKDEV_CAPTURE_VIEWS");
	SDL_unsetenv_unsafe("PROJECTV_LOOKDEV_CAPTURE_WARMUP_FRAMES");
	SDL_unsetenv_unsafe("PROJECTV_LOOKDEV_CAPTURE_INTERVAL_FRAMES");
	SDL_unsetenv_unsafe("PROJECTV_LOOKDEV_CAPTURE_QUIT");
}

void TestVoxelRaycastHitsSolidVoxelAndReturnsPlacementCell(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Glass, nullptr);
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
	SetVoxelMaterial(world, {0, 1, 1}, VoxelMaterial::Fluid, nullptr);
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
	const Uint64 timestamp = 0)
{
	SDL_Event event{};
	event.type = eventType;
	event.key.scancode = scancode;
	event.key.repeat = false;
	event.key.timestamp = timestamp;
	HandleInputActionEvent(*input, &event);
}

void PressInputAction(InputState &input, const InputAction action)
{
	input.actions[static_cast<size_t>(action)].pressed = true;
}

void SendRepeatedKeyEvent(
	InputState *input,
	const Uint32 eventType,
	const SDL_Scancode scancode,
	const Uint64 timestamp = 0)
{
	SDL_Event event{};
	event.type = eventType;
	event.key.scancode = scancode;
	event.key.repeat = true;
	event.key.timestamp = timestamp;
	HandleInputActionEvent(*input, &event);
}

CameraState MakeTestCamera(const std::array<float, 3> &position)
{
	CameraState camera{};
	camera.position = projectv::math::Vec3{position[0], position[1], position[2]};
	camera.yawRadians = 0.0f;
	camera.pitchRadians = 0.0f;
	return camera;
}

bool AdvanceUpdateAppWithSimulatedFrameDelta(
	PlatformState *platform,
	SimulationState *simulation,
	CameraState *camera,
	InputState *input,
	InteractionState *interaction,
	WorldState *world,
	PhysicsState *physics,
	RenderState *render,
	DebugState *debug,
	const float deltaSeconds)
{
	const Uint64 frequency = SDL_GetPerformanceFrequency();
	const Uint64 deltaCounter = std::max<Uint64>(
		1,
		static_cast<Uint64>(static_cast<double>(deltaSeconds) * static_cast<double>(frequency)));
	simulation->lastFrameCounter = SDL_GetPerformanceCounter() - deltaCounter;
	return UpdateApp(platform, simulation, camera, input, interaction, world, physics, render, debug);
}

const char *ToString(const CameraState::ControlMode mode)
{
	switch (mode) {
	case CameraState::ControlMode::Creative:
		return "Creative";
	case CameraState::ControlMode::Spectator:
		return "Spectator";
	case CameraState::ControlMode::Walk:
		return "Walk";
	}

	return "Unknown";
}

const char *ToString(const PhysicsWalkSupportDebugState state)
{
	switch (state) {
	case PhysicsWalkSupportDebugState::Air:
		return "Air";
	case PhysicsWalkSupportDebugState::Grounded:
		return "Grounded";
	case PhysicsWalkSupportDebugState::EdgeGrace:
		return "EdgeGrace";
	}

	return "Unknown";
}

std::string DescribeInputActionMask(const uint32_t mask)
{
	std::string text;
	const auto appendAction = [&text](const char *label) {
		if (!text.empty()) {
			text += '|';
		}
		text += label;
	};

	const auto hasAction = [mask](const InputAction action) {
		return (mask & 1u << static_cast<uint32_t>(action)) != 0u;
	};

	if (hasAction(InputAction::MoveForward)) {
		appendAction("W");
	}
	if (hasAction(InputAction::MoveBackward)) {
		appendAction("S");
	}
	if (hasAction(InputAction::MoveLeft)) {
		appendAction("A");
	}
	if (hasAction(InputAction::MoveRight)) {
		appendAction("D");
	}
	if (hasAction(InputAction::MoveUp)) {
		appendAction("SPACE");
	}
	if (hasAction(InputAction::MoveDown)) {
		appendAction("SHIFT");
	}
	if (hasAction(InputAction::SpeedBoost)) {
		appendAction("CTRL");
	}
	if (hasAction(InputAction::ToggleWalkCreativeMode)) {
		appendAction("DBLSPACE");
	}
	if (text.empty()) {
		text = "-";
	}

	return text;
}

int RunReplayAnalysisFromEnvironment()
{
	const char *requestedReplayPath = SDL_getenv("PROJECTV_ANALYZE_REPLAY_PATH");
	if (requestedReplayPath == nullptr || *requestedReplayPath == '\0') {
		return -1;
	}

	const std::string replayPath = requestedReplayPath;
	InputReplayCapture capture{};
	if (!LoadInputReplayCapture(replayPath, &capture)) {
		std::fprintf(stderr, "[ReplayAnalysis] failed to load replay: %s\n", replayPath.c_str());
		return EXIT_FAILURE;
	}

	auto loadResult = LoadVoxelWorldSnapshot(capture.snapshotPath.string());
	if (!loadResult.has_value()) {
		std::fprintf(stderr, "[ReplayAnalysis] failed to load snapshot: %s\n", capture.snapshotPath.string().c_str());
		return EXIT_FAILURE;
	}
	std::unique_ptr<VoxelWorld> world = std::move(loadResult).value();

	PlatformState platform{};
	SimulationState simulation{};
	CameraState camera = capture.initialCamera;
	InputState input{};
	InitializeInputState(input);
	InteractionState interaction = capture.initialInteraction;
	WorldState worldState{};
	worldState.voxelWorld = std::move(world);
	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	RenderState render{};
	DebugState debug{};
	if (!physics) {
		std::fprintf(stderr, "[ReplayAnalysis] failed to create physics state\n");
		return EXIT_FAILURE;
	}
	if (!SyncPhysicsWorld(physics.get(), worldState.voxelWorld.get())) {
		std::fprintf(stderr, "[ReplayAnalysis] SyncPhysicsWorld failed\n");
		return EXIT_FAILURE;
	}
	SetPhysicsWalkAirControlMode(physics.get(), capture.walkAirControlMode);
	SetPhysicsWalkAutoJumpEnabled(physics.get(), capture.walkAutoJumpEnabled);
	if (camera.controlMode == CameraState::ControlMode::Walk) {
		if (!SnapWalkCharacterToCamera(physics.get(), worldState.voxelWorld.get(), &camera)) {
			std::fprintf(stderr, "[ReplayAnalysis] SnapWalkCharacterToCamera failed\n");
			return EXIT_FAILURE;
		}
	} else if (camera.controlMode == CameraState::ControlMode::Creative) {
		if (!SnapCreativeCharacterToCamera(physics.get(), worldState.voxelWorld.get(), &camera)) {
			std::fprintf(stderr, "[ReplayAnalysis] SnapCreativeCharacterToCamera failed\n");
			return EXIT_FAILURE;
		}
	}

	std::fprintf(
		stderr,
		"[ReplayAnalysis] replay=%s frames=%zu snapshot=%s initialMode=%s\n",
		replayPath.c_str(),
		capture.frames.size(),
		capture.snapshotPath.string().c_str(),
		ToString(camera.controlMode));

	PhysicsWalkDebugInfo previousInfo = GetPhysicsWalkDebugInfo(physics.get());
	CameraState::ControlMode previousMode = camera.controlMode;
	uint32_t previousDownMask = 0u;
	float maxFeetY = previousInfo.feetPosition[1];
	float maxCameraY = camera.position[1];
	int lastPlacedEditVersion = static_cast<int>(worldState.voxelWorld->editVersion);
	for (size_t frameIndex = 0; frameIndex < capture.frames.size(); ++frameIndex) {
		const InputReplayFrame &frame = capture.frames[frameIndex];
		const bool downMaskChanged = frame.actionDownMask != previousDownMask;
		const bool jumpPressed =
			(frame.actionPressedMask & 1ull << static_cast<uint32_t>(InputAction::MoveUp)) != 0u;
		const bool toggleWalkCreativePressed =
			(frame.actionPressedMask & 1ull << static_cast<uint32_t>(InputAction::ToggleWalkCreativeMode)) != 0u;
		const bool toggleControlModePressed =
			(frame.actionPressedMask & 1ull << static_cast<uint32_t>(InputAction::ToggleControlMode)) != 0u;
		const bool interestingInput =
			downMaskChanged ||
			frame.placePressed ||
			frame.removePressed ||
			jumpPressed ||
			toggleWalkCreativePressed ||
			toggleControlModePressed;

		ApplyInputReplayFrame(&input, frame);
		if (!AdvanceUpdateAppWithSimulatedFrameDelta(
				&platform,
				&simulation,
				&camera,
				&input,
				&interaction,
				&worldState,
				physics.get(),
				&render,
				&debug,
				frame.deltaSeconds)) {
			std::fprintf(stderr, "[ReplayAnalysis] UpdateApp failed at frame %zu\n", frameIndex);
			return EXIT_FAILURE;
		}

		const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
		const bool supportChanged =
			info.valid != previousInfo.valid ||
			info.supportState != previousInfo.supportState ||
			info.sneakActive != previousInfo.sneakActive ||
			info.jumpLockActive != previousInfo.jumpLockActive;
		const bool modeChanged = camera.controlMode != previousMode;
		const bool newFeetPeak = info.valid && info.feetPosition[1] > maxFeetY + 0.02f;
		const bool newCameraPeak = camera.position[1] > maxCameraY + 0.02f;
		const bool suspiciousUpperSupport =
			info.valid &&
			info.supportState != PhysicsWalkSupportDebugState::Air &&
			info.feetPosition[1] >= previousInfo.feetPosition[1] + 0.20f;
		const bool worldEdited = static_cast<int>(worldState.voxelWorld->editVersion) != lastPlacedEditVersion;

		if (interestingInput || supportChanged || modeChanged || newFeetPeak || newCameraPeak || suspiciousUpperSupport || worldEdited) {
			std::fprintf(
				stderr,
				"[ReplayAnalysis] frame=%zu mode=%s cam=(%.3f, %.3f, %.3f) feet=(%.3f, %.3f, %.3f) support=%s score=%.3f input=%s pressSpace=%u dblSpace=%u place=%u remove=%u editVersion=%llu placement=(%d,%d,%d) target=(%d,%d,%d)\n",
				frameIndex,
				ToString(camera.controlMode),
				camera.position[0],
				camera.position[1],
				camera.position[2],
				info.feetPosition[0],
				info.feetPosition[1],
				info.feetPosition[2],
				ToString(info.supportState),
				info.footSupportScore,
				DescribeInputActionMask(frame.actionDownMask).c_str(),
				jumpPressed ? 1u : 0u,
				toggleWalkCreativePressed ? 1u : 0u,
				frame.placePressed ? 1u : 0u,
				frame.removePressed ? 1u : 0u,
				static_cast<unsigned long long>(worldState.voxelWorld->editVersion),
				interaction.selection.placementVoxel.x,
				interaction.selection.placementVoxel.y,
				interaction.selection.placementVoxel.z,
				interaction.selection.targetVoxel.x,
				interaction.selection.targetVoxel.y,
				interaction.selection.targetVoxel.z);
		}

		maxFeetY = std::max(maxFeetY, info.feetPosition[1]);
		maxCameraY = std::max(maxCameraY, camera.position[1]);
		previousInfo = info;
		previousMode = camera.controlMode;
		previousDownMask = frame.actionDownMask;
		lastPlacedEditVersion = static_cast<int>(worldState.voxelWorld->editVersion);
	}

	const PhysicsWalkDebugInfo finalInfo = GetPhysicsWalkDebugInfo(physics.get());
	std::fprintf(
		stderr,
		"[ReplayAnalysis] final mode=%s cam=(%.3f, %.3f, %.3f) feet=(%.3f, %.3f, %.3f) support=%s score=%.3f maxFeetY=%.3f maxCameraY=%.3f editVersion=%llu\n",
		ToString(camera.controlMode),
		camera.position[0],
		camera.position[1],
		camera.position[2],
		finalInfo.feetPosition[0],
		finalInfo.feetPosition[1],
		finalInfo.feetPosition[2],
		ToString(finalInfo.supportState),
		finalInfo.footSupportScore,
		maxFeetY,
		maxCameraY,
		static_cast<unsigned long long>(worldState.voxelWorld->editVersion));
	return EXIT_SUCCESS;
}

VoxelWorld MakeWalkTestWorld()
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	for (int z = world.min.z; z < world.maxExclusive.z; ++z) {
		for (int x = world.min.x; x < world.maxExclusive.x; ++x) {
			SetVoxelMaterial(world, {x, 0, z}, VoxelMaterial::FloorWhite, nullptr);
		}
	}
	ResetDirtyFlags(world);
	return world;
}

VoxelWorld MakeWalkLedgeTestWorld()
{
	VoxelWorld world = MakeWalkTestWorld();
	for (int z = 4; z < world.maxExclusive.z; ++z) {
		for (int x = world.min.x; x < world.maxExclusive.x; ++x) {
			SetVoxelMaterial(world, {x, 1, z}, VoxelMaterial::FloorGray, nullptr);
		}
	}
	ResetDirtyFlags(world);
	return world;
}

VoxelWorld MakeWalkLongLedgeTestWorld()
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {32, 8, 12}, 8);
	for (int z = world.min.z; z < world.maxExclusive.z; ++z) {
		for (int x = world.min.x; x < world.maxExclusive.x; ++x) {
			SetVoxelMaterial(world, {x, 0, z}, VoxelMaterial::FloorWhite, nullptr);
		}
	}
	for (int z = 4; z < world.maxExclusive.z; ++z) {
		for (int x = world.min.x; x < world.maxExclusive.x; ++x) {
			SetVoxelMaterial(world, {x, 1, z}, VoxelMaterial::FloorGray, nullptr);
		}
	}
	ResetDirtyFlags(world);
	return world;
}

VoxelWorld MakeWalkCornerLedgeTestWorld()
{
	VoxelWorld world = MakeWalkTestWorld();
	for (int z = 4; z < world.maxExclusive.z; ++z) {
		for (int x = 4; x < world.maxExclusive.x; ++x) {
			SetVoxelMaterial(world, {x, 1, z}, VoxelMaterial::FloorGray, nullptr);
		}
	}
	ResetDirtyFlags(world);
	return world;
}

VoxelWorld MakeWalkCornerLedgeLowCeilingTestWorld()
{
	VoxelWorld world = MakeWalkCornerLedgeTestWorld();
	for (int z = 4; z < world.maxExclusive.z; ++z) {
		for (int x = 4; x < world.maxExclusive.x; ++x) {
			SetVoxelMaterial(world, {x, 4, z}, VoxelMaterial::FloorGray, nullptr);
		}
	}
	ResetDirtyFlags(world);
	return world;
}

VoxelWorld MakeWalkNegativeSingleBlockTestWorld()
{
	VoxelWorld world = MakeTestWorld({-8, 0, 0}, {8, 8, 12}, 4);
	for (int z = world.min.z; z < world.maxExclusive.z; ++z) {
		for (int x = world.min.x; x < world.maxExclusive.x; ++x) {
			SetVoxelMaterial(world, {x, 0, z}, VoxelMaterial::FloorWhite, nullptr);
		}
	}
	SetVoxelMaterial(world, {-6, 1, 6}, VoxelMaterial::FloorGray, nullptr);
	ResetDirtyFlags(world);
	return world;
}

VoxelWorld MakeWalkPositiveSingleBlockTestWorld()
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {12, 8, 12}, 4);
	for (int z = world.min.z; z < world.maxExclusive.z; ++z) {
		for (int x = world.min.x; x < world.maxExclusive.x; ++x) {
			SetVoxelMaterial(world, {x, 0, z}, VoxelMaterial::FloorWhite, nullptr);
		}
	}
	SetVoxelMaterial(world, {5, 1, 6}, VoxelMaterial::FloorGray, nullptr);
	ResetDirtyFlags(world);
	return world;
}

VoxelWorld MakeWalkPositiveTwoBlockTestWorld()
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {12, 10, 12}, 4);
	for (int z = world.min.z; z < world.maxExclusive.z; ++z) {
		for (int x = world.min.x; x < world.maxExclusive.x; ++x) {
			SetVoxelMaterial(world, {x, 0, z}, VoxelMaterial::FloorWhite, nullptr);
		}
	}
	SetVoxelMaterial(world, {5, 1, 6}, VoxelMaterial::Glass, nullptr);
	SetVoxelMaterial(world, {5, 2, 6}, VoxelMaterial::Glass, nullptr);
	ResetDirtyFlags(world);
	return world;
}

VoxelWorld MakeWalkGlassColumnWallTestWorld()
{
	VoxelWorld world = MakeWalkTestWorld();
	for (int y = 1; y <= 4; ++y) {
		SetVoxelMaterial(world, {4, y, 5}, VoxelMaterial::Glass, nullptr);
	}
	ResetDirtyFlags(world);
	return world;
}

VoxelWorld MakeWalkNegativeSingleBlockSneakTestWorld()
{
	VoxelWorld world = MakeTestWorld({-12, 0, 0}, {8, 8, 8}, 4);
	SetVoxelMaterial(world, {-8, 3, 3}, VoxelMaterial::FloorGray, nullptr);
	ResetDirtyFlags(world);
	return world;
}

VoxelWorld MakeWalkIsolatedCornerSingleBlockTestWorld()
{
	VoxelWorld world = MakeTestWorld({-4, 0, -4}, {8, 24, 8}, 4);
	SetVoxelMaterial(world, {-1, 15, 0}, VoxelMaterial::FloorGray, nullptr);
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

	SendRepeatedKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	EXPECT_TRUE(context, IsInputActionDown(input, InputAction::MoveForward));
	EXPECT_TRUE(context, !ConsumeInputActionPressed(input, InputAction::MoveForward));

	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_W);
	EXPECT_TRUE(context, !IsInputActionDown(input, InputAction::MoveForward));

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	EXPECT_TRUE(context, IsInputActionDown(input, InputAction::MoveDown));
	EXPECT_TRUE(context, ConsumeInputActionPressed(input, InputAction::MoveDown));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_RSHIFT);
	EXPECT_TRUE(context, !IsInputActionDown(input, InputAction::MoveDown));

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_F1);
	EXPECT_TRUE(context, ConsumeInputActionPressed(input, InputAction::ToggleHud));
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_F2);
	EXPECT_TRUE(context, ConsumeInputActionPressed(input, InputAction::CyclePlacementMaterial));
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_GRAVE);
	EXPECT_TRUE(context, ConsumeInputActionPressed(input, InputAction::OpenHudSettings));
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_TAB);
	EXPECT_TRUE(context, ConsumeInputActionPressed(input, InputAction::ToggleRelativeMouseMode));

	EXPECT_EQ(
		context,
		SDL_SCANCODE_UNKNOWN,
		input.bindings[static_cast<size_t>(InputAction::CycleScenePreset)].scancodes[0]);
	EXPECT_EQ(
		context,
		SDL_SCANCODE_F2,
		input.bindings[static_cast<size_t>(InputAction::CyclePlacementMaterial)].scancodes[0]);
	EXPECT_EQ(
		context,
		SDL_SCANCODE_UNKNOWN,
		input.bindings[static_cast<size_t>(InputAction::CycleMsaaMode)].scancodes[0]);
	PressInputAction(input, InputAction::CycleScenePreset);
	EXPECT_TRUE(context, ConsumeInputActionPressed(input, InputAction::CycleScenePreset));

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE, SDL_MS_TO_NS(100));
	EXPECT_TRUE(context, ConsumeInputActionPressed(input, InputAction::MoveUp));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE, SDL_MS_TO_NS(160));
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE, SDL_MS_TO_NS(320));
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

void TestGetCameraVisibleSceneMaxDistanceClampsToMainlineRange(TestContext &context)
{
	CameraState farCamera = MakeTestCamera({0.0f, 0.0f, 0.0f});
	farCamera.nearPlane = 0.1f;
	farCamera.farPlane = 128.0f;
	EXPECT_NEAR(context, 64.0f, GetCameraVisibleSceneMaxDistance(farCamera));

	CameraState shortCamera = MakeTestCamera({0.0f, 0.0f, 0.0f});
	shortCamera.nearPlane = 0.1f;
	shortCamera.farPlane = 48.0f;
	EXPECT_NEAR(context, 48.0f, GetCameraVisibleSceneMaxDistance(shortCamera));

	CameraState invalidCamera = MakeTestCamera({0.0f, 0.0f, 0.0f});
	invalidCamera.nearPlane = 2.0f;
	invalidCamera.farPlane = 1.0f;
	EXPECT_NEAR(context, 2.0f, GetCameraVisibleSceneMaxDistance(invalidCamera));
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
	PackedSceneChunkDescriptor sourceDescriptor = MakePackedSceneChunkDescriptor({0, 0, 0}, {8, 8, 8});
	sourceDescriptor.chunkExtentAndNonAir[3] = 11u;
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

void TestSceneVoxelPayloadSyncPreservesMissedDirtyChunks(TestContext &context)
{
	SceneVoxelPayloadSyncState sync{};
	const std::array<size_t, 2> firstDirtyChunks{2u, 5u};
	const std::array<size_t, 2> secondDirtyChunks{5u, 7u};

	CompleteSceneVoxelPayloadSync(sync);
	AppendSceneVoxelPayloadDirtyChunks(sync, 5u, firstDirtyChunks);
	AppendSceneVoxelPayloadDirtyChunks(sync, 6u, secondDirtyChunks);

	EXPECT_EQ(
		context,
		SceneVoxelPayloadSyncMode::Patch,
		ResolveSceneVoxelPayloadSyncMode(sync, 4u, 6u));
	EXPECT_EQ(context, static_cast<size_t>(3), sync.pendingChunkIndices.size());
	EXPECT_EQ(context, static_cast<uint32_t>(2), sync.pendingChunkIndices[0]);
	EXPECT_EQ(context, static_cast<uint32_t>(5), sync.pendingChunkIndices[1]);
	EXPECT_EQ(context, static_cast<uint32_t>(7), sync.pendingChunkIndices[2]);

	SceneVoxelPayloadSyncState initialSync{};
	EXPECT_EQ(
		context,
		SceneVoxelPayloadSyncMode::Full,
		ResolveSceneVoxelPayloadSyncMode(initialSync, 0u, 1u));

	SceneVoxelPayloadSyncState missingVersionSync{};
	CompleteSceneVoxelPayloadSync(missingVersionSync);
	AppendSceneVoxelPayloadDirtyChunks(missingVersionSync, 6u, secondDirtyChunks);
	EXPECT_EQ(
		context,
		SceneVoxelPayloadSyncMode::Full,
		ResolveSceneVoxelPayloadSyncMode(missingVersionSync, 4u, 6u));
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
	PressInputAction(input, InputAction::ResetCamera);
	PressInputAction(input, InputAction::TogglePause);
	PressInputAction(input, InputAction::ToggleControlMode);
	PressInputAction(input, InputAction::CycleEditorTool);
	PressInputAction(input, InputAction::ToggleChunkBounds);
	PressInputAction(input, InputAction::ToggleDirtyChunkOverlay);
	PressInputAction(input, InputAction::ToggleDetailedHud);

	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, nullptr, &render, &debug));
	EXPECT_TRUE(context, !debug.hudVisible);
	EXPECT_TRUE(context, debug.statsOpen);
	EXPECT_TRUE(context, debug.detailedHudVisible);
	EXPECT_EQ(context, VoxelMaterial::FloorGray, interaction.placementMaterial);
	EXPECT_EQ(context, DebugEditorTool::Paint, interaction.editorTool);
	EXPECT_TRUE(context, simulation.paused);
	EXPECT_EQ(context, CameraState::ControlMode::Spectator, camera.controlMode);
	EXPECT_EQ(context, CameraState::ControlMode::Spectator, debug.stats.controlMode);
	EXPECT_TRUE(context, debug.stats.simulationPaused);
	EXPECT_TRUE(context, debug.showChunkBounds);
	EXPECT_TRUE(context, debug.showDirtyChunkOverlay);
	EXPECT_TRUE(context, debug.stats.detailedHudVisible);
	EXPECT_TRUE(context, debug.stats.showChunkBounds);
	EXPECT_TRUE(context, debug.stats.showDirtyChunkOverlay);
	EXPECT_NEAR(context, 0.0f, simulation.simulationAccumulatorSeconds);
	EXPECT_NEAR(context, 0.0f, camera.position[0]);
	EXPECT_NEAR(context, 8.0f, camera.position[1]);
	EXPECT_NEAR(context, 24.0f, camera.position[2]);
	EXPECT_NEAR(context, 10.0f, camera.moveSpeed);
	EXPECT_NEAR(context, 0.0f, camera.yawRadians);
	EXPECT_NEAR(context, -0.2f, camera.pitchRadians);
}

void TestUpdateAppUsesVisibleSceneDistanceForSunShadowCascadeSplits(TestContext &context)
{
	PlatformState platform{};
	SimulationState simulation{};
	CameraState camera = MakeTestCamera({2.0f, 3.0f, 4.0f});
	camera.nearPlane = 0.1f;
	camera.farPlane = 128.0f;
	InputState input{};
	InitializeInputState(input);
	InteractionState interaction{};
	WorldState world{};
	RenderState render{};
	DebugState debug{};

	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, nullptr, &render, &debug));
	// CSM removed per TODO.md §5.2.D (session 20x). The cascade-split
	// assertion block is retired; cascade split lambda / far plane /
	// depth splits are no longer computed.

	camera.farPlane = 40.0f;
	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, nullptr, &render, &debug));
}

void TestUpdateAppTogglesWalkAirControlMode(TestContext &context)
{
	PlatformState platform{};
	SimulationState simulation{};
	CameraState camera = MakeTestCamera({2.5f, 2.65f, 4.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
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
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), world.voxelWorld.get(), &camera));
	EXPECT_EQ(context, WalkAirControlMode::MinecraftLike, GetPhysicsWalkAirControlMode(physics.get()));

	PressInputAction(input, InputAction::ToggleWalkAirControlMode);
	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, physics.get(), &render, &debug));
	EXPECT_EQ(context, WalkAirControlMode::Realistic, GetPhysicsWalkAirControlMode(physics.get()));
	EXPECT_EQ(context, WalkAirControlMode::Realistic, debug.stats.walkAirControlMode);

	InputState secondInput{};
	InitializeInputState(secondInput);
	PressInputAction(secondInput, InputAction::ToggleWalkAirControlMode);
	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &secondInput, &interaction, &world, physics.get(), &render, &debug));
	EXPECT_EQ(context, WalkAirControlMode::MinecraftLike, GetPhysicsWalkAirControlMode(physics.get()));
	EXPECT_EQ(context, WalkAirControlMode::MinecraftLike, debug.stats.walkAirControlMode);
}

void TestUpdateAppTogglesWalkAutoJump(TestContext &context)
{
	PlatformState platform{};
	SimulationState simulation{};
	CameraState camera = MakeTestCamera({2.5f, 2.65f, 4.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
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
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), world.voxelWorld.get(), &camera));

	const bool initialAutoJumpEnabled = IsPhysicsWalkAutoJumpEnabled(physics.get());
	PressInputAction(input, InputAction::ToggleWalkAutoJump);
	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, physics.get(), &render, &debug));
	EXPECT_TRUE(context, IsPhysicsWalkAutoJumpEnabled(physics.get()) != initialAutoJumpEnabled);
	EXPECT_TRUE(context, debug.stats.walkAutoJumpEnabled == IsPhysicsWalkAutoJumpEnabled(physics.get()));

	InputState secondInput{};
	InitializeInputState(secondInput);
	PressInputAction(secondInput, InputAction::ToggleWalkAutoJump);
	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &secondInput, &interaction, &world, physics.get(), &render, &debug));
	EXPECT_TRUE(context, IsPhysicsWalkAutoJumpEnabled(physics.get()) == initialAutoJumpEnabled);
	EXPECT_TRUE(context, debug.stats.walkAutoJumpEnabled == initialAutoJumpEnabled);
}

void TestInputReplayCaptureRoundTripsFile(TestContext &context)
{
	InputReplayCapture capture{};
	capture.snapshotPath = GetTestInputReplaySnapshotPath();
	capture.initialCamera = MakeTestCamera({1.25f, 2.5f, 3.75f});
	capture.initialCamera.controlMode = CameraState::ControlMode::Walk;
	capture.initialCamera.yawRadians = 0.25f;
	capture.initialCamera.pitchRadians = -0.4f;
	capture.initialInteraction.placementMaterial = VoxelMaterial::Glass;
	capture.initialInteraction.maxInteractionDistance = 9.5f;
	capture.initialInteraction.editorTool = DebugEditorTool::Erase;
	capture.walkAirControlMode = WalkAirControlMode::Realistic;
	capture.walkAutoJumpEnabled = true;
	capture.walkAutoJumpDelayEnabled = false;
	capture.frames.push_back({
		.deltaSeconds = 1.0f / 60.0f,
		.mouseDeltaX = 3.5f,
		.mouseDeltaY = -1.25f,
		.actionDownMask = 0x15u,
		.actionPressedMask = 0x04u,
		.removePressed = true,
		.placePressed = false,
	});
	capture.frames.push_back({
		.deltaSeconds = 1.0f / 120.0f,
		.mouseDeltaX = 0.0f,
		.mouseDeltaY = 0.0f,
		.actionDownMask = 0x02u,
		.actionPressedMask = 0x00u,
		.removePressed = false,
		.placePressed = true,
	});

	const std::filesystem::path replayPath = GetTestInputReplayPath();
	std::error_code removeError;
	std::filesystem::remove(replayPath, removeError);

	EXPECT_TRUE(context, SaveInputReplayCapture(capture, replayPath.string()));

	InputReplayCapture loaded{};
	EXPECT_TRUE(context, LoadInputReplayCapture(replayPath.string(), &loaded));
	EXPECT_TRUE(context, loaded.snapshotPath == capture.snapshotPath);
	EXPECT_EQ(context, capture.initialCamera.controlMode, loaded.initialCamera.controlMode);
	EXPECT_NEAR(context, capture.initialCamera.position[0], loaded.initialCamera.position[0]);
	EXPECT_NEAR(context, capture.initialCamera.position[1], loaded.initialCamera.position[1]);
	EXPECT_NEAR(context, capture.initialCamera.position[2], loaded.initialCamera.position[2]);
	EXPECT_NEAR(context, capture.initialCamera.yawRadians, loaded.initialCamera.yawRadians);
	EXPECT_NEAR(context, capture.initialCamera.pitchRadians, loaded.initialCamera.pitchRadians);
	EXPECT_EQ(context, capture.initialInteraction.placementMaterial, loaded.initialInteraction.placementMaterial);
	EXPECT_EQ(context, capture.initialInteraction.editorTool, loaded.initialInteraction.editorTool);
	EXPECT_EQ(context, capture.walkAirControlMode, loaded.walkAirControlMode);
	EXPECT_TRUE(context, capture.walkAutoJumpEnabled == loaded.walkAutoJumpEnabled);
	EXPECT_TRUE(context, !loaded.walkAutoJumpDelayEnabled);
	EXPECT_EQ(context, capture.frames.size(), loaded.frames.size());
	EXPECT_NEAR(context, capture.frames[0].deltaSeconds, loaded.frames[0].deltaSeconds);
	EXPECT_EQ(context, capture.frames[0].actionDownMask, loaded.frames[0].actionDownMask);
	EXPECT_EQ(context, capture.frames[0].actionPressedMask, loaded.frames[0].actionPressedMask);
	EXPECT_TRUE(context, capture.frames[0].removePressed == loaded.frames[0].removePressed);
	EXPECT_TRUE(context, capture.frames[1].placePressed == loaded.frames[1].placePressed);

	std::filesystem::remove(replayPath, removeError);
}

void TestInputReplayCanDriveWalkSequence(TestContext &context)
{
	VoxelWorld world = MakeWalkTestWorld();
	const std::filesystem::path snapshotPath = GetTestInputReplaySnapshotPath();
	const std::filesystem::path replayPath = GetTestInputReplayPath();
	std::error_code removeError;
	std::filesystem::remove(snapshotPath, removeError);
	std::filesystem::remove(replayPath, removeError);
	EXPECT_TRUE(context, SaveVoxelWorldSnapshot(world, snapshotPath.string()).has_value());

	InputReplayCapture capture{};
	capture.snapshotPath = snapshotPath;
	capture.initialCamera = MakeTestCamera({4.0f, 2.65f, 2.0f});
	capture.initialCamera.controlMode = CameraState::ControlMode::Walk;
	capture.walkAirControlMode = WalkAirControlMode::MinecraftLike;
	capture.walkAutoJumpEnabled = true;
	capture.walkAutoJumpDelayEnabled = false;
	for (int frameIndex = 0; frameIndex < 30; ++frameIndex) {
		InputReplayFrame frame{};
		frame.deltaSeconds = 1.0f / 60.0f;
		frame.actionDownMask = 1ull << static_cast<size_t>(InputAction::MoveForward);
		if (frameIndex == 4) {
			frame.actionPressedMask |= 1ull << static_cast<size_t>(InputAction::MoveUp);
		}
		if (frameIndex >= 4 && frameIndex < 8) {
			frame.actionDownMask |= 1ull << static_cast<size_t>(InputAction::MoveUp);
		}
		capture.frames.push_back(frame);
	}
	EXPECT_TRUE(context, SaveInputReplayCapture(capture, replayPath.string()));

	InputReplayCapture loaded{};
	EXPECT_TRUE(context, LoadInputReplayCapture(replayPath.string(), &loaded));
	std::unique_ptr<VoxelWorld> loadedWorld = LoadVoxelWorldSnapshot(loaded.snapshotPath.string()).value();
	EXPECT_TRUE(context, loadedWorld != nullptr);
	if (!loadedWorld) {
		return;
	}

	struct ReplayTestState {
		AppState app;
		DebugState debug;
	};
	ReplayTestState state{};
	state.app.world().voxelWorld = std::move(loadedWorld);
	state.app.physics() = PhysicsStatePtr(CreatePhysicsState(), DestroyPhysicsState);

	CameraState camera = loaded.initialCamera;
	InputState input{};
	InitializeInputState(input);
	state.app.interaction() = loaded.initialInteraction;

	EXPECT_TRUE(context, state.app.physics() != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(state.app.physics().get(), state.app.world().voxelWorld.get()));
	SetPhysicsWalkAirControlMode(state.app.physics().get(), loaded.walkAirControlMode);
	SetPhysicsWalkAutoJumpEnabled(state.app.physics().get(), loaded.walkAutoJumpEnabled);
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(state.app.physics().get(), state.app.world().voxelWorld.get(), &camera));

	float maxCameraY = camera.position[1];
	for (const InputReplayFrame &frame : loaded.frames) {
		ApplyInputReplayFrame(&input, frame);
		EXPECT_TRUE(context, AdvanceUpdateAppWithSimulatedFrameDelta(
								 &state.app.platform(),
								 &state.app.simulation(),
								 &camera,
								 &input,
								 &state.app.interaction(),
								 &state.app.world(),
								 state.app.physics().get(),
								 &state.app.render(),
								 &state.debug,
								 frame.deltaSeconds));
		maxCameraY = std::max(maxCameraY, camera.position[1]);
	}

	EXPECT_TRUE(context, camera.position[2] < loaded.initialCamera.position[2] - 0.5f);
	EXPECT_TRUE(context, maxCameraY > loaded.initialCamera.position[1] + 0.2f);

	std::filesystem::remove(snapshotPath, removeError);
	std::filesystem::remove(replayPath, removeError);
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
	PressInputAction(input, InputAction::ResetCamera);
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
	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Glass, nullptr);

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

	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Glass, physics.get());
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));
	EXPECT_EQ(context, world.editVersion, GetPhysicsWorldSyncVersion(physics.get()));

	hit = RaycastPhysicsWorld(
		physics.get(),
		{1.5f, 4.5f, 1.5f},
		{0.0f, -1.0f, 0.0f},
		8.0f);
	EXPECT_TRUE(context, hit.hasHit);
	EXPECT_INT3_EQ(context, (Int3{1, 1, 1}), hit.voxel);

	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Air, physics.get());
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

	PressInputAction(input, InputAction::ToggleControlMode);
	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, physics.get(), &render, &debug));
	EXPECT_EQ(context, CameraState::ControlMode::Spectator, camera.controlMode);

	PressInputAction(input, InputAction::ToggleControlMode);
	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, physics.get(), &render, &debug));
	EXPECT_EQ(context, CameraState::ControlMode::Walk, camera.controlMode);
	EXPECT_EQ(context, CameraState::ControlMode::Walk, debug.stats.controlMode);
	EXPECT_NEAR(context, 6.0f, camera.position[1]);

	PressInputAction(input, InputAction::ToggleControlMode);
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

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE, SDL_MS_TO_NS(100));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE, SDL_MS_TO_NS(160));
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE, SDL_MS_TO_NS(260));
	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, physics.get(), &render, &debug));
	EXPECT_EQ(context, CameraState::ControlMode::Walk, camera.controlMode);
	EXPECT_EQ(context, CameraState::ControlMode::Walk, debug.stats.controlMode);
	EXPECT_NEAR(context, 6.0f, camera.position[1]);

	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE, SDL_MS_TO_NS(320));
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE, SDL_MS_TO_NS(500));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE, SDL_MS_TO_NS(560));
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE, SDL_MS_TO_NS(680));
	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, physics.get(), &render, &debug));
	EXPECT_EQ(context, CameraState::ControlMode::Creative, camera.controlMode);
	EXPECT_EQ(context, CameraState::ControlMode::Creative, debug.stats.controlMode);
	EXPECT_NEAR(context, 6.0f, camera.position[1]);
}

void TestWalkCharacterCollidesWithVoxelWall(TestContext &context)
{
	VoxelWorld world = MakeWalkTestWorld();
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Glass, nullptr);
	SetVoxelMaterial(world, {2, 2, 2}, VoxelMaterial::Glass, nullptr);

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

void TestGetPhysicsWalkDebugInfoReportsGroundedSupport(TestContext &context)
{
	const VoxelWorld world = MakeWalkTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({2.5f, 2.65f, 4.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
	EXPECT_TRUE(context, info.valid);
	EXPECT_EQ(context, PhysicsWalkSupportDebugState::Grounded, info.supportState);
	EXPECT_NEAR(context, 2.5f, info.feetPosition[0]);
	EXPECT_NEAR(context, 1.05f, info.feetPosition[1]);
	EXPECT_NEAR(context, 4.5f, info.feetPosition[2]);
	EXPECT_TRUE(context, info.footSupportScore >= 0.7f);
	EXPECT_TRUE(context, info.footSupportTotalSamples > 0u);
	EXPECT_TRUE(context, info.edgeGraceFramesRemaining > 0u);
}

void TestVoxelLabWalkDebugInfoMatchesLiveCenterReference(TestContext &context)
{
	AppState state{};
	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::VoxelLab));
	EXPECT_TRUE(context, state.world().voxelWorld != nullptr);

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), state.world().voxelWorld.get()));

	CameraState camera = MakeTestCamera({-8.5f, 2.65f, 8.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), state.world().voxelWorld.get(), &camera));

	const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
	EXPECT_TRUE(context, info.valid);
	EXPECT_EQ(context, PhysicsWalkSupportDebugState::Grounded, info.supportState);
	EXPECT_NEAR(context, -8.5f, info.feetPosition[0]);
	EXPECT_NEAR(context, 1.05f, info.feetPosition[1]);
	EXPECT_NEAR(context, 8.5f, info.feetPosition[2]);
	EXPECT_EQ(context, 12u, info.footSupportHitSamples);
	EXPECT_EQ(context, 12u, info.footSupportTotalSamples);
	EXPECT_TRUE(context, info.edgeGraceFramesRemaining > 0u);
}

void TestVoxelLabWalkJumpFromSideEdgeDoesNotImmediatelyDrop(TestContext &context)
{
	AppState state{};
	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::VoxelLab));
	EXPECT_TRUE(context, state.world().voxelWorld != nullptr);

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), state.world().voxelWorld.get()));

	CameraState camera = MakeTestCamera({-8.5f, 2.65f, 8.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = 3.1415926535f;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), state.world().voxelWorld.get(), &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 12; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	}
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_W);
	EXPECT_TRUE(context, camera.position[2] > 9.25f);
	EXPECT_TRUE(context, camera.position[2] < 9.55f);

	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	const PhysicsWalkDebugInfo beforeJump = GetPhysicsWalkDebugInfo(physics.get());
	const float jumpStartY = camera.position[1];
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	const float afterJumpTickY = camera.position[1];
	float minY = afterJumpTickY;
	for (int step = 0; step < 12; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
		minY = std::min(minY, camera.position[1]);
	}

	if (!(afterJumpTickY > jumpStartY + 0.03f && minY > jumpStartY - 0.02f)) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"VoxelLab side-edge jump regressed (before state=%u score=%.3f hits=%u/%u tgr=%u tck=%u, afterY=%.3f, minY=%.3f)",
			static_cast<unsigned>(beforeJump.supportState),
			beforeJump.footSupportScore,
			beforeJump.footSupportHitSamples,
			beforeJump.footSupportTotalSamples,
			beforeJump.groundTakeoffGraceFramesRemaining,
			beforeJump.groundTakeoffCached ? 1u : 0u,
			afterJumpTickY,
			minY);
		context.Fail(__LINE__, buffer);
	}
}

void TestVoxelLabWalkJumpFromCornerEdgeDoesNotImmediatelyDrop(TestContext &context)
{
	AppState state{};
	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::VoxelLab));
	EXPECT_TRUE(context, state.world().voxelWorld != nullptr);

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), state.world().voxelWorld.get()));

	CameraState camera = MakeTestCamera({-8.5f, 2.65f, 8.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = -2.35619449f;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), state.world().voxelWorld.get(), &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 17; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	}
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_W);
	EXPECT_TRUE(context, camera.position[0] < -9.2f);
	EXPECT_TRUE(context, camera.position[2] > 9.2f);

	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	const PhysicsWalkDebugInfo beforeJump = GetPhysicsWalkDebugInfo(physics.get());
	const float jumpStartY = camera.position[1];
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	const float afterJumpTickY = camera.position[1];
	float minY = afterJumpTickY;
	for (int step = 0; step < 12; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
		minY = std::min(minY, camera.position[1]);
	}

	if (!(afterJumpTickY > jumpStartY + 0.03f && minY > jumpStartY - 0.02f)) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"VoxelLab corner-edge jump regressed (before state=%u score=%.3f hits=%u/%u tgr=%u tck=%u, afterY=%.3f, minY=%.3f)",
			static_cast<unsigned>(beforeJump.supportState),
			beforeJump.footSupportScore,
			beforeJump.footSupportHitSamples,
			beforeJump.footSupportTotalSamples,
			beforeJump.groundTakeoffGraceFramesRemaining,
			beforeJump.groundTakeoffCached ? 1u : 0u,
			afterJumpTickY,
			minY);
		context.Fail(__LINE__, buffer);
	}
}

bool RunVoxelLabReapproachCase(
	TestContext &context,
	const int outwardFrames,
	const float initialYawRadians,
	const float postYawRadians,
	const int walkFrames,
	const char *caseName)
{
	AppState state{};
	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::VoxelLab));
	EXPECT_TRUE(context, state.world().voxelWorld != nullptr);

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), state.world().voxelWorld.get()));

	CameraState camera = MakeTestCamera({-8.5f, 2.65f, 8.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = initialYawRadians;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), state.world().voxelWorld.get(), &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < walkFrames; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	}

	const float jumpStartY = camera.position[1];
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	for (int step = 0; step < outwardFrames; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	}

	camera.yawRadians = postYawRadians;
	bool failed = false;
	const int kMaxSteps = SimulationFrameCount(24);
	for (int step = 0; step < kMaxSteps && !failed; ++step) {
		const float previousPositionX = camera.position[0];
		const float previousPositionZ = camera.position[2];
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
		const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
		const float horizontalDeltaX = camera.position[0] - previousPositionX;
		const float horizontalDeltaZ = camera.position[2] - previousPositionZ;
		const float horizontalDeltaSq = horizontalDeltaX * horizontalDeltaX + horizontalDeltaZ * horizontalDeltaZ;
		if ((camera.position[1] > jumpStartY + 0.3f &&
			 info.supportState == PhysicsWalkSupportDebugState::Grounded &&
			 info.feetPosition[1] < 1.15f) ||
			horizontalDeltaSq > 0.020f) {
			char buffer[256]{};
			std::snprintf(
				buffer,
				sizeof(buffer),
				"VoxelLab %s reapproach magnet snap (outwardFrames=%d step=%d cam=(%.3f, %.3f, %.3f) feet=(%.3f, %.3f, %.3f) d2=%.4f state=%u score=%.3f)",
				caseName,
				outwardFrames,
				step,
				camera.position[0],
				camera.position[1],
				camera.position[2],
				info.feetPosition[0],
				info.feetPosition[1],
				info.feetPosition[2],
				horizontalDeltaSq,
				static_cast<unsigned>(info.supportState),
				info.footSupportScore);
			context.Fail(__LINE__, buffer);
			failed = true;
		}
	}

	return !failed;
}

void TestVoxelLabWalkJumpReapproachDoesNotMagnetSnapBackToSameTopPlane(TestContext &context)
{
	constexpr float kPi = 3.1415926535f;
	for (int outwardFrames = 0; outwardFrames <= 8; ++outwardFrames) {
		if (!RunVoxelLabReapproachCase(context, outwardFrames, kPi, 0.0f, 12, "side")) {
			return;
		}
		if (!RunVoxelLabReapproachCase(context, outwardFrames, -2.35619449f, 0.78539816f, 17, "corner")) {
			return;
		}
	}
}

void TestVoxelLabWalkFreeFallDoesNotSnapToTopPlaneTooEarly(TestContext &context)
{
	constexpr std::array startXZ{
		std::array{-8.5f, 8.5f},
		std::array{-8.5f, 9.39f},
		std::array{-9.39f, 9.39f},
	};
	constexpr std::array startCameraY{3.2f, 3.3f, 3.4f, 3.8f};

	for (const std::array xz : startXZ) {
		for (const float cameraY : startCameraY) {
			AppState state{};
			EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::VoxelLab));
			EXPECT_TRUE(context, state.world().voxelWorld != nullptr);

			const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
			EXPECT_TRUE(context, physics != nullptr);
			EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), state.world().voxelWorld.get()));

			CameraState camera = MakeTestCamera({xz[0], cameraY, xz[1]});
			camera.controlMode = CameraState::ControlMode::Walk;
			EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), state.world().voxelWorld.get(), &camera));

			InputState input{};
			InitializeInputState(input);
			float previousCameraY = camera.position[1];
			for (int step = 0; step < 20; ++step) {
				EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
				const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
				const float cameraDrop = previousCameraY - camera.position[1];
				if (previousCameraY > 3.0f && cameraDrop > 0.18f) {
					char buffer[256]{};
					std::snprintf(
						buffer,
						sizeof(buffer),
						"VoxelLab freefall teleported downward at xz=(%.2f, %.2f) startY=%.2f step=%d prevCamY=%.3f cam=(%.3f, %.3f, %.3f) feetY=%.3f state=%u score=%.3f",
						xz[0],
						xz[1],
						cameraY,
						step,
						previousCameraY,
						camera.position[0],
						camera.position[1],
						camera.position[2],
						info.feetPosition[1],
						static_cast<unsigned>(info.supportState),
						info.footSupportScore);
					context.Fail(__LINE__, buffer);
					return;
				}
				if (camera.position[1] > 2.85f &&
					info.supportState == PhysicsWalkSupportDebugState::Grounded &&
					info.feetPosition[1] < 1.15f) {
					char buffer[256]{};
					std::snprintf(
						buffer,
						sizeof(buffer),
						"VoxelLab freefall snapped too early at xz=(%.2f, %.2f) startY=%.2f step=%d cam=(%.3f, %.3f, %.3f) feetY=%.3f score=%.3f",
						xz[0],
						xz[1],
						cameraY,
						step,
						camera.position[0],
						camera.position[1],
						camera.position[2],
						info.feetPosition[1],
						info.footSupportScore);
					context.Fail(__LINE__, buffer);
					return;
				}
				previousCameraY = camera.position[1];
			}
		}
	}
}

void TestVoxelLabWalkJumpInPlaceDoesNotMagnetSnapToCenterTopPlane(TestContext &context)
{
	AppState state{};
	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::VoxelLab));
	EXPECT_TRUE(context, state.world().voxelWorld != nullptr);

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), state.world().voxelWorld.get()));

	CameraState camera = MakeTestCamera({-8.5f, 2.65f, 8.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = 3.1415926535f;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), state.world().voxelWorld.get(), &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	float previousCameraY = camera.position[1];
	float maxFrameDrop = 0.0f;
	float maxDropPreviousCameraY = camera.position[1];
	float maxDropCurrentCameraY = camera.position[1];
	int maxDropStep = -1;
	PhysicsWalkDebugInfo maxDropInfo{};
	for (int step = 0; step < 60; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
		const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
		const float frameDrop = previousCameraY - camera.position[1];
		if (frameDrop > maxFrameDrop) {
			maxFrameDrop = frameDrop;
			maxDropPreviousCameraY = previousCameraY;
			maxDropCurrentCameraY = camera.position[1];
			maxDropStep = step;
			maxDropInfo = info;
		}
		previousCameraY = camera.position[1];
	}

	const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
	if (!(maxFrameDrop < 0.18f &&
		  camera.position[1] > 2.60f &&
		  camera.position[1] < 2.70f &&
		  info.feetPosition[1] > 1.0f &&
		  info.supportState != PhysicsWalkSupportDebugState::Air)) {
		char buffer[320]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"VoxelLab center jump magnet snap regressed (step=%d prevCamY=%.3f cam=(%.3f, %.3f, %.3f) feet=(%.3f, %.3f, %.3f) state=%u score=%.3f maxDrop=%.3f grc=%u tgr=%u tck=%u)",
			maxDropStep,
			maxDropPreviousCameraY,
			camera.position[0],
			maxDropCurrentCameraY,
			camera.position[2],
			maxDropInfo.feetPosition[0],
			maxDropInfo.feetPosition[1],
			maxDropInfo.feetPosition[2],
			static_cast<unsigned>(maxDropInfo.supportState),
			maxDropInfo.footSupportScore,
			maxFrameDrop,
			maxDropInfo.edgeGraceFramesRemaining,
			maxDropInfo.groundTakeoffGraceFramesRemaining,
			maxDropInfo.groundTakeoffCached ? 1u : 0u);
		context.Fail(__LINE__, buffer);
	}
}

void TestVoxelLabWalkJumpFromSideEdgeLandsBackOnTopPlane(TestContext &context)
{
	AppState state{};
	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::VoxelLab));
	EXPECT_TRUE(context, state.world().voxelWorld != nullptr);

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), state.world().voxelWorld.get()));

	CameraState camera = MakeTestCamera({-8.5f, 2.65f, 8.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = 3.1415926535f;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), state.world().voxelWorld.get(), &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 12; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	}
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_W);

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	float minCameraY = camera.position[1];
	float maxCameraY = camera.position[1];
	for (int step = 0; step < 75; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
		minCameraY = std::min(minCameraY, camera.position[1]);
		maxCameraY = std::max(maxCameraY, camera.position[1]);
	}

	float postLandingMinCameraY = camera.position[1];
	for (int step = 0; step < 180; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
		postLandingMinCameraY = std::min(postLandingMinCameraY, camera.position[1]);
	}

	const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
	if (!(camera.position[1] > 2.55f &&
		  camera.position[1] < 2.75f &&
		  camera.position[2] > 9.2f &&
		  camera.position[2] < 9.5f &&
		  postLandingMinCameraY > 2.40f &&
		  info.supportState != PhysicsWalkSupportDebugState::Air &&
		  info.feetPosition[1] > 0.95f)) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"VoxelLab side-edge landing regressed (cam=(%.3f, %.3f, %.3f) feet=(%.3f, %.3f, %.3f) state=%u score=%.3f minY=%.3f maxY=%.3f postLandingMinY=%.3f)",
			camera.position[0],
			camera.position[1],
			camera.position[2],
			info.feetPosition[0],
			info.feetPosition[1],
			info.feetPosition[2],
			static_cast<unsigned>(info.supportState),
			info.footSupportScore,
			minCameraY,
			maxCameraY,
			postLandingMinCameraY);
		context.Fail(__LINE__, buffer);
	}
}

void TestVoxelLabWalkJumpFromCornerEdgeLandsBackOnTopPlane(TestContext &context)
{
	AppState state{};
	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::VoxelLab));
	EXPECT_TRUE(context, state.world().voxelWorld != nullptr);

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), state.world().voxelWorld.get()));

	CameraState camera = MakeTestCamera({-8.5f, 2.65f, 8.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = -2.35619449f;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), state.world().voxelWorld.get(), &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 17; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	}
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_W);

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	float minCameraY = camera.position[1];
	float maxCameraY = camera.position[1];
	for (int step = 0; step < 75; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
		minCameraY = std::min(minCameraY, camera.position[1]);
		maxCameraY = std::max(maxCameraY, camera.position[1]);
	}

	float postLandingMinCameraY = camera.position[1];
	for (int step = 0; step < 180; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
		postLandingMinCameraY = std::min(postLandingMinCameraY, camera.position[1]);
	}

	const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
	if (!(camera.position[1] > 2.55f &&
		  camera.position[1] < 2.75f &&
		  camera.position[0] < -9.2f &&
		  camera.position[2] > 9.2f &&
		  postLandingMinCameraY > 2.40f &&
		  info.supportState != PhysicsWalkSupportDebugState::Air &&
		  info.feetPosition[1] > 0.95f)) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"VoxelLab corner-edge landing regressed (cam=(%.3f, %.3f, %.3f) feet=(%.3f, %.3f, %.3f) state=%u score=%.3f minY=%.3f maxY=%.3f postLandingMinY=%.3f)",
			camera.position[0],
			camera.position[1],
			camera.position[2],
			info.feetPosition[0],
			info.feetPosition[1],
			info.feetPosition[2],
			static_cast<unsigned>(info.supportState),
			info.footSupportScore,
			minCameraY,
			maxCameraY,
			postLandingMinCameraY);
		context.Fail(__LINE__, buffer);
	}
}

void TestVoxelLabWalkSneakJumpFromSideEdgeLandsBackOnTopPlane(TestContext &context)
{
	AppState state{};
	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::VoxelLab));
	EXPECT_TRUE(context, state.world().voxelWorld != nullptr);

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), state.world().voxelWorld.get()));

	CameraState camera = MakeTestCamera({-8.5f, 2.65f, 8.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = 3.1415926535f;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), state.world().voxelWorld.get(), &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 26; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	}
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_W);
	EXPECT_TRUE(context, camera.position[2] > 9.25f);
	EXPECT_TRUE(context, camera.position[2] < 9.5f);

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	float minCameraY = camera.position[1];
	float maxCameraY = camera.position[1];
	for (int step = 0; step < 180; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
		minCameraY = std::min(minCameraY, camera.position[1]);
		maxCameraY = std::max(maxCameraY, camera.position[1]);
	}

	const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
	if (!(camera.position[1] > 2.40f &&
		  camera.position[1] < 2.60f &&
		  camera.position[2] > 9.2f &&
		  camera.position[2] < 9.5f &&
		  info.sneakActive &&
		  info.feetPosition[1] > 0.95f)) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"VoxelLab sneak side-edge landing regressed (cam=(%.3f, %.3f, %.3f) feet=(%.3f, %.3f, %.3f) state=%u score=%.3f minY=%.3f maxY=%.3f)",
			camera.position[0],
			camera.position[1],
			camera.position[2],
			info.feetPosition[0],
			info.feetPosition[1],
			info.feetPosition[2],
			static_cast<unsigned>(info.supportState),
			info.footSupportScore,
			minCameraY,
			maxCameraY);
		context.Fail(__LINE__, buffer);
	}
}

void TestVoxelLabWalkJumpFromSideEdgeCanMoveAfterLanding(TestContext &context)
{
	AppState state{};
	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::VoxelLab));
	EXPECT_TRUE(context, state.world().voxelWorld != nullptr);

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), state.world().voxelWorld.get()));

	CameraState camera = MakeTestCamera({-8.5f, 2.65f, 8.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = 3.1415926535f;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), state.world().voxelWorld.get(), &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 12; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	}
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_W);

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);
	for (int step = 0; step < 75; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	}

	const float landedZ = camera.position[2];
	const float landedY = camera.position[1];
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_S);
	for (int step = 0; step < 12; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	}
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_S);

	const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
	if (!(camera.position[2] < landedZ - 0.12f &&
		  camera.position[1] > 2.45f &&
		  info.feetPosition[1] > 0.95f)) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"VoxelLab side-edge landing froze movement (landed=(%.3f, %.3f) final cam=(%.3f, %.3f, %.3f) feet=(%.3f, %.3f, %.3f) state=%u score=%.3f)",
			landedY,
			landedZ,
			camera.position[0],
			camera.position[1],
			camera.position[2],
			info.feetPosition[0],
			info.feetPosition[1],
			info.feetPosition[2],
			static_cast<unsigned>(info.supportState),
			info.footSupportScore);
		context.Fail(__LINE__, buffer);
	}
}

void TestVoxelLabWalkJumpFromExactSideEdgeWithHeldWDoesNotLoseSupportOnLanding(TestContext &context)
{
	AppState state{};
	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::VoxelLab));
	EXPECT_TRUE(context, state.world().voxelWorld != nullptr);

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), state.world().voxelWorld.get()));

	CameraState camera = MakeTestCamera({-8.5f, 2.65f, 9.39f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = 0.0f;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), state.world().voxelWorld.get(), &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 6; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	}

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	int touchedSupportPlaneFrames = 0;
	int consecutiveAirOnSupportPlane = 0;
	int maxConsecutiveAirOnSupportPlane = 0;
	float minCameraY = camera.position[1];
	for (int step = 0; step < 180; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
		minCameraY = std::min(minCameraY, camera.position[1]);
		const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
		EXPECT_TRUE(context, info.valid);
		const bool onSupportPlane =
			camera.position[1] >= 2.40f &&
			camera.position[1] <= 2.70f &&
			info.feetPosition[1] > 0.95f &&
			info.feetPosition[1] < 1.10f;
		if (onSupportPlane) {
			++touchedSupportPlaneFrames;
			if (info.supportState == PhysicsWalkSupportDebugState::Air) {
				++consecutiveAirOnSupportPlane;
				maxConsecutiveAirOnSupportPlane = std::max(maxConsecutiveAirOnSupportPlane, consecutiveAirOnSupportPlane);
			} else {
				consecutiveAirOnSupportPlane = 0;
			}
		} else {
			consecutiveAirOnSupportPlane = 0;
		}
	}

	const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
	if (!(touchedSupportPlaneFrames > 0 &&
		  maxConsecutiveAirOnSupportPlane <= 1 &&
		  camera.position[1] > 2.40f &&
		  camera.position[2] < 9.25f &&
		  info.supportState != PhysicsWalkSupportDebugState::Air &&
		  info.feetPosition[1] > 0.95f)) {
		char buffer[320]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"VoxelLab exact side-edge W+jump landing regressed (cam=(%.3f, %.3f, %.3f) feet=(%.3f, %.3f, %.3f) state=%u score=%.3f minY=%.3f planeFrames=%d airPlane=%d)",
			camera.position[0],
			camera.position[1],
			camera.position[2],
			info.feetPosition[0],
			info.feetPosition[1],
			info.feetPosition[2],
			static_cast<unsigned>(info.supportState),
			info.footSupportScore,
			minCameraY,
			touchedSupportPlaneFrames,
			maxConsecutiveAirOnSupportPlane);
		context.Fail(__LINE__, buffer);
	}
}

void TestVoxelLabWalkSneakJumpFromSideEdgeStillHoldsEdgeAfterLanding(TestContext &context)
{
	AppState state{};
	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::VoxelLab));
	EXPECT_TRUE(context, state.world().voxelWorld != nullptr);

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), state.world().voxelWorld.get()));

	CameraState camera = MakeTestCamera({-8.5f, 2.65f, 8.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = 3.1415926535f;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), state.world().voxelWorld.get(), &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 26; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	}
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_W);

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);
	for (int step = 0; step < 120; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	}

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 18; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	}
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_W);

	const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
	if (!(camera.position[1] > 2.40f &&
		  camera.position[1] < 2.60f &&
		  camera.position[2] < 9.55f &&
		  info.feetPosition[1] > 0.95f &&
		  info.supportState != PhysicsWalkSupportDebugState::Air &&
		  info.sneakActive)) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"VoxelLab sneak edge hold regressed after landing (cam=(%.3f, %.3f, %.3f) feet=(%.3f, %.3f, %.3f) state=%u score=%.3f jl=%u csk=%u csi=%u cref=%.3f sgr=%u lgr=%u)",
			camera.position[0],
			camera.position[1],
			camera.position[2],
			info.feetPosition[0],
			info.feetPosition[1],
			info.feetPosition[2],
			static_cast<unsigned>(info.supportState),
			info.footSupportScore,
			info.jumpLockActive ? 1u : 0u,
			info.cachedSneakSupportValid ? 1u : 0u,
			info.feetInsideCachedSneakSupport ? 1u : 0u,
			info.cachedSneakSupportReferenceFeetY,
			info.sneakSupportGraceFramesRemaining,
			info.ledgeReleaseGraceFramesRemaining);
		context.Fail(__LINE__, buffer);
	}
}

void TestVoxelLabWalkSneakJumpInPlaceFromExactSideEdgeStaysGrounded(TestContext &context)
{
	AppState state{};
	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::VoxelLab));
	EXPECT_TRUE(context, state.world().voxelWorld != nullptr);

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), state.world().voxelWorld.get()));

	CameraState camera = MakeTestCamera({-8.5f, 2.65f, 9.39f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = 3.1415926535f;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), state.world().voxelWorld.get(), &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	float minCameraY = camera.position[1];
	for (int step = 0; step < 240; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
		minCameraY = std::min(minCameraY, camera.position[1]);
	}

	const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
	if (!(camera.position[1] > 2.40f &&
		  camera.position[1] < 2.60f &&
		  minCameraY > 2.35f &&
		  std::abs(camera.position[0] + 8.5f) < 0.08f &&
		  std::abs(camera.position[2] - 9.39f) < 0.12f &&
		  info.feetPosition[1] > 0.95f &&
		  info.supportState != PhysicsWalkSupportDebugState::Air &&
		  info.sneakActive)) {
		char buffer[320]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"VoxelLab exact side-edge sneak idle regressed (cam=(%.3f, %.3f, %.3f) feet=(%.3f, %.3f, %.3f) state=%u score=%.3f minY=%.3f jl=%u csk=%u csi=%u)",
			camera.position[0],
			camera.position[1],
			camera.position[2],
			info.feetPosition[0],
			info.feetPosition[1],
			info.feetPosition[2],
			static_cast<unsigned>(info.supportState),
			info.footSupportScore,
			minCameraY,
			info.jumpLockActive ? 1u : 0u,
			info.cachedSneakSupportValid ? 1u : 0u,
			info.feetInsideCachedSneakSupport ? 1u : 0u);
		context.Fail(__LINE__, buffer);
	}
}

void TestVoxelLabWalkSneakJumpInPlaceFromExactCornerEdgeStaysGrounded(TestContext &context)
{
	AppState state{};
	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::VoxelLab));
	EXPECT_TRUE(context, state.world().voxelWorld != nullptr);

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), state.world().voxelWorld.get()));

	CameraState camera = MakeTestCamera({-9.39f, 2.65f, 9.39f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = 3.1415926535f;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), state.world().voxelWorld.get(), &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	float minCameraY = camera.position[1];
	for (int step = 0; step < 240; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
		minCameraY = std::min(minCameraY, camera.position[1]);
	}

	const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
	if (!(camera.position[1] > 2.40f &&
		  camera.position[1] < 2.60f &&
		  minCameraY > 2.35f &&
		  std::abs(camera.position[0] + 9.39f) < 0.12f &&
		  std::abs(camera.position[2] - 9.39f) < 0.12f &&
		  info.feetPosition[1] > 0.95f &&
		  info.supportState != PhysicsWalkSupportDebugState::Air &&
		  info.sneakActive)) {
		char buffer[320]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"VoxelLab exact corner-edge sneak idle regressed (cam=(%.3f, %.3f, %.3f) feet=(%.3f, %.3f, %.3f) state=%u score=%.3f minY=%.3f jl=%u csk=%u csi=%u)",
			camera.position[0],
			camera.position[1],
			camera.position[2],
			info.feetPosition[0],
			info.feetPosition[1],
			info.feetPosition[2],
			static_cast<unsigned>(info.supportState),
			info.footSupportScore,
			minCameraY,
			info.jumpLockActive ? 1u : 0u,
			info.cachedSneakSupportValid ? 1u : 0u,
			info.feetInsideCachedSneakSupport ? 1u : 0u);
		context.Fail(__LINE__, buffer);
	}
}

void RunUpdateAppVoxelLabSneakJumpInPlaceFromExactEdgeAtHighRenderRate(
	TestContext &context,
	const std::array<float, 3> &cameraStart,
	const std::string_view label)
{
	AppState state{};
	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::VoxelLab));
	EXPECT_TRUE(context, state.world().voxelWorld != nullptr);

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), state.world().voxelWorld.get()));

	PlatformState platform{};
	SimulationState simulation{};
	CameraState camera = MakeTestCamera(cameraStart);
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = 3.1415926535f;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), state.world().voxelWorld.get(), &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);

	InteractionState interaction{};
	RenderState render{};
	DebugState debug{};
	float minCameraY = camera.position[1];
	constexpr int kRenderFrames = 1200;
	for (int frame = 0; frame < kRenderFrames; ++frame) {
		if (frame == 20) {
			SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);
		}
		EXPECT_TRUE(
			context,
			AdvanceUpdateAppWithSimulatedFrameDelta(
				&platform,
				&simulation,
				&camera,
				&input,
				&interaction,
				&state.world(),
				physics.get(),
				&render,
				&debug,
				1.0f / 1000.0f));
		minCameraY = std::min(minCameraY, camera.position[1]);
	}

	const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
	if (!(simulation.simulationTick >= 60 &&
		  simulation.simulationTick <= 90 &&
		  camera.position[1] > 2.40f &&
		  camera.position[1] < 2.60f &&
		  minCameraY > 2.35f &&
		  info.feetPosition[1] > 0.95f &&
		  info.supportState != PhysicsWalkSupportDebugState::Air &&
		  info.sneakActive)) {
		char buffer[384]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"%.*s high-render-rate UpdateApp regressed (tick=%llu cam=(%.3f, %.3f, %.3f) feet=(%.3f, %.3f, %.3f) state=%u score=%.3f minY=%.3f jl=%u csk=%u csi=%u egr=%u sgr=%u lgr=%u)",
			static_cast<int>(label.size()),
			label.data(),
			static_cast<unsigned long long>(simulation.simulationTick),
			camera.position[0],
			camera.position[1],
			camera.position[2],
			info.feetPosition[0],
			info.feetPosition[1],
			info.feetPosition[2],
			static_cast<unsigned>(info.supportState),
			info.footSupportScore,
			minCameraY,
			info.jumpLockActive ? 1u : 0u,
			info.cachedSneakSupportValid ? 1u : 0u,
			info.feetInsideCachedSneakSupport ? 1u : 0u,
			info.edgeGraceFramesRemaining,
			info.sneakSupportGraceFramesRemaining,
			info.ledgeReleaseGraceFramesRemaining);
		context.Fail(__LINE__, buffer);
	}
}

void TestUpdateAppVoxelLabSneakJumpInPlaceFromExactSideEdgeStaysGroundedAtHighRenderRate(TestContext &context)
{
	RunUpdateAppVoxelLabSneakJumpInPlaceFromExactEdgeAtHighRenderRate(
		context,
		{-8.5f, 2.65f, 9.39f},
		"VoxelLab exact side-edge sneak idle");
}

void TestUpdateAppVoxelLabSneakJumpInPlaceFromExactCornerEdgeStaysGroundedAtHighRenderRate(TestContext &context)
{
	RunUpdateAppVoxelLabSneakJumpInPlaceFromExactEdgeAtHighRenderRate(
		context,
		{-9.39f, 2.65f, 9.39f},
		"VoxelLab exact corner-edge sneak idle");
}

void TestWalkCharacterClimbsOneBlockWithoutSlowWallSlide(TestContext &context)
{
	constexpr float kPi = 3.1415926535f;
	const VoxelWorld world = MakeWalkPositiveSingleBlockTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({5.5f, 2.65f, 4.45f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = kPi;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	int firstTopTouchFrame = -1;
	int wallSlideFrames = 0;
	int maxWallSlideFrames = 0;
	float previousZ = camera.position[2];
	for (int step = 0; step < 30; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		if (firstTopTouchFrame < 0 &&
			camera.position[1] > 3.3f &&
			camera.position[2] > 5.75f &&
			camera.position[2] < 7.1f &&
			camera.position[0] > 5.05f &&
			camera.position[0] < 5.95f) {
			firstTopTouchFrame = step;
		}

		const float zProgress = camera.position[2] - previousZ;
		if (firstTopTouchFrame < 0 &&
			camera.position[1] > 2.95f &&
			camera.position[1] < 3.75f &&
			zProgress < 0.015f) {
			++wallSlideFrames;
			maxWallSlideFrames = std::max(maxWallSlideFrames, wallSlideFrames);
		} else {
			wallSlideFrames = 0;
		}

		previousZ = camera.position[2];
	}

	if (!(firstTopTouchFrame >= 0 &&
		  firstTopTouchFrame <= 18 &&
		  maxWallSlideFrames <= 3)) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"one-block climb regressed into slow wall slide (touchFrame=%d maxWallSlide=%d final cam=(%.3f, %.3f, %.3f))",
			firstTopTouchFrame,
			maxWallSlideFrames,
			camera.position[0],
			camera.position[1],
			camera.position[2]);
		context.Fail(__LINE__, buffer);
	}
}

struct WallJumpTestResult {
	int landingFrame = -1;
	float finalY = 0.0f;
	float finalZ = 0.0f;
	float maxY = 0.0f;
	PhysicsWalkDebugInfo finalInfo{};
};

WallJumpTestResult RunSneakGlassColumnWallSlideCase(TestContext &context, const bool withWall)
{
	constexpr float kPi = 3.1415926535f;
	const int kSimulationFrames = SimulationFrameCount(60);
	VoxelWorld world = withWall ? MakeWalkGlassColumnWallTestWorld() : MakeWalkTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({4.5f, 2.65f, 3.45f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = kPi;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	const float startY = camera.position[1];
	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);

	WallJumpTestResult result{};
	result.maxY = camera.position[1];
	for (int step = 1; step <= kSimulationFrames; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
		result.maxY = std::max(result.maxY, camera.position[1]);
		if (result.landingFrame < 0 &&
			info.supportState != PhysicsWalkSupportDebugState::Air &&
			camera.position[1] <= startY + 0.05f) {
			result.landingFrame = step;
		}
		result.finalInfo = info;
	}

	result.finalY = camera.position[1];
	result.finalZ = camera.position[2];
	return result;
}

void TestWalkCharacterSneakJumpAgainstGlassColumnDoesNotSlowWallSlide(TestContext &context)
{
	const WallJumpTestResult controlResult = RunSneakGlassColumnWallSlideCase(context, false);
	const WallJumpTestResult wallResult = RunSneakGlassColumnWallSlideCase(context, true);
	if (!(controlResult.landingFrame >= 0 &&
		  wallResult.landingFrame >= 0 &&
		  wallResult.landingFrame <= controlResult.landingFrame + 3 &&
		  wallResult.finalY <= controlResult.finalY + 0.08f)) {
		char buffer[320]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"sneak glass-column wall slide regressed (control landing=%d final cam=(%.3f, %.3f) feet=(%.3f, %.3f, %.3f) state=%u; wall landing=%d final cam=(%.3f, %.3f) feet=(%.3f, %.3f, %.3f) state=%u score=%.3f sneak=%u)",
			controlResult.landingFrame,
			controlResult.finalY,
			controlResult.finalZ,
			controlResult.finalInfo.feetPosition[0],
			controlResult.finalInfo.feetPosition[1],
			controlResult.finalInfo.feetPosition[2],
			static_cast<unsigned>(controlResult.finalInfo.supportState),
			wallResult.landingFrame,
			wallResult.finalY,
			wallResult.finalZ,
			wallResult.finalInfo.feetPosition[0],
			wallResult.finalInfo.feetPosition[1],
			wallResult.finalInfo.feetPosition[2],
			static_cast<unsigned>(wallResult.finalInfo.supportState),
			wallResult.finalInfo.footSupportScore,
			wallResult.finalInfo.sneakActive ? 1u : 0u);
		context.Fail(__LINE__, buffer);
	}
}

void TestWalkCharacterJumpAgainstGlassColumnDoesNotWallStepUp(TestContext &context)
{
	constexpr float kPi = 3.1415926535f;
	VoxelWorld world = MakeWalkGlassColumnWallTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({4.5f, 2.65f, 3.45f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = kPi;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	const float startY = camera.position[1];
	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	float maxY = camera.position[1];
	for (int step = 0; step < 60; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		maxY = std::max(maxY, camera.position[1]);
	}

	const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
	if (!(camera.position[1] <= startY + 0.02f &&
		  info.feetPosition[1] <= 1.10f &&
		  maxY < 4.2f)) {
		char buffer[320]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"glass-column wall step-up regressed (final cam=(%.3f, %.3f, %.3f) feet=(%.3f, %.3f, %.3f) state=%u score=%.3f maxY=%.3f)",
			camera.position[0],
			camera.position[1],
			camera.position[2],
			info.feetPosition[0],
			info.feetPosition[1],
			info.feetPosition[2],
			static_cast<unsigned>(info.supportState),
			info.footSupportScore,
			maxY);
		context.Fail(__LINE__, buffer);
	}
}

struct WallCaseTestResult {
	float startX = 0.0f;
	float maxFeetY = 0.0f;
	float maxCameraY = 0.0f;
	bool wallTopSupportWindowFound = false;
	bool secondJumpPressed = false;
	PhysicsWalkDebugInfo finalInfo{};
	CameraState finalCamera{};
};

WallCaseTestResult RunHeldJumpGlassColumnCase(TestContext &context, const float startX)
{
	constexpr float kPi = 3.1415926535f;
	VoxelWorld world = MakeWalkGlassColumnWallTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({startX, 2.65f, 3.45f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = kPi;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	WallCaseTestResult result{};
	result.startX = startX;
	result.maxFeetY = GetPhysicsWalkDebugInfo(physics.get()).feetPosition[1];
	result.maxCameraY = camera.position[1];
	const int kSimulationFrames = SimulationFrameCount(120);
	for (int step = 0; step < kSimulationFrames; ++step) {
		const PhysicsWalkDebugInfo infoBeforeTick = GetPhysicsWalkDebugInfo(physics.get());
		const bool hasWallTopSupportWindow =
			infoBeforeTick.supportState != PhysicsWalkSupportDebugState::Air &&
			infoBeforeTick.feetPosition[1] >= 2.0f - 0.02f &&
			infoBeforeTick.feetPosition[1] <= 2.2f &&
			infoBeforeTick.feetPosition[0] >= 4.0f - 0.02f &&
			infoBeforeTick.feetPosition[0] <= 5.0f + 0.02f &&
			infoBeforeTick.feetPosition[2] >= 4.60f &&
			infoBeforeTick.feetPosition[2] <= 5.35f;
		if (hasWallTopSupportWindow) {
			result.wallTopSupportWindowFound = true;
		}
		if (hasWallTopSupportWindow && !result.secondJumpPressed) {
			SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
		}

		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));

		if (hasWallTopSupportWindow && !result.secondJumpPressed) {
			SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);
			result.secondJumpPressed = true;
		}
		const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
		result.maxFeetY = std::max(result.maxFeetY, info.feetPosition[1]);
		result.maxCameraY = std::max(result.maxCameraY, camera.position[1]);
		result.finalInfo = info;
		result.finalCamera = camera;
	}

	return result;
}

void TestWalkCharacterHeldJumpAgainstGlassColumnDoesNotGetSecondJumpFromWallTop(TestContext &context)
{
	constexpr std::array startXs{
		4.324f,
		4.352f,
		4.380f,
		4.500f,
		4.620f,
		4.648f,
		4.676f,
	};

	for (const float startX : startXs) {
		const auto [caseStartX, maxFeetY, maxCameraY, wallTopSupportWindowFound, secondJumpPressed, finalInfo, finalCamera] =
			RunHeldJumpGlassColumnCase(context, startX);
		if (!(wallTopSupportWindowFound == false &&
			  maxFeetY < 3.0f &&
			  maxCameraY < 4.6f &&
			  finalInfo.feetPosition[1] <= 1.10f)) {
			char buffer[384]{};
			std::snprintf(
				buffer,
				sizeof(buffer),
				"wall top/second jump regressed (startX=%.3f final cam=(%.3f, %.3f, %.3f) feet=(%.3f, %.3f, %.3f) state=%u score=%.3f maxFeetY=%.3f maxCameraY=%.3f wallTop=%u secondJump=%u)",
				caseStartX,
				finalCamera.position[0],
				finalCamera.position[1],
				finalCamera.position[2],
				finalInfo.feetPosition[0],
				finalInfo.feetPosition[1],
				finalInfo.feetPosition[2],
				static_cast<unsigned>(finalInfo.supportState),
				finalInfo.footSupportScore,
				maxFeetY,
				maxCameraY,
				wallTopSupportWindowFound ? 1u : 0u,
				secondJumpPressed ? 1u : 0u);
			context.Fail(__LINE__, buffer);
		}
	}
}

void TestWalkCharacterMidairSneakDoesNotAcquireTwoBlockWallTop(TestContext &context)
{
	constexpr float kPi = 3.1415926535f;
	VoxelWorld world = MakeWalkPositiveTwoBlockTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({5.5f, 2.65f, 4.45f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = kPi;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	for (int step = 0; step < 8; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	float maxFeetY = GetPhysicsWalkDebugInfo(physics.get()).feetPosition[1];
	bool touchedTwoBlockTop = false;
	bool touchedMidairWallSupport = false;
	for (int step = 0; step < 40; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
		maxFeetY = std::max(maxFeetY, info.feetPosition[1]);
		if (info.supportState != PhysicsWalkSupportDebugState::Air &&
			info.feetPosition[1] >= 2.0f - 0.02f) {
			touchedMidairWallSupport = true;
		}
		if (info.supportState != PhysicsWalkSupportDebugState::Air &&
			info.feetPosition[1] >= 3.0f - 0.01f) {
			touchedTwoBlockTop = true;
		}
	}

	if (!(maxFeetY < 3.0f && !touchedTwoBlockTop && !touchedMidairWallSupport)) {
		const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
		char buffer[320]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"midair sneak acquired wall support (maxFeetY=%.3f final cam=(%.3f, %.3f, %.3f) feet=(%.3f, %.3f, %.3f) state=%u score=%.3f sneak=%u csk=%u wallSupport=%u top=%u)",
			maxFeetY,
			camera.position[0],
			camera.position[1],
			camera.position[2],
			info.feetPosition[0],
			info.feetPosition[1],
			info.feetPosition[2],
			static_cast<unsigned>(info.supportState),
			info.footSupportScore,
			info.sneakActive ? 1u : 0u,
			info.cachedSneakSupportValid ? 1u : 0u,
			touchedMidairWallSupport ? 1u : 0u,
			touchedTwoBlockTop ? 1u : 0u);
		context.Fail(__LINE__, buffer);
	}
}

void TestWalkCharacterFallsAfterEditedSupportIsRemoved(TestContext &context)
{
	VoxelWorld world = MakeWalkPositiveTwoBlockTestWorld();
	SetVoxelMaterial(world, {8, 1, 8}, VoxelMaterial::Glass, nullptr);
	ResetDirtyFlags(world);

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({5.5f, 4.65f, 6.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	const PhysicsWalkDebugInfo beforeEdit = GetPhysicsWalkDebugInfo(physics.get());
	EXPECT_TRUE(context, beforeEdit.valid);
	EXPECT_TRUE(context, beforeEdit.feetPosition[1] > 3.0f);

	SetVoxelMaterial(world, {8, 1, 8}, VoxelMaterial::Air, nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));
	SetVoxelMaterial(world, {5, 2, 6}, VoxelMaterial::Air, nullptr);
	SetVoxelMaterial(world, {5, 1, 6}, VoxelMaterial::Air, nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	InputState input{};
	InitializeInputState(input);
	float minFeetY = beforeEdit.feetPosition[1];
	for (int step = 0; step < 15; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
		minFeetY = std::min(minFeetY, info.feetPosition[1]);
	}

	if (!(minFeetY < beforeEdit.feetPosition[1] - 0.2f)) {
		const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
		char buffer[320]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"edited support removal did not make walk fall (startFeetY=%.3f minFeetY=%.3f final cam=(%.3f, %.3f, %.3f) feet=(%.3f, %.3f, %.3f) state=%u score=%.3f)",
			beforeEdit.feetPosition[1],
			minFeetY,
			camera.position[0],
			camera.position[1],
			camera.position[2],
			info.feetPosition[0],
			info.feetPosition[1],
			info.feetPosition[2],
			static_cast<unsigned>(info.supportState),
			info.footSupportScore);
		context.Fail(__LINE__, buffer);
	}
}

struct JumpTestResult {
	float maxFeetY = 0.0f;
	float finalFeetY = 0.0f;
	float finalCameraY = 0.0f;
	int retryJumpPressCount = 0;
	PhysicsWalkDebugInfo finalInfo{};
};

JumpTestResult RunAirborneRetryJumpCase(TestContext &context, const bool pressSecondJump)
{
	constexpr float kPi = 3.1415926535f;
	VoxelWorld world = MakeWalkPositiveTwoBlockTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({5.5f, 2.65f, 4.45f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = kPi;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	JumpTestResult result{};
	result.maxFeetY = GetPhysicsWalkDebugInfo(physics.get()).feetPosition[1];
	int retryJumpPressCount = 0;
	const int kSimulationFrames = SimulationFrameCount(40);
	for (int step = 0; step < kSimulationFrames; ++step) {
		const PhysicsWalkDebugInfo infoBeforeTick = GetPhysicsWalkDebugInfo(physics.get());
		const bool canRetryAirborneJump =
			pressSecondJump &&
			infoBeforeTick.supportState == PhysicsWalkSupportDebugState::Air &&
			infoBeforeTick.groundTakeoffCached &&
			infoBeforeTick.groundTakeoffGraceFramesRemaining > 0 &&
			infoBeforeTick.feetPosition[1] > 1.15f;
		if (canRetryAirborneJump) {
			SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
		}
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		if (canRetryAirborneJump) {
			SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);
			++retryJumpPressCount;
		}
		const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
		result.maxFeetY = std::max(result.maxFeetY, info.feetPosition[1]);
		result.finalInfo = info;
	}

	result.retryJumpPressCount = retryJumpPressCount;
	result.finalFeetY = result.finalInfo.feetPosition[1];
	result.finalCameraY = camera.position[1];
	return result;
}

void TestWalkCharacterCannotDoubleJumpWhileAirborneNearWall(TestContext &context)
{
	const JumpTestResult controlResult = RunAirborneRetryJumpCase(context, false);
	const JumpTestResult retryResult = RunAirborneRetryJumpCase(context, true);
	if (!(retryResult.maxFeetY <= controlResult.maxFeetY + 0.05f &&
		  retryResult.finalFeetY <= controlResult.finalFeetY + 0.05f)) {
		char buffer[320]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"airborne retry jump regressed near wall (control maxFeetY=%.3f finalFeetY=%.3f state=%u; retry presses=%d maxFeetY=%.3f finalFeetY=%.3f state=%u score=%.3f)",
			controlResult.maxFeetY,
			controlResult.finalFeetY,
			static_cast<unsigned>(controlResult.finalInfo.supportState),
			retryResult.retryJumpPressCount,
			retryResult.maxFeetY,
			retryResult.finalFeetY,
			static_cast<unsigned>(retryResult.finalInfo.supportState),
			retryResult.finalInfo.footSupportScore);
		context.Fail(__LINE__, buffer);
	}
}

struct WallCatchTestResult {
	float startX = 0.0f;
	float maxFeetY = 0.0f;
	bool touchedFirstWallTop = false;
	bool touchedSecondWallTop = false;
	PhysicsWalkDebugInfo finalInfo{};
	CameraState finalCamera{};
};

WallCatchTestResult RunHeldJumpForeignWallTopCase(TestContext &context, const float startX)
{
	constexpr float kPi = 3.1415926535f;
	VoxelWorld world = MakeWalkPositiveTwoBlockTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({startX, 2.65f, 4.45f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = kPi;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);

	WallCatchTestResult result{};
	result.startX = startX;
	result.maxFeetY = GetPhysicsWalkDebugInfo(physics.get()).feetPosition[1];
	const int kSimulationFrames = SimulationFrameCount(120);
	for (int step = 0; step < kSimulationFrames; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
		result.maxFeetY = std::max(result.maxFeetY, info.feetPosition[1]);
		if (info.supportState != PhysicsWalkSupportDebugState::Air &&
			info.feetPosition[1] >= 2.0f - 0.02f &&
			info.feetPosition[1] < 3.0f - 0.02f) {
			result.touchedFirstWallTop = true;
		}
		if (info.supportState != PhysicsWalkSupportDebugState::Air &&
			info.feetPosition[1] >= 3.0f - 0.02f) {
			result.touchedSecondWallTop = true;
		}
		result.finalInfo = info;
		result.finalCamera = camera;
	}

	return result;
}

void TestWalkCharacterHeldJumpDoesNotAcquireForeignWallTopNearTwoBlockWall(TestContext &context)
{
	constexpr std::array startXs{
		5.324f,
		5.352f,
		5.380f,
		5.500f,
		5.620f,
		5.648f,
		5.676f,
	};

	for (const float startX : startXs) {
		const auto [caseStartX, maxFeetY, touchedFirstWallTop, touchedSecondWallTop, finalInfo, finalCamera] =
			RunHeldJumpForeignWallTopCase(context, startX);
		if (!(touchedFirstWallTop == false &&
			  touchedSecondWallTop == false)) {
			char buffer[384]{};
			std::snprintf(
				buffer,
				sizeof(buffer),
				"foreign wall-top catch regressed near two-block wall (startX=%.3f final cam=(%.3f, %.3f, %.3f) feet=(%.3f, %.3f, %.3f) state=%u score=%.3f maxFeetY=%.3f firstTop=%u secondTop=%u)",
				caseStartX,
				finalCamera.position[0],
				finalCamera.position[1],
				finalCamera.position[2],
				finalInfo.feetPosition[0],
				finalInfo.feetPosition[1],
				finalInfo.feetPosition[2],
				static_cast<unsigned>(finalInfo.supportState),
				finalInfo.footSupportScore,
				maxFeetY,
				touchedFirstWallTop ? 1u : 0u,
				touchedSecondWallTop ? 1u : 0u);
			context.Fail(__LINE__, buffer);
		}
	}
}

struct TraversalTestResult {
	float maxY = 0.0f;
	float finalY = 0.0f;
	float finalZ = 0.0f;
	bool touchedBlockTop = false;
};

TraversalTestResult RunAutoJumpToggleCase(TestContext &context, const bool autoJumpEnabled)
{
	constexpr float kPi = 3.1415926535f;
	constexpr int kSimulationFrames = 120;
	const VoxelWorld world = MakeWalkPositiveSingleBlockTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));
	SetPhysicsWalkAutoJumpEnabled(physics.get(), autoJumpEnabled);

	CameraState camera = MakeTestCamera({5.5f, 2.65f, 4.45f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = kPi;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);

	TraversalTestResult result{};
	result.maxY = camera.position[1];
	for (int step = 0; step < kSimulationFrames; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		result.maxY = std::max(result.maxY, camera.position[1]);
		if (camera.position[1] > 3.3f &&
			camera.position[2] > 5.75f &&
			camera.position[2] < 7.1f &&
			camera.position[0] > 5.05f &&
			camera.position[0] < 5.95f) {
			result.touchedBlockTop = true;
		}
	}

	result.finalY = camera.position[1];
	result.finalZ = camera.position[2];
	return result;
}

void TestWalkCharacterAutoJumpToggleControlsOneBlockTraversal(TestContext &context)
{
	const auto [maxY1, finalY1, finalZ1, touchedBlockTop1] = RunAutoJumpToggleCase(context, false);
	const auto [maxY, finalY, finalZ, touchedBlockTop] = RunAutoJumpToggleCase(context, true);
	if (!(maxY1 < 3.05f &&
		  !touchedBlockTop1 &&
		  finalZ1 < 6.0f &&
		  maxY > 3.8f &&
		  touchedBlockTop &&
		  finalZ > finalZ1 + 0.5f)) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"auto-jump toggle regressed (off maxY=%.3f final=(%.3f, %.3f) top=%u; on maxY=%.3f final=(%.3f, %.3f) top=%u)",
			maxY1,
			finalY1,
			finalZ1,
			touchedBlockTop1 ? 1u : 0u,
			maxY,
			finalY,
			finalZ,
			touchedBlockTop ? 1u : 0u);
		context.Fail(__LINE__, buffer);
	}
}

void TestWalkCharacterFallsSmoothlyAfterLeavingLedge(TestContext &context)
{
	const VoxelWorld world = MakeWalkLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));
	SetPhysicsWalkAirControlMode(physics.get(), WalkAirControlMode::Realistic);

	CameraState camera = MakeTestCamera({2.5f, 3.65f, 4.18f});
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 10; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	EXPECT_TRUE(context, camera.position[2] < 3.45f);
	EXPECT_TRUE(context, camera.position[2] > 3.0f);
	EXPECT_TRUE(context, camera.position[1] < 3.6f);
	EXPECT_TRUE(context, camera.position[1] > 3.0f);

	for (int step = 0; step < 20; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	EXPECT_TRUE(context, camera.position[1] > 2.45f);
	EXPECT_TRUE(context, camera.position[1] < 2.85f);
}

void TestWalkCharacterDoesNotPassivelySlideOffLedge(TestContext &context)
{
	const VoxelWorld world = MakeWalkLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));
	SetPhysicsWalkAirControlMode(physics.get(), WalkAirControlMode::Realistic);

	CameraState camera = MakeTestCamera({2.5f, 3.65f, 4.18f});
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	for (int step = 0; step < 30; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	EXPECT_TRUE(context, camera.position[2] > 4.08f);
	EXPECT_TRUE(context, camera.position[2] < 4.28f);
	EXPECT_TRUE(context, camera.position[1] > 3.55f);
	EXPECT_TRUE(context, camera.position[1] < 3.75f);
}

void TestWalkCharacterDoesNotSlideFromNarrowEdgeBand(TestContext &context)
{
	const VoxelWorld world = MakeWalkLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({2.5f, 3.65f, 4.05f});
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	for (int step = 0; step < 30; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	EXPECT_TRUE(context, camera.position[2] > 3.95f);
	EXPECT_TRUE(context, camera.position[2] < 4.15f);
	EXPECT_TRUE(context, camera.position[1] > 3.5f);
	EXPECT_TRUE(context, camera.position[1] < 3.75f);
}

void TestWalkCharacterCanJumpFromNarrowEdgeBandWithoutSneak(TestContext &context)
{
	const VoxelWorld world = MakeWalkLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({2.5f, 3.65f, 4.05f});
	camera.controlMode = CameraState::ControlMode::Walk;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	const float jumpStartY = camera.position[1];
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	const float afterJumpTickY = camera.position[1];
	float minY = afterJumpTickY;
	for (int step = 0; step < 12; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		minY = std::min(minY, camera.position[1]);
	}

	EXPECT_TRUE(context, afterJumpTickY > jumpStartY + 0.03f);
	EXPECT_TRUE(context, minY > jumpStartY - 0.02f);
	EXPECT_TRUE(context, camera.position[1] > jumpStartY + 0.12f);
}

void TestWalkCharacterCanJumpWhileMovingAcrossNarrowEdgeBandWithoutSneak(TestContext &context)
{
	VoxelWorld world = MakeWalkLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));
	SetPhysicsWalkAirControlMode(physics.get(), WalkAirControlMode::Realistic);

	CameraState camera = MakeTestCamera({2.5f, 3.65f, 4.18f});
	camera.controlMode = CameraState::ControlMode::Walk;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);

	PhysicsWalkDebugInfo preJumpInfo;
	bool foundMovingEdgeBand = false;
	for (int step = 0; step < 30; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		preJumpInfo = GetPhysicsWalkDebugInfo(physics.get());
		EXPECT_TRUE(context, preJumpInfo.valid);
		if (preJumpInfo.supportState == PhysicsWalkSupportDebugState::EdgeGrace &&
			preJumpInfo.footSupportScore >= 0.45f &&
			preJumpInfo.footSupportScore <= 0.55f) {
			foundMovingEdgeBand = true;
			break;
		}
	}

	if (!foundMovingEdgeBand) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"failed to reach moving narrow edge band before jump (cam=(%.3f, %.3f, %.3f) state=%u score=%.3f hits=%u/%u)",
			camera.position[0],
			camera.position[1],
			camera.position[2],
			static_cast<unsigned>(preJumpInfo.supportState),
			preJumpInfo.footSupportScore,
			preJumpInfo.footSupportHitSamples,
			preJumpInfo.footSupportTotalSamples);
		context.Fail(__LINE__, buffer);
		return;
	}

	const float jumpStartY = camera.position[1];
	const float jumpStartZ = camera.position[2];
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	const float afterJumpTickY = camera.position[1];
	float minY = afterJumpTickY;
	float maxY = afterJumpTickY;
	for (int step = 0; step < 12; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		minY = std::min(minY, camera.position[1]);
		maxY = std::max(maxY, camera.position[1]);
	}

	if (!(afterJumpTickY > jumpStartY + 0.03f &&
		  minY > jumpStartY - 0.02f &&
		  maxY > jumpStartY + 0.12f &&
		  camera.position[2] < jumpStartZ - 0.08f)) {
		const PhysicsWalkDebugInfo postJumpInfo = GetPhysicsWalkDebugInfo(physics.get());
		char buffer[320]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"moving narrow-edge jump regressed (startY=%.3f startZ=%.3f afterY=%.3f maxY=%.3f minY=%.3f final=(%.3f, %.3f, %.3f) preState=%u preScore=%.3f postState=%u postScore=%.3f)",
			jumpStartY,
			jumpStartZ,
			afterJumpTickY,
			maxY,
			minY,
			camera.position[0],
			camera.position[1],
			camera.position[2],
			static_cast<unsigned>(preJumpInfo.supportState),
			preJumpInfo.footSupportScore,
			static_cast<unsigned>(postJumpInfo.supportState),
			postJumpInfo.footSupportScore);
		context.Fail(__LINE__, buffer);
	}
}

void TestWalkCharacterCanJumpWhileBoostingAlongStraightEdgeWithoutSneak(TestContext &context)
{
	constexpr float kHalfPi = 1.5707963268f;
	const VoxelWorld world = MakeWalkLongLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));
	SetPhysicsWalkAirControlMode(physics.get(), WalkAirControlMode::Realistic);

	CameraState camera = MakeTestCamera({2.5f, 3.65f, 4.324f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = kHalfPi;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_LCTRL);

	float minY = camera.position[1];
	PhysicsWalkDebugInfo preJumpInfo;
	for (int step = 0; step < 120; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		minY = std::min(minY, camera.position[1]);
		preJumpInfo = GetPhysicsWalkDebugInfo(physics.get());
		EXPECT_TRUE(context, preJumpInfo.valid);
	}

	const float jumpStartY = camera.position[1];
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	const float afterJumpTickY = camera.position[1];
	if (!(minY > jumpStartY - 0.02f &&
		  preJumpInfo.supportState != PhysicsWalkSupportDebugState::Air &&
		  preJumpInfo.footSupportHitSamples > 0 &&
		  preJumpInfo.feetPosition[1] > 1.0f &&
		  afterJumpTickY > jumpStartY + 0.03f)) {
		const PhysicsWalkDebugInfo postJumpInfo = GetPhysicsWalkDebugInfo(physics.get());
		char buffer[384]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"boosted straight-edge jump regressed (minY=%.3f jumpStartY=%.3f afterJumpY=%.3f preFeet=(%.3f, %.3f, %.3f) preState=%u preScore=%.3f preHits=%u/%u postState=%u postScore=%.3f finalCam=(%.3f, %.3f, %.3f))",
			minY,
			jumpStartY,
			afterJumpTickY,
			preJumpInfo.feetPosition[0],
			preJumpInfo.feetPosition[1],
			preJumpInfo.feetPosition[2],
			static_cast<unsigned>(preJumpInfo.supportState),
			preJumpInfo.footSupportScore,
			preJumpInfo.footSupportHitSamples,
			preJumpInfo.footSupportTotalSamples,
			static_cast<unsigned>(postJumpInfo.supportState),
			postJumpInfo.footSupportScore,
			camera.position[0],
			camera.position[1],
			camera.position[2]);
		context.Fail(__LINE__, buffer);
	}
}

void TestWalkCharacterJumpHeightMatchesMinecraftLikeApex(TestContext &context)
{
	VoxelWorld world = MakeWalkTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({2.5f, 2.65f, 4.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	const PhysicsWalkDebugInfo startInfo = GetPhysicsWalkDebugInfo(physics.get());
	EXPECT_TRUE(context, startInfo.valid);
	const float startFeetY = startInfo.feetPosition[1];

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
	EXPECT_TRUE(context, info.valid);
	float maxFeetY = info.feetPosition[1];
	float minFeetY = info.feetPosition[1];
	for (int step = 0; step < 40; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		info = GetPhysicsWalkDebugInfo(physics.get());
		EXPECT_TRUE(context, info.valid);
		maxFeetY = std::max(maxFeetY, info.feetPosition[1]);
		minFeetY = std::min(minFeetY, info.feetPosition[1]);
	}

	const float apexGain = maxFeetY - startFeetY;
	if (!(apexGain > 1.20f && apexGain < 1.33f && minFeetY > startFeetY - 0.02f)) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"walk jump height regressed (startFeetY=%.3f maxFeetY=%.3f minFeetY=%.3f apexGain=%.3f finalCamY=%.3f)",
			startFeetY,
			maxFeetY,
			minFeetY,
			apexGain,
			camera.position[1]);
		context.Fail(__LINE__, buffer);
	}
}

void TestWalkCharacterHoldingJumpRepeatsOnLanding(TestContext &context)
{
	VoxelWorld world = MakeWalkTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({2.5f, 2.65f, 4.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);

	const float groundCamY = camera.position[1];
	bool airborne = false;
	int jumpCount = 0;
	int landingCount = 0;
	for (int step = 0; step < 180; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
		EXPECT_TRUE(context, info.valid);
		const bool groundedLike = info.supportState != PhysicsWalkSupportDebugState::Air;
		if (!airborne && camera.position[1] > groundCamY + 0.06f) {
			++jumpCount;
			airborne = true;
		}
		if (airborne && groundedLike && camera.position[1] <= groundCamY + 0.03f) {
			++landingCount;
			airborne = false;
		}
	}

	if (!(jumpCount >= 2 && landingCount >= 1)) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"held jump no longer repeats after landing (jumpCount=%d landingCount=%d finalCam=(%.3f, %.3f, %.3f))",
			jumpCount,
			landingCount,
			camera.position[0],
			camera.position[1],
			camera.position[2]);
		context.Fail(__LINE__, buffer);
	}
}

void TestWalkCharacterJumpInPlaceIgnoresAirborneMoveInput(TestContext &context)
{
	const VoxelWorld world = MakeWalkTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));
	SetPhysicsWalkAirControlMode(physics.get(), WalkAirControlMode::Realistic);

	CameraState camera = MakeTestCamera({2.5f, 2.65f, 4.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	const float startX = camera.position[0];
	const float startZ = camera.position[2];

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);

	for (int step = 0; step < 12; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	if (!(std::abs(camera.position[0] - startX) < 0.03f && std::abs(camera.position[2] - startZ) < 0.03f)) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"jump-in-place airborne input regressed (start=(%.3f, %.3f) final=(%.3f, %.3f, %.3f))",
			startX,
			startZ,
			camera.position[0],
			camera.position[1],
			camera.position[2]);
		context.Fail(__LINE__, buffer);
	}
}

struct JumpAirCaseTestResult {
	float afterJumpX = 0.0f;
	float afterJumpZ = 0.0f;
	float finalX = 0.0f;
	float finalY = 0.0f;
	float finalZ = 0.0f;
};

JumpAirCaseTestResult RunAirborneDirectionLockCase(TestContext &context, const bool releaseForwardAndPressStrafe)
{
	constexpr float kPi = 3.1415926535f;
	const VoxelWorld world = MakeWalkTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));
	SetPhysicsWalkAirControlMode(physics.get(), WalkAirControlMode::Realistic);

	CameraState camera = MakeTestCamera({2.5f, 2.65f, 4.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = kPi;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	JumpAirCaseTestResult result{};
	result.afterJumpX = camera.position[0];
	result.afterJumpZ = camera.position[2];
	if (releaseForwardAndPressStrafe) {
		SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_W);
		SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_A);
	}

	for (int step = 0; step < 12; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	result.finalX = camera.position[0];
	result.finalY = camera.position[1];
	result.finalZ = camera.position[2];
	return result;
}

void TestWalkCharacterAirborneInputDoesNotBendJumpTrajectory(TestContext &context)
{
	const auto [holdAfterJumpX, holdAfterJumpZ, holdFinalX, holdFinalY, holdFinalZ] = RunAirborneDirectionLockCase(context, false);
	const auto [releaseAfterJumpX, releaseAfterJumpZ, releaseFinalX, releaseFinalY, releaseFinalZ] = RunAirborneDirectionLockCase(context, true);
	const float holdForwardGain = holdFinalZ - holdAfterJumpZ;
	const float releaseForwardGain = releaseFinalZ - releaseAfterJumpZ;
	const float releaseLateralDrift = std::abs(releaseFinalX - releaseAfterJumpX);
	if (!(releaseLateralDrift < 0.05f &&
		  holdForwardGain > 0.60f &&
		  releaseForwardGain > 0.15f &&
		  releaseForwardGain < holdForwardGain - 0.15f)) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"airborne direction lock/brake regressed (holdGain=%.3f releaseGain=%.3f releaseLateral=%.3f holdFinal=(%.3f, %.3f, %.3f) releaseFinal=(%.3f, %.3f, %.3f))",
			holdForwardGain,
			releaseForwardGain,
			releaseLateralDrift,
			holdFinalX,
			holdFinalY,
			holdFinalZ,
			releaseFinalX,
			releaseFinalY,
			releaseFinalZ);
		context.Fail(__LINE__, buffer);
	}
}

JumpAirCaseTestResult RunMinecraftAirSteeringCase(TestContext &context, const bool switchToStrafe)
{
	constexpr float kPi = 3.1415926535f;
	const VoxelWorld world = MakeWalkTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));
	EXPECT_EQ(context, WalkAirControlMode::MinecraftLike, GetPhysicsWalkAirControlMode(physics.get()));

	CameraState camera = MakeTestCamera({2.5f, 2.65f, 4.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = kPi;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	JumpAirCaseTestResult result{};
	result.afterJumpX = camera.position[0];
	result.afterJumpZ = camera.position[2];
	if (switchToStrafe) {
		SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_W);
		SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_A);
	}

	const int kSimulationFrames = SimulationFrameCount(12);
	for (int step = 0; step < kSimulationFrames; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	result.finalX = camera.position[0];
	result.finalY = camera.position[1];
	result.finalZ = camera.position[2];
	return result;
}

void TestWalkCharacterMinecraftAirControlDefaultUsesLateWASDSteering(TestContext &context)
{
	const auto [holdAfterJumpX, holdAfterJumpZ, holdFinalX, holdFinalY, holdFinalZ] = RunMinecraftAirSteeringCase(context, false);
	const auto [strafeAfterJumpX, strafeAfterJumpZ, strafeFinalX, strafeFinalY, strafeFinalZ] = RunMinecraftAirSteeringCase(context, true);
	const float holdForwardGain = holdFinalZ - holdAfterJumpZ;
	const float strafeForwardGain = strafeFinalZ - strafeAfterJumpZ;
	const float strafeLateralDrift = std::abs(strafeFinalX - strafeAfterJumpX);
	if (!(strafeLateralDrift > 0.08f &&
		  holdForwardGain > 0.60f &&
		  strafeForwardGain > 0.05f &&
		  strafeForwardGain < holdForwardGain - 0.08f)) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"minecraft-like air steering/brake regressed (holdGain=%.3f strafeGain=%.3f strafeLateral=%.3f holdFinal=(%.3f, %.3f, %.3f) strafeFinal=(%.3f, %.3f, %.3f))",
			holdForwardGain,
			strafeForwardGain,
			strafeLateralDrift,
			holdFinalX,
			holdFinalY,
			holdFinalZ,
			strafeFinalX,
			strafeFinalY,
			strafeFinalZ);
		context.Fail(__LINE__, buffer);
	}
}

void TestWalkCharacterSneakLowersCameraHeight(TestContext &context)
{
	const VoxelWorld world = MakeWalkTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({2.5f, 2.65f, 4.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));
	const float standingHeight = camera.position[1];

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	EXPECT_TRUE(context, camera.position[1] < standingHeight - 0.1f);
	EXPECT_TRUE(context, camera.position[1] > standingHeight - 0.25f);

	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_RSHIFT);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	EXPECT_TRUE(context, camera.position[1] > standingHeight - 0.05f);
	EXPECT_TRUE(context, camera.position[1] < standingHeight + 0.05f);
}

void TestWalkCharacterSneakPreventsLeavingLedge(TestContext &context)
{
	const VoxelWorld world = MakeWalkLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));
	SetPhysicsWalkAirControlMode(physics.get(), WalkAirControlMode::Realistic);

	CameraState camera = MakeTestCamera({2.5f, 3.65f, 4.18f});
	camera.controlMode = CameraState::ControlMode::Walk;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 30; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	EXPECT_TRUE(context, camera.position[2] > 3.6f);
	EXPECT_TRUE(context, camera.position[2] < 3.85f);
	EXPECT_TRUE(context, camera.position[1] > 3.05f);
	EXPECT_TRUE(context, camera.position[1] < 3.6f);
}

void TestWalkCharacterSneakCanStrafeAlongLedge(TestContext &context)
{
	const VoxelWorld world = MakeWalkLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({2.5f, 3.65f, 4.05f});
	camera.controlMode = CameraState::ControlMode::Walk;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_D);
	for (int step = 0; step < 20; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	EXPECT_TRUE(context, camera.position[0] > 3.0f);
	EXPECT_TRUE(context, camera.position[2] > 3.95f);
	EXPECT_TRUE(context, camera.position[2] < 4.15f);
	EXPECT_TRUE(context, camera.position[1] > 3.25f);
	EXPECT_TRUE(context, camera.position[1] < 3.55f);
}

void TestWalkCharacterSneakCanMoveDiagonallyAlongLedge(TestContext &context)
{
	const VoxelWorld world = MakeWalkLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({2.5f, 3.65f, 4.12f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = 0.35f;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 30; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	EXPECT_TRUE(context, camera.position[0] > 2.75f);
	EXPECT_TRUE(context, camera.position[2] > 3.6f);
	EXPECT_TRUE(context, camera.position[2] < 3.85f);
	EXPECT_TRUE(context, camera.position[1] > 3.05f);
	EXPECT_TRUE(context, camera.position[1] < 3.6f);
}

void TestWalkCharacterSneakHoldsDiagonalEdgeOverTime(TestContext &context)
{
	const VoxelWorld world = MakeWalkLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({2.5f, 3.65f, 4.12f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = 0.35f;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	float minY = camera.position[1];
	for (int step = 0; step < 180; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		minY = std::min(minY, camera.position[1]);
	}

	EXPECT_TRUE(context, minY > 3.0f);
	EXPECT_TRUE(context, camera.position[0] > 3.0f);
	EXPECT_TRUE(context, camera.position[2] > 3.45f);
	EXPECT_TRUE(context, camera.position[2] < 3.9f);
}

void TestWalkCharacterSneakHoldsStraightEdgeOverTime(TestContext &context)
{
	const VoxelWorld world = MakeWalkLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({2.5f, 3.65f, 4.18f});
	camera.controlMode = CameraState::ControlMode::Walk;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	float minY = camera.position[1];
	for (int step = 0; step < 180; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		minY = std::min(minY, camera.position[1]);
	}

	EXPECT_TRUE(context, minY > 3.0f);
	EXPECT_TRUE(context, camera.position[2] > 3.55f);
	EXPECT_TRUE(context, camera.position[2] < 3.9f);
}

void TestWalkCharacterReleasingSneakDoesNotImmediatelyDropFromLedge(TestContext &context)
{
	const VoxelWorld world = MakeWalkLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({2.5f, 3.65f, 4.18f});
	camera.controlMode = CameraState::ControlMode::Walk;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 30; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_W);
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_RSHIFT);
	float minY = camera.position[1];
	for (int step = 0; step < 30; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		minY = std::min(minY, camera.position[1]);
	}

	EXPECT_TRUE(context, minY > 3.0f);
	EXPECT_TRUE(context, camera.position[2] > 3.55f);
	EXPECT_TRUE(context, camera.position[2] < 3.95f);
}

void TestVoxelLabReleasingSneakOnExactSideEdgeDoesNotDrop(TestContext &context)
{
	AppState state{};
	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::VoxelLab));
	EXPECT_TRUE(context, state.world().voxelWorld != nullptr);

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), state.world().voxelWorld.get()));

	CameraState camera = MakeTestCamera({-8.5f, 2.65f, 9.39f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = 3.1415926535f;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), state.world().voxelWorld.get(), &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	for (int step = 0; step < 5; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	}
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_RSHIFT);

	float minCameraY = camera.position[1];
	for (int step = 0; step < 180; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
		minCameraY = std::min(minCameraY, camera.position[1]);
	}

	const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
	if (!(camera.position[1] > 2.40f &&
		  camera.position[1] < 2.70f &&
		  minCameraY > 2.35f &&
		  std::abs(camera.position[0] + 8.5f) < 0.08f &&
		  std::abs(camera.position[2] - 9.39f) < 0.12f &&
		  info.feetPosition[1] > 0.95f &&
		  info.supportState != PhysicsWalkSupportDebugState::Air &&
		  !info.sneakActive)) {
		char buffer[320]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"VoxelLab release-sneak exact side-edge regressed (cam=(%.3f, %.3f, %.3f) feet=(%.3f, %.3f, %.3f) state=%u score=%.3f minY=%.3f sneak=%u csk=%u csi=%u lgr=%u)",
			camera.position[0],
			camera.position[1],
			camera.position[2],
			info.feetPosition[0],
			info.feetPosition[1],
			info.feetPosition[2],
			static_cast<unsigned>(info.supportState),
			info.footSupportScore,
			minCameraY,
			info.sneakActive ? 1u : 0u,
			info.cachedSneakSupportValid ? 1u : 0u,
			info.feetInsideCachedSneakSupport ? 1u : 0u,
			info.ledgeReleaseGraceFramesRemaining);
		context.Fail(__LINE__, buffer);
	}
}

void TestVoxelLabReleaseSneakThenJumpInPlaceFromExactSideEdgeLandsAndHolds(TestContext &context)
{
	AppState state{};
	EXPECT_TRUE(context, CreateVoxelSceneWorld(&state, VoxelScenePreset::VoxelLab));
	EXPECT_TRUE(context, state.world().voxelWorld != nullptr);

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), state.world().voxelWorld.get()));

	CameraState camera = MakeTestCamera({-8.5f, 2.65f, 9.39f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = 3.1415926535f;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), state.world().voxelWorld.get(), &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	for (int step = 0; step < 5; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	}
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_RSHIFT);
	for (int step = 0; step < 25; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	}

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	float minCameraY = camera.position[1];
	for (int step = 0; step < 240; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), state.world().voxelWorld.get(), &camera, &input, 1.0f / 60.0f));
		minCameraY = std::min(minCameraY, camera.position[1]);
	}

	const PhysicsWalkDebugInfo info = GetPhysicsWalkDebugInfo(physics.get());
	if (!(camera.position[1] > 2.40f &&
		  camera.position[1] < 2.70f &&
		  minCameraY > 2.35f &&
		  std::abs(camera.position[0] + 8.5f) < 0.08f &&
		  std::abs(camera.position[2] - 9.39f) < 0.12f &&
		  info.feetPosition[1] > 0.95f &&
		  info.supportState != PhysicsWalkSupportDebugState::Air &&
		  !info.sneakActive)) {
		char buffer[320]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"VoxelLab release-sneak->jump exact side-edge regressed (cam=(%.3f, %.3f, %.3f) feet=(%.3f, %.3f, %.3f) state=%u score=%.3f minY=%.3f sneak=%u jl=%u csk=%u lgr=%u)",
			camera.position[0],
			camera.position[1],
			camera.position[2],
			info.feetPosition[0],
			info.feetPosition[1],
			info.feetPosition[2],
			static_cast<unsigned>(info.supportState),
			info.footSupportScore,
			minCameraY,
			info.sneakActive ? 1u : 0u,
			info.jumpLockActive ? 1u : 0u,
			info.cachedSneakSupportValid ? 1u : 0u,
			info.ledgeReleaseGraceFramesRemaining);
		context.Fail(__LINE__, buffer);
	}
}

void TestWalkCharacterReleasingSneakThenJumpingFromStraightLedgeKeepsTakeoff(TestContext &context)
{
	for (int releaseDelayFrames = 0; releaseDelayFrames <= 1; ++releaseDelayFrames) {
		VoxelWorld world = MakeWalkLedgeTestWorld();

		const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
		EXPECT_TRUE(context, physics != nullptr);
		EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

		CameraState camera = MakeTestCamera({2.5f, 3.65f, 4.18f});
		camera.controlMode = CameraState::ControlMode::Walk;
		EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

		InputState input{};
		InitializeInputState(input);
		SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
		SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
		for (int step = 0; step < 30; ++step) {
			EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		}

		SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_RSHIFT);
		for (int step = 0; step < releaseDelayFrames; ++step) {
			EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		}

		const float jumpStartY = camera.position[1];
		const float jumpStartZ = camera.position[2];
		SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

		const float afterJumpTickY = camera.position[1];
		const float afterJumpTickZ = camera.position[2];
		float minY = afterJumpTickY;
		float previousZ = afterJumpTickZ;
		int stalledForwardFrames = 0;
		int maxStalledForwardFrames = 0;
		for (int step = 0; step < 12; ++step) {
			EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
			minY = std::min(minY, camera.position[1]);
			const float forwardProgress = previousZ - camera.position[2];
			if (camera.position[1] > jumpStartY - 0.02f && forwardProgress < 0.0025f) {
				++stalledForwardFrames;
				maxStalledForwardFrames = std::max(maxStalledForwardFrames, stalledForwardFrames);
			} else {
				stalledForwardFrames = 0;
			}
			previousZ = camera.position[2];
		}

		const bool keptTakeoff =
			afterJumpTickY > jumpStartY + 0.03f &&
			minY > jumpStartY - 0.02f &&
			camera.position[1] > jumpStartY + 0.12f &&
			camera.position[2] < jumpStartZ - 0.12f &&
			maxStalledForwardFrames <= 2;
		if (!keptTakeoff) {
			char buffer[256]{};
			std::snprintf(
				buffer,
				sizeof(buffer),
				"release->jump straight ledge regressed (delay=%d, start=%.3f/%.3f, after=%.3f/%.3f, final=%.3f/%.3f, minY=%.3f, max stall=%d)",
				releaseDelayFrames,
				jumpStartY,
				jumpStartZ,
				afterJumpTickY,
				afterJumpTickZ,
				camera.position[1],
				camera.position[2],
				minY,
				maxStalledForwardFrames);
			context.Fail(__LINE__, buffer);
		}
	}
}

void TestWalkCharacterReleasingSneakThenJumpingFromCornerLedgeKeepsTakeoff(TestContext &context)
{
	constexpr float kPiOverFour = 0.785398163f;
	for (int releaseDelayFrames = 0; releaseDelayFrames <= 1; ++releaseDelayFrames) {
		VoxelWorld world = MakeWalkCornerLedgeTestWorld();

		const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
		EXPECT_TRUE(context, physics != nullptr);
		EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

		CameraState camera = MakeTestCamera({4.18f, 3.65f, 4.18f});
		camera.controlMode = CameraState::ControlMode::Walk;
		camera.yawRadians = -kPiOverFour;
		EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

		InputState input{};
		InitializeInputState(input);
		SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
		SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
		for (int step = 0; step < 30; ++step) {
			EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		}

		SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_RSHIFT);
		for (int step = 0; step < releaseDelayFrames; ++step) {
			EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		}

		const float jumpStartX = camera.position[0];
		const float jumpStartY = camera.position[1];
		const float jumpStartZ = camera.position[2];
		SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

		const float afterJumpTickX = camera.position[0];
		const float afterJumpTickY = camera.position[1];
		const float afterJumpTickZ = camera.position[2];
		float minY = afterJumpTickY;
		float previousX = afterJumpTickX;
		float previousZ = afterJumpTickZ;
		int stalledForwardFrames = 0;
		int maxStalledForwardFrames = 0;
		for (int step = 0; step < 12; ++step) {
			EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
			minY = std::min(minY, camera.position[1]);
			const float stepX = previousX - camera.position[0];
			const float stepZ = previousZ - camera.position[2];
			const float forwardProgress = std::sqrt(stepX * stepX + stepZ * stepZ);
			if (camera.position[1] > jumpStartY - 0.02f && forwardProgress < 0.0025f) {
				++stalledForwardFrames;
				maxStalledForwardFrames = std::max(maxStalledForwardFrames, stalledForwardFrames);
			} else {
				stalledForwardFrames = 0;
			}
			previousX = camera.position[0];
			previousZ = camera.position[2];
		}

		const float finalHorizontalProgress =
			std::sqrt(
				(jumpStartX - camera.position[0]) * (jumpStartX - camera.position[0]) +
				(jumpStartZ - camera.position[2]) * (jumpStartZ - camera.position[2]));
		const bool keptTakeoff =
			afterJumpTickY > jumpStartY + 0.03f &&
			minY > jumpStartY - 0.02f &&
			camera.position[1] > jumpStartY + 0.12f &&
			finalHorizontalProgress > 0.14f &&
			maxStalledForwardFrames <= 2;
		if (!keptTakeoff) {
			char buffer[256]{};
			std::snprintf(
				buffer,
				sizeof(buffer),
				"release->jump corner ledge regressed (delay=%d, start=%.3f/%.3f/%.3f, after=%.3f/%.3f/%.3f, final=%.3f/%.3f/%.3f, minY=%.3f, max stall=%d)",
				releaseDelayFrames,
				jumpStartX,
				jumpStartY,
				jumpStartZ,
				afterJumpTickX,
				afterJumpTickY,
				afterJumpTickZ,
				camera.position[0],
				camera.position[1],
				camera.position[2],
				minY,
				maxStalledForwardFrames);
			context.Fail(__LINE__, buffer);
		}
	}
}

void TestWalkCharacterJumpingInPlaceAtCornerKeepsTopFaceOwnership(TestContext &context)
{
	constexpr float kPiOverFour = 0.785398163f;
	const VoxelWorld world = MakeWalkCornerLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({4.18f, 3.65f, 4.18f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = -kPiOverFour;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 30; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_W);
	const float cornerX = camera.position[0];
	const float cornerY = camera.position[1];
	const float cornerZ = camera.position[2];

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	float minY = camera.position[1];
	float maxHorizontalDrift = 0.0f;
	for (int step = 0; step < 30; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		minY = std::min(minY, camera.position[1]);
		const float driftX = camera.position[0] - cornerX;
		const float driftZ = camera.position[2] - cornerZ;
		maxHorizontalDrift = std::max(maxHorizontalDrift, std::sqrt(driftX * driftX + driftZ * driftZ));
	}

	EXPECT_TRUE(context, minY > cornerY - 0.35f);
	EXPECT_TRUE(context, camera.position[1] > cornerY - 0.25f);
	EXPECT_TRUE(context, camera.position[0] > 3.45f);
	EXPECT_TRUE(context, camera.position[2] > 3.45f);
	EXPECT_TRUE(context, maxHorizontalDrift < 0.22f);
}

void TestWalkCharacterSneakJumpUnderLowCeilingKeepsSupport(TestContext &context)
{
	constexpr float kPiOverFour = 0.785398163f;
	const VoxelWorld world = MakeWalkCornerLedgeLowCeilingTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({4.18f, 3.65f, 4.18f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = -kPiOverFour;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 30; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_W);
	const float cornerX = camera.position[0];
	const float cornerY = camera.position[1];
	const float cornerZ = camera.position[2];

	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	float minY = camera.position[1];
	float maxHorizontalDrift = 0.0f;
	for (int step = 0; step < 24; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		minY = std::min(minY, camera.position[1]);
		const float driftX = camera.position[0] - cornerX;
		const float driftZ = camera.position[2] - cornerZ;
		maxHorizontalDrift = std::max(maxHorizontalDrift, std::sqrt(driftX * driftX + driftZ * driftZ));
	}

	EXPECT_TRUE(context, minY > cornerY - 0.35f);
	EXPECT_TRUE(context, camera.position[1] > cornerY - 0.25f);
	EXPECT_TRUE(context, camera.position[0] > 3.45f);
	EXPECT_TRUE(context, camera.position[2] > 3.45f);
	EXPECT_TRUE(context, maxHorizontalDrift < 0.2f);
}

void TestWalkCharacterSneakJumpFromStraightEdgeWithOutwardInputFallsOffEdge(TestContext &context)
{
	const VoxelWorld world = MakeWalkLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({2.5f, 3.65f, 4.18f});
	camera.controlMode = CameraState::ControlMode::Walk;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 30; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	const float jumpStartY = camera.position[1];
	const float jumpStartZ = camera.position[2];
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	const float afterJumpTickY = camera.position[1];
	float minY = afterJumpTickY;
	for (int step = 0; step < 60; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		minY = std::min(minY, camera.position[1]);
	}

	const bool fellOffEdge =
		afterJumpTickY > jumpStartY + 0.03f &&
		minY < jumpStartY - 0.35f &&
		camera.position[1] < jumpStartY - 0.15f &&
		camera.position[2] < jumpStartZ - 0.08f;
	if (!fellOffEdge) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"sneak jump outward edge fall regressed (startY=%.3f, startZ=%.3f, afterY=%.3f, finalY=%.3f, finalZ=%.3f, minY=%.3f)",
			jumpStartY,
			jumpStartZ,
			afterJumpTickY,
			camera.position[1],
			camera.position[2],
			minY);
		context.Fail(__LINE__, buffer);
	}
}

void TestWalkCharacterSneakJumpFromStraightEdgeWithAlongAndOutwardInputLeavesEdge(TestContext &context)
{
	const VoxelWorld world = MakeWalkLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({2.5f, 3.65f, 4.18f});
	camera.controlMode = CameraState::ControlMode::Walk;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 30; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	const float jumpStartX = camera.position[0];
	const float jumpStartY = camera.position[1];
	const float jumpStartZ = camera.position[2];
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_D);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	const float afterJumpTickY = camera.position[1];
	float minY = afterJumpTickY;
	for (int step = 0; step < 60; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		minY = std::min(minY, camera.position[1]);
	}

	const bool leftEdgeWithMotion =
		afterJumpTickY > jumpStartY + 0.03f &&
		minY < jumpStartY - 0.35f &&
		camera.position[1] < jumpStartY - 0.15f &&
		camera.position[0] > jumpStartX + 0.08f &&
		camera.position[2] < jumpStartZ - 0.05f;
	if (!leftEdgeWithMotion) {
		char buffer[320]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"sneak jump edge exit regressed (start=%.3f/%.3f/%.3f, afterY=%.3f, final=%.3f/%.3f/%.3f, minY=%.3f)",
			jumpStartX,
			jumpStartY,
			jumpStartZ,
			afterJumpTickY,
			camera.position[0],
			camera.position[1],
			camera.position[2],
			minY);
		context.Fail(__LINE__, buffer);
	}
}

void TestWalkCharacterSneakJumpIntoCeilingCornerEdgeCanStillLeaveEdge(TestContext &context)
{
	constexpr float kPiOverFour = 0.785398163f;
	const VoxelWorld world = MakeWalkCornerLedgeLowCeilingTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({4.18f, 3.65f, 4.18f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = -kPiOverFour;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 30; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	const float jumpStartX = camera.position[0];
	const float jumpStartY = camera.position[1];
	const float jumpStartZ = camera.position[2];
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	const float afterJumpTickY = camera.position[1];
	float minY = afterJumpTickY;
	float maxOutwardSlip = 0.0f;
	for (int step = 0; step < 60; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		minY = std::min(minY, camera.position[1]);
		const float slipX = std::max(0.0f, jumpStartX - camera.position[0]);
		const float slipZ = std::max(0.0f, jumpStartZ - camera.position[2]);
		maxOutwardSlip = std::max(maxOutwardSlip, std::sqrt(slipX * slipX + slipZ * slipZ));
	}

	const bool leftEdge =
		afterJumpTickY > jumpStartY + 0.03f &&
		minY < jumpStartY - 0.35f &&
		camera.position[0] < jumpStartX - 0.08f &&
		camera.position[2] < jumpStartZ - 0.08f &&
		maxOutwardSlip > 0.08f;
	if (!leftEdge) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"sneak jump ceiling edge exit regressed (start=%.3f/%.3f/%.3f, afterY=%.3f, final=%.3f/%.3f/%.3f, minY=%.3f, maxSlip=%.3f)",
			jumpStartX,
			jumpStartY,
			jumpStartZ,
			afterJumpTickY,
			camera.position[0],
			camera.position[1],
			camera.position[2],
			minY,
			maxOutwardSlip);
		context.Fail(__LINE__, buffer);
	}
}

void TestWalkCharacterSneakCanMoveAlongCornerZEdge(TestContext &context)
{
	const VoxelWorld world = MakeWalkCornerLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({4.18f, 3.65f, 4.18f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = -0.785398163f;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 30; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	const float cornerX = camera.position[0];
	const float cornerZ = camera.position[2];
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_W);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_A);
	for (int step = 0; step < 20; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	EXPECT_TRUE(context, camera.position[2] > cornerZ + 0.12f);
	EXPECT_TRUE(context, camera.position[0] > 3.45f);
	EXPECT_TRUE(context, camera.position[0] < cornerX + 0.05f);
	EXPECT_TRUE(context, camera.position[1] > 3.2f);
}

void TestWalkCharacterSneakCanMoveAlongCornerXEdge(TestContext &context)
{
	const VoxelWorld world = MakeWalkCornerLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({4.18f, 3.65f, 4.18f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = -0.785398163f;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 30; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	const float cornerX = camera.position[0];
	const float cornerZ = camera.position[2];
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_W);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_D);
	for (int step = 0; step < 20; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	EXPECT_TRUE(context, camera.position[0] > cornerX + 0.12f);
	EXPECT_TRUE(context, camera.position[2] > 3.45f);
	EXPECT_TRUE(context, camera.position[2] < cornerZ + 0.05f);
	EXPECT_TRUE(context, camera.position[1] > 3.2f);
}

void TestWalkCharacterSneakCanReachCornerLedge(TestContext &context)
{
	const VoxelWorld world = MakeWalkCornerLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({4.18f, 3.65f, 4.18f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = -0.785398163f;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 30; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	EXPECT_TRUE(context, camera.position[0] > 3.6f);
	EXPECT_TRUE(context, camera.position[0] < 3.85f);
	EXPECT_TRUE(context, camera.position[2] > 3.6f);
	EXPECT_TRUE(context, camera.position[2] < 3.85f);
	EXPECT_TRUE(context, camera.position[1] > 3.25f);
	EXPECT_TRUE(context, camera.position[1] < 3.6f);
}

void TestWalkCharacterSneakDoesNotIdleSlideDownFromCorner(TestContext &context)
{
	const VoxelWorld world = MakeWalkCornerLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({4.18f, 3.65f, 4.18f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = -0.785398163f;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 30; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	const float idleCornerX = camera.position[0];
	const float idleCornerZ = camera.position[2];
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_W);
	float minY = camera.position[1];
	for (int step = 0; step < 180; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		minY = std::min(minY, camera.position[1]);
	}

	EXPECT_TRUE(context, minY > 3.2f);
	EXPECT_TRUE(context, camera.position[0] > idleCornerX - 0.08f);
	EXPECT_TRUE(context, camera.position[0] < idleCornerX + 0.08f);
	EXPECT_TRUE(context, camera.position[2] > idleCornerZ - 0.08f);
	EXPECT_TRUE(context, camera.position[2] < idleCornerZ + 0.08f);
}

void TestWalkCharacterLedgeJumpWithoutTakeoffMomentumIgnoresLateForwardInput(TestContext &context)
{
	const VoxelWorld world = MakeWalkLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));
	SetPhysicsWalkAirControlMode(physics.get(), WalkAirControlMode::Realistic);

	CameraState camera = MakeTestCamera({2.5f, 3.65f, 4.18f});
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));
	const float startX = camera.position[0];
	const float startZ = camera.position[2];

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 15; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	EXPECT_TRUE(context, camera.position[1] > 2.9f);
	EXPECT_TRUE(context, std::abs(camera.position[0] - startX) < 0.03f);
	EXPECT_TRUE(context, std::abs(camera.position[2] - startZ) < 0.03f);
}

void TestWalkCharacterOffsetLedgeJumpWithoutTakeoffMomentumIgnoresLateForwardInput(TestContext &context)
{
	const VoxelWorld world = MakeWalkLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));
	SetPhysicsWalkAirControlMode(physics.get(), WalkAirControlMode::Realistic);

	CameraState camera = MakeTestCamera({2.83f, 3.65f, 4.07f});
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));
	const float startX = camera.position[0];
	const float startZ = camera.position[2];

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	for (int step = 0; step < 15; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	EXPECT_TRUE(context, camera.position[1] > 2.9f);
	EXPECT_TRUE(context, std::abs(camera.position[0] - startX) < 0.03f);
	EXPECT_TRUE(context, std::abs(camera.position[2] - startZ) < 0.03f);
}

void TestWalkCharacterJumpApproachDoesNotTeleportAcrossOffsets(TestContext &context)
{
	constexpr float kPi = 3.1415926535f;
	for (int startXStep = 0; startXStep <= 55; ++startXStep) {
		const float startX = 1.25f + static_cast<float>(startXStep) * 0.1f;
		VoxelWorld world = MakeWalkLedgeTestWorld();

		const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
		EXPECT_TRUE(context, physics != nullptr);
		EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

		CameraState camera = MakeTestCamera({startX, 2.65f, 2.45f});
		camera.controlMode = CameraState::ControlMode::Walk;
		camera.yawRadians = kPi;
		EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

		InputState input{};
		InitializeInputState(input);
		SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
		SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		const float afterJumpTickY = camera.position[1];
		const float afterJumpTickZ = camera.position[2];
		float previousZ = afterJumpTickZ;
		int stalledTopEdgeFrames = 0;
		int maxStalledTopEdgeFrames = 0;
		SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);
		for (int step = 0; step < 25; ++step) {
			EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
			const float zProgress = camera.position[2] - previousZ;
			if (camera.position[1] > 2.95f && camera.position[1] < 3.55f && zProgress < 0.0025f) {
				++stalledTopEdgeFrames;
				maxStalledTopEdgeFrames = std::max(maxStalledTopEdgeFrames, stalledTopEdgeFrames);
			} else {
				stalledTopEdgeFrames = 0;
			}
			previousZ = camera.position[2];
		}

		const bool jumpedNaturallyWithoutTeleport =
			afterJumpTickY < 3.05f &&
			afterJumpTickZ > 2.48f &&
			camera.position[1] > 3.35f &&
			camera.position[2] > 4.05f &&
			maxStalledTopEdgeFrames <= 2;
		if (!jumpedNaturallyWithoutTeleport) {
			char buffer[256]{};
			std::snprintf(
				buffer,
				sizeof(buffer),
				"jump approach regressed at x=%.2f (after first tick=%.3f, %.3f, final=%.3f, %.3f, %.3f, max stall=%d)",
				startX,
				afterJumpTickY,
				afterJumpTickZ,
				camera.position[0],
				camera.position[1],
				camera.position[2],
				maxStalledTopEdgeFrames);
			context.Fail(__LINE__, buffer);
		}
	}
}

void TestWalkCharacterCanJumpOntoNegativeSingleBlockEdge(TestContext &context)
{
	constexpr float kPi = 3.1415926535f;
	const VoxelWorld world = MakeWalkNegativeSingleBlockTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({-5.5f, 2.65f, 4.45f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = kPi;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	const float afterJumpTickY = camera.position[1];
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	bool touchedBlockTop = false;
	float previousZ = camera.position[2];
	float previousY = camera.position[1];
	int stalledTopEdgeFrames = 0;
	int maxStalledTopEdgeFrames = 0;
	float maxLateFrameRise = 0.0f;
	for (int step = 0; step < 25; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		if (camera.position[1] > 3.3f &&
			camera.position[2] > 5.75f &&
			camera.position[2] < 7.1f &&
			camera.position[0] > -5.95f &&
			camera.position[0] < -5.05f) {
			touchedBlockTop = true;
		}

		const float zProgress = camera.position[2] - previousZ;
		maxLateFrameRise = std::max(maxLateFrameRise, camera.position[1] - previousY);
		if (camera.position[1] > 2.95f && camera.position[1] < 3.7f && zProgress < 0.0025f) {
			++stalledTopEdgeFrames;
			maxStalledTopEdgeFrames = std::max(maxStalledTopEdgeFrames, stalledTopEdgeFrames);
		} else {
			stalledTopEdgeFrames = 0;
		}
		previousZ = camera.position[2];
		previousY = camera.position[1];
	}

	EXPECT_TRUE(context, afterJumpTickY < 3.05f);
	EXPECT_TRUE(context, touchedBlockTop);
	EXPECT_TRUE(context, maxStalledTopEdgeFrames <= 2);
	if (!(maxLateFrameRise < 0.13f)) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"negative single-block jump late snap regressed (afterJumpY=%.3f final cam=(%.3f, %.3f, %.3f) maxLateRise=%.3f max stall=%d)",
			afterJumpTickY,
			camera.position[0],
			camera.position[1],
			camera.position[2],
			maxLateFrameRise,
			maxStalledTopEdgeFrames);
		context.Fail(__LINE__, buffer);
	}
}

void TestWalkCharacterCanJumpOntoPositiveSingleBlockEdge(TestContext &context)
{
	constexpr float kPi = 3.1415926535f;
	const VoxelWorld world = MakeWalkPositiveSingleBlockTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({5.5f, 2.65f, 4.45f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = kPi;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	const float afterJumpTickY = camera.position[1];
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	bool touchedBlockTop = false;
	float previousZ = camera.position[2];
	float previousY = camera.position[1];
	int stalledTopEdgeFrames = 0;
	int maxStalledTopEdgeFrames = 0;
	float maxLateFrameRise = 0.0f;
	for (int step = 0; step < 25; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		if (camera.position[1] > 3.3f &&
			camera.position[2] > 5.75f &&
			camera.position[2] < 7.1f &&
			camera.position[0] > 5.05f &&
			camera.position[0] < 5.95f) {
			touchedBlockTop = true;
		}

		const float zProgress = camera.position[2] - previousZ;
		maxLateFrameRise = std::max(maxLateFrameRise, camera.position[1] - previousY);
		if (camera.position[1] > 2.95f && camera.position[1] < 3.7f && zProgress < 0.0025f) {
			++stalledTopEdgeFrames;
			maxStalledTopEdgeFrames = std::max(maxStalledTopEdgeFrames, stalledTopEdgeFrames);
		} else {
			stalledTopEdgeFrames = 0;
		}
		previousZ = camera.position[2];
		previousY = camera.position[1];
	}

	EXPECT_TRUE(context, afterJumpTickY < 3.05f);
	EXPECT_TRUE(context, touchedBlockTop);
	EXPECT_TRUE(context, maxStalledTopEdgeFrames <= 2);
	if (!(maxLateFrameRise < 0.13f)) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"positive single-block jump late snap regressed (afterJumpY=%.3f final cam=(%.3f, %.3f, %.3f) maxLateRise=%.3f max stall=%d)",
			afterJumpTickY,
			camera.position[0],
			camera.position[1],
			camera.position[2],
			maxLateFrameRise,
			maxStalledTopEdgeFrames);
		context.Fail(__LINE__, buffer);
	}
}

void TestWalkCharacterCanJumpOntoCornerSideFromPerpendicularApproach(TestContext &context)
{
	constexpr float kPi = 3.1415926535f;
	const VoxelWorld world = MakeWalkCornerLedgeTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({4.18f, 2.65f, 3.45f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = kPi;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	const float jumpStartY = camera.position[1];
	EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	const float afterJumpTickY = camera.position[1];
	SendKeyEvent(&input, SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE);

	float maxY = afterJumpTickY;
	bool touchedCornerTop = false;
	for (int step = 0; step < 25; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
		maxY = std::max(maxY, camera.position[1]);
		if (camera.position[1] > 3.25f &&
			camera.position[0] > 4.02f &&
			camera.position[0] < 4.35f &&
			camera.position[2] > 4.02f) {
			touchedCornerTop = true;
		}
	}

	EXPECT_TRUE(context, afterJumpTickY > jumpStartY + 0.03f);
	EXPECT_TRUE(context, maxY > jumpStartY + 0.2f);
	EXPECT_TRUE(context, touchedCornerTop);
}

void TestWalkCharacterSneakCanMoveAlongNegativeSingleBlockEdge(TestContext &context)
{
	constexpr float kPi = 3.1415926535f;
	const VoxelWorld world = MakeWalkNegativeSingleBlockSneakTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({-6.82f, 5.65f, 3.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = kPi;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	const float startX = camera.position[0];
	const float startZ = camera.position[2];
	for (int step = 0; step < 20; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	EXPECT_TRUE(context, camera.position[2] > startZ + 0.35f);
	EXPECT_TRUE(context, camera.position[0] > startX - 0.05f);
	EXPECT_TRUE(context, camera.position[0] < startX + 0.08f);
	EXPECT_TRUE(context, camera.position[1] > 5.3f);
}

void TestWalkCharacterSneakCanMoveAlongIsolatedCornerEdge(TestContext &context)
{
	constexpr float kPi = 3.1415926535f;
	const VoxelWorld world = MakeWalkIsolatedCornerSingleBlockTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({-0.2f, 17.65f, 0.2f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.yawRadians = kPi;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT);
	SendKeyEvent(&input, SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W);
	const float startX = camera.position[0];
	const float startZ = camera.position[2];
	for (int step = 0; step < 20; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	EXPECT_TRUE(context, camera.position[2] > startZ + 0.3f);
	EXPECT_TRUE(context, camera.position[0] > startX - 0.08f);
	EXPECT_TRUE(context, camera.position[0] < startX + 0.08f);
	EXPECT_TRUE(context, camera.position[1] > 17.3f);
}

void TestWalkCharacterDoesNotMagnetSnapDuringFreeFall(TestContext &context)
{
	const VoxelWorld world = MakeWalkTestWorld();

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({2.5f, 5.0f, 4.5f});
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	InitializeInputState(input);
	for (int step = 0; step < 20; ++step) {
		EXPECT_TRUE(context, TickWalkCharacter(physics.get(), &world, &camera, &input, 1.0f / 60.0f));
	}

	EXPECT_TRUE(context, camera.position[1] < 4.5f);
	EXPECT_TRUE(context, camera.position[1] > 3.2f);
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

	PressInputAction(input, InputAction::CycleScenePreset);
	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, nullptr, &render, &debug));
	EXPECT_TRUE(context, world.scenePresetReloadRequested);
	EXPECT_EQ(context, VoxelScenePreset::MeshingStress, world.requestedScenePreset);
}

void TestUpdateAppRequestsWorldSnapshotSave(TestContext &context)
{
	PlatformState platform{};
	SimulationState simulation{};
	CameraState camera = MakeTestCamera({2.0f, 3.0f, 4.0f});
	InputState input{};
	InitializeInputState(input);
	InteractionState interaction{};
	WorldState world{};
	world.voxelWorld = std::make_unique<VoxelWorld>(MakeWalkTestWorld());
	RenderState render{};
	DebugState debug{};

	PressInputAction(input, InputAction::SaveWorldSnapshot);
	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, nullptr, &render, &debug));
	EXPECT_TRUE(context, world.snapshotSaveRequested);
	EXPECT_TRUE(context, !world.snapshotLoadRequested);
}

void TestUpdateAppRequestsWorldSnapshotLoad(TestContext &context)
{
	PlatformState platform{};
	SimulationState simulation{};
	CameraState camera = MakeTestCamera({2.0f, 3.0f, 4.0f});
	InputState input{};
	InitializeInputState(input);
	InteractionState interaction{};
	WorldState world{};
	world.voxelWorld = std::make_unique<VoxelWorld>(MakeWalkTestWorld());
	RenderState render{};
	DebugState debug{};

	PressInputAction(input, InputAction::LoadWorldSnapshot);
	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, nullptr, &render, &debug));
	EXPECT_TRUE(context, world.snapshotLoadRequested);
}

void TestUpdateAppRequestsScreenshotCapture(TestContext &context)
{
	PlatformState platform{};
	SimulationState simulation{};
	CameraState camera = MakeTestCamera({2.0f, 3.0f, 4.0f});
	InputState input{};
	InitializeInputState(input);
	InteractionState interaction{};
	WorldState world{};
	world.voxelWorld = std::make_unique<VoxelWorld>(MakeWalkTestWorld());
	RenderState render{};
	DebugState debug{};

	PressInputAction(input, InputAction::CaptureScreenshot);
	EXPECT_TRUE(context, UpdateApp(&platform, &simulation, &camera, &input, &interaction, &world, nullptr, &render, &debug));
	EXPECT_TRUE(context, render.screenshotCaptureRequested);
}

void TestUpdateAppAdjustsShadowTuningControls(TestContext &context)
{
	(void)context;
	// CSM removed per TODO.md §5.2.D (session 20x). The shadow tuning
	// controls ladder (O / U / I / V cycle, ShadowTuningTarget enum) is
	// retired; the corresponding keyboard shortcuts are no-ops.
}

void TestUpdateVoxelInteractionRemovesTargetedBlock(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Glass, nullptr);
	ResetDirtyFlags(world);

	InputState input{};
	input.removePressed = true;
	InteractionState interaction{};

	UpdateVoxelInteraction(
		MakeTestCamera({1.5f, 1.5f, 4.5f}),
		&input,
		&world,
		&interaction,
		true,
		nullptr);

	EXPECT_EQ(context, VoxelMaterial::Air, GetVoxelMaterial(world, {1, 1, 1}));
	EXPECT_TRUE(context, !input.removePressed);
	EXPECT_TRUE(context, !interaction.selection.hasHit);
	EXPECT_EQ(context, static_cast<uint32_t>(1), CountDirtyVoxelChunks(world));
}

void TestUpdateVoxelInteractionPlacesConfiguredBlock(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Glass, nullptr);
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
		true,
		nullptr);

	EXPECT_EQ(context, VoxelMaterial::FloorWhite, GetVoxelMaterial(world, {1, 1, 2}));
	EXPECT_TRUE(context, !input.placePressed);
	EXPECT_TRUE(context, interaction.selection.hasHit);
	EXPECT_EQ(context, VoxelMaterial::FloorWhite, interaction.selection.targetMaterial);
	EXPECT_INT3_EQ(context, (Int3{1, 1, 2}), interaction.selection.targetVoxel);
	EXPECT_EQ(context, static_cast<uint32_t>(1), CountDirtyVoxelChunks(world));
}

void TestUpdateVoxelInteractionRejectsPlacementInsidePhysicsCharacter(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	for (int z = world.min.z; z < world.maxExclusive.z; ++z) {
		for (int x = world.min.x; x < world.maxExclusive.x; ++x) {
			SetVoxelMaterial(world, {x, 0, z}, VoxelMaterial::FloorWhite, nullptr);
		}
	}
	ResetDirtyFlags(world);

	const std::unique_ptr<PhysicsState, void (*)(PhysicsState *)> physics(CreatePhysicsState(), DestroyPhysicsState);
	EXPECT_TRUE(context, physics != nullptr);
	EXPECT_TRUE(context, SyncPhysicsWorld(physics.get(), &world));

	CameraState camera = MakeTestCamera({1.5f, 2.65f, 1.5f});
	camera.controlMode = CameraState::ControlMode::Walk;
	camera.pitchRadians = -1.45f;
	EXPECT_TRUE(context, SnapWalkCharacterToCamera(physics.get(), &world, &camera));

	InputState input{};
	input.placePressed = true;
	InteractionState interaction{};
	interaction.placementMaterial = VoxelMaterial::FloorGray;

	UpdateVoxelInteraction(
		camera,
		&input,
		&world,
		&interaction,
		true,
		physics.get());

	EXPECT_EQ(context, VoxelMaterial::Air, GetVoxelMaterial(world, {1, 1, 1}));
	EXPECT_TRUE(context, interaction.selection.hasHit);
	EXPECT_TRUE(context, interaction.selection.hasPlacementVoxel);
	EXPECT_INT3_EQ(context, (Int3{1, 1, 1}), interaction.selection.placementVoxel);
	EXPECT_EQ(context, static_cast<uint32_t>(0), CountDirtyVoxelChunks(world));
}

void TestUpdateVoxelInteractionSkipsEditingWhenDisabled(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Glass, nullptr);
	ResetDirtyFlags(world);

	InputState input{};
	input.removePressed = true;
	InteractionState interaction{};

	UpdateVoxelInteraction(
		MakeTestCamera({1.5f, 1.5f, 4.5f}),
		&input,
		&world,
		&interaction,
		false,
		nullptr);

	EXPECT_EQ(context, VoxelMaterial::Glass, GetVoxelMaterial(world, {1, 1, 1}));
	EXPECT_TRUE(context, interaction.selection.hasHit);
	EXPECT_TRUE(context, !input.removePressed);
	EXPECT_EQ(context, static_cast<uint32_t>(0), CountDirtyVoxelChunks(world));
}

void TestUpdateVoxelInteractionPaintModePaintsTargetedBlock(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Glass, nullptr);
	ResetDirtyFlags(world);

	InputState input{};
	input.removePressed = true;
	InteractionState interaction{};
	interaction.editorTool = DebugEditorTool::Paint;
	interaction.placementMaterial = VoxelMaterial::FloorGray;

	UpdateVoxelInteraction(
		MakeTestCamera({1.5f, 1.5f, 4.5f}),
		&input,
		&world,
		&interaction,
		true,
		nullptr);

	EXPECT_EQ(context, VoxelMaterial::FloorGray, GetVoxelMaterial(world, {1, 1, 1}));
	EXPECT_TRUE(context, interaction.selection.hasHit);
	EXPECT_TRUE(context, interaction.selection.hasTargetChunk);
	EXPECT_EQ(context, VoxelMaterial::FloorGray, interaction.selection.targetMaterial);
	EXPECT_INT3_EQ(context, (Int3{0, 0, 0}), interaction.selection.targetChunkCoord);
	EXPECT_EQ(context, static_cast<uint32_t>(1), CountDirtyVoxelChunks(world));
}

void TestUpdateVoxelInteractionEraseModeRemovesTargetedBlock(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Glass, nullptr);
	ResetDirtyFlags(world);

	InputState input{};
	input.placePressed = true;
	InteractionState interaction{};
	interaction.editorTool = DebugEditorTool::Erase;

	UpdateVoxelInteraction(
		MakeTestCamera({1.5f, 1.5f, 4.5f}),
		&input,
		&world,
		&interaction,
		true,
		nullptr);

	EXPECT_EQ(context, VoxelMaterial::Air, GetVoxelMaterial(world, {1, 1, 1}));
	EXPECT_TRUE(context, !interaction.selection.hasHit);
	EXPECT_EQ(context, static_cast<uint32_t>(1), CountDirtyVoxelChunks(world));
}

void TestUpdateVoxelInteractionFillModeFloodFillsConnectedRegion(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Glass, nullptr);
	SetVoxelMaterial(world, {2, 1, 1}, VoxelMaterial::Glass, nullptr);
	SetVoxelMaterial(world, {4, 1, 1}, VoxelMaterial::Fluid, nullptr);
	ResetDirtyFlags(world);

	InputState input{};
	input.removePressed = true;
	InteractionState interaction{};
	interaction.editorTool = DebugEditorTool::Fill;
	interaction.placementMaterial = VoxelMaterial::FloorWhite;

	UpdateVoxelInteraction(
		MakeTestCamera({1.5f, 1.5f, 4.5f}),
		&input,
		&world,
		&interaction,
		true,
		nullptr);

	EXPECT_EQ(context, VoxelMaterial::FloorWhite, GetVoxelMaterial(world, {1, 1, 1}));
	EXPECT_EQ(context, VoxelMaterial::FloorWhite, GetVoxelMaterial(world, {2, 1, 1}));
	EXPECT_EQ(context, VoxelMaterial::Fluid, GetVoxelMaterial(world, {4, 1, 1}));
	EXPECT_TRUE(context, interaction.selection.hasHit);
	EXPECT_EQ(context, VoxelMaterial::FloorWhite, interaction.selection.targetMaterial);
	EXPECT_EQ(context, static_cast<uint32_t>(1), CountDirtyVoxelChunks(world));
}

void TestUpdateVoxelInteractionInspectModeKeepsWorldAndCapturesChunkInfo(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Glass, nullptr);
	ResetDirtyFlags(world);
	MarkVoxelChunkDirty(world, {1, 1, 1});

	InputState input{};
	input.removePressed = true;
	InteractionState interaction{};
	interaction.editorTool = DebugEditorTool::Inspect;

	UpdateVoxelInteraction(
		MakeTestCamera({1.5f, 1.5f, 4.5f}),
		&input,
		&world,
		&interaction,
		true,
		nullptr);

	EXPECT_EQ(context, VoxelMaterial::Glass, GetVoxelMaterial(world, {1, 1, 1}));
	EXPECT_TRUE(context, interaction.selection.hasHit);
	EXPECT_TRUE(context, interaction.selection.hasTargetChunk);
	EXPECT_TRUE(context, interaction.selection.hasPlacementVoxel);
	EXPECT_TRUE(context, interaction.selection.hasPlacementChunk);
	EXPECT_TRUE(context, interaction.selection.targetSolid);
	EXPECT_INT3_EQ(context, (Int3{1, 1, 1}), interaction.selection.targetVoxelInChunk);
	EXPECT_INT3_EQ(context, (Int3{1, 1, 2}), interaction.selection.placementVoxelInChunk);
	EXPECT_INT3_EQ(context, (Int3{0, 0, 0}), interaction.selection.targetChunkCoord);
	EXPECT_INT3_EQ(context, (Int3{0, 0, 0}), interaction.selection.targetChunkMin);
	EXPECT_INT3_EQ(context, (Int3{4, 4, 4}), interaction.selection.targetChunkMaxExclusive);
	EXPECT_INT3_EQ(context, (Int3{0, 0, 0}), interaction.selection.placementChunkCoord);
	EXPECT_INT3_EQ(context, (Int3{0, 0, 0}), interaction.selection.placementChunkMin);
	EXPECT_INT3_EQ(context, (Int3{4, 4, 4}), interaction.selection.placementChunkMaxExclusive);
	EXPECT_TRUE(context, interaction.selection.targetChunkDirty);
	EXPECT_TRUE(context, interaction.selection.targetChunkActive);
	EXPECT_TRUE(context, interaction.selection.placementChunkDirty);
	EXPECT_TRUE(context, interaction.selection.placementChunkActive);
	EXPECT_EQ(context, static_cast<uint32_t>(0), interaction.selection.targetChunkIndex);
	EXPECT_EQ(context, static_cast<uint32_t>(0), interaction.selection.placementChunkIndex);
	EXPECT_EQ(context, static_cast<uint32_t>(1), interaction.selection.targetChunkNonAirVoxelCount);
	EXPECT_EQ(context, static_cast<uint32_t>(1), interaction.selection.placementChunkNonAirVoxelCount);
	EXPECT_TRUE(context, !input.removePressed);
	EXPECT_EQ(context, static_cast<uint32_t>(1), CountDirtyVoxelChunks(world));
}

void TestUpdateVoxelInteractionPickTargetMaterialCopiesSelectedVoxelMaterial(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Glass, nullptr);
	ResetDirtyFlags(world);

	InputState input{};
	InitializeInputState(input);
	PressInputAction(input, InputAction::PickTargetMaterial);

	InteractionState interaction{};
	interaction.editorTool = DebugEditorTool::Inspect;
	interaction.placementMaterial = VoxelMaterial::FloorWhite;

	UpdateVoxelInteraction(
		MakeTestCamera({1.5f, 1.5f, 4.5f}),
		&input,
		&world,
		&interaction,
		false,
		nullptr);

	EXPECT_EQ(context, VoxelMaterial::Glass, interaction.placementMaterial);
	EXPECT_TRUE(context, interaction.selection.hasHit);
	EXPECT_EQ(context, VoxelMaterial::Glass, interaction.selection.targetMaterial);
	EXPECT_EQ(context, static_cast<uint32_t>(0), CountDirtyVoxelChunks(world));
}

void TestUpdateVoxelInteractionPaintModeAnchoredPlacementFillsVoxelBox(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Glass, nullptr);
	SetVoxelMaterial(world, {3, 1, 1}, VoxelMaterial::Glass, nullptr);
	ResetDirtyFlags(world);

	InteractionState interaction{};
	interaction.editorTool = DebugEditorTool::Paint;
	interaction.placementMaterial = VoxelMaterial::FloorGray;

	InputState anchorInput{};
	InitializeInputState(anchorInput);
	PressInputAction(anchorInput, InputAction::ToggleMutationAnchor);
	UpdateVoxelInteraction(
		MakeTestCamera({1.5f, 1.5f, 4.5f}),
		&anchorInput,
		&world,
		&interaction,
		true,
		nullptr);

	EXPECT_TRUE(context, interaction.mutationAnchorValid);
	EXPECT_TRUE(context, interaction.mutationAnchorUsesPlacementVoxel);
	EXPECT_INT3_EQ(context, (Int3{1, 1, 2}), interaction.mutationAnchorVoxel);

	InputState paintInput{};
	InitializeInputState(paintInput);
	paintInput.placePressed = true;
	UpdateVoxelInteraction(
		MakeTestCamera({3.5f, 1.5f, 4.5f}),
		&paintInput,
		&world,
		&interaction,
		true,
		nullptr);

	EXPECT_EQ(context, VoxelMaterial::FloorGray, GetVoxelMaterial(world, {1, 1, 2}));
	EXPECT_EQ(context, VoxelMaterial::FloorGray, GetVoxelMaterial(world, {2, 1, 2}));
	EXPECT_EQ(context, VoxelMaterial::FloorGray, GetVoxelMaterial(world, {3, 1, 2}));
	EXPECT_TRUE(context, interaction.selection.hasHit);
	EXPECT_EQ(context, VoxelMaterial::FloorGray, interaction.selection.targetMaterial);
	EXPECT_EQ(context, static_cast<uint32_t>(2), CountDirtyVoxelChunks(world));
}

void TestUpdateVoxelInteractionEraseModeAnchoredRemovalClearsVoxelBox(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Glass, nullptr);
	SetVoxelMaterial(world, {2, 1, 1}, VoxelMaterial::Glass, nullptr);
	SetVoxelMaterial(world, {3, 1, 1}, VoxelMaterial::Glass, nullptr);
	ResetDirtyFlags(world);

	InteractionState interaction{};
	interaction.editorTool = DebugEditorTool::Erase;

	InputState anchorInput{};
	InitializeInputState(anchorInput);
	PressInputAction(anchorInput, InputAction::ToggleMutationAnchor);
	UpdateVoxelInteraction(
		MakeTestCamera({1.5f, 1.5f, 4.5f}),
		&anchorInput,
		&world,
		&interaction,
		true,
		nullptr);

	EXPECT_TRUE(context, interaction.mutationAnchorValid);
	EXPECT_TRUE(context, !interaction.mutationAnchorUsesPlacementVoxel);
	EXPECT_INT3_EQ(context, (Int3{1, 1, 1}), interaction.mutationAnchorVoxel);

	InputState eraseInput{};
	InitializeInputState(eraseInput);
	eraseInput.placePressed = true;
	UpdateVoxelInteraction(
		MakeTestCamera({3.5f, 1.5f, 4.5f}),
		&eraseInput,
		&world,
		&interaction,
		true,
		nullptr);

	EXPECT_EQ(context, VoxelMaterial::Air, GetVoxelMaterial(world, {1, 1, 1}));
	EXPECT_EQ(context, VoxelMaterial::Air, GetVoxelMaterial(world, {2, 1, 1}));
	EXPECT_EQ(context, VoxelMaterial::Air, GetVoxelMaterial(world, {3, 1, 1}));
	EXPECT_TRUE(context, !interaction.selection.hasHit);
	EXPECT_EQ(context, static_cast<uint32_t>(2), CountDirtyVoxelChunks(world));
}

void TestBuildDebugOverlayBoxesReflectsSelectionAndChunkToggles(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Glass, nullptr);
	ResetDirtyFlags(world);
	MarkVoxelChunkDirty(world, {1, 1, 1});

	InteractionState interaction{};
	interaction.editorTool = DebugEditorTool::Inspect;
	interaction.selection.hasHit = true;
	interaction.selection.targetVoxel = {1, 1, 1};
	interaction.selection.hasPlacementVoxel = true;
	interaction.selection.placementVoxel = {1, 1, 2};
	interaction.selection.hasTargetChunk = true;
	interaction.selection.targetChunkCoord = {0, 0, 0};
	interaction.selection.targetChunkMin = {0, 0, 0};
	interaction.selection.targetChunkMaxExclusive = {4, 4, 4};
	interaction.selection.targetChunkDirty = true;
	interaction.selection.targetChunkActive = true;
	interaction.mutationAnchorValid = true;
	interaction.mutationAnchorUsesPlacementVoxel = true;
	interaction.mutationAnchorVoxel = {3, 1, 2};

	DebugState debug{};
	debug.hudVisible = true;
	debug.detailedHudVisible = true;
	debug.showChunkBounds = true;
	debug.showDirtyChunkOverlay = true;

	std::vector<DebugOverlayBox> boxes;
	BuildDebugOverlayBoxes(&world, interaction, debug, &boxes);

	EXPECT_EQ(context, static_cast<size_t>(14), boxes.size());
	if (boxes.size() >= 14) {
		EXPECT_INT3_EQ(context, (Int3{0, 0, 0}), boxes.front().min);
		EXPECT_INT3_EQ(context, (Int3{4, 4, 4}), boxes.front().maxExclusive);
		EXPECT_INT3_EQ(context, (Int3{1, 1, 1}), boxes[9].min);
		EXPECT_INT3_EQ(context, (Int3{2, 2, 2}), boxes[9].maxExclusive);
		EXPECT_INT3_EQ(context, (Int3{1, 1, 2}), boxes[10].min);
		EXPECT_INT3_EQ(context, (Int3{2, 2, 3}), boxes[10].maxExclusive);
		EXPECT_INT3_EQ(context, (Int3{3, 1, 2}), boxes[11].min);
		EXPECT_INT3_EQ(context, (Int3{4, 2, 3}), boxes[11].maxExclusive);
		EXPECT_INT3_EQ(context, (Int3{1, 1, 2}), boxes[12].min);
		EXPECT_INT3_EQ(context, (Int3{4, 2, 3}), boxes[12].maxExclusive);
		EXPECT_INT3_EQ(context, (Int3{0, 0, 0}), boxes[13].min);
		EXPECT_INT3_EQ(context, (Int3{4, 4, 4}), boxes[13].maxExclusive);
	}
}

void TestBuildDebugOverlayBoxesHidesDetailedSelectionOverlaysInBasicHud(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Glass, nullptr);
	ResetDirtyFlags(world);
	MarkVoxelChunkDirty(world, {1, 1, 1});

	InteractionState interaction{};
	interaction.editorTool = DebugEditorTool::Inspect;
	interaction.selection.hasHit = true;
	interaction.selection.targetVoxel = {1, 1, 1};
	interaction.selection.hasPlacementVoxel = true;
	interaction.selection.placementVoxel = {1, 1, 2};
	interaction.selection.hasTargetChunk = true;
	interaction.selection.targetChunkMin = {0, 0, 0};
	interaction.selection.targetChunkMaxExclusive = {4, 4, 4};
	interaction.mutationAnchorValid = true;
	interaction.mutationAnchorUsesPlacementVoxel = true;
	interaction.mutationAnchorVoxel = {3, 1, 2};

	DebugState debug{};
	debug.hudVisible = true;
	debug.detailedHudVisible = false;
	debug.showChunkBounds = true;
	debug.showDirtyChunkOverlay = true;

	std::vector<DebugOverlayBox> boxes;
	BuildDebugOverlayBoxes(&world, interaction, debug, &boxes);

	EXPECT_EQ(context, static_cast<size_t>(10), boxes.size());
	if (boxes.size() >= 10) {
		EXPECT_INT3_EQ(context, (Int3{1, 1, 1}), boxes[9].min);
		EXPECT_INT3_EQ(context, (Int3{2, 2, 2}), boxes[9].maxExclusive);
	}
}

void TestHudUiStateDefaultsAndOpenSettingsBinding(TestContext &context)
{
	DebugState debug{};
	EXPECT_TRUE(context, debug.hudVisible);
	EXPECT_TRUE(context, !debug.settingsOpen);
	EXPECT_TRUE(context, !debug.statsOpen);

	InputState input{};
	InitializeInputState(input);
	EXPECT_EQ(context, SDL_SCANCODE_F1, input.bindings[static_cast<size_t>(InputAction::ToggleHud)].scancodes[0]);
	EXPECT_EQ(
		context,
		SDL_SCANCODE_GRAVE,
		input.bindings[static_cast<size_t>(InputAction::OpenHudSettings)].scancodes[0]);
	EXPECT_EQ(
		context,
		SDL_SCANCODE_UNKNOWN,
		input.bindings[static_cast<size_t>(InputAction::CycleMsaaMode)].scancodes[0]);
}

void TestInitializeAppEcsCreatesPrimaryCameraPlayerAndSingletons(TestContext &context)
{
	AppState state{};

	EXPECT_TRUE(context, InitializeAppEcs(&state));
	EXPECT_TRUE(context, state.ecs() != nullptr);
	EXPECT_TRUE(context, GetPrimaryCameraEntityId(state.ecs().get()) != 0u);
	EXPECT_TRUE(context, GetPrimaryPlayerEntityId(state.ecs().get()) != 0u);
	EXPECT_EQ(context, GetPrimaryCameraEntityId(state.ecs().get()), GetPlayerControlledCameraEntityId(state.ecs().get()));

	CameraState *camera = GetPrimaryCameraState(state.ecs().get());
	const DebugState *debug = GetDebugState(state.ecs().get());
	const WorldState *world = GetWorldState(state.ecs().get());

	EXPECT_TRUE(context, camera != nullptr);
	EXPECT_TRUE(context, debug != nullptr);
	EXPECT_TRUE(context, world == &state.world());

	camera->moveSpeed = 17.0f;
	EXPECT_NEAR(context, 17.0f, GetPrimaryCameraState(state.ecs().get())->moveSpeed);
	EXPECT_TRUE(context, debug->hudVisible);
}

void TestSyncEcsWorldStateMirrorsVoxelChunksAndWorldSummary(TestContext &context)
{
	AppState state{};
	EXPECT_TRUE(context, InitializeAppEcs(&state));

	state.world().voxelWorld = std::make_unique<VoxelWorld>(MakeTestWorld({0, 0, 0}, {16, 8, 16}, 8));
	SetVoxelMaterial(*state.world().voxelWorld, {1, 1, 1}, VoxelMaterial::Glass);
	SetVoxelMaterial(*state.world().voxelWorld, {9, 1, 1}, VoxelMaterial::Fluid);

	DebugState *debug = GetDebugState(state.ecs().get());
	EXPECT_TRUE(context, debug != nullptr);
	debug->stats.dirtyChunkCount = 99;
	debug->stats.activeChunkCount = 99;
	debug->stats.nonAirVoxelCount = 99;

	EXPECT_TRUE(context, SyncEcsWorldState(state.ecs().get()));

	VoxelWorldStats summary{};
	size_t chunkEntityCount = 0;
	EXPECT_TRUE(context, GetEcsWorldChunkSummary(state.ecs().get(), &summary, &chunkEntityCount));

	EXPECT_EQ(context, state.world().voxelWorld->chunks.size(), chunkEntityCount);
	EXPECT_EQ(context, state.world().voxelWorld->stats.dirtyChunkCount, summary.dirtyChunkCount);
	EXPECT_EQ(context, state.world().voxelWorld->stats.activeChunkCount, summary.activeChunkCount);
	EXPECT_EQ(context, state.world().voxelWorld->stats.nonAirVoxelCount, summary.nonAirVoxelCount);
	EXPECT_EQ(context, summary.dirtyChunkCount, debug->stats.dirtyChunkCount);
	EXPECT_EQ(context, summary.activeChunkCount, debug->stats.activeChunkCount);
	EXPECT_EQ(context, summary.nonAirVoxelCount, debug->stats.nonAirVoxelCount);
}

void TestVoxelChunkStaticPromotion(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	const uint32_t totalChunks = static_cast<uint32_t>(world.chunks.size());
	EXPECT_TRUE(context, totalChunks > 1u);
	EXPECT_EQ(context, 0u, CountStaticVoxelChunks(world));

	for (uint32_t tick = 0; tick < 59u; ++tick) {
		TickVoxelChunkStaticPromotion(world, 60u);
		EXPECT_EQ(context, 0u, CountStaticVoxelChunks(world));
	}

	TickVoxelChunkStaticPromotion(world, 60u);
	EXPECT_EQ(context, totalChunks, CountStaticVoxelChunks(world));

	for (uint32_t tick = 0; tick < 30u; ++tick) {
		TickVoxelChunkStaticPromotion(world, 60u);
	}
	EXPECT_EQ(context, totalChunks, CountStaticVoxelChunks(world));

	SetVoxelMaterial(world, {0, 0, 0}, VoxelMaterial::FloorWhite, nullptr);
	const uint32_t staticAfterEdit = CountStaticVoxelChunks(world);
	EXPECT_TRUE(context, staticAfterEdit < totalChunks);
}

void TestVoxelChunkStaticPromotionThresholdFromEnv(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	const uint32_t defaultThreshold = GetVoxelChunkStaticPromotionThreshold();
	EXPECT_TRUE(context, defaultThreshold > 0u);

	for (uint32_t tick = 0; tick < defaultThreshold; ++tick) {
		TickVoxelChunkStaticPromotion(world, defaultThreshold);
	}
	EXPECT_EQ(context, static_cast<uint32_t>(world.chunks.size()), CountStaticVoxelChunks(world));
}

void TestIncrementalJoltPerChunkBodyMap(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	const auto physics = std::unique_ptr<PhysicsState, decltype(&DestroyPhysicsState)>(
		CreatePhysicsState(), &DestroyPhysicsState);

	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::FloorWhite, nullptr);
	QueueChunkRebuildRequest(physics.get(), 0);
	EXPECT_EQ(context, 1u, GetPendingChunkRebuildCount(physics.get()));
	const uint32_t rebuilt = ProcessChunkRebuildQueue(physics.get(), &world);
	EXPECT_EQ(context, 1u, rebuilt);
	EXPECT_EQ(context, 0u, GetPendingChunkRebuildCount(physics.get()));
	EXPECT_EQ(context, 1u, GetChunkBodyCount(physics.get()));

	QueueChunkRebuildRequest(physics.get(), 0);
	QueueChunkRebuildRequest(physics.get(), 0);
	EXPECT_EQ(context, 2u, GetPendingChunkRebuildCount(physics.get()));
	const uint32_t rebuiltDedup = ProcessChunkRebuildQueue(physics.get(), &world);
	EXPECT_EQ(context, 1u, rebuiltDedup);
	EXPECT_EQ(context, 1u, GetChunkBodyCount(physics.get()));

	SetVoxelMaterial(world, {1, 1, 1}, VoxelMaterial::Air, nullptr);
	QueueChunkRebuildRequest(physics.get(), 0);
	ProcessChunkRebuildQueue(physics.get(), &world);
	EXPECT_EQ(context, 0u, GetChunkBodyCount(physics.get()));
}

void TestLodLevelSelectionAndAssignment(TestContext &context)
{
	EXPECT_EQ(context, static_cast<uint8_t>(0), SelectLodLevelForDistance(0.0f));
	EXPECT_EQ(context, static_cast<uint8_t>(0), SelectLodLevelForDistance(31.9f));
	EXPECT_EQ(context, static_cast<uint8_t>(1), SelectLodLevelForDistance(32.0f));
	EXPECT_EQ(context, static_cast<uint8_t>(1), SelectLodLevelForDistance(63.9f));
	EXPECT_EQ(context, static_cast<uint8_t>(2), SelectLodLevelForDistance(64.0f));
	EXPECT_EQ(context, static_cast<uint8_t>(2), SelectLodLevelForDistance(127.9f));
	EXPECT_EQ(context, static_cast<uint8_t>(3), SelectLodLevelForDistance(128.0f));
	EXPECT_EQ(context, static_cast<uint8_t>(3), SelectLodLevelForDistance(500.0f));

	VoxelWorld world = MakeTestWorld({0, 0, 0}, {16, 8, 16}, 8);
	const uint32_t totalChunks = static_cast<uint32_t>(world.chunks.size());
	AssignLodLevels(world, 4.0f, 4.0f, 4.0f);
	const uint32_t nearCount = CountChunksAtLod(world, 0);
	EXPECT_TRUE(context, nearCount >= 1u);
	EXPECT_TRUE(context, nearCount <= totalChunks);

	const uint32_t totalAssigned = CountChunksAtLod(world, 0) + CountChunksAtLod(world, 1) + CountChunksAtLod(world, 2) + CountChunksAtLod(world, 3);
	EXPECT_EQ(context, totalChunks, totalAssigned);
}

void TestFluidCaGpuEnvToggleDefaultsOff(TestContext &context)
{
	ToggleFluidCaGpuEnabledForTesting(false);
	EXPECT_TRUE(context, !IsFluidCaGpuEnabled());
	ToggleFluidCaGpuEnabledForTesting(true);
	EXPECT_TRUE(context, IsFluidCaGpuEnabled());
	ToggleFluidCaGpuEnabledForTesting(false);
}

void TestBuildActiveChunkIdsForFluidCa(TestContext &context)
{
	VoxelWorld world = MakeTestWorld({0, 0, 0}, {8, 8, 8}, 4);
	EXPECT_TRUE(context, BuildActiveChunkIdsForFluidCa(world).empty());

	SetVoxelMaterial(world, {0, 0, 0}, VoxelMaterial::Fluid, nullptr);
	const auto one = BuildActiveChunkIdsForFluidCa(world);
	EXPECT_EQ(context, static_cast<size_t>(1), one.size());
	EXPECT_EQ(context, static_cast<uint32_t>(0), one[0]);

	SetVoxelMaterial(world, {5, 5, 5}, VoxelMaterial::FloorWhite, nullptr);
	const auto two = BuildActiveChunkIdsForFluidCa(world);
	EXPECT_EQ(context, static_cast<size_t>(2), two.size());
}

} // namespace

int main() // NOLINT(*-exception-escape)
{
	if (const int replayAnalysisExitCode = RunReplayAnalysisFromEnvironment();
		replayAnalysisExitCode >= 0) {
		return replayAnalysisExitCode;
	}

	TestContext context{};

	TestWorldBoundsAndChunkIndexing(context);
	TestVoxelScenePresetParsingAcceptsCanonicalAndFlexibleNames(context);
	TestCreateVoxelSceneWorldBuildsExpectedBaselineScenes(context);
	TestCreateVoxelSceneWorldReadsEnvironmentPreset(context);
	TestVoxelWorldSnapshotPathUsesEnvironmentOverride(context);
	TestScreenshotCapturePathUsesEnvironmentOverride(context);
	TestSaveScreenshotCaptureBmpWritesExpectedBmp(context);
	TestSaveScreenshotCaptureMetadataWritesLookDevState(context);
	TestVoxelFaceAmbientVisibilityStaysOpenForExposedTopFace(context);
	TestVoxelFaceAmbientVisibilityDarkensEnclosedTopFace(context);
	TestVoxelFaceAmbientVisibilityTreatsGlassAsNonOccluder(context);
	TestVoxelChunkStaticPromotion(context);
	TestVoxelChunkStaticPromotionThresholdFromEnv(context);
	TestIncrementalJoltPerChunkBodyMap(context);
	TestLodLevelSelectionAndAssignment(context);
	TestFluidCaGpuEnvToggleDefaultsOff(context);
	TestBuildActiveChunkIdsForFluidCa(context);
	TestVoxelWorldSnapshotRoundTripsWorldState(context);
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
	TestVoxelSceneLightingPresetsProvideDistinctLooks(context);
	TestBuildVoxelSceneLightingAppliesLookDevControls(context);
	TestLightingDebugViewCycleIncludesShadow(context);
	TestStartupCameraOverrideReadsEnvironment(context);
	TestLookDevCaptureAutomationRequestsConfiguredViews(context);
	TestShadowTuningTargetCycleAndLabels(context);
	TestBuildSunShadowProjectionFitsSceneBounds(context);
	TestBuildSunShadowProjectionUsesActiveChunkBoundsInsteadOfEmptyPadding(context);
	TestBuildSunShadowProjectionInterpretsSunDirectionAsTowardsSun(context);
	TestBuildSunShadowCascadeSplitsUsesStablePracticalSplitScheme(context);
	TestBuildSunShadowCascadeSplitsHonorsUniformAndLogarithmicLimits(context);
	TestBuildSunShadowCascadeSplitsClampsInvalidInputs(context);
	TestBuildSunShadowCascadeProjectionsFitEachViewDepthSlice(context);
	TestBuildSunShadowCascadeProjectionsSnapToShadowTexelGrid(context);
	TestBuildSunShadowCascadeProjectionsUseCascadeSpecificCasterCoverage(context);
	TestBuildSunShadowCascadeProjectionsExpandOrthoExtentForTallCasters(context);
	TestBuildSunShadowCascadeProjectionsKeepExpandedCastersAheadOfNearPlane(context);
	TestInputActionBindingsTrackPressedAndReleasedKeys(context);
	TestConsumeCameraLookInputAllowsNearVerticalPitch(context);
	TestTickCameraUsesActionStateAndSpeedModifiers(context);
	TestHandleCameraEventIgnoresLookInputWithoutRelativeMouseMode(context);
	TestGetCameraVisibleSceneMaxDistanceClampsToMainlineRange(context);
	TestSceneChunkVisibilityUsesFrustumAndDistanceCulling(context);
	TestSceneChunkVisibilityKeepsChunksVisibleAtFrustumEdges(context);
	TestMakeUploadedSceneChunkDescriptorPreservesGeneratedFaceCounts(context);
	TestSceneVoxelPayloadSyncPreservesMissedDirtyChunks(context);
	TestUpdateAppConsumesDebugInputActions(context);
	TestUpdateAppUsesVisibleSceneDistanceForSunShadowCascadeSplits(context);
	TestUpdateAppTogglesWalkAirControlMode(context);
	TestUpdateAppTogglesWalkAutoJump(context);
	TestInputReplayCaptureRoundTripsFile(context);
	TestInputReplayCanDriveWalkSequence(context);
	TestPlacementMaterialCycleCoversAllDebugMaterials(context);
	TestResetCameraPreservesControlMode(context);
	TestCreativeModeBlocksPausedMovement(context);
	TestSpectatorModeAllowsPausedMovementButBlocksEdits(context);
	TestPhysicsRaycastHitsStaticVoxelCollision(context);
	TestPhysicsWorldSyncTracksVoxelEdits(context);
	TestUpdateAppCyclesCreativeSpectatorAndWalkModes(context);
	TestUpdateAppDoubleSpaceTogglesCreativeAndWalk(context);
	TestWalkCharacterCollidesWithVoxelWall(context);
	TestGetPhysicsWalkDebugInfoReportsGroundedSupport(context);
	TestVoxelLabWalkDebugInfoMatchesLiveCenterReference(context);
	TestVoxelLabWalkJumpFromSideEdgeDoesNotImmediatelyDrop(context);
	TestVoxelLabWalkJumpFromCornerEdgeDoesNotImmediatelyDrop(context);
	TestVoxelLabWalkJumpReapproachDoesNotMagnetSnapBackToSameTopPlane(context);
	TestVoxelLabWalkFreeFallDoesNotSnapToTopPlaneTooEarly(context);
	TestVoxelLabWalkJumpInPlaceDoesNotMagnetSnapToCenterTopPlane(context);
	TestVoxelLabWalkJumpFromSideEdgeLandsBackOnTopPlane(context);
	TestVoxelLabWalkJumpFromCornerEdgeLandsBackOnTopPlane(context);
	TestVoxelLabWalkSneakJumpFromSideEdgeLandsBackOnTopPlane(context);
	TestVoxelLabWalkJumpFromSideEdgeCanMoveAfterLanding(context);
	TestVoxelLabWalkJumpFromExactSideEdgeWithHeldWDoesNotLoseSupportOnLanding(context);
	TestVoxelLabWalkSneakJumpFromSideEdgeStillHoldsEdgeAfterLanding(context);
	TestVoxelLabWalkSneakJumpInPlaceFromExactSideEdgeStaysGrounded(context);
	TestVoxelLabWalkSneakJumpInPlaceFromExactCornerEdgeStaysGrounded(context);
	TestUpdateAppVoxelLabSneakJumpInPlaceFromExactSideEdgeStaysGroundedAtHighRenderRate(context);
	TestUpdateAppVoxelLabSneakJumpInPlaceFromExactCornerEdgeStaysGroundedAtHighRenderRate(context);
	TestWalkCharacterFallsSmoothlyAfterLeavingLedge(context);
	TestWalkCharacterDoesNotPassivelySlideOffLedge(context);
	TestWalkCharacterDoesNotSlideFromNarrowEdgeBand(context);
	TestWalkCharacterCanJumpFromNarrowEdgeBandWithoutSneak(context);
	TestWalkCharacterCanJumpWhileMovingAcrossNarrowEdgeBandWithoutSneak(context);
	TestWalkCharacterCanJumpWhileBoostingAlongStraightEdgeWithoutSneak(context);
	TestWalkCharacterJumpHeightMatchesMinecraftLikeApex(context);
	TestWalkCharacterHoldingJumpRepeatsOnLanding(context);
	TestWalkCharacterJumpInPlaceIgnoresAirborneMoveInput(context);
	TestWalkCharacterAirborneInputDoesNotBendJumpTrajectory(context);
	TestWalkCharacterMinecraftAirControlDefaultUsesLateWASDSteering(context);
	TestWalkCharacterSneakLowersCameraHeight(context);
	TestWalkCharacterSneakPreventsLeavingLedge(context);
	TestWalkCharacterSneakCanStrafeAlongLedge(context);
	TestWalkCharacterSneakCanMoveDiagonallyAlongLedge(context);
	TestWalkCharacterSneakHoldsDiagonalEdgeOverTime(context);
	TestWalkCharacterSneakHoldsStraightEdgeOverTime(context);
	TestWalkCharacterReleasingSneakDoesNotImmediatelyDropFromLedge(context);
	TestVoxelLabReleasingSneakOnExactSideEdgeDoesNotDrop(context);
	TestVoxelLabReleaseSneakThenJumpInPlaceFromExactSideEdgeLandsAndHolds(context);
	TestWalkCharacterReleasingSneakThenJumpingFromStraightLedgeKeepsTakeoff(context);
	TestWalkCharacterReleasingSneakThenJumpingFromCornerLedgeKeepsTakeoff(context);
	TestWalkCharacterJumpingInPlaceAtCornerKeepsTopFaceOwnership(context);
	TestWalkCharacterSneakJumpUnderLowCeilingKeepsSupport(context);
	TestWalkCharacterSneakJumpFromStraightEdgeWithOutwardInputFallsOffEdge(context);
	TestWalkCharacterSneakJumpFromStraightEdgeWithAlongAndOutwardInputLeavesEdge(context);
	TestWalkCharacterSneakJumpIntoCeilingCornerEdgeCanStillLeaveEdge(context);
	TestWalkCharacterSneakCanMoveAlongCornerZEdge(context);
	TestWalkCharacterSneakCanMoveAlongCornerXEdge(context);
	TestWalkCharacterSneakCanReachCornerLedge(context);
	TestWalkCharacterSneakDoesNotIdleSlideDownFromCorner(context);
	TestWalkCharacterLedgeJumpWithoutTakeoffMomentumIgnoresLateForwardInput(context);
	TestWalkCharacterOffsetLedgeJumpWithoutTakeoffMomentumIgnoresLateForwardInput(context);
	TestWalkCharacterJumpApproachDoesNotTeleportAcrossOffsets(context);
	TestWalkCharacterCanJumpOntoNegativeSingleBlockEdge(context);
	TestWalkCharacterCanJumpOntoPositiveSingleBlockEdge(context);
	TestWalkCharacterClimbsOneBlockWithoutSlowWallSlide(context);
	TestWalkCharacterSneakJumpAgainstGlassColumnDoesNotSlowWallSlide(context);
	TestWalkCharacterJumpAgainstGlassColumnDoesNotWallStepUp(context);
	TestWalkCharacterHeldJumpAgainstGlassColumnDoesNotGetSecondJumpFromWallTop(context);
	TestWalkCharacterMidairSneakDoesNotAcquireTwoBlockWallTop(context);
	TestWalkCharacterCannotDoubleJumpWhileAirborneNearWall(context);
	TestWalkCharacterHeldJumpDoesNotAcquireForeignWallTopNearTwoBlockWall(context);
	TestWalkCharacterFallsAfterEditedSupportIsRemoved(context);
	TestWalkCharacterAutoJumpToggleControlsOneBlockTraversal(context);
	TestWalkCharacterCanJumpOntoCornerSideFromPerpendicularApproach(context);
	TestWalkCharacterSneakCanMoveAlongNegativeSingleBlockEdge(context);
	TestWalkCharacterSneakCanMoveAlongIsolatedCornerEdge(context);
	TestWalkCharacterDoesNotMagnetSnapDuringFreeFall(context);
	TestGetNextVoxelScenePresetCyclesAllBuiltinPresets(context);
	TestUpdateAppRequestsScenePresetReload(context);
	TestUpdateAppRequestsWorldSnapshotSave(context);
	TestUpdateAppRequestsWorldSnapshotLoad(context);
	TestUpdateAppRequestsScreenshotCapture(context);
	TestUpdateAppAdjustsShadowTuningControls(context);
	TestVoxelRaycastHitsSolidVoxelAndReturnsPlacementCell(context);
	TestVoxelRaycastStopsAtWorldBoundaryWithoutPlacementCell(context);
	TestVoxelRaycastMissesWhenNoSolidVoxelIsReached(context);
	TestUpdateVoxelInteractionRemovesTargetedBlock(context);
	TestUpdateVoxelInteractionPlacesConfiguredBlock(context);
	TestUpdateVoxelInteractionRejectsPlacementInsidePhysicsCharacter(context);
	TestUpdateVoxelInteractionSkipsEditingWhenDisabled(context);
	TestUpdateVoxelInteractionPaintModePaintsTargetedBlock(context);
	TestUpdateVoxelInteractionEraseModeRemovesTargetedBlock(context);
	TestUpdateVoxelInteractionFillModeFloodFillsConnectedRegion(context);
	TestUpdateVoxelInteractionInspectModeKeepsWorldAndCapturesChunkInfo(context);
	TestUpdateVoxelInteractionPickTargetMaterialCopiesSelectedVoxelMaterial(context);
	TestUpdateVoxelInteractionPaintModeAnchoredPlacementFillsVoxelBox(context);
	TestUpdateVoxelInteractionEraseModeAnchoredRemovalClearsVoxelBox(context);
	TestBuildDebugOverlayBoxesReflectsSelectionAndChunkToggles(context);
	TestBuildDebugOverlayBoxesHidesDetailedSelectionOverlaysInBasicHud(context);
	TestHudUiStateDefaultsAndOpenSettingsBinding(context);
	TestInitializeAppEcsCreatesPrimaryCameraPlayerAndSingletons(context);
	TestSyncEcsWorldStateMirrorsVoxelChunksAndWorldSummary(context);

	if (context.failures != 0) {
		return EXIT_FAILURE;
	}

	std::puts("ProjectVTests passed");
	return EXIT_SUCCESS;
}

#include "core/Math.hpp"
#include "core/StringId.hpp"
#include "voxel/VoxelWorldInternal.hpp"

#include "SDL3/SDL_log.h"
#include "core/RuntimeDiagnostics.hpp"
#include "core/Types.hpp"
#include "fmt/format.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>

namespace {
char NormalizePresetCharacter(const char character)
{
	if (character == '_' || character == '-' || character == ' ') {
		return '\0';
	}

	return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
}

bool MatchesPresetName(const std::string_view text, const std::string_view canonical)
{
	size_t textIndex = 0;
	size_t canonicalIndex = 0;
	while (true) {
		while (textIndex < text.size() && NormalizePresetCharacter(text[textIndex]) == '\0') {
			++textIndex;
		}
		while (canonicalIndex < canonical.size() && NormalizePresetCharacter(canonical[canonicalIndex]) == '\0') {
			++canonicalIndex;
		}

		const char normalizedText =
			textIndex < text.size() ? NormalizePresetCharacter(text[textIndex]) : '\0';
		const char normalizedCanonical =
			canonicalIndex < canonical.size() ? NormalizePresetCharacter(canonical[canonicalIndex]) : '\0';
		if (normalizedText != normalizedCanonical) {
			return false;
		}
		if (normalizedText == '\0') {
			return true;
		}

		++textIndex;
		++canonicalIndex;
	}
}

int ResolveWorldTopY(const VoxelWorldConfig &config)
{
	const int configuredTopY = std::max(config.worldTopY, config.floorY + 1);
	return configuredTopY;
}

int GetHalfFloorSize(const VoxelWorldConfig &config)
{
	return config.floorSize / 2;
}

VoxelLabShellConfig GetVoxelLabShellConfig()
{
	return {};
}

VoxelWorldConfig GetVoxelLabWorldConfig()
{
	const VoxelLabShellConfig shellConfig = GetVoxelLabShellConfig();
	VoxelWorldConfig config{};
	config.worldTopY = std::max(config.worldTopY, shellConfig.center.y + shellConfig.radius);
	return config;
}

void BuildCheckerboardFloor(VoxelWorld &world, const VoxelWorldConfig &config)
{
	const int halfFloor = GetHalfFloorSize(config);
	for (int z = -halfFloor; z < halfFloor; ++z) {
		for (int x = -halfFloor; x < halfFloor; ++x) {
			const VoxelMaterial material = (x + z & 1) == 0 ? VoxelMaterial::FloorWhite : VoxelMaterial::FloorGray;
			SetVoxelMaterial(world, {x, config.floorY, z}, material, nullptr);
		}
	}
}

void BuildVoxelLabShellAndFluid(VoxelWorld &world, const VoxelLabShellConfig &config)
{
	PV_ASSERT(config.radius > 0, "VoxelWorld", "BuildVoxelLabShellAndFluid", "VoxelLab shell radius must stay positive");

	const int outerRadiusSquared = config.radius * config.radius;
	const int innerRadius = std::max(config.radius - std::max(config.shellThickness, 0), 0);
	const int innerRadiusSquared = innerRadius * innerRadius;
	const int fluidTop = config.center.y - innerRadius +
						 static_cast<int>(std::round(2.0f * static_cast<float>(innerRadius) * config.fluidFillLevel));

	for (int dz = -config.radius; dz <= config.radius; ++dz) {
		for (int dy = -config.radius; dy <= config.radius; ++dy) {
			for (int dx = -config.radius; dx <= config.radius; ++dx) {
				const int distanceSquared = dx * dx + dy * dy + dz * dz;
				if (distanceSquared > outerRadiusSquared) {
					continue;
				}

				const Int3 position{
					config.center.x + dx,
					config.center.y + dy,
					config.center.z + dz,
				};

				if (distanceSquared > innerRadiusSquared) {
					SetVoxelMaterial(world, position, VoxelMaterial::Glass, nullptr);
					continue;
				}

				if (innerRadius > 0 && position.y <= fluidTop) {
					SetVoxelMaterial(world, position, VoxelMaterial::Fluid, nullptr);
				}
			}
		}
	}
}

void BuildVoxelLabOpaqueAnchors(VoxelWorld &world, const VoxelWorldConfig &config)
{
	const int baseY = config.floorY + 1;
	for (int z = 4; z <= 6; ++z) {
		for (int x = 5; x <= 8; ++x) {
			SetVoxelMaterial(world, {x, baseY, z}, VoxelMaterial::FloorGray, nullptr);
		}
	}

	for (int y = baseY + 1; y <= baseY + 5; ++y) {
		for (int z = 4; z <= 5; ++z) {
			for (int x = 6; x <= 7; ++x) {
				SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::FloorWhite, nullptr);
			}
		}
	}

	for (int y = baseY + 1; y <= baseY + 3; ++y) {
		SetVoxelMaterial(world, {5, y, 6}, VoxelMaterial::FloorWhite, nullptr);
	}
}

void BuildTransparencyStressColumns(VoxelWorld &world, const VoxelWorldConfig &config)
{
	const int halfFloor = GetHalfFloorSize(config);
	for (int z = -halfFloor + 2; z < halfFloor - 2; z += 2) {
		for (int x = -halfFloor + 2; x < halfFloor - 2; x += 2) {
			const int columnHeight = 4 + (std::abs(x) + std::abs(z)) % 8;
			for (int y = config.floorY + 1; y <= config.floorY + columnHeight; ++y) {
				SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::Glass, nullptr);
			}
		}
	}
}

void BuildChunkGridMarkers(VoxelWorld &world, const VoxelWorldConfig &config)
{
	for (int chunkZ = 0; chunkZ < world.chunkCountZ; ++chunkZ) {
		for (int chunkX = 0; chunkX < world.chunkCountX; ++chunkX) {
			const int markerX = world.min.x + chunkX * world.chunkSize;
			const int markerZ = world.min.z + chunkZ * world.chunkSize;
			const VoxelMaterial material = (chunkX + chunkZ & 1) == 0 ? VoxelMaterial::FloorWhite : VoxelMaterial::FloorGray;
			for (int y = config.floorY + 1; y < world.maxExclusive.y; ++y) {
				SetVoxelMaterial(world, {markerX, y, markerZ}, material, nullptr);
			}
		}
	}
}

void BuildMeshingStressVolume(VoxelWorld &world, const VoxelWorldConfig &config)
{
	const int halfFloor = GetHalfFloorSize(config);
	const int topY = std::min(config.floorY + 16, world.maxExclusive.y - 1);
	for (int z = -halfFloor + 1; z < halfFloor - 1; ++z) {
		for (int y = config.floorY + 1; y <= topY; ++y) {
			for (int x = -halfFloor + 1; x < halfFloor - 1; ++x) {
				if ((x + y + z & 1) != 0) {
					continue;
				}

				const VoxelMaterial material = (x + z & 2) == 0 ? VoxelMaterial::FloorWhite : VoxelMaterial::FloorGray;
				SetVoxelMaterial(world, {x, y, z}, material, nullptr);
			}
		}
	}
}

VoxelWorldConfig GetFlatBenchmarkWorldConfig()
{
	VoxelWorldConfig config{};
	config.floorSize = 64;
	config.worldTopY = 18;
	config.padding = 8;
	return config;
}

VoxelWorldConfig GetTransparencyStressWorldConfig()
{
	VoxelWorldConfig config{};
	config.floorSize = 48;
	config.worldTopY = 18;
	config.padding = 8;
	return config;
}

VoxelWorldConfig GetChunkGridWorldConfig()
{
	VoxelWorldConfig config{};
	config.floorSize = 48;
	config.worldTopY = 24;
	config.padding = 8;
	return config;
}

VoxelWorldConfig GetMeshingStressWorldConfig()
{
	VoxelWorldConfig config{};
	config.floorSize = 48;
	config.worldTopY = 20;
	config.padding = 8;
	return config;
}

std::unique_ptr<VoxelWorld> CreateSceneWorldWithFloor(
	const VoxelScenePreset scenePreset,
	const VoxelWorldConfig &worldConfig)
{
	std::unique_ptr<VoxelWorld> world = CreateEmptyVoxelWorld(worldConfig, scenePreset);
	BuildCheckerboardFloor(*world, worldConfig);
	return world;
}

std::unique_ptr<VoxelWorld> BuildVoxelLabSceneWorld()
{
	const VoxelWorldConfig worldConfig = GetVoxelLabWorldConfig();
	const VoxelLabShellConfig voxelLabShell = GetVoxelLabShellConfig();
	std::unique_ptr<VoxelWorld> world = CreateSceneWorldWithFloor(VoxelScenePreset::VoxelLab, worldConfig);
	BuildVoxelLabShellAndFluid(*world, voxelLabShell);
	BuildVoxelLabOpaqueAnchors(*world, worldConfig);

	MarkAllVoxelChunksDirty(world.get());
	return world;
}

std::unique_ptr<VoxelWorld> BuildFlatBenchmarkSceneWorld()
{
	const VoxelWorldConfig worldConfig = GetFlatBenchmarkWorldConfig();
	std::unique_ptr<VoxelWorld> world = CreateSceneWorldWithFloor(VoxelScenePreset::FlatBenchmark, worldConfig);
	MarkAllVoxelChunksDirty(world.get());
	return world;
}

std::unique_ptr<VoxelWorld> BuildTransparencyStressSceneWorld()
{
	const VoxelWorldConfig worldConfig = GetTransparencyStressWorldConfig();
	std::unique_ptr<VoxelWorld> world = CreateSceneWorldWithFloor(VoxelScenePreset::TransparencyStress, worldConfig);
	BuildTransparencyStressColumns(*world, worldConfig);
	MarkAllVoxelChunksDirty(world.get());
	return world;
}

std::unique_ptr<VoxelWorld> BuildChunkGridSceneWorld()
{
	const VoxelWorldConfig worldConfig = GetChunkGridWorldConfig();
	std::unique_ptr<VoxelWorld> world = CreateSceneWorldWithFloor(VoxelScenePreset::ChunkGrid, worldConfig);
	BuildChunkGridMarkers(*world, worldConfig);
	MarkAllVoxelChunksDirty(world.get());
	return world;
}

std::unique_ptr<VoxelWorld> BuildMeshingStressSceneWorld()
{
	const VoxelWorldConfig worldConfig = GetMeshingStressWorldConfig();
	std::unique_ptr<VoxelWorld> world = CreateSceneWorldWithFloor(VoxelScenePreset::MeshingStress, worldConfig);
	BuildMeshingStressVolume(*world, worldConfig);
	MarkAllVoxelChunksDirty(world.get());
	return world;
}

std::unique_ptr<VoxelWorld> BuildVoxelSceneWorld(const VoxelScenePreset scenePreset)
{
	switch (scenePreset) {
	case VoxelScenePreset::VoxelLab:
		return BuildVoxelLabSceneWorld();
	case VoxelScenePreset::FlatBenchmark:
		return BuildFlatBenchmarkSceneWorld();
	case VoxelScenePreset::TransparencyStress:
		return BuildTransparencyStressSceneWorld();
	case VoxelScenePreset::ChunkGrid:
		return BuildChunkGridSceneWorld();
	case VoxelScenePreset::MeshingStress:
		return BuildMeshingStressSceneWorld();
	}

	return BuildVoxelLabSceneWorld();
}
} // namespace

std::optional<VoxelScenePreset> ParseVoxelScenePreset(const std::string_view text)
{
	if (text.empty()) {
		return std::nullopt;
	}

	if (MatchesPresetName(text, "voxellab")) {
		return VoxelScenePreset::VoxelLab;
	}
	if (MatchesPresetName(text, "flatbenchmark")) {
		return VoxelScenePreset::FlatBenchmark;
	}
	if (MatchesPresetName(text, "transparencystress")) {
		return VoxelScenePreset::TransparencyStress;
	}
	if (MatchesPresetName(text, "chunkgrid")) {
		return VoxelScenePreset::ChunkGrid;
	}
	if (MatchesPresetName(text, "meshingstress")) {
		return VoxelScenePreset::MeshingStress;
	}

	return std::nullopt;
}

std::string_view VoxelScenePresetToString(const VoxelScenePreset preset)
{
	switch (preset) {
	case VoxelScenePreset::VoxelLab:
		return "VoxelLab";
	case VoxelScenePreset::FlatBenchmark:
		return "FlatBenchmark";
	case VoxelScenePreset::TransparencyStress:
		return "TransparencyStress";
	case VoxelScenePreset::ChunkGrid:
		return "ChunkGrid";
	case VoxelScenePreset::MeshingStress:
		return "MeshingStress";
	}

	return "VoxelLab";
}

VoxelScenePreset GetNextVoxelScenePreset(const VoxelScenePreset preset)
{
	switch (preset) {
	case VoxelScenePreset::VoxelLab:
		return VoxelScenePreset::FlatBenchmark;
	case VoxelScenePreset::FlatBenchmark:
		return VoxelScenePreset::TransparencyStress;
	case VoxelScenePreset::TransparencyStress:
		return VoxelScenePreset::ChunkGrid;
	case VoxelScenePreset::ChunkGrid:
		return VoxelScenePreset::MeshingStress;
	case VoxelScenePreset::MeshingStress:
		return VoxelScenePreset::VoxelLab;
	}

	return VoxelScenePreset::VoxelLab;
}

VoxelScenePreset GetRequestedVoxelScenePreset()
{
	const char *requestedPreset = SDL_getenv("PROJECTV_SCENE_PRESET");
	if (!requestedPreset || !*requestedPreset) {
		return kDefaultVoxelScenePreset;
	}

	if (const auto parsedPreset = ParseVoxelScenePreset(requestedPreset)) {
		return *parsedPreset;
	}

	SDL_Log(
		"Unknown PROJECTV_SCENE_PRESET='%s'; using %s",
		requestedPreset,
		std::string{VoxelScenePresetToString(kDefaultVoxelScenePreset)}.c_str());
	return kDefaultVoxelScenePreset;
}

bool CreateVoxelSceneWorld(AppState *state)
{
	return CreateVoxelSceneWorld(state, GetRequestedVoxelScenePreset());
}

bool CreateVoxelSceneWorld(AppState *state, const VoxelScenePreset preset)
{
	if (!state) {
		return false;
	}

	std::unique_ptr<VoxelWorld> world = BuildVoxelSceneWorld(preset);
	if (!world) {
		return false;
	}

	state->world().voxelWorld = std::move(world);
	state->world().requestedScenePreset = state->world().voxelWorld->scenePreset;
	state->world().scenePresetReloadRequested = false;
	state->world().snapshotSaveRequested = false;
	state->world().snapshotLoadRequested = false;
	if (state->world().voxelWorld) {
		SDL_Log(
			"Using voxel scene preset: %s",
			std::string{VoxelScenePresetToString(state->world().voxelWorld->scenePreset)}.c_str());
	}
	return static_cast<bool>(state->world().voxelWorld);
}

void DestroyVoxelSceneWorld(AppState *state)
{
	if (!state) {
		return;
	}

	state->world().voxelWorld.reset();
	state->world().scenePresetReloadRequested = false;
	state->world().requestedScenePreset = kDefaultVoxelScenePreset;
	state->world().snapshotSaveRequested = false;
	state->world().snapshotLoadRequested = false;
}

std::unique_ptr<VoxelWorld> CreateEmptyVoxelWorld(
	const VoxelWorldConfig &config,
	const VoxelScenePreset scenePreset,
	const Int3 min,
	const Int3 maxExclusive)
{
	if (config.chunkSize <= 0) {
		return nullptr;
	}

	int width = 0;
	int height = 0;
	int depth = 0;

	// TryComputeVoxelWorldDimensions uses output pointers, so we declare them first
	const int64_t width64 = static_cast<int64_t>(maxExclusive.x) - static_cast<int64_t>(min.x);
	const int64_t height64 = static_cast<int64_t>(maxExclusive.y) - static_cast<int64_t>(min.y);
	const int64_t depth64 = static_cast<int64_t>(maxExclusive.z) - static_cast<int64_t>(min.z);
	if (width64 <= 0 || height64 <= 0 || depth64 <= 0 ||
		width64 > std::numeric_limits<int>::max() ||
		height64 > std::numeric_limits<int>::max() ||
		depth64 > std::numeric_limits<int>::max()) {
		return nullptr;
	}
	width = static_cast<int>(width64);
	height = static_cast<int>(height64);
	depth = static_cast<int>(depth64);

	const int64_t voxelCount =
		static_cast<int64_t>(width) *
		static_cast<int64_t>(height) *
		static_cast<int64_t>(depth);
	if (voxelCount <= 0 ||
		static_cast<std::uint64_t>(voxelCount) > std::numeric_limits<size_t>::max()) {
		return nullptr;
	}

	const int chunkCountX = (width + config.chunkSize - 1) / config.chunkSize;
	const int chunkCountY = (height + config.chunkSize - 1) / config.chunkSize;
	const int chunkCountZ = (depth + config.chunkSize - 1) / config.chunkSize;
	const int64_t chunkCount64 =
		static_cast<int64_t>(chunkCountX) *
		static_cast<int64_t>(chunkCountY) *
		static_cast<int64_t>(chunkCountZ);
	if (chunkCount64 <= 0 ||
		static_cast<std::uint64_t>(chunkCount64) > std::numeric_limits<size_t>::max()) {
		return nullptr;
	}
	const size_t chunkCount = static_cast<size_t>(chunkCount64);

	auto world = std::make_unique<VoxelWorld>();
	world->scenePreset = scenePreset;
	world->config = config;
	world->chunkSize = config.chunkSize;
	world->min = min;
	world->maxExclusive = maxExclusive;
	world->floorMin = Int3{
		min.x + config.padding,
		min.y,
		min.z + config.padding,
	};
	world->floorMaxExclusive = Int3{
		maxExclusive.x - config.padding,
		maxExclusive.y,
		maxExclusive.z - config.padding,
	};
	world->width = width;
	world->height = height;
	world->depth = depth;
	world->sparseStorage.Reset(width, height, depth);

	world->chunkCountX = chunkCountX;
	world->chunkCountY = chunkCountY;
	world->chunkCountZ = chunkCountZ;
	world->chunks.resize(chunkCount);

	for (int chunkZ = 0; chunkZ < world->chunkCountZ; ++chunkZ) {
		for (int chunkY = 0; chunkY < world->chunkCountY; ++chunkY) {
			for (int chunkX = 0; chunkX < world->chunkCountX; ++chunkX) {
				const size_t chunkIndex = GetVoxelChunkIndex(*world, {chunkX, chunkY, chunkZ});
				VoxelChunk &chunk = world->chunks[chunkIndex];
				chunk.min = {
					world->min.x + chunkX * world->chunkSize,
					world->min.y + chunkY * world->chunkSize,
					world->min.z + chunkZ * world->chunkSize,
				};
				chunk.maxExclusive = {
					std::min(chunk.min.x + world->chunkSize, world->maxExclusive.x),
					std::min(chunk.min.y + world->chunkSize, world->maxExclusive.y),
					std::min(chunk.min.z + world->chunkSize, world->maxExclusive.z),
				};
				chunk.rebuildQueued = true;
			}
		}
	}
	world->pendingChunkRebuildIndices.reserve(world->chunks.size());
	world->pendingBlasRebuildIndices.reserve(world->chunks.size()); // Reserve space for initial BLAS rebuilds
	for (size_t chunkIndex = 0; chunkIndex < world->chunks.size(); ++chunkIndex) {
		world->pendingChunkRebuildIndices.push_back(chunkIndex);
		world->pendingBlasRebuildIndices.push_back(chunkIndex); // Mark chunk for BLAS rebuild initially
	}
	world->stats.dirtyChunkCount = static_cast<uint32_t>(world->pendingChunkRebuildIndices.size());

	return world;
}

std::unique_ptr<VoxelWorld> CreateEmptyVoxelWorld(const VoxelWorldConfig &config, const VoxelScenePreset scenePreset)
{
	const int halfFloor = GetHalfFloorSize(config);
	const Int3 min{
		-halfFloor - config.padding,
		config.floorY,
		-halfFloor - config.padding,
	};
	const Int3 maxExclusive{
		halfFloor + config.padding,
		ResolveWorldTopY(config) + config.padding,
		halfFloor + config.padding,
	};
	return CreateEmptyVoxelWorld(config, scenePreset, min, maxExclusive);
}

void RebuildVoxelWorldDerivedState(VoxelWorld &world)
{
	const uint32_t dirtyChunkCount = static_cast<uint32_t>(world.pendingChunkRebuildIndices.size());
	world.stats = {};
	world.stats.dirtyChunkCount = dirtyChunkCount;

	for (VoxelChunk &chunk : world.chunks) {
		chunk.nonAirVoxelCount = 0;
	}

	size_t voxelIndex = 0;
	for (int z = world.min.z; z < world.maxExclusive.z; ++z) {
		for (int y = world.min.y; y < world.maxExclusive.y; ++y) {
			for (int x = world.min.x; x < world.maxExclusive.x; ++x) {
				const VoxelMaterial material = GetVoxelMaterial(world, {x, y, z});
				if (material == VoxelMaterial::Air) {
					++voxelIndex;
					continue;
				}

				AccumulateMaterialCount(world.stats, material, 1);
				VoxelChunk &chunk = world.chunks[GetVoxelChunkIndex(world, GetVoxelChunkCoord(world, {x, y, z}))];
				++chunk.nonAirVoxelCount;
				++voxelIndex;
			}
		}
	}

	for (const VoxelChunk &chunk : world.chunks) {
		if (chunk.nonAirVoxelCount > 0) {
			++world.stats.activeChunkCount;
		}
	}
}

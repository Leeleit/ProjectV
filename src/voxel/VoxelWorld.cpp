#include "voxel/VoxelWorld.hpp"

#include "SDL3/SDL_log.h"
#include "core/RuntimeDiagnostics.hpp"
#include "core/Types.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>

namespace {
constexpr VoxelScenePreset kDefaultVoxelScenePreset = VoxelScenePreset::VoxelLab;

struct VoxelLabShellConfig {
	int radius = 6;
	Int3 center{0, 8, 0};
	int shellThickness = 1;
	float fluidFillLevel = 0.7f;
};

size_t ToVoxelIndex(const VoxelWorld &world, const Int3 position)
{
	const size_t localX = static_cast<size_t>(position.x - world.min.x);
	const size_t localY = static_cast<size_t>(position.y - world.min.y);
	const size_t localZ = static_cast<size_t>(position.z - world.min.z);
	return localX + static_cast<size_t>(world.width) * (localY + static_cast<size_t>(world.height) * localZ);
}

Int3 GetChunkCoord(const VoxelWorld &world, const Int3 position)
{
	return {
		(position.x - world.min.x) / world.chunkSize,
		(position.y - world.min.y) / world.chunkSize,
		(position.z - world.min.z) / world.chunkSize,
	};
}

void AccumulateMaterialCount(VoxelWorldStats &stats, const VoxelMaterial material, const int delta)
{
	switch (material) {
	case VoxelMaterial::Air:
		break;
	case VoxelMaterial::Glass:
		stats.glassVoxelCount = static_cast<uint32_t>(static_cast<int64_t>(stats.glassVoxelCount) + delta);
		stats.nonAirVoxelCount = static_cast<uint32_t>(static_cast<int64_t>(stats.nonAirVoxelCount) + delta);
		break;
	case VoxelMaterial::Fluid:
		stats.fluidVoxelCount = static_cast<uint32_t>(static_cast<int64_t>(stats.fluidVoxelCount) + delta);
		stats.nonAirVoxelCount = static_cast<uint32_t>(static_cast<int64_t>(stats.nonAirVoxelCount) + delta);
		break;
	case VoxelMaterial::FloorWhite:
		stats.floorWhiteVoxelCount = static_cast<uint32_t>(static_cast<int64_t>(stats.floorWhiteVoxelCount) + delta);
		stats.nonAirVoxelCount = static_cast<uint32_t>(static_cast<int64_t>(stats.nonAirVoxelCount) + delta);
		break;
	case VoxelMaterial::FloorGray:
		stats.floorGrayVoxelCount = static_cast<uint32_t>(static_cast<int64_t>(stats.floorGrayVoxelCount) + delta);
		stats.nonAirVoxelCount = static_cast<uint32_t>(static_cast<int64_t>(stats.nonAirVoxelCount) + delta);
		break;
	}
}

bool IsAirMaterial(const VoxelMaterial material)
{
	return material == VoxelMaterial::Air;
}

void QueueChunkRebuildRequest(VoxelWorld &world, const size_t chunkIndex)
{
	VoxelChunk &chunk = world.chunks[chunkIndex];
	if (chunk.rebuildQueued) {
		return;
	}

	chunk.rebuildQueued = true;
	world.pendingChunkRebuildIndices.push_back(chunkIndex);
	++world.stats.dirtyChunkCount;
}

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

std::unique_ptr<VoxelWorld> CreateEmptyVoxelWorld(const VoxelWorldConfig &config, const VoxelScenePreset scenePreset)
{
	auto world = std::make_unique<VoxelWorld>();
	world->scenePreset = scenePreset;
	world->config = config;
	world->chunkSize = config.chunkSize;

	const int halfFloor = GetHalfFloorSize(config);
	world->min = {
		-halfFloor - config.padding,
		config.floorY,
		-halfFloor - config.padding,
	};
	world->maxExclusive = {
		halfFloor + config.padding,
		ResolveWorldTopY(config) + config.padding,
		halfFloor + config.padding,
	};
	world->width = world->maxExclusive.x - world->min.x;
	world->height = world->maxExclusive.y - world->min.y;
	world->depth = world->maxExclusive.z - world->min.z;
	world->voxels.resize(
		static_cast<size_t>(world->width) *
			static_cast<size_t>(world->height) *
			static_cast<size_t>(world->depth),
		static_cast<uint8_t>(VoxelMaterial::Air));

	world->chunkCountX = (world->width + world->chunkSize - 1) / world->chunkSize;
	world->chunkCountY = (world->height + world->chunkSize - 1) / world->chunkSize;
	world->chunkCountZ = (world->depth + world->chunkSize - 1) / world->chunkSize;
	world->chunks.resize(
		static_cast<size_t>(world->chunkCountX) *
		static_cast<size_t>(world->chunkCountY) *
		static_cast<size_t>(world->chunkCountZ));

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
	for (size_t chunkIndex = 0; chunkIndex < world->chunks.size(); ++chunkIndex) {
		world->pendingChunkRebuildIndices.push_back(chunkIndex);
	}
	world->stats.dirtyChunkCount = static_cast<uint32_t>(world->pendingChunkRebuildIndices.size());

	return world;
}

void BuildCheckerboardFloor(VoxelWorld &world, const VoxelWorldConfig &config)
{
	const int halfFloor = GetHalfFloorSize(config);
	for (int z = -halfFloor; z < halfFloor; ++z) {
		for (int x = -halfFloor; x < halfFloor; ++x) {
			const VoxelMaterial material = (x + z & 1) == 0 ? VoxelMaterial::FloorWhite : VoxelMaterial::FloorGray;
			SetVoxelMaterial(world, {x, config.floorY, z}, material);
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
					SetVoxelMaterial(world, position, VoxelMaterial::Glass);
					continue;
				}

				if (innerRadius > 0 && position.y <= fluidTop) {
					SetVoxelMaterial(world, position, VoxelMaterial::Fluid);
				}
			}
		}
	}
}

void BuildTransparencyStressColumns(VoxelWorld &world, const VoxelWorldConfig &config)
{
	const int halfFloor = GetHalfFloorSize(config);
	for (int z = -halfFloor + 2; z < halfFloor - 2; z += 2) {
		for (int x = -halfFloor + 2; x < halfFloor - 2; x += 2) {
			const int columnHeight = 4 + (std::abs(x) + std::abs(z)) % 8;
			for (int y = config.floorY + 1; y <= config.floorY + columnHeight; ++y) {
				SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::Glass);
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
				SetVoxelMaterial(world, {markerX, y, markerZ}, material);
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
				SetVoxelMaterial(world, {x, y, z}, material);
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

bool TryParseVoxelScenePreset(const std::string_view text, VoxelScenePreset *outPreset)
{
	if (!outPreset || text.empty()) {
		return false;
	}

	if (MatchesPresetName(text, "voxellab")) {
		*outPreset = VoxelScenePreset::VoxelLab;
		return true;
	}
	if (MatchesPresetName(text, "flatbenchmark")) {
		*outPreset = VoxelScenePreset::FlatBenchmark;
		return true;
	}
	if (MatchesPresetName(text, "transparencystress")) {
		*outPreset = VoxelScenePreset::TransparencyStress;
		return true;
	}
	if (MatchesPresetName(text, "chunkgrid")) {
		*outPreset = VoxelScenePreset::ChunkGrid;
		return true;
	}
	if (MatchesPresetName(text, "meshingstress")) {
		*outPreset = VoxelScenePreset::MeshingStress;
		return true;
	}

	return false;
}

const char *VoxelScenePresetToString(const VoxelScenePreset preset)
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

	VoxelScenePreset scenePreset = kDefaultVoxelScenePreset;
	if (TryParseVoxelScenePreset(requestedPreset, &scenePreset)) {
		return scenePreset;
	}

	SDL_Log(
		"Unknown PROJECTV_SCENE_PRESET='%s'; using %s",
		requestedPreset,
		VoxelScenePresetToString(kDefaultVoxelScenePreset));
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

	state->world.voxelWorld = std::move(world);
	state->world.requestedScenePreset = state->world.voxelWorld->scenePreset;
	state->world.scenePresetReloadRequested = false;
	if (state->world.voxelWorld) {
		SDL_Log(
			"Using voxel scene preset: %s",
			VoxelScenePresetToString(state->world.voxelWorld->scenePreset));
	}
	return static_cast<bool>(state->world.voxelWorld);
}

void DestroyVoxelSceneWorld(AppState *state)
{
	if (!state) {
		return;
	}

	state->world.voxelWorld.reset();
	state->world.scenePresetReloadRequested = false;
	state->world.requestedScenePreset = kDefaultVoxelScenePreset;
}

bool IsInsideVoxelWorld(const VoxelWorld &world, const Int3 position)
{
	return position.x >= world.min.x && position.x < world.maxExclusive.x &&
		   position.y >= world.min.y && position.y < world.maxExclusive.y &&
		   position.z >= world.min.z && position.z < world.maxExclusive.z;
}

VoxelMaterial GetVoxelMaterial(const VoxelWorld &world, const Int3 position)
{
	if (!IsInsideVoxelWorld(world, position)) {
		return VoxelMaterial::Air;
	}

	return static_cast<VoxelMaterial>(world.voxels[ToVoxelIndex(world, position)]);
}

size_t GetVoxelChunkIndex(const VoxelWorld &world, const Int3 chunkCoord)
{
	return static_cast<size_t>(chunkCoord.x) +
		   static_cast<size_t>(world.chunkCountX) *
			   (static_cast<size_t>(chunkCoord.y) +
				static_cast<size_t>(world.chunkCountY) * static_cast<size_t>(chunkCoord.z));
}

void MarkVoxelChunkDirty(VoxelWorld &world, const Int3 position)
{
	if (!IsInsideVoxelWorld(world, position)) {
		return;
	}

	const Int3 chunkCoord = GetChunkCoord(world, position);
	QueueChunkRebuildRequest(world, GetVoxelChunkIndex(world, chunkCoord));
}

void MarkVoxelRegionDirty(VoxelWorld &world, const Int3 min, const Int3 maxExclusive)
{
	const Int3 clampedMin{
		std::max(min.x, world.min.x),
		std::max(min.y, world.min.y),
		std::max(min.z, world.min.z),
	};
	const Int3 clampedMax{
		std::min(maxExclusive.x, world.maxExclusive.x),
		std::min(maxExclusive.y, world.maxExclusive.y),
		std::min(maxExclusive.z, world.maxExclusive.z),
	};
	if (clampedMin.x >= clampedMax.x || clampedMin.y >= clampedMax.y || clampedMin.z >= clampedMax.z) {
		return;
	}

	const auto [firstChunkX, firstChunkY, firstChunkZ] = GetChunkCoord(world, clampedMin);
	const auto [lastChunkX, lastChunkY, lastChunkZ] =
		GetChunkCoord(world, {clampedMax.x - 1, clampedMax.y - 1, clampedMax.z - 1});

	for (int chunkZ = firstChunkZ; chunkZ <= lastChunkZ; ++chunkZ) {
		for (int chunkY = firstChunkY; chunkY <= lastChunkY; ++chunkY) {
			for (int chunkX = firstChunkX; chunkX <= lastChunkX; ++chunkX) {
				QueueChunkRebuildRequest(world, GetVoxelChunkIndex(world, {chunkX, chunkY, chunkZ}));
			}
		}
	}
}

void SetVoxelMaterial(VoxelWorld &world, const Int3 position, const VoxelMaterial material)
{
	if (!IsInsideVoxelWorld(world, position)) {
		return;
	}

	const size_t voxelIndex = ToVoxelIndex(world, position);
	const VoxelMaterial previousMaterial = static_cast<VoxelMaterial>(world.voxels[voxelIndex]);
	if (previousMaterial == material) {
		return;
	}

	world.voxels[voxelIndex] = static_cast<uint8_t>(material);
	++world.editVersion;
	AccumulateMaterialCount(world.stats, previousMaterial, -1);
	AccumulateMaterialCount(world.stats, material, 1);

	VoxelChunk &chunk = world.chunks[GetVoxelChunkIndex(world, GetChunkCoord(world, position))];
	const bool wasActive = chunk.nonAirVoxelCount > 0;
	if (!IsAirMaterial(previousMaterial)) {
		--chunk.nonAirVoxelCount;
	}
	if (!IsAirMaterial(material)) {
		++chunk.nonAirVoxelCount;
	}
	const bool isActive = chunk.nonAirVoxelCount > 0;
	if (!wasActive && isActive) {
		++world.stats.activeChunkCount;
	} else if (wasActive && !isActive) {
		--world.stats.activeChunkCount;
	}

	// A single voxel edit can change visible faces in adjacent chunks along every axis.
	MarkVoxelRegionDirty(
		world,
		{position.x - 1, position.y - 1, position.z - 1},
		{position.x + 2, position.y + 2, position.z + 2});
}

void MarkAllVoxelChunksDirty(VoxelWorld *world)
{
	if (!world) {
		return;
	}

	for (VoxelChunk &chunk : world->chunks) {
		chunk.rebuildQueued = true;
	}
	world->pendingChunkRebuildIndices.clear();
	world->pendingChunkRebuildIndices.reserve(world->chunks.size());
	for (size_t chunkIndex = 0; chunkIndex < world->chunks.size(); ++chunkIndex) {
		world->pendingChunkRebuildIndices.push_back(chunkIndex);
	}
	world->stats.dirtyChunkCount = static_cast<uint32_t>(world->pendingChunkRebuildIndices.size());
}

void CollectDirtyVoxelChunkRebuildRequests(VoxelWorld &world, std::vector<size_t> *outChunkIndices)
{
	if (!outChunkIndices || world.pendingChunkRebuildIndices.empty()) {
		return;
	}

	outChunkIndices->insert(
		outChunkIndices->end(),
		world.pendingChunkRebuildIndices.begin(),
		world.pendingChunkRebuildIndices.end());
	world.pendingChunkRebuildIndices.clear();
}

void CommitDirtyVoxelChunkRebuildRequests(VoxelWorld &world, const std::vector<size_t> &rebuiltChunkIndices)
{
	for (const size_t chunkIndex : rebuiltChunkIndices) {
		if (chunkIndex >= world.chunks.size()) {
			continue;
		}

		VoxelChunk &chunk = world.chunks[chunkIndex];
		if (!chunk.rebuildQueued) {
			continue;
		}

		chunk.rebuildQueued = false;
		if (world.stats.dirtyChunkCount > 0) {
			--world.stats.dirtyChunkCount;
		}
	}
}

uint32_t CountDirtyVoxelChunks(const VoxelWorld &world)
{
	return world.stats.dirtyChunkCount;
}

uint32_t CountActiveVoxelChunks(const VoxelWorld &world)
{
	return world.stats.activeChunkCount;
}

uint32_t CountVoxelsByMaterial(const VoxelWorld &world, const VoxelMaterial material)
{
	switch (material) {
	case VoxelMaterial::Air:
		return static_cast<uint32_t>(world.voxels.size()) - world.stats.nonAirVoxelCount;
	case VoxelMaterial::Glass:
		return world.stats.glassVoxelCount;
	case VoxelMaterial::Fluid:
		return world.stats.fluidVoxelCount;
	case VoxelMaterial::FloorWhite:
		return world.stats.floorWhiteVoxelCount;
	case VoxelMaterial::FloorGray:
		return world.stats.floorGrayVoxelCount;
	}

	return 0;
}

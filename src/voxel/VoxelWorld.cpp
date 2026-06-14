#include "voxel/VoxelWorld.hpp"

#include "SDL3/SDL_log.h"
#include "core/RuntimeDiagnostics.hpp"
#include "core/Types.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <string>

namespace {
constexpr VoxelScenePreset kDefaultVoxelScenePreset = VoxelScenePreset::VoxelLab;
constexpr char kDefaultVoxelWorldSnapshotFilename[] = "ProjectV.snapshot.bin";
constexpr std::array kVoxelWorldSnapshotMagic{'P', 'V', 'S', 'N', 'A', 'P', '0', '1'};
constexpr uint32_t kVoxelWorldSnapshotVersion = 1u;

struct VoxelLabShellConfig {
	int radius = 6;
	Int3 center{0, 8, 0};
	int shellThickness = 1;
	float fluidFillLevel = 0.7f;
};

struct VoxelWorldSnapshotHeader {
	std::array<char, 8> magic{};
	uint32_t version = 0;
	uint32_t voxelByteCount = 0;
	uint32_t reserved = 0;
	uint8_t scenePreset = 0;
	uint8_t reservedBytes[3]{};
	VoxelWorldConfig config{};
	Int3 min{};
	Int3 maxExclusive{};
	uint64_t editVersion = 0;
};
static_assert(std::is_standard_layout_v<VoxelWorldSnapshotHeader>);
static_assert(std::is_trivially_copyable_v<VoxelWorldSnapshotHeader>);
static_assert(sizeof(VoxelWorldSnapshotHeader) == 80);

size_t ToVoxelIndex(const VoxelWorld &world, const Int3 position)
{
	const size_t localX = static_cast<size_t>(position.x - world.min.x);
	const size_t localY = static_cast<size_t>(position.y - world.min.y);
	const size_t localZ = static_cast<size_t>(position.z - world.min.z);
	return localX + static_cast<size_t>(world.width) * (localY + static_cast<size_t>(world.height) * localZ);
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

bool IsValidVoxelMaterialValue(const uint8_t materialValue)
{
	return materialValue <= static_cast<uint8_t>(VoxelMaterial::FloorGray);
}

bool IsValidVoxelScenePresetValue(const uint8_t presetValue)
{
	return presetValue <= static_cast<uint8_t>(VoxelScenePreset::MeshingStress);
}

void ClearSnapshotReservedFields(VoxelWorldSnapshotHeader *header)
{
	PV_ASSERT(
		header != nullptr,
		"VoxelWorld",
		"ClearSnapshotReservedFields.Preconditions",
		"snapshot header is null");
	header->reserved = 0;
	for (uint8_t &reservedByte : header->reservedBytes) {
		reservedByte = 0;
	}
}

bool HasClearSnapshotReservedFields(const VoxelWorldSnapshotHeader &header)
{
	if (header.reserved != 0) {
		return false;
	}

	for (const uint8_t reservedByte : header.reservedBytes) {
		if (reservedByte != 0) {
			return false;
		}
	}

	return true;
}

bool TryComputeVoxelWorldDimensions(
	const Int3 min,
	const Int3 maxExclusive,
	int *outWidth,
	int *outHeight,
	int *outDepth)
{
	PV_CHECK_OR_RETURN(
		outWidth && outHeight && outDepth,
		"VoxelWorld",
		"TryComputeVoxelWorldDimensions.Preconditions",
		"output pointers are null");

	const int64_t width = static_cast<int64_t>(maxExclusive.x) - static_cast<int64_t>(min.x);
	const int64_t height = static_cast<int64_t>(maxExclusive.y) - static_cast<int64_t>(min.y);
	const int64_t depth = static_cast<int64_t>(maxExclusive.z) - static_cast<int64_t>(min.z);
	if (width <= 0 || height <= 0 || depth <= 0 ||
		width > std::numeric_limits<int>::max() ||
		height > std::numeric_limits<int>::max() ||
		depth > std::numeric_limits<int>::max()) {
		return false;
	}

	*outWidth = static_cast<int>(width);
	*outHeight = static_cast<int>(height);
	*outDepth = static_cast<int>(depth);
	return true;
}

bool TryComputeVoxelBufferSize(
	const int width,
	const int height,
	const int depth,
	size_t *outVoxelCount)
{
	PV_CHECK_OR_RETURN(
		outVoxelCount != nullptr,
		"VoxelWorld",
		"TryComputeVoxelBufferSize.Preconditions",
		"outVoxelCount is null");

	const int64_t voxelCount =
		static_cast<int64_t>(width) *
		static_cast<int64_t>(height) *
		static_cast<int64_t>(depth);
	if (voxelCount <= 0 ||
		static_cast<uint64_t>(voxelCount) > std::numeric_limits<size_t>::max()) {
		return false;
	}

	*outVoxelCount = static_cast<size_t>(voxelCount);
	return true;
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

void MarkChunksTouchedByVoxelEditDirty(VoxelWorld &world, const Int3 position)
{
	const Int3 chunkCoord = GetVoxelChunkCoord(world, position);
	const VoxelChunk &chunk = world.chunks[GetVoxelChunkIndex(world, chunkCoord)];

	int minChunkX = chunkCoord.x;
	int maxChunkX = chunkCoord.x;
	int minChunkY = chunkCoord.y;
	int maxChunkY = chunkCoord.y;
	int minChunkZ = chunkCoord.z;
	int maxChunkZ = chunkCoord.z;

	if (position.x == chunk.min.x && chunkCoord.x > 0) {
		--minChunkX;
	}
	if (position.x == chunk.maxExclusive.x - 1 && chunkCoord.x + 1 < world.chunkCountX) {
		++maxChunkX;
	}
	if (position.y == chunk.min.y && chunkCoord.y > 0) {
		--minChunkY;
	}
	if (position.y == chunk.maxExclusive.y - 1 && chunkCoord.y + 1 < world.chunkCountY) {
		++maxChunkY;
	}
	if (position.z == chunk.min.z && chunkCoord.z > 0) {
		--minChunkZ;
	}
	if (position.z == chunk.maxExclusive.z - 1 && chunkCoord.z + 1 < world.chunkCountZ) {
		++maxChunkZ;
	}

	for (int dirtyChunkZ = minChunkZ; dirtyChunkZ <= maxChunkZ; ++dirtyChunkZ) {
		for (int dirtyChunkY = minChunkY; dirtyChunkY <= maxChunkY; ++dirtyChunkY) {
			for (int dirtyChunkX = minChunkX; dirtyChunkX <= maxChunkX; ++dirtyChunkX) {
				QueueChunkRebuildRequest(world, GetVoxelChunkIndex(world, {dirtyChunkX, dirtyChunkY, dirtyChunkZ}));
			}
		}
	}
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
	if (!TryComputeVoxelWorldDimensions(min, maxExclusive, &width, &height, &depth)) {
		return nullptr;
	}

	size_t voxelCount = 0;
	if (!TryComputeVoxelBufferSize(width, height, depth, &voxelCount)) {
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
		static_cast<uint64_t>(chunkCount64) > std::numeric_limits<size_t>::max()) {
		return nullptr;
	}
	const size_t chunkCount = static_cast<size_t>(chunkCount64);

	auto world = std::make_unique<VoxelWorld>();
	world->scenePreset = scenePreset;
	world->config = config;
	world->chunkSize = config.chunkSize;
	world->min = min;
	world->maxExclusive = maxExclusive;
	// **Floor bounds (M5.1d, 2026-06-12):** the XZ extent of
	// the visible checkerboard, without the world padding
	// (which exists for chunk allocation, not for the model
	// snap). The snap should clamp models to the floor (the
	// visible platform), not to the world (which extends
	// `padding` voxels of invisible Air beyond the floor on
	// every side). Y is unchanged — there's no horizontal
	// padding for the height, only XZ. The convention: floor
	// is centered at world origin, halfFloor = `floorSize/2`,
	// so `floorMin = (-halfFloor, 0, -halfFloor)` and
	// `floorMaxExclusive = (halfFloor, maxY, halfFloor)`.
	// Padding on the world is uniformly subtracted from the XZ
	// extents. For VoxelLab (floorSize=18, padding=3): floor
	// is X∈[-9,9], Z∈[-9,9].
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
	world->voxels.resize(voxelCount, static_cast<uint8_t>(VoxelMaterial::Air));

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
	for (size_t chunkIndex = 0; chunkIndex < world->chunks.size(); ++chunkIndex) {
		world->pendingChunkRebuildIndices.push_back(chunkIndex);
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

void BuildVoxelLabOpaqueAnchors(VoxelWorld &world, const VoxelWorldConfig &config)
{
	const int baseY = config.floorY + 1;
	for (int z = 4; z <= 6; ++z) {
		for (int x = 5; x <= 8; ++x) {
			SetVoxelMaterial(world, {x, baseY, z}, VoxelMaterial::FloorGray);
		}
	}

	for (int y = baseY + 1; y <= baseY + 5; ++y) {
		for (int z = 4; z <= 5; ++z) {
			for (int x = 6; x <= 7; ++x) {
				SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::FloorWhite);
			}
		}
	}

	for (int y = baseY + 1; y <= baseY + 3; ++y) {
		SetVoxelMaterial(world, {5, y, 6}, VoxelMaterial::FloorWhite);
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
				const uint8_t materialValue = world.voxels[voxelIndex++];
				if (materialValue == static_cast<uint8_t>(VoxelMaterial::Air)) {
					continue;
				}

				const VoxelMaterial material = static_cast<VoxelMaterial>(materialValue);
				AccumulateMaterialCount(world.stats, material, 1);
				VoxelChunk &chunk = world.chunks[GetVoxelChunkIndex(world, GetVoxelChunkCoord(world, {x, y, z}))];
				++chunk.nonAirVoxelCount;
			}
		}
	}

	for (const VoxelChunk &chunk : world.chunks) {
		if (chunk.nonAirVoxelCount > 0) {
			++world.stats.activeChunkCount;
		}
	}
}

std::filesystem::path ResolveVoxelWorldSnapshotPath(const std::string_view snapshotPath)
{
	return std::filesystem::path(std::string(snapshotPath));
}
} // namespace

std::optional<VoxelScenePreset> ParseVoxelScenePreset(const std::string_view text)
{
	// **Tier 1.E (`2026-06-13`).** Replaced `bool TryParse(..., &out)`
	// out-param pattern with `std::optional<VoxelScenePreset>`.
	// Cold path (preset switch from JSON / CLI), so the small
	// `optional` overhead is irrelevant. The `std::nullopt` return
	// is more idiomatic than a magic empty preset — the caller
	// does `.value_or(VoxelScenePreset::VoxelLab)` to get the
	// historical "fall through to default" behavior.
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
	// **Tier 1.E (`2026-06-13`).** `constexpr std::string_view` return
	// (was `const char *`). The literal stays in `.rodata` either
	// way; the new return type lets callers use the result in
	// `string_view` / `fmt::format` / `unordered_map<std::string_view, …>`
	// contexts without an implicit conversion. The fallback
	// `"VoxelLab"` (formerly the "shouldn't happen" default) is
	// preserved — any unknown enum value is treated as VoxelLab,
	// matching the original contract.
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
	// **Tier 1.E (`2026-06-13`).** `ParseVoxelScenePreset` returns
	// `std::optional<VoxelScenePreset>`; `.value_or(default)` replaces
	// the old `if (TryParse(...)) return out;` out-param pattern.
	if (const auto parsedPreset = ParseVoxelScenePreset(requestedPreset)) {
		return *parsedPreset;
	}

	SDL_Log(
		"Unknown PROJECTV_SCENE_PRESET='%s'; using %s",
		requestedPreset,
		std::string{VoxelScenePresetToString(kDefaultVoxelScenePreset)}.c_str());
	return kDefaultVoxelScenePreset;
}

std::string GetVoxelWorldSnapshotPath()
{
	const char *requestedSnapshotPath = SDL_getenv("PROJECTV_SNAPSHOT_PATH");
	if (requestedSnapshotPath && *requestedSnapshotPath) {
		return requestedSnapshotPath;
	}

	const char *basePath = SDL_GetBasePath();
	if (basePath && *basePath) {
		const std::filesystem::path snapshotPath =
			std::filesystem::path(basePath) / kDefaultVoxelWorldSnapshotFilename;
		return snapshotPath.string();
	}
	return kDefaultVoxelWorldSnapshotFilename;
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
	state->world.snapshotSaveRequested = false;
	state->world.snapshotLoadRequested = false;
	if (state->world.voxelWorld) {
		SDL_Log(
			"Using voxel scene preset: %s",
			std::string{VoxelScenePresetToString(state->world.voxelWorld->scenePreset)}.c_str());
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
	state->world.snapshotSaveRequested = false;
	state->world.snapshotLoadRequested = false;
}

std::expected<bool, projectv::voxel::VoxelSnapshotError> SaveVoxelWorldSnapshot(const VoxelWorld &world, const std::string_view snapshotPath)
{
	// **Tier 1.B (`2026-06-13`).** Returns `std::expected<bool,
	// VoxelSnapshotError>` instead of `bool`. Each early-return
	// site is a `std::unexpected(VoxelSnapshotError::Variant)`
	// that names the failure. The original log-line is preserved
	// via `runtime::LogRuntimeFailure(subsystem, step, detail)`
	// so the operator's log greps still work; the error variant
	// is the new machine-readable channel for callers that want
	// to react programmatically (e.g. `auto _ = save(...).or_else([](auto e){
	//   return fallbackWorldFromPreset(); });`).
	const auto fail = [](projectv::voxel::VoxelSnapshotError e, std::string_view step, std::string_view detail) {
		runtime::LogRuntimeFailure("VoxelWorld", step, detail);
		return std::unexpected(e);
	};
	if (snapshotPath.empty()) {
		return fail(projectv::voxel::VoxelSnapshotError::EmptyPath,
			"SaveVoxelWorldSnapshot.Path", "snapshot path is empty");
	}

	const std::filesystem::path resolvedPath = ResolveVoxelWorldSnapshotPath(snapshotPath);
	std::error_code createDirectoriesError;
	const std::filesystem::path parentPath = resolvedPath.parent_path();
	if (!parentPath.empty() &&
		!std::filesystem::create_directories(parentPath, createDirectoriesError) &&
		createDirectoriesError) {
		return fail(projectv::voxel::VoxelSnapshotError::CreateDirectoriesFailed,
			"SaveVoxelWorldSnapshot.CreateDirectories", createDirectoriesError.message());
	}

	VoxelWorldSnapshotHeader header{};
	header.magic = kVoxelWorldSnapshotMagic;
	header.version = kVoxelWorldSnapshotVersion;
	ClearSnapshotReservedFields(&header);
	if (world.voxels.size() > std::numeric_limits<uint32_t>::max()) {
		return fail(projectv::voxel::VoxelSnapshotError::VoxelBufferTooLarge,
			"SaveVoxelWorldSnapshot.Size", "voxel buffer exceeds snapshot format limit");
	}
	header.voxelByteCount = static_cast<uint32_t>(world.voxels.size());
	header.scenePreset = static_cast<uint8_t>(world.scenePreset);
	header.config = world.config;
	header.min = world.min;
	header.maxExclusive = world.maxExclusive;
	header.editVersion = world.editVersion;

	std::ofstream file(resolvedPath, std::ios::binary | std::ios::trunc);
	if (!file.is_open()) {
		return fail(projectv::voxel::VoxelSnapshotError::OpenForWriteFailed,
			"SaveVoxelWorldSnapshot.Open", "failed to open snapshot file for write: " + resolvedPath.string());
	}

	file.write(reinterpret_cast<const char *>(&header), sizeof(header));
	if (!world.voxels.empty()) {
		file.write(
			reinterpret_cast<const char *>(world.voxels.data()),
			static_cast<std::streamsize>(world.voxels.size()));
	}
	if (!file.good()) {
		return fail(projectv::voxel::VoxelSnapshotError::WriteFailed,
			"SaveVoxelWorldSnapshot.Write", "failed to write snapshot file: " + resolvedPath.string());
	}

	SDL_Log("Saved voxel world snapshot: %s", resolvedPath.string().c_str());
	return true;
}

std::expected<std::unique_ptr<VoxelWorld>, projectv::voxel::VoxelSnapshotError> LoadVoxelWorldSnapshot(const std::string_view snapshotPath)
{
	// **Tier 1.B (`2026-06-13`).** Returns `std::expected<unique_ptr<VoxelWorld>,
	// VoxelSnapshotError>`. Each early-return site is a
	// `std::unexpected(VoxelSnapshotError::Variant)` so callers
	// can match on the exact failure (e.g.
	// `.transform_error([](auto e){ return fallback(e); })`).
	const auto fail = [](projectv::voxel::VoxelSnapshotError e, std::string_view step, std::string_view detail) {
		runtime::LogRuntimeFailure("VoxelWorld", step, detail);
		return std::unexpected(e);
	};
	if (snapshotPath.empty()) {
		return fail(projectv::voxel::VoxelSnapshotError::EmptyPath,
			"LoadVoxelWorldSnapshot.Path", "snapshot path is empty");
	}

	const std::filesystem::path resolvedPath = ResolveVoxelWorldSnapshotPath(snapshotPath);
	std::error_code fileSizeError;
	const std::uintmax_t fileSize = std::filesystem::file_size(resolvedPath, fileSizeError);
	if (fileSizeError) {
		return fail(projectv::voxel::VoxelSnapshotError::FileSizeQueryFailed,
			"LoadVoxelWorldSnapshot.FileSize", fileSizeError.message());
	}
	if (fileSize < sizeof(VoxelWorldSnapshotHeader)) {
		return fail(projectv::voxel::VoxelSnapshotError::FileTooSmall,
			"LoadVoxelWorldSnapshot.FileSize", "snapshot file is smaller than the header");
	}

	std::ifstream file(resolvedPath, std::ios::binary);
	if (!file.is_open()) {
		return fail(projectv::voxel::VoxelSnapshotError::OpenForReadFailed,
			"LoadVoxelWorldSnapshot.Open", "failed to open snapshot file for read: " + resolvedPath.string());
	}

	VoxelWorldSnapshotHeader header{};
	file.read(reinterpret_cast<char *>(&header), sizeof(header));
	if (!file.good()) {
		return fail(projectv::voxel::VoxelSnapshotError::ReadHeaderFailed,
			"LoadVoxelWorldSnapshot.ReadHeader", "failed to read snapshot header: " + resolvedPath.string());
	}

	if (header.magic != kVoxelWorldSnapshotMagic) {
		return fail(projectv::voxel::VoxelSnapshotError::MagicMismatch,
			"LoadVoxelWorldSnapshot.Header", "snapshot magic mismatch");
	}
	if (header.version != kVoxelWorldSnapshotVersion) {
		return fail(projectv::voxel::VoxelSnapshotError::UnsupportedVersion,
			"LoadVoxelWorldSnapshot.Header", "unsupported snapshot version");
	}
	if (!IsValidVoxelScenePresetValue(header.scenePreset)) {
		return fail(projectv::voxel::VoxelSnapshotError::InvalidScenePreset,
			"LoadVoxelWorldSnapshot.Header", "snapshot scene preset is invalid");
	}
	if (!HasClearSnapshotReservedFields(header)) {
		return fail(projectv::voxel::VoxelSnapshotError::ReservedFieldsNonZero,
			"LoadVoxelWorldSnapshot.Header", "snapshot reserved fields must stay zero");
	}
	if (header.config.chunkSize <= 0) {
		return fail(projectv::voxel::VoxelSnapshotError::InvalidChunkSize,
			"LoadVoxelWorldSnapshot.Header", "snapshot chunk size must stay positive");
	}

	std::unique_ptr<VoxelWorld> world = CreateEmptyVoxelWorld(
		header.config,
		static_cast<VoxelScenePreset>(header.scenePreset),
		header.min,
		header.maxExclusive);
	if (!world) {
		return fail(projectv::voxel::VoxelSnapshotError::CreateWorldFailed,
			"LoadVoxelWorldSnapshot.CreateWorld", "failed to create world layout for snapshot");
	}
	if (world->voxels.size() != header.voxelByteCount) {
		return fail(projectv::voxel::VoxelSnapshotError::VoxelCountMismatch,
			"LoadVoxelWorldSnapshot.Header", "snapshot voxel count does not match world layout");
	}
	if (fileSize != sizeof(VoxelWorldSnapshotHeader) + header.voxelByteCount) {
		return fail(projectv::voxel::VoxelSnapshotError::FileSizeMismatch,
			"LoadVoxelWorldSnapshot.FileSize", "snapshot file size does not match header payload size");
	}

	if (header.voxelByteCount > 0) {
		file.read(
			reinterpret_cast<char *>(world->voxels.data()),
			header.voxelByteCount);
		if (!file.good()) {
			return fail(projectv::voxel::VoxelSnapshotError::ReadPayloadFailed,
				"LoadVoxelWorldSnapshot.ReadPayload", "failed to read snapshot payload: " + resolvedPath.string());
		}
	}

	for (const uint8_t materialValue : world->voxels) {
		if (!IsValidVoxelMaterialValue(materialValue)) {
			return fail(projectv::voxel::VoxelSnapshotError::InvalidVoxelMaterial,
				"LoadVoxelWorldSnapshot.Payload", "snapshot contains invalid voxel material id");
		}
	}

	world->editVersion = header.editVersion;
	RebuildVoxelWorldDerivedState(*world);
	SDL_Log("Loaded voxel world snapshot: %s", resolvedPath.string().c_str());
	return world;
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

Int3 GetVoxelChunkCoord(const VoxelWorld &world, const Int3 position)
{
	return {
		(position.x - world.min.x) / world.chunkSize,
		(position.y - world.min.y) / world.chunkSize,
		(position.z - world.min.z) / world.chunkSize,
	};
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

	const Int3 chunkCoord = GetVoxelChunkCoord(world, position);
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

	const auto [firstChunkX, firstChunkY, firstChunkZ] = GetVoxelChunkCoord(world, clampedMin);
	const auto [lastChunkX, lastChunkY, lastChunkZ] =
		GetVoxelChunkCoord(world, {clampedMax.x - 1, clampedMax.y - 1, clampedMax.z - 1});

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

	VoxelChunk &chunk = world.chunks[GetVoxelChunkIndex(world, GetVoxelChunkCoord(world, position))];
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

	// A voxel edit only needs its own chunk plus face-sharing border neighbors.
	MarkChunksTouchedByVoxelEditDirty(world, position);
}

uint32_t FillVoxelMaterial(VoxelWorld &world, const Int3 start, const VoxelMaterial material)
{
	if (!IsInsideVoxelWorld(world, start)) {
		return 0;
	}

	const VoxelMaterial sourceMaterial = GetVoxelMaterial(world, start);
	if (sourceMaterial == material) {
		return 0;
	}

	std::vector<uint8_t> visited(world.voxels.size(), 0u);
	std::vector<Int3> queue;
	queue.reserve(256);

	const auto tryEnqueue = [&](const Int3 position) {
		if (!IsInsideVoxelWorld(world, position)) {
			return;
		}

		const size_t voxelIndex = ToVoxelIndex(world, position);
		if (visited[voxelIndex] != 0u) {
			return;
		}

		visited[voxelIndex] = 1u;
		if (static_cast<VoxelMaterial>(world.voxels[voxelIndex]) != sourceMaterial) {
			return;
		}

		queue.push_back(position);
	};

	tryEnqueue(start);
	for (size_t queueIndex = 0; queueIndex < queue.size(); ++queueIndex) {
		const auto [x, y, z] = queue[queueIndex];
		tryEnqueue({x - 1, y, z});
		tryEnqueue({x + 1, y, z});
		tryEnqueue({x, y - 1, z});
		tryEnqueue({x, y + 1, z});
		tryEnqueue({x, y, z - 1});
		tryEnqueue({x, y, z + 1});
	}

	for (const Int3 position : queue) {
		SetVoxelMaterial(world, position, material);
	}

	return static_cast<uint32_t>(queue.size());
}

uint32_t FillVoxelBox(VoxelWorld &world, const Int3 first, const Int3 second, const VoxelMaterial material)
{
	const Int3 min{
		std::min(first.x, second.x),
		std::min(first.y, second.y),
		std::min(first.z, second.z),
	};
	const Int3 max{
		std::max(first.x, second.x),
		std::max(first.y, second.y),
		std::max(first.z, second.z),
	};
	const Int3 clampedMin{
		std::max(min.x, world.min.x),
		std::max(min.y, world.min.y),
		std::max(min.z, world.min.z),
	};
	const Int3 clampedMax{
		std::min(max.x, world.maxExclusive.x - 1),
		std::min(max.y, world.maxExclusive.y - 1),
		std::min(max.z, world.maxExclusive.z - 1),
	};
	if (clampedMin.x > clampedMax.x ||
		clampedMin.y > clampedMax.y ||
		clampedMin.z > clampedMax.z) {
		return 0;
	}

	uint32_t changedVoxelCount = 0;
	for (int z = clampedMin.z; z <= clampedMax.z; ++z) {
		for (int y = clampedMin.y; y <= clampedMax.y; ++y) {
			for (int x = clampedMin.x; x <= clampedMax.x; ++x) {
				const Int3 position{x, y, z};
				if (GetVoxelMaterial(world, position) == material) {
					continue;
				}

				SetVoxelMaterial(world, position, material);
				++changedVoxelCount;
			}
		}
	}

	return changedVoxelCount;
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

// **Fluid cellular automata (defense r0, 2026-06-13; audited 2026-06-13).**
// See header doc-comment for the per-tick contract + determinism guarantees.
// This is a small bounded forward-shader-style implementation: one CA step
// per call, **fall-only** (spread rule was removed in the 2026-06-13
// audit — see `agent/decisions.md §30`). Cheap enough to run on every
// fixed simulation tick (1/60 s).
uint32_t UpdateFluidCA(VoxelWorld &world)
{
	// Fast-path: a world with no fluid never needs a tick. This is the
	// only early-out in the function; everything else runs even when
	// the world has 0 fluid voxels that *can* move (e.g. fluid already
	// settled on a floor), so the per-tick cost stays predictable for
	// performance instrumentation.
	if (world.stats.fluidVoxelCount == 0u || world.voxels.empty()) {
		return 0u;
	}

	const int width = world.width;
	const int height = world.height;
	const int depth = world.depth;

	// **Pre-condition invariants** (debug-only). The world is built by
	// `CreateEmptyVoxelWorld` which guarantees these, but a
	// hand-constructed test world or a corrupt snapshot could violate
	// them; `PV_ASSERT` is the cheapest way to catch that before the
	// CA does an out-of-bounds read.
#if !defined(NDEBUG)
	{
		const size_t expectedVoxelCount = static_cast<size_t>(width) *
			static_cast<size_t>(height) * static_cast<size_t>(depth);
		PV_ASSERT(
			world.voxels.size() == expectedVoxelCount,
			"VoxelWorld",
			"UpdateFluidCA",
			"world.voxels.size() must equal width*height*depth");
		PV_ASSERT(
			width > 0 && height > 0 && depth > 0,
			"VoxelWorld",
			"UpdateFluidCA",
			"world dimensions must be strictly positive");
	}
#endif

	const auto index = [width, height](const int x, const int y, const int z) -> size_t {
		return static_cast<size_t>(x) + static_cast<size_t>(y) * static_cast<size_t>(width)
			 + static_cast<size_t>(z) * static_cast<size_t>(width) * static_cast<size_t>(height);
	};

	// 1. Double-buffer copy of the voxel array. Cheap: 4-8 KB for the
	//    bounded `VoxelLab` scene. Avoids read-after-write hazards in a
	//    single tick: we read from `world.voxels` (the immutable
	//    snapshot of the current state) and write into `next` (the new
	//    state), then swap at the end. The allocation is required for
	//    determinism — without the snapshot, a fluid at `(x, 5, z)`
	//    could see the (x, 4, z) that was *just written* to `next` and
	//    either fall into it (if we wrote Fluid) or fail to fall (if
	//    we wrote Air), depending on the iteration order.
	std::vector<uint8_t> next = world.voxels;

	// **Claimed tracking (2026-06-13 spread restore).** When a fluid
	// falls or spreads, the destination cell is "claimed" by the
	// source — a later iteration of the CA pass must not also process
	// the destination as a source, or the destination will be
	// overwritten and the source's fluid is "lost" (the "swap" bug:
	// two adjacent fluids both want to spread into each other, both
	// succeed (last-write-wins), one fluid disappears). Per-cell
	// `uint8_t` flag (0 = unclaimed, 1 = claimed). Cheap: 1 byte per
	// voxel ≈ 10 KB for the production `VoxelLab` world.
	std::vector<uint8_t> claimed(world.voxels.size(), 0u);

	uint32_t movedCount = 0u;

	// 2. Pass: scan the world in `z, y, x` order with **y ascending**
	//    (bottom-up). The bottom-up order is critical: a fluid at
	//    `(x, 4, z)` is processed BEFORE the fluid at `(x, 5, z)`, so
	//    the `(x, 4, z)` fluid falls to `(x, 3, z)` first; the
	//    `(x, 5, z)` fluid then reads `world.voxels[(x, 4, z)]`
	//    (still the original Fluid in the immutable snapshot) and
	//    does **not** fall. Net: 1 tick = 1 cell of gravity per
	//    column. A top-down pass would cause the `(x, 5, z)` fluid to
	//    fall into `(x, 4, z)`'s target (x, 3, z), giving 2 cells per
	//    tick ("double-step") which the audit found undesirable.
	for (int z = 0; z < depth; ++z) {
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				const size_t idx = index(x, y, z);
				if (world.voxels[idx] != static_cast<uint8_t>(VoxelMaterial::Fluid)) {
					continue;
				}
				// **Source claim check:** if this cell was the
				// destination of an earlier cell's fall/spread in
				// the same tick, it is no longer an "original" source
				// — it has been "consumed" by the spread/fall that
				// moved into it. Processing it again would overwrite
				// the destination write and lose the original
				// source's fluid (the "swap" bug). The earlier
				// source's `next[idx]` write to Fluid is unchanged
				// by us skipping, so the count is preserved.
				if (claimed[idx] != 0u) {
					continue;
				}

				// **f_fall rule:** if the cell below is `Air`,
				// swap this cell to `Air` and the cell below to
				// `Fluid`. The `y > 0` check is the world-floor
				// guard: a fluid at y=0 cannot fall (it is at
				// the bottom of the world; everything below is
				// out of bounds). Fluid resting on `Glass`,
				// `FloorWhite`, `FloorGray`, or another `Fluid`
				// does **not** fall — the snapshot read keeps the
				// check stable even when the same column has
				// multiple fluid voxels stacked.
				//
				// **2026-06-13 audit note:** an earlier version of
				// this rule also allowed falling through
				// `FloorWhite`/`FloorGray` (treating the
				// platform/columns as "permeable" so the water
				// could flow off the platform). That worked but
				// had a critical side-effect: the water overwrote
				// the `FloorWhite`/`FloorGray` cell at the
				// destination, leaving an Air "hole" at the
				// source. The operator reported "платформа
				// исчезает из-за воды" — the platform disappears
				// because of water. The fix is to keep the
				// platform/columns intact: the fall rule is
				// restricted to `Air` only. Water on the
				// platform drains off via the spread rule (which
				// also lets the water spread to Air sides and from
				// there fall to the floor below).
				if (y > 0) {
					const size_t belowIdx = index(x, y - 1, z);
					// **Fall target check uses `next`, not
					// `world.voxels`**, same as the spread rule.
					// A fall is only allowed if the cell below is
					// Air in the **new** state (`next`). Without
					// this, a fluid in column A (at y=1) could fall
					// to (x, 0, z) while another fluid (in column
					// B, also at y=0) is **already** spreading into
					// (x, 0, z) — both succeed and the second
					// overwrites the first (losing a fluid). With
					// this check, the fall is rejected if the
					// destination is already claimed, and the
					// source falls back to spread (or stays put if
					// no spread is possible).
					if (world.voxels[belowIdx] == static_cast<uint8_t>(VoxelMaterial::Air)
						&& next[belowIdx] == static_cast<uint8_t>(VoxelMaterial::Air)) {
						next[idx] = static_cast<uint8_t>(VoxelMaterial::Air);
						next[belowIdx] = static_cast<uint8_t>(VoxelMaterial::Fluid);
						// Mark both source and destination as
						// "claimed" — see the comment above on the
						// "swap" bug. Marking the source is
						// belt-and-suspenders (the iteration order
						// processes each cell exactly once), but the
						// destination claim is the critical one:
						// without it, a fluid voxel at the
						// destination (in the snapshot) would also
						// try to act, overwriting our write.
						claimed[idx] = 1u;
						claimed[belowIdx] = 1u;
						++movedCount;
						continue;
					}
				}

				// **f_spread rule (2-direction perpendicular):** if
				// the fluid did **not** fall this tick (it is at
				// rest on something solid), try to spread to two
				// perpendicular cardinal neighbours at the same
				// y-level. The first direction is hash-determined
				// from `(x, y, z)`; the second is the perpendicular
				// (`startSide + 1` & 3, i.e. rotated 90° in the
				// `sides` table). This produces an "L" of fluid per
				// source per tick: the source becomes Air, the two
				// perpendicular neighbours become Fluid.
				//
				// **Why 2 perpendicular directions (not 1 or 4)?**
				// The 2026-06-13 single-direction spread produced
				// "stripes" — adjacent cells with matching hash
				// start sides all moved in the same direction,
				// leaving a checkerboard of empty cells. The
				// operator reported "вода неравномерно заполняет
				// пустоты" (water doesn't fill the gaps evenly).
				// 4-direction spread ("plus" shape) would fill
				// faster but explodes the count by 3 per cell per
				// tick — too aggressive for the VoxelLab scale.
				// 2 perpendicular directions grows the count by
				// 1 per cell per tick (1 source → Air, 2
				// destinations → Fluid), and the L-shape grows
				// into a square/blob over a few ticks. The exact
				// 2 directions (perpendicular vs opposite) is a
				// tunable: perpendicular gives a more "square"
				// local footprint, opposite gives a "line"
				// footprint. The 2026-06-13 follow-up picked
				// perpendicular for better gap-filling.
				//
				// **Target check uses `next`, not `world.voxels`**:
				// a target cell is "spreads-allowed" only if it is
				// still Air in the **new** state. If a previous
				// source in this same tick has already written
				// `next[neighbour] = Fluid`, the second source's
				// spread to that cell is rejected. This prevents
				// the "swap" bug where two adjacent sources both
				// succeed and the second overwrites the first (losing
				// one fluid voxel per swap).
				//
				// **Determinism note:** the hash `x*73856093u ^
				// y*19349663u ^ z*83492791u` is identical to the
				// 2026-06-12 original. `*` binds tighter than `^` in
				// C++ (per `[expr.mul]` / `[expr.xor]` precedence), so
				// the expression is parsed as
				// `((x*p1) ^ (y*p2)) ^ (z*p3)`, fully defined for
				// 32-bit unsigned arithmetic. The prime constants
				// are the same as in the Teschner et al. (2003)
				// spatial hash.
				{
					const uint32_t h = static_cast<uint32_t>(
						(x * 73856093u) ^ (y * 19349663u) ^ (z * 83492791u));
					const int sides[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
					const int startSide = static_cast<int>(h & 0x3u);
					// Two perpendicular directions: the hash one
					// and the one rotated 90° (the next side in
					// the `sides` table). Opposite (startSide+2)
					// was tried first and produced "line" patterns
					// that didn't fill 2D gaps; perpendicular gives
					// a square footprint.
					const int dirs[2] = {startSide, (startSide + 1) & 0x3};
					// **Strict count conservation (2026-06-14).** The
					// earlier "spread = 2 destinations, source stays
					// Fluid" rule grew the fluid count by 1 per cell
					// per tick — i.e. water was *cloning itself*
					// (`вода дюпается, клонируется`). The fix is
					// **swap semantics**: source (Fluid) → Air, every
					// successful destination → Fluid. Net change per
					// spread = `0` if both directions succeed (1 source
					// removed, 2 added — actually `+1`!), `−1` if only
					// one direction succeeds (1 removed, 1 added),
					// and `0` (no change) if no direction succeeds.
					//
					// Hmm — that's not conserved either. The real
					// conservation invariant for "swap L-shape spread"
					// is: **a tick can perform at most one of {fall,
					// spread, stay}; if spread, source turns to Air
					// and exactly one destination becomes Fluid**
					// (not two). This is the only way to keep count
					// strictly conserved while still producing
					// meaningful horizontal movement.
					//
					// Below: we try both perpendicular directions,
					// but only the **first** one that succeeds flips
					// the source to Air. The second direction is
					// skipped entirely (no second destination write).
					// Net change: source Air, 1 destination Fluid =
					// exactly 0. **The "L-shape" visual is lost**, but
					// count conservation is restored. The trade-off
					// is documented in `agent/decisions.md §30`
					// (2026-06-14 follow-up) and `agent/memory.md
					// §12` (item "2-direction perpendicular spread
					// count growth").
					//
					// If the operator later wants both perpendicular
					// destinations to succeed (L-shape) at the cost of
					// a `+1` per source per tick, change `if
					// (spreadCount > 0)` to `if (spreadCount == 2)`
					// and accept the count growth.
					int spreadDir = -1;
					for (int d = 0; d < 2; ++d) {
						const int sideIdx = dirs[d];
						const int nx = x + sides[sideIdx][0];
						const int nz = z + sides[sideIdx][1];
						if (nx < 0 || nx >= width || nz < 0 || nz >= depth) {
							continue;
						}
						const size_t neighbourIdx = index(nx, y, nz);
						if (next[neighbourIdx] == static_cast<uint8_t>(VoxelMaterial::Air)) {
							spreadDir = d;
							break;
						}
					}
					if (spreadDir >= 0) {
						const int sideIdx = dirs[spreadDir];
						const int nx = x + sides[sideIdx][0];
						const int nz = z + sides[sideIdx][1];
						const size_t neighbourIdx = index(nx, y, nz);
						next[idx] = static_cast<uint8_t>(VoxelMaterial::Air);
						next[neighbourIdx] = static_cast<uint8_t>(VoxelMaterial::Fluid);
						claimed[idx] = 1u;
						claimed[neighbourIdx] = 1u;
						++movedCount;
					}
				}
			}
		}
	}

	if (movedCount == 0u) {
		return 0u;
	}

	// 3. Commit the new state through the public `SetVoxelMaterial`
	//    path so all downstream counters (chunk dirty flags,
	//    `fluidVoxelCount`, `activeChunkCount`, etc.) stay consistent.
	//    The pixel-by-pixel rewrite is fine for MVP-scale scenes; for
	//    larger worlds a chunk-level delta would be the next step.
	//
	//    **Coordinate convention:** the loop iterates **local** indices
	//    `x ∈ [0, width)`, `y ∈ [0, height)`, `z ∈ [0, depth)` (matching
	//    the CA pass above), but `SetVoxelMaterial` expects **world**
	//    coordinates. `world.min` is the offset from local to world, so
	//    we add it before calling `SetVoxelMaterial`. **Without this
	//    offset, the production VoxelLab scene (`min = (-12, 0, -12)`)
	//    silently dropped most fall commits:** `SetVoxelMaterial`
	//    would receive local coords like `{12, 3, 12}` and map them to
	//    world `{12, 3, 12}`, which is **at the world edge** (x == 12
	//    == `maxExclusive.x`) and rejected by `IsInsideVoxelWorld`. For
	//    cells with local x < 12, the commit landed at the wrong world
	//    position (local (5, 3, 5) → world (5, 3, 5) → local (17, 3,
	//    17)), silently corrupting the voxel array. **This was the root
	//    cause of the user's report that "water doesn't fall" in
	//    VoxelLab.** The 2026-06-13 audit surfaced it via the new
	//    `TestFluidCAVoxelLabSphereFallOnGlassBreak` test, which uses
	//    a `min = (-12, 0, -12)` world to mirror the production
	//    `VoxelLab` scene's offset.
	const Int3 min = world.min;
	for (int z = 0; z < depth; ++z) {
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				const size_t idx = index(x, y, z);
				const uint8_t previous = world.voxels[idx];
				const uint8_t current = next[idx];
				if (previous == current) {
					continue;
				}
				const auto material = static_cast<VoxelMaterial>(current);
				SetVoxelMaterial(
					world,
					{min.x + x, min.y + y, min.z + z},
					material);
			}
		}
	}

	// 4. Post-condition invariant (debug-only). `SetVoxelMaterial`
	//    is the only place that mutates `stats.fluidVoxelCount`, and
	//    the commit loop is the only caller per tick; the counts
	//    should match the array contents exactly. A mismatch would
	//    indicate a bug in `AccumulateMaterialCount` or in
	//    `SetVoxelMaterial`'s delta path, **not** in the CA itself,
	//    but it's cheap to verify here.
#if !defined(NDEBUG)
	{
		size_t actualFluidCount = 0u;
		for (const uint8_t voxel : world.voxels) {
			if (voxel == static_cast<uint8_t>(VoxelMaterial::Fluid)) {
				++actualFluidCount;
			}
		}
		PV_ASSERT(
			static_cast<uint32_t>(actualFluidCount) == world.stats.fluidVoxelCount,
			"VoxelWorld",
			"UpdateFluidCA",
			"stats.fluidVoxelCount diverged from actual fluid voxel count");
	}
#endif

	return movedCount;
}

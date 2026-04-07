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
	state->world.snapshotSaveRequested = false;
	state->world.snapshotLoadRequested = false;
}

bool SaveVoxelWorldSnapshot(const VoxelWorld &world, const std::string_view snapshotPath)
{
	if (snapshotPath.empty()) {
		runtime::LogRuntimeFailure(
			"VoxelWorld",
			"SaveVoxelWorldSnapshot.Path",
			"snapshot path is empty");
		return false;
	}

	const std::filesystem::path resolvedPath = ResolveVoxelWorldSnapshotPath(snapshotPath);
	std::error_code createDirectoriesError;
	const std::filesystem::path parentPath = resolvedPath.parent_path();
	if (!parentPath.empty() &&
		!std::filesystem::create_directories(parentPath, createDirectoriesError) &&
		createDirectoriesError) {
		runtime::LogRuntimeFailure(
			"VoxelWorld",
			"SaveVoxelWorldSnapshot.CreateDirectories",
			createDirectoriesError.message());
		return false;
	}

	VoxelWorldSnapshotHeader header{};
	header.magic = kVoxelWorldSnapshotMagic;
	header.version = kVoxelWorldSnapshotVersion;
	ClearSnapshotReservedFields(&header);
	if (world.voxels.size() > std::numeric_limits<uint32_t>::max()) {
		runtime::LogRuntimeFailure(
			"VoxelWorld",
			"SaveVoxelWorldSnapshot.Size",
			"voxel buffer exceeds snapshot format limit");
		return false;
	}
	header.voxelByteCount = static_cast<uint32_t>(world.voxels.size());
	header.scenePreset = static_cast<uint8_t>(world.scenePreset);
	header.config = world.config;
	header.min = world.min;
	header.maxExclusive = world.maxExclusive;
	header.editVersion = world.editVersion;

	std::ofstream file(resolvedPath, std::ios::binary | std::ios::trunc);
	if (!file.is_open()) {
		runtime::LogRuntimeFailure(
			"VoxelWorld",
			"SaveVoxelWorldSnapshot.Open",
			"failed to open snapshot file for write: " + resolvedPath.string());
		return false;
	}

	file.write(reinterpret_cast<const char *>(&header), sizeof(header));
	if (!world.voxels.empty()) {
		file.write(
			reinterpret_cast<const char *>(world.voxels.data()),
			static_cast<std::streamsize>(world.voxels.size()));
	}
	if (!file.good()) {
		runtime::LogRuntimeFailure(
			"VoxelWorld",
			"SaveVoxelWorldSnapshot.Write",
			"failed to write snapshot file: " + resolvedPath.string());
		return false;
	}

	SDL_Log("Saved voxel world snapshot: %s", resolvedPath.string().c_str());
	return true;
}

std::unique_ptr<VoxelWorld> LoadVoxelWorldSnapshot(const std::string_view snapshotPath)
{
	if (snapshotPath.empty()) {
		runtime::LogRuntimeFailure(
			"VoxelWorld",
			"LoadVoxelWorldSnapshot.Path",
			"snapshot path is empty");
		return nullptr;
	}

	const std::filesystem::path resolvedPath = ResolveVoxelWorldSnapshotPath(snapshotPath);
	std::error_code fileSizeError;
	const std::uintmax_t fileSize = std::filesystem::file_size(resolvedPath, fileSizeError);
	if (fileSizeError) {
		runtime::LogRuntimeFailure(
			"VoxelWorld",
			"LoadVoxelWorldSnapshot.FileSize",
			fileSizeError.message());
		return nullptr;
	}
	if (fileSize < sizeof(VoxelWorldSnapshotHeader)) {
		runtime::LogRuntimeFailure(
			"VoxelWorld",
			"LoadVoxelWorldSnapshot.FileSize",
			"snapshot file is smaller than the header");
		return nullptr;
	}

	std::ifstream file(resolvedPath, std::ios::binary);
	if (!file.is_open()) {
		runtime::LogRuntimeFailure(
			"VoxelWorld",
			"LoadVoxelWorldSnapshot.Open",
			"failed to open snapshot file for read: " + resolvedPath.string());
		return nullptr;
	}

	VoxelWorldSnapshotHeader header{};
	file.read(reinterpret_cast<char *>(&header), sizeof(header));
	if (!file.good()) {
		runtime::LogRuntimeFailure(
			"VoxelWorld",
			"LoadVoxelWorldSnapshot.ReadHeader",
			"failed to read snapshot header: " + resolvedPath.string());
		return nullptr;
	}

	if (header.magic != kVoxelWorldSnapshotMagic) {
		runtime::LogRuntimeFailure(
			"VoxelWorld",
			"LoadVoxelWorldSnapshot.Header",
			"snapshot magic mismatch");
		return nullptr;
	}
	if (header.version != kVoxelWorldSnapshotVersion) {
		runtime::LogRuntimeFailure(
			"VoxelWorld",
			"LoadVoxelWorldSnapshot.Header",
			"unsupported snapshot version");
		return nullptr;
	}
	if (!IsValidVoxelScenePresetValue(header.scenePreset)) {
		runtime::LogRuntimeFailure(
			"VoxelWorld",
			"LoadVoxelWorldSnapshot.Header",
			"snapshot scene preset is invalid");
		return nullptr;
	}
	if (!HasClearSnapshotReservedFields(header)) {
		runtime::LogRuntimeFailure(
			"VoxelWorld",
			"LoadVoxelWorldSnapshot.Header",
			"snapshot reserved fields must stay zero");
		return nullptr;
	}
	if (header.config.chunkSize <= 0) {
		runtime::LogRuntimeFailure(
			"VoxelWorld",
			"LoadVoxelWorldSnapshot.Header",
			"snapshot chunk size must stay positive");
		return nullptr;
	}

	std::unique_ptr<VoxelWorld> world = CreateEmptyVoxelWorld(
		header.config,
		static_cast<VoxelScenePreset>(header.scenePreset),
		header.min,
		header.maxExclusive);
	if (!world) {
		runtime::LogRuntimeFailure(
			"VoxelWorld",
			"LoadVoxelWorldSnapshot.CreateWorld",
			"failed to create world layout for snapshot");
		return nullptr;
	}
	if (world->voxels.size() != header.voxelByteCount) {
		runtime::LogRuntimeFailure(
			"VoxelWorld",
			"LoadVoxelWorldSnapshot.Header",
			"snapshot voxel count does not match world layout");
		return nullptr;
	}
	if (fileSize != sizeof(VoxelWorldSnapshotHeader) + header.voxelByteCount) {
		runtime::LogRuntimeFailure(
			"VoxelWorld",
			"LoadVoxelWorldSnapshot.FileSize",
			"snapshot file size does not match header payload size");
		return nullptr;
	}

	if (header.voxelByteCount > 0) {
		file.read(
			reinterpret_cast<char *>(world->voxels.data()),
			header.voxelByteCount);
		if (!file.good()) {
			runtime::LogRuntimeFailure(
				"VoxelWorld",
				"LoadVoxelWorldSnapshot.ReadPayload",
				"failed to read snapshot payload: " + resolvedPath.string());
			return nullptr;
		}
	}

	for (const uint8_t materialValue : world->voxels) {
		if (!IsValidVoxelMaterialValue(materialValue)) {
			runtime::LogRuntimeFailure(
				"VoxelWorld",
				"LoadVoxelWorldSnapshot.Payload",
				"snapshot contains invalid voxel material id");
			return nullptr;
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

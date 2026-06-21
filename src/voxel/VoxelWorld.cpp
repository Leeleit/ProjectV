import projectv.math;
import projectv.string_id;

#include "voxel/VoxelWorld.hpp"
#include "voxel/VoxelLodDownsample.hpp"

#include "SDL3/SDL_log.h"
#include "core/RuntimeDiagnostics.hpp"
#include "core/Types.hpp"
#include "debug/Profiling.hpp"
#include "fmt/format.h"
#include "physics/PhysicsWorld.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <string>

namespace {
constexpr VoxelScenePreset kDefaultVoxelScenePreset = VoxelScenePreset::VoxelLab;
constexpr char kDefaultVoxelWorldSnapshotFilename[] = "ProjectV.snapshot.bin";
constexpr std::array kVoxelWorldSnapshotMagic{'P', 'V', 'S', 'N', 'A', 'P', '0', '1'};
constexpr uint32_t kVoxelWorldSnapshotVersion = 2u;

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

bool IsSparse64StorageEnabled()
{
	static const bool kEnabled = [] {
		const char *value = std::getenv("PROJECTV_SPARSE_64_STORAGE");
		if (value == nullptr) {
			return false;
		}
		const std::string v(value);
		return v == "on" || v == "1" || v == "true";
	}();
	return kEnabled;
}

void WriteVoxelToSparseStorage(VoxelWorld &world, const Int3 position, const uint8_t material)
{
	const int localX = position.x - world.min.x;
	const int localY = position.y - world.min.y;
	const int localZ = position.z - world.min.z;
	world.sparseStorage.SetCell(localX, localY, localZ, material);
}

uint8_t ReadVoxelFromSparseStorage(const VoxelWorld &world, const Int3 position)
{
	const int localX = position.x - world.min.x;
	const int localY = position.y - world.min.y;
	const int localZ = position.z - world.min.z;
	return world.sparseStorage.GetCell(localX, localY, localZ);
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
		static_cast<std::uint64_t>(voxelCount) > std::numeric_limits<size_t>::max()) {
		return false;
	}

	*outVoxelCount = static_cast<size_t>(voxelCount);
	return true;
}

void QueueChunkRebuildRequest(VoxelWorld &world, const size_t chunkIndex)
{
	VoxelChunk &chunk = world.chunks[chunkIndex];
	if (chunk.rebuildQueued) [[unlikely]] {
		return;
	}

	chunk.rebuildQueued = true;
	world.pendingChunkRebuildIndices.push_back(chunkIndex);
	world.pendingBlasRebuildIndices.push_back(chunkIndex);
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

std::filesystem::path ResolveVoxelWorldSnapshotPath(const std::string_view snapshotPath)
{
	return std::filesystem::path(std::string(snapshotPath));
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

std::expected<bool, projectv::voxel::VoxelSnapshotError> SaveVoxelWorldSnapshot(const VoxelWorld &world, const std::string_view snapshotPath)
{
	const auto fail = [](projectv::voxel::VoxelSnapshotError e, const std::string_view step, const std::string_view detail) {
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
	header.voxelByteCount = 0;
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

	const std::size_t numNodes = world.sparseStorage.NodeCount();
	if (numNodes > std::numeric_limits<uint32_t>::max()) {
		return fail(projectv::voxel::VoxelSnapshotError::VoxelBufferTooLarge,
					"SaveVoxelWorldSnapshot.Size", "sparse node count exceeds snapshot format limit");
	}
	const uint32_t numNodesU32 = static_cast<uint32_t>(numNodes);
	file.write(reinterpret_cast<const char *>(&numNodesU32), sizeof(numNodesU32));
	const uint32_t rootSlotU32 = world.sparseStorage.RootSlot();
	file.write(reinterpret_cast<const char *>(&rootSlotU32), sizeof(rootSlotU32));
	for (std::size_t i = 0; i < numNodes; ++i) {
		const projectv::voxel::Sparse64Tree::Node &node = world.sparseStorage.GetNodes()[i];
		file.write(reinterpret_cast<const char *>(&node), sizeof(node));
	}

	if (!file.good()) {
		return fail(projectv::voxel::VoxelSnapshotError::WriteFailed,
					"SaveVoxelWorldSnapshot.Write", "failed to write snapshot file: " + resolvedPath.string());
	}

	SDL_Log("Saved voxel world snapshot: %s (sparse nodes=%zu)", resolvedPath.string().c_str(), numNodes);
	return true;
}

std::expected<std::unique_ptr<VoxelWorld>, projectv::voxel::VoxelSnapshotError> LoadVoxelWorldSnapshot(const std::string_view snapshotPath)
{
	const auto fail = [](projectv::voxel::VoxelSnapshotError e, const std::string_view step, const std::string_view detail) {
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

	uint32_t numNodesU32 = 0;
	if (!file.read(reinterpret_cast<char *>(&numNodesU32), sizeof(numNodesU32))) {
		return fail(projectv::voxel::VoxelSnapshotError::ReadPayloadFailed,
					"LoadVoxelWorldSnapshot.ReadNodeCount", "failed to read sparse node count");
	}
	uint32_t rootSlotU32 = 0;
	if (!file.read(reinterpret_cast<char *>(&rootSlotU32), sizeof(rootSlotU32))) {
		return fail(projectv::voxel::VoxelSnapshotError::ReadPayloadFailed,
					"LoadVoxelWorldSnapshot.ReadRootSlot", "failed to read sparse root slot");
	}

	std::vector<projectv::voxel::Sparse64Tree::Node> nodes;
	nodes.reserve(numNodesU32);
	for (uint32_t i = 0; i < numNodesU32; ++i) {
		projectv::voxel::Sparse64Tree::Node node{};
		if (!file.read(reinterpret_cast<char *>(&node), sizeof(node))) {
			return fail(projectv::voxel::VoxelSnapshotError::ReadPayloadFailed,
						"LoadVoxelWorldSnapshot.ReadNode", "failed to read sparse node data");
		}
		nodes.push_back(node);
	}

	world->sparseStorage.RestoreFrom(rootSlotU32, std::move(nodes));

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
	return static_cast<VoxelMaterial>(ReadVoxelFromSparseStorage(world, position));
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

void SetVoxelMaterial(VoxelWorld &world, Int3 position, VoxelMaterial material, PhysicsState *physics)
{
	if (!IsInsideVoxelWorld(world, position)) {
		return;
	}

	const VoxelMaterial previousMaterial = GetVoxelMaterial(world, position);
	if (previousMaterial == material) {
		return;
	}

	const bool isFluidAirTransition = (previousMaterial == VoxelMaterial::Air && material == VoxelMaterial::Fluid) ||
									  (previousMaterial == VoxelMaterial::Fluid && material == VoxelMaterial::Air);

	WriteVoxelToSparseStorage(world, position, static_cast<uint8_t>(material));
	if (!isFluidAirTransition) {
		++world.editVersion;
	} else {
		profiling::PlotValue("Fluid Edit Version Bumps Suppressed", int64_t{1});
	}
	AccumulateMaterialCount(world.stats, previousMaterial, -1);
	AccumulateMaterialCount(world.stats, material, 1);

	if (previousMaterial == VoxelMaterial::Fluid || material == VoxelMaterial::Fluid) {
		world.fluidCAAabbMin.x = std::min(world.fluidCAAabbMin.x, position.x);
		world.fluidCAAabbMin.y = std::min(world.fluidCAAabbMin.y, position.y);
		world.fluidCAAabbMin.z = std::min(world.fluidCAAabbMin.z, position.z);
		world.fluidCAAabbMaxExclusive.x = std::max(world.fluidCAAabbMaxExclusive.x, position.x + 1);
		world.fluidCAAabbMaxExclusive.y = std::max(world.fluidCAAabbMaxExclusive.y, position.y + 1);
		world.fluidCAAabbMaxExclusive.z = std::max(world.fluidCAAabbMaxExclusive.z, position.z + 1);
	}

	VoxelChunk &chunk = world.chunks[GetVoxelChunkIndex(world, GetVoxelChunkCoord(world, position))];
	chunk.isStatic = false;
	chunk.ticksSinceLastEdit = 0;
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

	MarkChunksTouchedByVoxelEditDirty(world, position);

	if (physics != nullptr && !isFluidAirTransition) {
		const Int3 chunkCoord = GetVoxelChunkCoord(world, position);
		const VoxelChunk &centerChunk = world.chunks[GetVoxelChunkIndex(world, chunkCoord)];
		int minChunkX = chunkCoord.x;
		int maxChunkX = chunkCoord.x;
		int minChunkY = chunkCoord.y;
		int maxChunkY = chunkCoord.y;
		int minChunkZ = chunkCoord.z;
		int maxChunkZ = chunkCoord.z;
		if (position.x == centerChunk.min.x && chunkCoord.x > 0) {
			--minChunkX;
		}
		if (position.x == centerChunk.maxExclusive.x - 1 && chunkCoord.x + 1 < world.chunkCountX) {
			++maxChunkX;
		}
		if (position.y == centerChunk.min.y && chunkCoord.y > 0) {
			--minChunkY;
		}
		if (position.y == centerChunk.maxExclusive.y - 1 && chunkCoord.y + 1 < world.chunkCountY) {
			++maxChunkY;
		}
		if (position.z == centerChunk.min.z && chunkCoord.z > 0) {
			--minChunkZ;
		}
		if (position.z == centerChunk.maxExclusive.z - 1 && chunkCoord.z + 1 < world.chunkCountZ) {
			++maxChunkZ;
		}
		for (int dirtyChunkZ = minChunkZ; dirtyChunkZ <= maxChunkZ; ++dirtyChunkZ) {
			for (int dirtyChunkY = minChunkY; dirtyChunkY <= maxChunkY; ++dirtyChunkY) {
				for (int dirtyChunkX = minChunkX; dirtyChunkX <= maxChunkX; ++dirtyChunkX) {
					QueueChunkRebuildRequest(
						physics,
						GetVoxelChunkIndex(world, {dirtyChunkX, dirtyChunkY, dirtyChunkZ}));
				}
			}
		}
	}
}

uint32_t GetVoxelChunkStaticPromotionThreshold()
{
	if (const char *value = std::getenv("PROJECTV_SVDAG_STATIC_PROMOTION_TICKS")) {
		const int parsed = std::atoi(value);
		if (parsed > 0) {
			return static_cast<uint32_t>(parsed);
		}
	}
	return 60u;
}

void TickVoxelChunkStaticPromotion(VoxelWorld &world, const uint32_t threshold)
{
	for (VoxelChunk &chunk : world.chunks) {
		if (chunk.isStatic) {
			continue;
		}
		if (chunk.ticksSinceLastEdit < UINT32_MAX) {
			++chunk.ticksSinceLastEdit;
		}
		if (chunk.ticksSinceLastEdit >= threshold) {
			chunk.isStatic = true;
		}
	}
	if (world.sparseStorage.IsDeduplicationEnabled()) {
		world.sparseStorage.DedupPass();
	}
}

uint32_t CountStaticVoxelChunks(const VoxelWorld &world)
{
	uint32_t count = 0;
	for (const VoxelChunk &chunk : world.chunks) {
		if (chunk.isStatic) {
			++count;
		}
	}
	return count;
}

namespace {
bool gFluidCaGpuEnabledForTesting = false;
}

bool IsFluidCaGpuEnabled()
{
	if (const char *value = std::getenv("PROJECTV_FLUID_CA_GPU")) {
		return value[0] != '\0' && value[0] != '0';
	}
	return false;
}

void ToggleFluidCaGpuEnabledForTesting(const bool enabled)
{
	gFluidCaGpuEnabledForTesting = enabled;
}

std::vector<uint32_t> BuildActiveChunkIdsForFluidCa(const VoxelWorld &world)
{
	std::vector<uint32_t> active;
	active.reserve(world.chunks.size());
	for (size_t chunkIndex = 0; chunkIndex < world.chunks.size(); ++chunkIndex) {
		const VoxelChunk &chunk = world.chunks[chunkIndex];
		if (chunk.nonAirVoxelCount == 0u) {
			continue;
		}
		if (chunk.isStatic && (chunk.ticksSinceLastEdit < 30u)) {
			continue;
		}
		active.push_back(static_cast<uint32_t>(chunkIndex));
	}
	return active;
}

uint8_t SelectLodLevelForDistance(const float distanceMeters)
{
	if (distanceMeters < 32.0f) {
		return 0;
	}
	if (distanceMeters < 64.0f) {
		return 1;
	}
	if (distanceMeters < 128.0f) {
		return 2;
	}
	return 3;
}

uint32_t LodDownsampleStepForLod(const uint8_t lodLevel)
{
	return projectv::voxel::LodDownsampleStepForLod(lodLevel);
}

uint32_t LodDownsampledExtentForLod(const uint8_t lodLevel, const uint8_t chunkSize)
{
	return projectv::voxel::LodDownsampledExtentForLod(lodLevel, chunkSize);
}

void DownsampleChunkForLodSurfacePreserve(
	const VoxelWorld &world,
	const size_t chunkIndex,
	const uint8_t lodLevel,
	std::vector<uint8_t> &outDownsampled)
{
	projectv::voxel::DownsampleChunkForLodSurfacePreserve(world, chunkIndex, lodLevel, outDownsampled);
}

uint32_t RunLodDownsampleJobs(VoxelWorld &world)
{
	return projectv::voxel::RunLodDownsampleJobs(world);
}

bool IsLodDownsampleEnabled()
{
	return projectv::voxel::IsLodDownsampleEnabled();
}

void AssignLodLevels(VoxelWorld &world, const float cameraX, const float cameraY, const float cameraZ)
{
	for (VoxelChunk &chunk : world.chunks) {
		const float cx = 0.5f * static_cast<float>(chunk.min.x + chunk.maxExclusive.x);
		const float cy = 0.5f * static_cast<float>(chunk.min.y + chunk.maxExclusive.y);
		const float cz = 0.5f * static_cast<float>(chunk.min.z + chunk.maxExclusive.z);
		const float dx = cx - cameraX;
		const float dy = cy - cameraY;
		const float dz = cz - cameraZ;
		const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
		chunk.lodLevel = SelectLodLevelForDistance(distance);
	}
}

uint32_t CountChunksAtLod(const VoxelWorld &world, const uint8_t lodLevel)
{
	uint32_t count = 0;
	for (const VoxelChunk &chunk : world.chunks) {
		if (chunk.lodLevel == lodLevel) {
			++count;
		}
	}
	return count;
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

	const size_t totalCells = static_cast<size_t>(world.width) * world.height * world.depth;
	std::vector<uint8_t> visited(totalCells, 0u);
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
		if (GetVoxelMaterial(world, position) != sourceMaterial) {
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
		SetVoxelMaterial(world, position, material, nullptr);
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

				SetVoxelMaterial(world, position, material, nullptr);
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

void CollectDirtyVoxelChunkBlasRebuildRequests(VoxelWorld &world, std::vector<uint32_t> *outChunkIndices)
{
	if (!outChunkIndices || world.pendingBlasRebuildIndices.empty()) {
		return;
	}
	outChunkIndices->reserve(outChunkIndices->size() + world.pendingBlasRebuildIndices.size());
	for (const size_t index : world.pendingBlasRebuildIndices) {
		if (index <= UINT32_MAX) {
			outChunkIndices->push_back(static_cast<uint32_t>(index));
		}
	}
	world.pendingBlasRebuildIndices.clear();
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
	case VoxelMaterial::Air: {
		const size_t totalCells = static_cast<size_t>(world.width) * world.height * world.depth;
		return static_cast<uint32_t>(totalCells) - world.stats.nonAirVoxelCount;
	}
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

std::vector<uint8_t> BuildFlatVoxelSnapshot(const VoxelWorld &world)
{
	std::vector<uint8_t> flat;
	flat.reserve(static_cast<size_t>(world.width) * world.height * world.depth);
	for (int z = 0; z < world.depth; ++z) {
		for (int y = 0; y < world.height; ++y) {
			for (int x = 0; x < world.width; ++x) {
				flat.push_back(static_cast<uint8_t>(GetVoxelMaterial(world, {world.min.x + x, world.min.y + y, world.min.z + z})));
			}
		}
	}
	return flat;
}

uint32_t UpdateFluidCA(VoxelWorld &world)
{
	PV_PROFILE_ZONE_N("UpdateFluidCA");

	if (world.stats.fluidVoxelCount == 0u) {
		return 0u;
	}

	const int width = world.width;
	const int height = world.height;
	const int depth = world.depth;

#if !defined(NDEBUG)
	{
		PV_ASSERT(
			width > 0 && height > 0 && depth > 0,
			"VoxelWorld",
			"UpdateFluidCA",
			"world dimensions must be strictly positive");
	}
#endif

	const auto index = [width, height](const int x, const int y, const int z) -> size_t {
		return static_cast<size_t>(x) + static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(z) * static_cast<size_t>(width) * static_cast<size_t>(height);
	};

	const size_t totalCells = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth);
	std::vector<uint8_t> next(totalCells, 0u);

	int readMinX = world.fluidCAAabbMin.x - 1;
	int readMinY = world.fluidCAAabbMin.y - 1;
	int readMinZ = world.fluidCAAabbMin.z - 1;
	int readMaxX = world.fluidCAAabbMaxExclusive.x + 1;
	int readMaxY = world.fluidCAAabbMaxExclusive.y + 1;
	int readMaxZ = world.fluidCAAabbMaxExclusive.z + 1;
	if (readMinX < world.min.x) readMinX = world.min.x;
	if (readMinY < world.min.y) readMinY = world.min.y;
	if (readMinZ < world.min.z) readMinZ = world.min.z;
	if (readMaxX > world.maxExclusive.x) readMaxX = world.maxExclusive.x;
	if (readMaxY > world.maxExclusive.y) readMaxY = world.maxExclusive.y;
	if (readMaxZ > world.maxExclusive.z) readMaxZ = world.maxExclusive.z;

	const int simMinX = (readMinX < world.min.x) ? world.min.x : readMinX;
	const int simMinY = (readMinY < world.min.y) ? world.min.y : readMinY;
	const int simMinZ = (readMinZ < world.min.z) ? world.min.z : readMinZ;
	const int simMaxX = (readMaxX > world.maxExclusive.x) ? world.maxExclusive.x : readMaxX;
	const int simMaxY = (readMaxY > world.maxExclusive.y) ? world.maxExclusive.y : readMaxY;
	const int simMaxZ = (readMaxZ > world.maxExclusive.z) ? world.maxExclusive.z : readMaxZ;

	profiling::PlotValue("Fluid CA Cells Read", static_cast<int64_t>((readMaxX - readMinX) * (readMaxY - readMinY) * (readMaxZ - readMinZ)));

	{
		PV_PROFILE_ZONE_N("UpdateFluidCA.ReadPass");
		for (int z = readMinZ; z < readMaxZ; ++z) {
			for (int y = readMinY; y < readMaxY; ++y) {
				for (int x = readMinX; x < readMaxX; ++x) {
					const int lx = x - world.min.x;
					const int ly = y - world.min.y;
					const int lz = z - world.min.z;
					next[index(lx, ly, lz)] = ReadVoxelFromSparseStorage(world, {x, y, z});
				}
			}
		}
	}

	std::vector<uint8_t> claimed(totalCells, 0u);

	uint32_t movedCount = 0u;

	for (int z = simMinZ; z < simMaxZ; ++z) {
		for (int y = simMinY; y < simMaxY; ++y) {
			for (int x = simMinX; x < simMaxX; ++x) {
				const int lx = x - world.min.x;
				const int ly = y - world.min.y;
				const int lz = z - world.min.z;
				const size_t idx = index(lx, ly, lz);
				if (next[idx] != static_cast<uint8_t>(VoxelMaterial::Fluid)) {
					continue;
				}

				if (claimed[idx] != 0u) {
					continue;
				}

				if (ly > 0) {
					const size_t belowIdx = index(lx, ly - 1, lz);

					if (next[belowIdx] == static_cast<uint8_t>(VoxelMaterial::Air)) {
						next[idx] = static_cast<uint8_t>(VoxelMaterial::Air);
						next[belowIdx] = static_cast<uint8_t>(VoxelMaterial::Fluid);

						claimed[idx] = 1u;
						claimed[belowIdx] = 1u;
						++movedCount;
						continue;
					}
				}

				{
					const uint32_t h = lx * 73856093u ^ ly * 19349663u ^ lz * 83492791u;
					constexpr int sides[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
					const int startSide = static_cast<int>(h & 0x3u);

					const int dirs[2] = {startSide, startSide + 1 & 0x3};
					int spreadDir = -1;
					for (int d = 0; d < 2; ++d) {
						const int sideIdx = dirs[d];
						const int nlx = lx + sides[sideIdx][0];
						const int nlz = lz + sides[sideIdx][1];
						if (nlx < 0 || nlx >= width || nlz < 0 || nlz >= depth) {
							continue;
						}
						const size_t neighbourIdx = index(nlx, ly, nlz);
						if (next[neighbourIdx] == static_cast<uint8_t>(VoxelMaterial::Air)) {
							spreadDir = d;
							break;
						}
					}
					if (spreadDir >= 0) {
						const int sideIdx = dirs[spreadDir];
						const int nlx = lx + sides[sideIdx][0];
						const int nlz = lz + sides[sideIdx][1];
						const size_t neighbourIdx = index(nlx, ly, nlz);
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

	profiling::PlotValue("Fluid CA Cells Moved", static_cast<int64_t>(movedCount));
	{
		PV_PROFILE_ZONE_N("UpdateFluidCA.Commit");
		for (int z = simMinZ; z < simMaxZ; ++z) {
			for (int y = simMinY; y < simMaxY; ++y) {
				for (int x = simMinX; x < simMaxX; ++x) {
					const int lx = x - world.min.x;
					const int ly = y - world.min.y;
					const int lz = z - world.min.z;
					const size_t idx = index(lx, ly, lz);
					const uint8_t current = next[idx];
					const VoxelMaterial currentMaterial = static_cast<VoxelMaterial>(current);
					const VoxelMaterial previousMaterial = GetVoxelMaterial(world, {x, y, z});
					if (previousMaterial == currentMaterial) {
						continue;
					}
					SetVoxelMaterial(
						world,
						{x, y, z},
						currentMaterial);
				}
			}
		}
	}

#if !defined(NDEBUG)
	{
		const int fluidMinX = world.fluidCAAabbMin.x;
		const int fluidMinY = world.fluidCAAabbMin.y;
		const int fluidMinZ = world.fluidCAAabbMin.z;
		const int fluidMaxX = world.fluidCAAabbMaxExclusive.x;
		const int fluidMaxY = world.fluidCAAabbMaxExclusive.y;
		const int fluidMaxZ = world.fluidCAAabbMaxExclusive.z;
		uint32_t actualFluidCount = 0u;
		for (int z = fluidMinZ; z < fluidMaxZ; ++z) {
			for (int y = fluidMinY; y < fluidMaxY; ++y) {
				for (int x = fluidMinX; x < fluidMaxX; ++x) {
					if (GetVoxelMaterial(world, {x, y, z}) == VoxelMaterial::Fluid) {
						++actualFluidCount;
					}
				}
			}
		}
		PV_ASSERT(
			actualFluidCount == world.stats.fluidVoxelCount,
			"VoxelWorld",
			"UpdateFluidCA",
			"stats.fluidVoxelCount diverged from actual fluid voxel count");
	}
#endif

	return movedCount;
}

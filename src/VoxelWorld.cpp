#include "VoxelWorld.hpp"

#include "Types.hpp"

#include <cmath>
#include <memory>

namespace {
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

std::unique_ptr<VoxelWorld> BuildVoxelLabWorld(const VoxelLabConfig &config)
{
	auto world = std::make_unique<VoxelWorld>();
	world->config = config;
	world->chunkSize = config.chunkSize;

	const int halfFloor = config.floorSize / 2;
	world->min = {
		-halfFloor - config.padding,
		config.floorY,
		-halfFloor - config.padding,
	};
	world->maxExclusive = {
		halfFloor + config.padding,
		config.sphereCenter.y + config.sphereRadius + config.padding,
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
				chunk.dirty = true;
			}
		}
	}
	world->stats.dirtyChunkCount = static_cast<uint32_t>(world->chunks.size());

	for (int z = -halfFloor; z < halfFloor; ++z) {
		for (int x = -halfFloor; x < halfFloor; ++x) {
			const VoxelMaterial material = (x + z & 1) == 0 ? VoxelMaterial::FloorWhite : VoxelMaterial::FloorGray;
			SetVoxelMaterial(*world, {x, config.floorY, z}, material);
		}
	}

	const int outerRadiusSquared = config.sphereRadius * config.sphereRadius;
	const int innerRadius = config.sphereRadius - config.shellThickness;
	const int innerRadiusSquared = innerRadius * innerRadius;
	const int fluidTop = config.sphereCenter.y - innerRadius +
						 static_cast<int>(std::round(2.0f * static_cast<float>(innerRadius) * config.fluidFillLevel));

	for (int dz = -config.sphereRadius; dz <= config.sphereRadius; ++dz) {
		for (int dy = -config.sphereRadius; dy <= config.sphereRadius; ++dy) {
			for (int dx = -config.sphereRadius; dx <= config.sphereRadius; ++dx) {
				const int distanceSquared = dx * dx + dy * dy + dz * dz;
				if (distanceSquared > outerRadiusSquared) {
					continue;
				}

				const Int3 position{
					config.sphereCenter.x + dx,
					config.sphereCenter.y + dy,
					config.sphereCenter.z + dz,
				};

				if (distanceSquared > innerRadiusSquared) {
					SetVoxelMaterial(*world, position, VoxelMaterial::Glass);
					continue;
				}

				if (position.y <= fluidTop) {
					SetVoxelMaterial(*world, position, VoxelMaterial::Fluid);
				}
			}
		}
	}

	MarkAllVoxelChunksDirty(world.get());
	return world;
}
} // namespace

bool CreateVoxelLabWorld(AppState *state)
{
	if (!state) {
		return false;
	}

	state->world.voxelWorld = BuildVoxelLabWorld({});
	return static_cast<bool>(state->world.voxelWorld);
}

void DestroyVoxelLabWorld(AppState *state)
{
	if (!state) {
		return;
	}

	state->world.voxelWorld.reset();
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
	VoxelChunk &chunk = world.chunks[GetVoxelChunkIndex(world, chunkCoord)];
	if (!chunk.dirty) {
		chunk.dirty = true;
		++world.stats.dirtyChunkCount;
	}
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

	const Int3 firstChunk = GetChunkCoord(world, clampedMin);
	const Int3 lastChunk = GetChunkCoord(world, {clampedMax.x - 1, clampedMax.y - 1, clampedMax.z - 1});

	for (int chunkZ = firstChunk.z; chunkZ <= lastChunk.z; ++chunkZ) {
		for (int chunkY = firstChunk.y; chunkY <= lastChunk.y; ++chunkY) {
			for (int chunkX = firstChunk.x; chunkX <= lastChunk.x; ++chunkX) {
				VoxelChunk &chunk = world.chunks[GetVoxelChunkIndex(world, {chunkX, chunkY, chunkZ})];
				if (!chunk.dirty) {
					chunk.dirty = true;
					++world.stats.dirtyChunkCount;
				}
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

	MarkVoxelChunkDirty(world, position);
}

void MarkAllVoxelChunksDirty(VoxelWorld *world)
{
	if (!world) {
		return;
	}

	for (VoxelChunk &chunk : world->chunks) {
		chunk.dirty = true;
	}
	world->stats.dirtyChunkCount = static_cast<uint32_t>(world->chunks.size());
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

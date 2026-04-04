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

void MarkVoxelChunkDirty(VoxelWorld *world, const Int3 position)
{
	if (!world || !IsInsideVoxelWorld(*world, position)) {
		return;
	}

	const Int3 chunkCoord = GetChunkCoord(*world, position);
	world->chunks[GetVoxelChunkIndex(*world, chunkCoord)].dirty = true;
}

void SetVoxelMaterial(VoxelWorld &world, const Int3 position, const VoxelMaterial material)
{
	if (!IsInsideVoxelWorld(world, position)) {
		return;
	}

	world.voxels[ToVoxelIndex(world, position)] = static_cast<uint8_t>(material);
	MarkVoxelChunkDirty(&world, position);
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

	state->voxelWorld = BuildVoxelLabWorld({});
	return static_cast<bool>(state->voxelWorld);
}

void DestroyVoxelLabWorld(AppState *state)
{
	if (!state) {
		return;
	}

	state->voxelWorld.reset();
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

void MarkAllVoxelChunksDirty(VoxelWorld *world)
{
	if (!world) {
		return;
	}

	for (VoxelChunk &chunk : world->chunks) {
		chunk.dirty = true;
	}
}

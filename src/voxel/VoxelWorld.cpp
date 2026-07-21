#include "core/Math.hpp"
#include "core/StringId.hpp"
#include "voxel/VoxelWorldInternal.hpp"

#include "core/Types.hpp"
#include "physics/PhysicsWorld.hpp"

#include <algorithm>
#include <vector>

namespace {
size_t ToVoxelIndex(const VoxelWorld &world, const Int3 position)
{
	const size_t localX = static_cast<size_t>(position.x - world.min.x);
	const size_t localY = static_cast<size_t>(position.y - world.min.y);
	const size_t localZ = static_cast<size_t>(position.z - world.min.z);
	return localX + static_cast<size_t>(world.width) * (localY + static_cast<size_t>(world.height) * localZ);
}

bool IsAirMaterial(const VoxelMaterial material)
{
	return material == VoxelMaterial::Air;
}

void WriteVoxelToSparseStorage(VoxelWorld &world, const Int3 position, const uint8_t material)
{
	const int localX = position.x - world.min.x;
	const int localY = position.y - world.min.y;
	const int localZ = position.z - world.min.z;
	world.sparseStorage.SetCell(localX, localY, localZ, material);
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
} // namespace

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

void SetVoxelMaterial(VoxelWorld &world, const Int3 position, VoxelMaterial material, PhysicsState *physics)
{
	if (!IsInsideVoxelWorld(world, position)) {
		return;
	}

	const VoxelMaterial previousMaterial = GetVoxelMaterial(world, position);
	if (previousMaterial == material) {
		return;
	}

	WriteVoxelToSparseStorage(world, position, static_cast<uint8_t>(material));
	++world.editVersion;
	AccumulateMaterialCount(world.stats, previousMaterial, -1);
	AccumulateMaterialCount(world.stats, material, 1);

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

	if (physics != nullptr) {
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
	world->pendingBlasRebuildIndices.clear();						// Clear pending BLAS rebuild queue
	world->pendingBlasRebuildIndices.reserve(world->chunks.size()); // Reserve space for BLAS rebuilds
	for (size_t chunkIndex = 0; chunkIndex < world->chunks.size(); ++chunkIndex) {
		world->pendingChunkRebuildIndices.push_back(chunkIndex);
		world->pendingBlasRebuildIndices.push_back(chunkIndex); // Mark chunk for BLAS rebuild
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

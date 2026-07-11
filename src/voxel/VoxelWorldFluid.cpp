import projectv.math;
import projectv.string_id;

#include "voxel/VoxelWorldInternal.hpp"
#include "debug/Profiling.hpp"
#include "core/RuntimeDiagnostics.hpp"

#include <cstdlib>
#include <vector>
#include <algorithm>

namespace {
bool gFluidCaGpuEnabledForTesting = false;
} // namespace

bool IsFluidCaGpuEnabled()
{
	if (gFluidCaGpuEnabledForTesting) {
		return true;
	}
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
		const int worldMinX = world.min.x;
		const int worldMinY = world.min.y;
		const int worldMinZ = world.min.z;
		const int worldMaxX = world.maxExclusive.x;
		const int worldMaxY = world.maxExclusive.y;
		const int worldMaxZ = world.maxExclusive.z;
		uint32_t actualFluidCount = 0u;
		for (int z = worldMinZ; z < worldMaxZ; ++z) {
			for (int y = worldMinY; y < worldMaxY; ++y) {
				for (int x = worldMinX; x < worldMaxX; ++x) {
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

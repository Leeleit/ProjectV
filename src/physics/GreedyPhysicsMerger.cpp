#include "physics/GreedyPhysicsMerger.hpp"

#include <cstdlib>
#include <cstring>
#include <vector>

#include "voxel/VoxelWorld.hpp"

namespace projectv::physics {

namespace {

bool IsGreedyPhysicsMeshEnabledFromEnvironment()
{
	const char *value = std::getenv("PROJECTV_GREEDY_PHYSICS_MESH");
	if (value == nullptr) {
		return true;
	}
	return value[0] == 'O' && value[1] == 'N';
}

}  // namespace

bool IsGreedyPhysicsMeshEnabled()
{
	return IsGreedyPhysicsMeshEnabledFromEnvironment();
}

namespace {

inline bool IsSolidAt(const VoxelWorld &world, const int x, const int y, const int z)
{
	if (x < world.min.x || y < world.min.y || z < world.min.z ||
		x >= world.maxExclusive.x || y >= world.maxExclusive.y || z >= world.maxExclusive.z) {
		return false;
	}
	const Int3 voxel{x, y, z};
	switch (GetVoxelMaterial(world, voxel)) {
	case VoxelMaterial::Glass:
	case VoxelMaterial::FloorWhite:
	case VoxelMaterial::FloorGray:
		return true;
	case VoxelMaterial::Air:
	case VoxelMaterial::Fluid:
		return false;
	}
	return false;
}

}  // namespace

uint32_t GreedyMergeSolidVoxelsInBounds(
	const VoxelWorld &world,
	Int3 boundsMin,
	Int3 boundsMaxExclusive,
	std::vector<MergedVoxelBox> &outBoxes)
{
	outBoxes.clear();
	if (boundsMaxExclusive.x <= boundsMin.x ||
		boundsMaxExclusive.y <= boundsMin.y ||
		boundsMaxExclusive.z <= boundsMin.z) {
		return 0u;
	}

	const int startX = std::max(boundsMin.x, world.min.x);
	const int startY = std::max(boundsMin.y, world.min.y);
	const int startZ = std::max(boundsMin.z, world.min.z);
	const int endX = std::min(boundsMaxExclusive.x, world.maxExclusive.x);
	const int endY = std::min(boundsMaxExclusive.y, world.maxExclusive.y);
	const int endZ = std::min(boundsMaxExclusive.z, world.maxExclusive.z);

	if (endX <= startX || endY <= startY || endZ <= startZ) {
		return 0u;
	}

	const size_t strideX = static_cast<size_t>(1);
	const size_t strideY = static_cast<size_t>(endX - startX);
	const size_t strideZ = static_cast<size_t>(endY - startY) * strideY;
	std::vector<uint8_t> consumed(
		static_cast<size_t>(endX - startX) *
		static_cast<size_t>(endY - startY) *
		static_cast<size_t>(endZ - startZ),
		0u);

	const auto at = [&](const int x, const int y, const int z) -> uint8_t & {
		return consumed[
			(static_cast<size_t>(z - startZ) * strideZ) +
			(static_cast<size_t>(y - startY) * strideY) +
			(static_cast<size_t>(x - startX) * strideX)];
	};

	for (int z = startZ; z < endZ; ++z) {
		for (int y = startY; y < endY; ++y) {
			for (int x = startX; x < endX; ++x) {
				if (at(x, y, z) != 0u) {
					continue;
				}
				if (!IsSolidAt(world, x, y, z)) {
					continue;
				}

				int x1 = x + 1;
				while (x1 < endX && at(x1, y, z) == 0u && IsSolidAt(world, x1, y, z)) {
					++x1;
				}

				int y1 = y + 1;
				bool yOk = true;
				while (y1 < endY && yOk) {
					for (int xi = x; xi < x1; ++xi) {
						if (at(xi, y1, z) != 0u || !IsSolidAt(world, xi, y1, z)) {
							yOk = false;
							break;
						}
					}
					if (yOk) {
						++y1;
					}
				}

				int z1 = z + 1;
				bool zOk = true;
				while (z1 < endZ && zOk) {
					for (int yi = y; yi < y1; ++yi) {
						for (int xi = x; xi < x1; ++xi) {
							if (at(xi, yi, z1) != 0u || !IsSolidAt(world, xi, yi, z1)) {
								zOk = false;
								break;
							}
						}
						if (!zOk) {
							break;
						}
					}
					if (zOk) {
						++z1;
					}
				}

				for (int zi = z; zi < z1; ++zi) {
					for (int yi = y; yi < y1; ++yi) {
						for (int xi = x; xi < x1; ++xi) {
							at(xi, yi, zi) = 1u;
						}
					}
				}

				MergedVoxelBox box{};
				box.minX = x;
				box.minY = y;
				box.minZ = z;
				box.maxX = x1;
				box.maxY = y1;
				box.maxZ = z1;
				outBoxes.push_back(box);
			}
		}
	}

	return static_cast<uint32_t>(outBoxes.size());
}

}  // namespace projectv::physics

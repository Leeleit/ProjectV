#pragma once

#include <cstdint>
#include <vector>

#include "voxel/VoxelWorld.hpp"

namespace projectv::physics {

struct MergedVoxelBox {
	int minX = 0;
	int minY = 0;
	int minZ = 0;
	int maxX = 0;
	int maxY = 0;
	int maxZ = 0;
};

uint32_t GreedyMergeSolidVoxelsInBounds(
	const VoxelWorld &world,
	Int3 boundsMin,
	Int3 boundsMaxExclusive,
	std::vector<MergedVoxelBox> &outBoxes);

bool IsGreedyPhysicsMeshEnabled();

}  // namespace projectv::physics

#pragma once

#include "core/Types.hpp"
#include "voxel/VoxelMaterials.hpp"

#include <cstdint>
#include <vector>

namespace projectv::voxel {

struct CpuMeshInput {
	const uint8_t *voxels = nullptr;
	int widthX = 0;
	int heightY = 0;
	int depthZ = 0;
};

std::vector<PackedSceneVoxelFace> GenerateCpuChunkMeshXPositive(const CpuMeshInput &input);

}

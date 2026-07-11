#pragma once

#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
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

} // namespace projectv::voxel

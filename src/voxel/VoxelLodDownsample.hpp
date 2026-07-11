#pragma once

#include <cstdint> // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
#include <vector>

#include "voxel/VoxelWorld.hpp"

namespace projectv::voxel {

uint32_t LodDownsampleStepForLod(uint8_t lodLevel);
uint32_t LodDownsampledExtentForLod(uint8_t lodLevel, uint8_t chunkSize);

uint8_t SurfacePreserveVote8(
	const VoxelWorld &world,
	const Int3 &chunkOrigin,
	uint32_t outX,
	uint32_t outY,
	uint32_t outZ,
	uint32_t step);

void DownsampleChunkForLodSurfacePreserve(
	const VoxelWorld &world,
	size_t chunkIndex,
	uint8_t lodLevel,
	std::vector<uint8_t> &outDownsampled);

uint32_t RunLodDownsampleJobs(VoxelWorld &world);
bool IsLodDownsampleEnabled();

} // namespace projectv::voxel

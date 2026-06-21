#pragma once

#include <cstdint>

#include "core/Types.hpp"
#include "voxel/VoxelWorld.hpp"

namespace projectv::render {

bool IsLodDownsampledGpuConsumeEnabled();

uint32_t ComputeLodDownsampledVoxelPayloadBytes(
	uint32_t chunkCount,
	uint32_t chunkSize);

uint32_t ComputeChunkLodLevelsCapacity(uint32_t chunkCount);

bool RefreshLodDownsampledBuffers(
	VulkanContextState *context,
	RenderState *render,
	const VoxelWorld &world);

}  // namespace projectv::render

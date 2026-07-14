#pragma once

#include "core/Types.hpp"
#include "voxel/VoxelWorld.hpp"

namespace projectv::render {

bool IsLodDownsampledGpuConsumeEnabled();

uint32_t ComputeLodDownsampledVoxelPayloadBytes(
	uint32_t chunkCount,
	uint32_t chunkSize);

uint32_t ComputeChunkLodLevelsCapacity(uint32_t chunkCount);

inline constexpr uint32_t kLodPayloadWordStride = 16u; // 16 uint32 words for chunkSize=8 LOD 1 (64 bytes); must match shader stride.

uint32_t LodPayloadWordOffsetForChunk(uint32_t chunkIndex);

uint32_t EncodeChunkLodMetadata(uint8_t lodLevel, uint8_t outExtent);

void DecodeChunkLodMetadata(
	uint32_t metadata,
	uint8_t &outLodLevel,
	uint8_t &outExtent);

void BuildLodPayloadWordsFromDownsampled(
	const uint8_t *downsampledBytes,
	uint32_t byteCount,
	uint32_t *outWords);

bool RefreshLodDownsampledBuffers(
	VulkanContextState *context,
	RenderState *render,
	const VoxelWorld &world);

} // namespace projectv::render

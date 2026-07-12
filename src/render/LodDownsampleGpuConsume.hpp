#pragma once

#include "core/Types.hpp"
#include "voxel/VoxelWorld.hpp"

namespace projectv::render {

bool IsLodDownsampledGpuConsumeEnabled();

uint32_t ComputeLodDownsampledVoxelPayloadBytes(
	uint32_t chunkCount,
	uint32_t chunkSize);

uint32_t ComputeChunkLodLevelsCapacity(uint32_t chunkCount);

// EVIL: kLodPayloadWordStride = 16 for chunkSize=8, LOD 1 (outExtent=4, 64 bytes = 16 uint32 words).
// Stride is constant per chunk slot so the shader can compute base word offset as
// `chunkIndex * kLodPayloadWordStride` without per-chunk metadata. For chunkSize > 8 or future
// non-power-of-two chunkSize this stride must be recomputed from the worst-case outExtent.
// SYNC CHECKLIST (run before commit): `rg "kLodPayloadWordStride|kLodWordStride" src/ src/shaders/`
// MUST show both files at the same value.
inline constexpr uint32_t kLodPayloadWordStride = 16u;

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

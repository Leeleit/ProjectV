#pragma once

#include <array> // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
#include <cstddef>

#include <vulkan/vulkan.h>

struct VoxelMeshingPushConstants {
	std::array<int32_t, 4> worldMinAndChunkSize{};
	std::array<int32_t, 4> worldMaxExclusiveAndChunkCount{};
	std::array<uint32_t, 4> chunkGridAndTransparentFaceBase{};
	std::array<uint32_t, 4> faceCapacities{};
};

struct ChunkVisibilityCache {
	bool valid = false;
	uint64_t sceneVoxelPayloadVersion = 0;
	uint32_t chunkDescriptorCount = 0;

	int32_t quantizedCameraX = 0;
	int32_t quantizedCameraY = 0;
	int32_t quantizedCameraZ = 0;
	int32_t quantizedForwardX1000 = 0;
	int32_t quantizedForwardY1000 = 0;
	int32_t quantizedForwardZ1000 = 0;
	uint64_t hash = 0;
	uint32_t visibleChunkCount = 0;
	static constexpr std::size_t kChunkVisibilityCacheMaxChunks = 1024;
	std::array<VkDrawIndirectCommand, kChunkVisibilityCacheMaxChunks> opaqueCommands{};
	std::size_t opaqueCommandsSize = 0;
	std::array<VkDrawIndirectCommand, kChunkVisibilityCacheMaxChunks> transparentCommands{};
	std::size_t transparentCommandsSize = 0;

	uint32_t culledChunkCount = 0;

	uint64_t consecutiveHitCount = 0;
};

static_assert(std::is_standard_layout_v<VoxelMeshingPushConstants>);
static_assert(std::is_trivially_copyable_v<VoxelMeshingPushConstants>);
static_assert(sizeof(VoxelMeshingPushConstants) == 64);
static_assert(offsetof(VoxelMeshingPushConstants, worldMinAndChunkSize) == 0);
static_assert(offsetof(VoxelMeshingPushConstants, worldMaxExclusiveAndChunkCount) == 16);
static_assert(offsetof(VoxelMeshingPushConstants, chunkGridAndTransparentFaceBase) == 32);
static_assert(offsetof(VoxelMeshingPushConstants, faceCapacities) == 48);
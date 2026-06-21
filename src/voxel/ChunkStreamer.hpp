#pragma once

#include "voxel/Sparse64Tree.hpp"

#include <cstdint>
#include <expected>
#include <mutex>
#include <string>
#include <vector>

namespace projectv::voxel {

enum class ChunkStreamError : uint8_t {
	QueueFull = 0,
	InvalidChunk = 1,
	NotInitialized = 2,
};

struct ChunkStreamRequest {
	uint32_t chunkIndex = 0u;
	uint8_t priority = 0u;
};

struct ChunkData {
	std::vector<uint8_t> voxelBytes;
	std::vector<uint32_t> nodeWords;
};

bool IsChunkStreamingEnabled();

bool EnqueueChunkStreamRequest(const ChunkStreamRequest &request);

uint32_t DrainChunkStreamQueueSize();

std::expected<ChunkData, ChunkStreamError> TryDequeueChunkData();

}  // namespace projectv::voxel

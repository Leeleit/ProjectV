#pragma once

#include "voxel/VoxelWorld.hpp"

#include <cstdint>
#include <expected>
#include <string>
#include <thread>
#include <vector>

namespace projectv::voxel {

enum class ChunkStreamError : uint8_t {
	QueueFull = 0,
	InvalidChunk = 1,
	NotInitialized = 2,
	FileNotFound = 3,
	FileReadFailed = 4,
};

struct ChunkStreamRequest {
	uint32_t chunkIndex = 0u;
	uint8_t priority = 0u;
};

struct ChunkData {
	std::vector<uint8_t> voxelBytes;
	std::vector<uint32_t> nodeWords;
};

struct ChunkPrebakeStats {
	uint32_t chunksBaked = 0u;
	uint32_t chunksSkipped = 0u;
	uint64_t totalVoxelBytes = 0u;
};

bool IsChunkStreamingEnabled();

bool EnqueueChunkStreamRequest(const ChunkStreamRequest &request);

uint32_t DrainChunkStreamQueueSize();

std::expected<ChunkData, ChunkStreamError> TryDequeueChunkData();

void StartChunkStreamerWorker();

void StopChunkStreamerWorker();

bool IsChunkStreamerWorkerActive();

void ProcessPendingRequests(const std::stop_token &stopToken);

std::string GetChunkStreamerCachePath();

bool BakeAllChunksToDisk(
	const VoxelWorld &world,
	ChunkPrebakeStats &outStats);

bool IsChunkStreamerPrebakeReady();

uint64_t GetChunkStreamerPrebakeVersion();

uint32_t PreloadChunksAroundCamera(
	const VoxelWorld &world,
	float cameraX,
	float cameraY,
	float cameraZ,
	uint32_t radiusChunks);

} // namespace projectv::voxel

#include "voxel/ChunkStreamer.hpp"

#include <cstdlib>
#include <deque>
#include <mutex>

namespace projectv::voxel {

namespace {

bool IsChunkStreamingEnabledFromEnvironment()
{
	const char *value = std::getenv("PROJECTV_CHUNK_STREAMING");
	if (value == nullptr) {
		return true;
	}
	return value[0] == 'O' && value[1] == 'N';
}

std::mutex &GetQueueMutex()
{
	static std::mutex mutex;
	return mutex;
}

std::deque<ChunkStreamRequest> &GetPendingQueue()
{
	static std::deque<ChunkStreamRequest> queue;
	return queue;
}

std::deque<ChunkData> &GetReadyQueue()
{
	static std::deque<ChunkData> queue;
	return queue;
}

}  // namespace

bool IsChunkStreamingEnabled()
{
	return IsChunkStreamingEnabledFromEnvironment();
}

bool EnqueueChunkStreamRequest(const ChunkStreamRequest &request)
{
	if (!IsChunkStreamingEnabled()) {
		return false;
	}
	const std::lock_guard<std::mutex> lock(GetQueueMutex());
	GetPendingQueue().push_back(request);
	return true;
}

uint32_t DrainChunkStreamQueueSize()
{
	const std::lock_guard<std::mutex> lock(GetQueueMutex());
	return static_cast<uint32_t>(GetPendingQueue().size());
}

std::expected<ChunkData, ChunkStreamError> TryDequeueChunkData()
{
	if (!IsChunkStreamingEnabled()) {
		return std::unexpected(ChunkStreamError::NotInitialized);
	}
	const std::lock_guard<std::mutex> lock(GetQueueMutex());
	if (GetReadyQueue().empty()) {
		return std::unexpected(ChunkStreamError::QueueFull);
	}
	ChunkData data = std::move(GetReadyQueue().front());
	GetReadyQueue().pop_front();
	return data;
}

}  // namespace projectv::voxel

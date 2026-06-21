#include "voxel/ChunkStreamer.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

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

std::jthread &GetWorkerThread()
{
	static std::jthread thread;
	return thread;
}

std::atomic<bool> &GetWorkerActiveFlag()
{
	static std::atomic<bool> active{false};
	return active;
}

std::atomic<uint64_t> &GetPrebakeVersion()
{
	static std::atomic<uint64_t> version{0u};
	return version;
}

constexpr uint32_t kChunkFileHeaderMagic = 0x504B5631u;
constexpr uint32_t kChunkFileFormatVersion = 1u;
constexpr size_t kChunkFileHeaderBytes = 16u;
constexpr auto kWorkerPollInterval = std::chrono::milliseconds(1);
constexpr auto kWorkerIdleSleep = std::chrono::milliseconds(5);

std::string GetChunkStreamerCachePathFromEnvironment()
{
	const char *value = std::getenv("PROJECTV_CHUNK_PATH");
	if (value != nullptr && value[0] != '\0') {
		return std::string(value);
	}
	const char *buildDir = std::getenv("PROJECTV_CMAKE_BUILD_DIR");
	if (buildDir != nullptr && buildDir[0] != '\0') {
		return std::string(buildDir) + "/cache/chunks";
	}
	return "build/cache/chunks";
}

std::expected<ChunkData, ChunkStreamError> ReadChunkBinaryFile(
	const std::string &cachePath,
	uint32_t chunkIndex)
{
	std::ostringstream pathBuilder;
	pathBuilder << cachePath << "/chunk_" << chunkIndex << ".bin";
	const std::filesystem::path chunkPath = pathBuilder.str();
	if (!std::filesystem::exists(chunkPath)) {
		return std::unexpected(ChunkStreamError::FileNotFound);
	}
	std::ifstream file(chunkPath, std::ios::binary);
	if (!file.is_open()) {
		return std::unexpected(ChunkStreamError::FileReadFailed);
	}

	std::vector<uint8_t> header(kChunkFileHeaderBytes, 0u);
	file.read(reinterpret_cast<char *>(header.data()), static_cast<std::streamsize>(header.size()));
	if (file.gcount() != static_cast<std::streamsize>(header.size())) {
		return std::unexpected(ChunkStreamError::FileReadFailed);
	}
	const uint32_t magic = static_cast<uint32_t>(header[0]) |
		(static_cast<uint32_t>(header[1]) << 8) |
		(static_cast<uint32_t>(header[2]) << 16) |
		(static_cast<uint32_t>(header[3]) << 24);
	if (magic != kChunkFileHeaderMagic) {
		return std::unexpected(ChunkStreamError::FileReadFailed);
	}

	const uint64_t voxelByteCount = static_cast<uint64_t>(header[8]) |
		(static_cast<uint64_t>(header[9]) << 8) |
		(static_cast<uint64_t>(header[10]) << 16) |
		(static_cast<uint64_t>(header[11]) << 24) |
		(static_cast<uint64_t>(header[12]) << 32) |
		(static_cast<uint64_t>(header[13]) << 40) |
		(static_cast<uint64_t>(header[14]) << 48) |
		(static_cast<uint64_t>(header[15]) << 56);
	if (voxelByteCount > 16u * 1024u * 1024u) {
		return std::unexpected(ChunkStreamError::FileReadFailed);
	}

	ChunkData data{};
	data.voxelBytes.resize(static_cast<size_t>(voxelByteCount));
	if (voxelByteCount > 0u) {
		file.read(reinterpret_cast<char *>(data.voxelBytes.data()),
			static_cast<std::streamsize>(data.voxelBytes.size()));
		if (static_cast<uint64_t>(file.gcount()) != voxelByteCount) {
			return std::unexpected(ChunkStreamError::FileReadFailed);
		}
	}
	return data;
}

bool WriteChunkBinaryFile(
	const std::string &cachePath,
	uint32_t chunkIndex,
	const std::vector<uint8_t> &voxelBytes)
{
	std::error_code ec;
	std::filesystem::create_directories(cachePath, ec);
	if (ec) {
		return false;
	}
	std::ostringstream pathBuilder;
	pathBuilder << cachePath << "/chunk_" << chunkIndex << ".bin";
	const std::filesystem::path chunkPath = pathBuilder.str();

	std::ofstream file(chunkPath, std::ios::binary | std::ios::trunc);
	if (!file.is_open()) {
		return false;
	}

	std::array<uint8_t, kChunkFileHeaderBytes> header{};
	header[0] = static_cast<uint8_t>(kChunkFileHeaderMagic & 0xFFu);
	header[1] = static_cast<uint8_t>((kChunkFileHeaderMagic >> 8) & 0xFFu);
	header[2] = static_cast<uint8_t>((kChunkFileHeaderMagic >> 16) & 0xFFu);
	header[3] = static_cast<uint8_t>((kChunkFileHeaderMagic >> 24) & 0xFFu);
	header[4] = static_cast<uint8_t>(kChunkFileFormatVersion & 0xFFu);
	header[5] = static_cast<uint8_t>((kChunkFileFormatVersion >> 8) & 0xFFu);
	header[6] = 0u;
	header[7] = 0u;
	const uint64_t voxelByteCount = static_cast<uint64_t>(voxelBytes.size());
	header[8] = static_cast<uint8_t>(voxelByteCount & 0xFFu);
	header[9] = static_cast<uint8_t>((voxelByteCount >> 8) & 0xFFu);
	header[10] = static_cast<uint8_t>((voxelByteCount >> 16) & 0xFFu);
	header[11] = static_cast<uint8_t>((voxelByteCount >> 24) & 0xFFu);
	header[12] = static_cast<uint8_t>((voxelByteCount >> 32) & 0xFFu);
	header[13] = static_cast<uint8_t>((voxelByteCount >> 40) & 0xFFu);
	header[14] = static_cast<uint8_t>((voxelByteCount >> 48) & 0xFFu);
	header[15] = static_cast<uint8_t>((voxelByteCount >> 56) & 0xFFu);

	file.write(reinterpret_cast<const char *>(header.data()), static_cast<std::streamsize>(header.size()));
	if (!voxelBytes.empty()) {
		file.write(reinterpret_cast<const char *>(voxelBytes.data()),
			static_cast<std::streamsize>(voxelBytes.size()));
	}
	return file.good();
}

}  // namespace

std::string GetChunkStreamerCachePath()
{
	return GetChunkStreamerCachePathFromEnvironment();
}

bool IsChunkStreamingEnabled()
{
	return IsChunkStreamingEnabledFromEnvironment();
}

bool EnqueueChunkStreamRequest(const ChunkStreamRequest &request)
{
	if (!IsChunkStreamingEnabled()) {
		return false;
	}
	StartChunkStreamerWorker();
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

void ProcessPendingRequests(std::stop_token stopToken)
{
	const std::string cachePath = GetChunkStreamerCachePathFromEnvironment();
	while (!stopToken.stop_requested()) {
		std::deque<ChunkStreamRequest> localPending;
		{
			const std::lock_guard<std::mutex> lock(GetQueueMutex());
			if (!GetPendingQueue().empty()) {
				for (auto it = GetPendingQueue().begin(); it != GetPendingQueue().end(); ) {
					localPending.push_back(std::move(*it));
					it = GetPendingQueue().erase(it);
				}
			}
		}
		if (localPending.empty()) {
			std::this_thread::sleep_for(kWorkerIdleSleep);
			continue;
		}
		for (const ChunkStreamRequest &request : localPending) {
			if (stopToken.stop_requested()) {
				return;
			}
			std::expected<ChunkData, ChunkStreamError> result =
				ReadChunkBinaryFile(cachePath, request.chunkIndex);
			if (result.has_value()) {
				const std::lock_guard<std::mutex> lock(GetQueueMutex());
				GetReadyQueue().push_back(std::move(result.value()));
			}
		}
		std::this_thread::sleep_for(kWorkerPollInterval);
	}
}

void StartChunkStreamerWorker()
{
	if (!IsChunkStreamingEnabled()) {
		return;
	}
	bool expected = false;
	if (!GetWorkerActiveFlag().compare_exchange_strong(expected, true)) {
		return;
	}
	std::jthread &thread = GetWorkerThread();
	thread = std::jthread(ProcessPendingRequests);
}

void StopChunkStreamerWorker()
{
	std::jthread &thread = GetWorkerThread();
	if (thread.joinable()) {
		thread.request_stop();
		thread.join();
	}
	GetWorkerActiveFlag().store(false);
}

bool IsChunkStreamerWorkerActive()
{
	return GetWorkerActiveFlag().load();
}

bool BakeAllChunksToDisk(
	const VoxelWorld &world,
	ChunkPrebakeStats &outStats)
{
	if (!IsChunkStreamingEnabled()) {
		return false;
	}
	const std::string cachePath = GetChunkStreamerCachePathFromEnvironment();
	const uint32_t chunkSize = static_cast<uint32_t>(world.chunkSize);
	const size_t voxelCountPerChunk = static_cast<size_t>(chunkSize) * chunkSize * chunkSize;
	std::vector<uint8_t> voxelBytes(voxelCountPerChunk, 0u);
	outStats = ChunkPrebakeStats{};
	for (size_t i = 0; i < world.chunks.size(); ++i) {
		const VoxelChunk &chunk = world.chunks[i];
		for (uint32_t z = 0u; z < chunkSize; ++z) {
			for (uint32_t y = 0u; y < chunkSize; ++y) {
				for (uint32_t x = 0u; x < chunkSize; ++x) {
					const int wx = chunk.min.x + static_cast<int>(x) - world.min.x;
					const int wy = chunk.min.y + static_cast<int>(y) - world.min.y;
					const int wz = chunk.min.z + static_cast<int>(z) - world.min.z;
					const uint8_t material = world.sparseStorage.GetCell(
						static_cast<uint32_t>(wx),
						static_cast<uint32_t>(wy),
						static_cast<uint32_t>(wz));
					voxelBytes[(static_cast<size_t>(z) * chunkSize + y) * chunkSize + x] = material;
				}
			}
		}
		if (!WriteChunkBinaryFile(cachePath, static_cast<uint32_t>(i), voxelBytes)) {
			++outStats.chunksSkipped;
			continue;
		}
		++outStats.chunksBaked;
		outStats.totalVoxelBytes += voxelBytes.size();
	}
	GetPrebakeVersion().fetch_add(1u, std::memory_order_relaxed);
	return true;
}

bool IsChunkStreamerPrebakeReady()
{
	return GetPrebakeVersion().load(std::memory_order_relaxed) > 0u;
}

uint64_t GetChunkStreamerPrebakeVersion()
{
	return GetPrebakeVersion().load(std::memory_order_relaxed);
}

uint32_t PreloadChunksAroundCamera(
	const VoxelWorld &world,
	const float cameraX,
	const float cameraY,
	const float cameraZ,
	const uint32_t radiusChunks)
{
	if (!IsChunkStreamingEnabled()) {
		return 0u;
	}
	const uint32_t chunkSize = static_cast<uint32_t>(world.chunkSize);
	if (chunkSize == 0u || world.chunks.empty()) {
		return 0u;
	}
	const float invChunkSize = 1.0f / static_cast<float>(chunkSize);
	const int cameraChunkX = static_cast<int>(std::floor(static_cast<float>(cameraX - world.min.x) * invChunkSize));
	const int cameraChunkY = static_cast<int>(std::floor(static_cast<float>(cameraY - world.min.y) * invChunkSize));
	const int cameraChunkZ = static_cast<int>(std::floor(static_cast<float>(cameraZ - world.min.z) * invChunkSize));
	const int minX = cameraChunkX - static_cast<int>(radiusChunks);
	const int maxX = cameraChunkX + static_cast<int>(radiusChunks);
	const int minY = cameraChunkY - static_cast<int>(radiusChunks);
	const int maxY = cameraChunkY + static_cast<int>(radiusChunks);
	const int minZ = cameraChunkZ - static_cast<int>(radiusChunks);
	const int maxZ = cameraChunkZ + static_cast<int>(radiusChunks);
	const int gridWidth = (world.width > 0) ? ((world.width + chunkSize - 1) / chunkSize) : 0;
	const int gridHeight = (world.height > 0) ? ((world.height + chunkSize - 1) / chunkSize) : 0;
	const int gridDepth = (world.depth > 0) ? ((world.depth + chunkSize - 1) / chunkSize) : 0;
	uint32_t enqueued = 0u;
	for (int gz = minZ; gz <= maxZ; ++gz) {
		if (gridDepth > 0 && (gz < 0 || gz >= gridDepth)) {
			continue;
		}
		for (int gy = minY; gy <= maxY; ++gy) {
			if (gridHeight > 0 && (gy < 0 || gy >= gridHeight)) {
				continue;
			}
			for (int gx = minX; gx <= maxX; ++gx) {
				if (gridWidth > 0 && (gx < 0 || gx >= gridWidth)) {
					continue;
				}
				const size_t linearIndex = static_cast<size_t>(gz) *
					static_cast<size_t>(std::max(gridHeight, 1)) *
					static_cast<size_t>(std::max(gridWidth, 1)) +
					static_cast<size_t>(gy) * static_cast<size_t>(std::max(gridWidth, 1)) +
					static_cast<size_t>(gx);
				if (linearIndex >= world.chunks.size()) {
					continue;
				}
				ChunkStreamRequest request{};
				request.chunkIndex = static_cast<uint32_t>(linearIndex);
				request.priority = 0u;
				if (EnqueueChunkStreamRequest(request)) {
					++enqueued;
				}
			}
		}
	}
	return enqueued;
}

}  // namespace projectv::voxel

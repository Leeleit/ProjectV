#include "voxel/ChunkStreamer.hpp"
#include "core/EnvUtils.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

namespace {

struct StreamTestContext {
	int failures = 0;
	void Fail(const int line, const std::string_view message)
	{
		std::fprintf(stderr, "Test failure at line %d: %.*s\n", line, static_cast<int>(message.size()), message.data());
		++failures;
	}
};

void TestEnvGateDefault(StreamTestContext &test)
{
	(void)std::getenv("PROJECTV_CHUNK_STREAMING");
	if (!projectv::voxel::IsChunkStreamingEnabled()) {
		test.Fail(__LINE__, "Default env gate must enable chunk streaming");
	}
}

void TestEnvGateOff(StreamTestContext &test)
{
	const int setenvResult = projectv::core::SetEnvVar("PROJECTV_CHUNK_STREAMING", "OFF", 1);
	if (setenvResult != 0) {
		test.Fail(__LINE__, "setenv failed");
		return;
	}
	if (projectv::voxel::IsChunkStreamingEnabled()) {
		test.Fail(__LINE__, "env=OFF must disable chunk streaming");
	}
	const int unsetResult = projectv::core::UnsetEnvVar("PROJECTV_CHUNK_STREAMING");
	(void)unsetResult;
}

void TestEnqueueTracksSize(StreamTestContext &test)
{
	const int setenvResult = projectv::core::SetEnvVar("PROJECTV_CHUNK_STREAMING", "ON", 1);
	if (setenvResult != 0) {
		test.Fail(__LINE__, "setenv failed");
		return;
	}
	const uint32_t before = projectv::voxel::DrainChunkStreamQueueSize();
	projectv::voxel::ChunkStreamRequest req{};
	req.chunkIndex = 0u;
	req.priority = 1u;
	if (!projectv::voxel::EnqueueChunkStreamRequest(req)) {
		test.Fail(__LINE__, "EnqueueChunkStreamRequest must succeed when enabled");
	}
	const uint32_t after = projectv::voxel::DrainChunkStreamQueueSize();
	if (after <= before) {
		test.Fail(__LINE__, "Queue size must grow after enqueue");
	}
	const int unsetResult = projectv::core::UnsetEnvVar("PROJECTV_CHUNK_STREAMING");
	(void)unsetResult;
}

void TestDequeueEmptyReturnsError(StreamTestContext &test)
{
	const int setenvResult = projectv::core::SetEnvVar("PROJECTV_CHUNK_STREAMING", "OFF", 1);
	if (setenvResult != 0) {
		test.Fail(__LINE__, "setenv failed");
		return;
	}
	auto result = projectv::voxel::TryDequeueChunkData();
	if (result.has_value()) {
		test.Fail(__LINE__, "Dequeue on disabled streamer must return error");
	}
	if (result.error() != projectv::voxel::ChunkStreamError::NotInitialized) {
		test.Fail(__LINE__, "Expected NotInitialized error");
	}
	const int unsetResult = projectv::core::UnsetEnvVar("PROJECTV_CHUNK_STREAMING");
	(void)unsetResult;
}

void TestWorkerActiveFlagLifecycle(StreamTestContext &test)
{
	const int setenvResult = projectv::core::SetEnvVar("PROJECTV_CHUNK_STREAMING", "ON", 1);
	if (setenvResult != 0) {
		test.Fail(__LINE__, "setenv failed");
		return;
	}
	const bool wasActive = projectv::voxel::IsChunkStreamerWorkerActive();
	projectv::voxel::StartChunkStreamerWorker();
	const bool nowActive = projectv::voxel::IsChunkStreamerWorkerActive();
	if (!nowActive) {
		test.Fail(__LINE__, "Worker must be active after StartChunkStreamerWorker");
	}
	projectv::voxel::StopChunkStreamerWorker();
	const bool afterStop = projectv::voxel::IsChunkStreamerWorkerActive();
	if (afterStop) {
		test.Fail(__LINE__, "Worker must be inactive after StopChunkStreamerWorker");
	}
	(void)wasActive;
	const int unsetResult = projectv::core::UnsetEnvVar("PROJECTV_CHUNK_STREAMING");
	(void)unsetResult;
}

void TestCachePathFromEnv(StreamTestContext &test)
{
	const int setenvResult = projectv::core::SetEnvVar("PROJECTV_CHUNK_PATH", "/tmp/projectv_test_chunks", 1);
	if (setenvResult != 0) {
		test.Fail(__LINE__, "setenv failed");
		return;
	}
	const std::string path = projectv::voxel::GetChunkStreamerCachePath();
	if (path != "/tmp/projectv_test_chunks") {
		test.Fail(__LINE__, "GetChunkStreamerCachePath must return PROJECTV_CHUNK_PATH value");
	}
	const int unsetResult = projectv::core::UnsetEnvVar("PROJECTV_CHUNK_PATH");
	(void)unsetResult;
}

void TestCachePathFallback(StreamTestContext &test)
{
	const int unsetResult1 = projectv::core::UnsetEnvVar("PROJECTV_CHUNK_PATH");
	(void)unsetResult1;
	const std::string path = projectv::voxel::GetChunkStreamerCachePath();
	if (path.empty()) {
		test.Fail(__LINE__, "GetChunkStreamerCachePath must not return empty string");
	}
}

void TestProcessPendingRequestsStopToken(StreamTestContext &test)
{
	std::stop_source stopSource;
	std::jthread worker(projectv::voxel::ProcessPendingRequests, stopSource.get_token());
	std::this_thread::sleep_for(std::chrono::milliseconds(5));
	stopSource.request_stop();
	worker.join();
	if (!stopSource.stop_requested()) {
		test.Fail(__LINE__, "Stop token must be honoured by ProcessPendingRequests");
	}
}

void TestEnqueueStartsWorkerLazy(StreamTestContext &test)
{
	const int setenvResult = projectv::core::SetEnvVar("PROJECTV_CHUNK_STREAMING", "ON", 1);
	if (setenvResult != 0) {
		test.Fail(__LINE__, "setenv failed");
		return;
	}
	projectv::voxel::StopChunkStreamerWorker();
	projectv::voxel::ChunkStreamRequest req{};
	req.chunkIndex = 1u;
	projectv::voxel::EnqueueChunkStreamRequest(req);
	const bool active = projectv::voxel::IsChunkStreamerWorkerActive();
	if (!active) {
		test.Fail(__LINE__, "EnqueueChunkStreamRequest must start worker lazily");
	}
	projectv::voxel::StopChunkStreamerWorker();
	const int unsetResult = projectv::core::UnsetEnvVar("PROJECTV_CHUNK_STREAMING");
	(void)unsetResult;
}

void TestPrebakeVersionStartsZero(StreamTestContext &test)
{
	// The version is monotonic and non-zero only after a successful bake.
	// Without a bake, IsChunkStreamerPrebakeReady must be false.
	if (projectv::voxel::IsChunkStreamerPrebakeReady()) {
		test.Fail(__LINE__, "Prebake must be unready before first bake");
	}
}

void TestBakeAllChunksDisabledWhenStreamingOff(StreamTestContext &test)
{
	const int setenvResult = projectv::core::SetEnvVar("PROJECTV_CHUNK_STREAMING", "OFF", 1);
	if (setenvResult != 0) {
		test.Fail(__LINE__, "setenv failed");
		return;
	}
	const VoxelWorld world{};
	projectv::voxel::ChunkPrebakeStats stats{};
	const bool result = projectv::voxel::BakeAllChunksToDisk(world, stats);
	if (result) {
		test.Fail(__LINE__, "BakeAllChunksToDisk must return false when streaming disabled");
	}
	const int unsetResult = projectv::core::UnsetEnvVar("PROJECTV_CHUNK_STREAMING");
	(void)unsetResult;
}

void TestPreloadAroundCameraDisabledWhenStreamingOff(StreamTestContext &test)
{
	const int setenvResult = projectv::core::SetEnvVar("PROJECTV_CHUNK_STREAMING", "OFF", 1);
	if (setenvResult != 0) {
		test.Fail(__LINE__, "setenv failed");
		return;
	}
	const VoxelWorld world{};
	const uint32_t enqueued = projectv::voxel::PreloadChunksAroundCamera(world, 0.0f, 0.0f, 0.0f, 4u);
	if (enqueued != 0u) {
		test.Fail(__LINE__, "PreloadChunksAroundCamera must return 0 when streaming disabled");
	}
	const int unsetResult = projectv::core::UnsetEnvVar("PROJECTV_CHUNK_STREAMING");
	(void)unsetResult;
}

void TestPreloadAroundCameraEmptyWorldReturnsZero(StreamTestContext &test)
{
	const int setenvResult = projectv::core::SetEnvVar("PROJECTV_CHUNK_STREAMING", "ON", 1);
	if (setenvResult != 0) {
		test.Fail(__LINE__, "setenv failed");
		return;
	}
	VoxelWorld world{};
	world.chunkSize = 0;
	const uint32_t enqueued = projectv::voxel::PreloadChunksAroundCamera(world, 0.0f, 0.0f, 0.0f, 4u);
	if (enqueued != 0u) {
		test.Fail(__LINE__, "PreloadChunksAroundCamera must return 0 for empty world with chunkSize=0");
	}
	projectv::voxel::StopChunkStreamerWorker();
	const int unsetResult = projectv::core::UnsetEnvVar("PROJECTV_CHUNK_STREAMING");
	(void)unsetResult;
}

void TestPreloadAroundCameraPopulatesWhenChunksExist(StreamTestContext &test)
{
	const int setenvResult = projectv::core::SetEnvVar("PROJECTV_CHUNK_STREAMING", "ON", 1);
	if (setenvResult != 0) {
		test.Fail(__LINE__, "setenv failed");
		return;
	}
	VoxelWorld world{};
	world.chunkSize = 8;
	world.min = {0, 0, 0};
	world.maxExclusive = {16, 8, 16};
	world.width = 16;
	world.height = 8;
	world.depth = 16;
	world.sparseStorage.Reset(16, 8, 16);
	world.chunks.resize(2);
	world.chunks[0].min = {0, 0, 0};
	world.chunks[0].maxExclusive = {8, 8, 8};
	world.chunks[0].rebuildQueued = false;
	world.chunks[0].isStatic = true;
	world.chunks[1].min = {8, 0, 0};
	world.chunks[1].maxExclusive = {16, 8, 8};
	world.chunks[1].rebuildQueued = false;
	world.chunks[1].isStatic = true;
	const uint32_t enqueued = projectv::voxel::PreloadChunksAroundCamera(world, 0.0f, 0.0f, 0.0f, 1u);
	if (enqueued != 2u) {
		std::fprintf(stderr, "enqueued=%u expected=2 (chunks in [0..1]x[0..0]x[0..1] grid)\n", enqueued);
		test.Fail(__LINE__, "PreloadChunksAroundCamera with radius=1 must enqueue all chunks in range");
	}
	projectv::voxel::StopChunkStreamerWorker();
	const int unsetResult = projectv::core::UnsetEnvVar("PROJECTV_CHUNK_STREAMING");
	(void)unsetResult;
}

void TestPreloadAroundCameraZeroRadiusEnqueuesSingleChunk(StreamTestContext &test)
{
	const int setenvResult = projectv::core::SetEnvVar("PROJECTV_CHUNK_STREAMING", "ON", 1);
	if (setenvResult != 0) {
		test.Fail(__LINE__, "setenv failed");
		return;
	}
	VoxelWorld world{};
	world.chunkSize = 8;
	world.min = {0, 0, 0};
	world.maxExclusive = {8, 8, 8};
	world.width = 8;
	world.height = 8;
	world.depth = 8;
	world.sparseStorage.Reset(8, 8, 8);
	world.chunks.resize(1);
	world.chunks[0].min = {0, 0, 0};
	world.chunks[0].maxExclusive = {8, 8, 8};
	world.chunks[0].rebuildQueued = false;
	world.chunks[0].isStatic = true;
	const uint32_t enqueued = projectv::voxel::PreloadChunksAroundCamera(world, 0.0f, 0.0f, 0.0f, 0u);
	if (enqueued != 1u) {
		std::fprintf(stderr, "enqueued=%u expected=1 (radius=0 enqueues the camera's chunk)\n", enqueued);
		test.Fail(__LINE__, "PreloadChunksAroundCamera with radius=0 must enqueue the camera's chunk");
	}
	projectv::voxel::StopChunkStreamerWorker();
	const int unsetResult = projectv::core::UnsetEnvVar("PROJECTV_CHUNK_STREAMING");
	(void)unsetResult;
}

void TestPrebakeVersionIncrementsAfterBake(StreamTestContext &test)
{
	const int setenvResult = projectv::core::SetEnvVar("PROJECTV_CHUNK_STREAMING", "ON", 1);
	if (setenvResult != 0) {
		test.Fail(__LINE__, "setenv failed");
		return;
	}
	const uint64_t before = projectv::voxel::GetChunkStreamerPrebakeVersion();
	VoxelWorld world{};
	world.chunkSize = 8;
	world.min = {0, 0, 0};
	world.maxExclusive = {8, 8, 8};
	world.width = 8;
	world.height = 8;
	world.depth = 8;
	world.sparseStorage.Reset(8, 8, 8);
	world.chunks.resize(1);
	world.chunks[0].min = {0, 0, 0};
	world.chunks[0].maxExclusive = {8, 8, 8};
	world.chunks[0].rebuildQueued = false;
	world.chunks[0].isStatic = true;
	projectv::voxel::ChunkPrebakeStats stats{};
	const bool baked = projectv::voxel::BakeAllChunksToDisk(world, stats);
	if (!baked) {
		test.Fail(__LINE__, "BakeAllChunksToDisk on populated world must return true");
	}
	const uint64_t after = projectv::voxel::GetChunkStreamerPrebakeVersion();
	if (after <= before) {
		std::fprintf(stderr, "prebakeVersion before=%llu after=%llu\n",
					 static_cast<unsigned long long>(before), static_cast<unsigned long long>(after));
		test.Fail(__LINE__, "BakeAllChunksToDisk must bump prebake version");
	}
	projectv::voxel::StopChunkStreamerWorker();
	const int unsetResult = projectv::core::UnsetEnvVar("PROJECTV_CHUNK_STREAMING");
	(void)unsetResult;
}

} // namespace

int main() // NOLINT(*-exception-escape)
{
	StreamTestContext test{};
	TestEnvGateDefault(test);
	TestEnvGateOff(test);
	TestEnqueueTracksSize(test);
	TestDequeueEmptyReturnsError(test);
	TestWorkerActiveFlagLifecycle(test);
	TestCachePathFromEnv(test);
	TestCachePathFallback(test);
	TestProcessPendingRequestsStopToken(test);
	TestEnqueueStartsWorkerLazy(test);
	TestPrebakeVersionStartsZero(test);
	TestBakeAllChunksDisabledWhenStreamingOff(test);
	TestPreloadAroundCameraDisabledWhenStreamingOff(test);
	TestPreloadAroundCameraEmptyWorldReturnsZero(test);
	TestPreloadAroundCameraPopulatesWhenChunksExist(test);
	TestPreloadAroundCameraZeroRadiusEnqueuesSingleChunk(test);
	TestPrebakeVersionIncrementsAfterBake(test);

	if (test.failures > 0) {
		std::fprintf(stderr, "ProjectVChunkStreamingTests: %d failure(s)\n", test.failures);
		return 1;
	}
	std::puts("ProjectVChunkStreamingTests passed");
	return 0;
}

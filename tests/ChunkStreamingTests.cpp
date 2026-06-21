#include "voxel/ChunkStreamer.hpp"

#include <cstdio>
#include <cstdlib>
#include <string_view>

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
	const int setenvResult = setenv("PROJECTV_CHUNK_STREAMING", "OFF", 1);
	if (setenvResult != 0) {
		test.Fail(__LINE__, "setenv failed");
		return;
	}
	if (projectv::voxel::IsChunkStreamingEnabled()) {
		test.Fail(__LINE__, "env=OFF must disable chunk streaming");
	}
	const int unsetResult = unsetenv("PROJECTV_CHUNK_STREAMING");
	(void)unsetResult;
}

void TestEnqueueTracksSize(StreamTestContext &test)
{
	const int setenvResult = setenv("PROJECTV_CHUNK_STREAMING", "ON", 1);
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
	const int unsetResult = unsetenv("PROJECTV_CHUNK_STREAMING");
	(void)unsetResult;
}

void TestDequeueEmptyReturnsError(StreamTestContext &test)
{
	const int setenvResult = setenv("PROJECTV_CHUNK_STREAMING", "OFF", 1);
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
	const int unsetResult = unsetenv("PROJECTV_CHUNK_STREAMING");
	(void)unsetResult;
}

}  // namespace

int main()
{
	StreamTestContext test{};
	TestEnvGateDefault(test);
	TestEnvGateOff(test);
	TestEnqueueTracksSize(test);
	TestDequeueEmptyReturnsError(test);

	if (test.failures > 0) {
		std::fprintf(stderr, "ProjectVChunkStreamingTests: %d failure(s)\n", test.failures);
		return 1;
	}
	std::puts("ProjectVChunkStreamingTests passed");
	return 0;
}

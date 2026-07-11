#include "physics/PhysicsWorld.hpp"
#include "voxel/VoxelWorld.hpp"

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {

struct TestContext {
	int failures = 0;
	void Fail(const int line, const std::string_view message)
	{
		std::fprintf(stderr, "Test failure at line %d: %.*s\n", line, static_cast<int>(message.size()), message.data());
		++failures;
	}
};

void TestQueueChunkRebuildRequestRejectsNullPhysics(TestContext &context)
{
	(void)context;
	QueueChunkRebuildRequest(nullptr, 0u);
}

void TestQueueChunkRebuildRequestAppendsToQueue(TestContext &context)
{
	PhysicsState *physics = CreatePhysicsState();
	QueueChunkRebuildRequest(physics, 0u);
	QueueChunkRebuildRequest(physics, 1u);
	QueueChunkRebuildRequest(physics, 0u);
	const uint32_t pending = GetPendingChunkRebuildCount(physics);
	if (pending != 3u) {
		std::fprintf(stderr, "pending=%u expected=3\n", pending);
		context.Fail(__LINE__, "QueueChunkRebuildRequest must append to queue (no dedup)");
	}
	DestroyPhysicsState(physics);
}

void TestProcessChunkRebuildQueueRejectsNullInputs(TestContext &context)
{
	if (ProcessChunkRebuildQueue(nullptr, nullptr) != 0u) {
		context.Fail(__LINE__, "ProcessChunkRebuildQueue(null, null) must return 0");
	}
	PhysicsState *physics = CreatePhysicsState();
	const VoxelWorld world{};
	if (ProcessChunkRebuildQueue(physics, nullptr) != 0u) {
		context.Fail(__LINE__, "ProcessChunkRebuildQueue(physics, null world) must return 0");
	}
	if (ProcessChunkRebuildQueue(physics, &world) != 0u) {
		context.Fail(__LINE__, "ProcessChunkRebuildQueue(physics, empty world) must return 0");
	}
	DestroyPhysicsState(physics);
}

void TestGetPendingChunkRebuildCountRejectsNull(TestContext &context)
{
	const uint32_t pending = GetPendingChunkRebuildCount(nullptr);
	if (pending != 0u) {
		context.Fail(__LINE__, "GetPendingChunkRebuildCount(null) must return 0");
	}
}

void TestGetChunkBodyCountRejectsNull(TestContext &context)
{
	const uint32_t count = GetChunkBodyCount(nullptr);
	if (count != 0u) {
		context.Fail(__LINE__, "GetChunkBodyCount(null) must return 0");
	}
}

void TestProcessChunkRebuildQueueEmptyQueueReturnsZero(TestContext &context)
{
	PhysicsState *physics = CreatePhysicsState();
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
	const uint32_t processed = ProcessChunkRebuildQueue(physics, &world);
	if (processed != 0u) {
		std::fprintf(stderr, "processed=%u expected=0\n", processed);
		context.Fail(__LINE__, "Empty queue must return 0");
	}
	DestroyPhysicsState(physics);
}

}  // namespace

int main()
{
	TestContext context{};
	TestQueueChunkRebuildRequestRejectsNullPhysics(context);
	TestQueueChunkRebuildRequestAppendsToQueue(context);
	TestProcessChunkRebuildQueueRejectsNullInputs(context);
	TestGetPendingChunkRebuildCountRejectsNull(context);
	TestGetChunkBodyCountRejectsNull(context);
	TestProcessChunkRebuildQueueEmptyQueueReturnsZero(context);

	if (context.failures > 0) {
		std::fprintf(stderr, "ProjectVPhysicsIncrementalJoltTests: %d failure(s)\n", context.failures);
		return 1;
	}
	std::puts("ProjectVPhysicsIncrementalJoltTests passed");
	return 0;
}

#include "physics/PhysicsWorld.hpp"
#include "voxel/VoxelWorld.hpp"

#include <cstdio>
#include <string_view>
#include <vector>

namespace {

struct TestContext {
	int failures = 0;
	int passes = 0;
	void Fail(const int line, const std::string_view message)
	{
		std::fprintf(stderr, "Test failure at line %d: %.*s\n", line, static_cast<int>(message.size()), message.data());
		++failures;
	}
	void Pass() { ++passes; }
};

VoxelWorldConfig MakeSmallFlatWorldConfig()
{
	VoxelWorldConfig config{};
	config.floorSize = 16;
	config.worldTopY = 4;
	config.padding = 2;
	return config;
}

VoxelWorldConfig MakeFlatBenchSizedWorldConfig()
{
	VoxelWorldConfig config{};
	config.floorSize = 64;
	config.worldTopY = 18;
	config.padding = 8;
	return config;
}

std::unique_ptr<VoxelWorld> MakeFlatWorld(const VoxelWorldConfig &config)
{
	std::unique_ptr<VoxelWorld> world = std::make_unique<VoxelWorld>();
	world->config = config;
	world->scenePreset = VoxelScenePreset::FlatBenchmark;
	world->chunkSize = config.chunkSize;
	const int halfFloor = config.floorSize / 2;
	world->min = Int3{
		-halfFloor - config.padding,
		config.floorY,
		-halfFloor - config.padding};
	world->maxExclusive = Int3{
		halfFloor + config.padding,
		config.worldTopY + config.padding,
		halfFloor + config.padding};
	world->width = world->maxExclusive.x - world->min.x;
	world->height = world->maxExclusive.y - world->min.y;
	world->depth = world->maxExclusive.z - world->min.z;
	world->sparseStorage.Reset(world->width, world->height, world->depth);
	world->chunkCountX = (world->width + config.chunkSize - 1) / config.chunkSize;
	world->chunkCountY = (world->height + config.chunkSize - 1) / config.chunkSize;
	world->chunkCountZ = (world->depth + config.chunkSize - 1) / config.chunkSize;
	const size_t chunkCount = static_cast<size_t>(world->chunkCountX) *
							  static_cast<size_t>(world->chunkCountY) *
							  static_cast<size_t>(world->chunkCountZ);
	world->chunks.resize(chunkCount);
	for (int chunkZ = 0; chunkZ < world->chunkCountZ; ++chunkZ) {
		for (int chunkY = 0; chunkY < world->chunkCountY; ++chunkY) {
			for (int chunkX = 0; chunkX < world->chunkCountX; ++chunkX) {
				const size_t chunkIndex = GetVoxelChunkIndex(*world, {chunkX, chunkY, chunkZ});
				VoxelChunk &chunk = world->chunks[chunkIndex];
				chunk.min = Int3{
					world->min.x + chunkX * world->chunkSize,
					world->min.y + chunkY * world->chunkSize,
					world->min.z + chunkZ * world->chunkSize};
				chunk.maxExclusive = Int3{
					std::min(chunk.min.x + world->chunkSize, world->maxExclusive.x),
					std::min(chunk.min.y + world->chunkSize, world->maxExclusive.y),
					std::min(chunk.min.z + world->chunkSize, world->maxExclusive.z)};
				chunk.isStatic = true;
			}
		}
	}
	for (int z = -halfFloor; z < halfFloor; ++z) {
		for (int x = -halfFloor; x < halfFloor; ++x) {
			SetVoxelMaterial(*world, {x, config.floorY, z},
							 (x + z & 1) == 0 ? VoxelMaterial::FloorWhite : VoxelMaterial::FloorGray,
							 nullptr);
		}
	}
	return world;
}

std::unique_ptr<VoxelWorld> MakeFlatBenchSizedWorld()
{
	return MakeFlatWorld(MakeFlatBenchSizedWorldConfig());
}

std::unique_ptr<VoxelWorld> MakeSmallFlatWorld()
{
	return MakeFlatWorld(MakeSmallFlatWorldConfig());
}

void TestSyncPhysicsWorldInitialLoadBuildsFullBody(TestContext &context)
{
	PhysicsState *physics = CreatePhysicsState();
	const std::unique_ptr<VoxelWorld> world = MakeSmallFlatWorld();
	if (!SyncPhysicsWorld(physics, world.get())) {
		context.Fail(__LINE__, "SyncPhysicsWorld on initial load must succeed");
		DestroyPhysicsState(physics);
		return;
	}
	if (GetChunkBodyCount(physics) == 0u) {
		context.Fail(__LINE__, "After initial load chunk bodies must be populated");
	}
	if (GetPhysicsWorldSyncVersion(physics) != world->editVersion) {
		context.Fail(__LINE__, "Synced world edit version must match world after initial load");
	}
	DestroyPhysicsState(physics);
	context.Pass();
}

void TestSyncPhysicsWorldNoOpOnSameVersion(TestContext &context)
{
	PhysicsState *physics = CreatePhysicsState();
	const std::unique_ptr<VoxelWorld> world = MakeSmallFlatWorld();
	if (!SyncPhysicsWorld(physics, world.get())) {
		context.Fail(__LINE__, "First sync must succeed");
		DestroyPhysicsState(physics);
		return;
	}
	const uint64_t syncedVersion = GetPhysicsWorldSyncVersion(physics);
	if (!SyncPhysicsWorld(physics, world.get())) {
		context.Fail(__LINE__, "Second sync must succeed (no edit)");
	}
	if (GetPhysicsWorldSyncVersion(physics) != syncedVersion) {
		context.Fail(__LINE__, "Version must not change when no edit happened");
	}
	DestroyPhysicsState(physics);
	context.Pass();
}

void TestSyncPhysicsWorldIncrementalAfterSmallEdit(TestContext &context)
{
	PhysicsState *physics = CreatePhysicsState();
	const std::unique_ptr<VoxelWorld> world = MakeSmallFlatWorld();
	if (!SyncPhysicsWorld(physics, world.get())) {
		context.Fail(__LINE__, "Initial sync must succeed");
		DestroyPhysicsState(physics);
		return;
	}

	SetVoxelMaterial(*world, {0, 1, 0}, VoxelMaterial::Glass, physics);
	QueueChunkRebuildRequest(physics, GetVoxelChunkIndex(*world, GetVoxelChunkCoord(*world, {0, 1, 0})));

	if (!SyncPhysicsWorld(physics, world.get())) {
		context.Fail(__LINE__, "Incremental sync must succeed");
		DestroyPhysicsState(physics);
		return;
	}
	if (GetPhysicsWorldSyncVersion(physics) != world->editVersion) {
		context.Fail(__LINE__, "Incremental sync must advance version to world edit version");
	}
	DestroyPhysicsState(physics);
	context.Pass();
}

void TestSyncPhysicsWorldNullWorldClearsState(TestContext &context)
{
	PhysicsState *physics = CreatePhysicsState();
	const std::unique_ptr<VoxelWorld> world = MakeSmallFlatWorld();
	if (!SyncPhysicsWorld(physics, world.get())) {
		context.Fail(__LINE__, "Initial sync must succeed");
		DestroyPhysicsState(physics);
		return;
	}
	if (!SyncPhysicsWorld(physics, nullptr)) {
		context.Fail(__LINE__, "Null-world sync must succeed");
	}
	if (GetChunkBodyCount(physics) != 0u) {
		context.Fail(__LINE__, "Null-world sync must clear chunk bodies");
	}
	DestroyPhysicsState(physics);
	context.Pass();
}

void TestFluidAirTransitionBumpsEditVersion(TestContext &context)
{
	const std::unique_ptr<VoxelWorld> world = MakeSmallFlatWorld();
	const uint64_t versionBefore = world->editVersion;
	SetVoxelMaterial(*world, {0, 1, 0}, VoxelMaterial::Fluid, nullptr);
	if (world->editVersion != versionBefore + 1u) {
		std::fprintf(stderr, "editVersion before=%llu after=%llu\n",
					 static_cast<unsigned long long>(versionBefore),
					 static_cast<unsigned long long>(world->editVersion));
		context.Fail(__LINE__, "Air->Fluid transition must bump editVersion");
	}

	const uint64_t versionAfterPlace = world->editVersion;
	SetVoxelMaterial(*world, {0, 1, 0}, VoxelMaterial::Air, nullptr);
	if (world->editVersion != versionAfterPlace + 1u) {
		std::fprintf(stderr, "editVersion before=%llu after=%llu\n",
					 static_cast<unsigned long long>(versionAfterPlace),
					 static_cast<unsigned long long>(world->editVersion));
		context.Fail(__LINE__, "Fluid->Air transition must bump editVersion");
	}
	context.Pass();
}

void TestSolidEditBumpsEditVersion(TestContext &context)
{
	const std::unique_ptr<VoxelWorld> world = MakeSmallFlatWorld();
	const uint64_t versionBefore = world->editVersion;
	SetVoxelMaterial(*world, {0, 1, 0}, VoxelMaterial::Glass, nullptr);
	if (world->editVersion == versionBefore) {
		context.Fail(__LINE__, "Air->Glass transition must bump editVersion");
	}
	context.Pass();
}

void TestFluidAirTransitionQueuesPhysicsRebuild(TestContext &context)
{
	PhysicsState *physics = CreatePhysicsState();
	const std::unique_ptr<VoxelWorld> world = MakeSmallFlatWorld();
	if (!SyncPhysicsWorld(physics, world.get())) {
		context.Fail(__LINE__, "Initial sync must succeed");
		DestroyPhysicsState(physics);
		return;
	}
	const uint32_t pendingBefore = GetPendingChunkRebuildCount(physics);
	SetVoxelMaterial(*world, {0, 1, 0}, VoxelMaterial::Fluid, physics);
	const uint32_t pendingAfter = GetPendingChunkRebuildCount(physics);
	if (pendingAfter <= pendingBefore) {
		std::fprintf(stderr, "pending before=%u after=%u\n", pendingBefore, pendingAfter);
		context.Fail(__LINE__, "Air->Fluid with physics must enqueue a physics chunk rebuild");
	}
	DestroyPhysicsState(physics);
	context.Pass();
}

void TestRebuildStaticWorldBodyFromChunkShapesAfterEdit(TestContext &context)
{
	PhysicsState *physics = CreatePhysicsState();
	const std::unique_ptr<VoxelWorld> world = MakeSmallFlatWorld();
	if (!SyncPhysicsWorld(physics, world.get())) {
		context.Fail(__LINE__, "Initial sync must succeed");
		DestroyPhysicsState(physics);
		return;
	}

	SetVoxelMaterial(*world, {0, 1, 0}, VoxelMaterial::Glass, physics);
	QueueChunkRebuildRequest(physics, GetVoxelChunkIndex(*world, GetVoxelChunkCoord(*world, {0, 1, 0})));

	if (!SyncPhysicsWorld(physics, world.get())) {
		context.Fail(__LINE__, "Incremental sync must succeed");
		DestroyPhysicsState(physics);
		return;
	}

	if (!RebuildStaticWorldBodyFromChunkShapes(*physics, *world)) {
		context.Fail(__LINE__, "RebuildStaticWorldBodyFromChunkShapes must succeed");
	}
	DestroyPhysicsState(physics);
	context.Pass();
}

void TestPhysicsSyncBoundaryEditTriggersMultiChunkRebuild(TestContext &context)
{
	PhysicsState *physics = CreatePhysicsState();
	const std::unique_ptr<VoxelWorld> world = MakeSmallFlatWorld();
	if (!SyncPhysicsWorld(physics, world.get())) {
		context.Fail(__LINE__, "Initial sync must succeed");
		DestroyPhysicsState(physics);
		return;
	}

	const Int3 boundaryEdit = {world->min.x + 8, 1, 0};
	SetVoxelMaterial(*world, boundaryEdit, VoxelMaterial::Glass, physics);

	if (!SyncPhysicsWorld(physics, world.get())) {
		context.Fail(__LINE__, "Boundary edit sync must succeed");
		DestroyPhysicsState(physics);
		return;
	}

	if (GetChunkBodyCount(physics) < 2u) {
		context.Fail(__LINE__, "Boundary edit must populate at least 2 chunk bodies");
	}
	if (!RebuildStaticWorldBodyFromChunkShapes(*physics, *world)) {
		context.Fail(__LINE__, "RebuildStaticWorldBodyFromChunkShapes must succeed after multi-chunk edit");
	}

	DestroyPhysicsState(physics);
	context.Pass();
}

void TestSyncPhysicsWorldFlatBenchSizedWorldFitsInCapacity(TestContext &context)
{
	PhysicsState *physics = CreatePhysicsState();
	const std::unique_ptr<VoxelWorld> world = MakeFlatBenchSizedWorld();
	const size_t totalChunks = world->chunks.size();
	std::printf("TestSyncPhysicsWorldFlatBenchSizedWorldFitsInCapacity: chunks=%zu\n", totalChunks);
	if (!SyncPhysicsWorld(physics, world.get())) {
		context.Fail(__LINE__, "SyncPhysicsWorld on FlatBench-sized world (80x26x80 = 400 chunks) must succeed within kMaxPhysicsBodies");
		DestroyPhysicsState(physics);
		return;
	}
	if (GetChunkBodyCount(physics) == 0u) {
		context.Fail(__LINE__, "FlatBench-sized world must produce non-zero chunk body count");
	}
	DestroyPhysicsState(physics);
	context.Pass();
}

void TestGetPhysicsBroadphaseStatsNullReturnsZeroed(TestContext &context)
{
	const auto [totalBodies, maxBodies, staticBodies, dynamicBodies, activeDynamicBodies,
				kinematicBodies, activeKinematicBodies, pendingChunkRebuilds, chunkStaticBodies,
				chunkMergedBoxesEntries] = GetPhysicsBroadphaseStats(nullptr);
	if (totalBodies != 0u || maxBodies != 0u || staticBodies != 0u ||
		dynamicBodies != 0u || activeDynamicBodies != 0u ||
		kinematicBodies != 0u || activeKinematicBodies != 0u ||
		pendingChunkRebuilds != 0u || chunkStaticBodies != 0u ||
		chunkMergedBoxesEntries != 0u) {
		context.Fail(
			__LINE__,
			"GetPhysicsBroadphaseStats(nullptr) must return all-zero struct");
	}
}

void TestGetPhysicsBroadphaseStatsAfterSync(TestContext &context)
{
	PhysicsState *physicsState = CreatePhysicsState();
	const std::unique_ptr<VoxelWorld> world = MakeSmallFlatWorld();
	if (!SyncPhysicsWorld(physicsState, world.get())) {
		context.Fail(__LINE__, "Sync must succeed for broadphase stats test");
		DestroyPhysicsState(physicsState);
		return;
	}
	const PhysicsBroadphaseStats stats = GetPhysicsBroadphaseStats(physicsState);
	if (stats.totalBodies == 0u) {
		context.Fail(__LINE__, "After initial sync, totalBodies must be > 0");
	}
	if (stats.staticBodies == 0u) {
		context.Fail(__LINE__, "After initial sync, staticBodies must be > 0");
	}
	if (stats.maxBodies == 0u) {
		context.Fail(__LINE__, "maxBodies must be > 0 (Jolt Init capacity)");
	}
	if (stats.chunkStaticBodies == 0u) {
		context.Fail(__LINE__, "After initial sync, chunkStaticBodies must be > 0");
	}
	DestroyPhysicsState(physicsState);
	context.Pass();
}

} // namespace

int main() // NOLINT(*-exception-escape): MSVC STL vector resize may throw; terminating the test process is intended.
{
	TestContext context{};
	TestSyncPhysicsWorldInitialLoadBuildsFullBody(context);
	TestSyncPhysicsWorldNoOpOnSameVersion(context);
	TestSyncPhysicsWorldIncrementalAfterSmallEdit(context);
	TestSyncPhysicsWorldNullWorldClearsState(context);
	TestFluidAirTransitionBumpsEditVersion(context);
	TestSolidEditBumpsEditVersion(context);
	TestFluidAirTransitionQueuesPhysicsRebuild(context);
	TestRebuildStaticWorldBodyFromChunkShapesAfterEdit(context);
	TestSyncPhysicsWorldFlatBenchSizedWorldFitsInCapacity(context);
	TestPhysicsSyncBoundaryEditTriggersMultiChunkRebuild(context);
	TestGetPhysicsBroadphaseStatsNullReturnsZeroed(context);
	TestGetPhysicsBroadphaseStatsAfterSync(context);

	std::printf("ProjectVPhysicsSyncTests: %d passed, %d failed\n", context.passes, context.failures);
	if (context.failures > 0) {
		return 1;
	}
	std::puts("ProjectVPhysicsSyncTests passed");
	return 0;
}

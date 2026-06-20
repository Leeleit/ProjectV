
#include "voxel/VoxelWorld.hpp"

#include "core/Types.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>

namespace {
struct TestContext {
	int failures = 0;

	void Fail(const int line, const std::string_view message)
	{
		std::fprintf(stderr, "Test failure at line %d: %.*s\n", line, static_cast<int>(message.size()), message.data());
		++failures;
	}
};

template <typename T>
void ExpectEqual(
	TestContext &context,
	const T &expected,
	const T &actual,
	const int line,
	const std::string_view expr)
{
	if (!(expected == actual)) {
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"%.*s (expected %lld, got %lld)",
			static_cast<int>(expr.size()),
			expr.data(),
			static_cast<long long>(expected),
			static_cast<long long>(actual));
		context.Fail(line, buffer);
	}
}

void ExpectTrue(TestContext &context, const bool condition, const int line, const std::string_view expr)
{
	if (!condition) {
		context.Fail(line, expr);
	}
}

#define EXPECT_TRUE(context, expr) ExpectTrue(context, (expr), __LINE__, #expr)
#define EXPECT_EQ(context, expected, actual) ExpectEqual(context, (expected), (actual), __LINE__, #actual)


VoxelWorld MakeFluidCATestWorld(const int width, const int height, const int depth)
{
	constexpr int chunkSize = 4;
	VoxelWorld world{};
	world.min = {0, 0, 0};
	world.maxExclusive = {width, height, depth};
	world.width = width;
	world.height = height;
	world.depth = depth;
	world.chunkSize = chunkSize;
	world.chunkCountX = (width + chunkSize - 1) / chunkSize;
	world.chunkCountY = (height + chunkSize - 1) / chunkSize;
	world.chunkCountZ = (depth + chunkSize - 1) / chunkSize;
	world.sparseStorage.Reset(width, height, depth);
	for (int z = 0; z < depth; ++z) { for (int y = 0; y < height; ++y) { for (int x = 0; x < width; ++x) { SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::Air); } } }
	world.chunks.assign(
		static_cast<size_t>(world.chunkCountX) *
		static_cast<size_t>(world.chunkCountY) *
		static_cast<size_t>(world.chunkCountZ),
		VoxelChunk{});
	for (int chunkZ = 0; chunkZ < world.chunkCountZ; ++chunkZ) {
		for (int chunkY = 0; chunkY < world.chunkCountY; ++chunkY) {
			for (int chunkX = 0; chunkX < world.chunkCountX; ++chunkX) {
				const size_t chunkIndex = GetVoxelChunkIndex(world, {chunkX, chunkY, chunkZ});
				auto &[min, maxExclusive, rebuildQueued, nonAirVoxelCount] = world.chunks[chunkIndex];
				min = {
					world.min.x + chunkX * world.chunkSize,
					world.min.y + chunkY * world.chunkSize,
					world.min.z + chunkZ * world.chunkSize,
				};
				maxExclusive = {
					std::min(min.x + world.chunkSize, world.maxExclusive.x),
					std::min(min.y + world.chunkSize, world.maxExclusive.y),
					std::min(min.z + world.chunkSize, world.maxExclusive.z),
				};
				rebuildQueued = false;
				nonAirVoxelCount = 0;
			}
		}
	}
	return world;
}

size_t CountFluid(const VoxelWorld &world)
{
	return static_cast<size_t>(std::ranges::count(
		BuildFlatVoxelSnapshot(world),
		static_cast<uint8_t>(VoxelMaterial::Fluid)));
}
}


void TestFluidCASingleCellFallsOneCellPerTick(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(8, 8, 8);
	SetVoxelMaterial(world, {4, 5, 4}, VoxelMaterial::Fluid);

	const uint32_t moved = UpdateFluidCA(world);
	EXPECT_EQ(context, 1u, moved);
	EXPECT_EQ(context, VoxelMaterial::Air, GetVoxelMaterial(world, {4, 5, 4}));
	EXPECT_EQ(context, VoxelMaterial::Fluid, GetVoxelMaterial(world, {4, 4, 4}));
}

void TestFluidCAColumnPercolatesAndSpreadsOnFloor(TestContext &context)
{
	constexpr int kColumnHeight = 4;
	constexpr int kStartY = 5;
	VoxelWorld world = MakeFluidCATestWorld(4, 16, 4);
	for (int y = kStartY; y < kStartY + kColumnHeight; ++y) {
		SetVoxelMaterial(world, {2, y, 2}, VoxelMaterial::Fluid);
	}
	const uint32_t initialCount = world.stats.fluidVoxelCount;
	EXPECT_EQ(context, static_cast<uint32_t>(kColumnHeight), initialCount);

	for (int tick = 0; tick < 30; ++tick) {
		UpdateFluidCA(world);
		EXPECT_TRUE(context, world.stats.fluidVoxelCount >= initialCount);
	}

	for (int y = kStartY + kColumnHeight; y < world.height; ++y) {
		EXPECT_EQ(context, VoxelMaterial::Air, GetVoxelMaterial(world, {2, y, 2}));
	}
}

void TestFluidCARestingOnFloorStaysPut(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(4, 4, 4);

	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::Glass);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid);

	for (int tick = 0; tick < 5; ++tick) {
		UpdateFluidCA(world);
		EXPECT_EQ(
			context,
			VoxelMaterial::Glass,
			GetVoxelMaterial(world, {2, 0, 2}));
	}

	EXPECT_TRUE(context, world.stats.fluidVoxelCount >= 1u);
}


void TestFluidCAFluidOnGlassStaysPutThenFallsWhenGlassBreaks(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(4, 4, 4);
	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::Glass);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid);

	UpdateFluidCA(world);
	EXPECT_EQ(context, VoxelMaterial::Glass, GetVoxelMaterial(world, {2, 0, 2}));
	EXPECT_TRUE(context, world.stats.fluidVoxelCount >= 1u);


	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::Air);


	for (int tick = 0; tick < 5; ++tick) {
		UpdateFluidCA(world);
		EXPECT_TRUE(context, world.stats.fluidVoxelCount >= 1u);
	}
}


void TestFluidCAFluidAtY0IsStable(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(4, 4, 4);
	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::Fluid);


	for (int tick = 0; tick < 5; ++tick) {
		UpdateFluidCA(world);
		EXPECT_TRUE(context, world.stats.fluidVoxelCount >= 1u);
	}
}

void TestFluidCAFluidDoesNotFallThroughPlatform(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(4, 4, 4);
	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::FloorWhite);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid);

	const uint32_t moved = UpdateFluidCA(world);
	EXPECT_TRUE(context, moved > 0u);

	EXPECT_EQ(
		context,
		VoxelMaterial::FloorWhite,
		GetVoxelMaterial(world, {2, 0, 2}));

	EXPECT_EQ(context, 1u, world.stats.fluidVoxelCount);
}

void TestFluidCASpreadsToCardinalNeighbour(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(4, 4, 4);

	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::Glass);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid);

	const uint32_t moved = UpdateFluidCA(world);
	EXPECT_TRUE(context, moved > 0u);
	EXPECT_EQ(context, VoxelMaterial::Air, GetVoxelMaterial(world, {2, 1, 2}));

	const bool spreadToXPlus = GetVoxelMaterial(world, {3, 1, 2}) == VoxelMaterial::Fluid;
	const bool spreadToXMinus = GetVoxelMaterial(world, {1, 1, 2}) == VoxelMaterial::Fluid;
	const bool spreadToZPlus = GetVoxelMaterial(world, {2, 1, 3}) == VoxelMaterial::Fluid;
	const bool spreadToZMinus = GetVoxelMaterial(world, {2, 1, 1}) == VoxelMaterial::Fluid;
	const int spreadCount = static_cast<int>(spreadToXPlus) + static_cast<int>(spreadToXMinus)
		+ static_cast<int>(spreadToZPlus) + static_cast<int>(spreadToZMinus);
	EXPECT_EQ(context, 1, spreadCount);

	EXPECT_EQ(context, 1u, world.stats.fluidVoxelCount);
}


void TestFluidCASpreadIsDeterministic(TestContext &context)
{
	auto run = [] {
		VoxelWorld world = MakeFluidCATestWorld(4, 4, 4);
		SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::FloorWhite);
		SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid);
		for (int tick = 0; tick < 5; ++tick) {
			UpdateFluidCA(world);
		}
		return BuildFlatVoxelSnapshot(world);
	};
	const std::vector<uint8_t> first = run();
	const std::vector<uint8_t> second = run();
	EXPECT_TRUE(context, first == second);
}


void TestFluidCAEmptyWorldShortCircuits(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(4, 4, 4);
	EXPECT_EQ(context, 0u, world.stats.fluidVoxelCount);
	EXPECT_EQ(context, 0u, UpdateFluidCA(world));
	EXPECT_EQ(context, 0u, UpdateFluidCA(world));
	EXPECT_EQ(context, 0u, world.stats.fluidVoxelCount);
}


void TestFluidCADeterministicAcrossRuns(TestContext &context)
{
	auto runScenario = [] {
		VoxelWorld world = MakeFluidCATestWorld(8, 12, 8);
		SetVoxelMaterial(world, {3, 5, 3}, VoxelMaterial::Fluid);
		SetVoxelMaterial(world, {3, 6, 3}, VoxelMaterial::Fluid);
		SetVoxelMaterial(world, {4, 7, 3}, VoxelMaterial::Fluid);
		SetVoxelMaterial(world, {5, 8, 3}, VoxelMaterial::Fluid);
		for (int x = 0; x < 8; ++x) {
			for (int z = 0; z < 8; ++z) {
				SetVoxelMaterial(world, {x, 0, z}, VoxelMaterial::FloorWhite);
			}
		}
		for (int tick = 0; tick < 20; ++tick) {
			UpdateFluidCA(world);
		}
		return BuildFlatVoxelSnapshot(world);
	};

	const std::vector<uint8_t> first = runScenario();
	const std::vector<uint8_t> second = runScenario();
	EXPECT_EQ(context, first.size(), second.size());
	EXPECT_TRUE(context, first == second);
}

void TestFluidCAStatsCountStaysConsistent(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(4, 16, 4);
	for (int y = 5; y < 10; ++y) {
		SetVoxelMaterial(world, {2, y, 2}, VoxelMaterial::Fluid);
	}

	for (int tick = 0; tick < 30; ++tick) {
		UpdateFluidCA(world);
		const size_t actual = CountFluid(world);

		EXPECT_EQ(context, static_cast<uint32_t>(actual), world.stats.fluidVoxelCount);
	}
}


void TestFluidCALongColumnAtWorldFloorSpreadsOut(TestContext &context)
{
	constexpr int kColumnHeight = 4;
	VoxelWorld world = MakeFluidCATestWorld(4, 8, 4);
	for (int y = 0; y < kColumnHeight; ++y) {
		SetVoxelMaterial(world, {2, y, 2}, VoxelMaterial::Fluid);
	}

	for (int tick = 0; tick < 20; ++tick) {
		UpdateFluidCA(world);
		EXPECT_EQ(
			context,
			static_cast<uint32_t>(kColumnHeight),
			world.stats.fluidVoxelCount);
	}


	size_t y0FluidCount = 0;
	for (int x = 0; x < 4; ++x) {
		for (int z = 0; z < 4; ++z) {
			if (GetVoxelMaterial(world, {x, 0, z}) == VoxelMaterial::Fluid) {
				++y0FluidCount;
			}
		}
	}
	EXPECT_TRUE(context, y0FluidCount > 0u);
}

void TestFluidCAColumnDrainsViaSpreadPlatformStaysIntact(TestContext &context)
{
	constexpr int kColumnHeight = 4;
	VoxelWorld world = MakeFluidCATestWorld(4, 12, 4);
	for (int x = 0; x < 4; ++x) {
		for (int z = 0; z < 4; ++z) {
			SetVoxelMaterial(world, {x, 0, z}, VoxelMaterial::FloorWhite);
		}
	}
	for (int y = 2; y < 2 + kColumnHeight; ++y) {
		SetVoxelMaterial(world, {2, y, 2}, VoxelMaterial::Fluid);
	}

	for (int tick = 0; tick < 30; ++tick) {
		UpdateFluidCA(world);
		EXPECT_EQ(context, static_cast<uint32_t>(kColumnHeight), world.stats.fluidVoxelCount);
	}

	EXPECT_EQ(
		context,
		VoxelMaterial::FloorWhite,
		GetVoxelMaterial(world, {2, 0, 2}));

	size_t y0FloorIntact = 0;
	for (int x = 0; x < 4; ++x) {
		for (int z = 0; z < 4; ++z) {
			if (GetVoxelMaterial(world, {x, 0, z}) == VoxelMaterial::FloorWhite) {
				++y0FloorIntact;
			}
		}
	}
	EXPECT_EQ(context, static_cast<size_t>(16), y0FloorIntact);
}


void TestFluidCAFreshWorldHasNoFluidAndStaysEmpty(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(4, 4, 4);
	EXPECT_EQ(context, 0u, world.stats.fluidVoxelCount);
	const std::vector<uint8_t> snapshot = BuildFlatVoxelSnapshot(world);
	for (int tick = 0; tick < 20; ++tick) {
		EXPECT_EQ(context, 0u, UpdateFluidCA(world));
	}
	EXPECT_TRUE(context, snapshot == BuildFlatVoxelSnapshot(world));
}


void TestFluidCAVoxelLabSphereFallOnGlassBreak(TestContext &context)
{

	constexpr int width = 24;
	constexpr int height = 17;
	constexpr int depth = 24;
	constexpr int chunkSize = 8;
	VoxelWorld world{};
	world.min = {-12, 0, -12};
	world.maxExclusive = {width - 12, height, depth - 12};
	world.width = width;
	world.height = height;
	world.depth = depth;
	world.chunkSize = chunkSize;
	world.chunkCountX = (width + chunkSize - 1) / chunkSize;
	world.chunkCountY = (height + chunkSize - 1) / chunkSize;
	world.chunkCountZ = (depth + chunkSize - 1) / chunkSize;
	world.sparseStorage.Reset(width, height, depth);
	for (int z = 0; z < depth; ++z) { for (int y = 0; y < height; ++y) { for (int x = 0; x < width; ++x) { SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::Air); } } }
	world.chunks.assign(
		static_cast<size_t>(world.chunkCountX) *
		static_cast<size_t>(world.chunkCountY) *
		static_cast<size_t>(world.chunkCountZ),
		VoxelChunk{});
	for (int chunkZ = 0; chunkZ < world.chunkCountZ; ++chunkZ) {
		for (int chunkY = 0; chunkY < world.chunkCountY; ++chunkY) {
			for (int chunkX = 0; chunkX < world.chunkCountX; ++chunkX) {
				const size_t chunkIndex = GetVoxelChunkIndex(world, {chunkX, chunkY, chunkZ});
				auto &[min, maxExclusive, rebuildQueued, nonAirVoxelCount] = world.chunks[chunkIndex];
				min = {
					world.min.x + chunkX * world.chunkSize,
					world.min.y + chunkY * world.chunkSize,
					world.min.z + chunkZ * world.chunkSize,
				};
				maxExclusive = {
					std::min(min.x + world.chunkSize, world.maxExclusive.x),
					std::min(min.y + world.chunkSize, world.maxExclusive.y),
					std::min(min.z + world.chunkSize, world.maxExclusive.z),
				};
				rebuildQueued = false;
				nonAirVoxelCount = 0;
			}
		}
	}


	constexpr int centerY = 8;
	constexpr int radius = 6;
	constexpr int innerR = 5;
	constexpr int fluidTop = centerY - innerR + 7;
	for (int dz = -radius; dz <= radius; ++dz) {
		for (int dy = -radius; dy <= radius; ++dy) {
			for (int dx = -radius; dx <= radius; ++dx) {
				const int d2 = dx * dx + dy * dy + dz * dz;
				constexpr int outer2 = radius * radius;
				constexpr int inner2 = innerR * innerR;
				if (d2 > outer2) {
					continue;
				}
				const Int3 pos{dx, centerY + dy, dz};
				if (d2 > inner2) {
					SetVoxelMaterial(world, pos, VoxelMaterial::Glass);
				} else if (pos.y <= fluidTop) {
					SetVoxelMaterial(world, pos, VoxelMaterial::Fluid);
				}
			}
		}
	}

	EXPECT_EQ(context, VoxelMaterial::Fluid, GetVoxelMaterial(world, {0, 3, 0}));
	EXPECT_EQ(context, VoxelMaterial::Glass, GetVoxelMaterial(world, {0, 2, 0}));

	UpdateFluidCA(world);
	EXPECT_EQ(context, VoxelMaterial::Glass, GetVoxelMaterial(world, {0, 2, 0}));

	SetVoxelMaterial(world, {0, 2, 0}, VoxelMaterial::Air);


	const uint32_t moved = UpdateFluidCA(world);
	EXPECT_TRUE(context, moved > 0u);
	EXPECT_EQ(context, VoxelMaterial::Fluid, GetVoxelMaterial(world, {0, 2, 0}));

	const uint32_t initialFluidCount = world.stats.fluidVoxelCount;
	for (int tick = 0; tick < 10; ++tick) {
		UpdateFluidCA(world);
		EXPECT_TRUE(context, world.stats.fluidVoxelCount >= initialFluidCount);
	}
}

void TickFluidCA(
	SimulationState &simulation,
	VoxelWorld &world,
	const float frameDeltaSeconds,
	const int frameCount)
{
	for (int frame = 0; frame < frameCount; ++frame) {

		const bool frameStepNow = simulation.frameStepRequested;
		simulation.frameStepRequested = false;
		const bool effectivePaused = simulation.paused && !frameStepNow;

		const float scaledDelta = frameDeltaSeconds * simulation.timeScale;
		if (!effectivePaused && simulation.fluidTickRateHz > 0.0f) {
			simulation.fluidAccumulatorSeconds += scaledDelta;
			const float fluidInterval = 1.0f / simulation.fluidTickRateHz;
			while (simulation.fluidAccumulatorSeconds >= fluidInterval) {
				simulation.fluidAccumulatorSeconds -= fluidInterval;
				UpdateFluidCA(world);
			}
		} else if (effectivePaused) {
			simulation.fluidAccumulatorSeconds = 0.0f;
		}
	}
}

void TestFluidCAFluidDoesNotMoveOnPause(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(4, 4, 4);
	SetVoxelMaterial(world, {2, 2, 2}, VoxelMaterial::Fluid);

	SimulationState simulation{};
	simulation.paused = true;
	simulation.timeScale = 1.0f;
	simulation.fluidTickRateHz = 20.0f;

	const std::vector<uint8_t> before = BuildFlatVoxelSnapshot(world);
	TickFluidCA(simulation, world, 1.0f / 60.0f, 60);
	const std::vector<uint8_t> after = BuildFlatVoxelSnapshot(world);

	EXPECT_TRUE(context, before == after);
	EXPECT_EQ(context, 1u, world.stats.fluidVoxelCount);
}


void TestFluidCAFluidMovesOnUnpause(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(4, 4, 4);
	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::FloorWhite);
	SetVoxelMaterial(world, {2, 2, 2}, VoxelMaterial::Fluid);

	SimulationState simulation{};
	simulation.paused = false;
	simulation.timeScale = 1.0f;
	simulation.fluidTickRateHz = 20.0f;

	TickFluidCA(simulation, world, 1.0f / 60.0f, 3);

	EXPECT_EQ(context, VoxelMaterial::Air, GetVoxelMaterial(world, {2, 2, 2}));
	EXPECT_EQ(context, VoxelMaterial::Fluid, GetVoxelMaterial(world, {2, 1, 2}));
	EXPECT_EQ(context, VoxelMaterial::FloorWhite, GetVoxelMaterial(world, {2, 0, 2}));
}


void TestFluidCAFluidRateRespectsTimeScale(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(4, 4, 4);
	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::Fluid);

	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::FloorWhite);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid);

	SimulationState simulation{};
	simulation.paused = false;
	simulation.timeScale = 0.5f;
	simulation.fluidTickRateHz = 20.0f;

	uint32_t totalMoves = 0u;
	for (int frame = 0; frame < 60; ++frame) {
		const uint32_t before = world.stats.fluidVoxelCount;
		TickFluidCA(simulation, world, 1.0f / 60.0f, 1);
		const uint32_t after = world.stats.fluidVoxelCount;
		(void)before;
		(void)after;

	}

	totalMoves = 0u;
	(void)totalMoves;
	EXPECT_EQ(context, 1u, world.stats.fluidVoxelCount);
}


void TestFluidCAFluidRateAboveBase(TestContext &context)
{

	VoxelWorld world = MakeFluidCATestWorld(4, 4, 4);
	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::FloorWhite);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid);
	for (int x = 0; x < 4; ++x) {
		for (int z = 0; z < 4; ++z) {
			SetVoxelMaterial(world, {x, 2, z}, VoxelMaterial::Glass);
		}
	}

	SimulationState simulation{};
	simulation.paused = false;
	simulation.timeScale = 2.0f;
	simulation.fluidTickRateHz = 20.0f;

	constexpr float kFrameDelta = 1.0f / 60.0f;
	constexpr int kFrameCount = 60;
	int tickCount = 0;
	for (int frame = 0; frame < kFrameCount; ++frame) {
		const float scaledDelta = kFrameDelta * simulation.timeScale;
		simulation.fluidAccumulatorSeconds += scaledDelta;
		const float fluidInterval = 1.0f / simulation.fluidTickRateHz;
		while (simulation.fluidAccumulatorSeconds >= fluidInterval) {
			simulation.fluidAccumulatorSeconds -= fluidInterval;
			UpdateFluidCA(world);
			++tickCount;
		}
	}

	EXPECT_TRUE(context, tickCount >= 38 && tickCount <= 42);
}


void TestFluidCAFluidRateAtDefault(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(4, 4, 4);
	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::FloorWhite);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid);
	for (int x = 0; x < 4; ++x) {
		for (int z = 0; z < 4; ++z) {
			SetVoxelMaterial(world, {x, 2, z}, VoxelMaterial::Glass);
		}
	}

	SimulationState simulation{};
	simulation.paused = false;
	simulation.timeScale = 1.0f;
	simulation.fluidTickRateHz = 20.0f;

	constexpr float kFrameDelta = 1.0f / 60.0f;
	constexpr int kFrameCount = 60;
	int tickCount = 0;
	for (int frame = 0; frame < kFrameCount; ++frame) {
		const float scaledDelta = kFrameDelta * simulation.timeScale;
		simulation.fluidAccumulatorSeconds += scaledDelta;
		const float fluidInterval = 1.0f / simulation.fluidTickRateHz;
		while (simulation.fluidAccumulatorSeconds >= fluidInterval) {
			simulation.fluidAccumulatorSeconds -= fluidInterval;
			UpdateFluidCA(world);
			++tickCount;
		}
	}
	EXPECT_TRUE(context, tickCount >= 18 && tickCount <= 22);
}


void TestFluidCAFluidTimeScaleZeroStops(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(4, 4, 4);
	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::FloorWhite);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid);
	for (int x = 0; x < 4; ++x) {
		for (int z = 0; z < 4; ++z) {
			SetVoxelMaterial(world, {x, 2, z}, VoxelMaterial::Glass);
		}
	}

	SimulationState simulation{};
	simulation.paused = false;
	simulation.timeScale = 0.0f;
	simulation.fluidTickRateHz = 20.0f;

	const std::vector<uint8_t> before = BuildFlatVoxelSnapshot(world);
	constexpr float kFrameDelta = 1.0f / 60.0f;
	for (int frame = 0; frame < 60; ++frame) {
		const float scaledDelta = kFrameDelta * simulation.timeScale;
		simulation.fluidAccumulatorSeconds += scaledDelta;

		EXPECT_EQ(context, 0.0f, simulation.fluidAccumulatorSeconds);
	}
	const std::vector<uint8_t> after = BuildFlatVoxelSnapshot(world);
	EXPECT_TRUE(context, before == after);
}


void TestFluidCAFluidFrameStepWithTimeScaleZero(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(4, 4, 4);
	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::FloorWhite);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid);
	for (int x = 0; x < 4; ++x) {
		for (int z = 0; z < 4; ++z) {
			SetVoxelMaterial(world, {x, 2, z}, VoxelMaterial::Glass);
		}
	}

	SimulationState simulation{};
	simulation.paused = true;
	simulation.timeScale = 0.0f;
	simulation.fluidTickRateHz = 20.0f;
	simulation.frameStepRequested = true;

	const std::vector<uint8_t> before = BuildFlatVoxelSnapshot(world);

	TickFluidCA(simulation, world, 1.0f / 60.0f, 1);
	const std::vector<uint8_t> after = BuildFlatVoxelSnapshot(world);

	EXPECT_TRUE(context, !simulation.frameStepRequested);
	EXPECT_TRUE(context, before == after);
	EXPECT_EQ(context, 1u, world.stats.fluidVoxelCount);
}


void TestFluidCAFluidRateConfigurable(TestContext &context)
{
	auto runRate = [](const float hz) {
		VoxelWorld world = MakeFluidCATestWorld(4, 4, 4);
		SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::FloorWhite);
		SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid);
		for (int x = 0; x < 4; ++x) {
			for (int z = 0; z < 4; ++z) {
				SetVoxelMaterial(world, {x, 2, z}, VoxelMaterial::Glass);
			}
		}
		SimulationState simulation{};
		simulation.paused = false;
		simulation.timeScale = 1.0f;
		simulation.fluidTickRateHz = hz;

		constexpr float kFrameDelta = 1.0f / 60.0f;
		int tickCount = 0;
		for (int frame = 0; frame < 60; ++frame) {
			const float scaledDelta = kFrameDelta * simulation.timeScale;
			simulation.fluidAccumulatorSeconds += scaledDelta;
			const float fluidInterval = 1.0f / simulation.fluidTickRateHz;
			while (simulation.fluidAccumulatorSeconds >= fluidInterval) {
				simulation.fluidAccumulatorSeconds -= fluidInterval;
				UpdateFluidCA(world);
				++tickCount;
			}
		}
		return tickCount;
	};

	const int at5Hz = runRate(5.0f);
	const int at60Hz = runRate(60.0f);
	EXPECT_TRUE(context, at5Hz >= 4 && at5Hz <= 6);
	EXPECT_TRUE(context, at60Hz >= 58 && at60Hz <= 62);
}

int main()
{
	TestContext context{};

	TestFluidCASingleCellFallsOneCellPerTick(context);
	TestFluidCAColumnPercolatesAndSpreadsOnFloor(context);
	TestFluidCARestingOnFloorStaysPut(context);
	TestFluidCAFluidOnGlassStaysPutThenFallsWhenGlassBreaks(context);
	TestFluidCAFluidAtY0IsStable(context);
	TestFluidCAFluidDoesNotFallThroughPlatform(context);
	TestFluidCASpreadsToCardinalNeighbour(context);
	TestFluidCASpreadIsDeterministic(context);
	TestFluidCAEmptyWorldShortCircuits(context);
	TestFluidCADeterministicAcrossRuns(context);
	TestFluidCAStatsCountStaysConsistent(context);
	TestFluidCALongColumnAtWorldFloorSpreadsOut(context);
	TestFluidCAColumnDrainsViaSpreadPlatformStaysIntact(context);
	TestFluidCAFreshWorldHasNoFluidAndStaysEmpty(context);
	TestFluidCAVoxelLabSphereFallOnGlassBreak(context);

	TestFluidCAFluidDoesNotMoveOnPause(context);
	TestFluidCAFluidMovesOnUnpause(context);
	TestFluidCAFluidRateRespectsTimeScale(context);
	TestFluidCAFluidRateAboveBase(context);
	TestFluidCAFluidRateAtDefault(context);
	TestFluidCAFluidTimeScaleZeroStops(context);
	TestFluidCAFluidFrameStepWithTimeScaleZero(context);
	TestFluidCAFluidRateConfigurable(context);

	if (context.failures != 0) {
		return EXIT_FAILURE;
	}

	std::puts("ProjectVFluidCATests passed");
	return EXIT_SUCCESS;
}

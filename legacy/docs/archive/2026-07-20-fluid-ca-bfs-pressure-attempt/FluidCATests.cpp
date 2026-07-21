#include "voxel/VoxelWorld.hpp"

#include "core/Types.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "projectv_test_utils.hpp"

namespace {

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
	for (int z = 0; z < depth; ++z) {
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::Air, nullptr);
			}
		}
	}
	world.chunks.assign(
		static_cast<size_t>(world.chunkCountX) * static_cast<size_t>(world.chunkCountY) *
			static_cast<size_t>(world.chunkCountZ),
		VoxelChunk{});
	for (int chunkZ = 0; chunkZ < world.chunkCountZ; ++chunkZ) {
		for (int chunkY = 0; chunkY < world.chunkCountY; ++chunkY) {
			for (int chunkX = 0; chunkX < world.chunkCountX; ++chunkX) {
				const size_t chunkIndex = GetVoxelChunkIndex(world, {chunkX, chunkY, chunkZ});
				auto &[min, maxExclusive, rebuildQueued, isStatic, nonAirVoxelCount, ticksSinceLastEdit, lodLevel,
					   reserved0, reserved1, reserved2] = world.chunks[chunkIndex];
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
	const auto snapshot = BuildFlatVoxelSnapshot(world);
	return static_cast<size_t>(
		std::ranges::count(snapshot, static_cast<uint8_t>(VoxelMaterial::Fluid)));
}

int MaxFluidColumnHeight(const VoxelWorld &world)
{
	int maxH = 0;
	for (int z = world.min.z; z < world.maxExclusive.z; ++z) {
		for (int x = world.min.x; x < world.maxExclusive.x; ++x) {
			int h = 0;
			for (int y = world.min.y; y < world.maxExclusive.y; ++y) {
				if (GetVoxelMaterial(world, {x, y, z}) == VoxelMaterial::Fluid) {
					++h;
					maxH = std::max(maxH, h);
				} else {
					h = 0;
				}
			}
		}
	}
	return maxH;
}

int ColumnHeightAt(const VoxelWorld &world, const int x, const int z)
{
	int h = 0;
	for (int y = world.min.y; y < world.maxExclusive.y; ++y) {
		if (GetVoxelMaterial(world, {x, y, z}) == VoxelMaterial::Fluid) {
			++h;
		} else if (h > 0) {
			break;
		}
	}
	return h;
}

void FillFloor(VoxelWorld &world, const int y, const VoxelMaterial material)
{
	for (int z = world.min.z; z < world.maxExclusive.z; ++z) {
		for (int x = world.min.x; x < world.maxExclusive.x; ++x) {
			SetVoxelMaterial(world, {x, y, z}, material, nullptr);
		}
	}
}

std::string SnapshotMaterials(const VoxelWorld &world)
{
	const auto snap = BuildFlatVoxelSnapshot(world);
	return std::string(snap.begin(), snap.end());
}

} // namespace

void TestFluidCASingleCellFallsOneCellPerTick(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(8, 8, 8);
	SetVoxelMaterial(world, {4, 5, 4}, VoxelMaterial::Fluid, nullptr);

	const uint32_t moved = UpdateFluidCA(world);
	EXPECT_EQ(context, 1u, moved);
	EXPECT_EQ(context, VoxelMaterial::Air, GetVoxelMaterial(world, {4, 5, 4}));
	EXPECT_EQ(context, VoxelMaterial::Fluid, GetVoxelMaterial(world, {4, 4, 4}));
	EXPECT_EQ(context, size_t{1}, CountFluid(world));
}

void TestFluidCAMassPreservedEachTick(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(8, 12, 8);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	SetVoxelMaterial(world, {3, 4, 3}, VoxelMaterial::Fluid, nullptr);
	SetVoxelMaterial(world, {3, 5, 3}, VoxelMaterial::Fluid, nullptr);
	SetVoxelMaterial(world, {3, 6, 3}, VoxelMaterial::Fluid, nullptr);
	SetVoxelMaterial(world, {4, 4, 3}, VoxelMaterial::Fluid, nullptr);
	const size_t mass = CountFluid(world);
	for (int tick = 0; tick < 20; ++tick) {
		UpdateFluidCA(world);
		EXPECT_EQ(context, mass, CountFluid(world));
	}
}

void TestFluidCAHeight1StaysOnFlatFloor(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(8, 4, 8);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid, nullptr);
	for (int tick = 0; tick < 8; ++tick) {
		EXPECT_EQ(context, 0u, UpdateFluidCA(world));
		EXPECT_EQ(context, VoxelMaterial::Fluid, GetVoxelMaterial(world, {2, 1, 2}));
	}
	EXPECT_EQ(context, size_t{1}, CountFluid(world));
}

void TestFluidCAHeight1WalksIntoPitOnly(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(8, 5, 8);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::FloorGray, nullptr);
	SetVoxelMaterial(world, {2, 2, 2}, VoxelMaterial::Fluid, nullptr);
	bool leftPlatform = false;
	for (int tick = 0; tick < 8; ++tick) {
		UpdateFluidCA(world);
		EXPECT_EQ(context, size_t{1}, CountFluid(world));
		if (GetVoxelMaterial(world, {2, 2, 2}) == VoxelMaterial::Air) {
			leftPlatform = true;
			break;
		}
	}
	EXPECT_TRUE(context, leftPlatform);
	EXPECT_EQ(context, VoxelMaterial::Air, GetVoxelMaterial(world, {3, 2, 2})); // did not same-level wander onto empty
}

void TestFluidCAColumnBaseDrainsIntoSideHole(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(8, 6, 8);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid, nullptr);
	SetVoxelMaterial(world, {2, 2, 2}, VoxelMaterial::Fluid, nullptr);
	SetVoxelMaterial(world, {2, 3, 2}, VoxelMaterial::Fluid, nullptr);
	const size_t mass = CountFluid(world);
	EXPECT_EQ(context, 3, ColumnHeightAt(world, 2, 2));

	for (int tick = 0; tick < 16; ++tick) {
		UpdateFluidCA(world);
		EXPECT_EQ(context, mass, CountFluid(world));
		if (ColumnHeightAt(world, 2, 2) < 3) {
			break;
		}
	}
	EXPECT_TRUE(context, ColumnHeightAt(world, 2, 2) < 3);
}

void TestFluidCANoClimbOntoNeighborTop(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(6, 4, 6);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid, nullptr);
	SetVoxelMaterial(world, {3, 1, 2}, VoxelMaterial::Fluid, nullptr); // neighbor puddle; (3,2) must stay Air
	UpdateFluidCA(world);
	EXPECT_EQ(context, VoxelMaterial::Air, GetVoxelMaterial(world, {3, 2, 2}));
	EXPECT_EQ(context, size_t{2}, CountFluid(world));
}

void TestFluidCACenterTowerDisplacesEdgeOnOpenFloor(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(12, 4, 12);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	for (int z = 4; z <= 6; ++z) {
		for (int x = 4; x <= 6; ++x) {
			SetVoxelMaterial(world, {x, 1, z}, VoxelMaterial::Fluid, nullptr);
		}
	}
	SetVoxelMaterial(world, {5, 2, 5}, VoxelMaterial::Fluid, nullptr); // center tower
	const size_t mass = CountFluid(world);
	EXPECT_EQ(context, size_t{10}, mass);

	for (int tick = 0; tick < 64; ++tick) {
		UpdateFluidCA(world);
		EXPECT_EQ(context, mass, CountFluid(world));
		if (MaxFluidColumnHeight(world) == 1) {
			break;
		}
	}
	EXPECT_EQ(context, 1, MaxFluidColumnHeight(world));
	EXPECT_EQ(context, size_t{10}, CountFluid(world));
}

void TestFluidCAInteriorTowerOn5x5Flattens(TestContext &context)
{
	// Chebyshev edge distance ≥ 2: old displace-only CA deadlocked here (ascii stall).
	VoxelWorld world = MakeFluidCATestWorld(14, 4, 14);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	for (int z = 4; z <= 8; ++z) {
		for (int x = 4; x <= 8; ++x) {
			SetVoxelMaterial(world, {x, 1, z}, VoxelMaterial::Fluid, nullptr);
		}
	}
	SetVoxelMaterial(world, {6, 2, 6}, VoxelMaterial::Fluid, nullptr);
	const size_t mass = CountFluid(world);
	EXPECT_EQ(context, size_t{26}, mass);

	bool flat = false;
	for (int tick = 0; tick < 64; ++tick) {
		UpdateFluidCA(world);
		EXPECT_EQ(context, mass, CountFluid(world));
		if (MaxFluidColumnHeight(world) == 1) {
			flat = true;
			break;
		}
	}
	EXPECT_TRUE(context, flat);
	EXPECT_EQ(context, size_t{26}, CountFluid(world));
}

void TestFluidCAGapInPuddleFills(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(10, 4, 10);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	for (int z = 3; z <= 5; ++z) {
		for (int x = 3; x <= 5; ++x) {
			SetVoxelMaterial(world, {x, 1, z}, VoxelMaterial::Fluid, nullptr);
		}
	}
	SetVoxelMaterial(world, {4, 1, 4}, VoxelMaterial::Air, nullptr);   // hole in center
	SetVoxelMaterial(world, {2, 1, 4}, VoxelMaterial::Fluid, nullptr); // spare mass outside to move in
	const size_t mass = CountFluid(world);

	bool filled = false;
	for (int tick = 0; tick < 16; ++tick) {
		UpdateFluidCA(world);
		EXPECT_EQ(context, mass, CountFluid(world));
		if (GetVoxelMaterial(world, {4, 1, 4}) == VoxelMaterial::Fluid) {
			filled = true;
			break;
		}
	}
	EXPECT_TRUE(context, filled);
}

void TestFluidCAOpenFloorSpreadIsBalanced(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(16, 4, 16);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	for (int z = 6; z <= 8; ++z) {
		for (int x = 6; x <= 8; ++x) {
			SetVoxelMaterial(world, {x, 1, z}, VoxelMaterial::Fluid, nullptr);
		}
	}
	SetVoxelMaterial(world, {7, 2, 7}, VoxelMaterial::Fluid, nullptr); // center tower
	const size_t mass = CountFluid(world);

	for (int tick = 0; tick < 64; ++tick) {
		UpdateFluidCA(world);
		EXPECT_EQ(context, mass, CountFluid(world));
		if (MaxFluidColumnHeight(world) == 1) {
			break;
		}
	}
	EXPECT_EQ(context, 1, MaxFluidColumnHeight(world));

	int minX = 99;
	int maxX = -99;
	int minZ = 99;
	int maxZ = -99;
	for (int z = world.min.z; z < world.maxExclusive.z; ++z) {
		for (int x = world.min.x; x < world.maxExclusive.x; ++x) {
			if (GetVoxelMaterial(world, {x, 1, z}) != VoxelMaterial::Fluid) {
				continue;
			}
			minX = std::min(minX, x);
			maxX = std::max(maxX, x);
			minZ = std::min(minZ, z);
			maxZ = std::max(maxZ, z);
		}
	}
	const int spanX = maxX - minX + 1;
	const int spanZ = maxZ - minZ + 1;
	EXPECT_TRUE(context, (spanX > spanZ ? spanX - spanZ : spanZ - spanX) <= 2);
	EXPECT_TRUE(context, spanX <= 6 && spanZ <= 6);
}

void TestFluidCAElevatedSidewalkWhenFloorBlocked(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(8, 5, 8);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	// 3×3 puddle at y=1 centered at (3..5, 3..5); solid walls seal floor-adjacent Air outside
	for (int z = 3; z <= 5; ++z) {
		for (int x = 3; x <= 5; ++x) {
			SetVoxelMaterial(world, {x, 1, z}, VoxelMaterial::Fluid, nullptr);
		}
	}
	SetVoxelMaterial(world, {4, 2, 4}, VoxelMaterial::Fluid, nullptr);
	for (int z = 2; z <= 6; ++z) {
		for (int x = 2; x <= 6; ++x) {
			if (x >= 3 && x <= 5 && z >= 3 && z <= 5) {
				continue;
			}
			SetVoxelMaterial(world, {x, 1, z}, VoxelMaterial::Glass, nullptr); // wall ring on floor level
		}
	}
	const size_t mass = CountFluid(world);
	EXPECT_EQ(context, size_t{10}, mass);

	for (int tick = 0; tick < 16; ++tick) {
		UpdateFluidCA(world);
		EXPECT_EQ(context, mass, CountFluid(world));
	}
	// Sealed pocket: lone elevated may stay (no coalesce target); must not escape glass / lose mass
	EXPECT_TRUE(context, MaxFluidColumnHeight(world) >= 2);
	EXPECT_EQ(context, size_t{10}, CountFluid(world));
	for (int z = 2; z <= 6; ++z) {
		for (int x = 2; x <= 6; ++x) {
			if (x >= 3 && x <= 5 && z >= 3 && z <= 5) {
				continue;
			}
			EXPECT_EQ(context, VoxelMaterial::Air, GetVoxelMaterial(world, {x, 2, z}));	  // no skate over walls
			EXPECT_EQ(context, VoxelMaterial::Glass, GetVoxelMaterial(world, {x, 1, z})); // walls intact
		}
	}
}

void TestFluidCACraterDepth2DrainsToFloor(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(8, 6, 8);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	// Raised rim at y=2 with pit down to y=1 / floor y=0 solid; rim Fluid steps into unsupported Air
	for (int z = 2; z <= 5; ++z) {
		for (int x = 2; x <= 5; ++x) {
			SetVoxelMaterial(world, {x, 1, z}, VoxelMaterial::FloorGray, nullptr);
		}
	}
	SetVoxelMaterial(world, {3, 1, 3}, VoxelMaterial::Air, nullptr);
	SetVoxelMaterial(world, {4, 1, 3}, VoxelMaterial::Air, nullptr);
	SetVoxelMaterial(world, {3, 1, 4}, VoxelMaterial::Air, nullptr);
	SetVoxelMaterial(world, {4, 1, 4}, VoxelMaterial::Air, nullptr);
	SetVoxelMaterial(world, {2, 2, 3}, VoxelMaterial::Fluid, nullptr);
	const size_t mass = CountFluid(world);

	bool onBottom = false;
	for (int tick = 0; tick < 32; ++tick) {
		UpdateFluidCA(world);
		EXPECT_EQ(context, mass, CountFluid(world));
		if (GetVoxelMaterial(world, {3, 1, 3}) == VoxelMaterial::Fluid ||
			GetVoxelMaterial(world, {4, 1, 3}) == VoxelMaterial::Fluid ||
			GetVoxelMaterial(world, {3, 1, 4}) == VoxelMaterial::Fluid ||
			GetVoxelMaterial(world, {4, 1, 4}) == VoxelMaterial::Fluid ||
			GetVoxelMaterial(world, {3, 0, 3}) == VoxelMaterial::Fluid) {
			// y=0 is FloorWhite — fluid rests at y=1 in pit
			onBottom = GetVoxelMaterial(world, {3, 1, 3}) == VoxelMaterial::Fluid ||
					   GetVoxelMaterial(world, {4, 1, 3}) == VoxelMaterial::Fluid ||
					   GetVoxelMaterial(world, {3, 1, 4}) == VoxelMaterial::Fluid ||
					   GetVoxelMaterial(world, {4, 1, 4}) == VoxelMaterial::Fluid;
			if (onBottom) {
				break;
			}
		}
	}
	EXPECT_TRUE(context, onBottom);
}

void TestFluidCAUnsupportedStepRestYMinus1(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(6, 6, 6);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::FloorGray, nullptr);
	SetVoxelMaterial(world, {2, 2, 2}, VoxelMaterial::Fluid, nullptr); // restY=2; Air at (3,1) unsupported then floor
	const size_t mass = CountFluid(world);

	bool atFloor = false;
	for (int tick = 0; tick < 16; ++tick) {
		UpdateFluidCA(world);
		EXPECT_EQ(context, mass, CountFluid(world));
		if (GetVoxelMaterial(world, {3, 1, 2}) == VoxelMaterial::Fluid ||
			GetVoxelMaterial(world, {2, 1, 2}) == VoxelMaterial::Fluid) {
			// may land on platform or step off
		}
		for (int z = 0; z < 6; ++z) {
			for (int x = 0; x < 6; ++x) {
				if (GetVoxelMaterial(world, {x, 1, z}) == VoxelMaterial::Fluid &&
					GetVoxelMaterial(world, {x, 0, z}) == VoxelMaterial::FloorWhite) {
					atFloor = true;
				}
			}
		}
		if (atFloor) {
			break;
		}
	}
	EXPECT_TRUE(context, atFloor);
}

void TestFluidCADeterministicReplay(TestContext &context)
{
	auto run = []() -> std::string {
		VoxelWorld world = MakeFluidCATestWorld(10, 8, 10);
		FillFloor(world, 0, VoxelMaterial::FloorWhite);
		for (int z = 3; z <= 5; ++z) {
			for (int x = 3; x <= 5; ++x) {
				SetVoxelMaterial(world, {x, 1, z}, VoxelMaterial::Fluid, nullptr);
			}
		}
		SetVoxelMaterial(world, {4, 2, 4}, VoxelMaterial::Fluid, nullptr);
		SetVoxelMaterial(world, {4, 5, 4}, VoxelMaterial::Fluid, nullptr);
		for (int tick = 0; tick < 40; ++tick) {
			UpdateFluidCA(world);
		}
		return SnapshotMaterials(world);
	};
	EXPECT_TRUE(context, run() == run());
}

void TestFluidCAClosedPuddleSettles(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(8, 4, 8);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	for (int z = 2; z <= 4; ++z) {
		for (int x = 2; x <= 4; ++x) {
			SetVoxelMaterial(world, {x, 1, z}, VoxelMaterial::Fluid, nullptr);
		}
	}
	for (int z = 1; z <= 5; ++z) {
		for (int x = 1; x <= 5; ++x) {
			if (x >= 2 && x <= 4 && z >= 2 && z <= 4) {
				continue;
			}
			SetVoxelMaterial(world, {x, 1, z}, VoxelMaterial::Glass, nullptr);
		}
	}
	for (int tick = 0; tick < 8; ++tick) {
		UpdateFluidCA(world);
	}
	EXPECT_EQ(context, 0u, UpdateFluidCA(world));
	EXPECT_EQ(context, size_t{9}, CountFluid(world));
}

void TestFluidCAWorldFloorY0StaysPut(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(6, 4, 6);
	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::Fluid, nullptr);
	for (int tick = 0; tick < 4; ++tick) {
		EXPECT_EQ(context, 0u, UpdateFluidCA(world));
		EXPECT_EQ(context, VoxelMaterial::Fluid, GetVoxelMaterial(world, {2, 0, 2}));
	}
	EXPECT_EQ(context, size_t{1}, CountFluid(world));
}

void TestFluidCADoesNotEnterOrFallThroughGlass(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(8, 6, 8);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	// Glass platform + glass walls around fluid; only open path is none
	SetVoxelMaterial(world, {3, 1, 3}, VoxelMaterial::Glass, nullptr);
	SetVoxelMaterial(world, {3, 2, 3}, VoxelMaterial::Fluid, nullptr);
	for (int z = 2; z <= 4; ++z) {
		for (int x = 2; x <= 4; ++x) {
			if (x == 3 && z == 3) {
				continue;
			}
			SetVoxelMaterial(world, {x, 2, z}, VoxelMaterial::Glass, nullptr);
			SetVoxelMaterial(world, {x, 1, z}, VoxelMaterial::Glass, nullptr);
		}
	}
	const size_t mass = CountFluid(world);
	for (int tick = 0; tick < 16; ++tick) {
		UpdateFluidCA(world);
		EXPECT_EQ(context, mass, CountFluid(world));
		EXPECT_EQ(context, VoxelMaterial::Glass, GetVoxelMaterial(world, {3, 1, 3}));
		EXPECT_EQ(context, VoxelMaterial::Fluid, GetVoxelMaterial(world, {3, 2, 3}));
		for (int z = 2; z <= 4; ++z) {
			for (int x = 2; x <= 4; ++x) {
				if (x == 3 && z == 3) {
					continue;
				}
				EXPECT_EQ(context, VoxelMaterial::Glass, GetVoxelMaterial(world, {x, 2, z}));
			}
		}
	}
}

void TestFluidCANoDiagonalCornerCutThroughGlass(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(8, 4, 8);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid, nullptr);
	SetVoxelMaterial(world, {3, 1, 2}, VoxelMaterial::Glass, nullptr);
	SetVoxelMaterial(world, {2, 1, 3}, VoxelMaterial::Glass, nullptr);
	SetVoxelMaterial(world, {2, 1, 1}, VoxelMaterial::Glass, nullptr);
	SetVoxelMaterial(world, {1, 1, 2}, VoxelMaterial::Glass, nullptr);
	for (int tick = 0; tick < 8; ++tick) {
		UpdateFluidCA(world);
		EXPECT_EQ(context, VoxelMaterial::Air, GetVoxelMaterial(world, {3, 1, 3}));
		EXPECT_EQ(context, VoxelMaterial::Glass, GetVoxelMaterial(world, {3, 1, 2}));
		EXPECT_EQ(context, VoxelMaterial::Glass, GetVoxelMaterial(world, {2, 1, 3}));
	}
	EXPECT_EQ(context, size_t{1}, CountFluid(world));
	EXPECT_EQ(context, VoxelMaterial::Fluid, GetVoxelMaterial(world, {2, 1, 2}));
}

void TestFluidCADoesNotStepUnderGlassRim(TestContext &context)
{
	// Ascii leak: Fluid at (2,2) steps M8 to (3,1) under Glass at (3,2) → outside bowl.
	VoxelWorld world = MakeFluidCATestWorld(8, 5, 8);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::FloorGray, nullptr); // support under fluid
	SetVoxelMaterial(world, {3, 2, 2}, VoxelMaterial::Glass, nullptr);	   // rim
	SetVoxelMaterial(world, {2, 2, 2}, VoxelMaterial::Fluid, nullptr);	   // inside
	// Seal cardinal same-level exits
	SetVoxelMaterial(world, {2, 2, 1}, VoxelMaterial::Glass, nullptr);
	SetVoxelMaterial(world, {2, 2, 3}, VoxelMaterial::Glass, nullptr);
	SetVoxelMaterial(world, {1, 2, 2}, VoxelMaterial::Glass, nullptr);
	const size_t mass = CountFluid(world);
	for (int tick = 0; tick < 8; ++tick) {
		UpdateFluidCA(world);
		EXPECT_EQ(context, mass, CountFluid(world));
		EXPECT_EQ(context, VoxelMaterial::Air, GetVoxelMaterial(world, {3, 1, 2}));
		EXPECT_EQ(context, VoxelMaterial::Glass, GetVoxelMaterial(world, {3, 2, 2}));
	}
	EXPECT_EQ(context, VoxelMaterial::Fluid, GetVoxelMaterial(world, {2, 2, 2}));
}

int main()
{
	TestContext context;
	TestFluidCASingleCellFallsOneCellPerTick(context);
	TestFluidCAMassPreservedEachTick(context);
	TestFluidCAHeight1StaysOnFlatFloor(context);
	TestFluidCAHeight1WalksIntoPitOnly(context);
	TestFluidCAColumnBaseDrainsIntoSideHole(context);
	TestFluidCANoClimbOntoNeighborTop(context);
	TestFluidCACenterTowerDisplacesEdgeOnOpenFloor(context);
	TestFluidCAInteriorTowerOn5x5Flattens(context);
	TestFluidCAGapInPuddleFills(context);
	TestFluidCAOpenFloorSpreadIsBalanced(context);
	TestFluidCAElevatedSidewalkWhenFloorBlocked(context);
	TestFluidCACraterDepth2DrainsToFloor(context);
	TestFluidCAUnsupportedStepRestYMinus1(context);
	TestFluidCADeterministicReplay(context);
	TestFluidCAClosedPuddleSettles(context);
	TestFluidCAWorldFloorY0StaysPut(context);
	TestFluidCADoesNotEnterOrFallThroughGlass(context);
	TestFluidCANoDiagonalCornerCutThroughGlass(context);
	TestFluidCADoesNotStepUnderGlassRim(context);

	if (context.failures != 0) {
		std::fprintf(stderr, "ProjectVFluidCATests failed (%d)\n", context.failures);
		return 1;
	}
	std::puts("ProjectVFluidCATests passed");
	return 0;
}

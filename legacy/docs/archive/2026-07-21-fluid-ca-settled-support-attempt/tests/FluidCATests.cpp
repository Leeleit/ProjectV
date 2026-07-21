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

std::vector<Int3> CollectFluidCells(const VoxelWorld &world)
{
	std::vector<Int3> cells;
	for (int z = world.min.z; z < world.maxExclusive.z; ++z) {
		for (int y = world.min.y; y < world.maxExclusive.y; ++y) {
			for (int x = world.min.x; x < world.maxExclusive.x; ++x) {
				if (GetVoxelMaterial(world, {x, y, z}) == VoxelMaterial::Fluid) {
					cells.push_back({x, y, z});
				}
			}
		}
	}
	return cells;
}

bool ContainsCell(const std::vector<Int3> &cells, const Int3 cell)
{
	for (const Int3 c : cells) {
		if (c.x == cell.x && c.y == cell.y && c.z == cell.z) {
			return true;
		}
	}
	return false;
}

bool HasSixNeighbourInSet(const std::vector<Int3> &cells, const Int3 cell)
{
	constexpr int dirs[6][3] = {{0, -1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, 1, 0}};
	for (const auto &d : dirs) {
		if (ContainsCell(cells, {cell.x + d[0], cell.y + d[1], cell.z + d[2]})) {
			return true;
		}
	}
	return false;
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

void TestFluidCAHeight1StepsDownOffPlatform(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(8, 5, 8);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::FloorGray, nullptr);
	SetVoxelMaterial(world, {2, 2, 2}, VoxelMaterial::Fluid, nullptr);
	bool reachedFloor = false;
	for (int tick = 0; tick < 16; ++tick) {
		UpdateFluidCA(world);
		EXPECT_EQ(context, size_t{1}, CountFluid(world));
		for (int z = 0; z < 8 && !reachedFloor; ++z) {
			for (int x = 0; x < 8 && !reachedFloor; ++x) {
				reachedFloor = GetVoxelMaterial(world, {x, 1, z}) == VoxelMaterial::Fluid; // y=1 rests on floor y=0
			}
		}
		if (reachedFloor) {
			break;
		}
	}
	EXPECT_TRUE(context, reachedFloor);
	EXPECT_EQ(context, 0u, UpdateFluidCA(world)); // settled on the floor, no wander
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
		// contract: mass reaches the bottom (rests at y=1 over floor y=0) — pit or outside the rim
		for (int z = 0; z < 8 && !onBottom; ++z) {
			for (int x = 0; x < 8 && !onBottom; ++x) {
				onBottom = GetVoxelMaterial(world, {x, 1, z}) == VoxelMaterial::Fluid &&
						   GetVoxelMaterial(world, {x, 0, z}) == VoxelMaterial::FloorWhite;
			}
		}
		if (onBottom) {
			break;
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

void TestFluidCAStrictLayeredDrain(TestContext &context) // N1: per-column — only the top of each column may leave (bottom-first + settle)
{
	VoxelWorld world = MakeFluidCATestWorld(12, 8, 12);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	for (int y = 1; y <= 3; ++y) {
		for (int z = 4; z <= 6; ++z) {
			for (int x = 4; x <= 6; ++x) {
				SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::Fluid, nullptr);
			}
		}
	}
	const size_t mass = CountFluid(world);
	for (int tick = 0; tick < 200; ++tick) {
		const std::vector<Int3> before = CollectFluidCells(world);
		const uint32_t moved = UpdateFluidCA(world);
		EXPECT_EQ(context, mass, CountFluid(world));
		if (moved == 0u) {
			break;
		}
		const std::vector<Int3> after = CollectFluidCells(world);
		// Per (x,z): fluid may only be removed from the top of the column (settle/drain); lower cells stay put or receive settle
		for (int z = 0; z < 12; ++z) {
			for (int x = 0; x < 12; ++x) {
				std::vector<int> ysBefore;
				std::vector<int> ysAfter;
				for (const Int3 c : before) {
					if (c.x == x && c.z == z) {
						ysBefore.push_back(c.y);
					}
				}
				for (const Int3 c : after) {
					if (c.x == x && c.z == z) {
						ysAfter.push_back(c.y);
					}
				}
				std::sort(ysBefore.begin(), ysBefore.end());
				std::sort(ysAfter.begin(), ysAfter.end());
				if (ysBefore.empty()) {
					continue;
				}
				// Remaining cells in the column must be a bottom prefix of the previous set (top may shrink)
				EXPECT_TRUE(context, ysAfter.size() <= ysBefore.size());
				for (size_t i = 0; i < ysAfter.size(); ++i) {
					EXPECT_EQ(context, ysBefore[i], ysAfter[i]);
				}
			}
		}
	}
	EXPECT_EQ(context, 1, MaxFluidColumnHeight(world)); // 27 cells over an open floor flatten to one layer
}

void TestFluidCAContinuityNoTeleport(TestContext &context) // N2: growth is 6-adjacent to the body, ≤ K cells per tick
{
	VoxelWorld world = MakeFluidCATestWorld(12, 8, 12);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	for (int y = 1; y <= 3; ++y) {
		for (int z = 4; z <= 6; ++z) {
			for (int x = 4; x <= 6; ++x) {
				SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::Fluid, nullptr);
			}
		}
	}
	for (int tick = 0; tick < 200; ++tick) {
		const std::vector<Int3> before = CollectFluidCells(world);
		int exits = 0; // active outflow points before the tick (unsupported Air adjacent to the body)
		{
			std::vector<Int3> seenExits;
			for (const Int3 c : before) {
				constexpr int dirs[5][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}};
				for (const auto &d : dirs) {
					const Int3 n{c.x + d[0], c.y + d[1], c.z + d[2]};
					if (n.y <= 0 || GetVoxelMaterial(world, n) != VoxelMaterial::Air ||
						GetVoxelMaterial(world, {n.x, n.y - 1, n.z}) != VoxelMaterial::Air || ContainsCell(seenExits, n)) {
						continue;
					}
					seenExits.push_back(n);
					++exits;
				}
			}
		}
		const uint32_t moved = UpdateFluidCA(world);
		if (moved == 0u) {
			break;
		}
		const std::vector<Int3> after = CollectFluidCells(world);
		int added = 0;
		for (const Int3 c : after) {
			if (!ContainsCell(before, c)) {
				++added;
				EXPECT_TRUE(context, HasSixNeighbourInSet(before, c)); // no teleport: grown from the body surface
			}
		}
		EXPECT_TRUE(context, added <= 8 + exits); // K spread-chains + one emission per outflow point
	}
}

void TestFluidCAStreamContinuity(TestContext &context) // N3: the drain stream never splits into separated drops
{
	VoxelWorld world = MakeFluidCATestWorld(8, 10, 8);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	for (int y = 2; y <= 6; ++y) { // glass tube ring around column (4,4)
		for (int z = 3; z <= 5; ++z) {
			for (int x = 3; x <= 5; ++x) {
				if (x == 4 && z == 4) {
					continue;
				}
				SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::Glass, nullptr);
			}
		}
	}
	for (int y = 3; y <= 6; ++y) {
		SetVoxelMaterial(world, {4, y, 4}, VoxelMaterial::Fluid, nullptr);
	}
	const size_t mass = CountFluid(world);
	bool drained = false;
	for (int tick = 0; tick < 64; ++tick) {
		const uint32_t moved = UpdateFluidCA(world);
		EXPECT_EQ(context, mass, CountFluid(world));
		int lowestFluid = 99; // no Air gap between the lowest and highest Fluid in column (4,4)
		int highestFluid = -1;
		for (int y = 1; y <= 7; ++y) {
			if (GetVoxelMaterial(world, {4, y, 4}) == VoxelMaterial::Fluid) {
				lowestFluid = std::min(lowestFluid, y);
				highestFluid = std::max(highestFluid, y);
			}
		}
		for (int y = lowestFluid; y <= highestFluid && lowestFluid <= highestFluid; ++y) {
			EXPECT_TRUE(context, GetVoxelMaterial(world, {4, y, 4}) == VoxelMaterial::Fluid);
		}
		if (moved == 0u) {
			drained = true;
			break;
		}
	}
	EXPECT_TRUE(context, drained);
	for (int y = 3; y <= 6; ++y) { // tube empty above the mouth
		EXPECT_EQ(context, VoxelMaterial::Air, GetVoxelMaterial(world, {4, y, 4}));
	}
}

void TestFluidCASphereBreakEndToEnd(TestContext &context) // N4: side-broken sphere drains to the hole rim, settles, no hover
{
	VoxelWorld world = MakeFluidCATestWorld(16, 16, 16);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	const Int3 center{8, 10, 8};
	constexpr int radius = 4;
	for (int z = 0; z < 16; ++z) {
		for (int y = 0; y < 16; ++y) {
			for (int x = 0; x < 16; ++x) {
				const int dx = x - center.x;
				const int dy = y - center.y;
				const int dz = z - center.z;
				const int dist2 = dx * dx + dy * dy + dz * dz;
				if (dist2 >= radius * radius && dist2 <= radius * radius + radius) { // shell band
					SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::Glass, nullptr);
				} else if (dist2 < radius * radius) {
					SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::Fluid, nullptr);
				}
			}
		}
	}
	const int holeBottomY = center.y; // side hole at the +x equator
	for (int y = holeBottomY; y <= holeBottomY + 1; ++y) {
		SetVoxelMaterial(world, {center.x + radius, y, center.z}, VoxelMaterial::Air, nullptr);
		SetVoxelMaterial(world, {center.x + radius, y, center.z + 1}, VoxelMaterial::Air, nullptr);
	}
	const size_t mass = CountFluid(world);
	bool settled = false;
	for (int tick = 0; tick < 600; ++tick) {
		const uint32_t moved = UpdateFluidCA(world);
		EXPECT_EQ(context, mass, CountFluid(world));
		if (moved == 0u) {
			settled = true;
			break;
		}
	}
	EXPECT_TRUE(context, settled);
	for (int z = 0; z < 16; ++z) { // no hovering Fluid anywhere after settle
		for (int y = 1; y < 16; ++y) {
			for (int x = 0; x < 16; ++x) {
				if (GetVoxelMaterial(world, {x, y, z}) == VoxelMaterial::Fluid) {
					EXPECT_TRUE(context, GetVoxelMaterial(world, {x, y - 1, z}) != VoxelMaterial::Air);
				}
			}
		}
	}
	for (int z = 0; z < 16; ++z) { // interior above the hole rim is empty
		for (int y = holeBottomY + 1; y < 16; ++y) {
			for (int x = 0; x < 16; ++x) {
				const int dx = x - center.x;
				const int dy = y - center.y;
				const int dz = z - center.z;
				if (dx * dx + dy * dy + dz * dz < radius * radius) {
					EXPECT_EQ(context, VoxelMaterial::Air, GetVoxelMaterial(world, {x, y, z}));
				}
			}
		}
	}
}

void TestFluidCAIdleFastPath(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(8, 4, 8);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid, nullptr);
	EXPECT_EQ(context, 0u, UpdateFluidCA(world)); // lone cell settles immediately
	const uint64_t serial = world.fluidEditSerial;
	EXPECT_EQ(context, 0u, UpdateFluidCA(world)); // early-out: nothing changed since the no-move tick
	EXPECT_EQ(context, serial, world.fluidEditSerial);
	SetVoxelMaterial(world, {4, 1, 4}, VoxelMaterial::Fluid, nullptr); // external edit wakes the CA
	EXPECT_TRUE(context, world.fluidEditSerial != serial);
	UpdateFluidCA(world);
	EXPECT_EQ(context, size_t{2}, CountFluid(world));
}

void TestFluidCAGlassEditWakesCa(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(8, 6, 8);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	for (int z = 3; z <= 5; ++z) { // glass ring around the pool cell (4,1,4)
		for (int x = 3; x <= 5; ++x) {
			if (x == 4 && z == 4) {
				continue;
			}
			SetVoxelMaterial(world, {x, 1, z}, VoxelMaterial::Glass, nullptr);
		}
	}
	SetVoxelMaterial(world, {4, 1, 4}, VoxelMaterial::Fluid, nullptr);
	SetVoxelMaterial(world, {4, 2, 4}, VoxelMaterial::Fluid, nullptr);
	for (int tick = 0; tick < 4; ++tick) { // sealed pool settles
		UpdateFluidCA(world);
	}
	EXPECT_EQ(context, 0u, UpdateFluidCA(world));
	const uint64_t serialBefore = world.fluidEditSerial;
	SetVoxelMaterial(world, {3, 1, 4}, VoxelMaterial::Air, nullptr); // break the glass wall — no Air↔Fluid transition
	EXPECT_TRUE(context, world.fluidEditSerial != serialBefore);	 // CA must wake on any material change
	bool escaped = false;
	for (int tick = 0; tick < 16; ++tick) {
		UpdateFluidCA(world);
		EXPECT_EQ(context, size_t{2}, CountFluid(world));
		if (GetVoxelMaterial(world, {3, 1, 4}) == VoxelMaterial::Fluid) {
			escaped = true;
			break;
		}
	}
	EXPECT_TRUE(context, escaped); // water drained through the broken wall
}

void TestFluidCAOverflowSpreadsAroundPlatform(TestContext &context)
{
	VoxelWorld world = MakeFluidCATestWorld(16, 6, 16);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	for (int z = 4; z <= 11; ++z) { // raised glass slab 8×8 at y=1
		for (int x = 4; x <= 11; ++x) {
			SetVoxelMaterial(world, {x, 1, z}, VoxelMaterial::Glass, nullptr);
		}
	}
	for (int y = 2; y <= 3; ++y) { // 5×5×2 water block on the slab
		for (int z = 5; z <= 9; ++z) {
			for (int x = 5; x <= 9; ++x) {
				SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::Fluid, nullptr);
			}
		}
	}
	const size_t mass = CountFluid(world);
	bool settled = false;
	for (int tick = 0; tick < 300; ++tick) {
		const uint32_t moved = UpdateFluidCA(world);
		EXPECT_EQ(context, mass, CountFluid(world));
		if (moved == 0u) {
			settled = true;
			break;
		}
	}
	EXPECT_TRUE(context, settled);
	int sideN = 0; // overflow cells at y=1 beyond the slab footprint, attributed to the nearest slab side
	int sideS = 0;
	int sideW = 0;
	int sideE = 0;
	for (int z = 0; z < 16; ++z) {
		for (int x = 0; x < 16; ++x) {
			if (x >= 4 && x <= 11 && z >= 4 && z <= 11) {
				continue; // on the slab
			}
			if (GetVoxelMaterial(world, {x, 1, z}) != VoxelMaterial::Fluid) {
				continue;
			}
			if (z < 4) {
				++sideN;
			} else if (z > 11) {
				++sideS;
			} else if (x < 4) {
				++sideW;
			} else {
				++sideE;
			}
		}
	}
	const int total = sideN + sideS + sideW + sideE;
	EXPECT_TRUE(context, total > 0);
	int sidesUsed = 0;
	const int sides[4] = {sideN, sideS, sideW, sideE};
	int maxSide = 0;
	for (const int side : sides) {
		if (side > 0) {
			++sidesUsed;
		}
		maxSide = std::max(maxSide, side);
	}
	EXPECT_TRUE(context, sidesUsed >= 3);			 // overflow went around the slab, not one way
	EXPECT_TRUE(context, maxSide * 10 <= total * 7); // no single direction dominates (≤70%)
	int quadNW = 0;									 // overflow cells per quadrant relative to the slab center (8,8) — compass-bias anchor
	int quadNE = 0;
	int quadSW = 0;
	int quadSE = 0;
	for (int z = 0; z < 16; ++z) {
		for (int x = 0; x < 16; ++x) {
			if (x >= 4 && x <= 11 && z >= 4 && z <= 11) {
				continue;
			}
			if (GetVoxelMaterial(world, {x, 1, z}) != VoxelMaterial::Fluid) {
				continue;
			}
			if (x < 8 && z < 8) {
				++quadNW;
			} else if (x >= 8 && z < 8) {
				++quadNE;
			} else if (x < 8) {
				++quadSW;
			} else {
				++quadSE;
			}
		}
	}
	const int quadMax = std::max(std::max(quadNW, quadNE), std::max(quadSW, quadSE));
	EXPECT_TRUE(context, quadMax * 10 <= total * 6); // no quadrant holds >60% of the overflow
}

void TestFluidCABottomBreakDumpsWater(TestContext &context) // N8: bottom hole dumps the whole sphere onto the floor
{
	VoxelWorld world = MakeFluidCATestWorld(16, 16, 16);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	const Int3 center{8, 9, 8};
	constexpr int radius = 4;
	for (int z = 0; z < 16; ++z) {
		for (int y = 0; y < 16; ++y) {
			for (int x = 0; x < 16; ++x) {
				const int dx = x - center.x;
				const int dy = y - center.y;
				const int dz = z - center.z;
				const int dist2 = dx * dx + dy * dy + dz * dz;
				if (dist2 >= radius * radius && dist2 <= radius * radius + radius) {
					SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::Glass, nullptr);
				} else if (dist2 < radius * radius) {
					SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::Fluid, nullptr);
				}
			}
		}
	}
	SetVoxelMaterial(world, {center.x, center.y - radius, center.z}, VoxelMaterial::Air, nullptr); // bottom pole breach
	const size_t mass = CountFluid(world);
	bool settled = false;
	for (int tick = 0; tick < 600; ++tick) {
		const uint32_t moved = UpdateFluidCA(world);
		EXPECT_EQ(context, mass, CountFluid(world));
		if (moved == 0u) {
			settled = true;
			break;
		}
	}
	EXPECT_TRUE(context, settled);
	for (int z = 0; z < 16; ++z) { // sphere interior is empty after the dump
		for (int y = 0; y < 16; ++y) {
			for (int x = 0; x < 16; ++x) {
				const int dx = x - center.x;
				const int dy = y - center.y;
				const int dz = z - center.z;
				if (dx * dx + dy * dy + dz * dz < radius * radius) {
					EXPECT_EQ(context, VoxelMaterial::Air, GetVoxelMaterial(world, {x, y, z}));
				}
			}
		}
	}
	for (int z = 0; z < 16; ++z) { // no hovering Fluid after settle
		for (int y = 1; y < 16; ++y) {
			for (int x = 0; x < 16; ++x) {
				if (GetVoxelMaterial(world, {x, y, z}) == VoxelMaterial::Fluid) {
					EXPECT_TRUE(context, GetVoxelMaterial(world, {x, y - 1, z}) != VoxelMaterial::Air);
				}
			}
		}
	}
	int floorCount = 0; // dumped water rests on the floor
	for (int z = 0; z < 16; ++z) {
		for (int x = 0; x < 16; ++x) {
			if (GetVoxelMaterial(world, {x, 1, z}) == VoxelMaterial::Fluid) {
				++floorCount;
			}
		}
	}
	EXPECT_TRUE(context, floorCount > 0);
}

void TestFluidCAStrayCrawlsBackToPuddle(TestContext &context) // N9: isolated cell in the same basin rejoins the disc
{
	VoxelWorld world = MakeFluidCATestWorld(10, 4, 10);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	for (int z = 2; z <= 4; ++z) {
		for (int x = 2; x <= 4; ++x) {
			SetVoxelMaterial(world, {x, 1, z}, VoxelMaterial::Fluid, nullptr);
		}
	}
	SetVoxelMaterial(world, {8, 1, 4}, VoxelMaterial::Fluid, nullptr); // stray: isolated, same unobstructed floor
	const size_t mass = CountFluid(world);
	bool merged = false;
	for (int tick = 0; tick < 40; ++tick) {
		const uint32_t moved = UpdateFluidCA(world);
		EXPECT_EQ(context, mass, CountFluid(world));
		if (moved == 0u) {
			merged = true;
			break;
		}
	}
	EXPECT_TRUE(context, merged);
	const std::vector<Int3> cells = CollectFluidCells(world);
	bool seen[10][10] = {};
	for (const Int3 c : cells) {
		seen[c.x][c.z] = true;
	}
	int reached = 0; // one 6-connected component over XZ at y=1 (single-layer disc)
	std::vector<Int3> stack{cells.front()};
	seen[cells.front().x][cells.front().z] = false;
	while (!stack.empty()) {
		const Int3 cur = stack.back();
		stack.pop_back();
		++reached;
		constexpr int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
		for (const auto &d : dirs) {
			const int nx = cur.x + d[0];
			const int nz = cur.z + d[1];
			if (nx >= 0 && nx < 10 && nz >= 0 && nz < 10 && seen[nx][nz]) {
				seen[nx][nz] = false;
				stack.push_back({nx, 1, nz});
			}
		}
	}
	EXPECT_EQ(context, static_cast<int>(cells.size()), reached); // no strays left
}

void TestFluidCASideBreakStreamIsContinuous(TestContext &context) // N10: side breach emits a contiguous stream, no portions
{
	VoxelWorld world = MakeFluidCATestWorld(12, 12, 12);
	FillFloor(world, 0, VoxelMaterial::FloorWhite);
	for (int y = 2; y <= 6; ++y) { // sealed glass box 5×5×5 filled with fluid
		for (int z = 4; z <= 8; ++z) {
			for (int x = 4; x <= 8; ++x) {
				const bool shell = x == 4 || x == 8 || z == 4 || z == 8 || y == 2 || y == 6;
				SetVoxelMaterial(world, {x, y, z}, shell ? VoxelMaterial::Glass : VoxelMaterial::Fluid, nullptr);
			}
		}
	}
	SetVoxelMaterial(world, {8, 4, 6}, VoxelMaterial::Air, nullptr); // side breach at (8,4,6)
	const size_t mass = CountFluid(world);
	bool settled = false;
	for (int tick = 0; tick < 300; ++tick) {
		const uint32_t moved = UpdateFluidCA(world);
		EXPECT_EQ(context, mass, CountFluid(world));
		int low = 99; // no Air gap inside the falling column under the breach (exit at x=9)
		int high = -1;
		for (int y = 1; y <= 6; ++y) {
			if (GetVoxelMaterial(world, {9, y, 6}) == VoxelMaterial::Fluid) {
				low = std::min(low, y);
				high = std::max(high, y);
			}
		}
		for (int y = low; y <= high && low <= high; ++y) {
			EXPECT_TRUE(context, GetVoxelMaterial(world, {9, y, 6}) == VoxelMaterial::Fluid); // stream never splits
		}
		if (moved == 0u) {
			settled = true;
			break;
		}
	}
	EXPECT_TRUE(context, settled);
}

int main() // NOLINT(*-exception-escape): MSVC STL allocation may throw; terminating the test process is intended.
{
	TestContext context;
	TestFluidCASingleCellFallsOneCellPerTick(context);
	TestFluidCAMassPreservedEachTick(context);
	TestFluidCAHeight1StaysOnFlatFloor(context);
	TestFluidCAHeight1StepsDownOffPlatform(context);
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
	TestFluidCAStrictLayeredDrain(context);
	TestFluidCAContinuityNoTeleport(context);
	TestFluidCAStreamContinuity(context);
	TestFluidCASphereBreakEndToEnd(context);
	TestFluidCAIdleFastPath(context);
	TestFluidCAGlassEditWakesCa(context);
	TestFluidCAOverflowSpreadsAroundPlatform(context);
	TestFluidCABottomBreakDumpsWater(context);
	TestFluidCAStrayCrawlsBackToPuddle(context);
	TestFluidCASideBreakStreamIsContinuous(context);

	if (context.failures != 0) {
		std::fprintf(stderr, "ProjectVFluidCATests failed (%d)\n", context.failures);
		return 1;
	}
	std::puts("ProjectVFluidCATests passed");
	return 0;
}

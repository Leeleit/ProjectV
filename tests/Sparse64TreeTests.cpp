#include "voxel/Sparse64Tree.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string_view>
#include <tuple>
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

void ExpectEqualI(TestContext &context, int expected, int actual, int line, std::string_view expr)
{
	if (expected != actual) {
		char buffer[256]{};
		std::snprintf(buffer, sizeof(buffer), "%.*s (expected %d, got %d)", static_cast<int>(expr.size()), expr.data(), expected, actual);
		context.Fail(line, buffer);
	}
}

void ExpectTrue(TestContext &context, bool condition, int line, std::string_view expr)
{
	if (!condition) {
		context.Fail(line, expr);
	}
}

void TestDepthComputation(TestContext &context)
{
	ExpectEqualI(context, 0, projectv::voxel::ComputeSparse64Depth(0), __LINE__, "depth(0)=0");
	ExpectEqualI(context, 0, projectv::voxel::ComputeSparse64Depth(1), __LINE__, "depth(1)=0");
	ExpectEqualI(context, 1, projectv::voxel::ComputeSparse64Depth(2), __LINE__, "depth(2)=1");
	ExpectEqualI(context, 1, projectv::voxel::ComputeSparse64Depth(3), __LINE__, "depth(3)=1");
	ExpectEqualI(context, 1, projectv::voxel::ComputeSparse64Depth(4), __LINE__, "depth(4)=1");
	ExpectEqualI(context, 2, projectv::voxel::ComputeSparse64Depth(5), __LINE__, "depth(5)=2");
	ExpectEqualI(context, 2, projectv::voxel::ComputeSparse64Depth(8), __LINE__, "depth(8)=2");
	ExpectEqualI(context, 2, projectv::voxel::ComputeSparse64Depth(12), __LINE__, "depth(12)=2");
	ExpectEqualI(context, 2, projectv::voxel::ComputeSparse64Depth(16), __LINE__, "depth(16)=2");
	ExpectEqualI(context, 3, projectv::voxel::ComputeSparse64Depth(17), __LINE__, "depth(17)=3");
	ExpectEqualI(context, 3, projectv::voxel::ComputeSparse64Depth(32), __LINE__, "depth(32)=3");
	ExpectEqualI(context, 3, projectv::voxel::ComputeSparse64Depth(64), __LINE__, "depth(64)=3");
	ExpectEqualI(context, 4, projectv::voxel::ComputeSparse64Depth(65), __LINE__, "depth(65)=4");
	ExpectEqualI(context, 4, projectv::voxel::ComputeSparse64Depth(256), __LINE__, "depth(256)=4");
}

void TestSlotEncoding(TestContext &context)
{
	constexpr uint32_t airLeaf = projectv::voxel::MakeSparse64Leaf(0);
	ExpectTrue(context, projectv::voxel::IsSparse64Leaf(airLeaf), __LINE__, "MakeSparse64Leaf(0) is leaf");
	ExpectEqualI(context, 0, projectv::voxel::Sparse64LeafMaterial(airLeaf), __LINE__, "air material = 0");

	constexpr uint32_t glassLeaf = projectv::voxel::MakeSparse64Leaf(1);
	ExpectTrue(context, projectv::voxel::IsSparse64Leaf(glassLeaf), __LINE__, "MakeSparse64Leaf(1) is leaf");
	ExpectEqualI(context, 1, projectv::voxel::Sparse64LeafMaterial(glassLeaf), __LINE__, "glass material = 1");

	constexpr uint32_t maxLeaf = projectv::voxel::MakeSparse64Leaf(255);
	ExpectTrue(context, projectv::voxel::IsSparse64Leaf(maxLeaf), __LINE__, "MakeSparse64Leaf(255) is leaf");
	ExpectEqualI(context, 255, projectv::voxel::Sparse64LeafMaterial(maxLeaf), __LINE__, "max material = 255");

	constexpr uint32_t nodeSlot = 42u;
	ExpectTrue(context, !projectv::voxel::IsSparse64Leaf(nodeSlot), __LINE__, "raw index is not leaf");
	ExpectEqualI(context, 42, static_cast<int>(projectv::voxel::Sparse64NodeIndex(nodeSlot)), __LINE__, "node index preserved");
}

void TestEmptyTree(TestContext &context)
{
	const projectv::voxel::Sparse64Tree tree(8, 8, 8);
	ExpectEqualI(context, 8, tree.SideX(), __LINE__, "sideX=8");
	ExpectEqualI(context, 8, tree.SideY(), __LINE__, "sideY=8");
	ExpectEqualI(context, 8, tree.SideZ(), __LINE__, "sideZ=8");
	ExpectEqualI(context, 2, tree.MaxDepth(), __LINE__, "8x8x8 -> depth 2");
	ExpectTrue(context, tree.IsEmpty(), __LINE__, "fresh tree is empty");
	ExpectEqualI(context, 0, static_cast<int>(tree.NodeCount()), __LINE__, "fresh tree has 0 nodes");
	for (int z = 0; z < 8; ++z) {
		for (int y = 0; y < 8; ++y) {
			for (int x = 0; x < 8; ++x) {
				if (tree.GetCell(x, y, z) != 0) {
					char buf[128]{};
					std::snprintf(buf, sizeof(buf), "empty tree cell (%d,%d,%d) = 0", x, y, z);
					context.Fail(__LINE__, buf);
				}
			}
		}
	}
}

void TestSetSingleCell(TestContext &context)
{
	projectv::voxel::Sparse64Tree tree(8, 8, 8);
	tree.SetCell(3, 5, 7, 1);
	ExpectTrue(context, !tree.IsEmpty(), __LINE__, "tree non-empty after SetCell");
	ExpectEqualI(context, 1, static_cast<int>(tree.GetCell(3, 5, 7)), __LINE__, "cell (3,5,7) = 1");
	for (int z = 0; z < 8; ++z) {
		for (int y = 0; y < 8; ++y) {
			for (int x = 0; x < 8; ++x) {
				if (x == 3 && y == 5 && z == 7) {
					continue;
				}
				if (tree.GetCell(x, y, z) != 0) {
					char buf[128]{};
					std::snprintf(buf, sizeof(buf), "other cell (%d,%d,%d) should still be 0", x, y, z);
					context.Fail(__LINE__, buf);
				}
			}
		}
	}
}

void TestSetAllCells(TestContext &context)
{
	projectv::voxel::Sparse64Tree tree(8, 8, 8);
	for (int z = 0; z < 8; ++z) {
		for (int y = 0; y < 8; ++y) {
			for (int x = 0; x < 8; ++x) {
				tree.SetCell(x, y, z, 3);
			}
		}
	}
	for (int z = 0; z < 8; ++z) {
		for (int y = 0; y < 8; ++y) {
			for (int x = 0; x < 8; ++x) {
				if (tree.GetCell(x, y, z) != 3) {
					char buf[128]{};
					std::snprintf(buf, sizeof(buf), "all-cells (=%d,%d,%d) should be 3", x, y, z);
					context.Fail(__LINE__, buf);
				}
			}
		}
	}
}

void TestMixedMaterials(TestContext &context)
{
	projectv::voxel::Sparse64Tree tree(8, 8, 8);
	const std::vector<std::tuple<int, int, int, uint8_t>> edits{
		{0, 0, 0, 1},
		{7, 7, 7, 2},
		{3, 0, 5, 3},
		{0, 4, 0, 4},
		{4, 4, 4, 1},
		{4, 4, 5, 1},
		{4, 5, 4, 1},
		{5, 4, 4, 1},
		{4, 4, 6, 2},
		{1, 1, 1, 1},
		{6, 6, 6, 1},
	};
	for (const auto &[x, y, z, m] : edits) {
		tree.SetCell(x, y, z, m);
	}
	for (const auto &[x, y, z, m] : edits) {
		if (tree.GetCell(x, y, z) != m) {
			char buf[128]{};
			std::snprintf(buf, sizeof(buf), "cell (%d,%d,%d) = %d, want %d", x, y, z, tree.GetCell(x, y, z), m);
			context.Fail(__LINE__, buf);
		}
	}
	for (int z = 0; z < 8; ++z) {
		for (int y = 0; y < 8; ++y) {
			for (int x = 0; x < 8; ++x) {
				uint8_t expected = 0;
				for (const auto &[ex, ey, ez, em] : edits) {
					if (ex == x && ey == y && ez == z) {
						expected = em;
					}
				}
				if (tree.GetCell(x, y, z) != expected) {
					char buf[128]{};
					std::snprintf(buf, sizeof(buf), "mixed cell (%d,%d,%d) = %d, want %d", x, y, z, tree.GetCell(x, y, z), expected);
					context.Fail(__LINE__, buf);
				}
			}
		}
	}
}

void TestSetThenClear(TestContext &context)
{
	projectv::voxel::Sparse64Tree tree(8, 8, 8);
	tree.SetCell(2, 2, 2, 5);
	ExpectEqualI(context, 5, static_cast<int>(tree.GetCell(2, 2, 2)), __LINE__, "set 5");
	tree.SetCell(2, 2, 2, 0);
	ExpectEqualI(context, 0, static_cast<int>(tree.GetCell(2, 2, 2)), __LINE__, "cleared to 0");
	tree.SetCell(2, 2, 2, 7);
	ExpectEqualI(context, 7, static_cast<int>(tree.GetCell(2, 2, 2)), __LINE__, "set 7 after clear");
}

void TestOutOfBounds(TestContext &context)
{
	projectv::voxel::Sparse64Tree tree(8, 8, 8);
	tree.SetCell(-1, 0, 0, 5);
	tree.SetCell(0, 0, 8, 5);
	tree.SetCell(8, 0, 0, 5);
	ExpectTrue(context, tree.IsEmpty(), __LINE__, "out-of-bounds SetCell is no-op");
	if (tree.GetCell(-1, 0, 0) != 0) {
		context.Fail(__LINE__, "out-of-bounds GetCell returns 0");
	}
	if (tree.GetCell(8, 0, 0) != 0) {
		context.Fail(__LINE__, "out-of-bounds GetCell returns 0");
	}
}

void TestReset(TestContext &context)
{
	projectv::voxel::Sparse64Tree tree(8, 8, 8);
	tree.SetCell(0, 0, 0, 1);
	tree.SetCell(1, 0, 0, 2);
	ExpectTrue(context, tree.NodeCount() > 0, __LINE__, "tree has nodes after edits");
	tree.Reset(16, 16, 16);
	ExpectEqualI(context, 16, tree.SideX(), __LINE__, "Reset sideX=16");
	ExpectEqualI(context, 16, tree.SideY(), __LINE__, "Reset sideY=16");
	ExpectEqualI(context, 16, tree.SideZ(), __LINE__, "Reset sideZ=16");
	ExpectEqualI(context, 2, tree.MaxDepth(), __LINE__, "16x16x16 -> depth 2");
	ExpectTrue(context, tree.IsEmpty(), __LINE__, "tree empty after Reset");
}

void TestLargerTree(TestContext &context)
{
	projectv::voxel::Sparse64Tree tree(32, 32, 32);
	ExpectEqualI(context, 3, tree.MaxDepth(), __LINE__, "32x32x32 -> depth 3");
	ExpectTrue(context, tree.IsEmpty(), __LINE__, "32x32x32 fresh empty");
	tree.SetCell(0, 0, 0, 1);
	tree.SetCell(31, 31, 31, 2);
	tree.SetCell(15, 15, 15, 3);
	tree.SetCell(16, 16, 16, 4);
	tree.SetCell(7, 23, 11, 5);
	ExpectEqualI(context, 1, static_cast<int>(tree.GetCell(0, 0, 0)), __LINE__, "32^3 (0,0,0)");
	ExpectEqualI(context, 2, static_cast<int>(tree.GetCell(31, 31, 31)), __LINE__, "32^3 (31,31,31)");
	ExpectEqualI(context, 3, static_cast<int>(tree.GetCell(15, 15, 15)), __LINE__, "32^3 (15,15,15)");
	ExpectEqualI(context, 4, static_cast<int>(tree.GetCell(16, 16, 16)), __LINE__, "32^3 (16,16,16)");
	ExpectEqualI(context, 5, static_cast<int>(tree.GetCell(7, 23, 11)), __LINE__, "32^3 (7,23,11)");
	if (tree.GetCell(0, 0, 1) != 0) {
		context.Fail(__LINE__, "32^3 (0,0,1) = 0");
	}
	if (tree.GetCell(30, 31, 31) != 0) {
		context.Fail(__LINE__, "32^3 (30,31,31) = 0");
	}
}

void TestByteExactParityVsFlat(TestContext &context)
{
	const int sideX = 8;
	const int sideY = 8;
	const int sideZ = 8;
	std::vector<uint8_t> flat(static_cast<size_t>(sideX) * sideY * sideZ, 0);
	projectv::voxel::Sparse64Tree tree(sideX, sideY, sideZ);

	std::mt19937 rng(0xC0FFEEu);
	std::uniform_int_distribution<int> coordDistX(0, sideX - 1);
	std::uniform_int_distribution<int> coordDistY(0, sideY - 1);
	std::uniform_int_distribution<int> coordDistZ(0, sideZ - 1);
	std::uniform_int_distribution<int> matDist(1, 4);

	constexpr int kEditCount = 200;
	for (int i = 0; i < kEditCount; ++i) {
		const int x = coordDistX(rng);
		const int y = coordDistY(rng);
		const int z = coordDistZ(rng);
		const uint8_t m = static_cast<uint8_t>(matDist(rng));
		const size_t idx = static_cast<size_t>(x) + static_cast<size_t>(sideX) * (y + static_cast<size_t>(sideY) * z);
		flat[idx] = m;
		tree.SetCell(x, y, z, m);
	}

	int mismatches = 0;
	int firstMismatchX = -1;
	int firstMismatchY = -1;
	int firstMismatchZ = -1;
	int firstMismatchFlat = -1;
	int firstMismatchTree = -1;
	for (int z = 0; z < sideZ; ++z) {
		for (int y = 0; y < sideY; ++y) {
			for (int x = 0; x < sideX; ++x) {
				const size_t idx = static_cast<size_t>(x) + static_cast<size_t>(sideX) * (y + static_cast<size_t>(sideY) * z);
				const uint8_t flatV = flat[idx];
				const uint8_t treeV = tree.GetCell(x, y, z);
				if (flatV != treeV) {
					if (mismatches == 0) {
						firstMismatchX = x;
						firstMismatchY = y;
						firstMismatchZ = z;
						firstMismatchFlat = flatV;
						firstMismatchTree = treeV;
					}
					++mismatches;
				}
			}
		}
	}
	if (mismatches > 0) {
		char buf[256]{};
		std::snprintf(buf, sizeof(buf), "byte-exact parity: %d mismatches; first at (%d,%d,%d) flat=%d tree=%d",
			mismatches, firstMismatchX, firstMismatchY, firstMismatchZ, firstMismatchFlat, firstMismatchTree);
		context.Fail(__LINE__, buf);
	}
}

void TestFullSweepParity(TestContext &context)
{
	const int sideX = 4;
	const int sideY = 4;
	const int sideZ = 4;
	std::vector<uint8_t> flat(static_cast<size_t>(sideX) * sideY * sideZ, 0);
	projectv::voxel::Sparse64Tree tree(sideX, sideY, sideZ);

	for (int z = 0; z < sideZ; ++z) {
		for (int y = 0; y < sideY; ++y) {
			for (int x = 0; x < sideX; ++x) {
				const uint8_t m = static_cast<uint8_t>(((x * 3 + y * 5 + z * 7) % 4) + 1);
				const size_t idx = static_cast<size_t>(x) + static_cast<size_t>(sideX) * (y + static_cast<size_t>(sideY) * z);
				flat[idx] = m;
				tree.SetCell(x, y, z, m);
			}
		}
	}

	for (int z = 0; z < sideZ; ++z) {
		for (int y = 0; y < sideY; ++y) {
			for (int x = 0; x < sideX; ++x) {
				const size_t idx = static_cast<size_t>(x) + static_cast<size_t>(sideX) * (y + static_cast<size_t>(sideY) * z);
				if (flat[idx] != tree.GetCell(x, y, z)) {
					char buf[128]{};
					std::snprintf(buf, sizeof(buf), "full-sweep mismatch at (%d,%d,%d) flat=%d tree=%d",
						x, y, z, flat[idx], tree.GetCell(x, y, z));
					context.Fail(__LINE__, buf);
				}
			}
		}
	}
}

void TestOverwriteParity(TestContext &context)
{
	const int sideX = 8;
	const int sideY = 8;
	const int sideZ = 8;
	std::vector<uint8_t> flat(static_cast<size_t>(sideX) * sideY * sideZ, 0);
	projectv::voxel::Sparse64Tree tree(sideX, sideY, sideZ);

	std::mt19937 rng(0xBADBEEFu);
	std::uniform_int_distribution<int> coordDist(0, sideX - 1);
	std::uniform_int_distribution<int> matDist(0, 4);

	for (int pass = 0; pass < 5; ++pass) {
		for (int i = 0; i < 50; ++i) {
			const int x = coordDist(rng);
			const int y = coordDist(rng);
			const int z = coordDist(rng);
			const uint8_t m = static_cast<uint8_t>(matDist(rng));
			const size_t idx = static_cast<size_t>(x) + static_cast<size_t>(sideX) * (y + static_cast<size_t>(sideY) * z);
			flat[idx] = m;
			tree.SetCell(x, y, z, m);
		}
	}

	for (int z = 0; z < sideZ; ++z) {
		for (int y = 0; y < sideY; ++y) {
			for (int x = 0; x < sideX; ++x) {
				const size_t idx = static_cast<size_t>(x) + static_cast<size_t>(sideX) * (y + static_cast<size_t>(sideY) * z);
				if (flat[idx] != tree.GetCell(x, y, z)) {
					char buf[128]{};
					std::snprintf(buf, sizeof(buf), "overwrite mismatch at (%d,%d,%d) flat=%d tree=%d",
						x, y, z, flat[idx], tree.GetCell(x, y, z));
					context.Fail(__LINE__, buf);
				}
			}
		}
	}
}

void TestSubNodeSplitting(TestContext &context)
{
	projectv::voxel::Sparse64Tree tree(8, 8, 8);
	tree.SetCell(0, 0, 0, 5);
	ExpectEqualI(context, 2, static_cast<int>(tree.NodeCount()), __LINE__, "2 nodes after first SetCell (root+mid)");

	tree.SetCell(4, 0, 0, 5);
	ExpectEqualI(context, 3, static_cast<int>(tree.NodeCount()), __LINE__, "3 nodes after second SetCell in different sub-volume");

	tree.SetCell(0, 0, 0, 6);
	ExpectEqualI(context, 6, static_cast<int>(tree.GetCell(0, 0, 0)), __LINE__, "overwrite within same sub-volume");
	ExpectEqualI(context, 5, static_cast<int>(tree.GetCell(4, 0, 0)), __LINE__, "other sub-volume unchanged");
}

void TestNonAirCount(TestContext &context)
{
	projectv::voxel::Sparse64Tree tree(4, 4, 4);
	ExpectTrue(context, tree.IsEmpty(), __LINE__, "fresh 4x4x4 is empty");
	tree.SetCell(0, 0, 0, 1);
	ExpectTrue(context, !tree.IsEmpty(), __LINE__, "1 cell set -> not empty");
	tree.SetCell(0, 0, 0, 0);
	ExpectTrue(context, tree.IsEmpty(), __LINE__, "1 cell cleared -> empty");
	tree.SetCell(0, 0, 0, 2);
	tree.SetCell(1, 1, 1, 3);
	ExpectTrue(context, !tree.IsEmpty(), __LINE__, "2 cells set -> not empty");
}

void TestDedupOffBaseline(TestContext &context)
{
	projectv::voxel::Sparse64Tree tree(4, 4, 4);
	ExpectTrue(context, !tree.IsDeduplicationEnabled(), __LINE__, "dedup OFF by default");
	tree.SetCell(0, 0, 0, 1);
	tree.SetCell(1, 1, 1, 2);
	ExpectEqualI(context, 1, static_cast<int>(tree.GetCell(0, 0, 0)), __LINE__, "dedup-off (0,0,0)");
	ExpectEqualI(context, 2, static_cast<int>(tree.GetCell(1, 1, 1)), __LINE__, "dedup-off (1,1,1)");
}

void TestDedupExplicitMerge(TestContext &context)
{
	projectv::voxel::Sparse64Tree tree(8, 8, 8);
	const size_t beforeNodeCount = tree.NodeCount();
	tree.SetDeduplicationEnabled(true);
	tree.SetCell(0, 0, 0, 1);
	const size_t afterOneCellNodeCount = tree.NodeCount();
	ExpectTrue(context, afterOneCellNodeCount > beforeNodeCount, __LINE__, "dedup enabled grows nodes on SetCell");
	tree.SetCell(7, 7, 7, 1);
	const size_t afterTwoCellsNodeCount = tree.NodeCount();
	ExpectTrue(context, afterTwoCellsNodeCount >= afterOneCellNodeCount, __LINE__, "more cells -> more or equal nodes");
}

void TestCopyOnWriteOnMutation(TestContext &context)
{
	projectv::voxel::Sparse64Tree tree(8, 8, 8);
	tree.SetDeduplicationEnabled(true);
	tree.SetCell(0, 0, 0, 1);
	tree.SetCell(7, 7, 7, 1);
	const size_t nodesBefore = tree.NodeCount();
	ExpectTrue(context, nodesBefore > 0, __LINE__, "tree has nodes after edits");
	tree.SetCell(0, 0, 0, 2);
	ExpectEqualI(context, 2, static_cast<int>(tree.GetCell(0, 0, 0)), __LINE__, "after mutation cell (0,0,0)");
	ExpectEqualI(context, 1, static_cast<int>(tree.GetCell(7, 7, 7)), __LINE__, "other cell unchanged");
	const size_t nodesAfter = tree.NodeCount();
	ExpectTrue(context, nodesAfter >= nodesBefore, __LINE__, "mutation may add nodes (COW)");
}

void TestHomogeneousCollapseSmall(TestContext &context)
{
	projectv::voxel::Sparse64Tree tree(4, 4, 4);
	for (int z = 0; z < 4; ++z) {
		for (int y = 0; y < 4; ++y) {
			for (int x = 0; x < 4; ++x) {
				tree.SetCell(x, y, z, 1);
			}
		}
	}
	ExpectTrue(context, projectv::voxel::IsSparse64Homogeneous(tree.RootSlot()), __LINE__, "4x4x4 fully filled same material -> homogeneous root slot");
	ExpectTrue(context, tree.LiveNodeCount() == 0, __LINE__, "no live nodes after homogeneous collapse");
	for (int z = 0; z < 4; ++z) {
		for (int y = 0; y < 4; ++y) {
			for (int x = 0; x < 4; ++x) {
				ExpectEqualI(context, 1, static_cast<int>(tree.GetCell(x, y, z)), __LINE__, "all cells material 1");
			}
		}
	}
	ExpectEqualI(context, 64, static_cast<int>(tree.NonAirCount()), __LINE__, "NonAirCount 64");
}

void TestHomogeneousExpansionOnSingleEdit(TestContext &context)
{
	projectv::voxel::Sparse64Tree tree(4, 4, 4);
	for (int z = 0; z < 4; ++z) {
		for (int y = 0; y < 4; ++y) {
			for (int x = 0; x < 4; ++x) {
				tree.SetCell(x, y, z, 1);
			}
		}
	}
	ExpectTrue(context, tree.LiveNodeCount() == 0, __LINE__, "pre-edit homogeneous -> 0 live nodes");
	tree.SetCell(1, 1, 1, 2);
	ExpectTrue(context, !projectv::voxel::IsSparse64Homogeneous(tree.RootSlot()), __LINE__, "after 1 different edit -> heterogeneous root");
	ExpectTrue(context, tree.LiveNodeCount() > 0, __LINE__, "live node allocated on expansion");
	ExpectEqualI(context, 2, static_cast<int>(tree.GetCell(1, 1, 1)), __LINE__, "edited cell material 2");
	for (int z = 0; z < 4; ++z) {
		for (int y = 0; y < 4; ++y) {
			for (int x = 0; x < 4; ++x) {
				if (x == 1 && y == 1 && z == 1) continue;
				ExpectEqualI(context, 1, static_cast<int>(tree.GetCell(x, y, z)), __LINE__, "other cells still material 1");
			}
		}
	}
}

void TestHomogeneousCascadeSixteen(TestContext &context)
{
	projectv::voxel::Sparse64Tree tree(16, 16, 16);
	for (int z = 0; z < 16; ++z) {
		for (int y = 0; y < 16; ++y) {
			for (int x = 0; x < 16; ++x) {
				tree.SetCell(x, y, z, 1);
			}
		}
	}
	ExpectTrue(context, projectv::voxel::IsSparse64Homogeneous(tree.RootSlot()), __LINE__, "16x16x16 fully filled -> homogeneous at every level");
	ExpectTrue(context, tree.LiveNodeCount() == 0, __LINE__, "16x16x16 homogeneous cascade -> 0 live nodes");
	ExpectEqualI(context, 4096, static_cast<int>(tree.NonAirCount()), __LINE__, "16x16x16 NonAirCount 4096");
}

void TestHomogeneousRecollapseAfterEdit(TestContext &context)
{
	projectv::voxel::Sparse64Tree tree(16, 16, 16);
	for (int z = 0; z < 16; ++z) {
		for (int y = 0; y < 16; ++y) {
			for (int x = 0; x < 16; ++x) {
				tree.SetCell(x, y, z, 1);
			}
		}
	}
	tree.SetCell(7, 7, 7, 2);
	ExpectTrue(context, !projectv::voxel::IsSparse64Homogeneous(tree.RootSlot()), __LINE__, "after edit -> heterogeneous");
	ExpectTrue(context, tree.LiveNodeCount() > 0, __LINE__, "live nodes allocated during edit");
	tree.SetCell(7, 7, 7, 1);
	ExpectTrue(context, projectv::voxel::IsSparse64Homogeneous(tree.RootSlot()), __LINE__, "after revert to uniform -> homogeneous again");
	ExpectTrue(context, tree.LiveNodeCount() == 0, __LINE__, "internal nodes collapsed back to 0 live");
}

void TestHomogeneousDedupEqual(TestContext &context)
{
	projectv::voxel::Sparse64Tree treeA(16, 16, 16);
	projectv::voxel::Sparse64Tree treeB(16, 16, 16);
	for (int z = 0; z < 16; ++z) {
		for (int y = 0; y < 16; ++y) {
			for (int x = 0; x < 16; ++x) {
				treeA.SetCell(x, y, z, 1);
				treeB.SetCell(x, y, z, 1);
			}
		}
	}
	ExpectTrue(context, projectv::voxel::IsSparse64Homogeneous(treeA.RootSlot()), __LINE__, "tree A homogeneous");
	ExpectTrue(context, projectv::voxel::IsSparse64Homogeneous(treeB.RootSlot()), __LINE__, "tree B homogeneous");
	ExpectEqualI(context, static_cast<int>(treeA.RootSlot()), static_cast<int>(treeB.RootSlot()), __LINE__, "identical homogeneous root slot");
}

} // namespace

int main()
{
	TestContext context{};

	TestDepthComputation(context);
	TestSlotEncoding(context);
	TestEmptyTree(context);
	TestSetSingleCell(context);
	TestSetAllCells(context);
	TestMixedMaterials(context);
	TestSetThenClear(context);
	TestOutOfBounds(context);
	TestReset(context);
	TestLargerTree(context);
	TestSubNodeSplitting(context);
	TestNonAirCount(context);
	TestDedupOffBaseline(context);
	TestDedupExplicitMerge(context);
	TestCopyOnWriteOnMutation(context);
	TestHomogeneousCollapseSmall(context);
	TestHomogeneousExpansionOnSingleEdit(context);
	TestHomogeneousCascadeSixteen(context);
	TestHomogeneousRecollapseAfterEdit(context);
	TestHomogeneousDedupEqual(context);
	TestFullSweepParity(context);
	TestByteExactParityVsFlat(context);
	TestOverwriteParity(context);

	if (context.failures != 0) {
		return EXIT_FAILURE;
	}

	std::puts("ProjectVSparse64TreeTests passed");
	return EXIT_SUCCESS;
}

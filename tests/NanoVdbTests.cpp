#include "voxel/NanoVdb.hpp"
#include "voxel/Sparse64Tree.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

int g_assertionCount = 0;
int g_failureCount = 0;

#define EXPECT_TRUE(condition, message)                                                \
	do {                                                                                \
		++g_assertionCount;                                                             \
		if (!(condition)) {                                                             \
			++g_failureCount;                                                           \
			std::fprintf(stderr, "%s:%d: EXPECT_TRUE failed: %s\n", __FILE__, __LINE__, \
				(message));                                                             \
		}                                                                               \
	} while (false)

#define EXPECT_EQ(a, b, message)                                                                  \
	do {                                                                                          \
		++g_assertionCount;                                                                       \
		auto lhsValue = (a);                                                                       \
		auto rhsValue = (b);                                                                       \
		if (!(lhsValue == rhsValue)) {                                                             \
			++g_failureCount;                                                                     \
			std::fprintf(stderr, "%s:%d: EXPECT_EQ failed: %s (lhs=%lld rhs=%lld)\n", __FILE__,    \
				__LINE__, (message), static_cast<long long>(lhsValue), static_cast<long long>(rhsValue)); \
		}                                                                                         \
	} while (false)

void TestEmptyChunkFlattens()
{
	projectv::voxel::Sparse64Tree tree(8, 8, 8);
	projectv::voxel::nanovdb::NanoVdbFlattenResult result;
	EXPECT_TRUE(
		projectv::voxel::nanovdb::BuildNanoVdbFlatten(tree, nullptr, result),
		"BuildNanoVdbFlatten empty chunk returns true");
	EXPECT_EQ(result.upperCount, 0u, "empty chunk has 0 uppers (root is homogeneous air)");
	EXPECT_EQ(result.lowerCount, 64u, "empty chunk has 64 sparse lowers");
	EXPECT_EQ(result.leafCount, 0u, "empty chunk has 0 leaves");
}

void TestSingleVoxelFlatten()
{
	projectv::voxel::Sparse64Tree tree(8, 8, 8);
	tree.SetCell(2, 3, 4, 1u);

	projectv::voxel::nanovdb::NanoVdbFlattenResult result;
	EXPECT_TRUE(
		projectv::voxel::nanovdb::BuildNanoVdbFlatten(tree, nullptr, result),
		"BuildNanoVdbFlatten single voxel returns true");

	for (uint32_t z = 0; z < 8u; ++z) {
		for (uint32_t y = 0; y < 8u; ++y) {
			for (uint32_t x = 0; x < 8u; ++x) {
				const uint8_t expected = (x == 2u && y == 3u && z == 4u) ? 1u : 0u;
				const uint8_t actual = projectv::voxel::nanovdb::ReadNanoVdbVoxelMaterial(
					result,
					0u,
					x,
					y,
					z);
				EXPECT_EQ(
					static_cast<uint32_t>(actual),
					static_cast<uint32_t>(expected),
					"single voxel re-read matches at (x,y,z)");
			}
		}
	}
}

void TestMultipleVoxelsFlatten()
{
	projectv::voxel::Sparse64Tree tree(8, 8, 8);
	tree.SetCell(0, 0, 0, 1u);
	tree.SetCell(7, 7, 7, 5u);
	tree.SetCell(3, 5, 2, 3u);

	projectv::voxel::nanovdb::NanoVdbFlattenResult result;
	EXPECT_TRUE(
		projectv::voxel::nanovdb::BuildNanoVdbFlatten(tree, nullptr, result),
		"BuildNanoVdbFlatten multi voxel returns true");

	for (uint32_t z = 0; z < 8u; ++z) {
		for (uint32_t y = 0; y < 8u; ++y) {
			for (uint32_t x = 0; x < 8u; ++x) {
				uint8_t expected = 0u;
				if (x == 0u && y == 0u && z == 0u) expected = 1u;
				else if (x == 7u && y == 7u && z == 7u) expected = 5u;
				else if (x == 3u && y == 5u && z == 2u) expected = 3u;
				const uint8_t actual = projectv::voxel::nanovdb::ReadNanoVdbVoxelMaterial(
					result,
					0u,
					x,
					y,
					z);
				if (actual != expected && (expected != 0u || actual != 0u)) {
					std::fprintf(stderr, "DBG: (%u,%u,%u) actual=%u expected=%u\n", x, y, z, actual, expected);
				}
				EXPECT_EQ(
					static_cast<uint32_t>(actual),
					static_cast<uint32_t>(expected),
					"multi voxel re-read matches at (x,y,z)");
			}
		}
	}
}

void TestMaterialLookupApplied()
{
	const uint8_t lookup[16] = {
		0, 7, 14, 21, 28, 35, 42, 49,
		56, 63, 70, 77, 84, 91, 98, 105,
	};
	projectv::voxel::Sparse64Tree tree(8, 8, 8);
	tree.SetCell(1, 1, 1, 5u);
	tree.SetCell(2, 2, 2, 9u);

	projectv::voxel::nanovdb::NanoVdbFlattenResult result;
	EXPECT_TRUE(
		projectv::voxel::nanovdb::BuildNanoVdbFlatten(tree, lookup, result),
		"BuildNanoVdbFlatten with lookup returns true");

	EXPECT_EQ(
		static_cast<uint32_t>(result.materials[0]),
		35u,
		"first material lookup applied (5 -> 35)");
	EXPECT_EQ(
		static_cast<uint32_t>(result.materials[1]),
		63u,
		"second material lookup applied (9 -> 63)");
}

void TestHomogeneousLeafFlagSet()
{
	projectv::voxel::Sparse64Tree tree(8, 8, 8);
	tree.SetCell(0, 0, 0, 7u);

	projectv::voxel::nanovdb::NanoVdbFlattenResult result;
	EXPECT_TRUE(
		projectv::voxel::nanovdb::BuildNanoVdbFlatten(tree, nullptr, result),
		"BuildNanoVdbFlatten homogeneous leaf returns true");

	bool anyHomogeneous = false;
	for (const auto &leaf : result.leaves) {
		if (leaf.homogeneousFlag != 0u) {
			anyHomogeneous = true;
			break;
		}
	}
	EXPECT_TRUE(anyHomogeneous, "at least one leaf has homogeneousFlag set");
}

}  // namespace

int main()
{
	TestEmptyChunkFlattens();
	TestSingleVoxelFlatten();
	TestMultipleVoxelsFlatten();
	TestMaterialLookupApplied();
	TestHomogeneousLeafFlagSet();

	std::fprintf(stderr, "NanoVdb tests: %d assertions, %d failures\n",
		g_assertionCount,
		g_failureCount);
	return (g_failureCount == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
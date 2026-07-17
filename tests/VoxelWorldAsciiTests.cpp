#include "voxel/VoxelAsciiTickLogger.hpp"
#include "voxel/VoxelWorldAscii.hpp"
#include "voxel/VoxelWorld.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
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

#define EXPECT_TRUE(ctx, cond)           \
	do {                                 \
		if (!(cond)) {                   \
			(ctx).Fail(__LINE__, #cond); \
		}                                \
	} while (0)

#define EXPECT_EQ_STR(ctx, actual, expected)                                                            \
	do {                                                                                                \
		if ((actual) != (expected)) {                                                                   \
			std::fprintf(stderr, "expected:\n%s\nactual:\n%s\n", (expected).c_str(), (actual).c_str()); \
			(ctx).Fail(__LINE__, "string mismatch");                                                    \
		}                                                                                               \
	} while (0)

VoxelWorld MakeEmptyWorld(const int width, const int height, const int depth)
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
		static_cast<size_t>(world.chunkCountX) * world.chunkCountY * world.chunkCountZ,
		VoxelChunk{});
	for (int chunkZ = 0; chunkZ < world.chunkCountZ; ++chunkZ) {
		for (int chunkY = 0; chunkY < world.chunkCountY; ++chunkY) {
			for (int chunkX = 0; chunkX < world.chunkCountX; ++chunkX) {
				const size_t chunkIndex = GetVoxelChunkIndex(world, {chunkX, chunkY, chunkZ});
				VoxelChunk &chunk = world.chunks[chunkIndex];
				chunk.min = {
					world.min.x + chunkX * world.chunkSize,
					world.min.y + chunkY * world.chunkSize,
					world.min.z + chunkZ * world.chunkSize,
				};
				chunk.maxExclusive = {
					std::min(chunk.min.x + world.chunkSize, world.maxExclusive.x),
					std::min(chunk.min.y + world.chunkSize, world.maxExclusive.y),
					std::min(chunk.min.z + world.chunkSize, world.maxExclusive.z),
				};
			}
		}
	}
	return world;
}

void TestMaterialToAscii(TestContext &context)
{
	EXPECT_TRUE(context, VoxelMaterialToAscii(VoxelMaterial::Air) == '.');
	EXPECT_TRUE(context, VoxelMaterialToAscii(VoxelMaterial::Glass) == 'G');
	EXPECT_TRUE(context, VoxelMaterialToAscii(VoxelMaterial::Fluid) == '~');
	EXPECT_TRUE(context, VoxelMaterialToAscii(VoxelMaterial::FloorWhite) == '#');
	EXPECT_TRUE(context, VoxelMaterialToAscii(VoxelMaterial::FloorGray) == '%');
}

void TestEmptyWorldBounds(TestContext &context)
{
	VoxelWorld world = MakeEmptyWorld(8, 4, 8);
	EXPECT_TRUE(context, !ComputeVoxelAsciiBounds(world).has_value());
	EXPECT_TRUE(context, FormatVoxelAsciiYLayer(world, 1).empty());
	EXPECT_TRUE(context, FormatVoxelAsciiYLayers(world).empty());
}

void TestAutoTightLayer(TestContext &context)
{
	VoxelWorld world = MakeEmptyWorld(8, 4, 8);
	SetVoxelMaterial(world, {2, 1, 3}, VoxelMaterial::FloorWhite, nullptr);
	SetVoxelMaterial(world, {3, 1, 3}, VoxelMaterial::Fluid, nullptr);
	SetVoxelMaterial(world, {4, 1, 3}, VoxelMaterial::Glass, nullptr);
	SetVoxelMaterial(world, {3, 1, 4}, VoxelMaterial::FloorGray, nullptr);

	const auto bounds = ComputeVoxelAsciiBounds(world);
	EXPECT_TRUE(context, bounds.has_value());
	if (!bounds.has_value()) {
		return;
	}
	EXPECT_TRUE(context, bounds->min.x == 2 && bounds->maxExclusive.x == 5);
	EXPECT_TRUE(context, bounds->min.y == 1 && bounds->maxExclusive.y == 2);
	EXPECT_TRUE(context, bounds->min.z == 3 && bounds->maxExclusive.z == 5);

	const std::string actual = FormatVoxelAsciiYLayer(world, 1);
	const std::string expected =
		"y=1 (x=2..4, z=3..4)\n"
		"z=  3: #~G\n"
		"z=  4: .%.\n";
	EXPECT_EQ_STR(context, actual, expected);
}

void TestPaddingExpandsWindow(TestContext &context)
{
	VoxelWorld world = MakeEmptyWorld(8, 4, 8);
	SetVoxelMaterial(world, {3, 1, 3}, VoxelMaterial::Fluid, nullptr);

	VoxelAsciiOptions options{};
	options.padding = 1;
	const auto bounds = ComputeVoxelAsciiBounds(world, options.padding);
	EXPECT_TRUE(context, bounds.has_value());
	if (!bounds.has_value()) {
		return;
	}
	EXPECT_TRUE(context, bounds->min.x == 2 && bounds->maxExclusive.x == 5);
	EXPECT_TRUE(context, bounds->min.z == 2 && bounds->maxExclusive.z == 5);

	const std::string actual = FormatVoxelAsciiYLayer(world, 1, std::nullopt, options);
	const std::string expected =
		"y=1 (x=2..4, z=2..4)\n"
		"z=  2: ...\n"
		"z=  3: .~.\n"
		"z=  4: ...\n";
	EXPECT_EQ_STR(context, actual, expected);
}

void TestExplicitBoundsOverride(TestContext &context)
{
	VoxelWorld world = MakeEmptyWorld(8, 4, 8);
	SetVoxelMaterial(world, {3, 1, 3}, VoxelMaterial::Fluid, nullptr);

	const VoxelAsciiBounds overrideBounds{.min = {1, 0, 1}, .maxExclusive = {6, 3, 6}};
	const std::string actual = FormatVoxelAsciiYLayer(world, 1, overrideBounds);
	const std::string expected =
		"y=1 (x=1..5, z=1..5)\n"
		"z=  1: .....\n"
		"z=  2: .....\n"
		"z=  3: ..~..\n"
		"z=  4: .....\n"
		"z=  5: .....\n";
	EXPECT_EQ_STR(context, actual, expected);
}

void TestYLayersStackHighToLow(TestContext &context)
{
	VoxelWorld world = MakeEmptyWorld(8, 4, 8);
	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::FloorWhite, nullptr);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid, nullptr);
	SetVoxelMaterial(world, {2, 2, 2}, VoxelMaterial::Glass, nullptr);

	const std::string actual = FormatVoxelAsciiYLayers(world);
	const std::string expected =
		"y=2 (x=2..2, z=2..2)\n"
		"z=  2: G\n"
		"\n"
		"y=1 (x=2..2, z=2..2)\n"
		"z=  2: ~\n"
		"\n"
		"y=0 (x=2..2, z=2..2)\n"
		"z=  2: #\n";
	EXPECT_EQ_STR(context, actual, expected);
}

void TestOutOfWorldYIsEmpty(TestContext &context)
{
	VoxelWorld world = MakeEmptyWorld(8, 4, 8);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid, nullptr);
	EXPECT_TRUE(context, FormatVoxelAsciiYLayer(world, -1).empty());
	EXPECT_TRUE(context, FormatVoxelAsciiYLayer(world, 99).empty());
}

void TestTickLoggerSkipsUnchangedTick(TestContext &context)
{
	VoxelWorld world = MakeEmptyWorld(8, 4, 8);
	SetVoxelMaterial(world, {2, 1, 2}, VoxelMaterial::Fluid, nullptr);
	VoxelAsciiTickLogger logger{};
	std::ostringstream first;
	logger.OnSimulationTickTo(world, 1, first);
	EXPECT_TRUE(context, first.str().find("# tick=1") != std::string::npos);
	EXPECT_TRUE(context, first.str().find("y=1") != std::string::npos);

	std::ostringstream second;
	logger.OnSimulationTickTo(world, 2, second);
	EXPECT_TRUE(context, second.str().empty());
}

void TestTickLoggerBottomChangeSkipsUpper(TestContext &context)
{
	VoxelWorld world = MakeEmptyWorld(8, 4, 8);
	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::FloorWhite, nullptr);
	SetVoxelMaterial(world, {2, 2, 2}, VoxelMaterial::Glass, nullptr);
	VoxelAsciiTickLogger logger{};
	std::ostringstream warm;
	logger.OnSimulationTickTo(world, 10, warm);
	EXPECT_TRUE(context, warm.str().find("y=2") != std::string::npos);
	EXPECT_TRUE(context, warm.str().find("y=0") != std::string::npos);

	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::Fluid, nullptr); // change bottom only
	std::ostringstream tick;
	logger.OnSimulationTickTo(world, 11, tick);
	const std::string out = tick.str();
	EXPECT_TRUE(context, out.find("# tick=11") != std::string::npos);
	EXPECT_TRUE(context, out.find("y=0") != std::string::npos);
	EXPECT_TRUE(context, out.find("y=2") == std::string::npos); // upper unchanged → omitted
}

void TestTickLoggerYExpandWritesNewLayerOnly(TestContext &context)
{
	VoxelWorld world = MakeEmptyWorld(8, 4, 8);
	SetVoxelMaterial(world, {2, 0, 2}, VoxelMaterial::FloorWhite, nullptr);
	VoxelAsciiTickLogger logger{};
	std::ostringstream warm;
	logger.OnSimulationTickTo(world, 20, warm);
	EXPECT_TRUE(context, warm.str().find("y=0") != std::string::npos);

	SetVoxelMaterial(world, {2, 2, 2}, VoxelMaterial::Glass, nullptr); // expand Y upward, same XZ
	std::ostringstream tick;
	logger.OnSimulationTickTo(world, 21, tick);
	const std::string out = tick.str();
	EXPECT_TRUE(context, out.find("# tick=21") != std::string::npos);
	EXPECT_TRUE(context, out.find("y=2") != std::string::npos);
	EXPECT_TRUE(context, out.find("y=0") == std::string::npos); // bottom unchanged → omitted
}

void TestTickLoggerEmptyWorldSilent(TestContext &context)
{
	VoxelWorld world = MakeEmptyWorld(8, 4, 8);
	VoxelAsciiTickLogger logger{};
	std::ostringstream out;
	logger.OnSimulationTickTo(world, 1, out);
	EXPECT_TRUE(context, out.str().empty());
}

} // namespace

int main() // NOLINT(*-exception-escape): MSVC STL stream construction may throw; terminating the test process is intended.
{
	TestContext context{};
	TestMaterialToAscii(context);
	TestEmptyWorldBounds(context);
	TestAutoTightLayer(context);
	TestPaddingExpandsWindow(context);
	TestExplicitBoundsOverride(context);
	TestYLayersStackHighToLow(context);
	TestOutOfWorldYIsEmpty(context);
	TestTickLoggerSkipsUnchangedTick(context);
	TestTickLoggerBottomChangeSkipsUpper(context);
	TestTickLoggerYExpandWritesNewLayerOnly(context);
	TestTickLoggerEmptyWorldSilent(context);

	if (context.failures != 0) {
		return EXIT_FAILURE;
	}
	std::puts("ProjectVVoxelWorldAsciiTests passed");
	return EXIT_SUCCESS;
}

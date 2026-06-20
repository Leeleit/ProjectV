#include "voxel/NanoVdb.hpp"
#include "voxel/Sparse64Tree.hpp"

#include <array>
#include <cstdio>
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

void TestStructSizes(TestContext &context)
{
	if (sizeof(projectv::voxel::nanovdb::NanoVdbUpper) != 8u) {
		std::fprintf(stderr, "Test failure at line %d: NanoVdbUpper expected 8 bytes (got %zu)\n", __LINE__, sizeof(projectv::voxel::nanovdb::NanoVdbUpper));
		++context.failures;
	}
	if (sizeof(projectv::voxel::nanovdb::NanoVdbLower) != 16u) {
		std::fprintf(stderr, "Test failure at line %d: NanoVdbLower expected 16 bytes (got %zu)\n", __LINE__, sizeof(projectv::voxel::nanovdb::NanoVdbLower));
		++context.failures;
	}
	if (sizeof(projectv::voxel::nanovdb::NanoVdbLeaf) != 24u) {
		std::fprintf(stderr, "Test failure at line %d: NanoVdbLeaf expected 24 bytes (got %zu)\n", __LINE__, sizeof(projectv::voxel::nanovdb::NanoVdbLeaf));
		++context.failures;
	}
}

void TestInvalidIndexConstant(TestContext &context)
{
	if (projectv::voxel::nanovdb::kNanoVdbInvalidIndex != 0xFFFFFFFFu) {
		context.Fail(__LINE__, "kNanoVdbInvalidIndex must be 0xFFFFFFFF");
	}
	if (projectv::voxel::nanovdb::kNanoVdbMaxLevelCount != 8u) {
		context.Fail(__LINE__, "kNanoVdbMaxLevelCount must be 8");
	}
}

void TestFlattenEmptyTree(TestContext &context)
{
	projectv::voxel::Sparse64Tree tree(8, 8, 8);
	std::array<uint8_t, 256> lookup{};
	for (uint32_t i = 0; i < 256; ++i) {
		lookup[i] = static_cast<uint8_t>(i);
	}
	projectv::voxel::nanovdb::NanoVdbFlattenResult result;
	const bool ok = projectv::voxel::nanovdb::BuildNanoVdbFlatten(tree, lookup.data(), result);
	if (!ok) {
		context.Fail(__LINE__, "BuildNanoVdbFlatten on empty tree failed");
	}
}

void TestFlattenPopulatedTree(TestContext &context)
{
	projectv::voxel::Sparse64Tree tree(8, 8, 8);
	tree.SetCell(0, 0, 0, 3u);
	tree.SetCell(7, 7, 7, 5u);
	std::array<uint8_t, 256> lookup{};
	for (uint32_t i = 0; i < 256; ++i) {
		lookup[i] = static_cast<uint8_t>(i);
	}
	projectv::voxel::nanovdb::NanoVdbFlattenResult result;
	const bool ok = projectv::voxel::nanovdb::BuildNanoVdbFlatten(tree, lookup.data(), result);
	if (!ok) {
		context.Fail(__LINE__, "BuildNanoVdbFlatten on populated tree failed");
		return;
	}
	if (result.leafCount == 0u) {
		context.Fail(__LINE__, "populated tree should have non-zero leaf count");
	}
	const uint8_t materialAtOrigin = projectv::voxel::nanovdb::ReadNanoVdbVoxelMaterial(
		result, result.rootUpperIndex, 0u, 0u, 0u);
	if (materialAtOrigin != 3u) {
		std::fprintf(stderr, "Test failure at line %d: material at (0,0,0) expected 3 (got %u)\n", __LINE__, materialAtOrigin);
		++context.failures;
	}
	const uint8_t materialAtFar = projectv::voxel::nanovdb::ReadNanoVdbVoxelMaterial(
		result, result.rootUpperIndex, 7u, 7u, 7u);
	if (materialAtFar != 5u) {
		std::fprintf(stderr, "Test failure at line %d: material at (7,7,7) expected 5 (got %u)\n", __LINE__, materialAtFar);
		++context.failures;
	}
}

}  // namespace

int main()
{
	TestContext context{};
	TestStructSizes(context);
	TestInvalidIndexConstant(context);
	TestFlattenEmptyTree(context);
	TestFlattenPopulatedTree(context);

	if (context.failures > 0) {
		std::fprintf(stderr, "ProjectVNanoVdbFlattenTests: %d failure(s)\n", context.failures);
		return 1;
	}
	std::puts("ProjectVNanoVdbFlattenTests passed");
	return 0;
}

#include "voxel/NanoVdb.hpp"
#include "voxel/Sparse64Tree.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
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

std::array<uint8_t, 256> MakeIdentityLookup()
{
	std::array<uint8_t, 256> lookup{};
	for (uint32_t i = 0; i < 256; ++i) {
		lookup[i] = static_cast<uint8_t>(i);
	}
	return lookup;
}

void TestStructAlignmentContract(TestContext &context)
{
	if (sizeof(projectv::voxel::nanovdb::NanoVdbUpper) != 8u) {
		context.Fail(__LINE__, "NanoVdbUpper must remain 8 bytes for GPU upload contract");
	}
	if (sizeof(projectv::voxel::nanovdb::NanoVdbLower) != 16u) {
		context.Fail(__LINE__, "NanoVdbLower must remain 16 bytes for GPU upload contract");
	}
	if (sizeof(projectv::voxel::nanovdb::NanoVdbLeaf) != 24u) {
		context.Fail(__LINE__, "NanoVdbLeaf must remain 24 bytes for GPU upload contract");
	}
}

void TestPackEmptyFlatten(TestContext &context)
{
	projectv::voxel::nanovdb::NanoVdbFlattenResult flatten;
	std::vector<uint8_t> upperBytes(8u, 0xAAu);
	std::vector<uint8_t> lowerBytes(16u, 0xAAu);
	std::vector<uint8_t> leafBytes(24u, 0xAAu);
	std::vector<uint8_t> materialBytes(1u, 0xAAu);

	projectv::voxel::nanovdb::PackNanoVdbFlattenData(
		flatten,
		upperBytes.data(),
		lowerBytes.data(),
		leafBytes.data(),
		materialBytes.data());

	if (upperBytes[0] != 0xAAu || upperBytes[7] != 0xAAu) {
		context.Fail(__LINE__, "Empty flatten should not write upper bytes");
	}
	if (lowerBytes[0] != 0xAAu || lowerBytes[15] != 0xAAu) {
		context.Fail(__LINE__, "Empty flatten should not write lower bytes");
	}
	if (leafBytes[0] != 0xAAu || leafBytes[23] != 0xAAu) {
		context.Fail(__LINE__, "Empty flatten should not write leaf bytes");
	}
	if (materialBytes[0] != 0xAAu) {
		context.Fail(__LINE__, "Empty flatten should not write material bytes");
	}
}

void TestPackPopulatedFlatten(TestContext &context)
{
	projectv::voxel::Sparse64Tree tree(8, 8, 8);
	tree.SetCell(0, 0, 0, 7u);
	tree.SetCell(7, 7, 7, 11u);

	projectv::voxel::nanovdb::NanoVdbFlattenResult flatten;
	const bool ok = projectv::voxel::nanovdb::BuildNanoVdbFlatten(
		tree,
		MakeIdentityLookup().data(),
		flatten);
	if (!ok) {
		context.Fail(__LINE__, "BuildNanoVdbFlatten on populated tree failed");
		return;
	}

	if (flatten.uppers.empty()) {
		context.Fail(__LINE__, "Populated tree should produce at least one upper");
		return;
	}
	if (flatten.lowers.empty()) {
		context.Fail(__LINE__, "Populated tree should produce lowers");
		return;
	}
	if (flatten.leaves.empty()) {
		context.Fail(__LINE__, "Populated tree should produce leaves");
		return;
	}
	if (flatten.materials.empty()) {
		context.Fail(__LINE__, "Populated tree should produce materials");
		return;
	}

	std::vector<uint8_t> upperBytes(flatten.uppers.size() * sizeof(projectv::voxel::nanovdb::NanoVdbUpper), 0u);
	std::vector<uint8_t> lowerBytes(flatten.lowers.size() * sizeof(projectv::voxel::nanovdb::NanoVdbLower), 0u);
	std::vector<uint8_t> leafBytes(flatten.leaves.size() * sizeof(projectv::voxel::nanovdb::NanoVdbLeaf), 0u);
	std::vector<uint8_t> materialBytes(flatten.materials.size(), 0u);

	projectv::voxel::nanovdb::PackNanoVdbFlattenData(
		flatten,
		upperBytes.data(),
		lowerBytes.data(),
		leafBytes.data(),
		materialBytes.data());

	const auto *packedUppers = reinterpret_cast<const projectv::voxel::nanovdb::NanoVdbUpper *>(upperBytes.data());
	if (packedUppers[0].childMask == 0u) {
		context.Fail(__LINE__, "First upper should have non-zero childMask after pack");
	}

	const auto *packedLeaves = reinterpret_cast<const projectv::voxel::nanovdb::NanoVdbLeaf *>(leafBytes.data());
	bool foundMaterial7 = false;
	bool foundMaterial11 = false;
	for (uint32_t leafIndex = 0u; leafIndex < flatten.leaves.size(); ++leafIndex) {
		const projectv::voxel::nanovdb::NanoVdbLeaf &srcLeaf = flatten.leaves[leafIndex];
		const projectv::voxel::nanovdb::NanoVdbLeaf &packedLeaf = packedLeaves[leafIndex];
		if (srcLeaf.homogeneousFlag != packedLeaf.homogeneousFlag ||
			srcLeaf.homogeneousMaterial != packedLeaf.homogeneousMaterial ||
			srcLeaf.firstMaterial != packedLeaf.firstMaterial ||
			srcLeaf.materialCount != packedLeaf.materialCount) {
			context.Fail(__LINE__, "Leaf byte-level mismatch after pack");
		}
		if (srcLeaf.homogeneousFlag != 0u && srcLeaf.homogeneousMaterial == 7u) {
			foundMaterial7 = true;
		}
		if (srcLeaf.homogeneousFlag != 0u && srcLeaf.homogeneousMaterial == 11u) {
			foundMaterial11 = true;
		}
	}
	if (!foundMaterial7) {
		context.Fail(__LINE__, "Material 7 should appear in flattened leaves");
	}
	if (!foundMaterial11) {
		context.Fail(__LINE__, "Material 11 should appear in flattened leaves");
	}
}

void TestVersionedUploadTrigger(TestContext &context)
{
	projectv::voxel::nanovdb::NanoVdbFlattenResult flattenA;
	flattenA.uppers.push_back({});
	flattenA.uppers[0].childMask = 1u;
	flattenA.lowers.push_back({});
	flattenA.leaves.push_back({});
	flattenA.materials.push_back(42u);

	projectv::voxel::nanovdb::NanoVdbFlattenResult flattenB = flattenA;
	flattenB.materials[0] = 99u;

	const projectv::voxel::nanovdb::NanoVdbUpper srcUpper = flattenA.uppers[0];
	std::array<uint8_t, sizeof(projectv::voxel::nanovdb::NanoVdbUpper)> upperBytes{};
	std::array<uint8_t, sizeof(projectv::voxel::nanovdb::NanoVdbLower)> lowerBytes{};
	std::array<uint8_t, sizeof(projectv::voxel::nanovdb::NanoVdbLeaf)> leafBytes{};
	std::array<uint8_t, 1> materialBytes{};

	projectv::voxel::nanovdb::PackNanoVdbFlattenData(
		flattenA,
		upperBytes.data(),
		lowerBytes.data(),
		leafBytes.data(),
		materialBytes.data());

	const auto *packedUpper = reinterpret_cast<const projectv::voxel::nanovdb::NanoVdbUpper *>(upperBytes.data());
	if (packedUpper->childMask != srcUpper.childMask) {
		context.Fail(__LINE__, "Upper childMask should round-trip through pack");
	}
	if (materialBytes[0] != 42u) {
		context.Fail(__LINE__, "Material byte 42 should round-trip through pack");
	}

	std::memset(materialBytes.data(), 0, materialBytes.size());
	projectv::voxel::nanovdb::PackNanoVdbFlattenData(
		flattenB,
		upperBytes.data(),
		lowerBytes.data(),
		leafBytes.data(),
		materialBytes.data());

	if (materialBytes[0] != 99u) {
		context.Fail(__LINE__, "Material byte 99 should round-trip through second pack");
	}
}

void TestCapacityBudgetContract(TestContext &context)
{
	const uint64_t initialUpperBytes = sizeof(projectv::voxel::nanovdb::NanoVdbUpper) * 1u;
	const uint64_t initialLowerBytes = sizeof(projectv::voxel::nanovdb::NanoVdbLower) * 64u;
	const uint64_t initialLeafBytes = sizeof(projectv::voxel::nanovdb::NanoVdbLeaf) * 64u;
	const uint64_t initialMaterialBytes = sizeof(uint8_t) * 64u;

	if (initialUpperBytes != 8u) {
		context.Fail(__LINE__, "Initial upper capacity must be 8 bytes (1 entry)");
	}
	if (initialLowerBytes != 1024u) {
		context.Fail(__LINE__, "Initial lower capacity must be 1024 bytes (64 entries)");
	}
	if (initialLeafBytes != 1536u) {
		context.Fail(__LINE__, "Initial leaf capacity must be 1536 bytes (64 entries)");
	}
	if (initialMaterialBytes != 64u) {
		context.Fail(__LINE__, "Initial material capacity must be 64 bytes (64 entries)");
	}
}

void TestComputeGrownNanoVdbCapacityZeroCurrent(TestContext &context)
{
	const uint64_t grown = projectv::voxel::nanovdb::ComputeGrownNanoVdbCapacityForTest(0u, 1024u);
	if (grown != 1024u) {
		std::fprintf(stderr, "grown=%llu expected=1024\n", static_cast<unsigned long long>(grown));
		context.Fail(__LINE__, "Zero current capacity must return required capacity (floor 1)");
	}
}

void TestComputeGrownNanoVdbCapacitySmallerRequired(TestContext &context)
{
	const uint64_t grown = projectv::voxel::nanovdb::ComputeGrownNanoVdbCapacityForTest(2048u, 1024u);
	if (grown != 2048u) {
		std::fprintf(stderr, "grown=%llu expected=2048\n", static_cast<unsigned long long>(grown));
		context.Fail(__LINE__, "Smaller required must keep current capacity");
	}
}

void TestComputeGrownNanoVdbCapacityLargerRequired(TestContext &context)
{
	const uint64_t grown = projectv::voxel::nanovdb::ComputeGrownNanoVdbCapacityForTest(1000u, 3000u);
	if (grown < 3000u) {
		std::fprintf(stderr, "grown=%llu required=3000\n", static_cast<unsigned long long>(grown));
		context.Fail(__LINE__, "Grown capacity must satisfy required capacity");
	}
	if (grown < 1500u) {
		std::fprintf(stderr, "grown=%llu expected>=1500 (current*1.5)\n", static_cast<unsigned long long>(grown));
		context.Fail(__LINE__, "Grown capacity must include 1.5x growth factor");
	}
}

}  // namespace

int main()
{
	TestContext context{};
	TestStructAlignmentContract(context);
	TestPackEmptyFlatten(context);
	TestPackPopulatedFlatten(context);
	TestVersionedUploadTrigger(context);
	TestCapacityBudgetContract(context);
	TestComputeGrownNanoVdbCapacityZeroCurrent(context);
	TestComputeGrownNanoVdbCapacitySmallerRequired(context);
	TestComputeGrownNanoVdbCapacityLargerRequired(context);

	if (context.failures > 0) {
		std::fprintf(stderr, "ProjectVNanoVdbGpuUploadTests: %d failure(s)\n", context.failures);
		return 1;
	}
	std::puts("ProjectVNanoVdbGpuUploadTests passed");
	return 0;
}

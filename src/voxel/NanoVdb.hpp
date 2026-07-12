#pragma once

#include <cstdint>
#include <vector>

#include "voxel/Sparse64Tree.hpp"

namespace projectv::voxel::nanovdb {

inline constexpr uint32_t kNanoVdbInvalidIndex = 0xFFFFFFFFu;
inline constexpr uint32_t kNanoVdbMaxLevelCount = 8u;

constexpr uint64_t ComputeGrownNanoVdbCapacityForTest(const uint64_t currentCapacityBytes, const uint64_t requiredCapacityBytes)
{
	if (currentCapacityBytes == 0u) {
		return requiredCapacityBytes == 0u ? 1u : requiredCapacityBytes;
	}
	if (requiredCapacityBytes <= currentCapacityBytes) {
		return currentCapacityBytes;
	}
	const uint64_t grown = currentCapacityBytes + currentCapacityBytes / 2u;
	return grown > requiredCapacityBytes ? grown : requiredCapacityBytes;
}

struct NanoVdbUpper {
	uint32_t childMask = 0u;
	uint32_t firstLower = kNanoVdbInvalidIndex;
};
static_assert(sizeof(NanoVdbUpper) == 8);
static_assert(std::is_standard_layout_v<NanoVdbUpper>);
static_assert(std::is_trivially_copyable_v<NanoVdbUpper>);

struct NanoVdbLower {
	uint64_t childMask = 0u;
	uint32_t firstLeaf = kNanoVdbInvalidIndex;
	uint32_t valueMask32 = 0u;
};
static_assert(sizeof(NanoVdbLower) == 16);
static_assert(std::is_standard_layout_v<NanoVdbLower>);
static_assert(std::is_trivially_copyable_v<NanoVdbLower>);

struct NanoVdbLeaf {
	uint64_t valueMask = 0u;
	uint32_t firstMaterial = kNanoVdbInvalidIndex;
	uint16_t materialCount = 0u;
	uint16_t homogeneousMaterial = 0u;
	uint32_t homogeneousFlag = 0u;
	uint32_t reserved = 0u;
};
static_assert(sizeof(NanoVdbLeaf) == 24);
static_assert(std::is_standard_layout_v<NanoVdbLeaf>);
static_assert(std::is_trivially_copyable_v<NanoVdbLeaf>);

struct NanoVdbFlattenResult {
	std::vector<NanoVdbUpper> uppers;
	std::vector<NanoVdbLower> lowers;
	std::vector<NanoVdbLeaf> leaves;
	std::vector<uint8_t> materials;
	uint32_t upperCount = 0u;
	uint32_t lowerCount = 0u;
	uint32_t leafCount = 0u;
	uint32_t materialCount = 0u;
	uint32_t rootUpperIndex = 0u;
	int rootLevelDepth = 0;
};

bool BuildNanoVdbFlatten(
	const Sparse64Tree &tree,
	const uint8_t *materialLookup,
	NanoVdbFlattenResult &outResult);

uint8_t ReadNanoVdbVoxelMaterial(
	const NanoVdbFlattenResult &result,
	uint32_t rootUpperIndex,
	uint32_t localX,
	uint32_t localY,
	uint32_t localZ);

inline void PackNanoVdbFlattenData(
	const NanoVdbFlattenResult &flatten,
	void *upperDst,
	void *lowerDst,
	void *leafDst,
	void *materialDst)
{
	if (!flatten.uppers.empty() && upperDst != nullptr) {
		std::memcpy(
			upperDst,
			flatten.uppers.data(),
			flatten.uppers.size() * sizeof(NanoVdbUpper));
	}
	if (!flatten.lowers.empty() && lowerDst != nullptr) {
		std::memcpy(
			lowerDst,
			flatten.lowers.data(),
			flatten.lowers.size() * sizeof(NanoVdbLower));
	}
	if (!flatten.leaves.empty() && leafDst != nullptr) {
		std::memcpy(
			leafDst,
			flatten.leaves.data(),
			flatten.leaves.size() * sizeof(NanoVdbLeaf));
	}
	if (!flatten.materials.empty() && materialDst != nullptr) {
		std::memcpy(
			materialDst,
			flatten.materials.data(),
			flatten.materials.size() * sizeof(uint8_t));
	}
}

} // namespace projectv::voxel::nanovdb
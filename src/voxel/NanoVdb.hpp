#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "voxel/Sparse64Tree.hpp"

namespace projectv::voxel::nanovdb {

inline constexpr uint32_t kNanoVdbInvalidIndex = 0xFFFFFFFFu;
inline constexpr uint32_t kNanoVdbMaxLevelCount = 8u;

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

}  // namespace projectv::voxel::nanovdb
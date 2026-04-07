#ifndef VOXEL_MATERIALS_HPP
#define VOXEL_MATERIALS_HPP

#include "voxel/VoxelWorld.hpp"

#include <array>
#include <cstddef>
#include <type_traits>

struct VoxelMaterialVisual {
	std::array<float, 4> baseColor{};
	float ambient = 0.3f;
	float diffuse = 0.7f;
	float specular = 0.0f;
	float specularPower = 1.0f;
};
static_assert(std::is_standard_layout_v<VoxelMaterialVisual>);
static_assert(std::is_trivially_copyable_v<VoxelMaterialVisual>);
static_assert(sizeof(VoxelMaterialVisual) == 32);
static_assert(offsetof(VoxelMaterialVisual, baseColor) == 0);
static_assert(offsetof(VoxelMaterialVisual, ambient) == 16);
static_assert(offsetof(VoxelMaterialVisual, diffuse) == 20);
static_assert(offsetof(VoxelMaterialVisual, specular) == 24);
static_assert(offsetof(VoxelMaterialVisual, specularPower) == 28);

constexpr size_t kVoxelMaterialCount = 5;

VoxelMaterialVisual GetVoxelMaterialVisual(VoxelMaterial material);

#endif

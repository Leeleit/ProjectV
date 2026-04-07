#ifndef VOXEL_MATERIALS_HPP
#define VOXEL_MATERIALS_HPP

#include "voxel/VoxelWorld.hpp"

#include <array>
#include <cstddef>
#include <type_traits>

struct VoxelMaterialVisual {
	std::array<float, 4> baseColor{};
	std::array<float, 4> lighting{};
	std::array<float, 4> edgeTintAndPower{};
	std::array<float, 4> shadingExtras{};
};
static_assert(std::is_standard_layout_v<VoxelMaterialVisual>);
static_assert(std::is_trivially_copyable_v<VoxelMaterialVisual>);
static_assert(sizeof(VoxelMaterialVisual) == 64);
static_assert(offsetof(VoxelMaterialVisual, baseColor) == 0);
static_assert(offsetof(VoxelMaterialVisual, lighting) == 16);
static_assert(offsetof(VoxelMaterialVisual, edgeTintAndPower) == 32);
static_assert(offsetof(VoxelMaterialVisual, shadingExtras) == 48);

struct VoxelSceneLighting {
	std::array<float, 4> skyColorAndFogDensity{};
	std::array<float, 4> horizonColorAndFogStart{};
	std::array<float, 4> groundColorAndFogMax{};
	std::array<float, 4> sunColorAndIntensity{};
	std::array<float, 4> sunDirectionAndWrap{};
};
static_assert(std::is_standard_layout_v<VoxelSceneLighting>);
static_assert(std::is_trivially_copyable_v<VoxelSceneLighting>);
static_assert(sizeof(VoxelSceneLighting) == 80);
static_assert(offsetof(VoxelSceneLighting, skyColorAndFogDensity) == 0);
static_assert(offsetof(VoxelSceneLighting, horizonColorAndFogStart) == 16);
static_assert(offsetof(VoxelSceneLighting, groundColorAndFogMax) == 32);
static_assert(offsetof(VoxelSceneLighting, sunColorAndIntensity) == 48);
static_assert(offsetof(VoxelSceneLighting, sunDirectionAndWrap) == 64);

constexpr size_t kVoxelMaterialCount = 5;

VoxelMaterialVisual GetVoxelMaterialVisual(VoxelMaterial material);
VoxelSceneLighting GetVoxelSceneLighting(VoxelScenePreset preset);

#endif

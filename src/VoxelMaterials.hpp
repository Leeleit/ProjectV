#ifndef VOXEL_MATERIALS_HPP
#define VOXEL_MATERIALS_HPP

#include "VoxelWorld.hpp"

#include <array>

struct VoxelMaterialVisual {
	std::array<float, 4> baseColor{};
	float ambient = 0.3f;
	float diffuse = 0.7f;
	float specular = 0.0f;
	float specularPower = 1.0f;
};

VoxelMaterialVisual GetVoxelMaterialVisual(VoxelMaterial material);

#endif

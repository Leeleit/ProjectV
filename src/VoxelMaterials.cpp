#include "VoxelMaterials.hpp"

VoxelMaterialVisual GetVoxelMaterialVisual(const VoxelMaterial material)
{
	switch (material) {
	case VoxelMaterial::Glass:
		return {
			.baseColor = {0.94f, 0.97f, 1.00f, 0.32f},
			.ambient = 0.55f,
			.diffuse = 0.35f,
			.specular = 0.65f,
			.specularPower = 24.0f,
		};
	case VoxelMaterial::Fluid:
		return {
			.baseColor = {0.02f, 0.62f, 1.00f, 1.0f},
			.ambient = 0.40f,
			.diffuse = 0.60f,
			.specular = 0.20f,
			.specularPower = 10.0f,
		};
	case VoxelMaterial::FloorWhite:
		return {
			.baseColor = {1.00f, 1.00f, 1.00f, 1.0f},
			.ambient = 0.35f,
			.diffuse = 0.65f,
			.specular = 0.05f,
			.specularPower = 8.0f,
		};
	case VoxelMaterial::FloorGray:
		return {
			.baseColor = {0.78f, 0.80f, 0.82f, 1.0f},
			.ambient = 0.32f,
			.diffuse = 0.68f,
			.specular = 0.03f,
			.specularPower = 8.0f,
		};
	case VoxelMaterial::Air:
	default:
		return {};
	}
}

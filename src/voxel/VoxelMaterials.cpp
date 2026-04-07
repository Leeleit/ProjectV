#include "voxel/VoxelMaterials.hpp"

VoxelMaterialVisual GetVoxelMaterialVisual(const VoxelMaterial material)
{
	switch (material) {
	case VoxelMaterial::Glass:
		return {
			.baseColor = {0.82f, 0.93f, 1.00f, 0.24f},
			.lighting = {0.18f, 0.24f, 1.10f, 72.0f},
			.edgeTintAndPower = {0.76f, 0.90f, 1.00f, 5.5f},
			.shadingExtras = {0.92f, 0.62f, 0.55f, 0.00f},
		};
	case VoxelMaterial::Fluid:
		return {
			.baseColor = {0.08f, 0.43f, 0.92f, 1.0f},
			.lighting = {0.30f, 0.78f, 0.40f, 28.0f},
			.edgeTintAndPower = {0.10f, 0.84f, 1.00f, 4.0f},
			.shadingExtras = {0.48f, 0.20f, 0.85f, 0.10f},
		};
	case VoxelMaterial::FloorWhite:
		return {
			.baseColor = {0.96f, 0.96f, 0.94f, 1.0f},
			.lighting = {0.28f, 0.82f, 0.08f, 18.0f},
			.edgeTintAndPower = {0.00f, 0.00f, 0.00f, 1.0f},
			.shadingExtras = {0.00f, 0.00f, 1.00f, 0.00f},
		};
	case VoxelMaterial::FloorGray:
		return {
			.baseColor = {0.56f, 0.60f, 0.66f, 1.0f},
			.lighting = {0.24f, 0.78f, 0.10f, 22.0f},
			.edgeTintAndPower = {0.00f, 0.00f, 0.00f, 1.0f},
			.shadingExtras = {0.00f, 0.00f, 1.00f, 0.00f},
		};
	case VoxelMaterial::Air:
	default:
		return {};
	}
}

VoxelSceneLighting GetVoxelSceneLighting(const VoxelScenePreset preset)
{
	switch (preset) {
	case VoxelScenePreset::FlatBenchmark:
		return {
			.skyColorAndFogDensity = {0.78f, 0.82f, 0.86f, 0.010f},
			.horizonColorAndFogStart = {0.74f, 0.78f, 0.82f, 28.0f},
			.groundColorAndFogMax = {0.30f, 0.31f, 0.34f, 0.22f},
			.sunColorAndIntensity = {0.98f, 0.99f, 1.00f, 0.55f},
			.sunDirectionAndWrap = {-0.20f, 0.95f, -0.24f, 0.55f},
		};
	case VoxelScenePreset::TransparencyStress:
		return {
			.skyColorAndFogDensity = {0.35f, 0.62f, 0.92f, 0.016f},
			.horizonColorAndFogStart = {0.59f, 0.81f, 1.00f, 18.0f},
			.groundColorAndFogMax = {0.08f, 0.11f, 0.16f, 0.38f},
			.sunColorAndIntensity = {1.00f, 0.93f, 0.84f, 1.30f},
			.sunDirectionAndWrap = {-0.58f, 0.62f, -0.31f, 0.20f},
		};
	case VoxelScenePreset::ChunkGrid:
		return {
			.skyColorAndFogDensity = {0.92f, 0.74f, 0.56f, 0.012f},
			.horizonColorAndFogStart = {1.00f, 0.84f, 0.66f, 16.0f},
			.groundColorAndFogMax = {0.24f, 0.18f, 0.14f, 0.28f},
			.sunColorAndIntensity = {1.00f, 0.74f, 0.46f, 1.15f},
			.sunDirectionAndWrap = {-0.15f, 0.72f, -0.68f, 0.24f},
		};
	case VoxelScenePreset::MeshingStress:
		return {
			.skyColorAndFogDensity = {0.64f, 0.72f, 0.80f, 0.020f},
			.horizonColorAndFogStart = {0.86f, 0.78f, 0.70f, 14.0f},
			.groundColorAndFogMax = {0.14f, 0.16f, 0.18f, 0.40f},
			.sunColorAndIntensity = {1.00f, 0.88f, 0.76f, 1.45f},
			.sunDirectionAndWrap = {-0.70f, 0.48f, -0.18f, 0.18f},
		};
	case VoxelScenePreset::VoxelLab:
	default:
		return {
			.skyColorAndFogDensity = {0.70f, 0.83f, 0.97f, 0.014f},
			.horizonColorAndFogStart = {0.84f, 0.91f, 1.00f, 22.0f},
			.groundColorAndFogMax = {0.22f, 0.25f, 0.30f, 0.32f},
			.sunColorAndIntensity = {1.00f, 0.96f, 0.90f, 0.95f},
			.sunDirectionAndWrap = {-0.35f, 0.80f, -0.45f, 0.30f},
		};
	}
}

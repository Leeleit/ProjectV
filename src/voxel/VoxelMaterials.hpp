#ifndef VOXEL_MATERIALS_HPP
#define VOXEL_MATERIALS_HPP

#include "voxel/VoxelWorld.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

enum class ToneMapOperator : uint8_t {
	Linear = 0,
	Reinhard,
	AcesApprox,
};

enum class LightingDebugView : uint8_t {
	Final = 0,
	Ambient,
	Direct,
	Shadow,
	Fog,
};

enum class ShadowTuningTarget : uint8_t {
	Strength = 0,
	DepthBias,
	NormalBias,
	FilterRadius,
	Coverage,
};

struct VoxelLightingDebugControls {
	float exposureBiasStops = 0.0f;
	ToneMapOperator toneMapOperator = ToneMapOperator::AcesApprox;
	LightingDebugView debugView = LightingDebugView::Final;
	ShadowTuningTarget shadowTuningTarget = ShadowTuningTarget::Strength;
	float shadowStrengthOffset = 0.0f;
	float shadowDepthBiasOffset = 0.0f;
	float shadowNormalBiasOffset = 0.0f;
	float shadowFilterRadiusOffset = 0.0f;
	float shadowCoverageScale = 1.0f;
};
static_assert(std::is_standard_layout_v<VoxelLightingDebugControls>);
static_assert(std::is_trivially_copyable_v<VoxelLightingDebugControls>);

struct VoxelMaterialVisual {
	std::array<float, 4> baseColor{};
	// ambient occlusion, roughness, metallic, reflectance
	std::array<float, 4> surface{};
	// medium/scattering tint rgb, transmission
	std::array<float, 4> medium{};
	// fog factor, emissive strength, ambient response, direct diffuse response
	std::array<float, 4> shading{};
};
static_assert(std::is_standard_layout_v<VoxelMaterialVisual>);
static_assert(std::is_trivially_copyable_v<VoxelMaterialVisual>);
static_assert(sizeof(VoxelMaterialVisual) == 64);
static_assert(offsetof(VoxelMaterialVisual, baseColor) == 0);
static_assert(offsetof(VoxelMaterialVisual, surface) == 16);
static_assert(offsetof(VoxelMaterialVisual, medium) == 32);
static_assert(offsetof(VoxelMaterialVisual, shading) == 48);

struct VoxelSceneLighting {
	std::array<float, 4> skyColorAndFogDensity{};
	std::array<float, 4> horizonColorAndFogStart{};
	std::array<float, 4> groundColorAndFogMax{};
	std::array<float, 4> sunColorAndIntensity{};
	std::array<float, 4> sunDirectionAndWrap{};
	std::array<float, 4> postProcess{};
	std::array<float, 4> sunShadowParams{};
	std::array<float, 16> sunShadowViewProjection{};
};
static_assert(std::is_standard_layout_v<VoxelSceneLighting>);
static_assert(std::is_trivially_copyable_v<VoxelSceneLighting>);
static_assert(sizeof(VoxelSceneLighting) == 176);
static_assert(offsetof(VoxelSceneLighting, skyColorAndFogDensity) == 0);
static_assert(offsetof(VoxelSceneLighting, horizonColorAndFogStart) == 16);
static_assert(offsetof(VoxelSceneLighting, groundColorAndFogMax) == 32);
static_assert(offsetof(VoxelSceneLighting, sunColorAndIntensity) == 48);
static_assert(offsetof(VoxelSceneLighting, sunDirectionAndWrap) == 64);
static_assert(offsetof(VoxelSceneLighting, postProcess) == 80);
static_assert(offsetof(VoxelSceneLighting, sunShadowParams) == 96);
static_assert(offsetof(VoxelSceneLighting, sunShadowViewProjection) == 112);

constexpr size_t kVoxelMaterialCount = 5;

VoxelMaterialVisual GetVoxelMaterialVisual(VoxelMaterial material);
VoxelSceneLighting GetVoxelSceneLighting(VoxelScenePreset preset);
VoxelSceneLighting BuildVoxelSceneLighting(
	VoxelScenePreset preset,
	const VoxelLightingDebugControls &controls);
std::array<float, 4> GetVoxelSceneClearColor(const VoxelSceneLighting &lighting);
const char *ToneMapOperatorToString(ToneMapOperator toneMapOperator);
const char *LightingDebugViewToString(LightingDebugView debugView);
const char *ShadowTuningTargetToString(ShadowTuningTarget target);
ToneMapOperator GetNextToneMapOperator(ToneMapOperator toneMapOperator);
LightingDebugView GetNextLightingDebugView(LightingDebugView debugView);
ShadowTuningTarget GetNextShadowTuningTarget(ShadowTuningTarget target);

#endif

#pragma once

import projectv.math;
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

enum class ExposureMeteringMode : uint8_t {
	Manual = 0,
	SceneKey,
};

enum class LightingDebugView : uint8_t {
	Final = 0,
	Ambient = 1,
	Direct = 2,
	Local = 3,
	Shadow = 4,
	Contact = 5,
	Occlusion = 6,
	Fog = 7,
	DiffuseGI = 8,
	SpecularGI = 9,
	RtxSpecular = 10,
	VolumetricFog = 11,
	VolumetricTransmittance = 12,
	GreedyMeshing = 13,
};

struct VoxelLightingDebugControls {
	float exposureBiasStops = 0.0f;
	ToneMapOperator toneMapOperator = ToneMapOperator::AcesApprox;
	LightingDebugView debugView = LightingDebugView::Final;
};
static_assert(std::is_standard_layout_v<VoxelLightingDebugControls>);
static_assert(std::is_trivially_copyable_v<VoxelLightingDebugControls>);

struct VoxelMaterialVisual {
	std::array<float, 4> baseColor{};
	std::array<float, 4> surface{};
	std::array<float, 4> medium{};
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
	std::array<float, 4> sunContactShadowParams{};
	std::array<float, 4> ambientOcclusionParams{};
	std::array<float, 4> colorGrading{};
	std::array<float, 4> exposureControl{};
	std::array<float, 4> localPointLightPositionAndRadius{};
	std::array<float, 4> localPointLightColorAndIntensity{};
	std::array<float, 4> localPointLightParams{};

	std::array<float, 4> vctParams{};
	std::array<float, 4> vctSpecularParams{};
};
static_assert(std::is_standard_layout_v<VoxelSceneLighting>);
static_assert(std::is_trivially_copyable_v<VoxelSceneLighting>);
static_assert(sizeof(VoxelSceneLighting) == 240);
static_assert(offsetof(VoxelSceneLighting, skyColorAndFogDensity) == 0);
static_assert(offsetof(VoxelSceneLighting, horizonColorAndFogStart) == 16);
static_assert(offsetof(VoxelSceneLighting, groundColorAndFogMax) == 32);
static_assert(offsetof(VoxelSceneLighting, sunColorAndIntensity) == 48);
static_assert(offsetof(VoxelSceneLighting, sunDirectionAndWrap) == 64);
static_assert(offsetof(VoxelSceneLighting, postProcess) == 80);
static_assert(offsetof(VoxelSceneLighting, sunContactShadowParams) == 96);
static_assert(offsetof(VoxelSceneLighting, ambientOcclusionParams) == 112);
static_assert(offsetof(VoxelSceneLighting, colorGrading) == 128);
static_assert(offsetof(VoxelSceneLighting, exposureControl) == 144);
static_assert(offsetof(VoxelSceneLighting, localPointLightPositionAndRadius) == 160);
static_assert(offsetof(VoxelSceneLighting, localPointLightColorAndIntensity) == 176);
static_assert(offsetof(VoxelSceneLighting, localPointLightParams) == 192);
static_assert(offsetof(VoxelSceneLighting, vctParams) == 208);
static_assert(offsetof(VoxelSceneLighting, vctSpecularParams) == 224);

constexpr size_t kVoxelMaterialCount = 5;

VoxelMaterialVisual GetVoxelMaterialVisual(VoxelMaterial material);
VoxelSceneLighting GetVoxelSceneLighting(VoxelScenePreset preset);
VoxelSceneLighting BuildVoxelSceneLighting(
	VoxelScenePreset preset,
	const VoxelLightingDebugControls &controls);
std::array<float, 4> GetVoxelSceneClearColor(const VoxelSceneLighting &lighting);
float EstimateVoxelSceneExposureKey(const VoxelSceneLighting &lighting);
const char *ToneMapOperatorToString(ToneMapOperator toneMapOperator);
const char *ExposureMeteringModeToString(ExposureMeteringMode meteringMode);
const char *LightingDebugViewToString(LightingDebugView debugView);
ToneMapOperator GetNextToneMapOperator(ToneMapOperator toneMapOperator);
LightingDebugView GetNextLightingDebugView(LightingDebugView debugView);


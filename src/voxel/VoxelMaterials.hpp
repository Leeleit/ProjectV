#pragma once

import projectv.math;
#include "render/ShadowTypes.hpp"
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
	Ambient,
	Direct,
	Local,
	Shadow,
	Cascade,
	Contact,
	Occlusion,
	Fog,
	Taa,
};

enum class ShadowTuningTarget : uint8_t {
	Strength = 0,
	DepthBias,
	NormalBias,
	FilterRadius,
	Coverage,
	CascadeBlend,
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
	float shadowCascadeBlendOffset = 0.0f;
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
	std::array<float, 4> sunShadowParams{};
	std::array<float, 4> sunContactShadowParams{};
	std::array<float, 4> ambientOcclusionParams{};
	std::array<float, kSunShadowMatrixElementCount> sunShadowViewProjections{};
	std::array<float, 4> colorGrading{};
	std::array<float, 4> exposureControl{};
	std::array<float, 4> shadowCascadeDepthSplits{};
	std::array<float, 4> shadowCascadeBlendParams{};
	std::array<float, 4> localPointLightPositionAndRadius{};
	std::array<float, 4> localPointLightColorAndIntensity{};
	std::array<float, 4> localPointLightParams{};

	std::array<float, 4> taaParams{};
	projectv::math::Mat4 prevViewProjectionMatrix{};
	std::array<float, 4> taaHistoryParams{};

	std::array<float, 4> taaLayerHistoryParams{};
	std::array<float, 4> vctParams{};
	std::array<float, 4> vctSpecularParams{};
};
static_assert(std::is_standard_layout_v<VoxelSceneLighting>);
static_assert(std::is_trivially_copyable_v<VoxelSceneLighting>);
static_assert(sizeof(VoxelSceneLighting) == 656);
static_assert(offsetof(VoxelSceneLighting, skyColorAndFogDensity) == 0);
static_assert(offsetof(VoxelSceneLighting, horizonColorAndFogStart) == 16);
static_assert(offsetof(VoxelSceneLighting, groundColorAndFogMax) == 32);
static_assert(offsetof(VoxelSceneLighting, sunColorAndIntensity) == 48);
static_assert(offsetof(VoxelSceneLighting, sunDirectionAndWrap) == 64);
static_assert(offsetof(VoxelSceneLighting, postProcess) == 80);
static_assert(offsetof(VoxelSceneLighting, sunShadowParams) == 96);
static_assert(offsetof(VoxelSceneLighting, sunContactShadowParams) == 112);
static_assert(offsetof(VoxelSceneLighting, ambientOcclusionParams) == 128);
static_assert(offsetof(VoxelSceneLighting, sunShadowViewProjections) == 144);
static_assert(offsetof(VoxelSceneLighting, colorGrading) == 400);
static_assert(offsetof(VoxelSceneLighting, exposureControl) == 416);
static_assert(offsetof(VoxelSceneLighting, shadowCascadeDepthSplits) == 432);
static_assert(offsetof(VoxelSceneLighting, shadowCascadeBlendParams) == 448);
static_assert(offsetof(VoxelSceneLighting, localPointLightPositionAndRadius) == 464);
static_assert(offsetof(VoxelSceneLighting, localPointLightColorAndIntensity) == 480);
static_assert(offsetof(VoxelSceneLighting, localPointLightParams) == 496);
static_assert(offsetof(VoxelSceneLighting, taaParams) == 512);
static_assert(offsetof(VoxelSceneLighting, prevViewProjectionMatrix) == 528);
static_assert(offsetof(VoxelSceneLighting, taaHistoryParams) == 592);
static_assert(offsetof(VoxelSceneLighting, taaLayerHistoryParams) == 608);
static_assert(offsetof(VoxelSceneLighting, vctParams) == 624);
static_assert(offsetof(VoxelSceneLighting, vctSpecularParams) == 640);

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
const char *ShadowTuningTargetToString(ShadowTuningTarget target);
ToneMapOperator GetNextToneMapOperator(ToneMapOperator toneMapOperator);
LightingDebugView GetNextLightingDebugView(LightingDebugView debugView);
ShadowTuningTarget GetNextShadowTuningTarget(ShadowTuningTarget target);


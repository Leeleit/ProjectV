#ifndef VOXEL_MATERIALS_HPP
#define VOXEL_MATERIALS_HPP

#include "core/Math.hpp"
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
	// exposure, environment diffuse intensity, tone-map operator, lighting debug view
	std::array<float, 4> postProcess{};
	std::array<float, 4> sunShadowParams{};
	// contact shadow strength, max distance, reserved, reserved
	std::array<float, 4> sunContactShadowParams{};
	// ambient occlusion strength, radius, minimum visibility, reserved
	std::array<float, 4> ambientOcclusionParams{};
	std::array<float, kSunShadowMatrixElementCount> sunShadowViewProjections{};
	// white point, contrast, saturation, lift
	std::array<float, 4> colorGrading{};
	// metering mode, target scene key, minimum exposure, maximum exposure
	std::array<float, 4> exposureControl{};
	std::array<float, 4> shadowCascadeDepthSplits{};
	// cascade blend fraction, first cascade near plane, reserved, reserved
	std::array<float, 4> shadowCascadeBlendParams{};
	// position xyz, radius
	std::array<float, 4> localPointLightPositionAndRadius{};
	// color rgb, intensity
	std::array<float, 4> localPointLightColorAndIntensity{};
	// enabled, source radius, shadow strength, shadow bias
	std::array<float, 4> localPointLightParams{};
	// TAA (Temporal Anti-Aliasing) contract. Layout byte-for-byte matches
	// the three shader-side `SceneLightingBuffer` declarations
	// (`voxel.frag`, `voxel_shadow.vert`, `voxel_mesh.comp`); see
	// `agent/decisions.md` §18 — `taaEnabled` is the runtime gate
	// (1.0 = on, 0.0 = off), `taaBlend` is the history blend factor
	// (lower = more ghosting, higher = less stable). `taaJitterX/Y` carry
	// the current-frame sub-pixel Halton offset in NDC. `prevViewProjectionMatrix`
	// is the previous frame's viewProjection, used by the TAA resolve pass to
	// reproject the current pixel into the history buffer. `taaHistoryParams`
	// exposes texel size + a one-frame validity flag set to 0 after
	// resize / world reload / preset change / pause.
	std::array<float, 4> taaParams{};
	// previous frame's viewProjection (column-major, same layout as
	// `GraphicsPushConstants::viewProjection`). Reused by the TAA resolve
	// pass for depth-based reprojection; zeroed on the first frame.
	// **Tier 0.B (`2026-06-13`).** `Mat4` (16-byte aligned) replaces
	// `std::array<float, 16>`. Same byte size (64 B), the
	// `static_assert(offsetof(VoxelSceneLighting, prevViewProjectionMatrix) == 528)`
	// below still holds because `Mat4` and `std::array<float, 16>` have
	// the same size and alignment; the column-major field order
	// (`m[col*4 + row]`) matches the std430 GLSL `mat4` layout that the
	// shader uses.
	projectv::math::Mat4 prevViewProjectionMatrix{};
	// texel size x, texel size y, history valid (0/1), neighbourhood radius (1/3/5/7)
	std::array<float, 4> taaHistoryParams{};
	// 1.5 anti-flicker: per-layer temporal history parameters.
	// texel size x, texel size y, history valid (0/1), blend factor
	// (default 0.4 — `final = mix(raw, history, blend)`). Mirrors the
	// `taaHistoryParams` layout so the shader contract stays
	// predictable. Byte layout preserved as 16 B `vec4` after
	// `taaHistoryParams`; the new `static_assert` block below
	// confirms.
	std::array<float, 4> taaLayerHistoryParams{};
};
static_assert(std::is_standard_layout_v<VoxelSceneLighting>);
static_assert(std::is_trivially_copyable_v<VoxelSceneLighting>);
static_assert(sizeof(VoxelSceneLighting) == 624);
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

#endif

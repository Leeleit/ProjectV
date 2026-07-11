#include "voxel/VoxelMaterials.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr float kMinSceneExposure = 0.05f;		   // EVIL: 1/200s effective minimum shutter; clamps very dim scenes, do not retune casually
constexpr float kDefaultMaxSceneExposure = 4.0f;   // EVIL: 4x max over-exposure; prevents HDR scene-key blowout, tuned to VoxelLab
constexpr float kMinSceneKey = 0.001f;			   // EVIL: 1/1000 lower bound on authored scene key; 0 would cause log-domain div-by-zero downstream
constexpr float kMinExposureTargetKey = 0.05f;	   // EVIL: matched to kMinSceneExposure; both floors must stay symmetric to avoid negative EV comp
constexpr float kMaxExposureTargetKey = 4.0f;	   // EVIL: matched to kDefaultMaxSceneExposure; ceiling is intentional artistic headroom
constexpr float kMinEnvironmentIntensity = 0.0f;   // EVIL: 0 = pure ambient sky/horizon/ground fill; -ε would invert normal response
constexpr float kMaxEnvironmentIntensity = 2.0f;   // EVIL: 2x boost; >2x destabilizes VoxelLab ambient occlusion weighting
constexpr float kMinColorGradeWhitePoint = 0.25f;  // EVIL: white-point floor prevents pitch-black highlights on bright scenes
constexpr float kMaxColorGradeWhitePoint = 4.0f;   // EVIL: 4x max white-point; >4x blows past displayable range even after tone-map
constexpr float kMinColorGradeContrast = 0.0f;	   // EVIL: 0 = identity contrast (no S-curve); -ε inverts highlights/shadows unpredictably
constexpr float kMaxColorGradeContrast = 2.0f;	   // EVIL: 2x max S-curve; >2x creates posterization artifacts in mid-tones
constexpr float kMinColorGradeSaturation = 0.0f;   // EVIL: 0 = full desaturation (monochrome); -ε would invert chroma
constexpr float kMaxColorGradeSaturation = 2.0f;   // EVIL: 2x max boost; >2x overshoots into neon, breaks VoxelLab material identity
constexpr float kMinColorGradeLift = -0.25f;	   // EVIL: -0.25 lift floor; more negative crushes blacks into noise
constexpr float kMaxColorGradeLift = 0.25f;		   // EVIL: +0.25 lift ceiling; more positive washes out highlights
constexpr float kMaxContactShadowDistance = 12.0f; // EVIL: 12-voxel ray distance for contact shadow; longer bleeds across chunk boundaries
constexpr float kMaxAmbientOcclusionRadius = 6.0f;
constexpr float kMaxLocalPointLightRadius = 96.0f;
constexpr float kMaxLocalPointLightColor = 4.0f;
constexpr float kMaxLocalPointLightIntensity = 128.0f;
constexpr float kMaxLocalPointLightSourceRadius = 6.0f;
constexpr float kMaxLocalPointLightShadowBias = 0.25f;

float ComputeLuminance(const std::array<float, 4> &color)
{
	return color[0] * 0.2126f + color[1] * 0.7152f + color[2] * 0.0722f;
}

std::array<float, 3> ApplyToneMap(
	const std::array<float, 3> &linearColor,
	const ToneMapOperator toneMapOperator)
{
	switch (toneMapOperator) {
	case ToneMapOperator::Linear:
		return {
			std::clamp(linearColor[0], 0.0f, 1.0f),
			std::clamp(linearColor[1], 0.0f, 1.0f),
			std::clamp(linearColor[2], 0.0f, 1.0f),
		};
	case ToneMapOperator::Reinhard:
		return {
			linearColor[0] / (1.0f + std::max(linearColor[0], 0.0f)),
			linearColor[1] / (1.0f + std::max(linearColor[1], 0.0f)),
			linearColor[2] / (1.0f + std::max(linearColor[2], 0.0f)),
		};
	case ToneMapOperator::AcesApprox:
	default: {
		const auto applyAcesChannel = [](const float channel) {
			const float linear = std::max(channel, 0.0f);
			// EVIL: Narkowicz ACES filmic tone-map approximation constants (industry-standard, no need to retune)
			const float mapped =
				linear * (2.51f * linear + 0.03f) /
				(linear * (2.43f * linear + 0.59f) + 0.14f);
			return std::clamp(mapped, 0.0f, 1.0f);
		};
		return {
			applyAcesChannel(linearColor[0]),
			applyAcesChannel(linearColor[1]),
			applyAcesChannel(linearColor[2]),
		};
	}
	}
}

std::array<float, 3> ApplyColorGrading(
	const std::array<float, 3> &mappedColor,
	const std::array<float, 4> &colorGrading)
{
	const float whitePoint =
		std::clamp(colorGrading[0], kMinColorGradeWhitePoint, kMaxColorGradeWhitePoint);
	const float contrast =
		std::clamp(colorGrading[1], kMinColorGradeContrast, kMaxColorGradeContrast);
	const float saturation =
		std::clamp(colorGrading[2], kMinColorGradeSaturation, kMaxColorGradeSaturation);
	const float lift =
		std::clamp(colorGrading[3], kMinColorGradeLift, kMaxColorGradeLift);
	const std::array normalizedColor{
		mappedColor[0] / whitePoint,
		mappedColor[1] / whitePoint,
		mappedColor[2] / whitePoint,
	};
	const float luma =
		normalizedColor[0] * 0.2126f +
		normalizedColor[1] * 0.7152f +
		normalizedColor[2] * 0.0722f;
	return {
		std::clamp((luma + (normalizedColor[0] - luma) * saturation - 0.5f) * contrast + 0.5f + lift, 0.0f, 1.0f),
		std::clamp((luma + (normalizedColor[1] - luma) * saturation - 0.5f) * contrast + 0.5f + lift, 0.0f, 1.0f),
		std::clamp((luma + (normalizedColor[2] - luma) * saturation - 0.5f) * contrast + 0.5f + lift, 0.0f, 1.0f),
	};
}

std::array<float, 4> ClampColorGrading(const std::array<float, 4> &colorGrading)
{
	return {
		std::clamp(colorGrading[0], kMinColorGradeWhitePoint, kMaxColorGradeWhitePoint),
		std::clamp(colorGrading[1], kMinColorGradeContrast, kMaxColorGradeContrast),
		std::clamp(colorGrading[2], kMinColorGradeSaturation, kMaxColorGradeSaturation),
		std::clamp(colorGrading[3], kMinColorGradeLift, kMaxColorGradeLift),
	};
}

float BuildExposure(
	const VoxelSceneLighting &lighting,
	const float exposureBiasStops)
{
	const ExposureMeteringMode meteringMode =
		static_cast<ExposureMeteringMode>(std::lround(lighting.exposureControl[0]));
	const float minExposure = std::max(lighting.exposureControl[2], kMinSceneExposure);
	const float maxExposure = std::max(lighting.exposureControl[3], minExposure);
	float meteredExposure = std::clamp(lighting.postProcess[0], minExposure, maxExposure);
	if (meteringMode == ExposureMeteringMode::SceneKey) {
		const float exposureKey = EstimateVoxelSceneExposureKey(lighting);
		const float targetKey =
			std::clamp(lighting.exposureControl[1], kMinExposureTargetKey, kMaxExposureTargetKey);
		meteredExposure = std::clamp(targetKey / std::max(exposureKey, kMinSceneKey), minExposure, maxExposure);
	}
	return std::clamp(meteredExposure * std::exp2(exposureBiasStops), minExposure, maxExposure);
}
} // namespace

VoxelMaterialVisual GetVoxelMaterialVisual(const VoxelMaterial material)
{
	switch (material) {
	case VoxelMaterial::Glass:
		return {
			.baseColor = {0.82f, 0.93f, 1.00f, 0.24f},
			.surface = {0.38f, 0.08f, 0.00f, 0.60f},
			.medium = {0.76f, 0.90f, 1.00f, 0.62f},
			.shading = {0.55f, 0.00f, 0.18f, 0.24f},
		};
	case VoxelMaterial::Fluid:
		return {
			.baseColor = {0.08f, 0.43f, 0.92f, 1.0f},
			.surface = {0.62f, 0.14f, 0.00f, 0.45f},
			.medium = {0.10f, 0.84f, 1.00f, 0.20f},
			.shading = {0.85f, 0.10f, 0.30f, 0.78f},
		};
	case VoxelMaterial::FloorWhite:
		return {
			.baseColor = {0.96f, 0.96f, 0.94f, 1.0f},
			.surface = {1.00f, 0.72f, 0.00f, 0.50f},
			.medium = {0.00f, 0.00f, 0.00f, 0.00f},
			.shading = {1.00f, 0.00f, 0.28f, 0.82f},
		};
	case VoxelMaterial::FloorGray:
		return {
			.baseColor = {0.56f, 0.60f, 0.66f, 1.0f},
			.surface = {0.92f, 0.82f, 0.00f, 0.45f},
			.medium = {0.00f, 0.00f, 0.00f, 0.00f},
			.shading = {1.00f, 0.00f, 0.24f, 0.78f},
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
			.postProcess = {1.20f, 0.90f, static_cast<float>(ToneMapOperator::AcesApprox), 0.0f},
			// EVIL: sunContactShadowParams strength=0.28 / maxDistance=2.25 per VoxelLab tuned shot
			.sunContactShadowParams = {0.28f, 2.25f, 0.0f, 0.0f},
			// EVIL: ambientOcclusionParams strength=0.16 / radius=1.25 per VoxelLab tuned shot
			.ambientOcclusionParams = {0.16f, 1.25f, 0.78f, 0.0f},
			.colorGrading = {1.00f, 1.00f, 0.98f, 0.00f},
			.exposureControl = {static_cast<float>(ExposureMeteringMode::SceneKey), 0.785f, 0.45f, 2.50f},
			.localPointLightPositionAndRadius = {0.0f, 4.5f, 0.0f, 8.0f},
			.localPointLightColorAndIntensity = {1.00f, 0.96f, 0.86f, 12.0f},
			.localPointLightParams = {0.0f, 1.25f, 1.0f, 0.08f},
		};
	case VoxelScenePreset::TransparencyStress:
		return {
			.skyColorAndFogDensity = {0.35f, 0.62f, 0.92f, 0.016f},
			.horizonColorAndFogStart = {0.59f, 0.81f, 1.00f, 18.0f},
			.groundColorAndFogMax = {0.08f, 0.11f, 0.16f, 0.38f},
			.sunColorAndIntensity = {1.00f, 0.93f, 0.84f, 1.30f},
			.sunDirectionAndWrap = {-0.58f, 0.62f, -0.31f, 0.20f},
			.postProcess = {0.95f, 1.05f, static_cast<float>(ToneMapOperator::AcesApprox), 0.0f},
			.sunContactShadowParams = {0.34f, 2.75f, 0.0f, 0.0f},
			.ambientOcclusionParams = {0.18f, 1.35f, 0.74f, 0.0f},
			.colorGrading = {1.00f, 1.02f, 1.05f, 0.00f},
			.exposureControl = {static_cast<float>(ExposureMeteringMode::SceneKey), 0.705f, 0.40f, 2.40f},
			.localPointLightPositionAndRadius = {0.0f, 6.0f, 0.0f, 10.0f},
			.localPointLightColorAndIntensity = {0.50f, 0.80f, 1.00f, 18.0f},
			.localPointLightParams = {0.0f, 1.50f, 0.95f, 0.08f},
		};
	case VoxelScenePreset::ChunkGrid:
		return {
			.skyColorAndFogDensity = {0.92f, 0.74f, 0.56f, 0.012f},
			.horizonColorAndFogStart = {1.00f, 0.84f, 0.66f, 16.0f},
			.groundColorAndFogMax = {0.24f, 0.18f, 0.14f, 0.28f},
			.sunColorAndIntensity = {1.00f, 0.74f, 0.46f, 1.15f},
			.sunDirectionAndWrap = {-0.15f, 0.72f, -0.68f, 0.24f},
			.postProcess = {1.05f, 0.92f, static_cast<float>(ToneMapOperator::AcesApprox), 0.0f},
			.sunContactShadowParams = {0.42f, 3.00f, 0.0f, 0.0f},
			.ambientOcclusionParams = {0.22f, 1.50f, 0.70f, 0.0f},
			.colorGrading = {1.00f, 1.03f, 1.04f, -0.01f},
			.exposureControl = {static_cast<float>(ExposureMeteringMode::SceneKey), 0.755f, 0.45f, 2.60f},
			.localPointLightPositionAndRadius = {-8.0f, 6.0f, 8.0f, 14.0f},
			.localPointLightColorAndIntensity = {1.00f, 0.76f, 0.48f, 28.0f},
			.localPointLightParams = {0.0f, 1.75f, 1.0f, 0.06f},
		};
	case VoxelScenePreset::MeshingStress:
		return {
			.skyColorAndFogDensity = {0.64f, 0.72f, 0.80f, 0.020f},
			.horizonColorAndFogStart = {0.86f, 0.78f, 0.70f, 14.0f},
			.groundColorAndFogMax = {0.14f, 0.16f, 0.18f, 0.40f},
			.sunColorAndIntensity = {1.00f, 0.88f, 0.76f, 1.45f},
			.sunDirectionAndWrap = {-0.70f, 0.48f, -0.18f, 0.18f},
			.postProcess = {0.90f, 0.88f, static_cast<float>(ToneMapOperator::AcesApprox), 0.0f},
			.sunContactShadowParams = {0.48f, 3.50f, 0.0f, 0.0f},
			.ambientOcclusionParams = {0.24f, 1.75f, 0.68f, 0.0f},
			.colorGrading = {1.00f, 1.04f, 0.96f, -0.01f},
			.exposureControl = {static_cast<float>(ExposureMeteringMode::SceneKey), 0.651f, 0.40f, 2.50f},
			.localPointLightPositionAndRadius = {-22.0f, 20.0f, 22.0f, 24.0f},
			.localPointLightColorAndIntensity = {1.00f, 0.84f, 0.62f, 64.0f},
			.localPointLightParams = {0.0f, 2.00f, 1.0f, 0.05f},
		};
	case VoxelScenePreset::VoxelLab:
	default:
		return {
			.skyColorAndFogDensity = {0.70f, 0.83f, 0.97f, 0.014f},
			.horizonColorAndFogStart = {0.84f, 0.91f, 1.00f, 22.0f},
			.groundColorAndFogMax = {0.22f, 0.25f, 0.30f, 0.32f},
			.sunColorAndIntensity = {1.00f, 0.96f, 0.90f, 0.95f},
			.sunDirectionAndWrap = {-0.35f, 0.80f, -0.45f, 0.30f},
			.postProcess = {1.10f, 0.88f, static_cast<float>(ToneMapOperator::AcesApprox), 0.0f},
			.sunContactShadowParams = {0.50f, 3.25f, 0.0f, 0.0f},
			.ambientOcclusionParams = {0.20f, 1.30f, 0.74f, 0.0f},
			.colorGrading = {1.00f, 1.02f, 1.03f, 0.00f},
			.exposureControl = {static_cast<float>(ExposureMeteringMode::SceneKey), 0.867f, 0.45f, 2.60f},
			.localPointLightPositionAndRadius = {4.5f, 4.0f, 7.0f, 10.0f},
			.localPointLightColorAndIntensity = {0.64f, 0.84f, 1.00f, 24.0f},
			.localPointLightParams = {0.0f, 1.50f, 1.0f, 0.08f},
		};
	}
}

VoxelSceneLighting BuildVoxelSceneLighting(
	const VoxelScenePreset preset,
	const VoxelLightingDebugControls &controls)
{
	VoxelSceneLighting lighting = GetVoxelSceneLighting(preset);
	lighting.postProcess[1] = std::clamp(
		lighting.postProcess[1],
		kMinEnvironmentIntensity,
		kMaxEnvironmentIntensity);
	lighting.postProcess[2] = static_cast<float>(controls.toneMapOperator);
	lighting.postProcess[3] = static_cast<float>(controls.debugView);
	lighting.colorGrading = ClampColorGrading(lighting.colorGrading);
	lighting.exposureControl[2] = std::max(lighting.exposureControl[2], kMinSceneExposure);
	if (lighting.exposureControl[3] <= 0.0f) {
		lighting.exposureControl[3] = kDefaultMaxSceneExposure;
	}
	lighting.exposureControl[3] = std::max(lighting.exposureControl[3], lighting.exposureControl[2]);
	lighting.postProcess[0] = BuildExposure(lighting, controls.exposureBiasStops);
	lighting.sunContactShadowParams[0] = std::clamp(
		lighting.sunContactShadowParams[0],
		0.0f,
		1.0f);
	lighting.sunContactShadowParams[1] = std::clamp(
		lighting.sunContactShadowParams[1],
		0.0f,
		kMaxContactShadowDistance);
	lighting.sunContactShadowParams[2] = 0.0f;
	lighting.sunContactShadowParams[3] = 0.0f;
	lighting.ambientOcclusionParams[0] = std::clamp(
		lighting.ambientOcclusionParams[0],
		0.0f,
		1.0f);
	lighting.ambientOcclusionParams[1] = std::clamp(
		lighting.ambientOcclusionParams[1],
		0.0f,
		kMaxAmbientOcclusionRadius);
	lighting.ambientOcclusionParams[2] = std::clamp(
		lighting.ambientOcclusionParams[2],
		0.0f,
		1.0f);
	lighting.ambientOcclusionParams[3] = 0.0f;
	lighting.localPointLightPositionAndRadius[3] = std::clamp(
		lighting.localPointLightPositionAndRadius[3],
		0.0f,
		kMaxLocalPointLightRadius);
	lighting.localPointLightColorAndIntensity[0] = std::clamp(
		lighting.localPointLightColorAndIntensity[0],
		0.0f,
		kMaxLocalPointLightColor);
	lighting.localPointLightColorAndIntensity[1] = std::clamp(
		lighting.localPointLightColorAndIntensity[1],
		0.0f,
		kMaxLocalPointLightColor);
	lighting.localPointLightColorAndIntensity[2] = std::clamp(
		lighting.localPointLightColorAndIntensity[2],
		0.0f,
		kMaxLocalPointLightColor);
	lighting.localPointLightColorAndIntensity[3] = std::clamp(
		lighting.localPointLightColorAndIntensity[3],
		0.0f,
		kMaxLocalPointLightIntensity);
	const bool localPointLightEnabled =
		lighting.localPointLightParams[0] > 0.0f &&
		lighting.localPointLightPositionAndRadius[3] > 0.0f &&
		lighting.localPointLightColorAndIntensity[3] > 0.0f;
	lighting.localPointLightParams[0] = localPointLightEnabled ? 1.0f : 0.0f;
	lighting.localPointLightParams[1] = localPointLightEnabled
											? std::clamp(lighting.localPointLightParams[1], 0.05f, kMaxLocalPointLightSourceRadius)
											: 0.0f;
	lighting.localPointLightParams[2] = localPointLightEnabled
											? std::clamp(lighting.localPointLightParams[2], 0.0f, 1.0f)
											: 0.0f;
	lighting.localPointLightParams[3] = localPointLightEnabled
											? std::clamp(lighting.localPointLightParams[3], 0.0f, kMaxLocalPointLightShadowBias)
											: 0.0f;
	return lighting;
}

float EstimateVoxelSceneExposureKey(const VoxelSceneLighting &lighting)
{
	const float environmentIntensity =
		std::clamp(lighting.postProcess[1], kMinEnvironmentIntensity, kMaxEnvironmentIntensity);
	const float environmentKey =
		(ComputeLuminance(lighting.skyColorAndFogDensity) * 0.35f +
		 ComputeLuminance(lighting.horizonColorAndFogStart) * 0.35f +
		 ComputeLuminance(lighting.groundColorAndFogMax) * 0.20f) *
		environmentIntensity;
	const float sunKey =
		ComputeLuminance(lighting.sunColorAndIntensity) *
		std::max(lighting.sunColorAndIntensity[3], 0.0f) *
		0.18f;
	return std::max(environmentKey + sunKey, kMinSceneKey);
}

std::array<float, 4> GetVoxelSceneClearColor(const VoxelSceneLighting &lighting)
{
	const float exposure = std::max(lighting.postProcess[0], kMinSceneExposure);
	const ToneMapOperator toneMapOperator = static_cast<ToneMapOperator>(std::lround(lighting.postProcess[2]));
	const std::array exposedSkyColor{
		lighting.skyColorAndFogDensity[0] * exposure,
		lighting.skyColorAndFogDensity[1] * exposure,
		lighting.skyColorAndFogDensity[2] * exposure,
	};
	const std::array<float, 3> mappedSkyColor = ApplyToneMap(exposedSkyColor, toneMapOperator);
	const std::array<float, 3> gradedSkyColor = ApplyColorGrading(mappedSkyColor, lighting.colorGrading);
	return {gradedSkyColor[0], gradedSkyColor[1], gradedSkyColor[2], 1.0f};
}

const char *ToneMapOperatorToString(const ToneMapOperator toneMapOperator)
{
	switch (toneMapOperator) {
	case ToneMapOperator::Linear:
		return "LINEAR";
	case ToneMapOperator::Reinhard:
		return "REIN";
	case ToneMapOperator::AcesApprox:
	default:
		return "ACES";
	}
}

const char *ExposureMeteringModeToString(const ExposureMeteringMode meteringMode)
{
	switch (meteringMode) {
	case ExposureMeteringMode::Manual:
		return "MANUAL";
	case ExposureMeteringMode::SceneKey:
	default:
		return "SCENEKEY";
	}
}

const char *LightingDebugViewToString(const LightingDebugView debugView)
{
	switch (debugView) {
	case LightingDebugView::Ambient:
		return "AMB";
	case LightingDebugView::Direct:
		return "DIR";
	case LightingDebugView::Local:
		return "LOCL";
	case LightingDebugView::Shadow:
		return "SHDW";
	case LightingDebugView::Contact:
		return "CTSH";
	case LightingDebugView::Occlusion:
		return "AOCC";
	case LightingDebugView::Fog:
		return "FOG";
	case LightingDebugView::DiffuseGI:
		return "GI_DIF";
	case LightingDebugView::SpecularGI:
		return "GI_SPC";
	case LightingDebugView::RtxSpecular:
		return "RTX_SPC";
	case LightingDebugView::VolumetricFog:
		return "VOL_FOG";
	case LightingDebugView::VolumetricTransmittance:
		return "VOL_TRN";
	case LightingDebugView::GreedyMeshing:
		return "MESH";
	case LightingDebugView::Final:
	default:
		return "FINAL";
	}
}

ToneMapOperator GetNextToneMapOperator(const ToneMapOperator toneMapOperator)
{
	switch (toneMapOperator) {
	case ToneMapOperator::Linear:
		return ToneMapOperator::Reinhard;
	case ToneMapOperator::Reinhard:
		return ToneMapOperator::AcesApprox;
	case ToneMapOperator::AcesApprox:
	default:
		return ToneMapOperator::Linear;
	}
}

LightingDebugView GetNextLightingDebugView(const LightingDebugView debugView)
{
	switch (debugView) {
	case LightingDebugView::Final:
		return LightingDebugView::Ambient;
	case LightingDebugView::Ambient:
		return LightingDebugView::Direct;
	case LightingDebugView::Direct:
		return LightingDebugView::Local;
	case LightingDebugView::Local:
		return LightingDebugView::Shadow;
	case LightingDebugView::Shadow:
		return LightingDebugView::Contact;
	case LightingDebugView::Contact:
		return LightingDebugView::Occlusion;
	case LightingDebugView::Occlusion:
		return LightingDebugView::Fog;
	case LightingDebugView::Fog:
		return LightingDebugView::DiffuseGI;
	case LightingDebugView::DiffuseGI:
		return LightingDebugView::SpecularGI;
	case LightingDebugView::SpecularGI:
		return LightingDebugView::RtxSpecular;
	case LightingDebugView::RtxSpecular:
		return LightingDebugView::VolumetricFog;
	case LightingDebugView::VolumetricFog:
		return LightingDebugView::VolumetricTransmittance;
	case LightingDebugView::VolumetricTransmittance:
		return LightingDebugView::GreedyMeshing;
	case LightingDebugView::GreedyMeshing:
	default:
		return LightingDebugView::Final;
	}
}

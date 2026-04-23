#include "voxel/VoxelMaterials.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr float kMinSceneExposure = 0.05f;
constexpr float kMaxShadowDepthBias = 0.02f;
constexpr float kMaxShadowNormalBias = 0.05f;
constexpr float kMaxShadowFilterRadius = 8.0f;

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

float BuildExposure(
	const float baseExposure,
	const float exposureBiasStops)
{
	return std::max(baseExposure * std::exp2(exposureBiasStops), kMinSceneExposure);
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
			.postProcess = {1.20f, 1.0f, static_cast<float>(ToneMapOperator::AcesApprox), 0.0f},
			.sunShadowParams = {0.72f, 0.0009f, 0.0060f, 1.10f},
		};
	case VoxelScenePreset::TransparencyStress:
		return {
			.skyColorAndFogDensity = {0.35f, 0.62f, 0.92f, 0.016f},
			.horizonColorAndFogStart = {0.59f, 0.81f, 1.00f, 18.0f},
			.groundColorAndFogMax = {0.08f, 0.11f, 0.16f, 0.38f},
			.sunColorAndIntensity = {1.00f, 0.93f, 0.84f, 1.30f},
			.sunDirectionAndWrap = {-0.58f, 0.62f, -0.31f, 0.20f},
			.postProcess = {0.95f, 1.0f, static_cast<float>(ToneMapOperator::AcesApprox), 0.0f},
			.sunShadowParams = {0.64f, 0.0008f, 0.0055f, 1.25f},
		};
	case VoxelScenePreset::ChunkGrid:
		return {
			.skyColorAndFogDensity = {0.92f, 0.74f, 0.56f, 0.012f},
			.horizonColorAndFogStart = {1.00f, 0.84f, 0.66f, 16.0f},
			.groundColorAndFogMax = {0.24f, 0.18f, 0.14f, 0.28f},
			.sunColorAndIntensity = {1.00f, 0.74f, 0.46f, 1.15f},
			.sunDirectionAndWrap = {-0.15f, 0.72f, -0.68f, 0.24f},
			.postProcess = {1.05f, 1.0f, static_cast<float>(ToneMapOperator::AcesApprox), 0.0f},
			.sunShadowParams = {0.76f, 0.0010f, 0.0040f, 1.30f},
		};
	case VoxelScenePreset::MeshingStress:
		return {
			.skyColorAndFogDensity = {0.64f, 0.72f, 0.80f, 0.020f},
			.horizonColorAndFogStart = {0.86f, 0.78f, 0.70f, 14.0f},
			.groundColorAndFogMax = {0.14f, 0.16f, 0.18f, 0.40f},
			.sunColorAndIntensity = {1.00f, 0.88f, 0.76f, 1.45f},
			.sunDirectionAndWrap = {-0.70f, 0.48f, -0.18f, 0.18f},
			.postProcess = {0.90f, 1.0f, static_cast<float>(ToneMapOperator::AcesApprox), 0.0f},
			.sunShadowParams = {0.80f, 0.0010f, 0.0070f, 1.50f},
		};
	case VoxelScenePreset::VoxelLab:
	default:
		return {
			.skyColorAndFogDensity = {0.70f, 0.83f, 0.97f, 0.014f},
			.horizonColorAndFogStart = {0.84f, 0.91f, 1.00f, 22.0f},
			.groundColorAndFogMax = {0.22f, 0.25f, 0.30f, 0.32f},
			.sunColorAndIntensity = {1.00f, 0.96f, 0.90f, 0.95f},
			.sunDirectionAndWrap = {-0.35f, 0.80f, -0.45f, 0.30f},
			.postProcess = {1.10f, 1.0f, static_cast<float>(ToneMapOperator::AcesApprox), 0.0f},
			.sunShadowParams = {0.74f, 0.0009f, 0.0060f, 1.35f},
		};
	}
}

VoxelSceneLighting BuildVoxelSceneLighting(
	const VoxelScenePreset preset,
	const VoxelLightingDebugControls &controls)
{
	VoxelSceneLighting lighting = GetVoxelSceneLighting(preset);
	lighting.postProcess[0] = BuildExposure(lighting.postProcess[0], controls.exposureBiasStops);
	lighting.postProcess[2] = static_cast<float>(controls.toneMapOperator);
	lighting.postProcess[3] = static_cast<float>(controls.debugView);
	lighting.sunShadowParams[0] = std::clamp(
		lighting.sunShadowParams[0] + controls.shadowStrengthOffset,
		0.0f,
		1.0f);
	lighting.sunShadowParams[1] = std::clamp(
		lighting.sunShadowParams[1] + controls.shadowDepthBiasOffset,
		0.0f,
		kMaxShadowDepthBias);
	lighting.sunShadowParams[2] = std::clamp(
		lighting.sunShadowParams[2] + controls.shadowNormalBiasOffset,
		0.0f,
		kMaxShadowNormalBias);
	lighting.sunShadowParams[3] = std::clamp(
		lighting.sunShadowParams[3] + controls.shadowFilterRadiusOffset,
		0.0f,
		kMaxShadowFilterRadius);
	return lighting;
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
	return {mappedSkyColor[0], mappedSkyColor[1], mappedSkyColor[2], 1.0f};
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

const char *LightingDebugViewToString(const LightingDebugView debugView)
{
	switch (debugView) {
	case LightingDebugView::Ambient:
		return "AMB";
	case LightingDebugView::Direct:
		return "DIR";
	case LightingDebugView::Shadow:
		return "SHDW";
	case LightingDebugView::Fog:
		return "FOG";
	case LightingDebugView::Final:
	default:
		return "FINAL";
	}
}

const char *ShadowTuningTargetToString(const ShadowTuningTarget target)
{
	switch (target) {
	case ShadowTuningTarget::Strength:
		return "STR";
	case ShadowTuningTarget::DepthBias:
		return "BIAS";
	case ShadowTuningTarget::NormalBias:
		return "NRM";
	case ShadowTuningTarget::FilterRadius:
		return "FLT";
	case ShadowTuningTarget::Coverage:
	default:
		return "COV";
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
		return LightingDebugView::Shadow;
	case LightingDebugView::Shadow:
		return LightingDebugView::Fog;
	case LightingDebugView::Fog:
	default:
		return LightingDebugView::Final;
	}
}

ShadowTuningTarget GetNextShadowTuningTarget(const ShadowTuningTarget target)
{
	switch (target) {
	case ShadowTuningTarget::Strength:
		return ShadowTuningTarget::DepthBias;
	case ShadowTuningTarget::DepthBias:
		return ShadowTuningTarget::NormalBias;
	case ShadowTuningTarget::NormalBias:
		return ShadowTuningTarget::FilterRadius;
	case ShadowTuningTarget::FilterRadius:
		return ShadowTuningTarget::Coverage;
	case ShadowTuningTarget::Coverage:
	default:
		return ShadowTuningTarget::Strength;
	}
}

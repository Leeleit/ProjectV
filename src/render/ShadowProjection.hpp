#ifndef SHADOW_PROJECTION_HPP
#define SHADOW_PROJECTION_HPP

#include "render/ShadowTypes.hpp"
#include "voxel/VoxelWorld.hpp"

#include <array>

struct SunShadowProjection {
	std::array<float, 16> lightViewProjection{};
};

struct SunShadowCascadeProjectionInputs {
	std::array<float, 3> cameraPosition{};
	std::array<float, 3> cameraForward{};
	std::array<float, 3> cameraRight{};
	std::array<float, 3> cameraUp{};
	float tanHalfVerticalFov = 1.0f;
	float tanHalfHorizontalFov = 1.0f;
	uint32_t shadowMapResolution = 2048u;
	SunShadowCascadeSplits splits{};
};

struct SunShadowCascadeProjections {
	std::array<float, kSunShadowMatrixElementCount> lightViewProjections{};
	SunShadowCascadeDiagnostics diagnostics{};
};

SunShadowProjection BuildSunShadowProjection(
	const VoxelWorld &world,
	const std::array<float, 3> &sunDirection,
	float coverageScale = 1.0f);

SunShadowCascadeProjections BuildSunShadowCascadeProjections(
	const VoxelWorld &world,
	const std::array<float, 3> &sunDirection,
	const SunShadowCascadeProjectionInputs &inputs,
	float coverageScale = 1.0f);

SunShadowCascadeSplits BuildSunShadowCascadeSplits(
	float nearPlane,
	float farPlane,
	float splitLambda = 0.80f);

#endif

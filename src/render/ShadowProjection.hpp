#pragma once

#include "core/Math.hpp"
#include "render/ShadowTypes.hpp"
#include "voxel/VoxelWorld.hpp"

#include <array>

struct SunShadowProjection {
	projectv::math::Mat4 lightViewProjection{};
};

struct SunShadowCascadeProjectionInputs {
	projectv::math::Vec3 cameraPosition{};
	projectv::math::Vec3 cameraForward{};
	projectv::math::Vec3 cameraRight{};
	projectv::math::Vec3 cameraUp{};
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


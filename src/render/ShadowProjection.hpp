#pragma once

#include "core/Math.hpp"
#include "render/ShadowTypes.hpp"
#include "voxel/VoxelWorld.hpp"

#include <array>

// **Tier 0.B (`2026-06-13`).** `Mat4` (16-byte aligned) replaces
// `std::array<float, 16>` for the light viewProjection. Same
// byte size (64 B), same column-major field order, GPU layout
// preserved.
struct SunShadowProjection {
	projectv::math::Mat4 lightViewProjection{};
};

struct SunShadowCascadeProjectionInputs {
	// **Tier 0.B.** `Vec3` (16-byte aligned) replaces
	// `std::array<float, 3>`. ABI change: each field grows from
	// 12 to 16 bytes; the struct gains 16 bytes total. ABI
	// change in mainline internals; not a hot per-frame cost.
	projectv::math::Vec3 cameraPosition{};
	projectv::math::Vec3 cameraForward{};
	projectv::math::Vec3 cameraRight{};
	projectv::math::Vec3 cameraUp{};
	float tanHalfVerticalFov = 1.0f;
	float tanHalfHorizontalFov = 1.0f;
	uint32_t shadowMapResolution = 2048u;
	SunShadowCascadeSplits splits{};
};

// **Tier 0.B.** `lightViewProjections` stays `std::array<float, N>`
// (raw storage for memcpy into the UBO `sunShadowViewProjections`
// field) but `BuildSunShadowCascadeProjections` now writes
// `Mat4`s per-cascade and memcpy's them in. The 64 B * 4 = 256 B
// total byte size is unchanged.
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


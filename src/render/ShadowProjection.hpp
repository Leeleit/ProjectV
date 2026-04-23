#ifndef SHADOW_PROJECTION_HPP
#define SHADOW_PROJECTION_HPP

#include "voxel/VoxelWorld.hpp"

#include <array>

struct SunShadowProjection {
	std::array<float, 16> lightViewProjection{};
};

SunShadowProjection BuildSunShadowProjection(
	const VoxelWorld &world,
	const std::array<float, 3> &sunDirection,
	float coverageScale = 1.0f);

#endif

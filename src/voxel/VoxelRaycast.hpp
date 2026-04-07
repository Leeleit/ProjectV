#ifndef VOXEL_RAYCAST_HPP
#define VOXEL_RAYCAST_HPP

#include "voxel/VoxelWorld.hpp"

#include <array>

struct VoxelRaycastHit {
	bool hasHit = false;
	bool hasPlacementVoxel = false;
	Int3 voxel{};
	Int3 placementVoxel{};
	Int3 hitNormal{};
	VoxelMaterial material = VoxelMaterial::Air;
	float distance = 0.0f;
};

VoxelRaycastHit RaycastVoxelWorld(
	const VoxelWorld &world,
	const std::array<float, 3> &origin,
	const std::array<float, 3> &direction,
	float maxDistance);

#endif

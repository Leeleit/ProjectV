#ifndef PHYSICS_WORLD_HPP
#define PHYSICS_WORLD_HPP

#include "voxel/VoxelWorld.hpp"

#include <array>
#include <cstdint>

struct CameraState;
struct InputState;
struct PhysicsState;

struct PhysicsRaycastHit {
	bool hasHit = false;
	Int3 voxel{};
	std::array<float, 3> position{};
	std::array<float, 3> normal{};
	float distance = 0.0f;
};

PhysicsState *CreatePhysicsState();
void DestroyPhysicsState(PhysicsState *physics);
bool SyncPhysicsWorld(PhysicsState *physics, const VoxelWorld *world);
PhysicsRaycastHit RaycastPhysicsWorld(
	const PhysicsState *physics,
	const std::array<float, 3> &origin,
	const std::array<float, 3> &direction,
	float maxDistance);
void ResetWalkCharacter(PhysicsState *physics);
bool SnapWalkCharacterToCamera(
	PhysicsState *physics,
	const VoxelWorld *world,
	CameraState *camera);
bool TickWalkCharacter(
	PhysicsState *physics,
	const VoxelWorld *world,
	CameraState *camera,
	InputState *input,
	float deltaSeconds);
uint64_t GetPhysicsWorldSyncVersion(const PhysicsState *physics);

#endif

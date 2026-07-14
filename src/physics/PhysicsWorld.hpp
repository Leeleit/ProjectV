#pragma once

#include "voxel/VoxelWorld.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include <array>

struct CameraState;
struct InputState;
struct PhysicsState;
enum class WalkAirControlMode : uint8_t;

enum class PhysicsWalkSupportDebugState : uint8_t {
	Air = 0,
	Grounded,
	EdgeGrace,
};

struct PhysicsWalkDebugInfo {
	bool valid = false;
	PhysicsWalkSupportDebugState supportState = PhysicsWalkSupportDebugState::Air;
	std::array<float, 3> feetPosition{};
	float footSupportScore = 0.0f;
	uint32_t footSupportHitSamples = 0;
	uint32_t footSupportTotalSamples = 0;
	uint32_t edgeGraceFramesRemaining = 0;
	uint32_t groundTakeoffGraceFramesRemaining = 0;
	uint32_t sneakSupportGraceFramesRemaining = 0;
	uint32_t ledgeReleaseGraceFramesRemaining = 0;
	bool groundTakeoffCached = false;
	bool cachedSneakSupportValid = false;
	bool feetInsideCachedSneakSupport = false;
	bool sneakActive = false;
	bool jumpLockActive = false;
	bool suppressPassiveSlide = false;
	bool autoJumpEnabled = false;
	float cachedSneakSupportReferenceFeetY = 0.0f;
};

struct PhysicsRaycastHit {
	bool hasHit = false;
	Int3 voxel{};
	std::array<float, 3> position{};
	std::array<float, 3> normal{};
	float distance = 0.0f;
};

struct PhysicsBroadphaseStats {
	uint32_t totalBodies = 0;
	uint32_t maxBodies = 0;
	uint32_t staticBodies = 0;
	uint32_t dynamicBodies = 0;
	uint32_t activeDynamicBodies = 0;
	uint32_t kinematicBodies = 0;
	uint32_t activeKinematicBodies = 0;
	uint32_t pendingChunkRebuilds = 0;
	uint32_t chunkStaticBodies = 0;
	uint32_t chunkMergedBoxesEntries = 0;
};

PhysicsBroadphaseStats GetPhysicsBroadphaseStats(const PhysicsState *physics);

PhysicsState *CreatePhysicsState();
void DestroyPhysicsState(PhysicsState *physics); // NOLINT(readability-redundant-declaration): also declared in Types.hpp for deleter visibility
bool SyncPhysicsWorld(PhysicsState *physics, const VoxelWorld *world);
void QueueChunkRebuildRequest(PhysicsState *physics, uint32_t chunkIndex);
uint32_t ProcessChunkRebuildQueue(PhysicsState *physics, const VoxelWorld *world);
uint32_t GetPendingChunkRebuildCount(const PhysicsState *physics);
uint32_t GetChunkBodyCount(const PhysicsState *physics);
bool RebuildStaticWorldBodyFromChunkShapes(PhysicsState &physics, const VoxelWorld &world);
void DestroyAllChunkStaticBodies(PhysicsState &physics);
bool BuildChunkStaticCollisionBody(PhysicsState &physics, const VoxelWorld &world, uint32_t chunkIndex);
uint64_t GetPhysicsWorldSyncVersion(const PhysicsState *physics);
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
bool SnapCreativeCharacterToCamera(
	PhysicsState *physics,
	const VoxelWorld *world,
	CameraState *camera);
bool TickWalkCharacter(
	PhysicsState *physics,
	const VoxelWorld *world,
	CameraState *camera,
	InputState *input,
	float deltaSeconds);
bool TickCreativeCharacter(
	PhysicsState *physics,
	const VoxelWorld *world,
	CameraState *camera,
	const InputState *input,
	float deltaSeconds);
bool DoesPhysicsCharacterOverlapVoxel(
	const PhysicsState *physics,
	const CameraState &camera,
	const Int3 &voxel);
void SetPhysicsWalkAirControlMode(PhysicsState *physics, WalkAirControlMode mode);
WalkAirControlMode GetPhysicsWalkAirControlMode(const PhysicsState *physics);
void SetPhysicsWalkAutoJumpEnabled(PhysicsState *physics, bool enabled);
bool IsPhysicsWalkAutoJumpEnabled(const PhysicsState *physics);
PhysicsWalkDebugInfo GetPhysicsWalkDebugInfo(const PhysicsState *physics);




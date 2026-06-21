import projectv.math;
import projectv.string_id;


#include "physics/PhysicsWorld.hpp"
#include "physics/GreedyPhysicsMerger.hpp"

#include "app/InputActions.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "core/Types.hpp"
#include "debug/Profiling.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <unordered_map>

#pragma warning(push, 0)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/SubShapeID.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <Jolt/Math/Vec3.h>
#pragma clang diagnostic pop
#pragma warning(pop)

namespace {
// EVIL: kPhysicsDirectionEpsilon = 0.00001f is the smallest direction vector magnitude we still trust as
	// normalized. Below this we re-normalize to avoid 1/0 in cross products and length comparisons.
constexpr float kPhysicsDirectionEpsilon = 0.00001f;
// EVIL: kPhysicsRaycastVoxelEpsilon = 0.001f is the half-voxel inset used to avoid hitting the voxel's own
	// face. Below 1e-3 we saw numerical jitter on chunk boundary cells; above 1e-2 we'd risk skipping thin walls.
constexpr float kPhysicsRaycastVoxelEpsilon = 0.001f;
constexpr float kWalkCapsuleRadius = 0.35f;
constexpr float kWalkCapsuleHalfHeight = 0.55f;
constexpr float kWalkEyeHeight = 1.6f;
constexpr float kWalkSneakCapsuleHalfHeight = 0.45f;
constexpr float kWalkSneakEyeHeight = 1.45f;
constexpr float kWalkMoveSpeed = 4.5f;
constexpr float kWalkSneakMoveSpeedMultiplier = 0.45f;
constexpr float kWalkBoostMultiplier = 1.8f;
constexpr float kWalkSlowMultiplier = 0.35f;
constexpr float kWalkJumpSpeed = 8.0f;
// EVIL: kWalkSpawnClearance = 0.05f lifts the spawn position by 5cm to prevent physics engine from claiming
	// we're already penetrating ground at spawn. Hardcoded fallback before SpawnWalkCharacterToCamera probe.
constexpr float kWalkSpawnClearance = 0.05f;
// EVIL: kWalkSneakShapeMaxPenetrationDepth = 0.05f caps how deep the sneak capsule is allowed to sink into
	// ground voxels before forced ejection. Matches the spawn clearance above for consistency.
constexpr float kWalkSneakShapeMaxPenetrationDepth = 0.05f;
constexpr float kWalkJumpRealisticAirBrakeDeceleration = 14.0f;
constexpr float kWalkJumpRealisticAirReacceleration = 10.0f;
constexpr float kWalkJumpMinecraftAirBrakeDeceleration = 14.0f;
constexpr float kWalkJumpMinecraftAirControlAcceleration = 12.0f;
constexpr uint32_t kWalkAutoJumpDelayFrames = 12;
constexpr float kWalkStickToFloorDistance = 0.25f;
constexpr float kWalkStairsStepUpHeight = 0.4f;
constexpr float kWalkAutoJumpMinRise = kWalkStairsStepUpHeight + 0.05f;
constexpr float kWalkAutoJumpMaxRise = 1.05f;
constexpr float kWalkCameraAirRiseSmoothingMaxPerTick = 0.12f;
constexpr float kWalkPredictiveContactDistance = 0.02f;
constexpr float kWalkFootSupportSampleRadius = kWalkCapsuleRadius * 0.8f;
constexpr float kWalkFootSupportProbeDepth = 0.08f;
constexpr uint32_t kWalkFootSupportSampleGridResolution = 4;
constexpr float kWalkFootSupportGroundedScore = 0.7f;
constexpr float kWalkFootSupportEdgeGraceScore = 0.2f;
constexpr float kWalkFootSupportMovingEdgeGraceScore = 0.5f;
constexpr float kWalkGroundSupportRadius = 0.2f;
constexpr float kWalkGroundTakeoffSupportRadius = kWalkCapsuleRadius + 0.05f;
constexpr float kWalkGroundTakeoffSnapMaxDrop = 0.05f;
constexpr float kWalkRestingEdgeHoldMaxHorizontalDrift = 0.02f;
constexpr float kWalkSneakSupportSampleRadius = kWalkFootSupportSampleRadius;
[[maybe_unused]] constexpr float kWalkSneakSupportProbeDepth = 0.08f;
constexpr float kWalkSneakSupportRegionExtent = 0.39f;
constexpr float kWalkSneakBackoffInset = 0.01f;
constexpr float kWalkSneakOutwardDriftEpsilon = 0.001f;
constexpr float kWalkSneakStickProbeLift = 0.05f;
constexpr float kWalkSneakStickToFloorDistance = 0.08f;
constexpr float kWalkSneakStickPositiveVelocityEpsilon = 0.15f;
constexpr float kWalkSneakStickMinimumDrop = 0.01f;
constexpr float kWalkJumpLockedSupportCeilingReprojectMaxDistance = kWalkCapsuleRadius + kWalkSneakStickToFloorDistance;
constexpr float kWalkJumpLockedSupportMaxRiseAboveReference = 2.5f;
constexpr float kWalkJumpLockedSupportMaxDropBelowReference = 0.45f;
constexpr float kWalkJumpLockedSupportSourceWallMaxUpDot = 0.55f;
constexpr float kWalkJumpLockedSupportContactVoxelEpsilon = 0.02f;
constexpr float kWalkCollisionEpsilon = 0.001f;
constexpr float kWalkPenetrationResolveEpsilon = 0.0005f;
constexpr float kWalkGroundProbeEpsilon = 0.001f;
constexpr float kWalkHorizontalSubstepDistance = 0.05f;
constexpr uint32_t kWalkEdgeGraceFrames = 4;
constexpr uint32_t kWalkGroundTakeoffGraceFrames = 12;
constexpr uint32_t kWalkGroundReturnAnchorFrames = 48;
constexpr float kWalkGroundReturnRestoreMaxDrop = 0.65f;
constexpr float kWalkGroundReturnSnapMaxDrop = 0.05f;
constexpr float kWalkGroundReturnSupportMaxRise = 0.08f;
constexpr uint32_t kWalkSneakSupportGraceFrames = 3;
constexpr uint32_t kWalkLedgeReleaseGraceFrames = 4;
constexpr float kWalkGroundTakeoffGraceMaxDrift = 0.65f;
constexpr float kWalkGroundTakeoffLandingMaxDrift = 0.25f;
constexpr float kWalkSupportContactMaxHeightAboveFeet = 0.1f;
constexpr float kCreativeMoveSpeedMultiplier = 1.0f;
constexpr float kCreativeBoostMultiplier = 3.0f;
constexpr float kCreativeSlowMultiplier = 0.25f;
constexpr float kCreativeCollisionMaxStepDistance = 0.05f;
constexpr uint32_t kCreativeCollisionMaxSubsteps = 32;
constexpr uint32_t kMaxPhysicsBodies = 1024;
constexpr uint32_t kMaxBodyPairs = 4096;
constexpr uint32_t kMaxContactConstraints = 1024;
constexpr size_t kPhysicsTempAllocatorBytes = static_cast<size_t>(8) * 1024 * 1024;

namespace PhysicsLayers {
constexpr JPH::ObjectLayer Static = 0;
constexpr JPH::ObjectLayer Moving = 1;

constexpr JPH::BroadPhaseLayer StaticBroadPhase{0};
constexpr JPH::BroadPhaseLayer MovingBroadPhase{1};
constexpr uint32_t BroadPhaseLayerCount = 2;
} // namespace PhysicsLayers

struct Float3 {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

enum class WalkSupportState : uint8_t {
	Air = 0,
	Grounded,
	EdgeGrace,
};

struct WalkFootSupportInfo {
	float score = 0.0f;
	uint32_t hitSamples = 0;
	uint32_t totalSamples = 0;
	std::array<float, 3> centroid{};
};

bool HasMoveUpInputActionMaskBit(const uint32_t mask)
{
	return (mask & 1u << static_cast<uint32_t>(InputAction::MoveUp)) != 0u;
}

struct WalkSneakSupportFace {
	std::array<float, 2> min{};
	std::array<float, 2> max{};
};

struct WalkSneakSupportRegion {
	std::array<WalkSneakSupportFace, 36> faces{};
	uint32_t faceCount = 0;
	float sampleRadius = kWalkSneakSupportSampleRadius;
	std::array<float, 2> boundsMin{};
	std::array<float, 2> boundsMax{};
	std::array<float, 3> referenceFeetPosition{};
	bool valid = false;
};

struct WalkJumpLockedSupportState {
	WalkSneakSupportRegion region{};
	std::array<float, 3> anchorFeetPosition{};
	std::array<float, 3> constrainedFeetPosition{};
	bool constrainMovementWhileSneakHeld = false;
	bool valid = false;
};

struct WalkTopSupportCandidate {
	WalkSneakSupportRegion region{};
	std::array<float, 3> feetPosition{};
	bool valid = false;
};

struct WalkSupportContactKey {
	JPH::BodyID bodyId;
	JPH::SubShapeID subShapeId;
	bool valid = false;
};

Int3 FloorToVoxel(const std::array<float, 3> &position);
WalkSneakSupportRegion ComputeWalkSneakSupportRegion(
	const VoxelWorld &world,
	const std::array<float, 3> &feetPosition);
bool IsWalkFeetInsideSneakSupportRegion(
	const WalkSneakSupportRegion &region,
	const std::array<float, 3> &feetPosition);
std::array<float, 2> ProjectWalkFeetToSneakSupportRegion(
	const WalkSneakSupportRegion &region,
	const std::array<float, 3> &feetPosition);
bool FindWalkBestSupportFeetYAtXZ(
	const VoxelWorld &world,
	const std::array<float, 3> &referenceFeetPosition,
	float maxRise,
	float maxDrop,
	bool sneakActive,
	float footprintRadius,
	float &outFeetY);
bool IsWalkCharacterClearAt(
	const VoxelWorld &world,
	const std::array<float, 3> &feetPosition,
	bool sneakActive);
bool HasWalkSneakSupport(const VoxelWorld &world, const std::array<float, 3> &feetPosition);

class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
  public:
	[[nodiscard]] unsigned int GetNumBroadPhaseLayers() const override
	{
		return PhysicsLayers::BroadPhaseLayerCount;
	}

	[[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(const JPH::ObjectLayer layer) const override
	{
		switch (layer) {
		case PhysicsLayers::Static:
			return PhysicsLayers::StaticBroadPhase;
		case PhysicsLayers::Moving:
			return PhysicsLayers::MovingBroadPhase;
		default:
			return PhysicsLayers::StaticBroadPhase;
		}
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	const char *GetBroadPhaseLayerName(const JPH::BroadPhaseLayer layer) const override
	{
		switch (static_cast<uint32_t>(layer)) {
		case 0:
			return "STATIC";
		case 1:
			return "MOVING";
		default:
			return "UNKNOWN";
		}
	}
#endif
};

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
  public:
	[[nodiscard]] bool ShouldCollide(const JPH::ObjectLayer layer1, const JPH::ObjectLayer layer2) const override
	{
		if (layer1 == PhysicsLayers::Static) {
			return layer2 == PhysicsLayers::Moving;
		}

		return layer1 == PhysicsLayers::Moving;
	}
};

class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
  public:
	[[nodiscard]] bool ShouldCollide(const JPH::ObjectLayer layer, const JPH::BroadPhaseLayer broadPhaseLayer) const override
	{
		if (layer == PhysicsLayers::Static) {
			return broadPhaseLayer == PhysicsLayers::MovingBroadPhase;
		}

		return layer == PhysicsLayers::Moving;
	}
};

class WalkCharacterContactListener final : public JPH::CharacterContactListener {
  public:
	PhysicsState *physics = nullptr;

	void OnContactSolve(
		const JPH::CharacterVirtual *inCharacter,
		const JPH::BodyID &inBodyID2,
		const JPH::SubShapeID &inSubShapeID2,
		JPH::RVec3Arg inContactPosition,
		JPH::Vec3Arg inContactNormal,
		JPH::Vec3Arg inContactVelocity,
		const JPH::PhysicsMaterial *inContactMaterial,
		JPH::Vec3Arg inCharacterVelocity,
		JPH::Vec3 &ioNewCharacterVelocity) override;
};

struct JoltRuntimeState {
	std::mutex mutex;
	uint32_t refCount = 0;
};

JoltRuntimeState &GetJoltRuntimeState()
{
	static JoltRuntimeState state{};
	return state;
}
} // namespace

struct PhysicsState {
	BroadPhaseLayerInterfaceImpl broadPhaseLayerInterface{};
	ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter{};
	ObjectLayerPairFilterImpl objectLayerPairFilter{};
	JPH::PhysicsSystem physicsSystem;
	JPH::TempAllocatorImpl tempAllocator{static_cast<unsigned int>(kPhysicsTempAllocatorBytes)};
	JPH::BodyID staticWorldBodyId;
	JPH::RefConst<JPH::Shape> staticWorldShape;
	JPH::RefConst<JPH::Shape> walkStandingShape;
	JPH::RefConst<JPH::Shape> walkSneakShape;
	JPH::Ref<JPH::CharacterVirtual> walkCharacter;
	WalkCharacterContactListener walkContactListener{};
	const VoxelWorld *syncedWorld = nullptr;
	uint64_t syncedWorldEditVersion = 0;
	std::unordered_map<uint32_t, JPH::BodyID> chunkStaticBodies;
	std::unordered_map<uint32_t, std::vector<projectv::physics::MergedVoxelBox>> chunkMergedBoxes;
	std::vector<uint32_t> pendingChunkRebuilds;
	bool walkCharacterInitialized = false;
	WalkSupportState walkSupportState = WalkSupportState::Air;
	uint32_t walkEdgeGraceFramesRemaining = 0;
	uint32_t walkGroundTakeoffGraceFramesRemaining = 0;
	float walkFootSupportScore = 0.0f;
	uint32_t walkFootSupportHitSamples = 0;
	uint32_t walkFootSupportTotalSamples = 0;
	std::array<float, 3> walkFootSupportCentroid{};
	std::array<float, 3> walkCachedGroundTakeoffFeetPosition{};
	bool walkCachedGroundTakeoffValid = false;
	std::array<float, 3> walkGroundReturnAnchorFeetPosition{};
	uint32_t walkGroundReturnAnchorFramesRemaining = 0;
	bool walkGroundReturnAnchorValid = false;
	std::array<float, 3> walkPreviousSupportFeetPosition{};
	bool walkPreviousSupportFeetPositionValid = false;
	bool walkHadHorizontalMotionLastStep = false;
	JPH::Vec3 walkJumpBallisticHorizontalDirection = JPH::Vec3::sZero();
	JPH::Vec3 walkJumpBallisticHorizontalVelocity = JPH::Vec3::sZero();
	float walkJumpBallisticHorizontalTakeoffSpeed = 0.0f;
	bool walkJumpBallisticHorizontalVelocityActive = false;
	WalkAirControlMode walkAirControlMode = WalkAirControlMode::MinecraftLike;
	bool walkAutoJumpEnabled = false;
	uint32_t walkAutoJumpDelayFramesRemaining = 0;
	bool walkAutoJumpDelayEnabled = true;
	WalkSupportContactKey walkPassiveSlideContact{};
	bool walkSneakActive = false;
	WalkSneakSupportRegion walkCachedSneakSupportRegion{};
	WalkJumpLockedSupportState walkJumpLockedSupport{};
	uint32_t walkSneakSupportGraceFramesRemaining = 0;
	uint32_t walkLedgeReleaseGraceFramesRemaining = 0;
	bool walkSuppressPassiveSlide = false;
};

namespace {

void AcquireJoltRuntime()
{
	auto &[mutex, refCount] = GetJoltRuntimeState();
	const std::scoped_lock lock(mutex);
	if (refCount == 0) {
		JPH::RegisterDefaultAllocator();
		JPH::Factory::sInstance = new JPH::Factory();
		JPH::RegisterTypes();
	}

	++refCount;
}

void ReleaseJoltRuntime()
{
	auto &[mutex, refCount] = GetJoltRuntimeState();
	const std::scoped_lock lock(mutex);
	if (refCount == 0) {
		return;
	}

	--refCount;
	if (refCount == 0) {
		JPH::UnregisterTypes();
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;
	}
}

int GetWalkSneakSupportVoxelY(const WalkSneakSupportRegion &region)
{
	return FloorToVoxel(
			   {
				   region.referenceFeetPosition[0],
				   region.referenceFeetPosition[1] - kWalkSpawnClearance - kWalkGroundProbeEpsilon,
				   region.referenceFeetPosition[2],
			   })
		.y;
}

bool DoesWalkSneakSupportRegionContainVoxel(
	const WalkSneakSupportRegion &region,
	const int voxelX,
	const int voxelZ)
{
	for (uint32_t faceIndex = 0; faceIndex < region.faceCount; ++faceIndex) {
		const auto &[min, max] = region.faces[faceIndex];
		if (min[0] <= static_cast<float>(voxelX) + kWalkSneakOutwardDriftEpsilon &&
			max[0] >= static_cast<float>(voxelX + 1) - kWalkSneakOutwardDriftEpsilon &&
			min[1] <= static_cast<float>(voxelZ) + kWalkSneakOutwardDriftEpsilon &&
			max[1] >= static_cast<float>(voxelZ + 1) - kWalkSneakOutwardDriftEpsilon) {
			return true;
		}
	}

	return false;
}

bool IsWalkJumpLockedSupportTargetInsideRegion(const PhysicsState &physics)
{
	std::array<float, 3> constrainedFeetPosition = physics.walkJumpLockedSupport.constrainedFeetPosition;
	const std::array<float, 2> projectedFeetXZ =
		ProjectWalkFeetToSneakSupportRegion(physics.walkJumpLockedSupport.region, constrainedFeetPosition);
	constrainedFeetPosition[0] = projectedFeetXZ[0];
	constrainedFeetPosition[2] = projectedFeetXZ[1];
	return IsWalkFeetInsideSneakSupportRegion(physics.walkJumpLockedSupport.region, constrainedFeetPosition);
}

bool IsPhysicsStaticWorldBodyId(const PhysicsState &physics, const JPH::BodyID &bodyId)
{
	if (bodyId == physics.staticWorldBodyId) {
		return true;
	}
	for (const auto &entry : physics.chunkStaticBodies) {
		if (entry.second == bodyId) {
			return true;
		}
	}
	return false;
}

bool IsWalkJumpLockedSourceSupportSideWallContact(
	const PhysicsState &physics,
	const JPH::BodyID &bodyId,
	JPH::RVec3Arg contactPosition,
	JPH::Vec3Arg contactNormal)
{
	if (!physics.walkJumpLockedSupport.valid || !IsPhysicsStaticWorldBodyId(physics, bodyId)) {
		return false;
	}

	const JPH::CharacterVirtual *character = physics.walkCharacter.GetPtr();
	if (character == nullptr || !IsWalkJumpLockedSupportTargetInsideRegion(physics)) {
		return false;
	}

	const JPH::Vec3 up = character->GetUp();
	const JPH::Vec3 normalizedContactNormal = contactNormal.NormalizedOr(JPH::Vec3::sZero());
	if (normalizedContactNormal.IsNearZero() ||
		std::abs(normalizedContactNormal.Dot(up)) >= kWalkJumpLockedSupportSourceWallMaxUpDot) {
		return false;
	}

	const JPH::RVec3 samplePoint =
		contactPosition -
		JPH::RVec3(normalizedContactNormal * kWalkJumpLockedSupportContactVoxelEpsilon) -
		JPH::RVec3(up * kWalkJumpLockedSupportContactVoxelEpsilon);
	const Int3 contactVoxel{
		static_cast<int>(std::floor(samplePoint.GetX())),
		static_cast<int>(std::floor(samplePoint.GetY())),
		static_cast<int>(std::floor(samplePoint.GetZ())),
	};
	return contactVoxel.y == GetWalkSneakSupportVoxelY(physics.walkJumpLockedSupport.region) &&
		   DoesWalkSneakSupportRegionContainVoxel(
			   physics.walkJumpLockedSupport.region,
			   contactVoxel.x,
			   contactVoxel.z);
}

void WalkCharacterContactListener::OnContactSolve(
	const JPH::CharacterVirtual *inCharacter,
	const JPH::BodyID &inBodyID2,
	const JPH::SubShapeID &inSubShapeID2,
	JPH::RVec3Arg inContactPosition,
	JPH::Vec3Arg inContactNormal,
	JPH::Vec3Arg inContactVelocity,
	const JPH::PhysicsMaterial *inContactMaterial,
	JPH::Vec3Arg inCharacterVelocity,
	JPH::Vec3 &ioNewCharacterVelocity)
{
	(void)inSubShapeID2;
	(void)inContactMaterial;

	if (physics != nullptr &&
		IsWalkJumpLockedSourceSupportSideWallContact(*physics, inBodyID2, inContactPosition, inContactNormal)) {
		const JPH::Vec3 wallNormal = inContactNormal.NormalizedOr(JPH::Vec3::sZero());
		if (wallNormal.IsNearZero()) {
			ioNewCharacterVelocity = inCharacterVelocity;
			return;
		}

		const float intoWallVelocity = ioNewCharacterVelocity.Dot(wallNormal);
		if (intoWallVelocity < 0.0f) {
			ioNewCharacterVelocity -= wallNormal * intoWallVelocity;
		}

		const JPH::Vec3 up = inCharacter->GetUp();
		const float preSolveUpVelocity = inCharacterVelocity.Dot(up);
		const float postSolveUpVelocity = ioNewCharacterVelocity.Dot(up);
		if (postSolveUpVelocity < preSolveUpVelocity) {
			ioNewCharacterVelocity += up * (preSolveUpVelocity - postSolveUpVelocity);
		}
		return;
	}

	if (physics == nullptr || !physics->walkSuppressPassiveSlide) {
		return;
	}

	if (physics->walkJumpLockedSupport.valid ||
		physics->walkFootSupportScore < kWalkFootSupportEdgeGraceScore ||
		physics->walkSupportState == WalkSupportState::Air) {
		return;
	}

	if (!physics->walkPassiveSlideContact.valid ||
		physics->walkPassiveSlideContact.bodyId != inBodyID2 ||
		physics->walkPassiveSlideContact.subShapeId != inSubShapeID2) {
		return;
	}

	if (!inContactVelocity.IsNearZero() || inCharacter->IsSlopeTooSteep(inContactNormal)) {
		return;
	}

	const JPH::Vec3 up = inCharacter->GetUp();
	if (inCharacterVelocity.Dot(up) > 0.1f) {
		return;
	}
	if (inContactPosition.GetY() >
		inCharacter->GetPosition().GetY() + kWalkSupportContactMaxHeightAboveFeet) {
		return;
	}

	const JPH::Vec3 contactNormal = inContactNormal.Normalized();
	const float upDot = contactNormal.Dot(up);
	if (upDot <= 0.5f) {
		return;
	}

	const JPH::Vec3 uphill = up - contactNormal * upDot;
	const float uphillLengthSq = uphill.LengthSq();
	if (uphillLengthSq <= 1.0e-6f) {
		return;
	}

	const JPH::Vec3 downhill = -uphill * (1.0f / std::sqrt(uphillLengthSq));
	const float downhillVelocity = ioNewCharacterVelocity.Dot(downhill);
	if (downhillVelocity > 0.0f) {
		ioNewCharacterVelocity -= downhill * downhillVelocity;
	}
}

bool IsPhysicsSolidMaterial(const VoxelMaterial material)
{
	switch (material) {
	case VoxelMaterial::Glass:
	case VoxelMaterial::FloorWhite:
	case VoxelMaterial::FloorGray:
		return true;
	case VoxelMaterial::Air:
	case VoxelMaterial::Fluid:
		return false;
	}

	return false;
}

bool IsSolidAtPosition(const VoxelWorld &world, const std::array<float, 3> &position)
{
	const Int3 voxel = FloorToVoxel(position);
	return IsInsideVoxelWorld(world, voxel) && IsPhysicsSolidMaterial(GetVoxelMaterial(world, voxel));
}

float GetWalkEyeHeight(const PhysicsState &physics, const CameraState::ControlMode controlMode)
{
	return controlMode == CameraState::ControlMode::Walk && physics.walkSneakActive ? kWalkSneakEyeHeight : kWalkEyeHeight;
}

Float3 Normalize(const Float3 vector)
{
	const float length = std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
	if (length <= kPhysicsDirectionEpsilon) {
		return {};
	}

	return {
		vector.x / length,
		vector.y / length,
		vector.z / length,
	};
}

bool IsZeroVector(const Float3 vector)
{
	return std::abs(vector.x) <= kPhysicsDirectionEpsilon &&
		   std::abs(vector.y) <= kPhysicsDirectionEpsilon &&
		   std::abs(vector.z) <= kPhysicsDirectionEpsilon;
}

Float3 Cross(const Float3 a, const Float3 b)
{
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x,
	};
}

Float3 GetWalkForwardVector(const CameraState &camera)
{
	const Float3 forward{
		std::sin(camera.yawRadians),
		0.0f,
		-std::cos(camera.yawRadians),
	};
	return Normalize(forward);
}

JPH::RVec3 ToRVec3(const std::array<float, 3> &value)
{
	return {
		value[0],
		value[1],
		value[2],
	};
}

template <typename Vector3>
std::array<float, 3> ToArray(const Vector3 &value)
{
	return {
		static_cast<float>(value.GetX()),
		static_cast<float>(value.GetY()),
		static_cast<float>(value.GetZ()),
	};
}

int64_t ToWalkSupportProfilingValue(const WalkSupportState state)
{
	switch (state) {
	case WalkSupportState::Air:
		return 0;
	case WalkSupportState::EdgeGrace:
		return 1;
	case WalkSupportState::Grounded:
		return 2;
	}

	return 0;
}

int64_t ToWalkAirControlProfilingValue(const WalkAirControlMode mode)
{
	switch (mode) {
	case WalkAirControlMode::MinecraftLike:
		return 0;
	case WalkAirControlMode::Realistic:
		return 1;
	}

	return 0;
}

void PlotWalkProfilingState(
	const PhysicsState &physics,
	const std::array<float, 3> &feetPosition,
	const JPH::Vec3 &velocity)
{
	profiling::PlotValue("Walk Support State", ToWalkSupportProfilingValue(physics.walkSupportState));
	profiling::PlotValue("Walk Support Score", physics.walkFootSupportScore);
	profiling::PlotValue("Walk Feet Y", feetPosition[1]);
	profiling::PlotValue("Walk Velocity Y", velocity.GetY());
	profiling::PlotValue("Walk Air Control Mode", ToWalkAirControlProfilingValue(physics.walkAirControlMode));
	profiling::PlotValue("Walk Auto Jump", physics.walkAutoJumpEnabled ? int64_t{1} : int64_t{0});
	profiling::PlotValue("Walk Auto Jump Delay", physics.walkAutoJumpDelayEnabled ? int64_t{1} : int64_t{0});
	profiling::PlotValue("Walk Auto Jump Delay Frames", static_cast<int64_t>(physics.walkAutoJumpDelayFramesRemaining));
	profiling::PlotValue("Walk Sneak Active", physics.walkSneakActive ? int64_t{1} : int64_t{0});
	profiling::PlotValue("Walk Jump Lock", physics.walkJumpLockedSupport.valid ? int64_t{1} : int64_t{0});
	profiling::PlotValue(
		"Walk Jump Ballistic Lock",
		physics.walkJumpBallisticHorizontalVelocityActive ? int64_t{1} : int64_t{0});
	profiling::PlotValue(
		"Walk Cached Sneak Support",
		physics.walkCachedSneakSupportRegion.valid ? int64_t{1} : int64_t{0});
	profiling::PlotValue(
		"Walk Feet Inside Sneak Cache",
		physics.walkCachedSneakSupportRegion.valid &&
				IsWalkFeetInsideSneakSupportRegion(physics.walkCachedSneakSupportRegion, feetPosition)
			? int64_t{1}
			: int64_t{0});
	profiling::PlotValue("Walk Edge Grace", static_cast<int64_t>(physics.walkEdgeGraceFramesRemaining));
	profiling::PlotValue(
		"Walk Ground Takeoff Grace",
		static_cast<int64_t>(physics.walkGroundTakeoffGraceFramesRemaining));
	profiling::PlotValue(
		"Walk Sneak Support Grace",
		static_cast<int64_t>(physics.walkSneakSupportGraceFramesRemaining));
	profiling::PlotValue(
		"Walk Ledge Release Grace",
		static_cast<int64_t>(physics.walkLedgeReleaseGraceFramesRemaining));
	profiling::PlotValue(
		"Walk Ground Return Anchor",
		physics.walkGroundReturnAnchorValid ? int64_t{1} : int64_t{0});
}

void PlotBroadphaseDiagnostics(const PhysicsState &physics)
{
	PV_PROFILE_ZONE_N("PlotBroadphaseDiagnostics");
	const JPH::BodyManager::BodyStats stats = physics.physicsSystem.GetBodyStats();
	profiling::PlotValue("Physics Total Bodies", static_cast<int64_t>(stats.mNumBodies));
	profiling::PlotValue("Physics Max Bodies", static_cast<int64_t>(stats.mMaxBodies));
	profiling::PlotValue("Physics Static Bodies", static_cast<int64_t>(stats.mNumBodiesStatic));
	profiling::PlotValue("Physics Dynamic Bodies", static_cast<int64_t>(stats.mNumBodiesDynamic));
	profiling::PlotValue(
		"Physics Active Dynamic Bodies",
		static_cast<int64_t>(stats.mNumActiveBodiesDynamic));
	profiling::PlotValue(
		"Physics Kinematic Bodies",
		static_cast<int64_t>(stats.mNumBodiesKinematic));
	profiling::PlotValue(
		"Physics Active Kinematic Bodies",
		static_cast<int64_t>(stats.mNumActiveBodiesKinematic));
	profiling::PlotValue(
		"Physics Pending Chunk Rebuilds",
		static_cast<int64_t>(physics.pendingChunkRebuilds.size()));
	profiling::PlotValue(
		"Physics Chunk Static Bodies",
		static_cast<int64_t>(physics.chunkStaticBodies.size()));
	profiling::PlotValue(
		"Physics Chunk Merged Boxes Total",
		static_cast<int64_t>(physics.chunkMergedBoxes.size()));
}

void DestroyStaticWorldBody(PhysicsState &physics)
{
	if (!physics.staticWorldBodyId.IsInvalid()) {
		JPH::BodyInterface &bodyInterface = physics.physicsSystem.GetBodyInterface();
		bodyInterface.RemoveBody(physics.staticWorldBodyId);
		bodyInterface.DestroyBody(physics.staticWorldBodyId);
		physics.staticWorldBodyId = {};
	}

	physics.staticWorldShape = nullptr;
}

bool BuildStaticVoxelCollisionBody(PhysicsState &physics, const VoxelWorld &world)
{
	PV_PROFILE_ZONE_N("BuildStaticVoxelCollisionBody");
	JPH::StaticCompoundShapeSettings compoundSettings;

	std::vector<projectv::physics::MergedVoxelBox> mergedBoxes;
	if (projectv::physics::IsGreedyPhysicsMeshEnabled()) {
		projectv::physics::GreedyMergeSolidVoxelsInBounds(
			world,
			world.min,
			world.maxExclusive,
			mergedBoxes);
		profiling::PlotValue(
			"Physics Greedy Merge Box Count",
			static_cast<int64_t>(mergedBoxes.size()));
	}

	size_t solidVoxelCount = 0;
	if (!mergedBoxes.empty()) {
		for (const projectv::physics::MergedVoxelBox &box : mergedBoxes) {
			const int spanX = box.maxX - box.minX;
			const int spanY = box.maxY - box.minY;
			const int spanZ = box.maxZ - box.minZ;
			const float halfX = static_cast<float>(spanX) * 0.5f;
			const float halfY = static_cast<float>(spanY) * 0.5f;
			const float halfZ = static_cast<float>(spanZ) * 0.5f;
			const JPH::Vec3 halfExtent(halfX, halfY, halfZ);
			const JPH::Vec3 center(
				static_cast<float>(box.minX) + halfX,
				static_cast<float>(box.minY) + halfY,
				static_cast<float>(box.minZ) + halfZ);
			const JPH::RefConst<JPH::Shape> boxShape = new JPH::BoxShape(halfExtent);
			compoundSettings.AddShape(
				center,
				JPH::Quat::sIdentity(),
				boxShape.GetPtr());
			solidVoxelCount += static_cast<size_t>(spanX) *
				static_cast<size_t>(spanY) *
				static_cast<size_t>(spanZ);
		}
	} else {
		const JPH::RefConst<JPH::Shape> voxelShape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
		for (int z = world.min.z; z < world.maxExclusive.z; ++z) {
			for (int y = world.min.y; y < world.maxExclusive.y; ++y) {
				for (int x = world.min.x; x < world.maxExclusive.x; ++x) {
					const Int3 voxel{x, y, z};
					if (!IsPhysicsSolidMaterial(GetVoxelMaterial(world, voxel))) {
						continue;
					}

					compoundSettings.AddShape(
						JPH::Vec3(
							static_cast<float>(x) + 0.5f,
							static_cast<float>(y) + 0.5f,
							static_cast<float>(z) + 0.5f),
						JPH::Quat::sIdentity(),
						voxelShape.GetPtr());
					++solidVoxelCount;
				}
			}
		}
	}

	if (solidVoxelCount == 0) {
		return true;
	}

	const JPH::ShapeSettings::ShapeResult shapeResult = compoundSettings.Create(physics.tempAllocator);
	if (!shapeResult.IsValid()) {
		runtime::LogRuntimeFailure(
			"Physics",
			"BuildStaticVoxelCollisionBody.Create",
			shapeResult.GetError());
		return false;
	}

	physics.staticWorldShape = shapeResult.Get();
	const JPH::BodyCreationSettings worldBodySettings(
		physics.staticWorldShape,
		JPH::RVec3::sZero(),
		JPH::Quat::sIdentity(),
		JPH::EMotionType::Static,
		PhysicsLayers::Static);

	JPH::BodyInterface &bodyInterface = physics.physicsSystem.GetBodyInterface();
	const JPH::BodyID bodyId = bodyInterface.CreateAndAddBody(worldBodySettings, JPH::EActivation::DontActivate);
	if (bodyId.IsInvalid()) {
		runtime::LogRuntimeFailure(
			"Physics",
			"BuildStaticVoxelCollisionBody.CreateAndAddBody",
			"CreateAndAddBody returned an invalid body id");
		physics.staticWorldShape = nullptr;
		return false;
	}

	physics.staticWorldBodyId = bodyId;
	physics.physicsSystem.OptimizeBroadPhase();
	return true;
}

bool EnsureWalkCharacter(PhysicsState &physics)
{
	if (physics.walkCharacter != nullptr) {
		return true;
	}

	const auto ensureShape = [&](const float capsuleHalfHeight,
								 JPH::RefConst<JPH::Shape> &outShape,
								 const char *step) -> bool {
		if (outShape != nullptr) {
			return true;
		}

		const JPH::RotatedTranslatedShapeSettings characterShapeSettings(
			JPH::Vec3(0.0f, capsuleHalfHeight + kWalkCapsuleRadius, 0.0f),
			JPH::Quat::sIdentity(),
			new JPH::CapsuleShape(capsuleHalfHeight, kWalkCapsuleRadius));
		const JPH::ShapeSettings::ShapeResult characterShapeResult = characterShapeSettings.Create();
		if (!characterShapeResult.IsValid()) {
			runtime::LogRuntimeFailure(
				"Physics",
				step,
				characterShapeResult.GetError());
			return false;
		}

		outShape = characterShapeResult.Get();
		return true;
	};

	if (!ensureShape(kWalkCapsuleHalfHeight, physics.walkStandingShape, "EnsureWalkCharacter.CreateStandingShape")) {
		return false;
	}
	if (!ensureShape(kWalkSneakCapsuleHalfHeight, physics.walkSneakShape, "EnsureWalkCharacter.CreateSneakShape")) {
		return false;
	}

	const JPH::Ref characterSettings = new JPH::CharacterVirtualSettings();
	characterSettings->mShape = physics.walkStandingShape.GetPtr();
	characterSettings->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -kWalkCapsuleRadius);
	characterSettings->mEnhancedInternalEdgeRemoval = true;
	characterSettings->mPredictiveContactDistance = kWalkPredictiveContactDistance;
	characterSettings->mMaxStrength = 0.0f;
	characterSettings->mMass = 80.0f;

	physics.walkCharacter = new JPH::CharacterVirtual(
		characterSettings.GetPtr(),
		JPH::RVec3(0.0, 0.0, 0.0),
		JPH::Quat::sIdentity(),
		&physics.physicsSystem);
	physics.walkContactListener.physics = &physics;
	physics.walkCharacter->SetListener(&physics.walkContactListener);
	physics.walkCharacterInitialized = false;
	physics.walkSneakActive = false;
	return physics.walkCharacter != nullptr;
}

[[maybe_unused]] JPH::CharacterVirtual::ExtendedUpdateSettings BuildWalkUpdateSettings()
{
	JPH::CharacterVirtual::ExtendedUpdateSettings settings;
	settings.mStickToFloorStepDown = JPH::Vec3(0.0f, -kWalkStickToFloorDistance, 0.0f);
	settings.mWalkStairsStepUp = JPH::Vec3(0.0f, kWalkStairsStepUpHeight, 0.0f);
	return settings;
}

JPH::CharacterVirtual::ExtendedUpdateSettings BuildCreativeUpdateSettings()
{
	JPH::CharacterVirtual::ExtendedUpdateSettings settings;
	settings.mStickToFloorStepDown = JPH::Vec3::sZero();
	settings.mWalkStairsStepUp = JPH::Vec3::sZero();
	settings.mWalkStairsStepDownExtra = JPH::Vec3::sZero();
	return settings;
}

[[maybe_unused]] JPH::CharacterVirtual::ExtendedUpdateSettings BuildWalkEdgeGraceUpdateSettings()
{
	JPH::CharacterVirtual::ExtendedUpdateSettings settings;
	settings.mStickToFloorStepDown = JPH::Vec3::sZero();
	settings.mWalkStairsStepUp = JPH::Vec3::sZero();
	settings.mWalkStairsStepDownExtra = JPH::Vec3::sZero();
	return settings;
}

bool SetWalkSneakActive(
	PhysicsState &physics,
	const bool sneakActive)
{
	JPH::CharacterVirtual *character = physics.walkCharacter.GetPtr();
	if (character == nullptr || physics.walkSneakActive == sneakActive) {
		physics.walkSneakActive = sneakActive;
		return true;
	}

	const JPH::Shape *targetShape = sneakActive ? physics.walkSneakShape.GetPtr() : physics.walkStandingShape.GetPtr();
	if (targetShape == nullptr) {
		return false;
	}

	if (!character->SetShape(
			targetShape,
			sneakActive ? std::numeric_limits<float>::max() : kWalkSneakShapeMaxPenetrationDepth,
			physics.physicsSystem.GetDefaultBroadPhaseLayerFilter(PhysicsLayers::Moving),
			physics.physicsSystem.GetDefaultLayerFilter(PhysicsLayers::Moving),
			{},
			{},
			physics.tempAllocator)) {
		return false;
	}

	character->SetInnerBodyShape(targetShape);
	physics.walkSneakActive = sneakActive;
	return true;
}

void ClearWalkSneakSupportCache(PhysicsState &physics)
{
	physics.walkCachedSneakSupportRegion = {};
	physics.walkSneakSupportGraceFramesRemaining = 0;
	physics.walkLedgeReleaseGraceFramesRemaining = 0;
}

void ClearWalkJumpLockedSupport(PhysicsState &physics)
{
	physics.walkJumpLockedSupport = {};
}

void ClearWalkJumpBallisticHorizontalVelocity(PhysicsState &physics)
{
	physics.walkJumpBallisticHorizontalDirection = JPH::Vec3::sZero();
	physics.walkJumpBallisticHorizontalVelocity = JPH::Vec3::sZero();
	physics.walkJumpBallisticHorizontalTakeoffSpeed = 0.0f;
	physics.walkJumpBallisticHorizontalVelocityActive = false;
}

JPH::Vec3 MoveWalkJumpHorizontalVelocityTowards(
	const JPH::Vec3 &currentVelocity,
	const JPH::Vec3 &targetVelocity,
	const float maxSpeedDelta)
{
	const JPH::Vec3 deltaVelocity = targetVelocity - currentVelocity;
	const float deltaLength = deltaVelocity.Length();
	if (deltaLength <= kPhysicsDirectionEpsilon || deltaLength <= maxSpeedDelta) {
		return targetVelocity;
	}

	return currentVelocity + deltaVelocity * (maxSpeedDelta / deltaLength);
}

bool IsWalkJumpLockedSupportActive(const PhysicsState &physics)
{
	return physics.walkJumpLockedSupport.valid;
}

bool ShouldApplyWalkJumpLockedConstraint(const PhysicsState &physics)
{
	return physics.walkJumpLockedSupport.valid &&
		   physics.walkJumpLockedSupport.constrainMovementWhileSneakHeld &&
		   physics.walkSneakActive;
}

void UpdateWalkJumpLockedSupportTarget(
	PhysicsState &physics,
	const std::array<float, 3> &feetPosition)
{
	if (!physics.walkJumpLockedSupport.valid) {
		return;
	}

	physics.walkJumpLockedSupport.constrainedFeetPosition = feetPosition;
}

void PrimeWalkJumpLockedSupport(
	PhysicsState &physics,
	const WalkSneakSupportRegion &supportRegion,
	const std::array<float, 3> &anchorFeetPosition,
	const bool constrainMovementWhileSneakHeld)
{
	if (!supportRegion.valid) {
		ClearWalkJumpLockedSupport(physics);
		return;
	}

	physics.walkJumpLockedSupport.region = supportRegion;
	physics.walkJumpLockedSupport.anchorFeetPosition = anchorFeetPosition;
	physics.walkJumpLockedSupport.constrainedFeetPosition = anchorFeetPosition;
	physics.walkJumpLockedSupport.constrainMovementWhileSneakHeld = constrainMovementWhileSneakHeld;
	physics.walkJumpLockedSupport.valid = true;
}

void UpdateWalkJumpLockedSupportLifetime(PhysicsState &physics, const VoxelWorld &world)
{
	if (!physics.walkJumpLockedSupport.valid) {
		return;
	}

	const JPH::CharacterVirtual *character = physics.walkCharacter.GetPtr();
	if (character == nullptr) {
		ClearWalkJumpLockedSupport(physics);
		return;
	}

	if (physics.walkJumpLockedSupport.constrainMovementWhileSneakHeld && !physics.walkSneakActive) {
		ClearWalkJumpLockedSupport(physics);
		return;
	}

	const std::array<float, 3> currentFeetPosition = ToArray(character->GetPosition());
	const JPH::Vec3 velocity = character->GetLinearVelocity();
	const float referenceFeetY = physics.walkJumpLockedSupport.region.referenceFeetPosition[1];
	const float riseAboveReference = currentFeetPosition[1] - referenceFeetY;
	const float dropBelowReference = referenceFeetY - currentFeetPosition[1];
	const std::array<float, 2> projectedFeetXZ =
		ProjectWalkFeetToSneakSupportRegion(physics.walkJumpLockedSupport.region, currentFeetPosition);
	const float outsideX = projectedFeetXZ[0] - currentFeetPosition[0];
	const float outsideZ = projectedFeetXZ[1] - currentFeetPosition[2];
	const float outsideDistanceSq = outsideX * outsideX + outsideZ * outsideZ;
	float landedFeetY = 0.0f;
	const bool landed =
		velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon &&
		FindWalkBestSupportFeetYAtXZ(
			world,
			currentFeetPosition,
			0.0f,
			kWalkStickToFloorDistance,
			physics.walkSneakActive,
			kWalkGroundSupportRadius,
			landedFeetY) &&
		std::abs(landedFeetY - currentFeetPosition[1]) <= kWalkStickToFloorDistance + kWalkCollisionEpsilon;
	std::array<float, 3> landedOnSupportRegionFeetPosition = currentFeetPosition;
	landedOnSupportRegionFeetPosition[1] = referenceFeetY;
	const bool landedOnSupportRegion =
		velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon &&
		riseAboveReference <= kWalkCollisionEpsilon &&
		dropBelowReference <= kWalkJumpLockedSupportMaxDropBelowReference + kWalkCollisionEpsilon &&
		IsWalkFeetInsideSneakSupportRegion(
			physics.walkJumpLockedSupport.region,
			landedOnSupportRegionFeetPosition) &&
		IsWalkCharacterClearAt(world, landedOnSupportRegionFeetPosition, physics.walkSneakActive) &&
		HasWalkSneakSupport(world, landedOnSupportRegionFeetPosition);
	if (!physics.walkSneakActive && landedOnSupportRegion) {
		physics.walkGroundReturnAnchorFeetPosition = landedOnSupportRegionFeetPosition;
		physics.walkGroundReturnAnchorFramesRemaining = kWalkGroundReturnAnchorFrames;
		physics.walkGroundReturnAnchorValid = true;
		physics.walkEdgeGraceFramesRemaining = kWalkEdgeGraceFrames;
	}
	if (landed ||
		landedOnSupportRegion ||
		riseAboveReference > kWalkJumpLockedSupportMaxRiseAboveReference ||
		dropBelowReference > kWalkJumpLockedSupportMaxDropBelowReference ||
		outsideDistanceSq >
			kWalkJumpLockedSupportCeilingReprojectMaxDistance * kWalkJumpLockedSupportCeilingReprojectMaxDistance) {
		ClearWalkJumpLockedSupport(physics);
	}
}

void ReleaseWalkSneakSupportCacheIfUnused(PhysicsState &physics)
{
	if (physics.walkSneakSupportGraceFramesRemaining == 0 && physics.walkLedgeReleaseGraceFramesRemaining == 0) {
		physics.walkCachedSneakSupportRegion = {};
	}
}

void RefreshWalkCharacterContacts(PhysicsState &physics)
{
	JPH::CharacterVirtual *character = physics.walkCharacter.GetPtr();
	if (character == nullptr) {
		return;
	}

	character->RefreshContacts(
		physics.physicsSystem.GetDefaultBroadPhaseLayerFilter(PhysicsLayers::Moving),
		physics.physicsSystem.GetDefaultLayerFilter(PhysicsLayers::Moving),
		{},
		{},
		physics.tempAllocator);
	character->UpdateGroundVelocity();
}

WalkFootSupportInfo ComputeVoxelFootSupportInfo(const VoxelWorld &world, const std::array<float, 3> &feetPosition)
{
	WalkFootSupportInfo info{};
	constexpr float sampleRadius = kWalkFootSupportSampleRadius;
	constexpr float probeDepth = kWalkFootSupportProbeDepth;
	constexpr uint32_t sampleGridResolution = kWalkFootSupportSampleGridResolution;
	const float probeY = feetPosition[1] - probeDepth;
	std::array<float, 3> accumulatedSupport{};
	for (uint32_t gridZ = 0; gridZ < sampleGridResolution; ++gridZ) {
		for (uint32_t gridX = 0; gridX < sampleGridResolution; ++gridX) {
			const float normalizedX =
				(static_cast<float>(gridX) + 0.5f) / static_cast<float>(sampleGridResolution) * 2.0f -
				1.0f;
			const float normalizedZ =
				(static_cast<float>(gridZ) + 0.5f) / static_cast<float>(sampleGridResolution) * 2.0f -
				1.0f;
			const float sampleOffsetX = normalizedX * sampleRadius;
			const float sampleOffsetZ = normalizedZ * sampleRadius;
			if (sampleOffsetX * sampleOffsetX + sampleOffsetZ * sampleOffsetZ > sampleRadius * sampleRadius) {
				continue;
			}

			++info.totalSamples;
			const std::array samplePosition{
				feetPosition[0] + sampleOffsetX,
				probeY,
				feetPosition[2] + sampleOffsetZ,
			};
			if (!IsSolidAtPosition(world, samplePosition)) {
				continue;
			}

			++info.hitSamples;
			accumulatedSupport[0] += samplePosition[0];
			accumulatedSupport[1] += samplePosition[1];
			accumulatedSupport[2] += samplePosition[2];
		}
	}

	if (info.totalSamples > 0) {
		info.score = static_cast<float>(info.hitSamples) / static_cast<float>(info.totalSamples);
	}

	info.centroid = {
		feetPosition[0],
		probeY,
		feetPosition[2],
	};
	if (info.hitSamples > 0) {
		const float inverseHitCount = 1.0f / static_cast<float>(info.hitSamples);
		info.centroid = {
			accumulatedSupport[0] * inverseHitCount,
			accumulatedSupport[1] * inverseHitCount,
			accumulatedSupport[2] * inverseHitCount,
		};
	}

	return info;
}

std::array<float, 3> OffsetWalkFeetPosition(
	const std::array<float, 3> &feetPosition,
	const JPH::Vec3 &horizontalDelta)
{
	return {
		feetPosition[0] + horizontalDelta.GetX(),
		feetPosition[1],
		feetPosition[2] + horizontalDelta.GetZ(),
	};
}

float GetWalkCapsuleHalfHeight(const bool sneakActive)
{
	return sneakActive ? kWalkSneakCapsuleHalfHeight : kWalkCapsuleHalfHeight;
}

bool IsWalkCharacterClearAt(
	const VoxelWorld &world,
	const std::array<float, 3> &feetPosition,
	const bool sneakActive)
{
	const float totalHeight = 2.0f * (GetWalkCapsuleHalfHeight(sneakActive) + kWalkCapsuleRadius);
	const float minX = feetPosition[0] - kWalkCapsuleRadius + kWalkCollisionEpsilon;
	const float maxX = feetPosition[0] + kWalkCapsuleRadius - kWalkCollisionEpsilon;
	const float minY = feetPosition[1] + kWalkCollisionEpsilon;
	const float maxY = feetPosition[1] + totalHeight - kWalkCollisionEpsilon;
	const float minZ = feetPosition[2] - kWalkCapsuleRadius + kWalkCollisionEpsilon;
	const float maxZ = feetPosition[2] + kWalkCapsuleRadius - kWalkCollisionEpsilon;
	const int minVoxelX = static_cast<int>(std::floor(minX));
	const int maxVoxelX = static_cast<int>(std::floor(maxX));
	const int minVoxelY = static_cast<int>(std::floor(minY));
	const int maxVoxelY = static_cast<int>(std::floor(maxY));
	const int minVoxelZ = static_cast<int>(std::floor(minZ));
	const int maxVoxelZ = static_cast<int>(std::floor(maxZ));
	for (int voxelZ = minVoxelZ; voxelZ <= maxVoxelZ; ++voxelZ) {
		for (int voxelY = minVoxelY; voxelY <= maxVoxelY; ++voxelY) {
			for (int voxelX = minVoxelX; voxelX <= maxVoxelX; ++voxelX) {
				const Int3 voxel{voxelX, voxelY, voxelZ};
				if (!IsInsideVoxelWorld(world, voxel) || !IsPhysicsSolidMaterial(GetVoxelMaterial(world, voxel))) {
					continue;
				}

				const float voxelMinX = static_cast<float>(voxelX);
				const float voxelMaxX = voxelMinX + 1.0f;
				const float voxelMinY = static_cast<float>(voxelY);
				const float voxelMaxY = voxelMinY + 1.0f;
				const float voxelMinZ = static_cast<float>(voxelZ);
				const float voxelMaxZ = voxelMinZ + 1.0f;
				if (maxX <= voxelMinX + kWalkCollisionEpsilon ||
					minX >= voxelMaxX - kWalkCollisionEpsilon ||
					maxY <= voxelMinY + kWalkCollisionEpsilon ||
					minY >= voxelMaxY - kWalkCollisionEpsilon ||
					maxZ <= voxelMinZ + kWalkCollisionEpsilon ||
					minZ >= voxelMaxZ - kWalkCollisionEpsilon) {
					continue;
				}

				return false;
			}
		}
	}

	return true;
}

bool HasWalkSneakSupport(const VoxelWorld &world, const std::array<float, 3> &feetPosition)
{
	const WalkSneakSupportRegion supportRegion = ComputeWalkSneakSupportRegion(world, feetPosition);
	return supportRegion.valid && IsWalkFeetInsideSneakSupportRegion(supportRegion, feetPosition);
}

[[maybe_unused]] float GetWalkSneakDistanceOutsideInterval(const float value, const float minBound, const float maxBound)
{
	if (value < minBound) {
		return minBound - value;
	}

	if (value > maxBound) {
		return value - maxBound;
	}

	return 0.0f;
}

bool IsWalkFeetInsideSneakBackoffRegion(
	const WalkSneakSupportRegion &region,
	const std::array<float, 3> &feetPosition)
{
	if (feetPosition[0] < region.boundsMin[0] + kWalkSneakBackoffInset ||
		feetPosition[0] > region.boundsMax[0] - kWalkSneakBackoffInset ||
		feetPosition[2] < region.boundsMin[1] + kWalkSneakBackoffInset ||
		feetPosition[2] > region.boundsMax[1] - kWalkSneakBackoffInset) {
		return false;
	}

	for (uint32_t faceIndex = 0; faceIndex < region.faceCount; ++faceIndex) {
		const auto &[min, max] = region.faces[faceIndex];
		if (feetPosition[0] >= min[0] + kWalkSneakBackoffInset &&
			feetPosition[0] <= max[0] - kWalkSneakBackoffInset &&
			feetPosition[2] >= min[1] + kWalkSneakBackoffInset &&
			feetPosition[2] <= max[1] - kWalkSneakBackoffInset) {
			return true;
		}
	}

	return false;
}

bool IsWalkFeetInsideSneakSupportFace(
	const WalkSneakSupportFace &face,
	const float sampleRadius,
	const std::array<float, 3> &feetPosition)
{

	const float supportRadius = std::max(sampleRadius, kWalkCapsuleRadius);
	const float minX = face.min[0] + kWalkSneakSupportRegionExtent - supportRadius;
	const float maxX = face.max[0] - kWalkSneakSupportRegionExtent + supportRadius;
	const float minZ = face.min[1] + kWalkSneakSupportRegionExtent - supportRadius;
	const float maxZ = face.max[1] - kWalkSneakSupportRegionExtent + supportRadius;
	return feetPosition[0] >= minX - kWalkSneakOutwardDriftEpsilon &&
		   feetPosition[0] <= maxX + kWalkSneakOutwardDriftEpsilon &&
		   feetPosition[2] >= minZ - kWalkSneakOutwardDriftEpsilon &&
		   feetPosition[2] <= maxZ + kWalkSneakOutwardDriftEpsilon;
}

std::array<float, 2> ProjectWalkFeetToSneakSupportFace(
	const WalkSneakSupportFace &face,
	const float sampleRadius,
	const std::array<float, 2> &feetXZ)
{
	const float supportRadius = std::max(sampleRadius, kWalkCapsuleRadius);
	const float minX = face.min[0] + kWalkSneakSupportRegionExtent - supportRadius;
	const float maxX = face.max[0] - kWalkSneakSupportRegionExtent + supportRadius;
	const float minZ = face.min[1] + kWalkSneakSupportRegionExtent - supportRadius;
	const float maxZ = face.max[1] - kWalkSneakSupportRegionExtent + supportRadius;
	return {
		std::clamp(feetXZ[0], minX, maxX),
		std::clamp(feetXZ[1], minZ, maxZ),
	};
}

std::array<float, 2> ProjectWalkFeetToSneakSupportRegion(
	const WalkSneakSupportRegion &region,
	const std::array<float, 3> &feetPosition)
{
	const std::array feetXZ{
		feetPosition[0],
		feetPosition[2],
	};
	if (!region.valid || region.faceCount == 0) {
		return feetXZ;
	}

	if (IsWalkFeetInsideSneakSupportRegion(region, feetPosition)) {
		return feetXZ;
	}

	std::array<float, 2> bestProjection = feetXZ;
	float bestDistanceSq = std::numeric_limits<float>::max();
	bool hasBestProjection = false;
	for (uint32_t faceIndex = 0; faceIndex < region.faceCount; ++faceIndex) {
		const std::array<float, 2> projection =
			ProjectWalkFeetToSneakSupportFace(region.faces[faceIndex], region.sampleRadius, feetXZ);
		const float distanceX = projection[0] - feetXZ[0];
		const float distanceZ = projection[1] - feetXZ[1];
		const float distanceSq = distanceX * distanceX + distanceZ * distanceZ;
		if (!hasBestProjection || distanceSq < bestDistanceSq) {
			bestProjection = projection;
			bestDistanceSq = distanceSq;
			hasBestProjection = true;
		}
	}

	return hasBestProjection ? bestProjection : feetXZ;
}

WalkSneakSupportRegion ComputeWalkSneakSupportRegion(
	const VoxelWorld &world,
	const std::array<float, 3> &feetPosition)
{
	WalkSneakSupportRegion region{};
	region.referenceFeetPosition = feetPosition;
	constexpr float searchExtent = kWalkSneakSupportRegionExtent + kWalkCapsuleRadius;
	const int supportVoxelY = FloorToVoxel(
								  {
									  feetPosition[0],
									  feetPosition[1] - kWalkSpawnClearance - kWalkGroundProbeEpsilon,
									  feetPosition[2],
								  })
								  .y;
	const int minVoxelX = static_cast<int>(std::floor(feetPosition[0] - searchExtent)) - 1;
	const int maxVoxelX = static_cast<int>(std::floor(feetPosition[0] + searchExtent)) + 1;
	const int minVoxelZ = static_cast<int>(std::floor(feetPosition[2] - searchExtent)) - 1;
	const int maxVoxelZ = static_cast<int>(std::floor(feetPosition[2] + searchExtent)) + 1;
	bool hasBounds = false;
	for (int voxelZ = minVoxelZ; voxelZ <= maxVoxelZ; ++voxelZ) {
		for (int voxelX = minVoxelX; voxelX <= maxVoxelX; ++voxelX) {
			const Int3 voxel{voxelX, supportVoxelY, voxelZ};
			if (!IsInsideVoxelWorld(world, voxel) || !IsPhysicsSolidMaterial(GetVoxelMaterial(world, voxel))) {
				continue;
			}

			if (region.faceCount >= region.faces.size()) {
				continue;
			}

			auto &[min, max] = region.faces[region.faceCount++];
			min = {
				static_cast<float>(voxelX) - kWalkSneakSupportRegionExtent,
				static_cast<float>(voxelZ) - kWalkSneakSupportRegionExtent,
			};
			max = {
				static_cast<float>(voxelX + 1) + kWalkSneakSupportRegionExtent,
				static_cast<float>(voxelZ + 1) + kWalkSneakSupportRegionExtent,
			};

			if (!hasBounds) {
				region.boundsMin = min;
				region.boundsMax = max;
				hasBounds = true;
			} else {
				region.boundsMin[0] = std::min(region.boundsMin[0], min[0]);
				region.boundsMin[1] = std::min(region.boundsMin[1], min[1]);
				region.boundsMax[0] = std::max(region.boundsMax[0], max[0]);
				region.boundsMax[1] = std::max(region.boundsMax[1], max[1]);
			}
		}
	}

	if (region.faceCount > 0) {

		region.referenceFeetPosition[1] = static_cast<float>(supportVoxelY + 1) + kWalkSpawnClearance;
	}
	region.valid = region.faceCount > 0;
	return region;
}

bool IsWalkFeetInsideSneakSupportRegion(
	const WalkSneakSupportRegion &region,
	const std::array<float, 3> &feetPosition)
{
	if (!region.valid) {
		return false;
	}

	if (feetPosition[0] < region.boundsMin[0] - kWalkSneakOutwardDriftEpsilon ||
		feetPosition[0] > region.boundsMax[0] + kWalkSneakOutwardDriftEpsilon ||
		feetPosition[2] < region.boundsMin[1] - kWalkSneakOutwardDriftEpsilon ||
		feetPosition[2] > region.boundsMax[1] + kWalkSneakOutwardDriftEpsilon) {
		return false;
	}
	if (feetPosition[1] + kWalkSneakStickToFloorDistance <
		region.referenceFeetPosition[1] - kWalkCollisionEpsilon) {
		return false;
	}

	for (uint32_t faceIndex = 0; faceIndex < region.faceCount; ++faceIndex) {
		if (IsWalkFeetInsideSneakSupportFace(region.faces[faceIndex], region.sampleRadius, feetPosition)) {
			return true;
		}
	}

	return false;
}

float GetWalkBodyHeight(const bool sneakActive)
{
	return 2.0f * (GetWalkCapsuleHalfHeight(sneakActive) + kWalkCapsuleRadius);
}

float GetWalkSupportPlaneY(const float feetY)
{
	return feetY - kWalkSpawnClearance;
}

bool DoesWalkFootprintOverlapVoxel(
	const float centerX,
	const float centerZ,
	const int voxelX,
	const int voxelZ,
	const float footprintRadius)
{
	const float minX = centerX - footprintRadius + kWalkCollisionEpsilon;
	const float maxX = centerX + footprintRadius - kWalkCollisionEpsilon;
	const float minZ = centerZ - footprintRadius + kWalkCollisionEpsilon;
	const float maxZ = centerZ + footprintRadius - kWalkCollisionEpsilon;
	const float voxelMinX = static_cast<float>(voxelX);
	const float voxelMaxX = voxelMinX + 1.0f;
	const float voxelMinZ = static_cast<float>(voxelZ);
	const float voxelMaxZ = voxelMinZ + 1.0f;
	return maxX > voxelMinX + kWalkCollisionEpsilon &&
		   minX < voxelMaxX - kWalkCollisionEpsilon &&
		   maxZ > voxelMinZ + kWalkCollisionEpsilon &&
		   minZ < voxelMaxZ - kWalkCollisionEpsilon;
}

bool FindWalkBestSupportFeetYAtXZ(
	const VoxelWorld &world,
	const std::array<float, 3> &referenceFeetPosition,
	const float maxRise,
	const float maxDrop,
	const bool sneakActive,
	const float footprintRadius,
	float &outFeetY)
{
	const float supportPlaneY = GetWalkSupportPlaneY(referenceFeetPosition[1]);
	const int minVoxelX = static_cast<int>(std::floor(referenceFeetPosition[0] - kWalkCapsuleRadius));
	const int maxVoxelX = static_cast<int>(std::floor(referenceFeetPosition[0] + kWalkCapsuleRadius));
	const int minVoxelZ = static_cast<int>(std::floor(referenceFeetPosition[2] - kWalkCapsuleRadius));
	const int maxVoxelZ = static_cast<int>(std::floor(referenceFeetPosition[2] + kWalkCapsuleRadius));
	const int minVoxelY = static_cast<int>(std::floor(supportPlaneY - maxDrop - 1.0f));
	const int maxVoxelY = static_cast<int>(std::floor(supportPlaneY + maxRise + 1.0f));
	float bestFeetY = 0.0f;
	bool hasBestFeetY = false;
	for (int voxelY = minVoxelY; voxelY <= maxVoxelY; ++voxelY) {
		for (int voxelZ = minVoxelZ; voxelZ <= maxVoxelZ; ++voxelZ) {
			for (int voxelX = minVoxelX; voxelX <= maxVoxelX; ++voxelX) {
				const Int3 voxel{voxelX, voxelY, voxelZ};
				if (!IsInsideVoxelWorld(world, voxel) || !IsPhysicsSolidMaterial(GetVoxelMaterial(world, voxel))) {
					continue;
				}

				if (!DoesWalkFootprintOverlapVoxel(
						referenceFeetPosition[0],
						referenceFeetPosition[2],
						voxelX,
						voxelZ,
						footprintRadius)) {
					continue;
				}

				const float candidateFeetY = static_cast<float>(voxelY + 1) + kWalkSpawnClearance;
				const float rise = candidateFeetY - referenceFeetPosition[1];
				const float drop = referenceFeetPosition[1] - candidateFeetY;
				if (rise > maxRise + kWalkCollisionEpsilon || drop > maxDrop + kWalkCollisionEpsilon) {
					continue;
				}

				const std::array candidateFeetPosition{
					referenceFeetPosition[0],
					candidateFeetY,
					referenceFeetPosition[2],
				};
				if (!IsWalkCharacterClearAt(world, candidateFeetPosition, sneakActive)) {
					continue;
				}

				if (!hasBestFeetY || candidateFeetY > bestFeetY + kWalkCollisionEpsilon) {
					bestFeetY = candidateFeetY;
					hasBestFeetY = true;
				}
			}
		}
	}

	if (!hasBestFeetY) {
		return false;
	}

	outFeetY = bestFeetY;
	return true;
}

WalkTopSupportCandidate FindWalkTopSupportCandidate(
	const VoxelWorld &world,
	const std::array<float, 3> &desiredFeetPosition,
	const float maxRise,
	const bool sneakActive)
{
	WalkTopSupportCandidate candidate{};
	float candidateFeetY = 0.0f;
	if (!FindWalkBestSupportFeetYAtXZ(
			world,
			desiredFeetPosition,
			maxRise,
			0.0f,
			sneakActive,
			kWalkCapsuleRadius,
			candidateFeetY)) {
		return candidate;
	}

	candidate.feetPosition = desiredFeetPosition;
	candidate.feetPosition[1] = candidateFeetY;
	candidate.region = ComputeWalkSneakSupportRegion(world, candidate.feetPosition);
	candidate.valid = candidate.region.valid;
	return candidate;
}

bool IsWalkAutoJumpRiseInRange(const float rise)
{
	return rise >= kWalkAutoJumpMinRise - kWalkCollisionEpsilon &&
		   rise <= kWalkAutoJumpMaxRise + kWalkCollisionEpsilon;
}

bool ResolveWalkCharacterPenetration(
	const VoxelWorld &world,
	std::array<float, 3> &feetPosition,
	const bool sneakActive,
	const bool allowUpwardResolve)
{
	bool changed = false;
	for (int iteration = 0; iteration < 4; ++iteration) {
		if (IsWalkCharacterClearAt(world, feetPosition, sneakActive)) {
			return changed;
		}

		const float totalHeight = GetWalkBodyHeight(sneakActive);
		const float minX = feetPosition[0] - kWalkCapsuleRadius + kWalkCollisionEpsilon;
		const float maxX = feetPosition[0] + kWalkCapsuleRadius - kWalkCollisionEpsilon;
		const float minY = feetPosition[1] + kWalkCollisionEpsilon;
		const float maxY = feetPosition[1] + totalHeight - kWalkCollisionEpsilon;
		const float minZ = feetPosition[2] - kWalkCapsuleRadius + kWalkCollisionEpsilon;
		const float maxZ = feetPosition[2] + kWalkCapsuleRadius - kWalkCollisionEpsilon;
		const int minVoxelX = static_cast<int>(std::floor(minX));
		const int maxVoxelX = static_cast<int>(std::floor(maxX));
		const int minVoxelY = static_cast<int>(std::floor(minY));
		const int maxVoxelY = static_cast<int>(std::floor(maxY));
		const int minVoxelZ = static_cast<int>(std::floor(minZ));
		const int maxVoxelZ = static_cast<int>(std::floor(maxZ));

		float bestTranslation = 0.0f;
		int bestAxis = -1;
		for (int voxelZ = minVoxelZ; voxelZ <= maxVoxelZ; ++voxelZ) {
			for (int voxelY = minVoxelY; voxelY <= maxVoxelY; ++voxelY) {
				for (int voxelX = minVoxelX; voxelX <= maxVoxelX; ++voxelX) {
					const Int3 voxel{voxelX, voxelY, voxelZ};
					if (!IsInsideVoxelWorld(world, voxel) || !IsPhysicsSolidMaterial(GetVoxelMaterial(world, voxel))) {
						continue;
					}

					const float voxelMinX = static_cast<float>(voxelX);
					const float voxelMaxX = voxelMinX + 1.0f;
					const float voxelMinY = static_cast<float>(voxelY);
					const float voxelMaxY = voxelMinY + 1.0f;
					const float voxelMinZ = static_cast<float>(voxelZ);
					const float voxelMaxZ = voxelMinZ + 1.0f;
					if (maxX <= voxelMinX + kWalkCollisionEpsilon ||
						minX >= voxelMaxX - kWalkCollisionEpsilon ||
						maxY <= voxelMinY + kWalkCollisionEpsilon ||
						minY >= voxelMaxY - kWalkCollisionEpsilon ||
						maxZ <= voxelMinZ + kWalkCollisionEpsilon ||
						minZ >= voxelMaxZ - kWalkCollisionEpsilon) {
						continue;
					}

					const std::array translations{
						voxelMinX - maxX - kWalkPenetrationResolveEpsilon,
						voxelMaxX - minX + kWalkPenetrationResolveEpsilon,
						voxelMinY - maxY - kWalkPenetrationResolveEpsilon,
						voxelMaxY - minY + kWalkPenetrationResolveEpsilon,
						voxelMinZ - maxZ - kWalkPenetrationResolveEpsilon,
						voxelMaxZ - minZ + kWalkPenetrationResolveEpsilon,
					};
					for (int axisIndex = 0; axisIndex < static_cast<int>(translations.size()); ++axisIndex) {
						if (!allowUpwardResolve && axisIndex == 3) {
							continue;
						}

						const float translation = translations[axisIndex];
						if (bestAxis < 0 || std::abs(translation) < std::abs(bestTranslation)) {
							bestTranslation = translation;
							bestAxis = axisIndex;
						}
					}
				}
			}
		}

		if (bestAxis < 0 || std::abs(bestTranslation) <= kWalkPenetrationResolveEpsilon) {
			return changed;
		}

		const int translationComponent = bestAxis / 2;
		if (translationComponent < 0 || translationComponent >= static_cast<int>(feetPosition.size())) {
			return changed;
		}
		feetPosition[translationComponent] += bestTranslation;
		changed = true;
	}

	return changed;
}

bool TryBuildWalkSupportCorrectedFeetPosition(
	const PhysicsState &physics,
	const VoxelWorld &world,
	const WalkSneakSupportRegion &supportRegion,
	const std::array<float, 3> &sourceFeetPosition,
	const float maxHorizontalCorrection,
	const float maxUpwardRestore,
	std::array<float, 3> &outFeetPosition)
{
	if (!supportRegion.valid) {
		return false;
	}

	const JPH::CharacterVirtual *character = physics.walkCharacter.GetPtr();
	if (character == nullptr) {
		return false;
	}

	const std::array<float, 2> projectedFeetXZ = ProjectWalkFeetToSneakSupportRegion(supportRegion, sourceFeetPosition);
	const float correctionX = projectedFeetXZ[0] - sourceFeetPosition[0];
	const float correctionZ = projectedFeetXZ[1] - sourceFeetPosition[2];
	if (correctionX * correctionX + correctionZ * correctionZ >
		maxHorizontalCorrection * maxHorizontalCorrection) {
		return false;
	}

	std::array<float, 3> correctedFeetPosition = sourceFeetPosition;
	correctedFeetPosition[0] = projectedFeetXZ[0];
	correctedFeetPosition[2] = projectedFeetXZ[1];

	const float referenceFeetY = supportRegion.referenceFeetPosition[1];
	const float upwardRestore = referenceFeetY - sourceFeetPosition[1];
	if (upwardRestore > kWalkSneakOutwardDriftEpsilon) {
		if (upwardRestore > maxUpwardRestore) {
			return false;
		}

		correctedFeetPosition[1] = referenceFeetY;
	}

	if (!IsWalkFeetInsideSneakSupportRegion(supportRegion, correctedFeetPosition) ||
		!HasWalkSneakSupport(world, correctedFeetPosition) ||
		!IsWalkCharacterClearAt(world, correctedFeetPosition, physics.walkSneakActive)) {
		return false;
	}

	outFeetPosition = correctedFeetPosition;
	return true;
}

bool TryBuildWalkJumpTakeoffFeetPosition(
	const PhysicsState &physics,
	const VoxelWorld &world,
	const WalkSneakSupportRegion &supportRegion,
	std::array<float, 3> &outFeetPosition)
{
	const JPH::CharacterVirtual *character = physics.walkCharacter.GetPtr();
	if (character == nullptr) {
		return false;
	}

	return TryBuildWalkSupportCorrectedFeetPosition(
		physics,
		world,
		supportRegion,
		ToArray(character->GetPosition()),
		kWalkSneakStickToFloorDistance,
		kWalkSneakStickToFloorDistance,
		outFeetPosition);
}

bool TryReacquireWalkSneakGroundSupport(
	PhysicsState &physics,
	const VoxelWorld &world,
	std::array<float, 3> &feetPosition,
	JPH::Vec3 &velocity)
{
	if (!physics.walkSneakActive) {
		return false;
	}

	if (velocity.GetY() > kWalkSneakStickPositiveVelocityEpsilon) {
		return false;
	}

	std::array<WalkSneakSupportRegion, 2> candidateRegions{};
	uint32_t candidateCount = 0;
	if (physics.walkCachedSneakSupportRegion.valid) {
		candidateRegions[candidateCount++] = physics.walkCachedSneakSupportRegion;
	}

	const WalkSneakSupportRegion currentSupportRegion = ComputeWalkSneakSupportRegion(world, feetPosition);
	if (currentSupportRegion.valid) {
		candidateRegions[candidateCount++] = currentSupportRegion;
	}

	for (uint32_t candidateIndex = 0; candidateIndex < candidateCount; ++candidateIndex) {
		const WalkSneakSupportRegion &candidateRegion = candidateRegions[candidateIndex];
		std::array<float, 3> correctedFeetPosition{};
		if (!TryBuildWalkSupportCorrectedFeetPosition(
				physics,
				world,
				candidateRegion,
				feetPosition,
				kWalkCapsuleRadius,
				kWalkSneakStickToFloorDistance,
				correctedFeetPosition)) {
			continue;
		}

		feetPosition = correctedFeetPosition;
		if (velocity.GetY() < 0.0f) {
			velocity.SetY(0.0f);
		}
		physics.walkCachedSneakSupportRegion = candidateRegion;
		physics.walkSneakSupportGraceFramesRemaining = kWalkSneakSupportGraceFrames;
		return true;
	}

	return false;
}

bool CanWalkSneakMoveInsideSupportRegion(
	const VoxelWorld &world,
	const WalkSneakSupportRegion &supportRegion,
	const std::array<float, 3> &feetPosition,
	const bool sneakActive)
{
	if (!supportRegion.valid) {
		return true;
	}

	std::array<float, 3> candidateFeetPosition = feetPosition;
	candidateFeetPosition[1] = supportRegion.referenceFeetPosition[1];
	return IsWalkFeetInsideSneakBackoffRegion(supportRegion, candidateFeetPosition) &&
		   IsWalkCharacterClearAt(world, candidateFeetPosition, sneakActive) &&
		   HasWalkSneakSupport(world, candidateFeetPosition);
}

float BackOffWalkSneakDeltaComponent(const float delta)
{
	if (std::abs(delta) <= kWalkHorizontalSubstepDistance + kWalkCollisionEpsilon) {
		return 0.0f;
	}

	return delta - std::copysign(kWalkHorizontalSubstepDistance, delta);
}

JPH::Vec3 ApplyWalkSneakBackoff(
	const VoxelWorld &world,
	const WalkSneakSupportRegion &supportRegion,
	const std::array<float, 3> &feetPosition,
	const JPH::Vec3 &desiredHorizontalDelta,
	const bool sneakActive)
{
	if (desiredHorizontalDelta.IsNearZero()) {
		return desiredHorizontalDelta;
	}

	const auto canMoveTo = [&](const float deltaX, const float deltaZ) -> bool {
		std::array<float, 3> candidateFeetPosition = feetPosition;
		candidateFeetPosition[0] += deltaX;
		candidateFeetPosition[2] += deltaZ;
		return CanWalkSneakMoveInsideSupportRegion(world, supportRegion, candidateFeetPosition, sneakActive);
	};

	float deltaX = desiredHorizontalDelta.GetX();
	float deltaZ = desiredHorizontalDelta.GetZ();
	if (canMoveTo(deltaX, deltaZ)) {
		return desiredHorizontalDelta;
	}

	while (std::abs(deltaX) > kWalkCollisionEpsilon && !canMoveTo(deltaX, 0.0f)) {
		deltaX = BackOffWalkSneakDeltaComponent(deltaX);
	}

	while (std::abs(deltaZ) > kWalkCollisionEpsilon && !canMoveTo(0.0f, deltaZ)) {
		deltaZ = BackOffWalkSneakDeltaComponent(deltaZ);
	}

	while ((std::abs(deltaX) > kWalkCollisionEpsilon || std::abs(deltaZ) > kWalkCollisionEpsilon) &&
		   !canMoveTo(deltaX, deltaZ)) {
		deltaX = BackOffWalkSneakDeltaComponent(deltaX);
		deltaZ = BackOffWalkSneakDeltaComponent(deltaZ);
	}

	return JPH::Vec3(deltaX, 0.0f, deltaZ);
}

bool TryRestoreWalkSupportRegionPlane(
	const VoxelWorld &world,
	const WalkSneakSupportRegion &supportRegion,
	std::array<float, 3> &feetPosition,
	JPH::Vec3 &velocity,
	const bool sneakActive)
{
	const float targetFeetY = supportRegion.referenceFeetPosition[1];
	if (feetPosition[1] >= targetFeetY - kWalkCollisionEpsilon) {
		if (feetPosition[1] > targetFeetY + kWalkCollisionEpsilon) {
			return false;
		}
		feetPosition[1] = targetFeetY;
		if (velocity.GetY() < 0.0f) {
			velocity.SetY(0.0f);
		}
		return true;
	}

	std::array<float, 3> candidateFeetPosition = feetPosition;
	candidateFeetPosition[1] = targetFeetY;
	if (!IsWalkCharacterClearAt(world, candidateFeetPosition, sneakActive)) {
		return false;
	}

	feetPosition = candidateFeetPosition;
	if (velocity.GetY() < 0.0f) {
		velocity.SetY(0.0f);
	}
	return true;
}

bool SweepWalkVertical(
	const VoxelWorld &world,
	std::array<float, 3> &feetPosition,
	JPH::Vec3 &velocity,
	const float deltaSeconds,
	const bool sneakActive)
{
	if (deltaSeconds <= 0.0f) {
		return false;
	}

	const float deltaY = velocity.GetY() * deltaSeconds;
	if (std::abs(deltaY) <= kPhysicsDirectionEpsilon) {
		return false;
	}

	const float totalHeight = GetWalkBodyHeight(sneakActive);
	const float footprintMinX = feetPosition[0] - kWalkCapsuleRadius + kWalkCollisionEpsilon;
	const float footprintMaxX = feetPosition[0] + kWalkCapsuleRadius - kWalkCollisionEpsilon;
	const float footprintMinZ = feetPosition[2] - kWalkCapsuleRadius + kWalkCollisionEpsilon;
	const float footprintMaxZ = feetPosition[2] + kWalkCapsuleRadius - kWalkCollisionEpsilon;
	const int minVoxelX = static_cast<int>(std::floor(footprintMinX));
	const int maxVoxelX = static_cast<int>(std::floor(footprintMaxX));
	const int minVoxelZ = static_cast<int>(std::floor(footprintMinZ));
	const int maxVoxelZ = static_cast<int>(std::floor(footprintMaxZ));

	if (deltaY > 0.0f) {
		const float currentTopY = feetPosition[1] + totalHeight;
		const float targetTopY = currentTopY + deltaY;
		const int minVoxelY = static_cast<int>(std::floor(currentTopY));
		const int maxVoxelY = static_cast<int>(std::floor(targetTopY));
		float clampedFeetY = feetPosition[1] + deltaY;
		bool hitCeiling = false;
		for (int voxelZ = minVoxelZ; voxelZ <= maxVoxelZ; ++voxelZ) {
			for (int voxelY = minVoxelY; voxelY <= maxVoxelY; ++voxelY) {
				for (int voxelX = minVoxelX; voxelX <= maxVoxelX; ++voxelX) {
					const Int3 voxel{voxelX, voxelY, voxelZ};
					if (!IsInsideVoxelWorld(world, voxel) || !IsPhysicsSolidMaterial(GetVoxelMaterial(world, voxel))) {
						continue;
					}

					const float voxelMinY = static_cast<float>(voxelY);
					if (voxelMinY < currentTopY - kWalkCollisionEpsilon ||
						voxelMinY > targetTopY + kWalkCollisionEpsilon) {
						continue;
					}

					const float candidateFeetY = voxelMinY - totalHeight - kWalkCollisionEpsilon;
					if (!hitCeiling || candidateFeetY < clampedFeetY) {
						clampedFeetY = candidateFeetY;
						hitCeiling = true;
					}
				}
			}
		}

		feetPosition[1] = clampedFeetY;
		if (hitCeiling && velocity.GetY() > 0.0f) {
			velocity.SetY(0.0f);
		}
		return hitCeiling;
	}

	const std::array referenceFeetPosition{
		feetPosition[0],
		feetPosition[1],
		feetPosition[2],
	};
	float candidateFeetY = 0.0f;
	if (FindWalkBestSupportFeetYAtXZ(
			world,
			referenceFeetPosition,
			0.0f,
			-deltaY + kWalkCollisionEpsilon,
			sneakActive,
			kWalkGroundSupportRadius,
			candidateFeetY) &&
		feetPosition[1] + deltaY < candidateFeetY - kWalkCollisionEpsilon) {
		feetPosition[1] = candidateFeetY;
		if (velocity.GetY() < 0.0f) {
			velocity.SetY(0.0f);
		}
		return true;
	}

	feetPosition[1] += deltaY;
	return false;
}

template <bool tAllowStepUp>
bool TryMoveWalkSubstep(
	const VoxelWorld &world,
	const std::array<float, 3> &currentFeetPosition,
	const JPH::Vec3 &substepDelta,
	const bool sneakActive,
	std::array<float, 3> &outFeetPosition)
{
	const auto tryMoveTo = [&](const std::array<float, 3> &targetFeetPosition, std::array<float, 3> &outCandidate) -> bool {
		if (IsWalkCharacterClearAt(world, targetFeetPosition, sneakActive)) {
			outCandidate = targetFeetPosition;
			return true;
		}

		if constexpr (!tAllowStepUp) {
			return false;
		}

		const WalkTopSupportCandidate topCandidate =
			FindWalkTopSupportCandidate(world, targetFeetPosition, kWalkStairsStepUpHeight, sneakActive);
		if (!topCandidate.valid) {
			return false;
		}

		outCandidate = topCandidate.feetPosition;
		return true;
	};

	const std::array<float, 3> desiredFeetPosition = OffsetWalkFeetPosition(currentFeetPosition, substepDelta);
	if (tryMoveTo(desiredFeetPosition, outFeetPosition)) {
		return true;
	}

	const auto scoreProgress = [&](const std::array<float, 3> &candidateFeetPosition) -> float {
		const float deltaX = candidateFeetPosition[0] - currentFeetPosition[0];
		const float deltaZ = candidateFeetPosition[2] - currentFeetPosition[2];
		return deltaX * deltaX + deltaZ * deltaZ;
	};

	std::array<float, 3> bestFeetPosition = currentFeetPosition;
	float bestProgress = 0.0f;
	for (int order = 0; order < 2; ++order) {
		std::array<float, 3> candidateFeetPosition = currentFeetPosition;
		const float firstDelta = order == 0 ? substepDelta.GetX() : substepDelta.GetZ();
		const float secondDelta = order == 0 ? substepDelta.GetZ() : substepDelta.GetX();

		if (std::abs(firstDelta) > kPhysicsDirectionEpsilon) {
			std::array<float, 3> axisTarget = candidateFeetPosition;
			if (order == 0) {
				axisTarget[0] += firstDelta;
			} else {
				axisTarget[2] += firstDelta;
			}

			std::array<float, 3> axisResult{};
			if (tryMoveTo(axisTarget, axisResult)) {
				candidateFeetPosition = axisResult;
			}
		}

		if (std::abs(secondDelta) > kPhysicsDirectionEpsilon) {
			std::array<float, 3> axisTarget = candidateFeetPosition;
			if (order == 0) {
				axisTarget[2] += secondDelta;
			} else {
				axisTarget[0] += secondDelta;
			}

			std::array<float, 3> axisResult{};
			if (tryMoveTo(axisTarget, axisResult)) {
				candidateFeetPosition = axisResult;
			}
		}

		const float progress = scoreProgress(candidateFeetPosition);
		if (order == 0 || progress > bestProgress + kWalkCollisionEpsilon) {
			bestFeetPosition = candidateFeetPosition;
			bestProgress = progress;
		}
	}

	if (bestProgress <= kWalkCollisionEpsilon * kWalkCollisionEpsilon) {
		return false;
	}

	outFeetPosition = bestFeetPosition;
	return true;
}

template <bool tAllowStepUp>
void MoveWalkHorizontally(
	const VoxelWorld &world,
	std::array<float, 3> &feetPosition,
	const JPH::Vec3 &desiredHorizontalDelta,
	const bool sneakActive)
{
	if (desiredHorizontalDelta.IsNearZero()) {
		return;
	}

	const float maxComponent =
		std::max(std::abs(desiredHorizontalDelta.GetX()), std::abs(desiredHorizontalDelta.GetZ()));
	const int substepCount = std::max(1, static_cast<int>(std::ceil(maxComponent / kWalkHorizontalSubstepDistance)));
	const JPH::Vec3 substepDelta = desiredHorizontalDelta / static_cast<float>(substepCount);
	for (int substepIndex = 0; substepIndex < substepCount; ++substepIndex) {
		std::array<float, 3> candidateFeetPosition{};
		if (TryMoveWalkSubstep<tAllowStepUp>(world, feetPosition, substepDelta, sneakActive, candidateFeetPosition)) {
			feetPosition = candidateFeetPosition;
		}
	}

	ResolveWalkCharacterPenetration(world, feetPosition, sneakActive, false);
}

bool SnapWalkToFloor(
	const VoxelWorld &world,
	std::array<float, 3> &feetPosition,
	JPH::Vec3 &velocity,
	const bool sneakActive,
	const float maxDrop)
{
	if (maxDrop <= 0.0f) {
		return false;
	}

	float candidateFeetY = 0.0f;
	if (!FindWalkBestSupportFeetYAtXZ(
			world,
			feetPosition,
			0.0f,
			maxDrop,
			sneakActive,
			kWalkGroundSupportRadius,
			candidateFeetY)) {
		return false;
	}

	if (feetPosition[1] - candidateFeetY <= kWalkCollisionEpsilon) {
		return false;
	}

	feetPosition[1] = candidateFeetY;
	if (velocity.GetY() < 0.0f) {
		velocity.SetY(0.0f);
	}
	return true;
}

bool TrySnapWalkToGroundTakeoffAnchor(
	const PhysicsState &physics,
	const VoxelWorld &world,
	std::array<float, 3> &feetPosition,
	JPH::Vec3 &velocity,
	const float maxDrop)
{
	if (maxDrop <= 0.0f ||
		!physics.walkCachedGroundTakeoffValid ||
		physics.walkGroundTakeoffGraceFramesRemaining == 0 ||
		physics.walkSneakActive) {
		return false;
	}

	const float driftX = feetPosition[0] - physics.walkCachedGroundTakeoffFeetPosition[0];
	const float driftZ = feetPosition[2] - physics.walkCachedGroundTakeoffFeetPosition[2];
	if (driftX * driftX + driftZ * driftZ >
		kWalkGroundTakeoffLandingMaxDrift * kWalkGroundTakeoffLandingMaxDrift + kWalkCollisionEpsilon) {
		return false;
	}

	const float candidateFeetY = physics.walkCachedGroundTakeoffFeetPosition[1];
	const float drop = feetPosition[1] - candidateFeetY;
	if (drop <= kWalkCollisionEpsilon ||
		drop > maxDrop + kWalkCollisionEpsilon ||
		drop > kWalkGroundTakeoffSnapMaxDrop + kWalkCollisionEpsilon) {
		return false;
	}

	std::array<float, 3> candidateFeetPosition = feetPosition;
	candidateFeetPosition[1] = candidateFeetY;
	if (!IsWalkCharacterClearAt(world, candidateFeetPosition, false)) {
		return false;
	}

	feetPosition = candidateFeetPosition;
	if (velocity.GetY() < 0.0f) {
		velocity.SetY(0.0f);
	}
	return true;
}

bool TrySnapWalkToGroundReturnAnchor(
	const PhysicsState &physics,
	const VoxelWorld &world,
	std::array<float, 3> &feetPosition,
	JPH::Vec3 &velocity,
	const float maxDrop)
{
	if (maxDrop <= 0.0f ||
		!physics.walkGroundReturnAnchorValid ||
		physics.walkGroundReturnAnchorFramesRemaining == 0 ||
		physics.walkSneakActive) {
		return false;
	}

	const float driftX = feetPosition[0] - physics.walkGroundReturnAnchorFeetPosition[0];
	const float driftZ = feetPosition[2] - physics.walkGroundReturnAnchorFeetPosition[2];
	if (driftX * driftX + driftZ * driftZ >
		kWalkGroundTakeoffLandingMaxDrift * kWalkGroundTakeoffLandingMaxDrift + kWalkCollisionEpsilon) {
		return false;
	}

	const float candidateFeetY = physics.walkGroundReturnAnchorFeetPosition[1];
	const float drop = feetPosition[1] - candidateFeetY;
	if (drop <= kWalkCollisionEpsilon ||
		drop > maxDrop + kWalkCollisionEpsilon ||
		drop > kWalkGroundReturnSnapMaxDrop + kWalkCollisionEpsilon) {
		return false;
	}

	std::array<float, 3> candidateFeetPosition = feetPosition;
	candidateFeetPosition[1] = candidateFeetY;
	if (!IsWalkCharacterClearAt(world, candidateFeetPosition, false)) {
		return false;
	}

	feetPosition = candidateFeetPosition;
	if (velocity.GetY() < 0.0f) {
		velocity.SetY(0.0f);
	}
	return true;
}

bool TryRestoreWalkGroundReturnAnchorPlane(
	const PhysicsState &physics,
	const VoxelWorld &world,
	std::array<float, 3> &feetPosition,
	JPH::Vec3 &velocity)
{
	if (!physics.walkGroundReturnAnchorValid ||
		physics.walkGroundReturnAnchorFramesRemaining == 0 ||
		physics.walkSneakActive) {
		return false;
	}

	const float driftX = feetPosition[0] - physics.walkGroundReturnAnchorFeetPosition[0];
	const float driftZ = feetPosition[2] - physics.walkGroundReturnAnchorFeetPosition[2];
	if (driftX * driftX + driftZ * driftZ >
		kWalkGroundTakeoffLandingMaxDrift * kWalkGroundTakeoffLandingMaxDrift + kWalkCollisionEpsilon) {
		return false;
	}

	const float targetFeetY = physics.walkGroundReturnAnchorFeetPosition[1];
	const float upwardRestore = targetFeetY - feetPosition[1];
	if (upwardRestore <= kWalkCollisionEpsilon || upwardRestore > kWalkGroundReturnRestoreMaxDrop) {
		return false;
	}

	std::array<float, 3> candidateFeetPosition = feetPosition;
	candidateFeetPosition[1] = targetFeetY;
	if (!IsWalkCharacterClearAt(world, candidateFeetPosition, false)) {
		return false;
	}

	feetPosition = candidateFeetPosition;
	if (velocity.GetY() < 0.0f) {
		velocity.SetY(0.0f);
	}
	return true;
}

[[maybe_unused]] bool TryStickWalkSneakToFloor(
	PhysicsState &physics,
	const VoxelWorld &world,
	const WalkSneakSupportRegion &supportRegion)
{
	JPH::CharacterVirtual *character = physics.walkCharacter.GetPtr();
	const bool holdEdgeSupportActive =
		physics.walkSneakActive ||
		physics.walkLedgeReleaseGraceFramesRemaining > 0;
	if (character == nullptr || !holdEdgeSupportActive || physics.walkJumpLockedSupport.valid || !supportRegion.valid) {
		return false;
	}

	JPH::Vec3 velocity = character->GetLinearVelocity();
	if (velocity.GetY() > kWalkSneakStickPositiveVelocityEpsilon) {
		return false;
	}

	const std::array<float, 3> currentFeetPosition = ToArray(character->GetPosition());
	if (!IsWalkFeetInsideSneakSupportRegion(supportRegion, currentFeetPosition)) {
		return false;
	}

	const float referenceFeetY = supportRegion.referenceFeetPosition[1];
	const float restoreDistance = referenceFeetY - currentFeetPosition[1];
	if (restoreDistance > kWalkSneakOutwardDriftEpsilon &&
		restoreDistance <= kWalkSneakStickToFloorDistance) {
		const std::array restoredFeetPosition{
			currentFeetPosition[0],
			referenceFeetY,
			currentFeetPosition[2],
		};
		if (IsWalkCharacterClearAt(world, restoredFeetPosition, physics.walkSneakActive) &&
			HasWalkSneakSupport(world, restoredFeetPosition)) {
			character->SetPosition(ToRVec3(restoredFeetPosition));
			if (velocity.GetY() < 0.0f) {
				velocity.SetY(0.0f);
				character->SetLinearVelocity(velocity);
			}
			RefreshWalkCharacterContacts(physics);
			return true;
		}
	}

	const std::array probeOrigin{
		currentFeetPosition[0],
		currentFeetPosition[1] + kWalkSneakStickProbeLift,
		currentFeetPosition[2],
	};
	const PhysicsRaycastHit hit = RaycastPhysicsWorld(
		&physics,
		probeOrigin,
		{0.0f, -1.0f, 0.0f},
		kWalkSneakStickProbeLift + kWalkSneakStickToFloorDistance + kWalkSpawnClearance);
	if (!hit.hasHit || hit.normal[1] < 0.5f) {
		return false;
	}

	const float candidateFeetY = hit.position[1] + kWalkSpawnClearance;
	const float upwardRestore = candidateFeetY - currentFeetPosition[1];
	const float downwardDrop = currentFeetPosition[1] - candidateFeetY;
	if (upwardRestore > kWalkSneakOutwardDriftEpsilon) {

		if (candidateFeetY > referenceFeetY + kWalkSneakOutwardDriftEpsilon ||
			upwardRestore > kWalkSneakStickToFloorDistance) {
			return false;
		}
	} else {
		if (downwardDrop > kWalkSneakStickToFloorDistance) {
			return false;
		}

		if (candidateFeetY >= referenceFeetY - kWalkSneakStickMinimumDrop ||
			downwardDrop < kWalkSneakStickMinimumDrop) {
			return false;
		}
	}

	const std::array candidateFeetPosition{
		currentFeetPosition[0],
		candidateFeetY,
		currentFeetPosition[2],
	};
	if (!IsWalkFeetInsideSneakSupportRegion(supportRegion, candidateFeetPosition) ||
		!HasWalkSneakSupport(world, candidateFeetPosition)) {
		return false;
	}

	if (std::abs(candidateFeetY - currentFeetPosition[1]) <= kWalkSneakOutwardDriftEpsilon) {
		return false;
	}

	character->SetPosition(ToRVec3(candidateFeetPosition));
	if (velocity.GetY() < 0.0f) {
		velocity.SetY(0.0f);
		character->SetLinearVelocity(velocity);
	}
	RefreshWalkCharacterContacts(physics);
	return true;
}

[[maybe_unused]] WalkSupportContactKey SelectWalkPassiveSlideContact(
	const PhysicsState &physics,
	const WalkFootSupportInfo &supportInfo)
{
	WalkSupportContactKey bestContact{};
	const JPH::CharacterVirtual *character = physics.walkCharacter.GetPtr();
	if (character == nullptr || supportInfo.hitSamples == 0) {
		return bestContact;
	}

	const JPH::Vec3 up = character->GetUp();
	const float feetY = character->GetPosition().GetY();
	float bestUpDot = -std::numeric_limits<float>::infinity();
	float bestCentroidDistanceSq = std::numeric_limits<float>::infinity();
	float bestContactY = std::numeric_limits<float>::infinity();
	for (const JPH::CharacterContact &contact : character->GetActiveContacts()) {
		if (!contact.mHadCollision ||
			contact.mBodyB.IsInvalid() ||
			contact.mMotionTypeB != JPH::EMotionType::Static ||
			contact.mIsSensorB ||
			!contact.mLinearVelocity.IsNearZero()) {
			continue;
		}

		const JPH::Vec3 contactNormal = contact.mContactNormal.NormalizedOr(JPH::Vec3::sZero());
		if (contactNormal.IsNearZero() || character->IsSlopeTooSteep(contactNormal)) {
			continue;
		}

		const float upDot = contactNormal.Dot(up);
		if (upDot <= 0.5f) {
			continue;
		}

		const float contactY = contact.mPosition.GetY();
		if (contactY > feetY + kWalkSupportContactMaxHeightAboveFeet) {
			continue;
		}

		const float distanceX = contact.mPosition.GetX() - supportInfo.centroid[0];
		const float distanceZ = contact.mPosition.GetZ() - supportInfo.centroid[2];
		const float centroidDistanceSq = distanceX * distanceX + distanceZ * distanceZ;
		if (!bestContact.valid ||
			upDot > bestUpDot + 1.0e-4f ||
			(std::abs(upDot - bestUpDot) <= 1.0e-4f &&
			 (centroidDistanceSq < bestCentroidDistanceSq - 1.0e-4f ||
			  (std::abs(centroidDistanceSq - bestCentroidDistanceSq) <= 1.0e-4f && contactY < bestContactY)))) {
			bestContact.bodyId = contact.mBodyB;
			bestContact.subShapeId = contact.mSubShapeIDB;
			bestContact.valid = true;
			bestUpDot = upDot;
			bestCentroidDistanceSq = centroidDistanceSq;
			bestContactY = contactY;
		}
	}

	return bestContact;
}

void UpdateWalkGroundSupport(
	PhysicsState &physics,
	const VoxelWorld &world,
	const bool allowNarrowJumpEdgeSupport = false)
{
	PV_PROFILE_ZONE_N("UpdateWalkGroundSupport");
	JPH::CharacterVirtual *character = physics.walkCharacter.GetPtr();
	if (character == nullptr) {
		physics.walkSupportState = WalkSupportState::Air;
		physics.walkEdgeGraceFramesRemaining = 0;
		physics.walkGroundTakeoffGraceFramesRemaining = 0;
		physics.walkCachedGroundTakeoffValid = false;
		physics.walkFootSupportScore = 0.0f;
		physics.walkFootSupportHitSamples = 0;
		physics.walkFootSupportTotalSamples = 0;
		physics.walkFootSupportCentroid = {};
		physics.walkPreviousSupportFeetPosition = {};
		physics.walkPreviousSupportFeetPositionValid = false;
		physics.walkHadHorizontalMotionLastStep = false;
		ClearWalkJumpBallisticHorizontalVelocity(physics);
		physics.walkPassiveSlideContact = {};
		return;
	}

	const JPH::Vec3 velocity = character->GetLinearVelocity();
	const std::array<float, 3> currentFeetPosition = ToArray(character->GetPosition());
	const auto rememberCurrentFeetPosition = [&physics, &currentFeetPosition] {
		physics.walkPreviousSupportFeetPosition = currentFeetPosition;
		physics.walkPreviousSupportFeetPositionValid = true;
	};
	if (physics.walkJumpLockedSupport.valid) {
		physics.walkSupportState = WalkSupportState::Air;
		physics.walkEdgeGraceFramesRemaining = 0;
		physics.walkGroundTakeoffGraceFramesRemaining = 0;
		physics.walkCachedGroundTakeoffValid = false;
		physics.walkFootSupportScore = 0.0f;
		physics.walkFootSupportHitSamples = 0;
		physics.walkFootSupportTotalSamples = 0;
		physics.walkFootSupportCentroid = {};
		physics.walkPassiveSlideContact = {};
		rememberCurrentFeetPosition();
		return;
	}

	if (velocity.GetY() > 0.1f) {
		physics.walkSupportState = WalkSupportState::Air;
		physics.walkEdgeGraceFramesRemaining = 0;
		physics.walkFootSupportScore = 0.0f;
		physics.walkFootSupportHitSamples = 0;
		physics.walkFootSupportTotalSamples = 0;
		physics.walkFootSupportCentroid = {};
		physics.walkPassiveSlideContact = {};
		rememberCurrentFeetPosition();
		return;
	}

	if (velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon &&
		physics.walkGroundReturnAnchorValid) {
		const float anchorDriftX = currentFeetPosition[0] - physics.walkGroundReturnAnchorFeetPosition[0];
		const float anchorDriftZ = currentFeetPosition[2] - physics.walkGroundReturnAnchorFeetPosition[2];
		const float anchorFeetY = physics.walkGroundReturnAnchorFeetPosition[1];
		if (anchorDriftX * anchorDriftX + anchorDriftZ * anchorDriftZ <=
				kWalkGroundTakeoffLandingMaxDrift * kWalkGroundTakeoffLandingMaxDrift + kWalkCollisionEpsilon &&
			currentFeetPosition[1] + kWalkCollisionEpsilon >= anchorFeetY &&
			currentFeetPosition[1] - anchorFeetY <= kWalkGroundReturnSupportMaxRise + kWalkCollisionEpsilon) {
			physics.walkFootSupportScore = kWalkFootSupportEdgeGraceScore;
			physics.walkFootSupportHitSamples = 0;
			physics.walkFootSupportTotalSamples = 0;
			physics.walkFootSupportCentroid = currentFeetPosition;
			physics.walkPassiveSlideContact = {};
			physics.walkSupportState = WalkSupportState::EdgeGrace;
			physics.walkEdgeGraceFramesRemaining = kWalkEdgeGraceFrames;
			if (velocity.GetY() < 0.0f) {
				JPH::Vec3 correctedVelocity = velocity;
				correctedVelocity.SetY(0.0f);
				character->SetLinearVelocity(correctedVelocity);
			}
			rememberCurrentFeetPosition();
			return;
		}
	}

	const bool canUseSneakGroundSupport =
		velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon &&
		(physics.walkSneakActive || physics.walkLedgeReleaseGraceFramesRemaining > 0);
	if (canUseSneakGroundSupport) {
		WalkSneakSupportRegion sneakGroundSupportRegion{};
		if (physics.walkCachedSneakSupportRegion.valid &&
			IsWalkFeetInsideSneakSupportRegion(physics.walkCachedSneakSupportRegion, currentFeetPosition)) {
			sneakGroundSupportRegion = physics.walkCachedSneakSupportRegion;
		} else if (physics.walkSneakActive) {
			const WalkSneakSupportRegion currentSneakSupportRegion =
				ComputeWalkSneakSupportRegion(world, currentFeetPosition);
			if (currentSneakSupportRegion.valid &&
				IsWalkFeetInsideSneakSupportRegion(currentSneakSupportRegion, currentFeetPosition)) {
				sneakGroundSupportRegion = currentSneakSupportRegion;
			}
		}

		if (sneakGroundSupportRegion.valid) {
			const float supportFeetY = sneakGroundSupportRegion.referenceFeetPosition[1];
			const float aboveReference = currentFeetPosition[1] - supportFeetY;
			const float belowReference = supportFeetY - currentFeetPosition[1];
			std::array<float, 3> supportFeetPosition = currentFeetPosition;
			supportFeetPosition[1] = supportFeetY;
			if (aboveReference <= kWalkCollisionEpsilon &&
				belowReference <= kWalkSneakStickToFloorDistance + kWalkCollisionEpsilon &&
				IsWalkCharacterClearAt(world, supportFeetPosition, true) &&
				HasWalkSneakSupport(world, supportFeetPosition)) {
				physics.walkFootSupportScore = kWalkFootSupportGroundedScore;
				physics.walkFootSupportHitSamples = 0;
				physics.walkFootSupportTotalSamples = 0;
				physics.walkFootSupportCentroid = supportFeetPosition;
				physics.walkPassiveSlideContact = {};
				physics.walkSupportState = WalkSupportState::Grounded;
				physics.walkEdgeGraceFramesRemaining = kWalkEdgeGraceFrames;
				if (velocity.GetY() < 0.0f) {
					JPH::Vec3 correctedVelocity = velocity;
					correctedVelocity.SetY(0.0f);
					character->SetLinearVelocity(correctedVelocity);
				}
				rememberCurrentFeetPosition();
				return;
			}
		}
	}

	const WalkSupportState previousSupportState = physics.walkSupportState;
	const auto [score, hitSamples, totalSamples, centroid] = ComputeVoxelFootSupportInfo(world, currentFeetPosition);
	physics.walkFootSupportScore = score;
	physics.walkFootSupportHitSamples = hitSamples;
	physics.walkFootSupportTotalSamples = totalSamples;
	physics.walkFootSupportCentroid = centroid;
	physics.walkPassiveSlideContact = {};

	float supportedFeetY = 0.0f;
	const bool hasSupportUnderFeet =
		velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon &&
		FindWalkBestSupportFeetYAtXZ(
			world,
			currentFeetPosition,
			0.0f,
			kWalkStickToFloorDistance,
			physics.walkSneakActive,
			kWalkGroundSupportRadius,
			supportedFeetY);
	float groundTakeoffSupportedFeetY = 0.0f;
	const bool hasGroundTakeoffSupport =
		velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon &&
		FindWalkBestSupportFeetYAtXZ(
			world,
			currentFeetPosition,
			0.0f,
			kWalkStickToFloorDistance,
			physics.walkSneakActive,
			kWalkGroundTakeoffSupportRadius,
			groundTakeoffSupportedFeetY);
	const bool canRefreshGroundTakeoffCache =
		hasGroundTakeoffSupport &&
		!physics.walkJumpBallisticHorizontalVelocityActive;
	if (canRefreshGroundTakeoffCache) {
		physics.walkGroundTakeoffGraceFramesRemaining = kWalkGroundTakeoffGraceFrames;
		physics.walkCachedGroundTakeoffFeetPosition = currentFeetPosition;
		physics.walkCachedGroundTakeoffFeetPosition[1] = groundTakeoffSupportedFeetY;
		physics.walkCachedGroundTakeoffValid = true;
	}
	if (hasSupportUnderFeet &&
		std::abs(supportedFeetY - currentFeetPosition[1]) <= kWalkStickToFloorDistance + kWalkCollisionEpsilon &&
		score >= kWalkFootSupportGroundedScore) {
		physics.walkSupportState = WalkSupportState::Grounded;
		physics.walkEdgeGraceFramesRemaining = kWalkEdgeGraceFrames;
		if (velocity.GetY() < 0.0f) {
			JPH::Vec3 correctedVelocity = velocity;
			correctedVelocity.SetY(0.0f);
			character->SetLinearVelocity(correctedVelocity);
		}
		rememberCurrentFeetPosition();
		return;
	}

	const bool landedBackOnGroundTakeoffSupport =
		physics.walkJumpBallisticHorizontalVelocityActive &&
		physics.walkCachedGroundTakeoffValid &&
		physics.walkGroundTakeoffGraceFramesRemaining > 0 &&
		hasGroundTakeoffSupport &&
		std::abs(groundTakeoffSupportedFeetY - physics.walkCachedGroundTakeoffFeetPosition[1]) <=
			kWalkCollisionEpsilon &&
		[&] {
			const float cachedDriftX = currentFeetPosition[0] - physics.walkCachedGroundTakeoffFeetPosition[0];
			const float cachedDriftZ = currentFeetPosition[2] - physics.walkCachedGroundTakeoffFeetPosition[2];
			return cachedDriftX * cachedDriftX + cachedDriftZ * cachedDriftZ <=
				   kWalkGroundTakeoffLandingMaxDrift * kWalkGroundTakeoffLandingMaxDrift +
					   kWalkCollisionEpsilon;
		}() &&
		velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon &&
		currentFeetPosition[1] + kWalkCollisionEpsilon >= groundTakeoffSupportedFeetY &&
		currentFeetPosition[1] - groundTakeoffSupportedFeetY <= kWalkGroundTakeoffSnapMaxDrop + kWalkCollisionEpsilon;
	if (landedBackOnGroundTakeoffSupport) {
		physics.walkSupportState = WalkSupportState::EdgeGrace;
		physics.walkEdgeGraceFramesRemaining = kWalkEdgeGraceFrames;
		physics.walkFootSupportScore = std::max(physics.walkFootSupportScore, kWalkFootSupportEdgeGraceScore);
		if (velocity.GetY() < 0.0f) {
			JPH::Vec3 correctedVelocity = velocity;
			correctedVelocity.SetY(0.0f);
			character->SetLinearVelocity(correctedVelocity);
		}
		rememberCurrentFeetPosition();
		return;
	}

	float horizontalFeetDriftSq = std::numeric_limits<float>::max();
	float horizontalFeetDriftX = 0.0f;
	float horizontalFeetDriftZ = 0.0f;
	if (physics.walkPreviousSupportFeetPositionValid) {
		horizontalFeetDriftX = currentFeetPosition[0] - physics.walkPreviousSupportFeetPosition[0];
		horizontalFeetDriftZ = currentFeetPosition[2] - physics.walkPreviousSupportFeetPosition[2];
		horizontalFeetDriftSq =
			horizontalFeetDriftX * horizontalFeetDriftX + horizontalFeetDriftZ * horizontalFeetDriftZ;
	}
	const bool isRestingOnPartialEdgeSupport =
		hasSupportUnderFeet &&
		previousSupportState != WalkSupportState::Air &&
		!physics.walkHadHorizontalMotionLastStep &&
		physics.walkPreviousSupportFeetPositionValid &&
		hitSamples > 0 &&
		std::abs(supportedFeetY - currentFeetPosition[1]) <= kWalkStickToFloorDistance + kWalkCollisionEpsilon &&
		horizontalFeetDriftSq <=
			kWalkRestingEdgeHoldMaxHorizontalDrift * kWalkRestingEdgeHoldMaxHorizontalDrift + kWalkCollisionEpsilon;
	if (isRestingOnPartialEdgeSupport) {
		physics.walkSupportState = WalkSupportState::EdgeGrace;
		physics.walkEdgeGraceFramesRemaining = kWalkEdgeGraceFrames;
		physics.walkFootSupportScore = std::max(physics.walkFootSupportScore, kWalkFootSupportEdgeGraceScore);
		if (velocity.GetY() < 0.0f) {
			JPH::Vec3 correctedVelocity = velocity;
			correctedVelocity.SetY(0.0f);
			character->SetLinearVelocity(correctedVelocity);
		}
		rememberCurrentFeetPosition();
		return;
	}

	const bool isMovingOnPartialEdgeSupport =
		hasSupportUnderFeet &&
		score >= kWalkFootSupportMovingEdgeGraceScore &&
		velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon &&
		std::abs(supportedFeetY - currentFeetPosition[1]) <= kWalkStickToFloorDistance + kWalkCollisionEpsilon;
	if (isMovingOnPartialEdgeSupport) {
		physics.walkSupportState = WalkSupportState::EdgeGrace;
		physics.walkEdgeGraceFramesRemaining = kWalkEdgeGraceFrames;
		if (velocity.GetY() < 0.0f) {
			JPH::Vec3 correctedVelocity = velocity;
			correctedVelocity.SetY(0.0f);
			character->SetLinearVelocity(correctedVelocity);
		}
		rememberCurrentFeetPosition();
		return;
	}

	bool movingAwayFromRemainingSupport = false;
	if (physics.walkPreviousSupportFeetPositionValid && hitSamples > 0) {
		const float supportBiasX = centroid[0] - currentFeetPosition[0];
		const float supportBiasZ = centroid[2] - currentFeetPosition[2];
		const float supportBiasSq = supportBiasX * supportBiasX + supportBiasZ * supportBiasZ;
		const float driftSupportDot = horizontalFeetDriftX * supportBiasX + horizontalFeetDriftZ * supportBiasZ;
		movingAwayFromRemainingSupport =
			driftSupportDot < 0.0f &&
			driftSupportDot * driftSupportDot > 0.25f * horizontalFeetDriftSq * supportBiasSq;
	}

	const bool isMovingOnNarrowEdgeSupport =
		allowNarrowJumpEdgeSupport &&
		hasGroundTakeoffSupport &&
		hitSamples > 0 &&
		!movingAwayFromRemainingSupport &&
		velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon &&
		currentFeetPosition[1] + kWalkCollisionEpsilon >= groundTakeoffSupportedFeetY &&
		currentFeetPosition[1] - groundTakeoffSupportedFeetY <= kWalkGroundTakeoffSnapMaxDrop + kWalkCollisionEpsilon;
	if (isMovingOnNarrowEdgeSupport) {
		physics.walkSupportState = WalkSupportState::EdgeGrace;
		physics.walkEdgeGraceFramesRemaining = kWalkEdgeGraceFrames;
		physics.walkFootSupportScore = std::max(physics.walkFootSupportScore, kWalkFootSupportEdgeGraceScore);
		if (velocity.GetY() < 0.0f) {
			JPH::Vec3 correctedVelocity = velocity;
			correctedVelocity.SetY(0.0f);
			character->SetLinearVelocity(correctedVelocity);
		}
		rememberCurrentFeetPosition();
		return;
	}

	if (previousSupportState != WalkSupportState::Air &&
		physics.walkEdgeGraceFramesRemaining > 0) {
		physics.walkSupportState = WalkSupportState::EdgeGrace;
		physics.walkEdgeGraceFramesRemaining -= 1;
		if (physics.walkGroundTakeoffGraceFramesRemaining > 0) {
			physics.walkGroundTakeoffGraceFramesRemaining -= 1;
		} else {
			physics.walkCachedGroundTakeoffValid = false;
		}
		rememberCurrentFeetPosition();
		return;
	}

	physics.walkSupportState = WalkSupportState::Air;
	physics.walkEdgeGraceFramesRemaining = 0;
	if (physics.walkGroundTakeoffGraceFramesRemaining > 0) {
		physics.walkGroundTakeoffGraceFramesRemaining -= 1;
	} else {
		physics.walkCachedGroundTakeoffValid = false;
	}
	rememberCurrentFeetPosition();
}

void UpdateCameraFromWalkCharacter(const PhysicsState &physics, CameraState &camera)
{
	if (physics.walkCharacter == nullptr) {
		return;
	}

	const std::array<float, 3> feetPosition = ToArray(physics.walkCharacter->GetPosition());
	const float eyeHeight = GetWalkEyeHeight(physics, camera.controlMode);
	const float targetCameraY = feetPosition[1] + eyeHeight;
	float cameraY = targetCameraY;
	if (camera.controlMode == CameraState::ControlMode::Walk &&
		physics.walkSupportState == WalkSupportState::Air &&
		targetCameraY > camera.position[1] + kWalkCameraAirRiseSmoothingMaxPerTick &&
		targetCameraY <= camera.position[1] + kWalkStairsStepUpHeight + kWalkCollisionEpsilon) {
		cameraY = camera.position[1] + kWalkCameraAirRiseSmoothingMaxPerTick;
	}
	camera.position = {
		feetPosition[0],
		cameraY,
		feetPosition[2],
	};
}

bool TryBuildWalkSpawnFromRay(
	const PhysicsState &physics,
	const std::array<float, 3> &origin,
	const float maxDistance,
	std::array<float, 3> *outFeetPosition)
{
	const PhysicsRaycastHit hit = RaycastPhysicsWorld(&physics, origin, {0.0f, -1.0f, 0.0f}, maxDistance);
	if (!hit.hasHit || hit.normal[1] < 0.5f) {
		return false;
	}

	*outFeetPosition = {
		origin[0],
		hit.position[1] + kWalkSpawnClearance,
		origin[2],
	};
	return true;
}

std::array<float, 3> BuildFallbackWalkFeetPosition(const PhysicsState &physics, const CameraState &camera)
{
	const float eyeHeight = GetWalkEyeHeight(physics, camera.controlMode);
	return {
		camera.position[0],
		camera.position[1] - eyeHeight,
		camera.position[2],
	};
}

bool DoesWalkCharacterBodyOverlapVoxel(
	const std::array<float, 3> &feetPosition,
	const bool sneakActive,
	const Int3 &voxel)
{
	const float totalHeight = 2.0f * (GetWalkCapsuleHalfHeight(sneakActive) + kWalkCapsuleRadius);
	const float minX = feetPosition[0] - kWalkCapsuleRadius + kWalkCollisionEpsilon;
	const float maxX = feetPosition[0] + kWalkCapsuleRadius - kWalkCollisionEpsilon;
	const float minY = feetPosition[1] + kWalkCollisionEpsilon;
	const float maxY = feetPosition[1] + totalHeight - kWalkCollisionEpsilon;
	const float minZ = feetPosition[2] - kWalkCapsuleRadius + kWalkCollisionEpsilon;
	const float maxZ = feetPosition[2] + kWalkCapsuleRadius - kWalkCollisionEpsilon;
	const float voxelMinX = static_cast<float>(voxel.x);
	const float voxelMaxX = voxelMinX + 1.0f;
	const float voxelMinY = static_cast<float>(voxel.y);
	const float voxelMaxY = voxelMinY + 1.0f;
	const float voxelMinZ = static_cast<float>(voxel.z);
	const float voxelMaxZ = voxelMinZ + 1.0f;
	return !(maxX <= voxelMinX + kWalkCollisionEpsilon ||
			 minX >= voxelMaxX - kWalkCollisionEpsilon ||
			 maxY <= voxelMinY + kWalkCollisionEpsilon ||
			 minY >= voxelMaxY - kWalkCollisionEpsilon ||
			 maxZ <= voxelMinZ + kWalkCollisionEpsilon ||
			 minZ >= voxelMaxZ - kWalkCollisionEpsilon);
}

bool CameraNeedsGroundRecovery(const PhysicsState &physics, const VoxelWorld &world, const CameraState &camera)
{
	const std::array<float, 3> feetPosition = BuildFallbackWalkFeetPosition(physics, camera);
	return IsSolidAtPosition(world, feetPosition) ||
		   IsSolidAtPosition(world, {feetPosition[0], feetPosition[1] + 0.9f, feetPosition[2]}) ||
		   IsSolidAtPosition(world, {camera.position[0], camera.position[1], camera.position[2]});
}

void ApplyWalkCharacterState(
	PhysicsState &physics,
	const VoxelWorld &world,
	CameraState &camera,
	const std::array<float, 3> &feetPosition)
{
	physics.walkCharacter->SetPosition(ToRVec3(feetPosition));
	physics.walkCharacter->SetRotation(JPH::Quat::sRotation(JPH::Vec3::sAxisY(), camera.yawRadians));
	physics.walkCharacter->SetLinearVelocity(JPH::Vec3::sZero());
	physics.walkCharacterInitialized = true;
	physics.walkCachedGroundTakeoffFeetPosition = {};
	physics.walkCachedGroundTakeoffValid = false;
	physics.walkGroundReturnAnchorFeetPosition = {};
	physics.walkGroundReturnAnchorFramesRemaining = 0;
	physics.walkGroundReturnAnchorValid = false;
	physics.walkPreviousSupportFeetPosition = {};
	physics.walkPreviousSupportFeetPositionValid = false;
	physics.walkHadHorizontalMotionLastStep = false;
	physics.walkAutoJumpDelayFramesRemaining = 0;
	ClearWalkJumpBallisticHorizontalVelocity(physics);
	ClearWalkSneakSupportCache(physics);
	ClearWalkJumpLockedSupport(physics);
	physics.walkSuppressPassiveSlide = false;
	RefreshWalkCharacterContacts(physics);
	UpdateWalkGroundSupport(physics, world, false);
	UpdateCameraFromWalkCharacter(physics, camera);
}

bool RebuildCharacterFromCamera(
	PhysicsState &physics,
	const VoxelWorld &world,
	CameraState &camera)
{
	if (!EnsureWalkCharacter(physics)) {
		return false;
	}

	std::array<float, 3> feetPosition = BuildFallbackWalkFeetPosition(physics, camera);
	if (CameraNeedsGroundRecovery(physics, world, camera)) {
		if (TryBuildWalkSpawnFromRay(
				physics,
				{
					camera.position[0],
					camera.position[1] + 0.5f,
					camera.position[2],
				},
				std::max(32.0f, static_cast<float>(world.height) + 16.0f),
				&feetPosition)) {
			ApplyWalkCharacterState(physics, world, camera, feetPosition);
			return true;
		}

		const std::array topDownProbeOrigin{
			camera.position[0],
			std::max(camera.position[1] + 16.0f, static_cast<float>(world.maxExclusive.y) + 16.0f),
			camera.position[2],
		};
		if (TryBuildWalkSpawnFromRay(
				physics,
				topDownProbeOrigin,
				std::max(48.0f, static_cast<float>(world.height) + 48.0f),
				&feetPosition)) {
			ApplyWalkCharacterState(physics, world, camera, feetPosition);
			return true;
		}
	}

	ApplyWalkCharacterState(physics, world, camera, feetPosition);
	return true;
}

Float3 ComputeWalkMoveDirection(const CameraState &camera, const InputState &input)
{
	const Float3 forward = GetWalkForwardVector(camera);
	const Float3 right = Normalize(Cross(forward, Float3{0.0f, 1.0f, 0.0f}));

	Float3 moveDirection{};
	if (IsInputActionDown(input, InputAction::MoveForward)) {
		moveDirection.x += forward.x;
		moveDirection.z += forward.z;
	}
	if (IsInputActionDown(input, InputAction::MoveBackward)) {
		moveDirection.x -= forward.x;
		moveDirection.z -= forward.z;
	}
	if (IsInputActionDown(input, InputAction::MoveRight)) {
		moveDirection.x += right.x;
		moveDirection.z += right.z;
	}
	if (IsInputActionDown(input, InputAction::MoveLeft)) {
		moveDirection.x -= right.x;
		moveDirection.z -= right.z;
	}

	return Normalize(moveDirection);
}

Float3 ComputeCreativeMoveDirection(const CameraState &camera, const InputState &input)
{
	Float3 moveDirection = ComputeWalkMoveDirection(camera, input);
	if (IsInputActionDown(input, InputAction::MoveUp)) {
		moveDirection.y += 1.0f;
	}
	if (IsInputActionDown(input, InputAction::MoveDown)) {
		moveDirection.y -= 1.0f;
	}

	return Normalize(moveDirection);
}

Int3 FloorToVoxel(const std::array<float, 3> &position)
{
	return {
		static_cast<int>(std::floor(position[0])),
		static_cast<int>(std::floor(position[1])),
		static_cast<int>(std::floor(position[2])),
	};
}
} // namespace

bool BuildChunkStaticCollisionBody(PhysicsState &physics, const VoxelWorld &world, uint32_t chunkIndex)
{
	PV_PROFILE_ZONE_N("BuildChunkStaticCollisionBody");
	if (chunkIndex >= world.chunks.size()) {
		return false;
	}
	const VoxelChunk &chunk = world.chunks[chunkIndex];
	if (chunk.min.x >= chunk.maxExclusive.x ||
		chunk.min.y >= chunk.maxExclusive.y ||
		chunk.min.z >= chunk.maxExclusive.z) {
		return false;
	}

	const auto it = physics.chunkStaticBodies.find(chunkIndex);
	if (it != physics.chunkStaticBodies.end()) {
		JPH::BodyInterface &bodyInterface = physics.physicsSystem.GetBodyInterface();
		bodyInterface.RemoveBody(it->second);
		bodyInterface.DestroyBody(it->second);
		physics.chunkStaticBodies.erase(it);
	}
	physics.chunkMergedBoxes.erase(chunkIndex);

	JPH::StaticCompoundShapeSettings compoundSettings;

	std::vector<projectv::physics::MergedVoxelBox> mergedBoxes;
	if (projectv::physics::IsGreedyPhysicsMeshEnabled()) {
		projectv::physics::GreedyMergeSolidVoxelsInBounds(
			world,
			chunk.min,
			chunk.maxExclusive,
			mergedBoxes);
		profiling::PlotValue(
			"Physics Greedy Merge Chunk Box Count",
			static_cast<int64_t>(mergedBoxes.size()));
	}
	physics.chunkMergedBoxes[chunkIndex] = mergedBoxes;

	size_t solidVoxelCount = 0;
	if (!mergedBoxes.empty()) {
		for (const projectv::physics::MergedVoxelBox &box : mergedBoxes) {
			const int spanX = box.maxX - box.minX;
			const int spanY = box.maxY - box.minY;
			const int spanZ = box.maxZ - box.minZ;
			const float halfX = static_cast<float>(spanX) * 0.5f;
			const float halfY = static_cast<float>(spanY) * 0.5f;
			const float halfZ = static_cast<float>(spanZ) * 0.5f;
			const JPH::Vec3 halfExtent(halfX, halfY, halfZ);
			const JPH::Vec3 center(
				static_cast<float>(box.minX) + halfX,
				static_cast<float>(box.minY) + halfY,
				static_cast<float>(box.minZ) + halfZ);
			const JPH::RefConst<JPH::Shape> boxShape = new JPH::BoxShape(halfExtent);
			compoundSettings.AddShape(
				center,
				JPH::Quat::sIdentity(),
				boxShape.GetPtr());
			solidVoxelCount += static_cast<size_t>(spanX) *
				static_cast<size_t>(spanY) *
				static_cast<size_t>(spanZ);
		}
	} else {
		const JPH::RefConst<JPH::Shape> voxelShape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
		for (int z = chunk.min.z; z < chunk.maxExclusive.z; ++z) {
			for (int y = chunk.min.y; y < chunk.maxExclusive.y; ++y) {
				for (int x = chunk.min.x; x < chunk.maxExclusive.x; ++x) {
					const Int3 voxel{x, y, z};
					if (!IsPhysicsSolidMaterial(GetVoxelMaterial(world, voxel))) {
						continue;
					}

					compoundSettings.AddShape(
						JPH::Vec3(
							static_cast<float>(x) + 0.5f,
							static_cast<float>(y) + 0.5f,
							static_cast<float>(z) + 0.5f),
						JPH::Quat::sIdentity(),
						voxelShape.GetPtr());
					++solidVoxelCount;
				}
			}
		}
	}

	if (solidVoxelCount == 0u) {
		physics.chunkMergedBoxes.erase(chunkIndex);
		return true;
	}

	const JPH::ShapeSettings::ShapeResult shapeResult = compoundSettings.Create(physics.tempAllocator);
	if (!shapeResult.IsValid()) {
		runtime::LogRuntimeFailure(
			"Physics",
			"BuildChunkStaticCollisionBody.Create",
			shapeResult.GetError());
		return false;
	}

	const JPH::RefConst<JPH::Shape> chunkShape = shapeResult.Get();
	const JPH::BodyCreationSettings chunkBodySettings(
		chunkShape,
		JPH::RVec3::sZero(),
		JPH::Quat::sIdentity(),
		JPH::EMotionType::Static,
		PhysicsLayers::Static);

	JPH::BodyInterface &bodyInterface = physics.physicsSystem.GetBodyInterface();
	const JPH::BodyID bodyId = bodyInterface.CreateAndAddBody(chunkBodySettings, JPH::EActivation::DontActivate);
	if (bodyId.IsInvalid()) {
		runtime::LogRuntimeFailure(
			"Physics",
			"BuildChunkStaticCollisionBody.CreateAndAddBody",
			"CreateAndAddBody returned an invalid body id");
		return false;
	}

	physics.chunkStaticBodies[chunkIndex] = bodyId;
	return true;
}

void DestroyChunkStaticBody(PhysicsState &physics, uint32_t chunkIndex)
{
	const auto it = physics.chunkStaticBodies.find(chunkIndex);
	if (it == physics.chunkStaticBodies.end()) {
		return;
	}
	JPH::BodyInterface &bodyInterface = physics.physicsSystem.GetBodyInterface();
	bodyInterface.RemoveBody(it->second);
	bodyInterface.DestroyBody(it->second);
	physics.chunkStaticBodies.erase(it);
	physics.chunkMergedBoxes.erase(chunkIndex);
}

void DestroyAllChunkStaticBodies(PhysicsState &physics)
{
	if (physics.chunkStaticBodies.empty()) {
		return;
	}
	JPH::BodyInterface &bodyInterface = physics.physicsSystem.GetBodyInterface();
	for (const auto &entry : physics.chunkStaticBodies) {
		bodyInterface.RemoveBody(entry.second);
		bodyInterface.DestroyBody(entry.second);
	}
	physics.chunkStaticBodies.clear();
	physics.chunkMergedBoxes.clear();
}

bool RebuildStaticWorldBodyFromChunkShapes(PhysicsState &physics, const VoxelWorld &world)
{
	PV_PROFILE_ZONE_N("RebuildStaticWorldBodyFromChunkShapes");
	DestroyStaticWorldBody(physics);

	if (physics.chunkMergedBoxes.empty()) {
		return true;
	}

	JPH::StaticCompoundShapeSettings compoundSettings;
	for (const auto &entry : physics.chunkMergedBoxes) {
		for (const projectv::physics::MergedVoxelBox &box : entry.second) {
			const int spanX = box.maxX - box.minX;
			const int spanY = box.maxY - box.minY;
			const int spanZ = box.maxZ - box.minZ;
			if (spanX <= 0 || spanY <= 0 || spanZ <= 0) {
				continue;
			}
			const float halfX = static_cast<float>(spanX) * 0.5f;
			const float halfY = static_cast<float>(spanY) * 0.5f;
			const float halfZ = static_cast<float>(spanZ) * 0.5f;
			const JPH::Vec3 halfExtent(halfX, halfY, halfZ);
			const JPH::Vec3 center(
				static_cast<float>(box.minX) + halfX,
				static_cast<float>(box.minY) + halfY,
				static_cast<float>(box.minZ) + halfZ);
			const JPH::RefConst<JPH::Shape> boxShape = new JPH::BoxShape(halfExtent);
			compoundSettings.AddShape(
				center,
				JPH::Quat::sIdentity(),
				boxShape.GetPtr());
		}
	}

	if (compoundSettings.mSubShapes.empty()) {
		return true;
	}

	const JPH::ShapeSettings::ShapeResult shapeResult = compoundSettings.Create(physics.tempAllocator);
	if (!shapeResult.IsValid()) {
		runtime::LogRuntimeFailure(
			"Physics",
			"RebuildStaticWorldBodyFromChunkShapes.Create",
			shapeResult.GetError());
		return false;
	}

	physics.staticWorldShape = shapeResult.Get();
	const JPH::BodyCreationSettings worldBodySettings(
		physics.staticWorldShape,
		JPH::RVec3::sZero(),
		JPH::Quat::sIdentity(),
		JPH::EMotionType::Static,
		PhysicsLayers::Static);

	JPH::BodyInterface &bodyInterface = physics.physicsSystem.GetBodyInterface();
	const JPH::BodyID bodyId = bodyInterface.CreateAndAddBody(worldBodySettings, JPH::EActivation::DontActivate);
	if (bodyId.IsInvalid()) {
		runtime::LogRuntimeFailure(
			"Physics",
			"RebuildStaticWorldBodyFromChunkShapes.CreateAndAddBody",
			"CreateAndAddBody returned an invalid body id");
		physics.staticWorldShape = nullptr;
		return false;
	}

	physics.staticWorldBodyId = bodyId;
	return true;
}

void QueueChunkRebuildRequest(PhysicsState *physics, const uint32_t chunkIndex)
{
	if (!physics) {
		return;
	}
	physics->pendingChunkRebuilds.push_back(chunkIndex);
}

uint32_t ProcessChunkRebuildQueue(PhysicsState *physics, const VoxelWorld *world)
{
	PV_PROFILE_ZONE_N("ProcessChunkRebuildQueue");
	if (!physics || !world || physics->pendingChunkRebuilds.empty()) {
		return 0;
	}

	std::vector<uint32_t> pending;
	pending.swap(physics->pendingChunkRebuilds);
	std::sort(pending.begin(), pending.end());
	pending.erase(std::unique(pending.begin(), pending.end()), pending.end());

	uint32_t rebuiltCount = 0;
	for (const uint32_t chunkIndex : pending) {
		if (chunkIndex >= world->chunks.size()) {
			continue;
		}
		if (BuildChunkStaticCollisionBody(*physics, *world, chunkIndex)) {
			++rebuiltCount;
		}
	}
	if (rebuiltCount > 0) {
		physics->physicsSystem.OptimizeBroadPhase();
	}
	return rebuiltCount;
}

uint32_t GetPendingChunkRebuildCount(const PhysicsState *physics)
{
	if (!physics) {
		return 0;
	}
	return static_cast<uint32_t>(physics->pendingChunkRebuilds.size());
}

uint32_t GetChunkBodyCount(const PhysicsState *physics)
{
	if (!physics) {
		return 0;
	}
	return static_cast<uint32_t>(physics->chunkStaticBodies.size());
}

PhysicsBroadphaseStats GetPhysicsBroadphaseStats(const PhysicsState *physics)
{
	PhysicsBroadphaseStats out{};
	if (!physics) {
		return out;
	}
	const JPH::BodyManager::BodyStats bodyStats = physics->physicsSystem.GetBodyStats();
	out.totalBodies = bodyStats.mNumBodies;
	out.maxBodies = bodyStats.mMaxBodies;
	out.staticBodies = bodyStats.mNumBodiesStatic;
	out.dynamicBodies = bodyStats.mNumBodiesDynamic;
	out.activeDynamicBodies = bodyStats.mNumActiveBodiesDynamic;
	out.kinematicBodies = bodyStats.mNumBodiesKinematic;
	out.activeKinematicBodies = bodyStats.mNumActiveBodiesKinematic;
	out.pendingChunkRebuilds = static_cast<uint32_t>(physics->pendingChunkRebuilds.size());
	out.chunkStaticBodies = static_cast<uint32_t>(physics->chunkStaticBodies.size());
	out.chunkMergedBoxesEntries = static_cast<uint32_t>(physics->chunkMergedBoxes.size());
	return out;
}

void InvalidateWalkSupportStateForWorldEdit(PhysicsState &physics);

PhysicsState *CreatePhysicsState()
{
	AcquireJoltRuntime();
	auto *physics = new PhysicsState{};
	physics->physicsSystem.Init(
		kMaxPhysicsBodies,
		0,
		kMaxBodyPairs,
		kMaxContactConstraints,
		physics->broadPhaseLayerInterface,
		physics->objectVsBroadPhaseLayerFilter,
		physics->objectLayerPairFilter);
	physics->physicsSystem.SetGravity(JPH::Vec3(0.0f, -24.0f, 0.0f));
	return physics;
}

void DestroyPhysicsState(PhysicsState *physics)
{
	if (!physics) {
		return;
	}

	physics->walkCharacter = nullptr;
	DestroyStaticWorldBody(*physics);
	delete physics;
	ReleaseJoltRuntime();
}

bool SyncPhysicsWorld(PhysicsState *physics, const VoxelWorld *world)
{
	PV_PROFILE_ZONE_N("SyncPhysicsWorld");
	if (!physics) {
		return false;
	}

	if (physics->syncedWorld == world &&
		physics->syncedWorldEditVersion == (world != nullptr ? world->editVersion : 0)) {
		profiling::PlotValue("Physics Sync Skipped", int64_t{1});
		return true;
	}

	const bool worldPointerChanged = (physics->syncedWorld != world);
	physics->syncedWorld = world;
	physics->syncedWorldEditVersion = (world != nullptr ? world->editVersion : 0);

	if (!world) {
		DestroyStaticWorldBody(*physics);
		DestroyAllChunkStaticBodies(*physics);
		ResetWalkCharacter(physics);
		return true;
	}

	if (worldPointerChanged) {
		profiling::PlotValue("Physics Sync Full Rebuild", int64_t{1});
		DestroyStaticWorldBody(*physics);
		DestroyAllChunkStaticBodies(*physics);
		for (size_t chunkIndex = 0; chunkIndex < world->chunks.size(); ++chunkIndex) {
			BuildChunkStaticCollisionBody(*physics, *world, static_cast<uint32_t>(chunkIndex));
		}
	} else {
		profiling::PlotValue("Physics Sync Incremental", int64_t{1});
		ProcessChunkRebuildQueue(physics, world);
	}

	if (!RebuildStaticWorldBodyFromChunkShapes(*physics, *world)) {
		ResetWalkCharacter(physics);
		return false;
	}

	if (physics->walkCharacterInitialized && physics->walkCharacter.GetPtr() != nullptr) {
		RefreshWalkCharacterContacts(*physics);
		InvalidateWalkSupportStateForWorldEdit(*physics);
	}
	return true;
}

PhysicsRaycastHit RaycastPhysicsWorld(
	const PhysicsState *physics,
	const std::array<float, 3> &origin,
	const std::array<float, 3> &direction,
	const float maxDistance)
{
	PhysicsRaycastHit result{};
	if (!physics || maxDistance <= 0.0f) {
		return result;
	}

	const Float3 normalizedDirection = Normalize({direction[0], direction[1], direction[2]});
	if (IsZeroVector(normalizedDirection)) {
		return result;
	}

	const JPH::RRayCast ray(
		ToRVec3(origin),
		JPH::Vec3(
			normalizedDirection.x * maxDistance,
			normalizedDirection.y * maxDistance,
			normalizedDirection.z * maxDistance));
	JPH::RayCastResult hit;
	hit.Reset();
	if (!physics->physicsSystem.GetNarrowPhaseQuery().CastRay(ray, hit)) {
		return result;
	}

	const JPH::BodyLockRead bodyLock(physics->physicsSystem.GetBodyLockInterface(), hit.mBodyID);
	if (!bodyLock.Succeeded()) {
		return result;
	}

	const JPH::RVec3 hitPosition = ray.GetPointOnRay(hit.mFraction);
	const JPH::Vec3 hitNormal = bodyLock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, hitPosition);
	const std::array<float, 3> position = ToArray(hitPosition);
	const std::array<float, 3> normal = ToArray(hitNormal);
	const Int3 voxel = FloorToVoxel({
		position[0] - normal[0] * kPhysicsRaycastVoxelEpsilon,
		position[1] - normal[1] * kPhysicsRaycastVoxelEpsilon,
		position[2] - normal[2] * kPhysicsRaycastVoxelEpsilon,
	});

	result.hasHit = true;
	result.voxel = voxel;
	result.position = position;
	result.normal = normal;
	result.distance = hit.mFraction * maxDistance;
	return result;
}

void ResetWalkCharacter(PhysicsState *physics)
{
	if (!physics) {
		return;
	}

	physics->walkCharacter = nullptr;
	physics->walkCharacterInitialized = false;
	physics->walkSupportState = WalkSupportState::Air;
	physics->walkEdgeGraceFramesRemaining = 0;
	physics->walkGroundTakeoffGraceFramesRemaining = 0;
	physics->walkFootSupportScore = 0.0f;
	physics->walkFootSupportHitSamples = 0;
	physics->walkFootSupportTotalSamples = 0;
	physics->walkFootSupportCentroid = {};
	physics->walkCachedGroundTakeoffFeetPosition = {};
	physics->walkCachedGroundTakeoffValid = false;
	physics->walkGroundReturnAnchorFeetPosition = {};
	physics->walkGroundReturnAnchorFramesRemaining = 0;
	physics->walkGroundReturnAnchorValid = false;
	physics->walkPreviousSupportFeetPosition = {};
	physics->walkPreviousSupportFeetPositionValid = false;
	physics->walkHadHorizontalMotionLastStep = false;
	physics->walkAutoJumpDelayFramesRemaining = 0;
	physics->walkPassiveSlideContact = {};
	physics->walkSneakActive = false;
	ClearWalkSneakSupportCache(*physics);
	ClearWalkJumpLockedSupport(*physics);
	physics->walkSuppressPassiveSlide = false;
}

void InvalidateWalkSupportStateForWorldEdit(PhysicsState &physics)
{
	physics.walkSupportState = WalkSupportState::Air;
	physics.walkEdgeGraceFramesRemaining = 0;
	physics.walkGroundTakeoffGraceFramesRemaining = 0;
	physics.walkFootSupportScore = 0.0f;
	physics.walkFootSupportHitSamples = 0;
	physics.walkFootSupportTotalSamples = 0;
	physics.walkFootSupportCentroid = {};
	physics.walkCachedGroundTakeoffFeetPosition = {};
	physics.walkCachedGroundTakeoffValid = false;
	physics.walkGroundReturnAnchorFeetPosition = {};
	physics.walkGroundReturnAnchorFramesRemaining = 0;
	physics.walkGroundReturnAnchorValid = false;
	physics.walkPreviousSupportFeetPosition = {};
	physics.walkPreviousSupportFeetPositionValid = false;
	physics.walkHadHorizontalMotionLastStep = false;
	physics.walkAutoJumpDelayFramesRemaining = 0;
	physics.walkPassiveSlideContact = {};
	ClearWalkSneakSupportCache(physics);
	ClearWalkJumpLockedSupport(physics);
	physics.walkSuppressPassiveSlide = false;
}

bool SnapWalkCharacterToCamera(
	PhysicsState *physics,
	const VoxelWorld *world,
	CameraState *camera)
{
	if (!physics || !world || !camera) {
		return false;
	}

	return RebuildCharacterFromCamera(*physics, *world, *camera);
}

bool SnapCreativeCharacterToCamera(
	PhysicsState *physics,
	const VoxelWorld *world,
	CameraState *camera)
{
	if (!physics || !world || !camera) {
		return false;
	}

	return RebuildCharacterFromCamera(*physics, *world, *camera);
}

bool TickWalkCharacter(
	PhysicsState *physics,
	const VoxelWorld *world,
	CameraState *camera,
	InputState *input,
	const float deltaSeconds)
{
	PV_PROFILE_ZONE_N("TickWalkCharacter");
	if (!physics || !world || !camera || !input) {
		return false;
	}

	if (!physics->walkCharacterInitialized) {
		if (!RebuildCharacterFromCamera(*physics, *world, *camera)) {
			return false;
		}
	}

	JPH::CharacterVirtual *character = physics->walkCharacter.GetPtr();
	if (character == nullptr) {
		return false;
	}

	const std::array<float, 3> currentFeetPosition = ToArray(character->GetPosition());
	const bool wantsSneak = IsInputActionDown(*input, InputAction::MoveDown);
	const bool previousSneakActive = physics->walkSneakActive;
	bool nextSneakActive = wantsSneak;
	if (!nextSneakActive && !IsWalkCharacterClearAt(*world, currentFeetPosition, false)) {
		nextSneakActive = true;
	}
	if (!SetWalkSneakActive(*physics, nextSneakActive)) {
		physics->walkSneakActive = previousSneakActive;
	}
	if (physics->walkSneakActive != previousSneakActive) {
		if (physics->walkSneakActive) {
			physics->walkLedgeReleaseGraceFramesRemaining = 0;
		} else {
			physics->walkSneakSupportGraceFramesRemaining = 0;
			physics->walkLedgeReleaseGraceFramesRemaining =
				physics->walkCachedSneakSupportRegion.valid ? kWalkLedgeReleaseGraceFrames : 0;
			ReleaseWalkSneakSupportCacheIfUnused(*physics);
		}
	}

	const bool jumpHeld = IsInputActionDown(*input, InputAction::MoveUp);
	const bool jumpPressedForSupport =
		HasMoveUpInputActionMaskBit(GetInputActionPressedMask(*input));
	UpdateWalkGroundSupport(*physics, *world, jumpHeld || jumpPressedForSupport);

	if (deltaSeconds <= 0.0f) {
		physics->walkHadHorizontalMotionLastStep = false;
		UpdateCameraFromWalkCharacter(*physics, *camera);
		return true;
	}

	const Float3 moveDirection = ComputeWalkMoveDirection(*camera, *input);
	float moveSpeed = kWalkMoveSpeed;
	if (physics->walkSneakActive) {
		moveSpeed *= kWalkSneakMoveSpeedMultiplier;
	}
	if (IsInputActionDown(*input, InputAction::SpeedBoost)) {
		moveSpeed *= kWalkBoostMultiplier;
	}
	if (IsInputActionDown(*input, InputAction::SpeedSlow)) {
		moveSpeed *= kWalkSlowMultiplier;
	}
	const bool hasMovementInput = !IsZeroVector(moveDirection);
	if (physics->walkGroundReturnAnchorValid && hasMovementInput) {
		physics->walkGroundReturnAnchorFeetPosition = {};
		physics->walkGroundReturnAnchorFramesRemaining = 0;
		physics->walkGroundReturnAnchorValid = false;
	}
	const bool jumpPressed = ConsumeInputActionPressed(*input, InputAction::MoveUp);

	const bool hasJumpLockedSupport = IsWalkJumpLockedSupportActive(*physics);
	const bool isGrounded = physics->walkSupportState != WalkSupportState::Air;
	std::array<float, 3> feetPosition = ToArray(character->GetPosition());
	JPH::Vec3 velocity = character->GetLinearVelocity();
	const JPH::Vec3 requestedHorizontalVelocity =
		moveSpeed * JPH::Vec3(moveDirection.x, 0.0f, moveDirection.z);
	if (isGrounded) {
		ClearWalkJumpBallisticHorizontalVelocity(*physics);
	}
	if (physics->walkJumpBallisticHorizontalVelocityActive) {
		switch (physics->walkAirControlMode) {
		case WalkAirControlMode::Realistic: {
			const JPH::Vec3 lockedDirection = physics->walkJumpBallisticHorizontalDirection;
			float alignedIntent = 0.0f;
			if (!lockedDirection.IsNearZero() && !requestedHorizontalVelocity.IsNearZero()) {
				const JPH::Vec3 requestedDirection = requestedHorizontalVelocity.NormalizedOr(JPH::Vec3::sZero());
				alignedIntent = std::clamp(requestedDirection.Dot(lockedDirection), 0.0f, 1.0f);
			}

			const float currentSpeed = physics->walkJumpBallisticHorizontalVelocity.Length();
			const float targetSpeed = physics->walkJumpBallisticHorizontalTakeoffSpeed * alignedIntent;
			const float maxSpeedDelta =
				(targetSpeed < currentSpeed ? kWalkJumpRealisticAirBrakeDeceleration : kWalkJumpRealisticAirReacceleration) *
				deltaSeconds;
			const float updatedSpeed =
				targetSpeed < currentSpeed ? std::max(targetSpeed, currentSpeed - maxSpeedDelta)
										   : std::min(targetSpeed, currentSpeed + maxSpeedDelta);
			physics->walkJumpBallisticHorizontalVelocity = lockedDirection * updatedSpeed;
			break;
		}
		case WalkAirControlMode::MinecraftLike: {
			const float currentSpeed = physics->walkJumpBallisticHorizontalVelocity.Length();
			const float targetSpeed = requestedHorizontalVelocity.Length();
			const float maxSpeedDelta =
				(targetSpeed < currentSpeed ? kWalkJumpMinecraftAirBrakeDeceleration
											: kWalkJumpMinecraftAirControlAcceleration) *
				deltaSeconds;
			physics->walkJumpBallisticHorizontalVelocity = MoveWalkJumpHorizontalVelocityTowards(
				physics->walkJumpBallisticHorizontalVelocity,
				requestedHorizontalVelocity,
				maxSpeedDelta);
			break;
		}
		}
	}
	const JPH::Vec3 desiredHorizontalVelocity =
		physics->walkJumpBallisticHorizontalVelocityActive ? physics->walkJumpBallisticHorizontalVelocity
														   : requestedHorizontalVelocity;
	const JPH::Vec3 desiredHorizontalDelta = desiredHorizontalVelocity * deltaSeconds;
	WalkSneakSupportRegion currentSneakSupportRegion{};
	if (physics->walkSneakActive && !hasJumpLockedSupport) {
		currentSneakSupportRegion = ComputeWalkSneakSupportRegion(*world, feetPosition);
	}
	WalkSneakSupportRegion activeSneakSupportRegion{};
	bool hasCurrentSneakSupport = false;
	if (currentSneakSupportRegion.valid) {
		hasCurrentSneakSupport =
			IsWalkFeetInsideSneakSupportRegion(currentSneakSupportRegion, feetPosition);
	}
	if (physics->walkSneakActive && !hasJumpLockedSupport) {
		if (hasCurrentSneakSupport) {
			activeSneakSupportRegion = currentSneakSupportRegion;
			physics->walkCachedSneakSupportRegion = activeSneakSupportRegion;
			physics->walkSneakSupportGraceFramesRemaining = kWalkSneakSupportGraceFrames;
		}

		if (!activeSneakSupportRegion.valid &&
			physics->walkSneakSupportGraceFramesRemaining > 0 &&
			physics->walkCachedSneakSupportRegion.valid) {
			activeSneakSupportRegion = physics->walkCachedSneakSupportRegion;
		}
	}
	const bool canHoldReleasedLedge =
		!physics->walkSneakActive &&
		physics->walkLedgeReleaseGraceFramesRemaining > 0 &&
		physics->walkCachedSneakSupportRegion.valid &&
		!hasJumpLockedSupport &&
		velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon;
	std::array<float, 3> jumpTakeoffFeetPosition{};
	bool hasJumpTakeoffFeetPosition = false;
	if (!activeSneakSupportRegion.valid && canHoldReleasedLedge) {
		const std::array<float, 3> releaseTargetFeetPosition =
			OffsetWalkFeetPosition(feetPosition, desiredHorizontalDelta);
		const bool currentInsideCachedReleaseRegion =
			IsWalkFeetInsideSneakSupportRegion(physics->walkCachedSneakSupportRegion, feetPosition);
		const bool targetInsideCachedReleaseRegion =
			!hasMovementInput ||
			IsWalkFeetInsideSneakSupportRegion(physics->walkCachedSneakSupportRegion, releaseTargetFeetPosition);
		const bool canCommitReleasedLedgeJumpTakeoff =
			jumpPressed &&
			TryBuildWalkJumpTakeoffFeetPosition(
				*physics,
				*world,
				physics->walkCachedSneakSupportRegion,
				jumpTakeoffFeetPosition);
		if ((currentInsideCachedReleaseRegion && (jumpPressed || targetInsideCachedReleaseRegion)) ||
			canCommitReleasedLedgeJumpTakeoff) {
			activeSneakSupportRegion = physics->walkCachedSneakSupportRegion;
			hasJumpTakeoffFeetPosition = canCommitReleasedLedgeJumpTakeoff;
		}
	}
	const bool hasActiveSneakSupportRegion = activeSneakSupportRegion.valid;
	const bool holdingReleasedLedge =
		!physics->walkSneakActive &&
		hasActiveSneakSupportRegion &&
		physics->walkLedgeReleaseGraceFramesRemaining > 0;
	if (jumpPressed && hasActiveSneakSupportRegion && !hasJumpTakeoffFeetPosition) {
		hasJumpTakeoffFeetPosition =
			TryBuildWalkJumpTakeoffFeetPosition(*physics, *world, activeSneakSupportRegion, jumpTakeoffFeetPosition);
	}
	if (jumpPressed && !hasJumpTakeoffFeetPosition && !hasActiveSneakSupportRegion &&
		!physics->walkJumpBallisticHorizontalVelocityActive &&
		velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon &&
		physics->walkCachedGroundTakeoffValid &&
		physics->walkGroundTakeoffGraceFramesRemaining > 0) {
		const float cachedDriftX = feetPosition[0] - physics->walkCachedGroundTakeoffFeetPosition[0];
		const float cachedDriftZ = feetPosition[2] - physics->walkCachedGroundTakeoffFeetPosition[2];
		const float cachedDriftDistanceSq = cachedDriftX * cachedDriftX + cachedDriftZ * cachedDriftZ;
		if (cachedDriftDistanceSq <=
			kWalkGroundTakeoffGraceMaxDrift * kWalkGroundTakeoffGraceMaxDrift + kWalkCollisionEpsilon) {
			jumpTakeoffFeetPosition = feetPosition;
			jumpTakeoffFeetPosition[1] =
				std::max(feetPosition[1], physics->walkCachedGroundTakeoffFeetPosition[1]);
			hasJumpTakeoffFeetPosition = true;
		}
	}
	if (jumpPressed && !hasJumpTakeoffFeetPosition && !hasActiveSneakSupportRegion &&
		velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon &&
		physics->walkFootSupportScore >= kWalkFootSupportEdgeGraceScore) {
		float groundedTakeoffFeetY = 0.0f;
		if (FindWalkBestSupportFeetYAtXZ(
				*world,
				feetPosition,
				0.0f,
				kWalkStickToFloorDistance,
				physics->walkSneakActive,
				kWalkGroundSupportRadius,
				groundedTakeoffFeetY)) {
			jumpTakeoffFeetPosition = feetPosition;
			jumpTakeoffFeetPosition[1] = groundedTakeoffFeetY;
			hasJumpTakeoffFeetPosition = true;
		}
	}
	const bool isGroundedLike = isGrounded || hasCurrentSneakSupport || hasActiveSneakSupportRegion;
	JPH::Vec3 appliedHorizontalDelta = desiredHorizontalDelta;
	WalkTopSupportCandidate targetTopSupport{};
	WalkTopSupportCandidate autoJumpTopSupport{};
	if (hasMovementInput) {
		if (velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon) {
			const std::array<float, 3> desiredFeetPosition =
				OffsetWalkFeetPosition(feetPosition, desiredHorizontalDelta);
			targetTopSupport = FindWalkTopSupportCandidate(
				*world,
				desiredFeetPosition,
				kWalkStairsStepUpHeight,
				physics->walkSneakActive);
			if (physics->walkAutoJumpEnabled &&
				!physics->walkSneakActive &&
				!hasActiveSneakSupportRegion &&
				physics->walkSupportState == WalkSupportState::Grounded &&
				!hasJumpLockedSupport) {
				autoJumpTopSupport = FindWalkTopSupportCandidate(
					*world,
					desiredFeetPosition,
					kWalkAutoJumpMaxRise,
					false);
			}
		}
	}
	bool autoJumpPressed = false;
	const float autoJumpRise = autoJumpTopSupport.valid ? autoJumpTopSupport.feetPosition[1] - feetPosition[1] : 0.0f;
	const bool hasAutoJumpReadyCandidate =
		physics->walkAutoJumpEnabled &&
		!jumpPressed &&
		hasMovementInput &&
		!physics->walkSneakActive &&
		!hasActiveSneakSupportRegion &&
		physics->walkSupportState == WalkSupportState::Grounded &&
		velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon &&
		autoJumpTopSupport.valid &&
		IsWalkAutoJumpRiseInRange(autoJumpRise);
	if (jumpPressed || jumpHeld || !hasAutoJumpReadyCandidate) {
		physics->walkAutoJumpDelayFramesRemaining = 0;
	} else {
		if (!physics->walkAutoJumpDelayEnabled) {
			autoJumpPressed = hasAutoJumpReadyCandidate;
		} else if (physics->walkAutoJumpDelayFramesRemaining == 0) {
			physics->walkAutoJumpDelayFramesRemaining = kWalkAutoJumpDelayFrames;
		} else {
			--physics->walkAutoJumpDelayFramesRemaining;
			autoJumpPressed = physics->walkAutoJumpDelayFramesRemaining == 0;
		}
	}
	const bool wantsJump = (jumpPressed || jumpHeld || autoJumpPressed) && (isGroundedLike || hasJumpTakeoffFeetPosition);
	if (physics->walkSneakActive && hasMovementInput) {
		const WalkSneakSupportRegion *constraintRegion = nullptr;
		if (hasActiveSneakSupportRegion) {
			constraintRegion = targetTopSupport.valid ? &targetTopSupport.region : &activeSneakSupportRegion;
		} else if (ShouldApplyWalkJumpLockedConstraint(*physics)) {
			constraintRegion = &physics->walkJumpLockedSupport.region;
		}

		if (constraintRegion != nullptr) {
			const JPH::Vec3 constrainedHorizontalDelta = ApplyWalkSneakBackoff(
				*world,
				*constraintRegion,
				feetPosition,
				desiredHorizontalDelta,
				physics->walkSneakActive);
			appliedHorizontalDelta = constrainedHorizontalDelta;
			if (ShouldApplyWalkJumpLockedConstraint(*physics)) {
				UpdateWalkJumpLockedSupportTarget(
					*physics,
					OffsetWalkFeetPosition(feetPosition, constrainedHorizontalDelta));
			}
		}
	}

	if (isGroundedLike && velocity.GetY() < 0.1f) {
		velocity.SetY(0.0f);
	}

	if (wantsJump) {
		if (hasMovementInput) {
			appliedHorizontalDelta = desiredHorizontalDelta;
		}
		const std::array<float, 3> jumpLockAnchorFeetPosition =
			hasJumpTakeoffFeetPosition ? jumpTakeoffFeetPosition : feetPosition;
		if (hasActiveSneakSupportRegion) {
			PrimeWalkJumpLockedSupport(
				*physics,
				activeSneakSupportRegion,
				jumpLockAnchorFeetPosition,
				physics->walkSneakActive && !hasMovementInput);
			UpdateWalkJumpLockedSupportTarget(*physics, OffsetWalkFeetPosition(jumpLockAnchorFeetPosition, appliedHorizontalDelta));
		} else {
			ClearWalkJumpLockedSupport(*physics);
		}
		if (hasJumpTakeoffFeetPosition) {
			feetPosition = jumpTakeoffFeetPosition;
		}
		if (!hasActiveSneakSupportRegion && !hasMovementInput) {
			physics->walkGroundReturnAnchorFeetPosition =
				hasJumpTakeoffFeetPosition ? jumpTakeoffFeetPosition : feetPosition;
			physics->walkGroundReturnAnchorFramesRemaining = kWalkGroundReturnAnchorFrames;
			physics->walkGroundReturnAnchorValid = true;
		} else {
			physics->walkGroundReturnAnchorFeetPosition = {};
			physics->walkGroundReturnAnchorFramesRemaining = 0;
			physics->walkGroundReturnAnchorValid = false;
		}
		physics->walkSupportState = WalkSupportState::Air;
		physics->walkEdgeGraceFramesRemaining = 0;
		if (hasActiveSneakSupportRegion) {
			physics->walkGroundTakeoffGraceFramesRemaining = 0;
			physics->walkCachedGroundTakeoffValid = false;
		}
		ClearWalkSneakSupportCache(*physics);
		physics->walkJumpBallisticHorizontalVelocity =
			deltaSeconds > kPhysicsDirectionEpsilon ? appliedHorizontalDelta / deltaSeconds : JPH::Vec3::sZero();
		physics->walkJumpBallisticHorizontalDirection =
			physics->walkJumpBallisticHorizontalVelocity.NormalizedOr(JPH::Vec3::sZero());
		physics->walkJumpBallisticHorizontalTakeoffSpeed = physics->walkJumpBallisticHorizontalVelocity.Length();
		physics->walkJumpBallisticHorizontalVelocity.SetY(0.0f);
		physics->walkJumpBallisticHorizontalVelocityActive = true;
		velocity.SetY(kWalkJumpSpeed);
	}

	const JPH::Vec3 gravity = physics->physicsSystem.GetGravity();
	const bool holdSneakGroundSupport =
		hasActiveSneakSupportRegion &&
		isGroundedLike &&
		!wantsJump &&
		(physics->walkSneakActive || holdingReleasedLedge);
	if (holdSneakGroundSupport && velocity.GetY() < 0.0f) {
		velocity.SetY(0.0f);
	} else {
		velocity += gravity * deltaSeconds;
	}

	const bool allowStepUp = !wantsJump && velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon;

	SweepWalkVertical(*world, feetPosition, velocity, deltaSeconds, physics->walkSneakActive);
	if (allowStepUp) {
		MoveWalkHorizontally<true>(*world, feetPosition, appliedHorizontalDelta, physics->walkSneakActive);
	} else {
		MoveWalkHorizontally<false>(*world, feetPosition, appliedHorizontalDelta, physics->walkSneakActive);
	}
	if (!wantsJump &&
		!hasMovementInput &&
		velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon) {
		TryRestoreWalkGroundReturnAnchorPlane(*physics, *world, feetPosition, velocity);
	}
	if (!wantsJump &&
		physics->walkJumpLockedSupport.valid &&
		velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon) {
		std::array<float, 3> correctedJumpLockedFeetPosition{};
		if (TryBuildWalkSupportCorrectedFeetPosition(
				*physics,
				*world,
				physics->walkJumpLockedSupport.region,
				feetPosition,
				kWalkCapsuleRadius,
				kWalkJumpLockedSupportMaxDropBelowReference,
				correctedJumpLockedFeetPosition)) {
			feetPosition = correctedJumpLockedFeetPosition;
			if (velocity.GetY() < 0.0f) {
				velocity.SetY(0.0f);
			}
		}
	}
	if (physics->walkSneakActive &&
		!hasMovementInput &&
		!wantsJump &&
		!hasActiveSneakSupportRegion &&
		velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon) {
		TryReacquireWalkSneakGroundSupport(*physics, *world, feetPosition, velocity);
	}
	if ((holdingReleasedLedge || physics->walkSneakActive) &&
		hasActiveSneakSupportRegion &&
		!wantsJump &&
		velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon) {
		const std::array<float, 2> projectedFeetXZ = ProjectWalkFeetToSneakSupportRegion(activeSneakSupportRegion, feetPosition);
		feetPosition[0] = projectedFeetXZ[0];
		feetPosition[2] = projectedFeetXZ[1];
		TryRestoreWalkSupportRegionPlane(
			*world,
			activeSneakSupportRegion,
			feetPosition,
			velocity,
			physics->walkSneakActive);
	}

	const float snapDropDistance =
		!wantsJump && velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon
			? (hasActiveSneakSupportRegion || physics->walkJumpLockedSupport.valid || holdingReleasedLedge ? kWalkSneakStickToFloorDistance
																										   : kWalkStickToFloorDistance) +
				  std::max(0.0f, -velocity.GetY() * deltaSeconds)
			: 0.0f;
	const bool allowPlainFloorSnap =
		!wantsJump &&
		velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon &&
		(physics->walkSupportState != WalkSupportState::Air ||
		 hasCurrentSneakSupport ||
		 hasActiveSneakSupportRegion ||
		 physics->walkJumpLockedSupport.valid ||
		 holdingReleasedLedge);
	bool snappedToFloor = allowPlainFloorSnap &&
						  SnapWalkToFloor(*world, feetPosition, velocity, physics->walkSneakActive, snapDropDistance);
	if (!snappedToFloor) {
		snappedToFloor = TrySnapWalkToGroundReturnAnchor(*physics, *world, feetPosition, velocity, snapDropDistance);
	}
	if (!snappedToFloor) {
		TrySnapWalkToGroundTakeoffAnchor(*physics, *world, feetPosition, velocity, snapDropDistance);
	}
	const bool allowUpwardPenetrationResolve =
		!wantsJump &&
		(physics->walkSupportState != WalkSupportState::Air ||
		 hasActiveSneakSupportRegion ||
		 physics->walkJumpLockedSupport.valid ||
		 holdingReleasedLedge);
	ResolveWalkCharacterPenetration(*world, feetPosition, physics->walkSneakActive, allowUpwardPenetrationResolve);

	character->SetPosition(ToRVec3(feetPosition));
	character->SetLinearVelocity(velocity);
	character->SetRotation(JPH::Quat::sRotation(JPH::Vec3::sAxisY(), camera->yawRadians));

	UpdateWalkJumpLockedSupportTarget(*physics, feetPosition);
	UpdateWalkJumpLockedSupportLifetime(*physics, *world);

	if (physics->walkSneakActive && !physics->walkJumpLockedSupport.valid && !wantsJump) {
		const WalkSneakSupportRegion postUpdateSupportRegion = ComputeWalkSneakSupportRegion(*world, feetPosition);
		if (postUpdateSupportRegion.valid &&
			IsWalkFeetInsideSneakSupportRegion(postUpdateSupportRegion, feetPosition)) {
			physics->walkCachedSneakSupportRegion = postUpdateSupportRegion;
			physics->walkSneakSupportGraceFramesRemaining = kWalkSneakSupportGraceFrames;
		} else if (physics->walkSneakSupportGraceFramesRemaining > 0) {
			--physics->walkSneakSupportGraceFramesRemaining;
		}
		physics->walkLedgeReleaseGraceFramesRemaining = 0;
		ReleaseWalkSneakSupportCacheIfUnused(*physics);
	} else if (!physics->walkSneakActive && !wantsJump) {
		physics->walkSneakSupportGraceFramesRemaining = 0;
		const bool canRefreshReleaseHold =
			physics->walkCachedSneakSupportRegion.valid &&
			!physics->walkJumpLockedSupport.valid &&
			velocity.GetY() <= kWalkSneakStickPositiveVelocityEpsilon &&
			IsWalkFeetInsideSneakSupportRegion(physics->walkCachedSneakSupportRegion, feetPosition);
		if (canRefreshReleaseHold) {
			physics->walkLedgeReleaseGraceFramesRemaining = kWalkLedgeReleaseGraceFrames;
		} else if (physics->walkLedgeReleaseGraceFramesRemaining > 0) {
			--physics->walkLedgeReleaseGraceFramesRemaining;
		}
		ReleaseWalkSneakSupportCacheIfUnused(*physics);
	} else {
		ClearWalkSneakSupportCache(*physics);
	}

	physics->walkHadHorizontalMotionLastStep = !appliedHorizontalDelta.IsNearZero();
	UpdateWalkGroundSupport(*physics, *world, jumpHeld);
	if (physics->walkSupportState != WalkSupportState::Air) {
		ClearWalkJumpBallisticHorizontalVelocity(*physics);
	}
	PlotWalkProfilingState(*physics, feetPosition, velocity);
	PlotBroadphaseDiagnostics(*physics);
	if (physics->walkGroundReturnAnchorValid) {
		const float anchorDriftX = feetPosition[0] - physics->walkGroundReturnAnchorFeetPosition[0];
		const float anchorDriftZ = feetPosition[2] - physics->walkGroundReturnAnchorFeetPosition[2];
		if (anchorDriftX * anchorDriftX + anchorDriftZ * anchorDriftZ >
				kWalkGroundTakeoffLandingMaxDrift * kWalkGroundTakeoffLandingMaxDrift + kWalkCollisionEpsilon ||
			physics->walkGroundReturnAnchorFramesRemaining == 0) {
			physics->walkGroundReturnAnchorFeetPosition = {};
			physics->walkGroundReturnAnchorFramesRemaining = 0;
			physics->walkGroundReturnAnchorValid = false;
		} else if (physics->walkSupportState == WalkSupportState::Air) {
			--physics->walkGroundReturnAnchorFramesRemaining;
		}
	}
	UpdateCameraFromWalkCharacter(*physics, *camera);
	return true;
}

bool TickCreativeCharacter(
	PhysicsState *physics,
	const VoxelWorld *world,
	CameraState *camera,
	const InputState *input,
	const float deltaSeconds)
{
	if (!physics || !world || !camera || !input) {
		return false;
	}

	if (!physics->walkCharacterInitialized) {
		if (!RebuildCharacterFromCamera(*physics, *world, *camera)) {
			return false;
		}
	}

	if (deltaSeconds <= 0.0f) {
		UpdateCameraFromWalkCharacter(*physics, *camera);
		return true;
	}

	const auto [x, y, z] = ComputeCreativeMoveDirection(*camera, *input);
	float moveSpeed = camera->moveSpeed * kCreativeMoveSpeedMultiplier;
	if (IsInputActionDown(*input, InputAction::SpeedBoost)) {
		moveSpeed *= kCreativeBoostMultiplier;
	}
	if (IsInputActionDown(*input, InputAction::SpeedSlow)) {
		moveSpeed *= kCreativeSlowMultiplier;
	}

	JPH::CharacterVirtual *character = physics->walkCharacter.GetPtr();
	const JPH::Vec3 desiredVelocity = moveSpeed * JPH::Vec3(x, y, z);
	character->SetRotation(JPH::Quat::sRotation(JPH::Vec3::sAxisY(), camera->yawRadians));

	const JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings = BuildCreativeUpdateSettings();
	uint32_t substepCount = 1;
	const float desiredTravelDistance = desiredVelocity.Length() * deltaSeconds;
	if (desiredTravelDistance > kCreativeCollisionMaxStepDistance + kWalkCollisionEpsilon) {
		substepCount = static_cast<uint32_t>(std::ceil(desiredTravelDistance / kCreativeCollisionMaxStepDistance));
		substepCount = std::clamp(substepCount, 1u, kCreativeCollisionMaxSubsteps);
	}

	const float substepDeltaSeconds = deltaSeconds / static_cast<float>(substepCount);
	for (uint32_t substepIndex = 0; substepIndex < substepCount; ++substepIndex) {
		character->SetLinearVelocity(desiredVelocity);
		character->ExtendedUpdate(
			substepDeltaSeconds,
			JPH::Vec3::sZero(),
			updateSettings,
			physics->physicsSystem.GetDefaultBroadPhaseLayerFilter(PhysicsLayers::Moving),
			physics->physicsSystem.GetDefaultLayerFilter(PhysicsLayers::Moving),
			{},
			{},
			physics->tempAllocator);
	}

	UpdateCameraFromWalkCharacter(*physics, *camera);
	return true;
}

bool DoesPhysicsCharacterOverlapVoxel(
	const PhysicsState *physics,
	const CameraState &camera,
	const Int3 &voxel)
{
	if (physics == nullptr) {
		return false;
	}

	const bool sneakActive =
		camera.controlMode == CameraState::ControlMode::Walk && physics->walkSneakActive;
	const std::array<float, 3> feetPosition =
		physics->walkCharacterInitialized && physics->walkCharacter.GetPtr() != nullptr
			? ToArray(physics->walkCharacter->GetPosition())
			: BuildFallbackWalkFeetPosition(*physics, camera);
	return DoesWalkCharacterBodyOverlapVoxel(feetPosition, sneakActive, voxel);
}

void SetPhysicsWalkAirControlMode(PhysicsState *physics, const WalkAirControlMode mode)
{
	if (physics == nullptr) {
		return;
	}

	physics->walkAirControlMode = mode;
	if (!physics->walkJumpBallisticHorizontalVelocityActive) {
		return;
	}

	JPH::Vec3 horizontalVelocity = physics->walkJumpBallisticHorizontalVelocity;
	horizontalVelocity.SetY(0.0f);
	physics->walkJumpBallisticHorizontalVelocity = horizontalVelocity;
	physics->walkJumpBallisticHorizontalDirection = horizontalVelocity.NormalizedOr(JPH::Vec3::sZero());
	physics->walkJumpBallisticHorizontalTakeoffSpeed = horizontalVelocity.Length();
}

WalkAirControlMode GetPhysicsWalkAirControlMode(const PhysicsState *physics)
{
	return physics != nullptr ? physics->walkAirControlMode : WalkAirControlMode::MinecraftLike;
}

void SetPhysicsWalkAutoJumpEnabled(PhysicsState *physics, const bool enabled)
{
	if (physics == nullptr) {
		return;
	}

	physics->walkAutoJumpEnabled = enabled;
	physics->walkAutoJumpDelayFramesRemaining = 0;
}

bool IsPhysicsWalkAutoJumpEnabled(const PhysicsState *physics)
{
	return physics != nullptr ? physics->walkAutoJumpEnabled : false;
}

void SetPhysicsWalkAutoJumpDelayEnabled(PhysicsState *physics, const bool enabled)
{
	if (physics == nullptr) {
		return;
	}

	physics->walkAutoJumpDelayEnabled = enabled;
	physics->walkAutoJumpDelayFramesRemaining = 0;
}

bool IsPhysicsWalkAutoJumpDelayEnabled(const PhysicsState *physics)
{
	return physics != nullptr ? physics->walkAutoJumpDelayEnabled : true;
}

PhysicsWalkDebugInfo GetPhysicsWalkDebugInfo(const PhysicsState *physics)
{
	PhysicsWalkDebugInfo info{};
	if (physics == nullptr ||
		!physics->walkCharacterInitialized ||
		physics->walkCharacter.GetPtr() == nullptr) {
		return info;
	}

	info.valid = true;
	info.feetPosition = ToArray(physics->walkCharacter->GetPosition());
	info.footSupportScore = physics->walkFootSupportScore;
	info.footSupportHitSamples = physics->walkFootSupportHitSamples;
	info.footSupportTotalSamples = physics->walkFootSupportTotalSamples;
	info.edgeGraceFramesRemaining = physics->walkEdgeGraceFramesRemaining;
	info.groundTakeoffGraceFramesRemaining = physics->walkGroundTakeoffGraceFramesRemaining;
	info.sneakSupportGraceFramesRemaining = physics->walkSneakSupportGraceFramesRemaining;
	info.ledgeReleaseGraceFramesRemaining = physics->walkLedgeReleaseGraceFramesRemaining;
	info.autoJumpDelayFramesRemaining = physics->walkAutoJumpDelayFramesRemaining;
	info.groundTakeoffCached = physics->walkCachedGroundTakeoffValid;
	info.cachedSneakSupportValid = physics->walkCachedSneakSupportRegion.valid;
	info.feetInsideCachedSneakSupport =
		physics->walkCachedSneakSupportRegion.valid &&
		IsWalkFeetInsideSneakSupportRegion(physics->walkCachedSneakSupportRegion, info.feetPosition);
	info.sneakActive = physics->walkSneakActive;
	info.jumpLockActive = physics->walkJumpLockedSupport.valid;
	info.suppressPassiveSlide = physics->walkSuppressPassiveSlide;
	info.autoJumpEnabled = physics->walkAutoJumpEnabled;
	info.autoJumpDelayEnabled = physics->walkAutoJumpDelayEnabled;
	info.cachedSneakSupportReferenceFeetY =
		physics->walkCachedSneakSupportRegion.valid ? physics->walkCachedSneakSupportRegion.referenceFeetPosition[1] : 0.0f;

	switch (physics->walkSupportState) {
	case WalkSupportState::Grounded:
		info.supportState = PhysicsWalkSupportDebugState::Grounded;
		break;
	case WalkSupportState::EdgeGrace:
		info.supportState = PhysicsWalkSupportDebugState::EdgeGrace;
		break;
	case WalkSupportState::Air:
	default:
		info.supportState = PhysicsWalkSupportDebugState::Air;
		break;
	}

	return info;
}

uint64_t GetPhysicsWorldSyncVersion(const PhysicsState *physics)
{
	return physics != nullptr ? physics->syncedWorldEditVersion : 0;
}

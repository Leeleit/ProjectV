#pragma once

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

// shared constants
constexpr float kPhysicsDirectionEpsilon = 0.00001f;
constexpr float kPhysicsRaycastVoxelEpsilon = 0.001f;
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

struct WalkSneakSupportFace {
	std::array<float, 2> min{};
	std::array<float, 2> max{};
};

struct WalkSneakSupportRegion {
	std::array<WalkSneakSupportFace, 36> faces{};
	uint32_t faceCount = 0;
	float sampleRadius = 0.0f;
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

struct PhysicsState;

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

inline JoltRuntimeState &GetJoltRuntimeState()
{
	static JoltRuntimeState state{};
	return state;
}

inline void AcquireJoltRuntime()
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

inline void ReleaseJoltRuntime()
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

inline Int3 FloorToVoxel(const std::array<float, 3> &position)
{
	return {
		static_cast<int>(std::floor(position[0])),
		static_cast<int>(std::floor(position[1])),
		static_cast<int>(std::floor(position[2])),
	};
}

inline bool IsPhysicsSolidMaterial(const VoxelMaterial material)
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

// helper functions shared across physics TUs
inline Float3 Normalize(const Float3 vector)
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

inline bool IsZeroVector(const Float3 vector)
{
	return std::abs(vector.x) <= kPhysicsDirectionEpsilon &&
		   std::abs(vector.y) <= kPhysicsDirectionEpsilon &&
		   std::abs(vector.z) <= kPhysicsDirectionEpsilon;
}

inline JPH::RVec3 ToRVec3(const std::array<float, 3> &value)
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

// cross-TU function declarations
void RefreshWalkCharacterContacts(PhysicsState &physics);
void InvalidateWalkSupportStateForWorldEdit(PhysicsState &physics);

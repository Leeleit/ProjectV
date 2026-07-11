#pragma once

#include "physics/PhysicsWorld_Internal.hpp"
#include "physics/walk/WalkConstants.hpp"

int GetWalkSneakSupportVoxelY(const WalkSneakSupportRegion &region);

bool DoesWalkSneakSupportRegionContainVoxel(
	const WalkSneakSupportRegion &region,
	const int voxelX,
	const int voxelZ);

bool IsWalkJumpLockedSupportTargetInsideRegion(const PhysicsState &physics);

bool IsPhysicsStaticWorldBodyId(const PhysicsState &physics, const JPH::BodyID &bodyId);

bool IsWalkJumpLockedSourceSupportSideWallContact(
	const PhysicsState &physics,
	const JPH::BodyID &bodyId,
	JPH::RVec3Arg contactPosition,
	JPH::Vec3Arg contactNormal);

bool IsSolidAtPosition(const VoxelWorld &world, const std::array<float, 3> &position);

float GetWalkEyeHeight(const PhysicsState &physics, const CameraState::ControlMode controlMode);

Float3 Cross(const Float3 a, const Float3 b);

Float3 GetWalkForwardVector(const CameraState &camera);

int64_t ToWalkSupportProfilingValue(const WalkSupportState state);

int64_t ToWalkAirControlProfilingValue(const WalkAirControlMode mode);

void PlotWalkProfilingState(
	const PhysicsState &physics,
	const std::array<float, 3> &feetPosition,
	const JPH::Vec3 &velocity);

void PlotBroadphaseDiagnostics(const PhysicsState &physics);

bool EnsureWalkCharacter(PhysicsState &physics);

[[maybe_unused]] JPH::CharacterVirtual::ExtendedUpdateSettings BuildWalkUpdateSettings();

JPH::CharacterVirtual::ExtendedUpdateSettings BuildCreativeUpdateSettings();

[[maybe_unused]] JPH::CharacterVirtual::ExtendedUpdateSettings BuildWalkEdgeGraceUpdateSettings();

bool SetWalkSneakActive(
	PhysicsState &physics,
	const bool sneakActive);

void ClearWalkSneakSupportCache(PhysicsState &physics);

void ClearWalkJumpLockedSupport(PhysicsState &physics);

void ClearWalkJumpBallisticHorizontalVelocity(PhysicsState &physics);

JPH::Vec3 MoveWalkJumpHorizontalVelocityTowards(
	const JPH::Vec3 &currentVelocity,
	const JPH::Vec3 &targetVelocity,
	const float maxSpeedDelta);

bool IsWalkJumpLockedSupportActive(const PhysicsState &physics);

bool ShouldApplyWalkJumpLockedConstraint(const PhysicsState &physics);

void UpdateWalkJumpLockedSupportTarget(
	PhysicsState &physics,
	const std::array<float, 3> &feetPosition);

void PrimeWalkJumpLockedSupport(
	PhysicsState &physics,
	const WalkSneakSupportRegion &supportRegion,
	const std::array<float, 3> &anchorFeetPosition,
	const bool constrainMovementWhileSneakHeld);

void UpdateWalkJumpLockedSupportLifetime(PhysicsState &physics, const VoxelWorld &world);

void ReleaseWalkSneakSupportCacheIfUnused(PhysicsState &physics);

void RefreshWalkCharacterContacts(PhysicsState &physics);

WalkFootSupportInfo ComputeVoxelFootSupportInfo(const VoxelWorld &world, const std::array<float, 3> &feetPosition);

std::array<float, 3> OffsetWalkFeetPosition(
	const std::array<float, 3> &feetPosition,
	const JPH::Vec3 &horizontalDelta);

float GetWalkCapsuleHalfHeight(const bool sneakActive);

bool IsWalkCharacterClearAt(
	const VoxelWorld &world,
	const std::array<float, 3> &feetPosition,
	const bool sneakActive);

bool HasWalkSneakSupport(const VoxelWorld &world, const std::array<float, 3> &feetPosition);

[[maybe_unused]] float GetWalkSneakDistanceOutsideInterval(const float value, const float minBound, const float maxBound);

bool IsWalkFeetInsideSneakBackoffRegion(
	const WalkSneakSupportRegion &region,
	const std::array<float, 3> &feetPosition);

bool IsWalkFeetInsideSneakSupportFace(
	const WalkSneakSupportFace &face,
	const float sampleRadius,
	const std::array<float, 3> &feetPosition);

std::array<float, 2> ProjectWalkFeetToSneakSupportFace(
	const WalkSneakSupportFace &face,
	const float sampleRadius,
	const std::array<float, 2> &feetXZ);

std::array<float, 2> ProjectWalkFeetToSneakSupportRegion(
	const WalkSneakSupportRegion &region,
	const std::array<float, 3> &feetPosition);

WalkSneakSupportRegion ComputeWalkSneakSupportRegion(
	const VoxelWorld &world,
	const std::array<float, 3> &feetPosition);

bool IsWalkFeetInsideSneakSupportRegion(
	const WalkSneakSupportRegion &region,
	const std::array<float, 3> &feetPosition);

float GetWalkBodyHeight(const bool sneakActive);

float GetWalkSupportPlaneY(const float feetY);

bool DoesWalkFootprintOverlapVoxel(
	const float centerX,
	const float centerZ,
	const int voxelX,
	const int voxelZ,
	const float footprintRadius);

bool FindWalkBestSupportFeetYAtXZ(
	const VoxelWorld &world,
	const std::array<float, 3> &referenceFeetPosition,
	const float maxRise,
	const float maxDrop,
	const bool sneakActive,
	const float footprintRadius,
	float &outFeetY);

WalkTopSupportCandidate FindWalkTopSupportCandidate(
	const VoxelWorld &world,
	const std::array<float, 3> &desiredFeetPosition,
	const float maxRise,
	const bool sneakActive);

bool IsWalkAutoJumpRiseInRange(const float rise);

bool ResolveWalkCharacterPenetration(
	const VoxelWorld &world,
	std::array<float, 3> &feetPosition,
	const bool sneakActive,
	const bool allowUpwardResolve);

bool TryBuildWalkSupportCorrectedFeetPosition(
	const PhysicsState &physics,
	const VoxelWorld &world,
	const WalkSneakSupportRegion &supportRegion,
	const std::array<float, 3> &sourceFeetPosition,
	const float maxHorizontalCorrection,
	const float maxUpwardRestore,
	std::array<float, 3> &outFeetPosition);

bool TryBuildWalkJumpTakeoffFeetPosition(
	const PhysicsState &physics,
	const VoxelWorld &world,
	const WalkSneakSupportRegion &supportRegion,
	std::array<float, 3> &outFeetPosition);

bool TryReacquireWalkSneakGroundSupport(
	PhysicsState &physics,
	const VoxelWorld &world,
	std::array<float, 3> &feetPosition,
	JPH::Vec3 &velocity);

bool CanWalkSneakMoveInsideSupportRegion(
	const VoxelWorld &world,
	const WalkSneakSupportRegion &supportRegion,
	const std::array<float, 3> &feetPosition,
	const bool sneakActive);

float BackOffWalkSneakDeltaComponent(const float delta);

JPH::Vec3 ApplyWalkSneakBackoff(
	const VoxelWorld &world,
	const WalkSneakSupportRegion &supportRegion,
	const std::array<float, 3> &feetPosition,
	const JPH::Vec3 &desiredHorizontalDelta,
	const bool sneakActive);

bool TryRestoreWalkSupportRegionPlane(
	const VoxelWorld &world,
	const WalkSneakSupportRegion &supportRegion,
	std::array<float, 3> &feetPosition,
	JPH::Vec3 &velocity,
	const bool sneakActive);

bool SweepWalkVertical(
	const VoxelWorld &world,
	std::array<float, 3> &feetPosition,
	JPH::Vec3 &velocity,
	const float deltaSeconds,
	const bool sneakActive);

bool SnapWalkToFloor(
	const VoxelWorld &world,
	std::array<float, 3> &feetPosition,
	JPH::Vec3 &velocity,
	const bool sneakActive,
	const float maxDrop);

bool TrySnapWalkToGroundTakeoffAnchor(
	const PhysicsState &physics,
	const VoxelWorld &world,
	std::array<float, 3> &feetPosition,
	JPH::Vec3 &velocity,
	const float maxDrop);

bool TrySnapWalkToGroundReturnAnchor(
	const PhysicsState &physics,
	const VoxelWorld &world,
	std::array<float, 3> &feetPosition,
	JPH::Vec3 &velocity,
	const float maxDrop);

bool TryRestoreWalkGroundReturnAnchorPlane(
	const PhysicsState &physics,
	const VoxelWorld &world,
	std::array<float, 3> &feetPosition,
	JPH::Vec3 &velocity);

[[maybe_unused]] bool TryStickWalkSneakToFloor(
	PhysicsState &physics,
	const VoxelWorld &world,
	const WalkSneakSupportRegion &supportRegion);

[[maybe_unused]] WalkSupportContactKey SelectWalkPassiveSlideContact(
	const PhysicsState &physics,
	const WalkFootSupportInfo &supportInfo);

void UpdateWalkGroundSupport(
	PhysicsState &physics,
	const VoxelWorld &world,
	const bool allowNarrowJumpEdgeSupport = false);

void UpdateCameraFromWalkCharacter(const PhysicsState &physics, CameraState &camera);

bool TryBuildWalkSpawnFromRay(
	const PhysicsState &physics,
	const std::array<float, 3> &origin,
	const float maxDistance,
	std::array<float, 3> *outFeetPosition);

std::array<float, 3> BuildFallbackWalkFeetPosition(const PhysicsState &physics, const CameraState &camera);

bool DoesWalkCharacterBodyOverlapVoxel(
	const std::array<float, 3> &feetPosition,
	const bool sneakActive,
	const Int3 &voxel);

bool CameraNeedsGroundRecovery(const PhysicsState &physics, const VoxelWorld &world, const CameraState &camera);

void ApplyWalkCharacterState(
	PhysicsState &physics,
	const VoxelWorld &world,
	CameraState &camera,
	const std::array<float, 3> &feetPosition);

bool RebuildCharacterFromCamera(
	PhysicsState &physics,
	const VoxelWorld &world,
	CameraState &camera);

Float3 ComputeWalkMoveDirection(const CameraState &camera, const InputState &input);

Float3 ComputeCreativeMoveDirection(const CameraState &camera, const InputState &input);

inline bool HasMoveUpInputActionMaskBit(const uint32_t mask)
{
	return (mask & 1u << static_cast<uint32_t>(InputAction::MoveUp)) != 0u;
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


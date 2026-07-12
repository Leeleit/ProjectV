#include "physics/PhysicsWorld.hpp"
#include "physics/PhysicsWorld_Internal.hpp"
#include "physics/walk/WalkInternals.hpp"
#include "physics/walk/WalkConstants.hpp"

#include "app/InputActions.hpp"

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

	static_assert(static_cast<uint8_t>(WalkSupportState::Grounded) == static_cast<uint8_t>(PhysicsWalkSupportDebugState::Grounded));
	static_assert(static_cast<uint8_t>(WalkSupportState::EdgeGrace) == static_cast<uint8_t>(PhysicsWalkSupportDebugState::EdgeGrace));
	static_assert(static_cast<uint8_t>(WalkSupportState::Air) == static_cast<uint8_t>(PhysicsWalkSupportDebugState::Air));
	info.supportState = static_cast<PhysicsWalkSupportDebugState>(physics->walkSupportState);

	return info;
}


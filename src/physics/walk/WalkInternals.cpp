#include "physics/walk/WalkInternals.hpp"
#include "physics/PhysicsWorld_Internal.hpp"

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
	for (const auto &[chunkIndex, chunkBodyId] : physics.chunkStaticBodies) {
		(void)chunkIndex;
		if (chunkBodyId == bodyId) {
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


bool IsSolidAtPosition(const VoxelWorld &world, const std::array<float, 3> &position)
{
	const Int3 voxel = FloorToVoxel(position);
	return IsInsideVoxelWorld(world, voxel) && IsPhysicsSolidMaterial(GetVoxelMaterial(world, voxel));
}

float GetWalkEyeHeight(const PhysicsState &physics, const CameraState::ControlMode controlMode)
{
	return controlMode == CameraState::ControlMode::Walk && physics.walkSneakActive ? kWalkSneakEyeHeight : kWalkEyeHeight;
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


JPH::RefConst<JPH::Shape> CreateWalkCharacterShape(const float capsuleHalfHeight, const char *step)
{
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
		return nullptr;
	}

	return characterShapeResult.Get();
}

bool EnsureWalkCharacter(PhysicsState &physics)
{
	if (physics.walkCharacter != nullptr) {
		return true;
	}

	if (physics.walkStandingShape == nullptr) {
		physics.walkStandingShape = CreateWalkCharacterShape(kWalkCapsuleHalfHeight, "EnsureWalkCharacter.CreateStandingShape");
		if (physics.walkStandingShape == nullptr) {
			return false;
		}
	}
	if (physics.walkSneakShape == nullptr) {
		physics.walkSneakShape = CreateWalkCharacterShape(kWalkSneakCapsuleHalfHeight, "EnsureWalkCharacter.CreateSneakShape");
		if (physics.walkSneakShape == nullptr) {
			return false;
		}
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
	return true;
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
	const bool allowNarrowJumpEdgeSupport)
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

		if (TryBuildWalkSpawnFromRay(
				physics,
				{
					camera.position[0],
					std::max(camera.position[1] + 16.0f, static_cast<float>(world.maxExclusive.y) + 16.0f),
					camera.position[2],
				},
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


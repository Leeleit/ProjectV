#include "physics/PhysicsWorld.hpp"

#include "app/InputActions.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "core/Types.hpp"

#include <algorithm>
#include <cmath>
#include <mutex>

#pragma warning(push, 0)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#pragma clang diagnostic pop
#pragma warning(pop)

namespace {
constexpr float kPhysicsDirectionEpsilon = 0.00001f;
constexpr float kPhysicsRaycastVoxelEpsilon = 0.001f;
constexpr float kWalkCapsuleRadius = 0.35f;
constexpr float kWalkCapsuleHalfHeight = 0.55f;
constexpr float kWalkEyeHeight = 1.6f;
constexpr float kWalkMoveSpeed = 4.5f;
constexpr float kWalkBoostMultiplier = 1.8f;
constexpr float kWalkSlowMultiplier = 0.35f;
constexpr float kWalkJumpSpeed = 6.5f;
constexpr float kWalkSpawnClearance = 0.05f;
constexpr float kCreativeMoveSpeedMultiplier = 1.0f;
constexpr float kCreativeBoostMultiplier = 3.0f;
constexpr float kCreativeSlowMultiplier = 0.25f;
constexpr uint32_t kMaxPhysicsBodies = 32;
constexpr uint32_t kMaxBodyPairs = 64;
constexpr uint32_t kMaxContactConstraints = 64;
constexpr size_t kPhysicsTempAllocatorBytes = static_cast<size_t>(4) * 1024 * 1024;

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

Int3 FloorToVoxel(const std::array<float, 3> &position);

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
	JPH::PhysicsSystem physicsSystem{};
	JPH::TempAllocatorImpl tempAllocator{static_cast<unsigned int>(kPhysicsTempAllocatorBytes)};
	JPH::BodyID staticWorldBodyId;
	JPH::RefConst<JPH::Shape> staticWorldShape;
	JPH::Ref<JPH::CharacterVirtual> walkCharacter;
	const VoxelWorld *syncedWorld = nullptr;
	uint64_t syncedWorldEditVersion = 0;
	bool walkCharacterInitialized = false;
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
	JPH::StaticCompoundShapeSettings compoundSettings;
	JPH::RefConst<JPH::Shape> voxelShape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));

	size_t solidVoxelCount = 0;
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

	if (solidVoxelCount == 0) {
		return true;
	}

	JPH::ShapeSettings::ShapeResult shapeResult = compoundSettings.Create(physics.tempAllocator);
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

	const JPH::RotatedTranslatedShapeSettings characterShapeSettings(
		JPH::Vec3(0.0f, kWalkCapsuleHalfHeight + kWalkCapsuleRadius, 0.0f),
		JPH::Quat::sIdentity(),
		new JPH::CapsuleShape(kWalkCapsuleHalfHeight, kWalkCapsuleRadius));
	const JPH::ShapeSettings::ShapeResult characterShapeResult = characterShapeSettings.Create();
	if (!characterShapeResult.IsValid()) {
		runtime::LogRuntimeFailure(
			"Physics",
			"EnsureWalkCharacter.CreateShape",
			characterShapeResult.GetError());
		return false;
	}

	const JPH::Ref characterSettings = new JPH::CharacterVirtualSettings();
	characterSettings->mShape = characterShapeResult.Get();
	characterSettings->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -kWalkCapsuleRadius);
	characterSettings->mMaxStrength = 0.0f;
	characterSettings->mMass = 80.0f;

	physics.walkCharacter = new JPH::CharacterVirtual(
		characterSettings.GetPtr(),
		JPH::RVec3(0.0, 0.0, 0.0),
		JPH::Quat::sIdentity(),
		&physics.physicsSystem);
	physics.walkCharacterInitialized = false;
	return physics.walkCharacter != nullptr;
}

void UpdateCameraFromWalkCharacter(const PhysicsState &physics, CameraState &camera)
{
	if (physics.walkCharacter == nullptr) {
		return;
	}

	const std::array<float, 3> feetPosition = ToArray(physics.walkCharacter->GetPosition());
	camera.position = {
		feetPosition[0],
		feetPosition[1] + kWalkEyeHeight,
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

std::array<float, 3> BuildFallbackWalkFeetPosition(const CameraState &camera)
{
	return {
		camera.position[0],
		camera.position[1] - kWalkEyeHeight,
		camera.position[2],
	};
}

bool CameraNeedsGroundRecovery(const VoxelWorld &world, const CameraState &camera)
{
	const std::array<float, 3> feetPosition = BuildFallbackWalkFeetPosition(camera);
	return IsSolidAtPosition(world, feetPosition) ||
		   IsSolidAtPosition(world, {feetPosition[0], feetPosition[1] + 0.9f, feetPosition[2]}) ||
		   IsSolidAtPosition(world, camera.position);
}

void ApplyWalkCharacterState(PhysicsState &physics, CameraState &camera, const std::array<float, 3> &feetPosition)
{
	physics.walkCharacter->SetPosition(ToRVec3(feetPosition));
	physics.walkCharacter->SetRotation(JPH::Quat::sRotation(JPH::Vec3::sAxisY(), camera.yawRadians));
	physics.walkCharacter->SetLinearVelocity(JPH::Vec3::sZero());
	physics.walkCharacterInitialized = true;
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

	std::array<float, 3> feetPosition = BuildFallbackWalkFeetPosition(camera);
	if (CameraNeedsGroundRecovery(world, camera)) {
		if (TryBuildWalkSpawnFromRay(
				physics,
				{
					camera.position[0],
					camera.position[1] + 0.5f,
					camera.position[2],
				},
				std::max(32.0f, static_cast<float>(world.height) + 16.0f),
				&feetPosition)) {
			ApplyWalkCharacterState(physics, camera, feetPosition);
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
			ApplyWalkCharacterState(physics, camera, feetPosition);
			return true;
		}
	}

	ApplyWalkCharacterState(physics, camera, feetPosition);
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
	if (!physics) {
		return false;
	}

	if (physics->syncedWorld == world &&
		physics->syncedWorldEditVersion == (world != nullptr ? world->editVersion : 0)) {
		return true;
	}

	DestroyStaticWorldBody(*physics);
	if (!world) {
		physics->syncedWorld = nullptr;
		physics->syncedWorldEditVersion = 0;
		ResetWalkCharacter(physics);
		return true;
	}

	if (!BuildStaticVoxelCollisionBody(*physics, *world)) {
		physics->syncedWorld = nullptr;
		physics->syncedWorldEditVersion = 0;
		ResetWalkCharacter(physics);
		return false;
	}

	physics->syncedWorld = world;
	physics->syncedWorldEditVersion = world->editVersion;
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

	const Float3 moveDirection = ComputeWalkMoveDirection(*camera, *input);
	float moveSpeed = kWalkMoveSpeed;
	if (IsInputActionDown(*input, InputAction::SpeedBoost)) {
		moveSpeed *= kWalkBoostMultiplier;
	}
	if (IsInputActionDown(*input, InputAction::SpeedSlow)) {
		moveSpeed *= kWalkSlowMultiplier;
	}

	JPH::CharacterVirtual *character = physics->walkCharacter.GetPtr();
	JPH::Vec3 newVelocity = character->GetLinearVelocity();
	if (character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround &&
		newVelocity.GetY() < 0.1f) {
		newVelocity = JPH::Vec3::sZero();
	} else {
		newVelocity = newVelocity * JPH::Vec3(0.0f, 1.0f, 0.0f);
	}

	newVelocity += moveSpeed * JPH::Vec3(moveDirection.x, 0.0f, moveDirection.z);
	if (ConsumeInputActionPressed(*input, InputAction::MoveUp) &&
		character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround) {
		newVelocity.SetY(kWalkJumpSpeed);
	}

	const JPH::Vec3 gravity = physics->physicsSystem.GetGravity();
	newVelocity += gravity * deltaSeconds;
	character->SetLinearVelocity(newVelocity);
	character->SetRotation(JPH::Quat::sRotation(JPH::Vec3::sAxisY(), camera->yawRadians));

	const JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
	character->ExtendedUpdate(
		deltaSeconds,
		gravity,
		updateSettings,
		physics->physicsSystem.GetDefaultBroadPhaseLayerFilter(PhysicsLayers::Moving),
		physics->physicsSystem.GetDefaultLayerFilter(PhysicsLayers::Moving),
		{},
		{},
		physics->tempAllocator);

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
	character->SetLinearVelocity(moveSpeed * JPH::Vec3(x, y, z));
	character->SetRotation(JPH::Quat::sRotation(JPH::Vec3::sAxisY(), camera->yawRadians));

	const JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
	character->ExtendedUpdate(
		deltaSeconds,
		JPH::Vec3::sZero(),
		updateSettings,
		physics->physicsSystem.GetDefaultBroadPhaseLayerFilter(PhysicsLayers::Moving),
		physics->physicsSystem.GetDefaultLayerFilter(PhysicsLayers::Moving),
		{},
		{},
		physics->tempAllocator);

	UpdateCameraFromWalkCharacter(*physics, *camera);
	return true;
}

uint64_t GetPhysicsWorldSyncVersion(const PhysicsState *physics)
{
	return physics != nullptr ? physics->syncedWorldEditVersion : 0;
}

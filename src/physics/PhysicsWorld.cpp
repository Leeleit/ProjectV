#include "physics/PhysicsWorld_Internal.hpp"

namespace {

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

uint64_t GetPhysicsWorldSyncVersion(const PhysicsState *physics)
{
	return physics != nullptr ? physics->syncedWorldEditVersion : 0;
}

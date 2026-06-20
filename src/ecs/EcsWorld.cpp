import projectv.math;
import projectv.string_id;

#include "ecs/EcsWorld.hpp"

#include "core/Types.hpp"
#include "voxel/VoxelWorld.hpp"

#include <string>
#include <vector>

#include <flecs.h>

namespace {
struct PlayerTag {
};

struct CameraTag {
};

struct WorldBinding {
	WorldState *state = nullptr;
};

struct PlayerControlledCamera {
	uint64_t cameraEntity = 0;
};

struct ChunkState {
	bool rebuildQueued = false;
	uint32_t nonAirVoxelCount = 0;
};

struct WorldChunkSummary {
	VoxelWorldStats stats{};
	size_t chunkEntityCount = 0;
};

struct EcsStateImpl {
	EcsStateImpl() = default;

	flecs::world world;
	uint64_t primaryCameraEntity = 0;
	uint64_t primaryPlayerEntity = 0;
	std::vector<uint64_t> chunkEntities;
};

flecs::entity GetEntity(flecs::world &world, const uint64_t entityId)
{
	if (entityId == 0) {
		return {};
	}

	return world.entity(static_cast<flecs::entity_t>(entityId));
}

flecs::entity GetEntity(const flecs::world &world, const uint64_t entityId)
{
	if (entityId == 0) {
		return {};
	}

	return world.entity(static_cast<flecs::entity_t>(entityId));
}

WorldBinding *GetWorldBinding(EcsStateImpl &ecs)
{
	return ecs.world.try_get_mut<WorldBinding>();
}

const WorldBinding *GetWorldBinding(const EcsStateImpl &ecs)
{
	return ecs.world.try_get<WorldBinding>();
}

WorldChunkSummary *GetWorldChunkSummary(EcsStateImpl &ecs)
{
	return ecs.world.try_get_mut<WorldChunkSummary>();
}

const WorldChunkSummary *GetWorldChunkSummary(const EcsStateImpl &ecs)
{
	return ecs.world.try_get<WorldChunkSummary>();
}

bool EnsureChunkEntities(EcsStateImpl &ecs, const size_t chunkCount)
{
	if (ecs.chunkEntities.size() > chunkCount) {
		while (ecs.chunkEntities.size() > chunkCount) {
			GetEntity(ecs.world, ecs.chunkEntities.back()).destruct();
			ecs.chunkEntities.pop_back();
		}
	}

	while (ecs.chunkEntities.size() < chunkCount) {
		const size_t chunkIndex = ecs.chunkEntities.size();
		const std::string chunkName = std::string("VoxelChunk.") + std::to_string(chunkIndex);
		const auto entity = ecs.world.entity(chunkName.c_str()).add<ChunkState>();
		ecs.chunkEntities.push_back(entity.id());
	}

	return ecs.chunkEntities.size() == chunkCount;
}

void ResetWorldChunkSummary(EcsStateImpl &ecs)
{
	if (WorldChunkSummary *summary = GetWorldChunkSummary(ecs)) {
		*summary = {};
	}

	if (DebugState *debug = ecs.world.try_get_mut<DebugState>()) {
		debug->stats.dirtyChunkCount = 0;
		debug->stats.activeChunkCount = 0;
		debug->stats.nonAirVoxelCount = 0;
	}
}
} // namespace

struct EcsState {
	EcsStateImpl impl;
};

void DestroyEcsState(EcsState *ecs)
{
	delete ecs;
}

bool InitializeAppEcs(AppState *state)
{
	if (!state) {
		return false;
	}

	state->ecs.reset(new EcsState{});
	EcsStateImpl &ecs = state->ecs->impl;

	ecs.world.set<WorldBinding>({&state->world});
	ecs.world.set<WorldChunkSummary>({});
	ecs.world.set<DebugState>({});

	const auto cameraEntity = ecs.world.entity("Camera.Primary").add<CameraTag>().set<CameraState>({});
	ecs.primaryCameraEntity = cameraEntity.id();

	const auto playerEntity = ecs.world.entity("Player.Primary")
								  .add<PlayerTag>()
								  .set<PlayerControlledCamera>({ecs.primaryCameraEntity});
	ecs.primaryPlayerEntity = playerEntity.id();
	return true;
}

CameraState *GetPrimaryCameraState(EcsState *ecs)
{
	if (!ecs) {
		return nullptr;
	}

	EcsStateImpl &impl = ecs->impl;
	const flecs::entity playerEntity = GetEntity(impl.world, impl.primaryPlayerEntity);
	const PlayerControlledCamera *playerCamera = playerEntity.try_get<PlayerControlledCamera>();
	if (!playerCamera || playerCamera->cameraEntity == 0) {
		return nullptr;
	}

	return GetEntity(impl.world, playerCamera->cameraEntity).try_get_mut<CameraState>();
}

const CameraState *GetPrimaryCameraState(const EcsState *ecs)
{
	if (!ecs) {
		return nullptr;
	}

	const EcsStateImpl &impl = ecs->impl;
	const flecs::entity playerEntity = GetEntity(impl.world, impl.primaryPlayerEntity);
	const PlayerControlledCamera *playerCamera = playerEntity.try_get<PlayerControlledCamera>();
	if (!playerCamera || playerCamera->cameraEntity == 0) {
		return nullptr;
	}

	return GetEntity(impl.world, playerCamera->cameraEntity).try_get<CameraState>();
}

DebugState *GetDebugState(EcsState *ecs)
{
	if (!ecs) {
		return nullptr;
	}

	return ecs->impl.world.try_get_mut<DebugState>();
}

const DebugState *GetDebugState(const EcsState *ecs)
{
	if (!ecs) {
		return nullptr;
	}

	return ecs->impl.world.try_get<DebugState>();
}

WorldState *GetWorldState(EcsState *ecs)
{
	if (!ecs) {
		return nullptr;
	}

	const WorldBinding *binding = GetWorldBinding(ecs->impl);
	return binding ? binding->state : nullptr;
}

const WorldState *GetWorldState(const EcsState *ecs)
{
	if (!ecs) {
		return nullptr;
	}

	const WorldBinding *binding = GetWorldBinding(ecs->impl);
	return binding ? binding->state : nullptr;
}

bool SyncEcsWorldState(EcsState *ecs)
{
	if (!ecs) {
		return false;
	}

	EcsStateImpl &impl = ecs->impl;
	const WorldBinding *binding = GetWorldBinding(impl);
	if (!binding) {
		return false;
	}

	if (!binding->state || !binding->state->voxelWorld) {
		if (!EnsureChunkEntities(impl, 0)) {
			return false;
		}
		ResetWorldChunkSummary(impl);
		return true;
	}

	const VoxelWorld &voxelWorld = *binding->state->voxelWorld;
	if (!EnsureChunkEntities(impl, voxelWorld.chunks.size())) {
		return false;
	}

	for (size_t chunkIndex = 0; chunkIndex < voxelWorld.chunks.size(); ++chunkIndex) {
		const VoxelChunk &chunk = voxelWorld.chunks[chunkIndex];
		GetEntity(impl.world, impl.chunkEntities[chunkIndex]).set<ChunkState>({
			chunk.rebuildQueued,
			chunk.nonAirVoxelCount,
		});
	}

	WorldChunkSummary *summary = GetWorldChunkSummary(impl);
	if (!summary) {
		return false;
	}

	summary->stats = voxelWorld.stats;
	summary->stats.dirtyChunkCount = 0;
	summary->stats.activeChunkCount = 0;
	summary->stats.nonAirVoxelCount = 0;
	summary->chunkEntityCount = impl.chunkEntities.size();

	impl.world.each<const ChunkState>([&](const ChunkState &chunk) {
		if (chunk.rebuildQueued) {
			++summary->stats.dirtyChunkCount;
		}
		if (chunk.nonAirVoxelCount > 0) {
			++summary->stats.activeChunkCount;
		}
		summary->stats.nonAirVoxelCount += chunk.nonAirVoxelCount;
	});

	if (DebugState *debug = impl.world.try_get_mut<DebugState>()) {
		debug->stats.dirtyChunkCount = summary->stats.dirtyChunkCount;
		debug->stats.activeChunkCount = summary->stats.activeChunkCount;
		debug->stats.nonAirVoxelCount = summary->stats.nonAirVoxelCount;
	}

	return true;
}

uint64_t GetPrimaryCameraEntityId(const EcsState *ecs)
{
	if (!ecs) {
		return 0;
	}

	return ecs->impl.primaryCameraEntity;
}

uint64_t GetPrimaryPlayerEntityId(const EcsState *ecs)
{
	if (!ecs) {
		return 0;
	}

	return ecs->impl.primaryPlayerEntity;
}

uint64_t GetPlayerControlledCameraEntityId(const EcsState *ecs)
{
	if (!ecs) {
		return 0;
	}

	const EcsStateImpl &impl = ecs->impl;
	const flecs::entity playerEntity = GetEntity(impl.world, impl.primaryPlayerEntity);
	const PlayerControlledCamera *playerCamera = playerEntity.try_get<PlayerControlledCamera>();
	return playerCamera ? playerCamera->cameraEntity : 0;
}

bool GetEcsWorldChunkSummary(
	const EcsState *ecs,
	VoxelWorldStats *outStats,
	size_t *outChunkEntityCount)
{
	if (!ecs) {
		return false;
	}

	const WorldChunkSummary *summary = GetWorldChunkSummary(ecs->impl);
	if (!summary) {
		return false;
	}

	if (outStats) {
		*outStats = summary->stats;
	}
	if (outChunkEntityCount) {
		*outChunkEntityCount = summary->chunkEntityCount;
	}
	return true;
}

import projectv.math; // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
import projectv.string_id;

#include "ecs/EcsWorld.hpp"

#include "audio/AudioEngine.hpp"
#include "app/BenchmarkAutomation.hpp"
#include "app/LookDevCaptureAutomation.hpp"
#include "core/Types.hpp"
#include "voxel/VoxelWorld.hpp"

#include <SDL3/SDL.h>


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

struct AudioPlaylistRefreshRequest {
	bool requested = false;
};

struct FluidCATickState {
	float accumulatorSeconds = 0.0f;
};


struct BenchmarkTickResult {
	bool quitAfterFrame = false;
};

struct LookDevCaptureTickResult {
	bool quitAfterFrame = false;
};

struct AppStateBinding {
	AppState *state = nullptr;
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

	state->ecs().reset(new EcsState{});
	EcsStateImpl &ecs = state->ecs()->impl;

	ecs.world.set<WorldBinding>({&state->world()});
	ecs.world.set<WorldChunkSummary>({});
	ecs.world.set<DebugState>({});
	ecs.world.set<AudioPlaylistRefreshRequest>({});
	ecs.world.set<FluidCATickState>({});
	ecs.world.set<BenchmarkTickResult>({});
	ecs.world.set<LookDevCaptureTickResult>({});
	ecs.world.set<AppStateBinding>({state});

	const auto cameraEntity = ecs.world.entity("Camera.Primary").add<CameraTag>().set<CameraState>({});
	ecs.primaryCameraEntity = cameraEntity.id();

	const auto playerEntity = ecs.world.entity("Player.Primary")
								  .add<PlayerTag>()
								  .set<PlayerControlledCamera>({ecs.primaryCameraEntity});
	ecs.primaryPlayerEntity = playerEntity.id();

	ecs.world.system<AudioPlaylistRefreshRequest>("AudioRefreshPlaylistSystem")
		.kind(flecs::OnUpdate)
		.each([](flecs::entity e, AudioPlaylistRefreshRequest &req) {
			if (req.requested) {
				if (const AppStateBinding *binding = e.world().try_get_mut<AppStateBinding>()) {
					if (auto *audio = binding->state->audio().get()) {
						audio->RefreshPlaylistAsync();
					}
				}
				req.requested = false;
			}
		});

	ecs.world.system<FluidCATickState>("FluidCATickSystem")
		.kind(flecs::OnUpdate)
		.each([](flecs::entity e, FluidCATickState &tickState) {
			const AppStateBinding *binding = e.world().try_get_mut<AppStateBinding>();
			if (!binding || !binding->state) {
				return;
			}
			SimulationState &simulation = binding->state->simulation();
			if (simulation.effectivePaused || simulation.fluidTickRateHz <= 0.0f) {
				tickState.accumulatorSeconds = 0.0f;
				simulation.fluidAccumulatorSeconds = 0.0f;
				return;
			}
			VoxelWorld *voxelWorld = binding->state->world().voxelWorld.get();
			if (!voxelWorld) {
				tickState.accumulatorSeconds = 0.0f;
				simulation.fluidAccumulatorSeconds = 0.0f;
				return;
			}
			tickState.accumulatorSeconds += simulation.frameDeltaSeconds;
			const float fluidInterval = 1.0f / simulation.fluidTickRateHz;
			const bool gpuEnabled = IsFluidCaGpuEnabled();
			while (tickState.accumulatorSeconds >= fluidInterval) {
				tickState.accumulatorSeconds -= fluidInterval;
				if (gpuEnabled) {
					++simulation.fluidGpuTicksPending;
				} else {
					UpdateFluidCA(*voxelWorld);
				}
			}
			simulation.fluidAccumulatorSeconds = tickState.accumulatorSeconds;
		});

	ecs.world.system<BenchmarkTickResult>("BenchmarkAutomationTickSystem")
		.kind(flecs::OnUpdate)
		.each([](flecs::entity e, BenchmarkTickResult &result) {
			const AppStateBinding *binding = e.world().try_get_mut<AppStateBinding>();
			if (!binding || !binding->state) {
				result.quitAfterFrame = false;
				return;
			}
			const DebugState *debug = GetDebugState(binding->state->ecs().get());
			const DebugStats debugStats = debug ? debug->stats : DebugStats{};
			const Uint64 frameCounter = SDL_GetPerformanceCounter();
			result.quitAfterFrame = UpdateBenchmarkAutomation(
				&binding->state->benchmark(),
				debugStats,
				frameCounter);
		});

	ecs.world.system<LookDevCaptureTickResult>("LookDevCaptureTickSystem")
		.kind(flecs::OnUpdate)
		.each([](flecs::entity e, LookDevCaptureTickResult &result) {
			const AppStateBinding *binding = e.world().try_get_mut<AppStateBinding>();
			if (!binding || !binding->state) {
				result.quitAfterFrame = false;
				return;
			}
			result.quitAfterFrame = UpdateLookDevCaptureAutomation(
				&binding->state->lookDevCapture(),
				&binding->state->render());
		});
	return true;
}

void RequestAudioPlaylistRefresh(EcsState *ecs)
{
	if (!ecs) {
		return;
	}
	if (auto *req = ecs->impl.world.try_get_mut<AudioPlaylistRefreshRequest>()) {
		req->requested = true;
	}
}

void TickAudioRefreshPlaylistSystem(EcsState *ecs)
{
	if (!ecs) {
		return;
	}
	(void)ecs->impl.world.progress();
}

void TickFluidCASystem(EcsState *ecs)
{
	if (!ecs) {
		return;
	}
	(void)ecs->impl.world.progress();
}


void TickBenchmarkAutomationSystem(EcsState *ecs)
{
	if (!ecs) {
		return;
	}
	(void)ecs->impl.world.progress();
}

bool IsBenchmarkAutomationQuitRequested(const EcsState *ecs)
{
	if (!ecs) {
		return false;
	}
	if (const auto *result = ecs->impl.world.try_get<BenchmarkTickResult>()) {
		return result->quitAfterFrame;
	}
	return false;
}

void TickLookDevCaptureSystem(EcsState *ecs)
{
	if (!ecs) {
		return;
	}
	(void)ecs->impl.world.progress();
}

bool IsLookDevCaptureQuitRequested(const EcsState *ecs)
{
	if (!ecs) {
		return false;
	}
	if (const auto *result = ecs->impl.world.try_get<LookDevCaptureTickResult>()) {
		return result->quitAfterFrame;
	}
	return false;
}

bool IsAudioPlaylistRefreshRequested(const EcsState *ecs)
{
	if (!ecs) {
		return false;
	}
	if (const auto *req = ecs->impl.world.try_get<AudioPlaylistRefreshRequest>()) {
		return req->requested;
	}
	return false;
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

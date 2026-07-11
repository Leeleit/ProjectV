module; // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include <cstddef>
#include <cstdint>

export module projectv.ecs;

export import projectv.types;

export namespace projectv::ecs {

bool InitializeAppEcs(core::AppState *state);

core::CameraState *GetPrimaryCameraState(core::EcsState *ecs);
const core::CameraState *GetPrimaryCameraState(const core::EcsState *ecs);

core::DebugState *GetDebugState(core::EcsState *ecs);
const core::DebugState *GetDebugState(const core::EcsState *ecs);

core::WorldState *GetWorldState(core::EcsState *ecs);
const core::WorldState *GetWorldState(const core::EcsState *ecs);

bool SyncEcsWorldState(core::EcsState *ecs);

std::uint64_t GetPrimaryCameraEntityId(const core::EcsState *ecs);
std::uint64_t GetPrimaryPlayerEntityId(const core::EcsState *ecs);
std::uint64_t GetPlayerControlledCameraEntityId(const core::EcsState *ecs);

bool GetEcsWorldChunkSummary(
	const core::EcsState *ecs,
	core::VoxelWorldStats *outStats,
	std::size_t *outChunkEntityCount);

} // namespace projectv::ecs
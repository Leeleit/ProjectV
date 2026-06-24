module; // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include <cstddef>
#include <cstdint>

export module projectv.ecs;

export import projectv.types;

export namespace projectv::ecs {

bool InitializeAppEcs(::projectv::core::AppState *state);

::projectv::core::CameraState *GetPrimaryCameraState(::projectv::core::EcsState *ecs);
const ::projectv::core::CameraState *GetPrimaryCameraState(const ::projectv::core::EcsState *ecs);

::projectv::core::DebugState *GetDebugState(::projectv::core::EcsState *ecs);
const ::projectv::core::DebugState *GetDebugState(const ::projectv::core::EcsState *ecs);

::projectv::core::WorldState *GetWorldState(::projectv::core::EcsState *ecs);
const ::projectv::core::WorldState *GetWorldState(const ::projectv::core::EcsState *ecs);

bool SyncEcsWorldState(::projectv::core::EcsState *ecs);

std::uint64_t GetPrimaryCameraEntityId(const ::projectv::core::EcsState *ecs);
std::uint64_t GetPrimaryPlayerEntityId(const ::projectv::core::EcsState *ecs);
std::uint64_t GetPlayerControlledCameraEntityId(const ::projectv::core::EcsState *ecs);

bool GetEcsWorldChunkSummary(
	const ::projectv::core::EcsState *ecs,
	::projectv::core::VoxelWorldStats *outStats,
	std::size_t *outChunkEntityCount);

} // namespace projectv::ecs
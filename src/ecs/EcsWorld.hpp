#ifndef ECS_WORLD_HPP
#define ECS_WORLD_HPP

#include <cstddef>
#include <cstdint>

struct AppState;
struct CameraState;
struct DebugState;
struct EcsState;
struct VoxelWorldStats;
struct WorldState;

bool InitializeAppEcs(AppState *state);
CameraState *GetPrimaryCameraState(EcsState *ecs);
const CameraState *GetPrimaryCameraState(const EcsState *ecs);
DebugState *GetDebugState(EcsState *ecs);
const DebugState *GetDebugState(const EcsState *ecs);
WorldState *GetWorldState(EcsState *ecs);
const WorldState *GetWorldState(const EcsState *ecs);
bool SyncEcsWorldState(EcsState *ecs);
uint64_t GetPrimaryCameraEntityId(const EcsState *ecs);
uint64_t GetPrimaryPlayerEntityId(const EcsState *ecs);
uint64_t GetPlayerControlledCameraEntityId(const EcsState *ecs);
bool GetEcsWorldChunkSummary(
	const EcsState *ecs,
	VoxelWorldStats *outStats,
	size_t *outChunkEntityCount);

#endif

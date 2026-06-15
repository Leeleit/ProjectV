#pragma once

// noinspection CppUnusedIncludeDirective
// `<cstddef>` provides `size_t` for `GetEcsWorldChunkSummary`'s
// `size_t *outChunkEntityCount` out-parameter.
#include <cstddef>
// noinspection CppUnusedIncludeDirective
// `<cstdint>` provides `uint64_t` for the three `Get*EntityId`
// helpers. JetBrains' indexer doesn't see the use through the
// forward-declared pointer / return-type signatures.
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


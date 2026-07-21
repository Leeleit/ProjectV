#pragma once

// ReSharper disable once CppUnusedIncludeDirective
#include <cstddef>
// ReSharper disable once CppUnusedIncludeDirective
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

void RequestAudioPlaylistRefresh(EcsState *ecs);
void TickAudioRefreshPlaylistSystem(EcsState *ecs);
bool IsAudioPlaylistRefreshRequested(const EcsState *ecs);

void TickBenchmarkAutomationSystem(EcsState *ecs);
bool IsBenchmarkAutomationQuitRequested(const EcsState *ecs);

void TickLookDevCaptureSystem(EcsState *ecs);
bool IsLookDevCaptureQuitRequested(const EcsState *ecs);

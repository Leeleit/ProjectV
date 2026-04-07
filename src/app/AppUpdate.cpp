#include "app/AppUpdate.hpp"

#include "app/Camera.hpp"
#include "app/InputActions.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"
#include "physics/PhysicsWorld.hpp"
#include "voxel/VoxelInteraction.hpp"

#include <cmath>

namespace {
constexpr uint32_t kMaxSimulationStepsPerFrame = 5;
constexpr float kMaxFrameDeltaSeconds = 0.25f;

bool IsCreativeMode(const CameraState &camera)
{
	return camera.controlMode == CameraState::ControlMode::Creative;
}

bool IsSpectatorMode(const CameraState &camera)
{
	return camera.controlMode == CameraState::ControlMode::Spectator;
}

bool IsWalkMode(const CameraState &camera)
{
	return camera.controlMode == CameraState::ControlMode::Walk;
}

bool UsesPhysicsCharacter(const CameraState::ControlMode controlMode)
{
	return controlMode == CameraState::ControlMode::Creative ||
		   controlMode == CameraState::ControlMode::Walk;
}

CameraState::ControlMode GetNextWalkCreativeMode(const CameraState::ControlMode controlMode)
{
	switch (controlMode) {
	case CameraState::ControlMode::Walk:
		return CameraState::ControlMode::Creative;
	case CameraState::ControlMode::Creative:
		return CameraState::ControlMode::Walk;
	case CameraState::ControlMode::Spectator:
		return CameraState::ControlMode::Spectator;
	}

	return CameraState::ControlMode::Creative;
}

CameraState::ControlMode GetNextControlMode(const CameraState::ControlMode controlMode)
{
	switch (controlMode) {
	case CameraState::ControlMode::Creative:
		return CameraState::ControlMode::Spectator;
	case CameraState::ControlMode::Spectator:
		return CameraState::ControlMode::Walk;
	case CameraState::ControlMode::Walk:
		return CameraState::ControlMode::Creative;
	}

	return CameraState::ControlMode::Creative;
}

DebugEditorTool GetNextDebugEditorTool(const DebugEditorTool tool)
{
	switch (tool) {
	case DebugEditorTool::Classic:
		return DebugEditorTool::Paint;
	case DebugEditorTool::Paint:
		return DebugEditorTool::Erase;
	case DebugEditorTool::Erase:
		return DebugEditorTool::Fill;
	case DebugEditorTool::Fill:
		return DebugEditorTool::Inspect;
	case DebugEditorTool::Inspect:
		return DebugEditorTool::Classic;
	}

	return DebugEditorTool::Classic;
}

float ComputeFrameDeltaSeconds(SimulationState &simulation)
{
	const Uint64 now = SDL_GetPerformanceCounter();
	if (simulation.lastFrameCounter == 0) {
		simulation.lastFrameCounter = now;
		return 0.0f;
	}

	const Uint64 frequency = SDL_GetPerformanceFrequency();
	const Uint64 deltaCounter = now - simulation.lastFrameCounter;
	simulation.lastFrameCounter = now;
	return std::min(
		static_cast<float>(deltaCounter) / static_cast<float>(frequency),
		kMaxFrameDeltaSeconds);
}

bool SetRelativeMouseMode(
	PlatformState &platform,
	InputState &input,
	const bool enableRelativeMouseMode)
{
	if (!platform.window) {
		return false;
	}

	if (!SDL_SetWindowRelativeMouseMode(platform.window, enableRelativeMouseMode)) {
		runtime::LogSdlFailure("UpdateApp.ToggleRelativeMouseMode");
		return false;
	}

	input.relativeMouseModeEnabled = enableRelativeMouseMode;
	input.mouseDeltaX = 0.0f;
	input.mouseDeltaY = 0.0f;
	return true;
}

void ClearInteractionClickActions(InputState &input)
{
	input.removePressed = false;
	input.placePressed = false;
}

bool ApplyControlModeTransition(
	CameraState *camera,
	InputState *input,
	WorldState *world,
	PhysicsState *physics,
	const CameraState::ControlMode nextMode,
	const char *logStep)
{
	PV_CHECK_OR_RETURN(
		camera && input,
		"App",
		logStep,
		"camera/input is null");

	const CameraState::ControlMode previousMode = camera->controlMode;
	if (previousMode == nextMode) {
		return true;
	}

	camera->controlMode = nextMode;
	ClearInteractionClickActions(*input);
	if (UsesPhysicsCharacter(previousMode)) {
		ResetWalkCharacter(physics);
	}

	if (!UsesPhysicsCharacter(camera->controlMode)) {
		return true;
	}

	PV_CHECK_OR_RETURN(
		physics && world && world->voxelWorld,
		"App",
		logStep,
		"creative/walk mode requires initialized physics and voxel world");
	if (camera->controlMode == CameraState::ControlMode::Walk) {
		ConsumeInputActionPressed(*input, InputAction::MoveUp);
	}
	if (!SyncPhysicsWorld(physics, world->voxelWorld.get())) {
		runtime::LogRuntimeFailure(
			"App",
			logStep,
			"failed to sync physics world for creative/walk transition");
		return false;
	}
	const bool snapped =
		camera->controlMode == CameraState::ControlMode::Walk
			? SnapWalkCharacterToCamera(physics, world->voxelWorld.get(), camera)
			: SnapCreativeCharacterToCamera(physics, world->voxelWorld.get(), camera);
	if (!snapped) {
		runtime::LogRuntimeFailure(
			"App",
			logStep,
			"failed to initialize creative/walk physics state");
		return false;
	}

	return true;
}
} // namespace

bool UpdateApp(
	PlatformState *platform,
	SimulationState *simulation,
	CameraState *camera,
	InputState *input,
	InteractionState *interaction,
	WorldState *world,
	PhysicsState *physics,
	RenderState *render,
	DebugState *debug)
{
	PV_PROFILE_ZONE_N("UpdateApp");
	if (!platform || !simulation || !camera || !input || !interaction || !world || !render || !debug) {
		return false;
	}

	simulation->frameDeltaSeconds = ComputeFrameDeltaSeconds(*simulation);
	simulation->simulationStepsLastFrame = 0;
	debug->stats.framesPerSecond = simulation->frameDeltaSeconds > 0.0f ? 1.0f / simulation->frameDeltaSeconds : 0.0f;
	debug->stats.frameTimeMilliseconds = simulation->frameDeltaSeconds * 1000.0f;
	debug->stats.simulationStepsLastFrame = 0;
	debug->stats.sceneTriangleCount = render->sceneTriangleCount;

	if (ConsumeInputActionPressed(*input, InputAction::ToggleHud)) {
		debug->hudVisible = !debug->hudVisible;
	}
	if (ConsumeInputActionPressed(*input, InputAction::ToggleRelativeMouseMode)) {
		SetRelativeMouseMode(*platform, *input, !input->relativeMouseModeEnabled);
	}
	if (ConsumeInputActionPressed(*input, InputAction::CyclePlacementMaterial)) {
		interaction->placementMaterial = GetNextPlacementMaterial(interaction->placementMaterial);
	}
	if (ConsumeInputActionPressed(*input, InputAction::ResetCamera)) {
		ResetCameraState(camera);
		input->mouseDeltaX = 0.0f;
		input->mouseDeltaY = 0.0f;
	}
	if (ConsumeInputActionPressed(*input, InputAction::TogglePause)) {
		simulation->paused = !simulation->paused;
		simulation->simulationAccumulatorSeconds = 0.0f;
	}
	if (ConsumeInputActionPressed(*input, InputAction::ToggleWalkCreativeMode)) {
		if (!ApplyControlModeTransition(
				camera,
				input,
				world,
				physics,
				GetNextWalkCreativeMode(camera->controlMode),
				"UpdateApp.ToggleWalkCreativeMode")) {
			return false;
		}
	}
	if (ConsumeInputActionPressed(*input, InputAction::ToggleControlMode)) {
		if (!ApplyControlModeTransition(
				camera,
				input,
				world,
				physics,
				GetNextControlMode(camera->controlMode),
				"UpdateApp.ToggleControlMode")) {
			return false;
		}
	}
	if (ConsumeInputActionPressed(*input, InputAction::CycleScenePreset) &&
		world->voxelWorld) {
		world->requestedScenePreset = GetNextVoxelScenePreset(world->voxelWorld->scenePreset);
		world->scenePresetReloadRequested = true;
		ClearInteractionClickActions(*input);
	}
	if (ConsumeInputActionPressed(*input, InputAction::SaveWorldSnapshot) &&
		world->voxelWorld &&
		!world->scenePresetReloadRequested &&
		!world->snapshotLoadRequested) {
		world->snapshotSaveRequested = true;
	}
	if (ConsumeInputActionPressed(*input, InputAction::LoadWorldSnapshot) &&
		world->voxelWorld &&
		!world->scenePresetReloadRequested) {
		world->snapshotLoadRequested = true;
		ClearInteractionClickActions(*input);
	}
	if (ConsumeInputActionPressed(*input, InputAction::CycleEditorTool)) {
		interaction->editorTool = GetNextDebugEditorTool(interaction->editorTool);
		ClearInteractionClickActions(*input);
	}
	if (ConsumeInputActionPressed(*input, InputAction::ToggleChunkBounds)) {
		debug->showChunkBounds = !debug->showChunkBounds;
	}
	if (ConsumeInputActionPressed(*input, InputAction::ToggleDirtyChunkOverlay)) {
		debug->showDirtyChunkOverlay = !debug->showDirtyChunkOverlay;
	}

	const bool creativeMode = IsCreativeMode(*camera);
	const bool spectatorMode = IsSpectatorMode(*camera);
	const bool walkMode = IsWalkMode(*camera);
	const bool cameraCanUpdate = spectatorMode || !simulation->paused;
	const bool allowWorldEditing =
		(creativeMode || walkMode) &&
		!world->scenePresetReloadRequested &&
		!world->snapshotLoadRequested;

	if (physics && world->voxelWorld && !SyncPhysicsWorld(physics, world->voxelWorld.get())) {
		runtime::LogRuntimeFailure(
			"App",
			"UpdateApp.SyncPhysicsWorld",
			"SyncPhysicsWorld returned false before simulation tick");
		return false;
	}

	if (!simulation->paused) {
		simulation->simulationAccumulatorSeconds += simulation->frameDeltaSeconds;
	} else {
		simulation->simulationAccumulatorSeconds = 0.0f;
	}

	if (cameraCanUpdate) {
		ConsumeCameraLookInput(camera, input);
	} else {
		input->mouseDeltaX = 0.0f;
		input->mouseDeltaY = 0.0f;
	}

	while (simulation->simulationAccumulatorSeconds >= simulation->fixedSimulationDeltaSeconds &&
		   simulation->simulationStepsLastFrame < kMaxSimulationStepsPerFrame &&
		   !simulation->paused) {
		if (walkMode) {
			PV_CHECK_OR_RETURN(
				physics && world->voxelWorld,
				"App",
				"UpdateApp.TickWalkCharacter",
				"walk mode requires initialized physics and voxel world");
			if (!TickWalkCharacter(
					physics,
					world->voxelWorld.get(),
					camera,
					input,
					simulation->fixedSimulationDeltaSeconds)) {
				runtime::LogRuntimeFailure(
					"App",
					"UpdateApp.TickWalkCharacter",
					"TickWalkCharacter returned false");
				return false;
			}
		} else if (creativeMode) {
			PV_CHECK_OR_RETURN(
				physics && world->voxelWorld,
				"App",
				"UpdateApp.TickCreativeCharacter",
				"creative mode requires initialized physics and voxel world");
			if (!TickCreativeCharacter(
					physics,
					world->voxelWorld.get(),
					camera,
					input,
					simulation->fixedSimulationDeltaSeconds)) {
				runtime::LogRuntimeFailure(
					"App",
					"UpdateApp.TickCreativeCharacter",
					"TickCreativeCharacter returned false");
				return false;
			}
		} else {
			TickCamera(camera, *input, simulation->fixedSimulationDeltaSeconds);
		}
		simulation->simulationAccumulatorSeconds -= simulation->fixedSimulationDeltaSeconds;
		++simulation->simulationStepsLastFrame;
		++simulation->simulationTick;
	}

	if (simulation->simulationAccumulatorSeconds >= simulation->fixedSimulationDeltaSeconds) {
		simulation->simulationAccumulatorSeconds =
			std::fmod(simulation->simulationAccumulatorSeconds, simulation->fixedSimulationDeltaSeconds);
	}

	if (simulation->paused && spectatorMode && simulation->frameDeltaSeconds > 0.0f) {
		TickCamera(camera, *input, simulation->frameDeltaSeconds);
	}

	const uint64_t worldEditVersionBeforeInteraction =
		world->voxelWorld ? world->voxelWorld->editVersion : 0;
	UpdateVoxelInteraction(*camera, input, world->voxelWorld.get(), interaction, allowWorldEditing);
	if (physics &&
		world->voxelWorld &&
		world->voxelWorld->editVersion != worldEditVersionBeforeInteraction &&
		!SyncPhysicsWorld(physics, world->voxelWorld.get())) {
		runtime::LogRuntimeFailure(
			"App",
			"UpdateApp.PostInteractionPhysicsSync",
			"SyncPhysicsWorld returned false after voxel edit");
		return false;
	}

	profiling::PlotValue("Frame Delta (ms)", simulation->frameDeltaSeconds * 1000.0f);
	profiling::PlotValue(
		"Simulation Accumulator (ms)",
		simulation->simulationAccumulatorSeconds * 1000.0f);
	profiling::PlotValue("Simulation Steps", static_cast<int64_t>(simulation->simulationStepsLastFrame));

	if (world->voxelWorld) {
		debug->stats.simulationStepsLastFrame = simulation->simulationStepsLastFrame;
		debug->stats.dirtyChunkCount = world->voxelWorld->stats.dirtyChunkCount;
		debug->stats.activeChunkCount = world->voxelWorld->stats.activeChunkCount;
		debug->stats.glassVoxelCount = world->voxelWorld->stats.glassVoxelCount;
		debug->stats.fluidVoxelCount = world->voxelWorld->stats.fluidVoxelCount;
		debug->stats.floorVoxelCount =
			world->voxelWorld->stats.floorWhiteVoxelCount +
			world->voxelWorld->stats.floorGrayVoxelCount;
		debug->stats.nonAirVoxelCount = world->voxelWorld->stats.nonAirVoxelCount;
		debug->stats.sceneTriangleCount = render->sceneTriangleCount;
		debug->stats.sceneMemoryBytes = render->sceneMemoryBytes;
		debug->stats.scenePreset = world->voxelWorld->scenePreset;
	}
	debug->stats.controlMode = camera->controlMode;
	debug->stats.simulationPaused = simulation->paused;
	debug->stats.showChunkBounds = debug->showChunkBounds;
	debug->stats.showDirtyChunkOverlay = debug->showDirtyChunkOverlay;

	return true;
}

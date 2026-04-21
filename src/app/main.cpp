#define SDL_MAIN_USE_CALLBACKS 1
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

#include "app/AppUpdate.hpp"
#include "app/Camera.hpp"
#include "app/FramePreparation.hpp"
#include "app/InputActions.hpp"
#include "app/InputReplay.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "core/Types.hpp"
#include "debug/Profiling.hpp"
#include "ecs/EcsWorld.hpp"
#include "physics/PhysicsWorld.hpp"
#include "platform/PlatformEvents.hpp"
#include "render/Renderer.hpp"
#include "render/SceneResources.hpp"
#include "render/vulkan/VulkanInit.hpp"
#include "voxel/VoxelInteraction.hpp"
#include "voxel/VoxelWorld.hpp"

#include <string>

namespace {
bool WaitForDeviceIdle(VulkanContextState &context)
{
	if (context.device == VK_NULL_HANDLE) {
		return true;
	}

	const VkResult waitResult = vkDeviceWaitIdle(context.device);
	if (waitResult != VK_SUCCESS) {
		runtime::LogVkFailure("SDL_AppIterate.SceneReload.vkDeviceWaitIdle", waitResult);
		return false;
	}

	return true;
}

bool FinalizeActiveVoxelWorldReload(AppState *state, const std::string_view operationStep)
{
	CameraState *camera = GetPrimaryCameraState(state->ecs.get());
	DebugState *debug = GetDebugState(state->ecs.get());
	WorldState *world = GetWorldState(state->ecs.get());
	PV_CHECK_OR_RETURN(
		camera && debug && world && world->voxelWorld,
		"App",
		operationStep,
		"primary camera, debug singleton or active voxel world is unavailable");

	ResetCameraState(camera);
	state->input.mouseDeltaX = 0.0f;
	state->input.mouseDeltaY = 0.0f;
	state->input.removePressed = false;
	state->input.placePressed = false;
	state->interaction.selection = {};
	state->frame.renderData.interactionSelection = {};
	state->simulation.simulationAccumulatorSeconds = 0.0f;
	state->simulation.simulationStepsLastFrame = 0;

	if (!SyncEcsWorldState(state->ecs.get())) {
		runtime::LogRuntimeFailure(
			"App",
			std::string(operationStep) + ".SyncEcsWorldState",
			"SyncEcsWorldState returned false after world reload");
		return false;
	}

	if (!CreateSceneResources(&state->context, world, &state->render)) {
		runtime::LogRuntimeFailure(
			"App",
			std::string(operationStep) + ".CreateSceneResources",
			"CreateSceneResources returned false");
		return false;
	}

	if (state->physics &&
		!SyncPhysicsWorld(state->physics.get(), world->voxelWorld.get())) {
		runtime::LogRuntimeFailure(
			"App",
			std::string(operationStep) + ".SyncPhysicsWorld",
			"SyncPhysicsWorld returned false after world reload");
		return false;
	}

	if (camera->controlMode == CameraState::ControlMode::Walk) {
		ConsumeInputActionPressed(state->input, InputAction::MoveUp);
		if (!SnapWalkCharacterToCamera(state->physics.get(), world->voxelWorld.get(), camera)) {
			runtime::LogRuntimeFailure(
				"App",
				std::string(operationStep) + ".SnapWalkCharacterToCamera",
				"SnapWalkCharacterToCamera returned false after world reload");
			return false;
		}
	} else if (camera->controlMode == CameraState::ControlMode::Creative) {
		if (!SnapCreativeCharacterToCamera(state->physics.get(), world->voxelWorld.get(), camera)) {
			runtime::LogRuntimeFailure(
				"App",
				std::string(operationStep) + ".SnapCreativeCharacterToCamera",
				"SnapCreativeCharacterToCamera returned false after world reload");
			return false;
		}
	} else {
		ResetWalkCharacter(state->physics.get());
	}

	debug->stats.scenePreset = world->voxelWorld ? world->voxelWorld->scenePreset : VoxelScenePreset::VoxelLab;
	world->scenePresetReloadRequested = false;
	world->snapshotSaveRequested = false;
	world->snapshotLoadRequested = false;
	return true;
}

bool ReloadActiveVoxelScene(AppState *state, const VoxelScenePreset preset)
{
	PV_PROFILE_ZONE_N("ReloadActiveVoxelScene");
	PV_CHECK_OR_RETURN(state != nullptr, "App", "ReloadActiveVoxelScene.Preconditions", "AppState is null");

	if (!WaitForDeviceIdle(state->context)) {
		return false;
	}

	if (!CreateVoxelSceneWorld(state, preset)) {
		runtime::LogRuntimeFailure(
			"App",
			"ReloadActiveVoxelScene.CreateVoxelSceneWorld",
			"CreateVoxelSceneWorld returned false");
		return false;
	}

	return FinalizeActiveVoxelWorldReload(state, "ReloadActiveVoxelScene");
}

bool SaveActiveVoxelWorldSnapshot(AppState *state)
{
	PV_PROFILE_ZONE_N("SaveActiveVoxelWorldSnapshot");
	PV_CHECK_OR_RETURN(state != nullptr, "App", "SaveActiveVoxelWorldSnapshot.Preconditions", "AppState is null");

	WorldState *world = GetWorldState(state->ecs.get());
	PV_CHECK_OR_RETURN(
		world && world->voxelWorld,
		"App",
		"SaveActiveVoxelWorldSnapshot.World",
		"active voxel world is unavailable");

	const std::string snapshotPath = GetVoxelWorldSnapshotPath();
	if (!SaveVoxelWorldSnapshot(*world->voxelWorld, snapshotPath)) {
		runtime::LogRuntimeFailure(
			"App",
			"SaveActiveVoxelWorldSnapshot.SaveVoxelWorldSnapshot",
			"SaveVoxelWorldSnapshot returned false");
		return false;
	}

	world->snapshotSaveRequested = false;
	return true;
}

bool LoadActiveVoxelWorldSnapshot(AppState *state)
{
	PV_PROFILE_ZONE_N("LoadActiveVoxelWorldSnapshot");
	PV_CHECK_OR_RETURN(state != nullptr, "App", "LoadActiveVoxelWorldSnapshot.Preconditions", "AppState is null");

	if (!WaitForDeviceIdle(state->context)) {
		return false;
	}

	const std::string snapshotPath = GetVoxelWorldSnapshotPath();
	std::unique_ptr<VoxelWorld> loadedWorld = LoadVoxelWorldSnapshot(snapshotPath);
	if (!loadedWorld) {
		runtime::LogRuntimeFailure(
			"App",
			"LoadActiveVoxelWorldSnapshot.LoadVoxelWorldSnapshot",
			"LoadVoxelWorldSnapshot returned null");
		return false;
	}

	state->world.voxelWorld = std::move(loadedWorld);
	state->world.requestedScenePreset = state->world.voxelWorld->scenePreset;
	return FinalizeActiveVoxelWorldReload(state, "LoadActiveVoxelWorldSnapshot");
}

bool IsInteractiveInputEvent(const SDL_Event &event)
{
	switch (event.type) {
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP:
	case SDL_EVENT_MOUSE_MOTION:
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
	case SDL_EVENT_MOUSE_WHEEL:
		return true;
	default:
		return false;
	}
}

bool StartLastInputReplayPlayback(AppState *state)
{
	PV_PROFILE_ZONE_N("StartLastInputReplayPlayback");
	PV_CHECK_OR_RETURN(state != nullptr, "App", "StartLastInputReplayPlayback.Preconditions", "AppState is null");

	if (!LoadLatestInputReplay(&state->input)) {
		return false;
	}
	if (!state->input.replay.captureAvailable) {
		runtime::LogRuntimeFailure(
			"App",
			"StartLastInputReplayPlayback.Capture",
			"no replay capture is available");
		return false;
	}
	if (!WaitForDeviceIdle(state->context)) {
		return false;
	}

	std::unique_ptr<VoxelWorld> loadedWorld = LoadVoxelWorldSnapshot(state->input.replay.capture.snapshotPath);
	if (!loadedWorld) {
		runtime::LogRuntimeFailure(
			"App",
			"StartLastInputReplayPlayback.LoadVoxelWorldSnapshot",
			"LoadVoxelWorldSnapshot returned null");
		return false;
	}

	state->world.voxelWorld = std::move(loadedWorld);
	state->world.requestedScenePreset = state->world.voxelWorld->scenePreset;
	if (!FinalizeActiveVoxelWorldReload(state, "StartLastInputReplayPlayback")) {
		return false;
	}

	CameraState *camera = GetPrimaryCameraState(state->ecs.get());
	const WorldState *const world = GetWorldState(state->ecs.get());
	PV_CHECK_OR_RETURN(
		camera && world && world->voxelWorld,
		"App",
		"StartLastInputReplayPlayback.World",
		"playback requires initialized camera and voxel world");

	*camera = state->input.replay.capture.initialCamera;
	state->interaction = state->input.replay.capture.initialInteraction;
	state->simulation.lastFrameCounter = 0;
	state->simulation.frameDeltaSeconds = 0.0f;
	state->simulation.simulationAccumulatorSeconds = 0.0f;
	state->simulation.simulationStepsLastFrame = 0;
	state->simulation.simulationTick = 0;
	state->input.mouseDeltaX = 0.0f;
	state->input.mouseDeltaY = 0.0f;
	state->input.removePressed = false;
	state->input.placePressed = false;
	ApplyInputActionSnapshot(state->input, 0u, 0u);
	SetPhysicsWalkAirControlMode(state->physics.get(), state->input.replay.capture.walkAirControlMode);
	SetPhysicsWalkAutoJumpDelayEnabled(state->physics.get(), state->input.replay.capture.walkAutoJumpDelayEnabled);
	if (camera->controlMode == CameraState::ControlMode::Walk) {
		ConsumeInputActionPressed(state->input, InputAction::MoveUp);
		if (!SnapWalkCharacterToCamera(state->physics.get(), world->voxelWorld.get(), camera)) {
			runtime::LogRuntimeFailure(
				"App",
				"StartLastInputReplayPlayback.SnapWalkCharacterToCamera",
				"SnapWalkCharacterToCamera returned false");
			return false;
		}
	} else if (camera->controlMode == CameraState::ControlMode::Creative) {
		if (!SnapCreativeCharacterToCamera(state->physics.get(), world->voxelWorld.get(), camera)) {
			runtime::LogRuntimeFailure(
				"App",
				"StartLastInputReplayPlayback.SnapCreativeCharacterToCamera",
				"SnapCreativeCharacterToCamera returned false");
			return false;
		}
	} else {
		ResetWalkCharacter(state->physics.get());
	}

	state->input.replay.recording = false;
	state->input.replay.playbackRequested = false;
	state->input.replay.playbackActive = !state->input.replay.capture.frames.empty();
	state->input.replay.playbackFrameIndex = 0;
	SDL_Log(
		"[ProjectV][InputReplay] playback started replay=%s frames=%zu",
		state->input.replay.replayPath.c_str(),
		state->input.replay.capture.frames.size());
	return true;
}
} // namespace

SDL_AppResult SDL_AppInit(void **appstate, int, char **)
{
	PV_PROFILE_ZONE_N("SDL_AppInit");
	profiling::SetThreadName("Main Thread");
	profiling::ConfigureDefaultPlots();

	auto state = std::make_unique<AppState>();
	if (!InitializeAppEcs(state.get())) {
		runtime::LogRuntimeFailure("App", "SDL_AppInit.InitializeAppEcs", "InitializeAppEcs returned false");
		ShutdownVulkan(state.get());
		return SDL_APP_FAILURE;
	}
	if (!InitVulkan(state.get())) {
		runtime::LogRuntimeFailure("App", "SDL_AppInit.InitVulkan", "InitVulkan returned false");
		ShutdownVulkan(state.get());
		return SDL_APP_FAILURE;
	}

	if (!SDL_SetWindowRelativeMouseMode(state->platform.window, state->input.relativeMouseModeEnabled)) {
		runtime::LogSdlFailure("SDL_AppInit.SDL_SetWindowRelativeMouseMode");
		state->input.relativeMouseModeEnabled = false;
	}

	*appstate = state.release();
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	if (!event) {
		return SDL_APP_CONTINUE;
	}

	if (event->type == SDL_EVENT_QUIT ||
		event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
		(event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE)) {
		return SDL_APP_SUCCESS;
	}

	AppState *state = static_cast<AppState *>(appstate);
	if (!state) {
		return SDL_APP_CONTINUE;
	}

	if (ShouldRequestSwapchainRefreshForWindowEvent(event->type)) {
		state->platform.windowResized = true;
	}
	if (state->input.replay.playbackActive && IsInteractiveInputEvent(*event)) {
		return SDL_APP_CONTINUE;
	}

	CameraState *camera = GetPrimaryCameraState(state->ecs.get());
	HandleInputActionEvent(state->input, event);
	HandleCameraEvent(camera, &state->input, event);
	HandleInteractionEvent(&state->input, event);
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
	PV_PROFILE_ZONE_N("SDL_AppIterate");
	auto *state = static_cast<AppState *>(appstate);
	if (!state) {
		runtime::LogRuntimeFailure("App", "SDL_AppIterate", "appstate is null");
		return SDL_APP_FAILURE;
	}

	CameraState *camera = GetPrimaryCameraState(state->ecs.get());
	DebugState *debug = GetDebugState(state->ecs.get());
	WorldState *world = GetWorldState(state->ecs.get());
	if (!camera || !debug || !world) {
		runtime::LogRuntimeFailure(
			"App",
			"SDL_AppIterate.EcsAccess",
			"primary camera, debug singleton or world singleton is unavailable");
		return SDL_APP_FAILURE;
	}
	if (state->input.replay.playbackRequested &&
		!StartLastInputReplayPlayback(state)) {
		runtime::LogRuntimeFailure(
			"App",
			"SDL_AppIterate.StartLastInputReplayPlayback",
			"StartLastInputReplayPlayback returned false");
		return SDL_APP_FAILURE;
	}
	if (state->input.replay.playbackActive &&
		!PrepareNextInputReplayPlaybackFrame(&state->input, &state->simulation)) {
		StopInputReplayPlayback(&state->input);
	}

	SDL_AppResult result = SDL_APP_FAILURE;
	if (!UpdateApp(
			&state->platform,
			&state->simulation,
			camera,
			&state->input,
			&state->interaction,
			world,
			state->physics.get(),
			&state->render,
			debug)) {
		runtime::LogRuntimeFailure("App", "SDL_AppIterate.UpdateApp", "UpdateApp returned false");
	} else if (world->snapshotSaveRequested &&
			   !SaveActiveVoxelWorldSnapshot(state)) {
		runtime::LogRuntimeFailure(
			"App",
			"SDL_AppIterate.SaveActiveVoxelWorldSnapshot",
			"SaveActiveVoxelWorldSnapshot returned false");
	} else if (world->scenePresetReloadRequested &&
			   !ReloadActiveVoxelScene(state, world->requestedScenePreset)) {
		runtime::LogRuntimeFailure(
			"App",
			"SDL_AppIterate.ReloadActiveVoxelScene",
			"ReloadActiveVoxelScene returned false");
	} else if (world->snapshotLoadRequested &&
			   !LoadActiveVoxelWorldSnapshot(state)) {
		runtime::LogRuntimeFailure(
			"App",
			"SDL_AppIterate.LoadActiveVoxelWorldSnapshot",
			"LoadActiveVoxelWorldSnapshot returned false");
	} else if (!SyncEcsWorldState(state->ecs.get())) {
		runtime::LogRuntimeFailure("App", "SDL_AppIterate.SyncEcsWorldState", "SyncEcsWorldState returned false");
	} else if (!PrepareFrameRenderData(
				   &state->context,
				   &state->swapchain,
				   camera,
				   &state->interaction,
				   debug,
				   world,
				   &state->render,
				   &state->frame)) {
		runtime::LogRuntimeFailure(
			"App",
			"SDL_AppIterate.PrepareFrameRenderData",
			"PrepareFrameRenderData returned false");
	} else {
		result = DrawFrame(
			&state->platform,
			&state->context,
			&state->swapchain,
			&state->render,
			&state->frame);
	}
	if (state->input.replay.playbackActive &&
		state->input.replay.playbackFrameIndex >= state->input.replay.capture.frames.size()) {
		StopInputReplayPlayback(&state->input);
	}

	PV_PROFILE_FRAME_MARK();
	return result;
}

void SDL_AppQuit(void *appstate, SDL_AppResult)
{
	auto *state = static_cast<AppState *>(appstate);
	ShutdownVulkan(state);
	delete state;
}

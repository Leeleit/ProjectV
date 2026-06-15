#define SDL_MAIN_USE_CALLBACKS 1
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

#include "app/AppUpdate.hpp"
#include "app/BenchmarkAutomation.hpp"
#include "app/Camera.hpp"
#include "app/FramePreparation.hpp"
#include "app/InputActions.hpp"
#include "app/InputReplay.hpp"
#include "app/LookDevCaptureAutomation.hpp"
#include "asset/ModelManifestLoader.hpp"
#include "audio/AudioEngine.hpp"
#include "audio/MusicDirectoryPath.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "core/Types.hpp"
#include "debug/Profiling.hpp"
#include "ecs/EcsWorld.hpp"
#include "physics/PhysicsWorld.hpp"
#include "platform/PlatformEvents.hpp"
#include "render/RayMarchPass.hpp"
#include "render/Renderer.hpp"
#include "render/SceneResources.hpp"
#include "render/vulkan/VulkanInit.hpp"
#include "render/vulkan/VulkanSwapchain.hpp"
#include "voxel/SceneConfig.hpp"
#include "voxel/VoxelInteraction.hpp"
#include "voxel/VoxelWorld.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#ifndef PROJECTV_CMAKE_BUILD_DIR
// **Hot shader reload fallback (`2026-06-15`).** If the
// TU was compiled outside CMake (rare — `cxx` test, ad-hoc
// `clang++ -c`), fall back to a Linux default so the symbol
// still resolves. The CMake-injected value (`src/CMakeLists.txt`)
// is the production path; this is a compile-time safety net
// only.
#define PROJECTV_CMAKE_BUILD_DIR "build/linux-clang-debug"
#endif

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

// **Hot shader reload (defense r0, 2026-06-13).** F5 re-runs `cmake --build`
// for the `Shaders` target (which re-invokes `glslc` on every
// `.comp` / `.frag` / `.vert` under `src/shaders/`) and re-loads the produced
// `.spv` files from disk. Pipeline recreation is scoped to the ray-march
// pass for now (it owns the only freshly-added `.comp`); the pre-existing
// graphics / shadow / TAA pipelines keep their cached shader modules until
// a real pipeline-recreate PR lands (per ТЗ 4.1.6).
int RebuildAllShadersFromDisk()
{
	int reloadedCount = 0;

	// 1. The `Shaders` custom target in `src/CMakeLists.txt:78` depends on
	//    the source `.comp` / `.frag` / `.vert` files. Re-invoking
	//    `cmake --build` for that target is the canonical way to refresh
	//    the `.spv` outputs. The user's environment has `glslc`
	//    available; the build host uses the same compiler at build time
	//    and at run time, so re-compiled `.spv` are byte-stable enough
	//    for a runtime pipeline-recreate.
	const char *buildDir = std::getenv("PROJECTV_BUILD_DIR");
	if (buildDir == nullptr) {
		// **Cross-platform fallback (`2026-06-15`).** Per
		// `src/CMakeLists.txt` the build tree path is injected
		// at configure time via `target_compile_definitions(... PROJECTV_CMAKE_BUILD_DIR=...)`.
		// On Windows that resolves to e.g. `build/windows-clang-debug`;
		// on Linux to `build/linux-clang-debug`. The `PROJECTV_BUILD_DIR`
		// env var still wins over the compile-time default.
		buildDir = PROJECTV_CMAKE_BUILD_DIR;
	}

	// **Cross-platform log path (`2026-06-15`).** The previous
	// `/tmp/projectv_shader_reload.log` literal is Linux-only
	// (`cmd.exe` has no `/tmp`); use std::filesystem's portable
	// temp-directory lookup instead. On Windows that resolves
	// to `%TEMP%`; on Linux to `$TMPDIR` (typically `/tmp`).
	const std::filesystem::path logPath =
		std::filesystem::temp_directory_path() / "projectv_shader_reload.log";
	const std::string cmakeCmd = std::string("cmake --build ") + buildDir +
		" --target Shaders > \"" + logPath.string() + "\" 2>&1";
	const int rc = std::system(cmakeCmd.c_str());
	if (rc == 0) {
		++reloadedCount;
	}

	// 2. Recreate the ray-march pass so the freshly compiled compute
	//    shader is re-bound next frame. Other pipelines keep their
	//    cached shader modules until a fuller pipeline-recreate slice
	//    lands.
	projectv::render::RequestRayMarchPipelineRecreate();

	std::fprintf(
		stderr,
		"[ProjectV][App] HotReloadShaders: re-built shaders, requested ray-march pipeline recreate\n");
	return reloadedCount;
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
	ApplyStartupCameraOverrideFromEnvironment(camera);
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

	// M5.1b follow-up: lift the loaded `modelInstances` to sit
	// cleanly on top of the voxel floor instead of half-submerged
	// in it (the "half in textures" symptom after the
	// `box.glb@0,1,0` env-var spawn). Idempotent + cheap
	// (one `GetVoxelMaterial` probe per AABB sample); safe to
	// re-run on every world reload / preset switch.
	projectv::asset::SnapModelInstancesAboveGroundDispatch(*world->voxelWorld, &state->render);

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
	state->render.taaHistoryValid = false;
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
	// **Tier 1.B (`2026-06-13`).** `std::expected` returns the
	// exact `VoxelSnapshotError` variant. The implementation has
	// already logged the per-step detail; the high-level caller
	// logs the variant name so the operator gets a single
	// "variant" log line + the deeper "step" log line in the
	// diagnostic output.
	const auto saveResult = SaveVoxelWorldSnapshot(*world->voxelWorld, snapshotPath);
	if (!saveResult.has_value()) {
		runtime::LogRuntimeFailure(
			"App",
			"SaveActiveVoxelWorldSnapshot.SaveVoxelWorldSnapshot",
			std::string{"SaveVoxelWorldSnapshot returned: "} + std::string{toString(saveResult.error())});
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
	// **Tier 1.B (`2026-06-13`).** `std::expected` returns the
	// exact `VoxelSnapshotError` variant. On error, the high-level
	// caller logs the variant name; the per-step detail is in the
	// lower-level log line emitted inside `LoadVoxelWorldSnapshot`.
	auto loadedResult = LoadVoxelWorldSnapshot(snapshotPath);
	if (!loadedResult.has_value()) {
		runtime::LogRuntimeFailure(
			"App",
			"LoadActiveVoxelWorldSnapshot.LoadVoxelWorldSnapshot",
			std::string{"LoadVoxelWorldSnapshot returned: "} + std::string{toString(loadedResult.error())});
		return false;
	}
	std::unique_ptr<VoxelWorld> loadedWorld = std::move(*loadedResult);

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

	std::unique_ptr<VoxelWorld> loadedWorld = [&]() -> std::unique_ptr<VoxelWorld> {
		// **Tier 1.B (`2026-06-13`).** `std::expected` carries the
		// `VoxelSnapshotError` variant through; we unwrap it here
		// and convert the error variant into a high-level log line.
		auto result = LoadVoxelWorldSnapshot(state->input.replay.capture.snapshotPath);
		if (!result.has_value()) {
			runtime::LogRuntimeFailure(
				"App",
				"StartLastInputReplayPlayback.LoadVoxelWorldSnapshot",
				std::string{"LoadVoxelWorldSnapshot returned: "} + std::string{toString(result.error())});
			return nullptr;
		}
		return std::move(*result);
	}();
	if (!loadedWorld) {
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
	// **Tier 5.** `0ull` (was `0u`) — the mask type
	// is now `uint64_t`. The 0 value implicit-converts
	// to either width, but matching the function
	// signature explicitly avoids the narrowing warning.
	ApplyInputActionSnapshot(state->input, 0ull, 0ull);
	SetPhysicsWalkAirControlMode(state->physics.get(), state->input.replay.capture.walkAirControlMode);
	SetPhysicsWalkAutoJumpEnabled(state->physics.get(), state->input.replay.capture.walkAutoJumpEnabled);
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
	// **Tier 1.B (`2026-06-13`).** `InitVulkan` now returns
	// `std::expected<void, VulkanInitError>`. The per-step
	// detail is logged inside the implementation; the
	// high-level caller logs the variant name and tears down.
	const auto initResult = InitVulkan(state.get());
	if (!initResult.has_value()) {
		runtime::LogRuntimeFailure(
			"App",
			"SDL_AppInit.InitVulkan",
			std::string{"InitVulkan returned: "} + std::string{projectv::vulkan_init::toString(initResult.error())});
		ShutdownVulkan(state.get());
		return SDL_APP_FAILURE;
	}
	// **Audio engine, 2026-06-12.** Init is non-fatal
	// on failure (per `decisions.md §28` — graceful
	// degradation so a miniaudio/PulseAudio init
	// failure on the host doesn't break the rest
	// of the program). The deleter (`AudioEnginePtr`
	// in `core/Types.hpp`) takes care of
	// `shutdown()` in the deleter TU, so
	// `SDL_AppQuit` only needs to reset the
	// `unique_ptr` (which happens implicitly when
	// `state` is destroyed). Construct via the
	// `AudioEnginePtr` alias explicitly because the
	// custom function-pointer deleter does not
	// accept the `std::default_delete<T>` returned
	// by `std::make_unique<AudioEngine>()`.
	// `CppDFAMemoryLeak` false positive: the engine is
	// transferred into `state->audio` (a
	// `unique_ptr<AudioEngine, DestroyAudioEngine>`)
	// and `state` itself is handed off to SDL via
	// `*appstate = state.release()` on the line below.
	// `SDL_AppQuit` does `delete state` which destroys
	// the engine and runs the deleter. The DFA doesn't
	// see the SDL3 callback round-trip, so it
	// reports a leak. Suppress per-line.
	// noinspection CppDFAMemoryLeak
	// AudioEngine is owned by `state` (an `std::unique_ptr` with
	// custom deleter `DestroyAudioEngine`). `SDL_AppQuit` retrieves
	// `appstate` via `state.release()` and runs the deleter. The DFA
	// doesn't see the SDL3 callback round-trip, so it reports a leak.
	state->audio = AudioEnginePtr(
		new projectv::audio::AudioEngine(),
		DestroyAudioEngine);
	if (!state->audio->init()) {
		SDL_Log("[ProjectV][Audio] miniaudio init failed; running without music");
		state->audio.reset();
	} else {
		// First playlist scan. `loadMusicFolder` now
		// returns `std::expected<size_t, AudioLoadError>`.
		// `.value_or(0)` preserves the historical "0 is
		// valid" contract: an empty folder (or a
		// `create_directories` failure the caller chose
		// to fall through) yields 0 tracks and the engine
		// is still alive. The operator can drop a file
		// in the folder and the next 5-second tick will
		// pick it up — no app restart required.
		const size_t trackCount = state->audio->loadMusicFolder(
			projectv::audio::GetMusicDirectoryPath()).value_or(0);
		SDL_Log("[ProjectV][Audio] miniaudio initialized; %zu mp3 track(s) in %s",
				trackCount,
				state->audio->musicFolder().string().c_str());
	}
	ConfigureLookDevCaptureAutomationFromEnvironment(&state->lookDevCapture);
	ConfigureBenchmarkAutomationFromEnvironment(&state->benchmark);

	// **Scene config (defense r0, 2026-06-13).** Read the JSON
	// scene-config at `runtime/scene.json` and apply it as a runtime
	// scene-preset override (per ТЗ 4.5.1 "Использование
	// структурированных форматов"). The reload path mirrors the
	// `PROJECTV_VOXEL_SCENE_PRESET` env-var flow that
	// `GetRequestedVoxelScenePreset` already supports, so a failed
	// JSON parse simply falls back to the hard-coded default.
	{
		const std::string configPath = projectv::voxel::GetDefaultSceneConfigPath();
		projectv::voxel::EnsureDefaultSceneConfig(configPath);

		projectv::voxel::SceneConfig config;
		if (projectv::voxel::LoadSceneConfig(configPath, config)) {
			std::fprintf(
				stderr,
				"[ProjectV][SceneConfig] loaded '%s' from %s (preset=%s)\n",
				config.name.c_str(),
				configPath.c_str(),
				std::string{VoxelScenePresetToString(config.scenePreset)}.c_str());
			WorldState *worldState = GetWorldState(state->ecs.get());
			if (worldState && worldState->voxelWorld &&
				worldState->voxelWorld->scenePreset != config.scenePreset) {
				if (ReloadActiveVoxelScene(state.get(), config.scenePreset)) {
					std::fprintf(
						stderr,
						"[ProjectV][SceneConfig] applied preset=%s\n",
						std::string{VoxelScenePresetToString(config.scenePreset)}.c_str());
				}
			}
		}
	}

	if (!SDL_SetWindowRelativeMouseMode(state->platform.window, state->input.relativeMouseModeEnabled)) {
		runtime::LogSdlFailure("SDL_AppInit.SDL_SetWindowRelativeMouseMode");
		state->input.relativeMouseModeEnabled = false;
	}

	*appstate = state.release();
	// noinspection CppDFAMemoryLeak
	// Same AudioEngine ownership handoff as in `SDL_AppInit` —
	// DFA doesn't see the `SDL_AppQuit` -> `delete state` path.
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

	// **Defense r0 hotkeys (2026-06-13, relocated twice 2026-06-15).** Originally
	// F5 = hot reload shaders, F6 = ray-march pass toggle. **Relocated to
	// F11 / F12** after a pre-defense code audit (`session-2026-06-15T10-43Z-
	// defense-docs-audit-r0`) discovered that F5 and F6 are also bound in
	// the formal `InputAction` system (`CycleScenePreset` and
	// `SaveWorldSnapshot` per `src/app/InputActions.cpp:134-135`), which
	// produced a confusing double-fire on the same key press.
	//
	// F11 and F12 are also bound in `InputAction` (`ToggleWalkAirControlMode`
	// and `ToggleWalkAutoJumpDelay`), so the second relocation to **digit
	// keys 1 / 2 / 3** (per `session-2026-06-15T-post-windows-build-verification-r1`)
	// frees both F-keys for their original walk-controller bindings. All 26
	// letters A-Z and all F1-F12 are bound in `InputAction` (digits 0, 7,
	// 8, 9 are bound to music player controls), so digits 1, 2, 3 are the
	// only top-row non-letter, non-F-key free cluster. Walk-controller
	// toggles and lighting debug reset now fire exclusively on F11/F12/V
	// with no shadow from the developer bypasses.
	//
	// **TODO post-defense (`Phase 7+`):** route these through the formal
	// `InputAction` system by adding `ReloadShaders` and
	// `ToggleRayMarchPass` enum values in `core/Types.hpp` (currently
	// mid-edit by `session-2026-06-13-hardcore-perf-r0` — that constraint
	// was the original reason for the bypass). Once `core/Types.hpp` is
	// stable, replace the SDLK_* checks with `ConsumeInputActionPressed`
	// on the new enum values, freeing 1/2/3 for any future use.
	//
	// See `docs/DefenseReport.md §2.7` for the original defense r0 contract.
	if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
		if (event->key.key == SDLK_1) {
			RebuildAllShadersFromDisk();
		} else if (event->key.key == SDLK_2) {
			const bool newState = !projectv::render::IsRayMarchEnabled();
			projectv::render::SetRayMarchEnabled(newState);
			std::fprintf(
				stderr,
				"[ProjectV][App] ToggleRayMarch: %s\n",
				newState ? "ray-march pass ENABLED" : "ray-march pass DISABLED");
		} else if (event->key.key == SDLK_3) {
			// **V-sync toggle (`2026-06-13`, auto-detect
			// cycle on `2026-06-14`).** Walks the
			// runtime present-mode cycle (built once
			// per swapchain create from the surface's
			// supported modes; see
			// `BuildPresentModeCycle` in
			// `VulkanSwapchain.cpp`). Cycle length is
			// the number of modes the host surface
			// exposes — usually 2 on Linux/Wayland
			// (no IMMEDIATE), 3 on Windows. Every
			// press advances the cycle by one step;
			// the previous "press V and nothing
			// changes" failure mode on Wayland is
			// gone. The next `RecreateSwapchain`
			// (forced at the end of this branch)
			// picks up the new preference via
			// `ChoosePresentMode`.
			const VkPresentModeKHR newMode =
				CyclePreferredPresentMode();
			const char *modeName = "unknown";
			switch (newMode) {
			case VK_PRESENT_MODE_IMMEDIATE_KHR: modeName = "IMMEDIATE (uncapped, may tear)"; break;
			case VK_PRESENT_MODE_MAILBOX_KHR: modeName = "MAILBOX (tear-free, uncapped)"; break;
			case VK_PRESENT_MODE_FIFO_KHR: modeName = "FIFO (vsync on, FPS = display rate)"; break;
			default: break;
			}
			const std::size_t cycleSize = GetPresentModeCycleSize();
			const std::size_t cycleIndex = GetPresentModeCycleIndex(newMode);
			std::fprintf(
				stderr,
				"[ProjectV][App] CycleVsync: %s [cycle %zu/%zu]\n",
				modeName,
				cycleIndex + 1u,
				cycleSize);
			// Force a swapchain rebuild so the new
			// mode takes effect immediately, not on
			// the next natural recreate.
			if (!RecreateSwapchain(
					&state->platform,
					&state->context,
					&state->swapchain,
					&state->render)) {
				runtime::LogRuntimeFailure(
					"App",
					"SDL_AppEvent.CycleVsync.RecreateSwapchain",
					"RecreateSwapchain returned false after vsync mode change");
			}
		}
	}

	CameraState *camera = GetPrimaryCameraState(state->ecs.get());
	// **Fullscreen / window-resize mouse guard (`2026-06-14`).**
	// WM-driven fullscreen toggle and DPI-driven resize can drop SDL's
	// relative mouse mode, then re-deliver a pre-capture MOUSE_MOTION with
	// a huge `xrel` / `yrel` on the next frame. `skipFirstMouseMotion` only
	// drops one event, and SDL can deliver a 1-3 event burst here, so set
	// `mouseMotionFreezeCount` to drop the next N events. The clamp in
	// `HandleCameraEvent` is the final safety net for huge single events.
	if (event->type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN ||
		event->type == SDL_EVENT_WINDOW_LEAVE_FULLSCREEN ||
		event->type == SDL_EVENT_WINDOW_RESIZED ||
		event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
		state->input.skipFirstMouseMotion = true;
		state->input.mouseMotionFreezeCount = 5;
		state->input.mouseDeltaX = 0.0f;
		state->input.mouseDeltaY = 0.0f;
	}
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
	const bool quitAfterLookDevCaptureFrame =
		UpdateLookDevCaptureAutomation(&state->lookDevCapture, &state->render);
	const DebugState *benchmarkDebug = GetDebugState(state->ecs.get());
	const Uint64 benchmarkFrameCounter = SDL_GetPerformanceCounter();
	const bool quitAfterBenchmarkFrame =
		UpdateBenchmarkAutomation(
			&state->benchmark,
			benchmarkDebug ? benchmarkDebug->stats : DebugStats{},
			benchmarkFrameCounter);

	// **Fluid CA tick — moved to `UpdateApp` on 2026-06-14.**
	// The CA tick used to live here with a wall-clock throttle
	// (30 Hz). It was moved to `AppUpdate.cpp` (after the
	// `simulationAccumulatorSeconds` block) so it honours
	// `simulation->paused`, `simulation->timeScale`, and the
	// `frameStepRequested` flag for free. See
	// `agent/decisions.md §30` and `agent/memory.md §12`
	// (CA pause/timeScale fix history).

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
			debug,
			state->audio.get())) {
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
				   &state->frame,
				   &state->input)) {
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
		if (quitAfterLookDevCaptureFrame && result == SDL_APP_CONTINUE) {
			result = SDL_APP_SUCCESS;
		}
		if (quitAfterBenchmarkFrame && result == SDL_APP_CONTINUE) {
			result = SDL_APP_SUCCESS;
		}
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

import projectv.math; // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "app/AppUpdate.hpp"

#include "app/Camera.hpp"
#include "app/AppUpdateHelpers.hpp"
#include "app/InputActions.hpp"
#include "app/InputReplay.hpp"
#include "audio/AudioEngine.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"
#include "physics/PhysicsWorld.hpp"
#include "voxel/VoxelInteraction.hpp"
#include "voxel/VoxelWorld.hpp"

#include <algorithm>
#include <array>

namespace {
constexpr float kMaxFrameDeltaSeconds = 0.25f;		   // EVIL: 250ms cap; prevents huge dt spikes after pause/focus loss; tuned for 60 FPS frame budget
constexpr float kLightingExposureStepStops = 0.25f;	   // EVIL: 1/4 stop per keyboard step; smaller → sluggish, larger → jarring
constexpr float kMinLightingExposureBiasStops = -4.0f; // EVIL: -4 stops lower; matches ACES tone-map dark floor per VoxelMaterials.cpp:62
constexpr float kMaxLightingExposureBiasStops = 4.0f;  // EVIL: +4 stops upper; symmetric to kMin; prevents exposure runaway

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

WalkAirControlMode GetNextWalkAirControlMode(const WalkAirControlMode mode)
{
	switch (mode) {
	case WalkAirControlMode::MinecraftLike:
		return WalkAirControlMode::Realistic;
	case WalkAirControlMode::Realistic:
		return WalkAirControlMode::MinecraftLike;
	}

	return WalkAirControlMode::MinecraftLike;
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
	input.skipFirstMouseMotion = true;
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

void AdjustLightingExposure(
	RenderState &render,
	const float deltaStops)
{
	render.lightingDebugControls.exposureBiasStops = std::clamp(
		render.lightingDebugControls.exposureBiasStops + deltaStops,
		kMinLightingExposureBiasStops,
		kMaxLightingExposureBiasStops);
}

void ResetLightingDebugControls(RenderState &render)
{
	render.lightingDebugControls = {};
}

void MirrorVoxelStatsToDebugStats(
	const VoxelWorld &world,
	const RenderState &render,
	DebugStats &stats)
{
	stats.dirtyChunkCount = world.stats.dirtyChunkCount;
	stats.activeChunkCount = world.stats.activeChunkCount;
	stats.glassVoxelCount = world.stats.glassVoxelCount;
	stats.fluidVoxelCount = world.stats.fluidVoxelCount;
	stats.floorVoxelCount =
		world.stats.floorWhiteVoxelCount + world.stats.floorGrayVoxelCount;
	stats.nonAirVoxelCount = world.stats.nonAirVoxelCount;
	stats.sceneTriangleCount = render.sceneTriangleCount;
	stats.sceneMemoryBytes = render.sceneMemoryBytes;
	stats.worldEditVersion = world.editVersion;
	stats.scenePreset = world.scenePreset;
}

void MirrorRenderPassTimingsToDebugStats(
	const RenderState &render,
	DebugStats &stats)
{
	stats.renderPassShadowMs = render.renderPassTimings.shadowMs;
	stats.renderPassMeshingMs = render.renderPassTimings.meshingMs;
	stats.renderPassGraphicsMs = render.renderPassTimings.graphicsMs;
	stats.renderPassDebugOverlayMs = render.renderPassTimings.debugOverlayMs;
	stats.renderPassDebugHudMs = render.renderPassTimings.debugHudMs;
	stats.renderPassDirtyChunkRebuiltCount = render.renderPassTimings.dirtyChunkRebuiltCount;
	stats.renderPassOtherMs = std::max(
		0.0f,
		stats.frameTimeMilliseconds - render.renderPassTimings.graphicsMs);
}

void MirrorAudioStatsToDebugStats(
	const projectv::audio::AudioEngine *audio,
	DebugStats &stats)
{
	if (!audio) {
		stats.audioMusicInitialized = false;
		stats.audioMusicState = 0;
		stats.audioMusicVolume = 0.0f;
		stats.audioMusicPlaylistSize = 0;
		stats.audioMusicCurrentIndex = 0;
		std::ranges::fill(stats.audioMusicTrackName, '\0');
		std::ranges::fill(stats.audioMusicArtist, '\0');
		std::ranges::fill(stats.audioMusicTitle, '\0');
		stats.audioMusicPositionSec = 0.0f;
		stats.audioMusicDurationSec = 0.0f;
		return;
	}

	stats.audioMusicInitialized = audio->isInitialized();
	stats.audioMusicState = static_cast<uint8_t>(audio->state());
	stats.audioMusicVolume = audio->volume();
	stats.audioMusicPlaylistSize = static_cast<uint32_t>(audio->playlistSize());
	stats.audioMusicCurrentIndex = static_cast<uint32_t>(audio->currentIndex());
	const std::string &trackName = audio->currentTrackName();
	std::ranges::fill(stats.audioMusicTrackName, '\0');
	const size_t copyLen = std::min(trackName.size(),
									stats.audioMusicTrackName.size() - 1);
	std::copy_n(trackName.begin(), copyLen, stats.audioMusicTrackName.begin());
	const std::string &artist = audio->currentArtist();
	std::ranges::fill(stats.audioMusicArtist, '\0');
	const size_t artistCopyLen = std::min(artist.size(),
										  stats.audioMusicArtist.size() - 1);
	std::copy_n(artist.begin(), artistCopyLen, stats.audioMusicArtist.begin());

	const std::string &title = audio->currentTitle();
	std::ranges::fill(stats.audioMusicTitle, '\0');
	const size_t titleCopyLen = std::min(title.size(),
										 stats.audioMusicTitle.size() - 1);
	std::copy_n(title.begin(), titleCopyLen, stats.audioMusicTitle.begin());

	stats.audioMusicPositionSec = audio->positionSeconds();
	stats.audioMusicDurationSec = audio->durationSeconds();
}

void MirrorRenderLightingToDebugStats(
	RenderState &render,
	const CameraState &camera,
	DebugStats &stats)
{
	(void)camera;
	stats.sceneExposure = render.currentSceneLighting.postProcess[0];
	stats.sceneEnvironmentIntensity = render.currentSceneLighting.postProcess[1];
	stats.sceneColorGradeWhitePoint = render.currentSceneLighting.colorGrading[0];
	stats.sceneColorGradeContrast = render.currentSceneLighting.colorGrading[1];
	stats.sceneColorGradeSaturation = render.currentSceneLighting.colorGrading[2];
	stats.sceneColorGradeLift = render.currentSceneLighting.colorGrading[3];
	stats.sceneExposureMeteringMode =
		static_cast<ExposureMeteringMode>(std::lround(render.currentSceneLighting.exposureControl[0]));
	stats.sceneExposureKey = EstimateVoxelSceneExposureKey(render.currentSceneLighting);
	stats.sceneExposureTargetKey = render.currentSceneLighting.exposureControl[1];
	stats.sceneMinExposure = render.currentSceneLighting.exposureControl[2];
	stats.sceneMaxExposure = render.currentSceneLighting.exposureControl[3];
	stats.toneMapOperator = render.lightingDebugControls.toneMapOperator;
	stats.lightingDebugView = render.lightingDebugControls.debugView;
	stats.sunDirection = {
		render.currentSceneLighting.sunDirectionAndWrap[0],
		render.currentSceneLighting.sunDirectionAndWrap[1],
		render.currentSceneLighting.sunDirectionAndWrap[2],
	};
	stats.sunIntensity = render.currentSceneLighting.sunColorAndIntensity[3];
	stats.sunContactShadowStrength = render.currentSceneLighting.sunContactShadowParams[0];
	stats.sunContactShadowDistance = render.currentSceneLighting.sunContactShadowParams[1];
	stats.ambientOcclusionStrength = render.currentSceneLighting.ambientOcclusionParams[0];
	stats.ambientOcclusionRadius = render.currentSceneLighting.ambientOcclusionParams[1];
	stats.ambientOcclusionMinVisibility = render.currentSceneLighting.ambientOcclusionParams[2];
	stats.localPointLightPosition = {
		render.currentSceneLighting.localPointLightPositionAndRadius[0],
		render.currentSceneLighting.localPointLightPositionAndRadius[1],
		render.currentSceneLighting.localPointLightPositionAndRadius[2],
	};
	stats.localPointLightColor = {
		render.currentSceneLighting.localPointLightColorAndIntensity[0],
		render.currentSceneLighting.localPointLightColorAndIntensity[1],
		render.currentSceneLighting.localPointLightColorAndIntensity[2],
	};
	stats.localPointLightRadius = render.currentSceneLighting.localPointLightPositionAndRadius[3];
	stats.localPointLightIntensity = render.currentSceneLighting.localPointLightColorAndIntensity[3];
	stats.localPointLightEnabled = static_cast<bool>(render.currentSceneLighting.localPointLightParams[0]);
	stats.localPointLightSourceRadius = render.currentSceneLighting.localPointLightParams[1];
	stats.localPointLightShadowStrength = render.currentSceneLighting.localPointLightParams[2];
	stats.localPointLightShadowBias = render.currentSceneLighting.localPointLightParams[3];
	stats.transparentShadowPolicy = TransparentShadowPolicy::GlassIgnoredFluidCasts;
}

void MirrorWalkStatsToDebugStats(
	const PhysicsWalkDebugInfo &walkDebugInfo,
	DebugStats &stats)
{
	stats.walkDebugValid = walkDebugInfo.valid;
	stats.walkSupportState = static_cast<uint8_t>(walkDebugInfo.supportState);
	stats.walkFeetPosition = projectv::math::Vec3{
		walkDebugInfo.feetPosition[0],
		walkDebugInfo.feetPosition[1],
		walkDebugInfo.feetPosition[2],
	};
	stats.walkFootSupportScore = walkDebugInfo.footSupportScore;
	stats.walkFootSupportHitSamples = walkDebugInfo.footSupportHitSamples;
	stats.walkFootSupportTotalSamples = walkDebugInfo.footSupportTotalSamples;
	stats.walkEdgeGraceFramesRemaining = walkDebugInfo.edgeGraceFramesRemaining;
	stats.walkGroundTakeoffGraceFramesRemaining = walkDebugInfo.groundTakeoffGraceFramesRemaining;
	stats.walkSneakSupportGraceFramesRemaining = walkDebugInfo.sneakSupportGraceFramesRemaining;
	stats.walkLedgeReleaseGraceFramesRemaining = walkDebugInfo.ledgeReleaseGraceFramesRemaining;
	stats.walkAutoJumpDelayFramesRemaining = walkDebugInfo.autoJumpDelayFramesRemaining;
	stats.walkGroundTakeoffCached = walkDebugInfo.groundTakeoffCached;
	stats.walkSneakActive = walkDebugInfo.sneakActive;
	stats.walkJumpLockActive = walkDebugInfo.jumpLockActive;
	stats.walkSuppressPassiveSlide = walkDebugInfo.suppressPassiveSlide;
}

void MirrorInputReplayStatsToDebugStats(
	const InputState &input,
	DebugStats &stats)
{
	stats.inputReplayRecording = input.replay.recording;
	stats.inputReplayPlaybackActive = input.replay.playbackActive;
	stats.inputReplayReady = input.replay.captureAvailable;
	stats.inputReplayFrameCount = static_cast<uint32_t>(input.replay.capture.frames.size());
	stats.inputReplayPlaybackFrameIndex = static_cast<uint32_t>(input.replay.playbackFrameIndex);
}
} // namespace

bool ProcessInputActions(
	PlatformState *platform,
	CameraState *camera,
	SimulationState *simulation,
	InputState *input,
	InteractionState *interaction,
	WorldState *world,
	PhysicsState *physics,
	RenderState *render,
	DebugState *debug,
	projectv::audio::AudioEngine *audio)
{
	if (ConsumeInputActionPressed(*input, InputAction::ToggleHud)) {
		debug->hudVisible = !debug->hudVisible;
	}
	if (ConsumeInputActionPressed(*input, InputAction::ToggleDetailedHud)) {
		debug->detailedHudVisible = !debug->detailedHudVisible;
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
		interaction->mutationAnchorValid = false;
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
		interaction->mutationAnchorValid = false;
		ClearInteractionClickActions(*input);
	}
	if (ConsumeInputActionPressed(*input, InputAction::CaptureScreenshot) &&
		world->voxelWorld &&
		!world->scenePresetReloadRequested &&
		!world->snapshotLoadRequested) {
		render->screenshotCaptureRequested = true;
	}
	if (ConsumeInputActionPressed(*input, InputAction::CycleEditorTool)) {
		interaction->editorTool = GetNextDebugEditorTool(interaction->editorTool);
		interaction->mutationAnchorValid = false;
		ClearInteractionClickActions(*input);
	}
	if (ConsumeInputActionPressed(*input, InputAction::ToggleChunkBounds)) {
		debug->showChunkBounds = !debug->showChunkBounds;
	}
	if (ConsumeInputActionPressed(*input, InputAction::ToggleDirtyChunkOverlay)) {
		debug->showDirtyChunkOverlay = !debug->showDirtyChunkOverlay;
	}

	if (ConsumeInputActionPressed(*input, InputAction::ToggleCursorHitNormal)) {
		debug->showCursorHitNormal = !debug->showCursorHitNormal;
	}
	if (ConsumeInputActionPressed(*input, InputAction::ToggleWalkAirControlMode)) {
		SetPhysicsWalkAirControlMode(physics, GetNextWalkAirControlMode(GetPhysicsWalkAirControlMode(physics)));
	}
	if (ConsumeInputActionPressed(*input, InputAction::ToggleWalkAutoJump)) {
		SetPhysicsWalkAutoJumpEnabled(physics, !IsPhysicsWalkAutoJumpEnabled(physics));
	}
	if (ConsumeInputActionPressed(*input, InputAction::ToggleWalkAutoJumpDelay)) {
		SetPhysicsWalkAutoJumpDelayEnabled(physics, !IsPhysicsWalkAutoJumpDelayEnabled(physics));
	}
	if (ConsumeInputActionPressed(*input, InputAction::DecreaseTimeScale)) {
		constexpr float kTimeScaleDownStep = 0.5f;
		constexpr float kTimeScaleSnapToZeroThreshold = 0.01f;
		simulation->timeScale *= kTimeScaleDownStep;
		if (simulation->timeScale < kTimeScaleSnapToZeroThreshold) {
			simulation->timeScale = 0.0f;
		}
	}
	if (ConsumeInputActionPressed(*input, InputAction::IncreaseTimeScale)) {
		constexpr float kTimeScaleUpStep = 2.0f;
		constexpr float kTimeScaleSnapFromZeroTarget = 0.5f;
		if (simulation->timeScale <= 0.0f) {
			simulation->timeScale = kTimeScaleSnapFromZeroTarget;
		} else {
			simulation->timeScale *= kTimeScaleUpStep;
		}
		simulation->timeScale = std::min(simulation->timeScale, 4.0f);
	}
	if (ConsumeInputActionPressed(*input, InputAction::ResetTimeScale)) {
		simulation->timeScale = 1.0f;
	}
	if (ConsumeInputActionPressed(*input, InputAction::StepSingleFrame)) {
		simulation->frameStepRequested = true;
	}
	if (ConsumeInputActionPressed(*input, InputAction::DecreaseLightingExposure)) {
		AdjustLightingExposure(*render, -0.25f);
	}
	if (ConsumeInputActionPressed(*input, InputAction::IncreaseLightingExposure)) {
		AdjustLightingExposure(*render, 0.25f);
	}
	if (ConsumeInputActionPressed(*input, InputAction::CycleToneMapOperator)) {
		render->lightingDebugControls.toneMapOperator =
			GetNextToneMapOperator(render->lightingDebugControls.toneMapOperator);
	}
	if (ConsumeInputActionPressed(*input, InputAction::CycleLightingDebugView)) {
		render->lightingDebugControls.debugView =
			GetNextLightingDebugView(render->lightingDebugControls.debugView);
	}
	if (ConsumeInputActionPressed(*input, InputAction::ResetLightingDebugControls)) {
		ResetLightingDebugControls(*render);
	}
	if (audio) {
		constexpr float kMusicVolumeStep = 0.05f;
		if (ConsumeInputActionPressed(*input, InputAction::ToggleMusicPlayPause)) {
			audio->togglePlayPause();
		}
		if (ConsumeInputActionPressed(*input, InputAction::StopMusic)) {
			audio->stop();
		}
		if (ConsumeInputActionPressed(*input, InputAction::MusicVolumeDown)) {
			audio->decreaseVolume(kMusicVolumeStep);
		}
		if (ConsumeInputActionPressed(*input, InputAction::MusicVolumeUp)) {
			audio->increaseVolume(kMusicVolumeStep);
		}
		if (ConsumeInputActionPressed(*input, InputAction::NextMusicTrack)) {
			audio->nextTrack();
		}
		if (ConsumeInputActionPressed(*input, InputAction::PreviousMusicTrack)) {
			audio->previousTrack();
		}
		audio->tick();
	}
	if (ConsumeInputActionPressed(*input, InputAction::ToggleInputReplayRecording)) {
		if (input->replay.recording) {
			if (!StopInputReplayRecording(input)) {
				return false;
			}
		} else {
			PV_CHECK_OR_RETURN(
				world->voxelWorld,
				"App",
				"UpdateApp.StartInputReplayRecording",
				"input replay recording requires an active voxel world");
			if (!StartInputReplayRecording(
					input,
					*world->voxelWorld,
					*camera,
					*interaction,
					GetPhysicsWalkAirControlMode(physics),
					IsPhysicsWalkAutoJumpEnabled(physics),
					IsPhysicsWalkAutoJumpDelayEnabled(physics))) {
				return false;
			}
		}
	}
	if (ConsumeInputActionPressed(*input, InputAction::PlayLastInputReplay) &&
		!input->replay.recording &&
		!input->replay.playbackActive) {
		input->replay.playbackRequested = true;
	}
	return true;
}

bool RunFrameSimulation(
	CameraState *camera,
	InputState *input,
	WorldState *world,
	PhysicsState *physics,
	SimulationState *simulation,
	const bool cameraCanUpdate)
{
	if (physics && world->voxelWorld && !SyncPhysicsWorld(physics, world->voxelWorld.get())) {
		runtime::LogRuntimeFailure(
			"App",
			"UpdateApp.SyncPhysicsWorld",
			"SyncPhysicsWorld returned false before simulation tick");
		return false;
	}

	if (cameraCanUpdate) {
		ConsumeCameraLookInput(camera, input);
	} else {
		input->mouseDeltaX = 0.0f;
		input->mouseDeltaY = 0.0f;
	}

	if (!projectv::app::RunSimulationTickLoop(camera, input, *world, *simulation, physics)) {
		runtime::LogRuntimeFailure(
			"App",
			"UpdateApp.RunSimulationTickLoop",
			"simulation tick returned false");
		return false;
	}

	const uint64_t worldEditVersionBeforeInteraction =
		world->voxelWorld ? world->voxelWorld->editVersion : 0;
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

	if (physics && world->voxelWorld) {
		const uint32_t processedChunkRebuilds =
			ProcessChunkRebuildQueue(physics, world->voxelWorld.get());
		if (processedChunkRebuilds > 0u) {
			profiling::PlotValue(
				"Processed Chunk Rebuilds",
				static_cast<int64_t>(processedChunkRebuilds));
		}
	}

	return true;
}

void MirrorAllFrameStats(
	CameraState *camera,
	WorldState *world,
	RenderState *render,
	DebugState *debug,
	PhysicsState *physics,
	InputState &input,
	projectv::audio::AudioEngine *audio,
	const SimulationState &simulation)
{
	profiling::PlotValue("Frame Delta (ms)", simulation.frameDeltaSeconds * 1000.0f);
	profiling::PlotValue(
		"Simulation Accumulator (ms)",
		simulation.simulationAccumulatorSeconds * 1000.0f);
	profiling::PlotValue("Simulation Steps", static_cast<int64_t>(simulation.simulationStepsLastFrame));

	if (world->voxelWorld) {
		debug->stats.simulationStepsLastFrame = simulation.simulationStepsLastFrame;
		MirrorVoxelStatsToDebugStats(*world->voxelWorld, *render, debug->stats);
	}
	MirrorRenderPassTimingsToDebugStats(*render, debug->stats);
	MirrorAudioStatsToDebugStats(audio, debug->stats);
	debug->stats.controlMode = camera->controlMode;
	debug->stats.walkAirControlMode = GetPhysicsWalkAirControlMode(physics);
	debug->stats.detailedHudVisible = debug->detailedHudVisible;
	debug->stats.walkAutoJumpEnabled = IsPhysicsWalkAutoJumpEnabled(physics);
	debug->stats.walkAutoJumpDelayEnabled = IsPhysicsWalkAutoJumpDelayEnabled(physics);
	debug->stats.simulationPaused = simulation.paused;
	debug->stats.simulationTimeScale = simulation.timeScale;
	debug->stats.simulationFrameStepPending = simulation.frameStepRequested;
	debug->stats.showChunkBounds = debug->showChunkBounds;
	debug->stats.showDirtyChunkOverlay = debug->showDirtyChunkOverlay;
	MirrorRenderLightingToDebugStats(*render, *camera, debug->stats);
	MirrorWalkStatsToDebugStats(GetPhysicsWalkDebugInfo(physics), debug->stats);
	MirrorInputReplayStatsToDebugStats(input, debug->stats);
}

bool UpdateApp(
	PlatformState *platform,
	SimulationState *simulation,
	CameraState *camera,
	InputState *input,
	InteractionState *interaction,
	WorldState *world,
	PhysicsState *physics,
	RenderState *render,
	DebugState *debug,
	projectv::audio::AudioEngine *audio)
{
	PV_PROFILE_ZONE_N("UpdateApp");
	if (!platform || !simulation || !camera || !input || !interaction || !world || !render || !debug) {
		return false;
	}

	simulation->frameDeltaSeconds = ComputeFrameDeltaSeconds(*simulation);
	projectv::app::UpdateFrameStatistics(*simulation, *debug, *render);

	if (input->replay.recording) {
		RecordInputReplayFrame(input, simulation->frameDeltaSeconds);
	}

	if (!ProcessInputActions(
			platform,
			camera,
			simulation,
			input,
			interaction,
			world,
			physics,
			render,
			debug,
			audio)) {
		return false;
	}

	const bool effectivePaused = projectv::app::UpdateEffectivePausedAndEditing(*camera, *world, *simulation);
	const bool spectatorMode = IsSpectatorMode(*camera);
	const bool cameraCanUpdate = spectatorMode || !effectivePaused;

	if (!RunFrameSimulation(camera, input, world, physics, simulation, cameraCanUpdate)) {
		return false;
	}

	UpdateVoxelInteraction(*camera, input, world->voxelWorld.get(), interaction, world->allowWorldEditing, physics); // update voxel interaction based on camera raycast and player input

	MirrorAllFrameStats(camera, world, render, debug, physics, *input, audio, *simulation);

	return true;
}

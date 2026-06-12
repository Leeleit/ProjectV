#include "app/AppUpdate.hpp"

#include "app/Camera.hpp"
#include "app/InputActions.hpp"
#include "app/InputReplay.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"
#include "physics/PhysicsWorld.hpp"
#include "render/ShadowProjection.hpp"
#include "voxel/VoxelInteraction.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr uint32_t kMaxSimulationStepsPerFrame = 5;
constexpr float kMaxFrameDeltaSeconds = 0.25f;
constexpr float kLightingExposureStepStops = 0.25f;
constexpr float kMinLightingExposureBiasStops = -4.0f;
constexpr float kMaxLightingExposureBiasStops = 4.0f;
constexpr float kShadowStrengthStep = 0.05f;
constexpr float kShadowDepthBiasStep = 0.0001f;
constexpr float kShadowNormalBiasStep = 0.0005f;
constexpr float kShadowFilterRadiusStep = 0.10f;
constexpr float kShadowCoverageScaleStep = 0.10f;
constexpr float kShadowCascadeBlendStep = 0.02f;
constexpr float kMinShadowStrengthOffset = -1.0f;
constexpr float kMaxShadowStrengthOffset = 1.0f;
constexpr float kMinShadowDepthBiasOffset = -0.01f;
constexpr float kMaxShadowDepthBiasOffset = 0.01f;
constexpr float kMinShadowNormalBiasOffset = -0.05f;
constexpr float kMaxShadowNormalBiasOffset = 0.05f;
constexpr float kMinShadowFilterRadiusOffset = -4.0f;
constexpr float kMaxShadowFilterRadiusOffset = 4.0f;
constexpr float kMinShadowCoverageScale = 0.5f;
constexpr float kMaxShadowCoverageScale = 3.0f;
constexpr float kMinShadowCascadeBlendOffset = -0.50f;
constexpr float kMaxShadowCascadeBlendOffset = 0.50f;

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
	// P0.3 follow-up: any (re-)enable of relative mouse mode risks a large
	// spurious first MOUSE_MOTION because the cursor was unrestrained
	// between the previous disable and this enable, so the next relative
	// delta is measured from the unrestrained position. Drop the first
	// motion after each (re-)enable. The init path already defaults the
	// flag to true, so this line is what protects tab-toggle in-flight.
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

void AdjustShadowTuning(
	RenderState &render,
	const float delta)
{
	VoxelLightingDebugControls &controls = render.lightingDebugControls;
	switch (controls.shadowTuningTarget) {
	case ShadowTuningTarget::Strength:
		controls.shadowStrengthOffset = std::clamp(
			controls.shadowStrengthOffset + delta * kShadowStrengthStep,
			kMinShadowStrengthOffset,
			kMaxShadowStrengthOffset);
		break;
	case ShadowTuningTarget::DepthBias:
		controls.shadowDepthBiasOffset = std::clamp(
			controls.shadowDepthBiasOffset + delta * kShadowDepthBiasStep,
			kMinShadowDepthBiasOffset,
			kMaxShadowDepthBiasOffset);
		break;
	case ShadowTuningTarget::NormalBias:
		controls.shadowNormalBiasOffset = std::clamp(
			controls.shadowNormalBiasOffset + delta * kShadowNormalBiasStep,
			kMinShadowNormalBiasOffset,
			kMaxShadowNormalBiasOffset);
		break;
	case ShadowTuningTarget::FilterRadius:
		controls.shadowFilterRadiusOffset = std::clamp(
			controls.shadowFilterRadiusOffset + delta * kShadowFilterRadiusStep,
			kMinShadowFilterRadiusOffset,
			kMaxShadowFilterRadiusOffset);
		break;
	case ShadowTuningTarget::Coverage:
		controls.shadowCoverageScale = std::clamp(
			controls.shadowCoverageScale + delta * kShadowCoverageScaleStep,
			kMinShadowCoverageScale,
			kMaxShadowCoverageScale);
		break;
	case ShadowTuningTarget::CascadeBlend:
		controls.shadowCascadeBlendOffset = std::clamp(
			controls.shadowCascadeBlendOffset + delta * kShadowCascadeBlendStep,
			kMinShadowCascadeBlendOffset,
			kMaxShadowCascadeBlendOffset);
		break;
	}
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

	if (input->replay.recording) {
		RecordInputReplayFrame(input, simulation->frameDeltaSeconds);
	}

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
	// 5.2 debug gizmos: cascade split planes + cursor hit normal.
	// Both overlays follow the same hotkey-on/visibility-on pair
	// already used for chunk bounds / dirty chunk overlay: the
	// key only flips the flag, the actual emission gate is in
	// `BuildDebugOverlayBoxes` and additionally requires
	// `debug->hudVisible` to be true.
	if (ConsumeInputActionPressed(*input, InputAction::ToggleCascadeSplitPlanes)) {
		debug->showCascadeSplitPlanes = !debug->showCascadeSplitPlanes;
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
	if (ConsumeInputActionPressed(*input, InputAction::ToggleTaa) &&
		world->voxelWorld) {
		render->taaEnabled = !render->taaEnabled;
		render->taaHistoryValid = false;
		// 1.5 — same reset for the layer history pair. Toggling
		// TAA on/off should also drop the layer history (same
		// underlying reason — the next frame's blending would
		// be against stale per-layer data from before the
		// toggle).
		render->taaLayerHistoryValid = false;
	}
	// TAA tuning ladder: 4 live parameters behind `;`/`'`/`-`/`=`/`,`/`.`.
	// Jitter scale is a continuous multiplier in [0, 2] (step 0.25) on the
	// Halton(2,3) output; blend is the per-frame history weight in [0, 1]
	// (step 0.05); neighbourhood radius cycles through 1/3/5/7 for the
	// history clamp in `taa_resolve.frag`; history-invalidate forces one
	// frame of `taaHistoryValid=false` so the next resolve takes the current
	// scene as the only sample. All four invalidate history on change so
	// the next frame uses the new blend/jitter/radius cleanly.
	if (ConsumeInputActionPressed(*input, InputAction::DecreaseTaaJitterScale)) {
		render->taaJitterScale = std::clamp(render->taaJitterScale - 0.25f, 0.0f, 2.0f);
		render->taaHistoryValid = false;
		render->taaLayerHistoryValid = false;
	}
	if (ConsumeInputActionPressed(*input, InputAction::IncreaseTaaJitterScale)) {
		render->taaJitterScale = std::clamp(render->taaJitterScale + 0.25f, 0.0f, 2.0f);
		render->taaHistoryValid = false;
		render->taaLayerHistoryValid = false;
	}
	if (ConsumeInputActionPressed(*input, InputAction::DecreaseTaaBlend)) {
		render->taaBlend = std::clamp(render->taaBlend - 0.05f, 0.0f, 1.0f);
		render->taaHistoryValid = false;
		render->taaLayerHistoryValid = false;
	}
	if (ConsumeInputActionPressed(*input, InputAction::IncreaseTaaBlend)) {
		render->taaBlend = std::clamp(render->taaBlend + 0.05f, 0.0f, 1.0f);
		render->taaHistoryValid = false;
		render->taaLayerHistoryValid = false;
	}
	if (ConsumeInputActionPressed(*input, InputAction::CycleTaaNeighbourhoodRadius)) {
		// Cycle 1 -> 3 -> 5 -> 7 -> 1. The shader clamps to the same
		// 1/3/5/7 set, so odd values outside that set won't be applied.
		constexpr std::array<int32_t, 4> kNeighbourhoodCycle{1, 3, 5, 7};
		auto current = std::find(
			kNeighbourhoodCycle.begin(),
			kNeighbourhoodCycle.end(),
			render->taaNeighbourhoodRadius);
		if (current == kNeighbourhoodCycle.end()) {
			render->taaNeighbourhoodRadius = kNeighbourhoodCycle.front();
		} else {
			const size_t nextIndex = (static_cast<size_t>(current - kNeighbourhoodCycle.begin()) + 1u)
				% kNeighbourhoodCycle.size();
			render->taaNeighbourhoodRadius = kNeighbourhoodCycle[nextIndex];
		}
		render->taaHistoryValid = false;
		render->taaLayerHistoryValid = false;
	}
	if (ConsumeInputActionPressed(*input, InputAction::InvalidateTaaHistory)) {
		render->taaHistoryValid = false;
		// 1.5 — `.` invalidates both the colour and layer
		// history. The user's intent is "reset all TAA
		// history so the next frame starts fresh"; dropping
		// just the colour would leave the layer history
		// stale and the voxel pass would blend against
		// pre-invalidated data.
		render->taaLayerHistoryValid = false;
	}
	if (ConsumeInputActionPressed(*input, InputAction::DecreaseLightingExposure)) {
		AdjustLightingExposure(*render, -kLightingExposureStepStops);
	}
	if (ConsumeInputActionPressed(*input, InputAction::IncreaseLightingExposure)) {
		AdjustLightingExposure(*render, kLightingExposureStepStops);
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
	if (ConsumeInputActionPressed(*input, InputAction::CycleShadowTuningTarget)) {
		render->lightingDebugControls.shadowTuningTarget =
			GetNextShadowTuningTarget(render->lightingDebugControls.shadowTuningTarget);
	}
	if (ConsumeInputActionPressed(*input, InputAction::DecreaseShadowTuningValue)) {
		AdjustShadowTuning(*render, -1.0f);
	}
	if (ConsumeInputActionPressed(*input, InputAction::IncreaseShadowTuningValue)) {
		AdjustShadowTuning(*render, 1.0f);
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
	UpdateVoxelInteraction(*camera, input, world->voxelWorld.get(), interaction, allowWorldEditing, physics);
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
		debug->stats.worldEditVersion = world->voxelWorld->editVersion;
		debug->stats.scenePreset = world->voxelWorld->scenePreset;
	}
	debug->stats.controlMode = camera->controlMode;
	debug->stats.walkAirControlMode = GetPhysicsWalkAirControlMode(physics);
	debug->stats.detailedHudVisible = debug->detailedHudVisible;
	debug->stats.walkAutoJumpEnabled = IsPhysicsWalkAutoJumpEnabled(physics);
	debug->stats.walkAutoJumpDelayEnabled = IsPhysicsWalkAutoJumpDelayEnabled(physics);
	debug->stats.simulationPaused = simulation->paused;
	debug->stats.showChunkBounds = debug->showChunkBounds;
	debug->stats.showDirtyChunkOverlay = debug->showDirtyChunkOverlay;
	debug->stats.sceneExposure = render->currentSceneLighting.postProcess[0];
	debug->stats.sceneEnvironmentIntensity = render->currentSceneLighting.postProcess[1];
	debug->stats.sceneColorGradeWhitePoint = render->currentSceneLighting.colorGrading[0];
	debug->stats.sceneColorGradeContrast = render->currentSceneLighting.colorGrading[1];
	debug->stats.sceneColorGradeSaturation = render->currentSceneLighting.colorGrading[2];
	debug->stats.sceneColorGradeLift = render->currentSceneLighting.colorGrading[3];
	debug->stats.sceneExposureMeteringMode =
		static_cast<ExposureMeteringMode>(std::lround(render->currentSceneLighting.exposureControl[0]));
	debug->stats.sceneExposureKey = EstimateVoxelSceneExposureKey(render->currentSceneLighting);
	debug->stats.sceneExposureTargetKey = render->currentSceneLighting.exposureControl[1];
	debug->stats.sceneMinExposure = render->currentSceneLighting.exposureControl[2];
	debug->stats.sceneMaxExposure = render->currentSceneLighting.exposureControl[3];
	debug->stats.toneMapOperator = render->lightingDebugControls.toneMapOperator;
	debug->stats.lightingDebugView = render->lightingDebugControls.debugView;
	debug->stats.sunDirection = {
		render->currentSceneLighting.sunDirectionAndWrap[0],
		render->currentSceneLighting.sunDirectionAndWrap[1],
		render->currentSceneLighting.sunDirectionAndWrap[2],
	};
	debug->stats.sunIntensity = render->currentSceneLighting.sunColorAndIntensity[3];
	debug->stats.sunShadowStrength = render->currentSceneLighting.sunShadowParams[0];
	debug->stats.sunShadowDepthBias = render->currentSceneLighting.sunShadowParams[1];
	debug->stats.sunShadowNormalBias = render->currentSceneLighting.sunShadowParams[2];
	debug->stats.sunShadowFilterRadius = render->currentSceneLighting.sunShadowParams[3];
	debug->stats.sunContactShadowStrength = render->currentSceneLighting.sunContactShadowParams[0];
	debug->stats.sunContactShadowDistance = render->currentSceneLighting.sunContactShadowParams[1];
	debug->stats.ambientOcclusionStrength = render->currentSceneLighting.ambientOcclusionParams[0];
	debug->stats.ambientOcclusionRadius = render->currentSceneLighting.ambientOcclusionParams[1];
	debug->stats.ambientOcclusionMinVisibility = render->currentSceneLighting.ambientOcclusionParams[2];
	debug->stats.localPointLightPosition = {
		render->currentSceneLighting.localPointLightPositionAndRadius[0],
		render->currentSceneLighting.localPointLightPositionAndRadius[1],
		render->currentSceneLighting.localPointLightPositionAndRadius[2],
	};
	debug->stats.localPointLightColor = {
		render->currentSceneLighting.localPointLightColorAndIntensity[0],
		render->currentSceneLighting.localPointLightColorAndIntensity[1],
		render->currentSceneLighting.localPointLightColorAndIntensity[2],
	};
	debug->stats.localPointLightRadius = render->currentSceneLighting.localPointLightPositionAndRadius[3];
	debug->stats.localPointLightIntensity = render->currentSceneLighting.localPointLightColorAndIntensity[3];
	debug->stats.localPointLightEnabled = render->currentSceneLighting.localPointLightParams[0];
	debug->stats.localPointLightSourceRadius = render->currentSceneLighting.localPointLightParams[1];
	debug->stats.localPointLightShadowStrength = render->currentSceneLighting.localPointLightParams[2];
	debug->stats.localPointLightShadowBias = render->currentSceneLighting.localPointLightParams[3];
	debug->stats.sunShadowCoverageScale = render->lightingDebugControls.shadowCoverageScale;
	debug->stats.sunShadowCascadeBlend = render->currentSceneLighting.shadowCascadeBlendParams[0];
	const float sunShadowReceiverMaxDistance = GetCameraVisibleSceneMaxDistance(*camera);
	render->currentSunShadowCascadeSplits = BuildSunShadowCascadeSplits(
		camera->nearPlane,
		sunShadowReceiverMaxDistance,
		render->sunShadowCascadeSplitLambda);
	debug->stats.sunShadowCascadeSplitLambda = render->currentSunShadowCascadeSplits.splitLambda;
	debug->stats.sunShadowCascadeDepthSplits = render->currentSunShadowCascadeSplits.viewDepthSplits;
	debug->stats.sunShadowCascadeDiagnostics = render->currentSunShadowCascadeDiagnostics;
	debug->stats.shadowMapResolution = render->shadowMapExtent.width;
	debug->stats.transparentShadowPolicy = render->transparentShadowPolicy;
	debug->stats.shadowTuningTarget = render->lightingDebugControls.shadowTuningTarget;
	debug->stats.taaEnabled = render->taaEnabled;
	debug->stats.taaBlend = render->taaBlend;
	debug->stats.taaFrameCounter = render->taaFrameCounter;
	debug->stats.taaHistoryValid = render->taaHistoryValid;
	debug->stats.taaJitterX = render->taaJitterX;
	debug->stats.taaJitterY = render->taaJitterY;
	debug->stats.taaJitterScale = render->taaJitterScale;
	debug->stats.taaNeighbourhoodRadius = render->taaNeighbourhoodRadius;
	debug->stats.taaCasSharpnessMax = render->taaCasSharpnessMax;
	debug->stats.taaCameraCutCount = render->taaCameraCutCount;
	debug->stats.taaCameraCutMaxDelta = render->taaCameraCutMaxDelta;
	// 1.5 anti-flicker mirror. `taaLayerHistoryValid` is reset by
	// the same triggers as the colour history (`taaHistoryValid`)
	// — swapchain recreate, world reload, Taa toggle, jitter
	// scale, blend, neighbourhood radius, `.` invalidate, and
	// the 1.2 camera-cut detector — so the per-frame DebugStats
	// mirror reflects the same flag the voxel pass will sample.
	debug->stats.taaLayerHistoryValid = render->taaLayerHistoryValid;
	debug->stats.taaLayerBlendFactor = render->taaLayerBlendFactor;
	const PhysicsWalkDebugInfo walkDebugInfo = GetPhysicsWalkDebugInfo(physics);
	debug->stats.walkDebugValid = walkDebugInfo.valid;
	debug->stats.walkSupportState = static_cast<uint8_t>(walkDebugInfo.supportState);
	debug->stats.walkFeetPosition = walkDebugInfo.feetPosition;
	debug->stats.walkFootSupportScore = walkDebugInfo.footSupportScore;
	debug->stats.walkFootSupportHitSamples = walkDebugInfo.footSupportHitSamples;
	debug->stats.walkFootSupportTotalSamples = walkDebugInfo.footSupportTotalSamples;
	debug->stats.walkEdgeGraceFramesRemaining = walkDebugInfo.edgeGraceFramesRemaining;
	debug->stats.walkGroundTakeoffGraceFramesRemaining = walkDebugInfo.groundTakeoffGraceFramesRemaining;
	debug->stats.walkSneakSupportGraceFramesRemaining = walkDebugInfo.sneakSupportGraceFramesRemaining;
	debug->stats.walkLedgeReleaseGraceFramesRemaining = walkDebugInfo.ledgeReleaseGraceFramesRemaining;
	debug->stats.walkAutoJumpDelayFramesRemaining = walkDebugInfo.autoJumpDelayFramesRemaining;
	debug->stats.walkGroundTakeoffCached = walkDebugInfo.groundTakeoffCached;
	debug->stats.walkSneakActive = walkDebugInfo.sneakActive;
	debug->stats.walkJumpLockActive = walkDebugInfo.jumpLockActive;
	debug->stats.walkSuppressPassiveSlide = walkDebugInfo.suppressPassiveSlide;
	debug->stats.inputReplayRecording = input->replay.recording;
	debug->stats.inputReplayPlaybackActive = input->replay.playbackActive;
	debug->stats.inputReplayReady = input->replay.captureAvailable;
	debug->stats.inputReplayFrameCount = static_cast<uint32_t>(input->replay.capture.frames.size());
	debug->stats.inputReplayPlaybackFrameIndex = static_cast<uint32_t>(input->replay.playbackFrameIndex);

	return true;
}

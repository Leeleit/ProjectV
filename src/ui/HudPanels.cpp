#include "ui/HudPanels.hpp"

#include "app/AppUpdate.hpp"
#include "app/InputActions.hpp"
#include "audio/AudioEngine.hpp"
#include "physics/PhysicsWorld.hpp"
#include "render/AaPass.hpp"
#include "render/AntialiasingSettings.hpp"
#include "render/vulkan/VulkanSwapchain.hpp"
#include "voxel/VoxelMaterials.hpp"
#include "voxel/VoxelWorld.hpp"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace projectv::ui {
namespace {

void QueueAction(InputState &input, const InputAction action)
{
	input.actions[static_cast<size_t>(action)].pressed = true;
}

const char *ControlModeLabel(const CameraState::ControlMode mode)
{
	switch (mode) {
	case CameraState::ControlMode::Creative:
		return "Creative";
	case CameraState::ControlMode::Spectator:
		return "Spectator";
	case CameraState::ControlMode::Walk:
		return "Survival";
	}
	return "Creative";
}

const char *EditorToolLabel(const DebugEditorTool tool)
{
	switch (tool) {
	case DebugEditorTool::Classic:
		return "Classic";
	case DebugEditorTool::Paint:
		return "Paint";
	case DebugEditorTool::Erase:
		return "Erase";
	case DebugEditorTool::Fill:
		return "Fill";
	case DebugEditorTool::Inspect:
		return "Inspect";
	}
	return "Classic";
}

const char *PlacementMaterialLabel(const VoxelMaterial material)
{
	switch (material) {
	case VoxelMaterial::Air:
		return "Air";
	case VoxelMaterial::Glass:
		return "Glass";
	case VoxelMaterial::Fluid:
		return "Fluid";
	case VoxelMaterial::FloorWhite:
		return "White";
	case VoxelMaterial::FloorGray:
		return "Gray";
	}
	return "White";
}

const char *PresentModeLabel(const VkPresentModeKHR mode)
{
	switch (mode) {
	case VK_PRESENT_MODE_IMMEDIATE_KHR:
		return "Immediate";
	case VK_PRESENT_MODE_MAILBOX_KHR:
		return "Mailbox";
	case VK_PRESENT_MODE_FIFO_KHR:
		return "FIFO";
	default:
		return "Unknown";
	}
}

void DrawStatusStrip(const HudFrameContext &ctx)
{
	const DebugStats &stats = ctx.debug->stats;
	const ImGuiViewport *viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 8.0f, viewport->WorkPos.y + 8.0f), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.72f);
	const ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
	if (!ImGui::Begin("##HudStatusStrip", nullptr, flags)) {
		ImGui::End();
		return;
	}
	ImGui::TextColored(
		ImVec4(0.20f, 0.78f, 0.72f, 1.0f),
		"FPS %.1f  %.2f ms",
		stats.framesPerSecond,
		stats.frameTimeMilliseconds);
	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	ImGui::Text(
		"%s  %s  %s  %s",
		ControlModeLabel(stats.controlMode),
		stats.simulationPaused ? "PAUSE" : "RUN",
		EditorToolLabel(ctx.interaction->editorTool),
		PlacementMaterialLabel(ctx.interaction->placementMaterial));
	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	ImGui::Text(
		"AA %s  SMAA %s  x%s",
		std::string{projectv::render::ToString(stats.msaaMode)}.c_str(),
		stats.smaaEnabled ? "ON" : "OFF",
		std::string{projectv::render::ToString(stats.renderScaleMode)}.c_str());
	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	ImGui::TextDisabled("` Settings · F1 Hide · F2 Mat · Tab Mouse · F5 Rec · F6 Replay");
	ImGui::End();
}

void DrawSettings(HudFrameContext &ctx)
{
	if (!ctx.debug->settingsOpen) {
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(420.0f, 560.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Settings", &ctx.debug->settingsOpen)) {
		ImGui::End();
		return;
	}

	ImGui::Checkbox("Show Stats panel", &ctx.debug->statsOpen);
	ImGui::SeparatorText("Display / AA");
	{
		int msaa = static_cast<int>(ctx.render->msaaMode);
		const char *msaaItems[] = {"Off", "MSAA 2x", "MSAA 4x"};
		if (ImGui::Combo("MSAA", &msaa, msaaItems, IM_ARRAYSIZE(msaaItems))) {
			ctx.render->msaaMode = static_cast<projectv::render::MsaaMode>(msaa);
			ctx.render->aaPipelinesNeedRecreate = true;
			projectv::render::InvalidateProgressiveAccum(*ctx.render);
			ctx.platform->windowResized = true;
			PersistAaSettingsToSceneConfig(*ctx.render);
		}
		if (ImGui::Checkbox("SMAA", &ctx.render->smaaEnabled)) {
			projectv::render::InvalidateProgressiveAccum(*ctx.render);
			PersistAaSettingsToSceneConfig(*ctx.render);
		}
		int scale = static_cast<int>(ctx.render->renderScaleMode);
		const char *scaleItems[] = {"1.00", "1.25", "1.50"};
		if (ImGui::Combo("Render scale", &scale, scaleItems, IM_ARRAYSIZE(scaleItems))) {
			ctx.render->renderScaleMode = static_cast<projectv::render::RenderScaleMode>(scale);
			projectv::render::InvalidateProgressiveAccum(*ctx.render);
			ctx.platform->windowResized = true;
			PersistAaSettingsToSceneConfig(*ctx.render);
		}
		ImGui::Text(
			"Present: %s (%zu/%zu)",
			PresentModeLabel(GetActivePresentMode()),
			GetPresentModeCycleIndex(GetActivePresentMode()) + 1u,
			GetPresentModeCycleSize());
		if (ImGui::Button("Cycle present mode")) {
			ctx.debug->requestPresentModeCycle = true;
		}
	}

	ImGui::SeparatorText("Overlays");
	ImGui::Checkbox("Chunk bounds", &ctx.debug->showChunkBounds);
	ImGui::Checkbox("Dirty chunks", &ctx.debug->showDirtyChunkOverlay);
	ImGui::Checkbox("Cursor hit normal", &ctx.debug->showCursorHitNormal);

	ImGui::SeparatorText("Lighting");
	if (ImGui::Button("Cycle debug view")) {
		QueueAction(*ctx.input, InputAction::CycleLightingDebugView);
	}
	ImGui::SameLine();
	if (ImGui::Button("Cycle tonemap")) {
		QueueAction(*ctx.input, InputAction::CycleToneMapOperator);
	}
	if (ImGui::Button("Exposure -")) {
		QueueAction(*ctx.input, InputAction::DecreaseLightingExposure);
	}
	ImGui::SameLine();
	if (ImGui::Button("Exposure +")) {
		QueueAction(*ctx.input, InputAction::IncreaseLightingExposure);
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset lighting")) {
		QueueAction(*ctx.input, InputAction::ResetLightingDebugControls);
	}
	ImGui::Text("Exposure bias: %.2f", ctx.render->lightingDebugControls.exposureBiasStops);

	ImGui::SeparatorText("Survival");
	if (ImGui::Button("Cycle air control")) {
		QueueAction(*ctx.input, InputAction::ToggleWalkAirControlMode);
	}
	bool autoJump = IsPhysicsWalkAutoJumpEnabled(ctx.physics);
	if (ImGui::Checkbox("Auto-jump", &autoJump)) {
		SetPhysicsWalkAutoJumpEnabled(ctx.physics, autoJump);
	}

	ImGui::SeparatorText("World");
	bool paused = ctx.simulation->paused;
	if (ImGui::Checkbox("Pause", &paused)) {
		ctx.simulation->paused = paused;
		ctx.simulation->simulationAccumulatorSeconds = 0.0f;
	}
	if (ImGui::Button("Frame step")) {
		ctx.simulation->frameStepRequested = true;
	}
	ImGui::SliderFloat("Time scale", &ctx.simulation->timeScale, 0.0f, 4.0f, "%.2f");
	if (ImGui::Button("Reset time scale")) {
		ctx.simulation->timeScale = 1.0f;
	}
	if (ImGui::Button("Cycle scene preset")) {
		QueueAction(*ctx.input, InputAction::CycleScenePreset);
	}
	ImGui::SameLine();
	if (ImGui::Button("Save snapshot")) {
		QueueAction(*ctx.input, InputAction::SaveWorldSnapshot);
	}
	ImGui::SameLine();
	if (ImGui::Button("Load snapshot")) {
		QueueAction(*ctx.input, InputAction::LoadWorldSnapshot);
	}
	if (ImGui::Button("Cycle control mode")) {
		QueueAction(*ctx.input, InputAction::ToggleControlMode);
	}
	ImGui::SameLine();
	if (ImGui::Button("Creative / Survival")) {
		QueueAction(*ctx.input, InputAction::ToggleWalkCreativeMode);
	}

	ImGui::SeparatorText("Editor");
	if (ImGui::Button("Cycle tool")) {
		QueueAction(*ctx.input, InputAction::CycleEditorTool);
	}
	ImGui::SameLine();
	if (ImGui::Button("Cycle material (F2)")) {
		QueueAction(*ctx.input, InputAction::CyclePlacementMaterial);
	}
	if (ImGui::Button("Toggle mutation anchor")) {
		QueueAction(*ctx.input, InputAction::ToggleMutationAnchor);
	}
	ImGui::SameLine();
	if (ImGui::Button("Pick material")) {
		QueueAction(*ctx.input, InputAction::PickTargetMaterial);
	}
	ImGui::SameLine();
	if (ImGui::Button("Pick model")) {
		QueueAction(*ctx.input, InputAction::PickModel);
	}

	ImGui::SeparatorText("Music");
	if (ctx.audio) {
		if (ImGui::Button("Play/Pause")) {
			ctx.audio->togglePlayPause();
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop")) {
			ctx.audio->stop();
		}
		if (ImGui::Button("Vol -")) {
			ctx.audio->decreaseVolume(0.05f);
		}
		ImGui::SameLine();
		if (ImGui::Button("Vol +")) {
			ctx.audio->increaseVolume(0.05f);
		}
		ImGui::SameLine();
		if (ImGui::Button("Prev")) {
			ctx.audio->previousTrack();
		}
		ImGui::SameLine();
		if (ImGui::Button("Next")) {
			ctx.audio->nextTrack();
		}
		ImGui::Text("Volume %.2f", ctx.audio->volume());
	} else {
		ImGui::TextDisabled("Audio unavailable");
	}

	ImGui::SeparatorText("Dev");
	if (ImGui::Button("Screenshot")) {
		QueueAction(*ctx.input, InputAction::CaptureScreenshot);
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset camera")) {
		QueueAction(*ctx.input, InputAction::ResetCamera);
	}
	if (ImGui::Button("Reload shaders")) {
		ctx.debug->requestShaderReload = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Quit")) {
		ctx.debug->requestQuit = true;
	}

	ImGui::End();
}

void DrawStats(const HudFrameContext &ctx)
{
	if (!ctx.debug->statsOpen) {
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(380.0f, 480.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Stats", &ctx.debug->statsOpen)) {
		ImGui::End();
		return;
	}

	const DebugStats &stats = ctx.debug->stats;
	const CameraState &camera = *ctx.camera;
	const InteractionState &interaction = *ctx.interaction;

	ImGui::Text("FPS %.1f  MS %.2f", stats.framesPerSecond, stats.frameTimeMilliseconds);
	ImGui::Text(
		"VSync %s (%zu/%zu)",
		PresentModeLabel(GetActivePresentMode()),
		GetPresentModeCycleIndex(GetActivePresentMode()) + 1u,
		GetPresentModeCycleSize());
	ImGui::Text("Scene %s", std::string{VoxelScenePresetToString(stats.scenePreset)}.c_str());
	ImGui::Text(
		"LGT %s  %s  %.2f",
		LightingDebugViewToString(stats.lightingDebugView),
		ToneMapOperatorToString(stats.toneMapOperator),
		stats.sceneExposure);
	ImGui::Text(
		"AA %s SMAA %s SCALE %s ACCUM %u/%u",
		std::string{projectv::render::ToString(stats.msaaMode)}.c_str(),
		stats.smaaEnabled ? "ON" : "OFF",
		std::string{projectv::render::ToString(stats.renderScaleMode)}.c_str(),
		stats.progressiveAccumFrameIndex,
		projectv::render::kProgressiveAccumMaxFrames);
	ImGui::Text(
		"MODE %s  PAUSE %s",
		ControlModeLabel(stats.controlMode),
		stats.simulationPaused ? "ON" : "OFF");
	ImGui::Text("TIME %.2f", stats.simulationTimeScale);
	ImGui::Text("SIM %u  TRI %u", stats.simulationStepsLastFrame, stats.sceneTriangleCount);
	ImGui::Text("DIRTY %u  ACT %u", stats.dirtyChunkCount, stats.activeChunkCount);
	ImGui::Text(
		"VOX %u  MEM %.1f KiB  VER %llu",
		stats.nonAirVoxelCount,
		static_cast<double>(stats.sceneMemoryBytes) / 1024.0,
		static_cast<unsigned long long>(stats.worldEditVersion));
	ImGui::Text(
		"EDIT %s  MAT %s  BND %s  DIRTY %s",
		EditorToolLabel(interaction.editorTool),
		PlacementMaterialLabel(interaction.placementMaterial),
		stats.showChunkBounds ? "ON" : "OFF",
		stats.showDirtyChunkOverlay ? "ON" : "OFF");
	ImGui::Text("CAM %.3f %.3f %.3f", camera.position[0], camera.position[1], camera.position[2]);
	ImGui::Separator();
	ImGui::Text(
		"RPASS GFX %.2f  OTH %.2f",
		stats.renderPassGraphicsMs,
		stats.renderPassOtherMs);
	ImGui::Text(
		"SHAD %.2f MES %.2f OVL %.2f",
		stats.renderPassShadowMs,
		stats.renderPassMeshingMs,
		stats.renderPassDebugOverlayMs);
	ImGui::Text(
		"SUN %.2f %.2f %.2f  I %.2f",
		stats.sunDirection[0],
		stats.sunDirection[1],
		stats.sunDirection[2],
		stats.sunIntensity);
	if (interaction.selection.hasHit) {
		ImGui::Text(
			"HIT %d %d %d  n %d %d %d",
			interaction.selection.targetVoxel.x,
			interaction.selection.targetVoxel.y,
			interaction.selection.targetVoxel.z,
			interaction.selection.hitNormal.x,
			interaction.selection.hitNormal.y,
			interaction.selection.hitNormal.z);
	} else {
		ImGui::TextDisabled("HIT none");
	}
	if (stats.inputReplayReady || stats.inputReplayRecording || stats.inputReplayPlaybackActive) {
		ImGui::Text(
			"REP rec=%d play=%d frames=%u idx=%u",
			stats.inputReplayRecording ? 1 : 0,
			stats.inputReplayPlaybackActive ? 1 : 0,
			stats.inputReplayFrameCount,
			stats.inputReplayPlaybackFrameIndex);
	}
	ImGui::End();
}

} // namespace

void DrawHudFrame(HudFrameContext &ctx)
{
	if (!ctx.debug || !ctx.debug->hudVisible) {
		return;
	}
	DrawStatusStrip(ctx);
	DrawSettings(ctx);
	DrawStats(ctx);
}

} // namespace projectv::ui

#include "debug/DebugHud.hpp"

#include "app/Camera.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string_view>

namespace {
constexpr float kPanelPaddingPx = 8.0f;
constexpr float kPanelOriginXPx = 12.0f;
constexpr float kPanelOriginYPx = 12.0f;
constexpr float kPanelGapPx = 8.0f;
constexpr float kPanelShadowOffsetPx = 3.0f;
constexpr float kAccentStripHeightPx = 3.0f;
constexpr float kTextShadowOffsetPx = 1.0f;
constexpr float kGlyphPixelSizePx = 1.75f;
constexpr float kGlyphAdvancePx = 10.0f;
constexpr float kLineAdvancePx = 14.0f;
constexpr float kGlyphWidthPx = 5.0f * kGlyphPixelSizePx;
constexpr float kGlyphHeightPx = 7.0f * kGlyphPixelSizePx;
constexpr float kStatsPanelMinWidthPx = 276.0f;
constexpr float kHelperPanelMinWidthPx = 244.0f;
constexpr size_t kHudLineBufferSize = 96;
constexpr size_t kMaxStatsLineCount = 38;
constexpr size_t kMaxHelperLineCount = 16;

std::array<uint8_t, 7> GetGlyphRows(const char character)
{
	switch (character) {
	case 'A':
		return {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
	case 'B':
		return {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
	case 'C':
		return {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
	case 'D':
		return {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C};
	case 'E':
		return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
	case 'F':
		return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
	case 'G':
		return {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E};
	case 'H':
		return {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
	case 'I':
		return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F};
	case 'J':
		return {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E};
	case 'K':
		return {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
	case 'L':
		return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
	case 'M':
		return {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
	case 'N':
		return {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
	case 'O':
		return {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
	case 'P':
		return {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
	case 'R':
		return {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
	case 'S':
		return {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
	case 'T':
		return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
	case 'U':
		return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
	case 'V':
		return {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
	case 'W':
		return {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A};
	case 'X':
		return {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
	case 'Y':
		return {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
	case '0':
		return {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
	case '1':
		return {0x04, 0x0C, 0x14, 0x04, 0x04, 0x04, 0x1F};
	case '2':
		return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
	case '3':
		return {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
	case '4':
		return {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
	case '5':
		return {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
	case '6':
		return {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E};
	case '7':
		return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
	case '8':
		return {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
	case '9':
		return {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E};
	case '.':
		return {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
	case '-':
		return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
	case ':':
		return {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00};
	default:
		return {};
	}
}

float PixelToNdcX(const float pixelX, const float width)
{
	return pixelX / width * 2.0f - 1.0f;
}

float PixelToNdcY(const float pixelY, const float height)
{
	return pixelY / height * 2.0f - 1.0f;
}

void AppendQuad(
	DebugHudVertex *outVertices,
	uint32_t &vertexCount,
	const uint32_t maxVertexCount,
	const VkExtent2D extent,
	const float minXPx,
	const float minYPx,
	const float maxXPx,
	const float maxYPx,
	const std::array<float, 4> &color)
{
	if (vertexCount + 6u > maxVertexCount) {
		return;
	}

	const float width = static_cast<float>(extent.width);
	const float height = static_cast<float>(extent.height);
	const DebugHudVertex quadVertices[6]{
		{{PixelToNdcX(minXPx, width), PixelToNdcY(minYPx, height)}, color},
		{{PixelToNdcX(maxXPx, width), PixelToNdcY(minYPx, height)}, color},
		{{PixelToNdcX(maxXPx, width), PixelToNdcY(maxYPx, height)}, color},
		{{PixelToNdcX(minXPx, width), PixelToNdcY(minYPx, height)}, color},
		{{PixelToNdcX(maxXPx, width), PixelToNdcY(maxYPx, height)}, color},
		{{PixelToNdcX(minXPx, width), PixelToNdcY(maxYPx, height)}, color},
	};

	std::copy_n(quadVertices, 6u, outVertices + vertexCount);
	vertexCount += 6;
}

void AppendTextLine(
	DebugHudVertex *outVertices,
	uint32_t &vertexCount,
	const uint32_t maxVertexCount,
	const VkExtent2D extent,
	const float originYPx,
	const std::string_view text,
	const std::array<float, 4> &color,
	const float xOffsetPx = 0.0f)
{
	float cursorXPx = kPanelOriginXPx + kPanelPaddingPx + xOffsetPx;
	for (const char character : text) {
		if (character == ' ') {
			cursorXPx += kGlyphAdvancePx;
			continue;
		}

		const std::array<uint8_t, 7> rows = GetGlyphRows(character);
		for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
			const uint8_t row = rows[rowIndex];
			for (int column = 0; column < 5; ++column) {
				const uint8_t mask = static_cast<uint8_t>(1u << (4 - column));
				if ((row & mask) == 0) {
					continue;
				}

				const float pixelMinX = cursorXPx + static_cast<float>(column) * kGlyphPixelSizePx;
				const float pixelMinY = originYPx + static_cast<float>(rowIndex) * kGlyphPixelSizePx;
				AppendQuad(
					outVertices,
					vertexCount,
					maxVertexCount,
					extent,
					pixelMinX,
					pixelMinY,
					pixelMinX + kGlyphPixelSizePx,
					pixelMinY + kGlyphPixelSizePx,
					color);
			}
		}

		cursorXPx += kGlyphAdvancePx;
	}
}

void AppendShadowedTextLine(
	DebugHudVertex *outVertices,
	uint32_t &vertexCount,
	const uint32_t maxVertexCount,
	const VkExtent2D extent,
	const float originYPx,
	const std::string_view text,
	const std::array<float, 4> &color)
{
	AppendTextLine(
		outVertices,
		vertexCount,
		maxVertexCount,
		extent,
		originYPx + kTextShadowOffsetPx,
		text,
		{0.0f, 0.0f, 0.0f, color[3] * 0.65f},
		kTextShadowOffsetPx);
	AppendTextLine(
		outVertices,
		vertexCount,
		maxVertexCount,
		extent,
		originYPx,
		text,
		color);
}

void AppendPanel(
	DebugHudVertex *outVertices,
	uint32_t &vertexCount,
	const uint32_t maxVertexCount,
	const VkExtent2D extent,
	const float minYPx,
	const float panelWidthPx,
	const float panelHeightPx,
	const std::array<float, 4> &panelColor,
	const std::array<float, 4> &accentColor)
{
	constexpr float minXPx = kPanelOriginXPx;
	const float maxXPx = minXPx + panelWidthPx;
	const float maxYPx = minYPx + panelHeightPx;
	AppendQuad(
		outVertices,
		vertexCount,
		maxVertexCount,
		extent,
		minXPx + kPanelShadowOffsetPx,
		minYPx + kPanelShadowOffsetPx,
		maxXPx + kPanelShadowOffsetPx,
		maxYPx + kPanelShadowOffsetPx,
		{0.0f, 0.0f, 0.0f, 0.18f});
	AppendQuad(
		outVertices,
		vertexCount,
		maxVertexCount,
		extent,
		minXPx,
		minYPx,
		maxXPx,
		maxYPx,
		panelColor);
	AppendQuad(
		outVertices,
		vertexCount,
		maxVertexCount,
		extent,
		minXPx,
		minYPx,
		maxXPx,
		minYPx + kAccentStripHeightPx,
		accentColor);
}

float MeasureTextWidthPx(const std::string_view text)
{
	float widthPx = 0.0f;
	float cursorXPx = 0.0f;
	for (const char character : text) {
		if (character != ' ') {
			widthPx = std::max(widthPx, cursorXPx + kGlyphWidthPx);
		}
		cursorXPx += kGlyphAdvancePx;
	}

	return widthPx;
}

template <size_t TLineCount>
float ComputePanelWidthPx(
	const std::array<std::array<char, kHudLineBufferSize>, TLineCount> &lines,
	const std::string_view title,
	const float minimumWidthPx)
{
	float contentWidthPx = MeasureTextWidthPx(title);
	for (const auto &line : lines) {
		contentWidthPx = std::max(contentWidthPx, MeasureTextWidthPx(line.data()));
	}

	return std::max(
		minimumWidthPx,
		kPanelPaddingPx * 2.0f + contentWidthPx + kTextShadowOffsetPx);
}

template <size_t TLineCount>
char *BeginHudLine(
	std::array<std::array<char, kHudLineBufferSize>, TLineCount> &outLines,
	size_t &lineCount)
{
	if (lineCount >= outLines.size()) {
		return nullptr;
	}

	char *line = outLines.at(lineCount).data();
	line[0] = '\0';
	++lineCount;
	return line;
}

#define PV_APPEND_HUD_LINE(outLines, lineCount, ...)                 \
	do {                                                             \
		if (char *hudLine = BeginHudLine(outLines, lineCount)) {     \
			std::snprintf(hudLine, kHudLineBufferSize, __VA_ARGS__); \
		}                                                            \
	} while (false)

const char *GetVoxelMaterialLabel(const VoxelMaterial material)
{
	switch (material) {
	case VoxelMaterial::Air:
		return "AIR";
	case VoxelMaterial::Glass:
		return "GLASS";
	case VoxelMaterial::Fluid:
		return "FLUID";
	case VoxelMaterial::FloorWhite:
		return "WHITE";
	case VoxelMaterial::FloorGray:
		return "GRAY";
	}

	return "NONE";
}

const char *GetControlModeLabel(const CameraState::ControlMode controlMode)
{
	switch (controlMode) {
	case CameraState::ControlMode::Creative:
		return "CREATIVE";
	case CameraState::ControlMode::Spectator:
		return "SPECTATOR";
	case CameraState::ControlMode::Walk:
		return "WALK";
	}

	return "UNKNOWN";
}

const char *GetScenePresetLabel(const VoxelScenePreset scenePreset)
{
	switch (scenePreset) {
	case VoxelScenePreset::VoxelLab:
		return "VOXELLAB";
	case VoxelScenePreset::FlatBenchmark:
		return "FLATBENCH";
	case VoxelScenePreset::TransparencyStress:
		return "TRANSTRESS";
	case VoxelScenePreset::ChunkGrid:
		return "CHUNKGRID";
	case VoxelScenePreset::MeshingStress:
		return "MESHSTRESS";
	}

	return "VOXELLAB";
}

const char *GetDebugEditorToolLabel(const DebugEditorTool tool)
{
	switch (tool) {
	case DebugEditorTool::Classic:
		return "OFF";
	case DebugEditorTool::Paint:
		return "PAINT";
	case DebugEditorTool::Erase:
		return "ERASE";
	case DebugEditorTool::Fill:
		return "FILL";
	case DebugEditorTool::Inspect:
		return "INSPECT";
	}

	return "OFF";
}

const char *GetBoolLabel(const bool value)
{
	return value ? "ON" : "OFF";
}

const char *GetAnchorKindLabel(const bool usesPlacementVoxel)
{
	return usesPlacementVoxel ? "PLC" : "TGT";
}

const char *GetWalkSupportStateLabel(const uint8_t state)
{
	switch (state) {
	case 1:
		return "GROUND";
	case 2:
		return "EDGE";
	default:
		return "AIR";
	}
}

const char *GetWalkAirControlModeLabel(const WalkAirControlMode mode)
{
	switch (mode) {
	case WalkAirControlMode::MinecraftLike:
		return "MC";
	case WalkAirControlMode::Realistic:
		return "REAL";
	}

	return "MC";
}

size_t BuildStatsLines(
	const DebugStats &stats,
	const CameraState &camera,
	const InteractionState &interaction,
	std::array<std::array<char, kHudLineBufferSize>, kMaxStatsLineCount> &outLines)
{
	const std::array<float, 3> forward = GetCameraForwardVector(camera);
	const bool detailedHudVisible = stats.detailedHudVisible;
	size_t lineCount = 0;

	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"FPS %.1f  MS %.2f",
		stats.framesPerSecond,
		stats.frameTimeMilliseconds);
	PV_APPEND_HUD_LINE(outLines, lineCount, "SCENE %s", GetScenePresetLabel(stats.scenePreset));
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"LGT %s  %s  %.2f",
		LightingDebugViewToString(stats.lightingDebugView),
		ToneMapOperatorToString(stats.toneMapOperator),
		stats.sceneExposure);
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"MODE %s  PAUSE %s  AIR %s",
		GetControlModeLabel(stats.controlMode),
		stats.simulationPaused ? "ON" : "OFF",
		GetWalkAirControlModeLabel(stats.walkAirControlMode));
	// Frame-step / slow-motion line. Default `TIME 1.00`. The
	// `STEP` line is only emitted on the same frame the
	// operator pressed `\` — it is a one-frame indicator for
	// "the next tick is a forced step", not a sticky latched
	// flag. Stays adjacent to the `PAUSE` line above so the
	// two pause-related runtime axes read as a group.
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"TIME %.2f",
		stats.simulationTimeScale);
	if (stats.simulationFrameStepPending) {
		PV_APPEND_HUD_LINE(outLines, lineCount, "STEP");
	}
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"AUTO %s  DELAY %s",
		GetBoolLabel(stats.walkAutoJumpEnabled),
		GetBoolLabel(stats.walkAutoJumpDelayEnabled));
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"SIM %u  TRI %u",
		stats.simulationStepsLastFrame,
		stats.sceneTriangleCount);
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"DIRTY %u  ACT %u",
		stats.dirtyChunkCount,
		stats.activeChunkCount);
	if (detailedHudVisible) {
		PV_APPEND_HUD_LINE(
			outLines,
			lineCount,
			"VOX %u  MEM %.1f  VER %llu",
			stats.nonAirVoxelCount,
			static_cast<double>(stats.sceneMemoryBytes) / 1024.0,
			static_cast<unsigned long long>(stats.worldEditVersion));
	} else {
		PV_APPEND_HUD_LINE(
			outLines,
			lineCount,
			"VOX %u  MEM %.1f",
			stats.nonAirVoxelCount,
			static_cast<double>(stats.sceneMemoryBytes) / 1024.0);
	}
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"EDIT %s  BND %s  DIRTY %s",
		GetDebugEditorToolLabel(interaction.editorTool),
		GetBoolLabel(stats.showChunkBounds),
		GetBoolLabel(stats.showDirtyChunkOverlay));
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"CAM %.3f %.3f %.3f",
		camera.position[0],
		camera.position[1],
		camera.position[2]);

	if (stats.walkDebugValid) {
		PV_APPEND_HUD_LINE(
			outLines,
			lineCount,
			"FEET %.3f %.3f %.3f",
			stats.walkFeetPosition[0],
			stats.walkFeetPosition[1],
			stats.walkFeetPosition[2]);
	} else {
		PV_APPEND_HUD_LINE(outLines, lineCount, "FEET NONE");
	}

	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"LOOK %.2f %.2f %.2f",
		forward[0],
		forward[1],
		forward[2]);

	if (stats.walkDebugValid) {
		PV_APPEND_HUD_LINE(
			outLines,
			lineCount,
			"SUP %s %.3f %u %u",
			GetWalkSupportStateLabel(stats.walkSupportState),
			stats.walkFootSupportScore,
			stats.walkFootSupportHitSamples,
			stats.walkFootSupportTotalSamples);
	} else {
		PV_APPEND_HUD_LINE(outLines, lineCount, "SUP NONE");
	}

	if (!detailedHudVisible) {
		return lineCount;
	}

	// Per-pass CPU timing lines (2026-06-12). Two-line
	// format: first line shows the total graphics time
	// (`GFX`) and the unaccounted-for slice (`OTH` =
	// `frameTimeMs - GFX`, i.e. SDL events, scene upload,
	// AppUpdate itself). Second line shows the breakdown
	// of sub-passes inside `RecordGraphicsCommands` so the
	// operator can see which sub-pass is dominating. The
	// `CHNK N` tail mirrors the per-frame dirty-chunk
	// count that was actually requested for re-meshing —
	// useful for the TODO §4.5 perf-budget analysis
	// ("halve-res AO/contact upscale" needs to know which
	// pass is the bottleneck, not just the total). Lives
	// in the detailed-only section because the per-pass
	// breakdown is diagnostic data — the basic HUD keeps
	// the high-level `frameTimeMilliseconds` line and
	// stops there, the detailed HUD shows where the budget
	// is going.
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"RPASS GFX %.2f  OTH %.2f ms",
		stats.renderPassGraphicsMs,
		stats.renderPassOtherMs);
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"RPASS SHAD %.2f MES %.2f TAA %.2f OVL %.2f HUD %.2f CHNK %u",
		stats.renderPassShadowMs,
		stats.renderPassMeshingMs,
		stats.renderPassTaaResolveMs,
		stats.renderPassDebugOverlayMs,
		stats.renderPassDebugHudMs,
		stats.renderPassDirtyChunkRebuiltCount);

	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"SUN %.2f %.2f %.2f %.2f",
		stats.sunDirection[0],
		stats.sunDirection[1],
		stats.sunDirection[2],
		stats.sunIntensity);
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"ENV %.2f",
		stats.sceneEnvironmentIntensity);
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"GRD WP %.2f CON %.2f SAT %.2f LFT %.2f",
		stats.sceneColorGradeWhitePoint,
		stats.sceneColorGradeContrast,
		stats.sceneColorGradeSaturation,
		stats.sceneColorGradeLift);
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"EXP %s KEY %.2f TGT %.2f RNG %.2f %.2f",
		ExposureMeteringModeToString(stats.sceneExposureMeteringMode),
		stats.sceneExposureKey,
		stats.sceneExposureTargetKey,
		stats.sceneMinExposure,
		stats.sceneMaxExposure);
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"SHDW %u STR %.2f FLT %.2f",
		stats.shadowMapResolution,
		stats.sunShadowStrength,
		stats.sunShadowFilterRadius);
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"BIAS %.4f NRM %.4f",
		stats.sunShadowDepthBias,
		stats.sunShadowNormalBias);
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"CTSH STR %.2f DST %.2f",
		stats.sunContactShadowStrength,
		stats.sunContactShadowDistance);
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"AOCC STR %.2f RAD %.2f MIN %.2f",
		stats.ambientOcclusionStrength,
		stats.ambientOcclusionRadius,
		stats.ambientOcclusionMinVisibility);
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"LOCL %.1f %.1f %.1f R %.1f E %.0f",
		stats.localPointLightPosition[0],
		stats.localPointLightPosition[1],
		stats.localPointLightPosition[2],
		stats.localPointLightRadius,
		stats.localPointLightEnabled);
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"LCLR %.2f %.2f %.2f I %.2f S %.2f",
		stats.localPointLightColor[0],
		stats.localPointLightColor[1],
		stats.localPointLightColor[2],
		stats.localPointLightIntensity,
		stats.localPointLightSourceRadius);
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"LSHD STR %.2f B %.3f",
		stats.localPointLightShadowStrength,
		stats.localPointLightShadowBias);
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"COV %.2f BLD %.2f TUNE %s",
		stats.sunShadowCoverageScale,
		stats.sunShadowCascadeBlend,
		ShadowTuningTargetToString(stats.shadowTuningTarget));
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"CSM %u L %.2f %.1f %.1f %.1f %.1f",
		kSunShadowCascadeCount,
		stats.sunShadowCascadeSplitLambda,
		stats.sunShadowCascadeDepthSplits[0],
		stats.sunShadowCascadeDepthSplits[1],
		stats.sunShadowCascadeDepthSplits[2],
		stats.sunShadowCascadeDepthSplits[3]);
	for (uint32_t cascadeIndex = 0; cascadeIndex < kSunShadowCascadeCount; ++cascadeIndex) {
		PV_APPEND_HUD_LINE(
			outLines,
			lineCount,
			"C%u VD %.1f %.1f EXT %.1f %.1f TX %.4f CD %.1f %.1f",
			cascadeIndex,
			stats.sunShadowCascadeDiagnostics.viewNearDepths[cascadeIndex],
			stats.sunShadowCascadeDiagnostics.viewFarDepths[cascadeIndex],
			stats.sunShadowCascadeDiagnostics.orthoWidths[cascadeIndex],
			stats.sunShadowCascadeDiagnostics.orthoHeights[cascadeIndex],
			stats.sunShadowCascadeDiagnostics.texelWorldSizes[cascadeIndex],
			stats.sunShadowCascadeDiagnostics.casterLightNearDepths[cascadeIndex],
			stats.sunShadowCascadeDiagnostics.casterLightFarDepths[cascadeIndex]);
	}
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"TSHD %s",
		TransparentShadowPolicyToString(stats.transparentShadowPolicy));
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"TAA %s JIT %.2f %.2f JSC %.2f BLND %.2f NHOOD %dx%d HIST %s CAS %.2f",
		GetBoolLabel(stats.taaEnabled),
		stats.taaJitterX,
		stats.taaJitterY,
		stats.taaJitterScale,
		stats.taaBlend,
		2 * stats.taaNeighbourhoodRadius + 1,
		2 * stats.taaNeighbourhoodRadius + 1,
		GetBoolLabel(stats.taaHistoryValid),
		stats.taaCasSharpnessMax);
	// 1.5 anti-flicker layer history: `LYR` shows whether the
	// previous-frame layer mask is currently valid (false on
	// the first frame after swapchain-recreate / world-reload /
	// Taa-toggle / etc., true afterwards). `BLF` is the
	// per-frame blend factor for the per-layer temporal filter
	// (default 0.4). Together they tell the operator whether
	// the layer anti-flicker is currently active.
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"TAALYR %s BLF %.2f",
		GetBoolLabel(stats.taaLayerHistoryValid),
		stats.taaLayerBlendFactor);
	// 1.2 camera-cut detector. `CUT` accumulates the number of
	// view-projection discontinuities seen since startup; `CLR`
	// tracks the worst Chebyshev distance so the operator can
	// compare a live repro against `decisions.md` §19's expected
	// delta ranges. Stays on its own line so the TAA summary
	// above stays compact.
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"TAACUT %u CLR %.2f",
		stats.taaCameraCutCount,
		stats.taaCameraCutMaxDelta);
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"FLG TCK %s SNK %s LCK %s PSL %s",
		GetBoolLabel(stats.walkGroundTakeoffCached),
		GetBoolLabel(stats.walkSneakActive),
		GetBoolLabel(stats.walkJumpLockActive),
		GetBoolLabel(stats.walkSuppressPassiveSlide));
	PV_APPEND_HUD_LINE(
		outLines,
		lineCount,
		"GRC %u TGR %u SGR %u LGR %u AJR %u",
		stats.walkEdgeGraceFramesRemaining,
		stats.walkGroundTakeoffGraceFramesRemaining,
		stats.walkSneakSupportGraceFramesRemaining,
		stats.walkLedgeReleaseGraceFramesRemaining,
		stats.walkAutoJumpDelayFramesRemaining);

	if (interaction.selection.hasHit) {
		PV_APPEND_HUD_LINE(
			outLines,
			lineCount,
			"SEL %d %d %d %s %.1f",
			interaction.selection.targetVoxel.x,
			interaction.selection.targetVoxel.y,
			interaction.selection.targetVoxel.z,
			GetVoxelMaterialLabel(interaction.selection.targetMaterial),
			interaction.selection.hitDistance);
	} else {
		PV_APPEND_HUD_LINE(outLines, lineCount, "SEL NONE");
	}

	if (interaction.selection.hasPlacementVoxel) {
		PV_APPEND_HUD_LINE(
			outLines,
			lineCount,
			"PUT %d %d %d %s",
			interaction.selection.placementVoxel.x,
			interaction.selection.placementVoxel.y,
			interaction.selection.placementVoxel.z,
			GetVoxelMaterialLabel(interaction.placementMaterial));
	} else {
		PV_APPEND_HUD_LINE(
			outLines,
			lineCount,
			"PUT NONE %s",
			GetVoxelMaterialLabel(interaction.placementMaterial));
	}

	if (interaction.selection.hasHit) {
		PV_APPEND_HUD_LINE(
			outLines,
			lineCount,
			"LOC %d %d %d  NRM %d %d %d",
			interaction.selection.targetVoxelInChunk.x,
			interaction.selection.targetVoxelInChunk.y,
			interaction.selection.targetVoxelInChunk.z,
			interaction.selection.hitNormal.x,
			interaction.selection.hitNormal.y,
			interaction.selection.hitNormal.z);
	} else {
		PV_APPEND_HUD_LINE(outLines, lineCount, "LOC NONE");
	}

	if (interaction.selection.hasTargetChunk) {
		PV_APPEND_HUD_LINE(
			outLines,
			lineCount,
			"CHK %d %d %d  I %u  C %u  D %d A %d",
			interaction.selection.targetChunkCoord.x,
			interaction.selection.targetChunkCoord.y,
			interaction.selection.targetChunkCoord.z,
			interaction.selection.targetChunkIndex,
			interaction.selection.targetChunkNonAirVoxelCount,
			interaction.selection.targetChunkDirty ? 1 : 0,
			interaction.selection.targetChunkActive ? 1 : 0);
	} else {
		PV_APPEND_HUD_LINE(outLines, lineCount, "CHK NONE");
	}

	if (interaction.selection.hasPlacementChunk) {
		PV_APPEND_HUD_LINE(
			outLines,
			lineCount,
			"PCH %d %d %d  I %u  C %u  D %d A %d",
			interaction.selection.placementChunkCoord.x,
			interaction.selection.placementChunkCoord.y,
			interaction.selection.placementChunkCoord.z,
			interaction.selection.placementChunkIndex,
			interaction.selection.placementChunkNonAirVoxelCount,
			interaction.selection.placementChunkDirty ? 1 : 0,
			interaction.selection.placementChunkActive ? 1 : 0);
	} else {
		PV_APPEND_HUD_LINE(outLines, lineCount, "PCH NONE");
	}

	if (interaction.mutationAnchorValid) {
		PV_APPEND_HUD_LINE(
			outLines,
			lineCount,
			"BOX %s %d %d %d",
			GetAnchorKindLabel(interaction.mutationAnchorUsesPlacementVoxel),
			interaction.mutationAnchorVoxel.x,
			interaction.mutationAnchorVoxel.y,
			interaction.mutationAnchorVoxel.z);
	} else {
		PV_APPEND_HUD_LINE(outLines, lineCount, "BOX NONE");
	}

	if (stats.inputReplayPlaybackActive) {
		PV_APPEND_HUD_LINE(
			outLines,
			lineCount,
			"REP PLAY %u OF %u",
			stats.inputReplayPlaybackFrameIndex,
			stats.inputReplayFrameCount);
	} else if (stats.inputReplayRecording) {
		PV_APPEND_HUD_LINE(
			outLines,
			lineCount,
			"REP REC %u",
			stats.inputReplayFrameCount);
	} else if (stats.inputReplayReady) {
		PV_APPEND_HUD_LINE(
			outLines,
			lineCount,
			"REP READY %u",
			stats.inputReplayFrameCount);
	} else {
		PV_APPEND_HUD_LINE(outLines, lineCount, "REP NONE");
	}

	return lineCount;
}

size_t BuildHelperLines(
	const DebugStats &stats,
	std::array<std::array<char, kHudLineBufferSize>, kMaxHelperLineCount> &outLines)
{
	size_t lineCount = 0;
	PV_APPEND_HUD_LINE(outLines, lineCount, "F1 UI  G DETAIL");
	PV_APPEND_HUD_LINE(outLines, lineCount, "F2 MAT  F3 CAM");
	PV_APPEND_HUD_LINE(outLines, lineCount, "F4 MODE  F5 SCENE");
	PV_APPEND_HUD_LINE(outLines, lineCount, "F6 SAVE  F7 LOAD");
	// M5.1d gravigun: F picks/drops a model under the crosshair
	// and snaps its AABB min to integer voxel grid; logs the
	// final integer coords on drop.
	PV_APPEND_HUD_LINE(outLines, lineCount, "F GRAVIGUN");
	if (stats.detailedHudVisible) {
		PV_APPEND_HUD_LINE(outLines, lineCount, "F8 TOOL  F9 BND");
		PV_APPEND_HUD_LINE(outLines, lineCount, "F10 DIRTY  F11 AIR");
		PV_APPEND_HUD_LINE(outLines, lineCount, "J AUTOJUMP  F12 DELAY");
		PV_APPEND_HUD_LINE(outLines, lineCount, "B VIEW  N TMAP");
		PV_APPEND_HUD_LINE(outLines, lineCount, "H EXP-  K EXPUP  V RESET");
		PV_APPEND_HUD_LINE(outLines, lineCount, "O SHDW  U DEC  I INC");
		PV_APPEND_HUD_LINE(outLines, lineCount, "C SHOT");
		PV_APPEND_HUD_LINE(outLines, lineCount, "R REC  Y PLAY");
		PV_APPEND_HUD_LINE(outLines, lineCount, "X ANCH  M PICK");
		PV_APPEND_HUD_LINE(outLines, lineCount, "T TAA  ;' JIT  -= BLND");
		PV_APPEND_HUD_LINE(outLines, lineCount, ", NHOOD  . INVHIST");
		PV_APPEND_HUD_LINE(outLines, lineCount, "TAB MOUSE  P PAUSE");
		// Frame-step / slow-motion: `[` halves, `]` doubles
		// (clamped 0..4, snapped to 0 below 0.01), `\` queues
		// one fixed tick, `` ` `` resets to 1.0. The bracket
		// and backslash / backtick keys have no glyph in the
		// HUD font (only A-Z, 0-9, `.`, `-`, `:`), so the
		// helper spells them out. The keys themselves are
		// not redefined — the binding stays at the
		// SDL_SCANCODE level in `InputActions.cpp`.
		PV_APPEND_HUD_LINE(outLines, lineCount, "TIMECTL DOWN  UP");
		PV_APPEND_HUD_LINE(outLines, lineCount, "TIMESTEP STEP  RESET 1X");
	} else {
		PV_APPEND_HUD_LINE(outLines, lineCount, "TAB MOUSE  P PAUSE");
		PV_APPEND_HUD_LINE(outLines, lineCount, "F11 AIR  J AUTOJUMP");
		PV_APPEND_HUD_LINE(outLines, lineCount, "F12 DELAY");
		PV_APPEND_HUD_LINE(outLines, lineCount, "B VIEW  H K EXP");
		PV_APPEND_HUD_LINE(outLines, lineCount, "C SHOT");
	}
	PV_APPEND_HUD_LINE(outLines, lineCount, "LMB TOOL  RMB ALT");
	return lineCount;
}
} // namespace

uint32_t BuildDebugHudVertices(
	const DebugStats &stats,
	const CameraState &camera,
	const InteractionState &interaction,
	const bool hudVisible,
	const VkExtent2D extent,
	DebugHudVertex *outVertices,
	const uint32_t maxVertexCount)
{
	if (!hudVisible || !outVertices || maxVertexCount == 0 || extent.width == 0 || extent.height == 0) {
		return 0;
	}

	uint32_t vertexCount = 0;
	constexpr std::array statsPanelColor{0.05f, 0.07f, 0.10f, 0.80f};
	constexpr std::array helperPanelColor{0.07f, 0.09f, 0.12f, 0.76f};
	constexpr std::array accentColor{0.96f, 0.79f, 0.31f, 0.95f};
	constexpr std::array titleColor{0.98f, 0.96f, 0.88f, 0.98f};
	constexpr std::array textColor{0.95f, 0.97f, 0.98f, 0.96f};
	constexpr float titleOffsetPx = 2.0f;
	constexpr float textBoundsHeightPx = kGlyphHeightPx + kTextShadowOffsetPx;
	std::array<std::array<char, kHudLineBufferSize>, kMaxStatsLineCount> statsLines{};
	const size_t statsLineCount = BuildStatsLines(stats, camera, interaction, statsLines);
	std::array<std::array<char, kHudLineBufferSize>, kMaxHelperLineCount> helperLines{};
	const size_t helperLineCount = BuildHelperLines(stats, helperLines);
	const float statsPanelWidthPx = ComputePanelWidthPx(statsLines, "STAT", kStatsPanelMinWidthPx);
	const float helperPanelWidthPx = ComputePanelWidthPx(helperLines, "HELP", kHelperPanelMinWidthPx);
	const float hudStackWidthPx = std::max(statsPanelWidthPx, helperPanelWidthPx);
	const float statsPanelHeightPx =
		kPanelPaddingPx * 2.0f + titleOffsetPx + static_cast<float>(statsLineCount) * kLineAdvancePx + textBoundsHeightPx;
	const float helperPanelHeightPx =
		kPanelPaddingPx * 2.0f + titleOffsetPx + static_cast<float>(helperLineCount) * kLineAdvancePx + textBoundsHeightPx;
	constexpr float statsPanelMinY = kPanelOriginYPx;
	const float statsPanelMaxY = statsPanelMinY + statsPanelHeightPx;
	const float helperPanelMinY = statsPanelMaxY + kPanelGapPx;
	AppendPanel(
		outVertices,
		vertexCount,
		maxVertexCount,
		extent,
		statsPanelMinY,
		hudStackWidthPx,
		statsPanelHeightPx,
		statsPanelColor,
		accentColor);
	AppendPanel(
		outVertices,
		vertexCount,
		maxVertexCount,
		extent,
		helperPanelMinY,
		hudStackWidthPx,
		helperPanelHeightPx,
		helperPanelColor,
		accentColor);

	AppendShadowedTextLine(
		outVertices,
		vertexCount,
		maxVertexCount,
		extent,
		statsPanelMinY + kPanelPaddingPx + titleOffsetPx,
		"STAT",
		titleColor);
	AppendShadowedTextLine(
		outVertices,
		vertexCount,
		maxVertexCount,
		extent,
		helperPanelMinY + kPanelPaddingPx + titleOffsetPx,
		"HELP",
		titleColor);

	for (size_t lineIndex = 0; lineIndex < statsLineCount; ++lineIndex) {
		const float originYPx =
			statsPanelMinY + kPanelPaddingPx + titleOffsetPx + static_cast<float>(lineIndex + 1) * kLineAdvancePx;
		AppendShadowedTextLine(
			outVertices,
			vertexCount,
			maxVertexCount,
			extent,
			originYPx,
			statsLines[lineIndex].data(),
			textColor);
	}

	for (size_t lineIndex = 0; lineIndex < helperLineCount; ++lineIndex) {
		const float originYPx =
			helperPanelMinY + kPanelPaddingPx + titleOffsetPx + static_cast<float>(lineIndex + 1) * kLineAdvancePx;
		AppendShadowedTextLine(
			outVertices,
			vertexCount,
			maxVertexCount,
			extent,
			originYPx,
			helperLines[lineIndex].data(),
			textColor);
	}

	return std::min(vertexCount, maxVertexCount);
}

#undef PV_APPEND_HUD_LINE

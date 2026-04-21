#include "debug/DebugHud.hpp"

#include "app/Camera.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
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
constexpr size_t kStatsLineCount = 17;
constexpr size_t kHelperLineCount = 8;

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

	std::memcpy(outVertices + vertexCount, quadVertices, sizeof(quadVertices));
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

const char *GetChunkDirtyLabel(const bool dirty)
{
	return dirty ? "DIRTY" : "CLEAN";
}

const char *GetChunkActivityLabel(const bool active)
{
	return active ? "ACTIVE" : "EMPTY";
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

void BuildStatsLines(
	const DebugStats &stats,
	const CameraState &camera,
	const InteractionState &interaction,
	std::array<std::array<char, kHudLineBufferSize>, kStatsLineCount> &outLines)
{
	const std::array<float, 3> forward = GetCameraForwardVector(camera);

	std::snprintf(
		outLines.at(0).data(),
		kHudLineBufferSize,
		"FPS %.1f  MS %.2f",
		stats.framesPerSecond,
		stats.frameTimeMilliseconds);
	std::snprintf(outLines.at(1).data(), kHudLineBufferSize, "SCENE %s", GetScenePresetLabel(stats.scenePreset));
	std::snprintf(
		outLines.at(2).data(),
		kHudLineBufferSize,
		"MODE %s  PAUSE %s  AIR %s  AJD %s",
		GetControlModeLabel(stats.controlMode),
		stats.simulationPaused ? "ON" : "OFF",
		GetWalkAirControlModeLabel(stats.walkAirControlMode),
		GetBoolLabel(stats.walkAutoJumpDelayEnabled));
	std::snprintf(
		outLines.at(3).data(),
		kHudLineBufferSize,
		"SIM %u  TRI %u",
		stats.simulationStepsLastFrame,
		stats.sceneTriangleCount);
	std::snprintf(
		outLines.at(4).data(),
		kHudLineBufferSize,
		"DIRTY %u  ACT %u",
		stats.dirtyChunkCount,
		stats.activeChunkCount);
	std::snprintf(
		outLines.at(5).data(),
		kHudLineBufferSize,
		"VOX %u  MEM %.1f",
		stats.nonAirVoxelCount,
		static_cast<double>(stats.sceneMemoryBytes) / 1024.0);
	std::snprintf(
		outLines.at(6).data(),
		kHudLineBufferSize,
		"EDIT %s  BND %s  DIRTY %s",
		GetDebugEditorToolLabel(interaction.editorTool),
		GetBoolLabel(stats.showChunkBounds),
		GetBoolLabel(stats.showDirtyChunkOverlay));
	std::snprintf(
		outLines.at(7).data(),
		kHudLineBufferSize,
		"CAM %.3f %.3f %.3f",
		camera.position[0],
		camera.position[1],
		camera.position[2]);

	if (stats.walkDebugValid) {
		std::snprintf(
			outLines.at(8).data(),
			kHudLineBufferSize,
			"FEET %.3f %.3f %.3f",
			stats.walkFeetPosition[0],
			stats.walkFeetPosition[1],
			stats.walkFeetPosition[2]);
	} else {
		std::snprintf(outLines.at(8).data(), kHudLineBufferSize, "FEET NONE");
	}

	std::snprintf(
		outLines.at(9).data(),
		kHudLineBufferSize,
		"LOOK %.2f %.2f %.2f",
		forward[0],
		forward[1],
		forward[2]);

	if (stats.walkDebugValid) {
		std::snprintf(
			outLines.at(10).data(),
			kHudLineBufferSize,
			"SUP %s %.3f %u %u",
			GetWalkSupportStateLabel(stats.walkSupportState),
			stats.walkFootSupportScore,
			stats.walkFootSupportHitSamples,
			stats.walkFootSupportTotalSamples);
	} else {
		std::snprintf(
			outLines.at(10).data(),
			kHudLineBufferSize,
			"SUP NONE");
	}

	std::snprintf(
		outLines.at(11).data(),
		kHudLineBufferSize,
		"FLG TCK %s SNK %s LCK %s PSL %s",
		GetBoolLabel(stats.walkGroundTakeoffCached),
		GetBoolLabel(stats.walkSneakActive),
		GetBoolLabel(stats.walkJumpLockActive),
		GetBoolLabel(stats.walkSuppressPassiveSlide));

	std::snprintf(
		outLines.at(12).data(),
		kHudLineBufferSize,
		"GRC %u TGR %u SGR %u LGR %u AJR %u",
		stats.walkEdgeGraceFramesRemaining,
		stats.walkGroundTakeoffGraceFramesRemaining,
		stats.walkSneakSupportGraceFramesRemaining,
		stats.walkLedgeReleaseGraceFramesRemaining,
		stats.walkAutoJumpDelayFramesRemaining);

	if (interaction.selection.hasHit) {
		std::snprintf(
			outLines.at(13).data(),
			kHudLineBufferSize,
			"SEL %d %d %d %s %.1f",
			interaction.selection.targetVoxel.x,
			interaction.selection.targetVoxel.y,
			interaction.selection.targetVoxel.z,
			GetVoxelMaterialLabel(interaction.selection.targetMaterial),
			interaction.selection.hitDistance);
	} else {
		std::snprintf(outLines.at(13).data(), kHudLineBufferSize, "SEL NONE");
	}

	if (interaction.selection.hasPlacementVoxel) {
		std::snprintf(
			outLines.at(14).data(),
			kHudLineBufferSize,
			"PUT %d %d %d %s",
			interaction.selection.placementVoxel.x,
			interaction.selection.placementVoxel.y,
			interaction.selection.placementVoxel.z,
			GetVoxelMaterialLabel(interaction.placementMaterial));
	} else {
		std::snprintf(
			outLines.at(14).data(),
			kHudLineBufferSize,
			"PUT NONE %s",
			GetVoxelMaterialLabel(interaction.placementMaterial));
	}

	if (stats.inputReplayPlaybackActive) {
		std::snprintf(
			outLines.at(15).data(),
			kHudLineBufferSize,
			"REP PLAY %u OF %u",
			stats.inputReplayPlaybackFrameIndex,
			stats.inputReplayFrameCount);
	} else if (stats.inputReplayRecording) {
		std::snprintf(
			outLines.at(15).data(),
			kHudLineBufferSize,
			"REP REC %u",
			stats.inputReplayFrameCount);
	} else if (stats.inputReplayReady) {
		std::snprintf(
			outLines.at(15).data(),
			kHudLineBufferSize,
			"REP READY %u",
			stats.inputReplayFrameCount);
	} else {
		std::snprintf(outLines.at(15).data(), kHudLineBufferSize, "REP NONE");
	}

	if (interaction.selection.hasTargetChunk) {
		std::snprintf(
			outLines.at(16).data(),
			kHudLineBufferSize,
			"CHK %d %d %d %s %s",
			interaction.selection.targetChunkCoord.x,
			interaction.selection.targetChunkCoord.y,
			interaction.selection.targetChunkCoord.z,
			GetChunkDirtyLabel(interaction.selection.targetChunkDirty),
			GetChunkActivityLabel(interaction.selection.targetChunkActive));
	} else {
		std::snprintf(outLines.at(16).data(), kHudLineBufferSize, "CHK NONE");
	}
}

void BuildHelperLines(std::array<std::array<char, kHudLineBufferSize>, kHelperLineCount> &outLines)
{
	std::snprintf(outLines.at(0).data(), kHudLineBufferSize, "F1 UI  F2 MAT  F3 CAM");
	std::snprintf(outLines.at(1).data(), kHudLineBufferSize, "F4 MODE  F5 SCENE");
	std::snprintf(outLines.at(2).data(), kHudLineBufferSize, "F6 SAVE  F7 LOAD");
	std::snprintf(outLines.at(3).data(), kHudLineBufferSize, "F8 TOOL  F9 BND");
	std::snprintf(outLines.at(4).data(), kHudLineBufferSize, "F10 DIRTY  F11 AIR");
	std::snprintf(outLines.at(5).data(), kHudLineBufferSize, "F12 AJDLY  R REC");
	std::snprintf(outLines.at(6).data(), kHudLineBufferSize, "Y PLAY  TAB MOUSE");
	std::snprintf(outLines.at(7).data(), kHudLineBufferSize, "LMB TOOL  RMB ALT  P");
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
	constexpr std::array helperTextColor{0.77f, 0.84f, 0.90f, 0.94f};
	constexpr float titleOffsetPx = 2.0f;
	constexpr float textBoundsHeightPx = kGlyphHeightPx + kTextShadowOffsetPx;
	std::array<std::array<char, kHudLineBufferSize>, kStatsLineCount> statsLines{};
	BuildStatsLines(stats, camera, interaction, statsLines);
	std::array<std::array<char, kHudLineBufferSize>, kHelperLineCount> helperLines{};
	BuildHelperLines(helperLines);
	const float statsPanelWidthPx = ComputePanelWidthPx(statsLines, "STAT", kStatsPanelMinWidthPx);
	const float helperPanelWidthPx = ComputePanelWidthPx(helperLines, "HELP", kHelperPanelMinWidthPx);
	const float hudStackWidthPx = std::max(statsPanelWidthPx, helperPanelWidthPx);
	constexpr float statsPanelHeightPx =
		kPanelPaddingPx * 2.0f + titleOffsetPx + static_cast<float>(kStatsLineCount) * kLineAdvancePx + textBoundsHeightPx;
	constexpr float helperPanelHeightPx =
		kPanelPaddingPx * 2.0f + titleOffsetPx + static_cast<float>(kHelperLineCount) * kLineAdvancePx + textBoundsHeightPx;
	constexpr float statsPanelMinY = kPanelOriginYPx;
	constexpr float statsPanelMaxY = statsPanelMinY + statsPanelHeightPx;
	constexpr float helperPanelMinY = statsPanelMaxY + kPanelGapPx;
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

	for (size_t lineIndex = 0; lineIndex < statsLines.size(); ++lineIndex) {
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

	for (size_t lineIndex = 0; lineIndex < helperLines.size(); ++lineIndex) {
		const float originYPx =
			helperPanelMinY + kPanelPaddingPx + titleOffsetPx + static_cast<float>(lineIndex + 1) * kLineAdvancePx;
		AppendShadowedTextLine(
			outVertices,
			vertexCount,
			maxVertexCount,
			extent,
			originYPx,
			helperLines[lineIndex].data(),
			helperTextColor);
	}

	return std::min(vertexCount, maxVertexCount);
}

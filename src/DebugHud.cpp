#include "DebugHud.hpp"

#include "Camera.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace {
constexpr float kPanelPaddingPx = 8.0f;
constexpr float kPanelOriginXPx = 12.0f;
constexpr float kPanelOriginYPx = 12.0f;
constexpr float kGlyphPixelSizePx = 2.0f;
constexpr float kGlyphAdvancePx = 12.0f;
constexpr float kLineAdvancePx = 18.0f;
constexpr float kPanelWidthPx = 320.0f;
constexpr size_t kHudLineBufferSize = 96;
constexpr size_t kHudLineCount = 12;

std::array<uint8_t, 7> GetGlyphRows(const char character)
{
	switch (character) {
	case 'A': return {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
	case 'B': return {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
	case 'C': return {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
	case 'D': return {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C};
	case 'E': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
	case 'F': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
	case 'G': return {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E};
	case 'H': return {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
	case 'I': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F};
	case 'K': return {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
	case 'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
	case 'M': return {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
	case 'N': return {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
	case 'O': return {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
	case 'P': return {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
	case 'R': return {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
	case 'S': return {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
	case 'T': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
	case 'U': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
	case 'V': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
	case 'W': return {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A};
	case 'X': return {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
	case 'Y': return {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
	case '0': return {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
	case '1': return {0x04, 0x0C, 0x14, 0x04, 0x04, 0x04, 0x1F};
	case '2': return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
	case '3': return {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
	case '4': return {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
	case '5': return {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
	case '6': return {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E};
	case '7': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
	case '8': return {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
	case '9': return {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E};
	case '.': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
	case '-': return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
	case ':': return {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00};
	default: return {};
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

bool CanAppendVertices(const uint32_t vertexCount, const uint32_t additionalVertices, const uint32_t maxVertexCount)
{
	return vertexCount + additionalVertices <= maxVertexCount;
}

void AppendQuad(
	DebugHudVertex *outVertices,
	uint32_t *vertexCount,
	const uint32_t maxVertexCount,
	const VkExtent2D extent,
	const float minXPx,
	const float minYPx,
	const float maxXPx,
	const float maxYPx,
	const std::array<float, 4> &color)
{
	if (!outVertices || !vertexCount || extent.width == 0 || extent.height == 0) {
		return;
	}
	if (!CanAppendVertices(*vertexCount, 6, maxVertexCount)) {
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

	std::memcpy(outVertices + *vertexCount, quadVertices, sizeof(quadVertices));
	*vertexCount += 6;
}

void AppendText(
	DebugHudVertex *outVertices,
	uint32_t *vertexCount,
	const uint32_t maxVertexCount,
	const VkExtent2D extent,
	const float originXPx,
	const float originYPx,
	const std::string_view text,
	const std::array<float, 4> &color)
{
	float cursorXPx = originXPx;
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

void AppendShadowedText(
	DebugHudVertex *outVertices,
	uint32_t *vertexCount,
	const uint32_t maxVertexCount,
	const VkExtent2D extent,
	const float originXPx,
	const float originYPx,
	const std::string_view text,
	const std::array<float, 4> &color)
{
	AppendText(
		outVertices,
		vertexCount,
		maxVertexCount,
		extent,
		originXPx + 1.0f,
		originYPx + 1.0f,
		text,
		{0.0f, 0.0f, 0.0f, color[3] * 0.65f});
	AppendText(
		outVertices,
		vertexCount,
		maxVertexCount,
		extent,
		originXPx,
		originYPx,
		text,
		color);
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

void BuildHudLines(
	const DebugStats &stats,
	const CameraState &camera,
	const InteractionState &interaction,
	std::array<std::array<char, kHudLineBufferSize>, kHudLineCount> *outLines)
{
	if (!outLines) {
		return;
	}

	const std::array<float, 3> forward = GetCameraForwardVector(camera);

	std::snprintf(outLines->at(0).data(), kHudLineBufferSize, "FPS %.1f", stats.framesPerSecond);
	std::snprintf(outLines->at(1).data(), kHudLineBufferSize, "MS %.2f", stats.frameTimeMilliseconds);
	std::snprintf(outLines->at(2).data(), kHudLineBufferSize, "SIM %u TRI %u", stats.simulationStepsLastFrame, stats.sceneTriangleCount);
	std::snprintf(outLines->at(3).data(), kHudLineBufferSize, "DIRTY %u ACTIVE %u", stats.dirtyChunkCount, stats.activeChunkCount);
	std::snprintf(outLines->at(4).data(), kHudLineBufferSize, "VOX %u MEM %.1f KB", stats.nonAirVoxelCount, static_cast<double>(stats.sceneMemoryBytes) / 1024.0);
#if defined(PROJECTV_ENABLE_TRACY)
	constexpr const char *tracyFlag = "ON";
#else
	constexpr const char *tracyFlag = "OFF";
#endif
#if !defined(NDEBUG)
	constexpr const char *validationFlag = "ON";
#else
	constexpr const char *validationFlag = "OFF";
#endif
	std::snprintf(outLines->at(5).data(), kHudLineBufferSize, "VAL %s TRACY %s", validationFlag, tracyFlag);
	std::snprintf(outLines->at(6).data(), kHudLineBufferSize, "POS %.1f %.1f %.1f", camera.position[0], camera.position[1], camera.position[2]);
	std::snprintf(outLines->at(7).data(), kHudLineBufferSize, "DIR %.2f %.2f %.2f", forward[0], forward[1], forward[2]);

	if (interaction.selection.hasHit) {
		std::snprintf(
			outLines->at(8).data(),
			kHudLineBufferSize,
			"SEL %d %d %d %s %.2f",
			interaction.selection.targetVoxel.x,
			interaction.selection.targetVoxel.y,
			interaction.selection.targetVoxel.z,
			GetVoxelMaterialLabel(interaction.selection.targetMaterial),
			interaction.selection.hitDistance);
	} else {
		std::snprintf(outLines->at(8).data(), kHudLineBufferSize, "SEL NONE");
	}

	if (interaction.selection.hasPlacementVoxel) {
		std::snprintf(
			outLines->at(9).data(),
			kHudLineBufferSize,
			"PLACE %d %d %d %s",
			interaction.selection.placementVoxel.x,
			interaction.selection.placementVoxel.y,
			interaction.selection.placementVoxel.z,
			GetVoxelMaterialLabel(interaction.placementMaterial));
	} else {
		std::snprintf(
			outLines->at(9).data(),
			kHudLineBufferSize,
			"PLACE NONE %s",
			GetVoxelMaterialLabel(interaction.placementMaterial));
	}

	std::snprintf(outLines->at(10).data(), kHudLineBufferSize, "GLASS %u FLUID %u", stats.glassVoxelCount, stats.fluidVoxelCount);
	std::snprintf(outLines->at(11).data(), kHudLineBufferSize, "FLOOR %u HUD F1", stats.floorVoxelCount);
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
	const std::array<float, 4> panelColor{0.06f, 0.08f, 0.10f, 0.78f};
	const std::array<float, 4> textColor{0.96f, 0.97f, 0.98f, 0.96f};
	const float panelHeightPx = kPanelPaddingPx * 2.0f + static_cast<float>(kHudLineCount) * kLineAdvancePx - 4.0f;
	AppendQuad(
		outVertices,
		&vertexCount,
		maxVertexCount,
		extent,
		kPanelOriginXPx,
		kPanelOriginYPx,
		kPanelOriginXPx + kPanelWidthPx,
		kPanelOriginYPx + panelHeightPx,
		panelColor);

	std::array<std::array<char, kHudLineBufferSize>, kHudLineCount> lines{};
	BuildHudLines(stats, camera, interaction, &lines);

	for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
		const float originXPx = kPanelOriginXPx + kPanelPaddingPx;
		const float originYPx = kPanelOriginYPx + kPanelPaddingPx + static_cast<float>(lineIndex) * kLineAdvancePx;
		AppendShadowedText(
			outVertices,
			&vertexCount,
			maxVertexCount,
			extent,
			originXPx,
			originYPx,
			lines[lineIndex].data(),
			textColor);
	}

	return std::min(vertexCount, maxVertexCount);
}

#pragma once

#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include <filesystem>
#include <string_view>

std::filesystem::path GetScreenshotCaptureDirectoryPath();
std::filesystem::path BuildScreenshotCapturePath(
	VoxelScenePreset scenePreset,
	uint64_t captureSequence);
std::filesystem::path BuildScreenshotCaptureMetadataPath(std::string_view screenshotPath);
bool SaveScreenshotCaptureBmp(
	const void *pixels,
	uint32_t width,
	uint32_t height,
	VkFormat format,
	std::string_view screenshotPath);
bool SaveScreenshotCaptureMetadata(
	const RenderState &render,
	VoxelScenePreset scenePreset,
	std::string_view screenshotPath,
	std::string_view metadataPath);


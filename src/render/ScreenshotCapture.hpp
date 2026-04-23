#ifndef SCREENSHOT_CAPTURE_HPP
#define SCREENSHOT_CAPTURE_HPP

#include "core/Types.hpp"

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

#endif

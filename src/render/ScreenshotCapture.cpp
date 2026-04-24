#include "render/ScreenshotCapture.hpp"

#include "core/RuntimeDiagnostics.hpp"
#include "voxel/VoxelWorld.hpp"

#include "SDL3/SDL.h"
#include "fmt/format.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <system_error>
#include <vector>

namespace {
constexpr char kScreenshotDirectoryEnvVar[] = "PROJECTV_SCREENSHOT_DIR";
constexpr char kDefaultScreenshotDirectoryName[] = "ProjectVScreenshots";
constexpr uint32_t kBmpFileHeaderSize = 14u;
constexpr uint32_t kBmpInfoHeaderSize = 40u;
constexpr uint32_t kBmpPixelBytesPerPixel = 3u;

bool EnsureParentDirectoryExists(
	const std::filesystem::path &path,
	const std::string_view step)
{
	const std::filesystem::path parentPath = path.parent_path();
	if (parentPath.empty()) {
		return true;
	}

	std::error_code error;
	std::filesystem::create_directories(parentPath, error);
	if (error) {
		runtime::LogRuntimeFailure("Capture", step, error.message());
		return false;
	}

	return true;
}

void StoreLittleEndianU16(
	std::ofstream &stream,
	const uint16_t value)
{
	const std::array bytes{
		static_cast<uint8_t>(value & 0xFFu),
		static_cast<uint8_t>(value >> 8u & 0xFFu),
	};
	stream.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}

void StoreLittleEndianU32(
	std::ofstream &stream,
	const uint32_t value)
{
	const std::array bytes{
		static_cast<uint8_t>(value & 0xFFu),
		static_cast<uint8_t>(value >> 8u & 0xFFu),
		static_cast<uint8_t>(value >> 16u & 0xFFu),
		static_cast<uint8_t>(value >> 24u & 0xFFu),
	};
	stream.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}

bool IsSupportedScreenshotFormat(const VkFormat format)
{
	return format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_R8G8B8A8_UNORM;
}
} // namespace

std::filesystem::path GetScreenshotCaptureDirectoryPath()
{
	if (const char *overridePath = SDL_getenv(kScreenshotDirectoryEnvVar);
		overridePath && *overridePath) {
		return std::filesystem::path(overridePath);
	}

	if (const char *basePath = SDL_GetBasePath();
		basePath && *basePath) {
		const std::filesystem::path resolvedPath =
			std::filesystem::path(basePath) / kDefaultScreenshotDirectoryName;
		return resolvedPath;
	}

	return std::filesystem::path(kDefaultScreenshotDirectoryName);
}

std::filesystem::path BuildScreenshotCapturePath(
	const VoxelScenePreset scenePreset,
	const uint64_t captureSequence)
{
	const auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
	const uint64_t epochMilliseconds = static_cast<uint64_t>(now.time_since_epoch().count());
	return GetScreenshotCaptureDirectoryPath() /
		   fmt::format(
			   "ProjectV-{}-{}-{:04}.bmp",
			   VoxelScenePresetToString(scenePreset),
			   epochMilliseconds,
			   captureSequence);
}

std::filesystem::path BuildScreenshotCaptureMetadataPath(const std::string_view screenshotPath)
{
	std::filesystem::path metadataPath{std::string(screenshotPath)};
	metadataPath.replace_extension(".txt");
	return metadataPath;
}

bool SaveScreenshotCaptureBmp(
	const void *pixels,
	const uint32_t width,
	const uint32_t height,
	const VkFormat format,
	const std::string_view screenshotPath)
{
	if (!pixels) {
		runtime::LogRuntimeFailure("Capture", "SaveScreenshotCaptureBmp.Pixels", "pixel buffer is null");
		return false;
	}
	if (width == 0 || height == 0) {
		runtime::LogRuntimeFailure("Capture", "SaveScreenshotCaptureBmp.Extent", "screenshot extent is zero");
		return false;
	}
	if (screenshotPath.empty()) {
		runtime::LogRuntimeFailure("Capture", "SaveScreenshotCaptureBmp.Path", "screenshot path is empty");
		return false;
	}
	if (!IsSupportedScreenshotFormat(format)) {
		runtime::LogRuntimeFailure(
			"Capture",
			"SaveScreenshotCaptureBmp.Format",
			fmt::format("unsupported swapchain format {}", static_cast<int>(format)));
		return false;
	}

	const std::filesystem::path resolvedPath{std::string(screenshotPath)};
	if (!EnsureParentDirectoryExists(resolvedPath, "SaveScreenshotCaptureBmp.CreateDirectories")) {
		return false;
	}

	const uint64_t pixelRowStride = static_cast<uint64_t>(width) * kBmpPixelBytesPerPixel;
	const uint32_t rowPadding = static_cast<uint32_t>((4u - pixelRowStride % 4u) % 4u);
	const uint64_t fileRowStride = pixelRowStride + rowPadding;
	const uint64_t pixelDataSize = fileRowStride * height;
	const uint64_t fileSize = kBmpFileHeaderSize + kBmpInfoHeaderSize + pixelDataSize;
	if (pixelDataSize > UINT32_MAX || fileSize > UINT32_MAX) {
		runtime::LogRuntimeFailure("Capture", "SaveScreenshotCaptureBmp.Size", "screenshot is too large for BMP");
		return false;
	}

	std::ofstream stream(resolvedPath, std::ios::binary | std::ios::trunc);
	if (!stream) {
		runtime::LogRuntimeFailure("Capture", "SaveScreenshotCaptureBmp.Open", resolvedPath.string());
		return false;
	}

	stream.put('B');
	stream.put('M');
	StoreLittleEndianU32(stream, static_cast<uint32_t>(fileSize));
	StoreLittleEndianU16(stream, 0u);
	StoreLittleEndianU16(stream, 0u);
	StoreLittleEndianU32(stream, kBmpFileHeaderSize + kBmpInfoHeaderSize);

	StoreLittleEndianU32(stream, kBmpInfoHeaderSize);
	StoreLittleEndianU32(stream, width);
	StoreLittleEndianU32(stream, height);
	StoreLittleEndianU16(stream, 1u);
	StoreLittleEndianU16(stream, 24u);
	StoreLittleEndianU32(stream, 0u);
	StoreLittleEndianU32(stream, static_cast<uint32_t>(pixelDataSize));
	StoreLittleEndianU32(stream, 2835u);
	StoreLittleEndianU32(stream, 2835u);
	StoreLittleEndianU32(stream, 0u);
	StoreLittleEndianU32(stream, 0u);

	std::vector<uint8_t> rowBuffer(fileRowStride, 0u);
	const uint8_t *const sourcePixels = static_cast<const uint8_t *>(pixels);
	for (uint32_t sourceRow = 0; sourceRow < height; ++sourceRow) {
		const uint32_t flippedRow = height - 1u - sourceRow;
		const uint8_t *const sourceRowPixels =
			sourcePixels + static_cast<size_t>(flippedRow) * static_cast<size_t>(width) * 4u;
		for (uint32_t x = 0; x < width; ++x) {
			const size_t sourceOffset = static_cast<size_t>(x) * 4u;
			const size_t destinationOffset = static_cast<size_t>(x) * kBmpPixelBytesPerPixel;
			if (format == VK_FORMAT_B8G8R8A8_UNORM) {
				rowBuffer[destinationOffset + 0u] = sourceRowPixels[sourceOffset + 0u];
				rowBuffer[destinationOffset + 1u] = sourceRowPixels[sourceOffset + 1u];
				rowBuffer[destinationOffset + 2u] = sourceRowPixels[sourceOffset + 2u];
			} else {
				rowBuffer[destinationOffset + 0u] = sourceRowPixels[sourceOffset + 2u];
				rowBuffer[destinationOffset + 1u] = sourceRowPixels[sourceOffset + 1u];
				rowBuffer[destinationOffset + 2u] = sourceRowPixels[sourceOffset + 0u];
			}
		}
		stream.write(reinterpret_cast<const char *>(rowBuffer.data()), static_cast<std::streamsize>(rowBuffer.size()));
	}

	if (!stream) {
		runtime::LogRuntimeFailure("Capture", "SaveScreenshotCaptureBmp.Write", resolvedPath.string());
		return false;
	}

	return true;
}

bool SaveScreenshotCaptureMetadata(
	const RenderState &render,
	const VoxelScenePreset scenePreset,
	const std::string_view screenshotPath,
	const std::string_view metadataPath)
{
	if (metadataPath.empty()) {
		runtime::LogRuntimeFailure("Capture", "SaveScreenshotCaptureMetadata.Path", "metadata path is empty");
		return false;
	}

	const std::filesystem::path resolvedPath{std::string(metadataPath)};
	if (!EnsureParentDirectoryExists(resolvedPath, "SaveScreenshotCaptureMetadata.CreateDirectories")) {
		return false;
	}

	std::ofstream stream(resolvedPath, std::ios::binary | std::ios::trunc);
	if (!stream) {
		runtime::LogRuntimeFailure("Capture", "SaveScreenshotCaptureMetadata.Open", resolvedPath.string());
		return false;
	}

	stream << fmt::format(
		"image_path={}\n"
		"scene_preset={}\n"
		"tone_map={}\n"
		"debug_view={}\n"
		"exposure={:.6f}\n"
		"environment_intensity={:.6f}\n"
		"grading_white_point={:.6f}\n"
		"grading_contrast={:.6f}\n"
		"grading_saturation={:.6f}\n"
		"grading_lift={:.6f}\n"
		"exposure_metering={}\n"
		"exposure_key={:.6f}\n"
		"exposure_target_key={:.6f}\n"
		"exposure_min={:.6f}\n"
		"exposure_max={:.6f}\n"
		"exposure_bias_stops={:.6f}\n"
		"sun_direction={:.6f} {:.6f} {:.6f}\n"
		"sun_intensity={:.6f}\n"
		"shadow_map_resolution={}\n"
		"shadow_strength={:.6f}\n"
		"shadow_depth_bias={:.6f}\n"
		"shadow_normal_bias={:.6f}\n"
		"shadow_filter_radius={:.6f}\n"
		"shadow_coverage_scale={:.6f}\n"
		"shadow_cascade_blend={:.6f}\n"
		"shadow_cascade_count={}\n"
		"shadow_cascade_lambda={:.6f}\n"
		"shadow_cascade_splits={:.6f} {:.6f} {:.6f} {:.6f}\n"
		"shadow_cascade_view_ranges={:.6f}:{:.6f} {:.6f}:{:.6f} {:.6f}:{:.6f} {:.6f}:{:.6f}\n"
		"shadow_cascade_ortho_extents={:.6f}x{:.6f} {:.6f}x{:.6f} {:.6f}x{:.6f} {:.6f}x{:.6f}\n"
		"shadow_cascade_texel_world={:.6f} {:.6f} {:.6f} {:.6f}\n"
		"shadow_cascade_caster_light_ranges={:.6f}:{:.6f} {:.6f}:{:.6f} {:.6f}:{:.6f} {:.6f}:{:.6f}\n"
		"transparent_shadow_policy={}\n"
		"shadow_tuning_target={}\n"
		"shadow_strength_offset={:.6f}\n"
		"shadow_depth_bias_offset={:.6f}\n"
		"shadow_normal_bias_offset={:.6f}\n"
		"shadow_filter_radius_offset={:.6f}\n"
		"shadow_cascade_blend_offset={:.6f}\n",
		screenshotPath,
		VoxelScenePresetToString(scenePreset),
		ToneMapOperatorToString(render.lightingDebugControls.toneMapOperator),
		LightingDebugViewToString(render.lightingDebugControls.debugView),
		render.currentSceneLighting.postProcess[0],
		render.currentSceneLighting.postProcess[1],
		render.currentSceneLighting.colorGrading[0],
		render.currentSceneLighting.colorGrading[1],
		render.currentSceneLighting.colorGrading[2],
		render.currentSceneLighting.colorGrading[3],
		ExposureMeteringModeToString(static_cast<ExposureMeteringMode>(
			std::lround(render.currentSceneLighting.exposureControl[0]))),
		EstimateVoxelSceneExposureKey(render.currentSceneLighting),
		render.currentSceneLighting.exposureControl[1],
		render.currentSceneLighting.exposureControl[2],
		render.currentSceneLighting.exposureControl[3],
		render.lightingDebugControls.exposureBiasStops,
		render.currentSceneLighting.sunDirectionAndWrap[0],
		render.currentSceneLighting.sunDirectionAndWrap[1],
		render.currentSceneLighting.sunDirectionAndWrap[2],
		render.currentSceneLighting.sunColorAndIntensity[3],
		render.shadowMapExtent.width,
		render.currentSceneLighting.sunShadowParams[0],
		render.currentSceneLighting.sunShadowParams[1],
		render.currentSceneLighting.sunShadowParams[2],
		render.currentSceneLighting.sunShadowParams[3],
		render.lightingDebugControls.shadowCoverageScale,
		render.currentSceneLighting.shadowCascadeBlendParams[0],
		kSunShadowCascadeCount,
		render.currentSunShadowCascadeSplits.splitLambda,
		render.currentSunShadowCascadeSplits.viewDepthSplits[0],
		render.currentSunShadowCascadeSplits.viewDepthSplits[1],
		render.currentSunShadowCascadeSplits.viewDepthSplits[2],
		render.currentSunShadowCascadeSplits.viewDepthSplits[3],
		render.currentSunShadowCascadeDiagnostics.viewNearDepths[0],
		render.currentSunShadowCascadeDiagnostics.viewFarDepths[0],
		render.currentSunShadowCascadeDiagnostics.viewNearDepths[1],
		render.currentSunShadowCascadeDiagnostics.viewFarDepths[1],
		render.currentSunShadowCascadeDiagnostics.viewNearDepths[2],
		render.currentSunShadowCascadeDiagnostics.viewFarDepths[2],
		render.currentSunShadowCascadeDiagnostics.viewNearDepths[3],
		render.currentSunShadowCascadeDiagnostics.viewFarDepths[3],
		render.currentSunShadowCascadeDiagnostics.orthoWidths[0],
		render.currentSunShadowCascadeDiagnostics.orthoHeights[0],
		render.currentSunShadowCascadeDiagnostics.orthoWidths[1],
		render.currentSunShadowCascadeDiagnostics.orthoHeights[1],
		render.currentSunShadowCascadeDiagnostics.orthoWidths[2],
		render.currentSunShadowCascadeDiagnostics.orthoHeights[2],
		render.currentSunShadowCascadeDiagnostics.orthoWidths[3],
		render.currentSunShadowCascadeDiagnostics.orthoHeights[3],
		render.currentSunShadowCascadeDiagnostics.texelWorldSizes[0],
		render.currentSunShadowCascadeDiagnostics.texelWorldSizes[1],
		render.currentSunShadowCascadeDiagnostics.texelWorldSizes[2],
		render.currentSunShadowCascadeDiagnostics.texelWorldSizes[3],
		render.currentSunShadowCascadeDiagnostics.casterLightNearDepths[0],
		render.currentSunShadowCascadeDiagnostics.casterLightFarDepths[0],
		render.currentSunShadowCascadeDiagnostics.casterLightNearDepths[1],
		render.currentSunShadowCascadeDiagnostics.casterLightFarDepths[1],
		render.currentSunShadowCascadeDiagnostics.casterLightNearDepths[2],
		render.currentSunShadowCascadeDiagnostics.casterLightFarDepths[2],
		render.currentSunShadowCascadeDiagnostics.casterLightNearDepths[3],
		render.currentSunShadowCascadeDiagnostics.casterLightFarDepths[3],
		TransparentShadowPolicyToString(render.transparentShadowPolicy),
		ShadowTuningTargetToString(render.lightingDebugControls.shadowTuningTarget),
		render.lightingDebugControls.shadowStrengthOffset,
		render.lightingDebugControls.shadowDepthBiasOffset,
		render.lightingDebugControls.shadowNormalBiasOffset,
		render.lightingDebugControls.shadowFilterRadiusOffset,
		render.lightingDebugControls.shadowCascadeBlendOffset);

	if (!stream) {
		runtime::LogRuntimeFailure("Capture", "SaveScreenshotCaptureMetadata.Write", resolvedPath.string());
		return false;
	}

	return true;
}

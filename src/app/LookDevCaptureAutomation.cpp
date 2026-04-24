#include "app/LookDevCaptureAutomation.hpp"

#include "voxel/VoxelMaterials.hpp"

#include "SDL3/SDL.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {
constexpr char kCameraPositionEnvVar[] = "PROJECTV_START_CAMERA_POSITION";
constexpr char kCameraLookEnvVar[] = "PROJECTV_START_CAMERA_LOOK";
constexpr char kCaptureViewsEnvVar[] = "PROJECTV_LOOKDEV_CAPTURE_VIEWS";
constexpr char kCaptureWarmupFramesEnvVar[] = "PROJECTV_LOOKDEV_CAPTURE_WARMUP_FRAMES";
constexpr char kCaptureIntervalFramesEnvVar[] = "PROJECTV_LOOKDEV_CAPTURE_INTERVAL_FRAMES";
constexpr char kCaptureQuitEnvVar[] = "PROJECTV_LOOKDEV_CAPTURE_QUIT";
constexpr uint32_t kDefaultCaptureWarmupFrames = 30u;
constexpr uint32_t kDefaultCaptureIntervalFrames = 2u;
constexpr float kMinLookVectorLength = 0.00001f;

bool IsValueSeparator(const char value)
{
	return value == ' ' || value == '\t' || value == '\n' || value == '\r' || value == ',' || value == ';';
}

bool IsTokenSeparator(const char value)
{
	return IsValueSeparator(value) || value == '|';
}

std::string NormalizeToken(const std::string_view text)
{
	std::string result;
	result.reserve(text.size());
	for (const char value : text) {
		if (value == '-' || value == '_' || value == ' ') {
			continue;
		}
		result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(value))));
	}
	return result;
}

bool TryParseFloat3(const std::string_view text, std::array<float, 3> *outValue)
{
	if (!outValue) {
		return false;
	}

	std::string copy{text};
	char *cursor = copy.data();
	for (float &component : *outValue) {
		while (*cursor != '\0' && IsValueSeparator(*cursor)) {
			++cursor;
		}

		errno = 0;
		char *end = cursor;
		const float value = std::strtof(cursor, &end);
		if (end == cursor || errno == ERANGE) {
			return false;
		}
		if (!std::isfinite(value)) {
			return false;
		}

		component = value;
		cursor = end;
	}

	while (*cursor != '\0') {
		if (!IsValueSeparator(*cursor)) {
			return false;
		}
		++cursor;
	}
	return true;
}

bool TryParseUnsigned(const char *text, uint32_t *outValue)
{
	if (!text || !*text || !outValue) {
		return false;
	}

	while (*text != '\0' && IsValueSeparator(*text)) {
		++text;
	}
	if (*text == '-') {
		return false;
	}

	errno = 0;
	char *end = nullptr;
	const unsigned long value = std::strtoul(text, &end, 10);
	if (end == text || errno == ERANGE || value > UINT32_MAX) {
		return false;
	}
	while (*end != '\0') {
		if (!IsValueSeparator(*end)) {
			return false;
		}
		++end;
	}

	*outValue = static_cast<uint32_t>(value);
	return true;
}

uint32_t ReadUnsignedEnvironment(
	const char *name,
	const uint32_t fallbackValue)
{
	const char *text = SDL_getenv(name);
	if (!text || !*text) {
		return fallbackValue;
	}

	uint32_t value = fallbackValue;
	if (!TryParseUnsigned(text, &value)) {
		SDL_Log("[ProjectV][LookDevCapture] ignored invalid %s='%s'", name, text);
		return fallbackValue;
	}
	return value;
}

bool ReadBoolEnvironment(const char *name)
{
	const char *text = SDL_getenv(name);
	if (!text || !*text) {
		return false;
	}

	const std::string token = NormalizeToken(text);
	return token == "1" || token == "true" || token == "yes" || token == "on";
}

bool TryParseDebugViewToken(
	const std::string_view token,
	LightingDebugView *outView)
{
	if (!outView) {
		return false;
	}

	const std::string normalized = NormalizeToken(token);
	if (normalized == "final") {
		*outView = LightingDebugView::Final;
		return true;
	}
	if (normalized == "amb" || normalized == "ambient") {
		*outView = LightingDebugView::Ambient;
		return true;
	}
	if (normalized == "dir" || normalized == "direct") {
		*outView = LightingDebugView::Direct;
		return true;
	}
	if (normalized == "shdw" || normalized == "shadow") {
		*outView = LightingDebugView::Shadow;
		return true;
	}
	if (normalized == "csm" || normalized == "cascade" || normalized == "cascades") {
		*outView = LightingDebugView::Cascade;
		return true;
	}
	if (normalized == "fog") {
		*outView = LightingDebugView::Fog;
		return true;
	}
	return false;
}

bool TryParseDebugViewList(
	const std::string_view text,
	std::array<LightingDebugView, MAX_LOOK_DEV_CAPTURE_VIEW_COUNT> *outViews,
	uint32_t *outViewCount)
{
	if (!outViews || !outViewCount) {
		return false;
	}

	*outViews = {};
	*outViewCount = 0;
	size_t tokenStart = 0;
	while (tokenStart < text.size()) {
		while (tokenStart < text.size() && IsTokenSeparator(text[tokenStart])) {
			++tokenStart;
		}
		if (tokenStart >= text.size()) {
			break;
		}

		size_t tokenEnd = tokenStart;
		while (tokenEnd < text.size() && !IsTokenSeparator(text[tokenEnd])) {
			++tokenEnd;
		}

		if (*outViewCount >= MAX_LOOK_DEV_CAPTURE_VIEW_COUNT) {
			return false;
		}

		LightingDebugView view = LightingDebugView::Final;
		if (!TryParseDebugViewToken(text.substr(tokenStart, tokenEnd - tokenStart), &view)) {
			return false;
		}
		(*outViews)[*outViewCount] = view;
		++(*outViewCount);
		tokenStart = tokenEnd;
	}

	return *outViewCount > 0;
}

void ApplyCameraLook(CameraState &camera, const std::array<float, 3> &look)
{
	const float length = std::sqrt(look[0] * look[0] + look[1] * look[1] + look[2] * look[2]);
	if (length <= kMinLookVectorLength) {
		return;
	}

	const std::array normalized{
		look[0] / length,
		look[1] / length,
		look[2] / length,
	};
	camera.pitchRadians = std::asin(std::clamp(normalized[1], -1.0f, 1.0f));
	camera.yawRadians = std::atan2(normalized[0], -normalized[2]);
}
} // namespace

void ApplyStartupCameraOverrideFromEnvironment(CameraState *camera)
{
	if (!camera) {
		return;
	}

	if (const char *positionText = SDL_getenv(kCameraPositionEnvVar);
		positionText && *positionText) {
		std::array<float, 3> position{};
		if (TryParseFloat3(positionText, &position)) {
			camera->position = position;
		} else {
			SDL_Log("[ProjectV][LookDevCapture] ignored invalid %s='%s'", kCameraPositionEnvVar, positionText);
		}
	}

	if (const char *lookText = SDL_getenv(kCameraLookEnvVar);
		lookText && *lookText) {
		std::array<float, 3> look{};
		if (TryParseFloat3(lookText, &look)) {
			ApplyCameraLook(*camera, look);
		} else {
			SDL_Log("[ProjectV][LookDevCapture] ignored invalid %s='%s'", kCameraLookEnvVar, lookText);
		}
	}
}

void ConfigureLookDevCaptureAutomationFromEnvironment(LookDevCaptureAutomationState *automation)
{
	if (!automation) {
		return;
	}

	*automation = {};
	const char *viewText = SDL_getenv(kCaptureViewsEnvVar);
	if (!viewText || !*viewText) {
		return;
	}

	uint32_t viewCount = 0;
	std::array<LightingDebugView, MAX_LOOK_DEV_CAPTURE_VIEW_COUNT> views{};
	if (!TryParseDebugViewList(viewText, &views, &viewCount)) {
		SDL_Log("[ProjectV][LookDevCapture] ignored invalid %s='%s'", kCaptureViewsEnvVar, viewText);
		return;
	}

	automation->active = true;
	automation->views = views;
	automation->viewCount = viewCount;
	automation->warmupFramesRemaining =
		ReadUnsignedEnvironment(kCaptureWarmupFramesEnvVar, kDefaultCaptureWarmupFrames);
	automation->intervalFrames =
		ReadUnsignedEnvironment(kCaptureIntervalFramesEnvVar, kDefaultCaptureIntervalFrames);
	automation->quitWhenDone = ReadBoolEnvironment(kCaptureQuitEnvVar);

	SDL_Log(
		"[ProjectV][LookDevCapture] armed views=%u warmup=%u interval=%u quit=%s",
		automation->viewCount,
		automation->warmupFramesRemaining,
		automation->intervalFrames,
		automation->quitWhenDone ? "true" : "false");
}

bool UpdateLookDevCaptureAutomation(
	LookDevCaptureAutomationState *automation,
	RenderState *render)
{
	if (!automation || !render || !automation->active || automation->completed) {
		return false;
	}

	if (automation->warmupFramesRemaining > 0) {
		--automation->warmupFramesRemaining;
		return false;
	}
	if (automation->intervalFramesRemaining > 0) {
		--automation->intervalFramesRemaining;
		return false;
	}
	if (automation->nextViewIndex >= automation->viewCount) {
		automation->completed = true;
		return false;
	}

	const LightingDebugView view = automation->views[automation->nextViewIndex++];
	render->lightingDebugControls.debugView = view;
	render->screenshotCaptureRequested = true;
	automation->intervalFramesRemaining = automation->intervalFrames;

	SDL_Log(
		"[ProjectV][LookDevCapture] capture requested view=%s index=%u/%u",
		LightingDebugViewToString(view),
		automation->nextViewIndex,
		automation->viewCount);

	if (automation->nextViewIndex >= automation->viewCount) {
		automation->completed = true;
		return automation->quitWhenDone;
	}
	return false;
}

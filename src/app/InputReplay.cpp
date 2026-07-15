#include "app/InputReplay.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "app/InputActions.hpp"
#include "core/EnvUtils.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "voxel/VoxelWorld.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {
constexpr std::string_view kInputReplayMagic = "PROJECTV_INPUT_REPLAY";
constexpr int kInputReplayVersion = 1; // sim-tick samples only; no legacy readers

std::filesystem::path GetInputReplayDirectoryPath()
{
	if (const char *overrideDirectory = projectv::core::GetEnvVar("PROJECTV_INPUT_REPLAY_DIR");
		overrideDirectory && *overrideDirectory) {
		return overrideDirectory;
	}

	std::error_code error;
	const std::filesystem::path tempDirectory = std::filesystem::temp_directory_path(error);
	if (error) {
		return std::filesystem::path("ProjectVInputReplay");
	}

	return tempDirectory / "ProjectV" / "InputReplay";
}

std::filesystem::path GetResolvedInputReplayPath()
{
	if (const char *overridePath = projectv::core::GetEnvVar("PROJECTV_INPUT_REPLAY_PATH");
		overridePath && *overridePath) {
		return overridePath;
	}

	return GetInputReplayDirectoryPath() / "latest.projectv.replay";
}

std::filesystem::path GetResolvedInputReplaySnapshotPath()
{
	if (const char *overridePath = projectv::core::GetEnvVar("PROJECTV_INPUT_REPLAY_SNAPSHOT_PATH");
		overridePath && *overridePath) {
		return overridePath;
	}

	return GetInputReplayDirectoryPath() / "latest.projectv.replay.snapshot.bin";
}

bool EnsureParentDirectoryExists(const std::filesystem::path &path)
{
	const std::filesystem::path parentPath = path.parent_path();
	if (parentPath.empty()) {
		return true;
	}

	std::error_code error;
	std::filesystem::create_directories(parentPath, error);
	if (error) {
		runtime::LogRuntimeFailure("InputReplay", "SaveInputReplayCapture.CreateDirectories", error.message());
		return false;
	}

	return true;
}

void ResetReplayFrameApplication(InputState &input)
{
	input.mouseDeltaX = 0.0f;
	input.mouseDeltaY = 0.0f;
	input.removePressed = false;
	input.placePressed = false;
	input.lastMoveUpPressedTimestampNs = 0;
	ApplyInputActionSnapshot(input, 0ull, 0ull);
}

bool WriteReplayCapture(std::ostream &stream, const InputReplayCapture &capture)
{
	stream << kInputReplayMagic << ' ' << kInputReplayVersion << '\n';
	stream << "snapshot_path " << std::quoted(capture.snapshotPath.string()) << '\n';
	stream << "camera_pos "
		   << capture.initialCamera.position[0] << ' '
		   << capture.initialCamera.position[1] << ' '
		   << capture.initialCamera.position[2] << '\n';
	stream << "camera_angles "
		   << capture.initialCamera.yawRadians << ' '
		   << capture.initialCamera.pitchRadians << '\n';
	stream << "camera_move "
		   << capture.initialCamera.moveSpeed << ' '
		   << capture.initialCamera.mouseSensitivity << ' '
		   << capture.initialCamera.verticalFovRadians << ' '
		   << capture.initialCamera.nearPlane << ' '
		   << capture.initialCamera.farPlane << '\n';
	stream << "camera_control_mode " << static_cast<int>(capture.initialCamera.controlMode) << '\n';
	stream << "interaction "
		   << static_cast<int>(capture.initialInteraction.placementMaterial) << ' '
		   << capture.initialInteraction.maxInteractionDistance << ' '
		   << static_cast<int>(capture.initialInteraction.editorTool) << '\n';
	stream << "walk "
		   << static_cast<int>(capture.walkAirControlMode) << ' '
		   << static_cast<int>(capture.walkAutoJumpEnabled) << ' '
		   << 0 << '\n'; // walkAutoJumpDelayEnabled always 0
	stream << "frame_count " << capture.frames.size() << '\n';
	for (const auto &[deltaSeconds, mouseDeltaX, mouseDeltaY, actionDownMask, actionPressedMask, removePressed, placePressed] : capture.frames) {
		stream << "frame "
			   << deltaSeconds << ' '
			   << mouseDeltaX << ' '
			   << mouseDeltaY << ' '
			   << actionDownMask << ' '
			   << actionPressedMask << ' '
			   << static_cast<int>(removePressed) << ' '
			   << static_cast<int>(placePressed) << '\n';
	}

	return stream.good();
}

bool ReadReplayCapture(std::istream &stream, InputReplayCapture *outCapture)
{
	bool ok = true;
	if (!outCapture) {
		runtime::LogRuntimeFailure("InputReplay", "ReadReplayCapture.OutCapture", "outCapture is null");
		ok = false;
	}

	std::string magic;
	int version = 0;
	if (ok && (!(stream >> magic >> version) ||
			   magic != kInputReplayMagic ||
			   version != kInputReplayVersion)) {
		runtime::LogRuntimeFailure("InputReplay", "ReadReplayCapture.Header", "invalid replay header");
		ok = false;
	}

	InputReplayCapture capture{};
	size_t expectedFrameCount = 0;
	if (ok) {
		std::string key;
		while (ok && stream >> key) {
			if (key == "snapshot_path") {
				std::string snapshotPathString;
				stream >> std::quoted(snapshotPathString);
				capture.snapshotPath = std::filesystem::path(snapshotPathString);
			} else if (key == "camera_pos") {
				stream >> capture.initialCamera.position[0] >> capture.initialCamera.position[1] >> capture.initialCamera.position[2];
			} else if (key == "camera_angles") {
				stream >> capture.initialCamera.yawRadians >> capture.initialCamera.pitchRadians;
			} else if (key == "camera_move") {
				stream >> capture.initialCamera.moveSpeed >> capture.initialCamera.mouseSensitivity >> capture.initialCamera.verticalFovRadians >> capture.initialCamera.nearPlane >> capture.initialCamera.farPlane;
			} else if (key == "camera_control_mode") {
				int controlMode = 0;
				stream >> controlMode;
				capture.initialCamera.controlMode = static_cast<CameraState::ControlMode>(controlMode);
			} else if (key == "interaction") {
				int placementMaterial = 0;
				int editorTool = 0;
				stream >> placementMaterial >> capture.initialInteraction.maxInteractionDistance >> editorTool;
				capture.initialInteraction.placementMaterial = static_cast<VoxelMaterial>(placementMaterial);
				capture.initialInteraction.editorTool = static_cast<DebugEditorTool>(editorTool);
			} else if (key == "walk") {
				int walkAirControlMode = 0;
				int autoJumpEnabled = 0;
				int autoJumpDelayEnabled = 0;
				stream >> walkAirControlMode >> autoJumpEnabled >> autoJumpDelayEnabled;
				(void)autoJumpDelayEnabled;
				capture.walkAirControlMode = static_cast<WalkAirControlMode>(walkAirControlMode);
				capture.walkAutoJumpEnabled = autoJumpEnabled != 0;
				capture.walkAutoJumpDelayEnabled = false;
			} else if (key == "frame_count") {
				stream >> expectedFrameCount;
				capture.frames.reserve(expectedFrameCount);
			} else if (key == "frame") {
				InputReplayFrame frame{};
				int removePressed = 0;
				int placePressed = 0;
				stream >> frame.deltaSeconds >> frame.mouseDeltaX >> frame.mouseDeltaY >> frame.actionDownMask >> frame.actionPressedMask >> removePressed >> placePressed;
				frame.removePressed = removePressed != 0;
				frame.placePressed = placePressed != 0;
				capture.frames.push_back(frame);
			} else {
				runtime::LogRuntimeFailure("InputReplay", "ReadReplayCapture.Token", key);
				ok = false;
			}

			if (ok && !stream.good() && !stream.eof()) {
				runtime::LogRuntimeFailure("InputReplay", "ReadReplayCapture.Parse", "failed to parse replay payload");
				ok = false;
			}
		}

		if (ok && !stream.eof()) {
			runtime::LogRuntimeFailure("InputReplay", "ReadReplayCapture.Stream", "unexpected stream state");
			ok = false;
		}
		if (ok && capture.snapshotPath.empty()) {
			runtime::LogRuntimeFailure("InputReplay", "ReadReplayCapture.SnapshotPath", "snapshot path is empty");
			ok = false;
		}
		if (ok && expectedFrameCount != 0 && expectedFrameCount != capture.frames.size()) {
			runtime::LogRuntimeFailure("InputReplay", "ReadReplayCapture.FrameCount", "frame count mismatch");
			ok = false;
		}
	}

	if (ok) {
		*outCapture = std::move(capture);
	}
	return ok;
}
} // namespace

std::string GetInputReplayPath()
{
	return GetResolvedInputReplayPath().string();
}

std::string GetInputReplaySnapshotPath()
{
	return GetResolvedInputReplaySnapshotPath().string();
}

bool SaveInputReplayCapture(const InputReplayCapture &capture, const std::string_view replayPath)
{
	const std::filesystem::path path = replayPath.empty() ? GetResolvedInputReplayPath() : std::filesystem::path(replayPath);
	if (!EnsureParentDirectoryExists(path)) {
		return false;
	}

	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream.is_open()) {
		runtime::LogRuntimeFailure("InputReplay", "SaveInputReplayCapture.Open", path.string());
		return false;
	}

	if (!WriteReplayCapture(stream, capture)) {
		runtime::LogRuntimeFailure("InputReplay", "SaveInputReplayCapture.Write", path.string());
		return false;
	}

	return true;
}

bool LoadInputReplayCapture(const std::string_view replayPath, InputReplayCapture *outCapture)
{
	const std::filesystem::path path = replayPath.empty() ? GetResolvedInputReplayPath() : std::filesystem::path(replayPath);
	std::ifstream stream(path, std::ios::binary);
	if (!stream.is_open()) {
		runtime::LogRuntimeFailure("InputReplay", "LoadInputReplayCapture.Open", path.string());
		return false;
	}

	return ReadReplayCapture(stream, outCapture);
}

bool StartInputReplayRecording(
	InputState *input,
	const VoxelWorld &world,
	const CameraState &camera,
	const InteractionState &interaction,
	const WalkAirControlMode walkAirControlMode,
	const bool walkAutoJumpEnabled)
{
	if (!input) {
		runtime::LogRuntimeFailure("InputReplay", "StartInputReplayRecording.Input", "input is null");
		return false;
	}

	InputReplayCapture capture{};
	capture.snapshotPath = std::filesystem::path(GetInputReplaySnapshotPath());
	capture.initialCamera = camera;
	capture.initialInteraction = interaction;
	capture.walkAirControlMode = walkAirControlMode;
	capture.walkAutoJumpEnabled = walkAutoJumpEnabled;
	capture.walkAutoJumpDelayEnabled = false;
	capture.frames.reserve(512);

	const auto saveResult = SaveVoxelWorldSnapshot(world, capture.snapshotPath.string());
	if (!saveResult.has_value()) {
		runtime::LogRuntimeFailure(
			"InputReplay",
			"StartInputReplayRecording.SaveSnapshot",
			capture.snapshotPath.string() + " (variant=" + std::string{toString(saveResult.error())} + ")");
		return false;
	}

	input->replay.capture = std::move(capture);
	input->replay.replayPath = std::filesystem::path(GetInputReplayPath());
	input->replay.recording = true;
	input->replay.playbackRequested = false;
	input->replay.playbackActive = false;
	input->replay.captureAvailable = false;
	input->replay.playbackFrameIndex = 0;
	input->replay.pendingMouseDeltaX = 0.0f;
	input->replay.pendingMouseDeltaY = 0.0f;
	input->replay.pendingActionPressedMask = 0ull;
	input->replay.pendingRemovePressed = false;
	input->replay.pendingPlacePressed = false;
	SDL_Log(
		"[ProjectV][InputReplay] recording started replay=%s snapshot=%s",
		input->replay.replayPath.string().c_str(),
		input->replay.capture.snapshotPath.string().c_str());
	return true;
}

bool StopInputReplayRecording(InputState *input)
{
	if (!input) {
		runtime::LogRuntimeFailure("InputReplay", "StopInputReplayRecording.Input", "input is null");
		return false;
	}
	if (!input->replay.recording) {
		return true;
	}

	if (!SaveInputReplayCapture(input->replay.capture, input->replay.replayPath.string())) {
		return false;
	}

	input->replay.recording = false;
	input->replay.captureAvailable = true;
	SDL_Log(
		"[ProjectV][InputReplay] recording saved replay=%s frames=%zu",
		input->replay.replayPath.string().c_str(),
		input->replay.capture.frames.size());
	return true;
}

void AccumulateInputReplayPending(InputState *input)
{
	if (!input || !input->replay.recording) {
		return;
	}
	input->replay.pendingMouseDeltaX += input->mouseDeltaX;
	input->replay.pendingMouseDeltaY += input->mouseDeltaY;
	input->replay.pendingActionPressedMask |= GetInputActionPressedMask(*input);
	input->replay.pendingRemovePressed = input->replay.pendingRemovePressed || input->removePressed;
	input->replay.pendingPlacePressed = input->replay.pendingPlacePressed || input->placePressed;
}

void RecordInputReplaySimTick(InputState *input, const float fixedDeltaSeconds)
{
	if (!input || !input->replay.recording) {
		return;
	}

	input->replay.capture.frames.push_back({
		.deltaSeconds = fixedDeltaSeconds,
		.mouseDeltaX = input->replay.pendingMouseDeltaX,
		.mouseDeltaY = input->replay.pendingMouseDeltaY,
		.actionDownMask = GetInputActionDownMask(*input),
		.actionPressedMask = input->replay.pendingActionPressedMask,
		.removePressed = input->replay.pendingRemovePressed,
		.placePressed = input->replay.pendingPlacePressed,
	});
	input->mouseDeltaX = input->replay.pendingMouseDeltaX; // feed look for this tick only
	input->mouseDeltaY = input->replay.pendingMouseDeltaY;
	input->replay.pendingMouseDeltaX = 0.0f;
	input->replay.pendingMouseDeltaY = 0.0f;
	input->replay.pendingActionPressedMask = 0ull;
	input->replay.pendingRemovePressed = false;
	input->replay.pendingPlacePressed = false;
}

bool ArmInputReplayCapture(InputState &input, const std::string &replayPath); // defined below

bool LoadLatestInputReplay(InputState &input)
{
	const std::string replayPath = GetInputReplayPath();
	if (!ArmInputReplayCapture(input, replayPath)) {
		runtime::LogRuntimeFailure("InputReplay", "LoadLatestInputReplay.Load", replayPath);
		return false;
	}
	return true;
}

bool EnvTokenIsTruthy(const char *value)
{
	if (value == nullptr || *value == '\0') {
		return false;
	}
	return std::strcmp(value, "1") == 0 || std::strcmp(value, "true") == 0 ||
		   std::strcmp(value, "TRUE") == 0 || std::strcmp(value, "yes") == 0 ||
		   std::strcmp(value, "YES") == 0 || std::strcmp(value, "on") == 0 ||
		   std::strcmp(value, "ON") == 0;
}

bool EnvTokenIsFalsy(const char *value)
{
	if (value == nullptr || *value == '\0') {
		return true;
	}
	return std::strcmp(value, "0") == 0 || std::strcmp(value, "false") == 0 ||
		   std::strcmp(value, "FALSE") == 0 || std::strcmp(value, "no") == 0 ||
		   std::strcmp(value, "NO") == 0 || std::strcmp(value, "off") == 0 ||
		   std::strcmp(value, "OFF") == 0;
}

bool LooksLikeReplayPath(const char *value)
{
	if (value == nullptr || *value == '\0') {
		return false;
	}
	return std::strchr(value, '\\') != nullptr || std::strchr(value, '/') != nullptr ||
		   std::strstr(value, ".replay") != nullptr;
}

bool ArmInputReplayCapture(InputState &input, const std::string &replayPath)
{
	InputReplayCapture capture{};
	if (!LoadInputReplayCapture(replayPath, &capture)) {
		return false;
	}
	input.replay.capture = std::move(capture);
	input.replay.replayPath = std::filesystem::path(replayPath);
	input.replay.captureAvailable = true;
	input.replay.playbackRequested = true;
	input.replay.playbackActive = false;
	input.replay.playbackFrameIndex = 0;
	input.replay.pendingMouseDeltaX = 0.0f;
	input.replay.pendingMouseDeltaY = 0.0f;
	input.replay.pendingActionPressedMask = 0ull;
	input.replay.pendingRemovePressed = false;
	input.replay.pendingPlacePressed = false;
	return true;
}

void ConfigureInputReplayFromEnvironment(InputState &input)
{
	const char *const quitValue = projectv::core::GetEnvVar("PROJECTV_INPUT_REPLAY_QUIT");
	input.replay.quitWhenPlaybackDone = EnvTokenIsTruthy(quitValue);

	const char *const autoplayValue = projectv::core::GetEnvVar("PROJECTV_INPUT_REPLAY_AUTOPLAY");
	if (EnvTokenIsFalsy(autoplayValue)) {
		return; // unset / 0 / false / off — no autoplay
	}

	// AUTOPLAY=1|true|on OR a filesystem path to a .replay (path was previously ignored).
	const std::string replayPath =
		LooksLikeReplayPath(autoplayValue) ? std::string{autoplayValue} : GetInputReplayPath();
	if (!ArmInputReplayCapture(input, replayPath)) {
		SDL_Log("[ProjectV][InputReplay] AUTOPLAY requested but failed to load %s", replayPath.c_str());
		return;
	}
	SDL_Log(
		"[ProjectV][InputReplay] AUTOPLAY armed replay=%s frames=%zu quit=%s",
		input.replay.replayPath.string().c_str(),
		input.replay.capture.frames.size(),
		input.replay.quitWhenPlaybackDone ? "true" : "false");
}

void ApplyInputReplayFrame(
	InputState *input,
	const InputReplayFrame &frame)
{
	if (!input) {
		return;
	}

	input->mouseDeltaX = frame.mouseDeltaX;
	input->mouseDeltaY = frame.mouseDeltaY;
	input->removePressed = frame.removePressed;
	input->placePressed = frame.placePressed;
	ApplyInputActionSnapshot(*input, frame.actionDownMask, frame.actionPressedMask);
}

bool ApplyNextInputReplaySimTick(InputState *input)
{
	if (!input || !input->replay.playbackActive) {
		return false;
	}
	auto &replay = input->replay;
	if (replay.playbackFrameIndex >= replay.capture.frames.size()) {
		return false;
	}

	const InputReplayFrame &frame = replay.capture.frames[replay.playbackFrameIndex++];
	ApplyInputReplayFrame(input, frame);
	if (replay.playbackFrameIndex == 1u ||
		replay.playbackFrameIndex == replay.capture.frames.size() ||
		(replay.playbackFrameIndex % 120u) == 0u) {
		SDL_Log(
			"[ProjectV][InputReplay] progress %zu/%zu",
			replay.playbackFrameIndex,
			replay.capture.frames.size());
	}
	return true;
}

void StopInputReplayPlayback(InputState *input)
{
	if (!input) {
		return;
	}

	const bool wasPlaying = input->replay.playbackActive;
	input->replay.playbackRequested = false;
	input->replay.playbackActive = false;
	input->replay.playbackFrameIndex = 0;
	input->replay.pendingMouseDeltaX = 0.0f;
	input->replay.pendingMouseDeltaY = 0.0f;
	input->replay.pendingActionPressedMask = 0ull;
	input->replay.pendingRemovePressed = false;
	input->replay.pendingPlacePressed = false;
	ResetReplayFrameApplication(*input);
	if (wasPlaying) {
		SDL_Log("[ProjectV][InputReplay] playback finished");
	}
}

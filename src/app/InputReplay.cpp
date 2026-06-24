#include "app/InputReplay.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "app/InputActions.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "voxel/VoxelWorld.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {
constexpr std::string_view kInputReplayMagic = "PROJECTV_INPUT_REPLAY";
constexpr int kInputReplayVersion = 3;

std::filesystem::path GetInputReplayDirectoryPath()
{
	if (const char *overrideDirectory = SDL_getenv("PROJECTV_INPUT_REPLAY_DIR");
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
	if (const char *overridePath = SDL_getenv("PROJECTV_INPUT_REPLAY_PATH");
		overridePath && *overridePath) {
		return overridePath;
	}

	return GetInputReplayDirectoryPath() / "latest.projectv.replay";
}

std::filesystem::path GetResolvedInputReplaySnapshotPath()
{
	if (const char *overridePath = SDL_getenv("PROJECTV_INPUT_REPLAY_SNAPSHOT_PATH");
		overridePath && *overridePath) {
		return overridePath;
	}

	return GetInputReplayDirectoryPath() / "latest.projectv.replay.snapshot.bin";
}

bool EnsureParentDirectoryExists(const std::filesystem::path &path, const std::string_view step)
{
	const std::filesystem::path parentPath = path.parent_path();
	if (parentPath.empty()) {
		return true;
	}

	std::error_code error;
	std::filesystem::create_directories(parentPath, error);
	if (error) {
		runtime::LogRuntimeFailure("InputReplay", step, error.message());
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
		   << static_cast<int>(capture.walkAutoJumpDelayEnabled) << '\n';
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
	if (!outCapture) {
		runtime::LogRuntimeFailure("InputReplay", "ReadReplayCapture.OutCapture", "outCapture is null");
		return false;
	}

	std::string magic;
	int version = 0;
	if (!(stream >> magic >> version) ||
		magic != kInputReplayMagic ||
		(version != 1 && version != 2 && version != kInputReplayVersion)) {
		runtime::LogRuntimeFailure("InputReplay", "ReadReplayCapture.Header", "invalid replay header");
		return false;
	}

	InputReplayCapture capture{};
	size_t expectedFrameCount = 0;
	std::string key;
	while (stream >> key) {
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
			std::string walkLine;
			std::getline(stream >> std::ws, walkLine);
			std::istringstream walkStream(walkLine);
			int walkAirControlMode = 0;
			int autoJumpEnabled = version == 1 ? 1 : 0;
			int autoJumpDelayEnabled = 0;
			walkStream >> walkAirControlMode;
			if (version == 1) {
				walkStream >> autoJumpDelayEnabled;
			} else {
				walkStream >> autoJumpEnabled >> autoJumpDelayEnabled;
			}
			capture.walkAirControlMode = static_cast<WalkAirControlMode>(walkAirControlMode);
			capture.walkAutoJumpEnabled = autoJumpEnabled != 0;
			capture.walkAutoJumpDelayEnabled = autoJumpDelayEnabled != 0;
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
			return false;
		}

		if (!stream.good() && !stream.eof()) {
			runtime::LogRuntimeFailure("InputReplay", "ReadReplayCapture.Parse", "failed to parse replay payload");
			return false;
		}
	}

	if (!stream.eof()) {
		runtime::LogRuntimeFailure("InputReplay", "ReadReplayCapture.Stream", "unexpected stream state");
		return false;
	}
	if (capture.snapshotPath.empty()) {
		runtime::LogRuntimeFailure("InputReplay", "ReadReplayCapture.SnapshotPath", "snapshot path is empty");
		return false;
	}
	if (expectedFrameCount != 0 && expectedFrameCount != capture.frames.size()) {
		runtime::LogRuntimeFailure("InputReplay", "ReadReplayCapture.FrameCount", "frame count mismatch");
		return false;
	}

	*outCapture = std::move(capture);
	return true;
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
	if (!EnsureParentDirectoryExists(path, "SaveInputReplayCapture.CreateDirectories")) {
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
	const bool walkAutoJumpEnabled,
	const bool walkAutoJumpDelayEnabled)
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
	capture.walkAutoJumpDelayEnabled = walkAutoJumpDelayEnabled;
	capture.frames.reserve(512);

	if (!EnsureParentDirectoryExists(capture.snapshotPath, "StartInputReplayRecording.CreateDirectories")) {
		return false;
	}
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

void RecordInputReplayFrame(
	InputState *input,
	const float deltaSeconds)
{
	if (!input || !input->replay.recording) {
		return;
	}

	input->replay.capture.frames.push_back({
		.deltaSeconds = deltaSeconds,
		.mouseDeltaX = input->mouseDeltaX,
		.mouseDeltaY = input->mouseDeltaY,
		.actionDownMask = GetInputActionDownMask(*input),
		.actionPressedMask = GetInputActionPressedMask(*input),
		.removePressed = input->removePressed,
		.placePressed = input->placePressed,
	});
}

bool LoadLatestInputReplay(InputState *input)
{
	if (!input) {
		runtime::LogRuntimeFailure("InputReplay", "LoadLatestInputReplay.Input", "input is null");
		return false;
	}

	InputReplayCapture capture{};
	const std::string replayPath = GetInputReplayPath();
	if (!LoadInputReplayCapture(replayPath, &capture)) {
		return false;
	}

	input->replay.capture = std::move(capture);
	input->replay.replayPath = std::filesystem::path(replayPath);
	input->replay.captureAvailable = true;
	input->replay.playbackRequested = true;
	input->replay.playbackActive = false;
	input->replay.playbackFrameIndex = 0;
	return true;
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

bool PrepareNextInputReplayPlaybackFrame(
	InputState *input,
	SimulationState *simulation)
{
	if (!input || !simulation || !input->replay.playbackActive) {
		return false;
	}
	if (input->replay.playbackFrameIndex >= input->replay.capture.frames.size()) {
		return false;
	}

	const InputReplayFrame &frame = input->replay.capture.frames[input->replay.playbackFrameIndex++];
	ApplyInputReplayFrame(input, frame);

	const Uint64 frequency = SDL_GetPerformanceFrequency();
	const Uint64 deltaCounter = std::max<Uint64>(
		1,
		static_cast<Uint64>(static_cast<double>(std::max(frame.deltaSeconds, 0.0f)) * static_cast<double>(frequency)));
	simulation->lastFrameCounter = SDL_GetPerformanceCounter() - deltaCounter;
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
	ResetReplayFrameApplication(*input);
	if (wasPlaying) {
		SDL_Log("[ProjectV][InputReplay] playback finished");
	}
}

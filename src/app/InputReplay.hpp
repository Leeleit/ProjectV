#ifndef INPUT_REPLAY_HPP
#define INPUT_REPLAY_HPP

#include "core/Types.hpp"

#include <string_view>

std::string GetInputReplayPath();
std::string GetInputReplaySnapshotPath();
bool SaveInputReplayCapture(const InputReplayCapture &capture, std::string_view replayPath);
bool LoadInputReplayCapture(std::string_view replayPath, InputReplayCapture *outCapture);
bool StartInputReplayRecording(
	InputState *input,
	const VoxelWorld &world,
	const CameraState &camera,
	const InteractionState &interaction,
	WalkAirControlMode walkAirControlMode,
	bool walkAutoJumpDelayEnabled);
bool StopInputReplayRecording(InputState *input);
void RecordInputReplayFrame(
	InputState *input,
	float deltaSeconds);
bool LoadLatestInputReplay(InputState *input);
void ApplyInputReplayFrame(
	InputState *input,
	const InputReplayFrame &frame);
bool PrepareNextInputReplayPlaybackFrame(
	InputState *input,
	SimulationState *simulation);
void StopInputReplayPlayback(InputState *input);

#endif

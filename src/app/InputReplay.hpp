#pragma once

#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

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
	bool walkAutoJumpEnabled);
bool StopInputReplayRecording(InputState *input);
void RecordInputReplayFrame(
	InputState *input,
	float deltaSeconds);
bool LoadLatestInputReplay(InputState &input);
void ApplyInputReplayFrame(
	InputState *input,
	const InputReplayFrame &frame);
bool PrepareNextInputReplayPlaybackFrame(
	InputState *input,
	SimulationState *simulation);
void StopInputReplayPlayback(InputState *input);


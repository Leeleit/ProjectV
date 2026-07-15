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
void AccumulateInputReplayPending(InputState *input);					   // call once per render frame while recording
void RecordInputReplaySimTick(InputState *input, float fixedDeltaSeconds); // one sample per sim tick
bool LoadLatestInputReplay(InputState &input);
void ConfigureInputReplayFromEnvironment(InputState &input);
void ApplyInputReplayFrame(
	InputState *input,
	const InputReplayFrame &frame);
bool ApplyNextInputReplaySimTick(InputState *input); // one sample per sim tick during playback
void StopInputReplayPlayback(InputState *input);

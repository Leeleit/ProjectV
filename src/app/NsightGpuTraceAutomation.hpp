#pragma once

#include "core/Types.hpp"

#include <cstddef>

void ConfigureNsightGpuTraceAutomationFromEnvironment(
	NsightGpuTraceAutomationState *automation);
void InitializeNsightGpuTraceAutomationBeforeVulkan(
	NsightGpuTraceAutomationState *automation);
void TryStartNsightGpuTraceAtReplayTick(
	NsightGpuTraceAutomationState *automation,
	size_t replayTick);

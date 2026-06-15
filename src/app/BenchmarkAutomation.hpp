#pragma once

#include "core/Types.hpp"

void ConfigureBenchmarkAutomationFromEnvironment(BenchmarkAutomationState *state);
bool UpdateBenchmarkAutomation(
	BenchmarkAutomationState *state,
	const DebugStats &debugStats,
	Uint64 frameCounter);


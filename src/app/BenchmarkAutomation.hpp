#pragma once

#include "core/Types.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

void ConfigureBenchmarkAutomationFromEnvironment(BenchmarkAutomationState *state);
[[nodiscard]] bool UpdateBenchmarkAutomation(
	BenchmarkAutomationState *state,
	const DebugStats &debugStats,
	Uint64 frameCounter);


#include "app/BenchmarkAutomation.hpp"

#include "core/Types.hpp"

#include "SDL3/SDL.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
constexpr char kBenchmarkFramesEnvVar[] = "PROJECTV_BENCHMARK_FRAMES";
constexpr char kBenchmarkWarmupFramesEnvVar[] = "PROJECTV_BENCHMARK_WARMUP_FRAMES";
constexpr char kBenchmarkQuitEnvVar[] = "PROJECTV_BENCHMARK_QUIT";
constexpr char kBenchmarkLogEveryEnvVar[] = "PROJECTV_BENCHMARK_LOG_EVERY";
constexpr uint32_t kDefaultBenchmarkWarmupFrames = 30u;
constexpr uint32_t kDefaultBenchmarkLogEvery = 60u;

bool IsValueSeparator(const char value)
{
	return value == ' ' || value == '\t' || value == '\n' || value == '\r' || value == ',' || value == ';';
}

bool TryParseUnsigned(const char *text, uint32_t *outValue)
{
	if (!text || !*text) {
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

uint32_t ReadUnsignedEnvironment(const char *name, const uint32_t fallbackValue)
{
	const char *text = SDL_getenv(name);
	if (!text || !*text) {
		return fallbackValue;
	}

	uint32_t value = fallbackValue;
	if (!TryParseUnsigned(text, &value)) {
		SDL_Log("[ProjectV][BenchmarkAutomation] ignored invalid %s='%s'", name, text);
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

	std::string token;
	token.reserve(std::strlen(text));
	for (const char *c = text; *c != '\0'; ++c) {
		if (*c == ' ' || *c == '\t' || *c == '\n' || *c == '\r') {
			continue;
		}
		token.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*c))));
	}
	return token == "1" || token == "true" || token == "yes" || token == "on";
}

void LogBenchmarkSummary(
	const BenchmarkAutomationState &state,
	const DebugStats &debugStats)
{
	if (state.framesRendered == 0) {
		return;
	}

	const double meanSeconds = static_cast<double>(state.totalFrameSeconds) / static_cast<double>(state.framesRendered);
	const double meanFps = meanSeconds > 0.0 ? 1.0 / meanSeconds : 0.0;
	const double minFps = state.maxFrameSeconds > 0.0f ? 1.0 / static_cast<double>(state.maxFrameSeconds) : 0.0;
	const double maxFps = state.minFrameSeconds < 1e20f ? 1.0 / static_cast<double>(state.minFrameSeconds) : 0.0;

	SDL_Log(
		"[ProjectV][BenchmarkAutomation] done frames=%u mean_ms=%.3f min_ms=%.3f max_ms=%.3f mean_fps=%.2f min_fps=%.2f max_fps=%.2f tris=%u nonair=%u",
		state.framesRendered,
		meanSeconds * 1000.0,
		static_cast<double>(state.minFrameSeconds) * 1000.0,
		static_cast<double>(state.maxFrameSeconds) * 1000.0,
		meanFps,
		minFps,
		maxFps,
		debugStats.sceneTriangleCount,
		debugStats.nonAirVoxelCount);
}
} // namespace

void ConfigureBenchmarkAutomationFromEnvironment(BenchmarkAutomationState *state)
{
	if (!state) {
		return;
	}

	*state = {};

	const uint32_t targetFrames = ReadUnsignedEnvironment(kBenchmarkFramesEnvVar, 0u);
	if (targetFrames == 0u) {
		return;
	}

	state->active = true;
	state->targetFrameCount = targetFrames;
	state->warmupFramesRemaining =
		ReadUnsignedEnvironment(kBenchmarkWarmupFramesEnvVar, kDefaultBenchmarkWarmupFrames);
	state->logEveryFrames = std::max<uint32_t>(
		1u,
		ReadUnsignedEnvironment(kBenchmarkLogEveryEnvVar, kDefaultBenchmarkLogEvery));
	state->quitWhenDone = ReadBoolEnvironment(kBenchmarkQuitEnvVar);
	state->minFrameSeconds = 1e30f;
	state->maxFrameSeconds = 0.0f;
	state->totalFrameSeconds = 0.0f;
	state->framesRendered = 0u;
	state->startCounter = SDL_GetPerformanceCounter();
	state->firstFrameCounter = 0u;
	state->lastFrameCounter = state->startCounter;

	SDL_Log(
		"[ProjectV][BenchmarkAutomation] armed target_frames=%u warmup=%u log_every=%u quit=%s",
		state->targetFrameCount,
		state->warmupFramesRemaining,
		state->logEveryFrames,
		state->quitWhenDone ? "true" : "false");
}

bool UpdateBenchmarkAutomation(
	BenchmarkAutomationState *state,
	const DebugStats &debugStats,
	const Uint64 frameCounter)
{
	if (!state || !state->active || state->completed) {
		return false;
	}

	if (state->warmupFramesRemaining > 0u) {
		--state->warmupFramesRemaining;
		state->lastFrameCounter = frameCounter;
		if (state->warmupFramesRemaining == 0u) {
			state->firstFrameCounter = frameCounter;
			SDL_Log("[ProjectV][BenchmarkAutomation] warmup complete, starting measurement");
		}
		return false;
	}

	if (state->framesRendered >= state->targetFrameCount) {
		state->completed = true;
		LogBenchmarkSummary(*state, debugStats);
		return state->quitWhenDone;
	}

	const Uint64 deltaCounter = frameCounter - state->lastFrameCounter;
	state->lastFrameCounter = frameCounter;
	const Uint64 frequency = SDL_GetPerformanceFrequency();
	const float frameSeconds = frequency > 0
								   ? static_cast<float>(deltaCounter) / static_cast<float>(frequency)
								   : 0.0f;

	state->totalFrameSeconds += frameSeconds;
	state->minFrameSeconds = std::min(state->minFrameSeconds, frameSeconds);
	state->maxFrameSeconds = std::max(state->maxFrameSeconds, frameSeconds);
	++state->framesRendered;

	if (state->framesRendered % state->logEveryFrames == 0u) {
		const double meanSeconds = state->totalFrameSeconds / static_cast<double>(state->framesRendered);
		SDL_Log(
			"[ProjectV][BenchmarkAutomation] progress %u/%u mean_ms=%.3f last_ms=%.3f fps_now=%.2f",
			state->framesRendered,
			state->targetFrameCount,
			meanSeconds * 1000.0,
			static_cast<double>(frameSeconds) * 1000.0,
			frameSeconds > 0.0f ? 1.0 / static_cast<double>(frameSeconds) : 0.0);
	}

	if (state->framesRendered >= state->targetFrameCount) {
		state->completed = true;
		LogBenchmarkSummary(*state, debugStats);
		return state->quitWhenDone;
	}

	return false;
}

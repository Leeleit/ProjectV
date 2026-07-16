#include "app/NsightGpuTraceAutomation.hpp"

#include "core/EnvUtils.hpp"

#include "SDL3/SDL_log.h"

#include <cerrno>
#include <climits>
#include <cstdlib>

#ifndef PROJECTV_HAS_NGFX_GPU_TRACE_SDK
#define PROJECTV_HAS_NGFX_GPU_TRACE_SDK 0
#endif

#if PROJECTV_HAS_NGFX_GPU_TRACE_SDK
#include <NGFX_GPUTrace_Vulkan.h>
#endif

namespace {

constexpr char kReplayTickEnvVar[] = "PROJECTV_NGFX_GPU_TRACE_REPLAY_TICK";

bool TryParsePositiveUint32(
	const char *const text,
	uint32_t *const outValue)
{
	if (text == nullptr || *text == '\0' || outValue == nullptr || *text == '-') {
		return false;
	}
	errno = 0;
	char *end = nullptr;
	const unsigned long parsed = std::strtoul(text, &end, 10);
	if (end == text || *end != '\0' || errno == ERANGE || parsed == 0ul ||
		parsed > UINT_MAX) {
		return false;
	}
	*outValue = static_cast<uint32_t>(parsed);
	return true;
}

} // namespace

void ConfigureNsightGpuTraceAutomationFromEnvironment(
	NsightGpuTraceAutomationState *const automation)
{
	if (automation == nullptr) {
		return;
	}
	*automation = {};
	const char *const value = projectv::core::GetEnvVar(kReplayTickEnvVar);
	if (value == nullptr || *value == '\0') {
		return;
	}
	uint32_t targetReplayTick = 0u;
	if (!TryParsePositiveUint32(value, &targetReplayTick)) {
		SDL_Log(
			"[ProjectV][NgfxGpuTrace] ignored invalid %s='%s'",
			kReplayTickEnvVar,
			value);
		return;
	}
#if PROJECTV_HAS_NGFX_GPU_TRACE_SDK
	automation->targetReplayTick = targetReplayTick;
	automation->active = true;
	SDL_Log(
		"[ProjectV][NgfxGpuTrace] armed replay_tick=%u; use Nsight GPU Trace Start After=NGFX SDK Start",
		targetReplayTick);
#else
	SDL_Log(
		"[ProjectV][NgfxGpuTrace] ignored %s=%u because this build lacks the Nsight Graphics SDK",
		kReplayTickEnvVar,
		targetReplayTick);
#endif
}

void InitializeNsightGpuTraceAutomationBeforeVulkan(
	NsightGpuTraceAutomationState *const automation)
{
	if (automation == nullptr || !automation->active) {
		return;
	}
#if PROJECTV_HAS_NGFX_GPU_TRACE_SDK
	NGFX_GPUTrace_InitializeActivity_Vulkan_Params params{
		NGFX_GPUTrace_InitializeActivity_Vulkan_Params_VER};
	const NGFX_Result result = NGFX_GPUTrace_InitializeActivity_Vulkan(&params);
	if (result != NGFX_Result_Success) {
		SDL_Log(
			"[ProjectV][NgfxGpuTrace] initialization unavailable result=%d; launch from attached Nsight Graphics",
			static_cast<int>(result));
		automation->active = false;
		return;
	}
	automation->activityInitialized = true;
#endif
}

void TryStartNsightGpuTraceAtReplayTick(
	NsightGpuTraceAutomationState *const automation,
	const size_t replayTick)
{
	if (automation == nullptr || !automation->active || !automation->activityInitialized ||
		automation->traceStartAttempted || replayTick < automation->targetReplayTick) {
		return;
	}
	automation->traceStartAttempted = true;
#if PROJECTV_HAS_NGFX_GPU_TRACE_SDK
	NGFX_GPUTrace_StartTrace_Vulkan_Params params{
		NGFX_GPUTrace_StartTrace_Vulkan_Params_VER};
	const NGFX_Result result = NGFX_GPUTrace_StartTrace_Vulkan(&params);
	if (result == NGFX_Result_Success) {
		SDL_Log(
			"[ProjectV][NgfxGpuTrace] started at replay_tick=%zu target_tick=%u",
			replayTick,
			automation->targetReplayTick);
		return;
	}
	SDL_Log(
		"[ProjectV][NgfxGpuTrace] start failed result=%d replay_tick=%zu",
		static_cast<int>(result),
		replayTick);
#endif
}

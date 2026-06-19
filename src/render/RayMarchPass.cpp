#include "render/RayMarchPass.hpp"

#include <cstdio>

namespace projectv::render {

namespace {

struct RayMarchState {
	bool enabled = false;
	bool recreatePending = true;
};

RayMarchState &MutableRayMarchState()
{
	static RayMarchState state;
	return state;
}

} // namespace

void SetRayMarchEnabled(const bool enabled)
{
	auto &[isEnabled, recreatePending] = MutableRayMarchState();
	if (isEnabled) {
		return;
	}
	isEnabled = enabled;
	recreatePending = true;
}

bool IsRayMarchEnabled()
{
	return MutableRayMarchState().enabled;
}

void RequestRayMarchPipelineRecreate()
{
	MutableRayMarchState().recreatePending = true;
}

bool IsRayMarchPipelineRecreatePending()
{
	auto &[enabled, recreatePending] = MutableRayMarchState();
	if (recreatePending) {
		recreatePending = false;
		return true;
	}
	return false;
}

void RecordRayMarchCommands(const VulkanContextState &context, const FrameRenderData &frameData)
{
	const auto &[enabled, recreatePending] = MutableRayMarchState();
	if (!enabled) {
		return;
	}
	if (context.device == VK_NULL_HANDLE) {
		return;
	}

	std::fprintf(
		stderr,
		"[ProjectV][RayMarch] RecordRayMarchCommands invoked (deferred Phase 7 follow-up: shader is compiled, pipeline / offscreen target / composite are the next slice)\n");
}

} // namespace projectv::render

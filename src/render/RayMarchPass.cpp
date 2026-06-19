#include "render/RayMarchPass.hpp"

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

} // namespace projectv::render

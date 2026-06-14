#include "render/RayMarchPass.hpp"

#include "core/RuntimeDiagnostics.hpp"

#include <cstdio>

namespace projectv::render {

namespace {

// **State for the ray-march pass (defense r0, 2026-06-13).** A single
// boolean flag pair lives in static storage so the main-thread `main.cpp`
// and any future render-thread dispatcher share the same view without
// touching `core/Types.hpp` (which is mid-edit under
// `session-2026-06-13-hardcore-perf-r0`).
struct RayMarchState {
	bool enabled = false;
	bool recreatePending = true;
};

RayMarchState &MutableRayMarchState()
{
	static RayMarchState state;
	return state;
}

}  // namespace

void SetRayMarchEnabled(const bool enabled)
{
	auto &state = MutableRayMarchState();
	if (state.enabled == enabled) {
		return;
	}
	state.enabled = enabled;
	state.recreatePending = true;
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
	auto &state = MutableRayMarchState();
	if (state.recreatePending) {
		state.recreatePending = false;
		return true;
	}
	return false;
}

void RecordRayMarchCommands(const VulkanContextState &context, const FrameRenderData &frameData)
{
	const auto &state = MutableRayMarchState();
	if (!state.enabled) {
		return;
	}
	if (context.device == VK_NULL_HANDLE) {
		return;
	}

	// **Phase 7 follow-up (defense r0, 2026-06-13).** The full Vulkan
	// implementation binds the `ray_march.comp.spv` shader, allocates an
	// offscreen RGBA8 storage image sized to the swapchain, and dispatches
	// a 8x8x1 compute kernel per pixel. The current entry point emits a
	// diagnostic record so the toggle is observable in the runtime
	// output stream and the call site is not silently swallowed. See
	// `docs/DefenseReport.md §3` for the deferred-item contract.
	std::fprintf(
		stderr,
		"[ProjectV][RayMarch] RecordRayMarchCommands invoked (deferred Phase 7 follow-up: shader is compiled, pipeline / offscreen target / composite are the next slice)\n");
}

}  // namespace projectv::render

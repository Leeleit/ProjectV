#pragma once

#include <cstdint>

#include "core/Types.hpp"

namespace projectv::render {

void SetRayMarchEnabled(bool enabled);
bool IsRayMarchEnabled();

/// \brief Mark the compute pipeline for recreation.
///
/// \details
/// Called from F5 hot-reload
///  in `main.cpp` so the next frame picks up the freshly compiled

///  `ray_march.comp.spv`. No-op if the pass is currently disabled.

void RequestRayMarchPipelineRecreate();

/// \brief Returns true when the pass is enabled AND a pipeline recreate was
///
/// \details
///  requested (or no pipeline exists yet). Consumed by the renderer's

///  graphics command stream to decide whether to dispatch the compute

///  pass this frame.

bool IsRayMarchPipelineRecreatePending();

/// \brief Per-frame command recording entry point.
///
/// \details
/// Dispatches the ray-march
///  compute shader into the per-frame output image when enabled. The

///  `state` argument is opaque; pass `&state->context` to read the

///  current `VkCommandBuffer`, `VkDevice`, etc.

///  **NOTE (defense r0):** this entry point currently logs the call and

///  returns. Full Vulkan integration is a Phase 7 follow-up — see

///  `docs/DefenseReport.md §3` for the deferred-item contract. The

///  signature is final; the implementation will be back-filled without

///  breaking callers.

void RecordRayMarchCommands(const VulkanContextState &context, const FrameRenderData &frameData);

}  // namespace projectv::render


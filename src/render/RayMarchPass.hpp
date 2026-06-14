#ifndef RAY_MARCH_PASS_HPP
#define RAY_MARCH_PASS_HPP

#include <cstdint>

#include "core/Types.hpp"

namespace projectv::render {

// **Ray-march compute pass (defense r0, 2026-06-13).** A second, optional
// rendering path that re-casts the scene through a DDA ray-march over the
// packed chunk voxel payload, on top of (or in place of) the mesh-based
// primary path. Intended as the GPU ray-marching demonstration called out
// in ТЗ 4.1.2: "визуализация воксельной сцены через GPU ray-marching с
// использованием compute shaders".
//
// **Current scope:** the pass owns:
//   - a runtime `enabled` flag, toggled by F6 in `main.cpp::SDL_AppEvent`;
//   - a compute-shader source file at `src/shaders/ray_march.comp` that
//     does an Amanatides-Woo DDA through the packed voxel payload;
//   - a pipeline-recreate request flag, set by F5 hot-reload and consumed
//     at the next frame boundary.
//
// The current implementation is an **API + state contract only** — the
// `RecordRayMarchCommands` entry point logs the per-frame state and is a
// no-op until a small follow-up slice binds the compute pass into the
// graphics command stream. That follow-up is documented in
// `docs/DefenseReport.md §3` and `agent/decisions.md` (deferred to Phase 7
// — full compute pipeline + offscreen color attachment + blit to
// swapchain). The shape of the API is stable; consumers can wire their
// side of the call site now.
void SetRayMarchEnabled(bool enabled);
bool IsRayMarchEnabled();

// Mark the compute pipeline for recreation. Called from F5 hot-reload
// in `main.cpp` so the next frame picks up the freshly compiled
// `ray_march.comp.spv`. No-op if the pass is currently disabled.
void RequestRayMarchPipelineRecreate();

// Returns true when the pass is enabled AND a pipeline recreate was
// requested (or no pipeline exists yet). Consumed by the renderer's
// graphics command stream to decide whether to dispatch the compute
// pass this frame.
bool IsRayMarchPipelineRecreatePending();

// Per-frame command recording entry point. Dispatches the ray-march
// compute shader into the per-frame output image when enabled. The
// `state` argument is opaque; pass `&state->context` to read the
// current `VkCommandBuffer`, `VkDevice`, etc.
//
// **NOTE (defense r0):** this entry point currently logs the call and
// returns. Full Vulkan integration is a Phase 7 follow-up — see
// `docs/DefenseReport.md §3` for the deferred-item contract. The
// signature is final; the implementation will be back-filled without
// breaking callers.
void RecordRayMarchCommands(const VulkanContextState &context, const FrameRenderData &frameData);

}  // namespace projectv::render

#endif  // RAY_MARCH_PASS_HPP

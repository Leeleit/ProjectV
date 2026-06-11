#ifndef VULKAN_TAA_RESOLVE_PIPELINE_HPP
#define VULKAN_TAA_RESOLVE_PIPELINE_HPP

// TAA resolve pipeline. The 6th graphics pipeline in the renderer. It is
// intentionally split out of `VulkanGraphicsPipeline.cpp` so the main file
// stays focused on the opaque / transparent / shadow / debug-overlay /
// debug-HUD pipelines and the TAA-specific descriptor set + history
// bindings do not inflate it further. The `taa_resolve.{vert,frag}` shader
// pair is consumed here; everything else in the file is boilerplate
// matching the conventions of the other pipelines (fullscreen triangle
// vertex input, dynamic viewport + scissor, swapchain color format).
//
// Owned by `RenderState` (see `core/Types.hpp`):
//   - `taaResolveDescriptorSetLayout`  - set 0, 4 bindings (b0 sceneColor,
//     b1 historyColor, b2 depth, b3 sceneLighting SSBO), all FRAGMENT_BIT.
//   - `taaResolveDescriptorPool`       - per-frame `vkAllocateDescriptorSets`.
//   - `taaResolveDescriptorSets[]`     - 1 per in-flight frame (MAX_FRAMES_IN_FLIGHT).
//   - `taaResolvePipelineLayout`       - 1 descriptor set + 1 push constant
//     range 144 B FRAGMENT_BIT (matches `ResolvePushConstants`).
//   - `taaResolvePipeline`             - the fullscreen-triangle pipeline
//     object, depthTest=VK_FALSE, no depth attachment, color write mask
//     RGBA, no blend, dynamic viewport + scissor.

#include "core/Types.hpp"

struct VulkanContextState;
struct SwapchainState;

bool CreateTaaResolvePipeline(
	VulkanContextState *context,
	const SwapchainState *swapchain,
	RenderState *render);

bool RefreshTaaResolveResourceBindings(
	VulkanContextState *context,
	RenderState *render);

void DestroyTaaResolvePipeline(
	VulkanContextState *context,
	RenderState *render);

#endif

#ifndef PROJECTV_TAA_RENDER_TARGETS_HPP
#define PROJECTV_TAA_RENDER_TARGETS_HPP

#include "core/Types.hpp"

// Forward declarations. The full definitions live in `core/Types.hpp`
// (which is included above), but `TaaRenderTargets.hpp` itself is
// pulled in by `Types.hpp` *before* the `VulkanContextState`
// forward-declaration line. Re-declaring it here as a no-op forward
// declaration keeps the parameter signature well-formed and lets the
// real `struct VulkanContextState { ... }` definition later in
// `Types.hpp` merge cleanly. `VmaAllocation` is already typedef'd
// by `vk_mem_alloc.h` which is reachable through the same chain.
struct VulkanContextState;

// `VmaAllocation` is a typedef for `VmaAllocation_T*` inside
// `vk_mem_alloc.h`, but we only need the handle here (the TAA
// render-target helper never dereferences it). Re-typedef it from
// the C++ builtin so the header stays self-contained — the real
// VMA header is included by the .cpp via the `RenderState` chain.
namespace projectv::taa {
using VmaAllocationHandle = void*;
}

#include <vulkan/vulkan.h>

namespace projectv::taa {

// Single source of truth for the TAA offscreen colour format. The
// pipeline declaration in `VulkanGraphicsPipeline.cpp` and the image
// allocation in `CreateOrRecreateTaaRenderTargets` both consume this
// constant so they cannot drift.
//
// Format choice (`VK_FORMAT_B10G11R11_UFLOAT_PACK32`): 32 bits per pixel
// (4 B/pixel) vs the previous `VK_FORMAT_R16G16B16A16_SFLOAT` 64 bits
// per pixel (8 B/pixel). 2× bandwidth save on the resolve-pass read
// (`historyColor` sample) and the per-frame `vkCmdCopyImage` history
// update. The shader writes `vec4` to `outSceneColor` regardless, and
// the `B10G11R11_UFLOAT` format is unsigned-float packed-RGB with the
// alpha channel ignored on store (Vulkan spec: alpha is undefined
// for packed formats); `taa_resolve.frag` only consumes `.rgb` from
// the history sample, so the dropped alpha is a no-op for the
// resolve. The resolve output goes straight to the swapchain (B8G8R8A8
// UNORM on most desktops) — the format transition is transparent to
// the rest of the pipeline.
//
// Loss of precision vs R16G16B16A16_SFLOAT: 5 bits for B, 6 for G, 5
// for R (with shared exponent). Visible at < 0.1% intensity in dim
// areas; if banding shows up in the live look-dev captures, fall
// back by reverting to `VK_FORMAT_R16G16B16A16_SFLOAT`. Tunable via
// the captured `taa_scene_color_format` sidecar key.
inline constexpr VkFormat kTaaSceneColorFormat = VK_FORMAT_B10G11R11_UFLOAT_PACK32;

// TAA offscreen render target + linear sampler. Created / recreated in
// `VulkanSwapchain.cpp::CreateOrRecreateSwapchain` so the size stays in
// lockstep with the swapchain extent. Two identical
// `kTaaSceneColorFormat` (B10G11R11_UFLOAT_PACK32) images are allocated
// and double-buffered: the resolve pass reads from `history` while
// writing to `current`, then the post-resolve ping-pong swap (or copy)
// makes the freshly-resolved `current` the next frame's `history`.
//
// Both images start in `UNDEFINED` layout; the first caller's
// `TransitionImage` helper is responsible for moving them to
// `COLOR_ATTACHMENT_OPTIMAL` (for writes) and `SHADER_READ_ONLY_OPTIMAL`
// (for the resolve read). Recreate path lives next to the swapchain
// recreate so resize-time lifecycle is one place.
struct OffscreenColorTarget {
	VkImage image = VK_NULL_HANDLE;
	VkImageView imageView = VK_NULL_HANDLE;
	projectv::taa::VmaAllocationHandle allocation = nullptr;
};

// Build / recreate the two TAA offscreen color images + linear sampler.
// Returns false on any allocation / view creation failure; the caller
// is expected to clean up partial state and abort frame submission
// until the next successful recreate.
bool CreateOrRecreateTaaRenderTargets(
	VulkanContextState *context,
	VkExtent2D extent,
	OffscreenColorTarget &sceneColor,
	OffscreenColorTarget &historyColor,
	VkSampler &linearSampler);

// Free everything allocated by `CreateOrRecreateTaaRenderTargets`.
// Safe to call with `VK_NULL_HANDLE` fields.
void DestroyTaaRenderTargets(
	VulkanContextState *context,
	OffscreenColorTarget &sceneColor,
	OffscreenColorTarget &historyColor,
	VkSampler &linearSampler);

// Transition helpers used by `Renderer.cpp::RecordGraphicsCommands`.
// Each one issues a single `vkCmdPipelineBarrier2` so the TAA pass can
// chain them cheaply between the main voxel pass and the fullscreen
// resolve pass. `format` is the surface format from
// `SwapchainState::format` (only the depth-stencil transition needs it,
// and only when the caller is changing the depth attachment usage).
void TransitionTaaSceneColorForWrite(
	const VkCommandBuffer cmd,
	const OffscreenColorTarget &sceneColor);
void TransitionTaaSceneColorForSample(
	const VkCommandBuffer cmd,
	const OffscreenColorTarget &sceneColor);
void TransitionTaaHistoryForSample(
	const VkCommandBuffer cmd,
	const OffscreenColorTarget &historyColor);
// Copy the just-resolved `sceneColor` into `historyColor` so the next
// frame's resolve pass has a fresh history sample to reproject against.
// A `vkCmdCopyImage` keeps this O(1) regardless of the current pixel
// count, and avoids the extra `vkCmdBlitImage` path that the spec warns
// about for colour-only copies.
void RecordTaaHistoryCopy(
	const VkCommandBuffer cmd,
	const OffscreenColorTarget &sceneColor,
	const OffscreenColorTarget &historyColor,
	VkExtent2D extent);

} // namespace projectv::taa

#endif

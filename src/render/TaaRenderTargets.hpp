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

// TAA offscreen render target + linear sampler. Created / recreated in
// `VulkanSwapchain.cpp::CreateOrRecreateSwapchain` so the size stays in
// lockstep with the swapchain extent. Two identical R16G16B16A16_SFLOAT
// images are allocated and double-buffered: the resolve pass reads from
// `history` while writing to `current`, then the post-resolve ping-pong
// swap (or copy) makes the freshly-resolved `current` the next frame's
// `history`.
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

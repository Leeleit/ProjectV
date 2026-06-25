#pragma once

#include "core/Types.hpp"
#include "render/vulkan/VulkanInit.hpp"

struct SwapchainState;

namespace projectv::render {

bool CreateHdrColorResources(
	VulkanContextState *context,
	const SwapchainState *swapchain,
	RenderState *render);

void DestroyHdrColorResources(VulkanContextState *context, RenderState *render);

bool CreateToneMapPass(
	VulkanContextState *context,
	const SwapchainState *swapchain,
	RenderState *render);

void DestroyToneMapPass(VulkanContextState *context, RenderState *render);

bool RefreshToneMapResourceBindings(VulkanContextState *context, RenderState *render);

void RecordToneMapPass(
	VkCommandBuffer cmd,
	const RenderState &render,
	VkImageView swapchainImageView,
	const VkExtent2D &extent,
	uint32_t imageIndex,
	uint32_t frameIndex);

} // namespace projectv::render

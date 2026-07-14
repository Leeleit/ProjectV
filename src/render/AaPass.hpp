#pragma once

#include "core/Types.hpp"

#include <vulkan/vulkan.h>

namespace projectv::render {

struct AaTonemapPushConstants {
	std::array<float, 4> params0{}; // x=exposure, y=toneMapOperator
	std::array<float, 4> params1{}; // colorGrading
};
static_assert(sizeof(AaTonemapPushConstants) == 32);

struct AaAccumPushConstants {
	std::array<float, 4> params0{}; // x=frameIndex
};
static_assert(sizeof(AaAccumPushConstants) == 16);

bool CreateAaPassResources(VulkanContextState *context, RenderState *render, VkExtent2D internalExtent);
void DestroyAaPassResources(VulkanContextState *context, RenderState *render);
bool CreateAaSceneTargets(VulkanContextState *context, RenderState *render, VkExtent2D internalExtent);
void DestroyAaSceneTargets(VulkanContextState *context, RenderState *render);
void ResolveMsaaSampleCount(VulkanContextState *context, RenderState *render);
bool RecreateAaDependentPipelines(VulkanContextState *context, SwapchainState *swapchain, RenderState *render);

// HDR input → progressive accum (optional) → tonemap → SMAA (optional) → LDR presentSrc
bool RecordAaResolvePass(
	VkCommandBuffer commandBuffer,
	VulkanContextState &context,
	RenderState &render,
	VkImage hdrSourceImage,
	VkImageView hdrSourceView,
	VkImageLayout &hdrSourceLayout,
	VkExtent2D extent,
	uint32_t frameIndex);

void InvalidateProgressiveAccum(RenderState &render);
bool UpdateProgressiveAccumState(
	RenderState &render,
	const CameraState &camera,
	bool sceneDirty,
	VkExtent2D renderExtent);

} // namespace projectv::render

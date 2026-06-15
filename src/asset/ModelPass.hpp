#pragma once

#include "core/Types.hpp"
#include "render/TaaRenderTargets.hpp"

namespace projectv::asset {

bool CreateModelPipeline(
	VulkanContextState *context,
	VkPipelineLayout sharedPipelineLayout,
	VkFormat colorFormat,
	VkFormat depthFormat,
	RenderState *render);

void DestroyModelPipeline(VulkanContextState *context, RenderState *render);

// Selects the right `modelPipeline` / `modelPipelineTaaOn` based on
// whether TAA is currently active (`render.taaEnabled`). Returns
// `VK_NULL_HANDLE` if no model pipeline exists.
VkPipeline PickModelPipeline(const RenderState &render);

} // namespace projectv::asset




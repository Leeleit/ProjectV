#pragma once

#include "core/Types.hpp"

namespace projectv::asset {

bool CreateModelPipeline(
	VulkanContextState *context,
	VkPipelineLayout sharedPipelineLayout,
	VkFormat colorFormat,
	VkFormat depthFormat,
	RenderState *render);

void DestroyModelPipeline(VulkanContextState *context, RenderState *render);

VkPipeline PickModelPipeline(const RenderState &render);

} // namespace projectv::asset




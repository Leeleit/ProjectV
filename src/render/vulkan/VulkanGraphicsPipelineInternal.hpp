#pragma once

#include "volk.h"

#include <vector>

struct VulkanContextState;
struct SwapchainState;
struct RenderState;

void LogGraphicsPipelineVkFailure(const char *step, const VkResult result);
void LogGraphicsPipelineTextFailure(const char *step, const char *detail);
VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char> &code);
bool SupportsDepthAttachment(VkPhysicalDevice physicalDevice, VkFormat format);
bool SupportsSampledImage(VkPhysicalDevice physicalDevice, VkFormat format);
VkFormat ChooseDepthFormat(VkPhysicalDevice physicalDevice);

void DestroyGraphicsResourceBindings(VulkanContextState &context, RenderState &render);
void DestroyDebugOverlayPipeline(VulkanContextState &context, RenderState &render);
bool CreateDebugOverlayPipeline(VulkanContextState &context, const SwapchainState &swapchain, RenderState &render);
void DestroyDebugHudPipeline(VulkanContextState &context, RenderState &render);
bool CreateDebugHudPipeline(VulkanContextState &context, const SwapchainState &swapchain, RenderState &render);

inline constexpr VkPipelineColorBlendAttachmentState kAlphaBlendAttachmentState{
	.blendEnable = VK_TRUE,
	.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
	.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	.colorBlendOp = VK_BLEND_OP_ADD,
	.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
	.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	.alphaBlendOp = VK_BLEND_OP_ADD,
	.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT,
};

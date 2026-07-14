#include "volk.h" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "render/Cloudscape.hpp"

#include "debug/Profiling.hpp"

#include <array>

namespace projectv::render {

void DestroyCloudscapeResources(VulkanContextState *context, RenderState *render)
{
	if (context == nullptr || render == nullptr || context->device == VK_NULL_HANDLE) {
		return;
	}
	if (render->cloudscapePipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(context->device, render->cloudscapePipeline, nullptr);
		render->cloudscapePipeline = VK_NULL_HANDLE;
	}
	if (render->cloudscapePipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(context->device, render->cloudscapePipelineLayout, nullptr);
		render->cloudscapePipelineLayout = VK_NULL_HANDLE;
	}
	if (render->cloudscapeVertexShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context->device, render->cloudscapeVertexShaderModule, nullptr);
		render->cloudscapeVertexShaderModule = VK_NULL_HANDLE;
	}
	if (render->cloudscapeFragmentShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context->device, render->cloudscapeFragmentShaderModule, nullptr);
		render->cloudscapeFragmentShaderModule = VK_NULL_HANDLE;
	}
	for (VkDescriptorSet &descriptorSet : render->cloudscapeDescriptorSets) {
		descriptorSet = VK_NULL_HANDLE;
	}
	if (render->cloudscapeDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(context->device, render->cloudscapeDescriptorPool, nullptr);
		render->cloudscapeDescriptorPool = VK_NULL_HANDLE;
	}
	if (render->cloudscapeDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(context->device, render->cloudscapeDescriptorSetLayout, nullptr);
		render->cloudscapeDescriptorSetLayout = VK_NULL_HANDLE;
	}
	if (render->cloudscapeLinearSampler != VK_NULL_HANDLE) {
		vkDestroySampler(context->device, render->cloudscapeLinearSampler, nullptr);
		render->cloudscapeLinearSampler = VK_NULL_HANDLE;
	}
	if (render->cloudscapeNoiseView != VK_NULL_HANDLE) {
		vkDestroyImageView(context->device, render->cloudscapeNoiseView, nullptr);
		render->cloudscapeNoiseView = VK_NULL_HANDLE;
	}
	if (render->cloudscapeNoiseImage != VK_NULL_HANDLE) {
		vmaDestroyImage(context->allocator, render->cloudscapeNoiseImage, render->cloudscapeNoiseAllocation);
		render->cloudscapeNoiseImage = VK_NULL_HANDLE;
		render->cloudscapeNoiseAllocation = nullptr;
	}
	render->cloudscapeNoiseHostCopied = false;
	render->cloudscapePipelineEnabled = false;
}

bool RecordCloudscapeRaymarchPass(
	const VkCommandBuffer commandBuffer,
	RenderState &render,
	const CloudscapePushConstants &pushConstants,
	const VkImageView sceneColorView,
	const VkImageView depthView,
	const VkExtent2D extent,
	const uint32_t frameIndex)
{
	PV_PROFILE_ZONE_N("RecordCloudscapeRaymarchPass");
	if (commandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (render.cloudscapePipeline == VK_NULL_HANDLE ||
		render.cloudscapePipelineLayout == VK_NULL_HANDLE) {
		return false;
	}
	if (frameIndex >= MAX_FRAMES_IN_FLIGHT) {
		return false;
	}
	if (render.cloudscapeDescriptorSets[frameIndex] == VK_NULL_HANDLE) {
		return false;
	}
	if (sceneColorView == VK_NULL_HANDLE || depthView == VK_NULL_HANDLE) {
		return false;
	}
	if (extent.width == 0u || extent.height == 0u) {
		return false;
	}

	const VkViewport viewport{
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(extent.width),
		.height = static_cast<float>(extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	const VkRect2D scissor{
		.offset = {0, 0},
		.extent = extent,
	};

	const VkRenderingAttachmentInfo colorAttachment{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = sceneColorView,
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
	};
	const VkRenderingAttachmentInfo depthAttachment{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = depthView,
		.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
	};

	const VkRenderingInfo renderingInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = {{0, 0}, extent},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachment,
		.pDepthAttachment = &depthAttachment,
	};

	vkCmdBeginRendering(commandBuffer, &renderingInfo);
	vkCmdSetViewport(commandBuffer, 0u, 1u, &viewport);
	vkCmdSetScissor(commandBuffer, 0u, 1u, &scissor);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render.cloudscapePipeline);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		render.cloudscapePipelineLayout,
		0u,
		1u,
		&render.cloudscapeDescriptorSets[frameIndex],
		0u,
		nullptr);
	vkCmdPushConstants(
		commandBuffer,
		render.cloudscapePipelineLayout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0u,
		sizeof(CloudscapePushConstants),
		&pushConstants);
	vkCmdDraw(commandBuffer, 3u, 1u, 0u, 0u);
	vkCmdEndRendering(commandBuffer);

	profiling::PlotValue("Cloudscape Pass", 1.0);
	return true;
}

} // namespace projectv::render

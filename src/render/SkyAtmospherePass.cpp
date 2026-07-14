#include "render/SkyAtmosphere.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"

namespace projectv::render {
bool RecordSkyAtmosphereDraw(
	const VkCommandBuffer commandBuffer,
	RenderState &render,
	const SkyAtmospherePushConstants &pushConstants,
	const uint32_t frameIndex)
{
	PV_PROFILE_ZONE_N("RecordSkyAtmosphereDraw");
	if (commandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (render.skyAtmospherePipeline == VK_NULL_HANDLE ||
		render.skyAtmospherePipelineLayout == VK_NULL_HANDLE) {
		return false;
	}
	if (frameIndex >= MAX_FRAMES_IN_FLIGHT) {
		return false;
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render.skyAtmospherePipeline);
	if (render.skyAtmosphereDescriptorSets[frameIndex] != VK_NULL_HANDLE) {
		vkCmdBindDescriptorSets(
			commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			render.skyAtmospherePipelineLayout,
			0u,
			1u,
			&render.skyAtmosphereDescriptorSets[frameIndex],
			0u,
			nullptr);
	}
	vkCmdPushConstants(
		commandBuffer,
		render.skyAtmospherePipelineLayout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0u,
		sizeof(SkyAtmospherePushConstants),
		&pushConstants);
	vkCmdDraw(commandBuffer, 3u, 1u, 0u, 0u);
	profiling::PlotValue("Sky Atmosphere Pass", 1.0);
	return true;
}

bool RecordSkyAtmospherePass(
	const VkCommandBuffer commandBuffer,
	RenderState &render,
	const SkyAtmospherePushConstants &pushConstants,
	const VkImageView sceneColorView,
	const VkImageView depthView,
	const VkExtent2D extent,
	const uint32_t frameIndex)
{
	PV_PROFILE_ZONE_N("RecordSkyAtmospherePass");
	if (commandBuffer == VK_NULL_HANDLE) {
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
		.loadOp = VK_ATTACHMENT_LOAD_OP_NONE, // Sky pass fully overwrites the attachment; no prior contents are read.
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
	};
	const VkRenderingAttachmentInfo depthAttachment{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = depthView,
		.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_NONE, // Depth is written by the sky pass; previous contents are irrelevant.
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = {.depthStencil = {1.0f, 0}},
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
	const bool drawn = RecordSkyAtmosphereDraw(commandBuffer, render, pushConstants, frameIndex);
	vkCmdEndRendering(commandBuffer);
	return drawn;
}
} // namespace projectv::render

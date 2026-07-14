#include "render/vulkan/VulkanMeshShaderPipeline.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"
#include "render/vulkan/VulkanDebug.hpp"

namespace projectv::render {
void DestroyMeshShaderPipelines(VulkanContextState *context, RenderState *render)
{
	for (SceneFrameResources &frameResources : render->sceneFrameResources) {
		frameResources.meshShaderDescriptorSet = VK_NULL_HANDLE;
		frameResources.meshClusters.clusterizeDescriptorSet = VK_NULL_HANDLE;
	}

	if (context->device != VK_NULL_HANDLE) {
		if (render->meshCullPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(context->device, render->meshCullPipeline, nullptr);
			render->meshCullPipeline = VK_NULL_HANDLE;
		}
		if (render->meshClusterizePipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(context->device, render->meshClusterizePipeline, nullptr);
			render->meshClusterizePipeline = VK_NULL_HANDLE;
		}
		if (render->meshShaderPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(context->device, render->meshShaderPipeline, nullptr);
			render->meshShaderPipeline = VK_NULL_HANDLE;
		}
		if (render->meshCullPipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(context->device, render->meshCullPipelineLayout, nullptr);
			render->meshCullPipelineLayout = VK_NULL_HANDLE;
		}
		if (render->meshClusterizePipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(context->device, render->meshClusterizePipelineLayout, nullptr);
			render->meshClusterizePipelineLayout = VK_NULL_HANDLE;
		}
		if (render->meshShaderPipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(context->device, render->meshShaderPipelineLayout, nullptr);
			render->meshShaderPipelineLayout = VK_NULL_HANDLE;
		}
		if (render->meshShaderDescriptorSetLayout != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(context->device, render->meshShaderDescriptorSetLayout, nullptr);
			render->meshShaderDescriptorSetLayout = VK_NULL_HANDLE;
		}
		if (render->meshClusterizeDescriptorSetLayout != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(context->device, render->meshClusterizeDescriptorSetLayout, nullptr);
			render->meshClusterizeDescriptorSetLayout = VK_NULL_HANDLE;
		}
		if (render->meshCullShaderModule != VK_NULL_HANDLE) {
			vkDestroyShaderModule(context->device, render->meshCullShaderModule, nullptr);
			render->meshCullShaderModule = VK_NULL_HANDLE;
		}
		if (render->meshClusterizeShaderModule != VK_NULL_HANDLE) {
			vkDestroyShaderModule(context->device, render->meshClusterizeShaderModule, nullptr);
			render->meshClusterizeShaderModule = VK_NULL_HANDLE;
		}
		if (render->meshShaderModule != VK_NULL_HANDLE) {
			vkDestroyShaderModule(context->device, render->meshShaderModule, nullptr);
			render->meshShaderModule = VK_NULL_HANDLE;
		}
		if (render->meshShaderDescriptorPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(context->device, render->meshShaderDescriptorPool, nullptr);
			render->meshShaderDescriptorPool = VK_NULL_HANDLE;
		}
	}
	render->meshShaderEnabled = false;
}

bool RecordMeshShaderDraw(
	const VkCommandBuffer commandBuffer,
	RenderState &render,
	SceneFrameResources &frameResources,
	const GraphicsPushConstants &drawPushConstants,
	const uint32_t fallbackTaskCount)
{
	if (!render.meshShaderEnabled) {
		return false;
	}
	if (commandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (render.meshShaderPipeline == VK_NULL_HANDLE ||
		render.meshShaderPipelineLayout == VK_NULL_HANDLE) {
		return false;
	}
	if (frameResources.graphicsDescriptorSet == VK_NULL_HANDLE ||
		frameResources.meshShaderDescriptorSet == VK_NULL_HANDLE) {
		return false;
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render.meshShaderPipeline);
	const VkDescriptorSet sets[2]{
		frameResources.graphicsDescriptorSet,
		frameResources.meshShaderDescriptorSet,
	};
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		render.meshShaderPipelineLayout,
		0u,
		2u,
		sets,
		0u,
		nullptr);
	vkCmdPushConstants(
		commandBuffer,
		render.meshShaderPipelineLayout,
		VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0u,
		sizeof(GraphicsPushConstants),
		&drawPushConstants);

	if (render.meshShaderIndirectEnabled &&
		frameResources.meshClusters.meshDrawIndirectBuffer != VK_NULL_HANDLE) {
		vkCmdDrawMeshTasksIndirectEXT(
			commandBuffer,
			frameResources.meshClusters.meshDrawIndirectBuffer,
			0,
			1u,
			sizeof(VkDrawMeshTasksIndirectCommandEXT));
	} else if (fallbackTaskCount > 0u) {
		vkCmdDrawMeshTasksEXT(commandBuffer, fallbackTaskCount, 1u, 1u);
	}

	return true;
}
} // namespace projectv::render

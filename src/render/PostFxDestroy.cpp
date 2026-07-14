#include "render/PostFx.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"
#include "render/vulkan/VulkanDebug.hpp"

namespace projectv::render {
void DestroyPostFxResources(
	VulkanContextState *context,
	RenderState *render)
{
	if (!context || !render || context->device == VK_NULL_HANDLE) {
		return;
	}

	if (render->postFxBindlessCompositePipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(context->device, render->postFxBindlessCompositePipeline, nullptr);
		render->postFxBindlessCompositePipeline = VK_NULL_HANDLE;
	}
	if (render->postFxBindlessCompositeShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context->device, render->postFxBindlessCompositeShaderModule, nullptr);
		render->postFxBindlessCompositeShaderModule = VK_NULL_HANDLE;
	}
	if (render->postFxBindlessDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(context->device, render->postFxBindlessDescriptorPool, nullptr);
		render->postFxBindlessDescriptorPool = VK_NULL_HANDLE;
	}
	render->postFxBindlessDescriptorSets.clear();
	if (render->postFxBindlessPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(context->device, render->postFxBindlessPipelineLayout, nullptr);
		render->postFxBindlessPipelineLayout = VK_NULL_HANDLE;
	}
	if (render->postFxBindlessDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(context->device, render->postFxBindlessDescriptorSetLayout, nullptr);
		render->postFxBindlessDescriptorSetLayout = VK_NULL_HANDLE;
	}
	render->postFxBindlessEnabled = false;

	if (render->bloomCompositePipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(context->device, render->bloomCompositePipeline, nullptr);
		render->bloomCompositePipeline = VK_NULL_HANDLE;
	}
	if (render->bloomUpsamplePipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(context->device, render->bloomUpsamplePipeline, nullptr);
		render->bloomUpsamplePipeline = VK_NULL_HANDLE;
	}
	if (render->bloomDownsamplePipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(context->device, render->bloomDownsamplePipeline, nullptr);
		render->bloomDownsamplePipeline = VK_NULL_HANDLE;
	}
	if (render->bloomThresholdPipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(context->device, render->bloomThresholdPipeline, nullptr);
		render->bloomThresholdPipeline = VK_NULL_HANDLE;
	}

	if (render->bloomCompositeShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context->device, render->bloomCompositeShaderModule, nullptr);
		render->bloomCompositeShaderModule = VK_NULL_HANDLE;
	}
	if (render->bloomUpsampleShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context->device, render->bloomUpsampleShaderModule, nullptr);
		render->bloomUpsampleShaderModule = VK_NULL_HANDLE;
	}
	if (render->bloomDownsampleShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context->device, render->bloomDownsampleShaderModule, nullptr);
		render->bloomDownsampleShaderModule = VK_NULL_HANDLE;
	}
	if (render->bloomThresholdShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context->device, render->bloomThresholdShaderModule, nullptr);
		render->bloomThresholdShaderModule = VK_NULL_HANDLE;
	}

	if (render->postFxDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(context->device, render->postFxDescriptorPool, nullptr);
		render->postFxDescriptorPool = VK_NULL_HANDLE;
	}
	render->postFxDescriptorSets.clear();

	if (render->postFxPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(context->device, render->postFxPipelineLayout, nullptr);
		render->postFxPipelineLayout = VK_NULL_HANDLE;
	}
	if (render->postFxDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(context->device, render->postFxDescriptorSetLayout, nullptr);
		render->postFxDescriptorSetLayout = VK_NULL_HANDLE;
	}

	for (VkImageView view : render->bloomScratchMipViews) {
		if (view != VK_NULL_HANDLE) {
			vkDestroyImageView(context->device, view, nullptr);
		}
	}
	render->bloomScratchMipViews.clear();

	if (render->postFxOutputImageView != VK_NULL_HANDLE) {
		vkDestroyImageView(context->device, render->postFxOutputImageView, nullptr);
		render->postFxOutputImageView = VK_NULL_HANDLE;
	}
	if (render->postFxOutputImage != VK_NULL_HANDLE) {
		vmaDestroyImage(context->allocator, render->postFxOutputImage, render->postFxOutputImageAllocation);
		render->postFxOutputImage = VK_NULL_HANDLE;
		render->postFxOutputImageAllocation = nullptr;
	}

	if (render->bloomResultImageView != VK_NULL_HANDLE) {
		vkDestroyImageView(context->device, render->bloomResultImageView, nullptr);
		render->bloomResultImageView = VK_NULL_HANDLE;
	}
	if (render->bloomResultImage != VK_NULL_HANDLE) {
		vmaDestroyImage(context->allocator, render->bloomResultImage, render->bloomResultImageAllocation);
		render->bloomResultImage = VK_NULL_HANDLE;
		render->bloomResultImageAllocation = nullptr;
	}

	if (render->bloomScratchImageView != VK_NULL_HANDLE) {
		vkDestroyImageView(context->device, render->bloomScratchImageView, nullptr);
		render->bloomScratchImageView = VK_NULL_HANDLE;
	}
	if (render->bloomScratchImage != VK_NULL_HANDLE) {
		vmaDestroyImage(context->allocator, render->bloomScratchImage, render->bloomScratchImageAllocation);
		render->bloomScratchImage = VK_NULL_HANDLE;
		render->bloomScratchImageAllocation = nullptr;
	}

	if (render->postFxLinearSampler != VK_NULL_HANDLE) {
		vkDestroySampler(context->device, render->postFxLinearSampler, nullptr);
		render->postFxLinearSampler = VK_NULL_HANDLE;
	}

	render->bloomPipelineEnabled = false;
	render->aerialPerspectivePipelineEnabled = false;
	render->postFxExtent = VkExtent2D{0u, 0u};
}
} // namespace projectv::render

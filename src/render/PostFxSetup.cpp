#include "render/PostFx.hpp"

#include "SDL3/SDL_log.h"
#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "debug/Profiling.hpp"
#include "render/RendererInternal.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <fmt/format.h>

#include <array>
#include <vector>

namespace {

constexpr char kBloomThresholdShaderFilename[] = "bloom_threshold.comp.spv";
constexpr char kBloomDownsampleShaderFilename[] = "bloom_downsample.comp.spv";
constexpr char kBloomUpsampleShaderFilename[] = "bloom_upsample.comp.spv";
constexpr char kPostCompositeShaderFilename[] = "post_composite.comp.spv";

// One combined image sampler for each sampled input, one storage image for output.
constexpr std::array kPostFxDescriptorBindings{
	VkDescriptorSetLayoutBinding{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 2,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 3,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
	},
};

constexpr VkDescriptorSetLayoutCreateInfo kPostFxDescriptorSetLayoutInfo{
	.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	.pNext = nullptr,
	.flags = 0,
	.bindingCount = static_cast<uint32_t>(kPostFxDescriptorBindings.size()),
	.pBindings = kPostFxDescriptorBindings.data(),
};

// Pool sized for all descriptor sets used by the post-FX pass.
constexpr std::array kPostFxDescriptorPoolSizes{
	VkDescriptorPoolSize{
		.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = projectv::render::kPostFxDescriptorSetCount * 3u,
	},
	VkDescriptorPoolSize{
		.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		.descriptorCount = projectv::render::kPostFxDescriptorSetCount,
	},
};

void PostFxMemoryBarrier(VkCommandBuffer commandBuffer)
{
	VkMemoryBarrier2 barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
	barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
	barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.memoryBarrierCount = 1;
	depInfo.pMemoryBarriers = &barrier;
	vkCmdPipelineBarrier2(commandBuffer, &depInfo);
}

bool CreatePostFxSampler(
	VulkanContextState *context,
	RenderState *render)
{
	PV_CHECK_OR_RETURN(
		context && render && context->device != VK_NULL_HANDLE,
		"Render",
		"CreatePostFxSampler.Preconditions",
		"missing context");
	if (render->postFxLinearSampler != VK_NULL_HANDLE) {
		return true;
	}

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 16.0f;

	const VkResult result = vkCreateSampler(context->device, &samplerInfo, nullptr, &render->postFxLinearSampler);
	if (result != VK_SUCCESS) {
		runtime::LogVkFailure("CreatePostFxSampler.vkCreateSampler", result);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->postFxLinearSampler), VK_OBJECT_TYPE_SAMPLER, "PostFxLinearSampler");
	return true;
}

bool CreatePostFxImage(
	VulkanContextState *context,
	RenderState *render,
	VkExtent2D extent,
	VkFormat format,
	uint32_t mipLevels,
	VkImageUsageFlags usage,
	VkImage *outImage,
	VkImageView *outView,
	VmaAllocation *outAllocation,
	const char *imageName,
	const char *viewName)
{
	PV_CHECK_OR_RETURN(
		context && render && context->device != VK_NULL_HANDLE && context->allocator != nullptr,
		"Render",
		"CreatePostFxImage.Preconditions",
		"missing context");

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = format;
	imageInfo.extent = {extent.width, extent.height, 1u};
	imageInfo.mipLevels = mipLevels;
	imageInfo.arrayLayers = 1u;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

	const VkResult createResult = vmaCreateImage(
		context->allocator,
		&imageInfo,
		&allocationInfo,
		outImage,
		outAllocation,
		nullptr);
	if (createResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreatePostFxImage.vmaCreateImage", createResult);
		return false;
	}

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = *outImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, mipLevels, 0u, 1u};
	if (vkCreateImageView(context->device, &viewInfo, nullptr, outView) != VK_SUCCESS) {
		vmaDestroyImage(context->allocator, *outImage, *outAllocation);
		*outImage = VK_NULL_HANDLE;
		*outAllocation = nullptr;
		runtime::LogRuntimeFailure("Render", "CreatePostFxImage.vkCreateImageView", "failed");
		return false;
	}

	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(*outImage), VK_OBJECT_TYPE_IMAGE, imageName);
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(*outView), VK_OBJECT_TYPE_IMAGE_VIEW, viewName);
	return true;
}

void TransitionPostFxImageAllMips(
	VkCommandBuffer cmd,
	VkImage image,
	VkImageAspectFlags aspectMask,
	uint32_t mipLevels,
	VkImageLayout oldLayout,
	VkImageLayout newLayout)
{
	VkImageMemoryBarrier2 barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	barrier.srcAccessMask = 0;
	barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	barrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.image = image;
	barrier.subresourceRange = {aspectMask, 0u, mipLevels, 0u, 1u};

	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers = &barrier;
	vkCmdPipelineBarrier2(cmd, &depInfo);
}

bool TransitionPostFxImagesToGeneral(
	VulkanContextState *context,
	RenderState *render)
{
	PV_CHECK_OR_RETURN(
		context && render && context->device != VK_NULL_HANDLE && context->commandPool != VK_NULL_HANDLE && context->queue != VK_NULL_HANDLE,
		"Render",
		"TransitionPostFxImagesToGeneral.Preconditions",
		"missing context");

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = context->commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	if (vkAllocateCommandBuffers(context->device, &allocInfo, &cmd) != VK_SUCCESS) {
		runtime::LogRuntimeFailure("Render", "TransitionPostFxImagesToGeneral.Allocate", "failed");
		return false;
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);

	TransitionPostFxImageAllMips(
		cmd,
		render->bloomScratchImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		projectv::render::kBloomMipCount,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL);

	TransitionPostFxImageAllMips(
		cmd,
		render->bloomResultImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		1u,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL);

	TransitionPostFxImageAllMips(
		cmd,
		render->postFxOutputImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		1u,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL);

	vkEndCommandBuffer(cmd);

	VkCommandBufferSubmitInfo commandBufferSubmitInfo{};
	commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	commandBufferSubmitInfo.commandBuffer = cmd;
	VkSubmitInfo2 submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.commandBufferInfoCount = 1u;
	submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VkFence fence = VK_NULL_HANDLE;
	if (vkCreateFence(context->device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
		vkFreeCommandBuffers(context->device, context->commandPool, 1, &cmd);
		runtime::LogRuntimeFailure("Render", "TransitionPostFxImagesToGeneral.CreateFence", "failed");
		return false;
	}

	vkQueueSubmit2(context->queue, 1u, &submitInfo, fence);
	const VkResult waitResult = vkWaitForFences(context->device, 1, &fence, VK_TRUE, UINT64_MAX);
	vkDestroyFence(context->device, fence, nullptr);
	vkFreeCommandBuffers(context->device, context->commandPool, 1, &cmd);

	if (waitResult != VK_SUCCESS) {
		runtime::LogVkFailure("TransitionPostFxImagesToGeneral.vkWaitForFences", waitResult);
		return false;
	}
	return true;
}

bool CreatePostFxMipViews(
	VulkanContextState *context,
	RenderState *render)
{
	PV_CHECK_OR_RETURN(
		context && render && context->device != VK_NULL_HANDLE,
		"Render",
		"CreatePostFxMipViews.Preconditions",
		"missing context");

	render->bloomScratchMipViews.clear();
	for (uint32_t mip = 0; mip < projectv::render::kBloomMipCount; ++mip) {
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = render->bloomScratchImage;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, mip, 1u, 0u, 1u};
		VkImageView view = VK_NULL_HANDLE;
		if (vkCreateImageView(context->device, &viewInfo, nullptr, &view) != VK_SUCCESS) {
			runtime::LogRuntimeFailure("Render", "CreatePostFxMipViews.vkCreateImageView", "failed");
			return false;
		}
		render->bloomScratchMipViews.push_back(view);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(view),
			VK_OBJECT_TYPE_IMAGE_VIEW,
			fmt::format("BloomScratchMip{}View", mip).c_str());
	}
	return true;
}

bool CreatePostFxPipeline(
	VulkanContextState *context,
	RenderState *render,
	const char *shaderFilename,
	VkShaderModule *outShaderModule,
	VkPipeline *outPipeline)
{
	PV_CHECK_OR_RETURN(
		context && render && context->device != VK_NULL_HANDLE,
		"Render",
		"CreatePostFxPipeline.Preconditions",
		"missing context");

	const std::vector<char> shaderCode = ReadShaderFile(shaderFilename);
	if (shaderCode.empty()) {
		runtime::LogRuntimeFailure(
			"Render", "CreatePostFxPipeline.ReadShaderFile", fmt::format("{} not found", shaderFilename));
		return false;
	}

	VkShaderModuleCreateInfo shaderModuleInfo{};
	shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleInfo.codeSize = shaderCode.size();
	shaderModuleInfo.pCode = reinterpret_cast<const uint32_t *>(shaderCode.data());
	const VkResult shaderResult = vkCreateShaderModule(context->device, &shaderModuleInfo, nullptr, outShaderModule);
	if (shaderResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreatePostFxPipeline.vkCreateShaderModule", shaderResult);
		return false;
	}

	VkComputePipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	pipelineInfo.stage.module = *outShaderModule;
	pipelineInfo.stage.pName = "main";
	pipelineInfo.layout = render->postFxPipelineLayout;

	const VkResult pipelineResult = vkCreateComputePipelines(
		context->device,
		context->pipelineCache,
		1,
		&pipelineInfo,
		nullptr,
		outPipeline);
	if (pipelineResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreatePostFxPipeline.vkCreateComputePipelines", pipelineResult);
		return false;
	}
	return true;
}

void DispatchPostFx(
	VkCommandBuffer cmd,
	VkPipeline pipeline,
	VkPipelineLayout layout,
	VkDescriptorSet descriptorSet,
	const projectv::render::PostFxPushConstants &pushConstants,
	uint32_t groupCountX,
	uint32_t groupCountY)
{
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		layout,
		0,
		1,
		&descriptorSet,
		0,
		nullptr);
	vkCmdPushConstants(
		cmd,
		layout,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0,
		sizeof(pushConstants),
		&pushConstants);
	vkCmdDispatch(cmd, groupCountX, groupCountY, 1);
}

} // namespace

namespace projectv::render {
bool CreatePostFxResources(
	VulkanContextState *context,
	RenderState *render,
	const VkExtent2D extent)
{
	PV_CHECK_OR_RETURN(
		context && render && context->device != VK_NULL_HANDLE && context->allocator != nullptr,
		"Render",
		"CreatePostFxResources.Preconditions",
		"missing context");

	if (!IsPostFxEnabled()) {
		DestroyPostFxResources(context, render);
		return true;
	}

	if (render->postFxOutputImage != VK_NULL_HANDLE &&
		(render->postFxExtent.width != extent.width || render->postFxExtent.height != extent.height)) {
		DestroyPostFxResources(context, render);
	}

	if (render->postFxPipelineLayout != VK_NULL_HANDLE &&
		render->postFxOutputImage != VK_NULL_HANDLE &&
		extent.width > 0 && extent.height > 0) {
		return true;
	}

	DestroyPostFxResources(context, render);
	render->postFxExtent = extent;

	if (!CreatePostFxSampler(context, render)) {
		return false;
	}

	const VkExtent2D halfExtent{
		std::max(extent.width / 2u, 1u),
		std::max(extent.height / 2u, 1u)};

	if (!CreatePostFxImage(
			context,
			render,
			halfExtent,
			VK_FORMAT_R16G16B16A16_SFLOAT,
			kBloomMipCount,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			&render->bloomScratchImage,
			&render->bloomScratchImageView,
			&render->bloomScratchImageAllocation,
			"BloomScratchImage",
			"BloomScratchImageView")) {
		DestroyPostFxResources(context, render);
		return false;
	}

	if (!CreatePostFxMipViews(context, render)) {
		DestroyPostFxResources(context, render);
		return false;
	}

	if (!CreatePostFxImage(
			context,
			render,
			halfExtent,
			VK_FORMAT_R16G16B16A16_SFLOAT,
			1u,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			&render->bloomResultImage,
			&render->bloomResultImageView,
			&render->bloomResultImageAllocation,
			"BloomResultImage",
			"BloomResultImageView")) {
		DestroyPostFxResources(context, render);
		return false;
	}

	if (!CreatePostFxImage(
			context,
			render,
			extent,
			VK_FORMAT_B10G11R11_UFLOAT_PACK32,
			1u,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			&render->postFxOutputImage,
			&render->postFxOutputImageView,
			&render->postFxOutputImageAllocation,
			"PostFxOutputImage",
			"PostFxOutputImageView")) {
		DestroyPostFxResources(context, render);
		return false;
	}

	if (!TransitionPostFxImagesToGeneral(context, render)) {
		DestroyPostFxResources(context, render);
		return false;
	}

	const VkResult layoutResult = vkCreateDescriptorSetLayout(
		context->device,
		&kPostFxDescriptorSetLayoutInfo,
		nullptr,
		&render->postFxDescriptorSetLayout);
	if (layoutResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreatePostFxResources.vkCreateDescriptorSetLayout", layoutResult);
		DestroyPostFxResources(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->postFxDescriptorSetLayout), VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "PostFxDescriptorSetLayout");

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(PostFxPushConstants);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &render->postFxDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	const VkResult pipelineLayoutResult = vkCreatePipelineLayout(
		context->device,
		&pipelineLayoutInfo,
		nullptr,
		&render->postFxPipelineLayout);
	if (pipelineLayoutResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreatePostFxResources.vkCreatePipelineLayout", pipelineLayoutResult);
		DestroyPostFxResources(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->postFxPipelineLayout), VK_OBJECT_TYPE_PIPELINE_LAYOUT, "PostFxPipelineLayout");

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = kPostFxDescriptorSetCount;
	poolInfo.poolSizeCount = static_cast<uint32_t>(kPostFxDescriptorPoolSizes.size());
	poolInfo.pPoolSizes = kPostFxDescriptorPoolSizes.data();

	const VkResult poolResult = vkCreateDescriptorPool(
		context->device,
		&poolInfo,
		nullptr,
		&render->postFxDescriptorPool);
	if (poolResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreatePostFxResources.vkCreateDescriptorPool", poolResult);
		DestroyPostFxResources(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->postFxDescriptorPool), VK_OBJECT_TYPE_DESCRIPTOR_POOL, "PostFxDescriptorPool");

	render->postFxDescriptorSets.resize(kPostFxDescriptorSetCount, VK_NULL_HANDLE);
	std::vector<VkDescriptorSetLayout> layouts(kPostFxDescriptorSetCount, render->postFxDescriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = render->postFxDescriptorPool;
	allocInfo.descriptorSetCount = kPostFxDescriptorSetCount;
	allocInfo.pSetLayouts = layouts.data();
	if (vkAllocateDescriptorSets(context->device, &allocInfo, render->postFxDescriptorSets.data()) != VK_SUCCESS) {
		runtime::LogRuntimeFailure("Render", "CreatePostFxResources.vkAllocateDescriptorSets", "failed");
		DestroyPostFxResources(context, render);
		return false;
	}

	if (!CreatePostFxPipeline(context, render, kBloomThresholdShaderFilename, &render->bloomThresholdShaderModule, &render->bloomThresholdPipeline) ||
		!CreatePostFxPipeline(context, render, kBloomDownsampleShaderFilename, &render->bloomDownsampleShaderModule, &render->bloomDownsamplePipeline) ||
		!CreatePostFxPipeline(context, render, kBloomUpsampleShaderFilename, &render->bloomUpsampleShaderModule, &render->bloomUpsamplePipeline) ||
		!CreatePostFxPipeline(context, render, kPostCompositeShaderFilename, &render->bloomCompositeShaderModule, &render->bloomCompositePipeline)) {
		DestroyPostFxResources(context, render);
		return false;
	}

	render->postFxBindlessEnabled = false;
	if (context->bindless) {
		constexpr uint32_t kBindlessSamplerArraySize = 8u;
		constexpr uint32_t kBindlessUsedSamplers = 3u;
		constexpr uint32_t kBindlessSetCount = MAX_FRAMES_IN_FLIGHT;

		VkDescriptorSetLayoutBinding bindlessBindings[2]{};
		bindlessBindings[0].binding = 0;
		bindlessBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		bindlessBindings[0].descriptorCount = 1u;
		bindlessBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		bindlessBindings[1].binding = 1;
		bindlessBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindlessBindings[1].descriptorCount = kBindlessSamplerArraySize;
		bindlessBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		const VkDescriptorBindingFlags bindingFlags[2] = {
			0u,
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
		};
		VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
		bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
		bindingFlagsInfo.bindingCount = 2u;
		bindingFlagsInfo.pBindingFlags = bindingFlags;

		VkDescriptorSetLayoutCreateInfo bindlessLayoutInfo{};
		bindlessLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		bindlessLayoutInfo.pNext = &bindingFlagsInfo;
		bindlessLayoutInfo.bindingCount = 2u;
		bindlessLayoutInfo.pBindings = bindlessBindings;
		if (vkCreateDescriptorSetLayout(
				context->device,
				&bindlessLayoutInfo,
				nullptr,
				&render->postFxBindlessDescriptorSetLayout) == VK_SUCCESS) {
			VkPushConstantRange pushConstantRange{};
			pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			pushConstantRange.offset = 0;
			pushConstantRange.size = sizeof(PostFxPushConstants);
			VkPipelineLayoutCreateInfo bindlessPipeLayoutInfo{};
			bindlessPipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			bindlessPipeLayoutInfo.setLayoutCount = 1u;
			bindlessPipeLayoutInfo.pSetLayouts = &render->postFxBindlessDescriptorSetLayout;
			bindlessPipeLayoutInfo.pushConstantRangeCount = 1u;
			bindlessPipeLayoutInfo.pPushConstantRanges = &pushConstantRange;
			if (vkCreatePipelineLayout(
					context->device,
					&bindlessPipeLayoutInfo,
					nullptr,
					&render->postFxBindlessPipelineLayout) == VK_SUCCESS) {
				std::array poolSizes = {
					VkDescriptorPoolSize{
						.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
						.descriptorCount = kBindlessSetCount * kBindlessSamplerArraySize},
					VkDescriptorPoolSize{
						.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
						.descriptorCount = kBindlessSetCount}};
				VkDescriptorPoolCreateInfo bindlessPoolInfo{};
				bindlessPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
				bindlessPoolInfo.maxSets = kBindlessSetCount;
				bindlessPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
				bindlessPoolInfo.pPoolSizes = poolSizes.data();
				if (vkCreateDescriptorPool(
						context->device,
						&bindlessPoolInfo,
						nullptr,
						&render->postFxBindlessDescriptorPool) == VK_SUCCESS) {
					render->postFxBindlessDescriptorSets.resize(kBindlessSetCount, VK_NULL_HANDLE);
					std::vector<VkDescriptorSetLayout> setLayouts(
						kBindlessSetCount,
						render->postFxBindlessDescriptorSetLayout);
					std::vector<uint32_t> variableCounts(kBindlessSetCount, kBindlessUsedSamplers);
					VkDescriptorSetVariableDescriptorCountAllocateInfo variableAlloc{};
					variableAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
					variableAlloc.descriptorSetCount = kBindlessSetCount;
					variableAlloc.pDescriptorCounts = variableCounts.data();
					VkDescriptorSetAllocateInfo bindlessAlloc{};
					bindlessAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
					bindlessAlloc.pNext = &variableAlloc;
					bindlessAlloc.descriptorPool = render->postFxBindlessDescriptorPool;
					bindlessAlloc.descriptorSetCount = kBindlessSetCount;
					bindlessAlloc.pSetLayouts = setLayouts.data();
					if (vkAllocateDescriptorSets(
							context->device,
							&bindlessAlloc,
							render->postFxBindlessDescriptorSets.data()) == VK_SUCCESS) {
						const VkPipelineLayout savedLayout = render->postFxPipelineLayout;
						render->postFxPipelineLayout = render->postFxBindlessPipelineLayout;
						const bool compositeOk = CreatePostFxPipeline(
							context,
							render,
							"post_composite_bindless.comp.spv",
							&render->postFxBindlessCompositeShaderModule,
							&render->postFxBindlessCompositePipeline);
						render->postFxPipelineLayout = savedLayout;
						if (compositeOk) {
							render->postFxBindlessEnabled = true;
							SDL_Log("Render: PostFX bindless composite path enabled");
						}
					}
				}
			}
		}
		if (!render->postFxBindlessEnabled) {
			SDL_Log("Render: PostFX bindless composite path unavailable; using classic bindings");
		}
	}

	render->bloomPipelineEnabled = IsBloomEnabled();
	render->aerialPerspectivePipelineEnabled = IsAerialPerspectiveEnabled();
	return true;
}
} // namespace projectv::render

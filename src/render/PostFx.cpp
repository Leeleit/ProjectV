#include "render/PostFx.hpp"

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

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VkFence fence = VK_NULL_HANDLE;
	if (vkCreateFence(context->device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
		vkFreeCommandBuffers(context->device, context->commandPool, 1, &cmd);
		runtime::LogRuntimeFailure("Render", "TransitionPostFxImagesToGeneral.CreateFence", "failed");
		return false;
	}

	vkQueueSubmit(context->queue, 1, &submitInfo, fence);
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
		VK_NULL_HANDLE,
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

	render->bloomPipelineEnabled = IsBloomEnabled();
	render->aerialPerspectivePipelineEnabled = IsAerialPerspectiveEnabled();
	return true;
}

void DestroyPostFxResources(
	VulkanContextState *context,
	RenderState *render)
{
	if (!context || !render || context->device == VK_NULL_HANDLE) {
		return;
	}

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

bool RecordPostFxPass(
	VkCommandBuffer commandBuffer,
	VulkanContextState &context,
	RenderState &render,
	const VoxelSceneLighting &lighting,
	const FrameRenderData &frameRenderData,
	const VkExtent2D extent,
	const uint32_t frameIndex)
{
	PV_PROFILE_ZONE_N("RecordPostFxPass");
	if (commandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (!IsPostFxEnabled()) {
		return true;
	}
	if (render.postFxDescriptorSets.empty()) {
		return false;
	}

	const VkExtent2D halfExtent{
		std::max(extent.width / 2u, 1u),
		std::max(extent.height / 2u, 1u)};

	const bool bloomEnabled = IsBloomEnabled();
	const bool aerialEnabled = IsAerialPerspectiveEnabled();

	// Transition sceneColor from COLOR_ATTACHMENT_OPTIMAL to SHADER_READ_ONLY_OPTIMAL.
	::TransitionImage(
		commandBuffer,
		render.sceneColorImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		render.sceneColorCurrentLayout,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
	render.sceneColorCurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Transition depth to DEPTH_READ_ONLY_OPTIMAL for composite sampler.
	::TransitionImage(
		commandBuffer,
		render.depthImage,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		render.depthImageCurrentLayout,
		VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
	render.depthImageCurrentLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;

	// Transition postFxOutput to GENERAL for compute writes.
	::TransitionImage(
		commandBuffer,
		render.postFxOutputImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_2_NONE,
		0,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		VK_ACCESS_2_SHADER_WRITE_BIT);

	PostFxPushConstants push{};
	push.params0[0] = 0.8f;								   // threshold
	push.params0[1] = 0.5f;								   // softKnee
	push.params0[2] = 0.5f;								   // bloomIntensity
	push.params1[0] = lighting.skyColorAndFogDensity[3];   // fogDensity
	push.params1[1] = lighting.horizonColorAndFogStart[3]; // fogMax
	push.params1[2] = lighting.postProcess[0];			   // exposure
	push.params2[0] = lighting.sunDirectionAndWrap[0];
	push.params2[1] = lighting.sunDirectionAndWrap[1];
	push.params2[2] = lighting.sunDirectionAndWrap[2];
	push.params3[0] = frameRenderData.graphicsPushConstants.cameraPosition.x;
	push.params3[1] = frameRenderData.graphicsPushConstants.cameraPosition.y;
	push.params3[2] = frameRenderData.graphicsPushConstants.cameraPosition.z;
	push.params4[0] = lighting.horizonColorAndFogStart[0];
	push.params4[1] = lighting.horizonColorAndFogStart[1];
	push.params4[2] = lighting.horizonColorAndFogStart[2];

	constexpr uint32_t kSetsPerFrame = kPostFxDescriptorSetCount / MAX_FRAMES_IN_FLIGHT;
	uint32_t setIndex = frameIndex * kSetsPerFrame;
	const auto allocateSet = [&]() -> VkDescriptorSet {
		if (setIndex >= render.postFxDescriptorSets.size()) {
			return VK_NULL_HANDLE;
		}
		return render.postFxDescriptorSets[setIndex++];
	};

	// Helper to update a descriptor set with up to four bindings.
	const auto updateSet = [&](
							   VkDescriptorSet descriptorSet,
							   VkSampler sampler,
							   VkImageView view0,
							   VkImageLayout layout0,
							   VkImageView view1,
							   VkImageLayout layout1,
							   VkImageView view2,
							   VkImageLayout layout2,
							   VkImageView view3,
							   VkImageLayout layout3,
							   bool useView1,
							   bool useView3) {
		std::array<VkDescriptorImageInfo, 4> infos{};
		std::array<VkWriteDescriptorSet, 4> writes{};
		uint32_t writeCount = 0;

		infos[0] = {sampler, view0, layout0};
		writes[writeCount] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &infos[0], nullptr, nullptr};
		++writeCount;

		// Binding 1 is only used by the composite shader; provide a valid fallback view when unused.
		infos[1] = {sampler, useView1 ? view1 : view0, layout1};
		writes[writeCount] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &infos[1], nullptr, nullptr};
		++writeCount;

		infos[2] = {VK_NULL_HANDLE, view2, layout2};
		writes[writeCount] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &infos[2], nullptr, nullptr};
		++writeCount;

		infos[3] = {sampler, useView3 ? view3 : view0, layout3};
		writes[writeCount] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 3, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &infos[3], nullptr, nullptr};
		++writeCount;

		vkUpdateDescriptorSets(context.device, writeCount, writes.data(), 0, nullptr);
	};

	if (bloomEnabled) {
		// Bloom threshold: sceneColor -> bloom scratch mip 0.
		{
			const VkDescriptorSet descriptorSet = allocateSet();
			if (descriptorSet == VK_NULL_HANDLE) {
				return false;
			}
			push.params0[3] = 0.0f;
			updateSet(
				descriptorSet,
				render.postFxLinearSampler,
				render.sceneColorImageView,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				render.sceneColorImageView,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				render.bloomScratchMipViews[0],
				VK_IMAGE_LAYOUT_GENERAL,
				render.sceneColorImageView,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				false,
				false);

			const uint32_t groupsX = (halfExtent.width + 15) / 16;
			const uint32_t groupsY = (halfExtent.height + 15) / 16;
			DispatchPostFx(commandBuffer, render.bloomThresholdPipeline, render.postFxPipelineLayout, descriptorSet, push, groupsX, groupsY);
		}

		PostFxMemoryBarrier(commandBuffer);

		// Bloom downsample chain.
		for (uint32_t mip = 0; mip + 1 < kBloomMipCount; ++mip) {
			const VkDescriptorSet descriptorSet = allocateSet();
			if (descriptorSet == VK_NULL_HANDLE) {
				return false;
			}
			const VkExtent2D dstExtent{
				std::max(halfExtent.width >> (mip + 1), 1u),
				std::max(halfExtent.height >> (mip + 1), 1u)};
			push.params0[3] = static_cast<float>(mip);

			updateSet(
				descriptorSet,
				render.postFxLinearSampler,
				render.bloomScratchMipViews[mip],
				VK_IMAGE_LAYOUT_GENERAL,
				render.bloomScratchMipViews[mip],
				VK_IMAGE_LAYOUT_GENERAL,
				render.bloomScratchMipViews[mip + 1],
				VK_IMAGE_LAYOUT_GENERAL,
				render.bloomScratchMipViews[mip],
				VK_IMAGE_LAYOUT_GENERAL,
				false,
				false);

			const uint32_t groupsX = (dstExtent.width + 15) / 16;
			const uint32_t groupsY = (dstExtent.height + 15) / 16;
			DispatchPostFx(commandBuffer, render.bloomDownsamplePipeline, render.postFxPipelineLayout, descriptorSet, push, groupsX, groupsY);

			PostFxMemoryBarrier(commandBuffer);
		}

		// Bloom upsample chain: start from top mip and write into bloomResultImage.
		{
			const VkDescriptorSet descriptorSet = allocateSet();
			if (descriptorSet == VK_NULL_HANDLE) {
				return false;
			}
			push.params0[3] = static_cast<float>(kBloomMipCount - 1);

			updateSet(
				descriptorSet,
				render.postFxLinearSampler,
				render.bloomScratchMipViews[kBloomMipCount - 1],
				VK_IMAGE_LAYOUT_GENERAL,
				render.bloomScratchMipViews[kBloomMipCount - 1],
				VK_IMAGE_LAYOUT_GENERAL,
				render.bloomResultImageView,
				VK_IMAGE_LAYOUT_GENERAL,
				render.bloomScratchMipViews[kBloomMipCount - 1],
				VK_IMAGE_LAYOUT_GENERAL,
				false,
				false);

			const uint32_t groupsX = (halfExtent.width + 15) / 16;
			const uint32_t groupsY = (halfExtent.height + 15) / 16;
			DispatchPostFx(commandBuffer, render.bloomUpsamplePipeline, render.postFxPipelineLayout, descriptorSet, push, groupsX, groupsY);
			PostFxMemoryBarrier(commandBuffer);
		}
	}

	// Composite pass: sceneColor + depth + bloomResult -> postFxOutput.
	{
		const VkDescriptorSet descriptorSet = allocateSet();
		if (descriptorSet == VK_NULL_HANDLE) {
			return false;
		}
		push.params0[3] = aerialEnabled ? 1.0f : 0.0f;

		updateSet(
			descriptorSet,
			render.postFxLinearSampler,
			render.sceneColorImageView,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			render.depthImageView,
			VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
			render.postFxOutputImageView,
			VK_IMAGE_LAYOUT_GENERAL,
			bloomEnabled ? render.bloomResultImageView : render.sceneColorImageView,
			bloomEnabled ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			true,
			bloomEnabled);

		const uint32_t groupsX = (extent.width + 15) / 16;
		const uint32_t groupsY = (extent.height + 15) / 16;
		DispatchPostFx(commandBuffer, render.bloomCompositePipeline, render.postFxPipelineLayout, descriptorSet, push, groupsX, groupsY);
	}

	// Transition postFxOutput to TRANSFER_SRC for blit.
	::TransitionImage(
		commandBuffer,
		render.postFxOutputImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		VK_ACCESS_2_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_READ_BIT);

	return true;
}

} // namespace projectv::render

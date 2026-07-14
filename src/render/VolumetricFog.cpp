#include "render/VolumetricFog.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "debug/Profiling.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <array>
#include <vector>

namespace {
constexpr char kVolumetricFogShaderFilename[] = "volumetric_fog.comp.spv";

constexpr std::array kVolumetricFogDescriptorBindings{
	VkDescriptorSetLayoutBinding{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 2,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
	},
};

constexpr VkDescriptorSetLayoutCreateInfo kVolumetricFogDescriptorSetLayoutInfo{
	.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	.pNext = nullptr,
	.flags = 0,
	.bindingCount = static_cast<uint32_t>(kVolumetricFogDescriptorBindings.size()),
	.pBindings = kVolumetricFogDescriptorBindings.data(),
};

constexpr std::array kVolumetricFogDescriptorPoolSizes{
	VkDescriptorPoolSize{
		.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		.descriptorCount = MAX_FRAMES_IN_FLIGHT,
	},
	VkDescriptorPoolSize{
		.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		.descriptorCount = MAX_FRAMES_IN_FLIGHT * 2u,
	},
};

bool CreateVolumetricFogFroxelImage(
	VulkanContextState *context,
	RenderState *render)
{
	PV_CHECK_OR_RETURN(
		context && render && context->device != VK_NULL_HANDLE && context->allocator != nullptr,
		"Render", "CreateVolumetricFogFroxelImage.Preconditions", "missing context");
	if (render->volumetricFogFroxelImage != VK_NULL_HANDLE) {
		return true;
	}

	VkImageCreateInfo imageInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .samples = VK_SAMPLE_COUNT_1_BIT};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_3D;
	imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	imageInfo.extent = {projectv::render::kVolumetricFogFroxelWidth, projectv::render::kVolumetricFogFroxelHeight, projectv::render::kVolumetricFogFroxelDepth};
	imageInfo.mipLevels = 1u;
	imageInfo.arrayLayers = 1u;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

	const VkResult createResult = vmaCreateImage(
		context->allocator,
		&imageInfo,
		&allocationInfo,
		&render->volumetricFogFroxelImage,
		&render->volumetricFogFroxelAllocation,
		nullptr);
	if (createResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateVolumetricFogFroxelImage.vmaCreateImage", createResult);
		return false;
	}

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = render->volumetricFogFroxelImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
	viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	if (vkCreateImageView(context->device, &viewInfo, nullptr, &render->volumetricFogFroxelView) != VK_SUCCESS) {
		vmaDestroyImage(context->allocator, render->volumetricFogFroxelImage, render->volumetricFogFroxelAllocation);
		render->volumetricFogFroxelImage = VK_NULL_HANDLE;
		render->volumetricFogFroxelAllocation = nullptr;
		runtime::LogRuntimeFailure(
			"Render", "CreateVolumetricFogFroxelImage.vkCreateImageView", "failed");
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->volumetricFogFroxelImage), VK_OBJECT_TYPE_IMAGE, "VolumetricFogFroxelImage");
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->volumetricFogFroxelView), VK_OBJECT_TYPE_IMAGE_VIEW, "VolumetricFogFroxelView");
	return true;
}

void DestroyVolumetricFogFroxelImage(
	VulkanContextState *context,
	RenderState *render)
{
	if (render->volumetricFogFroxelView != VK_NULL_HANDLE) {
		vkDestroyImageView(context->device, render->volumetricFogFroxelView, nullptr);
		render->volumetricFogFroxelView = VK_NULL_HANDLE;
	}
	if (render->volumetricFogFroxelImage != VK_NULL_HANDLE) {
		vmaDestroyImage(context->allocator, render->volumetricFogFroxelImage, render->volumetricFogFroxelAllocation);
		render->volumetricFogFroxelImage = VK_NULL_HANDLE;
		render->volumetricFogFroxelAllocation = nullptr;
	}
}

bool CreateVolumetricFogSampler(
	VulkanContextState *context,
	RenderState *render)
{
	PV_CHECK_OR_RETURN(
		context && render && context->device != VK_NULL_HANDLE,
		"Render", "CreateVolumetricFogSampler.Preconditions", "missing context");
	if (render->volumetricFogLinearSampler != VK_NULL_HANDLE) {
		return true;
	}

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;

	if (vkCreateSampler(context->device, &samplerInfo, nullptr, &render->volumetricFogLinearSampler) != VK_SUCCESS) {
		runtime::LogRuntimeFailure(
			"Render", "CreateVolumetricFogSampler.vkCreateSampler", "failed");
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->volumetricFogLinearSampler), VK_OBJECT_TYPE_SAMPLER, "VolumetricFogLinearSampler");
	return true;
}

} // namespace

bool CreateVolumetricFogFallbackImage(
	VulkanContextState *context,
	RenderState *render)
{
	PV_CHECK_OR_RETURN(
		context && render && context->device != VK_NULL_HANDLE && context->allocator != nullptr,
		"Render", "CreateVolumetricFogFallbackImage.Preconditions", "missing context");
	if (render->volumetricFogFallbackImage != VK_NULL_HANDLE) {
		return true;
	}

	VkImageCreateInfo imageInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .samples = VK_SAMPLE_COUNT_1_BIT};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_3D;
	imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	imageInfo.extent = {1u, 1u, 1u};
	imageInfo.mipLevels = 1u;
	imageInfo.arrayLayers = 1u;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

	const VkResult createResult = vmaCreateImage(
		context->allocator,
		&imageInfo,
		&allocationInfo,
		&render->volumetricFogFallbackImage,
		&render->volumetricFogFallbackAllocation,
		nullptr);
	if (createResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateVolumetricFogFallbackImage.vmaCreateImage", createResult);
		return false;
	}
	VmaAllocationInfo allocInfo{};
	vmaGetAllocationInfo(context->allocator, render->volumetricFogFallbackAllocation, &allocInfo);
	render->volumetricFogFallbackMemory = allocInfo.deviceMemory;

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = render->volumetricFogFallbackImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
	viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	if (vkCreateImageView(context->device, &viewInfo, nullptr, &render->volumetricFogFallbackView) != VK_SUCCESS) {
		vmaDestroyImage(
			context->allocator,
			render->volumetricFogFallbackImage,
			render->volumetricFogFallbackAllocation);
		render->volumetricFogFallbackImage = VK_NULL_HANDLE;
		render->volumetricFogFallbackAllocation = nullptr;
		render->volumetricFogFallbackMemory = VK_NULL_HANDLE;
		runtime::LogRuntimeFailure(
			"Render", "CreateVolumetricFogFallbackImage.vkCreateImageView", "failed");
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->volumetricFogFallbackImage), VK_OBJECT_TYPE_IMAGE, "VolumetricFogFallbackImage");
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->volumetricFogFallbackView), VK_OBJECT_TYPE_IMAGE_VIEW, "VolumetricFogFallbackView");
	return true;
}

namespace projectv::render {

void DestroyVolumetricFogResources(VulkanContextState *context, RenderState *render)
{
	if (context == nullptr || render == nullptr || context->device == VK_NULL_HANDLE) {
		return;
	}
	if (render->volumetricFogFallbackView != VK_NULL_HANDLE) {
		vkDestroyImageView(context->device, render->volumetricFogFallbackView, nullptr);
		render->volumetricFogFallbackView = VK_NULL_HANDLE;
	}
	if (render->volumetricFogFallbackImage != VK_NULL_HANDLE) {
		vmaDestroyImage(
			context->allocator,
			render->volumetricFogFallbackImage,
			render->volumetricFogFallbackAllocation);
		render->volumetricFogFallbackImage = VK_NULL_HANDLE;
		render->volumetricFogFallbackAllocation = nullptr;
		render->volumetricFogFallbackMemory = VK_NULL_HANDLE;
	}
	if (render->volumetricFogPipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(context->device, render->volumetricFogPipeline, nullptr);
		render->volumetricFogPipeline = VK_NULL_HANDLE;
	}
	if (render->volumetricFogPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(context->device, render->volumetricFogPipelineLayout, nullptr);
		render->volumetricFogPipelineLayout = VK_NULL_HANDLE;
	}
	if (render->volumetricFogShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context->device, render->volumetricFogShaderModule, nullptr);
		render->volumetricFogShaderModule = VK_NULL_HANDLE;
	}
	for (VkDescriptorSet &descriptorSet : render->volumetricFogDescriptorSets) {
		descriptorSet = VK_NULL_HANDLE;
	}
	if (render->volumetricFogDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(context->device, render->volumetricFogDescriptorPool, nullptr);
		render->volumetricFogDescriptorPool = VK_NULL_HANDLE;
	}
	if (render->volumetricFogDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(context->device, render->volumetricFogDescriptorSetLayout, nullptr);
		render->volumetricFogDescriptorSetLayout = VK_NULL_HANDLE;
	}
	if (render->volumetricFogLinearSampler != VK_NULL_HANDLE) {
		vkDestroySampler(context->device, render->volumetricFogLinearSampler, nullptr);
		render->volumetricFogLinearSampler = VK_NULL_HANDLE;
	}
	DestroyVolumetricFogFroxelImage(context, render);
	render->volumetricFogPipelineEnabled = false;
}

bool CreateVolumetricFogFallbackOnly(VulkanContextState *context, RenderState *render)
{
	PV_PROFILE_ZONE_N("CreateVolumetricFogFallbackOnly");
	PV_CHECK_OR_RETURN(
		context && render && context->device && context->allocator,
		"Render", "CreateVolumetricFogFallbackOnly.Preconditions", "missing context");
	if (!CreateVolumetricFogFallbackImage(context, render)) {
		return false;
	}
	if (!CreateVolumetricFogSampler(context, render)) { // EVIL: sampler must exist before binding-12 descriptor write (gate OFF path).
		return false;
	}
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = context->commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1u;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	if (vkAllocateCommandBuffers(context->device, &allocInfo, &cmd) != VK_SUCCESS) {
		return true; // Sampler is created; image layout transition is best-effort.
	}
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);
	VkImageMemoryBarrier2 imageBarrier{};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
	imageBarrier.srcAccessMask = 0;
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	imageBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageBarrier.image = render->volumetricFogFallbackImage;
	imageBarrier.subresourceRange = {
		VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.imageMemoryBarrierCount = 1u;
	depInfo.pImageMemoryBarriers = &imageBarrier;
	vkCmdPipelineBarrier2(cmd, &depInfo); // EVIL: transition fallback image UNDEFINED -> SHADER_READ_ONLY_OPTIMAL for binding 12.
	vkEndCommandBuffer(cmd);
	VkCommandBufferSubmitInfo commandBufferSubmitInfo{};
	commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	commandBufferSubmitInfo.commandBuffer = cmd;
	VkSubmitInfo2 submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.commandBufferInfoCount = 1u;
	submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;
	const VkResult submitResult = vkQueueSubmit2(context->queue, 1u, &submitInfo, VK_NULL_HANDLE);
	if (submitResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateVolumetricFogFallbackOnly.vkQueueSubmit2", submitResult);
		vkFreeCommandBuffers(context->device, context->commandPool, 1u, &cmd);
		return true;
	}
	vkQueueWaitIdle(context->queue);
	vkFreeCommandBuffers(context->device, context->commandPool, 1u, &cmd);
	return true;
}

bool CreateVolumetricFogResources(VulkanContextState *context, RenderState *render)
{
	PV_PROFILE_ZONE_N("CreateVolumetricFogResources");
	PV_CHECK_OR_RETURN(
		context && render && context->device && context->allocator,
		"Render", "CreateVolumetricFogResources.Preconditions", "missing context");
	if (!IsVolumetricFogEnabled()) {
		DestroyVolumetricFogResources(context, render);
		return false;
	}

	DestroyVolumetricFogResources(context, render);

	if (!CreateVolumetricFogFallbackImage(context, render)) {
		DestroyVolumetricFogResources(context, render);
		return false;
	}

	if (!CreateVolumetricFogFroxelImage(context, render)) {
		DestroyVolumetricFogResources(context, render);
		return false;
	}

	if (!CreateVolumetricFogSampler(context, render)) {
		DestroyVolumetricFogResources(context, render);
		return false;
	}

	const std::vector<char> shaderCode = ReadShaderFile(kVolumetricFogShaderFilename);
	if (shaderCode.empty()) {
		runtime::LogRuntimeFailure(
			"Render", "CreateVolumetricFogResources.ReadShaderFile", "volumetric_fog.comp.spv not found");
		DestroyVolumetricFogResources(context, render);
		return false;
	}

	VkShaderModuleCreateInfo moduleInfo{};
	moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleInfo.codeSize = shaderCode.size();
	moduleInfo.pCode = reinterpret_cast<const uint32_t *>(shaderCode.data());
	if (vkCreateShaderModule(context->device, &moduleInfo, nullptr, &render->volumetricFogShaderModule) != VK_SUCCESS) {
		DestroyVolumetricFogResources(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->volumetricFogShaderModule), VK_OBJECT_TYPE_SHADER_MODULE, "VolumetricFogShaderModule");

	const VkResult layoutResult = vkCreateDescriptorSetLayout(
		context->device,
		&kVolumetricFogDescriptorSetLayoutInfo,
		nullptr,
		&render->volumetricFogDescriptorSetLayout);
	if (layoutResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateVolumetricFogResources.vkCreateDescriptorSetLayout", layoutResult);
		DestroyVolumetricFogResources(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->volumetricFogDescriptorSetLayout), VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "VolumetricFogDescriptorSetLayout");

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(VolumetricFogPushConstants);

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &render->volumetricFogDescriptorSetLayout;
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushConstantRange;
	if (vkCreatePipelineLayout(context->device, &layoutInfo, nullptr, &render->volumetricFogPipelineLayout) != VK_SUCCESS) {
		DestroyVolumetricFogResources(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->volumetricFogPipelineLayout), VK_OBJECT_TYPE_PIPELINE_LAYOUT, "VolumetricFogPipelineLayout");

	const VkPipelineShaderStageCreateInfo stage{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = render->volumetricFogShaderModule,
		.pName = "main",
	};

	VkComputePipelineCreateInfo pipelineInfo{.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .stage = stage};
	pipelineInfo.layout = render->volumetricFogPipelineLayout;
	if (vkCreateComputePipelines(context->device, context->pipelineCache, 1u, &pipelineInfo, nullptr, &render->volumetricFogPipeline) != VK_SUCCESS) {
		DestroyVolumetricFogResources(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->volumetricFogPipeline), VK_OBJECT_TYPE_PIPELINE, "VolumetricFogPipeline");

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
	poolInfo.poolSizeCount = static_cast<uint32_t>(kVolumetricFogDescriptorPoolSizes.size());
	poolInfo.pPoolSizes = kVolumetricFogDescriptorPoolSizes.data();
	if (vkCreateDescriptorPool(context->device, &poolInfo, nullptr, &render->volumetricFogDescriptorPool) != VK_SUCCESS) {
		DestroyVolumetricFogResources(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->volumetricFogDescriptorPool), VK_OBJECT_TYPE_DESCRIPTOR_POOL, "VolumetricFogDescriptorPool");

	std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts{};
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		layouts[i] = render->volumetricFogDescriptorSetLayout;
	}
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = render->volumetricFogDescriptorPool;
	allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	allocInfo.pSetLayouts = layouts.data();
	if (vkAllocateDescriptorSets(context->device, &allocInfo, render->volumetricFogDescriptorSets.data()) != VK_SUCCESS) {
		DestroyVolumetricFogResources(context, render);
		return false;
	}

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		VkDescriptorImageInfo froxelInfo{};
		froxelInfo.sampler = VK_NULL_HANDLE;
		froxelInfo.imageView = render->volumetricFogFroxelView;
		froxelInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkDescriptorImageInfo sceneColorInfo{};
		sceneColorInfo.sampler = render->volumetricFogLinearSampler;
		sceneColorInfo.imageView = render->sceneColorImageView != VK_NULL_HANDLE
									   ? render->sceneColorImageView
									   : VK_NULL_HANDLE;
		sceneColorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo depthInfo{};
		depthInfo.sampler = render->volumetricFogLinearSampler;
		depthInfo.imageView = render->depthImageView;
		depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;

		std::array<VkWriteDescriptorSet, 3> writes{};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = render->volumetricFogDescriptorSets[i];
		writes[0].dstBinding = 0;
		writes[0].dstArrayElement = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[0].pImageInfo = &froxelInfo;

		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = render->volumetricFogDescriptorSets[i];
		writes[1].dstBinding = 1;
		writes[1].dstArrayElement = 0;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		writes[1].pImageInfo = &sceneColorInfo;

		writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2].dstSet = render->volumetricFogDescriptorSets[i];
		writes[2].dstBinding = 2;
		writes[2].dstArrayElement = 0;
		writes[2].descriptorCount = 1;
		writes[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		writes[2].pImageInfo = &depthInfo;

		vkUpdateDescriptorSets(
			context->device,
			static_cast<uint32_t>(writes.size()),
			writes.data(),
			0u,
			nullptr);
	}

	render->volumetricFogPipelineEnabled = true;
	return true;
}

bool RecordVolumetricFogAccumulationPass(
	const VkCommandBuffer commandBuffer,
	RenderState &render,
	const VolumetricFogPushConstants &pushConstants,
	const uint32_t frameIndex)
{
	PV_PROFILE_ZONE_N("RecordVolumetricFogAccumulationPass");
	if (commandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (render.volumetricFogPipeline == VK_NULL_HANDLE ||
		render.volumetricFogPipelineLayout == VK_NULL_HANDLE) {
		return false;
	}
	if (frameIndex >= MAX_FRAMES_IN_FLIGHT) {
		return false;
	}
	if (render.volumetricFogDescriptorSets[frameIndex] == VK_NULL_HANDLE) {
		return false;
	}
	if (render.volumetricFogFroxelImage == VK_NULL_HANDLE) {
		return false;
	}

	VkImageMemoryBarrier2 preBarrier{};
	preBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	preBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
	preBarrier.srcAccessMask = VK_ACCESS_2_NONE;
	preBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	preBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	preBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	preBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	preBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	preBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	preBarrier.image = render.volumetricFogFroxelImage;
	preBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	VkDependencyInfo preDepInfo{};
	preDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	preDepInfo.imageMemoryBarrierCount = 1u;
	preDepInfo.pImageMemoryBarriers = &preBarrier;
	vkCmdPipelineBarrier2(commandBuffer, &preDepInfo);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, render.volumetricFogPipeline);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		render.volumetricFogPipelineLayout,
		0u,
		1u,
		&render.volumetricFogDescriptorSets[frameIndex],
		0u,
		nullptr);
	vkCmdPushConstants(
		commandBuffer,
		render.volumetricFogPipelineLayout,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0u,
		sizeof(VolumetricFogPushConstants),
		&pushConstants);
	vkCmdDispatch(
		commandBuffer,
		kVolumetricFogFroxelWidth / 8u,
		kVolumetricFogFroxelHeight / 8u,
		kVolumetricFogFroxelDepth / 4u);

	VkImageMemoryBarrier2 postBarrier{};
	postBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	postBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	postBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	postBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	postBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	postBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	postBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	postBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	postBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	postBarrier.image = render.volumetricFogFroxelImage;
	postBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	VkDependencyInfo postDepInfo{};
	postDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	postDepInfo.imageMemoryBarrierCount = 1u;
	postDepInfo.pImageMemoryBarriers = &postBarrier;
	vkCmdPipelineBarrier2(commandBuffer, &postDepInfo);

	profiling::PlotValue("Volumetric Fog Pass", 1.0);
	return true;
}

} // namespace projectv::render

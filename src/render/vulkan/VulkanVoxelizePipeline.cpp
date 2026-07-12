#include "render/vulkan/VulkanVoxelizePipeline.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "debug/Profiling.hpp"
#include "render/SceneResources.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <array>
#include <vector>

namespace {
constexpr uint32_t kVoxelizeDescriptorSetCount = MAX_FRAMES_IN_FLIGHT;
constexpr char kVoxelizeShaderFilename[] = "voxelize.comp.spv";

constexpr std::array kVoxelizeDescriptorBindings{
	VkDescriptorSetLayoutBinding{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 2,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr,
	},
};

constexpr VkDescriptorSetLayoutCreateInfo kVoxelizeDescriptorSetLayoutInfo{
	.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	.pNext = nullptr,
	.flags = 0,
	.bindingCount = static_cast<uint32_t>(kVoxelizeDescriptorBindings.size()),
	.pBindings = kVoxelizeDescriptorBindings.data(),
};

constexpr std::array kVoxelizeDescriptorPoolSizes{
	VkDescriptorPoolSize{
		.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = kVoxelizeDescriptorSetCount * 2u,
	},
	VkDescriptorPoolSize{
		.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		.descriptorCount = kVoxelizeDescriptorSetCount,
	},
};

bool CreateVoxelizeClipmapImage(
	VulkanContextState *context,
	RenderState *render)
{
	PV_CHECK_OR_RETURN(
		context && render && context->device != VK_NULL_HANDLE && context->allocator != VK_NULL_HANDLE,
		"Render", "CreateVoxelizeClipmapImage.Preconditions", "missing context");

	if (render->vctClipmapImage != VK_NULL_HANDLE) {
		return true;
	}

	const uint32_t resolution = render->vctClipmapResolution;
	const uint32_t mipLevels = render->vctClipmapMipLevelCount;
	if (resolution == 0u || mipLevels == 0u) {
		return false;
	}

	VkImageCreateInfo imageInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .samples = VK_SAMPLE_COUNT_1_BIT};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_3D;
	imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	imageInfo.extent = {resolution, resolution, resolution};
	imageInfo.mipLevels = mipLevels;
	imageInfo.arrayLayers = 1u;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

	const VkResult createResult = vmaCreateImage(
		context->allocator,
		&imageInfo,
		&allocationInfo,
		&render->vctClipmapImage,
		&render->vctClipmapAllocation,
		nullptr);
	if (createResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateVoxelizeClipmapImage.vmaCreateImage", createResult);
		return false;
	}

	VmaAllocationInfo allocInfo{};
	vmaGetAllocationInfo(context->allocator, render->vctClipmapAllocation, &allocInfo);
	render->vctClipmapMemory = allocInfo.deviceMemory;

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = render->vctClipmapImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
	viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, mipLevels, 0u, 1u};
	if (vkCreateImageView(context->device, &viewInfo, nullptr, &render->vctClipmapView) != VK_SUCCESS) {
		vmaDestroyImage(context->allocator, render->vctClipmapImage, render->vctClipmapAllocation);
		render->vctClipmapImage = VK_NULL_HANDLE;
		render->vctClipmapAllocation = nullptr;
		render->vctClipmapMemory = VK_NULL_HANDLE;
		runtime::LogRuntimeFailure(
			"Render", "CreateVoxelizeClipmapImage.vkCreateImageView", "failed");
		return false;
	}

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = static_cast<float>(mipLevels);
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;

	if (vkCreateSampler(context->device, &samplerInfo, nullptr, &render->vctClipmapSampler) != VK_SUCCESS) {
		vkDestroyImageView(context->device, render->vctClipmapView, nullptr);
		vmaDestroyImage(context->allocator, render->vctClipmapImage, render->vctClipmapAllocation);
		render->vctClipmapImage = VK_NULL_HANDLE;
		render->vctClipmapView = VK_NULL_HANDLE;
		render->vctClipmapAllocation = nullptr;
		render->vctClipmapMemory = VK_NULL_HANDLE;
		runtime::LogRuntimeFailure(
			"Render", "CreateVoxelizeClipmapImage.vkCreateSampler", "failed");
		return false;
	}

	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->vctClipmapImage), VK_OBJECT_TYPE_IMAGE, "VctClipmapImage");
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->vctClipmapView), VK_OBJECT_TYPE_IMAGE_VIEW, "VctClipmapView");
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->vctClipmapSampler), VK_OBJECT_TYPE_SAMPLER, "VctClipmapSampler");
	return true;
}
} // namespace

void DestroyVoxelizeClipmapImage(
	VulkanContextState *context,
	RenderState *render)
{
	if (context == nullptr || render == nullptr || context->device == VK_NULL_HANDLE) {
		return;
	}
	if (render->vctClipmapSampler != VK_NULL_HANDLE) {
		vkDestroySampler(context->device, render->vctClipmapSampler, nullptr);
		render->vctClipmapSampler = VK_NULL_HANDLE;
	}
	if (render->vctClipmapView != VK_NULL_HANDLE) {
		vkDestroyImageView(context->device, render->vctClipmapView, nullptr);
		render->vctClipmapView = VK_NULL_HANDLE;
	}
	if (render->vctClipmapImage != VK_NULL_HANDLE) {
		vmaDestroyImage(context->allocator, render->vctClipmapImage, render->vctClipmapAllocation);
		render->vctClipmapImage = VK_NULL_HANDLE;
		render->vctClipmapAllocation = nullptr;
		render->vctClipmapMemory = VK_NULL_HANDLE;
	}
}

namespace projectv::render {

bool IsVctGpuPipelineRequested()
{
	const char *value = std::getenv("PROJECTV_VCT_GPU");
	if (value == nullptr) {
		return false;
	}
	return value[0] != '\0' && value[0] != '0';
}

bool CreateVctClipmapFallbackSamplerOnly(VulkanContextState *context, RenderState *render)
{
	PV_CHECK_OR_RETURN(
		context && render && context->device != VK_NULL_HANDLE,
		"Render", "CreateVctClipmapFallbackSamplerOnly.Preconditions", "missing context");
	if (render->vctClipmapSampler != VK_NULL_HANDLE) {
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
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 1.0f;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	const VkResult createSamplerResult = vkCreateSampler(
		context->device, &samplerInfo, nullptr, &render->vctClipmapSampler);
	if (createSamplerResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateVctClipmapFallbackSamplerOnly.vkCreateSampler", createSamplerResult);
		return false;
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->vctClipmapSampler),
		VK_OBJECT_TYPE_SAMPLER,
		"VctClipmapFallbackSampler");
	return true;
}

void DestroyVoxelizePipelines(VulkanContextState *context, RenderState *render)
{
	if (context == nullptr || render == nullptr) {
		return;
	}
	if (render->vctVoxelizeDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(context->device, render->vctVoxelizeDescriptorPool, nullptr);
		render->vctVoxelizeDescriptorPool = VK_NULL_HANDLE;
	}
	if (render->vctVoxelizeDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(context->device, render->vctVoxelizeDescriptorSetLayout, nullptr);
		render->vctVoxelizeDescriptorSetLayout = VK_NULL_HANDLE;
	}
	if (render->vctVoxelizePipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(context->device, render->vctVoxelizePipeline, nullptr);
		render->vctVoxelizePipeline = VK_NULL_HANDLE;
	}
	if (render->vctVoxelizePipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(context->device, render->vctVoxelizePipelineLayout, nullptr);
		render->vctVoxelizePipelineLayout = VK_NULL_HANDLE;
	}
	if (render->vctVoxelizeShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context->device, render->vctVoxelizeShaderModule, nullptr);
		render->vctVoxelizeShaderModule = VK_NULL_HANDLE;
	}
	for (VkDescriptorSet &descriptorSet : render->vctVoxelizeDescriptorSets) {
		descriptorSet = VK_NULL_HANDLE;
	}
	DestroyVoxelizeClipmapImage(context, render);
	render->vctClipmapEnabled = false;
}

bool CreateVoxelizePipelines(VulkanContextState *context, RenderState *render)
{
	PV_PROFILE_ZONE_N("CreateVoxelizePipelines");
	PV_CHECK_OR_RETURN(
		context && render && context->device && context->allocator,
		"Render", "CreateVoxelizePipelines.Preconditions", "missing context");
	if (!IsVctGpuPipelineRequested()) {
		return false;
	}

	DestroyVoxelizePipelines(context, render);

	if (!CreateVoxelizeClipmapImage(context, render)) {
		return false;
	}

	const std::vector<char> shaderCode = ReadShaderFile(kVoxelizeShaderFilename);
	if (shaderCode.empty()) {
		runtime::LogRuntimeFailure(
			"Render", "CreateVoxelizePipelines.ReadShaderFile", "voxelize.comp.spv not found");
		DestroyVoxelizePipelines(context, render);
		return false;
	}

	VkShaderModuleCreateInfo moduleInfo{};
	moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleInfo.codeSize = shaderCode.size();
	moduleInfo.pCode = reinterpret_cast<const uint32_t *>(shaderCode.data());
	if (vkCreateShaderModule(context->device, &moduleInfo, nullptr, &render->vctVoxelizeShaderModule) != VK_SUCCESS) {
		DestroyVoxelizePipelines(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->vctVoxelizeShaderModule), VK_OBJECT_TYPE_SHADER_MODULE, "VoxelizeShaderModule");

	const VkResult layoutResult = vkCreateDescriptorSetLayout(
		context->device, &kVoxelizeDescriptorSetLayoutInfo, nullptr, &render->vctVoxelizeDescriptorSetLayout);
	if (layoutResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateVoxelizePipelines.vkCreateDescriptorSetLayout", layoutResult);
		DestroyVoxelizePipelines(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->vctVoxelizeDescriptorSetLayout), VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "VoxelizeDescriptorSetLayout");

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(VoxelizePushConstants);

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &render->vctVoxelizeDescriptorSetLayout;
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushConstantRange;
	if (vkCreatePipelineLayout(context->device, &layoutInfo, nullptr, &render->vctVoxelizePipelineLayout) != VK_SUCCESS) {
		DestroyVoxelizePipelines(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->vctVoxelizePipelineLayout), VK_OBJECT_TYPE_PIPELINE_LAYOUT, "VoxelizePipelineLayout");

	const VkPipelineShaderStageCreateInfo stage{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = render->vctVoxelizeShaderModule,
		.pName = "main",
		.pSpecializationInfo = nullptr,
	};

	VkComputePipelineCreateInfo pipelineInfo{.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .stage = stage};
	pipelineInfo.layout = render->vctVoxelizePipelineLayout;
	if (vkCreateComputePipelines(context->device, VK_NULL_HANDLE, 1u, &pipelineInfo, nullptr, &render->vctVoxelizePipeline) != VK_SUCCESS) {
		DestroyVoxelizePipelines(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->vctVoxelizePipeline), VK_OBJECT_TYPE_PIPELINE, "VoxelizePipeline");

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = kVoxelizeDescriptorSetCount;
	poolInfo.poolSizeCount = static_cast<uint32_t>(kVoxelizeDescriptorPoolSizes.size());
	poolInfo.pPoolSizes = kVoxelizeDescriptorPoolSizes.data();
	if (vkCreateDescriptorPool(context->device, &poolInfo, nullptr, &render->vctVoxelizeDescriptorPool) != VK_SUCCESS) {
		DestroyVoxelizePipelines(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->vctVoxelizeDescriptorPool), VK_OBJECT_TYPE_DESCRIPTOR_POOL, "VoxelizeDescriptorPool");

	std::array<VkDescriptorSetLayout, kVoxelizeDescriptorSetCount> layouts{};
	for (uint32_t i = 0; i < kVoxelizeDescriptorSetCount; ++i) {
		layouts[i] = render->vctVoxelizeDescriptorSetLayout;
	}
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = render->vctVoxelizeDescriptorPool;
	allocInfo.descriptorSetCount = kVoxelizeDescriptorSetCount;
	allocInfo.pSetLayouts = layouts.data();
	if (vkAllocateDescriptorSets(context->device, &allocInfo, render->vctVoxelizeDescriptorSets.data()) != VK_SUCCESS) {
		DestroyVoxelizePipelines(context, render);
		return false;
	}

	render->vctClipmapEnabled = true;
	return true;
}

bool RefreshVoxelizeResourceBindings(
	VulkanContextState *context,
	RenderState *render,
	const uint32_t frameIndex)
{
	if (context == nullptr || render == nullptr) {
		return false;
	}
	if (render->vctVoxelizeDescriptorSetLayout == VK_NULL_HANDLE) {
		return false;
	}
	if (frameIndex >= render->sceneFrameResources.size()) {
		return false;
	}
	if (render->vctClipmapImage == VK_NULL_HANDLE || render->vctClipmapView == VK_NULL_HANDLE) {
		return false;
	}

	const SceneFrameResources &frameResources = render->sceneFrameResources[frameIndex];
	if (frameResources.chunkDescriptorBuffer == VK_NULL_HANDLE ||
		frameResources.chunkVoxelPayloadBuffer == VK_NULL_HANDLE) {
		return false;
	}

	VkDescriptorBufferInfo chunkDescriptorInfo{};
	chunkDescriptorInfo.buffer = frameResources.chunkDescriptorBuffer;
	chunkDescriptorInfo.offset = 0;
	chunkDescriptorInfo.range = VK_WHOLE_SIZE;

	VkDescriptorBufferInfo chunkVoxelPayloadInfo{};
	chunkVoxelPayloadInfo.buffer = frameResources.chunkVoxelPayloadBuffer;
	chunkVoxelPayloadInfo.offset = 0;
	chunkVoxelPayloadInfo.range = VK_WHOLE_SIZE;

	VkDescriptorImageInfo clipmapInfo{};
	clipmapInfo.sampler = VK_NULL_HANDLE;
	clipmapInfo.imageView = render->vctClipmapView;
	clipmapInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	std::array<VkWriteDescriptorSet, 3> writes{};
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = render->vctVoxelizeDescriptorSets[frameIndex];
	writes[0].dstBinding = 0;
	writes[0].dstArrayElement = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &chunkDescriptorInfo;

	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = render->vctVoxelizeDescriptorSets[frameIndex];
	writes[1].dstBinding = 1;
	writes[1].dstArrayElement = 0;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[1].pBufferInfo = &chunkVoxelPayloadInfo;

	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = render->vctVoxelizeDescriptorSets[frameIndex];
	writes[2].dstBinding = 2;
	writes[2].dstArrayElement = 0;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[2].pImageInfo = &clipmapInfo;

	vkUpdateDescriptorSets(
		context->device,
		static_cast<uint32_t>(writes.size()),
		writes.data(),
		0u,
		nullptr);
	return true;
}

bool RecordVoxelizeDispatch(
	const VkCommandBuffer commandBuffer,
	RenderState &render,
	SceneFrameResources &frameResources,
	const VoxelizePushConstants &pushConstants,
	const uint32_t activeChunkCount)
{
	PV_PROFILE_ZONE_N("RecordVoxelizeDispatch");
	if (commandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (render.vctVoxelizePipeline == VK_NULL_HANDLE ||
		render.vctVoxelizePipelineLayout == VK_NULL_HANDLE) {
		return false;
	}
	if (frameResources.vctVoxelizeDescriptorSet == VK_NULL_HANDLE) {
		return false;
	}
	if (activeChunkCount == 0u) {
		return true;
	}

	profiling::PlotValue("VCT Voxelize Chunks", static_cast<int64_t>(activeChunkCount));

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, render.vctVoxelizePipeline);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		render.vctVoxelizePipelineLayout,
		0u,
		1u,
		&frameResources.vctVoxelizeDescriptorSet,
		0u,
		nullptr);
	vkCmdPushConstants(
		commandBuffer,
		render.vctVoxelizePipelineLayout,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0u,
		sizeof(VoxelizePushConstants),
		&pushConstants);
	vkCmdDispatch(commandBuffer, activeChunkCount, 1u, 1u);
	return true;
}

bool BuildVctClipmapMipChain(
	const VkCommandBuffer commandBuffer,
	RenderState &render)
{
	PV_PROFILE_ZONE_N("BuildVctClipmapMipChain");
	if (commandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (render.vctClipmapImage == VK_NULL_HANDLE) {
		return false;
	}
	const uint32_t mipLevels = render.vctClipmapMipLevelCount;
	if (mipLevels <= 1u) {
		return true;
	}
	const uint32_t resolution = render.vctClipmapResolution;
	if (resolution == 0u) {
		return false;
	}

	profiling::PlotValue("VCT Mip Chain Mips", static_cast<int64_t>(mipLevels - 1u));

	static constexpr VkImageLayout prevLayout = VK_IMAGE_LAYOUT_GENERAL;
	static constexpr VkImageLayout newLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkImageMemoryBarrier preBarrier{};
	preBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	preBarrier.pNext = nullptr;
	preBarrier.srcAccessMask = 0u;
	preBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	preBarrier.oldLayout = prevLayout;
	preBarrier.newLayout = prevLayout;
	preBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	preBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	preBarrier.image = render.vctClipmapImage;
	preBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0u,
		0u, nullptr,
		0u, nullptr,
		1u, &preBarrier);

	uint32_t srcWidth = resolution;
	uint32_t srcHeight = resolution;
	uint32_t srcDepth = resolution;
	for (uint32_t mipLevel = 1u; mipLevel < mipLevels; ++mipLevel) {
		const uint32_t dstWidth = std::max(1u, srcWidth >> 1u);
		const uint32_t dstHeight = std::max(1u, srcHeight >> 1u);
		const uint32_t dstDepth = std::max(1u, srcDepth >> 1u);

		VkImageBlit blit{};
		blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevel - 1u, 0u, 1u};
		blit.srcOffsets[0] = {0, 0, 0};
		blit.srcOffsets[1] = {static_cast<int32_t>(srcWidth), static_cast<int32_t>(srcHeight), static_cast<int32_t>(srcDepth)};
		blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 0u, 1u};
		blit.dstOffsets[0] = {0, 0, 0};
		blit.dstOffsets[1] = {static_cast<int32_t>(dstWidth), static_cast<int32_t>(dstHeight), static_cast<int32_t>(dstDepth)};
		vkCmdBlitImage(
			commandBuffer,
			render.vctClipmapImage,
			VK_IMAGE_LAYOUT_GENERAL,
			render.vctClipmapImage,
			VK_IMAGE_LAYOUT_GENERAL,
			1u, &blit,
			VK_FILTER_LINEAR);

		VkImageMemoryBarrier mipBarrier{};
		mipBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		mipBarrier.pNext = nullptr;
		mipBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		mipBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		mipBarrier.oldLayout = newLayout;
		mipBarrier.newLayout = newLayout;
		mipBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		mipBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		mipBarrier.image = render.vctClipmapImage;
		mipBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 1u, 0u, 1u};
		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0u,
			0u, nullptr,
			0u, nullptr,
			1u, &mipBarrier);

		srcWidth = dstWidth;
		srcHeight = dstHeight;
		srcDepth = dstDepth;
	}

	(void)prevLayout;
	(void)newLayout;
	return true;
}

} // namespace projectv::render

#include "render/HizCulling.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/EnvUtils.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

#include "SDL3/SDL_log.h"
#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "render/vulkan/VulkanDebug.hpp"

namespace {
constexpr uint32_t kHizCullingDescriptorSetCount = MAX_FRAMES_IN_FLIGHT;
constexpr char kHizCullingShaderFilename[] = "hzb_cull.comp.spv";

constexpr std::array kHizCullingDescriptorBindings{
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
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 3,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 4,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 5,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr,
	},
};

constexpr VkDescriptorSetLayoutCreateInfo kHizCullingDescriptorSetLayoutInfo{
	.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	.pNext = nullptr,
	.flags = 0,
	.bindingCount = static_cast<uint32_t>(kHizCullingDescriptorBindings.size()),
	.pBindings = kHizCullingDescriptorBindings.data(),
};

constexpr std::array kHizCullingDescriptorPoolSizes{
	VkDescriptorPoolSize{
		.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = kHizCullingDescriptorSetCount * 4u,
	},
	VkDescriptorPoolSize{
		.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		.descriptorCount = kHizCullingDescriptorSetCount,
	},
	VkDescriptorPoolSize{
		.type = VK_DESCRIPTOR_TYPE_SAMPLER,
		.descriptorCount = kHizCullingDescriptorSetCount,
	},
	VkDescriptorPoolSize{
		.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		.descriptorCount = 0u,
	},
};
} // namespace

namespace projectv::render {

bool IsHzbCullingEnabled()
{
	if (const char *value = core::GetEnvVar("PROJECTV_HZB_CULLING")) {
		return value[0] != '\0' && value[0] != '0';
	}
	return false;
}

bool IsMeshShaderPipelineEnabled()
{
	if (const char *value = core::GetEnvVar("PROJECTV_MESH_SHADER_PIPELINE")) {
		return value[0] != '\0' && value[0] != '0';
	}
	return false;
}

bool IsHzbSmartMipEnabled()
{
	if (const char *value = core::GetEnvVar("PROJECTV_HZB_SMART_MIP")) {
		return value[0] != '\0' && value[0] != '0';
	}
	return false;
}

bool IsHzbSmartBlendWidthEnabled()
{
	if (const char *value = core::GetEnvVar("PROJECTV_HZB_SMART_BLEND_WIDTH")) {
		return value[0] == 'O' && value[1] == 'N';
	}
	return false;
}

bool IsHzbMinMipEnabled()
{
	if (const char *value = core::GetEnvVar("PROJECTV_HZB_MIN_MIP")) {
		if (value[0] == '0' || (value[0] == 'O' && value[1] == 'F')) {
			return false;
		}
		return value[0] != '\0';
	}
	return true; // default ON once minify pipeline is available
}

uint32_t ComputeBlendWidthForChunkMip(
	const uint32_t projectedExtentXTexels,
	const uint32_t projectedExtentYTexels,
	const uint32_t mipLevel,
	const uint32_t maxBlendWidth)
{
	if (maxBlendWidth == 0u) {
		return 0u;
	}
	const uint32_t maxExtent = std::max(projectedExtentXTexels, projectedExtentYTexels);
	if (maxExtent == 0u || mipLevel == 0u) {
		return 0u;
	}
	const uint32_t texelsAtMip = std::max<uint32_t>(maxExtent >> mipLevel, 1u);
	const uint32_t frac = maxExtent % (1u << std::min<uint32_t>(mipLevel, 16u));
	const uint32_t blendEstimate = std::min<uint32_t>(texelsAtMip / 4u + frac / 8u, maxBlendWidth);
	return std::min<uint32_t>(blendEstimate, maxBlendWidth);
}

uint32_t ComputeHzbMipLevelCount(const uint32_t baseWidth, const uint32_t baseHeight)
{
	const uint32_t minExtent = std::max(1u, std::min(baseWidth, baseHeight));
	uint32_t levels = 1u;
	uint32_t currentExtent = minExtent;
	while (currentExtent > 1u) {
		currentExtent = std::max(1u, currentExtent >> 1u);
		++levels;
	}
	return levels;
}
bool CreateHizBuffer(
	VulkanContextState *context,
	const uint32_t baseWidth,
	const uint32_t baseHeight,
	HizBuffer &outBuffer)
{
	if (!context || !context->allocator || context->device == VK_NULL_HANDLE) {
		return false;
	}
	if (baseWidth == 0u || baseHeight == 0u) {
		return false;
	}

	const uint32_t mipLevels = ComputeHzbMipLevelCount(baseWidth, baseHeight);

	VkImageCreateInfo imageInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .samples = VK_SAMPLE_COUNT_1_BIT};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R32_SFLOAT;
	imageInfo.extent = {baseWidth, baseHeight, 1u};
	imageInfo.mipLevels = mipLevels;
	imageInfo.arrayLayers = 1u;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
					  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

	if (vmaCreateImage(
			context->allocator,
			&imageInfo,
			&allocationInfo,
			&outBuffer.image,
			&outBuffer.allocation,
			nullptr) != VK_SUCCESS) {
		runtime::LogRuntimeFailure(
			"Render",
			"CreateHizBuffer.vmaCreateImage",
			"failed to allocate Hi-Z mip chain image");
		return false;
	}

	VmaAllocationInfo vmaInfo{};
	vmaGetAllocationInfo(context->allocator, outBuffer.allocation, &vmaInfo);
	outBuffer.memory = vmaInfo.deviceMemory;

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = outBuffer.image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R32_SFLOAT;
	viewInfo.subresourceRange = {
		VK_IMAGE_ASPECT_COLOR_BIT,
		0u,
		mipLevels,
		0u,
		1u,
	};
	if (vkCreateImageView(context->device, &viewInfo, nullptr, &outBuffer.imageView) != VK_SUCCESS) {
		vmaDestroyImage(context->allocator, outBuffer.image, outBuffer.allocation);
		outBuffer = {};
		runtime::LogRuntimeFailure(
			"Render",
			"CreateHizBuffer.vkCreateImageView",
			"failed to create Hi-Z image view");
		return false;
	}

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_NEAREST;
	samplerInfo.minFilter = VK_FILTER_NEAREST;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;

	if (vkCreateSampler(context->device, &samplerInfo, nullptr, &outBuffer.sampler) != VK_SUCCESS) {
		vkDestroyImageView(context->device, outBuffer.imageView, nullptr);
		vmaDestroyImage(context->allocator, outBuffer.image, outBuffer.allocation);
		outBuffer = {};
		runtime::LogRuntimeFailure(
			"Render",
			"CreateHizBuffer.vkCreateSampler",
			"failed to create Hi-Z sampler");
		return false;
	}

	outBuffer.baseWidth = baseWidth;
	outBuffer.baseHeight = baseHeight;
	outBuffer.mipLevelCount = mipLevels;
	outBuffer.mipStorageViewCount = 0u;
	outBuffer.mipStorageViews = {};
	const uint32_t viewCount = std::min(mipLevels, static_cast<uint32_t>(outBuffer.mipStorageViews.size()));
	for (uint32_t mip = 0u; mip < viewCount; ++mip) {
		VkImageViewCreateInfo mipViewInfo{};
		mipViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		mipViewInfo.image = outBuffer.image;
		mipViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		mipViewInfo.format = VK_FORMAT_R32_SFLOAT;
		mipViewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, mip, 1u, 0u, 1u};
		if (vkCreateImageView(context->device, &mipViewInfo, nullptr, &outBuffer.mipStorageViews[mip]) != VK_SUCCESS) {
			for (uint32_t destroyMip = 0u; destroyMip < mip; ++destroyMip) {
				vkDestroyImageView(context->device, outBuffer.mipStorageViews[destroyMip], nullptr);
				outBuffer.mipStorageViews[destroyMip] = VK_NULL_HANDLE;
			}
			vkDestroySampler(context->device, outBuffer.sampler, nullptr);
			vkDestroyImageView(context->device, outBuffer.imageView, nullptr);
			vmaDestroyImage(context->allocator, outBuffer.image, outBuffer.allocation);
			outBuffer = {};
			runtime::LogRuntimeFailure(
				"Render",
				"CreateHizBuffer.mipStorageView",
				"failed to create Hi-Z per-mip storage view");
			return false;
		}
		outBuffer.mipStorageViewCount = mip + 1u;
	}

	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(outBuffer.image),
		VK_OBJECT_TYPE_IMAGE,
		"HizBufferImage");
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(outBuffer.imageView),
		VK_OBJECT_TYPE_IMAGE_VIEW,
		"HizBufferImageView");
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(outBuffer.sampler),
		VK_OBJECT_TYPE_SAMPLER,
		"HizBufferSampler");
	return true;
}

void DestroyHizBuffer(VulkanContextState *context, HizBuffer &buffer)
{
	if (!context || !context->allocator) {
		return;
	}
	if (context->device != VK_NULL_HANDLE) {
		for (uint32_t mip = 0u; mip < buffer.mipStorageViewCount; ++mip) {
			if (buffer.mipStorageViews[mip] != VK_NULL_HANDLE) {
				vkDestroyImageView(context->device, buffer.mipStorageViews[mip], nullptr);
				buffer.mipStorageViews[mip] = VK_NULL_HANDLE;
			}
		}
	}
	buffer.mipStorageViewCount = 0u;
	if (buffer.sampler != VK_NULL_HANDLE && context->device != VK_NULL_HANDLE) {
		vkDestroySampler(context->device, buffer.sampler, nullptr);
	}
	if (buffer.imageView != VK_NULL_HANDLE && context->device != VK_NULL_HANDLE) {
		vkDestroyImageView(context->device, buffer.imageView, nullptr);
	}
	if (buffer.image != VK_NULL_HANDLE) {
		vmaDestroyImage(context->allocator, buffer.image, buffer.allocation);
	}
	buffer = {};
}
bool CreateHizCullingPipeline(
	VulkanContextState *context,
	RenderState *render)
{
	if (!context || !render || context->device == VK_NULL_HANDLE) {
		return false;
	}

	DestroyHizCullingPipeline(context, render);

	const std::vector<char> cullShaderCode = ReadShaderFile(kHizCullingShaderFilename);
	if (cullShaderCode.empty()) {
		runtime::LogRuntimeFailure(
			"Render",
			"CreateHizCullingPipeline.ReadShaderFile",
			"failed to read hzb_cull.comp.spv");
		DestroyHizCullingPipeline(context, render);
		return false;
	}

	VkShaderModuleCreateInfo moduleInfo{};
	moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleInfo.codeSize = cullShaderCode.size();
	moduleInfo.pCode = reinterpret_cast<const uint32_t *>(cullShaderCode.data());

	const VkResult moduleResult = vkCreateShaderModule(
		context->device,
		&moduleInfo,
		nullptr,
		&render->hizCullingShaderModule);
	if (moduleResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"CreateHizCullingPipeline.vkCreateShaderModule",
			moduleResult);
		DestroyHizCullingPipeline(context, render);
		return false;
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->hizCullingShaderModule),
		VK_OBJECT_TYPE_SHADER_MODULE,
		"HizCullingShaderModule");

	const VkResult layoutResult = vkCreateDescriptorSetLayout(
		context->device,
		&kHizCullingDescriptorSetLayoutInfo,
		nullptr,
		&render->hizCullingDescriptorSetLayout);
	if (layoutResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"CreateHizCullingPipeline.vkCreateDescriptorSetLayout",
			layoutResult);
		DestroyHizCullingPipeline(context, render);
		return false;
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->hizCullingDescriptorSetLayout),
		VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
		"HizCullingDescriptorSetLayout");

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(HizCullingPushConstants);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &render->hizCullingDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	const VkResult pipelineLayoutResult = vkCreatePipelineLayout(
		context->device,
		&pipelineLayoutInfo,
		nullptr,
		&render->hizCullingPipelineLayout);
	if (pipelineLayoutResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"CreateHizCullingPipeline.vkCreatePipelineLayout",
			pipelineLayoutResult);
		DestroyHizCullingPipeline(context, render);
		return false;
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->hizCullingPipelineLayout),
		VK_OBJECT_TYPE_PIPELINE_LAYOUT,
		"HizCullingPipelineLayout");

	const VkPipelineShaderStageCreateInfo cullStage{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = render->hizCullingShaderModule,
		.pName = "main",
		.pSpecializationInfo = nullptr,
	};

	VkComputePipelineCreateInfo pipelineInfo{.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .stage = cullStage};
	pipelineInfo.layout = render->hizCullingPipelineLayout;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = 0;

	const VkResult pipelineResult = vkCreateComputePipelines(
		context->device,
		context->pipelineCache,
		1u,
		&pipelineInfo,
		nullptr,
		&render->hizCullingPipeline);
	if (pipelineResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"CreateHizCullingPipeline.vkCreateComputePipelines",
			pipelineResult);
		DestroyHizCullingPipeline(context, render);
		return false;
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->hizCullingPipeline),
		VK_OBJECT_TYPE_PIPELINE,
		"HizCullingPipeline");

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = kHizCullingDescriptorSetCount;
	poolInfo.poolSizeCount = static_cast<uint32_t>(kHizCullingDescriptorPoolSizes.size());
	poolInfo.pPoolSizes = kHizCullingDescriptorPoolSizes.data();

	const VkResult poolResult = vkCreateDescriptorPool(
		context->device,
		&poolInfo,
		nullptr,
		&render->hizCullingDescriptorPool);
	if (poolResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"CreateHizCullingPipeline.vkCreateDescriptorPool",
			poolResult);
		DestroyHizCullingPipeline(context, render);
		return false;
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->hizCullingDescriptorPool),
		VK_OBJECT_TYPE_DESCRIPTOR_POOL,
		"HizCullingDescriptorPool");

	std::array<VkDescriptorSetLayout, kHizCullingDescriptorSetCount> layouts{};
	for (uint32_t i = 0; i < kHizCullingDescriptorSetCount; ++i) {
		layouts[i] = render->hizCullingDescriptorSetLayout;
	}

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = render->hizCullingDescriptorPool;
	allocInfo.descriptorSetCount = kHizCullingDescriptorSetCount;
	allocInfo.pSetLayouts = layouts.data();

	std::array<VkDescriptorSet, kHizCullingDescriptorSetCount> sets{};
	const VkResult allocResult = vkAllocateDescriptorSets(
		context->device,
		&allocInfo,
		sets.data());
	if (allocResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"CreateHizCullingPipeline.vkAllocateDescriptorSets",
			allocResult);
		DestroyHizCullingPipeline(context, render);
		return false;
	}
	for (uint32_t i = 0; i < kHizCullingDescriptorSetCount && i < render->sceneFrameResources.size(); ++i) {
		render->sceneFrameResources[i].hizCullingDescriptorSet = sets[i];
	}

	render->hizCullingEnabled = true;

	// HZB min-reduction compute (Task 4.0). Optional; blit LINEAR path remains if this fails.
	render->hizMinifyEnabled = false;
	render->hizMinifyUsesPushDescriptors = false;
	if (IsHzbMinMipEnabled()) {
		const std::vector<char> minifyCode = ReadShaderFile("hiz_minify.comp.spv");
		if (!minifyCode.empty()) {
			const bool usePushDescriptors = context->features14.pushDescriptor == VK_TRUE;
			std::array minifyBindings = {
				VkDescriptorSetLayoutBinding{
					.binding = 0,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
					.descriptorCount = 1u,
					.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
					.pImmutableSamplers = nullptr},
				VkDescriptorSetLayoutBinding{
					.binding = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
					.descriptorCount = 1u,
					.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
					.pImmutableSamplers = nullptr}};
			VkDescriptorSetLayoutCreateInfo minifyLayoutInfo{};
			minifyLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			minifyLayoutInfo.flags =
				usePushDescriptors ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR : 0u;
			minifyLayoutInfo.bindingCount = static_cast<uint32_t>(minifyBindings.size());
			minifyLayoutInfo.pBindings = minifyBindings.data();
			if (vkCreateDescriptorSetLayout(
					context->device,
					&minifyLayoutInfo,
					nullptr,
					&render->hizMinifyDescriptorSetLayout) == VK_SUCCESS) {
				VkPipelineLayoutCreateInfo minifyPipeLayoutInfo{};
				minifyPipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
				minifyPipeLayoutInfo.setLayoutCount = 1u;
				minifyPipeLayoutInfo.pSetLayouts = &render->hizMinifyDescriptorSetLayout;
				if (vkCreatePipelineLayout(
						context->device,
						&minifyPipeLayoutInfo,
						nullptr,
						&render->hizMinifyPipelineLayout) == VK_SUCCESS) {
					VkShaderModuleCreateInfo minifyModuleInfo{};
					minifyModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
					minifyModuleInfo.codeSize = minifyCode.size();
					minifyModuleInfo.pCode = reinterpret_cast<const uint32_t *>(minifyCode.data());
					if (vkCreateShaderModule(
							context->device,
							&minifyModuleInfo,
							nullptr,
							&render->hizMinifyShaderModule) == VK_SUCCESS) {
						VkComputePipelineCreateInfo minifyPipelineInfo{};
						minifyPipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
						minifyPipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
						minifyPipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
						minifyPipelineInfo.stage.module = render->hizMinifyShaderModule;
						minifyPipelineInfo.stage.pName = "main";
						minifyPipelineInfo.layout = render->hizMinifyPipelineLayout;
						if (vkCreateComputePipelines(
								context->device,
								context->pipelineCache,
								1u,
								&minifyPipelineInfo,
								nullptr,
								&render->hizMinifyPipeline) == VK_SUCCESS) {
							bool descriptorsReady = usePushDescriptors;
							if (!usePushDescriptors) {
								VkDescriptorPoolSize minifyPoolSize{
									.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
									.descriptorCount = 2u};
								VkDescriptorPoolCreateInfo minifyPoolInfo{};
								minifyPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
								minifyPoolInfo.maxSets = 1u;
								minifyPoolInfo.poolSizeCount = 1u;
								minifyPoolInfo.pPoolSizes = &minifyPoolSize;
								if (vkCreateDescriptorPool(
										context->device,
										&minifyPoolInfo,
										nullptr,
										&render->hizMinifyDescriptorPool) == VK_SUCCESS) {
									VkDescriptorSetAllocateInfo minifyAlloc{};
									minifyAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
									minifyAlloc.descriptorPool = render->hizMinifyDescriptorPool;
									minifyAlloc.descriptorSetCount = 1u;
									minifyAlloc.pSetLayouts = &render->hizMinifyDescriptorSetLayout;
									descriptorsReady = vkAllocateDescriptorSets(
														   context->device,
														   &minifyAlloc,
														   &render->hizMinifyDescriptorSet) == VK_SUCCESS;
								}
							}
							if (descriptorsReady) {
								render->hizMinifyUsesPushDescriptors = usePushDescriptors;
								render->hizMinifyEnabled = true;
								SDL_Log(
									"Render: HZB min-reduction mip pipeline ready (pushDescriptors=%d)",
									usePushDescriptors ? 1 : 0);
							}
						}
					}
				}
			}
		}
		if (!render->hizMinifyEnabled) {
			SDL_Log("Render: HZB min-reduction pipeline unavailable; using LINEAR blit mips");
		}
	}

	return true;
}

void DestroyHizCullingPipeline(
	VulkanContextState *context,
	RenderState *render)
{
	if (!context || !render) {
		return;
	}

	for (SceneFrameResources &frameResources : render->sceneFrameResources) {
		frameResources.hizCullingDescriptorSet = VK_NULL_HANDLE;
	}

	if (context->device != VK_NULL_HANDLE) {
		if (render->hizCullingPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(context->device, render->hizCullingPipeline, nullptr);
			render->hizCullingPipeline = VK_NULL_HANDLE;
		}
		if (render->hizCullingPipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(context->device, render->hizCullingPipelineLayout, nullptr);
			render->hizCullingPipelineLayout = VK_NULL_HANDLE;
		}
		if (render->hizCullingDescriptorSetLayout != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(context->device, render->hizCullingDescriptorSetLayout, nullptr);
			render->hizCullingDescriptorSetLayout = VK_NULL_HANDLE;
		}
		if (render->hizCullingShaderModule != VK_NULL_HANDLE) {
			vkDestroyShaderModule(context->device, render->hizCullingShaderModule, nullptr);
			render->hizCullingShaderModule = VK_NULL_HANDLE;
		}
		if (render->hizCullingDescriptorPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(context->device, render->hizCullingDescriptorPool, nullptr);
			render->hizCullingDescriptorPool = VK_NULL_HANDLE;
		}
		if (render->hizMinifyPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(context->device, render->hizMinifyPipeline, nullptr);
			render->hizMinifyPipeline = VK_NULL_HANDLE;
		}
		if (render->hizMinifyPipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(context->device, render->hizMinifyPipelineLayout, nullptr);
			render->hizMinifyPipelineLayout = VK_NULL_HANDLE;
		}
		if (render->hizMinifyDescriptorPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(context->device, render->hizMinifyDescriptorPool, nullptr);
			render->hizMinifyDescriptorPool = VK_NULL_HANDLE;
		}
		render->hizMinifyDescriptorSet = VK_NULL_HANDLE;
		if (render->hizMinifyDescriptorSetLayout != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(context->device, render->hizMinifyDescriptorSetLayout, nullptr);
			render->hizMinifyDescriptorSetLayout = VK_NULL_HANDLE;
		}
		if (render->hizMinifyShaderModule != VK_NULL_HANDLE) {
			vkDestroyShaderModule(context->device, render->hizMinifyShaderModule, nullptr);
			render->hizMinifyShaderModule = VK_NULL_HANDLE;
		}
	}

	render->hizCullingEnabled = false;
	render->hizMinifyEnabled = false;
	render->hizMinifyUsesPushDescriptors = false;
}
} // namespace projectv::render

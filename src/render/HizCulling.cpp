#include "render/HizCulling.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/EnvUtils.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

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
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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

void BuildHizMipChain(
	VkCommandBuffer commandBuffer,
	VkImage depthImage,
	const VkImageLayout depthImageLayout,
	const HizBuffer &hizBuffer)
{
	if (commandBuffer == VK_NULL_HANDLE ||
		depthImage == VK_NULL_HANDLE ||
		hizBuffer.image == VK_NULL_HANDLE) {
		return;
	}

	const uint32_t mipLevels = hizBuffer.mipLevelCount;
	if (mipLevels == 0u) {
		return;
	}

	VkImageMemoryBarrier barriers[2]{};
	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barriers[0].oldLayout = depthImageLayout;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].image = depthImage;
	barriers[0].subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u};

	barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[1].srcAccessMask = 0u;
	barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].image = hizBuffer.image;
	barriers[1].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0u,
		0u,
		nullptr,
		0u,
		nullptr,
		2u,
		barriers);

	VkImageBlit blit{};
	blit.srcSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
	blit.srcOffsets[0] = {0, 0, 0};
	blit.srcOffsets[1] = {
		static_cast<int32_t>(hizBuffer.baseWidth),
		static_cast<int32_t>(hizBuffer.baseHeight),
		1,
	};
	blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
	blit.dstOffsets[0] = {0, 0, 0};
	blit.dstOffsets[1] = {
		static_cast<int32_t>(hizBuffer.baseWidth),
		static_cast<int32_t>(hizBuffer.baseHeight),
		1,
	};
	vkCmdBlitImage(
		commandBuffer,
		depthImage,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		hizBuffer.image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1u,
		&blit,
		VK_FILTER_NEAREST);

	uint32_t srcWidth = hizBuffer.baseWidth;
	uint32_t srcHeight = hizBuffer.baseHeight;
	for (uint32_t mipLevel = 1u; mipLevel < mipLevels; ++mipLevel) {
		const uint32_t dstWidth = std::max(1u, srcWidth >> 1u);
		const uint32_t dstHeight = std::max(1u, srcHeight >> 1u);

		VkImageMemoryBarrier mipBarrier{};
		mipBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		mipBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		mipBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		mipBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		mipBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		mipBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		mipBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		mipBarrier.image = hizBuffer.image;
		mipBarrier.subresourceRange = {
			VK_IMAGE_ASPECT_COLOR_BIT,
			mipLevel - 1u,
			1u,
			0u,
			1u,
		};
		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0u,
			0u,
			nullptr,
			0u,
			nullptr,
			1u,
			&mipBarrier);

		VkImageBlit mipBlit{};
		mipBlit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevel - 1u, 0u, 1u};
		mipBlit.srcOffsets[0] = {0, 0, 0};
		mipBlit.srcOffsets[1] = {
			static_cast<int32_t>(srcWidth),
			static_cast<int32_t>(srcHeight),
			1,
		};
		mipBlit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 0u, 1u};
		mipBlit.dstOffsets[0] = {0, 0, 0};
		mipBlit.dstOffsets[1] = {
			static_cast<int32_t>(dstWidth),
			static_cast<int32_t>(dstHeight),
			1,
		};
		vkCmdBlitImage(
			commandBuffer,
			hizBuffer.image,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			hizBuffer.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1u,
			&mipBlit,
			VK_FILTER_LINEAR);

		srcWidth = dstWidth;
		srcHeight = dstHeight;
	}

	VkImageMemoryBarrier finalBarrier{};
	finalBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	finalBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	finalBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	finalBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	finalBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	finalBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	finalBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	finalBarrier.image = hizBuffer.image;
	finalBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, mipLevels, 0u, 1u};
	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0u,
		0u,
		nullptr,
		0u,
		nullptr,
		1u,
		&finalBarrier);

	VkImageMemoryBarrier restoreDepth{};
	restoreDepth.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	restoreDepth.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	restoreDepth.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	restoreDepth.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	restoreDepth.newLayout = depthImageLayout;
	restoreDepth.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	restoreDepth.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	restoreDepth.image = depthImage;
	restoreDepth.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u};
	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		0u,
		0u,
		nullptr,
		0u,
		nullptr,
		1u,
		&restoreDepth);
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
		VK_NULL_HANDLE,
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
	}

	render->hizCullingEnabled = false;
}

bool RecordHzbCullingDispatch(
	const VkCommandBuffer commandBuffer,
	VulkanContextState *context,
	RenderState &render,
	SceneFrameResources &frameResources,
	const float (&inverseViewProjection)[16],
	const uint32_t chunkDescriptorCount)
{
	if (!IsHzbCullingEnabled()) {
		return false;
	}
	if (commandBuffer == VK_NULL_HANDLE || context == nullptr) {
		return false;
	}
	if (render.hizCullingPipeline == VK_NULL_HANDLE ||
		render.hizCullingPipelineLayout == VK_NULL_HANDLE) {
		return false;
	}
	if (frameResources.hizCullingDescriptorSet == VK_NULL_HANDLE ||
		frameResources.chunkAabbBuffer == VK_NULL_HANDLE ||
		frameResources.visibilityMaskBuffer == VK_NULL_HANDLE ||
		frameResources.hzbVisibleCountBuffer == VK_NULL_HANDLE ||
		frameResources.hzbPerChunkMipBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (render.hizBuffer.imageView == VK_NULL_HANDLE ||
		render.hizBuffer.sampler == VK_NULL_HANDLE) {
		return false;
	}

	const uint32_t visibilityMaskWordCount =
		(chunkDescriptorCount + 31u) / 32u;

	if (visibilityMaskWordCount > 0u) {
		vkCmdFillBuffer(
			commandBuffer,
			frameResources.visibilityMaskBuffer,
			0u,
			static_cast<VkDeviceSize>(visibilityMaskWordCount) * sizeof(uint32_t),
			0u);
	}
	vkCmdFillBuffer(
		commandBuffer,
		frameResources.hzbVisibleCountBuffer,
		0u,
		sizeof(uint32_t),
		0u);

	VkBufferMemoryBarrier2 fillBarriers[2]{};
	fillBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	fillBarriers[0].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	fillBarriers[0].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	fillBarriers[0].dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	fillBarriers[0].dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	fillBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	fillBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	fillBarriers[0].buffer = frameResources.visibilityMaskBuffer;
	fillBarriers[0].offset = 0u;
	fillBarriers[0].size = static_cast<VkDeviceSize>(visibilityMaskWordCount) * sizeof(uint32_t);

	fillBarriers[1] = fillBarriers[0];
	fillBarriers[1].buffer = frameResources.hzbVisibleCountBuffer;
	fillBarriers[1].size = sizeof(uint32_t);

	VkDependencyInfo fillDepInfo{};
	fillDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	fillDepInfo.bufferMemoryBarrierCount = 2u;
	fillDepInfo.pBufferMemoryBarriers = fillBarriers;
	vkCmdPipelineBarrier2(commandBuffer, &fillDepInfo);

	VkDescriptorBufferInfo chunkAabbInfo{};
	chunkAabbInfo.buffer = frameResources.chunkAabbBuffer;
	chunkAabbInfo.offset = 0;
	chunkAabbInfo.range = VK_WHOLE_SIZE;

	VkDescriptorBufferInfo visibilityMaskInfo{};
	visibilityMaskInfo.buffer = frameResources.visibilityMaskBuffer;
	visibilityMaskInfo.offset = 0;
	visibilityMaskInfo.range = VK_WHOLE_SIZE;

	VkDescriptorImageInfo hizImageInfo{};
	hizImageInfo.sampler = VK_NULL_HANDLE;
	hizImageInfo.imageView = render.hizBuffer.imageView;
	hizImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo hizSamplerInfo{};
	hizSamplerInfo.sampler = render.hizBuffer.sampler;
	hizSamplerInfo.imageView = VK_NULL_HANDLE;
	hizSamplerInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkDescriptorBufferInfo visibleCountInfo{};
	visibleCountInfo.buffer = frameResources.hzbVisibleCountBuffer;
	visibleCountInfo.offset = 0;
	visibleCountInfo.range = sizeof(uint32_t);

	VkDescriptorBufferInfo perChunkMipInfo{};
	perChunkMipInfo.buffer = frameResources.hzbPerChunkMipBuffer;
	perChunkMipInfo.offset = 0;
	perChunkMipInfo.range = VK_WHOLE_SIZE;

	std::array<VkWriteDescriptorSet, 6> writes{};
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = frameResources.hizCullingDescriptorSet;
	writes[0].dstBinding = 0;
	writes[0].dstArrayElement = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &chunkAabbInfo;

	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = frameResources.hizCullingDescriptorSet;
	writes[1].dstBinding = 1;
	writes[1].dstArrayElement = 0;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[1].pBufferInfo = &visibilityMaskInfo;

	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = frameResources.hizCullingDescriptorSet;
	writes[2].dstBinding = 2;
	writes[2].dstArrayElement = 0;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	writes[2].pImageInfo = &hizImageInfo;

	writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[3].dstSet = frameResources.hizCullingDescriptorSet;
	writes[3].dstBinding = 3;
	writes[3].dstArrayElement = 0;
	writes[3].descriptorCount = 1;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	writes[3].pImageInfo = &hizSamplerInfo;

	writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[4].dstSet = frameResources.hizCullingDescriptorSet;
	writes[4].dstBinding = 4;
	writes[4].dstArrayElement = 0;
	writes[4].descriptorCount = 1;
	writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[4].pBufferInfo = &visibleCountInfo;

	writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[5].dstSet = frameResources.hizCullingDescriptorSet;
	writes[5].dstBinding = 5;
	writes[5].dstArrayElement = 0;
	writes[5].descriptorCount = 1;
	writes[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[5].pBufferInfo = &perChunkMipInfo;

	vkUpdateDescriptorSets(
		context->device,
		static_cast<uint32_t>(writes.size()),
		writes.data(),
		0u,
		nullptr);

	HizCullingPushConstants pushConstants{};
	for (uint32_t i = 0; i < 16u; ++i) {
		pushConstants.inverseViewProjection[i] = inverseViewProjection[i];
	}
	pushConstants.hizExtentAndMipCount = {
		render.hizBuffer.baseWidth,
		render.hizBuffer.baseHeight,
		chunkDescriptorCount,
		0u,
	};
	pushConstants.depthUnpackParams = {1.0f, 0.0f, 0.0f, 0.0f};

	vkCmdBindPipeline(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		render.hizCullingPipeline);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		render.hizCullingPipelineLayout,
		0u,
		1u,
		&frameResources.hizCullingDescriptorSet,
		0u,
		nullptr);
	vkCmdPushConstants(
		commandBuffer,
		render.hizCullingPipelineLayout,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0u,
		sizeof(HizCullingPushConstants),
		&pushConstants);

	const uint32_t workgroupCount = (chunkDescriptorCount + 63u) / 64u;
	if (workgroupCount > 0u) {
		vkCmdDispatch(commandBuffer, workgroupCount, 1u, 1u);
	}
	return true;
}

uint32_t ComputePerChunkMipLevelCpu(
	const float projectedExtentXTexels,
	const float projectedExtentYTexels,
	const uint32_t maxMipLevel)
{
	const float maxExtent = std::max(projectedExtentXTexels, projectedExtentYTexels);
	if (maxExtent <= 1.0f) {
		return 0u;
	}
	const float logVal = std::log2(maxExtent);
	const int32_t floored = static_cast<int32_t>(logVal);
	if (floored < 0) {
		return 0u;
	}
	const uint32_t capped = static_cast<uint32_t>(floored);
	return std::min(capped, maxMipLevel);
}

uint32_t ComputePerChunkMipLevelsFromAabbs(
	const std::vector<std::array<float, 4>> &chunkCenters,
	const std::vector<std::array<float, 4>> &chunkHalfExtents,
	const std::array<float, 16> &viewProjection,
	const uint32_t baseWidth,
	const uint32_t baseHeight,
	const uint32_t maxMipLevel,
	std::vector<uint32_t> &outMipLevels)
{
	const size_t count = std::min(chunkCenters.size(), chunkHalfExtents.size());
	outMipLevels.assign(count, 0u);
	if (count == 0u) {
		return 0u;
	}
	for (size_t i = 0; i < count; ++i) {
		const float centerX = chunkCenters[i][0];
		const float centerY = chunkCenters[i][1];
		const float centerZ = chunkCenters[i][2];
		const float halfExtent = chunkHalfExtents[i][0];
		float minX = std::numeric_limits<float>::infinity();
		float minY = std::numeric_limits<float>::infinity();
		float maxX = -std::numeric_limits<float>::infinity();
		float maxY = -std::numeric_limits<float>::infinity();
		for (int sx = -1; sx <= 1; sx += 2) {
			for (int sy = -1; sy <= 1; sy += 2) {
				for (int sz = -1; sz <= 1; sz += 2) {
					const float cornerX = centerX + static_cast<float>(sx) * halfExtent;
					const float cornerY = centerY + static_cast<float>(sy) * halfExtent;
					const float cornerZ = centerZ + static_cast<float>(sz) * halfExtent;
					const float clipX = viewProjection[0] * cornerX + viewProjection[4] * cornerY + viewProjection[8] * cornerZ + viewProjection[12];
					const float clipY = viewProjection[1] * cornerX + viewProjection[5] * cornerY + viewProjection[9] * cornerZ + viewProjection[13];
					const float clipW = viewProjection[3] * cornerX + viewProjection[7] * cornerY + viewProjection[11] * cornerZ + viewProjection[15];
					if (clipW <= 0.0001f) {
						outMipLevels[i] = 0u;
						goto next_chunk;
					}
					const float ndcX = clipX / clipW;
					const float ndcY = clipY / clipW;
					const float uvX = ndcX * 0.5f + 0.5f;
					const float uvY = ndcY * 0.5f + 0.5f;
					if (uvX < minX)
						minX = uvX;
					if (uvY < minY)
						minY = uvY;
					if (uvX > maxX)
						maxX = uvX;
					if (uvY > maxY)
						maxY = uvY;
				}
			}
		}
		{
			const float projectedXTexels = (maxX - minX) * static_cast<float>(baseWidth);
			const float projectedYTexels = (maxY - minY) * static_cast<float>(baseHeight);
			outMipLevels[i] = ComputePerChunkMipLevelCpu(
				projectedXTexels,
				projectedYTexels,
				maxMipLevel);
		}
	next_chunk:;
	}
	return static_cast<uint32_t>(count);
}

uint32_t ComputePerChunkMipAndBlendWidthsFromAabbs(
	const std::vector<std::array<float, 4>> &chunkCenters,
	const std::vector<std::array<float, 4>> &chunkHalfExtents,
	const std::array<float, 16> &viewProjection,
	const uint32_t baseWidth,
	const uint32_t baseHeight,
	const uint32_t maxMipLevel,
	const uint32_t maxBlendWidth,
	std::vector<uint32_t> &outMipAndBlendWidths)
{
	const size_t count = std::min(chunkCenters.size(), chunkHalfExtents.size());
	outMipAndBlendWidths.assign(count * 2u, 0u);
	if (count == 0u) {
		return 0u;
	}
	for (size_t i = 0; i < count; ++i) {
		const float centerX = chunkCenters[i][0];
		const float centerY = chunkCenters[i][1];
		const float centerZ = chunkCenters[i][2];
		const float halfExtent = chunkHalfExtents[i][0];
		float minX = std::numeric_limits<float>::infinity();
		float minY = std::numeric_limits<float>::infinity();
		float maxX = -std::numeric_limits<float>::infinity();
		float maxY = -std::numeric_limits<float>::infinity();
		bool skipBlend = false;
		for (int sx = -1; sx <= 1; sx += 2) {
			for (int sy = -1; sy <= 1; sy += 2) {
				for (int sz = -1; sz <= 1; sz += 2) {
					const float cornerX = centerX + static_cast<float>(sx) * halfExtent;
					const float cornerY = centerY + static_cast<float>(sy) * halfExtent;
					const float cornerZ = centerZ + static_cast<float>(sz) * halfExtent;
					const float clipX = viewProjection[0] * cornerX + viewProjection[4] * cornerY + viewProjection[8] * cornerZ + viewProjection[12];
					const float clipY = viewProjection[1] * cornerX + viewProjection[5] * cornerY + viewProjection[9] * cornerZ + viewProjection[13];
					const float clipW = viewProjection[3] * cornerX + viewProjection[7] * cornerY + viewProjection[11] * cornerZ + viewProjection[15];
					if (clipW <= 0.0001f) {
						outMipAndBlendWidths[i * 2u] = 0u;
						outMipAndBlendWidths[i * 2u + 1u] = 0u;
						skipBlend = true;
						goto next_chunk_blend;
					}
					const float ndcX = clipX / clipW;
					const float ndcY = clipY / clipW;
					const float uvX = ndcX * 0.5f + 0.5f;
					const float uvY = ndcY * 0.5f + 0.5f;
					if (uvX < minX)
						minX = uvX;
					if (uvY < minY)
						minY = uvY;
					if (uvX > maxX)
						maxX = uvX;
					if (uvY > maxY)
						maxY = uvY;
				}
			}
		}
		{
			const uint32_t projectedXTexels = static_cast<uint32_t>(std::abs(maxX - minX) * static_cast<float>(baseWidth));
			const uint32_t projectedYTexels = static_cast<uint32_t>(std::abs(maxY - minY) * static_cast<float>(baseHeight));
			const uint32_t mip = ComputePerChunkMipLevelCpu(
				static_cast<float>(projectedXTexels),
				static_cast<float>(projectedYTexels),
				maxMipLevel);
			outMipAndBlendWidths[i * 2u] = mip;
			outMipAndBlendWidths[i * 2u + 1u] = mip == 0u
													? 0u
													: ComputeBlendWidthForChunkMip(
														  projectedXTexels,
														  projectedYTexels,
														  mip,
														  maxBlendWidth);
		}
	next_chunk_blend:;
		(void)skipBlend;
	}
	return static_cast<uint32_t>(count);
}

void WritePerChunkMipAndBlendWidthsToBuffer(
	void *mappedData,
	const uint32_t *mipAndBlendWidths,
	const uint32_t chunkCount)
{
	if (mappedData == nullptr || mipAndBlendWidths == nullptr) {
		return;
	}
	// chunkCount frozen at frame start; consumer (hzb_cull.comp) reads same chunkCount.
	// Layout invariant: each chunk takes exactly 2 uint32 words (mip, blendWidth).
	static_assert(kHizMipAndBlendWidthWordsPerChunk == 2u,
				  "kHizMipAndBlendWidthWordsPerChunk must equal 2 (mip + blendWidth packed)");
	auto *dest = static_cast<uint32_t *>(mappedData);
	for (uint32_t i = 0u; i < chunkCount; ++i) {
		const uint32_t baseIndex = i * kHizMipAndBlendWidthWordsPerChunk;
		dest[baseIndex] = mipAndBlendWidths[baseIndex];
		dest[baseIndex + 1u] = mipAndBlendWidths[baseIndex + 1u];
	}
}

} // namespace projectv::render

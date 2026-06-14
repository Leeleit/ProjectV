#include "render/vulkan/VulkanGraphicsPipeline.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "debug/Profiling.hpp"
#include "render/TaaRenderTargets.hpp"
#include "render/vulkan/VulkanDebug.hpp"
#include "render/vulkan/TaaResolvePipeline.hpp"

#include <array>
#include <cstdio>
#include <vector>

namespace {
constexpr uint32_t kGraphicsDescriptorSetCount = MAX_FRAMES_IN_FLIGHT;
constexpr uint32_t kShadowDescriptorSetCount = MAX_FRAMES_IN_FLIGHT;
constexpr VkDescriptorPoolSize kGraphicsStorageDescriptorPoolSize{
	.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	.descriptorCount = kGraphicsDescriptorSetCount * 5u,
};
// 1.5 anti-flicker: bump the COMBINED_IMAGE_SAMPLER count from
// 1 per frame to 2 per frame — binding 4 (sunShadowMap) and
// binding 6 (layerHistory) both need combined image samplers
// for the voxel pass. With `MAX_FRAMES_IN_FLIGHT = 2` the pool
// now needs `2 * 2 = 4` descriptors. The previous
// `descriptorCount = 1 * kGraphicsDescriptorSetCount = 2` was
// sized for the pre-1.5 single-sampler case and triggered
// validation `VUID-VkDescriptorPool-size-...` on the first
// `vkAllocateDescriptorSets` after binding 6 was added.
constexpr VkDescriptorPoolSize kGraphicsShadowSamplerDescriptorPoolSize{
	.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	.descriptorCount = kGraphicsDescriptorSetCount * 2u,
};
constexpr std::array kGraphicsDescriptorPoolSizes{
	kGraphicsStorageDescriptorPoolSize,
	kGraphicsShadowSamplerDescriptorPoolSize,
};
constexpr std::array kGraphicsDescriptorBindings{
	VkDescriptorSetLayoutBinding{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 2,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 3,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 4,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 5,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		// P0.3 follow-up (final): per-vertex AO was removed (see voxel.vert
		// header for the full rationale). The fragment stage still consumes
		// this storage buffer for the local-light / AOCC / contact-shadow
		// DDA reads, so we keep FRAGMENT_BIT. The vertex shader no longer
		// needs access because the `inAmbientVisibility` interpolator is
		// driven from a constant 1.0.
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	// 1.5 anti-flicker: per-layer temporal history. The voxel pass
	// reads this at the current fragment UV, blends the freshly-
	// computed raw CTSH/AOCC/LOCL with the previous frame's
	// values, and uses the blended values for this frame's lighting.
	// The freshly-computed raw values are then written to
	// `outLayerMask` (Location 2 MRT output) and copied to this
	// history at end of frame. The previous-frame TAA colour
	// history is read-only inside the resolve pass and lives
	// outside the graphics descriptor set, so this binding
	// is the voxel pass's only layer-history access point.
	VkDescriptorSetLayoutBinding{
		.binding = 6,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
};
constexpr VkDescriptorSetLayoutCreateInfo kGraphicsDescriptorSetLayoutInfo{
	.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	.pNext = nullptr,
	.flags = 0,
	.bindingCount = static_cast<uint32_t>(kGraphicsDescriptorBindings.size()),
	.pBindings = kGraphicsDescriptorBindings.data(),
};
constexpr VkDescriptorPoolSize kShadowStorageDescriptorPoolSize{
	.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	.descriptorCount = kShadowDescriptorSetCount * 3u,
};
constexpr std::array kShadowDescriptorPoolSizes{
	kShadowStorageDescriptorPoolSize,
};
constexpr std::array kShadowDescriptorBindings{
	VkDescriptorSetLayoutBinding{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 3,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.pImmutableSamplers = nullptr,
	},
};
constexpr VkDescriptorSetLayoutCreateInfo kShadowDescriptorSetLayoutInfo{
	.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	.pNext = nullptr,
	.flags = 0,
	.bindingCount = static_cast<uint32_t>(kShadowDescriptorBindings.size()),
	.pBindings = kShadowDescriptorBindings.data(),
};
constexpr VkPipelineColorBlendAttachmentState kAlphaBlendAttachmentState{
	.blendEnable = VK_TRUE,
	.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
	.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	.colorBlendOp = VK_BLEND_OP_ADD,
	.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
	.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	.alphaBlendOp = VK_BLEND_OP_ADD,
	.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT,
};

void LogGraphicsPipelineVkFailure(const char *step, const VkResult result)
{
	runtime::LogVkFailure(step, result);
}

void LogGraphicsPipelineTextFailure(const char *step, const char *detail)
{
	runtime::LogRuntimeFailure("Graphics", step, detail);
}

VkShaderModule CreateShaderModule(const VkDevice device, const std::vector<char> &code)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

	VkShaderModule shaderModule = VK_NULL_HANDLE;
	const VkResult shaderModuleResult = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
	if (shaderModuleResult != VK_SUCCESS) {
		LogGraphicsPipelineVkFailure("vkCreateShaderModule", shaderModuleResult);
		return VK_NULL_HANDLE;
	}

	return shaderModule;
}

bool SupportsDepthAttachment(const VkPhysicalDevice physicalDevice, const VkFormat format)
{
	VkFormatProperties props{};
	vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
	return (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}

bool SupportsSampledImage(const VkPhysicalDevice physicalDevice, const VkFormat format)
{
	VkFormatProperties props{};
	vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
	return (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0;
}

VkFormat ChooseDepthFormat(const VkPhysicalDevice physicalDevice)
{
	// Mirrors `ChooseShadowDepthFormat`: the depth image is now also
	// bound as a `COMBINED_IMAGE_SAMPLER` by the TAA resolve pass
	// (binding 2 in `TaaResolvePipeline.cpp`), so the chosen format
	// must support `SAMPLED_IMAGE_BIT` in addition to
	// `DEPTH_STENCIL_ATTACHMENT_BIT`. Rejecting formats that lack
	// sampled-image support fails fast here instead of silently
	// selecting a stencil layout that the shadow path would later
	// reject at `vkCreateImageView` time.
	constexpr std::array candidates{
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
	};
	for (const VkFormat candidate : candidates) {
		if (SupportsDepthAttachment(physicalDevice, candidate) &&
			SupportsSampledImage(physicalDevice, candidate)) {
			return candidate;
		}
	}
	return VK_FORMAT_UNDEFINED;
}

VkFormat ChooseShadowDepthFormat(const VkPhysicalDevice physicalDevice)
{
	constexpr std::array candidates{
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
	};
	for (const VkFormat candidate : candidates) {
		if (SupportsDepthAttachment(physicalDevice, candidate) &&
			SupportsSampledImage(physicalDevice, candidate)) {
			return candidate;
		}
	}
	return VK_FORMAT_UNDEFINED;
}

bool CreateDepthResources(
	VulkanContextState *context,
	const SwapchainState *swapchain,
	RenderState *render)
{
	PV_PROFILE_ZONE_N("CreateDepthResources");
	const VkFormat depthFormat = ChooseDepthFormat(context->physicalDevice);
	if (depthFormat == VK_FORMAT_UNDEFINED) {
		runtime::LogRuntimeFailure(
			"Graphics",
			"CreateDepthResources.ChooseDepthFormat",
			"no supported depth format found");
		return false;
	}

	const VkImageCreateInfo imageInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = depthFormat,
		.extent = {swapchain->extent.width, swapchain->extent.height, 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
				 VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	VmaAllocationInfo allocationResultInfo{};

	const VkResult createDepthImageResult = vmaCreateImage(
		context->allocator,
		&imageInfo,
		&allocationInfo,
		&render->depthImage,
		&render->depthAllocation,
		&allocationResultInfo);
	if (createDepthImageResult != VK_SUCCESS) {
		LogGraphicsPipelineVkFailure("CreateDepthResources.vmaCreateImage", createDepthImageResult);
		return false;
	}
	profiling::RecordAllocation(
		render->depthAllocation,
		allocationResultInfo.size,
		"DepthImageAllocation");

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = render->depthImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = depthFormat;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	const VkResult depthImageViewResult = vkCreateImageView(context->device, &viewInfo, nullptr, &render->depthImageView);
	if (depthImageViewResult != VK_SUCCESS) {
		LogGraphicsPipelineVkFailure("CreateDepthResources.vkCreateImageView", depthImageViewResult);
		profiling::RecordFree(render->depthAllocation, "DepthImageAllocation");
		vmaDestroyImage(context->allocator, render->depthImage, render->depthAllocation);
		render->depthImage = VK_NULL_HANDLE;
		render->depthAllocation = VK_NULL_HANDLE;
		return false;
	}

	render->depthImageNeedsInit = true;
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->depthImage),
		VK_OBJECT_TYPE_IMAGE,
		"DepthImage");
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->depthImageView),
		VK_OBJECT_TYPE_IMAGE_VIEW,
		"DepthImageView");
	return true;
}

bool CreateShadowResources(
	VulkanContextState *context,
	RenderState *render)
{
	PV_PROFILE_ZONE_N("CreateShadowResources");
	const VkFormat shadowDepthFormat = ChooseShadowDepthFormat(context->physicalDevice);
	if (shadowDepthFormat == VK_FORMAT_UNDEFINED) {
		runtime::LogRuntimeFailure(
			"Graphics",
			"CreateShadowResources.ChooseShadowDepthFormat",
			"no supported sampled depth format found");
		return false;
	}

	render->shadowDepthFormat = shadowDepthFormat;

	const VkImageCreateInfo imageInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = shadowDepthFormat,
		.extent = {render->shadowMapExtent.width, render->shadowMapExtent.height, 1},
		.mipLevels = 1,
		.arrayLayers = kSunShadowCascadeCount,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	VmaAllocationInfo allocationResultInfo{};

	const VkResult createShadowImageResult = vmaCreateImage(
		context->allocator,
		&imageInfo,
		&allocationInfo,
		&render->shadowImage,
		&render->shadowAllocation,
		&allocationResultInfo);
	if (createShadowImageResult != VK_SUCCESS) {
		LogGraphicsPipelineVkFailure("CreateShadowResources.vmaCreateImage", createShadowImageResult);
		render->shadowDepthFormat = VK_FORMAT_UNDEFINED;
		return false;
	}
	profiling::RecordAllocation(
		render->shadowAllocation,
		allocationResultInfo.size,
		"ShadowImageAllocation");

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = render->shadowImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	viewInfo.format = shadowDepthFormat;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = kSunShadowCascadeCount;
	const VkResult shadowImageViewResult = vkCreateImageView(context->device, &viewInfo, nullptr, &render->shadowImageView);
	if (shadowImageViewResult != VK_SUCCESS) {
		LogGraphicsPipelineVkFailure("CreateShadowResources.vkCreateImageView", shadowImageViewResult);
		profiling::RecordFree(render->shadowAllocation, "ShadowImageAllocation");
		vmaDestroyImage(context->allocator, render->shadowImage, render->shadowAllocation);
		render->shadowImage = VK_NULL_HANDLE;
		render->shadowAllocation = VK_NULL_HANDLE;
		render->shadowDepthFormat = VK_FORMAT_UNDEFINED;
		return false;
	}

	VkImageViewCreateInfo cascadeViewInfo = viewInfo;
	cascadeViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	cascadeViewInfo.subresourceRange.layerCount = 1;
	for (uint32_t cascadeIndex = 0; cascadeIndex < kSunShadowCascadeCount; ++cascadeIndex) {
		cascadeViewInfo.subresourceRange.baseArrayLayer = cascadeIndex;
		const VkResult cascadeViewResult = vkCreateImageView(
			context->device,
			&cascadeViewInfo,
			nullptr,
			&render->shadowCascadeImageViews[cascadeIndex]);
		if (cascadeViewResult != VK_SUCCESS) {
			LogGraphicsPipelineVkFailure("CreateShadowResources.vkCreateImageView.Cascade", cascadeViewResult);
			for (VkImageView &cascadeImageView : render->shadowCascadeImageViews) {
				if (cascadeImageView != VK_NULL_HANDLE) {
					vkDestroyImageView(context->device, cascadeImageView, nullptr);
					cascadeImageView = VK_NULL_HANDLE;
				}
			}
			vkDestroyImageView(context->device, render->shadowImageView, nullptr);
			render->shadowImageView = VK_NULL_HANDLE;
			profiling::RecordFree(render->shadowAllocation, "ShadowImageAllocation");
			vmaDestroyImage(context->allocator, render->shadowImage, render->shadowAllocation);
			render->shadowImage = VK_NULL_HANDLE;
			render->shadowAllocation = VK_NULL_HANDLE;
			render->shadowDepthFormat = VK_FORMAT_UNDEFINED;
			return false;
		}
	}

	constexpr VkSamplerCreateInfo samplerInfo{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
		.mipLodBias = 0.0f,
		.anisotropyEnable = VK_FALSE,
		.maxAnisotropy = 1.0f,
		.compareEnable = VK_TRUE,
		.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
		.minLod = 0.0f,
		.maxLod = 0.0f,
		.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
		.unnormalizedCoordinates = VK_FALSE,
	};
	const VkResult shadowSamplerResult = vkCreateSampler(context->device, &samplerInfo, nullptr, &render->shadowSampler);
	if (shadowSamplerResult != VK_SUCCESS) {
		LogGraphicsPipelineVkFailure("CreateShadowResources.vkCreateSampler", shadowSamplerResult);
		for (VkImageView &cascadeImageView : render->shadowCascadeImageViews) {
			if (cascadeImageView != VK_NULL_HANDLE) {
				vkDestroyImageView(context->device, cascadeImageView, nullptr);
				cascadeImageView = VK_NULL_HANDLE;
			}
		}
		vkDestroyImageView(context->device, render->shadowImageView, nullptr);
		render->shadowImageView = VK_NULL_HANDLE;
		profiling::RecordFree(render->shadowAllocation, "ShadowImageAllocation");
		vmaDestroyImage(context->allocator, render->shadowImage, render->shadowAllocation);
		render->shadowImage = VK_NULL_HANDLE;
		render->shadowAllocation = VK_NULL_HANDLE;
		render->shadowDepthFormat = VK_FORMAT_UNDEFINED;
		return false;
	}

	render->shadowImageNeedsInit = true;
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->shadowImage),
		VK_OBJECT_TYPE_IMAGE,
		"ShadowImage");
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->shadowImageView),
		VK_OBJECT_TYPE_IMAGE_VIEW,
		"ShadowImageView");
	for (uint32_t cascadeIndex = 0; cascadeIndex < kSunShadowCascadeCount; ++cascadeIndex) {
		char viewName[64]{};
		std::snprintf(viewName, sizeof(viewName), "ShadowCascadeImageView[%u]", cascadeIndex);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(render->shadowCascadeImageViews[cascadeIndex]),
			VK_OBJECT_TYPE_IMAGE_VIEW,
			viewName);
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->shadowSampler),
		VK_OBJECT_TYPE_SAMPLER,
		"ShadowSampler");
	return true;
}

bool CreateScreenshotReadbackResources(
	VulkanContextState *context,
	const SwapchainState *swapchain,
	RenderState *render)
{
	PV_PROFILE_ZONE_N("CreateScreenshotReadbackResources");
	if (!swapchain->supportsTransferSrc) {
		render->screenshotCaptureSupported = false;
		return true;
	}

	const uint64_t requiredSize =
		static_cast<uint64_t>(swapchain->extent.width) *
		static_cast<uint64_t>(swapchain->extent.height) *
		4u;
	if (requiredSize == 0u) {
		render->screenshotCaptureSupported = false;
		return true;
	}

	const VkBufferCreateInfo bufferInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.size = requiredSize,
		.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
	};

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	allocationInfo.flags =
		VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
		VMA_ALLOCATION_CREATE_MAPPED_BIT;
	allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

	VmaAllocationInfo allocationResultInfo{};
	const VkResult createBufferResult = vmaCreateBuffer(
		context->allocator,
		&bufferInfo,
		&allocationInfo,
		&render->screenshotReadbackBuffer,
		&render->screenshotReadbackAllocation,
		&allocationResultInfo);
	if (createBufferResult != VK_SUCCESS) {
		LogGraphicsPipelineVkFailure("CreateScreenshotReadbackResources.vmaCreateBuffer", createBufferResult);
		render->screenshotCaptureSupported = false;
		return false;
	}
	if (!allocationResultInfo.pMappedData) {
		LogGraphicsPipelineTextFailure(
			"CreateScreenshotReadbackResources.MappedData",
			"screenshot readback allocation is not mapped");
		vmaDestroyBuffer(context->allocator, render->screenshotReadbackBuffer, render->screenshotReadbackAllocation);
		render->screenshotReadbackBuffer = VK_NULL_HANDLE;
		render->screenshotReadbackAllocation = VK_NULL_HANDLE;
		render->screenshotCaptureSupported = false;
		return false;
	}

	render->screenshotReadbackMappedData = allocationResultInfo.pMappedData;
	render->screenshotReadbackBufferSize = allocationResultInfo.size;
	render->screenshotCaptureSupported = true;
	profiling::RecordAllocation(
		render->screenshotReadbackAllocation,
		allocationResultInfo.size,
		"ScreenshotReadbackAllocation");
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->screenshotReadbackBuffer),
		VK_OBJECT_TYPE_BUFFER,
		"ScreenshotReadbackBuffer");
	return true;
}

void DestroyGraphicsResourceBindings(
	VulkanContextState &context,
	RenderState &render)
{
	for (SceneFrameResources &frameResources : render.sceneFrameResources) {
		frameResources.graphicsDescriptorSet = VK_NULL_HANDLE;
		frameResources.shadowDescriptorSet = VK_NULL_HANDLE;
	}

	if (render.graphicsDescriptorPool) {
		vkDestroyDescriptorPool(context.device, render.graphicsDescriptorPool, nullptr);
		render.graphicsDescriptorPool = VK_NULL_HANDLE;
	}

	if (render.shadowDescriptorPool) {
		vkDestroyDescriptorPool(context.device, render.shadowDescriptorPool, nullptr);
		render.shadowDescriptorPool = VK_NULL_HANDLE;
	}

	if (render.graphicsDescriptorSetLayout) {
		vkDestroyDescriptorSetLayout(context.device, render.graphicsDescriptorSetLayout, nullptr);
		render.graphicsDescriptorSetLayout = VK_NULL_HANDLE;
	}

	if (render.shadowDescriptorSetLayout) {
		vkDestroyDescriptorSetLayout(context.device, render.shadowDescriptorSetLayout, nullptr);
		render.shadowDescriptorSetLayout = VK_NULL_HANDLE;
	}
}

void DestroyDebugOverlayPipeline(
	VulkanContextState &context,
	RenderState &render)
{
	if (render.debugCrosshairPipeline) {
		vkDestroyPipeline(context.device, render.debugCrosshairPipeline, nullptr);
		render.debugCrosshairPipeline = VK_NULL_HANDLE;
	}

	if (render.debugOverlayPipeline) {
		vkDestroyPipeline(context.device, render.debugOverlayPipeline, nullptr);
		render.debugOverlayPipeline = VK_NULL_HANDLE;
	}

	if (render.debugOverlayPipelineLayout) {
		vkDestroyPipelineLayout(context.device, render.debugOverlayPipelineLayout, nullptr);
		render.debugOverlayPipelineLayout = VK_NULL_HANDLE;
	}
}

bool CreateDebugOverlayPipeline(
	VulkanContextState &context,
	const SwapchainState &swapchain,
	RenderState &render)
{
	std::vector<char> vertexShaderCode = ReadShaderFile("debug_overlay.vert.spv");
	std::vector<char> fragmentShaderCode = ReadShaderFile("debug_overlay.frag.spv");
	if (vertexShaderCode.empty() || fragmentShaderCode.empty()) {
		LogGraphicsPipelineTextFailure("CreateDebugOverlayPipeline.ReadFile", "debug overlay shader blob is empty");
		return false;
	}

	VkShaderModule vertexShaderModule = CreateShaderModule(context.device, vertexShaderCode);
	VkShaderModule fragmentShaderModule = CreateShaderModule(context.device, fragmentShaderCode);
	if (!vertexShaderModule || !fragmentShaderModule) {
		LogGraphicsPipelineTextFailure(
			"CreateDebugOverlayPipeline.CreateShaderModule",
			"debug overlay shader module creation returned null");
		if (vertexShaderModule) {
			vkDestroyShaderModule(context.device, vertexShaderModule, nullptr);
		}
		if (fragmentShaderModule) {
			vkDestroyShaderModule(context.device, fragmentShaderModule, nullptr);
		}
		return false;
	}

	const std::array shaderStages{
		VkPipelineShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertexShaderModule,
			.pName = "main",
			.pSpecializationInfo = nullptr,
		},
		VkPipelineShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragmentShaderModule,
			.pName = "main",
			.pSpecializationInfo = nullptr,
		},
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

	constexpr std::array dynamicStates{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	constexpr VkPipelineMultisampleStateCreateInfo multisampling{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 0.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE,
	};

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &kAlphaBlendAttachmentState;

	constexpr VkPipelineColorBlendAttachmentState kCrosshairLogicOpAttachmentState{
		.blendEnable = VK_FALSE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT,
	};
	VkPipelineInputAssemblyStateCreateInfo crosshairInputAssembly = inputAssembly;
	crosshairInputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPhysicalDeviceFeatures supportedFeatures{};
	vkGetPhysicalDeviceFeatures(context.physicalDevice, &supportedFeatures);
	const bool useLogicOpCrosshair = supportedFeatures.logicOp == VK_TRUE;
	VkPipelineColorBlendStateCreateInfo crosshairColorBlending = colorBlending;
	crosshairColorBlending.logicOpEnable = useLogicOpCrosshair ? VK_TRUE : VK_FALSE;
	crosshairColorBlending.logicOp = VK_LOGIC_OP_XOR;
	crosshairColorBlending.pAttachments =
		useLogicOpCrosshair ? &kCrosshairLogicOpAttachmentState : &kAlphaBlendAttachmentState;

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(DebugOverlayPushConstants);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
	const VkResult debugOverlayLayoutResult =
		vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &render.debugOverlayPipelineLayout);
	if (debugOverlayLayoutResult != VK_SUCCESS) {
		LogGraphicsPipelineVkFailure("CreateDebugOverlayPipeline.vkCreatePipelineLayout", debugOverlayLayoutResult);
		vkDestroyShaderModule(context.device, vertexShaderModule, nullptr);
		vkDestroyShaderModule(context.device, fragmentShaderModule, nullptr);
		return false;
	}

	const VkPipelineRenderingCreateInfo renderingInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.pNext = nullptr,
		.viewMask = 0,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &swapchain.format,
		.depthAttachmentFormat = ChooseDepthFormat(context.physicalDevice),
		.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
	};

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = &renderingInfo;
	pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = render.debugOverlayPipelineLayout;

	const VkResult debugOverlayPipelineResult = vkCreateGraphicsPipelines(
		context.device,
		VK_NULL_HANDLE,
		1,
		&pipelineInfo,
		nullptr,
		&render.debugOverlayPipeline);
	if (debugOverlayPipelineResult != VK_SUCCESS) {
		vkDestroyShaderModule(context.device, vertexShaderModule, nullptr);
		vkDestroyShaderModule(context.device, fragmentShaderModule, nullptr);
		LogGraphicsPipelineVkFailure("CreateDebugOverlayPipeline.vkCreateGraphicsPipelines", debugOverlayPipelineResult);
		DestroyDebugOverlayPipeline(context, render);
		return false;
	}

	pipelineInfo.pInputAssemblyState = &crosshairInputAssembly;
	pipelineInfo.pColorBlendState = &crosshairColorBlending;
	const VkResult debugCrosshairPipelineResult = vkCreateGraphicsPipelines(
		context.device,
		VK_NULL_HANDLE,
		1,
		&pipelineInfo,
		nullptr,
		&render.debugCrosshairPipeline);
	vkDestroyShaderModule(context.device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(context.device, fragmentShaderModule, nullptr);
	if (debugCrosshairPipelineResult != VK_SUCCESS) {
		LogGraphicsPipelineVkFailure(
			"CreateDebugOverlayPipeline.vkCreateGraphicsPipelines.Crosshair",
			debugCrosshairPipelineResult);
		DestroyDebugOverlayPipeline(context, render);
		return false;
	}

	SetVulkanObjectName(
		context,
		reinterpret_cast<uint64_t>(render.debugOverlayPipelineLayout),
		VK_OBJECT_TYPE_PIPELINE_LAYOUT,
		"DebugOverlayPipelineLayout");
	SetVulkanObjectName(
		context,
		reinterpret_cast<uint64_t>(render.debugOverlayPipeline),
		VK_OBJECT_TYPE_PIPELINE,
		"DebugOverlayPipeline");
	SetVulkanObjectName(
		context,
		reinterpret_cast<uint64_t>(render.debugCrosshairPipeline),
		VK_OBJECT_TYPE_PIPELINE,
		"DebugCrosshairPipeline");
	return true;
}

void DestroyDebugHudPipeline(
	VulkanContextState &context,
	RenderState &render)
{
	if (render.debugHudPipeline) {
		vkDestroyPipeline(context.device, render.debugHudPipeline, nullptr);
		render.debugHudPipeline = VK_NULL_HANDLE;
	}

	if (render.debugHudPipelineLayout) {
		vkDestroyPipelineLayout(context.device, render.debugHudPipelineLayout, nullptr);
		render.debugHudPipelineLayout = VK_NULL_HANDLE;
	}
}

bool CreateDebugHudPipeline(
	VulkanContextState &context,
	const SwapchainState &swapchain,
	RenderState &render)
{
	std::vector<char> vertexShaderCode = ReadShaderFile("debug_hud.vert.spv");
	std::vector<char> fragmentShaderCode = ReadShaderFile("debug_hud.frag.spv");
	if (vertexShaderCode.empty() || fragmentShaderCode.empty()) {
		LogGraphicsPipelineTextFailure("CreateDebugHudPipeline.ReadFile", "debug HUD shader blob is empty");
		return false;
	}

	VkShaderModule vertexShaderModule = CreateShaderModule(context.device, vertexShaderCode);
	VkShaderModule fragmentShaderModule = CreateShaderModule(context.device, fragmentShaderCode);
	if (!vertexShaderModule || !fragmentShaderModule) {
		LogGraphicsPipelineTextFailure(
			"CreateDebugHudPipeline.CreateShaderModule",
			"debug HUD shader module creation returned null");
		if (vertexShaderModule) {
			vkDestroyShaderModule(context.device, vertexShaderModule, nullptr);
		}
		if (fragmentShaderModule) {
			vkDestroyShaderModule(context.device, fragmentShaderModule, nullptr);
		}
		return false;
	}

	const std::array shaderStages{
		VkPipelineShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertexShaderModule,
			.pName = "main",
			.pSpecializationInfo = nullptr,
		},
		VkPipelineShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragmentShaderModule,
			.pName = "main",
			.pSpecializationInfo = nullptr,
		},
	};

	constexpr VkVertexInputBindingDescription vertexBindingDescription{
		.binding = 0,
		.stride = sizeof(DebugHudVertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};
	constexpr std::array vertexAttributeDescriptions{
		VkVertexInputAttributeDescription{
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32G32_SFLOAT,
			.offset = offsetof(DebugHudVertex, positionNdc),
		},
		VkVertexInputAttributeDescription{
			.location = 1,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32A32_SFLOAT,
			.offset = offsetof(DebugHudVertex, color),
		},
	};
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions.size());
	vertexInputInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	constexpr std::array dynamicStates{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	constexpr VkPipelineMultisampleStateCreateInfo multisampling{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 0.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE,
	};

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &kAlphaBlendAttachmentState;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	const VkResult debugHudLayoutResult =
		vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &render.debugHudPipelineLayout);
	if (debugHudLayoutResult != VK_SUCCESS) {
		LogGraphicsPipelineVkFailure("CreateDebugHudPipeline.vkCreatePipelineLayout", debugHudLayoutResult);
		vkDestroyShaderModule(context.device, vertexShaderModule, nullptr);
		vkDestroyShaderModule(context.device, fragmentShaderModule, nullptr);
		return false;
	}

	const VkPipelineRenderingCreateInfo renderingInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.pNext = nullptr,
		.viewMask = 0,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &swapchain.format,
		.depthAttachmentFormat = ChooseDepthFormat(context.physicalDevice),
		.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
	};

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = &renderingInfo;
	pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = render.debugHudPipelineLayout;

	const VkResult debugHudPipelineResult = vkCreateGraphicsPipelines(
		context.device,
		VK_NULL_HANDLE,
		1,
		&pipelineInfo,
		nullptr,
		&render.debugHudPipeline);
	vkDestroyShaderModule(context.device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(context.device, fragmentShaderModule, nullptr);
	if (debugHudPipelineResult != VK_SUCCESS) {
		LogGraphicsPipelineVkFailure("CreateDebugHudPipeline.vkCreateGraphicsPipelines", debugHudPipelineResult);
		DestroyDebugHudPipeline(context, render);
		return false;
	}

	SetVulkanObjectName(
		context,
		reinterpret_cast<uint64_t>(render.debugHudPipelineLayout),
		VK_OBJECT_TYPE_PIPELINE_LAYOUT,
		"DebugHudPipelineLayout");
	SetVulkanObjectName(
		context,
		reinterpret_cast<uint64_t>(render.debugHudPipeline),
		VK_OBJECT_TYPE_PIPELINE,
		"DebugHudPipeline");
	return true;
}
} // namespace

bool RefreshGraphicsResourceBindings(
	VulkanContextState *context,
	RenderState *render)
{
	PV_PROFILE_ZONE_N("RefreshGraphicsResourceBindings");
	PV_CHECK_OR_RETURN(
		context && render && context->device,
		"Graphics",
		"RefreshGraphicsResourceBindings.Preconditions",
		"context/render/device is incomplete");
	if (!render->graphicsDescriptorSetLayout) {
		return true;
	}

	if (render->graphicsDescriptorPool) {
		vkDestroyDescriptorPool(context->device, render->graphicsDescriptorPool, nullptr);
		render->graphicsDescriptorPool = VK_NULL_HANDLE;
	}

	constexpr VkDescriptorPoolCreateInfo poolInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.maxSets = kGraphicsDescriptorSetCount,
		.poolSizeCount = static_cast<uint32_t>(kGraphicsDescriptorPoolSizes.size()),
		.pPoolSizes = kGraphicsDescriptorPoolSizes.data(),
	};
	const VkResult descriptorPoolResult =
		vkCreateDescriptorPool(context->device, &poolInfo, nullptr, &render->graphicsDescriptorPool);
	if (descriptorPoolResult != VK_SUCCESS) {
		LogGraphicsPipelineVkFailure("RefreshGraphicsResourceBindings.vkCreateDescriptorPool", descriptorPoolResult);
		return false;
	}

	const std::vector setLayouts(render->sceneFrameResources.size(), render->graphicsDescriptorSetLayout);
	std::vector<VkDescriptorSet> descriptorSets(render->sceneFrameResources.size(), VK_NULL_HANDLE);
	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = render->graphicsDescriptorPool;
	allocateInfo.descriptorSetCount = static_cast<uint32_t>(setLayouts.size());
	allocateInfo.pSetLayouts = setLayouts.data();
	const VkResult allocateDescriptorSetsResult =
		vkAllocateDescriptorSets(context->device, &allocateInfo, 		descriptorSets.data());
	if (allocateDescriptorSetsResult != VK_SUCCESS) {
		LogGraphicsPipelineVkFailure(
			"RefreshGraphicsResourceBindings.vkAllocateDescriptorSets",
			allocateDescriptorSetsResult);
		vkDestroyDescriptorPool(context->device, render->graphicsDescriptorPool, nullptr);
		render->graphicsDescriptorPool = VK_NULL_HANDLE;
		return false;
	}

	for (size_t frameIndex = 0; frameIndex < render->sceneFrameResources.size(); ++frameIndex) {
		SceneFrameResources &frameResources = render->sceneFrameResources[frameIndex];
		frameResources.graphicsDescriptorSet = descriptorSets[frameIndex];

		const VkDescriptorBufferInfo packedFaceBufferInfo{
			.buffer = frameResources.packedFaceBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo chunkDescriptorBufferInfo{
			.buffer = frameResources.chunkDescriptorBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo materialVisualBufferInfo{
			.buffer = render->materialVisualBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo chunkVoxelPayloadBufferInfo{
			.buffer = frameResources.chunkVoxelPayloadBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo sceneLightingBufferInfo{
			.buffer = frameResources.sceneLightingBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorImageInfo shadowImageInfo{
			.sampler = render->shadowSampler,
			.imageView = render->shadowImageView,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
		};
		// 1.5 anti-flicker: layer history. Uses the same linear
		// sampler as the colour history (the resolve pass and
		// other consumers also share this sampler). The voxel
		// pass uses the texture's texel size from
		// `VoxelSceneLighting.taaLayerHistoryParams.xy` to step
		// the read UV, not the sampler's filtering mode, so
		// `LINEAR` / `NEAREST` is a non-issue for the layer
		// history read; we keep the project's shared sampler
		// for consistency. `SHADER_READ_ONLY_OPTIMAL` matches
		// the layout the layer history is in after the per-
		// frame `vkCmdCopyImage` (history copy block in
		// `Renderer.cpp`).
		const VkDescriptorImageInfo layerHistoryImageInfo{
			.sampler = render->taaLinearSampler,
			.imageView = render->taaLayerHistoryColorTarget != nullptr
							 ? render->taaLayerHistoryColorTarget->imageView
							 : VK_NULL_HANDLE,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};
		const std::array descriptorWrites{
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.graphicsDescriptorSet,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &packedFaceBufferInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.graphicsDescriptorSet,
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &chunkDescriptorBufferInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.graphicsDescriptorSet,
				.dstBinding = 2,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &materialVisualBufferInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.graphicsDescriptorSet,
				.dstBinding = 3,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &sceneLightingBufferInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.graphicsDescriptorSet,
				.dstBinding = 4,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &shadowImageInfo,
				.pBufferInfo = nullptr,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.graphicsDescriptorSet,
				.dstBinding = 5,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &chunkVoxelPayloadBufferInfo,
				.pTexelBufferView = nullptr,
			},
			// 1.5 anti-flicker: per-layer temporal history (binding 6).
			// See `kGraphicsDescriptorBindings` for the contract.
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.graphicsDescriptorSet,
				.dstBinding = 6,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &layerHistoryImageInfo,
				.pBufferInfo = nullptr,
				.pTexelBufferView = nullptr,
			},
		};
		vkUpdateDescriptorSets(
			context->device,
			static_cast<uint32_t>(descriptorWrites.size()),
			descriptorWrites.data(),
			0,
			nullptr);

		// **TAA resolve descriptor update (`2026-06-13`).** The
		// TAA resolve pass has its own per-frame descriptor
		// set (`render->taaResolveDescriptorSets[frameIndex]`,
		// allocated inside `CreateTaaResolvePipeline` and
		// not recreated by `RefreshGraphicsResourceBindings`).
		// Without this patch, the TAA resolve set's
		// binding 3 (sceneLighting) keeps the *old*
		// `sceneLightingBuffer` handle across a scene
		// preset switch (F5) and the next `vkCmdDraw`
		// issued by the TAA resolve pass triggers
		// `VUID-vkCmdDraw-None-08114`
		// ("the storage buffer descriptor
		// sceneLighting is using buffer ... that is
		// invalid or has been destroyed"). The
		// validation error references the *TAA resolve*
		// descriptor set, not the graphics set, which
		// is why the F5 validation persists even after
		// `RefreshGraphicsResourceBindings` correctly
		// rewrites the graphics set. Mirroring the
		// `sceneLightingBufferInfo` write into the TAA
		// resolve set's binding 3 here closes the race.
		if (render->taaResolveDescriptorSets[frameIndex] != VK_NULL_HANDLE) {
			const VkWriteDescriptorSet taaResolveWrite{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = render->taaResolveDescriptorSets[frameIndex],
				.dstBinding = 3,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &sceneLightingBufferInfo,
				.pTexelBufferView = nullptr,
			};
			vkUpdateDescriptorSets(
				context->device,
				1,
				&taaResolveWrite,
				0,
				nullptr);
		}
	}

	if (!render->shadowDescriptorSetLayout) {
		return true;
	}

	if (render->shadowDescriptorPool) {
		vkDestroyDescriptorPool(context->device, render->shadowDescriptorPool, nullptr);
		render->shadowDescriptorPool = VK_NULL_HANDLE;
	}

	constexpr VkDescriptorPoolCreateInfo shadowPoolInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.maxSets = kShadowDescriptorSetCount,
		.poolSizeCount = static_cast<uint32_t>(kShadowDescriptorPoolSizes.size()),
		.pPoolSizes = kShadowDescriptorPoolSizes.data(),
	};
	const VkResult shadowDescriptorPoolResult =
		vkCreateDescriptorPool(context->device, &shadowPoolInfo, nullptr, &render->shadowDescriptorPool);
	if (shadowDescriptorPoolResult != VK_SUCCESS) {
		LogGraphicsPipelineVkFailure("RefreshGraphicsResourceBindings.vkCreateShadowDescriptorPool", shadowDescriptorPoolResult);
		return false;
	}

	const std::vector shadowSetLayouts(render->sceneFrameResources.size(), render->shadowDescriptorSetLayout);
	std::vector<VkDescriptorSet> shadowDescriptorSets(render->sceneFrameResources.size(), VK_NULL_HANDLE);
	VkDescriptorSetAllocateInfo shadowAllocateInfo{};
	shadowAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	shadowAllocateInfo.descriptorPool = render->shadowDescriptorPool;
	shadowAllocateInfo.descriptorSetCount = static_cast<uint32_t>(shadowSetLayouts.size());
	shadowAllocateInfo.pSetLayouts = shadowSetLayouts.data();
	const VkResult allocateShadowDescriptorSetsResult =
		vkAllocateDescriptorSets(context->device, &shadowAllocateInfo, shadowDescriptorSets.data());
	if (allocateShadowDescriptorSetsResult != VK_SUCCESS) {
		LogGraphicsPipelineVkFailure(
			"RefreshGraphicsResourceBindings.vkAllocateShadowDescriptorSets",
			allocateShadowDescriptorSetsResult);
		vkDestroyDescriptorPool(context->device, render->shadowDescriptorPool, nullptr);
		render->shadowDescriptorPool = VK_NULL_HANDLE;
		return false;
	}

	for (size_t frameIndex = 0; frameIndex < render->sceneFrameResources.size(); ++frameIndex) {
		SceneFrameResources &frameResources = render->sceneFrameResources[frameIndex];
		frameResources.shadowDescriptorSet = shadowDescriptorSets[frameIndex];

		const VkDescriptorBufferInfo packedFaceBufferInfo{
			.buffer = frameResources.packedFaceBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo chunkDescriptorBufferInfo{
			.buffer = frameResources.chunkDescriptorBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo sceneLightingBufferInfo{
			.buffer = frameResources.sceneLightingBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const std::array shadowDescriptorWrites{
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.shadowDescriptorSet,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &packedFaceBufferInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.shadowDescriptorSet,
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &chunkDescriptorBufferInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.shadowDescriptorSet,
				.dstBinding = 3,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &sceneLightingBufferInfo,
				.pTexelBufferView = nullptr,
			},
		};
		vkUpdateDescriptorSets(
			context->device,
			static_cast<uint32_t>(shadowDescriptorWrites.size()),
			shadowDescriptorWrites.data(),
			0,
			nullptr);
	}

	return true;
}

void DestroyGraphicsPipeline(
	VulkanContextState *context,
	RenderState *render)
{
	PV_PROFILE_ZONE_N("DestroyGraphicsPipeline");
	if (!context || !render || !context->device) {
		return;
	}

	DestroyGraphicsResourceBindings(*context, *render);

	if (render->transparentGraphicsPipeline) {
		PV_PROFILE_ZONE_N("DestroyTransparentGraphicsPipeline");
		vkDestroyPipeline(context->device, render->transparentGraphicsPipeline, nullptr);
		render->transparentGraphicsPipeline = VK_NULL_HANDLE;
	}
	if (render->transparentGraphicsPipelineTaaOn) {
		PV_PROFILE_ZONE_N("DestroyTransparentGraphicsPipelineTaaOn");
		vkDestroyPipeline(context->device, render->transparentGraphicsPipelineTaaOn, nullptr);
		render->transparentGraphicsPipelineTaaOn = VK_NULL_HANDLE;
	}

	if (render->shadowGraphicsPipeline) {
		PV_PROFILE_ZONE_N("DestroyShadowGraphicsPipeline");
		vkDestroyPipeline(context->device, render->shadowGraphicsPipeline, nullptr);
		render->shadowGraphicsPipeline = VK_NULL_HANDLE;
	}

	DestroyDebugOverlayPipeline(*context, *render);
	DestroyDebugHudPipeline(*context, *render);
	DestroyTaaResolvePipeline(context, render);

	if (render->graphicsPipeline) {
		PV_PROFILE_ZONE_N("DestroyOpaqueGraphicsPipeline");
		vkDestroyPipeline(context->device, render->graphicsPipeline, nullptr);
		render->graphicsPipeline = VK_NULL_HANDLE;
	}
	if (render->graphicsPipelineTaaOn) {
		PV_PROFILE_ZONE_N("DestroyOpaqueGraphicsPipelineTaaOn");
		vkDestroyPipeline(context->device, render->graphicsPipelineTaaOn, nullptr);
		render->graphicsPipelineTaaOn = VK_NULL_HANDLE;
	}

	if (render->graphicsPipelineLayout) {
		PV_PROFILE_ZONE_N("DestroyGraphicsPipelineLayout");
		vkDestroyPipelineLayout(context->device, render->graphicsPipelineLayout, nullptr);
		render->graphicsPipelineLayout = VK_NULL_HANDLE;
	}

	if (render->shadowPipelineLayout) {
		PV_PROFILE_ZONE_N("DestroyShadowPipelineLayout");
		vkDestroyPipelineLayout(context->device, render->shadowPipelineLayout, nullptr);
		render->shadowPipelineLayout = VK_NULL_HANDLE;
	}

	if (render->depthImageView) {
		PV_PROFILE_ZONE_N("DestroyDepthImageView");
		vkDestroyImageView(context->device, render->depthImageView, nullptr);
		render->depthImageView = VK_NULL_HANDLE;
	}

	if (render->shadowSampler) {
		PV_PROFILE_ZONE_N("DestroyShadowSampler");
		vkDestroySampler(context->device, render->shadowSampler, nullptr);
		render->shadowSampler = VK_NULL_HANDLE;
	}

	for (VkImageView &cascadeImageView : render->shadowCascadeImageViews) {
		if (cascadeImageView) {
			PV_PROFILE_ZONE_N("DestroyShadowCascadeImageView");
			vkDestroyImageView(context->device, cascadeImageView, nullptr);
			cascadeImageView = VK_NULL_HANDLE;
		}
	}

	if (render->shadowImageView) {
		PV_PROFILE_ZONE_N("DestroyShadowImageView");
		vkDestroyImageView(context->device, render->shadowImageView, nullptr);
		render->shadowImageView = VK_NULL_HANDLE;
	}

	if (render->shadowImage && render->shadowAllocation) {
		PV_PROFILE_ZONE_N("DestroyShadowImage");
		profiling::RecordFree(render->shadowAllocation, "ShadowImageAllocation");
		vmaDestroyImage(context->allocator, render->shadowImage, render->shadowAllocation);
		render->shadowImage = VK_NULL_HANDLE;
		render->shadowAllocation = VK_NULL_HANDLE;
	}

	if (render->depthImage && render->depthAllocation) {
		PV_PROFILE_ZONE_N("DestroyDepthImage");
		profiling::RecordFree(render->depthAllocation, "DepthImageAllocation");
		vmaDestroyImage(context->allocator, render->depthImage, render->depthAllocation);
		render->depthImage = VK_NULL_HANDLE;
		render->depthAllocation = VK_NULL_HANDLE;
	}
	if (render->screenshotReadbackBuffer && render->screenshotReadbackAllocation) {
		PV_PROFILE_ZONE_N("DestroyScreenshotReadbackBuffer");
		profiling::RecordFree(render->screenshotReadbackAllocation, "ScreenshotReadbackAllocation");
		vmaDestroyBuffer(
			context->allocator,
			render->screenshotReadbackBuffer,
			render->screenshotReadbackAllocation);
		render->screenshotReadbackBuffer = VK_NULL_HANDLE;
		render->screenshotReadbackAllocation = VK_NULL_HANDLE;
	}

	render->depthImageNeedsInit = false;
	render->shadowImageNeedsInit = false;
	render->shadowDepthFormat = VK_FORMAT_UNDEFINED;
	render->screenshotReadbackMappedData = nullptr;
	render->screenshotReadbackBufferSize = 0;
	render->screenshotCaptureSupported = false;
}

bool CreateGraphicsPipeline(
	VulkanContextState *context,
	const SwapchainState *swapchain,
	RenderState *render)
{
	PV_PROFILE_ZONE_N("CreateGraphicsPipeline");
	PV_CHECK_OR_RETURN(
		context && swapchain && render && context->device && !swapchain->imageViews.empty(),
		"Graphics",
		"CreateGraphicsPipeline.Preconditions",
		"context/swapchain/render is incomplete");

	{
		PV_PROFILE_ZONE_N("CreateGraphicsPipeline.DepthResources");
		if (!CreateDepthResources(context, swapchain, render)) {
			LogGraphicsPipelineTextFailure("CreateGraphicsPipeline.DepthResources", "depth resource creation failed");
			return false;
		}
		if (!CreateShadowResources(context, render)) {
			LogGraphicsPipelineTextFailure("CreateGraphicsPipeline.ShadowResources", "shadow resource creation failed");
			DestroyGraphicsPipeline(context, render);
			return false;
		}
		if (!CreateScreenshotReadbackResources(context, swapchain, render)) {
			LogGraphicsPipelineTextFailure(
				"CreateGraphicsPipeline.ScreenshotResources",
				"screenshot readback buffer creation failed");
		}
	}

	std::vector<char> vertexShaderCode;
	std::vector<char> fragmentShaderCode;
	std::vector<char> fragmentShaderCodeTaaOn;
	std::vector<char> shadowVertexShaderCode;
	std::vector<char> shadowFragmentShaderCode;
	{
		PV_PROFILE_ZONE_N("CreateGraphicsPipeline.ReadShaders");
		vertexShaderCode = ReadShaderFile("voxel.vert.spv");
		fragmentShaderCode = ReadShaderFile("voxel.frag.spv");
		fragmentShaderCodeTaaOn = ReadShaderFile("voxel.frag.taa_on.spv");
		shadowVertexShaderCode = ReadShaderFile("voxel_shadow.vert.spv");
		shadowFragmentShaderCode = ReadShaderFile("voxel_shadow.frag.spv");
	}
	if (vertexShaderCode.empty() || fragmentShaderCode.empty() ||
		fragmentShaderCodeTaaOn.empty() ||
		shadowVertexShaderCode.empty() || shadowFragmentShaderCode.empty()) {
		LogGraphicsPipelineTextFailure("CreateGraphicsPipeline.ReadShaders", "voxel shader blob is empty");
		DestroyGraphicsPipeline(context, render);
		return false;
	}

	VkShaderModule vertexShaderModule = VK_NULL_HANDLE;
	VkShaderModule fragmentShaderModule = VK_NULL_HANDLE;
	VkShaderModule fragmentShaderModuleTaaOn = VK_NULL_HANDLE;
	VkShaderModule shadowVertexShaderModule = VK_NULL_HANDLE;
	VkShaderModule shadowFragmentShaderModule = VK_NULL_HANDLE;
	const auto destroyShaderModules = [&] {
		if (shadowFragmentShaderModule) {
			vkDestroyShaderModule(context->device, shadowFragmentShaderModule, nullptr);
			shadowFragmentShaderModule = VK_NULL_HANDLE;
		}
		if (shadowVertexShaderModule) {
			vkDestroyShaderModule(context->device, shadowVertexShaderModule, nullptr);
			shadowVertexShaderModule = VK_NULL_HANDLE;
		}
		if (fragmentShaderModuleTaaOn) {
			vkDestroyShaderModule(context->device, fragmentShaderModuleTaaOn, nullptr);
			fragmentShaderModuleTaaOn = VK_NULL_HANDLE;
		}
		if (fragmentShaderModule) {
			vkDestroyShaderModule(context->device, fragmentShaderModule, nullptr);
			fragmentShaderModule = VK_NULL_HANDLE;
		}
		if (vertexShaderModule) {
			vkDestroyShaderModule(context->device, vertexShaderModule, nullptr);
			vertexShaderModule = VK_NULL_HANDLE;
		}
	};
	{
		PV_PROFILE_ZONE_N("CreateGraphicsPipeline.CreateShaderModules");
		vertexShaderModule = CreateShaderModule(context->device, vertexShaderCode);
		fragmentShaderModule = CreateShaderModule(context->device, fragmentShaderCode);
		fragmentShaderModuleTaaOn = CreateShaderModule(context->device, fragmentShaderCodeTaaOn);
		shadowVertexShaderModule = CreateShaderModule(context->device, shadowVertexShaderCode);
		shadowFragmentShaderModule = CreateShaderModule(context->device, shadowFragmentShaderCode);
	}
	if (!vertexShaderModule || !fragmentShaderModule || !fragmentShaderModuleTaaOn ||
		!shadowVertexShaderModule || !shadowFragmentShaderModule) {
		LogGraphicsPipelineTextFailure(
			"CreateGraphicsPipeline.CreateShaderModules",
			"voxel shader module creation returned null");
		destroyShaderModules();
		DestroyGraphicsPipeline(context, render);
		return false;
	}

	const VkPipelineShaderStageCreateInfo vertexStageInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = vertexShaderModule,
		.pName = "main",
		.pSpecializationInfo = nullptr,
	};
	const VkPipelineShaderStageCreateInfo fragStageTaaOff{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = fragmentShaderModule,
		.pName = "main",
		.pSpecializationInfo = nullptr,
	};
	const VkPipelineShaderStageCreateInfo fragStageTaaOn{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = fragmentShaderModuleTaaOn,
		.pName = "main",
		.pSpecializationInfo = nullptr,
	};
	const std::array shaderStagesTaaOff{vertexStageInfo, fragStageTaaOff};
	const std::array shaderStagesTaaOn{vertexStageInfo, fragStageTaaOn};
	const std::array shadowShaderStages{
		VkPipelineShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = shadowVertexShaderModule,
			.pName = "main",
			.pSpecializationInfo = nullptr,
		},
		VkPipelineShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = shadowFragmentShaderModule,
			.pName = "main",
			.pSpecializationInfo = nullptr,
		},
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	constexpr std::array dynamicStates{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	// **Fluid (2026-06-13 follow-up):** disable back-face culling for
	// the main pass so water voxels show all 6 faces (the
	// back faces are normally culled by `VK_CULL_MODE_BACK_BIT`,
	// but for water we want the user to see every face even
	// when looking at a single voxel). The operator reported
	// "у воды вроде бы одной грани куба нет" — the water
	// seems to be missing one face of a cube. Disabling
	// back-face culling ensures the back face is also rendered
	// (the depth test still hides faces occluded by closer
	// geometry, so the visual is not "double-faced" — just
	// complete). Other voxels (Opaque, Glass) are unaffected
	// because their back faces are still behind the front
	// faces and culled by the depth test.
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	VkPipelineRasterizationStateCreateInfo shadowRasterizer = rasterizer;
	// Shadow camera looks from the light toward the scene, so a face whose normal points
	// away from the light is a *back* face from the main pass's view, but a *front* face
	// from the shadow pass's view. With VK_CULL_MODE_BACK_BIT inherited from the main
	// rasterizer, the shadow pass culls faces that it actually needs to write, which
	// produces a checkerboard of holes in the shadow map and shows up as stair-stepped
	// patches in the receiver pass. Disable face culling for the shadow pass.
	shadowRasterizer.cullMode = VK_CULL_MODE_NONE;
	shadowRasterizer.depthBiasEnable = VK_TRUE;
	shadowRasterizer.depthBiasConstantFactor = 1.25f;
	shadowRasterizer.depthBiasSlopeFactor = 1.75f;

	constexpr VkPipelineMultisampleStateCreateInfo multisampling{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 0.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE,
	};

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	// The third slot of the dual-format pipeline is unused at the
	// shader level in the TAA-on variant (TAA-on writes to Location
	// 1 only, the per-frame `VkRenderingAttachmentInfo` for slot 2
	// can be `VK_NULL_HANDLE`), but the pipeline still has to
	// declare all 3 slots so VUID-VkGraphicsPipelineCreateInfo-renderPass-06055
	// (`pColorBlendState->attachmentCount == colorAttachmentCount`)
	// is satisfied and `dynamicRenderingUnusedAttachments` lets
	// the per-frame `imageView` go null on any unused slot.
	// VUID-VkPipelineColorBlendStateCreateInfo-pAttachments-00605
	// requires all entries to be identical when `independentBlend`
	// is not enabled, so all three slots point at the same blend
	// state. The third (1.5 layer history) slot uses a write mask
	// of `0xF` even though the layer mask only carries meaningful
	// data in the RGB channels; writing to alpha (the reserved 1.0
	// channel) is a no-op since the value is constant.
	VkPipelineColorBlendAttachmentState colorBlendAttachments[3] = {
		colorBlendAttachment,
		colorBlendAttachment,
		colorBlendAttachment,
	};
	VkPipelineColorBlendAttachmentState transparentColorBlendAttachment = kAlphaBlendAttachmentState;
	VkPipelineColorBlendAttachmentState transparentColorBlendAttachments[3] = {
		transparentColorBlendAttachment,
		transparentColorBlendAttachment,
		transparentColorBlendAttachment,
	};

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.attachmentCount = 3;
	colorBlending.pAttachments = colorBlendAttachments;

	VkPipelineColorBlendStateCreateInfo transparentColorBlending = colorBlending;
	transparentColorBlending.pAttachments = transparentColorBlendAttachments;
	VkPipelineColorBlendStateCreateInfo shadowColorBlending{};
	shadowColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	shadowColorBlending.attachmentCount = 0;
	shadowColorBlending.pAttachments = nullptr;

	VkPipelineDepthStencilStateCreateInfo transparentDepthStencil = depthStencil;
	transparentDepthStencil.depthWriteEnable = VK_FALSE;
	VkPipelineDepthStencilStateCreateInfo shadowDepthStencil = depthStencil;
	shadowDepthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(GraphicsPushConstants);

	{
		PV_PROFILE_ZONE_N("CreateGraphicsPipeline.DescriptorSetLayout");
		const VkResult descriptorSetLayoutResult = vkCreateDescriptorSetLayout(
			context->device,
			&kGraphicsDescriptorSetLayoutInfo,
			nullptr,
			&render->graphicsDescriptorSetLayout);
		if (descriptorSetLayoutResult != VK_SUCCESS) {
			LogGraphicsPipelineVkFailure(
				"CreateGraphicsPipeline.vkCreateDescriptorSetLayout",
				descriptorSetLayoutResult);
			destroyShaderModules();
			DestroyGraphicsPipeline(context, render);
			return false;
		}
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->graphicsDescriptorSetLayout),
		VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
		"VoxelGraphicsDescriptorSetLayout");
	{
		PV_PROFILE_ZONE_N("CreateGraphicsPipeline.ShadowDescriptorSetLayout");
		const VkResult shadowDescriptorSetLayoutResult = vkCreateDescriptorSetLayout(
			context->device,
			&kShadowDescriptorSetLayoutInfo,
			nullptr,
			&render->shadowDescriptorSetLayout);
		if (shadowDescriptorSetLayoutResult != VK_SUCCESS) {
			LogGraphicsPipelineVkFailure(
				"CreateGraphicsPipeline.vkCreateShadowDescriptorSetLayout",
				shadowDescriptorSetLayoutResult);
			destroyShaderModules();
			DestroyGraphicsPipeline(context, render);
			return false;
		}
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->shadowDescriptorSetLayout),
		VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
		"VoxelShadowDescriptorSetLayout");

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &render->graphicsDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	{
		PV_PROFILE_ZONE_N("CreateGraphicsPipeline.PipelineLayout");
		const VkResult pipelineLayoutResult =
			vkCreatePipelineLayout(context->device, &pipelineLayoutInfo, nullptr, &render->graphicsPipelineLayout);
		if (pipelineLayoutResult != VK_SUCCESS) {
			LogGraphicsPipelineVkFailure("CreateGraphicsPipeline.vkCreatePipelineLayout", pipelineLayoutResult);
			destroyShaderModules();
			DestroyGraphicsPipeline(context, render);
			return false;
		}
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->graphicsPipelineLayout),
		VK_OBJECT_TYPE_PIPELINE_LAYOUT,
		"VoxelGraphicsPipelineLayout");
	VkPipelineLayoutCreateInfo shadowPipelineLayoutInfo{};
	shadowPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	shadowPipelineLayoutInfo.setLayoutCount = 1;
	shadowPipelineLayoutInfo.pSetLayouts = &render->shadowDescriptorSetLayout;
	VkPushConstantRange shadowPushConstantRange{};
	shadowPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	shadowPushConstantRange.offset = 0;
	shadowPushConstantRange.size = sizeof(ShadowPushConstants);
	shadowPipelineLayoutInfo.pushConstantRangeCount = 1;
	shadowPipelineLayoutInfo.pPushConstantRanges = &shadowPushConstantRange;

	{
		PV_PROFILE_ZONE_N("CreateGraphicsPipeline.ShadowPipelineLayout");
		const VkResult shadowPipelineLayoutResult =
			vkCreatePipelineLayout(context->device, &shadowPipelineLayoutInfo, nullptr, &render->shadowPipelineLayout);
		if (shadowPipelineLayoutResult != VK_SUCCESS) {
			LogGraphicsPipelineVkFailure(
				"CreateGraphicsPipeline.vkCreateShadowPipelineLayout",
				shadowPipelineLayoutResult);
			destroyShaderModules();
			DestroyGraphicsPipeline(context, render);
			return false;
		}
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->shadowPipelineLayout),
		VK_OBJECT_TYPE_PIPELINE_LAYOUT,
		"VoxelShadowPipelineLayout");

	// The main voxel pipeline declares two color attachment formats so the
	// same pipeline can drive both the TAA-on path (render to slot 1 =
	// TAA offscreen target, slot 0 = NULL) and the TAA-off path (render
	// to slot 0 = swapchain, slot 1 = NULL). The TAA offscreen format
	// is consumed from the same `kTaaSceneColorFormat` constant that
	// the image allocator in `TaaRenderTargets.cpp` uses, so the
	// pipeline declaration and the actual `VkImage` format cannot
	// drift. The ability to bind with `imageView = VK_NULL_HANDLE` on
	// the unused slot requires the `dynamicRenderingUnusedAttachments`
	// feature, enabled by `VK_EXT_dynamic_rendering_unused_attachments`
	// in `VulkanBootstrap.cpp::InitializeVulkanBase`. Without that
	// feature the pipeline declaration itself is still valid (formats
	// can be `VK_FORMAT_UNDEFINED` for unused slots) but the per-frame
	// `VkRenderingAttachmentInfo::imageView` would have to match, which
	// would force a second pipeline variant. We fail fast on devices
	// that lack the feature so the failure mode is a clear init error
	// rather than a runtime VUID every frame.
	if (!context->supportsDynamicRenderingUnusedAttachments) {
		LogGraphicsPipelineTextFailure(
			"CreateGraphicsPipeline.DynamicRenderingUnusedAttachments",
			"device does not support VK_EXT_dynamic_rendering_unused_attachments; "
			"main voxel pipeline requires it for the dual-format TAA contract");
		return false;
	}
	const VkFormat mainColorAttachmentFormats[3] = {
		swapchain->format,
		projectv::taa::kTaaSceneColorFormat,
		// 1.5 anti-flicker: per-layer temporal mask (R8G8B8A8_UNORM,
		// 4 B/pixel). The voxel pass writes the raw per-layer values
		// (CTSH, AOCC, LOCL) here; the next frame samples them as
		// `sampler2D layerHistory` (binding 6) for temporal blending.
		// `dynamicRenderingUnusedAttachments` allows the per-frame
		// `VkRenderingAttachmentInfo::imageView` to be
		// `VK_NULL_HANDLE` for slots the current variant doesn't
		// actually write to (TAA-on skips slot 0, TAA-off skips
		// slot 1), so the same pipeline declaration works for
		// both shader variants.
		projectv::taa::kTaaLayerHistoryColorFormat,
	};
	const VkPipelineRenderingCreateInfo renderingInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.pNext = nullptr,
		.viewMask = 0,
		.colorAttachmentCount = 3,
		.pColorAttachmentFormats = mainColorAttachmentFormats,
		.depthAttachmentFormat = ChooseDepthFormat(context->physicalDevice),
		.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
	};
	const VkPipelineRenderingCreateInfo shadowRenderingInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.pNext = nullptr,
		.viewMask = 0,
		.colorAttachmentCount = 0,
		.pColorAttachmentFormats = nullptr,
		.depthAttachmentFormat = render->shadowDepthFormat,
		.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
	};

	VkGraphicsPipelineCreateInfo pipelineBase{};
	pipelineBase.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineBase.pNext = &renderingInfo;
	pipelineBase.pVertexInputState = &vertexInputInfo;
	pipelineBase.pInputAssemblyState = &inputAssembly;
	pipelineBase.pViewportState = &viewportState;
	pipelineBase.pRasterizationState = &rasterizer;
	pipelineBase.pMultisampleState = &multisampling;
	pipelineBase.pDepthStencilState = &depthStencil;
	pipelineBase.pColorBlendState = &colorBlending;
	pipelineBase.pDynamicState = &dynamicState;
	pipelineBase.layout = render->graphicsPipelineLayout;

	VkGraphicsPipelineCreateInfo opaqueInfoTaaOff = pipelineBase;
	opaqueInfoTaaOff.stageCount = static_cast<uint32_t>(shaderStagesTaaOff.size());
	opaqueInfoTaaOff.pStages = shaderStagesTaaOff.data();
	VkGraphicsPipelineCreateInfo opaqueInfoTaaOn = pipelineBase;
	opaqueInfoTaaOn.stageCount = static_cast<uint32_t>(shaderStagesTaaOn.size());
	opaqueInfoTaaOn.pStages = shaderStagesTaaOn.data();
	const VkGraphicsPipelineCreateInfo opaquePipelineInfos[2] = {opaqueInfoTaaOff, opaqueInfoTaaOn};
	VkPipeline opaquePipelines[2]{};
	{
		PV_PROFILE_ZONE_N("CreateGraphicsPipeline.OpaquePipeline");
		const VkResult opaquePipelinesResult = vkCreateGraphicsPipelines(
			context->device,
			VK_NULL_HANDLE,
			2,
			opaquePipelineInfos,
			nullptr,
			opaquePipelines);
		if (opaquePipelinesResult != VK_SUCCESS) {
			LogGraphicsPipelineVkFailure("CreateGraphicsPipeline.Opaque.vkCreateGraphicsPipelines", opaquePipelinesResult);
			destroyShaderModules();
			DestroyGraphicsPipeline(context, render);
			return false;
		}
	}
	render->graphicsPipeline = opaquePipelines[0];
	render->graphicsPipelineTaaOn = opaquePipelines[1];
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->graphicsPipeline),
		VK_OBJECT_TYPE_PIPELINE,
		"VoxelOpaquePipeline");
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->graphicsPipelineTaaOn),
		VK_OBJECT_TYPE_PIPELINE,
		"VoxelOpaquePipelineTaaOn");

	VkGraphicsPipelineCreateInfo transparentInfoTaaOff = pipelineBase;
	transparentInfoTaaOff.pDepthStencilState = &transparentDepthStencil;
	transparentInfoTaaOff.pColorBlendState = &transparentColorBlending;
	transparentInfoTaaOff.stageCount = static_cast<uint32_t>(shaderStagesTaaOff.size());
	transparentInfoTaaOff.pStages = shaderStagesTaaOff.data();
	VkGraphicsPipelineCreateInfo transparentInfoTaaOn = pipelineBase;
	transparentInfoTaaOn.pDepthStencilState = &transparentDepthStencil;
	transparentInfoTaaOn.pColorBlendState = &transparentColorBlending;
	transparentInfoTaaOn.stageCount = static_cast<uint32_t>(shaderStagesTaaOn.size());
	transparentInfoTaaOn.pStages = shaderStagesTaaOn.data();
	const VkGraphicsPipelineCreateInfo transparentPipelineInfos[2] = {transparentInfoTaaOff, transparentInfoTaaOn};
	VkPipeline transparentPipelines[2]{};
	{
		PV_PROFILE_ZONE_N("CreateGraphicsPipeline.TransparentPipeline");
		const VkResult transparentPipelinesResult = vkCreateGraphicsPipelines(
			context->device,
			VK_NULL_HANDLE,
			2,
			transparentPipelineInfos,
			nullptr,
			transparentPipelines);
		if (transparentPipelinesResult != VK_SUCCESS) {
			LogGraphicsPipelineVkFailure(
				"CreateGraphicsPipeline.Transparent.vkCreateGraphicsPipelines",
				transparentPipelinesResult);
			destroyShaderModules();
			DestroyGraphicsPipeline(context, render);
			return false;
		}
	}
	render->transparentGraphicsPipeline = transparentPipelines[0];
	render->transparentGraphicsPipelineTaaOn = transparentPipelines[1];
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->transparentGraphicsPipeline),
		VK_OBJECT_TYPE_PIPELINE,
		"VoxelTransparentPipeline");
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->transparentGraphicsPipelineTaaOn),
		VK_OBJECT_TYPE_PIPELINE,
		"VoxelTransparentPipelineTaaOn");

	VkGraphicsPipelineCreateInfo shadowPipelineInfo = pipelineBase;
	shadowPipelineInfo.pNext = &shadowRenderingInfo;
	shadowPipelineInfo.stageCount = static_cast<uint32_t>(shadowShaderStages.size());
	shadowPipelineInfo.pStages = shadowShaderStages.data();
	shadowPipelineInfo.pRasterizationState = &shadowRasterizer;
	shadowPipelineInfo.pDepthStencilState = &shadowDepthStencil;
	shadowPipelineInfo.pColorBlendState = &shadowColorBlending;
	shadowPipelineInfo.layout = render->shadowPipelineLayout;
	{
		PV_PROFILE_ZONE_N("CreateGraphicsPipeline.ShadowPipeline");
		const VkResult shadowPipelineResult = vkCreateGraphicsPipelines(
			context->device,
			VK_NULL_HANDLE,
			1,
			&shadowPipelineInfo,
			nullptr,
			&render->shadowGraphicsPipeline);
		if (shadowPipelineResult != VK_SUCCESS) {
			LogGraphicsPipelineVkFailure(
				"CreateGraphicsPipeline.Shadow.vkCreateGraphicsPipelines",
				shadowPipelineResult);
			destroyShaderModules();
			DestroyGraphicsPipeline(context, render);
			return false;
		}
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->shadowGraphicsPipeline),
		VK_OBJECT_TYPE_PIPELINE,
		"VoxelShadowPipeline");

	if (!RefreshGraphicsResourceBindings(context, render)) {
		LogGraphicsPipelineTextFailure(
			"CreateGraphicsPipeline.RefreshGraphicsResourceBindings",
			"graphics descriptor rebinding failed");
		destroyShaderModules();
		DestroyGraphicsPipeline(context, render);
		return false;
	}

	if (!CreateDebugOverlayPipeline(*context, *swapchain, *render)) {
		LogGraphicsPipelineTextFailure("CreateGraphicsPipeline.DebugOverlay", "debug overlay pipeline creation failed");
		destroyShaderModules();
		DestroyGraphicsPipeline(context, render);
		return false;
	}

	if (!CreateDebugHudPipeline(*context, *swapchain, *render)) {
		LogGraphicsPipelineTextFailure("CreateGraphicsPipeline.DebugHud", "debug HUD pipeline creation failed");
		destroyShaderModules();
		DestroyGraphicsPipeline(context, render);
		return false;
	}

	if (!CreateTaaResolvePipeline(context, swapchain, render)) {
		LogGraphicsPipelineTextFailure(
			"CreateGraphicsPipeline.TaaResolve",
			"TAA resolve pipeline creation failed");
		destroyShaderModules();
		DestroyGraphicsPipeline(context, render);
		return false;
	}

	destroyShaderModules();
	return true;
}

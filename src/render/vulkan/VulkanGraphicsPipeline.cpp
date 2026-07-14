#include "render/vulkan/VulkanGraphicsPipeline.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
#include "render/vulkan/VulkanGraphicsPipelineInternal.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"
#include "render/AaPass.hpp"
#include "render/AntialiasingSettings.hpp"
#include "render/RayTracedShadows.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <array>
#include <vector>

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
	render->depthFormat = depthFormat;
	projectv::render::ResolveMsaaSampleCount(context, render);
	const VkExtent2D depthExtent = render->internalRenderExtent.width != 0u
									   ? render->internalRenderExtent
									   : projectv::render::ComputeInternalRenderExtent(swapchain->extent, render->renderScaleMode);
	render->internalRenderExtent = depthExtent;

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = depthFormat;
	imageInfo.extent = {depthExtent.width, depthExtent.height, 1};
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = projectv::render::ToVkSampleCount(render->msaaSampleCount);
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

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
	(void)context;
	(void)render;
	// CSM removed per TODO.md §5.2.D (session 20x). RTX shadows are the
	// canonical sun shadow path; no shadow image / sampler / image-view
	// stack is created.
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
	if (render->transparentDebugGraphicsPipeline) {
		PV_PROFILE_ZONE_N("DestroyTransparentDebugGraphicsPipeline");
		vkDestroyPipeline(context->device, render->transparentDebugGraphicsPipeline, nullptr);
		render->transparentDebugGraphicsPipeline = VK_NULL_HANDLE;
	}

	if (render->shadowGraphicsPipeline) {
		PV_PROFILE_ZONE_N("DestroyShadowGraphicsPipeline");
		vkDestroyPipeline(context->device, render->shadowGraphicsPipeline, nullptr);
		render->shadowGraphicsPipeline = VK_NULL_HANDLE;
	}

	DestroyDebugOverlayPipeline(*context, *render);

	if (render->graphicsPipeline) {
		PV_PROFILE_ZONE_N("DestroyOpaqueGraphicsPipeline");
		vkDestroyPipeline(context->device, render->graphicsPipeline, nullptr);
		render->graphicsPipeline = VK_NULL_HANDLE;
	}
	if (render->graphicsPipelineRtx) {
		PV_PROFILE_ZONE_N("DestroyOpaqueGraphicsPipelineRtx");
		vkDestroyPipeline(context->device, render->graphicsPipelineRtx, nullptr);
		render->graphicsPipelineRtx = VK_NULL_HANDLE;
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
}

void DestroyDepthResources(
	VulkanContextState *context,
	RenderState *render)
{
	PV_PROFILE_ZONE_N("DestroyDepthResources");
	if (!context || !render) {
		return;
	}

	if (render->depthImageView) {
		PV_PROFILE_ZONE_N("DestroyDepthImageView");
		vkDestroyImageView(context->device, render->depthImageView, nullptr);
		render->depthImageView = VK_NULL_HANDLE;
	}

	if (render->depthImage && render->depthAllocation) {
		PV_PROFILE_ZONE_N("DestroyDepthImage");
		profiling::RecordFree(render->depthAllocation, "DepthImageAllocation");
		vmaDestroyImage(context->allocator, render->depthImage, render->depthAllocation);
		render->depthImage = VK_NULL_HANDLE;
		render->depthAllocation = VK_NULL_HANDLE;
	}

	render->depthImageNeedsInit = false;
}

void DestroyShadowResources(
	VulkanContextState *context,
	RenderState *render)
{
	(void)context;
	(void)render;
	// CSM removed per TODO.md §5.2.D (session 20x). RTX shadows use the
	// graphics descriptor set (binding 13 = rtxTlas); no shadow image /
	// sampler / image-view stack to destroy.
}

void DestroyScreenshotReadbackResources(
	VulkanContextState *context,
	RenderState *render)
{
	PV_PROFILE_ZONE_N("DestroyScreenshotReadbackResources");
	if (!context || !render) {
		return;
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

	render->screenshotReadbackMappedData = nullptr;
	render->screenshotReadbackBufferSize = 0;
	render->screenshotCaptureSupported = false;
}

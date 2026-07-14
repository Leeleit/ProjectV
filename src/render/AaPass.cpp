#include "render/AaPass.hpp"

#include "asset/ModelPass.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "render/AntialiasingSettings.hpp"
#include "render/Cloudscape.hpp"
#include "render/RendererInternal.hpp"
#include "render/SkyAtmosphere.hpp"
#include "render/vulkan/VulkanDebug.hpp"
#include "render/vulkan/VulkanGraphicsPipeline.hpp"
#include "render/vulkan/VulkanMeshShaderPipeline.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace projectv::render {
namespace {

constexpr uint32_t kAaDescriptorSetCount = 8u;
constexpr uint32_t kComputeGroupSize = 16u;

void AaMemoryBarrier(const VkCommandBuffer commandBuffer)
{
	VkMemoryBarrier2 barrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
	barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
	barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
	VkDependencyInfo dependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
	dependencyInfo.memoryBarrierCount = 1u;
	dependencyInfo.pMemoryBarriers = &barrier;
	vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

void DestroyImage(
	VulkanContextState &context,
	VkImage &image,
	VkImageView &view,
	VmaAllocation &allocation)
{
	if (view != VK_NULL_HANDLE) {
		vkDestroyImageView(context.device, view, nullptr);
		view = VK_NULL_HANDLE;
	}
	if (image != VK_NULL_HANDLE) {
		vmaDestroyImage(context.allocator, image, allocation);
		image = VK_NULL_HANDLE;
		allocation = nullptr;
	}
}

bool CreateImage2D(
	VulkanContextState &context,
	const VkFormat format,
	const VkExtent2D extent,
	const VkSampleCountFlagBits samples,
	const VkImageUsageFlags usage,
	const VkImageAspectFlags aspect,
	VkImage &image,
	VkImageView &view,
	VmaAllocation &allocation,
	const char *const imageName,
	const char *const viewName)
{
	VkImageCreateInfo imageInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = format;
	imageInfo.extent = {extent.width, extent.height, 1u};
	imageInfo.mipLevels = 1u;
	imageInfo.arrayLayers = 1u;
	imageInfo.samples = samples;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	const VkResult imageResult = vmaCreateImage(
		context.allocator, &imageInfo, &allocationInfo, &image, &allocation, nullptr);
	if (imageResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateAaImage2D.vmaCreateImage", imageResult);
		return false;
	}

	VkImageViewCreateInfo viewInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange = {aspect, 0u, 1u, 0u, 1u};
	const VkResult viewResult = vkCreateImageView(context.device, &viewInfo, nullptr, &view);
	if (viewResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateAaImage2D.vkCreateImageView", viewResult);
		vmaDestroyImage(context.allocator, image, allocation);
		image = VK_NULL_HANDLE;
		allocation = nullptr;
		return false;
	}
	SetVulkanObjectName(context, reinterpret_cast<uint64_t>(image), VK_OBJECT_TYPE_IMAGE, imageName);
	SetVulkanObjectName(context, reinterpret_cast<uint64_t>(view), VK_OBJECT_TYPE_IMAGE_VIEW, viewName);
	return true;
}

bool CreateComputePipeline(
	VulkanContextState &context,
	const VkPipelineLayout layout,
	const char *const shaderFilename,
	VkShaderModule &shaderModule,
	VkPipeline &pipeline)
{
	const std::vector<char> shaderCode = ReadShaderFile(shaderFilename);
	if (shaderCode.empty()) {
		runtime::LogRuntimeFailure("AaPass", "CreateComputePipeline.ReadShaderFile", shaderFilename);
		return false;
	}
	VkShaderModuleCreateInfo moduleInfo{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
	moduleInfo.codeSize = shaderCode.size();
	moduleInfo.pCode = reinterpret_cast<const uint32_t *>(shaderCode.data());
	const VkResult moduleResult = vkCreateShaderModule(context.device, &moduleInfo, nullptr, &shaderModule);
	if (moduleResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateComputePipeline.vkCreateShaderModule", moduleResult);
		return false;
	}

	VkComputePipelineCreateInfo pipelineInfo{.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
	pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	pipelineInfo.stage.module = shaderModule;
	pipelineInfo.stage.pName = "main";
	pipelineInfo.layout = layout;
	const VkResult pipelineResult = vkCreateComputePipelines(
		context.device, context.pipelineCache, 1u, &pipelineInfo, nullptr, &pipeline);
	if (pipelineResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateComputePipeline.vkCreateComputePipelines", pipelineResult);
		vkDestroyShaderModule(context.device, shaderModule, nullptr);
		shaderModule = VK_NULL_HANDLE;
		return false;
	}
	SetVulkanObjectName(context, reinterpret_cast<uint64_t>(pipeline), VK_OBJECT_TYPE_PIPELINE, shaderFilename);
	return true;
}

VkDescriptorSet AllocateAaSet(
	VulkanContextState &context,
	RenderState &render,
	const VkDescriptorSetLayout layout,
	const uint32_t frameIndex)
{
	const uint32_t poolIndex = frameIndex % MAX_FRAMES_IN_FLIGHT;
	VkDescriptorPool pool = render.aaFrameDescriptorPools[poolIndex];
	if (pool == VK_NULL_HANDLE) {
		pool = render.aaDescriptorPool;
	}
	VkDescriptorSetAllocateInfo allocateInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	allocateInfo.descriptorPool = pool;
	allocateInfo.descriptorSetCount = 1u;
	allocateInfo.pSetLayouts = &layout;
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	if (vkAllocateDescriptorSets(context.device, &allocateInfo, &descriptorSet) != VK_SUCCESS) {
		runtime::LogRuntimeFailure("AaPass", "AllocateAaSet.vkAllocateDescriptorSets", "descriptor pool exhausted");
	}
	return descriptorSet;
}

void WriteSampledStorage(
	VulkanContextState &context,
	const VkDescriptorSet descriptorSet,
	const VkSampler sampler,
	const VkImageView sampledView,
	const VkImageLayout sampledLayout,
	const VkImageView storageView,
	const VkImageLayout storageLayout)
{
	std::array imageInfos{
		VkDescriptorImageInfo{sampler, sampledView, sampledLayout},
		VkDescriptorImageInfo{VK_NULL_HANDLE, storageView, storageLayout},
	};
	std::array writes{
		VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptorSet, .dstBinding = 0u, .descriptorCount = 1u, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &imageInfos[0]},
		VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptorSet, .dstBinding = 1u, .descriptorCount = 1u, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &imageInfos[1]},
	};
	vkUpdateDescriptorSets(context.device, static_cast<uint32_t>(writes.size()), writes.data(), 0u, nullptr);
}

void WriteTwoSampledOneStorage(
	VulkanContextState &context,
	const VkDescriptorSet descriptorSet,
	const VkSampler sampler,
	const VkImageView sampledFirstView,
	const VkImageLayout sampledFirstLayout,
	const VkImageView sampledSecondView,
	const VkImageLayout sampledSecondLayout,
	const VkImageView storageView,
	const VkImageLayout storageLayout)
{
	std::array imageInfos{
		VkDescriptorImageInfo{sampler, sampledFirstView, sampledFirstLayout},
		VkDescriptorImageInfo{sampler, sampledSecondView, sampledSecondLayout},
		VkDescriptorImageInfo{VK_NULL_HANDLE, storageView, storageLayout},
	};
	std::array writes{
		VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptorSet, .dstBinding = 0u, .descriptorCount = 1u, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &imageInfos[0]},
		VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptorSet, .dstBinding = 1u, .descriptorCount = 1u, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &imageInfos[1]},
		VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptorSet, .dstBinding = 2u, .descriptorCount = 1u, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &imageInfos[2]},
	};
	vkUpdateDescriptorSets(context.device, static_cast<uint32_t>(writes.size()), writes.data(), 0u, nullptr);
}

void DispatchSimple(
	const VkCommandBuffer commandBuffer,
	const VkPipeline pipeline,
	const VkPipelineLayout layout,
	const VkDescriptorSet descriptorSet,
	const VkExtent2D extent,
	const AaTonemapPushConstants &pushConstants)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0u, 1u, &descriptorSet, 0u, nullptr);
	vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(pushConstants), &pushConstants);
	vkCmdDispatch(commandBuffer, (extent.width + kComputeGroupSize - 1u) / kComputeGroupSize, (extent.height + kComputeGroupSize - 1u) / kComputeGroupSize, 1u);
}

void TransitionForComputeWrite(
	const VkCommandBuffer commandBuffer,
	const VkImage image,
	VkImageLayout &layout)
{
	::TransitionImage(
		commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT, layout, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
	layout = VK_IMAGE_LAYOUT_GENERAL;
}

void TransitionForComputeRead(
	const VkCommandBuffer commandBuffer,
	const VkImage image,
	VkImageLayout &layout)
{
	::TransitionImage(
		commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT, layout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
	layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

} // namespace

void ResolveMsaaSampleCount(VulkanContextState *const context, RenderState *const render)
{
	if (context == nullptr || render == nullptr || context->physicalDevice == VK_NULL_HANDLE) {
		return;
	}
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(context->physicalDevice, &properties);
	render->msaaSampleCount = ClampMsaaSampleCount(
		properties.limits.framebufferColorSampleCounts,
		properties.limits.framebufferDepthSampleCounts,
		MsaaSampleCount(render->msaaMode));
}

void InvalidateProgressiveAccum(RenderState &render)
{
	render.progressiveAccumFrameIndex = 0u;
	render.progressiveAccumHistoryValid = false;
	render.progressiveAccumUpdateThisFrame = false;
	render.progressiveAccumApplyHalton = false;
	render.progressiveHaltonNdcX = 0.0f;
	render.progressiveHaltonNdcY = 0.0f;
	render.progressiveAccumPrevCameraValid = false;
	render.progressiveAccumPrevLightingValid = false;
}

bool UpdateProgressiveAccumState(
	RenderState &render,
	const CameraState &camera,
	const bool sceneDirty,
	const VkExtent2D renderExtent)
{
	const auto halton = [](uint32_t index, const uint32_t base) {
		float value = 0.0f;
		float fraction = 1.0f;
		while (index > 0u) {
			fraction /= static_cast<float>(base);
			value += fraction * static_cast<float>(index % base);
			index /= base;
		}
		return value;
	};
	const auto setHaltonNdc = [&render, &halton, renderExtent](const uint32_t index) {
		const float width = static_cast<float>(std::max(renderExtent.width, 1u));
		const float height = static_cast<float>(std::max(renderExtent.height, 1u));
		render.progressiveHaltonNdcX = (halton(index, 2u) - 0.5f) * 2.0f / width;
		render.progressiveHaltonNdcY = (halton(index, 3u) - 0.5f) * 2.0f / height;
	};

	const bool debugViewChanged = render.progressiveAccumPrevDebugView != render.lightingDebugControls.debugView;
	render.progressiveAccumPrevDebugView = render.lightingDebugControls.debugView;
	constexpr float kPositionEpsilon = 1.0e-4f;
	constexpr float kAngleEpsilon = 1.0e-4f;
	constexpr float kSunDirectionEpsilon = 1.0e-3f;
	constexpr float kExposureEpsilon = 1.0e-3f;
	const auto &lighting = render.currentSceneLighting;
	const projectv::math::Vec3 sunDirection{
		lighting.sunDirectionAndWrap[0],
		lighting.sunDirectionAndWrap[1],
		lighting.sunDirectionAndWrap[2],
	};
	const float exposure = lighting.postProcess[0];
	const float envIntensity = lighting.postProcess[1];
	const bool lightingChanged = !render.progressiveAccumPrevLightingValid ||
								 std::abs(sunDirection.x - render.progressiveAccumPrevSunDirection.x) > kSunDirectionEpsilon ||
								 std::abs(sunDirection.y - render.progressiveAccumPrevSunDirection.y) > kSunDirectionEpsilon ||
								 std::abs(sunDirection.z - render.progressiveAccumPrevSunDirection.z) > kSunDirectionEpsilon ||
								 std::abs(exposure - render.progressiveAccumPrevExposure) > kExposureEpsilon ||
								 std::abs(envIntensity - render.progressiveAccumPrevEnvIntensity) > kExposureEpsilon;
	render.progressiveAccumPrevSunDirection = sunDirection;
	render.progressiveAccumPrevExposure = exposure;
	render.progressiveAccumPrevEnvIntensity = envIntensity;
	render.progressiveAccumPrevLightingValid = true;

	const bool cameraMoved = !render.progressiveAccumPrevCameraValid ||
							 std::abs(camera.position.x - render.progressiveAccumPrevCameraPosition.x) > kPositionEpsilon ||
							 std::abs(camera.position.y - render.progressiveAccumPrevCameraPosition.y) > kPositionEpsilon ||
							 std::abs(camera.position.z - render.progressiveAccumPrevCameraPosition.z) > kPositionEpsilon ||
							 std::abs(camera.yawRadians - render.progressiveAccumPrevYaw) > kAngleEpsilon ||
							 std::abs(camera.pitchRadians - render.progressiveAccumPrevPitch) > kAngleEpsilon;
	render.progressiveAccumPrevCameraPosition = camera.position;
	render.progressiveAccumPrevYaw = camera.yawRadians;
	render.progressiveAccumPrevPitch = camera.pitchRadians;
	render.progressiveAccumPrevCameraValid = true;
	if (sceneDirty || cameraMoved || debugViewChanged || lightingChanged) {
		InvalidateProgressiveAccum(render);
		render.progressiveAccumPrevCameraValid = true;
		render.progressiveAccumPrevCameraPosition = camera.position;
		render.progressiveAccumPrevYaw = camera.yawRadians;
		render.progressiveAccumPrevPitch = camera.pitchRadians;
		render.progressiveAccumPrevLightingValid = true;
		render.progressiveAccumPrevSunDirection = sunDirection;
		render.progressiveAccumPrevExposure = exposure;
		render.progressiveAccumPrevEnvIntensity = envIntensity;
		return false;
	}
	if (render.progressiveAccumFrameIndex >= kProgressiveAccumMaxFrames) {
		// Freeze Halton mean: further /N mixes of a centered frame wash AA back to 1× MSAA.
		render.progressiveAccumUpdateThisFrame = false;
		render.progressiveAccumApplyHalton = false;
		render.progressiveHaltonNdcX = 0.0f;
		render.progressiveHaltonNdcY = 0.0f;
		render.progressiveAccumHistoryValid = true;
		return true;
	}
	++render.progressiveAccumFrameIndex;
	render.progressiveAccumUpdateThisFrame = true;
	render.progressiveAccumApplyHalton = render.progressiveAccumFrameIndex > 1u;
	if (render.progressiveAccumApplyHalton) {
		setHaltonNdc(render.progressiveAccumFrameIndex);
	} else {
		render.progressiveHaltonNdcX = 0.0f;
		render.progressiveHaltonNdcY = 0.0f;
	}
	render.progressiveAccumHistoryValid = render.progressiveAccumFrameIndex > 0u;
	return render.progressiveAccumFrameIndex > 1u;
}

void DestroyAaSceneTargets(VulkanContextState *const context, RenderState *const render)
{
	if (context == nullptr || render == nullptr || context->device == VK_NULL_HANDLE) {
		return;
	}
	DestroyImage(*context, render->sceneColorMsImage, render->sceneColorMsImageView, render->sceneColorMsAllocation);
	DestroyImage(*context, render->depthResolveImage, render->depthResolveImageView, render->depthResolveAllocation);
	DestroyImage(*context, render->sceneColorImage, render->sceneColorImageView, render->sceneColorAllocation);
	render->sceneColorCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	render->sceneColorMsCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	render->depthResolveCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	render->sceneColorNeedsInit = false;
}

bool CreateAaSceneTargets(
	VulkanContextState *const context,
	RenderState *const render,
	const VkExtent2D internalExtent)
{
	if (context == nullptr || render == nullptr || internalExtent.width == 0u || internalExtent.height == 0u) {
		return false;
	}
	DestroyAaSceneTargets(context, render);
	ResolveMsaaSampleCount(context, render);
	render->internalRenderExtent = internalExtent;
	constexpr VkFormat kSceneColorFormat = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
	if (!CreateImage2D(
			*context, kSceneColorFormat, internalExtent, VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT, render->sceneColorImage, render->sceneColorImageView, render->sceneColorAllocation,
			"SceneColorImage", "SceneColorImageView")) {
		return false;
	}

	if (render->msaaSampleCount > 1u) {
		if (!CreateImage2D(
				*context, kSceneColorFormat, internalExtent, ToVkSampleCount(render->msaaSampleCount),
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // no TRANSIENT: MS must persist across sky+geometry in one render; transient discards mid-frame
				VK_IMAGE_ASPECT_COLOR_BIT, render->sceneColorMsImage, render->sceneColorMsImageView, render->sceneColorMsAllocation,
				"SceneColorMsImage", "SceneColorMsImageView")) {
			DestroyAaSceneTargets(context, render);
			return false;
		}
		const VkFormat depthFormat = render->depthFormat == VK_FORMAT_UNDEFINED ? VK_FORMAT_D32_SFLOAT : render->depthFormat;
		if (!CreateImage2D(
				*context, depthFormat, internalExtent, VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_IMAGE_ASPECT_DEPTH_BIT, render->depthResolveImage, render->depthResolveImageView, render->depthResolveAllocation,
				"DepthResolveImage", "DepthResolveImageView")) {
			DestroyAaSceneTargets(context, render);
			return false;
		}
	}
	InvalidateProgressiveAccum(*render);
	return true;
}

void DestroyAaPassResources(VulkanContextState *const context, RenderState *const render)
{
	if (context == nullptr || render == nullptr || context->device == VK_NULL_HANDLE) {
		return;
	}
	const auto destroyPipeline = [&](VkPipeline &pipeline) {
		if (pipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(context->device, pipeline, nullptr);
			pipeline = VK_NULL_HANDLE;
		}
	};
	const auto destroyModule = [&](VkShaderModule &module) {
		if (module != VK_NULL_HANDLE) {
			vkDestroyShaderModule(context->device, module, nullptr);
			module = VK_NULL_HANDLE;
		}
	};
	destroyPipeline(render->tonemapResolvePipeline);
	destroyPipeline(render->progressiveAccumPipeline);
	destroyPipeline(render->smaaEdgePipeline);
	destroyPipeline(render->smaaBlendPipeline);
	destroyPipeline(render->smaaNeighborhoodPipeline);
	destroyModule(render->tonemapResolveShaderModule);
	destroyModule(render->progressiveAccumShaderModule);
	destroyModule(render->smaaEdgeShaderModule);
	destroyModule(render->smaaBlendShaderModule);
	destroyModule(render->smaaNeighborhoodShaderModule);
	if (render->aaSimplePipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(context->device, render->aaSimplePipelineLayout, nullptr);
		render->aaSimplePipelineLayout = VK_NULL_HANDLE;
	}
	if (render->aaNeighborhoodPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(context->device, render->aaNeighborhoodPipelineLayout, nullptr);
		render->aaNeighborhoodPipelineLayout = VK_NULL_HANDLE;
	}
	if (render->aaDescriptorPool != VK_NULL_HANDLE) {
		// aaDescriptorPool is an alias of aaFrameDescriptorPools[0]; destroy only via frame pools.
		render->aaDescriptorPool = VK_NULL_HANDLE;
	}
	for (VkDescriptorPool &framePool : render->aaFrameDescriptorPools) {
		if (framePool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(context->device, framePool, nullptr);
			framePool = VK_NULL_HANDLE;
		}
	}
	if (render->aaSimpleDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(context->device, render->aaSimpleDescriptorSetLayout, nullptr);
		render->aaSimpleDescriptorSetLayout = VK_NULL_HANDLE;
	}
	if (render->aaNeighborhoodDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(context->device, render->aaNeighborhoodDescriptorSetLayout, nullptr);
		render->aaNeighborhoodDescriptorSetLayout = VK_NULL_HANDLE;
	}
	if (render->aaLinearSampler != VK_NULL_HANDLE) {
		vkDestroySampler(context->device, render->aaLinearSampler, nullptr);
		render->aaLinearSampler = VK_NULL_HANDLE;
	}
	DestroyImage(*context, render->ldrColorImage, render->ldrColorImageView, render->ldrColorAllocation);
	DestroyImage(*context, render->accumHistoryImage, render->accumHistoryImageView, render->accumHistoryAllocation);
	DestroyImage(*context, render->smaaEdgesImage, render->smaaEdgesImageView, render->smaaEdgesAllocation);
	DestroyImage(*context, render->smaaBlendImage, render->smaaBlendImageView, render->smaaBlendAllocation);
	DestroyImage(*context, render->smaaOutputImage, render->smaaOutputImageView, render->smaaOutputAllocation);
	render->ldrColorCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	render->accumHistoryCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	render->aaPresentImage = VK_NULL_HANDLE;
	render->aaPresentImageView = VK_NULL_HANDLE;
	render->aaPresentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

bool CreateAaPassResources(
	VulkanContextState *const context,
	RenderState *const render,
	const VkExtent2D internalExtent)
{
	if (context == nullptr || render == nullptr || context->device == VK_NULL_HANDLE || context->allocator == nullptr ||
		internalExtent.width == 0u || internalExtent.height == 0u) {
		return false;
	}
	DestroyAaPassResources(context, render);

	VkSamplerCreateInfo samplerInfo{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	if (const VkResult result = vkCreateSampler(context->device, &samplerInfo, nullptr, &render->aaLinearSampler); result != VK_SUCCESS) {
		runtime::LogVkFailure("CreateAaPassResources.vkCreateSampler", result);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->aaLinearSampler), VK_OBJECT_TYPE_SAMPLER, "AaLinearSampler");

	const std::array simpleBindings{
		VkDescriptorSetLayoutBinding{.binding = 0u, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1u, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
		VkDescriptorSetLayoutBinding{.binding = 1u, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1u, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
	};
	const std::array neighborhoodBindings{
		VkDescriptorSetLayoutBinding{.binding = 0u, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1u, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
		VkDescriptorSetLayoutBinding{.binding = 1u, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1u, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
		VkDescriptorSetLayoutBinding{.binding = 2u, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1u, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
	};
	const auto createLayout = [&](const auto &bindings, VkDescriptorSetLayout &layout, const char *const name) {
		VkDescriptorSetLayoutCreateInfo layoutInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();
		const VkResult result = vkCreateDescriptorSetLayout(context->device, &layoutInfo, nullptr, &layout);
		if (result != VK_SUCCESS) {
			runtime::LogVkFailure("CreateAaPassResources.vkCreateDescriptorSetLayout", result);
			return false;
		}
		SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(layout), VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, name);
		return true;
	};
	if (!createLayout(simpleBindings, render->aaSimpleDescriptorSetLayout, "AaSimpleDescriptorSetLayout") ||
		!createLayout(neighborhoodBindings, render->aaNeighborhoodDescriptorSetLayout, "AaNeighborhoodDescriptorSetLayout")) {
		DestroyAaPassResources(context, render);
		return false;
	}

	VkPushConstantRange pushRange{};
	pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushRange.size = sizeof(AaTonemapPushConstants);
	const auto createPipelineLayout = [&](const VkDescriptorSetLayout descriptorSetLayout, VkPipelineLayout &layout, const char *const name) {
		VkPipelineLayoutCreateInfo layoutInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
		layoutInfo.setLayoutCount = 1u;
		layoutInfo.pSetLayouts = &descriptorSetLayout;
		layoutInfo.pushConstantRangeCount = 1u;
		layoutInfo.pPushConstantRanges = &pushRange;
		const VkResult result = vkCreatePipelineLayout(context->device, &layoutInfo, nullptr, &layout);
		if (result != VK_SUCCESS) {
			runtime::LogVkFailure("CreateAaPassResources.vkCreatePipelineLayout", result);
			return false;
		}
		SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(layout), VK_OBJECT_TYPE_PIPELINE_LAYOUT, name);
		return true;
	};
	if (!createPipelineLayout(render->aaSimpleDescriptorSetLayout, render->aaSimplePipelineLayout, "AaSimplePipelineLayout") ||
		!createPipelineLayout(render->aaNeighborhoodDescriptorSetLayout, render->aaNeighborhoodPipelineLayout, "AaNeighborhoodPipelineLayout")) {
		DestroyAaPassResources(context, render);
		return false;
	}

	const std::array poolSizes{
		VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = kAaDescriptorSetCount * 2u},
		VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = kAaDescriptorSetCount},
	};
	VkDescriptorPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	poolInfo.maxSets = kAaDescriptorSetCount;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	for (uint32_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
		if (const VkResult result = vkCreateDescriptorPool(context->device, &poolInfo, nullptr, &render->aaFrameDescriptorPools[frame]); result != VK_SUCCESS) {
			runtime::LogVkFailure("CreateAaPassResources.vkCreateDescriptorPool", result);
			DestroyAaPassResources(context, render);
			return false;
		}
	}
	render->aaDescriptorPool = render->aaFrameDescriptorPools[0]; // legacy fallback alias for first frame
	if (!CreateComputePipeline(*context, render->aaSimplePipelineLayout, "tonemap_resolve.comp.spv", render->tonemapResolveShaderModule, render->tonemapResolvePipeline) ||
		!CreateComputePipeline(*context, render->aaSimplePipelineLayout, "progressive_accum.comp.spv", render->progressiveAccumShaderModule, render->progressiveAccumPipeline) ||
		!CreateComputePipeline(*context, render->aaSimplePipelineLayout, "smaa_edge.comp.spv", render->smaaEdgeShaderModule, render->smaaEdgePipeline) ||
		!CreateComputePipeline(*context, render->aaSimplePipelineLayout, "smaa_blend.comp.spv", render->smaaBlendShaderModule, render->smaaBlendPipeline) ||
		!CreateComputePipeline(*context, render->aaNeighborhoodPipelineLayout, "smaa_neighborhood.comp.spv", render->smaaNeighborhoodShaderModule, render->smaaNeighborhoodPipeline) ||
		!CreateImage2D(*context, VK_FORMAT_R8G8B8A8_UNORM, internalExtent, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT, render->ldrColorImage, render->ldrColorImageView, render->ldrColorAllocation, "LdrColorImage", "LdrColorImageView") ||
		!CreateImage2D(*context, VK_FORMAT_R16G16B16A16_SFLOAT, internalExtent, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, render->accumHistoryImage, render->accumHistoryImageView, render->accumHistoryAllocation, "AccumHistoryImage", "AccumHistoryImageView") ||
		!CreateImage2D(*context, VK_FORMAT_R16G16_SFLOAT, internalExtent, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, render->smaaEdgesImage, render->smaaEdgesImageView, render->smaaEdgesAllocation, "SmaaEdgesImage", "SmaaEdgesImageView") ||
		!CreateImage2D(*context, VK_FORMAT_R8G8B8A8_UNORM, internalExtent, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, render->smaaBlendImage, render->smaaBlendImageView, render->smaaBlendAllocation, "SmaaBlendImage", "SmaaBlendImageView") ||
		!CreateImage2D(*context, VK_FORMAT_R8G8B8A8_UNORM, internalExtent, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT, render->smaaOutputImage, render->smaaOutputImageView, render->smaaOutputAllocation, "SmaaOutputImage", "SmaaOutputImageView")) {
		DestroyAaPassResources(context, render);
		return false;
	}
	render->ldrColorCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	render->accumHistoryCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	InvalidateProgressiveAccum(*render);
	return true;
}

bool RecreateAaDependentPipelines(
	VulkanContextState *const context,
	SwapchainState *const swapchain,
	RenderState *const render)
{
	if (context == nullptr || swapchain == nullptr || render == nullptr) {
		return false;
	}
	DestroyGraphicsPipeline(context, render);
	if (!CreateGraphicsPipeline(context, swapchain, render)) {
		return false;
	}
	if (IsMeshShaderPipelineRequested()) {
		DestroyMeshShaderPipelines(context, render);
		if (!CreateMeshShaderPipelines(context, render)) {
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "AA recreate: mesh shaders unavailable; PackedFace path remains");
		}
	}
	DestroySkyAtmospherePipelines(context, render);
	if (IsSkyAtmosphereEnabled() && !CreateSkyAtmospherePipelines(context, render)) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "AA recreate: sky atmosphere pipelines failed");
	}
	DestroyCloudscapeResources(context, render);
	if (!CreateCloudscapeResources(context, render)) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "AA recreate: cloudscape resources failed");
	}
	if (render->graphicsPipelineLayout != VK_NULL_HANDLE) {
		projectv::asset::DestroyModelPipeline(context, render);
		const VkFormat colorFormat = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
		const VkFormat depthFormat = render->depthFormat != VK_FORMAT_UNDEFINED
										 ? render->depthFormat
										 : VK_FORMAT_D32_SFLOAT;
		if (!projectv::asset::CreateModelPipeline(
				context, render->graphicsPipelineLayout, colorFormat, depthFormat, render)) {
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "AA recreate: model pipeline failed");
		}
	}
	if (!RefreshGraphicsResourceBindings(context, render)) {
		runtime::LogRuntimeFailure(
			"AaPass",
			"RecreateAaDependentPipelines.RefreshGraphicsResourceBindings",
			"graphics descriptor sets missing after AA pipeline recreate");
		return false;
	}
	render->pipelinesMsaaSampleCount = render->msaaSampleCount;
	render->aaPipelinesNeedRecreate = false;
	return true;
}

bool RecordAaResolvePass(
	const VkCommandBuffer commandBuffer,
	VulkanContextState &context,
	RenderState &render,
	const VkImage hdrSourceImage,
	const VkImageView hdrSourceView,
	VkImageLayout &hdrSourceLayout,
	const VkExtent2D extent,
	const uint32_t frameIndex)
{
	if (commandBuffer == VK_NULL_HANDLE || render.tonemapResolvePipeline == VK_NULL_HANDLE || render.ldrColorImage == VK_NULL_HANDLE) {
		return false;
	}
	const uint32_t poolIndex = frameIndex % MAX_FRAMES_IN_FLIGHT;
	VkDescriptorPool &framePool = render.aaFrameDescriptorPools[poolIndex];
	if (framePool == VK_NULL_HANDLE) {
		return false;
	}
	if (const VkResult result = vkResetDescriptorPool(context.device, framePool, 0u); result != VK_SUCCESS) {
		runtime::LogVkFailure("RecordAaResolvePass.vkResetDescriptorPool", result);
		return false;
	}
	TransitionForComputeRead(commandBuffer, hdrSourceImage, hdrSourceLayout);

	VkImageView tonemapInputView = hdrSourceView;
	VkImageLayout tonemapInputLayout = hdrSourceLayout;
	if (render.progressiveAccumFrameIndex > 0u && render.accumHistoryImage != VK_NULL_HANDLE) {
		if (render.progressiveAccumUpdateThisFrame && render.progressiveAccumPipeline != VK_NULL_HANDLE) {
			TransitionForComputeWrite(commandBuffer, render.accumHistoryImage, render.accumHistoryCurrentLayout);
			const VkDescriptorSet descriptorSet = AllocateAaSet(context, render, render.aaSimpleDescriptorSetLayout, frameIndex);
			if (descriptorSet == VK_NULL_HANDLE) {
				return false;
			}
			WriteSampledStorage(context, descriptorSet, render.aaLinearSampler, hdrSourceView, hdrSourceLayout, render.accumHistoryImageView, VK_IMAGE_LAYOUT_GENERAL);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, render.progressiveAccumPipeline);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, render.aaSimplePipelineLayout, 0u, 1u, &descriptorSet, 0u, nullptr);
			AaAccumPushConstants accumPush{};
			accumPush.params0[0] = static_cast<float>(std::max(render.progressiveAccumFrameIndex, 1u));
			vkCmdPushConstants(commandBuffer, render.aaSimplePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(accumPush), &accumPush);
			vkCmdDispatch(commandBuffer, (extent.width + 15u) / 16u, (extent.height + 15u) / 16u, 1u);
			AaMemoryBarrier(commandBuffer);
		}
		TransitionForComputeRead(commandBuffer, render.accumHistoryImage, render.accumHistoryCurrentLayout);
		tonemapInputView = render.accumHistoryImageView;
		tonemapInputLayout = render.accumHistoryCurrentLayout;
	}

	TransitionForComputeWrite(commandBuffer, render.ldrColorImage, render.ldrColorCurrentLayout);
	const VkDescriptorSet tonemapSet = AllocateAaSet(context, render, render.aaSimpleDescriptorSetLayout, frameIndex);
	if (tonemapSet == VK_NULL_HANDLE) {
		return false;
	}
	WriteSampledStorage(context, tonemapSet, render.aaLinearSampler, tonemapInputView, tonemapInputLayout, render.ldrColorImageView, VK_IMAGE_LAYOUT_GENERAL);
	AaTonemapPushConstants toneMapPush{};
	toneMapPush.params0[0] = render.currentSceneLighting.postProcess[0];
	toneMapPush.params0[1] = render.currentSceneLighting.postProcess[2];
	toneMapPush.params1 = render.currentSceneLighting.colorGrading;
	DispatchSimple(commandBuffer, render.tonemapResolvePipeline, render.aaSimplePipelineLayout, tonemapSet, extent, toneMapPush);
	AaMemoryBarrier(commandBuffer);

	VkImage finalImage = render.ldrColorImage;
	VkImageView finalView = render.ldrColorImageView;
	VkImageLayout finalLayout = VK_IMAGE_LAYOUT_GENERAL;
	if (render.smaaEnabled && render.smaaEdgePipeline != VK_NULL_HANDLE &&
		render.smaaBlendPipeline != VK_NULL_HANDLE && render.smaaNeighborhoodPipeline != VK_NULL_HANDLE) {
		TransitionForComputeRead(commandBuffer, render.ldrColorImage, render.ldrColorCurrentLayout);
		VkImageLayout edgesLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		TransitionForComputeWrite(commandBuffer, render.smaaEdgesImage, edgesLayout);
		const VkDescriptorSet edgeSet = AllocateAaSet(context, render, render.aaSimpleDescriptorSetLayout, frameIndex);
		if (edgeSet == VK_NULL_HANDLE) {
			return false;
		}
		WriteSampledStorage(context, edgeSet, render.aaLinearSampler, render.ldrColorImageView, render.ldrColorCurrentLayout, render.smaaEdgesImageView, edgesLayout);
		DispatchSimple(commandBuffer, render.smaaEdgePipeline, render.aaSimplePipelineLayout, edgeSet, extent, {});
		AaMemoryBarrier(commandBuffer);
		TransitionForComputeRead(commandBuffer, render.smaaEdgesImage, edgesLayout);

		VkImageLayout blendLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		TransitionForComputeWrite(commandBuffer, render.smaaBlendImage, blendLayout);
		const VkDescriptorSet blendSet = AllocateAaSet(context, render, render.aaSimpleDescriptorSetLayout, frameIndex);
		if (blendSet == VK_NULL_HANDLE) {
			return false;
		}
		WriteSampledStorage(context, blendSet, render.aaLinearSampler, render.smaaEdgesImageView, edgesLayout, render.smaaBlendImageView, blendLayout);
		DispatchSimple(commandBuffer, render.smaaBlendPipeline, render.aaSimplePipelineLayout, blendSet, extent, {});
		AaMemoryBarrier(commandBuffer);
		TransitionForComputeRead(commandBuffer, render.smaaBlendImage, blendLayout);

		VkImageLayout outputLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		TransitionForComputeWrite(commandBuffer, render.smaaOutputImage, outputLayout);
		const VkDescriptorSet neighborhoodSet = AllocateAaSet(context, render, render.aaNeighborhoodDescriptorSetLayout, frameIndex);
		if (neighborhoodSet == VK_NULL_HANDLE) {
			return false;
		}
		WriteTwoSampledOneStorage(context, neighborhoodSet, render.aaLinearSampler, render.ldrColorImageView, render.ldrColorCurrentLayout, render.smaaBlendImageView, blendLayout, render.smaaOutputImageView, outputLayout);
		DispatchSimple(commandBuffer, render.smaaNeighborhoodPipeline, render.aaNeighborhoodPipelineLayout, neighborhoodSet, extent, {});
		AaMemoryBarrier(commandBuffer);
		::TransitionImage(
			commandBuffer, render.smaaOutputImage, VK_IMAGE_ASPECT_COLOR_BIT, outputLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
		finalImage = render.smaaOutputImage;
		finalView = render.smaaOutputImageView;
		finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	} else {
		::TransitionImage(
			commandBuffer, render.ldrColorImage, VK_IMAGE_ASPECT_COLOR_BIT, render.ldrColorCurrentLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
		render.ldrColorCurrentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		finalLayout = render.ldrColorCurrentLayout;
	}
	render.aaPresentImage = finalImage;
	render.aaPresentImageView = finalView;
	render.aaPresentLayout = finalLayout;
	(void)frameIndex;
	return true;
}

} // namespace projectv::render

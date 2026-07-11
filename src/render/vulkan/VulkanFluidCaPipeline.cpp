#include "render/vulkan/VulkanFluidCaPipeline.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "debug/Profiling.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <array>
#include <vector>

#include "SDL3/SDL_log.h"

namespace {
constexpr uint32_t kFluidCaDescriptorSetCount = MAX_FRAMES_IN_FLIGHT;
constexpr char kFluidCaShaderFilename[] = "fluid_ca.comp.spv";

constexpr std::array kFluidCaDescriptorBindings{
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
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 3,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
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
};

constexpr VkDescriptorSetLayoutCreateInfo kFluidCaDescriptorSetLayoutInfo{
	.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	.pNext = nullptr,
	.flags = 0,
	.bindingCount = static_cast<uint32_t>(kFluidCaDescriptorBindings.size()),
	.pBindings = kFluidCaDescriptorBindings.data(),
};

constexpr std::array kFluidCaDescriptorPoolSizes{
	VkDescriptorPoolSize{
		.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = kFluidCaDescriptorSetCount * 5u,
	},
};

VkShaderModule CreateFluidCaShaderModule(const VkDevice device, const std::vector<char> &code)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

	VkShaderModule shaderModule = VK_NULL_HANDLE;
	const VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
	if (result != VK_SUCCESS) {
		runtime::LogVkFailure("CreateFluidCaShaderModule", result);
		return VK_NULL_HANDLE;
	}
	return shaderModule;
}
}  // namespace

namespace projectv::render {

void DestroyFluidCaPipelines(VulkanContextState *context, RenderState *render)
{
	if (context == nullptr || render == nullptr) {
		return;
	}
	if (render->fluidCaDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(context->device, render->fluidCaDescriptorPool, nullptr);
		render->fluidCaDescriptorPool = VK_NULL_HANDLE;
	}
	if (render->fluidCaDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(context->device, render->fluidCaDescriptorSetLayout, nullptr);
		render->fluidCaDescriptorSetLayout = VK_NULL_HANDLE;
	}
	if (render->fluidCaPipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(context->device, render->fluidCaPipeline, nullptr);
		render->fluidCaPipeline = VK_NULL_HANDLE;
	}
	if (render->fluidCaPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(context->device, render->fluidCaPipelineLayout, nullptr);
		render->fluidCaPipelineLayout = VK_NULL_HANDLE;
	}
	if (render->fluidCaShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context->device, render->fluidCaShaderModule, nullptr);
		render->fluidCaShaderModule = VK_NULL_HANDLE;
	}
	for (SceneFrameResources &frameResources : render->sceneFrameResources) {
		if (frameResources.fluidCaSourceBuffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(context->allocator, frameResources.fluidCaSourceBuffer, frameResources.fluidCaSourceAllocation);
			frameResources.fluidCaSourceBuffer = VK_NULL_HANDLE;
			frameResources.fluidCaSourceAllocation = nullptr;
			frameResources.fluidCaSourceMappedData = nullptr;
		}
		if (frameResources.fluidCaDestinationBuffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(context->allocator, frameResources.fluidCaDestinationBuffer, frameResources.fluidCaDestinationAllocation);
			frameResources.fluidCaDestinationBuffer = VK_NULL_HANDLE;
			frameResources.fluidCaDestinationAllocation = nullptr;
			frameResources.fluidCaDestinationMappedData = nullptr;
		}
		if (frameResources.fluidCaActiveChunkIdBuffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(context->allocator, frameResources.fluidCaActiveChunkIdBuffer, frameResources.fluidCaActiveChunkIdAllocation);
			frameResources.fluidCaActiveChunkIdBuffer = VK_NULL_HANDLE;
			frameResources.fluidCaActiveChunkIdAllocation = nullptr;
			frameResources.fluidCaActiveChunkIdMappedData = nullptr;
		}
		if (frameResources.fluidCaStatsBuffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(context->allocator, frameResources.fluidCaStatsBuffer, frameResources.fluidCaStatsAllocation);
			frameResources.fluidCaStatsBuffer = VK_NULL_HANDLE;
			frameResources.fluidCaStatsAllocation = nullptr;
			frameResources.fluidCaStatsMappedData = nullptr;
		}
	}
	render->fluidCaPipelineEnabled = false;
	render->fluidCaPingPongBufferBytes = 0u;
	render->fluidCaMaxActiveChunks = 0u;
}

bool CreateFluidCaPipelines(VulkanContextState *context, RenderState *render)
{
	PV_PROFILE_ZONE_N("CreateFluidCaPipelines");
	PV_CHECK_OR_RETURN(
		context && render && context->device && context->allocator,
		"Render",
		"CreateFluidCaPipelines.Preconditions",
		"context/render/device/allocator incomplete");
	if (!IsFluidCaGpuPipelineRequested()) {
		return false;
	}

	DestroyFluidCaPipelines(context, render);

	const std::vector<char> shaderCode = ReadShaderFile(kFluidCaShaderFilename);
	if (shaderCode.empty()) {
		runtime::LogRuntimeFailure(
			"Render",
			"CreateFluidCaPipelines.ReadShaderFile",
			"fluid_ca.comp.spv not found or empty");
		DestroyFluidCaPipelines(context, render);
		return false;
	}

	VkShaderModule shaderModule = CreateFluidCaShaderModule(context->device, shaderCode);
	if (shaderModule == VK_NULL_HANDLE) {
		DestroyFluidCaPipelines(context, render);
		return false;
	}
	render->fluidCaShaderModule = shaderModule;
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(shaderModule), VK_OBJECT_TYPE_SHADER_MODULE, "FluidCaShader");

	const VkDescriptorSetLayoutCreateInfo layoutInfo = kFluidCaDescriptorSetLayoutInfo;
	VkResult layoutResult = vkCreateDescriptorSetLayout(context->device, &layoutInfo, nullptr, &render->fluidCaDescriptorSetLayout);
	if (layoutResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateFluidCaPipelines.vkCreateDescriptorSetLayout", layoutResult);
		DestroyFluidCaPipelines(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->fluidCaDescriptorSetLayout), VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "FluidCaDescriptorSetLayout");

	const VkPushConstantRange pushConstantRange{
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.offset = 0u,
		.size = sizeof(FluidCaPushConstants),
	};
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1u;
	pipelineLayoutInfo.pSetLayouts = &render->fluidCaDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 1u;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
	VkResult layoutCreateResult = vkCreatePipelineLayout(context->device, &pipelineLayoutInfo, nullptr, &render->fluidCaPipelineLayout);
	if (layoutCreateResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateFluidCaPipelines.vkCreatePipelineLayout", layoutCreateResult);
		DestroyFluidCaPipelines(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->fluidCaPipelineLayout), VK_OBJECT_TYPE_PIPELINE_LAYOUT, "FluidCaPipelineLayout");

	VkComputePipelineCreateInfo pipelineInfo{
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.stage =
			{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			 .stage = VK_SHADER_STAGE_COMPUTE_BIT,
			 .module = shaderModule,
			 .pName = "main"},
		.layout = render->fluidCaPipelineLayout,
	};
	VkResult pipelineResult = vkCreateComputePipelines(context->device, VK_NULL_HANDLE, 1u, &pipelineInfo, nullptr, &render->fluidCaPipeline);
	if (pipelineResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateFluidCaPipelines.vkCreateComputePipelines", pipelineResult);
		DestroyFluidCaPipelines(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->fluidCaPipeline), VK_OBJECT_TYPE_PIPELINE, "FluidCaPipeline");

	render->fluidCaPipelineEnabled = true;
	SDL_LogInfo(
		SDL_LOG_CATEGORY_APPLICATION,
		"Fluid CA GPU pipeline created (descriptorSetLayout+pool+pipeline+layout ready)");
	return true;
}

bool RefreshFluidCaResourceBindings(
	VulkanContextState *context,
	RenderState *render)
{
	PV_PROFILE_ZONE_N("RefreshFluidCaResourceBindings");
	PV_CHECK_OR_RETURN(
		context && render && context->device,
		"Render",
		"RefreshFluidCaResourceBindings.Preconditions",
		"context/render/device incomplete");
	if (!render->fluidCaPipelineEnabled) {
		return true;
	}
	if (render->fluidCaDescriptorSetLayout == VK_NULL_HANDLE) {
		return true;
	}

	if (render->fluidCaDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(context->device, render->fluidCaDescriptorPool, nullptr);
		render->fluidCaDescriptorPool = VK_NULL_HANDLE;
	}

	static constexpr VkDescriptorPoolCreateInfo poolInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.maxSets = kFluidCaDescriptorSetCount,
		.poolSizeCount = static_cast<uint32_t>(kFluidCaDescriptorPoolSizes.size()),
		.pPoolSizes = kFluidCaDescriptorPoolSizes.data(),
	};
	VkResult poolResult = vkCreateDescriptorPool(context->device, &poolInfo, nullptr, &render->fluidCaDescriptorPool);
	if (poolResult != VK_SUCCESS) {
		runtime::LogVkFailure("RefreshFluidCaResourceBindings.vkCreateDescriptorPool", poolResult);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->fluidCaDescriptorPool), VK_OBJECT_TYPE_DESCRIPTOR_POOL, "FluidCaDescriptorPool");

	for (size_t i = 0; i < render->sceneFrameResources.size(); ++i) {
		SceneFrameResources &frameResources = render->sceneFrameResources[i];
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = render->fluidCaDescriptorPool;
		allocInfo.descriptorSetCount = 1u;
		allocInfo.pSetLayouts = &render->fluidCaDescriptorSetLayout;
		VkResult allocResult = vkAllocateDescriptorSets(context->device, &allocInfo, &frameResources.fluidCaDescriptorSet);
		if (allocResult != VK_SUCCESS) {
			runtime::LogVkFailure("RefreshFluidCaResourceBindings.vkAllocateDescriptorSets", allocResult);
			return false;
		}

		if (frameResources.chunkDescriptorBuffer == VK_NULL_HANDLE ||
			frameResources.fluidCaActiveChunkIdBuffer == VK_NULL_HANDLE ||
			frameResources.fluidCaSourceBuffer == VK_NULL_HANDLE ||
			frameResources.fluidCaDestinationBuffer == VK_NULL_HANDLE ||
			frameResources.fluidCaStatsBuffer == VK_NULL_HANDLE) {
			continue;
		}

		const VkDescriptorBufferInfo chunkDescInfo{
			.buffer = frameResources.chunkDescriptorBuffer,
			.offset = 0u,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo activeChunkIdInfo{
			.buffer = frameResources.fluidCaActiveChunkIdBuffer,
			.offset = 0u,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo sourceInfo{
			.buffer = frameResources.fluidCaSourceBuffer,
			.offset = 0u,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo destinationInfo{
			.buffer = frameResources.fluidCaDestinationBuffer,
			.offset = 0u,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo statsInfo{
			.buffer = frameResources.fluidCaStatsBuffer,
			.offset = 0u,
			.range = VK_WHOLE_SIZE,
		};

		const std::array descriptorWrites{
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.fluidCaDescriptorSet,
				.dstBinding = 0u,
				.dstArrayElement = 0u,
				.descriptorCount = 1u,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &chunkDescInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.fluidCaDescriptorSet,
				.dstBinding = 1u,
				.dstArrayElement = 0u,
				.descriptorCount = 1u,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &activeChunkIdInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.fluidCaDescriptorSet,
				.dstBinding = 2u,
				.dstArrayElement = 0u,
				.descriptorCount = 1u,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &sourceInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.fluidCaDescriptorSet,
				.dstBinding = 3u,
				.dstArrayElement = 0u,
				.descriptorCount = 1u,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &destinationInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.fluidCaDescriptorSet,
				.dstBinding = 4u,
				.dstArrayElement = 0u,
				.descriptorCount = 1u,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &statsInfo,
				.pTexelBufferView = nullptr,
			},
		};
		vkUpdateDescriptorSets(
			context->device,
			static_cast<uint32_t>(descriptorWrites.size()),
			descriptorWrites.data(),
			0u,
			nullptr);
	}
	return true;
}

bool RecordFluidCaDispatch(
	VkCommandBuffer commandBuffer,
	RenderState &render,
	SceneFrameResources &frameResources,
	const FluidCaPushConstants &pushConstants,
	uint32_t activeChunkCount)
{
	PV_PROFILE_ZONE_N("RecordFluidCaDispatch");
	if (!render.fluidCaPipelineEnabled) {
		return false;
	}
	if (commandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (render.fluidCaPipeline == VK_NULL_HANDLE ||
		render.fluidCaPipelineLayout == VK_NULL_HANDLE) {
		return false;
	}
	if (frameResources.fluidCaDescriptorSet == VK_NULL_HANDLE) {
		return false;
	}
	if (frameResources.fluidCaSourceBuffer == VK_NULL_HANDLE ||
		frameResources.fluidCaDestinationBuffer == VK_NULL_HANDLE ||
		frameResources.fluidCaActiveChunkIdBuffer == VK_NULL_HANDLE ||
		frameResources.fluidCaStatsBuffer == VK_NULL_HANDLE) {
		return false;
	}

	if (frameResources.fluidCaStatsMappedData) {
		std::memset(frameResources.fluidCaStatsMappedData, 0, sizeof(FluidCaGpuFrameStats));
	}

	VkBufferMemoryBarrier2 statsFillBarrier{};
	statsFillBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	statsFillBarrier.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
	statsFillBarrier.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT;
	statsFillBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	statsFillBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	statsFillBarrier.buffer = frameResources.fluidCaStatsBuffer;
	statsFillBarrier.offset = 0u;
	statsFillBarrier.size = sizeof(FluidCaGpuFrameStats);

	VkBufferMemoryBarrier2 sourceBarrier{};
	sourceBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	sourceBarrier.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
	sourceBarrier.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT;
	sourceBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	sourceBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	sourceBarrier.buffer = frameResources.fluidCaSourceBuffer;
	sourceBarrier.offset = 0u;
	sourceBarrier.size = VK_WHOLE_SIZE;

	VkBufferMemoryBarrier2 activeChunkIdBarrier{};
	activeChunkIdBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	activeChunkIdBarrier.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
	activeChunkIdBarrier.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT;
	activeChunkIdBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	activeChunkIdBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	activeChunkIdBarrier.buffer = frameResources.fluidCaActiveChunkIdBuffer;
	activeChunkIdBarrier.offset = 0u;
	activeChunkIdBarrier.size = VK_WHOLE_SIZE;

	{
		const std::array preBarriers{statsFillBarrier, sourceBarrier, activeChunkIdBarrier};
		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(preBarriers.size());
		depInfo.pBufferMemoryBarriers = preBarriers.data();
		vkCmdPipelineBarrier2(commandBuffer, &depInfo);
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, render.fluidCaPipeline);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		render.fluidCaPipelineLayout,
		0u,
		1u,
		&frameResources.fluidCaDescriptorSet,
		0u,
		nullptr);
	vkCmdPushConstants(
		commandBuffer,
		render.fluidCaPipelineLayout,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0u,
		sizeof(FluidCaPushConstants),
		&pushConstants);

	if (activeChunkCount > 0u) {
		vkCmdDispatch(commandBuffer, activeChunkCount, 1u, 1u);
	}

	VkBufferMemoryBarrier2 postBarrier{};
	postBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	postBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	postBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	postBarrier.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
	postBarrier.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
	postBarrier.buffer = frameResources.fluidCaStatsBuffer;
	postBarrier.offset = 0u;
	postBarrier.size = sizeof(FluidCaGpuFrameStats);

	VkBufferMemoryBarrier2 destBarrier{};
	destBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	destBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	destBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	destBarrier.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
	destBarrier.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
	destBarrier.buffer = frameResources.fluidCaDestinationBuffer;
	destBarrier.offset = 0u;
	destBarrier.size = VK_WHOLE_SIZE;

	{
		const std::array postBarriers{postBarrier, destBarrier};
		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(postBarriers.size());
		depInfo.pBufferMemoryBarriers = postBarriers.data();
		vkCmdPipelineBarrier2(commandBuffer, &depInfo);
	}

	return true;
}

bool SubmitFluidCaToComputeQueue(
	VulkanContextState *context,
	RenderState &render,
	VkCommandBuffer commandBuffer,
	uint64_t *outTimelineValue)
{
	PV_PROFILE_ZONE_N("SubmitFluidCaToComputeQueue");
	PV_CHECK_OR_RETURN(
		context && context->dedicatedComputeQueue != VK_NULL_HANDLE &&
		context->renderTimelineSemaphore != VK_NULL_HANDLE,
		"Render",
		"SubmitFluidCaToComputeQueue.Preconditions",
		"dedicated compute queue or timeline semaphore unavailable");
	if (commandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (!render.fluidCaPipelineEnabled) {
		return false;
	}

	context->renderTimelineValue += 1u;
	const uint64_t timelineValue = context->renderTimelineValue;
	if (outTimelineValue != nullptr) {
		*outTimelineValue = timelineValue;
	}

	VkCommandBufferSubmitInfo commandBufferSubmitInfo{};
	commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	commandBufferSubmitInfo.commandBuffer = commandBuffer;

	VkSemaphoreSubmitInfo waitSemaphoreInfo{};
	waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	waitSemaphoreInfo.semaphore = context->renderTimelineSemaphore;
	waitSemaphoreInfo.value = timelineValue - 1u;
	waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

	VkSemaphoreSubmitInfo signalSemaphoreInfo{};
	signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalSemaphoreInfo.semaphore = context->renderTimelineSemaphore;
	signalSemaphoreInfo.value = timelineValue;
	signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

	VkSubmitInfo2 submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.commandBufferInfoCount = 1u;
	submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;
	submitInfo.waitSemaphoreInfoCount = 1u;
	submitInfo.pWaitSemaphoreInfos = &waitSemaphoreInfo;
	submitInfo.signalSemaphoreInfoCount = 1u;
	submitInfo.pSignalSemaphoreInfos = &signalSemaphoreInfo;

	const VkResult result = vkQueueSubmit2(context->dedicatedComputeQueue, 1u, &submitInfo, VK_NULL_HANDLE);
	if (result != VK_SUCCESS) {
		runtime::LogVkFailure("SubmitFluidCaToComputeQueue.vkQueueSubmit2", result);
		context->renderTimelineValue -= 1u;
		return false;
	}
	return true;
}

bool ReadFluidCaFrameStats(
	VulkanContextState *context,
	const RenderState &render,
	uint32_t frameIndex,
	FluidCaGpuFrameStats *outStats)
{
	PV_PROFILE_ZONE_N("ReadFluidCaFrameStats");
	if (context == nullptr || outStats == nullptr) {
		return false;
	}
	if (!render.fluidCaPipelineEnabled) {
		return false;
	}
	if (static_cast<size_t>(frameIndex) >= render.sceneFrameResources.size()) {
		return false;
	}
	const SceneFrameResources &frameResources = render.sceneFrameResources[frameIndex];
	if (frameResources.fluidCaStatsBuffer == VK_NULL_HANDLE ||
		frameResources.fluidCaStatsAllocation == nullptr ||
		frameResources.fluidCaStatsMappedData == nullptr) {
		return false;
	}

	const VkResult invalidateResult = vmaInvalidateAllocation(
		context->allocator,
		frameResources.fluidCaStatsAllocation,
		0u,
		sizeof(FluidCaGpuFrameStats));
	if (invalidateResult != VK_SUCCESS) {
		runtime::LogVmaFailure("ReadFluidCaFrameStats.vmaInvalidateAllocation", invalidateResult);
		return false;
	}
	std::memcpy(outStats, frameResources.fluidCaStatsMappedData, sizeof(FluidCaGpuFrameStats));
	return true;
}

}  // namespace projectv::render

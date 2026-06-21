#include "render/vulkan/VulkanWorldGenPipeline.hpp"

#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "debug/Profiling.hpp"
#include "render/SceneResources.hpp"
#include "render/vulkan/VulkanDebug.hpp"
#include "voxel/VoxelWorld.hpp"

#include <cstring>
#include <vector>

#include <vk_mem_alloc.h>

namespace projectv::render {

bool CreateWorldGenPipelines(VulkanContextState *context, RenderState *render)
{
	if (render->worldGenPipelineEnabled) {
		return true;
	}
	if (!context || !render) {
		return false;
	}
	if (context->device == VK_NULL_HANDLE) {
		return false;
	}
	if (!IsWorldGenGpuPipelineRequested()) {
		return false;
	}

	DestroyWorldGenPipelines(context, render);

	const std::vector<char> shaderCode = ReadShaderFile("world_gen.comp.spv");
	if (shaderCode.empty()) {
		runtime::LogRuntimeFailure(
			"Render",
			"CreateWorldGenPipelines.ReadShaderFile",
			"world_gen.comp.spv not found or empty");
		return false;
	}

	VkShaderModuleCreateInfo shaderInfo{};
	shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderInfo.codeSize = static_cast<uint32_t>(shaderCode.size());
	shaderInfo.pCode = reinterpret_cast<const uint32_t *>(shaderCode.data());
	if (vkCreateShaderModule(context->device, &shaderInfo, nullptr, &render->worldGenShaderModule) != VK_SUCCESS) {
		return false;
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->worldGenShaderModule),
		VK_OBJECT_TYPE_SHADER_MODULE,
		"WorldGenShaderModule");

	const VkPushConstantRange pushConstantRange{
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.offset = 0u,
		.size = sizeof(WorldGenPushConstants),
	};

	const std::array<VkDescriptorSetLayoutBinding, 1> bindings{{
		{
			.binding = 0u,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1u,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr,
		},
	}};

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();
	if (vkCreateDescriptorSetLayout(context->device, &layoutInfo, nullptr, &render->worldGenDescriptorSetLayout) != VK_SUCCESS) {
		vkDestroyShaderModule(context->device, render->worldGenShaderModule, nullptr);
		render->worldGenShaderModule = VK_NULL_HANDLE;
		return false;
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->worldGenDescriptorSetLayout),
		VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
		"WorldGenDescriptorSetLayout");

	VkPipelineLayoutCreateInfo layoutCreateInfo{};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.setLayoutCount = 1u;
	layoutCreateInfo.pSetLayouts = &render->worldGenDescriptorSetLayout;
	layoutCreateInfo.pushConstantRangeCount = 1u;
	layoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	if (vkCreatePipelineLayout(context->device, &layoutCreateInfo, nullptr, &render->worldGenPipelineLayout) != VK_SUCCESS) {
		vkDestroyDescriptorSetLayout(context->device, render->worldGenDescriptorSetLayout, nullptr);
		render->worldGenDescriptorSetLayout = VK_NULL_HANDLE;
		vkDestroyShaderModule(context->device, render->worldGenShaderModule, nullptr);
		render->worldGenShaderModule = VK_NULL_HANDLE;
		return false;
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->worldGenPipelineLayout),
		VK_OBJECT_TYPE_PIPELINE_LAYOUT,
		"WorldGenPipelineLayout");

	VkComputePipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.layout = render->worldGenPipelineLayout;
	pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	pipelineInfo.stage.module = render->worldGenShaderModule;
	pipelineInfo.stage.pName = "main";
	if (vkCreateComputePipelines(context->device, VK_NULL_HANDLE, 1u, &pipelineInfo, nullptr, &render->worldGenPipeline) != VK_SUCCESS) {
		vkDestroyPipelineLayout(context->device, render->worldGenPipelineLayout, nullptr);
		render->worldGenPipelineLayout = VK_NULL_HANDLE;
		vkDestroyDescriptorSetLayout(context->device, render->worldGenDescriptorSetLayout, nullptr);
		render->worldGenDescriptorSetLayout = VK_NULL_HANDLE;
		vkDestroyShaderModule(context->device, render->worldGenShaderModule, nullptr);
		render->worldGenShaderModule = VK_NULL_HANDLE;
		return false;
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->worldGenPipeline),
		VK_OBJECT_TYPE_PIPELINE,
		"WorldGenPipeline");

	render->worldGenPipelineEnabled = true;
	return true;
}

void DestroyWorldGenPipelines(VulkanContextState *context, RenderState *render)
{
	if (!context || !render) {
		return;
	}
	for (SceneFrameResources &frameResources : render->sceneFrameResources) {
		frameResources.worldGenDescriptorSet = VK_NULL_HANDLE;
	}
	if (render->worldGenDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(context->device, render->worldGenDescriptorPool, nullptr);
		render->worldGenDescriptorPool = VK_NULL_HANDLE;
	}
	if (render->worldGenPipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(context->device, render->worldGenPipeline, nullptr);
		render->worldGenPipeline = VK_NULL_HANDLE;
	}
	if (render->worldGenPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(context->device, render->worldGenPipelineLayout, nullptr);
		render->worldGenPipelineLayout = VK_NULL_HANDLE;
	}
	if (render->worldGenDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(context->device, render->worldGenDescriptorSetLayout, nullptr);
		render->worldGenDescriptorSetLayout = VK_NULL_HANDLE;
	}
	if (render->worldGenShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context->device, render->worldGenShaderModule, nullptr);
		render->worldGenShaderModule = VK_NULL_HANDLE;
	}
	render->worldGenPipelineEnabled = false;
}

bool RefreshWorldGenResourceBindings(VulkanContextState *context, RenderState *render)
{
	if (!context || !render) {
		return false;
	}
	if (!render->worldGenPipelineEnabled) {
		return false;
	}
	if (render->worldGenDescriptorPool == VK_NULL_HANDLE) {
		VkDescriptorPoolSize poolSizes[1]{};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		poolInfo.poolSizeCount = 1u;
		poolInfo.pPoolSizes = poolSizes;
		if (vkCreateDescriptorPool(context->device, &poolInfo, nullptr, &render->worldGenDescriptorPool) != VK_SUCCESS) {
			return false;
		}
	}

	for (uint32_t frameIndex = 0; frameIndex < static_cast<uint32_t>(render->sceneFrameResources.size()); ++frameIndex) {
		SceneFrameResources &frameResources = render->sceneFrameResources[frameIndex];
		if (frameResources.worldGenVoxelBuffer == VK_NULL_HANDLE) {
			continue;
		}
		if (frameResources.worldGenDescriptorSet == VK_NULL_HANDLE) {
			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = render->worldGenDescriptorPool;
			allocInfo.descriptorSetCount = 1u;
			allocInfo.pSetLayouts = &render->worldGenDescriptorSetLayout;
			if (vkAllocateDescriptorSets(
					context->device,
					&allocInfo,
					&frameResources.worldGenDescriptorSet) != VK_SUCCESS) {
				return false;
			}
		}

		VkDescriptorBufferInfo voxelBufferInfo{};
		voxelBufferInfo.buffer = frameResources.worldGenVoxelBuffer;
		voxelBufferInfo.offset = 0u;
		voxelBufferInfo.range = VK_WHOLE_SIZE;

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = frameResources.worldGenDescriptorSet;
		write.dstBinding = 0u;
		write.dstArrayElement = 0u;
		write.descriptorCount = 1u;
		write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write.pBufferInfo = &voxelBufferInfo;
		vkUpdateDescriptorSets(context->device, 1u, &write, 0u, nullptr);
	}
	return true;
}

uint32_t BuildActiveChunkIdsForWorldGen(
	const VoxelWorld &world,
	std::vector<uint32_t> &outChunkIds)
{
	outChunkIds.clear();
	outChunkIds.reserve(world.chunks.size());
	for (uint32_t chunkIndex = 0; chunkIndex < static_cast<uint32_t>(world.chunks.size()); ++chunkIndex) {
		const VoxelChunk &chunk = world.chunks[chunkIndex];
		if (chunk.min.x >= chunk.maxExclusive.x ||
			chunk.min.y >= chunk.maxExclusive.y ||
			chunk.min.z >= chunk.maxExclusive.z) {
			continue;
		}
		if (chunk.nonAirVoxelCount > 0u) {
			continue;
		}
		outChunkIds.push_back(chunkIndex);
	}
	return static_cast<uint32_t>(outChunkIds.size());
}

bool RecordWorldGenDispatch(
	VkCommandBuffer commandBuffer,
	RenderState &render,
	SceneFrameResources &frameResources,
	const WorldGenPushConstants &pushConstants,
	uint32_t activeChunkCount)
{
	if (commandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (!render.worldGenPipelineEnabled) {
		return false;
	}
	if (frameResources.worldGenVoxelBuffer == VK_NULL_HANDLE ||
		frameResources.worldGenDescriptorSet == VK_NULL_HANDLE) {
		return false;
	}
	if (activeChunkCount == 0u) {
		return true;
	}

	if (frameResources.worldGenVoxelMappedData != nullptr) {
		std::memset(frameResources.worldGenVoxelMappedData, 0, static_cast<size_t>(activeChunkCount) * kWorldGenVoxelBufferBytesPerChunk);
	}

	VkBufferMemoryBarrier2 bufferBarrier{};
	bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	bufferBarrier.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
	bufferBarrier.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT;
	bufferBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	bufferBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	bufferBarrier.buffer = frameResources.worldGenVoxelBuffer;
	bufferBarrier.offset = 0u;
	bufferBarrier.size = VK_WHOLE_SIZE;

	VkDependencyInfo hostToCompute{};
	hostToCompute.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	hostToCompute.bufferMemoryBarrierCount = 1u;
	hostToCompute.pBufferMemoryBarriers = &bufferBarrier;
	vkCmdPipelineBarrier2(commandBuffer, &hostToCompute);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, render.worldGenPipeline);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		render.worldGenPipelineLayout,
		0u,
		1u,
		&frameResources.worldGenDescriptorSet,
		0u,
		nullptr);
	vkCmdPushConstants(
		commandBuffer,
		render.worldGenPipelineLayout,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0u,
		sizeof(WorldGenPushConstants),
		&pushConstants);
	vkCmdDispatch(commandBuffer, activeChunkCount, 1u, 1u);
	return true;
}

}  // namespace projectv::render

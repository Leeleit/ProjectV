#include "render/vulkan/VulkanVoxelMeshingPipeline.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "debug/Profiling.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <array>
#include <vector>

namespace {
constexpr uint32_t kVoxelMeshingDescriptorSetCount = MAX_FRAMES_IN_FLIGHT;
constexpr char kVoxelMeshingShaderFilename[] = "voxel_mesh.comp.spv";
constexpr VkDescriptorPoolSize kVoxelMeshingDescriptorPoolSize{
	.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	.descriptorCount = kVoxelMeshingDescriptorSetCount * 11u,
};
constexpr std::array kVoxelMeshingDescriptorBindings{
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
	VkDescriptorSetLayoutBinding{
		.binding = 5,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 6,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 7,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 8,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 9, // lodDownsampled STORAGE_BUFFER for Stage 4.2 LOD consume path.
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 10, // chunkLodLevelsBuffer STORAGE_BUFFER companion of binding 9.
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = nullptr,
	},
};
constexpr VkDescriptorSetLayoutCreateInfo kVoxelMeshingDescriptorSetLayoutInfo{
	.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	.pNext = nullptr,
	.flags = 0,
	.bindingCount = static_cast<uint32_t>(kVoxelMeshingDescriptorBindings.size()),
	.pBindings = kVoxelMeshingDescriptorBindings.data(),
};

VkShaderModule CreateShaderModule(const VkDevice device, const std::vector<char> &code)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

	VkShaderModule shaderModule = VK_NULL_HANDLE;
	const VkResult shaderModuleResult = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
	if (shaderModuleResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateVoxelMeshingPipeline.vkCreateShaderModule", shaderModuleResult);
		return VK_NULL_HANDLE;
	}

	return shaderModule;
}

void DestroyVoxelMeshingResourceBindings(VulkanContextState &context, RenderState &render)
{
	for (SceneFrameResources &frameResources : render.sceneFrameResources) {
		frameResources.voxelMeshingDescriptorSet = VK_NULL_HANDLE;
	}

	if (render.voxelMeshingDescriptorPool) {
		vkDestroyDescriptorPool(context.device, render.voxelMeshingDescriptorPool, nullptr);
		render.voxelMeshingDescriptorPool = VK_NULL_HANDLE;
	}

	if (render.voxelMeshingDescriptorSetLayout) {
		vkDestroyDescriptorSetLayout(context.device, render.voxelMeshingDescriptorSetLayout, nullptr);
		render.voxelMeshingDescriptorSetLayout = VK_NULL_HANDLE;
	}
}
} // namespace

bool RefreshVoxelMeshingResourceBindings(
	VulkanContextState *context,
	RenderState *render)
{
	PV_PROFILE_ZONE_N("RefreshVoxelMeshingResourceBindings");
	PV_CHECK_OR_RETURN(
		context && render && context->device,
		"Meshing",
		"RefreshVoxelMeshingResourceBindings.Preconditions",
		"context/render/device is incomplete");
	if (!render->voxelMeshingDescriptorSetLayout) {
		return true;
	}

	if (render->voxelMeshingDescriptorPool != VK_NULL_HANDLE) {
		vkResetDescriptorPool(context->device, render->voxelMeshingDescriptorPool, 0u);
	}

	if (render->voxelMeshingDescriptorPool == VK_NULL_HANDLE) {
		constexpr VkDescriptorPoolCreateInfo poolInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.maxSets = kVoxelMeshingDescriptorSetCount,
			.poolSizeCount = 1,
			.pPoolSizes = &kVoxelMeshingDescriptorPoolSize,
		};
		const VkResult descriptorPoolResult =
			vkCreateDescriptorPool(context->device, &poolInfo, nullptr, &render->voxelMeshingDescriptorPool);
		if (descriptorPoolResult != VK_SUCCESS) {
			runtime::LogVkFailure(
				"RefreshVoxelMeshingResourceBindings.vkCreateDescriptorPool",
				descriptorPoolResult);
			return false;
		}
	}

	const std::vector setLayouts(render->sceneFrameResources.size(), render->voxelMeshingDescriptorSetLayout);
	std::vector<VkDescriptorSet> descriptorSets(render->sceneFrameResources.size(), VK_NULL_HANDLE);
	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = render->voxelMeshingDescriptorPool;
	allocateInfo.descriptorSetCount = static_cast<uint32_t>(setLayouts.size());
	allocateInfo.pSetLayouts = setLayouts.data();
	const VkResult allocateDescriptorSetsResult =
		vkAllocateDescriptorSets(context->device, &allocateInfo, descriptorSets.data());
	if (allocateDescriptorSetsResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"RefreshVoxelMeshingResourceBindings.vkAllocateDescriptorSets",
			allocateDescriptorSetsResult);
		vkDestroyDescriptorPool(context->device, render->voxelMeshingDescriptorPool, nullptr);
		render->voxelMeshingDescriptorPool = VK_NULL_HANDLE;
		return false;
	}

	for (size_t frameIndex = 0; frameIndex < render->sceneFrameResources.size(); ++frameIndex) {
		SceneFrameResources &frameResources = render->sceneFrameResources[frameIndex];
		frameResources.voxelMeshingDescriptorSet = descriptorSets[frameIndex];

		const VkDescriptorBufferInfo chunkDescriptorBufferInfo{
			.buffer = frameResources.chunkDescriptorBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo chunkVoxelPayloadBufferInfo{
			.buffer = frameResources.chunkVoxelPayloadBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo packedFaceBufferInfo{
			.buffer = frameResources.packedFaceBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo dirtyChunkIndexBufferInfo{
			.buffer = frameResources.dirtyChunkIndexBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo opaqueIndirectBufferInfo{
			.buffer = frameResources.opaqueIndirectBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo transparentIndirectBufferInfo{
			.buffer = frameResources.transparentIndirectBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo chunkCullingBufferInfo{
			.buffer = frameResources.chunkCullingBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo sceneLightingBufferInfo{
			.buffer = frameResources.sceneLightingBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo lodDownsampledVoxelPayloadBufferInfo{
			.buffer = frameResources.lodDownsampledVoxelPayloadBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo chunkLodLevelsBufferInfo{
			.buffer = frameResources.chunkLodLevelsBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};

		const std::array descriptorWrites{
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.voxelMeshingDescriptorSet,
				.dstBinding = 0,
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
				.dstSet = frameResources.voxelMeshingDescriptorSet,
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &chunkVoxelPayloadBufferInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.voxelMeshingDescriptorSet,
				.dstBinding = 2,
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
				.dstSet = frameResources.voxelMeshingDescriptorSet,
				.dstBinding = 3,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &dirtyChunkIndexBufferInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.voxelMeshingDescriptorSet,
				.dstBinding = 4,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &opaqueIndirectBufferInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.voxelMeshingDescriptorSet,
				.dstBinding = 5,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &transparentIndirectBufferInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.voxelMeshingDescriptorSet,
				.dstBinding = 6,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &chunkCullingBufferInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.voxelMeshingDescriptorSet,
				.dstBinding = 7,
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
				.dstSet = frameResources.voxelMeshingDescriptorSet,
				.dstBinding = 8,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &lodDownsampledVoxelPayloadBufferInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.voxelMeshingDescriptorSet,
				.dstBinding = 9,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &chunkLodLevelsBufferInfo,
				.pTexelBufferView = nullptr,
			},
		};
		vkUpdateDescriptorSets(
			context->device,
			static_cast<uint32_t>(descriptorWrites.size()),
			descriptorWrites.data(),
			0,
			nullptr);
	}

	return true;
}

void DestroyVoxelMeshingPipeline(
	VulkanContextState *context,
	RenderState *render)
{
	PV_PROFILE_ZONE_N("DestroyVoxelMeshingPipeline");
	if (!context || !render || !context->device) {
		return;
	}

	DestroyVoxelMeshingResourceBindings(*context, *render);

	if (render->voxelMeshingPipeline) {
		vkDestroyPipeline(context->device, render->voxelMeshingPipeline, nullptr);
		render->voxelMeshingPipeline = VK_NULL_HANDLE;
	}

	if (render->voxelMeshingPipelineLayout) {
		vkDestroyPipelineLayout(context->device, render->voxelMeshingPipelineLayout, nullptr);
		render->voxelMeshingPipelineLayout = VK_NULL_HANDLE;
	}
}

bool CreateVoxelMeshingPipeline(
	VulkanContextState *context,
	RenderState *render)
{
	PV_PROFILE_ZONE_N("CreateVoxelMeshingPipeline");
	PV_CHECK_OR_RETURN(
		context && render && context->device,
		"Meshing",
		"CreateVoxelMeshingPipeline.Preconditions",
		"context/render/device is incomplete");

	DestroyVoxelMeshingPipeline(context, render);

	const std::vector<char> meshingShaderCode = ReadShaderFile(kVoxelMeshingShaderFilename);
	if (meshingShaderCode.empty()) {
		DestroyVoxelMeshingPipeline(context, render);
		return false;
	}

	const VkShaderModule meshingShaderModule = CreateShaderModule(context->device, meshingShaderCode);
	if (!meshingShaderModule) {
		runtime::LogRuntimeFailure(
			"Meshing",
			"CreateVoxelMeshingPipeline.CreateShaderModule",
			"shader module creation returned null");
		DestroyVoxelMeshingPipeline(context, render);
		return false;
	}

	const VkResult descriptorSetLayoutResult = vkCreateDescriptorSetLayout(
		context->device,
		&kVoxelMeshingDescriptorSetLayoutInfo,
		nullptr,
		&render->voxelMeshingDescriptorSetLayout);
	if (descriptorSetLayoutResult != VK_SUCCESS) {
		runtime::LogVkFailure(
			"CreateVoxelMeshingPipeline.vkCreateDescriptorSetLayout",
			descriptorSetLayoutResult);
		vkDestroyShaderModule(context->device, meshingShaderModule, nullptr);
		DestroyVoxelMeshingPipeline(context, render);
		return false;
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->voxelMeshingDescriptorSetLayout),
		VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
		"VoxelMeshingDescriptorSetLayout");

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(VoxelMeshingPushConstants);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &render->voxelMeshingDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
	const VkResult pipelineLayoutResult =
		vkCreatePipelineLayout(context->device, &pipelineLayoutInfo, nullptr, &render->voxelMeshingPipelineLayout);
	if (pipelineLayoutResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateVoxelMeshingPipeline.vkCreatePipelineLayout", pipelineLayoutResult);
		vkDestroyShaderModule(context->device, meshingShaderModule, nullptr);
		DestroyVoxelMeshingPipeline(context, render);
		return false;
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->voxelMeshingPipelineLayout),
		VK_OBJECT_TYPE_PIPELINE_LAYOUT,
		"VoxelMeshingPipelineLayout");

	const VkPipelineShaderStageCreateInfo meshingShaderStage{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = meshingShaderModule,
		.pName = "main",
		.pSpecializationInfo = nullptr,
	};
	const VkComputePipelineCreateInfo meshingPipelineInfo{
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stage = meshingShaderStage,
		.layout = render->voxelMeshingPipelineLayout,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = -1,
	};

	const VkResult meshingPipelineResult = vkCreateComputePipelines(
		context->device,
		context->pipelineCache,
		1,
		&meshingPipelineInfo,
		nullptr,
		&render->voxelMeshingPipeline);
	if (meshingPipelineResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateVoxelMeshingPipeline.vkCreateComputePipelines", meshingPipelineResult);
		vkDestroyShaderModule(context->device, meshingShaderModule, nullptr);
		DestroyVoxelMeshingPipeline(context, render);
		return false;
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->voxelMeshingPipeline),
		VK_OBJECT_TYPE_PIPELINE,
		"VoxelMeshingPipeline");

	if (!RefreshVoxelMeshingResourceBindings(context, render)) {
		runtime::LogRuntimeFailure(
			"Meshing",
			"CreateVoxelMeshingPipeline.RefreshVoxelMeshingResourceBindings",
			"descriptor rebinding returned false");
		vkDestroyShaderModule(context->device, meshingShaderModule, nullptr);
		DestroyVoxelMeshingPipeline(context, render);
		return false;
	}

	vkDestroyShaderModule(context->device, meshingShaderModule, nullptr);
	return true;
}

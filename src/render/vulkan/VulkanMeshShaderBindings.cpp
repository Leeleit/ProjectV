#include "render/vulkan/VulkanMeshShaderPipeline.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <array>
#include <vector>

namespace {
constexpr uint32_t kMeshShaderDescriptorSetCount = MAX_FRAMES_IN_FLIGHT;

constexpr std::array kMeshShaderDescriptorPoolSizes{
	VkDescriptorPoolSize{
		.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = kMeshShaderDescriptorSetCount * 4u,
	},
};
} // namespace

namespace projectv::render {
bool RefreshMeshShaderResourceBindings(
	VulkanContextState *context,
	RenderState *render)
{
	PV_PROFILE_ZONE_N("RefreshMeshShaderResourceBindings");
	PV_CHECK_OR_RETURN(
		context && render && context->device,
		"Render",
		"RefreshMeshShaderResourceBindings.Preconditions",
		"context/render/device is incomplete");
	if (!render->meshShaderDescriptorSetLayout) {
		return true;
	}
	if (render->meshShaderEnabled == false) {
		return true;
	}

	if (render->meshShaderDescriptorPool != VK_NULL_HANDLE) {
		vkResetDescriptorPool(context->device, render->meshShaderDescriptorPool, 0u);
	}

	if (render->meshShaderDescriptorPool == VK_NULL_HANDLE) {
		static constexpr VkDescriptorPoolCreateInfo poolInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.maxSets = kMeshShaderDescriptorSetCount,
			.poolSizeCount = static_cast<uint32_t>(kMeshShaderDescriptorPoolSizes.size()),
			.pPoolSizes = kMeshShaderDescriptorPoolSizes.data(),
		};
		const VkResult poolResult = vkCreateDescriptorPool(
			context->device,
			&poolInfo,
			nullptr,
			&render->meshShaderDescriptorPool);
		if (poolResult != VK_SUCCESS) {
			runtime::LogVkFailure("RefreshMeshShaderResourceBindings.vkCreateDescriptorPool", poolResult);
			return false;
		}
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(render->meshShaderDescriptorPool),
			VK_OBJECT_TYPE_DESCRIPTOR_POOL,
			"MeshShaderDescriptorPool");
	}

	const std::vector setLayouts(render->sceneFrameResources.size(), render->meshShaderDescriptorSetLayout);
	std::vector<VkDescriptorSet> descriptorSets(render->sceneFrameResources.size(), VK_NULL_HANDLE);
	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = render->meshShaderDescriptorPool;
	allocateInfo.descriptorSetCount = static_cast<uint32_t>(setLayouts.size());
	allocateInfo.pSetLayouts = setLayouts.data();
	const VkResult allocResult = vkAllocateDescriptorSets(
		context->device,
		&allocateInfo,
		descriptorSets.data());
	if (allocResult != VK_SUCCESS) {
		runtime::LogVkFailure("RefreshMeshShaderResourceBindings.vkAllocateDescriptorSets", allocResult);
		vkDestroyDescriptorPool(context->device, render->meshShaderDescriptorPool, nullptr);
		render->meshShaderDescriptorPool = VK_NULL_HANDLE;
		return false;
	}

	for (size_t frameIndex = 0; frameIndex < render->sceneFrameResources.size(); ++frameIndex) {
		SceneFrameResources &frameResources = render->sceneFrameResources[frameIndex];
		frameResources.meshShaderDescriptorSet = descriptorSets[frameIndex];

		const VkDescriptorBufferInfo chunkDescriptorInfo{
			.buffer = frameResources.chunkDescriptorBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo chunkVoxelPayloadInfo{
			.buffer = frameResources.chunkVoxelPayloadBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo visibleChunkIdInfo{
			.buffer = frameResources.visibleChunkIdBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo visibilityCounterInfo{
			.buffer = frameResources.visibilityCounterBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};

		const std::array writes{
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.meshShaderDescriptorSet,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &chunkDescriptorInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.meshShaderDescriptorSet,
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &chunkVoxelPayloadInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.meshShaderDescriptorSet,
				.dstBinding = 2,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &visibleChunkIdInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.meshShaderDescriptorSet,
				.dstBinding = 3,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &visibilityCounterInfo,
				.pTexelBufferView = nullptr,
			},
		};
		vkUpdateDescriptorSets(
			context->device,
			static_cast<uint32_t>(writes.size()),
			writes.data(),
			0u,
			nullptr);
	}

	return true;
}
} // namespace projectv::render

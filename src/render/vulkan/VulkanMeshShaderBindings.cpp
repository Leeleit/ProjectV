#include "render/vulkan/VulkanMeshShaderPipeline.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <array>
#include <vector>

namespace {
constexpr uint32_t kMeshShaderDescriptorSetCount = MAX_FRAMES_IN_FLIGHT;

constexpr std::array kMeshVisibilityPoolSizes{
	VkDescriptorPoolSize{
		.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = kMeshShaderDescriptorSetCount * 16u,
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
	if (!render->meshShaderDescriptorSetLayout || !render->meshClusterizeDescriptorSetLayout) {
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
			.maxSets = kMeshShaderDescriptorSetCount * 2u,
			.poolSizeCount = static_cast<uint32_t>(kMeshVisibilityPoolSizes.size()),
			.pPoolSizes = kMeshVisibilityPoolSizes.data(),
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
	}

	std::vector<VkDescriptorSetLayout> setLayouts;
	setLayouts.reserve(render->sceneFrameResources.size() * 2u);
	for (size_t i = 0; i < render->sceneFrameResources.size(); ++i) {
		setLayouts.push_back(render->meshShaderDescriptorSetLayout);
	}
	for (size_t i = 0; i < render->sceneFrameResources.size(); ++i) {
		setLayouts.push_back(render->meshClusterizeDescriptorSetLayout);
	}

	std::vector<VkDescriptorSet> descriptorSets(setLayouts.size(), VK_NULL_HANDLE);
	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = render->meshShaderDescriptorPool;
	allocateInfo.descriptorSetCount = static_cast<uint32_t>(setLayouts.size());
	allocateInfo.pSetLayouts = setLayouts.data();
	const VkResult allocResult = vkAllocateDescriptorSets(context->device, &allocateInfo, descriptorSets.data());
	if (allocResult != VK_SUCCESS) {
		runtime::LogVkFailure("RefreshMeshShaderResourceBindings.vkAllocateDescriptorSets", allocResult);
		return false;
	}

	const size_t frameCount = render->sceneFrameResources.size();
	for (size_t frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
		SceneFrameResources &frameResources = render->sceneFrameResources[frameIndex];
		frameResources.meshShaderDescriptorSet = descriptorSets[frameIndex];
		frameResources.meshClusters.clusterizeDescriptorSet = descriptorSets[frameCount + frameIndex];
		if (frameResources.meshClusters.faceClusterBuffer == VK_NULL_HANDLE) {
			continue;
		}

		const VkDescriptorBufferInfo faceClusterInfo{.buffer = frameResources.meshClusters.faceClusterBuffer, .offset = 0, .range = VK_WHOLE_SIZE};
		const VkDescriptorBufferInfo visibleClusterIdInfo{.buffer = frameResources.visibleChunkIdBuffer, .offset = 0, .range = VK_WHOLE_SIZE};
		const VkDescriptorBufferInfo faceClusterCountInfo{.buffer = frameResources.meshClusters.faceClusterCountBuffer, .offset = 0, .range = VK_WHOLE_SIZE};
		const VkDescriptorBufferInfo meshDrawIndirectInfo{.buffer = frameResources.meshClusters.meshDrawIndirectBuffer, .offset = 0, .range = VK_WHOLE_SIZE};
		const VkDescriptorBufferInfo chunkDescriptorInfo{.buffer = frameResources.chunkDescriptorBuffer, .offset = 0, .range = VK_WHOLE_SIZE};

		const std::array visibilityWrites{
			VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = frameResources.meshShaderDescriptorSet, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &faceClusterInfo},
			VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = frameResources.meshShaderDescriptorSet, .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &visibleClusterIdInfo},
			VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = frameResources.meshShaderDescriptorSet, .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &faceClusterCountInfo},
			VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = frameResources.meshShaderDescriptorSet, .dstBinding = 3, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &meshDrawIndirectInfo},
		};
		vkUpdateDescriptorSets(context->device, static_cast<uint32_t>(visibilityWrites.size()), visibilityWrites.data(), 0u, nullptr);

		const std::array clusterizeWrites{
			VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = frameResources.meshClusters.clusterizeDescriptorSet, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &chunkDescriptorInfo},
			VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = frameResources.meshClusters.clusterizeDescriptorSet, .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &faceClusterInfo},
			VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = frameResources.meshClusters.clusterizeDescriptorSet, .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &faceClusterCountInfo},
		};
		vkUpdateDescriptorSets(context->device, static_cast<uint32_t>(clusterizeWrites.size()), clusterizeWrites.data(), 0u, nullptr);
	}

	return true;
}
} // namespace projectv::render

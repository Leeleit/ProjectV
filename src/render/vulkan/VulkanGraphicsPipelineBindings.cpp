#include "render/vulkan/VulkanGraphicsPipeline.hpp"
#include "render/vulkan/VulkanGraphicsPipelineInternal.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"
#include "render/RayTracedShadows.hpp"
#include "render/RtxGiProbes.hpp"

#include <array>
#include <vector>

namespace {
constexpr uint32_t kGraphicsDescriptorSetCount = MAX_FRAMES_IN_FLIGHT;
constexpr VkDescriptorPoolSize kGraphicsStorageDescriptorPoolSize{
	.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	.descriptorCount = kGraphicsDescriptorSetCount * 7u,
};
constexpr VkDescriptorPoolSize kGraphicsCombinedImageSamplerDescriptorPoolSize{
	.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	.descriptorCount = kGraphicsDescriptorSetCount * 8u,
};
constexpr VkDescriptorPoolSize kGraphicsAccelerationStructureDescriptorPoolSize{
	.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
	.descriptorCount = kGraphicsDescriptorSetCount * 1u,
};
constexpr std::array kGraphicsDescriptorPoolSizes{
	kGraphicsStorageDescriptorPoolSize,
	kGraphicsCombinedImageSamplerDescriptorPoolSize,
	kGraphicsAccelerationStructureDescriptorPoolSize,
};
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

	const bool rtxLayoutActive = context->rayTracing.rayQuery
		&& context->rayTracing.accelerationStructure
		&& projectv::render::IsRayTracedShadowEnabled(*context);
	std::vector<VkDescriptorPoolSize> poolSizes{};
	poolSizes.reserve(kGraphicsDescriptorPoolSizes.size());
	for (const VkDescriptorPoolSize &size : kGraphicsDescriptorPoolSizes) {
		if (size.type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR && !rtxLayoutActive) {
			continue;
		}
		poolSizes.push_back(size);
	}
	const VkDescriptorPoolCreateInfo poolInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.maxSets = kGraphicsDescriptorSetCount,
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data(),
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
		vkAllocateDescriptorSets(context->device, &allocateInfo, descriptorSets.data());
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

		const VkDescriptorImageInfo vctClipmapImageInfo{
			.sampler = render->vctClipmapSampler,
			.imageView = render->vctClipmapView != VK_NULL_HANDLE
							 ? render->vctClipmapView
							 : render->volumetricFogFallbackView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};

		const VkDescriptorImageInfo volumetricFogImageInfo{
			.sampler = render->volumetricFogLinearSampler,
			.imageView = render->volumetricFogFroxelView != VK_NULL_HANDLE
							 ? render->volumetricFogFroxelView
							 : render->volumetricFogFallbackView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};

		const bool rtxGiActive = render->rtxGiProbes != nullptr
			&& render->rtxGiProbes->IsEnabled();
		const VkDescriptorImageInfo rtxGiIrradianceImageInfo{
			.sampler = rtxGiActive ? render->rtxGiProbes->GetConfig().irradianceSampler : VK_NULL_HANDLE,
			.imageView = rtxGiActive ? render->rtxGiProbes->GetConfig().irradianceView : VK_NULL_HANDLE,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};
		const VkDescriptorImageInfo rtxGiDistanceImageInfo{
			.sampler = rtxGiActive ? render->rtxGiProbes->GetConfig().irradianceSampler : VK_NULL_HANDLE,
			.imageView = rtxGiActive ? render->rtxGiProbes->GetConfig().distanceView : VK_NULL_HANDLE,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};
		const VkDescriptorImageInfo rtxGiProbeDataImageInfo{
			.sampler = rtxGiActive ? render->rtxGiProbes->GetConfig().irradianceSampler : VK_NULL_HANDLE,
			.imageView = rtxGiActive ? render->rtxGiProbes->GetConfig().probeDataView : VK_NULL_HANDLE,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};
		const VkDescriptorBufferInfo rtxGiVolumeDescBufferInfo{
			.buffer = rtxGiActive ? render->rtxGiProbes->GetConfig().volumeDescBuffer : VK_NULL_HANDLE,
			.offset = 0,
			.range = rtxGiActive ? sizeof(float) * 16u : 0u,
		};
		std::vector<VkWriteDescriptorSet> descriptorWrites{};
		descriptorWrites.reserve(16u);
		descriptorWrites.push_back(VkWriteDescriptorSet{
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
		});
		descriptorWrites.push_back(VkWriteDescriptorSet{
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
		});
		descriptorWrites.push_back(VkWriteDescriptorSet{
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
		});
		descriptorWrites.push_back(VkWriteDescriptorSet{
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
		});
		descriptorWrites.push_back(VkWriteDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = frameResources.graphicsDescriptorSet,
			.dstBinding = 4,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pImageInfo = nullptr,
			.pBufferInfo = &chunkVoxelPayloadBufferInfo,
			.pTexelBufferView = nullptr,
		});

		descriptorWrites.push_back(VkWriteDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = frameResources.graphicsDescriptorSet,
			.dstBinding = 11,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &vctClipmapImageInfo,
			.pBufferInfo = nullptr,
			.pTexelBufferView = nullptr,
		});
		descriptorWrites.push_back(VkWriteDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = frameResources.graphicsDescriptorSet,
			.dstBinding = 12,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &volumetricFogImageInfo,
			.pBufferInfo = nullptr,
			.pTexelBufferView = nullptr,
		});
		if (rtxGiActive) {
			descriptorWrites.push_back(VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.graphicsDescriptorSet,
				.dstBinding = 14,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &rtxGiIrradianceImageInfo,
				.pBufferInfo = nullptr,
				.pTexelBufferView = nullptr,
			});
			descriptorWrites.push_back(VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.graphicsDescriptorSet,
				.dstBinding = 15,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &rtxGiDistanceImageInfo,
				.pBufferInfo = nullptr,
				.pTexelBufferView = nullptr,
			});
			descriptorWrites.push_back(VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.graphicsDescriptorSet,
				.dstBinding = 16,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &rtxGiProbeDataImageInfo,
				.pBufferInfo = nullptr,
				.pTexelBufferView = nullptr,
			});
			descriptorWrites.push_back(VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.graphicsDescriptorSet,
				.dstBinding = 17,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &rtxGiVolumeDescBufferInfo,
				.pTexelBufferView = nullptr,
			});
		}

		const bool rtxActive = render->rayTracedShadows != nullptr
			&& render->rayTracedShadows->IsEnabled()
			&& render->rayTracedShadows->GetConfig().tlas != VK_NULL_HANDLE;
		if (rtxActive) {
			VkWriteDescriptorSet tlasWrite{};
			tlasWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			tlasWrite.pNext = nullptr;
			tlasWrite.dstSet = frameResources.graphicsDescriptorSet;
			tlasWrite.dstBinding = 13;
			tlasWrite.dstArrayElement = 0;
			tlasWrite.descriptorCount = 1;
			tlasWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
			tlasWrite.pImageInfo = nullptr;
			tlasWrite.pBufferInfo = nullptr;
			tlasWrite.pTexelBufferView = nullptr;
			VkAccelerationStructureKHR tlasHandle = render->rayTracedShadows->GetConfig().tlas;
			const VkWriteDescriptorSetAccelerationStructureKHR tlasWriteInfo{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
				.pNext = nullptr,
				.accelerationStructureCount = 1u,
				.pAccelerationStructures = &tlasHandle,
			};
			tlasWrite.pNext = &tlasWriteInfo;
			descriptorWrites.push_back(tlasWrite);
		}

		const VkImageView shadowMaskView =
			(render->rayTracedShadows != nullptr && render->rayTracedShadows->IsVoxelAwareRtxActive())
				? render->rayTracedShadows->GetShadowMaskImageView()
				: render->rtxShadowMaskFallbackView;
		if (shadowMaskView != VK_NULL_HANDLE) {
			const VkDescriptorImageInfo shadowMaskImageInfo{
				.sampler = render->vctClipmapSampler,
				.imageView = shadowMaskView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			};
			descriptorWrites.push_back(VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.graphicsDescriptorSet,
				.dstBinding = 18,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &shadowMaskImageInfo,
				.pBufferInfo = nullptr,
				.pTexelBufferView = nullptr,
			});
		}
		vkUpdateDescriptorSets(
			context->device,
			static_cast<uint32_t>(descriptorWrites.size()),
			descriptorWrites.data(),
			0,
			nullptr);
	}

	// CSM removed per TODO.md §5.2.D (session 20x). RTX shadows use the
	// graphics descriptor set (binding 13 = rtxTlas) instead of a separate
	// shadow descriptor set. The remaining code in this function handles
	// only the graphics descriptor set.

	return true;
}
#include "volk.h"
#include "render/HizCulling.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/Types.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace projectv::render {
void BuildHizMipChain(
	VkCommandBuffer commandBuffer,
	VkImage depthImage,
	const VkImageLayout depthImageLayout,
	const HizBuffer &hizBuffer,
	RenderState *render,
	VulkanContextState *context)
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

	const bool useDepthReduce =
		context != nullptr &&
		context->device != VK_NULL_HANDLE &&
		render != nullptr &&
		render->hizMinifyEnabled &&
		IsHzbMinMipEnabled() &&
		render->hizMinifyPipeline != VK_NULL_HANDLE &&
		hizBuffer.mipStorageViewCount >= mipLevels &&
		(render->hizMinifyUsesPushDescriptors ||
		 render->hizMinifyDescriptorSetCount >= mipLevels);
	if (!useDepthReduce) {
		return;
	}
	const VkImageView depthImageView =
		depthImage == render->depthResolveImage
			? render->depthResolveImageView
			: render->depthImageView;
	if (depthImageView == VK_NULL_HANDLE) {
		return;
	}

	if (!render->hizMinifyUsesPushDescriptors &&
		!render->hizMinifyDescriptorsInitialized) {
		std::array<VkDescriptorImageInfo, 16> sourceInfos{};
		std::array<VkDescriptorImageInfo, 16> samplerInfos{};
		std::array<VkDescriptorImageInfo, 16> destinationInfos{};
		std::array<VkWriteDescriptorSet, static_cast<std::size_t>(16u) * 3u> writes{};
		for (uint32_t mipLevel = 0u; mipLevel < mipLevels; ++mipLevel) {
			sourceInfos[mipLevel] = {
				.sampler = VK_NULL_HANDLE,
				.imageView = mipLevel == 0u ? depthImageView : hizBuffer.mipStorageViews[mipLevel - 1u],
				.imageLayout = mipLevel == 0u
								   ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
								   : VK_IMAGE_LAYOUT_GENERAL};
			samplerInfos[mipLevel] = {
				.sampler = hizBuffer.sampler,
				.imageView = VK_NULL_HANDLE,
				.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED};
			destinationInfos[mipLevel] = {
				.sampler = VK_NULL_HANDLE,
				.imageView = hizBuffer.mipStorageViews[mipLevel],
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL};

			const uint32_t writeIndex = mipLevel * 3u;
			writes[writeIndex] = {
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = render->hizMinifyDescriptorSets[mipLevel],
				.dstBinding = 0u,
				.descriptorCount = 1u,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.pImageInfo = &sourceInfos[mipLevel]};
			writes[writeIndex + 1u] = {
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = render->hizMinifyDescriptorSets[mipLevel],
				.dstBinding = 1u,
				.descriptorCount = 1u,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
				.pImageInfo = &samplerInfos[mipLevel]};
			writes[writeIndex + 2u] = {
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = render->hizMinifyDescriptorSets[mipLevel],
				.dstBinding = 2u,
				.descriptorCount = 1u,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.pImageInfo = &destinationInfos[mipLevel]};
		}
		vkUpdateDescriptorSets(
			context->device,
			mipLevels * 3u,
			writes.data(),
			0u,
			nullptr);
		render->hizMinifyDescriptorsInitialized = true;
	}

	VkImageMemoryBarrier2 barriers[2]{};
	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barriers[0].srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	barriers[0].srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	barriers[0].dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	barriers[0].oldLayout = depthImageLayout;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
	barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].image = depthImage;
	barriers[0].subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u};

	barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barriers[1].srcStageMask = render->hizBufferNeedsInit
								   ? VK_PIPELINE_STAGE_2_NONE
								   : VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	barriers[1].srcAccessMask = render->hizBufferNeedsInit
									? VK_ACCESS_2_NONE
									: VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	barriers[1].dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	barriers[1].dstAccessMask =
		VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	barriers[1].oldLayout = render->hizBufferNeedsInit
								? VK_IMAGE_LAYOUT_UNDEFINED
								: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].image = hizBuffer.image;
	barriers[1].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, mipLevels, 0u, 1u};

	VkDependencyInfo barrierDep{};
	barrierDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	barrierDep.imageMemoryBarrierCount = 2u;
	barrierDep.pImageMemoryBarriers = barriers;
	vkCmdPipelineBarrier2(commandBuffer, &barrierDep);

	uint32_t srcWidth = hizBuffer.baseWidth;
	uint32_t srcHeight = hizBuffer.baseHeight;

	for (uint32_t mipLevel = 0u; mipLevel < mipLevels; ++mipLevel) {
		const uint32_t dstWidth =
			mipLevel == 0u ? srcWidth : std::max(1u, srcWidth >> 1u);
		const uint32_t dstHeight =
			mipLevel == 0u ? srcHeight : std::max(1u, srcHeight >> 1u);

		if (mipLevel > 0u) {
			VkImageMemoryBarrier2 sourceBarrier{};
			sourceBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			sourceBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
			sourceBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
			sourceBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
			sourceBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
			sourceBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			sourceBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			sourceBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			sourceBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			sourceBarrier.image = hizBuffer.image;
			sourceBarrier.subresourceRange = {
				VK_IMAGE_ASPECT_COLOR_BIT,
				mipLevel - 1u,
				1u,
				0u,
				1u,
			};
			VkDependencyInfo sourceDep{};
			sourceDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			sourceDep.imageMemoryBarrierCount = 1u;
			sourceDep.pImageMemoryBarriers = &sourceBarrier;
			vkCmdPipelineBarrier2(commandBuffer, &sourceDep);
		}

		const VkDescriptorImageInfo srcInfo{
			.sampler = VK_NULL_HANDLE,
			.imageView =
				mipLevel == 0u ? depthImageView : hizBuffer.mipStorageViews[mipLevel - 1u],
			.imageLayout =
				mipLevel == 0u
					? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
					: VK_IMAGE_LAYOUT_GENERAL};
		const VkDescriptorImageInfo samplerInfo{
			.sampler = hizBuffer.sampler,
			.imageView = VK_NULL_HANDLE,
			.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED};
		const VkDescriptorImageInfo dstInfo{
			.sampler = VK_NULL_HANDLE,
			.imageView = hizBuffer.mipStorageViews[mipLevel],
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL};
		std::array<VkWriteDescriptorSet, 3> writes{};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstBinding = 0u;
		writes[0].descriptorCount = 1u;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		writes[0].pImageInfo = &srcInfo;
		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstBinding = 1u;
		writes[1].descriptorCount = 1u;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		writes[1].pImageInfo = &samplerInfo;
		writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2].dstBinding = 2u;
		writes[2].descriptorCount = 1u;
		writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[2].pImageInfo = &dstInfo;

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, render->hizMinifyPipeline);
		if (render->hizMinifyUsesPushDescriptors) {
			vkCmdPushDescriptorSet(
				commandBuffer,
				VK_PIPELINE_BIND_POINT_COMPUTE,
				render->hizMinifyPipelineLayout,
				0u,
				static_cast<uint32_t>(writes.size()),
				writes.data());
		} else {
			const VkDescriptorSet descriptorSet = render->hizMinifyDescriptorSets[mipLevel];
			vkCmdBindDescriptorSets(
				commandBuffer,
				VK_PIPELINE_BIND_POINT_COMPUTE,
				render->hizMinifyPipelineLayout,
				0u,
				1u,
				&descriptorSet,
				0u,
				nullptr);
		}
		const HizMinifyPushConstants pushConstants{
			.copySourceAndPadding = {mipLevel == 0u ? 1u : 0u, 0u, 0u, 0u},
		};
		vkCmdPushConstants(
			commandBuffer,
			render->hizMinifyPipelineLayout,
			VK_SHADER_STAGE_COMPUTE_BIT,
			0u,
			sizeof(HizMinifyPushConstants),
			&pushConstants);
		const uint32_t groupsX = (dstWidth + 7u) / 8u;
		const uint32_t groupsY = (dstHeight + 7u) / 8u;
		vkCmdDispatch(commandBuffer, groupsX, groupsY, 1u);

		srcWidth = dstWidth;
		srcHeight = dstHeight;
	}

	VkImageMemoryBarrier2 finalBarrier{};
	finalBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	finalBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	finalBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	finalBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	finalBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	finalBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	finalBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	finalBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	finalBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	finalBarrier.image = hizBuffer.image;
	finalBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, mipLevels, 0u, 1u};
	VkDependencyInfo finalDep{};
	finalDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	finalDep.imageMemoryBarrierCount = 1u;
	finalDep.pImageMemoryBarriers = &finalBarrier;
	vkCmdPipelineBarrier2(commandBuffer, &finalDep);
	VkImageMemoryBarrier2 restoreDepth{};
	restoreDepth.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	restoreDepth.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	restoreDepth.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	restoreDepth.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	restoreDepth.dstAccessMask =
		VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
		VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	restoreDepth.oldLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
	restoreDepth.newLayout = depthImageLayout;
	restoreDepth.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	restoreDepth.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	restoreDepth.image = depthImage;
	restoreDepth.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u};
	VkDependencyInfo restoreDep{};
	restoreDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	restoreDep.imageMemoryBarrierCount = 1u;
	restoreDep.pImageMemoryBarriers = &restoreDepth;
	vkCmdPipelineBarrier2(commandBuffer, &restoreDep);
	render->hizBufferNeedsInit = false;
}
} // namespace projectv::render

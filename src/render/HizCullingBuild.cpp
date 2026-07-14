#include "render/HizCulling.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"

#include <algorithm>

namespace projectv::render {
void BuildHizMipChain(
	VkCommandBuffer commandBuffer,
	VkImage depthImage,
	const VkImageLayout depthImageLayout,
	const HizBuffer &hizBuffer)
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

	VkImageMemoryBarrier2 barriers[2]{};
	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barriers[0].srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	barriers[0].srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	barriers[0].dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	barriers[0].dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
	barriers[0].oldLayout = depthImageLayout;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].image = depthImage;
	barriers[0].subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u};

	barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barriers[1].srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	barriers[1].srcAccessMask = VK_ACCESS_2_NONE;
	barriers[1].dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	barriers[1].dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].image = hizBuffer.image;
	barriers[1].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};

	VkDependencyInfo barrierDep{};
	barrierDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	barrierDep.imageMemoryBarrierCount = 2u;
	barrierDep.pImageMemoryBarriers = barriers;
	vkCmdPipelineBarrier2(commandBuffer, &barrierDep);

	VkImageBlit blit{};
	blit.srcSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
	blit.srcOffsets[0] = {0, 0, 0};
	blit.srcOffsets[1] = {
		static_cast<int32_t>(hizBuffer.baseWidth),
		static_cast<int32_t>(hizBuffer.baseHeight),
		1,
	};
	blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
	blit.dstOffsets[0] = {0, 0, 0};
	blit.dstOffsets[1] = {
		static_cast<int32_t>(hizBuffer.baseWidth),
		static_cast<int32_t>(hizBuffer.baseHeight),
		1,
	};
	vkCmdBlitImage(
		commandBuffer,
		depthImage,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		hizBuffer.image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1u,
		&blit,
		VK_FILTER_NEAREST);

	uint32_t srcWidth = hizBuffer.baseWidth;
	uint32_t srcHeight = hizBuffer.baseHeight;
	for (uint32_t mipLevel = 1u; mipLevel < mipLevels; ++mipLevel) {
		const uint32_t dstWidth = std::max(1u, srcWidth >> 1u);
		const uint32_t dstHeight = std::max(1u, srcHeight >> 1u);

		VkImageMemoryBarrier2 mipBarrier{};
		mipBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		mipBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		mipBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		mipBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		mipBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
		mipBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		mipBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		mipBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		mipBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		mipBarrier.image = hizBuffer.image;
		mipBarrier.subresourceRange = {
			VK_IMAGE_ASPECT_COLOR_BIT,
			mipLevel - 1u,
			1u,
			0u,
			1u,
		};
		VkDependencyInfo mipDep{};
		mipDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		mipDep.imageMemoryBarrierCount = 1u;
		mipDep.pImageMemoryBarriers = &mipBarrier;
		vkCmdPipelineBarrier2(commandBuffer, &mipDep);

		VkImageBlit mipBlit{};
		mipBlit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevel - 1u, 0u, 1u};
		mipBlit.srcOffsets[0] = {0, 0, 0};
		mipBlit.srcOffsets[1] = {
			static_cast<int32_t>(srcWidth),
			static_cast<int32_t>(srcHeight),
			1,
		};
		mipBlit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 0u, 1u};
		mipBlit.dstOffsets[0] = {0, 0, 0};
		mipBlit.dstOffsets[1] = {
			static_cast<int32_t>(dstWidth),
			static_cast<int32_t>(dstHeight),
			1,
		};
		vkCmdBlitImage(
			commandBuffer,
			hizBuffer.image,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			hizBuffer.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1u,
			&mipBlit,
			VK_FILTER_LINEAR);

		srcWidth = dstWidth;
		srcHeight = dstHeight;
	}

	VkImageMemoryBarrier2 finalBarriers[2]{};
	finalBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	finalBarriers[0].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	finalBarriers[0].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	finalBarriers[0].dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	finalBarriers[0].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
	finalBarriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	finalBarriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	finalBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	finalBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	finalBarriers[0].image = hizBuffer.image;
	finalBarriers[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevels - 1u, 1u, 0u, 1u};

	if (mipLevels > 1u) {
		finalBarriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		finalBarriers[1].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		finalBarriers[1].srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
		finalBarriers[1].dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		finalBarriers[1].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
		finalBarriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		finalBarriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		finalBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		finalBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		finalBarriers[1].image = hizBuffer.image;
		finalBarriers[1].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, mipLevels - 1u, 0u, 1u};
	}

	VkDependencyInfo finalDep{};
	finalDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	finalDep.imageMemoryBarrierCount = (mipLevels > 1u) ? 2u : 1u;
	finalDep.pImageMemoryBarriers = finalBarriers;
	vkCmdPipelineBarrier2(commandBuffer, &finalDep);

	VkImageMemoryBarrier2 restoreDepth{};
	restoreDepth.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	restoreDepth.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	restoreDepth.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
	restoreDepth.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	restoreDepth.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	restoreDepth.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
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
}
} // namespace projectv::render

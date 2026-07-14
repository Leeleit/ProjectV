#include "render/vulkan/VulkanVoxelizePipeline.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"

#include <algorithm>

namespace projectv::render {
bool RecordVoxelizeDispatch(
	const VkCommandBuffer commandBuffer,
	RenderState &render,
	SceneFrameResources &frameResources,
	const VoxelizePushConstants &pushConstants,
	const uint32_t activeChunkCount)
{
	PV_PROFILE_ZONE_N("RecordVoxelizeDispatch");
	if (commandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (render.vctVoxelizePipeline == VK_NULL_HANDLE ||
		render.vctVoxelizePipelineLayout == VK_NULL_HANDLE) {
		return false;
	}
	if (frameResources.vctVoxelizeDescriptorSet == VK_NULL_HANDLE) {
		return false;
	}
	if (activeChunkCount == 0u) {
		return true;
	}

	profiling::PlotValue("VCT Voxelize Chunks", static_cast<int64_t>(activeChunkCount));

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, render.vctVoxelizePipeline);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		render.vctVoxelizePipelineLayout,
		0u,
		1u,
		&frameResources.vctVoxelizeDescriptorSet,
		0u,
		nullptr);
	vkCmdPushConstants(
		commandBuffer,
		render.vctVoxelizePipelineLayout,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0u,
		sizeof(VoxelizePushConstants),
		&pushConstants);
	vkCmdDispatch(commandBuffer, activeChunkCount, 1u, 1u);
	return true;
}

bool BuildVctClipmapMipChain(
	const VkCommandBuffer commandBuffer,
	RenderState &render)
{
	PV_PROFILE_ZONE_N("BuildVctClipmapMipChain");
	if (commandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (render.vctClipmapImage == VK_NULL_HANDLE) {
		return false;
	}
	const uint32_t mipLevels = render.vctClipmapMipLevelCount;
	if (mipLevels <= 1u) {
		return true;
	}
	const uint32_t resolution = render.vctClipmapResolution;
	if (resolution == 0u) {
		return false;
	}

	profiling::PlotValue("VCT Mip Chain Mips", static_cast<int64_t>(mipLevels - 1u));

	static constexpr VkImageLayout prevLayout = VK_IMAGE_LAYOUT_GENERAL;
	static constexpr VkImageLayout newLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkImageMemoryBarrier2 preBarrier{};
	preBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	preBarrier.pNext = nullptr;
	preBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	preBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	preBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	preBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
	preBarrier.oldLayout = prevLayout;
	preBarrier.newLayout = prevLayout;
	preBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	preBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	preBarrier.image = render.vctClipmapImage;
	preBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	VkDependencyInfo preDep{};
	preDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	preDep.imageMemoryBarrierCount = 1u;
	preDep.pImageMemoryBarriers = &preBarrier;
	vkCmdPipelineBarrier2(commandBuffer, &preDep);

	uint32_t srcWidth = resolution;
	uint32_t srcHeight = resolution;
	uint32_t srcDepth = resolution;
	for (uint32_t mipLevel = 1u; mipLevel < mipLevels; ++mipLevel) {
		const uint32_t dstWidth = std::max(1u, srcWidth >> 1u);
		const uint32_t dstHeight = std::max(1u, srcHeight >> 1u);
		const uint32_t dstDepth = std::max(1u, srcDepth >> 1u);

		VkImageBlit blit{};
		blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevel - 1u, 0u, 1u};
		blit.srcOffsets[0] = {0, 0, 0};
		blit.srcOffsets[1] = {static_cast<int32_t>(srcWidth), static_cast<int32_t>(srcHeight), static_cast<int32_t>(srcDepth)};
		blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 0u, 1u};
		blit.dstOffsets[0] = {0, 0, 0};
		blit.dstOffsets[1] = {static_cast<int32_t>(dstWidth), static_cast<int32_t>(dstHeight), static_cast<int32_t>(dstDepth)};
		vkCmdBlitImage(
			commandBuffer,
			render.vctClipmapImage,
			VK_IMAGE_LAYOUT_GENERAL,
			render.vctClipmapImage,
			VK_IMAGE_LAYOUT_GENERAL,
			1u, &blit,
			VK_FILTER_LINEAR);

		VkImageMemoryBarrier2 mipBarrier{};
		mipBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		mipBarrier.pNext = nullptr;
		mipBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		mipBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		mipBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		mipBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
		mipBarrier.oldLayout = newLayout;
		mipBarrier.newLayout = newLayout;
		mipBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		mipBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		mipBarrier.image = render.vctClipmapImage;
		mipBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 1u, 0u, 1u};
		VkDependencyInfo mipDep{};
		mipDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		mipDep.imageMemoryBarrierCount = 1u;
		mipDep.pImageMemoryBarriers = &mipBarrier;
		vkCmdPipelineBarrier2(commandBuffer, &mipDep);

		srcWidth = dstWidth;
		srcHeight = dstHeight;
		srcDepth = dstDepth;
	}

	(void)prevLayout;
	(void)newLayout;
	return true;
}

} // namespace projectv::render

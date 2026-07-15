#include "render/HizCulling.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace projectv::render {
bool RecordHzbCullingDispatch(
	const VkCommandBuffer commandBuffer,
	VulkanContextState *context,
	RenderState &render,
	SceneFrameResources &frameResources,
	const float (&viewProjection)[16],
	const uint32_t chunkDescriptorCount)
{
	if (!IsHzbCullingEnabled()) {
		return false;
	}
	if (commandBuffer == VK_NULL_HANDLE || context == nullptr) {
		return false;
	}
	if (render.hizCullingPipeline == VK_NULL_HANDLE ||
		render.hizCullingPipelineLayout == VK_NULL_HANDLE) {
		return false;
	}
	if (frameResources.hizCullingDescriptorSet == VK_NULL_HANDLE ||
		frameResources.chunkAabbBuffer == VK_NULL_HANDLE ||
		frameResources.visibilityMaskBuffer == VK_NULL_HANDLE ||
		frameResources.hzbVisibleCountBuffer == VK_NULL_HANDLE ||
		frameResources.hzbPerChunkMipBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (render.hizBuffer.imageView == VK_NULL_HANDLE ||
		render.hizBuffer.sampler == VK_NULL_HANDLE) {
		return false;
	}

	const uint32_t visibilityMaskWordCount =
		(chunkDescriptorCount + 31u) / 32u;

	if (visibilityMaskWordCount > 0u &&
		frameResources.prevVisibilityMaskBuffer != VK_NULL_HANDLE &&
		frameResources.visibilityMaskBuffer != VK_NULL_HANDLE) {
		VkBufferCopy maskCopy{};
		maskCopy.srcOffset = 0u;
		maskCopy.dstOffset = 0u;
		maskCopy.size = static_cast<VkDeviceSize>(visibilityMaskWordCount) * sizeof(uint32_t);
		vkCmdCopyBuffer(
			commandBuffer,
			frameResources.visibilityMaskBuffer,
			frameResources.prevVisibilityMaskBuffer,
			1u,
			&maskCopy);

		VkBufferMemoryBarrier2 copyBarrier{};
		copyBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		copyBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
		copyBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		copyBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		copyBarrier.dstAccessMask =
			VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
		copyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		copyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		copyBarrier.buffer = frameResources.prevVisibilityMaskBuffer;
		copyBarrier.offset = 0u;
		copyBarrier.size = maskCopy.size;

		VkDependencyInfo copyDepInfo{};
		copyDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		copyDepInfo.bufferMemoryBarrierCount = 1u;
		copyDepInfo.pBufferMemoryBarriers = &copyBarrier;
		vkCmdPipelineBarrier2(commandBuffer, &copyDepInfo);
	}

	if (visibilityMaskWordCount > 0u) {
		vkCmdFillBuffer(
			commandBuffer,
			frameResources.visibilityMaskBuffer,
			0u,
			static_cast<VkDeviceSize>(visibilityMaskWordCount) * sizeof(uint32_t),
			0u);
	}
	vkCmdFillBuffer(
		commandBuffer,
		frameResources.hzbVisibleCountBuffer,
		0u,
		sizeof(uint32_t),
		0u);

	VkBufferMemoryBarrier2 fillBarriers[2]{};
	fillBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	fillBarriers[0].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	fillBarriers[0].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	fillBarriers[0].dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	fillBarriers[0].dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	fillBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	fillBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	fillBarriers[0].buffer = frameResources.visibilityMaskBuffer;
	fillBarriers[0].offset = 0u;
	fillBarriers[0].size = static_cast<VkDeviceSize>(visibilityMaskWordCount) * sizeof(uint32_t);

	fillBarriers[1] = fillBarriers[0];
	fillBarriers[1].buffer = frameResources.hzbVisibleCountBuffer;
	fillBarriers[1].size = sizeof(uint32_t);

	VkDependencyInfo fillDepInfo{};
	fillDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	fillDepInfo.bufferMemoryBarrierCount = 2u;
	fillDepInfo.pBufferMemoryBarriers = fillBarriers;
	vkCmdPipelineBarrier2(commandBuffer, &fillDepInfo);

	VkDescriptorBufferInfo chunkAabbInfo{};
	chunkAabbInfo.buffer = frameResources.chunkAabbBuffer;
	chunkAabbInfo.offset = 0;
	chunkAabbInfo.range = VK_WHOLE_SIZE;

	VkDescriptorBufferInfo visibilityMaskInfo{};
	visibilityMaskInfo.buffer = frameResources.visibilityMaskBuffer;
	visibilityMaskInfo.offset = 0;
	visibilityMaskInfo.range = VK_WHOLE_SIZE;

	VkDescriptorImageInfo hizImageInfo{};
	hizImageInfo.sampler = VK_NULL_HANDLE;
	hizImageInfo.imageView = render.hizBuffer.imageView;
	hizImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorImageInfo hizSamplerInfo{};
	hizSamplerInfo.sampler = render.hizBuffer.sampler;
	hizSamplerInfo.imageView = VK_NULL_HANDLE;
	hizSamplerInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkDescriptorBufferInfo visibleCountInfo{};
	visibleCountInfo.buffer = frameResources.hzbVisibleCountBuffer;
	visibleCountInfo.offset = 0;
	visibleCountInfo.range = sizeof(uint32_t);

	VkDescriptorBufferInfo perChunkMipInfo{};
	perChunkMipInfo.buffer = frameResources.hzbPerChunkMipBuffer;
	perChunkMipInfo.offset = 0;
	perChunkMipInfo.range = VK_WHOLE_SIZE;

	std::array<VkWriteDescriptorSet, 6> writes{};
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = frameResources.hizCullingDescriptorSet;
	writes[0].dstBinding = 0;
	writes[0].dstArrayElement = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &chunkAabbInfo;

	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = frameResources.hizCullingDescriptorSet;
	writes[1].dstBinding = 1;
	writes[1].dstArrayElement = 0;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[1].pBufferInfo = &visibilityMaskInfo;

	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = frameResources.hizCullingDescriptorSet;
	writes[2].dstBinding = 2;
	writes[2].dstArrayElement = 0;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	writes[2].pImageInfo = &hizImageInfo;

	writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[3].dstSet = frameResources.hizCullingDescriptorSet;
	writes[3].dstBinding = 3;
	writes[3].dstArrayElement = 0;
	writes[3].descriptorCount = 1;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	writes[3].pImageInfo = &hizSamplerInfo;

	writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[4].dstSet = frameResources.hizCullingDescriptorSet;
	writes[4].dstBinding = 4;
	writes[4].dstArrayElement = 0;
	writes[4].descriptorCount = 1;
	writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[4].pBufferInfo = &visibleCountInfo;

	writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[5].dstSet = frameResources.hizCullingDescriptorSet;
	writes[5].dstBinding = 5;
	writes[5].dstArrayElement = 0;
	writes[5].descriptorCount = 1;
	writes[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[5].pBufferInfo = &perChunkMipInfo;

	vkUpdateDescriptorSets(
		context->device,
		static_cast<uint32_t>(writes.size()),
		writes.data(),
		0u,
		nullptr);

	HizCullingPushConstants pushConstants{};
	for (uint32_t i = 0; i < 16u; ++i) {
		pushConstants.viewProjection[i] = viewProjection[i];
	}
	const uint32_t maxMipInclusive =
		render.hizBuffer.mipLevelCount > 0u ? render.hizBuffer.mipLevelCount - 1u : 0u;
	pushConstants.hizExtentAndMipCount = {
		render.hizBuffer.baseWidth,
		render.hizBuffer.baseHeight,
		chunkDescriptorCount,
		maxMipInclusive,
	};
	pushConstants.depthUnpackParams = {1.0f, 0.0f, 0.0f, 0.0f};

	vkCmdBindPipeline(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		render.hizCullingPipeline);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		render.hizCullingPipelineLayout,
		0u,
		1u,
		&frameResources.hizCullingDescriptorSet,
		0u,
		nullptr);
	vkCmdPushConstants(
		commandBuffer,
		render.hizCullingPipelineLayout,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0u,
		sizeof(HizCullingPushConstants),
		&pushConstants);

	const uint32_t workgroupCount = (chunkDescriptorCount + 63u) / 64u;
	if (workgroupCount > 0u) {
		vkCmdDispatch(commandBuffer, workgroupCount, 1u, 1u);
	}
	VkBufferMemoryBarrier2 cullToApplyBarrier{};
	cullToApplyBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	cullToApplyBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	cullToApplyBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	cullToApplyBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	cullToApplyBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	cullToApplyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	cullToApplyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	cullToApplyBarrier.buffer = frameResources.visibilityMaskBuffer;
	cullToApplyBarrier.offset = 0u;
	cullToApplyBarrier.size = static_cast<VkDeviceSize>(visibilityMaskWordCount) * sizeof(uint32_t);
	VkDependencyInfo cullToApplyDep{};
	cullToApplyDep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	cullToApplyDep.bufferMemoryBarrierCount = 1u;
	cullToApplyDep.pBufferMemoryBarriers = &cullToApplyBarrier;
	vkCmdPipelineBarrier2(commandBuffer, &cullToApplyDep);
	// Do not copy visibility into other FIF slots — those buffers may still be in-flight (race → flicker).
	render.hzbMaskValid = true;
	if (frameResources.visibilityMaskMappedData != nullptr) {
		render.hzbCullSerialCounter += 1u;
		const uint32_t frameIndex = static_cast<uint32_t>(
			&frameResources - render.sceneFrameResources.data());
		if (frameIndex < render.hzbSlotCullSerial.size()) {
			render.hzbSlotCullSerial[frameIndex] = render.hzbCullSerialCounter;
		}
	}
	return true;
}
uint32_t ComputePerChunkMipLevelCpu(
	const float projectedExtentXTexels,
	const float projectedExtentYTexels,
	const uint32_t maxMipLevel)
{
	const float maxExtent = std::max(projectedExtentXTexels, projectedExtentYTexels);
	if (maxExtent <= 1.0f) {
		return 0u;
	}
	const float logVal = std::log2(maxExtent);
	const int32_t floored = static_cast<int32_t>(logVal);
	if (floored < 0) {
		return 0u;
	}
	const uint32_t capped = static_cast<uint32_t>(floored);
	return std::min(capped, maxMipLevel);
}

uint32_t ComputePerChunkMipLevelsFromAabbs(
	const std::vector<std::array<float, 4>> &chunkCenters,
	const std::vector<std::array<float, 4>> &chunkHalfExtents,
	const std::array<float, 16> &viewProjection,
	const uint32_t baseWidth,
	const uint32_t baseHeight,
	const uint32_t maxMipLevel,
	std::vector<uint32_t> &outMipLevels)
{
	const size_t count = std::min(chunkCenters.size(), chunkHalfExtents.size());
	outMipLevels.assign(count, 0u);
	if (count == 0u) {
		return 0u;
	}
	for (size_t i = 0; i < count; ++i) {
		const float centerX = chunkCenters[i][0];
		const float centerY = chunkCenters[i][1];
		const float centerZ = chunkCenters[i][2];
		const float halfExtent = chunkHalfExtents[i][0];
		float minX = std::numeric_limits<float>::infinity();
		float minY = std::numeric_limits<float>::infinity();
		float maxX = -std::numeric_limits<float>::infinity();
		float maxY = -std::numeric_limits<float>::infinity();
		for (int sx = -1; sx <= 1; sx += 2) {
			for (int sy = -1; sy <= 1; sy += 2) {
				for (int sz = -1; sz <= 1; sz += 2) {
					const float cornerX = centerX + static_cast<float>(sx) * halfExtent;
					const float cornerY = centerY + static_cast<float>(sy) * halfExtent;
					const float cornerZ = centerZ + static_cast<float>(sz) * halfExtent;
					const float clipX = viewProjection[0] * cornerX + viewProjection[4] * cornerY + viewProjection[8] * cornerZ + viewProjection[12];
					const float clipY = viewProjection[1] * cornerX + viewProjection[5] * cornerY + viewProjection[9] * cornerZ + viewProjection[13];
					const float clipW = viewProjection[3] * cornerX + viewProjection[7] * cornerY + viewProjection[11] * cornerZ + viewProjection[15];
					if (clipW <= 0.0001f) {
						outMipLevels[i] = 0u;
						goto next_chunk;
					}
					const float ndcX = clipX / clipW;
					const float ndcY = clipY / clipW;
					const float uvX = ndcX * 0.5f + 0.5f;
					const float uvY = ndcY * 0.5f + 0.5f;
					if (uvX < minX)
						minX = uvX;
					if (uvY < minY)
						minY = uvY;
					if (uvX > maxX)
						maxX = uvX;
					if (uvY > maxY)
						maxY = uvY;
				}
			}
		}
		{
			const float projectedXTexels = (maxX - minX) * static_cast<float>(baseWidth);
			const float projectedYTexels = (maxY - minY) * static_cast<float>(baseHeight);
			outMipLevels[i] = ComputePerChunkMipLevelCpu(
				projectedXTexels,
				projectedYTexels,
				maxMipLevel);
		}
	next_chunk:;
	}
	return static_cast<uint32_t>(count);
}

uint32_t ComputePerChunkMipAndBlendWidthsFromAabbs(
	const std::vector<std::array<float, 4>> &chunkCenters,
	const std::vector<std::array<float, 4>> &chunkHalfExtents,
	const std::array<float, 16> &viewProjection,
	const uint32_t baseWidth,
	const uint32_t baseHeight,
	const uint32_t maxMipLevel,
	const uint32_t maxBlendWidth,
	std::vector<uint32_t> &outMipAndBlendWidths)
{
	const size_t count = std::min(chunkCenters.size(), chunkHalfExtents.size());
	outMipAndBlendWidths.assign(count * 2u, 0u);
	if (count == 0u) {
		return 0u;
	}
	for (size_t i = 0; i < count; ++i) {
		const float centerX = chunkCenters[i][0];
		const float centerY = chunkCenters[i][1];
		const float centerZ = chunkCenters[i][2];
		const float halfExtent = chunkHalfExtents[i][0];
		float minX = std::numeric_limits<float>::infinity();
		float minY = std::numeric_limits<float>::infinity();
		float maxX = -std::numeric_limits<float>::infinity();
		float maxY = -std::numeric_limits<float>::infinity();
		bool skipBlend = false;
		for (int sx = -1; sx <= 1; sx += 2) {
			for (int sy = -1; sy <= 1; sy += 2) {
				for (int sz = -1; sz <= 1; sz += 2) {
					const float cornerX = centerX + static_cast<float>(sx) * halfExtent;
					const float cornerY = centerY + static_cast<float>(sy) * halfExtent;
					const float cornerZ = centerZ + static_cast<float>(sz) * halfExtent;
					const float clipX = viewProjection[0] * cornerX + viewProjection[4] * cornerY + viewProjection[8] * cornerZ + viewProjection[12];
					const float clipY = viewProjection[1] * cornerX + viewProjection[5] * cornerY + viewProjection[9] * cornerZ + viewProjection[13];
					const float clipW = viewProjection[3] * cornerX + viewProjection[7] * cornerY + viewProjection[11] * cornerZ + viewProjection[15];
					if (clipW <= 0.0001f) {
						outMipAndBlendWidths[i * 2u] = 0u;
						outMipAndBlendWidths[i * 2u + 1u] = 0u;
						skipBlend = true;
						goto next_chunk_blend;
					}
					const float ndcX = clipX / clipW;
					const float ndcY = clipY / clipW;
					const float uvX = ndcX * 0.5f + 0.5f;
					const float uvY = ndcY * 0.5f + 0.5f;
					if (uvX < minX)
						minX = uvX;
					if (uvY < minY)
						minY = uvY;
					if (uvX > maxX)
						maxX = uvX;
					if (uvY > maxY)
						maxY = uvY;
				}
			}
		}
		{
			const uint32_t projectedXTexels = static_cast<uint32_t>(std::abs(maxX - minX) * static_cast<float>(baseWidth));
			const uint32_t projectedYTexels = static_cast<uint32_t>(std::abs(maxY - minY) * static_cast<float>(baseHeight));
			const uint32_t mip = ComputePerChunkMipLevelCpu(
				static_cast<float>(projectedXTexels),
				static_cast<float>(projectedYTexels),
				maxMipLevel);
			outMipAndBlendWidths[i * 2u] = mip;
			outMipAndBlendWidths[i * 2u + 1u] = mip == 0u
													? 0u
													: ComputeBlendWidthForChunkMip(
														  projectedXTexels,
														  projectedYTexels,
														  mip,
														  maxBlendWidth);
		}
	next_chunk_blend:;
		(void)skipBlend;
	}
	return static_cast<uint32_t>(count);
}

void WritePerChunkMipAndBlendWidthsToBuffer(
	void *mappedData,
	const uint32_t *mipAndBlendWidths,
	const uint32_t chunkCount)
{
	if (mappedData == nullptr || mipAndBlendWidths == nullptr) {
		return;
	}
	// chunkCount frozen at frame start; consumer (hzb_cull.comp) reads same chunkCount.
	// Layout invariant: each chunk takes exactly 2 uint32 words (mip, blendWidth).
	static_assert(kHizMipAndBlendWidthWordsPerChunk == 2u,
				  "kHizMipAndBlendWidthWordsPerChunk must equal 2 (mip + blendWidth packed)");
	auto *dest = static_cast<uint32_t *>(mappedData);
	for (uint32_t i = 0u; i < chunkCount; ++i) {
		const uint32_t baseIndex = i * kHizMipAndBlendWidthWordsPerChunk;
		dest[baseIndex] = mipAndBlendWidths[baseIndex];
		dest[baseIndex + 1u] = mipAndBlendWidths[baseIndex + 1u];
	}
}
} // namespace projectv::render

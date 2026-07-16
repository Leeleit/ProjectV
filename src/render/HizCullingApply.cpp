#include "render/HizCulling.hpp"

#include "core/RuntimeDiagnostics.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <array>
#include <cmath>
#include <cstring>

namespace projectv::render {
namespace {

constexpr float kHzbCameraCutDotThreshold = 0.5f; // ~60 deg

} // namespace

bool DetectHzbCameraCut(RenderState &render, const projectv::math::Vec3 &cameraForward)
{
	const float lengthSq =
		cameraForward.x * cameraForward.x +
		cameraForward.y * cameraForward.y +
		cameraForward.z * cameraForward.z;
	if (lengthSq < 1.0e-8f) {
		render.hzbCameraCutThisFrame = true;
		return true;
	}
	const float invLength = 1.0f / std::sqrt(lengthSq);
	const projectv::math::Vec3 normalized{
		cameraForward.x * invLength,
		cameraForward.y * invLength,
		cameraForward.z * invLength,
	};
	bool cameraCut = !render.hzbPrevCameraForwardValid || !render.hzbMaskValid;
	if (render.hzbPrevCameraForwardValid) {
		const float dot =
			normalized.x * render.hzbPrevCameraForward.x +
			normalized.y * render.hzbPrevCameraForward.y +
			normalized.z * render.hzbPrevCameraForward.z;
		if (dot < kHzbCameraCutDotThreshold) {
			cameraCut = true;
		}
	}
	render.hzbPrevCameraForward = normalized;
	render.hzbPrevCameraForwardValid = true;
	render.hzbCameraCutThisFrame = cameraCut;
	return cameraCut;
}

void SyncHzbUnifiedVisibilityAfterFence(
	VulkanContextState *context,
	RenderState &render,
	const uint32_t frameIndex)
{
	if (!IsHzbCullingEnabled() || context == nullptr || context->allocator == nullptr) {
		return;
	}
	if (frameIndex >= render.sceneFrameResources.size() || frameIndex >= render.hzbSlotCullSerial.size()) {
		return;
	}
	if (render.hzbSlotCullSerial[frameIndex] == 0u ||
		render.hzbSlotCullSerial[frameIndex] <= render.hzbUnifiedCullSerial) {
		return;
	}
	SceneFrameResources &slot = render.sceneFrameResources[frameIndex];
	if (slot.visibilityMaskMappedData == nullptr || slot.visibilityMaskAllocation == nullptr) {
		return;
	}
	VmaAllocationInfo allocInfo{};
	vmaGetAllocationInfo(context->allocator, slot.visibilityMaskAllocation, &allocInfo);
	const size_t bytes = static_cast<size_t>(allocInfo.size);
	if (bytes == 0u) {
		return;
	}
	vmaInvalidateAllocation(context->allocator, slot.visibilityMaskAllocation, 0, bytes);
	const size_t wordCount = bytes / sizeof(uint32_t);
	if (render.hzbUnifiedVisibilityWords.size() != wordCount) {
		render.hzbUnifiedVisibilityWords.assign(wordCount, ~0u);
	}
	std::memcpy(
		render.hzbUnifiedVisibilityWords.data(),
		slot.visibilityMaskMappedData,
		wordCount * sizeof(uint32_t));
	const uint32_t visibleChunkCount =
		CountHzbVisibleChunks(render.hzbUnifiedVisibilityWords, slot.chunkDescriptorCount);
	render.hzbLastVisibleChunkCount = visibleChunkCount;
	render.hzbLastCulledChunkCount =
		slot.chunkDescriptorCount > visibleChunkCount
			? slot.chunkDescriptorCount - visibleChunkCount
			: 0u;
	render.hzbUnifiedCullSerial = render.hzbSlotCullSerial[frameIndex];
}

void SeedHzbSlotVisibilityFromUnified(
	VulkanContextState *context,
	RenderState &render,
	const uint32_t frameIndex)
{
	if (!IsHzbCullingEnabled() || render.hzbUnifiedVisibilityWords.empty()) {
		return;
	}
	if (context == nullptr || context->allocator == nullptr) {
		return;
	}
	if (frameIndex >= render.sceneFrameResources.size()) {
		return;
	}
	SceneFrameResources &slot = render.sceneFrameResources[frameIndex];
	if (slot.visibilityMaskMappedData == nullptr || slot.visibilityMaskAllocation == nullptr) {
		return;
	}
	const size_t bytes = render.hzbUnifiedVisibilityWords.size() * sizeof(uint32_t);
	std::memcpy(slot.visibilityMaskMappedData, render.hzbUnifiedVisibilityWords.data(), bytes);
	vmaFlushAllocation(context->allocator, slot.visibilityMaskAllocation, 0, bytes);
}

bool RecordHzbApplyVisibility(
	const VkCommandBuffer commandBuffer,
	VulkanContextState *context,
	RenderState &render,
	SceneFrameResources &frameResources,
	const uint32_t chunkDescriptorCount,
	const HzbApplyMode mode)
{
	if (!IsHzbCullingEnabled()) {
		return false;
	}
	if (commandBuffer == VK_NULL_HANDLE || context == nullptr) {
		return false;
	}
	if (render.hizApplyPipeline == VK_NULL_HANDLE ||
		render.hizApplyPipelineLayout == VK_NULL_HANDLE) {
		return false;
	}
	if (frameResources.hizApplyDescriptorSet == VK_NULL_HANDLE ||
		frameResources.visibilityMaskBuffer == VK_NULL_HANDLE ||
		frameResources.prevVisibilityMaskBuffer == VK_NULL_HANDLE ||
		frameResources.opaqueIndirectBuffer == VK_NULL_HANDLE ||
		frameResources.opaqueHzbDrawIndirectBuffer == VK_NULL_HANDLE ||
		frameResources.transparentIndirectBuffer == VK_NULL_HANDLE ||
		frameResources.transparentHzbDrawIndirectBuffer == VK_NULL_HANDLE ||
		frameResources.hzbVisibleCountBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (chunkDescriptorCount == 0u) {
		return false;
	}

	vkCmdFillBuffer(
		commandBuffer,
		frameResources.hzbVisibleCountBuffer,
		0u,
		sizeof(uint32_t),
		0u);

	VkBufferMemoryBarrier2 fillBarrier{};
	fillBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	fillBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	fillBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	fillBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	fillBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	fillBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	fillBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	fillBarrier.buffer = frameResources.hzbVisibleCountBuffer;
	fillBarrier.offset = 0u;
	fillBarrier.size = sizeof(uint32_t);

	VkDependencyInfo fillDepInfo{};
	fillDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	fillDepInfo.bufferMemoryBarrierCount = 1u;
	fillDepInfo.pBufferMemoryBarriers = &fillBarrier;
	vkCmdPipelineBarrier2(commandBuffer, &fillDepInfo);

	if (!frameResources.hizApplyDescriptorSetInitialized) {
		VkDescriptorBufferInfo visibilityMaskInfo{};
		visibilityMaskInfo.buffer = frameResources.visibilityMaskBuffer;
		visibilityMaskInfo.offset = 0;
		visibilityMaskInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo prevVisibilityMaskInfo{};
		prevVisibilityMaskInfo.buffer = frameResources.prevVisibilityMaskBuffer;
		prevVisibilityMaskInfo.offset = 0;
		prevVisibilityMaskInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo sourceOpaqueInfo{};
		sourceOpaqueInfo.buffer = frameResources.opaqueIndirectBuffer;
		sourceOpaqueInfo.offset = 0;
		sourceOpaqueInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo destOpaqueInfo{};
		destOpaqueInfo.buffer = frameResources.opaqueHzbDrawIndirectBuffer;
		destOpaqueInfo.offset = 0;
		destOpaqueInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo sourceTransparentInfo{};
		sourceTransparentInfo.buffer = frameResources.transparentIndirectBuffer;
		sourceTransparentInfo.offset = 0;
		sourceTransparentInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo destTransparentInfo{};
		destTransparentInfo.buffer = frameResources.transparentHzbDrawIndirectBuffer;
		destTransparentInfo.offset = 0;
		destTransparentInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo visibleCountInfo{};
		visibleCountInfo.buffer = frameResources.hzbVisibleCountBuffer;
		visibleCountInfo.offset = 0;
		visibleCountInfo.range = sizeof(uint32_t);

		std::array<VkWriteDescriptorSet, 7> writes{};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = frameResources.hizApplyDescriptorSet;
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[0].pBufferInfo = &visibilityMaskInfo;

		writes[1] = writes[0];
		writes[1].dstBinding = 1;
		writes[1].pBufferInfo = &prevVisibilityMaskInfo;

		writes[2] = writes[0];
		writes[2].dstBinding = 2;
		writes[2].pBufferInfo = &sourceOpaqueInfo;

		writes[3] = writes[0];
		writes[3].dstBinding = 3;
		writes[3].pBufferInfo = &destOpaqueInfo;

		writes[4] = writes[0];
		writes[4].dstBinding = 4;
		writes[4].pBufferInfo = &sourceTransparentInfo;

		writes[5] = writes[0];
		writes[5].dstBinding = 5;
		writes[5].pBufferInfo = &destTransparentInfo;

		writes[6] = writes[0];
		writes[6].dstBinding = 6;
		writes[6].pBufferInfo = &visibleCountInfo;

		vkUpdateDescriptorSets(
			context->device,
			static_cast<uint32_t>(writes.size()),
			writes.data(),
			0u,
			nullptr);
		frameResources.hizApplyDescriptorSetInitialized = true;
	}

	HzbApplyPushConstants pushConstants{};
	pushConstants.chunkCountAndMode = {
		chunkDescriptorCount,
		static_cast<uint32_t>(mode),
		0u,
		0u,
	};

	vkCmdBindPipeline(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		render.hizApplyPipeline);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		render.hizApplyPipelineLayout,
		0u,
		1u,
		&frameResources.hizApplyDescriptorSet,
		0u,
		nullptr);
	vkCmdPushConstants(
		commandBuffer,
		render.hizApplyPipelineLayout,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0u,
		sizeof(HzbApplyPushConstants),
		&pushConstants);

	const uint32_t workgroupCount = (chunkDescriptorCount + 63u) / 64u;
	vkCmdDispatch(commandBuffer, workgroupCount, 1u, 1u);

	VkBufferMemoryBarrier2 drawBarriers[2]{};
	drawBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	drawBarriers[0].srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	drawBarriers[0].srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	drawBarriers[0].dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
	drawBarriers[0].dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
	drawBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	drawBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	drawBarriers[0].buffer = frameResources.opaqueHzbDrawIndirectBuffer;
	drawBarriers[0].offset = 0u;
	drawBarriers[0].size = VK_WHOLE_SIZE;

	drawBarriers[1] = drawBarriers[0];
	drawBarriers[1].buffer = frameResources.transparentHzbDrawIndirectBuffer;

	VkDependencyInfo drawDepInfo{};
	drawDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	drawDepInfo.bufferMemoryBarrierCount = 2u;
	drawDepInfo.pBufferMemoryBarriers = drawBarriers;
	vkCmdPipelineBarrier2(commandBuffer, &drawDepInfo);
	return true;
}
} // namespace projectv::render

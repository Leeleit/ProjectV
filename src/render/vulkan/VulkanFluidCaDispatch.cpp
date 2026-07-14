#include "render/vulkan/VulkanFluidCaPipeline.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "debug/Profiling.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <array>
#include <cstring>

namespace projectv::render {
bool RecordFluidCaDispatch(
	VkCommandBuffer commandBuffer,
	RenderState &render,
	SceneFrameResources &frameResources,
	const FluidCaPushConstants &pushConstants,
	uint32_t activeChunkCount)
{
	PV_PROFILE_ZONE_N("RecordFluidCaDispatch");
	if (!render.fluidCaPipelineEnabled) {
		return false;
	}
	if (commandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (render.fluidCaPipeline == VK_NULL_HANDLE ||
		render.fluidCaPipelineLayout == VK_NULL_HANDLE) {
		return false;
	}
	if (frameResources.fluidCaDescriptorSet == VK_NULL_HANDLE) {
		return false;
	}
	if (frameResources.fluidCaSourceBuffer == VK_NULL_HANDLE ||
		frameResources.fluidCaDestinationBuffer == VK_NULL_HANDLE ||
		frameResources.fluidCaActiveChunkIdBuffer == VK_NULL_HANDLE ||
		frameResources.fluidCaStatsBuffer == VK_NULL_HANDLE) {
		return false;
	}

	if (frameResources.fluidCaStatsMappedData) {
		std::memset(frameResources.fluidCaStatsMappedData, 0, sizeof(FluidCaGpuFrameStats));
	}

	VkBufferMemoryBarrier2 statsFillBarrier{};
	statsFillBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	statsFillBarrier.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
	statsFillBarrier.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT;
	statsFillBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	statsFillBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	statsFillBarrier.buffer = frameResources.fluidCaStatsBuffer;
	statsFillBarrier.offset = 0u;
	statsFillBarrier.size = sizeof(FluidCaGpuFrameStats);

	VkBufferMemoryBarrier2 sourceBarrier{};
	sourceBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	sourceBarrier.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
	sourceBarrier.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT;
	sourceBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	sourceBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	sourceBarrier.buffer = frameResources.fluidCaSourceBuffer;
	sourceBarrier.offset = 0u;
	sourceBarrier.size = VK_WHOLE_SIZE;

	VkBufferMemoryBarrier2 activeChunkIdBarrier{};
	activeChunkIdBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	activeChunkIdBarrier.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
	activeChunkIdBarrier.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT;
	activeChunkIdBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	activeChunkIdBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	activeChunkIdBarrier.buffer = frameResources.fluidCaActiveChunkIdBuffer;
	activeChunkIdBarrier.offset = 0u;
	activeChunkIdBarrier.size = VK_WHOLE_SIZE;

	{
		const std::array preBarriers{statsFillBarrier, sourceBarrier, activeChunkIdBarrier};
		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(preBarriers.size());
		depInfo.pBufferMemoryBarriers = preBarriers.data();
		vkCmdPipelineBarrier2(commandBuffer, &depInfo);
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, render.fluidCaPipeline);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		render.fluidCaPipelineLayout,
		0u,
		1u,
		&frameResources.fluidCaDescriptorSet,
		0u,
		nullptr);
	vkCmdPushConstants(
		commandBuffer,
		render.fluidCaPipelineLayout,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0u,
		sizeof(FluidCaPushConstants),
		&pushConstants);

	if (activeChunkCount > 0u) {
		vkCmdDispatch(commandBuffer, activeChunkCount, 1u, 1u);
	}

	VkBufferMemoryBarrier2 postBarrier{};
	postBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	postBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	postBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	postBarrier.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
	postBarrier.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
	postBarrier.buffer = frameResources.fluidCaStatsBuffer;
	postBarrier.offset = 0u;
	postBarrier.size = sizeof(FluidCaGpuFrameStats);

	VkBufferMemoryBarrier2 destBarrier{};
	destBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	destBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	destBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	destBarrier.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
	destBarrier.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
	destBarrier.buffer = frameResources.fluidCaDestinationBuffer;
	destBarrier.offset = 0u;
	destBarrier.size = VK_WHOLE_SIZE;

	{
		const std::array postBarriers{postBarrier, destBarrier};
		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(postBarriers.size());
		depInfo.pBufferMemoryBarriers = postBarriers.data();
		vkCmdPipelineBarrier2(commandBuffer, &depInfo);
	}

	return true;
}

bool SubmitFluidCaToComputeQueue(
	VulkanContextState *context,
	RenderState &render,
	const VkCommandBuffer commandBuffer,
	uint64_t *outTimelineValue)
{
	PV_PROFILE_ZONE_N("SubmitFluidCaToComputeQueue");
	PV_CHECK_OR_RETURN(
		context && context->dedicatedComputeQueue != VK_NULL_HANDLE &&
			context->renderTimelineSemaphore != VK_NULL_HANDLE,
		"Render",
		"SubmitFluidCaToComputeQueue.Preconditions",
		"dedicated compute queue or timeline semaphore unavailable");
	if (commandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (!render.fluidCaPipelineEnabled) {
		return false;
	}

	context->renderTimelineValue += 1u;
	const uint64_t timelineValue = context->renderTimelineValue;
	if (outTimelineValue != nullptr) {
		*outTimelineValue = timelineValue;
	}

	VkCommandBufferSubmitInfo commandBufferSubmitInfo{};
	commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	commandBufferSubmitInfo.commandBuffer = commandBuffer;

	VkSemaphoreSubmitInfo waitSemaphoreInfo{};
	waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	waitSemaphoreInfo.semaphore = context->renderTimelineSemaphore;
	waitSemaphoreInfo.value = timelineValue - 1u;
	waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

	VkSemaphoreSubmitInfo signalSemaphoreInfo{};
	signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalSemaphoreInfo.semaphore = context->renderTimelineSemaphore;
	signalSemaphoreInfo.value = timelineValue;
	signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

	VkSubmitInfo2 submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.commandBufferInfoCount = 1u;
	submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;
	submitInfo.waitSemaphoreInfoCount = 1u;
	submitInfo.pWaitSemaphoreInfos = &waitSemaphoreInfo;
	submitInfo.signalSemaphoreInfoCount = 1u;
	submitInfo.pSignalSemaphoreInfos = &signalSemaphoreInfo;

	const VkResult result = vkQueueSubmit2(context->dedicatedComputeQueue, 1u, &submitInfo, VK_NULL_HANDLE);
	if (result != VK_SUCCESS) {
		runtime::LogVkFailure("SubmitFluidCaToComputeQueue.vkQueueSubmit2", result);
		context->renderTimelineValue -= 1u;
		return false;
	}
	return true;
}

bool ReadFluidCaFrameStats(
	VulkanContextState *context,
	const RenderState &render,
	const uint32_t frameIndex,
	FluidCaGpuFrameStats *outStats)
{
	PV_PROFILE_ZONE_N("ReadFluidCaFrameStats");
	if (context == nullptr || outStats == nullptr) {
		return false;
	}
	if (!render.fluidCaPipelineEnabled) {
		return false;
	}
	if (static_cast<size_t>(frameIndex) >= render.sceneFrameResources.size()) {
		return false;
	}
	const SceneFrameResources &frameResources = render.sceneFrameResources[frameIndex];
	if (frameResources.fluidCaStatsBuffer == VK_NULL_HANDLE ||
		frameResources.fluidCaStatsAllocation == nullptr ||
		frameResources.fluidCaStatsMappedData == nullptr) {
		return false;
	}

	const VkResult invalidateResult = vmaInvalidateAllocation(
		context->allocator,
		frameResources.fluidCaStatsAllocation,
		0u,
		sizeof(FluidCaGpuFrameStats));
	if (invalidateResult != VK_SUCCESS) {
		runtime::LogVmaFailure("ReadFluidCaFrameStats.vmaInvalidateAllocation", invalidateResult);
		return false;
	}
	std::memcpy(outStats, frameResources.fluidCaStatsMappedData, sizeof(FluidCaGpuFrameStats));
	return true;
}

} // namespace projectv::render

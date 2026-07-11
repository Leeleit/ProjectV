#include "render/vulkan/VulkanAsyncCompute.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"
#include "render/HizCulling.hpp"
#include "render/SceneResources.hpp"
#include "render/vulkan/VulkanDebug.hpp"
#include "render/vulkan/VulkanFluidCaPipeline.hpp"
#include "render/vulkan/VulkanWorldGenPipeline.hpp"

#include <array>
#include <vector>


namespace projectv::render {

bool IsAsyncComputeResourcesAllocated(const VulkanContextState &context)
{
	return context.asyncComputeCommandPool != VK_NULL_HANDLE &&
		context.asyncComputeCommandBuffer != VK_NULL_HANDLE;
}

bool EnsureAsyncComputeResources(VulkanContextState *context)
{
	PV_PROFILE_ZONE_N("EnsureAsyncComputeResources");
	if (context == nullptr) {
		return false;
	}
	if (context->device == VK_NULL_HANDLE) {
		return false;
	}
	if (context->dedicatedComputeQueue == VK_NULL_HANDLE) {
		return false;
	}
	if (context->dedicatedComputeQueueFamilyIndex == UINT32_MAX) {
		return false;
	}
	if (IsAsyncComputeResourcesAllocated(*context)) {
		return true;
	}

	const VkCommandPoolCreateInfo poolInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = context->dedicatedComputeQueueFamilyIndex,
	};
	const VkResult poolResult = vkCreateCommandPool(context->device, &poolInfo, nullptr, &context->asyncComputeCommandPool);
	if (poolResult != VK_SUCCESS) {
		runtime::LogVkFailure("EnsureAsyncComputeResources.vkCreateCommandPool", poolResult);
		DestroyAsyncComputeResources(context);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(context->asyncComputeCommandPool), VK_OBJECT_TYPE_COMMAND_POOL, "AsyncComputeCommandPool");

	const VkCommandBufferAllocateInfo allocInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,
		.commandPool = context->asyncComputeCommandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1u,
	};
	const VkResult allocResult = vkAllocateCommandBuffers(context->device, &allocInfo, &context->asyncComputeCommandBuffer);
	if (allocResult != VK_SUCCESS) {
		runtime::LogVkFailure("EnsureAsyncComputeResources.vkAllocateCommandBuffers", allocResult);
		DestroyAsyncComputeResources(context);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(context->asyncComputeCommandBuffer), VK_OBJECT_TYPE_COMMAND_BUFFER, "AsyncComputeCommandBuffer");
	// EVIL: reset the persistent asyncComputeCommandBuffer once at allocation so the
	// validation layer sees it in initial state (not pending). Without this, the
	// validation layer flags VUID-vkBeginCommandBuffer-commandBuffer-00049 if/when
	// vkBeginCommandBuffer is later called without a prior wait/reset cycle.
	vkResetCommandBuffer(context->asyncComputeCommandBuffer, 0u);
	return true;
}

void DestroyAsyncComputeResources(VulkanContextState *context)
{
	if (context == nullptr) {
		return;
	}
	if (context->device == VK_NULL_HANDLE) {
		return;
	}
	if (context->asyncComputeCommandBuffer != VK_NULL_HANDLE) {
		vkFreeCommandBuffers(context->device, context->asyncComputeCommandPool, 1u, &context->asyncComputeCommandBuffer);
		context->asyncComputeCommandBuffer = VK_NULL_HANDLE;
	}
	if (context->asyncComputeCommandPool != VK_NULL_HANDLE) {
		vkDestroyCommandPool(context->device, context->asyncComputeCommandPool, nullptr);
		context->asyncComputeCommandPool = VK_NULL_HANDLE;
	}
}

bool RecordAsyncComputePass(
	VkCommandBuffer asyncCommandBuffer,
	VulkanContextState &context,
	RenderState &render,
	AppState *state,
	FrameState *frame)
{
	PV_PROFILE_ZONE_N("RecordAsyncComputePass");
	if (asyncCommandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (state == nullptr || frame == nullptr) {
		return false;
	}

	// EVIL: wait on renderTimelineSemaphore so the persistent asyncComputeCommandBuffer
	// is in executable/reset state before vkBeginCommandBuffer. Per Vulkan spec
	// VUID-vkBeginCommandBuffer-commandBuffer-00049 the cmd buffer must not be in
	// pending/recording state when recording begins. SubmitToComputeQueue signals
	// renderTimelineSemaphore (NOT hzbBuildTimelineSemaphore, which is for HZB
	// async cull); we must wait on the same semaphore that was used to signal
	// the previous submission. Waiting on hzbBuildTimelineSemaphore is wrong
	// because it does not synchronize this cmd buffer's previous submission.
	if (context.renderTimelineSemaphore != VK_NULL_HANDLE && context.renderTimelineValue > 0u) {
		const VkSemaphoreWaitInfo waitInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
			.pNext = nullptr,
			.flags = 0,
			.semaphoreCount = 1u,
			.pSemaphores = &context.renderTimelineSemaphore,
			.pValues = &context.renderTimelineValue,
		};
		const VkResult waitResult = vkWaitSemaphores(context.device, &waitInfo, UINT64_MAX);
		if (waitResult != VK_SUCCESS) {
			runtime::LogVkFailure("RecordAsyncComputePass.vkWaitSemaphores", waitResult);
			return false;
		}
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = 0u;
	beginInfo.pInheritanceInfo = nullptr;
	const VkResult beginResult = vkBeginCommandBuffer(asyncCommandBuffer, &beginInfo);
	if (beginResult != VK_SUCCESS) {
		runtime::LogVkFailure("RecordAsyncComputePass.vkBeginCommandBuffer", beginResult);
		return false;
	}

	SceneFrameResources &frameResources = render.sceneFrameResources[frame->currentFrame];
	bool dispatched = false;

	if (render.fluidCaPipelineEnabled && state->simulation().fluidGpuTicksPending > 0u) {
		PV_PROFILE_ZONE_N("AsyncPass.RecordFluidCa");
		const VoxelWorld *voxelWorld = state->world().voxelWorld.get();
		if (voxelWorld != nullptr) {
			const std::vector<uint32_t> activeChunkIds = BuildActiveChunkIdsForFluidCa(*voxelWorld);
			if (frameResources.fluidCaActiveChunkIdMappedData != nullptr && !activeChunkIds.empty()) {
				std::memcpy(
					frameResources.fluidCaActiveChunkIdMappedData,
					activeChunkIds.data(),
					activeChunkIds.size() * sizeof(uint32_t));
			}
			FluidCaPushConstants fluidCaPush{};
			fluidCaPush.chunkDimensions = {
				static_cast<uint32_t>(voxelWorld->chunkSize),
				static_cast<uint32_t>(voxelWorld->chunkSize),
				static_cast<uint32_t>(voxelWorld->chunkSize),
				0u,
			};
			fluidCaPush.chunkCountAndFlags = {
				static_cast<uint32_t>(activeChunkIds.size()),
				0u,
				0u,
				0u,
			};
			fluidCaPush.fluidTickInterval = 1.0f / std::max(state->simulation().fluidTickRateHz, 1.0f);
			for (uint32_t tickIndex = 0; tickIndex < state->simulation().fluidGpuTicksPending; ++tickIndex) {
				RecordFluidCaDispatch(
					asyncCommandBuffer,
					render,
					frameResources,
					fluidCaPush,
					static_cast<uint32_t>(activeChunkIds.size()));
			}
			state->simulation().fluidGpuTicksPending = 0u;
			dispatched = true;
		}
	}

	if (render.worldGenPipelineEnabled && state->world().voxelWorld != nullptr) {
		PV_PROFILE_ZONE_N("AsyncPass.RecordWorldGen");
		const VoxelWorld *voxelWorld = state->world().voxelWorld.get();
		std::vector<uint32_t> activeWorldGenChunkIds;
		const uint32_t worldGenChunkCount = BuildActiveChunkIdsForWorldGen(
			*voxelWorld,
			activeWorldGenChunkIds);
		if (worldGenChunkCount > 0u && frameResources.worldGenVoxelBuffer != VK_NULL_HANDLE) {
			if (frameResources.worldGenVoxelMappedData != nullptr) {
				std::memset(
					frameResources.worldGenVoxelMappedData,
					0,
					static_cast<size_t>(worldGenChunkCount) *
						static_cast<size_t>(kWorldGenVoxelBufferBytesPerChunk));
			}
			WorldGenPushConstants worldGenPush{};
			worldGenPush.chunkOriginAndChunkSize = {
				0,
				0,
				0,
				voxelWorld->chunkSize,
			};
			worldGenPush.chunkCountAndFlags = {
				worldGenChunkCount,
				0u,
				0u,
				0u,
			};
			worldGenPush.noiseParams = {
				0.5f,
				0.5f,
				4.0f,
				2.0f,
			};
			worldGenPush.seed = static_cast<uint32_t>(state->simulation().simulationTick);
			RecordWorldGenDispatch(
				asyncCommandBuffer,
				render,
				frameResources,
				worldGenPush,
				worldGenChunkCount);
			dispatched = true;
		}
	}

	if (!dispatched) {
		vkEndCommandBuffer(asyncCommandBuffer);
		return false;
	}

	const VkResult endResult = vkEndCommandBuffer(asyncCommandBuffer);
	if (endResult != VK_SUCCESS) {
		runtime::LogVkFailure("RecordAsyncComputePass.vkEndCommandBuffer", endResult);
		return false;
	}
	return true;
}

bool SubmitToComputeQueue(
	VulkanContextState *context,
	VkCommandBuffer commandBuffer,
	uint64_t *outTimelineValue)
{
	PV_PROFILE_ZONE_N("SubmitToComputeQueue");
	if (context == nullptr) {
		return false;
	}
	if (context->dedicatedComputeQueue == VK_NULL_HANDLE) {
		return false;
	}
	if (context->renderTimelineSemaphore == VK_NULL_HANDLE) {
		return false;
	}
	if (commandBuffer == VK_NULL_HANDLE) {
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
		runtime::LogVkFailure("SubmitToComputeQueue.vkQueueSubmit2", result);
		context->renderTimelineValue -= 1u;
		if (outTimelineValue != nullptr) {
			*outTimelineValue = 0u;
		}
		return false;
	}
	return true;
}

bool RecordHzbAsyncCullPass(
	VkCommandBuffer asyncCommandBuffer,
	VulkanContextState &context,
	RenderState &render,
	const float (&inverseViewProjection)[16],
	const uint32_t chunkDescriptorCount)
{
	PV_PROFILE_ZONE_N("RecordHzbAsyncCullPass");
	if (asyncCommandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (context.device == VK_NULL_HANDLE) {
		return false;
	}
	if (render.hizCullingPipeline == VK_NULL_HANDLE) {
		return false;
	}
	// noinspection CppDFAUnreachableCode
	if (render.sceneFrameResources.empty()) {
		return false;
	}

	// EVIL: wait on hzbBuildTimelineSemaphore so the persistent asyncComputeCommandBuffer
	// is in executable/reset state. Per VUID-vkBeginCommandBuffer-commandBuffer-00049.
	if (context.hzbBuildTimelineSemaphore != VK_NULL_HANDLE && context.hzbBuildLastTimelineValue > 0u) {
		const VkSemaphoreWaitInfo waitInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
			.pNext = nullptr,
			.flags = 0,
			.semaphoreCount = 1u,
			.pSemaphores = &context.hzbBuildTimelineSemaphore,
			.pValues = &context.hzbBuildLastTimelineValue,
		};
		const VkResult waitResult = vkWaitSemaphores(context.device, &waitInfo, UINT64_MAX);
		if (waitResult != VK_SUCCESS) {
			runtime::LogVkFailure("RecordHzbAsyncCullPass.vkWaitSemaphores", waitResult);
			return false;
		}
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = 0u;
	beginInfo.pInheritanceInfo = nullptr;
	const const VkResult beginResult = vkBeginCommandBuffer(asyncCommandBuffer, &beginInfo);
	if (beginResult != VK_SUCCESS) {
		runtime::LogVkFailure("RecordHzbAsyncCullPass.vkBeginCommandBuffer", beginResult);
		return false;
	}

	static constexpr uint32_t currentFrame = 0u;
	SceneFrameResources &frameResources = render.sceneFrameResources[currentFrame];

	VkImageMemoryBarrier2 hizBarrier{};
	hizBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	hizBarrier.pNext = nullptr;
	hizBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	hizBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	hizBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	hizBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
	hizBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	hizBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	hizBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	hizBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	hizBarrier.image = render.hizBuffer.image;
	hizBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, render.hizBuffer.mipLevelCount, 0u, 1u};

	VkDependencyInfo hizDepInfo{};
	hizDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	hizDepInfo.imageMemoryBarrierCount = 1u;
	hizDepInfo.pImageMemoryBarriers = &hizBarrier;
	vkCmdPipelineBarrier2(asyncCommandBuffer, &hizDepInfo);

	const bool recorded = RecordHzbCullingDispatch(
		asyncCommandBuffer,
		&context,
		render,
		frameResources,
		inverseViewProjection,
		chunkDescriptorCount);

	if (!recorded) {
		runtime::LogRuntimeFailure(
			"Render",
			"RecordHzbAsyncCullPass.RecordHzbCullingDispatch",
			"RecordHzbCullingDispatch returned false on async compute CB");
	}

	const VkResult endResult = vkEndCommandBuffer(asyncCommandBuffer);
	if (endResult != VK_SUCCESS) {
		runtime::LogVkFailure("RecordHzbAsyncCullPass.vkEndCommandBuffer", endResult);
		return false;
	}
	return recorded;
}

bool SubmitHzbAsyncCullToComputeQueue(
	VulkanContextState *context,
	VkCommandBuffer asyncCommandBuffer,
	uint64_t *outTimelineValue)
{
	PV_PROFILE_ZONE_N("SubmitHzbAsyncCullToComputeQueue");
	if (context == nullptr) {
		return false;
	}
	if (context->dedicatedComputeQueue == VK_NULL_HANDLE) {
		return false;
	}
	if (context->hzbBuildTimelineSemaphore == VK_NULL_HANDLE) {
		return false;
	}
	if (asyncCommandBuffer == VK_NULL_HANDLE) {
		return false;
	}

	context->hzbBuildLastTimelineValue += 1u;
	const uint64_t timelineValue = context->hzbBuildLastTimelineValue;
	if (outTimelineValue != nullptr) {
		*outTimelineValue = timelineValue;
	}

	VkCommandBufferSubmitInfo commandBufferSubmitInfo{};
	commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	commandBufferSubmitInfo.commandBuffer = asyncCommandBuffer;

	VkSemaphoreSubmitInfo waitSemaphoreInfo{};
	waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	waitSemaphoreInfo.semaphore = context->hzbBuildTimelineSemaphore;
	waitSemaphoreInfo.value = timelineValue - 1u;
	waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

	VkSemaphoreSubmitInfo signalSemaphoreInfo{};
	signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalSemaphoreInfo.semaphore = context->hzbBuildTimelineSemaphore;
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
		runtime::LogVkFailure("SubmitHzbAsyncCullToComputeQueue.vkQueueSubmit2", result);
		context->hzbBuildLastTimelineValue -= 1u;
		if (outTimelineValue != nullptr) {
			*outTimelineValue = 0u;
		}
		return false;
	}
	return true;
}

}  // namespace projectv::render

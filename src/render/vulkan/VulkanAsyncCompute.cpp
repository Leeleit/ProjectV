#include "render/vulkan/VulkanAsyncCompute.hpp"

#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "debug/Profiling.hpp"
#include "render/SceneResources.hpp"
#include "render/vulkan/VulkanDebug.hpp"
#include "render/vulkan/VulkanFluidCaPipeline.hpp"
#include "render/vulkan/VulkanSyncPrimitives.hpp"
#include "render/vulkan/VulkanWorldGenPipeline.hpp"

#include <array>
#include <cstring>
#include <vector>

#include "SDL3/SDL_log.h"

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
	VkResult poolResult = vkCreateCommandPool(context->device, &poolInfo, nullptr, &context->asyncComputeCommandPool);
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
	VkResult allocResult = vkAllocateCommandBuffers(context->device, &allocInfo, &context->asyncComputeCommandBuffer);
	if (allocResult != VK_SUCCESS) {
		runtime::LogVkFailure("EnsureAsyncComputeResources.vkAllocateCommandBuffers", allocResult);
		DestroyAsyncComputeResources(context);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(context->asyncComputeCommandBuffer), VK_OBJECT_TYPE_COMMAND_BUFFER, "AsyncComputeCommandBuffer");
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

	constexpr VkCommandBufferUsageFlags kUsageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = kUsageFlags;
	beginInfo.pInheritanceInfo = nullptr;
	VkResult beginResult = vkBeginCommandBuffer(asyncCommandBuffer, &beginInfo);
	if (beginResult != VK_SUCCESS) {
		runtime::LogVkFailure("RecordAsyncComputePass.vkBeginCommandBuffer", beginResult);
		return false;
	}

	SceneFrameResources &frameResources = render.sceneFrameResources[frame->currentFrame];
	bool dispatched = false;

	if (render.fluidCaPipelineEnabled && state->simulation().fluidGpuTicksPending > 0u) {
		PV_PROFILE_ZONE_N("AsyncPass.RecordFluidCa");
		VoxelWorld *voxelWorld = state->world().voxelWorld.get();
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
		VoxelWorld *voxelWorld = state->world().voxelWorld.get();
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
						static_cast<size_t>(projectv::render::kWorldGenVoxelBufferBytesPerChunk));
			}
			WorldGenPushConstants worldGenPush{};
			worldGenPush.chunkOriginAndChunkSize = {
				0,
				0,
				0,
				static_cast<int32_t>(voxelWorld->chunkSize),
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

}  // namespace projectv::render

#include "render/LodDownsampleGpuConsume.hpp"

#include "core/RuntimeDiagnostics.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>

#include "SDL3/SDL_log.h"

namespace projectv::render {

bool IsLodDownsampledGpuConsumeEnabled()
{
	if (const char *value = std::getenv("PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME")) {
		return value[0] == 'O' && value[1] == 'N';
	}
	return false;
}

uint32_t ComputeLodDownsampledVoxelPayloadBytes(
	const uint32_t chunkCount,
	const uint32_t chunkSize)
{
	const uint32_t worstCaseExtent = std::max<uint32_t>(chunkSize / 2u, 1u);
	const VkDeviceSize worstCaseVoxelCount =
		static_cast<VkDeviceSize>(worstCaseExtent) *
		static_cast<VkDeviceSize>(worstCaseExtent) *
		static_cast<VkDeviceSize>(worstCaseExtent);
	const VkDeviceSize totalBytes = worstCaseVoxelCount * chunkCount;
	return static_cast<uint32_t>(std::min<VkDeviceSize>(totalBytes, 64u * 1024u * 1024u));
}

uint32_t ComputeChunkLodLevelsCapacity(const uint32_t chunkCount)
{
	return std::max<uint32_t>(chunkCount, 1u);
}

bool RefreshLodDownsampledBuffers(
	VulkanContextState *context,
	RenderState *render,
	const VoxelWorld &world)
{
	if (context == nullptr || render == nullptr) {
		return false;
	}
	if (context->device == VK_NULL_HANDLE || context->allocator == VK_NULL_HANDLE) {
		return false;
	}

	const uint32_t chunkCount = static_cast<uint32_t>(world.chunks.size());
	const uint32_t chunkSize = static_cast<uint32_t>(world.chunkSize);

	const uint32_t neededPayloadBytes = ComputeLodDownsampledVoxelPayloadBytes(chunkCount, chunkSize);
	const uint32_t neededChunkLodLevels = ComputeChunkLodLevelsCapacity(chunkCount);

	for (SceneFrameResources &frameResources : render->sceneFrameResources) {
		if (frameResources.lodDownsampledVoxelPayloadBuffer == VK_NULL_HANDLE) {
			frameResources.lodDownsampledVoxelPayloadCapacityBytes = 0u;
		}
		if (frameResources.chunkLodLevelsBuffer == VK_NULL_HANDLE) {
			frameResources.chunkLodLevelsCapacity = 0u;
		}

		if (neededPayloadBytes > frameResources.lodDownsampledVoxelPayloadCapacityBytes) {
			runtime::LogRuntimeFailure(
				"Render",
				"RefreshLodDownsampledBuffers.PayloadCapacity",
				"required payload bytes exceed current allocation; resource creation is gated on the per-frame alloc in CreateSceneResources");
			return false;
		}
		if (neededChunkLodLevels > frameResources.chunkLodLevelsCapacity) {
			runtime::LogRuntimeFailure(
				"Render",
				"RefreshLodDownsampledBuffers.ChunkLodCapacity",
				"required chunk-lod-level count exceeds current allocation");
			return false;
		}

		if (frameResources.chunkLodLevelsMappedData != nullptr) {
			for (uint32_t i = 0u; i < chunkCount; ++i) {
				const uint8_t lod = (i < world.chunks.size()) ? world.chunks[i].lodLevel : 0u;
				static_cast<uint32_t *>(frameResources.chunkLodLevelsMappedData)[i] = static_cast<uint32_t>(lod);
			}
		}

		if (frameResources.lodDownsampledVoxelPayloadMappedData != nullptr) {
			std::memset(
				frameResources.lodDownsampledVoxelPayloadMappedData,
				0,
				neededPayloadBytes);
		}
	}

	render->lodDownsampledPayloadVersion += 1u;
	return true;
}

}  // namespace projectv::render

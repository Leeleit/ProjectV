#pragma once

#include "core/Types.hpp"
#include "render/SceneResources.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>

#include <vulkan/vulkan.h>

namespace projectv::render {

struct WorldGenPushConstants {
	std::array<int32_t, 4> chunkOriginAndChunkSize{};
	std::array<uint32_t, 4> chunkCountAndFlags{};
	std::array<float, 4> noiseParams{};
	uint32_t seed = 0u;
	std::array<uint32_t, 3> reserved{};
};
static_assert(sizeof(WorldGenPushConstants) == 64);

inline bool IsWorldGenGpuPipelineRequested()
{
	const char *value = std::getenv("PROJECTV_WORLD_GEN_GPU");
	if (value == nullptr) {
		return true;
	}
	return value[0] == 'O' && value[1] == 'N';
}

inline constexpr VkDeviceSize kWorldGenVoxelBufferBytesPerChunk = sizeof(uint32_t) * 8u * 8u * 8u;

bool CreateWorldGenPipelines(VulkanContextState *context, RenderState *render);

void DestroyWorldGenPipelines(VulkanContextState *context, RenderState *render);

bool RefreshWorldGenResourceBindings(VulkanContextState *context, RenderState *render);

uint32_t BuildActiveChunkIdsForWorldGen(
	const VoxelWorld &world,
	std::vector<uint32_t> &outChunkIds);

bool RecordWorldGenDispatch(
	VkCommandBuffer commandBuffer,
	RenderState &render,
	SceneFrameResources &frameResources,
	const WorldGenPushConstants &pushConstants,
	uint32_t activeChunkCount);

}  // namespace projectv::render

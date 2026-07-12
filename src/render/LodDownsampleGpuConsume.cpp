#include "render/LodDownsampleGpuConsume.hpp"

#include "core/EnvUtils.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "voxel/VoxelLodDownsample.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace projectv::render {

bool IsLodDownsampledGpuConsumeEnabled()
{
	if (const char *value = core::GetEnvVar("PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME")) {
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
	return static_cast<uint32_t>(std::min<VkDeviceSize>(totalBytes, static_cast<VkDeviceSize>(64u) * 1024u * 1024u));
}

uint32_t ComputeChunkLodLevelsCapacity(const uint32_t chunkCount)
{
	return std::max<uint32_t>(chunkCount, 1u);
}

uint32_t LodPayloadWordOffsetForChunk(const uint32_t chunkIndex)
{
	return chunkIndex * kLodPayloadWordStride;
}

uint32_t EncodeChunkLodMetadata(const uint8_t lodLevel, const uint8_t outExtent)
{
	return static_cast<uint32_t>(lodLevel) |
		   static_cast<uint32_t>(outExtent) << 8u;
}

void DecodeChunkLodMetadata(
	const uint32_t metadata,
	uint8_t &outLodLevel,
	uint8_t &outExtent)
{
	outLodLevel = static_cast<uint8_t>(metadata & 0xFFu);
	outExtent = static_cast<uint8_t>(metadata >> 8u & 0xFFu);
}

void BuildLodPayloadWordsFromDownsampled(
	const uint8_t *downsampledBytes,
	const uint32_t byteCount,
	uint32_t *outWords)
{
	if (downsampledBytes == nullptr || outWords == nullptr) {
		return;
	}
	const uint32_t wordCount = (byteCount + 3u) / 4u;
	for (uint32_t w = 0u; w < wordCount; ++w) {
		const uint32_t baseByte = w * 4u;
		uint32_t packed = 0u;
		for (uint32_t b = 0u; b < 4u; ++b) {
			const uint32_t byteIndex = baseByte + b;
			const uint8_t byte = byteIndex < byteCount ? downsampledBytes[byteIndex] : 0u;
			packed |= static_cast<uint32_t>(byte) << (b * 8u);
		}
		outWords[w] = packed;
	}
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

	bool anyLodAboveZero = false;

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
				const uint8_t lod = i < world.chunks.size() ? world.chunks[i].lodLevel : 0u;
				const uint8_t outExtent = lod == 0u
											  ? 0u
											  : static_cast<uint8_t>(voxel::LodDownsampledExtentForLod(lod, static_cast<uint8_t>(chunkSize)));
				static_cast<uint32_t *>(frameResources.chunkLodLevelsMappedData)[i] =
					EncodeChunkLodMetadata(lod, outExtent);
				if (lod > 0u) {
					anyLodAboveZero = true;
				}
			}
		}

		if (frameResources.lodDownsampledVoxelPayloadMappedData != nullptr) {
			std::memset(
				frameResources.lodDownsampledVoxelPayloadMappedData,
				0,
				neededPayloadBytes);
		}

		if (frameResources.lodDownsampledVoxelPayloadMappedData != nullptr) {
			std::vector<uint8_t> downsampled;
			for (uint32_t i = 0u; i < chunkCount; ++i) {
				if (i >= world.chunks.size()) {
					break;
				}
				const VoxelChunk &chunk = world.chunks[i];
				if (chunk.lodLevel == 0u) {
					continue;
				}
				voxel::DownsampleChunkForLodSurfacePreserve(
					world,
					i,
					chunk.lodLevel,
					downsampled);
				const uint32_t chunkWordOffset = LodPayloadWordOffsetForChunk(i);
				const uint32_t byteCount = static_cast<uint32_t>(downsampled.size());
				BuildLodPayloadWordsFromDownsampled(
					downsampled.data(),
					byteCount,
					static_cast<uint32_t *>(frameResources.lodDownsampledVoxelPayloadMappedData) + chunkWordOffset);
			}
		}
	}

	if (anyLodAboveZero) {
		render->lodDownsampledPayloadVersion += 1u;
	}
	return true;
}

} // namespace projectv::render

#include "render/SceneResourcesInternal.hpp"

void InitializeSceneChunkDescriptorsAndVoxelPayloadLayout(
	const VoxelWorld &world,
	RenderState &render)
{
	render.sceneChunkDescriptors.clear();
	render.sceneChunkDescriptors.resize(world.chunks.size());
	render.sceneChunkVoxelPayloadRanges.clear();
	render.sceneChunkVoxelPayloadRanges.resize(world.chunks.size());
	render.sceneNanoVdbVersion = 0;
	render.lastNanoVdbSyncedEditVersion = 0;

	uint32_t voxelWordOffset = 0;
	uint32_t opaqueFaceOffset = 0;
	uint32_t transparentFaceOffset = render.sceneTransparentFaceBase;
	for (size_t chunkIndex = 0; chunkIndex < world.chunks.size(); ++chunkIndex) {
		const VoxelChunk &chunk = world.chunks[chunkIndex];
		const uint32_t extentX = static_cast<uint32_t>(chunk.maxExclusive.x - chunk.min.x);
		const uint32_t extentY = static_cast<uint32_t>(chunk.maxExclusive.y - chunk.min.y);
		const uint32_t extentZ = static_cast<uint32_t>(chunk.maxExclusive.z - chunk.min.z);
		const uint32_t voxelCount = extentX * extentY * extentZ;
		const uint32_t voxelWordCount = GetChunkVoxelWordCount(chunk);

		render.sceneChunkDescriptors[chunkIndex].chunkOrigin = {
			chunk.min.x,
			chunk.min.y,
			chunk.min.z,
			0,
		};
		render.sceneChunkDescriptors[chunkIndex].chunkExtentAndNonAir = {
			extentX,
			extentY,
			extentZ,
			chunk.nonAirVoxelCount,
		};
		render.sceneChunkDescriptors[chunkIndex].voxelDataInfo = {
			voxelWordOffset,
			voxelCount,
			voxelWordCount,
			0u,
		};
		render.sceneChunkDescriptors[chunkIndex].drawRanges = {
			opaqueFaceOffset,
			0u,
			transparentFaceOffset,
			0u,
		};
		render.sceneChunkVoxelPayloadRanges[chunkIndex] = {
			.wordOffset = voxelWordOffset,
			.voxelCount = voxelCount,
			.wordCount = voxelWordCount,
			.reserved = 0u,
		};
		voxelWordOffset += voxelWordCount;
		opaqueFaceOffset += voxelCount * 6u;
		transparentFaceOffset += voxelCount * 6u;
	}

	render.sceneChunkVoxelPayloadWordCount = voxelWordOffset;
	render.sceneChunkVoxelPayloadWords.clear();
	render.sceneChunkVoxelPayloadWords.resize(voxelWordOffset, 0u);
}

void RepackChunkVoxelPayload(
	const VoxelWorld &world,
	const size_t chunkIndex,
	RenderState &render)
{
	if (chunkIndex >= world.chunks.size() ||
		chunkIndex >= render.sceneChunkVoxelPayloadRanges.size() ||
		chunkIndex >= render.sceneChunkDescriptors.size()) {
		return;
	}

	const VoxelChunk &chunk = world.chunks[chunkIndex];
	const SceneChunkVoxelPayloadRange &payloadRange = render.sceneChunkVoxelPayloadRanges[chunkIndex];
	if (payloadRange.wordOffset + payloadRange.wordCount > render.sceneChunkVoxelPayloadWords.size()) {
		return;
	}

	uint32_t *chunkVoxelWords = render.sceneChunkVoxelPayloadWords.data() + payloadRange.wordOffset;
	std::fill_n(chunkVoxelWords, payloadRange.wordCount, 0u);

	uint32_t localVoxelIndex = 0;
	for (int z = chunk.min.z; z < chunk.maxExclusive.z; ++z) {
		for (int y = chunk.min.y; y < chunk.maxExclusive.y; ++y) {
			for (int x = chunk.min.x; x < chunk.maxExclusive.x; ++x) {
				const uint32_t material = static_cast<uint32_t>(GetVoxelMaterial(world, {x, y, z}));
				const uint32_t wordIndex = localVoxelIndex / kVoxelMaterialsPerWord;
				const uint32_t shift = localVoxelIndex % kVoxelMaterialsPerWord * 8u;
				chunkVoxelWords[wordIndex] |= material << shift;
				++localVoxelIndex;
			}
		}
	}

	render.sceneChunkDescriptors[chunkIndex].chunkExtentAndNonAir[3] = chunk.nonAirVoxelCount;
}

void UpdateGeneratedFaceStatsFromFrameResources(
	RenderState &render,
	const SceneFrameResources &frameResources)
{
	if (!frameResources.chunkDescriptorMappedData) {
		render.sceneOpaqueFaceCount = 0;
		render.sceneTransparentFaceCount = 0;
		render.sceneTriangleCount = 0;
		return;
	}

	const auto *chunkDescriptors = static_cast<const PackedSceneChunkDescriptor *>(frameResources.chunkDescriptorMappedData);
	uint64_t opaqueFaceCount = 0;
	uint64_t transparentFaceCount = 0;
	for (uint32_t chunkIndex = 0; chunkIndex < frameResources.chunkDescriptorCount; ++chunkIndex) {
		opaqueFaceCount += chunkDescriptors[chunkIndex].drawRanges[1];
		transparentFaceCount += chunkDescriptors[chunkIndex].drawRanges[3];
	}

	render.sceneOpaqueFaceCount = static_cast<uint32_t>(std::min<uint64_t>(opaqueFaceCount, render.sceneFaceCapacity));
	render.sceneTransparentFaceCount =
		static_cast<uint32_t>(std::min<uint64_t>(transparentFaceCount, render.sceneFaceCapacity));
	render.sceneTriangleCount = (render.sceneOpaqueFaceCount + render.sceneTransparentFaceCount) * 2u;
}

namespace {
struct ChunkVisibilityRebuildResult {
	uint32_t visibleChunkCount = 0;
};

ChunkVisibilityRebuildResult RebuildChunkVisibilityAndFillCache(
	const RenderState &render,
	SceneFrameResources &frameResources,
	const ChunkCullingParameters &parameters,
	ChunkVisibilityCache &cache)
{
	ChunkVisibilityRebuildResult result{};
	if (!frameResources.chunkDescriptorMappedData ||
		!frameResources.opaqueIndirectMappedData ||
		!frameResources.transparentIndirectMappedData) {
		return result;
	}

	const auto *chunkDescriptors = static_cast<const PackedSceneChunkDescriptor *>(frameResources.chunkDescriptorMappedData);
	auto *opaqueCommands = static_cast<VkDrawIndirectCommand *>(frameResources.opaqueIndirectMappedData);
	auto *transparentCommands = static_cast<VkDrawIndirectCommand *>(frameResources.transparentIndirectMappedData);

	const uint32_t chunkDescriptorCount = frameResources.chunkDescriptorCount;
	assert(chunkDescriptorCount <= ChunkVisibilityCache::kChunkVisibilityCacheMaxChunks);
	if (cache.opaqueCommandsSize != chunkDescriptorCount) {
		cache.opaqueCommandsSize = chunkDescriptorCount;
	}
	if (cache.transparentCommandsSize != chunkDescriptorCount) {
		cache.transparentCommandsSize = chunkDescriptorCount;
	}

	for (uint32_t chunkIndex = 0; chunkIndex < chunkDescriptorCount; ++chunkIndex) {
		const PackedSceneChunkDescriptor &chunkDescriptor = chunkDescriptors[chunkIndex];
		const bool visible = IsSceneChunkVisible(chunkDescriptor, parameters);
		if (visible) {
			++result.visibleChunkCount;
		}

		const VkDrawIndirectCommand opaqueCommand = BuildChunkIndirectCommand(
			chunkDescriptor.drawRanges[0],
			chunkDescriptor.drawRanges[1],
			visible);
		const VkDrawIndirectCommand transparentCommand = BuildChunkIndirectCommand(
			chunkDescriptor.drawRanges[2],
			chunkDescriptor.drawRanges[3],
			visible);

		opaqueCommands[chunkIndex] = opaqueCommand;
		cache.opaqueCommands[chunkIndex] = opaqueCommand;
		transparentCommands[chunkIndex] = transparentCommand;
		cache.transparentCommands[chunkIndex] = transparentCommand;
	}

	return result;
}

void ApplyCachedChunkVisibilityCommands(
	const ChunkVisibilityCache &cache,
	SceneFrameResources &frameResources)
{
	if (frameResources.opaqueIndirectMappedData &&
		cache.opaqueCommandsSize == frameResources.chunkDescriptorCount) {
		std::memcpy(
			frameResources.opaqueIndirectMappedData,
			cache.opaqueCommands.data(),
			cache.opaqueCommandsSize * sizeof(VkDrawIndirectCommand));
	}
	if (frameResources.transparentIndirectMappedData &&
		cache.transparentCommandsSize == frameResources.chunkDescriptorCount) {
		std::memcpy(
			frameResources.transparentIndirectMappedData,
			cache.transparentCommands.data(),
			cache.transparentCommandsSize * sizeof(VkDrawIndirectCommand));
	}
}
} // namespace

uint32_t PrepareDirtyChunkMeshingList(
	const RenderState &render,
	SceneFrameResources &frameResources)
{
	if (!frameResources.dirtyChunkIndexMappedData) {
		return 0;
	}

	if (frameResources.meshedSceneVersion == render.sceneVoxelPayloadVersion) {
		return 0;
	}

	auto *dirtyChunkIndices = static_cast<uint32_t *>(frameResources.dirtyChunkIndexMappedData);
	uint32_t dirtyChunkCount = 0;
	const bool canPatchLatestDirtyChunks =
		frameResources.meshedSceneVersion + 1u == render.sceneVoxelPayloadVersion &&
		!render.latestVoxelPayloadChunkIndices.empty();

	if (canPatchLatestDirtyChunks) {
		for (const size_t chunkIndex : render.latestVoxelPayloadChunkIndices) {
			dirtyChunkIndices[dirtyChunkCount++] = static_cast<uint32_t>(chunkIndex);
		}
	} else {
		for (uint32_t chunkIndex = 0; chunkIndex < static_cast<uint32_t>(render.sceneChunkDescriptors.size()); ++chunkIndex) {
			dirtyChunkIndices[dirtyChunkCount++] = chunkIndex;
		}
	}

	frameResources.meshedSceneVersion = render.sceneVoxelPayloadVersion;
	return dirtyChunkCount;
}

bool UpdateSceneFrameChunkVisibility(
	RenderState &render,
	const uint32_t frameIndex,
	const ChunkCullingParameters &parameters)
{
	if (static_cast<size_t>(frameIndex) >= render.sceneFrameResources.size()) {
		return false;
	}

	SceneFrameResources &frameResources = render.sceneFrameResources[frameIndex];
	if (frameResources.chunkCullingMappedData) {
		std::memcpy(frameResources.chunkCullingMappedData, &parameters, sizeof(parameters));
	}

	const uint64_t hash = projectv::visibility_cache::ComputeVisibilityCacheHash(
		parameters,
		render.sceneVoxelPayloadVersion,
		frameResources.chunkDescriptorCount);
	ChunkVisibilityCache &cache = render.chunkVisibilityCache;
	if (cache.valid &&
		cache.hash == hash &&
		cache.chunkDescriptorCount == frameResources.chunkDescriptorCount &&
		cache.sceneVoxelPayloadVersion == render.sceneVoxelPayloadVersion &&
		cache.opaqueCommandsSize == frameResources.chunkDescriptorCount &&
		cache.transparentCommandsSize == frameResources.chunkDescriptorCount) {
		ApplyCachedChunkVisibilityCommands(cache, frameResources);
		++cache.consecutiveHitCount;
		profiling::PlotValue("Visible Chunks", static_cast<int64_t>(cache.visibleChunkCount));
		profiling::PlotValue("Culled Chunks", static_cast<int64_t>(cache.culledChunkCount));
		profiling::PlotValue("ChunkVisibilityCacheHits", static_cast<int64_t>(cache.consecutiveHitCount));
		return true;
	}

	const ChunkVisibilityRebuildResult result = RebuildChunkVisibilityAndFillCache(
		render,
		frameResources,
		parameters,
		cache);
	const uint32_t culledChunkCount =
		frameResources.chunkDescriptorCount > result.visibleChunkCount
			? frameResources.chunkDescriptorCount - result.visibleChunkCount
			: 0u;

	const bool hasGeneratedFaces = frameResources.opaqueFaceCount > 0u ||
								   frameResources.transparentFaceCount > 0u;

	const bool dispatchDoneThisFrame = frameResources.dirtyChunkCount == 0u;
	if (hasGeneratedFaces && dispatchDoneThisFrame) {
		cache.valid = true;
		cache.hash = hash;
		cache.sceneVoxelPayloadVersion = render.sceneVoxelPayloadVersion;
		cache.chunkDescriptorCount = frameResources.chunkDescriptorCount;
		cache.visibleChunkCount = result.visibleChunkCount;
		cache.culledChunkCount = culledChunkCount;
		cache.consecutiveHitCount = 0;
	} else {

		cache.valid = false;
	}
	profiling::PlotValue("Visible Chunks", static_cast<int64_t>(result.visibleChunkCount));
	profiling::PlotValue("Culled Chunks", static_cast<int64_t>(culledChunkCount));
	profiling::PlotValue("ChunkVisibilityCacheHits", static_cast<int64_t>(0));
	return true;
}


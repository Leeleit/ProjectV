import projectv.math;

#include "render/SceneResources.hpp"

#include "render/VoxelMeshingPushConstants.hpp"

#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"
#include "render/ShadowProjection.hpp"
#include "render/Taa.hpp"
#include "render/vulkan/VulkanDebug.hpp"
#include "render/vulkan/VulkanGraphicsPipeline.hpp"
#include "render/vulkan/VulkanVoxelMeshingPipeline.hpp"
#include "voxel/VoxelMaterials.hpp"
#include "voxel/VoxelWorld.hpp"

#include <algorithm>
#include <array>
#include <span>

namespace {
constexpr uint32_t kVoxelMaterialsPerWord = 4u;

std::span<const VoxelMaterialVisual> BuildMaterialVisualTable()
{
	static const std::array<VoxelMaterialVisual, kVoxelMaterialCount> visuals{
		GetVoxelMaterialVisual(VoxelMaterial::Air),
		GetVoxelMaterialVisual(VoxelMaterial::Glass),
		GetVoxelMaterialVisual(VoxelMaterial::Fluid),
		GetVoxelMaterialVisual(VoxelMaterial::FloorWhite),
		GetVoxelMaterialVisual(VoxelMaterial::FloorGray),
	};
	return visuals;
}

VoxelSceneLighting BuildSceneLighting(
	const VoxelWorld &world,
	const RenderState &render)
{
	return BuildVoxelSceneLighting(world.scenePreset, render.lightingDebugControls);
}

void StoreSunShadowProjection(
	VoxelSceneLighting &lighting,
	const uint32_t cascadeIndex,
	const projectv::math::Mat4 &projection)
{
	const size_t matrixOffset = static_cast<size_t>(cascadeIndex) * 16u;
	std::memcpy(
		lighting.sunShadowViewProjections.data() + matrixOffset,
		projection.data(),
		sizeof(projectv::math::Mat4));
}

void StoreSunShadowCascadeProjections(
	VoxelSceneLighting &lighting,
	const SunShadowCascadeProjections &projections)
{
	lighting.sunShadowViewProjections = projections.lightViewProjections;
}

void StoreSunShadowCascadeSplits(
	VoxelSceneLighting &lighting,
	const SunShadowCascadeSplits &splits)
{
	for (uint32_t cascadeIndex = 0; cascadeIndex < kSunShadowCascadeCount; ++cascadeIndex) {
		lighting.shadowCascadeDepthSplits[cascadeIndex] = splits.viewDepthSplits[cascadeIndex];
	}
	lighting.shadowCascadeBlendParams[1] = splits.nearPlane;
	lighting.shadowCascadeBlendParams[2] = 0.0f;
	lighting.shadowCascadeBlendParams[3] = 0.0f;
}

SunShadowCascadeProjectionInputs BuildSunShadowCascadeProjectionInputs(
	const ChunkCullingParameters &parameters,
	const SunShadowCascadeSplits &splits,
	const uint32_t shadowMapResolution)
{
	return {
		.cameraPosition = {
			parameters.cameraPositionAndMaxDistance[0],
			parameters.cameraPositionAndMaxDistance[1],
			parameters.cameraPositionAndMaxDistance[2],
		},
		.cameraForward = {
			parameters.cameraForwardAndTanHalfVerticalFov[0],
			parameters.cameraForwardAndTanHalfVerticalFov[1],
			parameters.cameraForwardAndTanHalfVerticalFov[2],
		},
		.cameraRight = {
			parameters.cameraRightAndTanHalfHorizontalFov[0],
			parameters.cameraRightAndTanHalfHorizontalFov[1],
			parameters.cameraRightAndTanHalfHorizontalFov[2],
		},
		.cameraUp = {
			parameters.cameraUpAndNearPlane[0],
			parameters.cameraUpAndNearPlane[1],
			parameters.cameraUpAndNearPlane[2],
		},
		.tanHalfVerticalFov = parameters.cameraForwardAndTanHalfVerticalFov[3],
		.tanHalfHorizontalFov = parameters.cameraRightAndTanHalfHorizontalFov[3],
		.shadowMapResolution = shadowMapResolution,
		.splits = splits,
	};
}

void RefreshSceneLightingBuffer(
	const VoxelWorld &world,
	RenderState &render,
	const ChunkCullingParameters &shadowProjectionParameters,
	const VkExtent2D renderExtent)
{
	const float shadowReceiverNearPlane =
		std::max(shadowProjectionParameters.cameraUpAndNearPlane[3], 0.01f);
	const float shadowReceiverFarPlane = std::max(
		shadowReceiverNearPlane,
		shadowProjectionParameters.cameraPositionAndMaxDistance[3]);
	render.currentScenePreset = world.scenePreset;
	render.currentSceneLighting = BuildSceneLighting(world, render);
	render.currentSunShadowCascadeSplits = BuildSunShadowCascadeSplits(
		shadowReceiverNearPlane,
		shadowReceiverFarPlane,
		render.sunShadowCascadeSplitLambda);
	const auto [lightViewProjection] = BuildSunShadowProjection(
		world,
		{
			render.currentSceneLighting.sunDirectionAndWrap[0],
			render.currentSceneLighting.sunDirectionAndWrap[1],
			render.currentSceneLighting.sunDirectionAndWrap[2],
		},
		render.lightingDebugControls.shadowCoverageScale);
	for (uint32_t cascadeIndex = 0; cascadeIndex < kSunShadowCascadeCount; ++cascadeIndex) {
		StoreSunShadowProjection(render.currentSceneLighting, cascadeIndex, lightViewProjection);
	}
	const SunShadowCascadeProjectionInputs cascadeInputs = BuildSunShadowCascadeProjectionInputs(
		shadowProjectionParameters,
		render.currentSunShadowCascadeSplits,
		render.shadowMapExtent.width);
	const SunShadowCascadeProjections cascadeProjections = BuildSunShadowCascadeProjections(
		world,
		{
			render.currentSceneLighting.sunDirectionAndWrap[0],
			render.currentSceneLighting.sunDirectionAndWrap[1],
			render.currentSceneLighting.sunDirectionAndWrap[2],
		},
		cascadeInputs,
		render.lightingDebugControls.shadowCoverageScale);
	StoreSunShadowCascadeProjections(render.currentSceneLighting, cascadeProjections);
	render.currentSunShadowCascadeDiagnostics = cascadeProjections.diagnostics;
	StoreSunShadowCascadeSplits(render.currentSceneLighting, render.currentSunShadowCascadeSplits);

	render.currentSceneLighting.taaParams = {
		render.taaJitterX,
		render.taaJitterY,
		render.taaEnabled ? render.taaBlend : 0.0f,
		render.taaEnabled ? 1.0f : 0.0f,
	};
	render.currentSceneLighting.prevViewProjectionMatrix = render.taaPrevViewProjectionMatrix;

	const float texelX = renderExtent.width > 0u
							 ? 1.0f / static_cast<float>(renderExtent.width)
							 : 0.0f;
	const float texelY = renderExtent.height > 0u
							 ? 1.0f / static_cast<float>(renderExtent.height)
							 : 0.0f;
	render.currentSceneLighting.taaHistoryParams = {
		texelX,
		texelY,
		render.taaHistoryValid ? 1.0f : 0.0f,
		static_cast<float>(render.taaNeighbourhoodRadius),
	};
	render.currentSceneLighting.taaLayerHistoryParams = {
		texelX,
		texelY,
		render.taaLayerHistoryValid ? 1.0f : 0.0f,
		render.taaLayerBlendFactor,
	};
}

bool CreateBuffer(
	VulkanContextState *context,
	const VkDeviceSize size,
	const VkBufferUsageFlags usage,
	const VmaAllocationCreateInfo &allocationInfo,
	VkBuffer *outBuffer,
	VmaAllocation *outAllocation,
	VmaAllocationInfo *outAllocationInfo = nullptr)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	return vmaCreateBuffer(
			   context->allocator,
			   &bufferInfo,
			   &allocationInfo,
			   outBuffer,
			   outAllocation,
			   outAllocationInfo) == VK_SUCCESS;
}

uint32_t GetChunkVoxelCount(const VoxelChunk &chunk)
{
	const uint32_t extentX = static_cast<uint32_t>(chunk.maxExclusive.x - chunk.min.x);
	const uint32_t extentY = static_cast<uint32_t>(chunk.maxExclusive.y - chunk.min.y);
	const uint32_t extentZ = static_cast<uint32_t>(chunk.maxExclusive.z - chunk.min.z);
	return extentX * extentY * extentZ;
}

uint32_t GetChunkVoxelWordCount(const VoxelChunk &chunk)
{
	const uint32_t voxelCount = GetChunkVoxelCount(chunk);
	return (voxelCount + kVoxelMaterialsPerWord - 1u) / kVoxelMaterialsPerWord;
}

VkDrawIndirectCommand BuildChunkIndirectCommand(
	const uint32_t firstInstance,
	const uint32_t faceCount,
	const bool visible)
{
	return {
		.vertexCount = 6u,
		.instanceCount = visible ? faceCount : 0u,
		.firstVertex = 0u,
		.firstInstance = firstInstance,
	};
}

uint32_t GetShadowIndirectCommandCount(const uint32_t chunkDescriptorCount)
{
	return chunkDescriptorCount;
}

uint32_t GetShadowIndirectBufferCommandCount(const uint32_t chunkDescriptorCount)
{
	return chunkDescriptorCount * kSunShadowCascadeCount;
}

uint32_t GetMaxSceneFaceCount(const VoxelWorld &world)
{
	const size_t totalCells = static_cast<size_t>(world.width) * world.height * world.depth;
	return static_cast<uint32_t>(totalCells) * 6u;
}

void InitializeSceneChunkDescriptorsAndVoxelPayloadLayout(
	const VoxelWorld &world,
	RenderState &render)
{
	render.sceneChunkDescriptors.clear();
	render.sceneChunkDescriptors.resize(world.chunks.size());
	render.sceneChunkVoxelPayloadRanges.clear();
	render.sceneChunkVoxelPayloadRanges.resize(world.chunks.size());

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
	std::array<uint32_t, kSunShadowCascadeCount> shadowCascadeVisibleChunkCounts{};
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
		!frameResources.shadowIndirectMappedData ||
		!frameResources.transparentIndirectMappedData) {
		return result;
	}

	const auto *chunkDescriptors = static_cast<const PackedSceneChunkDescriptor *>(frameResources.chunkDescriptorMappedData);
	auto *opaqueCommands = static_cast<VkDrawIndirectCommand *>(frameResources.opaqueIndirectMappedData);
	auto *shadowCommands = static_cast<VkDrawIndirectCommand *>(frameResources.shadowIndirectMappedData);
	auto *transparentCommands = static_cast<VkDrawIndirectCommand *>(frameResources.transparentIndirectMappedData);
	result.shadowCascadeVisibleChunkCounts.fill(0u);
	std::array<projectv::math::Mat4, kSunShadowCascadeCount> shadowCascadeMatrices{};
	for (uint32_t cascadeIndex = 0; cascadeIndex < kSunShadowCascadeCount; ++cascadeIndex) {
		const size_t matrixOffset = static_cast<size_t>(cascadeIndex) * 16u;
		std::memcpy(
			shadowCascadeMatrices[cascadeIndex].data(),
			render.currentSceneLighting.sunShadowViewProjections.data() + matrixOffset,
			sizeof(projectv::math::Mat4));
	}

	const uint32_t chunkDescriptorCount = frameResources.chunkDescriptorCount;
	assert(chunkDescriptorCount <= ChunkVisibilityCache::kChunkVisibilityCacheMaxChunks);
	assert(static_cast<size_t>(chunkDescriptorCount) * kSunShadowCascadeCount <=
		   ChunkVisibilityCache::kChunkVisibilityCacheMaxChunks * kSunShadowCascadeCount);
	if (cache.opaqueCommandsSize != chunkDescriptorCount) {
		cache.opaqueCommandsSize = chunkDescriptorCount;
	}
	if (cache.shadowCommandsSize != static_cast<size_t>(chunkDescriptorCount) * kSunShadowCascadeCount) {
		cache.shadowCommandsSize = static_cast<size_t>(chunkDescriptorCount) * kSunShadowCascadeCount;
	}
	if (cache.transparentCommandsSize != chunkDescriptorCount) {
		cache.transparentCommandsSize = chunkDescriptorCount;
	}

	const uint32_t shadowCommandStride = chunkDescriptorCount;
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

		for (uint32_t cascadeIndex = 0; cascadeIndex < kSunShadowCascadeCount; ++cascadeIndex) {
			const bool shadowVisible = IsSceneChunkVisibleInShadowCascade(
				chunkDescriptor,
				shadowCascadeMatrices[cascadeIndex]);
			if (shadowVisible) {
				++result.shadowCascadeVisibleChunkCounts[cascadeIndex];
			}
			const VkDrawIndirectCommand shadowCommand = BuildChunkIndirectCommand(
				chunkDescriptor.drawRanges[0],
				chunkDescriptor.drawRanges[1],
				shadowVisible);
			const size_t shadowSlot = static_cast<size_t>(cascadeIndex) * shadowCommandStride + chunkIndex;
			shadowCommands[shadowSlot] = shadowCommand;
			cache.shadowCommands[shadowSlot] = shadowCommand;
		}
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
	if (frameResources.shadowIndirectMappedData &&
		cache.shadowCommandsSize ==
			static_cast<size_t>(frameResources.chunkDescriptorCount) * kSunShadowCascadeCount) {
		std::memcpy(
			frameResources.shadowIndirectMappedData,
			cache.shadowCommands.data(),
			cache.shadowCommandsSize * sizeof(VkDrawIndirectCommand));
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
} // namespace

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
		cache.shadowCommandsSize ==
			static_cast<size_t>(frameResources.chunkDescriptorCount) * kSunShadowCascadeCount &&
		cache.transparentCommandsSize == frameResources.chunkDescriptorCount) {
		ApplyCachedChunkVisibilityCommands(cache, frameResources);
		frameResources.shadowCascadeVisibleChunkCounts = cache.shadowCascadeVisibleChunkCounts;
		++cache.consecutiveHitCount;
		profiling::PlotValue("Visible Chunks", static_cast<int64_t>(cache.visibleChunkCount));
		profiling::PlotValue("Culled Chunks", static_cast<int64_t>(cache.culledChunkCount));
		profiling::PlotValue("ChunkVisibilityCacheHits", static_cast<int64_t>(cache.consecutiveHitCount));
		return true;
	}

	const auto &[resultVisibleChunkCount, resultShadowCascadeVisibleChunkCounts] = RebuildChunkVisibilityAndFillCache(
		render,
		frameResources,
		parameters,
		cache);
	frameResources.shadowCascadeVisibleChunkCounts = resultShadowCascadeVisibleChunkCounts;
	const uint32_t culledChunkCount =
		frameResources.chunkDescriptorCount > resultVisibleChunkCount
			? frameResources.chunkDescriptorCount - resultVisibleChunkCount
			: 0u;

	const bool hasGeneratedFaces = frameResources.opaqueFaceCount > 0u ||
								   frameResources.transparentFaceCount > 0u;

	const bool dispatchDoneThisFrame = frameResources.dirtyChunkCount == 0u;
	if (hasGeneratedFaces && dispatchDoneThisFrame) {
		cache.valid = true;
		cache.hash = hash;
		cache.sceneVoxelPayloadVersion = render.sceneVoxelPayloadVersion;
		cache.chunkDescriptorCount = frameResources.chunkDescriptorCount;
		cache.visibleChunkCount = resultVisibleChunkCount;
		cache.shadowCascadeVisibleChunkCounts = resultShadowCascadeVisibleChunkCounts;
		cache.culledChunkCount = culledChunkCount;
		cache.consecutiveHitCount = 0;
	} else {

		cache.valid = false;
	}
	profiling::PlotValue("Visible Chunks", static_cast<int64_t>(resultVisibleChunkCount));
	profiling::PlotValue("Culled Chunks", static_cast<int64_t>(culledChunkCount));
	profiling::PlotValue("ChunkVisibilityCacheHits", static_cast<int64_t>(0));
	return true;
}

void DestroySceneResources(
	VulkanContextState *context,
	RenderState *render)
{
	if (!context || !render || !context->allocator) {
		return;
	}

	if (context->device != VK_NULL_HANDLE) {
		const VkResult idleResult = vkDeviceWaitIdle(context->device);
		if (idleResult != VK_SUCCESS) {
			runtime::LogVkFailure(
				"DestroySceneResources.vkDeviceWaitIdle",
				idleResult);
		}
	}

	for (auto &[packedFaceMappedData, packedFaceBuffer, packedFaceAllocation, debugHudVertexMappedData, debugHudVertexBuffer, debugHudVertexAllocation, chunkDescriptorMappedData, chunkDescriptorBuffer, chunkDescriptorAllocation, chunkVoxelPayloadMappedData, chunkVoxelPayloadBuffer, chunkVoxelPayloadAllocation, opaqueIndirectMappedData, opaqueIndirectBuffer, opaqueIndirectAllocation, shadowIndirectMappedData, shadowIndirectBuffer, shadowIndirectAllocation, transparentIndirectMappedData, transparentIndirectBuffer, transparentIndirectAllocation, dirtyChunkIndexMappedData, dirtyChunkIndexBuffer, dirtyChunkIndexAllocation, chunkCullingMappedData, chunkCullingBuffer, chunkCullingAllocation, sceneLightingMappedData, sceneLightingBuffer, sceneLightingAllocation, chunkAabbMappedData, chunkAabbBuffer, chunkAabbAllocation, visibilityMaskMappedData, visibilityMaskBuffer, visibilityMaskAllocation, visibleChunkIdMappedData, visibleChunkIdBuffer, visibleChunkIdAllocation, visibilityCounterMappedData, visibilityCounterBuffer, visibilityCounterAllocation, graphicsDescriptorSet, meshShaderDescriptorSet, shadowDescriptorSet, voxelMeshingDescriptorSet, hizCullingDescriptorSet, uploadedSceneVersion, uploadedVoxelPayloadVersion, meshedSceneVersion, chunkDescriptorCount, shadowIndirectCommandCount, shadowCascadeVisibleChunkCounts, dirtyChunkCount, opaqueFaceCount, transparentFaceCount, debugHudVertexCount] : render->sceneFrameResources) {
		if (packedFaceBuffer && packedFaceAllocation) {
			profiling::RecordFree(packedFaceAllocation, "ScenePackedFaceBufferAllocation");
			vmaDestroyBuffer(context->allocator, packedFaceBuffer, packedFaceAllocation);
		}
		if (debugHudVertexBuffer && debugHudVertexAllocation) {
			profiling::RecordFree(debugHudVertexAllocation, "SceneDebugHudVertexBufferAllocation");
			vmaDestroyBuffer(context->allocator, debugHudVertexBuffer, debugHudVertexAllocation);
		}
		if (chunkDescriptorBuffer && chunkDescriptorAllocation) {
			profiling::RecordFree(chunkDescriptorAllocation, "SceneChunkDescriptorBufferAllocation");
			vmaDestroyBuffer(context->allocator, chunkDescriptorBuffer, chunkDescriptorAllocation);
		}
		if (chunkVoxelPayloadBuffer && chunkVoxelPayloadAllocation) {
			profiling::RecordFree(chunkVoxelPayloadAllocation, "SceneChunkVoxelPayloadBufferAllocation");
			vmaDestroyBuffer(context->allocator, chunkVoxelPayloadBuffer, chunkVoxelPayloadAllocation);
		}
		if (opaqueIndirectBuffer && opaqueIndirectAllocation) {
			profiling::RecordFree(opaqueIndirectAllocation, "SceneOpaqueIndirectBufferAllocation");
			vmaDestroyBuffer(context->allocator, opaqueIndirectBuffer, opaqueIndirectAllocation);
		}
		if (shadowIndirectBuffer && shadowIndirectAllocation) {
			profiling::RecordFree(shadowIndirectAllocation, "SceneShadowIndirectBufferAllocation");
			vmaDestroyBuffer(context->allocator, shadowIndirectBuffer, shadowIndirectAllocation);
		}
		if (transparentIndirectBuffer && transparentIndirectAllocation) {
			profiling::RecordFree(transparentIndirectAllocation, "SceneTransparentIndirectBufferAllocation");
			vmaDestroyBuffer(context->allocator, transparentIndirectBuffer, transparentIndirectAllocation);
		}
		if (dirtyChunkIndexBuffer && dirtyChunkIndexAllocation) {
			profiling::RecordFree(dirtyChunkIndexAllocation, "SceneDirtyChunkIndexBufferAllocation");
			vmaDestroyBuffer(context->allocator, dirtyChunkIndexBuffer, dirtyChunkIndexAllocation);
		}
		if (chunkCullingBuffer && chunkCullingAllocation) {
			profiling::RecordFree(chunkCullingAllocation, "SceneChunkCullingBufferAllocation");
			vmaDestroyBuffer(context->allocator, chunkCullingBuffer, chunkCullingAllocation);
		}
		if (sceneLightingBuffer && sceneLightingAllocation) {
			profiling::RecordFree(sceneLightingAllocation, "VoxelSceneLightingBufferAllocation");
			vmaDestroyBuffer(context->allocator, sceneLightingBuffer, sceneLightingAllocation);
		}
		if (chunkAabbBuffer && chunkAabbAllocation) {
			profiling::RecordFree(chunkAabbAllocation, "SceneChunkAabbBufferAllocation");
			vmaDestroyBuffer(context->allocator, chunkAabbBuffer, chunkAabbAllocation);
		}
		if (visibilityMaskBuffer && visibilityMaskAllocation) {
			profiling::RecordFree(visibilityMaskAllocation, "SceneVisibilityMaskBufferAllocation");
			vmaDestroyBuffer(context->allocator, visibilityMaskBuffer, visibilityMaskAllocation);
		}
		if (visibleChunkIdBuffer && visibleChunkIdAllocation) {
			profiling::RecordFree(visibleChunkIdAllocation, "SceneVisibleChunkIdBufferAllocation");
			vmaDestroyBuffer(context->allocator, visibleChunkIdBuffer, visibleChunkIdAllocation);
		}
		if (visibilityCounterBuffer && visibilityCounterAllocation) {
			profiling::RecordFree(visibilityCounterAllocation, "SceneVisibilityCounterBufferAllocation");
			vmaDestroyBuffer(context->allocator, visibilityCounterBuffer, visibilityCounterAllocation);
		}

		packedFaceMappedData = nullptr;
		packedFaceBuffer = VK_NULL_HANDLE;
		packedFaceAllocation = VK_NULL_HANDLE;
		debugHudVertexMappedData = nullptr;
		debugHudVertexBuffer = VK_NULL_HANDLE;
		debugHudVertexAllocation = VK_NULL_HANDLE;
		chunkDescriptorMappedData = nullptr;
		chunkDescriptorBuffer = VK_NULL_HANDLE;
		chunkDescriptorAllocation = VK_NULL_HANDLE;
		chunkVoxelPayloadMappedData = nullptr;
		chunkVoxelPayloadBuffer = VK_NULL_HANDLE;
		chunkVoxelPayloadAllocation = VK_NULL_HANDLE;
		opaqueIndirectMappedData = nullptr;
		opaqueIndirectBuffer = VK_NULL_HANDLE;
		opaqueIndirectAllocation = VK_NULL_HANDLE;
		shadowIndirectMappedData = nullptr;
		shadowIndirectBuffer = VK_NULL_HANDLE;
		shadowIndirectAllocation = VK_NULL_HANDLE;
		transparentIndirectMappedData = nullptr;
		transparentIndirectBuffer = VK_NULL_HANDLE;
		transparentIndirectAllocation = VK_NULL_HANDLE;
		dirtyChunkIndexMappedData = nullptr;
		dirtyChunkIndexBuffer = VK_NULL_HANDLE;
		dirtyChunkIndexAllocation = VK_NULL_HANDLE;
		chunkCullingMappedData = nullptr;
		chunkCullingBuffer = VK_NULL_HANDLE;
		chunkCullingAllocation = VK_NULL_HANDLE;
sceneLightingMappedData = nullptr;
		sceneLightingBuffer = VK_NULL_HANDLE;
		sceneLightingAllocation = nullptr;
		chunkAabbMappedData = nullptr;
		chunkAabbBuffer = VK_NULL_HANDLE;
		chunkAabbAllocation = nullptr;
		visibilityMaskMappedData = nullptr;
		visibilityMaskBuffer = VK_NULL_HANDLE;
		visibilityMaskAllocation = nullptr;
		visibleChunkIdMappedData = nullptr;
		visibleChunkIdBuffer = VK_NULL_HANDLE;
		visibleChunkIdAllocation = nullptr;
		visibilityCounterMappedData = nullptr;
		visibilityCounterBuffer = VK_NULL_HANDLE;
		visibilityCounterAllocation = nullptr;
		graphicsDescriptorSet = VK_NULL_HANDLE;
		meshShaderDescriptorSet = VK_NULL_HANDLE;
		shadowDescriptorSet = VK_NULL_HANDLE;
		voxelMeshingDescriptorSet = VK_NULL_HANDLE;
		hizCullingDescriptorSet = VK_NULL_HANDLE;
		uploadedSceneVersion = 0;
		uploadedVoxelPayloadVersion = 0;
		meshedSceneVersion = 0;
		chunkDescriptorCount = 0;
		shadowIndirectCommandCount = 0;
		shadowCascadeVisibleChunkCounts = {};
		dirtyChunkCount = 0;
		opaqueFaceCount = 0;
		transparentFaceCount = 0;
		debugHudVertexCount = 0;
	}

	if (render->materialVisualBuffer && render->materialVisualAllocation) {
		profiling::RecordFree(render->materialVisualAllocation, "VoxelMaterialVisualBufferAllocation");
		vmaDestroyBuffer(context->allocator, render->materialVisualBuffer, render->materialVisualAllocation);
	}
	render->materialVisualMappedData = nullptr;
	render->materialVisualBuffer = VK_NULL_HANDLE;
	render->materialVisualAllocation = VK_NULL_HANDLE;
	render->currentSceneLighting = {};

	render->sceneFaceCapacity = 0;
	render->sceneTransparentFaceBase = 0;
	render->sceneOpaqueFaceCount = 0;
	render->sceneTransparentFaceCount = 0;
	render->sceneChunkVoxelPayloadWordCount = 0;
	render->sceneUploadVersion = 0;
	render->sceneVoxelPayloadVersion = 0;
	render->sceneTriangleCount = 0;
	render->sceneMemoryBytes = 0;
	render->sceneChunkDescriptors.clear();
	render->sceneChunkVoxelPayloadRanges.clear();
	render->sceneChunkVoxelPayloadWords.clear();
	render->latestVoxelPayloadChunkIndices.clear();
	render->pendingChunkRebuildIndices.clear();
	render->completedChunkRebuildIndices.clear();
}

bool CreateSceneResources(
	VulkanContextState *context,
	WorldState *world,
	RenderState *render)
{
	if (!context || !world || !render || !context->allocator || !world->voxelWorld) {
		return false;
	}
	if (world->voxelWorld->chunkSize > 255) {
		SDL_Log("Chunk size %d exceeds packed scene face payload limit", world->voxelWorld->chunkSize);
		return false;
	}

	DestroySceneResources(context, render);
	render->sceneFaceCapacity = GetMaxSceneFaceCount(*world->voxelWorld);
	render->sceneTransparentFaceBase = render->sceneFaceCapacity;
	InitializeSceneChunkDescriptorsAndVoxelPayloadLayout(*world->voxelWorld, *render);
	render->sceneMemoryBytes = 0;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.flags =
		VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
		VMA_ALLOCATION_CREATE_MAPPED_BIT;
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

	{
		VmaAllocationInfo materialVisualAllocationInfo{};
		if (!CreateBuffer(
				context,
				sizeof(VoxelMaterialVisual) * kVoxelMaterialCount,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&render->materialVisualBuffer,
				&render->materialVisualAllocation,
				&materialVisualAllocationInfo)) {
			DestroySceneResources(context, render);
			return false;
		}
		render->materialVisualMappedData = materialVisualAllocationInfo.pMappedData;
		profiling::RecordAllocation(
			render->materialVisualAllocation,
			materialVisualAllocationInfo.size,
			"VoxelMaterialVisualBufferAllocation");
		render->sceneMemoryBytes += materialVisualAllocationInfo.size;
		const std::span<const VoxelMaterialVisual> materialVisuals = BuildMaterialVisualTable();
		std::memcpy(
			render->materialVisualMappedData,
			materialVisuals.data(),
			materialVisuals.size_bytes());
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(render->materialVisualBuffer),
			VK_OBJECT_TYPE_BUFFER,
			"VoxelMaterialVisualBuffer");
	}

	render->currentSceneLighting = BuildSceneLighting(*world->voxelWorld, *render);
	render->currentSunShadowCascadeSplits = BuildSunShadowCascadeSplits(
		render->currentSunShadowCascadeSplits.nearPlane,
		render->currentSunShadowCascadeSplits.farPlane,
		render->sunShadowCascadeSplitLambda);
	const auto [initialLightViewProjection] = BuildSunShadowProjection(
		*world->voxelWorld,
		{
			render->currentSceneLighting.sunDirectionAndWrap[0],
			render->currentSceneLighting.sunDirectionAndWrap[1],
			render->currentSceneLighting.sunDirectionAndWrap[2],
		},
		render->lightingDebugControls.shadowCoverageScale);
	for (uint32_t cascadeIndex = 0; cascadeIndex < kSunShadowCascadeCount; ++cascadeIndex) {
		StoreSunShadowProjection(render->currentSceneLighting, cascadeIndex, initialLightViewProjection);
	}
	StoreSunShadowCascadeSplits(render->currentSceneLighting, render->currentSunShadowCascadeSplits);

	for (SceneFrameResources &frameResources : render->sceneFrameResources) {
		{
			VmaAllocationInfo sceneLightingAllocationInfo{};
			if (!CreateBuffer(
					context,
					sizeof(VoxelSceneLighting),
					VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					allocationInfo,
					&frameResources.sceneLightingBuffer,
					&frameResources.sceneLightingAllocation,
					&sceneLightingAllocationInfo)) {
				DestroySceneResources(context, render);
				return false;
			}
			frameResources.sceneLightingMappedData = sceneLightingAllocationInfo.pMappedData;
			profiling::RecordAllocation(
				frameResources.sceneLightingAllocation,
				sceneLightingAllocationInfo.size,
				"VoxelSceneLightingBufferAllocation");
			render->sceneMemoryBytes += sceneLightingAllocationInfo.size;
		}
		std::memcpy(
			frameResources.sceneLightingMappedData,
			&render->currentSceneLighting,
			sizeof(render->currentSceneLighting));

		VmaAllocationInfo packedFaceAllocationInfo{};
		if (!CreateBuffer(
				context,
				sizeof(PackedSceneVoxelFace) *
					static_cast<VkDeviceSize>(render->sceneFaceCapacity * 2u),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.packedFaceBuffer,
				&frameResources.packedFaceAllocation,
				&packedFaceAllocationInfo)) {
			DestroySceneResources(context, render);
			return false;
		}
		frameResources.packedFaceMappedData = packedFaceAllocationInfo.pMappedData;
		profiling::RecordAllocation(
			frameResources.packedFaceAllocation,
			packedFaceAllocationInfo.size,
			"ScenePackedFaceBufferAllocation");
		render->sceneMemoryBytes += packedFaceAllocationInfo.size;

		VmaAllocationInfo debugHudVertexAllocationInfo{};
		if (!CreateBuffer(
				context,
				sizeof(DebugHudVertex) * static_cast<VkDeviceSize>(DEBUG_HUD_MAX_VERTEX_COUNT),
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				allocationInfo,
				&frameResources.debugHudVertexBuffer,
				&frameResources.debugHudVertexAllocation,
				&debugHudVertexAllocationInfo)) {
			DestroySceneResources(context, render);
			return false;
		}
		frameResources.debugHudVertexMappedData = debugHudVertexAllocationInfo.pMappedData;
		profiling::RecordAllocation(
			frameResources.debugHudVertexAllocation,
			debugHudVertexAllocationInfo.size,
			"SceneDebugHudVertexBufferAllocation");
		render->sceneMemoryBytes += debugHudVertexAllocationInfo.size;

		VmaAllocationInfo chunkDescriptorAllocationInfo{};
		if (!CreateBuffer(
				context,
				sizeof(PackedSceneChunkDescriptor) * world->voxelWorld->chunks.size(),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.chunkDescriptorBuffer,
				&frameResources.chunkDescriptorAllocation,
				&chunkDescriptorAllocationInfo)) {
			DestroySceneResources(context, render);
			return false;
		}
		frameResources.chunkDescriptorMappedData = chunkDescriptorAllocationInfo.pMappedData;
		profiling::RecordAllocation(
			frameResources.chunkDescriptorAllocation,
			chunkDescriptorAllocationInfo.size,
			"SceneChunkDescriptorBufferAllocation");
		render->sceneMemoryBytes += chunkDescriptorAllocationInfo.size;

		VmaAllocationInfo chunkVoxelPayloadAllocationInfo{};
		if (!CreateBuffer(
				context,
				sizeof(uint32_t) * static_cast<VkDeviceSize>(render->sceneChunkVoxelPayloadWordCount),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.chunkVoxelPayloadBuffer,
				&frameResources.chunkVoxelPayloadAllocation,
				&chunkVoxelPayloadAllocationInfo)) {
			DestroySceneResources(context, render);
			return false;
		}
		frameResources.chunkVoxelPayloadMappedData = chunkVoxelPayloadAllocationInfo.pMappedData;
		profiling::RecordAllocation(
			frameResources.chunkVoxelPayloadAllocation,
			chunkVoxelPayloadAllocationInfo.size,
			"SceneChunkVoxelPayloadBufferAllocation");
		render->sceneMemoryBytes += chunkVoxelPayloadAllocationInfo.size;

		const uint32_t visibleChunkIdCapacity = static_cast<uint32_t>(world->voxelWorld->chunks.size());
		render->visibleChunkIdCapacity = visibleChunkIdCapacity;
		VmaAllocationInfo visibleChunkIdAllocationInfo{};
		if (!CreateBuffer(
				context,
				sizeof(uint32_t) * static_cast<VkDeviceSize>(std::max(visibleChunkIdCapacity, 1u)),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.visibleChunkIdBuffer,
				&frameResources.visibleChunkIdAllocation,
				&visibleChunkIdAllocationInfo)) {
			DestroySceneResources(context, render);
			return false;
		}
		frameResources.visibleChunkIdMappedData = visibleChunkIdAllocationInfo.pMappedData;
		profiling::RecordAllocation(
			frameResources.visibleChunkIdAllocation,
			visibleChunkIdAllocationInfo.size,
			"SceneVisibleChunkIdBufferAllocation");
		render->sceneMemoryBytes += visibleChunkIdAllocationInfo.size;

		VmaAllocationInfo visibilityCounterAllocationInfo{};
		if (!CreateBuffer(
				context,
				sizeof(uint32_t),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.visibilityCounterBuffer,
				&frameResources.visibilityCounterAllocation,
				&visibilityCounterAllocationInfo)) {
			DestroySceneResources(context, render);
			return false;
		}
		frameResources.visibilityCounterMappedData = visibilityCounterAllocationInfo.pMappedData;
		profiling::RecordAllocation(
			frameResources.visibilityCounterAllocation,
			visibilityCounterAllocationInfo.size,
			"SceneVisibilityCounterBufferAllocation");
		render->sceneMemoryBytes += visibilityCounterAllocationInfo.size;
		std::memset(frameResources.visibilityCounterMappedData, 0, sizeof(uint32_t));

		VmaAllocationInfo opaqueIndirectAllocationInfo{};
		if (!CreateBuffer(
				context,
				sizeof(VkDrawIndirectCommand) * world->voxelWorld->chunks.size(),
				VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.opaqueIndirectBuffer,
				&frameResources.opaqueIndirectAllocation,
				&opaqueIndirectAllocationInfo)) {
			DestroySceneResources(context, render);
			return false;
		}
		frameResources.opaqueIndirectMappedData = opaqueIndirectAllocationInfo.pMappedData;
		profiling::RecordAllocation(
			frameResources.opaqueIndirectAllocation,
			opaqueIndirectAllocationInfo.size,
			"SceneOpaqueIndirectBufferAllocation");
		render->sceneMemoryBytes += opaqueIndirectAllocationInfo.size;
		std::memset(
			frameResources.opaqueIndirectMappedData,
			0,
			sizeof(VkDrawIndirectCommand) * world->voxelWorld->chunks.size());

		VmaAllocationInfo shadowIndirectAllocationInfo{};
		if (!CreateBuffer(
				context,
				sizeof(VkDrawIndirectCommand) *
					static_cast<VkDeviceSize>(GetShadowIndirectBufferCommandCount(
						static_cast<uint32_t>(world->voxelWorld->chunks.size()))),
				VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.shadowIndirectBuffer,
				&frameResources.shadowIndirectAllocation,
				&shadowIndirectAllocationInfo)) {
			DestroySceneResources(context, render);
			return false;
		}
		frameResources.shadowIndirectMappedData = shadowIndirectAllocationInfo.pMappedData;
		profiling::RecordAllocation(
			frameResources.shadowIndirectAllocation,
			shadowIndirectAllocationInfo.size,
			"SceneShadowIndirectBufferAllocation");
		render->sceneMemoryBytes += shadowIndirectAllocationInfo.size;
		std::memset(
			frameResources.shadowIndirectMappedData,
			0,
			sizeof(VkDrawIndirectCommand) *
				static_cast<size_t>(GetShadowIndirectBufferCommandCount(
					static_cast<uint32_t>(world->voxelWorld->chunks.size()))));

		VmaAllocationInfo transparentIndirectAllocationInfo{};
		if (!CreateBuffer(
				context,
				sizeof(VkDrawIndirectCommand) * world->voxelWorld->chunks.size(),
				VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.transparentIndirectBuffer,
				&frameResources.transparentIndirectAllocation,
				&transparentIndirectAllocationInfo)) {
			DestroySceneResources(context, render);
			return false;
		}
		frameResources.transparentIndirectMappedData = transparentIndirectAllocationInfo.pMappedData;
		profiling::RecordAllocation(
			frameResources.transparentIndirectAllocation,
			transparentIndirectAllocationInfo.size,
			"SceneTransparentIndirectBufferAllocation");
		render->sceneMemoryBytes += transparentIndirectAllocationInfo.size;
		std::memset(
			frameResources.transparentIndirectMappedData,
			0,
			sizeof(VkDrawIndirectCommand) * world->voxelWorld->chunks.size());

		VmaAllocationInfo dirtyChunkIndexAllocationInfo{};
		if (!CreateBuffer(
				context,
				sizeof(uint32_t) * world->voxelWorld->chunks.size(),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.dirtyChunkIndexBuffer,
				&frameResources.dirtyChunkIndexAllocation,
				&dirtyChunkIndexAllocationInfo)) {
			DestroySceneResources(context, render);
			return false;
		}
		frameResources.dirtyChunkIndexMappedData = dirtyChunkIndexAllocationInfo.pMappedData;
		profiling::RecordAllocation(
			frameResources.dirtyChunkIndexAllocation,
			dirtyChunkIndexAllocationInfo.size,
			"SceneDirtyChunkIndexBufferAllocation");
		render->sceneMemoryBytes += dirtyChunkIndexAllocationInfo.size;
		std::memset(
			frameResources.dirtyChunkIndexMappedData,
			0,
			sizeof(uint32_t) * world->voxelWorld->chunks.size());

		VmaAllocationInfo chunkCullingAllocationInfo{};
		if (!CreateBuffer(
				context,
				sizeof(ChunkCullingParameters),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.chunkCullingBuffer,
				&frameResources.chunkCullingAllocation,
				&chunkCullingAllocationInfo)) {
			DestroySceneResources(context, render);
			return false;
		}
		frameResources.chunkCullingMappedData = chunkCullingAllocationInfo.pMappedData;
		profiling::RecordAllocation(
			frameResources.chunkCullingAllocation,
			chunkCullingAllocationInfo.size,
			"SceneChunkCullingBufferAllocation");
		render->sceneMemoryBytes += chunkCullingAllocationInfo.size;
		std::memset(frameResources.chunkCullingMappedData, 0, sizeof(ChunkCullingParameters));

		const VkDeviceSize chunkAabbStrideBytes = sizeof(PackedSceneChunkAabb);
		const VkDeviceSize chunkAabbBufferBytes = chunkAabbStrideBytes *
			static_cast<VkDeviceSize>(world->voxelWorld->chunks.size());
		if (chunkAabbBufferBytes > 0) {
			VmaAllocationInfo chunkAabbAllocationInfo{};
			if (!CreateBuffer(
					context,
					chunkAabbBufferBytes,
					VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					allocationInfo,
					&frameResources.chunkAabbBuffer,
					&frameResources.chunkAabbAllocation,
					&chunkAabbAllocationInfo)) {
				DestroySceneResources(context, render);
				return false;
			}
			frameResources.chunkAabbMappedData = chunkAabbAllocationInfo.pMappedData;
			profiling::RecordAllocation(
				frameResources.chunkAabbAllocation,
				chunkAabbAllocationInfo.size,
				"SceneChunkAabbBufferAllocation");
			render->sceneMemoryBytes += chunkAabbAllocationInfo.size;
			std::memset(frameResources.chunkAabbMappedData, 0, chunkAabbBufferBytes);
		}

		const uint32_t visibilityMaskWordCount = (static_cast<uint32_t>(
			world->voxelWorld->chunks.size()) + 31u) / 32u;
		if (visibilityMaskWordCount > 0u) {
			VmaAllocationInfo visibilityMaskAllocationInfo{};
			if (!CreateBuffer(
					context,
					sizeof(uint32_t) * visibilityMaskWordCount,
					VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					allocationInfo,
					&frameResources.visibilityMaskBuffer,
					&frameResources.visibilityMaskAllocation,
					&visibilityMaskAllocationInfo)) {
				DestroySceneResources(context, render);
				return false;
			}
			frameResources.visibilityMaskMappedData = visibilityMaskAllocationInfo.pMappedData;
			profiling::RecordAllocation(
				frameResources.visibilityMaskAllocation,
				visibilityMaskAllocationInfo.size,
				"SceneVisibilityMaskBufferAllocation");
			render->sceneMemoryBytes += visibilityMaskAllocationInfo.size;
			std::memset(
				frameResources.visibilityMaskMappedData,
				0,
				sizeof(uint32_t) * visibilityMaskWordCount);
		}

		char bufferName[64]{};
		const size_t frameResourceIndex = static_cast<size_t>(&frameResources - render->sceneFrameResources.data());

		std::snprintf(bufferName, sizeof(bufferName), "ScenePackedFaceBuffer[%zu]", frameResourceIndex);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frameResources.packedFaceBuffer),
			VK_OBJECT_TYPE_BUFFER,
			bufferName);
		std::snprintf(bufferName, sizeof(bufferName), "SceneDebugHudVertexBuffer[%zu]", frameResourceIndex);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frameResources.debugHudVertexBuffer),
			VK_OBJECT_TYPE_BUFFER,
			bufferName);
		std::snprintf(bufferName, sizeof(bufferName), "SceneChunkDescriptorBuffer[%zu]", frameResourceIndex);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frameResources.chunkDescriptorBuffer),
			VK_OBJECT_TYPE_BUFFER,
			bufferName);
		std::snprintf(bufferName, sizeof(bufferName), "SceneChunkVoxelPayloadBuffer[%zu]", frameResourceIndex);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frameResources.chunkVoxelPayloadBuffer),
			VK_OBJECT_TYPE_BUFFER,
			bufferName);
		std::snprintf(bufferName, sizeof(bufferName), "SceneOpaqueIndirectBuffer[%zu]", frameResourceIndex);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frameResources.opaqueIndirectBuffer),
			VK_OBJECT_TYPE_BUFFER,
			bufferName);
		std::snprintf(bufferName, sizeof(bufferName), "SceneShadowIndirectBuffer[%zu]", frameResourceIndex);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frameResources.shadowIndirectBuffer),
			VK_OBJECT_TYPE_BUFFER,
			bufferName);
		std::snprintf(bufferName, sizeof(bufferName), "SceneTransparentIndirectBuffer[%zu]", frameResourceIndex);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frameResources.transparentIndirectBuffer),
			VK_OBJECT_TYPE_BUFFER,
			bufferName);
		std::snprintf(bufferName, sizeof(bufferName), "SceneDirtyChunkIndexBuffer[%zu]", frameResourceIndex);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frameResources.dirtyChunkIndexBuffer),
			VK_OBJECT_TYPE_BUFFER,
			bufferName);
		std::snprintf(bufferName, sizeof(bufferName), "SceneChunkCullingBuffer[%zu]", frameResourceIndex);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frameResources.chunkCullingBuffer),
			VK_OBJECT_TYPE_BUFFER,
			bufferName);
		frameResources.debugHudVertexCount = 0;
	}

	render->sceneOpaqueFaceCount = 0;
	render->sceneTransparentFaceCount = 0;
	render->sceneTriangleCount = 0;
	render->sceneUploadVersion = 1;
	render->sceneVoxelPayloadVersion = 0;
	render->latestVoxelPayloadChunkIndices.clear();
	render->latestVoxelPayloadChunkIndices.reserve(world->voxelWorld->chunks.size());
	render->pendingChunkRebuildIndices.clear();
	render->pendingChunkRebuildIndices.reserve(world->voxelWorld->chunks.size());
	render->completedChunkRebuildIndices.clear();
	render->completedChunkRebuildIndices.reserve(world->voxelWorld->chunks.size());

	if (!RefreshGraphicsResourceBindings(context, render)) {
		DestroySceneResources(context, render);
		return false;
	}
	if (!RefreshVoxelMeshingResourceBindings(context, render)) {
		DestroySceneResources(context, render);
		return false;
	}
	return true;
}

bool UpdateSceneResources(
	WorldState *world,
	RenderState *render,
	const ChunkCullingParameters &shadowProjectionParameters,
	const VkExtent2D swapchainExtent)
{
	PV_PROFILE_ZONE_N("UpdateSceneResources");
	if (!world || !render || !world->voxelWorld) {
		return false;
	}

	const uint32_t dirtyChunkCount = world->voxelWorld->stats.dirtyChunkCount;
	const uint32_t activeChunkCount = world->voxelWorld->stats.activeChunkCount;
	uint32_t rebuiltChunkCount = 0;
	uint64_t repackedVoxelCount = 0;

	if (render->sceneChunkDescriptors.size() != world->voxelWorld->chunks.size() ||
		render->sceneChunkVoxelPayloadRanges.size() != world->voxelWorld->chunks.size()) {
		render->sceneFaceCapacity = GetMaxSceneFaceCount(*world->voxelWorld);
		render->sceneTransparentFaceBase = render->sceneFaceCapacity;
		InitializeSceneChunkDescriptorsAndVoxelPayloadLayout(*world->voxelWorld, *render);
		++render->sceneUploadVersion;
		++render->sceneVoxelPayloadVersion;
	}

	render->completedChunkRebuildIndices.clear();
	render->latestVoxelPayloadChunkIndices.clear();
	for (const size_t chunkIndex : render->pendingChunkRebuildIndices) {
		if (chunkIndex >= world->voxelWorld->chunks.size()) {
			continue;
		}

		RepackChunkVoxelPayload(*world->voxelWorld, chunkIndex, *render);
		++rebuiltChunkCount;
		repackedVoxelCount += GetChunkVoxelCount(world->voxelWorld->chunks[chunkIndex]);
		render->latestVoxelPayloadChunkIndices.push_back(chunkIndex);
		render->completedChunkRebuildIndices.push_back(chunkIndex);
	}
	render->pendingChunkRebuildIndices.clear();

	if (!render->completedChunkRebuildIndices.empty()) {

		++render->sceneVoxelPayloadVersion;
	}

	RefreshSceneLightingBuffer(*world->voxelWorld, *render, shadowProjectionParameters, swapchainExtent);

	profiling::PlotValue("Dirty Chunks", static_cast<int64_t>(dirtyChunkCount));
	profiling::PlotValue("Active Chunks", static_cast<int64_t>(activeChunkCount));
	profiling::PlotValue("Rebuilt Chunks", static_cast<int64_t>(rebuiltChunkCount));
	profiling::PlotValue("Repacked Chunk Voxels", static_cast<int64_t>(repackedVoxelCount));
	profiling::PlotValue(
		"Scene Exposure x100",
		static_cast<int64_t>(render->currentSceneLighting.postProcess[0] * 100.0f));
	return true;
}

bool UploadSceneFrameResources(
	RenderState &render,
	const uint32_t frameIndex)
{
	PV_PROFILE_ZONE_N("UploadSceneFrameResources");
	if (static_cast<size_t>(frameIndex) >= render.sceneFrameResources.size()) {
		return false;
	}

	SceneFrameResources &frameResources = render.sceneFrameResources[frameIndex];
	if (frameResources.sceneLightingMappedData) {
		std::memcpy(
			frameResources.sceneLightingMappedData,
			&render.currentSceneLighting,
			sizeof(render.currentSceneLighting));
	}
	UpdateGeneratedFaceStatsFromFrameResources(render, frameResources);

	uint32_t uploadedChunkDescriptorCount = 0;
	uint32_t uploadedVoxelChunkCount = 0;
	uint32_t uploadedChunkVoxelWordCount = 0;
	uint32_t meshingDirtyChunkCount = 0;
	bool descriptorBufferUploaded = false;

	if (frameResources.uploadedSceneVersion != render.sceneUploadVersion) {
		if (!render.sceneChunkDescriptors.empty() && frameResources.chunkDescriptorMappedData) {
			auto *uploadedChunkDescriptors =
				static_cast<PackedSceneChunkDescriptor *>(frameResources.chunkDescriptorMappedData);
			const bool preserveGeneratedFaceCounts =
				frameResources.chunkDescriptorCount == render.sceneChunkDescriptors.size();
			for (size_t chunkIndex = 0; chunkIndex < render.sceneChunkDescriptors.size(); ++chunkIndex) {
				const PackedSceneChunkDescriptor *existingDescriptor =
					preserveGeneratedFaceCounts ? &uploadedChunkDescriptors[chunkIndex] : nullptr;
				uploadedChunkDescriptors[chunkIndex] = MakeUploadedSceneChunkDescriptor(
					render.sceneChunkDescriptors[chunkIndex],
					existingDescriptor);
			}
			uploadedChunkDescriptorCount = static_cast<uint32_t>(render.sceneChunkDescriptors.size());
			descriptorBufferUploaded = true;
		}
		frameResources.uploadedSceneVersion = render.sceneUploadVersion;
	}

	if (frameResources.uploadedVoxelPayloadVersion != render.sceneVoxelPayloadVersion) {
		const bool canPatchLatestDirtyChunks =
			frameResources.uploadedVoxelPayloadVersion + 1u == render.sceneVoxelPayloadVersion &&
			!render.latestVoxelPayloadChunkIndices.empty();

		if (canPatchLatestDirtyChunks) {
			for (const size_t chunkIndex : render.latestVoxelPayloadChunkIndices) {
				if (chunkIndex >= render.sceneChunkVoxelPayloadRanges.size()) {
					continue;
				}

				const SceneChunkVoxelPayloadRange &payloadRange = render.sceneChunkVoxelPayloadRanges[chunkIndex];
				if (payloadRange.wordOffset + payloadRange.wordCount > render.sceneChunkVoxelPayloadWords.size()) {
					continue;
				}

				if (payloadRange.wordCount > 0) {
					std::memcpy(
						static_cast<uint32_t *>(frameResources.chunkVoxelPayloadMappedData) + payloadRange.wordOffset,
						render.sceneChunkVoxelPayloadWords.data() + payloadRange.wordOffset,
						static_cast<size_t>(payloadRange.wordCount) * sizeof(uint32_t));
					uploadedChunkVoxelWordCount += payloadRange.wordCount;
				}

				if (!descriptorBufferUploaded && frameResources.chunkDescriptorMappedData) {
					auto *uploadedChunkDescriptors =
						static_cast<PackedSceneChunkDescriptor *>(frameResources.chunkDescriptorMappedData);
					uploadedChunkDescriptors[chunkIndex] = MakeUploadedSceneChunkDescriptor(
						render.sceneChunkDescriptors[chunkIndex],
						&uploadedChunkDescriptors[chunkIndex]);
				}

				++uploadedVoxelChunkCount;
			}
		} else {
			if (!render.sceneChunkVoxelPayloadWords.empty()) {
				std::memcpy(
					frameResources.chunkVoxelPayloadMappedData,
					render.sceneChunkVoxelPayloadWords.data(),
					render.sceneChunkVoxelPayloadWords.size() * sizeof(uint32_t));
				uploadedChunkVoxelWordCount = render.sceneChunkVoxelPayloadWordCount;
			}

			if (!descriptorBufferUploaded &&
				!render.sceneChunkDescriptors.empty() &&
				frameResources.chunkDescriptorMappedData) {
				auto *uploadedChunkDescriptors =
					static_cast<PackedSceneChunkDescriptor *>(frameResources.chunkDescriptorMappedData);
				const bool preserveGeneratedFaceCounts =
					frameResources.chunkDescriptorCount == render.sceneChunkDescriptors.size();
				for (size_t chunkIndex = 0; chunkIndex < render.sceneChunkDescriptors.size(); ++chunkIndex) {
					const PackedSceneChunkDescriptor *existingDescriptor =
						preserveGeneratedFaceCounts ? &uploadedChunkDescriptors[chunkIndex] : nullptr;
					uploadedChunkDescriptors[chunkIndex] = MakeUploadedSceneChunkDescriptor(
						render.sceneChunkDescriptors[chunkIndex],
						existingDescriptor);
				}
			}

			uploadedVoxelChunkCount = static_cast<uint32_t>(render.sceneChunkVoxelPayloadRanges.size());
		}

		frameResources.uploadedVoxelPayloadVersion = render.sceneVoxelPayloadVersion;
	}

	frameResources.chunkDescriptorCount = static_cast<uint32_t>(render.sceneChunkDescriptors.size());
	frameResources.shadowIndirectCommandCount = GetShadowIndirectCommandCount(frameResources.chunkDescriptorCount);
	frameResources.dirtyChunkCount = PrepareDirtyChunkMeshingList(render, frameResources);
	frameResources.opaqueFaceCount = render.sceneOpaqueFaceCount;
	frameResources.transparentFaceCount = render.sceneTransparentFaceCount;
	meshingDirtyChunkCount = frameResources.dirtyChunkCount;

	profiling::PlotValue("Scene Triangles", static_cast<int64_t>(render.sceneTriangleCount));
	profiling::PlotValue("Generated Opaque Faces", static_cast<int64_t>(render.sceneOpaqueFaceCount));
	profiling::PlotValue("Generated Transparent Faces", static_cast<int64_t>(render.sceneTransparentFaceCount));
	profiling::PlotValue("Meshing Dirty Chunks", static_cast<int64_t>(meshingDirtyChunkCount));
	profiling::PlotValue("Uploaded Chunk Descriptors", static_cast<int64_t>(uploadedChunkDescriptorCount));
	profiling::PlotValue("Uploaded Voxel Payload Chunks", static_cast<int64_t>(uploadedVoxelChunkCount));
	profiling::PlotValue("Chunk Voxel Words", static_cast<int64_t>(render.sceneChunkVoxelPayloadWordCount));
	profiling::PlotValue("Uploaded Chunk Voxel Words", static_cast<int64_t>(uploadedChunkVoxelWordCount));
	profiling::PlotValue(
		"Upload Descriptor Bytes",
		static_cast<int64_t>(uploadedChunkDescriptorCount * sizeof(PackedSceneChunkDescriptor)));
	profiling::PlotValue(
		"Upload Chunk Voxel Bytes",
		static_cast<int64_t>(uploadedChunkVoxelWordCount * sizeof(uint32_t)));
	return true;
}

bool RefreshChunkAabbBuffer(
	std::span<const VoxelChunk> chunks,
	std::span<const PackedSceneChunkDescriptor> descriptors,
	SceneFrameResources &frameResources)
{
	(void)descriptors;
	if (!frameResources.chunkAabbMappedData) {
		return false;
	}
	const size_t chunkCount = std::min(chunks.size(), descriptors.size());
	auto *packedAabbs = static_cast<PackedSceneChunkAabb *>(frameResources.chunkAabbMappedData);
	for (size_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex) {
		const VoxelChunk &chunk = chunks[chunkIndex];
		const float minX = static_cast<float>(chunk.min.x);
		const float minY = static_cast<float>(chunk.min.y);
		const float minZ = static_cast<float>(chunk.min.z);
		const float maxX = static_cast<float>(chunk.maxExclusive.x);
		const float maxY = static_cast<float>(chunk.maxExclusive.y);
		const float maxZ = static_cast<float>(chunk.maxExclusive.z);
		const float centerX = (minX + maxX) * 0.5f;
		const float centerY = (minY + maxY) * 0.5f;
		const float centerZ = (minZ + maxZ) * 0.5f;
		const float halfExtent = 0.5f;
		packedAabbs[chunkIndex].centerAndHalfExtent = {
			centerX,
			centerY,
			centerZ,
			halfExtent,
		};
		packedAabbs[chunkIndex].originAndPadding = {
			minX,
			minY,
			minZ,
			0.0f,
		};
	}
	for (size_t chunkIndex = chunkCount;
		chunkIndex < static_cast<size_t>(frameResources.chunkDescriptorCount);
		++chunkIndex) {
		packedAabbs[chunkIndex] = {};
	}
	if (frameResources.visibilityMaskMappedData) {
		const uint32_t wordCount = (static_cast<uint32_t>(chunkCount) + 31u) / 32u;
		std::memset(frameResources.visibilityMaskMappedData, 0, sizeof(uint32_t) * wordCount);
	}
	return true;
}

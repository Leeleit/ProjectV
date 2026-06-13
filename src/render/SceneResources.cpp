#include "render/SceneResources.hpp"

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
#include <cstdio>
#include <cstring>

namespace {
constexpr uint32_t kVoxelMaterialsPerWord = 4u;

std::array<VoxelMaterialVisual, kVoxelMaterialCount> BuildMaterialVisualTable()
{
	return {
		GetVoxelMaterialVisual(VoxelMaterial::Air),
		GetVoxelMaterialVisual(VoxelMaterial::Glass),
		GetVoxelMaterialVisual(VoxelMaterial::Fluid),
		GetVoxelMaterialVisual(VoxelMaterial::FloorWhite),
		GetVoxelMaterialVisual(VoxelMaterial::FloorGray),
	};
}

VoxelSceneLighting BuildSceneLighting(
	const VoxelWorld &world,
	const RenderState &render)
{
	return BuildVoxelSceneLighting(world.scenePreset, render.lightingDebugControls);
}

// **Tier 0.B (`2026-06-13`).** `Mat4` (16-byte aligned) replaces
// `std::array<float, 16>`. Same column-major field order, same
// 64 B byte size; the destination is a `std::array<float, 64>`
// (raw storage for the `sunShadowViewProjections` UBO field,
// std430 GLSL `mat4[4]`) which is memcpy'd from the source.
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
	// TAA fields: `taaParams` carries the runtime gate (`enabled` = 1 / 0), the
	// current-frame sub-pixel jitter in *pixel* units (jitterX/jitterY) and the
	// per-frame history blend factor. `prevViewProjectionMatrix` is the previous
	// frame's viewProjection, sampled from `render.taaPrevViewProjectionMatrix`
	// (FramePreparation stashes that *after* BuildGraphicsPushConstants so it
	// matches the matrix the voxel pass actually rendered with). `taaHistoryParams`
	// exposes texel size + a one-frame validity flag. The first frame is treated
	// as invalid because `taaPrevViewProjectionMatrix` is zero-initialised and
	// would reproject every fragment to a single pixel.
	// The taaHistoryParams texel-size cells default to 0 in this refresh path
	// because `RefreshSceneLightingBuffer` is only called from the CPU side
	// and has no direct view of the swapchain extent. The TAA resolve pass
	// (and the TAA contract documented in `VoxelSceneLighting`) treat 0-sized
	// texels as "skip the per-pixel reprojection step" and instead fall back
	// to a direct current-pixel sample, which is the correct behaviour on
	// frames where the swapchain is being recreated or the TAA path is
	// temporarily disabled. The actual texel size for the *next* frame is
	// patched in by `FramePreparation` via `UploadSceneFrameResources` once
	// the swapchain extent is known; until then this zeroed value is the
	// conservative correct choice.
	render.currentSceneLighting.taaParams = {
		render.taaJitterX,
		render.taaJitterY,
		render.taaEnabled ? render.taaBlend : 0.0f,
		render.taaEnabled ? 1.0f : 0.0f,
	};
	render.currentSceneLighting.prevViewProjectionMatrix = render.taaPrevViewProjectionMatrix;
	// 1.5 (and TAA fix in the same edit): the colour history
	// texel-size cells were previously left at 0 by this refresh
	// path; the comment claimed the size was "patched in by
	// FramePreparation via UploadSceneFrameResources" but no
	// such patch existed in the codebase, which meant the
	// `taa_resolve.frag` reprojection step ran with
	// `texelSize = (0, 0)` and silently fell back to the
	// "current-pixel-only" branch — the temporal blend was
	// effectively disabled even when the user said `TAA on`.
	// The fix is to populate the size here, since this function
	// already has the swapchain extent available in
	// `renderExtent`; the comment claiming a separate patch was
	// the symptom of the bug, not the bug itself. Same fix
	// applies to `taaLayerHistoryParams.xy` for the 1.5 layer
	// anti-flicker history (without the texel size, the layer
	// sampling would do `gl_FragCoord.xy * vec2(0)` and read
	// from a single texel, breaking the temporal blend).
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
	return static_cast<uint32_t>(world.voxels.size()) * 6u;
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
// **Two-level cache rebuild path (2026-06-12).** Per-chunk
// loop that also fills `cache.opaqueCommands` / `cache.shadowCommands` /
// `cache.transparentCommands` so the next call can short-circuit
// the loop entirely on a cache hit. The cache path is opt-in
// via `UpdateSceneFrameChunkVisibility`'s hash check.
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
	// **Tier 0.B (`2026-06-13`).** `Mat4` (16-byte aligned) per
	// element replaces `std::array<float, 16>`. Same column-major
	// field order, same 64 B byte size. The `sunShadowViewProjections`
	// UBO field stays `std::array<float, 64>` (raw storage for
	// the std430 GLSL `mat4[4]`) and is memcpy'd into the
	// per-cascade `Mat4`.
	std::array<projectv::math::Mat4, kSunShadowCascadeCount> shadowCascadeMatrices{};
	for (uint32_t cascadeIndex = 0; cascadeIndex < kSunShadowCascadeCount; ++cascadeIndex) {
		const size_t matrixOffset = static_cast<size_t>(cascadeIndex) * 16u;
		std::memcpy(
			shadowCascadeMatrices[cascadeIndex].data(),
			render.currentSceneLighting.sunShadowViewProjections.data() + matrixOffset,
			sizeof(projectv::math::Mat4));
	}

	const uint32_t chunkDescriptorCount = frameResources.chunkDescriptorCount;
	// **Tier 1.A (`2026-06-13`).** `ChunkVisibilityCache` now uses
	// `std::inplace_vector` (P0843, C++26) with a fixed cap of
	// `kChunkVisibilityCacheMaxChunks` (1024). The cap covers
	// VoxelLab + MeshingStress worst case; exceeding it is a logic
	// error and would have been an OOM on the old `std::vector`
	// path. `resize()` value-initialises new slots in-place
	// (no heap allocation, no realloc copy of existing data).
	assert(chunkDescriptorCount <= ChunkVisibilityCache::kChunkVisibilityCacheMaxChunks);
	assert(static_cast<size_t>(chunkDescriptorCount) * kSunShadowCascadeCount <=
		ChunkVisibilityCache::kChunkVisibilityCacheMaxChunks * kSunShadowCascadeCount);
	if (cache.opaqueCommands.size() != chunkDescriptorCount) {
		cache.opaqueCommands.resize(chunkDescriptorCount);
	}
	if (cache.shadowCommands.size() != static_cast<size_t>(chunkDescriptorCount) * kSunShadowCascadeCount) {
		cache.shadowCommands.resize(static_cast<size_t>(chunkDescriptorCount) * kSunShadowCascadeCount);
	}
	if (cache.transparentCommands.size() != chunkDescriptorCount) {
		cache.transparentCommands.resize(chunkDescriptorCount);
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

// **Two-level cache hit path (2026-06-12).** Copies the cached
// `VkDrawIndirectCommand` arrays into the per-frame mapped GPU
// indirect buffers. Three `memcpy` calls (opaque, shadow,
// transparent) replace the 1500+ dot products of the per-chunk
// loop. The shadow buffer is `chunkDescriptorCount *
// kSunShadowCascadeCount` `VkDrawIndirectCommand` entries — at
// 300 chunks and 4 cascades that's 300*4*16 = ~19 KB, well
// under any L1 cache.
void ApplyCachedChunkVisibilityCommands(
	const ChunkVisibilityCache &cache,
	SceneFrameResources &frameResources)
{
	if (frameResources.opaqueIndirectMappedData &&
		cache.opaqueCommands.size() == frameResources.chunkDescriptorCount) {
		std::memcpy(
			frameResources.opaqueIndirectMappedData,
			cache.opaqueCommands.data(),
			cache.opaqueCommands.size() * sizeof(VkDrawIndirectCommand));
	}
	if (frameResources.shadowIndirectMappedData &&
		cache.shadowCommands.size() ==
			static_cast<size_t>(frameResources.chunkDescriptorCount) * kSunShadowCascadeCount) {
		std::memcpy(
			frameResources.shadowIndirectMappedData,
			cache.shadowCommands.data(),
			cache.shadowCommands.size() * sizeof(VkDrawIndirectCommand));
	}
	if (frameResources.transparentIndirectMappedData &&
		cache.transparentCommands.size() == frameResources.chunkDescriptorCount) {
		std::memcpy(
			frameResources.transparentIndirectMappedData,
			cache.transparentCommands.data(),
			cache.transparentCommands.size() * sizeof(VkDrawIndirectCommand));
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

	// **Two-level cache check, 2026-06-12.** The hash folds
	// quantized camera position (0.25 voxel) + quantized
	// camera forward (0.005, ~0.3°) + scene voxel payload
	// version + chunk descriptor count. On a hit we skip the
	// per-chunk loop entirely and `memcpy` the cached
	// `VkDrawIndirectCommand` arrays straight into the
	// per-frame mapped GPU indirect buffers. See
	// `ChunkVisibilityCache` and
	// `projectv::visibility_cache::ComputeVisibilityCacheHash`
	// for the per-field contract and rationale.
	const uint64_t hash = projectv::visibility_cache::ComputeVisibilityCacheHash(
		parameters,
		render.sceneVoxelPayloadVersion,
		frameResources.chunkDescriptorCount);
	ChunkVisibilityCache &cache = render.chunkVisibilityCache;
	if (cache.valid &&
		cache.hash == hash &&
		cache.chunkDescriptorCount == frameResources.chunkDescriptorCount &&
		cache.sceneVoxelPayloadVersion == render.sceneVoxelPayloadVersion &&
		cache.opaqueCommands.size() == frameResources.chunkDescriptorCount &&
		cache.shadowCommands.size() ==
			static_cast<size_t>(frameResources.chunkDescriptorCount) * kSunShadowCascadeCount &&
		cache.transparentCommands.size() == frameResources.chunkDescriptorCount) {
		ApplyCachedChunkVisibilityCommands(cache, frameResources);
		frameResources.shadowCascadeVisibleChunkCounts = cache.shadowCascadeVisibleChunkCounts;
		++cache.consecutiveHitCount;
		profiling::PlotValue("Visible Chunks", static_cast<int64_t>(cache.visibleChunkCount));
		profiling::PlotValue("Culled Chunks", static_cast<int64_t>(cache.culledChunkCount));
		profiling::PlotValue("ChunkVisibilityCacheHits", static_cast<int64_t>(cache.consecutiveHitCount));
		return true;
	}

	// **Cache miss path (2026-06-12).** Run the canonical
	// per-chunk loop AND fill the cache in the same pass so the
	// next frame's hit check has data to `memcpy`. The two side
	// effects (frameResources mapped writes + cache fills) share
	// the same per-chunk math, so we don't pay an extra pass
	// here.
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
	// Stamp the cache with the just-rebuilt result, BUT only once
	// the CPU can actually see the dispatch's face counts. On the
	// first two frames after startup `render.sceneOpaqueFaceCount`
	// (read into `frameResources.opaqueFaceCount` in
	// `UploadSceneFrameResources` from `chunkDescriptor.drawRanges[1]`)
	// is 0 because the voxel meshing compute has not yet written
	// anything the CPU can observe. If we stamped the cache in that
	// state, the miss path would persist `instanceCount = 0`
	// commands; the very next frame the dispatch would skip (its
	// per-frame `meshedSceneVersion` already matches
	// `sceneVoxelPayloadVersion`), the cache would HIT, and the
	// zero-instance commands would be `memcpy`'d back over the
	// indirect buffer forever — the swapchain would render with no
	// voxels drawn (just the clear-color sky) until a camera move
	// invalidated the cache hash and forced another miss-path read,
	// by which point the CPU could finally observe the dispatch's
	// real output. Gating on `opaqueFaceCount`/`transparentFaceCount
	// > 0` keeps the cache invalid until the GPU has actually
	// produced faces this generation, so the first validated stamp
	// happens on the frame where the miss path can read correct
	// values. Verified end-to-end via Tracy `Generated Opaque
	// Faces` plot (steps from 0 → 908 on the validation frame) and
	// by visual smoke: VoxelLab reference shot now renders the
	// world from frame 0 without any camera input.
	const bool hasGeneratedFaces = frameResources.opaqueFaceCount > 0u ||
								   frameResources.transparentFaceCount > 0u;
	// `dirtyChunkCount > 0` means the voxel meshing compute will run
	// for this frame's per-frame resource. In that case the
	// `chunkDescriptor.drawRanges` we just read is the *pre-dispatch*
	// state — the GPU hasn't run yet, and on the very first frame
	// after a world edit (or a startup frame) the read is the source
	// descriptor's `0` rather than the dispatch's real output. If we
	// stamped the cache in that state, the miss path would persist
	// `instanceCount = 0` (or any other stale pre-dispatch value) and
	// the next frame's `ApplyCached…` `memcpy` would pin those stale
	// commands into the indirect buffer for as long as the camera
	// hash holds — the user sees a voxel's face vanish forever after
	// breaking a neighbor (the cache only refreshes once a camera
	// move changes the hash, and by then the GPU has the right
	// output sitting in `drawRanges` but the cache is locked to
	// yesterday's instance count). Gating on `dirtyChunkCount == 0`
	// forces the next frame to take the miss path one more time, by
	// which point the dispatch's write is visible to the CPU (via
	// the per-frame resource rotation's 1-frame staleness window)
	// and the cache can validate with the real face count.
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
		// CPU cannot yet observe any GPU-written face counts. If we
		// validated the cache here, every future frame would apply
		// the `instanceCount = 0` commands via `ApplyCached…` and
		// the swapchain would render empty until something else
		// (camera move, world edit) tripped the cache hash. Force
		// the next frame to take the miss path so it re-reads
		// `chunkDescriptor.drawRanges[1]` after the dispatch has had
		// a chance to populate it.
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

	// **Pre-existing race fix (`2026-06-13`).** Wait for
	// the GPU to finish before destroying the
	// per-frame buffers. The validation layer
	// reports: "the storage buffer descriptor
	// sceneLighting is using buffer ... that is
	// invalid or has been destroyed" on
	// `vkCmdDraw` calls issued after a scene preset
	// switch (`F5` or the startup auto-cycle) — the
	// previous frame's draw is still in flight when
	// we tear down its descriptor-backed buffers.
	// The fix is the brute-force `vkDeviceWaitIdle`:
	// a per-resource timeline-semaphore lifetime
	// tracker would be cleaner, but `DestroySceneResources`
	// is only called on scene switches / allocation
	// error rollbacks (not per-frame), so the
	// pipeline stall is acceptable. The pre-existing
	// call sites that follow this preamble
	// (`CreateSceneResources` and the per-buffer
	// `vmaCreateBuffer` failure-rollback paths) all
	// destroy a known set of buffers that may be
	// in-flight in any of the
	// `render->sceneFrameResources` slots, so a
	// single device-wide idle is the simplest
	// correct fix.
	if (context->device != VK_NULL_HANDLE) {
		const VkResult idleResult = vkDeviceWaitIdle(context->device);
		if (idleResult != VK_SUCCESS) {
			runtime::LogVkFailure(
				"DestroySceneResources.vkDeviceWaitIdle",
				idleResult);
		}
	}

	for (auto &[packedFaceMappedData, packedFaceBuffer, packedFaceAllocation, debugHudVertexMappedData, debugHudVertexBuffer, debugHudVertexAllocation, chunkDescriptorMappedData, chunkDescriptorBuffer, chunkDescriptorAllocation, chunkVoxelPayloadMappedData, chunkVoxelPayloadBuffer, chunkVoxelPayloadAllocation, opaqueIndirectMappedData, opaqueIndirectBuffer, opaqueIndirectAllocation, shadowIndirectMappedData, shadowIndirectBuffer, shadowIndirectAllocation, transparentIndirectMappedData, transparentIndirectBuffer, transparentIndirectAllocation, dirtyChunkIndexMappedData, dirtyChunkIndexBuffer, dirtyChunkIndexAllocation, chunkCullingMappedData, chunkCullingBuffer, chunkCullingAllocation, sceneLightingMappedData, sceneLightingBuffer, sceneLightingAllocation, graphicsDescriptorSet, shadowDescriptorSet, voxelMeshingDescriptorSet, uploadedSceneVersion, uploadedVoxelPayloadVersion, meshedSceneVersion, chunkDescriptorCount, shadowIndirectCommandCount, shadowCascadeVisibleChunkCounts, dirtyChunkCount, opaqueFaceCount, transparentFaceCount, debugHudVertexCount] : render->sceneFrameResources) {
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
		sceneLightingAllocation = VK_NULL_HANDLE;
		graphicsDescriptorSet = VK_NULL_HANDLE;
		shadowDescriptorSet = VK_NULL_HANDLE;
		voxelMeshingDescriptorSet = VK_NULL_HANDLE;
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
		const auto materialVisuals = BuildMaterialVisualTable();
		std::memcpy(
			render->materialVisualMappedData,
			materialVisuals.data(),
			sizeof(materialVisuals));
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
	// Force the first frame on each swapchain image to upload descriptor layout/state once.
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
		// Per-voxel edits only change payload data plus the dirty chunks' non-air counts.
		// Full descriptor reuploads would wipe GPU-generated drawRanges for every chunk.
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

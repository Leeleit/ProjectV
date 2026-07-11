import projectv.math; // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "render/SceneResources.hpp"
#include "render/SceneResourcesInternal.hpp"

#include "render/LodDownsampleGpuConsume.hpp"

#include "debug/Profiling.hpp"
#include "render/vulkan/VulkanDebug.hpp"
#include "render/vulkan/VulkanGraphicsPipeline.hpp"
#include "render/vulkan/VulkanVoxelMeshingPipeline.hpp"
#include "voxel/NanoVdb.hpp"
#include "voxel/VoxelMaterials.hpp"
#include "voxel/VoxelWorld.hpp"

#include <algorithm>
#include <array>
#include <span>

namespace {
std::span<const VoxelMaterialVisual> BuildMaterialVisualTable()
{
	static const std::array visuals{
		GetVoxelMaterialVisual(VoxelMaterial::Air),
		GetVoxelMaterialVisual(VoxelMaterial::Glass),
		GetVoxelMaterialVisual(VoxelMaterial::Fluid),
		GetVoxelMaterialVisual(VoxelMaterial::FloorWhite),
		GetVoxelMaterialVisual(VoxelMaterial::FloorGray),
	};
	return visuals;
}
} // namespace

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

	for (SceneFrameResources &frameResources : render->sceneFrameResources) {
		(void)frameResources;
		const size_t frameResourceIndex = static_cast<size_t>(&frameResources - render->sceneFrameResources.data());
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
			world->voxelWorld->chunks.size();
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

		VmaAllocationInfo hzbVisibleCountAllocationInfo{};
		if (!CreateBuffer(
				context,
				sizeof(uint32_t),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
				allocationInfo,
				&frameResources.hzbVisibleCountBuffer,
				&frameResources.hzbVisibleCountAllocation,
				&hzbVisibleCountAllocationInfo)) {
			DestroySceneResources(context, render);
			return false;
		}
		frameResources.hzbVisibleCountMappedData = hzbVisibleCountAllocationInfo.pMappedData;
		profiling::RecordAllocation(
			frameResources.hzbVisibleCountAllocation,
			hzbVisibleCountAllocationInfo.size,
			"SceneHzbVisibleCountBufferAllocation");
		render->sceneMemoryBytes += hzbVisibleCountAllocationInfo.size;
		const uint32_t initialHzbVisibleCount =
			static_cast<uint32_t>(world->voxelWorld->chunks.size());
		std::memcpy(
			frameResources.hzbVisibleCountMappedData,
			&initialHzbVisibleCount,
			sizeof(uint32_t));

		const uint32_t hzbPerChunkMipCount =
			std::max(static_cast<uint32_t>(world->voxelWorld->chunks.size()), 1u);
		const VkDeviceSize hzbPerChunkMipBytes = sizeof(uint32_t) * 2u * hzbPerChunkMipCount;
		VmaAllocationInfo hzbPerChunkMipAllocationInfo{};
		if (!CreateBuffer(
				context,
				hzbPerChunkMipBytes,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.hzbPerChunkMipBuffer,
				&frameResources.hzbPerChunkMipAllocation,
				&hzbPerChunkMipAllocationInfo)) {
			DestroySceneResources(context, render);
			return false;
		}
		frameResources.hzbPerChunkMipMappedData = hzbPerChunkMipAllocationInfo.pMappedData;
		frameResources.hzbPerChunkMipCapacityBytes = hzbPerChunkMipAllocationInfo.size;
		profiling::RecordAllocation(
			frameResources.hzbPerChunkMipAllocation,
			hzbPerChunkMipAllocationInfo.size,
			"SceneHzbPerChunkMipBufferAllocation");
		render->sceneMemoryBytes += hzbPerChunkMipAllocationInfo.size;

		const uint32_t chunkLodLevelsCount =
			std::max(static_cast<uint32_t>(world->voxelWorld->chunks.size()), 1u);
		const VkDeviceSize chunkLodLevelsBytes = sizeof(uint32_t) * chunkLodLevelsCount;
		VmaAllocationInfo chunkLodLevelsAllocationInfo{};
		if (!CreateBuffer(
				context,
				chunkLodLevelsBytes,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.chunkLodLevelsBuffer,
				&frameResources.chunkLodLevelsAllocation,
				&chunkLodLevelsAllocationInfo)) {
			DestroySceneResources(context, render);
			return false;
		}
		frameResources.chunkLodLevelsMappedData = chunkLodLevelsAllocationInfo.pMappedData;
		frameResources.chunkLodLevelsCapacity = static_cast<uint32_t>(chunkLodLevelsCount);
		profiling::RecordAllocation(
			frameResources.chunkLodLevelsAllocation,
			chunkLodLevelsAllocationInfo.size,
			"SceneChunkLodLevelsBufferAllocation");
		render->sceneMemoryBytes += chunkLodLevelsAllocationInfo.size;
		std::memset(frameResources.chunkLodLevelsMappedData, 0, chunkLodLevelsBytes);

		const uint32_t chunkSizeForLodPayload = static_cast<uint32_t>(world->voxelWorld->chunkSize);
		const uint32_t lodPayloadBytesNeeded =
			projectv::render::ComputeLodDownsampledVoxelPayloadBytes(
				static_cast<uint32_t>(world->voxelWorld->chunks.size()),
				chunkSizeForLodPayload);
		const VkDeviceSize lodPayloadBytes = std::max<VkDeviceSize>(lodPayloadBytesNeeded, 1u);
		VmaAllocationInfo lodPayloadAllocationInfo{};
		if (!CreateBuffer(
				context,
				lodPayloadBytes,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.lodDownsampledVoxelPayloadBuffer,
				&frameResources.lodDownsampledVoxelPayloadAllocation,
				&lodPayloadAllocationInfo)) {
			DestroySceneResources(context, render);
			return false;
		}
		frameResources.lodDownsampledVoxelPayloadMappedData = lodPayloadAllocationInfo.pMappedData;
		frameResources.lodDownsampledVoxelPayloadCapacityBytes = lodPayloadAllocationInfo.size;
		profiling::RecordAllocation(
			frameResources.lodDownsampledVoxelPayloadAllocation,
			lodPayloadAllocationInfo.size,
			"SceneLodDownsampledVoxelPayloadBufferAllocation");
		render->sceneMemoryBytes += lodPayloadAllocationInfo.size;
		std::memset(frameResources.lodDownsampledVoxelPayloadMappedData, 0, lodPayloadBytes);

		const uint32_t fluidCaMaxActiveChunks = static_cast<uint32_t>(world->voxelWorld->chunks.size());
		const VkDeviceSize fluidCaActiveChunkIdBytes = sizeof(uint32_t) * std::max(fluidCaMaxActiveChunks, 1u);
		VmaAllocationInfo fluidCaActiveChunkIdAllocationInfo{};
		if (!CreateBuffer(
				context,
				fluidCaActiveChunkIdBytes,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.fluidCaActiveChunkIdBuffer,
				&frameResources.fluidCaActiveChunkIdAllocation,
				&fluidCaActiveChunkIdAllocationInfo)) {
			DestroySceneResources(context, render);
			return false;
		}
		frameResources.fluidCaActiveChunkIdMappedData = fluidCaActiveChunkIdAllocationInfo.pMappedData;
		profiling::RecordAllocation(
			frameResources.fluidCaActiveChunkIdAllocation,
			fluidCaActiveChunkIdAllocationInfo.size,
			"SceneFluidCaActiveChunkIdBufferAllocation");
		render->sceneMemoryBytes += fluidCaActiveChunkIdAllocationInfo.size;
		std::memset(frameResources.fluidCaActiveChunkIdMappedData, 0, fluidCaActiveChunkIdBytes);

		VmaAllocationInfo fluidCaStatsAllocationInfo{};
		if (!CreateBuffer(
				context,
				sizeof(uint32_t) * 4u,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.fluidCaStatsBuffer,
				&frameResources.fluidCaStatsAllocation,
				&fluidCaStatsAllocationInfo)) {
			DestroySceneResources(context, render);
			return false;
		}
		frameResources.fluidCaStatsMappedData = fluidCaStatsAllocationInfo.pMappedData;
		profiling::RecordAllocation(
			frameResources.fluidCaStatsAllocation,
			fluidCaStatsAllocationInfo.size,
			"SceneFluidCaStatsBufferAllocation");
		render->sceneMemoryBytes += fluidCaStatsAllocationInfo.size;
		std::memset(frameResources.fluidCaStatsMappedData, 0, sizeof(uint32_t) * 4u);

		if (frameResourceIndex == 0) {
			render->fluidCaMaxActiveChunks = fluidCaMaxActiveChunks;
		}

		const uint32_t chunkVoxelCount = static_cast<uint32_t>(world->voxelWorld->chunkSize) *
			static_cast<uint32_t>(world->voxelWorld->chunkSize) *
			static_cast<uint32_t>(world->voxelWorld->chunkSize);
		const VkDeviceSize fluidCaPingPongBytes = std::max<VkDeviceSize>(
			sizeof(uint32_t) * 4u * std::max(chunkVoxelCount, 1u) * std::max(fluidCaMaxActiveChunks, 1u),
			sizeof(uint32_t) * 4u);
		if (frameResourceIndex == 0) {
			render->fluidCaPingPongBufferBytes = static_cast<uint32_t>(fluidCaPingPongBytes);
		}

		for (uint32_t pingPong = 0u; pingPong < 2u; ++pingPong) {
			VkBuffer &targetBuffer = pingPong == 0u ? frameResources.fluidCaSourceBuffer
													   : frameResources.fluidCaDestinationBuffer;
			VmaAllocation &targetAllocation = pingPong == 0u ? frameResources.fluidCaSourceAllocation
																 : frameResources.fluidCaDestinationAllocation;
			void *&targetMappedData = pingPong == 0u ? frameResources.fluidCaSourceMappedData
														: frameResources.fluidCaDestinationMappedData;
			VmaAllocationInfo pingPongAllocationInfo{};
			if (!CreateBuffer(
					context,
					fluidCaPingPongBytes,
					VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					allocationInfo,
					&targetBuffer,
					&targetAllocation,
					&pingPongAllocationInfo)) {
				DestroySceneResources(context, render);
				return false;
			}
			targetMappedData = pingPongAllocationInfo.pMappedData;
			profiling::RecordAllocation(
				targetAllocation,
				pingPongAllocationInfo.size,
				pingPong == 0u ? "SceneFluidCaSourceBufferAllocation" : "SceneFluidCaDestinationBufferAllocation");
			render->sceneMemoryBytes += pingPongAllocationInfo.size;
			std::memset(targetMappedData, 0, fluidCaPingPongBytes);
		}

		constexpr VkDeviceSize kNanoVdbInitialUpperCapacityBytes = sizeof(projectv::voxel::nanovdb::NanoVdbUpper) * 1u;
		constexpr VkDeviceSize kNanoVdbInitialLowerCapacityBytes = sizeof(projectv::voxel::nanovdb::NanoVdbLower) * 64u;
		constexpr VkDeviceSize kNanoVdbInitialLeafCapacityBytes = sizeof(projectv::voxel::nanovdb::NanoVdbLeaf) * 64u;
		constexpr VkDeviceSize kNanoVdbInitialMaterialCapacityBytes = sizeof(uint8_t) * 64u;

		{
			VmaAllocationInfo nanovdbUpperAllocationInfo{};
			if (!CreateBuffer(
					context,
					kNanoVdbInitialUpperCapacityBytes,
					VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					allocationInfo,
					&frameResources.nanovdbUpperBuffer,
					&frameResources.nanovdbUpperAllocation,
					&nanovdbUpperAllocationInfo)) {
				DestroySceneResources(context, render);
				return false;
			}
			frameResources.nanovdbUpperMappedData = nanovdbUpperAllocationInfo.pMappedData;
			frameResources.nanovdbUpperCapacityBytes = nanovdbUpperAllocationInfo.size;
			profiling::RecordAllocation(
				frameResources.nanovdbUpperAllocation,
				nanovdbUpperAllocationInfo.size,
				"SceneNanoVdbUpperBufferAllocation");
			render->sceneMemoryBytes += nanovdbUpperAllocationInfo.size;
		}

		{
			VmaAllocationInfo nanovdbLowerAllocationInfo{};
			if (!CreateBuffer(
					context,
					kNanoVdbInitialLowerCapacityBytes,
					VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					allocationInfo,
					&frameResources.nanovdbLowerBuffer,
					&frameResources.nanovdbLowerAllocation,
					&nanovdbLowerAllocationInfo)) {
				DestroySceneResources(context, render);
				return false;
			}
			frameResources.nanovdbLowerMappedData = nanovdbLowerAllocationInfo.pMappedData;
			frameResources.nanovdbLowerCapacityBytes = nanovdbLowerAllocationInfo.size;
			profiling::RecordAllocation(
				frameResources.nanovdbLowerAllocation,
				nanovdbLowerAllocationInfo.size,
				"SceneNanoVdbLowerBufferAllocation");
			render->sceneMemoryBytes += nanovdbLowerAllocationInfo.size;
		}

		{
			VmaAllocationInfo nanovdbLeafAllocationInfo{};
			if (!CreateBuffer(
					context,
					kNanoVdbInitialLeafCapacityBytes,
					VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					allocationInfo,
					&frameResources.nanovdbLeafBuffer,
					&frameResources.nanovdbLeafAllocation,
					&nanovdbLeafAllocationInfo)) {
				DestroySceneResources(context, render);
				return false;
			}
			frameResources.nanovdbLeafMappedData = nanovdbLeafAllocationInfo.pMappedData;
			frameResources.nanovdbLeafCapacityBytes = nanovdbLeafAllocationInfo.size;
			profiling::RecordAllocation(
				frameResources.nanovdbLeafAllocation,
				nanovdbLeafAllocationInfo.size,
				"SceneNanoVdbLeafBufferAllocation");
			render->sceneMemoryBytes += nanovdbLeafAllocationInfo.size;
		}

		{
			VmaAllocationInfo nanovdbMaterialAllocationInfo{};
			if (!CreateBuffer(
					context,
					kNanoVdbInitialMaterialCapacityBytes,
					VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					allocationInfo,
					&frameResources.nanovdbMaterialBuffer,
					&frameResources.nanovdbMaterialAllocation,
					&nanovdbMaterialAllocationInfo)) {
				DestroySceneResources(context, render);
				return false;
			}
			frameResources.nanovdbMaterialMappedData = nanovdbMaterialAllocationInfo.pMappedData;
			frameResources.nanovdbMaterialCapacityBytes = nanovdbMaterialAllocationInfo.size;
			profiling::RecordAllocation(
				frameResources.nanovdbMaterialAllocation,
				nanovdbMaterialAllocationInfo.size,
				"SceneNanoVdbMaterialBufferAllocation");
			render->sceneMemoryBytes += nanovdbMaterialAllocationInfo.size;
		}

		const VkDeviceSize kWorldGenVoxelBufferBytes = sizeof(uint32_t) * 8u * 8u * 8u *
			std::max(world->voxelWorld->chunks.size(), static_cast<size_t>(1u));
		{
			VmaAllocationInfo worldGenVoxelAllocationInfo{};
			if (!CreateBuffer(
					context,
					kWorldGenVoxelBufferBytes,
					VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					allocationInfo,
					&frameResources.worldGenVoxelBuffer,
					&frameResources.worldGenVoxelAllocation,
					&worldGenVoxelAllocationInfo)) {
				DestroySceneResources(context, render);
				return false;
			}
			frameResources.worldGenVoxelMappedData = worldGenVoxelAllocationInfo.pMappedData;
			frameResources.worldGenVoxelCapacityBytes = worldGenVoxelAllocationInfo.size;
			profiling::RecordAllocation(
				frameResources.worldGenVoxelAllocation,
				worldGenVoxelAllocationInfo.size,
				"SceneWorldGenVoxelBufferAllocation");
			render->sceneMemoryBytes += worldGenVoxelAllocationInfo.size;
		}

		char bufferName[64]{};

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
		std::snprintf(bufferName, sizeof(bufferName), "SceneFluidCaActiveChunkIdBuffer[%zu]", frameResourceIndex);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frameResources.fluidCaActiveChunkIdBuffer),
			VK_OBJECT_TYPE_BUFFER,
			bufferName);
		std::snprintf(bufferName, sizeof(bufferName), "SceneFluidCaStatsBuffer[%zu]", frameResourceIndex);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frameResources.fluidCaStatsBuffer),
			VK_OBJECT_TYPE_BUFFER,
			bufferName);
		std::snprintf(bufferName, sizeof(bufferName), "SceneFluidCaSourceBuffer[%zu]", frameResourceIndex);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frameResources.fluidCaSourceBuffer),
			VK_OBJECT_TYPE_BUFFER,
			bufferName);
		std::snprintf(bufferName, sizeof(bufferName), "SceneFluidCaDestinationBuffer[%zu]", frameResourceIndex);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frameResources.fluidCaDestinationBuffer),
			VK_OBJECT_TYPE_BUFFER,
			bufferName);
		std::snprintf(bufferName, sizeof(bufferName), "SceneNanoVdbUpperBuffer[%zu]", frameResourceIndex);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frameResources.nanovdbUpperBuffer),
			VK_OBJECT_TYPE_BUFFER,
			bufferName);
		std::snprintf(bufferName, sizeof(bufferName), "SceneNanoVdbLowerBuffer[%zu]", frameResourceIndex);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frameResources.nanovdbLowerBuffer),
			VK_OBJECT_TYPE_BUFFER,
			bufferName);
		std::snprintf(bufferName, sizeof(bufferName), "SceneNanoVdbLeafBuffer[%zu]", frameResourceIndex);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frameResources.nanovdbLeafBuffer),
			VK_OBJECT_TYPE_BUFFER,
			bufferName);
		std::snprintf(bufferName, sizeof(bufferName), "SceneNanoVdbMaterialBuffer[%zu]", frameResourceIndex);
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frameResources.nanovdbMaterialBuffer),
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

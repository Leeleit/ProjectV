#include "SceneResources.hpp"

#include "Profiling.hpp"
#include "VoxelWorld.hpp"
#include "VulkanDebug.hpp"
#include "VulkanGraphicsPipeline.hpp"

#include <array>
#include <cstdio>
// ReSharper disable once CppUnusedIncludeDirective
#include <cstring>
#include <vector>

namespace {
constexpr uint32_t kMaxSceneVertices = 262144;

uint32_t PackLocalPositionMaterial(
	const uint32_t localX,
	const uint32_t localY,
	const uint32_t localZ,
	const VoxelMaterial material)
{
	const uint32_t packedLocalX = localX & 0xFFu;
	const uint32_t packedLocalY = (localY & 0xFFu) << 8u;
	const uint32_t packedLocalZ = (localZ & 0xFFu) << 16u;
	const uint32_t packedMaterial = static_cast<uint32_t>(material) << 24u;
	return packedLocalX | packedLocalY | packedLocalZ | packedMaterial;
}

VoxelMaterial UnpackMaterial(const SceneChunkMeshVertex packedVertex)
{
	return static_cast<VoxelMaterial>(packedVertex.localPositionMaterial >> 24u & 0xFFu);
}

bool CreateBuffer(
	VulkanContextState *context,
	// ReSharper disable once CppDFAConstantParameter
	const VkDeviceSize size,
	// ReSharper disable once CppDFAConstantParameter
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

bool ShouldEmitVoxelFace(const VoxelMaterial material, const VoxelMaterial neighborMaterial)
{
	if (material == VoxelMaterial::Air) {
		return false;
	}

	if (material == VoxelMaterial::Fluid) {
		return neighborMaterial == VoxelMaterial::Air || neighborMaterial == VoxelMaterial::Glass;
	}

	return neighborMaterial == VoxelMaterial::Air;
}

void EmitFaceToChunkMesh(
	SceneChunkRenderCache &chunkRenderCache,
	const VoxelMaterial material,
	const uint32_t faceIndex,
	const std::array<std::array<uint32_t, 3>, 4> &corners)
{
	const SceneChunkMeshVertex v0{
		.localPositionMaterial = PackLocalPositionMaterial(corners[0][0], corners[0][1], corners[0][2], material),
		.faceIndex = faceIndex,
	};
	const SceneChunkMeshVertex v1{
		.localPositionMaterial = PackLocalPositionMaterial(corners[1][0], corners[1][1], corners[1][2], material),
		.faceIndex = faceIndex,
	};
	const SceneChunkMeshVertex v2{
		.localPositionMaterial = PackLocalPositionMaterial(corners[2][0], corners[2][1], corners[2][2], material),
		.faceIndex = faceIndex,
	};
	const SceneChunkMeshVertex v3{
		.localPositionMaterial = PackLocalPositionMaterial(corners[3][0], corners[3][1], corners[3][2], material),
		.faceIndex = faceIndex,
	};

	chunkRenderCache.meshVertices.push_back(v0);
	chunkRenderCache.meshVertices.push_back(v1);
	chunkRenderCache.meshVertices.push_back(v2);
	chunkRenderCache.meshVertices.push_back(v0);
	chunkRenderCache.meshVertices.push_back(v2);
	chunkRenderCache.meshVertices.push_back(v3);
}

void RebuildChunkMesh(const VoxelWorld &world, const VoxelChunk &chunk, SceneChunkRenderCache &chunkRenderCache)
{
	PV_PROFILE_ZONE_N("RebuildChunkMesh");
	chunkRenderCache.meshVertices.clear();

	constexpr std::array<Int3, 6> neighborOffsets{{
		{1, 0, 0},
		{-1, 0, 0},
		{0, 1, 0},
		{0, -1, 0},
		{0, 0, 1},
		{0, 0, -1},
	}};

	for (int z = chunk.min.z; z < chunk.maxExclusive.z; ++z) {
		for (int y = chunk.min.y; y < chunk.maxExclusive.y; ++y) {
			for (int x = chunk.min.x; x < chunk.maxExclusive.x; ++x) {
				const Int3 position{x, y, z};
				const VoxelMaterial material = GetVoxelMaterial(world, position);
				if (material == VoxelMaterial::Air) {
					continue;
				}

				const uint32_t localX = static_cast<uint32_t>(x - chunk.min.x);
				const uint32_t localX1 = localX + 1u;
				const uint32_t localY = static_cast<uint32_t>(y - chunk.min.y);
				const uint32_t localY1 = localY + 1u;
				const uint32_t localZ = static_cast<uint32_t>(z - chunk.min.z);
				const uint32_t localZ1 = localZ + 1u;

				const std::array<std::array<std::array<uint32_t, 3>, 4>, 6> faceCorners{{
					{{{localX1, localY, localZ}, {localX1, localY1, localZ}, {localX1, localY1, localZ1}, {localX1, localY, localZ1}}},
					{{{localX, localY, localZ1}, {localX, localY1, localZ1}, {localX, localY1, localZ}, {localX, localY, localZ}}},
					{{{localX, localY1, localZ}, {localX, localY1, localZ1}, {localX1, localY1, localZ1}, {localX1, localY1, localZ}}},
					{{{localX, localY, localZ1}, {localX, localY, localZ}, {localX1, localY, localZ}, {localX1, localY, localZ1}}},
					{{{localX1, localY, localZ1}, {localX1, localY1, localZ1}, {localX, localY1, localZ1}, {localX, localY, localZ1}}},
					{{{localX, localY, localZ}, {localX, localY1, localZ}, {localX1, localY1, localZ}, {localX1, localY, localZ}}},
				}};

				for (size_t faceIndex = 0; faceIndex < neighborOffsets.size(); ++faceIndex) {
					const Int3 neighbor{
						x + neighborOffsets[faceIndex].x,
						y + neighborOffsets[faceIndex].y,
						z + neighborOffsets[faceIndex].z,
					};
					if (!ShouldEmitVoxelFace(material, GetVoxelMaterial(world, neighbor))) {
						continue;
					}

					EmitFaceToChunkMesh(
						chunkRenderCache,
						material,
						static_cast<uint32_t>(faceIndex),
						faceCorners[faceIndex]);
				}
			}
		}
	}
}

void InitializeSceneChunkDescriptors(
	const VoxelWorld &world,
	RenderState &render)
{
	render.sceneChunkDescriptors.clear();
	render.sceneChunkDescriptors.resize(world.chunks.size());
	for (size_t chunkIndex = 0; chunkIndex < world.chunks.size(); ++chunkIndex) {
		const VoxelChunk &chunk = world.chunks[chunkIndex];
		render.sceneChunkDescriptors[chunkIndex].chunkOrigin = {
			chunk.min.x,
			chunk.min.y,
			chunk.min.z,
			0,
		};
		render.sceneChunkDescriptors[chunkIndex].drawRanges = {0u, 0u, 0u, 0u};
	}
}

struct SceneUploadCounts {
	uint32_t vertexCount = 0;
	uint32_t opaqueVertexCount = 0;
	uint32_t transparentVertexCount = 0;
};

SceneUploadCounts BuildSceneUploadCache(RenderState &render)
{
	PV_PROFILE_ZONE_N("BuildSceneUploadCache");
	SceneUploadCounts outCounts{};

	uint32_t opaqueVertexCount = 0;
	std::vector<SceneChunkMeshVertex> &packedVertexPayload = render.scenePackedVertexPayload;
	packedVertexPayload.clear();
	if (packedVertexPayload.capacity() < render.sceneVertexCapacity) {
		packedVertexPayload.reserve(render.sceneVertexCapacity);
	}

	std::vector<SceneChunkMeshVertex> &transparentPackedVertices = render.transparentPackedVertexScratch;
	transparentPackedVertices.clear();
	if (transparentPackedVertices.capacity() < render.sceneVertexCapacity / 4) {
		transparentPackedVertices.reserve(render.sceneVertexCapacity / 4);
	}

	std::vector<SceneChunkDrawRange> &opaqueChunkDrawRanges = render.opaqueChunkDrawRanges;
	opaqueChunkDrawRanges.clear();
	if (opaqueChunkDrawRanges.capacity() < render.sceneChunkRenderCaches.size()) {
		opaqueChunkDrawRanges.reserve(render.sceneChunkRenderCaches.size());
	}

	std::vector<SceneChunkDrawRange> &transparentChunkDrawRanges = render.transparentChunkDrawRanges;
	transparentChunkDrawRanges.clear();
	if (transparentChunkDrawRanges.capacity() < render.sceneChunkRenderCaches.size()) {
		transparentChunkDrawRanges.reserve(render.sceneChunkRenderCaches.size());
	}

	for (size_t chunkDescriptorIndex = 0; chunkDescriptorIndex < render.sceneChunkDescriptors.size();
		 ++chunkDescriptorIndex) {
		render.sceneChunkDescriptors[chunkDescriptorIndex].drawRanges = {0u, 0u, 0u, 0u};
	}

	bool reachedVertexCapacity = false;
	for (size_t chunkIndex = 0; chunkIndex < render.sceneChunkRenderCaches.size(); ++chunkIndex) {
		const std::vector<SceneChunkMeshVertex> &meshVertices = render.sceneChunkRenderCaches[chunkIndex].meshVertices;
		const uint32_t chunkOpaqueFirstVertex = opaqueVertexCount;
		const uint32_t chunkTransparentFirstVertex = static_cast<uint32_t>(transparentPackedVertices.size());
		uint32_t chunkOpaqueVertexCount = 0;
		uint32_t chunkTransparentVertexCount = 0;

		for (const SceneChunkMeshVertex &packedVertex : meshVertices) {
			if (opaqueVertexCount + transparentPackedVertices.size() >= render.sceneVertexCapacity) {
				reachedVertexCapacity = true;
				break;
			}

			const VoxelMaterial material = UnpackMaterial(packedVertex);
			if (material == VoxelMaterial::Glass) {
				transparentPackedVertices.push_back(packedVertex);
				++chunkTransparentVertexCount;
			} else {
				packedVertexPayload.push_back(packedVertex);
				++opaqueVertexCount;
				++chunkOpaqueVertexCount;
			}
		}

		if (chunkIndex < render.sceneChunkDescriptors.size()) {
			std::array<uint32_t, 4> &drawRanges = render.sceneChunkDescriptors[chunkIndex].drawRanges;
			drawRanges[0] = chunkOpaqueFirstVertex;
			drawRanges[1] = chunkOpaqueVertexCount;
			drawRanges[2] = chunkTransparentFirstVertex;
			drawRanges[3] = chunkTransparentVertexCount;
		}

		if (chunkOpaqueVertexCount > 0) {
			opaqueChunkDrawRanges.push_back({
				.firstVertex = chunkOpaqueFirstVertex,
				.vertexCount = chunkOpaqueVertexCount,
				.chunkIndex = static_cast<uint32_t>(chunkIndex),
			});
		}
		if (chunkTransparentVertexCount > 0) {
			transparentChunkDrawRanges.push_back({
				.firstVertex = chunkTransparentFirstVertex,
				.vertexCount = chunkTransparentVertexCount,
				.chunkIndex = static_cast<uint32_t>(chunkIndex),
			});
		}

		if (reachedVertexCapacity) {
			break;
		}
	}

	const uint32_t transparentBaseVertex = opaqueVertexCount;
	if (!transparentPackedVertices.empty()) {
		packedVertexPayload.insert(
			packedVertexPayload.end(),
			transparentPackedVertices.begin(),
			transparentPackedVertices.end());
	}
	for (SceneChunkDrawRange &transparentChunkDrawRange : transparentChunkDrawRanges) {
		transparentChunkDrawRange.firstVertex += transparentBaseVertex;
	}
	for (size_t chunkDescriptorIndex = 0; chunkDescriptorIndex < render.sceneChunkDescriptors.size();
		 ++chunkDescriptorIndex) {
		std::array<uint32_t, 4> &drawRanges = render.sceneChunkDescriptors[chunkDescriptorIndex].drawRanges;
		if (drawRanges[3] > 0) {
			drawRanges[2] += transparentBaseVertex;
		}
	}

	outCounts.opaqueVertexCount = opaqueVertexCount;
	outCounts.transparentVertexCount = static_cast<uint32_t>(transparentPackedVertices.size());
	outCounts.vertexCount = opaqueVertexCount + outCounts.transparentVertexCount;
	return outCounts;
}

void EnsureSceneUploadCache(RenderState &render)
{
	if (!render.sceneUploadCacheDirty) {
		return;
	}

	const auto [vertexCount, opaqueVertexCount, transparentVertexCount] = BuildSceneUploadCache(render);
	render.sceneUploadVertexCount = vertexCount;
	render.sceneUploadOpaqueVertexCount = opaqueVertexCount;
	render.sceneUploadTransparentVertexCount = transparentVertexCount;
	render.sceneTriangleCount = vertexCount / 3;
	++render.sceneUploadVersion;
	render.sceneUploadCacheDirty = false;
}
} // namespace

void DestroySceneResources(
	VulkanContextState *context,
	RenderState *render)
{
	if (!context || !render || !context->allocator) {
		return;
	}

	for (auto &[packedVertexMappedData, packedVertexBuffer, packedVertexAllocation, chunkDescriptorMappedData, chunkDescriptorBuffer, chunkDescriptorAllocation, graphicsDescriptorSet, uploadedSceneVersion, chunkDescriptorCount, vertexCount, opaqueVertexCount, transparentVertexCount] : render->sceneFrameResources) {
		if (packedVertexBuffer && packedVertexAllocation) {
			profiling::RecordFree(packedVertexAllocation, "ScenePackedVertexBufferAllocation");
			vmaDestroyBuffer(context->allocator, packedVertexBuffer, packedVertexAllocation);
			packedVertexBuffer = VK_NULL_HANDLE;
			packedVertexAllocation = VK_NULL_HANDLE;
		}
		if (chunkDescriptorBuffer && chunkDescriptorAllocation) {
			profiling::RecordFree(chunkDescriptorAllocation, "SceneChunkDescriptorBufferAllocation");
			vmaDestroyBuffer(context->allocator, chunkDescriptorBuffer, chunkDescriptorAllocation);
			chunkDescriptorBuffer = VK_NULL_HANDLE;
			chunkDescriptorAllocation = VK_NULL_HANDLE;
		}
		packedVertexMappedData = nullptr;
		chunkDescriptorMappedData = nullptr;
		graphicsDescriptorSet = VK_NULL_HANDLE;
		uploadedSceneVersion = 0;
		chunkDescriptorCount = 0;
		vertexCount = 0;
		opaqueVertexCount = 0;
		transparentVertexCount = 0;
	}

	render->sceneVertexCapacity = 0;
	render->sceneTriangleCount = 0;
	render->sceneUploadVertexCount = 0;
	render->sceneUploadOpaqueVertexCount = 0;
	render->sceneUploadTransparentVertexCount = 0;
	render->sceneUploadVersion = 0;
	render->sceneUploadCacheDirty = true;
	render->sceneChunkRenderCaches.clear();
	render->scenePackedVertexPayload.clear();
	render->transparentPackedVertexScratch.clear();
	render->sceneChunkDescriptors.clear();
	render->opaqueChunkDrawRanges.clear();
	render->transparentChunkDrawRanges.clear();
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
		SDL_Log("Chunk size %d exceeds packed scene payload limit", world->voxelWorld->chunkSize);
		return false;
	}

	DestroySceneResources(context, render);

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.flags =
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
		VMA_ALLOCATION_CREATE_MAPPED_BIT;
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

	for (SceneFrameResources &frameResources : render->sceneFrameResources) {
		VmaAllocationInfo packedVertexAllocationInfo{};
		if (!CreateBuffer(
				context,
				sizeof(SceneChunkMeshVertex) * static_cast<VkDeviceSize>(kMaxSceneVertices),
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				allocationInfo,
				&frameResources.packedVertexBuffer,
				&frameResources.packedVertexAllocation,
				&packedVertexAllocationInfo)) {
			DestroySceneResources(context, render);
			return false;
		}
		frameResources.packedVertexMappedData = packedVertexAllocationInfo.pMappedData;
		profiling::RecordAllocation(
			frameResources.packedVertexAllocation,
			packedVertexAllocationInfo.size,
			"ScenePackedVertexBufferAllocation");

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

		char bufferName[64]{};
		std::snprintf(bufferName, sizeof(bufferName), "ScenePackedVertexBuffer[%zu]", static_cast<size_t>(&frameResources - render->sceneFrameResources.data()));
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frameResources.packedVertexBuffer),
			VK_OBJECT_TYPE_BUFFER,
			bufferName);
		std::snprintf(bufferName, sizeof(bufferName), "SceneChunkDescriptorBuffer[%zu]", static_cast<size_t>(&frameResources - render->sceneFrameResources.data()));
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frameResources.chunkDescriptorBuffer),
			VK_OBJECT_TYPE_BUFFER,
			bufferName);
	}

	render->sceneVertexCapacity = kMaxSceneVertices;
	render->sceneUploadVertexCount = 0;
	render->sceneUploadOpaqueVertexCount = 0;
	render->sceneUploadTransparentVertexCount = 0;
	render->sceneUploadVersion = 0;
	render->sceneUploadCacheDirty = true;
	render->sceneChunkRenderCaches.clear();
	render->sceneChunkRenderCaches.resize(world->voxelWorld->chunks.size());
	render->scenePackedVertexPayload.clear();
	render->scenePackedVertexPayload.reserve(kMaxSceneVertices);
	render->transparentPackedVertexScratch.clear();
	render->transparentPackedVertexScratch.reserve(kMaxSceneVertices / 4);
	InitializeSceneChunkDescriptors(*world->voxelWorld, *render);
	render->opaqueChunkDrawRanges.clear();
	render->opaqueChunkDrawRanges.reserve(world->voxelWorld->chunks.size());
	render->transparentChunkDrawRanges.clear();
	render->transparentChunkDrawRanges.reserve(world->voxelWorld->chunks.size());
	render->pendingChunkRebuildIndices.clear();
	render->pendingChunkRebuildIndices.reserve(world->voxelWorld->chunks.size());
	render->completedChunkRebuildIndices.clear();
	render->completedChunkRebuildIndices.reserve(world->voxelWorld->chunks.size());
	if (!RefreshGraphicsResourceBindings(context, render)) {
		DestroySceneResources(context, render);
		return false;
	}
	return true;
}

bool UpdateSceneResources(
	WorldState *world,
	RenderState *render)
{
	PV_PROFILE_ZONE_N("UpdateSceneResources");
	if (!world || !render || !world->voxelWorld) {
		return false;
	}

	const uint32_t dirtyChunkCount = world->voxelWorld->stats.dirtyChunkCount;
	const uint32_t activeChunkCount = world->voxelWorld->stats.activeChunkCount;
	uint32_t rebuiltChunkCount = 0;
	uint64_t rebuiltMeshVertexCount = 0;

	if (render->sceneChunkRenderCaches.size() != world->voxelWorld->chunks.size()) {
		render->sceneChunkRenderCaches.clear();
		render->sceneChunkRenderCaches.resize(world->voxelWorld->chunks.size());
		InitializeSceneChunkDescriptors(*world->voxelWorld, *render);
		render->sceneUploadCacheDirty = true;
	}

	render->completedChunkRebuildIndices.clear();
	for (const size_t chunkIndex : render->pendingChunkRebuildIndices) {
		if (chunkIndex >= world->voxelWorld->chunks.size() ||
			chunkIndex >= render->sceneChunkRenderCaches.size()) {
			continue;
		}

		const VoxelChunk &chunk = world->voxelWorld->chunks[chunkIndex];
		RebuildChunkMesh(*world->voxelWorld, chunk, render->sceneChunkRenderCaches[chunkIndex]);
		++rebuiltChunkCount;
		rebuiltMeshVertexCount += render->sceneChunkRenderCaches[chunkIndex].meshVertices.size();
		render->completedChunkRebuildIndices.push_back(chunkIndex);
		render->sceneUploadCacheDirty = true;
	}
	render->pendingChunkRebuildIndices.clear();

	profiling::PlotValue("Dirty Chunks", static_cast<int64_t>(dirtyChunkCount));
	profiling::PlotValue("Active Chunks", static_cast<int64_t>(activeChunkCount));
	profiling::PlotValue("Rebuilt Chunks", static_cast<int64_t>(rebuiltChunkCount));
	profiling::PlotValue("Rebuilt Mesh Vertices", static_cast<int64_t>(rebuiltMeshVertexCount));
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
	EnsureSceneUploadCache(render);

	SceneFrameResources &frameResources = render.sceneFrameResources[frameIndex];
	uint32_t uploadedVertexCount = 0;
	uint32_t uploadedOpaqueVertexCount = 0;
	uint32_t uploadedTransparentVertexCount = 0;
	uint32_t uploadedChunkDescriptorCount = 0;

	if (frameResources.uploadedSceneVersion != render.sceneUploadVersion) {
		if (!render.scenePackedVertexPayload.empty()) {
			std::memcpy(
				frameResources.packedVertexMappedData,
				render.scenePackedVertexPayload.data(),
				render.scenePackedVertexPayload.size() * sizeof(SceneChunkMeshVertex));
		}
		if (!render.sceneChunkDescriptors.empty()) {
			std::memcpy(
				frameResources.chunkDescriptorMappedData,
				render.sceneChunkDescriptors.data(),
				render.sceneChunkDescriptors.size() * sizeof(PackedSceneChunkDescriptor));
		}
		frameResources.uploadedSceneVersion = render.sceneUploadVersion;
		uploadedChunkDescriptorCount = static_cast<uint32_t>(render.sceneChunkDescriptors.size());
		uploadedVertexCount = render.sceneUploadVertexCount;
		uploadedOpaqueVertexCount = render.sceneUploadOpaqueVertexCount;
		uploadedTransparentVertexCount = render.sceneUploadTransparentVertexCount;
	}

	frameResources.chunkDescriptorCount = static_cast<uint32_t>(render.sceneChunkDescriptors.size());
	frameResources.vertexCount = render.sceneUploadVertexCount;
	frameResources.opaqueVertexCount = render.sceneUploadOpaqueVertexCount;
	frameResources.transparentVertexCount = render.sceneUploadTransparentVertexCount;
	profiling::PlotValue("Scene Triangles", static_cast<int64_t>(render.sceneTriangleCount));
	profiling::PlotValue("Opaque Chunk Draws", static_cast<int64_t>(render.opaqueChunkDrawRanges.size()));
	profiling::PlotValue("Transparent Chunk Draws", static_cast<int64_t>(render.transparentChunkDrawRanges.size()));
	profiling::PlotValue("Uploaded Vertices", static_cast<int64_t>(uploadedVertexCount));
	profiling::PlotValue("Uploaded Chunk Descriptors", static_cast<int64_t>(uploadedChunkDescriptorCount));
	profiling::PlotValue("Uploaded Opaque Vertices", static_cast<int64_t>(uploadedOpaqueVertexCount));
	profiling::PlotValue(
		"Uploaded Transparent Vertices",
		static_cast<int64_t>(uploadedTransparentVertexCount));
	profiling::PlotValue(
		"Upload Vertex Bytes",
		static_cast<int64_t>(uploadedVertexCount * sizeof(SceneChunkMeshVertex)));
	profiling::PlotValue(
		"Upload Descriptor Bytes",
		static_cast<int64_t>(uploadedChunkDescriptorCount * sizeof(PackedSceneChunkDescriptor)));
	return true;
}

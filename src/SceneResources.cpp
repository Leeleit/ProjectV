#include "SceneResources.hpp"

#include "VoxelMaterials.hpp"
#include "VoxelWorld.hpp"
#include "VulkanDebug.hpp"

#include <array>
#include <cstdio>
#include <vector>

namespace {
constexpr uint32_t kMaxSceneVertices = 262144;
constexpr float kRenderMaterialOpaque = 0.0f;
constexpr float kRenderMaterialGlass = 1.0f;
constexpr float kRenderMaterialFluid = 2.0f;

struct Float3 {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

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
	const Float3 normal,
	const std::array<Float3, 4> &corners)
{
	const SceneChunkMeshVertex v0{
		.position = {corners[0].x, corners[0].y, corners[0].z},
		.normal = {normal.x, normal.y, normal.z},
		.material = material,
	};
	const SceneChunkMeshVertex v1{
		.position = {corners[1].x, corners[1].y, corners[1].z},
		.normal = {normal.x, normal.y, normal.z},
		.material = material,
	};
	const SceneChunkMeshVertex v2{
		.position = {corners[2].x, corners[2].y, corners[2].z},
		.normal = {normal.x, normal.y, normal.z},
		.material = material,
	};
	const SceneChunkMeshVertex v3{
		.position = {corners[3].x, corners[3].y, corners[3].z},
		.normal = {normal.x, normal.y, normal.z},
		.material = material,
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
	chunkRenderCache.meshVertices.clear();

	constexpr std::array<Int3, 6> neighborOffsets{{
		{1, 0, 0},
		{-1, 0, 0},
		{0, 1, 0},
		{0, -1, 0},
		{0, 0, 1},
		{0, 0, -1},
	}};
	constexpr std::array<Float3, 6> normals{{
		{1.0f, 0.0f, 0.0f},
		{-1.0f, 0.0f, 0.0f},
		{0.0f, 1.0f, 0.0f},
		{0.0f, -1.0f, 0.0f},
		{0.0f, 0.0f, 1.0f},
		{0.0f, 0.0f, -1.0f},
	}};

	for (int z = chunk.min.z; z < chunk.maxExclusive.z; ++z) {
		for (int y = chunk.min.y; y < chunk.maxExclusive.y; ++y) {
			for (int x = chunk.min.x; x < chunk.maxExclusive.x; ++x) {
				const Int3 position{x, y, z};
				const VoxelMaterial material = GetVoxelMaterial(world, position);
				if (material == VoxelMaterial::Air) {
					continue;
				}

				const float x0 = static_cast<float>(x);
				const float x1 = x0 + 1.0f;
				const float y0 = static_cast<float>(y);
				const float y1 = y0 + 1.0f;
				const float z0 = static_cast<float>(z);
				const float z1 = z0 + 1.0f;

				const std::array<std::array<Float3, 4>, 6> faceCorners{{
					{{{x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, {x1, y0, z1}}},
					{{{x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}, {x0, y0, z0}}},
					{{{x0, y1, z0}, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}}},
					{{{x0, y0, z1}, {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}}},
					{{{x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}, {x0, y0, z1}}},
					{{{x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}, {x1, y0, z0}}},
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

					EmitFaceToChunkMesh(chunkRenderCache, material, normals[faceIndex], faceCorners[faceIndex]);
				}
			}
		}
	}
}

struct SceneUploadCounts {
	uint32_t vertexCount = 0;
	uint32_t opaqueVertexCount = 0;
	uint32_t transparentVertexCount = 0;
};

bool BuildCombinedSceneVertices(
	const RenderState &render,
	RenderVertex *mappedVertices,
	SceneUploadCounts *outCounts)
{
	if (!mappedVertices || !outCounts) {
		return false;
	}

	uint32_t opaqueVertexCount = 0;
	std::vector<RenderVertex> transparentVertices;
	transparentVertices.reserve(render.sceneVertexCapacity / 4);

	for (const auto &[meshVertices] : render.sceneChunkRenderCaches) {
		for (const auto &[position, normal, material] : meshVertices) {
			if (opaqueVertexCount + transparentVertices.size() >= render.sceneVertexCapacity) {
				break;
			}

			const VoxelMaterialVisual visual = GetVoxelMaterialVisual(material);
			const float materialKind =
				material == VoxelMaterial::Glass   ? kRenderMaterialGlass
				: material == VoxelMaterial::Fluid ? kRenderMaterialFluid
												   : kRenderMaterialOpaque;
			const RenderVertex renderVertex{
				.position = position,
				.normal = normal,
				.color = visual.baseColor,
				.materialKind = materialKind,
			};
			if (material == VoxelMaterial::Glass) {
				transparentVertices.push_back(renderVertex);
			} else {
				mappedVertices[opaqueVertexCount++] = renderVertex;
			}
		}

		if (opaqueVertexCount + transparentVertices.size() >= render.sceneVertexCapacity) {
			break;
		}
	}

	if (!transparentVertices.empty()) {
		std::memcpy(
			mappedVertices + opaqueVertexCount,
			transparentVertices.data(),
			transparentVertices.size() * sizeof(RenderVertex));
	}

	outCounts->opaqueVertexCount = opaqueVertexCount;
	outCounts->transparentVertexCount = static_cast<uint32_t>(transparentVertices.size());
	outCounts->vertexCount = opaqueVertexCount + outCounts->transparentVertexCount;
	return true;
}
} // namespace

void DestroySceneResources(
	VulkanContextState *context,
	RenderState *render)
{
	if (!context || !render || !context->allocator) {
		return;
	}

	for (auto &[mappedData, vertexBuffer, vertexAllocation, vertexCount, opaqueVertexCount, transparentVertexCount] : render->sceneFrameResources) {
		if (vertexBuffer && vertexAllocation) {
			vmaDestroyBuffer(context->allocator, vertexBuffer, vertexAllocation);
			vertexBuffer = VK_NULL_HANDLE;
			vertexAllocation = VK_NULL_HANDLE;
		}
		mappedData = nullptr;
		vertexCount = 0;
		opaqueVertexCount = 0;
		transparentVertexCount = 0;
	}

	render->sceneVertexCapacity = 0;
	render->sceneTriangleCount = 0;
	render->sceneVertexBufferDirty = true;
	render->sceneChunkRenderCaches.clear();
}

bool CreateSceneResources(
	VulkanContextState *context,
	WorldState *world,
	RenderState *render)
{
	if (!context || !world || !render || !context->allocator || !world->voxelWorld) {
		return false;
	}

	DestroySceneResources(context, render);

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.flags =
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
		VMA_ALLOCATION_CREATE_MAPPED_BIT;
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

	for (SceneFrameResources &frameResources : render->sceneFrameResources) {
		VmaAllocationInfo allocationResultInfo{};
		if (!CreateBuffer(
				context,
				sizeof(RenderVertex) * static_cast<VkDeviceSize>(kMaxSceneVertices),
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				allocationInfo,
				&frameResources.vertexBuffer,
				&frameResources.vertexAllocation,
				&allocationResultInfo)) {
			DestroySceneResources(context, render);
			return false;
		}
		frameResources.mappedData = allocationResultInfo.pMappedData;

		char bufferName[64]{};
		std::snprintf(bufferName, sizeof(bufferName), "SceneVertexBuffer[%zu]", static_cast<size_t>(&frameResources - render->sceneFrameResources.data()));
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(frameResources.vertexBuffer),
			VK_OBJECT_TYPE_BUFFER,
			bufferName);
	}

	render->sceneVertexCapacity = kMaxSceneVertices;
	render->sceneVertexBufferDirty = true;
	render->sceneChunkRenderCaches.clear();
	render->sceneChunkRenderCaches.resize(world->voxelWorld->chunks.size());
	return UpdateSceneResources(world, render);
}

bool UpdateSceneResources(
	WorldState *world,
	RenderState *render)
{
	if (!world || !render || !world->voxelWorld) {
		return false;
	}

	if (render->sceneChunkRenderCaches.size() != world->voxelWorld->chunks.size()) {
		render->sceneChunkRenderCaches.clear();
		render->sceneChunkRenderCaches.resize(world->voxelWorld->chunks.size());
		render->sceneVertexBufferDirty = true;
	}

	for (size_t chunkIndex = 0; chunkIndex < world->voxelWorld->chunks.size(); ++chunkIndex) {
		VoxelChunk &chunk = world->voxelWorld->chunks[chunkIndex];
		if (chunk.dirty) {
			RebuildChunkMesh(*world->voxelWorld, chunk, render->sceneChunkRenderCaches[chunkIndex]);
			chunk.dirty = false;
			if (world->voxelWorld->stats.dirtyChunkCount > 0) {
				--world->voxelWorld->stats.dirtyChunkCount;
			}
			render->sceneVertexBufferDirty = true;
		}
	}

	if (!render->sceneVertexBufferDirty) {
		return true;
	}

	render->sceneVertexBufferDirty = false;
	return true;
}

bool UploadSceneFrameResources(
	const WorldState *world,
	RenderState *render,
	const uint32_t frameIndex)
{
	if (!world || !render || frameIndex >= render->sceneFrameResources.size()) {
		return false;
	}
	if (!world->voxelWorld) {
		return false;
	}

	SceneFrameResources &frameResources = render->sceneFrameResources[frameIndex];
	SceneUploadCounts uploadCounts{};
	if (!BuildCombinedSceneVertices(
			*render,
			static_cast<RenderVertex *>(frameResources.mappedData),
			&uploadCounts)) {
		return false;
	}

	frameResources.vertexCount = uploadCounts.vertexCount;
	frameResources.opaqueVertexCount = uploadCounts.opaqueVertexCount;
	frameResources.transparentVertexCount = uploadCounts.transparentVertexCount;
	render->sceneTriangleCount = uploadCounts.vertexCount / 3;
	return true;
}

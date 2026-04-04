#include "SceneResources.hpp"

#include "VoxelMaterials.hpp"
#include "VoxelWorld.hpp"

#include <array>
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
	AppState *state,
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
			   state->allocator,
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
	VoxelChunk &chunk,
	const VoxelMaterial material,
	const Float3 normal,
	const std::array<Float3, 4> &corners)
{
	const VoxelChunkMeshVertex v0{
		.position = {corners[0].x, corners[0].y, corners[0].z},
		.normal = {normal.x, normal.y, normal.z},
		.material = material,
	};
	const VoxelChunkMeshVertex v1{
		.position = {corners[1].x, corners[1].y, corners[1].z},
		.normal = {normal.x, normal.y, normal.z},
		.material = material,
	};
	const VoxelChunkMeshVertex v2{
		.position = {corners[2].x, corners[2].y, corners[2].z},
		.normal = {normal.x, normal.y, normal.z},
		.material = material,
	};
	const VoxelChunkMeshVertex v3{
		.position = {corners[3].x, corners[3].y, corners[3].z},
		.normal = {normal.x, normal.y, normal.z},
		.material = material,
	};

	chunk.meshVertices.push_back(v0);
	chunk.meshVertices.push_back(v1);
	chunk.meshVertices.push_back(v2);
	chunk.meshVertices.push_back(v0);
	chunk.meshVertices.push_back(v2);
	chunk.meshVertices.push_back(v3);
}

void RebuildChunkMesh(const VoxelWorld &world, VoxelChunk &chunk)
{
	chunk.meshVertices.clear();

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

					EmitFaceToChunkMesh(chunk, material, normals[faceIndex], faceCorners[faceIndex]);
				}
			}
		}
	}

	chunk.dirty = false;
}

bool RebuildCombinedSceneVertexBuffer(AppState &state)
{
	if (!state.voxelWorld || !state.sceneVertexMappedData) {
		return false;
	}

	RenderVertex *mappedVertices = static_cast<RenderVertex *>(state.sceneVertexMappedData);
	uint32_t opaqueVertexCount = 0;
	std::vector<RenderVertex> transparentVertices;
	transparentVertices.reserve(state.sceneVertexCapacity / 4);

	for (const VoxelChunk &chunk : state.voxelWorld->chunks) {
		for (const auto &[position, normal, material] : chunk.meshVertices) {
			if (opaqueVertexCount + transparentVertices.size() >= state.sceneVertexCapacity) {
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

		if (opaqueVertexCount + transparentVertices.size() >= state.sceneVertexCapacity) {
			break;
		}
	}

	if (!transparentVertices.empty()) {
		std::memcpy(
			mappedVertices + opaqueVertexCount,
			transparentVertices.data(),
			transparentVertices.size() * sizeof(RenderVertex));
	}

	state.sceneOpaqueVertexCount = opaqueVertexCount;
	state.sceneTransparentVertexCount = static_cast<uint32_t>(transparentVertices.size());
	state.sceneVertexCount = opaqueVertexCount + state.sceneTransparentVertexCount;
	state.sceneTriangleCount = state.sceneVertexCount / 3;
	state.sceneVertexBufferDirty = false;
	return true;
}
} // namespace

void DestroySceneResources(AppState *state)
{
	if (!state || !state->allocator) {
		return;
	}

	if (state->sceneVertexBuffer && state->sceneVertexAllocation) {
		vmaDestroyBuffer(state->allocator, state->sceneVertexBuffer, state->sceneVertexAllocation);
		state->sceneVertexBuffer = VK_NULL_HANDLE;
		state->sceneVertexAllocation = VK_NULL_HANDLE;
	}

	state->sceneVertexMappedData = nullptr;
	state->sceneVertexCapacity = 0;
	state->sceneVertexCount = 0;
	state->sceneOpaqueVertexCount = 0;
	state->sceneTransparentVertexCount = 0;
	state->sceneTriangleCount = 0;
	state->sceneVertexBufferDirty = true;
}

bool CreateSceneResources(AppState *state)
{
	if (!state || !state->allocator || !state->voxelWorld) {
		return false;
	}

	DestroySceneResources(state);

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.flags =
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
		VMA_ALLOCATION_CREATE_MAPPED_BIT;
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

	VmaAllocationInfo allocationResultInfo{};
	if (!CreateBuffer(
			state,
			sizeof(RenderVertex) * static_cast<VkDeviceSize>(kMaxSceneVertices),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			allocationInfo,
			&state->sceneVertexBuffer,
			&state->sceneVertexAllocation,
			&allocationResultInfo)) {
		return false;
	}

	state->sceneVertexMappedData = allocationResultInfo.pMappedData;
	state->sceneVertexCapacity = kMaxSceneVertices;
	state->sceneVertexBufferDirty = true;
	return UpdateSceneResources(state);
}

bool UpdateSceneResources(AppState *state)
{
	if (!state || !state->voxelWorld) {
		return false;
	}

	for (VoxelChunk &chunk : state->voxelWorld->chunks) {
		if (chunk.dirty) {
			RebuildChunkMesh(*state->voxelWorld, chunk);
			state->sceneVertexBufferDirty = true;
		}
	}

	if (!state->sceneVertexBufferDirty) {
		return true;
	}

	return RebuildCombinedSceneVertexBuffer(*state);
}

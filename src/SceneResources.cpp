#include "SceneResources.hpp"

#include "VoxelWorld.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace {
constexpr uint32_t kMaxSceneTriangles = 8192;

struct Float3 {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

struct ProjectedVertex {
	ComputeVertex vertex{};
	bool valid = false;
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

Float3 Normalize(const Float3 vector)
{
	const float length = std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
	if (length <= 0.00001f) {
		return {};
	}

	return {vector.x / length, vector.y / length, vector.z / length};
}

float Dot(const Float3 a, const Float3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

Float3 Cross(const Float3 a, const Float3 b)
{
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x,
	};
}

Float3 Subtract(const Float3 a, const Float3 b)
{
	return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Float3 GetForwardVector(const CameraState &camera)
{
	return {
		std::cos(camera.pitchRadians) * std::sin(camera.yawRadians),
		std::sin(camera.pitchRadians),
		-std::cos(camera.pitchRadians) * std::cos(camera.yawRadians),
	};
}

Float3 GetRightVector(const CameraState &camera)
{
	return Normalize(Cross(GetForwardVector(camera), Float3{0.0f, 1.0f, 0.0f}));
}

Float3 GetUpVector(const CameraState &camera)
{
	return Normalize(Cross(GetRightVector(camera), GetForwardVector(camera)));
}

std::array<float, 4> GetMaterialColor(const VoxelMaterial material)
{
	switch (material) {
	case VoxelMaterial::Glass:
		return {0.94f, 0.96f, 1.00f, 1.0f};
	case VoxelMaterial::Fluid:
		return {0.00f, 0.60f, 1.00f, 1.0f};
	case VoxelMaterial::FloorWhite:
		return {1.00f, 1.00f, 1.00f, 1.0f};
	case VoxelMaterial::FloorGray:
		return {0.78f, 0.80f, 0.82f, 1.0f};
	case VoxelMaterial::Air:
	default:
		return {0.0f, 0.0f, 0.0f, 1.0f};
	}
}

Float3 ApplyLighting(const std::array<float, 4> &baseColor, const Float3 normal)
{
	const auto [x, y, z] = Normalize(Float3{0.35f, -0.80f, 0.45f});
	const float diffuse = std::max(0.0f, Dot(normal, Float3{-x, -y, -z}));
	const float light = 0.30f + diffuse * 0.70f;
	return {
		baseColor[0] * light,
		baseColor[1] * light,
		baseColor[2] * light,
	};
}

ProjectedVertex ProjectVertex(AppState &state, const Float3 worldPosition, const Float3 color)
{
	const Float3 cameraPosition{
		state.camera.position[0],
		state.camera.position[1],
		state.camera.position[2],
	};
	const Float3 forward = Normalize(GetForwardVector(state.camera));
	const Float3 right = GetRightVector(state.camera);
	const Float3 up = GetUpVector(state.camera);
	const Float3 relative = Subtract(worldPosition, cameraPosition);
	const float viewX = Dot(relative, right);
	const float viewY = Dot(relative, up);
	const float viewZ = Dot(relative, forward);
	if (viewZ <= state.camera.nearPlane || viewZ >= state.camera.farPlane) {
		return {};
	}

	const float aspect = static_cast<float>(state.extent.width) / static_cast<float>(state.extent.height);
	const float tanHalfFov = std::tan(state.camera.verticalFovRadians * 0.5f);
	const float ndcX = viewX / (viewZ * tanHalfFov * aspect);
	const float ndcY = viewY / (viewZ * tanHalfFov);
	if (std::abs(ndcX) > 1.5f || std::abs(ndcY) > 1.5f) {
		return {};
	}

	ProjectedVertex projected{};
	projected.vertex.position = {
		ndcX,
		ndcY,
		(viewZ - state.camera.nearPlane) / (state.camera.farPlane - state.camera.nearPlane),
		1.0f,
	};
	projected.vertex.color = {color.x, color.y, color.z, 1.0f};
	projected.valid = true;
	return projected;
}

void EmitTriangle(
	std::vector<ComputeVertex> &vertices,
	const ProjectedVertex &a,
	const ProjectedVertex &b,
	const ProjectedVertex &c)
{
	if (!a.valid || !b.valid || !c.valid) {
		return;
	}

	vertices.push_back(a.vertex);
	vertices.push_back(b.vertex);
	vertices.push_back(c.vertex);
}

void EmitVoxelFace(
	AppState &state,
	std::vector<ComputeVertex> &vertices,
	const Int3 voxelPosition,
	const VoxelMaterial material,
	const Float3 normal,
	const std::array<Float3, 4> &corners)
{
	if (vertices.size() + 6 > state.sceneVertexCapacity) {
		return;
	}

	const Float3 faceCenter{
		static_cast<float>(voxelPosition.x) + 0.5f + normal.x * 0.5f,
		static_cast<float>(voxelPosition.y) + 0.5f + normal.y * 0.5f,
		static_cast<float>(voxelPosition.z) + 0.5f + normal.z * 0.5f,
	};
	const Float3 cameraPosition{
		state.camera.position[0],
		state.camera.position[1],
		state.camera.position[2],
	};
	const Float3 toCamera = Subtract(cameraPosition, faceCenter);
	if (Dot(normal, toCamera) <= 0.0f) {
		return;
	}

	const Float3 litColor = ApplyLighting(GetMaterialColor(material), normal);
	const ProjectedVertex projected0 = ProjectVertex(state, corners[0], litColor);
	const ProjectedVertex projected1 = ProjectVertex(state, corners[1], litColor);
	const ProjectedVertex projected2 = ProjectVertex(state, corners[2], litColor);
	const ProjectedVertex projected3 = ProjectVertex(state, corners[3], litColor);
	if (!projected0.valid || !projected1.valid || !projected2.valid || !projected3.valid) {
		return;
	}

	EmitTriangle(vertices, projected0, projected1, projected2);
	EmitTriangle(vertices, projected0, projected2, projected3);
}

bool RebuildSceneBuffer(AppState *state)
{
	if (!state || !state->voxelWorld || !state->sceneVertexMappedData) {
		return false;
	}

	if (state->extent.width == 0 || state->extent.height == 0) {
		state->sceneTriangleCount = 0;
		return true;
	}

	std::vector<ComputeVertex> vertices;
	vertices.reserve(state->sceneVertexCapacity);

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

	for (int z = state->voxelWorld->min.z; z < state->voxelWorld->maxExclusive.z; ++z) {
		for (int y = state->voxelWorld->min.y; y < state->voxelWorld->maxExclusive.y; ++y) {
			for (int x = state->voxelWorld->min.x; x < state->voxelWorld->maxExclusive.x; ++x) {
				const Int3 position{x, y, z};
				const VoxelMaterial material = GetVoxelMaterial(*state->voxelWorld, position);
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
					if (GetVoxelMaterial(*state->voxelWorld, neighbor) != VoxelMaterial::Air) {
						continue;
					}

					if (vertices.size() + 6 > state->sceneVertexCapacity) {
						break;
					}

					EmitVoxelFace(
						*state,
						vertices,
						position,
						material,
						normals[faceIndex],
						faceCorners[faceIndex]);
				}
			}
		}
	}

	const size_t byteSize = vertices.size() * sizeof(ComputeVertex);
	if (byteSize > 0) {
		std::memcpy(state->sceneVertexMappedData, vertices.data(), byteSize);
	}
	state->sceneTriangleCount = static_cast<uint32_t>(vertices.size() / 3);
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
	state->sceneTriangleCount = 0;
}

bool CreateSceneResources(AppState *state)
{
	if (!state || !state->allocator) {
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
			sizeof(ComputeVertex) * kMaxSceneTriangles * 3ull,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			allocationInfo,
			&state->sceneVertexBuffer,
			&state->sceneVertexAllocation,
			&allocationResultInfo)) {
		return false;
	}

	state->sceneVertexMappedData = allocationResultInfo.pMappedData;
	state->sceneVertexCapacity = kMaxSceneTriangles * 3;
	return RebuildSceneBuffer(state);
}

bool UpdateSceneResources(AppState *state)
{
	return RebuildSceneBuffer(state);
}

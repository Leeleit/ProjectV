#pragma once

#include <cstdint>
#include <vector>

namespace projectv::voxel {

constexpr uint32_t kMaxChunkExtentForGreedyCpu = 64u;

struct CpuGreedyFace {
	uint32_t localVoxelFace = 0;
	uint32_t chunkIndexMaterial = 0;
	uint32_t lightingData = 0;
	uint32_t packedExtents = 0;
	bool operator==(const CpuGreedyFace &) const = default;
};
static_assert(sizeof(CpuGreedyFace) == 16);

struct CpuGreedyChunkDesc {
	int32_t chunkOrigin[3] = {0, 0, 0};
	uint32_t extent[3] = {0, 0, 0};
	uint32_t nonAirCount = 0;
	uint32_t chunkIndex = 0;
};

struct CpuGreedyInput {
	const uint8_t *worldVoxels = nullptr;
	int worldDim[3] = {0, 0, 0};
	int32_t worldMin[3] = {0, 0, 0};
	CpuGreedyChunkDesc chunk;
	uint8_t lodLevel = 0;
	uint32_t lodExtent = 0;
	const uint8_t *lodVoxels = nullptr;
};

struct CpuGreedyMeshResult {
	std::vector<CpuGreedyFace> opaqueFaces;
	std::vector<CpuGreedyFace> transparentFaces;
};

bool ShouldEmitVoxelFaceCPU(uint8_t materialIndex, uint8_t neighborMaterialIndex);
bool IsSameMeshingGroupCPU(uint8_t materialA, uint8_t materialB);

uint32_t PackLocalVoxelFaceCPU(uint32_t x, uint32_t y, uint32_t z, uint32_t faceIndex);
uint32_t PackQuadExtentsCPU(uint32_t width, uint32_t height);
uint32_t PackChunkIndexMaterialCPU(uint32_t chunkIndex, uint32_t materialIndex);

struct UnpackedLocalVoxelFace {
	uint32_t x = 0;
	uint32_t y = 0;
	uint32_t z = 0;
	uint32_t faceIndex = 0;
};
UnpackedLocalVoxelFace UnpackLocalVoxelFaceCPU(uint32_t packed);

struct UnpackedQuadExtents {
	uint32_t width = 0;
	uint32_t height = 0;
};
UnpackedQuadExtents UnpackQuadExtentsCPU(uint32_t packed);

struct UnpackedChunkIndexMaterial {
	uint32_t chunkIndex = 0;
	uint32_t materialIndex = 0;
};
UnpackedChunkIndexMaterial UnpackChunkIndexMaterialCPU(uint32_t packed);

uint8_t ReadVoxelMaterialCPU(const CpuGreedyInput &input, int32_t worldX, int32_t worldY, int32_t worldZ);

CpuGreedyMeshResult GenerateCpuGreedyMesh(const CpuGreedyInput &input);

struct CpuChunkCullingParams {
	float cameraX = 0.0f, cameraY = 0.0f, cameraZ = 0.0f;
	float maxDistance = 0.0f;
	float cameraForwardX = 0.0f, cameraForwardY = 0.0f, cameraForwardZ = 0.0f;
	float tanHalfVerticalFov = 0.0f;
	float cameraRightX = 0.0f, cameraRightY = 0.0f, cameraRightZ = 0.0f;
	float tanHalfHorizontalFov = 0.0f;
	float cameraUpX = 0.0f, cameraUpY = 0.0f, cameraUpZ = 0.0f;
	float nearPlane = 0.0f;
};

bool IsChunkVisibleCPU(const CpuGreedyChunkDesc &chunk, const CpuChunkCullingParams &culling);

} 

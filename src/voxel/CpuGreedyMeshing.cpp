#include "voxel/CpuGreedyMeshing.hpp"

#include <algorithm>
#include <cmath>

namespace projectv::voxel {

bool ShouldEmitVoxelFaceCPU(const uint8_t materialIndex, const uint8_t neighborMaterialIndex)
{
	if (materialIndex == 0u) {
		return false;
	}
	if (materialIndex >= 3u) {
		return neighborMaterialIndex == 0u || neighborMaterialIndex == 1u;
	}
	if (materialIndex == 2u) {
		return true;
	}
	return neighborMaterialIndex == 0u;
}

bool IsSameMeshingGroupCPU(const uint8_t materialA, const uint8_t materialB)
{
	if (materialA == materialB) { return true; }
	const bool aIsFloor = materialA == 3u || materialA == 4u;
	const bool bIsFloor = materialB == 3u || materialB == 4u;
	return aIsFloor && bIsFloor;
}

uint32_t PackLocalVoxelFaceCPU(const uint32_t x, const uint32_t y, const uint32_t z, const uint32_t faceIndex)
{
	return (x & 0xFFu) | ((y & 0xFFu) << 8u) | ((z & 0xFFu) << 16u) | ((faceIndex & 0xFFu) << 24u);
}

uint32_t PackQuadExtentsCPU(const uint32_t width, const uint32_t height)
{
	return (width & 0xFFu) | ((height & 0xFFu) << 8u);
}

uint32_t PackChunkIndexMaterialCPU(const uint32_t chunkIndex, const uint32_t materialIndex)
{
	return (chunkIndex & 0x00FFFFFFu) | ((materialIndex & 0xFFu) << 24u);
}

UnpackedLocalVoxelFace UnpackLocalVoxelFaceCPU(const uint32_t packed)
{
	return {packed & 0xFFu, (packed >> 8u) & 0xFFu, (packed >> 16u) & 0xFFu, (packed >> 24u) & 0xFFu};
}

UnpackedQuadExtents UnpackQuadExtentsCPU(const uint32_t packed)
{
	return {packed & 0xFFu, (packed >> 8u) & 0xFFu};
}

UnpackedChunkIndexMaterial UnpackChunkIndexMaterialCPU(const uint32_t packed)
{
	return {packed & 0x00FFFFFFu, (packed >> 24u) & 0xFFu};
}

uint8_t ReadVoxelMaterialCPU(const CpuGreedyInput &input, const int32_t worldX, const int32_t worldY, const int32_t worldZ)
{
	const int32_t maxX = input.worldMin[0] + input.worldDim[0];
	const int32_t maxY = input.worldMin[1] + input.worldDim[1];
	const int32_t maxZ = input.worldMin[2] + input.worldDim[2];
	if (worldX < input.worldMin[0] || worldY < input.worldMin[1] || worldZ < input.worldMin[2] ||
		worldX >= maxX || worldY >= maxY || worldZ >= maxZ) {
		return 0u;
	}
	const int32_t lx = worldX - input.worldMin[0];
	const int32_t ly = worldY - input.worldMin[1];
	const int32_t lz = worldZ - input.worldMin[2];
	const size_t idx = static_cast<size_t>(lx) +
		static_cast<size_t>(input.worldDim[0]) *
			(static_cast<size_t>(ly) + static_cast<size_t>(input.worldDim[1]) * static_cast<size_t>(lz));
	return input.worldVoxels[idx];
}

namespace {

uint8_t GetCellMaterialCPU(const CpuGreedyInput &input, const uint32_t lx, const uint32_t ly, const uint32_t lz)
{
	if (input.lodLevel == 0u) {
		const int32_t wx = input.chunk.chunkOrigin[0] + static_cast<int32_t>(lx);
		const int32_t wy = input.chunk.chunkOrigin[1] + static_cast<int32_t>(ly);
		const int32_t wz = input.chunk.chunkOrigin[2] + static_cast<int32_t>(lz);
		return ReadVoxelMaterialCPU(input, wx, wy, wz);
	}
	if (input.lodExtent == 0u || input.lodVoxels == nullptr) {
		return 0u;
	}
	const uint32_t idx = lx + input.lodExtent * (ly + input.lodExtent * lz);
	return input.lodVoxels[idx];
}

void EmitFace(const uint8_t materialIndex, const uint32_t faceIndex, const uint32_t localCoord[3],
			  const uint32_t chunkIndex, const uint32_t W, const uint32_t H, CpuGreedyMeshResult &result)
{
	CpuGreedyFace face{};
	face.localVoxelFace = PackLocalVoxelFaceCPU(localCoord[0], localCoord[1], localCoord[2], faceIndex);
	face.chunkIndexMaterial = PackChunkIndexMaterialCPU(chunkIndex, materialIndex);
	face.lightingData = 0u;
	face.packedExtents = PackQuadExtentsCPU(W, H);
	if (materialIndex == 1u) {
		result.transparentFaces.push_back(face);
	} else {
		result.opaqueFaces.push_back(face);
	}
}

// EVIL: uint64_t visited (not uint32 like the shader) so extents up to 64 work correctly.
// The shader uses uint visited[64] (32-bit rows), which is buggy for extentU > 32.
// In practice chunkSize=8 so both agree. Property tests use sizes <= 32.
void GreedyFacePassCPU(const uint32_t faceIndex, const uint32_t axisN, const uint32_t axisU, const uint32_t axisV,
					   const int32_t signN, const CpuGreedyInput &input, CpuGreedyMeshResult &result)
{
	const uint32_t extentN = (input.lodLevel == 0u) ? input.chunk.extent[axisN] : input.lodExtent;
	const uint32_t extentU = (input.lodLevel == 0u) ? input.chunk.extent[axisU] : input.lodExtent;
	const uint32_t extentV = (input.lodLevel == 0u) ? input.chunk.extent[axisV] : input.lodExtent;

	const bool useGreedy = (extentU <= kMaxChunkExtentForGreedyCpu) && (extentV <= kMaxChunkExtentForGreedyCpu);

	for (uint32_t pN = 0u; pN < extentN; ++pN) {
		if (useGreedy) {
			uint64_t visited[kMaxChunkExtentForGreedyCpu] = {};

			for (uint32_t pV = 0u; pV < extentV; ++pV) {
				for (uint32_t pU = 0u; pU < extentU; ++pU) {
					if (((visited[pV] >> pU) & 1ULL) != 0ULL) {
						continue;
					}

					uint32_t localCoord[3] = {};
					localCoord[axisN] = pN;
					localCoord[axisU] = pU;
					localCoord[axisV] = pV;

					const uint8_t cellMaterial = GetCellMaterialCPU(input, localCoord[0], localCoord[1], localCoord[2]);
					if (cellMaterial == 0u) {
						continue;
					}

					int32_t worldPos[3] = {};
					worldPos[0] = input.chunk.chunkOrigin[0] + static_cast<int32_t>(localCoord[0]);
					worldPos[1] = input.chunk.chunkOrigin[1] + static_cast<int32_t>(localCoord[1]);
					worldPos[2] = input.chunk.chunkOrigin[2] + static_cast<int32_t>(localCoord[2]);

					int32_t neighborPos[3] = {worldPos[0], worldPos[1], worldPos[2]};
					neighborPos[axisN] += signN;

					const uint8_t neighborMaterial = ReadVoxelMaterialCPU(input, neighborPos[0], neighborPos[1], neighborPos[2]);
					if (!ShouldEmitVoxelFaceCPU(cellMaterial, neighborMaterial)) {
						continue;
					}

					uint32_t W = 0u;
					while (pU + W < extentU) {
						if (((visited[pV] >> (pU + W)) & 1ULL) != 0ULL) {
							break;
						}
						uint32_t testCoord[3] = {};
						testCoord[axisN] = pN;
						testCoord[axisU] = pU + W;
						testCoord[axisV] = pV;
						const uint8_t testMaterial = GetCellMaterialCPU(input, testCoord[0], testCoord[1], testCoord[2]);
						if (!IsSameMeshingGroupCPU(testMaterial, cellMaterial)) {
							break;
						}
						int32_t testNeighbor[3] = {worldPos[0], worldPos[1], worldPos[2]};
						testNeighbor[axisN] += signN;
						testNeighbor[axisU] += static_cast<int32_t>(W);
						const uint8_t testNeighborMaterial =
							ReadVoxelMaterialCPU(input, testNeighbor[0], testNeighbor[1], testNeighbor[2]);
						if (!ShouldEmitVoxelFaceCPU(testMaterial, testNeighborMaterial)) {
							break;
						}
						++W;
					}
					if (W == 0u) {
						continue;
					}

					uint32_t H = 1u;
					while (pV + H < extentV) {
						bool rowValid = true;
						for (uint32_t wi = 0u; wi < W; ++wi) {
							if (((visited[pV + H] >> (pU + wi)) & 1ULL) != 0ULL) {
								rowValid = false;
								break;
							}
							uint32_t testCoord[3] = {};
							testCoord[axisN] = pN;
							testCoord[axisU] = pU + wi;
							testCoord[axisV] = pV + H;
							const uint8_t testMaterial =
								GetCellMaterialCPU(input, testCoord[0], testCoord[1], testCoord[2]);
							if (!IsSameMeshingGroupCPU(testMaterial, cellMaterial)) {
								rowValid = false;
								break;
							}
							int32_t testNeighbor[3] = {worldPos[0], worldPos[1], worldPos[2]};
							testNeighbor[axisN] += signN;
							testNeighbor[axisU] += static_cast<int32_t>(wi);
							testNeighbor[axisV] += static_cast<int32_t>(H);
							const uint8_t testNeighborMaterial =
								ReadVoxelMaterialCPU(input, testNeighbor[0], testNeighbor[1], testNeighbor[2]);
							if (!ShouldEmitVoxelFaceCPU(testMaterial, testNeighborMaterial)) {
								rowValid = false;
								break;
							}
						}
						if (!rowValid) {
							break;
						}
						++H;
					}

					EmitFace(cellMaterial, faceIndex, localCoord, input.chunk.chunkIndex, W, H, result);

					const uint64_t mask = ((1ULL << W) - 1ULL) << pU;
					for (uint32_t v = 0u; v < H; ++v) {
						visited[pV + v] |= mask;
					}
				}
			}
		} else {
			for (uint32_t pV = 0u; pV < extentV; ++pV) {
				for (uint32_t pU = 0u; pU < extentU; ++pU) {
					uint32_t localCoord[3] = {};
					localCoord[axisN] = pN;
					localCoord[axisU] = pU;
					localCoord[axisV] = pV;

					const uint8_t cellMaterial = GetCellMaterialCPU(input, localCoord[0], localCoord[1], localCoord[2]);
					if (cellMaterial == 0u) {
						continue;
					}

					int32_t worldPos[3] = {};
					worldPos[0] = input.chunk.chunkOrigin[0] + static_cast<int32_t>(localCoord[0]);
					worldPos[1] = input.chunk.chunkOrigin[1] + static_cast<int32_t>(localCoord[1]);
					worldPos[2] = input.chunk.chunkOrigin[2] + static_cast<int32_t>(localCoord[2]);

					int32_t neighborPos[3] = {worldPos[0], worldPos[1], worldPos[2]};
					neighborPos[axisN] += signN;

					const uint8_t neighborMaterial = ReadVoxelMaterialCPU(input, neighborPos[0], neighborPos[1], neighborPos[2]);
					if (!ShouldEmitVoxelFaceCPU(cellMaterial, neighborMaterial)) {
						continue;
					}

					EmitFace(cellMaterial, faceIndex, localCoord, input.chunk.chunkIndex, 1u, 1u, result);
				}
			}
		}
	}
}

} 

CpuGreedyMeshResult GenerateCpuGreedyMesh(const CpuGreedyInput &input)
{
	CpuGreedyMeshResult result;

	GreedyFacePassCPU(0u, 0u, 1u, 2u, 1, input, result);  // X+
	GreedyFacePassCPU(1u, 0u, 1u, 2u, -1, input, result); // X-
	GreedyFacePassCPU(2u, 1u, 0u, 2u, 1, input, result);  // Y+
	GreedyFacePassCPU(3u, 1u, 0u, 2u, -1, input, result); // Y-
	GreedyFacePassCPU(4u, 2u, 0u, 1u, 1, input, result);  // Z+
	GreedyFacePassCPU(5u, 2u, 0u, 1u, -1, input, result); // Z-

	return result;
}

bool IsChunkVisibleCPU(const CpuGreedyChunkDesc &chunk, const CpuChunkCullingParams &culling)
{
	if (chunk.nonAirCount == 0u) {
		return false;
	}

	const float halfExt[3] = {
		static_cast<float>(chunk.extent[0]) * 0.5f,
		static_cast<float>(chunk.extent[1]) * 0.5f,
		static_cast<float>(chunk.extent[2]) * 0.5f};
	const float center[3] = {
		static_cast<float>(chunk.chunkOrigin[0]) + halfExt[0],
		static_cast<float>(chunk.chunkOrigin[1]) + halfExt[1],
		static_cast<float>(chunk.chunkOrigin[2]) + halfExt[2]};
	const float toCenter[3] = {
		center[0] - culling.cameraX,
		center[1] - culling.cameraY,
		center[2] - culling.cameraZ};

	const float forward[3] = {culling.cameraForwardX, culling.cameraForwardY, culling.cameraForwardZ};
	const float right[3] = {culling.cameraRightX, culling.cameraRightY, culling.cameraRightZ};
	const float up[3] = {culling.cameraUpX, culling.cameraUpY, culling.cameraUpZ};
	const float tanHFov = std::max(culling.tanHalfVerticalFov, 0.0f);
	const float tanHWid = std::max(culling.tanHalfHorizontalFov, 0.0f);

	const float dotAbsFwdHalf = std::abs(forward[0]) * halfExt[0] + std::abs(forward[1]) * halfExt[1] + std::abs(forward[2]) * halfExt[2];
	const float chunkRadius = std::sqrt(halfExt[0] * halfExt[0] + halfExt[1] * halfExt[1] + halfExt[2] * halfExt[2]);
	const float nearPlane = std::max(culling.nearPlane, 0.0f);
	const float fwdDist = toCenter[0] * forward[0] + toCenter[1] * forward[1] + toCenter[2] * forward[2];
	if (fwdDist + dotAbsFwdHalf < nearPlane) {
		return false;
	}

	if (culling.maxDistance > 0.0f) {
		const float maxCenterDist = culling.maxDistance + chunkRadius;
		const float toCenterLenSq = toCenter[0] * toCenter[0] + toCenter[1] * toCenter[1] + toCenter[2] * toCenter[2];
		if (toCenterLenSq > maxCenterDist * maxCenterDist) {
			return false;
		}
	}

	const float leftPlane[3] = {forward[0] * tanHWid + right[0], forward[1] * tanHWid + right[1], forward[2] * tanHWid + right[2]};
	const float rghtPlane[3] = {forward[0] * tanHWid - right[0], forward[1] * tanHWid - right[1], forward[2] * tanHWid - right[2]};
	const float botPlane[3] = {forward[0] * tanHFov + up[0], forward[1] * tanHFov + up[1], forward[2] * tanHFov + up[2]};
	const float topPlane[3] = {forward[0] * tanHFov - up[0], forward[1] * tanHFov - up[1], forward[2] * tanHFov - up[2]};

	const auto testPlane = [&](const float plane[3]) -> bool {
		const float dotTC = toCenter[0] * plane[0] + toCenter[1] * plane[1] + toCenter[2] * plane[2];
		const float dotAbs = std::abs(plane[0]) * halfExt[0] + std::abs(plane[1]) * halfExt[1] + std::abs(plane[2]) * halfExt[2];
		return dotTC + dotAbs >= 0.0f;
	};

	if (!testPlane(leftPlane)) {
		return false;
	}
	if (!testPlane(rghtPlane)) {
		return false;
	}
	if (!testPlane(botPlane)) {
		return false;
	}
	if (!testPlane(topPlane)) {
		return false;
	}

	return true;
}

} 

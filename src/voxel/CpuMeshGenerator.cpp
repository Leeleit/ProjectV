#include "voxel/CpuMeshGenerator.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md


namespace projectv::voxel {

namespace {

bool ShouldEmitFaceAtXPlus(const uint8_t *const voxels, const int x, const int y, const int z, const int widthX, const int heightY, const int depthZ)
{
	(void)depthZ;
	const int flatIndex = x + widthX * (y + heightY * z);
	const uint8_t current = voxels[flatIndex];
	if (current == 0) {
		return false;
	}
	if (x + 1 >= widthX) {
		return true;
	}
	const uint8_t neighbor = voxels[x + 1 + widthX * (y + heightY * z)];
	return current != neighbor;
}

} // namespace

std::vector<PackedSceneVoxelFace> GenerateCpuChunkMeshXPositive(const CpuMeshInput &input)
{
	std::vector<PackedSceneVoxelFace> out;
	if (input.voxels == nullptr || input.widthX <= 0 || input.heightY <= 0 || input.depthZ <= 0) {
		return out;
	}

	static constexpr int kMaxExtent = 64;
	if (input.widthX > kMaxExtent || input.heightY > kMaxExtent || input.depthZ > kMaxExtent) {
		return out;
	}

	for (int z = 0; z < input.depthZ; ++z) {
		for (int y = 0; y < input.heightY; ++y) {
			for (int x = 0; x < input.widthX; ++x) {
				if (!ShouldEmitFaceAtXPlus(input.voxels, x, y, z, input.widthX, input.heightY, input.depthZ)) {
					continue;
				}
				const int flatIndex = x + input.widthX * (y + input.heightY * z);
				const uint8_t material = input.voxels[flatIndex];
				PackedSceneVoxelFace face{};
				face.localVoxelFace = 0u;
				face.lightingData = 0u;
				face.chunkIndexMaterial = material;
				face.packedExtents = 0u;
				out.push_back(face);
			}
		}
	}

	return out;
}

} // namespace projectv::voxel

#include "voxel/VoxelLodDownsample.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace projectv::voxel {

uint32_t LodDownsampleStepForLod(const uint8_t lodLevel)
{
	if (lodLevel == 0u) {
		return 1u;
	}
	if (lodLevel == 1u) {
		return 2u;
	}
	if (lodLevel == 2u) {
		return 4u;
	}
	return 8u;
}

uint32_t LodDownsampledExtentForLod(const uint8_t lodLevel, const uint8_t chunkSize)
{
	const uint32_t step = LodDownsampleStepForLod(lodLevel);
	const uint32_t extent = static_cast<uint32_t>(chunkSize) / step;
	return extent == 0u ? 1u : extent;
}

uint8_t ReadSourceVoxel(const VoxelWorld &world, const Int3 &position)
{
	const int localX = position.x - world.min.x;
	const int localY = position.y - world.min.y;
	const int localZ = position.z - world.min.z;
	return world.sparseStorage.GetCell(localX, localY, localZ);
}

// EVIL: B_SurfacePreserve kernel per `2026-06-21-lod-mesh-downsampling` verdict=mixed.
// Reads step^3 source voxels (Air=0 or non-Air material) and emits:
//   - Air if all step^3 are Air
//   - first non-Air material found if any (lexicographic over the step^3 iteration order)
// Property: 0 T-junction holes across 75 boundary configurations (per the experiment).
// Cost: < 1.5 us/chunk at any LOD on Zen 3 5800X (well under 50 us Stage 4.1 budget).
// Other kernels (A_Majority3D, C_SolidOnly, D_MaxPool) fail 10-32% on cave_stress + collapse in LOD 1.
// See `agent/knowledge.md` 3-step migration precedent and the experiment README.
uint8_t SurfacePreserveVote8(
	const VoxelWorld &world,
	const Int3 &chunkOrigin,
	const uint32_t outX,
	const uint32_t outY,
	const uint32_t outZ,
	const uint32_t step)
{
	for (uint32_t sz = 0; sz < step; ++sz) {
		for (uint32_t sy = 0; sy < step; ++sy) {
			for (uint32_t sx = 0; sx < step; ++sx) {
				const Int3 sourcePos{
					chunkOrigin.x + static_cast<int>(outX * step + sx),
					chunkOrigin.y + static_cast<int>(outY * step + sy),
					chunkOrigin.z + static_cast<int>(outZ * step + sz),
				};
				const uint8_t material = ReadSourceVoxel(world, sourcePos);
				if (material != 0u) {
					return material;
				}
			}
		}
	}
	return 0u;
}

void DownsampleChunkForLodSurfacePreserve(
	const VoxelWorld &world,
	const size_t chunkIndex,
	const uint8_t lodLevel,
	std::vector<uint8_t> &outDownsampled)
{
	outDownsampled.clear();
	if (chunkIndex >= world.chunks.size()) {
		return;
	}
	if (lodLevel == 0u) {
		return;
	}
	const VoxelChunk &chunk = world.chunks[chunkIndex];
	const uint32_t step = LodDownsampleStepForLod(lodLevel);
	const uint32_t outExtent = LodDownsampledExtentForLod(lodLevel, static_cast<uint8_t>(world.chunkSize));
	const size_t outVoxelCount = static_cast<size_t>(outExtent) * outExtent * outExtent;
	outDownsampled.resize(outVoxelCount, 0u);

	for (uint32_t oz = 0; oz < outExtent; ++oz) {
		for (uint32_t oy = 0; oy < outExtent; ++oy) {
			for (uint32_t ox = 0; ox < outExtent; ++ox) {
				const uint8_t material = SurfacePreserveVote8(world, chunk.min, ox, oy, oz, step);
				outDownsampled[(oz * outExtent + oy) * outExtent + ox] = material;
			}
		}
	}
}

uint32_t RunLodDownsampleJobs(VoxelWorld &world)
{
	uint32_t processed = 0u;
	for (size_t chunkIndex = 0; chunkIndex < world.chunks.size(); ++chunkIndex) {
		VoxelChunk &chunk = world.chunks[chunkIndex];
		if (chunk.lodLevel == 0u) {
			chunk.lodDownsampledNonAirCount = 0u;
			continue;
		}
		std::vector<uint8_t> downsampled;
		voxel::DownsampleChunkForLodSurfacePreserve(world, chunkIndex, chunk.lodLevel, downsampled);
		uint32_t nonAirCount = 0u;
		for (const uint8_t material : downsampled) {
			if (material != 0u) {
				++nonAirCount;
			}
		}
		chunk.lodDownsampledNonAirCount = static_cast<uint8_t>(std::min<uint32_t>(nonAirCount, 255u));
		++processed;
	}
	return processed;
}

bool IsLodDownsampleEnabled()
{
	if (const char *value = std::getenv("PROJECTV_LOD_DOWNSAMPLE")) {
		return value[0] != '\0' && value[0] != '0';
	}
	return false;
}

} // namespace projectv::voxel

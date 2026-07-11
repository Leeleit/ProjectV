import projectv.math;
import projectv.string_id;

#include "voxel/VoxelWorldInternal.hpp"
#include "voxel/VoxelLodDownsample.hpp"

#include <cmath>
#include <vector>

uint8_t SelectLodLevelForDistance(const float distanceMeters)
{
	if (distanceMeters < 32.0f) {
		return 0;
	}
	if (distanceMeters < 64.0f) {
		return 1;
	}
	if (distanceMeters < 128.0f) {
		return 2;
	}
	return 3;
}

uint32_t LodDownsampleStepForLod(const uint8_t lodLevel)
{
	return projectv::voxel::LodDownsampleStepForLod(lodLevel);
}

uint32_t LodDownsampledExtentForLod(const uint8_t lodLevel, const uint8_t chunkSize)
{
	return projectv::voxel::LodDownsampledExtentForLod(lodLevel, chunkSize);
}

void DownsampleChunkForLodSurfacePreserve(
	const VoxelWorld &world,
	const size_t chunkIndex,
	const uint8_t lodLevel,
	std::vector<uint8_t> &outDownsampled)
{
	projectv::voxel::DownsampleChunkForLodSurfacePreserve(world, chunkIndex, lodLevel, outDownsampled);
}

uint32_t RunLodDownsampleJobs(VoxelWorld &world)
{
	return projectv::voxel::RunLodDownsampleJobs(world);
}

bool IsLodDownsampleEnabled()
{
	return projectv::voxel::IsLodDownsampleEnabled();
}

void AssignLodLevels(VoxelWorld &world, const float cameraX, const float cameraY, const float cameraZ)
{
	for (VoxelChunk &chunk : world.chunks) {
		const float cx = 0.5f * static_cast<float>(chunk.min.x + chunk.maxExclusive.x);
		const float cy = 0.5f * static_cast<float>(chunk.min.y + chunk.maxExclusive.y);
		const float cz = 0.5f * static_cast<float>(chunk.min.z + chunk.maxExclusive.z);
		const float dx = cx - cameraX;
		const float dy = cy - cameraY;
		const float dz = cz - cameraZ;
		const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
		chunk.lodLevel = SelectLodLevelForDistance(distance);
	}
}

uint32_t CountChunksAtLod(const VoxelWorld &world, const uint8_t lodLevel)
{
	uint32_t count = 0;
	for (const VoxelChunk &chunk : world.chunks) {
		if (chunk.lodLevel == lodLevel) {
			++count;
		}
	}
	return count;
}

#include "core/Math.hpp"
#include "core/EnvUtils.hpp"
#include "core/StringId.hpp"
#include "voxel/VoxelWorldInternal.hpp"
#include <cstdlib>

uint32_t GetVoxelChunkStaticPromotionThreshold()
{
	if (const char *value = projectv::core::GetEnvVar("PROJECTV_SVDAG_STATIC_PROMOTION_TICKS")) {
		const long parsed = std::strtol(value, nullptr, 10);
		if (parsed > 0) {
			return static_cast<uint32_t>(parsed);
		}
	}
	return 60u;
}

void TickVoxelChunkStaticPromotion(VoxelWorld &world, const uint32_t threshold)
{
	for (VoxelChunk &chunk : world.chunks) {
		if (chunk.isStatic) {
			continue;
		}
		if (chunk.ticksSinceLastEdit < UINT32_MAX) {
			++chunk.ticksSinceLastEdit;
		}
		if (chunk.ticksSinceLastEdit >= threshold) {
			chunk.isStatic = true;
		}
	}
	if (world.sparseStorage.IsDeduplicationEnabled()) {
		world.sparseStorage.DedupPass();
	}
}

uint32_t CountStaticVoxelChunks(const VoxelWorld &world)
{
	uint32_t count = 0;
	for (const VoxelChunk &chunk : world.chunks) {
		if (chunk.isStatic) {
			++count;
		}
	}
	return count;
}

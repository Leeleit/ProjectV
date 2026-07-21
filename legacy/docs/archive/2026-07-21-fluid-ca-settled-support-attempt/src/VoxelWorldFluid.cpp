#include "core/Math.hpp"
#include "core/EnvUtils.hpp"
#include "core/StringId.hpp"
#include "voxel/VoxelWorldFluidInternal.hpp"
#include "voxel/VoxelWorldInternal.hpp"
#include "debug/Profiling.hpp"
#include "core/RuntimeDiagnostics.hpp"

#include <cstdint>
#include <vector>

namespace {

bool gFluidCaGpuEnabledForTesting = false;

} // namespace

bool IsFluidCaGpuEnabled()
{
	if (gFluidCaGpuEnabledForTesting) {
		return true;
	}
	if (const char *value = projectv::core::GetEnvVar("PROJECTV_FLUID_CA_GPU")) {
		return value[0] != '\0' && value[0] != '0';
	}
	return false;
}

void ToggleFluidCaGpuEnabledForTesting(const bool enabled)
{
	gFluidCaGpuEnabledForTesting = enabled;
}

std::vector<uint32_t> BuildActiveChunkIdsForFluidCa(const VoxelWorld &world)
{
	std::vector<uint32_t> active;
	active.reserve(world.chunks.size());
	for (size_t chunkIndex = 0; chunkIndex < world.chunks.size(); ++chunkIndex) {
		const VoxelChunk &chunk = world.chunks[chunkIndex];
		if (chunk.nonAirVoxelCount == 0u) {
			continue;
		}
		if (chunk.isStatic && chunk.ticksSinceLastEdit < 30u) {
			continue;
		}
		active.push_back(static_cast<uint32_t>(chunkIndex));
	}
	return active;
}

// Binary Fluid CA driver: READ → FALL (contiguous runs shift down as a unit) →
// per-body hydrostatic engine (ProcessFluidBodyChains, VoxelWorldFluidBodies.cpp)
// → COMMIT. Idle fast-path: no Air↔Fluid edits since a no-move tick → free return.
// Spec: docs/superpowers/specs/2026-07-20-fluid-ca-hydrostatic-chains-design.md
uint32_t UpdateFluidCA(VoxelWorld &world)
{
	PV_PROFILE_ZONE_N("UpdateFluidCA");

	if (world.stats.fluidVoxelCount == 0u) {
		return 0u;
	}

	if (world.fluidEditSerial == world.lastFluidCaSerial) {
		return 0u; // settled water costs nothing until an edit wakes the CA
	}
	const uint64_t serialAtEntry = world.fluidEditSerial;

	const int width = world.width;
	const int height = world.height;
	const int depth = world.depth;

#if !defined(NDEBUG)
	PV_ASSERT(
		width > 0 && height > 0 && depth > 0,
		"VoxelWorld",
		"UpdateFluidCA",
		"world dimensions must be strictly positive");
#endif

	const auto index = [width, height](const int lx, const int ly, const int lz) -> size_t {
		return static_cast<size_t>(lx) + static_cast<size_t>(ly) * static_cast<size_t>(width) +
			   static_cast<size_t>(lz) * static_cast<size_t>(width) * static_cast<size_t>(height);
	};

	const size_t totalCells = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth);
	std::vector<uint8_t> next(totalCells, 0u);

	const int readMinX = world.min.x;
	const int readMinY = world.min.y;
	const int readMinZ = world.min.z;
	const int readMaxX = world.maxExclusive.x;
	const int readMaxY = world.maxExclusive.y;
	const int readMaxZ = world.maxExclusive.z;

	profiling::PlotValue(
		"Fluid CA Cells Read",
		static_cast<int64_t>(readMaxX - readMinX) * (readMaxY - readMinY) * (readMaxZ - readMinZ));

	{
		PV_PROFILE_ZONE_N("UpdateFluidCA.ReadPass");
		for (int z = readMinZ; z < readMaxZ; ++z) {
			for (int y = readMinY; y < readMaxY; ++y) {
				for (int x = readMinX; x < readMaxX; ++x) {
					next[index(x - readMinX, y - readMinY, z - readMinZ)] = ReadVoxelFromSparseStorage(world, {x, y, z});
				}
			}
		}
	}

	uint32_t moved = 0u;
	constexpr uint8_t kAir = static_cast<uint8_t>(VoxelMaterial::Air);
	constexpr uint8_t kFluid = static_cast<uint8_t>(VoxelMaterial::Fluid);

	// Phase 1: FALL — contiguous Fluid runs shift down by 1 as a unit (stream never shreds into drops)
	for (int z = 0; z < depth; ++z) {
		for (int x = 0; x < width; ++x) {
			int y = 0;
			while (y < height) {
				if (next[index(x, y, z)] != kFluid || y == 0 || next[index(x, y - 1, z)] != kAir) {
					++y;
					continue;
				}
				int runTop = y; // maximal contiguous Fluid run above the Air gap
				while (runTop + 1 < height && next[index(x, runTop + 1, z)] == kFluid) {
					++runTop;
				}
				for (int ry = y; ry <= runTop; ++ry) { // the whole run descends exactly 1 cell
					next[index(x, ry - 1, z)] = kFluid;
					next[index(x, ry, z)] = kAir;
				}
				moved += static_cast<uint32_t>(runTop - y + 1);
				y = runTop + 1;
			}
		}
	}

	// Phase 2: per-body hydrostatic target + gradient chains
	moved += ProcessFluidBodyChains(width, height, depth, next);

	if (moved == 0u) {
		world.lastFluidCaSerial = world.fluidEditSerial; // settled: next tick early-outs unless edits bump the serial
		return 0u;
	}

	profiling::PlotValue("Fluid CA Cells Moved", static_cast<int64_t>(moved));
	{
		PV_PROFILE_ZONE_N("UpdateFluidCA.Commit");
		for (int z = readMinZ; z < readMaxZ; ++z) {
			for (int y = readMinY; y < readMaxY; ++y) {
				for (int x = readMinX; x < readMaxX; ++x) {
					const VoxelMaterial currentMaterial = static_cast<VoxelMaterial>(next[index(x - readMinX, y - readMinY, z - readMinZ)]);
					const VoxelMaterial previousMaterial = GetVoxelMaterial(world, {x, y, z});
					if (previousMaterial == currentMaterial) {
						continue;
					}
					SetVoxelMaterial(world, {x, y, z}, currentMaterial);
				}
			}
		}
	}

#if !defined(NDEBUG)
	{
		uint32_t actualFluidCount = 0u;
		for (int z = world.min.z; z < world.maxExclusive.z; ++z) {
			for (int y = world.min.y; y < world.maxExclusive.y; ++y) {
				for (int x = world.min.x; x < world.maxExclusive.x; ++x) {
					if (GetVoxelMaterial(world, {x, y, z}) == VoxelMaterial::Fluid) {
						++actualFluidCount;
					}
				}
			}
		}
		PV_ASSERT(
			actualFluidCount == world.stats.fluidVoxelCount,
			"VoxelWorld",
			"UpdateFluidCA",
			"stats.fluidVoxelCount diverged from actual fluid voxel count");
	}
#endif

	world.lastFluidCaSerial = serialAtEntry; // own commits bumped the serial — re-run next tick until a no-move tick
	return moved;
}

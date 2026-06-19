#include "benchmark/benchmark.h"

#include "core/Math.hpp"
#include "core/Types.hpp"
#include "render/ShadowProjection.hpp"
#include "render/ShadowTypes.hpp"
#include "voxel/VoxelWorld.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace {

// **Test fixture builder.** Lifted from
// `tests/VoxelWorldTests.cpp::MakeTestWorld` (the
// internal helper). The benchmark owns its own copy
// to avoid coupling the `bench/` directory to the
// `tests/` directory's internal headers.
VoxelWorld BuildBenchmarkWorld() {
	VoxelWorld world{};
	world.min = Int3{-12, 0, -12};
	world.maxExclusive = Int3{12, 16, 12};
	world.width = world.maxExclusive.x - world.min.x;
	world.height = world.maxExclusive.y - world.min.y;
	world.depth = world.maxExclusive.z - world.min.z;
	world.chunkSize = 8;
	world.chunkCountX = (world.width + world.chunkSize - 1) / world.chunkSize;
	world.chunkCountY = (world.height + world.chunkSize - 1) / world.chunkSize;
	world.chunkCountZ = (world.depth + world.chunkSize - 1) / world.chunkSize;
	world.voxels.resize(
		static_cast<size_t>(world.width) *
		static_cast<size_t>(world.height) *
		static_cast<size_t>(world.depth),
		static_cast<uint8_t>(VoxelMaterial::Air));
	world.chunks.resize(
		static_cast<size_t>(world.chunkCountX) *
		static_cast<size_t>(world.chunkCountY) *
		static_cast<size_t>(world.chunkCountZ));
	// **Voxelise the floor.** Y=0 is the floor layer;
	// we mark it `FloorWhite` so
	// `BuildSunShadowProjection` finds non-trivial
	// scene bounds via `TryGetActiveSceneBounds`.
	// Without any solid voxels, the helper would
	// short-circuit to the `min/max` world bounds,
	// which is a degenerate path we don't want to
	// measure here.
	for (int x = 0; x < world.width; ++x) {
		for (int z = 0; z < world.depth; ++z) {
			const size_t index =
				static_cast<size_t>(z) *
				static_cast<size_t>(world.width) *
				static_cast<size_t>(world.height) +
				static_cast<size_t>(x) *
				static_cast<size_t>(world.height);
			world.voxels[index] = static_cast<uint8_t>(VoxelMaterial::FloorWhite);
		}
	}
	// **Populate chunk metadata.** The shadow helper
	// iterates `world.chunks` to compute its bounds;
	// a chunk with `nonAirVoxelCount == 0` is treated
	// as empty. We need to set the Y=0 floor chunks'
	// `min` / `maxExclusive` to the chunk's voxel
	// extent and `nonAirVoxelCount` to a non-zero
	// value so the bounds accumulator finds them.
	for (int chunkZ = 0; chunkZ < world.chunkCountZ; ++chunkZ) {
		for (int chunkY = 0; chunkY < world.chunkCountY; ++chunkY) {
			for (int chunkX = 0; chunkX < world.chunkCountX; ++chunkX) {
				const size_t chunkIndex =
					static_cast<size_t>(chunkZ) *
					static_cast<size_t>(world.chunkCountX) *
					static_cast<size_t>(world.chunkCountY) +
					static_cast<size_t>(chunkY) *
					static_cast<size_t>(world.chunkCountX) +
					static_cast<size_t>(chunkX);
				VoxelChunk &chunk = world.chunks[chunkIndex];
				// `chunk.min` is the chunk's world-space
				// minimum corner. The world origin is
				// at `world.min`, so chunk 0 has
				// min = world.min.
				chunk.min = Int3{
					world.min.x + chunkX * world.chunkSize,
					world.min.y + chunkY * world.chunkSize,
					world.min.z + chunkZ * world.chunkSize};
				chunk.maxExclusive = Int3{
					chunk.min.x + world.chunkSize,
					chunk.min.y + world.chunkSize,
					chunk.min.z + world.chunkSize};
				chunk.nonAirVoxelCount = 0u;
				if (chunkY == 0) {
					// The Y=0 floor layer has
					// non-air voxels (the
					// voxelise-floor loop above).
					chunk.nonAirVoxelCount = static_cast<uint32_t>(world.chunkSize) * static_cast<uint32_t>(world.chunkSize);
				}
			}
		}
	}
	return world;
}

} // namespace

static void BM_BuildSunShadowProjection(benchmark::State &state) {
	const VoxelWorld world = BuildBenchmarkWorld();
	const std::array<float, 3> sunDirection{0.35f, 0.88f, 0.22f};
	const float coverageScale = 1.10f;
	for (auto _ : state) {
		const SunShadowProjection result =
			BuildSunShadowProjection(world, sunDirection, coverageScale);
		benchmark::DoNotOptimize(result);
	}
	state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
	state.SetLabel("BuildSunShadowProjection (per-frame shadow VP)");
}

// **Cascade splits benchmark.** `BuildSunShadowCascadeSplits`
// is the other shadow-projection hot path — called once
// per `FramePreparation` to compute the
// `SunShadowCascadeSplits` for the cascaded shadow map.
// The cost is dominated by `std::pow` (logarithmic
// depth distribution); the benchmark tracks that cost
// across refactors.
static void BM_BuildSunShadowCascadeSplits(benchmark::State &state) {
	for (auto _ : state) {
		const SunShadowCascadeSplits splits =
			BuildSunShadowCascadeSplits(0.1f, 128.0f);
		benchmark::DoNotOptimize(splits);
	}
	state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
	state.SetLabel("BuildSunShadowCascadeSplits (per-frame cascade splits)");
}

int main(int argc, char *argv[]) {
	benchmark::RegisterBenchmark("BM_BuildSunShadowProjection", &BM_BuildSunShadowProjection);
	benchmark::RegisterBenchmark("BM_BuildSunShadowCascadeSplits", &BM_BuildSunShadowCascadeSplits);
	benchmark::Initialize(&argc, argv);
	if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
		return 1;
	}
	benchmark::RunSpecifiedBenchmarks();
	benchmark::Shutdown();
	return 0;
}

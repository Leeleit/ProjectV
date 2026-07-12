#include "voxel/CpuGreedyMeshing.hpp"

#include "benchmark/benchmark.h"

#include <vector>

namespace {

using projectv::voxel::CpuGreedyInput;
using projectv::voxel::GenerateCpuGreedyMesh;

struct Xorshift32 {
	uint32_t state = 42u;
	Xorshift32() = default;
	uint32_t Next()
	{
		uint32_t x = state;
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		return state = x;
	}
	uint8_t NextMaterial()
	{
		static constexpr uint8_t materials[10] = {0u, 0u, 0u, 0u, 3u, 3u, 4u, 4u, 1u, 2u};
		return materials[Next() % 10u];
	}
};

std::vector<uint8_t> MakeRandomChunk(const int size)
{
	Xorshift32 rng;
	std::vector<uint8_t> voxels(static_cast<size_t>(size) * static_cast<size_t>(size) * static_cast<size_t>(size));
	if (voxels.empty()) {
		return voxels;
	}
	for (auto &v : voxels) {
		v = rng.NextMaterial();
	}
	return voxels;
}

CpuGreedyInput MakeInput(const std::vector<uint8_t> &voxels, const int size)
{
	CpuGreedyInput input{};
	input.worldVoxels = voxels.data();
	input.worldDim[0] = size;
	input.worldDim[1] = size;
	input.worldDim[2] = size;
	input.chunk.chunkOrigin[0] = 0;
	input.chunk.chunkOrigin[1] = 0;
	input.chunk.chunkOrigin[2] = 0;
	input.chunk.extent[0] = static_cast<uint32_t>(size);
	input.chunk.extent[1] = static_cast<uint32_t>(size);
	input.chunk.extent[2] = static_cast<uint32_t>(size);
	input.chunk.nonAirCount = 0u;
	for (const uint8_t v : voxels) {
		if (v != 0u) {
			++input.chunk.nonAirCount;
		}
	}
	return input;
}

void BM_GreedyMeshRandom8(benchmark::State &state)
{
	const auto voxels = MakeRandomChunk(8);
	const auto input = MakeInput(voxels, 8);
	while (state.KeepRunning()) {
		auto mesh = GenerateCpuGreedyMesh(input);
		benchmark::DoNotOptimize(mesh);
	}
}
BENCHMARK(BM_GreedyMeshRandom8);

void BM_GreedyMeshRandom16(benchmark::State &state)
{
	const auto voxels = MakeRandomChunk(16);
	const auto input = MakeInput(voxels, 16);
	while (state.KeepRunning()) {
		auto mesh = GenerateCpuGreedyMesh(input);
		benchmark::DoNotOptimize(mesh);
	}
}
BENCHMARK(BM_GreedyMeshRandom16);

void BM_GreedyMeshRandom32(benchmark::State &state)
{
	const auto voxels = MakeRandomChunk(32);
	const auto input = MakeInput(voxels, 32);
	while (state.KeepRunning()) {
		auto mesh = GenerateCpuGreedyMesh(input);
		benchmark::DoNotOptimize(mesh);
	}
}
BENCHMARK(BM_GreedyMeshRandom32);

void BM_GreedyMeshSolid8(benchmark::State &state)
{
	const std::vector<uint8_t> voxels(static_cast<size_t>(8) * 8 * 8, 3u);
	const auto input = MakeInput(voxels, 8);
	while (state.KeepRunning()) {
		auto mesh = GenerateCpuGreedyMesh(input);
		benchmark::DoNotOptimize(mesh);
	}
}
BENCHMARK(BM_GreedyMeshSolid8);

void BM_GreedyMeshSolid32(benchmark::State &state)
{
	const std::vector<uint8_t> voxels(static_cast<size_t>(32) * 32 * 32, 3u);
	const auto input = MakeInput(voxels, 32);
	while (state.KeepRunning()) {
		auto mesh = GenerateCpuGreedyMesh(input);
		benchmark::DoNotOptimize(mesh);
	}
}
BENCHMARK(BM_GreedyMeshSolid32);

} // namespace

BENCHMARK_MAIN();

#include "voxel/CpuGreedyMeshing.hpp"

#include "benchmark/benchmark.h"

#include <cstdint>
#include <vector>

namespace {

using projectv::voxel::CpuGreedyInput;
using projectv::voxel::GenerateCpuGreedyMesh;

struct Xorshift32 {
	uint32_t state;
	explicit Xorshift32(uint32_t seed) : state(seed ? seed : 1u) {}
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
		const uint32_t r = Next() % 10u;
		if (r < 4u) {
			return 0u;
		}
		if (r < 6u) {
			return 3u;
		}
		if (r < 8u) {
			return 4u;
		}
		if (r == 8u) {
			return 1u;
		}
		return 2u;
	}
};

std::vector<uint8_t> MakeRandomChunk(int size, uint32_t seed)
{
	Xorshift32 rng(seed);
	std::vector<uint8_t> voxels(static_cast<size_t>(size) * static_cast<size_t>(size) * static_cast<size_t>(size));
	for (auto &v : voxels) {
		v = rng.NextMaterial();
	}
	return voxels;
}

CpuGreedyInput MakeInput(const std::vector<uint8_t> &voxels, int size)
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
	for (uint8_t v : voxels) {
		if (v != 0u) {
			++input.chunk.nonAirCount;
		}
	}
	return input;
}

void BM_GreedyMeshRandom8(benchmark::State &state)
{
	auto voxels = MakeRandomChunk(8, 42u);
	const auto input = MakeInput(voxels, 8);
	for (auto _ : state) {
		auto mesh = GenerateCpuGreedyMesh(input);
		benchmark::DoNotOptimize(mesh);
	}
}
BENCHMARK(BM_GreedyMeshRandom8);

void BM_GreedyMeshRandom16(benchmark::State &state)
{
	auto voxels = MakeRandomChunk(16, 42u);
	const auto input = MakeInput(voxels, 16);
	for (auto _ : state) {
		auto mesh = GenerateCpuGreedyMesh(input);
		benchmark::DoNotOptimize(mesh);
	}
}
BENCHMARK(BM_GreedyMeshRandom16);

void BM_GreedyMeshRandom32(benchmark::State &state)
{
	auto voxels = MakeRandomChunk(32, 42u);
	const auto input = MakeInput(voxels, 32);
	for (auto _ : state) {
		auto mesh = GenerateCpuGreedyMesh(input);
		benchmark::DoNotOptimize(mesh);
	}
}
BENCHMARK(BM_GreedyMeshRandom32);

void BM_GreedyMeshSolid8(benchmark::State &state)
{
	std::vector<uint8_t> voxels(static_cast<size_t>(8) * 8 * 8, 3u);
	const auto input = MakeInput(voxels, 8);
	for (auto _ : state) {
		auto mesh = GenerateCpuGreedyMesh(input);
		benchmark::DoNotOptimize(mesh);
	}
}
BENCHMARK(BM_GreedyMeshSolid8);

void BM_GreedyMeshSolid32(benchmark::State &state)
{
	std::vector<uint8_t> voxels(static_cast<size_t>(32) * 32 * 32, 3u);
	const auto input = MakeInput(voxels, 32);
	for (auto _ : state) {
		auto mesh = GenerateCpuGreedyMesh(input);
		benchmark::DoNotOptimize(mesh);
	}
}
BENCHMARK(BM_GreedyMeshSolid32);

} // namespace

BENCHMARK_MAIN();

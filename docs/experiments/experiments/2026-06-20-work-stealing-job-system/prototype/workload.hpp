// workload.hpp - Synthetic ProjectV chunk generation workload.
//
// Models per-chunk work that would be done CPU-side by Stage 4.1 (background world gen)
// and Stage 3.1 (per-chunk Fluid CA bookkeeping):
//
//   for each chunk (0..numChunks):
//     for each voxel in chunk (8x8x8 = 512 voxels):
//       compute material ID via splitmix32 hash (immutable, per-chunk seed)
//       OR sub-block fill-mask via 64-bit popcount (models SVDAG 4x4x4 sub-block)
//
// Per-chunk output = uint64_t hash (8 bytes) — 1 cache line per chunk. Mimics the
// per-chunk metadata that would be written back to the SVDAG node pool.
//
// Deterministic: same seed → same outputs. Hash-only (no syscalls, no allocations
// in the hot loop), so the only limiting factor is CPU compute + memory bandwidth
// for the output array.

#pragma once

#include <array>
#include <cstdint>

namespace workload {

// splitmix32 — 32-bit hash with avalanche, used as material ID seed per voxel.
// Per Chris Wellons (nullprogram.com), public domain.
[[gnu::always_inline]] static inline uint32_t splitmix32(uint32_t &state) noexcept
{
	state += 0x9E3779B9u;
	uint32_t z = state;
	z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
	z = (z ^ (z >> 13)) * 0xC2B2AE35u;
	return z ^ (z >> 16);
}

struct ChunkGen {
	std::array<uint64_t, 512> voxels; // 8x8x8 = 512 voxels/chunk, 4 KiB/chunk
	uint64_t rootHash;				  // 64-bit SVDAG-like structural hash

	void generate(uint32_t seed) noexcept
	{
		// 8x8x8 = 512 voxels/chunk. Use splitmix32 chained 3 times per voxel
		// to model ~3 mixed-precision ops (splitmix + extract + branch) per
		// voxel, like a real perlin/simplex noise compute would require.
		uint32_t state = seed;
		for (int i = 0; i < 512; ++i) {
			uint32_t a = splitmix32(state);
			uint32_t b = splitmix32(state);
			uint32_t c = splitmix32(state);
			// Mix into 64-bit material ID + position tag.
			voxels[i] = (static_cast<uint64_t>(a) << 32) ^ b ^ (static_cast<uint64_t>(c) * 0x9E3779B97F4A7C15ull);
		}
		// Structural hash: fold over 8x8x8 sub-blocks (4x4x4 = 64 cells/block, 8 blocks).
		// Models SVDAG 64-ary tree top-level fill mask computation.
		rootHash = 0;
		for (int block = 0; block < 8; ++block) {
			uint64_t blockMask = 0;
			for (int sub = 0; sub < 64; ++sub) {
				int idx = block * 64 + sub;
				if ((voxels[idx] & 0xFFu) != 0u) {
					blockMask |= (1ull << sub);
				}
			}
			rootHash ^= blockMask * 0x9E3779B97F4A7C15ull;
		}
	}
};

// Single chunk gen — for serial baseline.
static inline void GenerateSingle(ChunkGen *chunks, int n, uint32_t baseSeed) noexcept
{
	for (int i = 0; i < n; ++i) {
		chunks[i].generate(baseSeed + static_cast<uint32_t>(i));
	}
}

} // namespace workload

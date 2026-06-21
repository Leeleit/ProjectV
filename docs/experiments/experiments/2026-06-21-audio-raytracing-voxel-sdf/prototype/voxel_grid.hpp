// voxel_grid.hpp — synthetic Sparse64Tree-aligned voxel grid
//
// Layout: chunkSize=8, depth=2 per `2026-06-20-nanovdb-on-gpu` §6
//   - Depth 2 (root): 8³ voxels = 512 voxels per chunk
//   - Depth 1 (mid):  4³ voxels = 64 voxels per sub-block, 8 sub-blocks per chunk
//   - Depth 0 (leaf): 2³ voxels = 8 voxels per leaf, 8 leaves per sub-block
//
// Standalone — does NOT include ProjectV mainline `src/voxel/Sparse64Tree.hpp`.
// Storage: bitmask (512 bits per chunk) + material byte per voxel.
// DDA traversal (Amanatides & Woo 1987) — simple, no SIMD, single-threaded.

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace audio_rt {

struct Float3 {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

enum class Material : uint8_t {
	Air = 0,
	Stone = 1, // 0.70 reflection, diffuse
	Wood = 2,  // 0.40 reflection, absorptive
	Glass = 3, // 0.90 reflection, specular
	Water = 4, // 0.50 reflection
	Sand = 5,  // 0.20 reflection, very absorptive
};

constexpr int kChunkSize = 8;
constexpr int kVoxelsPerChunk = kChunkSize * kChunkSize * kChunkSize; // 512

struct alignas(16) VoxelChunk {
	std::array<uint64_t, 64> occupancy{}; // 512 bits = 1 bit per voxel
	std::array<uint8_t, kVoxelsPerChunk> materials{};
};

class VoxelGrid {
  public:
	VoxelGrid(int chunks_x, int chunks_y, int chunks_z);

	void setVoxel(int x, int y, int z, Material m) noexcept;
	[[nodiscard]] std::optional<Material> getVoxel(int x, int y, int z) const noexcept;
	[[nodiscard]] bool isOccupied(int x, int y, int z) const noexcept;

	struct Hit {
		float t = 0.0f;
		int x = 0;
		int y = 0;
		int z = 0;
		Material mat = Material::Air;
		Float3 normal{};
	};

	// DDA ray traversal. Returns first solid voxel hit within max_t.
	// If max_t < 0 — no distance limit.
	[[nodiscard]] std::optional<Hit> traceRay(float ox, float oy, float oz, float dx, float dy, float dz,
											  float max_t = -1.0f) const noexcept;

	[[nodiscard]] int chunksX() const noexcept { return cx_; }
	[[nodiscard]] int chunksY() const noexcept { return cy_; }
	[[nodiscard]] int chunksZ() const noexcept { return cz_; }
	[[nodiscard]] int worldSizeX() const noexcept { return cx_ * kChunkSize; }
	[[nodiscard]] int worldSizeY() const noexcept { return cy_ * kChunkSize; }
	[[nodiscard]] int worldSizeZ() const noexcept { return cz_ * kChunkSize; }

	// Scene generators for benchmark
	static VoxelGrid makeCave(int chunks_x = 16, int chunks_y = 16, int chunks_z = 16, uint64_t seed = 1);
	static VoxelGrid makeOpenPlains(int chunks_x = 16, int chunks_y = 8, int chunks_z = 16, uint64_t seed = 1);
	static VoxelGrid makeMultiRoom(int chunks_x = 16, int chunks_y = 16, int chunks_z = 16, uint64_t seed = 1);

  private:
	int cx_;
	int cy_;
	int cz_;
	std::vector<VoxelChunk> chunks_;

	[[nodiscard]] static int voxelIdx(int lx, int ly, int lz) noexcept {
		return lx + ly * kChunkSize + lz * kChunkSize * kChunkSize;
	}
	[[nodiscard]] static int bitIdx(int lx, int ly, int lz) noexcept {
		return lx + ly * kChunkSize + lz * kChunkSize * kChunkSize;
	}
	[[nodiscard]] const VoxelChunk &chunkAt(int cx, int cy, int cz) const noexcept;
	[[nodiscard]] VoxelChunk &chunkAt(int cx, int cy, int cz) noexcept;
	[[nodiscard]] bool worldInBounds(int x, int y, int z) const noexcept;
};

} // namespace audio_rt

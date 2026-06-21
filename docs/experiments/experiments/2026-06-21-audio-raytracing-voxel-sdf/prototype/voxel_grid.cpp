// voxel_grid.cpp — VoxelGrid implementation

#include "voxel_grid.hpp"
#include <algorithm>
#include <cmath>

namespace audio_rt {

VoxelGrid::VoxelGrid(int chunks_x, int chunks_y, int chunks_z)
	: cx_(chunks_x), cy_(chunks_y), cz_(chunks_z), chunks_(static_cast<size_t>(cx_) * cy_ * cz_) {}

VoxelChunk &VoxelGrid::chunkAt(int cx, int cy, int cz) noexcept {
	return chunks_[static_cast<size_t>(cx) + static_cast<size_t>(cy) * cx_ + static_cast<size_t>(cz) * cx_ * cy_];
}

const VoxelChunk &VoxelGrid::chunkAt(int cx, int cy, int cz) const noexcept {
	return chunks_[static_cast<size_t>(cx) + static_cast<size_t>(cy) * cx_ + static_cast<size_t>(cz) * cx_ * cy_];
}

bool VoxelGrid::worldInBounds(int x, int y, int z) const noexcept {
	return x >= 0 && y >= 0 && z >= 0 && x < cx_ * kChunkSize && y < cy_ * kChunkSize && z < cz_ * kChunkSize;
}

void VoxelGrid::setVoxel(int x, int y, int z, Material m) noexcept {
	if (!worldInBounds(x, y, z))
		return;
	int cx = x >> 3;
	int cy = y >> 3;
	int cz = z >> 3;
	int lx = x & 7;
	int ly = y & 7;
	int lz = z & 7;
	auto &chunk = chunkAt(cx, cy, cz);
	int vi = voxelIdx(lx, ly, lz);
	int bi = bitIdx(lx, ly, lz);
	if (m == Material::Air) {
		chunk.occupancy[bi >> 6] &= ~(1ULL << (bi & 63));
		chunk.materials[vi] = 0;
	} else {
		chunk.occupancy[bi >> 6] |= (1ULL << (bi & 63));
		chunk.materials[vi] = static_cast<uint8_t>(m);
	}
}

std::optional<Material> VoxelGrid::getVoxel(int x, int y, int z) const noexcept {
	if (!worldInBounds(x, y, z))
		return std::nullopt;
	int cx = x >> 3;
	int cy = y >> 3;
	int cz = z >> 3;
	int lx = x & 7;
	int ly = y & 7;
	int lz = z & 7;
	const auto &chunk = chunkAt(cx, cy, cz);
	int bi = bitIdx(lx, ly, lz);
	if ((chunk.occupancy[bi >> 6] >> (bi & 63)) & 1ULL)
		return static_cast<Material>(chunk.materials[voxelIdx(lx, ly, lz)]);
	return Material::Air;
}

bool VoxelGrid::isOccupied(int x, int y, int z) const noexcept {
	if (!worldInBounds(x, y, z))
		return false;
	int cx = x >> 3;
	int cy = y >> 3;
	int cz = z >> 3;
	int lx = x & 7;
	int ly = y & 7;
	int lz = z & 7;
	const auto &chunk = chunkAt(cx, cy, cz);
	int bi = bitIdx(lx, ly, lz);
	return ((chunk.occupancy[bi >> 6] >> (bi & 63)) & 1ULL) != 0;
}

// DDA traversal (Amanatides & Woo 1987).
std::optional<VoxelGrid::Hit> VoxelGrid::traceRay(float ox, float oy, float oz, float dx, float dy, float dz,
												  float max_t) const noexcept {
	int ix = static_cast<int>(std::floor(ox));
	int iy = static_cast<int>(std::floor(oy));
	int iz = static_cast<int>(std::floor(oz));

	int step_x = (dx > 0.0f) ? 1 : ((dx < 0.0f) ? -1 : 0);
	int step_y = (dy > 0.0f) ? 1 : ((dy < 0.0f) ? -1 : 0);
	int step_z = (dz > 0.0f) ? 1 : ((dz < 0.0f) ? -1 : 0);

	float t_max_x = (step_x != 0) ? (static_cast<float>(step_x > 0 ? ix + 1 : ix) - ox) / dx : 1e30f;
	float t_max_y = (step_y != 0) ? (static_cast<float>(step_y > 0 ? iy + 1 : iy) - oy) / dy : 1e30f;
	float t_max_z = (step_z != 0) ? (static_cast<float>(step_z > 0 ? iz + 1 : iz) - oz) / dz : 1e30f;

	float t_delta_x = (step_x != 0) ? std::abs(1.0f / dx) : 1e30f;
	float t_delta_y = (step_y != 0) ? std::abs(1.0f / dy) : 1e30f;
	float t_delta_z = (step_z != 0) ? std::abs(1.0f / dz) : 1e30f;

	float t = 0.0f;
	Float3 normal{};

	// Safety bound — max voxels traversed per ray (avoid infinite loop on edge cases).
	constexpr int kMaxSteps = 8192;
	for (int step = 0; step < kMaxSteps; ++step) {
		if (!worldInBounds(ix, iy, iz))
			return std::nullopt;
		auto mat = getVoxel(ix, iy, iz);
		if (mat && *mat != Material::Air) {
			Hit h;
			h.t = t;
			h.x = ix;
			h.y = iy;
			h.z = iz;
			h.mat = *mat;
			h.normal = normal;
			return h;
		}
		if (max_t >= 0.0f && t > max_t)
			return std::nullopt;
		if (t_max_x < t_max_y) {
			if (t_max_x < t_max_z) {
				ix += step_x;
				t = t_max_x;
				t_max_x += t_delta_x;
				normal = Float3{-static_cast<float>(step_x), 0.0f, 0.0f};
			} else {
				iz += step_z;
				t = t_max_z;
				t_max_z += t_delta_z;
				normal = Float3{0.0f, 0.0f, -static_cast<float>(step_z)};
			}
		} else {
			if (t_max_y < t_max_z) {
				iy += step_y;
				t = t_max_y;
				t_max_y += t_delta_y;
				normal = Float3{0.0f, -static_cast<float>(step_y), 0.0f};
			} else {
				iz += step_z;
				t = t_max_z;
				t_max_z += t_delta_z;
				normal = Float3{0.0f, 0.0f, -static_cast<float>(step_z)};
			}
		}
	}
	return std::nullopt;
}

// ---------------------------------------------------------------------------
// Scene generators — synthetic ProjectV-style voxel worlds.
// ---------------------------------------------------------------------------

namespace {
// Cheap deterministic RNG for scene gen.
struct SplitMix {
	uint64_t s;
	explicit SplitMix(uint64_t seed) : s(seed) {}
	uint64_t next() noexcept {
		s += 0x9E3779B97F4A7C15ULL;
		uint64_t z = s;
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
		return z ^ (z >> 31);
	}
	float unit() noexcept { return static_cast<float>(next() >> 11) * (1.0f / (1ULL << 53)); }
};
} // namespace

VoxelGrid VoxelGrid::makeCave(int chunks_x, int chunks_y, int chunks_z, uint64_t seed) {
	VoxelGrid g(chunks_x, chunks_y, chunks_z);
	SplitMix rng(seed);
	int wx = chunks_x * kChunkSize;
	int wy = chunks_y * kChunkSize;
	int wz = chunks_z * kChunkSize;
	// Floor (stone)
	for (int x = 0; x < wx; ++x)
		for (int z = 0; z < wz; ++z)
			g.setVoxel(x, 0, z, Material::Stone);
	// Ceiling (stone)
	for (int x = 0; x < wx; ++x)
		for (int z = 0; z < wz; ++z)
			g.setVoxel(x, wy - 1, z, Material::Stone);
	// Random cave walls — pillars + chunks
	for (int i = 0; i < 200; ++i) {
		int cx = static_cast<int>(rng.unit() * wx);
		int cz = static_cast<int>(rng.unit() * wz);
		int cy = static_cast<int>(rng.unit() * (wy - 4)) + 2;
		int r = 1 + static_cast<int>(rng.unit() * 3);
		for (int dx_ = -r; dx_ <= r; ++dx_)
			for (int dy_ = -r; dy_ <= r; ++dy_)
				for (int dz_ = -r; dz_ <= r; ++dz_)
					if (dx_ * dx_ + dy_ * dy_ + dz_ * dz_ <= r * r)
						g.setVoxel(cx + dx_, cy + dy_, cz + dz_, Material::Stone);
	}
	return g;
}

VoxelGrid VoxelGrid::makeOpenPlains(int chunks_x, int chunks_y, int chunks_z, uint64_t seed) {
	VoxelGrid g(chunks_x, chunks_y, chunks_z);
	SplitMix rng(seed);
	int wx = chunks_x * kChunkSize;
	int wz = chunks_z * kChunkSize;
	(void)chunks_y; // wy used implicitly via getVoxel bounds checks
	// Ground (sand) + sparse low obstacles (wood)
	for (int x = 0; x < wx; ++x)
		for (int z = 0; z < wz; ++z) {
			g.setVoxel(x, 0, z, Material::Sand);
			int h = 1 + static_cast<int>(rng.unit() * 2);
			for (int y = 1; y < h; ++y)
				g.setVoxel(x, y, z, Material::Sand);
		}
	// Sparse pillars
	for (int i = 0; i < 30; ++i) {
		int px = static_cast<int>(rng.unit() * wx);
		int pz = static_cast<int>(rng.unit() * wz);
		int h = 3 + static_cast<int>(rng.unit() * 4);
		for (int y = 1; y <= h; ++y)
			g.setVoxel(px, y, pz, Material::Wood);
	}
	return g;
}

VoxelGrid VoxelGrid::makeMultiRoom(int chunks_x, int chunks_y, int chunks_z, uint64_t seed) {
	VoxelGrid g(chunks_x, chunks_y, chunks_z);
	SplitMix rng(seed);
	int wx = chunks_x * kChunkSize;
	int wy = chunks_y * kChunkSize;
	int wz = chunks_z * kChunkSize;
	// Solid outer shell (stone) — walls, floor, ceiling
	for (int x = 0; x < wx; ++x)
		for (int y = 0; y < wy; ++y)
			for (int z = 0; z < wz; ++z) {
				if (x == 0 || x == wx - 1 || y == 0 || y == wy - 1 || z == 0 || z == wz - 1)
					g.setVoxel(x, y, z, Material::Stone);
			}
	// Internal walls splitting into rooms
	for (int i = 1; i < 4; ++i) {
		int wx_split = wx / 4 * i;
		for (int y = 1; y < wy - 1; ++y)
			for (int z = 1; z < wz - 1; ++z)
				if (rng.unit() > 0.15f) // doorways (15% gaps)
					g.setVoxel(wx_split, y, z, Material::Glass);
	}
	return g;
}

} // namespace audio_rt

// Scene generator + greedy meshing.
// Produces PackedFace output directly (matches ProjectV's GPU-side layout).

#include "scene.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <vector>

namespace vb {

namespace {

// PCG-XSH-RR deterministic RNG (no <random> state dependency).
struct PcgRng {
	uint64_t state;
	uint64_t inc;

	explicit PcgRng(uint64_t seed)
	{
		state = 0u;
		inc = (seed << 1u) | 1u;
		next();
		state += seed;
		next();
	}

	uint32_t next()
	{
		uint64_t oldstate = state;
		state = oldstate * 6364136223846793005ULL + inc;
		uint32_t xorshifted = uint32_t(((oldstate >> 18u) ^ oldstate) >> 27u);
		uint32_t rot = uint32_t(oldstate >> 59u);
		return (xorshifted >> rot) | (xorshifted << ((-int32_t(rot)) & 31u));
	}

	uint32_t range(uint32_t lo, uint32_t hi)
	{
		return lo + next() % (hi - lo + 1);
	}
};

// Encode face position: x,y,z each 8 bits, faceIdx 8 bits. Match ProjectV's bit layout.
constexpr uint32_t PackLocalVoxelFace(uint32_t x, uint32_t y, uint32_t z, uint32_t face)
{
	return (face << 24u) | (z << 16u) | (y << 8u) | x;
}

constexpr uint32_t PackChunkIndexMaterial(uint32_t chunkIdx, uint32_t matIdx)
{
	return (matIdx << 24u) | (chunkIdx & 0x00FFFFFFu);
}

constexpr uint32_t PackExtents(uint32_t u, uint32_t v)
{
	return (v << 8u) | u;
}

constexpr bool IsFaceVisible(uint32_t self, uint32_t neighbor)
{
	// Air or different material → visible face (greedy can merge same-material).
	return self != neighbor;
}

// Greedy-merge one slab of one face direction into a list of (x0,y0,z0, extentU, extentV, face, mat) quads.
// Operates on chunk-local voxel grid (1 byte per voxel).
struct Quad {
	uint32_t x0, y0, z0;
	uint32_t extentU, extentV; // in voxel units.
	uint32_t face;			   // 0..5.
	uint32_t mat;			   // material id (1..N).
};

// Face axes per ProjectV voxel.vert conventions:
//   face 0: +X, normal (1,0,0). Plane (Y, Z). extentU=Y, extentV=Z.
//   face 1: -X, normal (-1,0,0). Plane (Y, Z). extentU=Y, extentV=Z.
//   face 2: +Y, normal (0,1,0). Plane (X, Z). extentU=X, extentV=Z.
//   face 3: -Y, normal (0,-1,0). Plane (X, Z). extentU=X, extentV=Z.
//   face 4: +Z, normal (0,0,1). Plane (X, Y). extentU=X, extentV=Y.
//   face 5: -Z, normal (0,0,-1). Plane (X, Y). extentU=X, extentV=Y.
struct FacePlane {
	uint32_t normalAxis;	   // 0=X, 1=Y, 2=Z.
	int32_t normalSign;		   // +1 or -1.
	uint32_t axisU, axisV;	   // two remaining axes.
	uint32_t dimU, dimV, dimN; // dimensions along U, V, normal.
	uint32_t uMin, vMin, nMin;
};

FacePlane MakePlane(uint32_t face, uint32_t dim)
{
	FacePlane p{};
	p.dimN = dim;
	p.dimU = dim;
	p.dimV = dim;
	p.normalAxis = face / 2;
	p.normalSign = (face % 2 == 0) ? 1 : -1;
	uint32_t a1 = (p.normalAxis + 1) % 3;
	uint32_t a2 = (p.normalAxis + 2) % 3;
	p.axisU = a1;
	p.axisV = a2;
	return p;
}

// Get voxel at (u, v, n) in face plane.
uint8_t VoxelAt(const std::vector<uint8_t> &voxels, uint32_t dim,
				const FacePlane &p, uint32_t u, uint32_t v, uint32_t n)
{
	uint32_t xyz[3]{};
	xyz[p.axisU] = u;
	xyz[p.axisV] = v;
	xyz[p.normalAxis] = n;
	uint32_t idx = (xyz[2] * dim + xyz[1]) * dim + xyz[0];
	return voxels[idx];
}

// Neighbor (along normal axis, +/- 1 step). Returns 0 (air) for OOB.
uint8_t NeighborAt(const std::vector<uint8_t> &voxels, uint32_t dim,
				   const FacePlane &p, uint32_t u, uint32_t v, uint32_t n)
{
	int32_t nn = int32_t(n) + p.normalSign;
	if (nn < 0 || nn >= int32_t(dim))
		return 0;
	return VoxelAt(voxels, dim, p, u, v, uint32_t(nn));
}

void GreedyMerge(const std::vector<uint8_t> &voxels, uint32_t dim, uint32_t face,
				 std::vector<Quad> &out)
{
	FacePlane p = MakePlane(face, dim);
	// visited[dimU][dimV] - track which voxels already consumed.
	std::vector<uint8_t> visited(p.dimU * p.dimV, 0);

	auto idxUV = [&](uint32_t u, uint32_t v) { return v * p.dimU + u; };

	for (uint32_t v = 0; v < p.dimV; ++v) {
		for (uint32_t u = 0; u < p.dimU; ++u) {
			if (visited[idxUV(u, v)])
				continue;

			uint8_t self = VoxelAt(voxels, dim, p, u, v, /*n*/ 0); // we iterate per n later.
			// Actually — for face greedy, we want to find voxels where THIS face is visible.
			// Self material == voxel material at (u,v,n=nMin).
			// Neighbor == air or different material.
			// Iterate n separately per n-slab; merge within a slab.

			// Reset — outer loop is over n.
			(void)self;
		}
	}

	// Correct per-slab greedy merge: iterate n, then for each n-slab, merge in (u,v).
	for (uint32_t n = 0; n < p.dimN; ++n) {
		std::fill(visited.begin(), visited.end(), 0);
		for (uint32_t v = 0; v < p.dimV; ++v) {
			for (uint32_t u = 0; u < p.dimU; ++u) {
				if (visited[idxUV(u, v)])
					continue;

				uint8_t self = VoxelAt(voxels, dim, p, u, v, n);
				if (self == 0)
					continue; // air voxel — no face.

				uint8_t neighbor = NeighborAt(voxels, dim, p, u, v, n);
				if (!IsFaceVisible(self, neighbor))
					continue;

				// Extend along U.
				uint32_t uEnd = u + 1;
				while (uEnd < p.dimU) {
					if (visited[idxUV(uEnd, v)])
						break;
					uint8_t s = VoxelAt(voxels, dim, p, uEnd, v, n);
					uint8_t nb = NeighborAt(voxels, dim, p, uEnd, v, n);
					if (s != self || !IsFaceVisible(s, nb))
						break;
					++uEnd;
				}

				// Extend along V.
				uint32_t vEnd = v + 1;
				bool vExtend = true;
				while (vExtend && vEnd < p.dimV) {
					for (uint32_t uu = u; uu < uEnd; ++uu) {
						if (visited[idxUV(uu, vEnd)]) {
							vExtend = false;
							break;
						}
						uint8_t s = VoxelAt(voxels, dim, p, uu, vEnd, n);
						uint8_t nb = NeighborAt(voxels, dim, p, uu, vEnd, n);
						if (s != self || !IsFaceVisible(s, nb)) {
							vExtend = false;
							break;
						}
					}
					if (vExtend)
						++vEnd;
				}

				// Emit one quad: u..uEnd-1, v..vEnd-1.
				Quad q{};
				q.extentU = uEnd - u;
				q.extentV = vEnd - v;
				q.face = face;
				q.mat = self;
				// Place origin at (u, v, n) in face-plane coords.
				uint32_t xyz[3]{};
				xyz[p.axisU] = u;
				xyz[p.axisV] = v;
				xyz[p.normalAxis] = n;
				q.x0 = xyz[0];
				q.y0 = xyz[1];
				q.z0 = xyz[2];
				out.push_back(q);

				for (uint32_t vv = v; vv < vEnd; ++vv)
					for (uint32_t uu = u; uu < uEnd; ++uu)
						visited[idxUV(uu, vv)] = 1;
			}
		}
	}
}

} // namespace

void GenerateScene(const SceneConfig &cfg,
				   std::vector<std::vector<uint8_t>> &chunksOut,
				   std::vector<PackedFace> &facesOut,
				   std::vector<ChunkDescriptor> &chunkDescriptorsOut,
				   std::vector<MaterialVisual> &materialsOut)
{
	chunksOut.clear();
	facesOut.clear();
	chunkDescriptorsOut.clear();
	materialsOut.clear();

	const uint32_t dim = cfg.chunkDim;
	const uint32_t N = cfg.chunksPerSide;

	// ---- Materials (ProjectV-style palette) ----
	materialsOut.resize(std::max(1u, cfg.materialCount));
	// Spread hues across material count for visual distinction.
	for (uint32_t i = 0; i < materialsOut.size(); ++i) {
		float h = (float(i) / float(materialsOut.size())) * 6.0f;
		float r = std::clamp(std::abs(h - 3.0f) - 1.0f, 0.0f, 1.0f);
		float g = std::clamp(2.0f - std::abs(h - 2.0f), 0.0f, 1.0f);
		float b = std::clamp(2.0f - std::abs(h - 4.0f), 0.0f, 1.0f);
		materialsOut[i].baseColor = {0.2f + 0.8f * r, 0.2f + 0.8f * g, 0.2f + 0.8f * b, 1.0f};
		materialsOut[i].surface = {0.5f, 0.5f, 0.5f, 0.0f}; // (roughness, metallic, reflectance, _)
		materialsOut[i].medium = {0.0f, 0.0f, 0.0f, 0.0f};
		materialsOut[i].shading = {0.0f, 0.0f, 0.0f, 0.0f};
	}

	// ---- Procedural scene: small "VoxelLab" — ground plateau + scattered columns ----
	PcgRng rng(cfg.seed);
	chunksOut.resize(N * N * N);
	uint32_t faceOffset = 0;
	for (uint32_t cz = 0; cz < N; ++cz) {
		for (uint32_t cy = 0; cy < N; ++cy) {
			for (uint32_t cx = 0; cx < N; ++cx) {
				std::vector<uint8_t> &voxels = chunksOut[cz * N * N + cy * N + cx];
				voxels.assign(dim * dim * dim, 0);

				// Ground layer (bottom 2 slices), material 1 (grass-like).
				for (uint32_t z = 0; z < std::min(dim, 2u); ++z)
					for (uint32_t y = 0; y < dim; ++y)
						for (uint32_t x = 0; x < dim; ++x) {
							uint32_t idx = (z * dim + y) * dim + x;
							voxels[idx] = 1;
						}

				// Mid-layer: scatter columns + walls (material 2..N).
				for (uint32_t y = 1; y + 1 < dim; ++y) {
					for (uint32_t x = 1; x + 1 < dim; ++x) {
						if ((x + y + cx * 7 + cy * 5 + cz * 3) % 5 == 0) {
							uint32_t mid = 2 + (rng.next() % std::max(1u, cfg.materialCount - 1));
							for (uint32_t z = 2; z + 2 < dim; ++z) {
								uint32_t idx = (z * dim + y) * dim + x;
								voxels[idx] = uint8_t(mid);
							}
						}
					}
				}

				// Top "ceiling": rare, material N (snow-like).
				if ((cx + cy * 3 + cz * 5) % 7 == 0) {
					uint32_t z = dim - 1;
					for (uint32_t y = 0; y < dim; ++y)
						for (uint32_t x = 0; x < dim; ++x) {
							uint32_t idx = (z * dim + y) * dim + x;
							voxels[idx] = uint8_t(cfg.materialCount);
						}
				}
			}
		}
	}

	// ---- Greedy meshing → PackedFace list ----
	std::vector<Quad> allQuads;
	for (uint32_t ci = 0; ci < chunksOut.size(); ++ci) {
		uint32_t cx = ci % N;
		uint32_t cy = (ci / N) % N;
		uint32_t cz = ci / (N * N);

		size_t before = allQuads.size();
		for (uint32_t face = 0; face < 6; ++face) {
			GreedyMerge(chunksOut[ci], dim, face, allQuads);
		}
		size_t after = allQuads.size();

		// Encode quads → PackedFace (one PackedFace per quad → 6 verts after vertex shader expansion).
		for (size_t q = before; q < after; ++q) {
			const Quad &qd = allQuads[q];
			PackedFace pf{};
			pf.localVoxelFace = PackLocalVoxelFace(qd.x0, qd.y0, qd.z0, qd.face);
			pf.chunkIndexMaterial = PackChunkIndexMaterial(ci, qd.mat);
			pf.lightingData = 0;
			pf.packedExtents = PackExtents(qd.extentU, qd.extentV);
			facesOut.push_back(pf);
		}

		ChunkDescriptor cd{};
		cd.chunkOrigin = {int32_t(cx * dim), int32_t(cy * dim), int32_t(cz * dim), 0};
		cd.chunkExtentAndNonAir = {dim, dim, dim, uint32_t(after - before)};
		cd.voxelDataInfo = {0, 0, 0, 0};
		cd.drawRanges = {uint32_t(faceOffset), uint32_t(after - before), 0, 0};
		faceOffset += uint32_t(after - before);
		chunkDescriptorsOut.push_back(cd);
	}
}

} // namespace vb

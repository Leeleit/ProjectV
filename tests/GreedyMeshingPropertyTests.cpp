#include "voxel/CpuGreedyMeshing.hpp"

#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>

namespace {

struct TestContext {
	int failures = 0;
	void Fail(const int line, const std::string_view msg)
	{
		std::fprintf(stderr, "FAIL line %d: %.*s\n", line, static_cast<int>(msg.size()), msg.data());
		++failures;
	}
};

using namespace projectv::voxel;

struct Xorshift32 {
	uint32_t state;
	explicit Xorshift32(const uint32_t seed) : state(seed ? seed : 1u) {}
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

struct CoverageMap {
	std::vector<bool> covered;
	uint32_t dimN = 0;
	uint32_t dimU = 0;
	uint32_t dimV = 0;

	void Init(const uint32_t nN, const uint32_t nU, const uint32_t nV)
	{
		dimN = nN;
		dimU = nU;
		dimV = nV;
		covered.assign(static_cast<size_t>(nN) * static_cast<size_t>(nU) * static_cast<size_t>(nV), false);
	}

	void Mark(const uint32_t pN, const uint32_t pU, const uint32_t pV)
	{
		if (pN >= dimN || pU >= dimU || pV >= dimV) {
			return;
		}
		const size_t idx = static_cast<size_t>(pN) * dimU * dimV + static_cast<size_t>(pU) * dimV + static_cast<size_t>(pV);
		covered[idx] = true;
	}

	[[nodiscard]] bool IsMarked(const uint32_t pN, const uint32_t pU, const uint32_t pV) const
	{
		if (pN >= dimN || pU >= dimU || pV >= dimV) {
			return false;
		}
		const size_t idx = static_cast<size_t>(pN) * dimU * dimV + static_cast<size_t>(pU) * dimV + static_cast<size_t>(pV);
		return covered[idx];
	}
};

const int kFaceAxisN[6] = {0, 0, 1, 1, 2, 2};
const int kFaceAxisU[6] = {1, 1, 0, 0, 0, 0};
const int kFaceAxisV[6] = {2, 2, 2, 2, 1, 1};
[[maybe_unused]] constexpr int kFaceSign[6] = {1, -1, 1, -1, 1, -1};

struct QuadCoverageVerifier {
	const CpuGreedyInput &input;

	void CheckNoGapsNoOverlaps(TestContext &ctx, const std::vector<CpuGreedyFace> &faces) const
	{
		for (uint32_t fi = 0u; fi < 6u; ++fi) {
			const uint32_t extentN = input.lodLevel == 0u ? input.chunk.extent[kFaceAxisN[fi]] : input.lodExtent;
			const uint32_t extentU = input.lodLevel == 0u ? input.chunk.extent[kFaceAxisU[fi]] : input.lodExtent;
			const uint32_t extentV = input.lodLevel == 0u ? input.chunk.extent[kFaceAxisV[fi]] : input.lodExtent;

			CoverageMap cov;
			cov.Init(extentN, extentU, extentV);

			for (const auto &f : faces) {
				const auto [x, y, z, faceIndexLocal] = UnpackLocalVoxelFaceCPU(f.localVoxelFace);
				if (faceIndexLocal != fi) {
					continue;
				}
				const auto [width, height] = UnpackQuadExtentsCPU(f.packedExtents);
				const uint32_t lc[3] = {x, y, z};
				const uint32_t pN = lc[kFaceAxisN[fi]];
				const uint32_t pU = lc[kFaceAxisU[fi]];
				const uint32_t pV = lc[kFaceAxisV[fi]];

				for (uint32_t dv = 0u; dv < height; ++dv) {
					for (uint32_t du = 0u; du < width; ++du) {
						if (cov.IsMarked(pN, pU + du, pV + dv)) {
							ctx.Fail(__LINE__, "OVERLAP: two quads cover the same (pN,pU,pV) cell");
							std::fprintf(stderr, "  face=%u pN=%u pU=%u pV=%u du=%u dv=%u\n", fi, pN, pU, pV, du, dv);
						}
						cov.Mark(pN, pU + du, pV + dv);
					}
				}
			}
		}
	}
};

// noinspection DfaConstantParameter
void TestPropertyVolumePreservation(TestContext &ctx, const uint32_t seed, const int chunkSize, const int iterations)
{
	Xorshift32 rng(seed);
	for (int iter = 0; iter < iterations; ++iter) {
		std::vector<uint8_t> voxels(static_cast<size_t>(chunkSize) * static_cast<size_t>(chunkSize) * static_cast<size_t>(chunkSize));
		for (auto &v : voxels) {
			v = rng.NextMaterial();
		}

		CpuGreedyInput input{};
		input.worldVoxels = voxels.data();
		input.worldDim[0] = chunkSize;
		input.worldDim[1] = chunkSize;
		input.worldDim[2] = chunkSize;
		input.chunk.chunkOrigin[0] = 0;
		input.chunk.chunkOrigin[1] = 0;
		input.chunk.chunkOrigin[2] = 0;
		input.chunk.extent[0] = static_cast<uint32_t>(chunkSize);
		input.chunk.extent[1] = static_cast<uint32_t>(chunkSize);
		input.chunk.extent[2] = static_cast<uint32_t>(chunkSize);

		uint32_t nonAir = 0u;
		for (const uint8_t v : voxels) {
			if (v != 0u) {
				++nonAir;
			}
		}
		input.chunk.nonAirCount = nonAir;
		input.chunk.chunkIndex = 0u;

		const auto [opaqueFaces, transparentFaces] = GenerateCpuGreedyMesh(input);

		static const int32_t offsets[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
		uint32_t bruteTotal = 0u;
		uint32_t bruteOpaque = 0u;
		uint32_t bruteTransparent = 0u;
		for (int z = 0; z < chunkSize; ++z) {
			for (int y = 0; y < chunkSize; ++y) {
				for (int x = 0; x < chunkSize; ++x) {
					const uint8_t mat = voxels[static_cast<size_t>(x) + static_cast<size_t>(chunkSize) * (static_cast<size_t>(y) + static_cast<size_t>(chunkSize) * static_cast<size_t>(z))];
					if (mat == 0u) {
						continue;
					}
					for (int d = 0; d < 6; ++d) {
						const int nx = x + offsets[d][0];
						const int ny = y + offsets[d][1];
						const int nz = z + offsets[d][2];
						uint8_t nbr = 0u;
						if (nx >= 0 && ny >= 0 && nz >= 0 && nx < chunkSize && ny < chunkSize && nz < chunkSize) {
							nbr = voxels[static_cast<size_t>(nx) + static_cast<size_t>(chunkSize) * (static_cast<size_t>(ny) + static_cast<size_t>(chunkSize) * static_cast<size_t>(nz))];
						}
						if (ShouldEmitVoxelFaceCPU(mat, nbr)) {
							++bruteTotal;
							if (mat == 1u) {
								++bruteTransparent;
							} else {
								++bruteOpaque;
							}
						}
					}
				}
			}
		}

		uint32_t greedyOpaque = 0u;
		for (const auto &f : opaqueFaces) {
			const auto [width, height] = UnpackQuadExtentsCPU(f.packedExtents);
			greedyOpaque += width * height;
		}
		uint32_t greedyTransparent = 0u;
		for (const auto &f : transparentFaces) {
			const auto [width, height] = UnpackQuadExtentsCPU(f.packedExtents);
			greedyTransparent += width * height;
		}

		if (greedyOpaque != bruteOpaque) {
			ctx.Fail(__LINE__, "volume preservation: opaque area mismatch");
			std::fprintf(stderr, "  seed=%u iter=%d size=%d greedy=%u brute=%u\n", seed, iter, chunkSize, greedyOpaque, bruteOpaque);
			return;
		}
		if (greedyTransparent != bruteTransparent) {
			ctx.Fail(__LINE__, "volume preservation: transparent area mismatch");
			std::fprintf(stderr, "  seed=%u iter=%d size=%d greedy=%u brute=%u\n", seed, iter, chunkSize, greedyTransparent, bruteTransparent);
			return;
		}
		if (greedyOpaque + greedyTransparent != bruteTotal) {
			ctx.Fail(__LINE__, "volume preservation: total mismatch");
			return;
		}
	}
}

void TestPropertyNoOverlap(TestContext &ctx, const uint32_t seed, const int chunkSize, const int iterations)
{
	Xorshift32 rng(seed);
	for (int iter = 0; iter < iterations; ++iter) {
		std::vector<uint8_t> voxels(static_cast<size_t>(chunkSize) * static_cast<size_t>(chunkSize) * static_cast<size_t>(chunkSize));
		for (auto &v : voxels) {
			v = rng.NextMaterial();
		}

		CpuGreedyInput input{};
		input.worldVoxels = voxels.data();
		input.worldDim[0] = chunkSize;
		input.worldDim[1] = chunkSize;
		input.worldDim[2] = chunkSize;
		input.chunk.chunkOrigin[0] = 0;
		input.chunk.chunkOrigin[1] = 0;
		input.chunk.chunkOrigin[2] = 0;
		input.chunk.extent[0] = static_cast<uint32_t>(chunkSize);
		input.chunk.extent[1] = static_cast<uint32_t>(chunkSize);
		input.chunk.extent[2] = static_cast<uint32_t>(chunkSize);
		input.chunk.nonAirCount = 0u;
		for (const uint8_t v : voxels) {
			if (v != 0u) {
				++input.chunk.nonAirCount;
			}
		}

		const auto [opaqueFaces, transparentFaces] = GenerateCpuGreedyMesh(input);

		QuadCoverageVerifier verifier{input};
		verifier.CheckNoGapsNoOverlaps(ctx, opaqueFaces);
		verifier.CheckNoGapsNoOverlaps(ctx, transparentFaces);

		if (ctx.failures > 0) {
			std::fprintf(stderr, "  seed=%u iter=%d size=%d\n", seed, iter, chunkSize);
			return;
		}
	}
}

// noinspection DfaConstantParameter
void TestPropertyDeterminism(TestContext &ctx, uint32_t seed, int chunkSize, int iterations)
{
	Xorshift32 rng(seed);
	for (int iter = 0; iter < iterations; ++iter) {
		std::vector<uint8_t> voxels(static_cast<size_t>(chunkSize) * static_cast<size_t>(chunkSize) * static_cast<size_t>(chunkSize));
		for (auto &v : voxels) {
			v = rng.NextMaterial();
		}

		CpuGreedyInput input{};
		input.worldVoxels = voxels.data();
		input.worldDim[0] = chunkSize;
		input.worldDim[1] = chunkSize;
		input.worldDim[2] = chunkSize;
		input.chunk.chunkOrigin[0] = 0;
		input.chunk.chunkOrigin[1] = 0;
		input.chunk.chunkOrigin[2] = 0;
		input.chunk.extent[0] = static_cast<uint32_t>(chunkSize);
		input.chunk.extent[1] = static_cast<uint32_t>(chunkSize);
		input.chunk.extent[2] = static_cast<uint32_t>(chunkSize);
		input.chunk.nonAirCount = 0u;
		for (const uint8_t v : voxels) {
			if (v != 0u) {
				++input.chunk.nonAirCount;
			}
		}

		const auto [opaqueFaces1, transparentFaces1] = GenerateCpuGreedyMesh(input);
		const auto [opaqueFaces, transparentFaces] = GenerateCpuGreedyMesh(input);
		if (opaqueFaces1 != opaqueFaces || transparentFaces1 != transparentFaces) {
			ctx.Fail(__LINE__, "determinism: identical inputs must produce identical outputs");
			std::fprintf(stderr, "  seed=%u iter=%d size=%d\n", seed, iter, chunkSize);
			return;
		}
	}
}

// noinspection DfaConstantParameter
void TestPropertyMaterialConsistency(TestContext &ctx, uint32_t seed, int chunkSize, int iterations)
{
	Xorshift32 rng(seed);
	for (int iter = 0; iter < iterations; ++iter) {
		std::vector<uint8_t> voxels(static_cast<size_t>(chunkSize) * static_cast<size_t>(chunkSize) * static_cast<size_t>(chunkSize));
		for (auto &v : voxels) {
			v = rng.NextMaterial();
		}

		CpuGreedyInput input{};
		input.worldVoxels = voxels.data();
		input.worldDim[0] = chunkSize;
		input.worldDim[1] = chunkSize;
		input.worldDim[2] = chunkSize;
		input.chunk.chunkOrigin[0] = 0;
		input.chunk.chunkOrigin[1] = 0;
		input.chunk.chunkOrigin[2] = 0;
		input.chunk.extent[0] = static_cast<uint32_t>(chunkSize);
		input.chunk.extent[1] = static_cast<uint32_t>(chunkSize);
		input.chunk.extent[2] = static_cast<uint32_t>(chunkSize);
		input.chunk.nonAirCount = 0u;
		for (const uint8_t v : voxels) {
			if (v != 0u) {
				++input.chunk.nonAirCount;
			}
		}

		const auto [opaqueFaces, transparentFaces] = GenerateCpuGreedyMesh(input);

		for (const auto &f : opaqueFaces) {
			const auto [chunkIndex, materialIndex] = UnpackChunkIndexMaterialCPU(f.chunkIndexMaterial);
			if (materialIndex == 0u || materialIndex == 1u) {
				ctx.Fail(__LINE__, "opaque face must have non-Air non-Glass material");
				std::fprintf(stderr, "  seed=%u iter=%d mat=%u\n", seed, iter, materialIndex);
				return;
			}
		}
		for (const auto &f : transparentFaces) {
			const auto [chunkIndex, materialIndex] = UnpackChunkIndexMaterialCPU(f.chunkIndexMaterial);
			if (materialIndex != 1u) {
				ctx.Fail(__LINE__, "transparent face must have Glass material (1)");
				std::fprintf(stderr, "  seed=%u iter=%d mat=%u\n", seed, iter, materialIndex);
				return;
			}
		}
	}
}

// noinspection DfaConstantParameter
void TestPropertyExtentsWithinBounds(TestContext &ctx, uint32_t seed, int chunkSize, int iterations)
{
	Xorshift32 rng(seed);
	for (int iter = 0; iter < iterations; ++iter) {
		std::vector<uint8_t> voxels(static_cast<size_t>(chunkSize) * static_cast<size_t>(chunkSize) * static_cast<size_t>(chunkSize));
		for (auto &v : voxels) {
			v = rng.NextMaterial();
		}

		CpuGreedyInput input{};
		input.worldVoxels = voxels.data();
		input.worldDim[0] = chunkSize;
		input.worldDim[1] = chunkSize;
		input.worldDim[2] = chunkSize;
		input.chunk.chunkOrigin[0] = 0;
		input.chunk.chunkOrigin[1] = 0;
		input.chunk.chunkOrigin[2] = 0;
		input.chunk.extent[0] = static_cast<uint32_t>(chunkSize);
		input.chunk.extent[1] = static_cast<uint32_t>(chunkSize);
		input.chunk.extent[2] = static_cast<uint32_t>(chunkSize);
		input.chunk.nonAirCount = 0u;
		for (const uint8_t v : voxels) {
			if (v != 0u) {
				++input.chunk.nonAirCount;
			}
		}

		const auto [opaqueFaces, transparentFaces] = GenerateCpuGreedyMesh(input);
		const uint32_t maxExtent = static_cast<uint32_t>(chunkSize);
		auto checkFaces = [&](const std::vector<CpuGreedyFace> &faces) {
			for (const auto &f : faces) {
				const auto [x, y, z, faceIndex] = UnpackLocalVoxelFaceCPU(f.localVoxelFace);
				const auto [width, height] = UnpackQuadExtentsCPU(f.packedExtents);
				if (x >= maxExtent || y >= maxExtent || z >= maxExtent) {
					ctx.Fail(__LINE__, "localVoxelFace coordinate out of bounds");
					return;
				}
				if (width == 0u || height == 0u) {
					ctx.Fail(__LINE__, "quad extents must be non-zero");
					return;
				}
				const uint32_t axisU = kFaceAxisU[faceIndex];
				const uint32_t axisV = kFaceAxisV[faceIndex];
				const uint32_t lcArr[3] = {x, y, z};
				if (lcArr[axisU] + width > maxExtent) {
					ctx.Fail(__LINE__, "quad width extends past chunk boundary");
					return;
				}
				if (lcArr[axisV] + height > maxExtent) {
					ctx.Fail(__LINE__, "quad height extends past chunk boundary");
					return;
				}
			}
		};
		checkFaces(opaqueFaces);
		checkFaces(transparentFaces);
		if (ctx.failures > 0) {
			std::fprintf(stderr, "  seed=%u iter=%d size=%d\n", seed, iter, chunkSize);
			return;
		}
	}
}

} // namespace

int main()
{
	TestContext ctx{};
	constexpr uint32_t kSeed = 0xDEADBEEFu;
	constexpr int kIterations = 1000;

	TestPropertyVolumePreservation(ctx, kSeed, 4, kIterations);
	TestPropertyVolumePreservation(ctx, kSeed + 1, 8, kIterations);
	TestPropertyVolumePreservation(ctx, kSeed + 2, 16, kIterations);

	TestPropertyNoOverlap(ctx, kSeed + 10, 8, kIterations);
	TestPropertyNoOverlap(ctx, kSeed + 11, 16, 200);

	TestPropertyDeterminism(ctx, kSeed + 20, 8, kIterations);

	TestPropertyMaterialConsistency(ctx, kSeed + 30, 8, kIterations);

	TestPropertyExtentsWithinBounds(ctx, kSeed + 40, 8, kIterations);

	if (ctx.failures > 0) {
		std::fprintf(stderr, "ProjectVGreedyMeshingPropertyTests: %d failure(s)\n", ctx.failures);
		return EXIT_FAILURE;
	}
	std::puts("ProjectVGreedyMeshingPropertyTests passed");
	return EXIT_SUCCESS;
}

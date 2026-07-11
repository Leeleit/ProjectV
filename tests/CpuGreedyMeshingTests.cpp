#include "voxel/CpuGreedyMeshing.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
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

struct TestWorld {
	std::vector<uint8_t> voxels;
	int dim[3] = {};
	int32_t origin[3] = {};

	[[nodiscard]] CpuGreedyInput MakeInput(const uint32_t chunkIndex = 0u) const
	{
		CpuGreedyInput input{};
		input.worldVoxels = voxels.data();
		input.worldDim[0] = dim[0];
		input.worldDim[1] = dim[1];
		input.worldDim[2] = dim[2];
		input.worldMin[0] = origin[0];
		input.worldMin[1] = origin[1];
		input.worldMin[2] = origin[2];
		input.chunk.chunkOrigin[0] = origin[0];
		input.chunk.chunkOrigin[1] = origin[1];
		input.chunk.chunkOrigin[2] = origin[2];
		input.chunk.extent[0] = static_cast<uint32_t>(dim[0]);
		input.chunk.extent[1] = static_cast<uint32_t>(dim[1]);
		input.chunk.extent[2] = static_cast<uint32_t>(dim[2]);
		input.chunk.chunkIndex = chunkIndex;
		uint32_t nonAir = 0u;
		for (const uint8_t v : voxels) {
			if (v != 0u) {
				++nonAir;
			}
		}
		input.chunk.nonAirCount = nonAir;
		return input;
	}
};

TestWorld MakeWorld(const int sx, const int sy, const int sz, const uint8_t fillValue = 0u)
{
	TestWorld w;
	w.dim[0] = sx;
	w.dim[1] = sy;
	w.dim[2] = sz;
	w.origin[0] = 0;
	w.origin[1] = 0;
	w.origin[2] = 0;
	w.voxels.assign(static_cast<size_t>(sx) * static_cast<size_t>(sy) * static_cast<size_t>(sz), fillValue);
	return w;
}

void SetVoxel(TestWorld &w, const int x, const int y, const int z, const uint8_t mat)
{
	const size_t idx = static_cast<size_t>(x) + static_cast<size_t>(w.dim[0]) * (static_cast<size_t>(y) + static_cast<size_t>(w.dim[1]) * static_cast<size_t>(z));
	w.voxels[idx] = mat;
}

uint32_t TotalFaces(const CpuGreedyMeshResult &r)
{
	return static_cast<uint32_t>(r.opaqueFaces.size() + r.transparentFaces.size());
}

uint32_t SumQuadAreas(const std::vector<CpuGreedyFace> &faces)
{
	uint32_t sum = 0u;
	for (const auto &f : faces) {
		const auto ext = UnpackQuadExtentsCPU(f.packedExtents);
		sum += ext.width * ext.height;
	}
	return sum;
}

uint32_t SumQuadAreaForFace(const std::vector<CpuGreedyFace> &faces, const uint32_t faceIndex)
{
	uint32_t sum = 0u;
	for (const auto &f : faces) {
		const auto u = UnpackLocalVoxelFaceCPU(f.localVoxelFace);
		if (u.faceIndex == faceIndex) {
			const auto ext = UnpackQuadExtentsCPU(f.packedExtents);
			sum += ext.width * ext.height;
		}
	}
	return sum;
}

struct BruteForceCounts {
	uint32_t perDir[6] = {};
	uint32_t total = 0u;
	uint32_t opaque = 0u;
	uint32_t transparent = 0u;
};

BruteForceCounts CountFacesBruteForce(const CpuGreedyInput &input)
{
	BruteForceCounts result{};
	static const int32_t offsets[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
	for (uint32_t z = 0u; z < input.chunk.extent[2]; ++z) {
		for (uint32_t y = 0u; y < input.chunk.extent[1]; ++y) {
			for (uint32_t x = 0u; x < input.chunk.extent[0]; ++x) {
				const int32_t wx = input.chunk.chunkOrigin[0] + static_cast<int32_t>(x);
				const int32_t wy = input.chunk.chunkOrigin[1] + static_cast<int32_t>(y);
				const int32_t wz = input.chunk.chunkOrigin[2] + static_cast<int32_t>(z);
				const uint8_t mat = ReadVoxelMaterialCPU(input, wx, wy, wz);
				if (mat == 0u) {
					continue;
				}
				for (int d = 0; d < 6; ++d) {
					const uint8_t nbr = ReadVoxelMaterialCPU(input, wx + offsets[d][0], wy + offsets[d][1], wz + offsets[d][2]);
					if (ShouldEmitVoxelFaceCPU(mat, nbr)) {
						result.perDir[d]++;
						result.total++;
						if (mat == 1u) {
							result.transparent++;
						} else {
							result.opaque++;
						}
					}
				}
			}
		}
	}
	return result;
}

void CheckVolumePreservation(TestContext &ctx, const CpuGreedyInput &input, const CpuGreedyMeshResult &mesh)
{
	const BruteForceCounts bf = CountFacesBruteForce(input);
	const uint32_t greedyOpaque = SumQuadAreas(mesh.opaqueFaces);
	const uint32_t greedyTransparent = SumQuadAreas(mesh.transparentFaces);

	if (greedyOpaque != bf.opaque) {
		ctx.Fail(__LINE__, "opaque volume mismatch");
		std::fprintf(stderr, "  greedy=%u brute=%u\n", greedyOpaque, bf.opaque);
	}
	if (greedyTransparent != bf.transparent) {
		ctx.Fail(__LINE__, "transparent volume mismatch");
		std::fprintf(stderr, "  greedy=%u brute=%u\n", greedyTransparent, bf.transparent);
	}
	for (int d = 0; d < 6; ++d) {
		const uint32_t greedyDir = SumQuadAreaForFace(mesh.opaqueFaces, static_cast<uint32_t>(d)) +
								   SumQuadAreaForFace(mesh.transparentFaces, static_cast<uint32_t>(d));
		if (greedyDir != bf.perDir[d]) {
			ctx.Fail(__LINE__, "per-direction volume mismatch");
			std::fprintf(stderr, "  dir=%d greedy=%u brute=%u\n", d, greedyDir, bf.perDir[d]);
		}
	}
}

void TestEmptyChunk(TestContext &ctx)
{
	const TestWorld w = MakeWorld(4, 4, 4);
	const auto input = w.MakeInput();
	const auto mesh = GenerateCpuGreedyMesh(input);
	if (TotalFaces(mesh) != 0u) {
		ctx.Fail(__LINE__, "empty chunk must produce zero faces");
	}
	CheckVolumePreservation(ctx, input, mesh);
}

void TestSingleVoxelCenter(TestContext &ctx)
{
	TestWorld w = MakeWorld(4, 4, 4);
	SetVoxel(w, 1, 1, 1, 3u);
	const auto input = w.MakeInput();
	const auto mesh = GenerateCpuGreedyMesh(input);
	if (mesh.opaqueFaces.size() != 6u) {
		ctx.Fail(__LINE__, "single center voxel must produce 6 opaque faces");
	}
	if (!mesh.transparentFaces.empty()) {
		ctx.Fail(__LINE__, "single FloorWhite voxel must produce 0 transparent faces");
	}
	for (const auto &f : mesh.opaqueFaces) {
		const auto ext = UnpackQuadExtentsCPU(f.packedExtents);
		if (ext.width != 1u || ext.height != 1u) {
			ctx.Fail(__LINE__, "isolated voxel faces must be 1x1");
		}
	}
	CheckVolumePreservation(ctx, input, mesh);
}

void TestSingleVoxelCorner(TestContext &ctx)
{
	TestWorld w = MakeWorld(4, 4, 4);
	SetVoxel(w, 0, 0, 0, 3u);
	const auto input = w.MakeInput();
	const auto mesh = GenerateCpuGreedyMesh(input);
	if (mesh.opaqueFaces.size() != 6u) {
		ctx.Fail(__LINE__, "corner voxel must produce 6 faces (OOB = Air)");
	}
	CheckVolumePreservation(ctx, input, mesh);
}

void TestFullyFilledChunk(TestContext &ctx)
{
	const TestWorld w = MakeWorld(4, 4, 4, 3u);
	const auto input = w.MakeInput();
	const auto mesh = GenerateCpuGreedyMesh(input);
	if (mesh.opaqueFaces.size() != 6u) {
		ctx.Fail(__LINE__, "fully filled chunk must merge to 6 faces (one per direction)");
		std::fprintf(stderr, "  got %zu\n", mesh.opaqueFaces.size());
	}
	for (const auto &f : mesh.opaqueFaces) {
		const auto ext = UnpackQuadExtentsCPU(f.packedExtents);
		if (ext.width != 4u || ext.height != 4u) {
			ctx.Fail(__LINE__, "4x4 chunk face must merge to 4x4 quad");
			std::fprintf(stderr, "  got %ux%u\n", ext.width, ext.height);
		}
	}
	CheckVolumePreservation(ctx, input, mesh);
}

void TestTwoAdjacentSameMaterial(TestContext &ctx)
{
	TestWorld w = MakeWorld(4, 4, 4);
	SetVoxel(w, 1, 1, 1, 3u);
	SetVoxel(w, 2, 1, 1, 3u);
	const auto input = w.MakeInput();
	const auto mesh = GenerateCpuGreedyMesh(input);
	const uint32_t yPlusArea = SumQuadAreaForFace(mesh.opaqueFaces, 2u);
	if (yPlusArea != 2u) {
		ctx.Fail(__LINE__, "Y+ direction must have area 2 for two adjacent voxels");
	}
	const uint32_t xPlusArea = SumQuadAreaForFace(mesh.opaqueFaces, 0u);
	if (xPlusArea != 1u) {
		ctx.Fail(__LINE__, "X+ direction must have area 1 (only rightmost voxel exposed)");
	}
	CheckVolumePreservation(ctx, input, mesh);
}

void TestTwoAdjacentDifferentMaterials(TestContext &ctx)
{
	TestWorld w = MakeWorld(4, 4, 4);
	SetVoxel(w, 1, 1, 1, 3u);
	SetVoxel(w, 2, 1, 1, 4u);
	const auto input = w.MakeInput();
	const auto mesh = GenerateCpuGreedyMesh(input);
	CheckVolumePreservation(ctx, input, mesh);
	uint32_t floorWhiteFaces = 0u;
	uint32_t floorGrayFaces = 0u;
	for (const auto &f : mesh.opaqueFaces) {
		const auto cm = UnpackChunkIndexMaterialCPU(f.chunkIndexMaterial);
		if (cm.materialIndex == 3u) {
			++floorWhiteFaces;
		} else if (cm.materialIndex == 4u) {
			++floorGrayFaces;
		}
	}
	if (floorWhiteFaces == 0u || floorGrayFaces == 0u) {
		ctx.Fail(__LINE__, "both materials must be present in output");
	}
}

void TestGlassGoesTransparent(TestContext &ctx)
{
	TestWorld w = MakeWorld(4, 4, 4);
	SetVoxel(w, 1, 1, 1, 1u);
	const auto input = w.MakeInput();
	const auto mesh = GenerateCpuGreedyMesh(input);
	if (!mesh.opaqueFaces.empty()) {
		ctx.Fail(__LINE__, "Glass voxel must produce zero opaque faces");
	}
	if (mesh.transparentFaces.size() != 6u) {
		ctx.Fail(__LINE__, "Glass voxel must produce 6 transparent faces");
	}
	CheckVolumePreservation(ctx, input, mesh);
}

void TestFullyFilledFluidEmitsInteriorFaces(TestContext &ctx)
{
	const TestWorld w = MakeWorld(4, 4, 4, 2u);
	const auto input = w.MakeInput();
	const auto mesh = GenerateCpuGreedyMesh(input);
	CheckVolumePreservation(ctx, input, mesh);
	const BruteForceCounts bf = CountFacesBruteForce(input);
	constexpr uint32_t expectedPerDir = 4u * 4u * 4u;
	for (int d = 0; d < 6; ++d) {
		if (bf.perDir[d] != expectedPerDir) {
			ctx.Fail(__LINE__, "Fluid must emit faces for every voxel in every direction");
		}
	}
}

void TestFullyFilledGlassOnlyShell(TestContext &ctx)
{
	const TestWorld w = MakeWorld(4, 4, 4, 1u);
	const auto input = w.MakeInput();
	const auto mesh = GenerateCpuGreedyMesh(input);
	if (!mesh.opaqueFaces.empty()) {
		ctx.Fail(__LINE__, "Glass chunk must produce zero opaque faces");
	}
	CheckVolumePreservation(ctx, input, mesh);
}

void TestDeterminism(TestContext &ctx)
{
	TestWorld w = MakeWorld(8, 8, 8);
	SetVoxel(w, 1, 2, 3, 3u);
	SetVoxel(w, 4, 5, 6, 1u);
	SetVoxel(w, 3, 3, 3, 4u);
	const auto input = w.MakeInput();
	const auto mesh1 = GenerateCpuGreedyMesh(input);
	const auto mesh2 = GenerateCpuGreedyMesh(input);
	if (mesh1.opaqueFaces.size() != mesh2.opaqueFaces.size() || mesh1.transparentFaces.size() != mesh2.transparentFaces.size()) {
		ctx.Fail(__LINE__, "determinism: face counts must match");
	}
	if (mesh1.opaqueFaces != mesh2.opaqueFaces) {
		ctx.Fail(__LINE__, "determinism: opaque face data must be identical");
	}
	if (mesh1.transparentFaces != mesh2.transparentFaces) {
		ctx.Fail(__LINE__, "determinism: transparent face data must be identical");
	}
}

void TestCheckerboardNoMerge(TestContext &ctx)
{
	TestWorld w = MakeWorld(4, 4, 4);
	for (int z = 0; z < 4; ++z) {
		for (int y = 0; y < 4; ++y) {
			for (int x = 0; x < 4; ++x) {
				if ((x + y + z) % 2 == 0) {
					SetVoxel(w, x, y, z, 3u);
				}
			}
		}
	}
	const auto input = w.MakeInput();
	const auto mesh = GenerateCpuGreedyMesh(input);
	CheckVolumePreservation(ctx, input, mesh);
	for (const auto &f : mesh.opaqueFaces) {
		const auto ext = UnpackQuadExtentsCPU(f.packedExtents);
		if (ext.width != 1u || ext.height != 1u) {
			ctx.Fail(__LINE__, "checkerboard must never merge (all quads 1x1)");
		}
	}
}

void TestNonOriginChunk(TestContext &ctx)
{
	TestWorld w = MakeWorld(8, 8, 8);
	SetVoxel(w, 4, 4, 4, 3u);
	CpuGreedyInput input = w.MakeInput();
	input.chunk.chunkOrigin[0] = 4;
	input.chunk.chunkOrigin[1] = 4;
	input.chunk.chunkOrigin[2] = 4;
	input.chunk.extent[0] = 4;
	input.chunk.extent[1] = 4;
	input.chunk.extent[2] = 4;
	input.chunk.nonAirCount = 1u;
	const auto mesh = GenerateCpuGreedyMesh(input);
	if (mesh.opaqueFaces.size() != 6u) {
		ctx.Fail(__LINE__, "non-origin single voxel must produce 6 faces");
	}
	CheckVolumePreservation(ctx, input, mesh);
}

void TestSlabGreedyMerge(TestContext &ctx)
{
	const TestWorld w = MakeWorld(8, 8, 8, 3u);
	const auto input = w.MakeInput();
	const auto mesh = GenerateCpuGreedyMesh(input);
	const uint32_t xPlusArea = SumQuadAreaForFace(mesh.opaqueFaces, 0u);
	if (xPlusArea != 64u) {
		ctx.Fail(__LINE__, "8x8 slab X+ face must have area 64");
		std::fprintf(stderr, "  got %u\n", xPlusArea);
	}
	CheckVolumePreservation(ctx, input, mesh);
}

void TestMaterialPreservation(TestContext &ctx)
{
	TestWorld w = MakeWorld(4, 4, 4);
	SetVoxel(w, 1, 1, 1, 3u);
	SetVoxel(w, 2, 2, 2, 4u);
	const auto input = w.MakeInput();
	const auto mesh = GenerateCpuGreedyMesh(input);
	for (const auto &f : mesh.opaqueFaces) {
		const auto cm = UnpackChunkIndexMaterialCPU(f.chunkIndexMaterial);
		if (cm.materialIndex != 3u && cm.materialIndex != 4u) {
			ctx.Fail(__LINE__, "emitted face material must match a source voxel material");
		}
	}
}

void TestChunkIndexEncoding(TestContext &ctx)
{
	TestWorld w = MakeWorld(4, 4, 4);
	SetVoxel(w, 1, 1, 1, 3u);
	const uint32_t testChunkIndex = 42u;
	const auto input = w.MakeInput(testChunkIndex);
	const auto mesh = GenerateCpuGreedyMesh(input);
	for (const auto &f : mesh.opaqueFaces) {
		const auto cm = UnpackChunkIndexMaterialCPU(f.chunkIndexMaterial);
		if (cm.chunkIndex != testChunkIndex) {
			ctx.Fail(__LINE__, "emitted face chunkIndex must match input");
		}
	}
}

void TestLargerChunk16(TestContext &ctx)
{
	const TestWorld w = MakeWorld(16, 16, 16, 3u);
	const auto input = w.MakeInput();
	const auto mesh = GenerateCpuGreedyMesh(input);
	if (mesh.opaqueFaces.size() != 6u) {
		ctx.Fail(__LINE__, "16^3 fully filled must merge to 6 faces");
		std::fprintf(stderr, "  got %zu\n", mesh.opaqueFaces.size());
	}
	for (const auto &f : mesh.opaqueFaces) {
		const auto ext = UnpackQuadExtentsCPU(f.packedExtents);
		if (ext.width != 16u || ext.height != 16u) {
			ctx.Fail(__LINE__, "16^3 face must merge to 16x16 quad");
		}
	}
	CheckVolumePreservation(ctx, input, mesh);
}

void TestMixedGlassAndSolidAdjacent(TestContext &ctx)
{
	TestWorld w = MakeWorld(4, 4, 4);
	SetVoxel(w, 1, 1, 1, 3u);
	SetVoxel(w, 2, 1, 1, 1u);
	const auto input = w.MakeInput();
	const auto mesh = GenerateCpuGreedyMesh(input);
	CheckVolumePreservation(ctx, input, mesh);
	if (mesh.opaqueFaces.empty()) {
		ctx.Fail(__LINE__, "FloorWhite next to Glass must emit opaque faces");
	}
	if (mesh.transparentFaces.empty()) {
		ctx.Fail(__LINE__, "Glass next to FloorWhite must emit transparent faces");
	}
}

void TestNonUniformSlab(TestContext &ctx)
{
	TestWorld w = MakeWorld(8, 4, 8, 3u);
	for (int z = 0; z < 8; ++z) {
		for (int x = 0; x < 8; ++x) {
			SetVoxel(w, x, 0, z, 4u);
		}
	}
	const auto input = w.MakeInput();
	const auto mesh = GenerateCpuGreedyMesh(input);
	CheckVolumePreservation(ctx, input, mesh);
}

void Test1x1x1Chunk(TestContext &ctx)
{
	const TestWorld w = MakeWorld(1, 1, 1, 3u);
	const auto input = w.MakeInput();
	const auto mesh = GenerateCpuGreedyMesh(input);
	if (mesh.opaqueFaces.size() != 6u) {
		ctx.Fail(__LINE__, "1x1x1 chunk must produce 6 faces");
	}
	CheckVolumePreservation(ctx, input, mesh);
}

} // namespace

int main()
{
	TestContext ctx{};
	TestEmptyChunk(ctx);
	TestSingleVoxelCenter(ctx);
	TestSingleVoxelCorner(ctx);
	TestFullyFilledChunk(ctx);
	TestTwoAdjacentSameMaterial(ctx);
	TestTwoAdjacentDifferentMaterials(ctx);
	TestGlassGoesTransparent(ctx);
	TestFullyFilledFluidEmitsInteriorFaces(ctx);
	TestFullyFilledGlassOnlyShell(ctx);
	TestDeterminism(ctx);
	TestCheckerboardNoMerge(ctx);
	TestNonOriginChunk(ctx);
	TestSlabGreedyMerge(ctx);
	TestMaterialPreservation(ctx);
	TestChunkIndexEncoding(ctx);
	TestLargerChunk16(ctx);
	TestMixedGlassAndSolidAdjacent(ctx);
	TestNonUniformSlab(ctx);
	Test1x1x1Chunk(ctx);
	if (ctx.failures > 0) {
		std::fprintf(stderr, "ProjectVCpuGreedyMeshingTests: %d failure(s)\n", ctx.failures);
		return EXIT_FAILURE;
	}
	std::puts("ProjectVCpuGreedyMeshingTests passed");
	return EXIT_SUCCESS;
}

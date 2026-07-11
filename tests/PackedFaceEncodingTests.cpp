#include "voxel/CpuGreedyMeshing.hpp"

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {

struct TestContext {
	int failures = 0;
	void Fail(const int line, const std::string_view msg)
	{
		std::fprintf(stderr, "FAIL line %d: %.*s\n", line, static_cast<int>(msg.size()), msg.data());
		++failures;
	}
};

using projectv::voxel::PackChunkIndexMaterialCPU;
using projectv::voxel::PackLocalVoxelFaceCPU;
using projectv::voxel::PackQuadExtentsCPU;
using projectv::voxel::UnpackChunkIndexMaterialCPU;
using projectv::voxel::UnpackLocalVoxelFaceCPU;
using projectv::voxel::UnpackQuadExtentsCPU;

void TestPackLocalVoxelFaceKnownValues(TestContext &ctx)
{
	const uint32_t packed = PackLocalVoxelFaceCPU(1u, 2u, 3u, 4u);
	if (packed != 0x04030201u) {
		ctx.Fail(__LINE__, "PackLocalVoxelFace(1,2,3,4) must be 0x04030201");
	}
	const uint32_t zero = PackLocalVoxelFaceCPU(0u, 0u, 0u, 0u);
	if (zero != 0u) {
		ctx.Fail(__LINE__, "PackLocalVoxelFace(0,0,0,0) must be 0");
	}
	const uint32_t maxed = PackLocalVoxelFaceCPU(255u, 255u, 255u, 255u);
	if (maxed != 0xFFFFFFFFu) {
		ctx.Fail(__LINE__, "PackLocalVoxelFace(255,255,255,255) must be 0xFFFFFFFF");
	}
}

void TestPackQuadExtentsKnownValues(TestContext &ctx)
{
	if (PackQuadExtentsCPU(1u, 1u) != 0x00000101u) {
		ctx.Fail(__LINE__, "PackQuadExtents(1,1) mismatch");
	}
	if (PackQuadExtentsCPU(0u, 0u) != 0u) {
		ctx.Fail(__LINE__, "PackQuadExtents(0,0) must be 0");
	}
	if (PackQuadExtentsCPU(255u, 255u) != 0x0000FFFFu) {
		ctx.Fail(__LINE__, "PackQuadExtents(255,255) must be 0xFFFF");
	}
	if (PackQuadExtentsCPU(4u, 8u) != 0x00000804u) {
		ctx.Fail(__LINE__, "PackQuadExtents(4,8) mismatch");
	}
}

void TestPackChunkIndexMaterialKnownValues(TestContext &ctx)
{
	if (PackChunkIndexMaterialCPU(0u, 0u) != 0u) {
		ctx.Fail(__LINE__, "PackChunkIndexMaterial(0,0) must be 0");
	}
	const uint32_t packed = PackChunkIndexMaterialCPU(1u, 3u);
	if (packed != 0x03000001u) {
		ctx.Fail(__LINE__, "PackChunkIndexMaterial(1,3) must be 0x03000001");
	}
	const uint32_t maxChunk = PackChunkIndexMaterialCPU(0x00FFFFFFu, 0u);
	if (maxChunk != 0x00FFFFFFu) {
		ctx.Fail(__LINE__, "PackChunkIndexMaterial(maxChunk,0) must be 0x00FFFFFF");
	}
	const uint32_t maxMat = PackChunkIndexMaterialCPU(0u, 255u);
	if (maxMat != 0xFF000000u) {
		ctx.Fail(__LINE__, "PackChunkIndexMaterial(0,255) must be 0xFF000000");
	}
}

void TestUnpackLocalVoxelFaceRoundTrip(TestContext &ctx)
{
	for (uint32_t x = 0u; x <= 255u; x += 17u) {
		for (uint32_t y = 0u; y <= 255u; y += 23u) {
			for (uint32_t z = 0u; z <= 255u; z += 31u) {
				for (uint32_t face = 0u; face <= 5u; ++face) {
					const uint32_t packed = PackLocalVoxelFaceCPU(x, y, z, face);
					const auto [ux, uy, uz, faceIndex] = UnpackLocalVoxelFaceCPU(packed);
					if (ux != x || uy != y || uz != z || faceIndex != face) {
						ctx.Fail(__LINE__, "UnpackLocalVoxelFace round-trip failed");
					}
				}
			}
		}
	}
}

void TestUnpackQuadExtentsRoundTrip(TestContext &ctx)
{
	for (uint32_t w = 0u; w <= 255u; w += 7u) {
		for (uint32_t h = 0u; h <= 255u; h += 13u) {
			const uint32_t packed = PackQuadExtentsCPU(w, h);
			const auto [width, height] = UnpackQuadExtentsCPU(packed);
			if (width != w || height != h) {
				ctx.Fail(__LINE__, "UnpackQuadExtents round-trip failed");
			}
		}
	}
}

void TestUnpackChunkIndexMaterialRoundTrip(TestContext &ctx)
{
	constexpr uint32_t chunks[] = {0u, 1u, 255u, 65535u, 0x00FFFFFFu};
	constexpr uint32_t mats[] = {0u, 1u, 2u, 3u, 4u, 255u};
	for (const uint32_t ci : chunks) {
		for (const uint32_t mi : mats) {
			const uint32_t packed = PackChunkIndexMaterialCPU(ci, mi);
			const auto [chunkIndex, materialIndex] = UnpackChunkIndexMaterialCPU(packed);
			if (chunkIndex != ci || materialIndex != mi) {
				ctx.Fail(__LINE__, "UnpackChunkIndexMaterial round-trip failed");
			}
		}
	}
}

void TestByteLayoutLocalVoxelFace(TestContext &ctx)
{
	if ((PackLocalVoxelFaceCPU(1u, 0u, 0u, 0u) & 0xFFu) != 1u) {
		ctx.Fail(__LINE__, "x must occupy byte 0");
	}
	if ((PackLocalVoxelFaceCPU(0u, 1u, 0u, 0u) & 0xFF00u) != 0x100u) {
		ctx.Fail(__LINE__, "y must occupy byte 1");
	}
	if ((PackLocalVoxelFaceCPU(0u, 0u, 1u, 0u) & 0xFF0000u) != 0x10000u) {
		ctx.Fail(__LINE__, "z must occupy byte 2");
	}
	if ((PackLocalVoxelFaceCPU(0u, 0u, 0u, 1u) & 0xFF000000u) != 0x1000000u) {
		ctx.Fail(__LINE__, "faceIndex must occupy byte 3");
	}
}

void TestChunkIndex24BitLimit(TestContext &ctx)
{
	const uint32_t overflow = PackChunkIndexMaterialCPU(0x01000000u, 0u);
	if (overflow != 0u) {
		ctx.Fail(__LINE__, "chunkIndex bits above 24 must be masked off");
	}
}

} // namespace

int main()
{
	TestContext ctx{};
	TestPackLocalVoxelFaceKnownValues(ctx);
	TestPackQuadExtentsKnownValues(ctx);
	TestPackChunkIndexMaterialKnownValues(ctx);
	TestUnpackLocalVoxelFaceRoundTrip(ctx);
	TestUnpackQuadExtentsRoundTrip(ctx);
	TestUnpackChunkIndexMaterialRoundTrip(ctx);
	TestByteLayoutLocalVoxelFace(ctx);
	TestChunkIndex24BitLimit(ctx);
	if (ctx.failures > 0) {
		std::fprintf(stderr, "ProjectVPackedFaceEncodingTests: %d failure(s)\n", ctx.failures);
		return EXIT_FAILURE;
	}
	std::puts("ProjectVPackedFaceEncodingTests passed");
	return EXIT_SUCCESS;
}

#include "voxel/CpuGreedyMeshing.hpp"

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {

struct TestContext {
	int failures = 0;
	void Fail(int line, std::string_view msg)
	{
		std::fprintf(stderr, "FAIL line %d: %.*s\n", line, static_cast<int>(msg.size()), msg.data());
		++failures;
	}
};

using projectv::voxel::ShouldEmitVoxelFaceCPU;

void TestFullMaterialMatrix(TestContext &ctx)
{
	constexpr uint8_t kMaterials[] = {0u, 1u, 2u, 3u, 4u};
	constexpr size_t kCount = sizeof(kMaterials) / sizeof(kMaterials[0]);

	for (size_t mi = 0; mi < kCount; ++mi) {
		for (size_t ni = 0; ni < kCount; ++ni) {
			const uint8_t mat = kMaterials[mi];
			const uint8_t nbr = kMaterials[ni];
			const bool result = ShouldEmitVoxelFaceCPU(mat, nbr);

			bool expected = false;
			if (mat == 0u) {
				expected = false;
			} else if (mat >= 3u) {
				expected = nbr == 0u || nbr == 1u;
			} else if (mat == 2u) {
				expected = true;
			} else {
				expected = nbr == 0u;
			}

			if (result != expected) {
				ctx.Fail(__LINE__, "material matrix mismatch");
				std::fprintf(stderr, "  mat=%u nbr=%u got=%d exp=%d\n", mat, nbr, result, expected);
			}
		}
	}
}

void TestAirNeverEmits(TestContext &ctx)
{
	for (uint8_t nbr = 0u; nbr <= 4u; ++nbr) {
		if (ShouldEmitVoxelFaceCPU(0u, nbr)) {
			ctx.Fail(__LINE__, "Air must never emit a face");
		}
	}
}

void TestFluidAlwaysEmits(TestContext &ctx)
{
	for (uint8_t nbr = 0u; nbr <= 4u; ++nbr) {
		if (!ShouldEmitVoxelFaceCPU(2u, nbr)) {
			ctx.Fail(__LINE__, "Fluid must always emit a face regardless of neighbor");
		}
	}
}

void TestSolidEmitsTowardAirAndGlass(TestContext &ctx)
{
	for (uint8_t mat = 3u; mat <= 4u; ++mat) {
		if (!ShouldEmitVoxelFaceCPU(mat, 0u)) {
			ctx.Fail(__LINE__, "Solid must emit toward Air");
		}
		if (!ShouldEmitVoxelFaceCPU(mat, 1u)) {
			ctx.Fail(__LINE__, "Solid must emit toward Glass");
		}
		if (ShouldEmitVoxelFaceCPU(mat, 2u)) {
			ctx.Fail(__LINE__, "Solid must NOT emit toward Fluid");
		}
		if (ShouldEmitVoxelFaceCPU(mat, mat)) {
			ctx.Fail(__LINE__, "Solid must NOT emit toward same material");
		}
	}
}

void TestGlassEmitsOnlyTowardAir(TestContext &ctx)
{
	if (!ShouldEmitVoxelFaceCPU(1u, 0u)) {
		ctx.Fail(__LINE__, "Glass must emit toward Air");
	}
	for (uint8_t nbr = 1u; nbr <= 4u; ++nbr) {
		if (ShouldEmitVoxelFaceCPU(1u, nbr)) {
			ctx.Fail(__LINE__, "Glass must NOT emit toward non-Air");
		}
	}
}

void TestSolidVsSolidSameMaterialNoEmit(TestContext &ctx)
{
	if (ShouldEmitVoxelFaceCPU(3u, 3u)) {
		ctx.Fail(__LINE__, "FloorWhite vs FloorWhite must not emit");
	}
	if (ShouldEmitVoxelFaceCPU(3u, 4u)) {
		ctx.Fail(__LINE__, "FloorWhite vs FloorGray must not emit");
	}
	if (ShouldEmitVoxelFaceCPU(4u, 3u)) {
		ctx.Fail(__LINE__, "FloorGray vs FloorWhite must not emit");
	}
}

} // namespace

int main()
{
	TestContext ctx{};
	TestFullMaterialMatrix(ctx);
	TestAirNeverEmits(ctx);
	TestFluidAlwaysEmits(ctx);
	TestSolidEmitsTowardAirAndGlass(ctx);
	TestGlassEmitsOnlyTowardAir(ctx);
	TestSolidVsSolidSameMaterialNoEmit(ctx);
	if (ctx.failures > 0) {
		std::fprintf(stderr, "ProjectVShouldEmitVoxelFaceTests: %d failure(s)\n", ctx.failures);
		return EXIT_FAILURE;
	}
	std::puts("ProjectVShouldEmitVoxelFaceTests passed");
	return EXIT_SUCCESS;
}

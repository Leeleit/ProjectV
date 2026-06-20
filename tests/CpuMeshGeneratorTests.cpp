#include "voxel/CpuMeshGenerator.hpp"
#include "voxel/VoxelWorld.hpp"

#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>

namespace {

struct TestContext {
	int failures = 0;
	void Fail(const int line, const std::string_view message)
	{
		std::fprintf(stderr, "Test failure at line %d: %.*s\n", line, static_cast<int>(message.size()), message.data());
		++failures;
	}
};

void ExpectTrue(TestContext &context, bool condition, int line, std::string_view expr)
{
	if (!condition) {
		context.Fail(line, expr);
	}
}

void TestEmptyChunkGeneratesNoFaces(TestContext &context)
{
	std::vector<uint8_t> voxels(4 * 4 * 4, 0);
	projectv::voxel::CpuMeshInput input{};
	input.voxels = voxels.data();
	input.widthX = 4;
	input.heightY = 4;
	input.depthZ = 4;
	const auto faces = projectv::voxel::GenerateCpuChunkMeshXPositive(input);
	ExpectTrue(context, faces.empty(), __LINE__, "empty chunk -> no faces");
}

void TestSingleVoxelAtInterior(TestContext &context)
{
	std::vector<uint8_t> voxels(4 * 4 * 4, 0);
	voxels[1 + 4 * (1 + 4 * 1)] = 1;
	projectv::voxel::CpuMeshInput input{};
	input.voxels = voxels.data();
	input.widthX = 4;
	input.heightY = 4;
	input.depthZ = 4;
	const auto faces = projectv::voxel::GenerateCpuChunkMeshXPositive(input);
	ExpectTrue(context, faces.size() == 1, __LINE__, "single interior voxel -> 1 face");
}

void TestVoxelAtBoundaryEmitsFace(TestContext &context)
{
	std::vector<uint8_t> voxels(4 * 4 * 4, 0);
	voxels[3 + 4 * (1 + 4 * 1)] = 1;
	projectv::voxel::CpuMeshInput input{};
	input.voxels = voxels.data();
	input.widthX = 4;
	input.heightY = 4;
	input.depthZ = 4;
	const auto faces = projectv::voxel::GenerateCpuChunkMeshXPositive(input);
	ExpectTrue(context, faces.size() == 1, __LINE__, "boundary voxel -> 1 face (Air neighbor at OOB)");
}

void TestFilledChunkEmitsPerVoxelFace(TestContext &context)
{
	std::vector<uint8_t> voxels(4 * 4 * 4, 0);
	for (int z = 0; z < 4; ++z) {
		for (int y = 0; y < 4; ++y) {
			for (int x = 0; x < 4; ++x) {
				voxels[x + 4 * (y + 4 * z)] = 1;
			}
		}
	}
	projectv::voxel::CpuMeshInput input{};
	input.voxels = voxels.data();
	input.widthX = 4;
	input.heightY = 4;
	input.depthZ = 4;
	const auto faces = projectv::voxel::GenerateCpuChunkMeshXPositive(input);
	ExpectTrue(context, faces.size() == 4 * 4, __LINE__, "filled chunk -> 4 * 4 faces (X+ side of rightmost column)");
}

} // namespace

int main()
{
	TestContext context{};
	TestEmptyChunkGeneratesNoFaces(context);
	TestSingleVoxelAtInterior(context);
	TestVoxelAtBoundaryEmitsFace(context);
	TestFilledChunkEmitsPerVoxelFace(context);
	if (context.failures != 0) {
		return EXIT_FAILURE;
	}
	std::puts("ProjectVCpuMeshGeneratorTests passed");
	return EXIT_SUCCESS;
}

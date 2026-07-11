#include "voxel/CpuMeshGenerator.hpp"

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

void ExpectTrue(TestContext &context, const bool condition, const int line, const std::string_view expr)
{
	if (!condition) {
		context.Fail(line, expr);
	}
}

void TestEmptyChunkGeneratesNoFaces(TestContext &context)
{
	const std::vector<uint8_t> voxels(static_cast<size_t>(4) * 4 * 4, 0);
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
	std::vector<uint8_t> voxels(static_cast<size_t>(4) * 4 * 4, 0);
	voxels[1 + static_cast<size_t>(4) * (1 + static_cast<size_t>(4) * 1)] = 1;
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
	std::vector<uint8_t> voxels(static_cast<size_t>(4) * 4 * 4, 0);
	voxels[3 + static_cast<size_t>(4) * (1 + static_cast<size_t>(4) * 1)] = 1;
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
				voxels[x + static_cast<size_t>(4) * (y + static_cast<size_t>(4) * z)] = 1;
			}
		}
	}
	projectv::voxel::CpuMeshInput input{};
	input.voxels = voxels.data();
	input.widthX = 4;
	input.heightY = 4;
	input.depthZ = 4;
	const auto faces = projectv::voxel::GenerateCpuChunkMeshXPositive(input);
	ExpectTrue(context, faces.size() == static_cast<size_t>(4) * 4, __LINE__, "filled chunk -> 4 * 4 faces (X+ side of rightmost column)");
}

void TestNullInputReturnsEmpty(TestContext &context)
{
	projectv::voxel::CpuMeshInput input{};
	input.voxels = nullptr;
	input.widthX = 4;
	input.heightY = 4;
	input.depthZ = 4;
	const auto faces = projectv::voxel::GenerateCpuChunkMeshXPositive(input);
	ExpectTrue(context, faces.empty(), __LINE__, "null voxels pointer -> empty result");
}

void TestZeroDimensionsReturnEmpty(TestContext &context)
{
	const std::vector<uint8_t> voxels(1, 1);
	for (int i = 0; i < 3; ++i) {
		projectv::voxel::CpuMeshInput input{};
		input.voxels = voxels.data();
		input.widthX = i == 0 ? 0 : 1;
		input.heightY = i == 1 ? 0 : 1;
		input.depthZ = i == 2 ? 0 : 1;
		const auto faces = projectv::voxel::GenerateCpuChunkMeshXPositive(input);
		ExpectTrue(context, faces.empty(), __LINE__, "zero dimension -> empty result");
	}
}

void TestOversizedExtentReturnsEmpty(TestContext &context)
{
	const std::vector<uint8_t> voxels(1, 1);
	projectv::voxel::CpuMeshInput input{};
	input.voxels = voxels.data();
	input.widthX = 65;
	input.heightY = 1;
	input.depthZ = 1;
	const auto faces = projectv::voxel::GenerateCpuChunkMeshXPositive(input);
	ExpectTrue(context, faces.empty(), __LINE__, "extent > 64 -> empty result (kMaxExtent guard)");
}

void TestDifferentMaterialsBothEmit(TestContext &context)
{
	std::vector<uint8_t> voxels(static_cast<size_t>(2) * 1 * 1, 0);
	voxels[0] = 3;
	voxels[1] = 4;
	projectv::voxel::CpuMeshInput input{};
	input.voxels = voxels.data();
	input.widthX = 2;
	input.heightY = 1;
	input.depthZ = 1;
	const auto faces = projectv::voxel::GenerateCpuChunkMeshXPositive(input);
	ExpectTrue(context, faces.size() == 2, __LINE__, "two different materials -> 2 faces (current != neighbor)");
}

void TestSameMaterialDoesNotEmitInterior(TestContext &context)
{
	std::vector<uint8_t> voxels(static_cast<size_t>(2) * 1 * 1, 0);
	voxels[0] = 3;
	voxels[1] = 3;
	projectv::voxel::CpuMeshInput input{};
	input.voxels = voxels.data();
	input.widthX = 2;
	input.heightY = 1;
	input.depthZ = 1;
	const auto faces = projectv::voxel::GenerateCpuChunkMeshXPositive(input);
	ExpectTrue(context, faces.size() == 1, __LINE__, "same material pair -> 1 face (only rightmost boundary)");
}

void TestSingleVoxelAt1x1x1(TestContext &context)
{
	const std::vector<uint8_t> voxels(1, 3);
	projectv::voxel::CpuMeshInput input{};
	input.voxels = voxels.data();
	input.widthX = 1;
	input.heightY = 1;
	input.depthZ = 1;
	const auto faces = projectv::voxel::GenerateCpuChunkMeshXPositive(input);
	ExpectTrue(context, faces.size() == 1, __LINE__, "1x1x1 voxel -> 1 face (OOB = Air)");
}

void TestMaterialPreservation(TestContext &context)
{
	std::vector<uint8_t> voxels(static_cast<size_t>(4) * 1 * 1, 0);
	voxels[0] = 3;
	voxels[3] = 4;
	projectv::voxel::CpuMeshInput input{};
	input.voxels = voxels.data();
	input.widthX = 4;
	input.heightY = 1;
	input.depthZ = 1;
	const auto faces = projectv::voxel::GenerateCpuChunkMeshXPositive(input);
	ExpectTrue(context, faces.size() == 2, __LINE__, "two isolated voxels -> 2 faces");
	if (faces.size() == 2) {
		ExpectTrue(context, faces[0].chunkIndexMaterial == 3u, __LINE__, "first face material must be 3");
		ExpectTrue(context, faces[1].chunkIndexMaterial == 4u, __LINE__, "second face material must be 4");
	}
}

} // namespace

int main()
{
	TestContext context{};
	TestEmptyChunkGeneratesNoFaces(context);
	TestSingleVoxelAtInterior(context);
	TestVoxelAtBoundaryEmitsFace(context);
	TestFilledChunkEmitsPerVoxelFace(context);
	TestNullInputReturnsEmpty(context);
	TestZeroDimensionsReturnEmpty(context);
	TestOversizedExtentReturnsEmpty(context);
	TestDifferentMaterialsBothEmit(context);
	TestSameMaterialDoesNotEmitInterior(context);
	TestSingleVoxelAt1x1x1(context);
	TestMaterialPreservation(context);
	if (context.failures != 0) {
		return EXIT_FAILURE;
	}
	std::puts("ProjectVCpuMeshGeneratorTests passed");
	return EXIT_SUCCESS;
}

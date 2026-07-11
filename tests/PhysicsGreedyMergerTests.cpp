#include "physics/GreedyPhysicsMerger.hpp"
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

// noinspection DfaConstantParameter
VoxelWorld MakeTestWorld(const int width, const int height, const int depth)
{
	constexpr int chunkSize = 4;
	VoxelWorld world{};
	world.min = {0, 0, 0};
	world.maxExclusive = {width, height, depth};
	world.width = width;
	world.height = height;
	world.depth = depth;
	world.chunkSize = chunkSize;
	world.chunkCountX = (width + chunkSize - 1) / chunkSize;
	world.chunkCountY = (height + chunkSize - 1) / chunkSize;
	world.chunkCountZ = (depth + chunkSize - 1) / chunkSize;
	world.sparseStorage.Reset(width, height, depth);
	for (int z = 0; z < depth; ++z) {
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::Air, nullptr);
			}
		}
	}
	world.chunks.assign(
		static_cast<size_t>(world.chunkCountX) *
		static_cast<size_t>(world.chunkCountY) *
		static_cast<size_t>(world.chunkCountZ),
		VoxelChunk{});
	return world;
}

uint32_t CountSolidVoxels(const VoxelWorld &world)
{
	uint32_t count = 0u;
	for (int z = 0; z < world.maxExclusive.z; ++z) {
		for (int y = 0; y < world.maxExclusive.y; ++y) {
			for (int x = 0; x < world.maxExclusive.x; ++x) {
				const VoxelMaterial m = GetVoxelMaterial(world, {x, y, z});
				if (m == VoxelMaterial::Glass ||
					m == VoxelMaterial::FloorWhite ||
					m == VoxelMaterial::FloorGray) {
					++count;
				}
			}
		}
	}
	return count;
}

uint64_t SumMergedBoxVolumes(const std::vector<projectv::physics::MergedVoxelBox> &boxes)
{
	uint64_t total = 0u;
	for (const auto &[minX, minY, minZ, maxX, maxY, maxZ] : boxes) {
		const uint64_t spanX = static_cast<uint64_t>(maxX - minX);
		const uint64_t spanY = static_cast<uint64_t>(maxY - minY);
		const uint64_t spanZ = static_cast<uint64_t>(maxZ - minZ);
		total += spanX * spanY * spanZ;
	}
	return total;
}

void TestEmptyWorldProducesZeroBoxes(TestContext &context)
{
	const VoxelWorld world = MakeTestWorld(8, 8, 8);
	std::vector<projectv::physics::MergedVoxelBox> boxes;
	const uint32_t count = projectv::physics::GreedyMergeSolidVoxelsInBounds(
		world,
		Int3{0, 0, 0},
		Int3{8, 8, 8},
		boxes);
	if (count != 0u) {
		context.Fail(__LINE__, "Empty world must produce zero merged boxes");
	}
	if (!boxes.empty()) {
		context.Fail(__LINE__, "Empty world must clear output vector");
	}
}

void TestSingleVoxelEmitsSingleUnitBox(TestContext &context)
{
	VoxelWorld world = MakeTestWorld(8, 8, 8);
	SetVoxelMaterial(world, {3, 3, 3}, VoxelMaterial::FloorWhite, nullptr);
	std::vector<projectv::physics::MergedVoxelBox> boxes;
	const uint32_t count = projectv::physics::GreedyMergeSolidVoxelsInBounds(
		world,
		Int3{0, 0, 0},
		Int3{8, 8, 8},
		boxes);
	if (count != 1u) {
		context.Fail(__LINE__, "Single voxel must produce exactly one merged box");
	}
	if (!boxes.empty() && (boxes[0].minX != 3 || boxes[0].maxX != 4 ||
		boxes[0].minY != 3 || boxes[0].maxY != 4 ||
		boxes[0].minZ != 3 || boxes[0].maxZ != 4)) {
		context.Fail(__LINE__, "Single voxel box must be unit extents at the source voxel");
	}
}

void TestFullChunkEmitsSingleBox(TestContext &context)
{
	VoxelWorld world = MakeTestWorld(8, 8, 8);
	for (int z = 0; z < 8; ++z) {
		for (int y = 0; y < 8; ++y) {
			for (int x = 0; x < 8; ++x) {
				SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::FloorWhite, nullptr);
			}
		}
	}
	std::vector<projectv::physics::MergedVoxelBox> boxes;
	const uint32_t count = projectv::physics::GreedyMergeSolidVoxelsInBounds(
		world,
		Int3{0, 0, 0},
		Int3{8, 8, 8},
		boxes);
	if (count != 1u) {
		context.Fail(__LINE__, "Fully solid chunk must merge into one box");
	}
	if (!boxes.empty() && (boxes[0].minX != 0 || boxes[0].maxX != 8 ||
		boxes[0].minY != 0 || boxes[0].maxY != 8 ||
		boxes[0].minZ != 0 || boxes[0].maxZ != 8)) {
		context.Fail(__LINE__, "Full chunk box must span the entire chunk bounds");
	}
}

void TestVolumePreservation(TestContext &context)
{
	VoxelWorld world = MakeTestWorld(8, 8, 8);
	for (int z = 0; z < 4; ++z) {
		for (int y = 0; y < 8; ++y) {
			for (int x = 0; x < 8; ++x) {
				SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::FloorWhite, nullptr);
			}
		}
	}
	std::vector<projectv::physics::MergedVoxelBox> boxes;
	projectv::physics::GreedyMergeSolidVoxelsInBounds(
		world,
		Int3{0, 0, 0},
		Int3{8, 8, 8},
		boxes);
	const uint32_t solidCount = CountSolidVoxels(world);
	const uint64_t mergedVolume = SumMergedBoxVolumes(boxes);
	if (mergedVolume != solidCount) {
		std::fprintf(
			stderr,
			"Test failure at line %d: merged volume %llu != solid voxel count %u\n",
			__LINE__,
			static_cast<unsigned long long>(mergedVolume),
			solidCount);
		++context.failures;
	}
}

void TestMixedHalfChunkHasReduction(TestContext &context)
{
	VoxelWorld world = MakeTestWorld(8, 8, 8);
	for (int y = 0; y < 8; ++y) {
		for (int x = 0; x < 8; ++x) {
			SetVoxelMaterial(world, {x, y, 0}, VoxelMaterial::FloorWhite, nullptr);
		}
	}
	for (int y = 0; y < 8; ++y) {
		for (int x = 0; x < 4; ++x) {
			SetVoxelMaterial(world, {x, y, 1}, VoxelMaterial::FloorWhite, nullptr);
		}
	}
	std::vector<projectv::physics::MergedVoxelBox> boxes;
	projectv::physics::GreedyMergeSolidVoxelsInBounds(
		world,
		Int3{0, 0, 0},
		Int3{8, 8, 8},
		boxes);
	const uint32_t solidCount = CountSolidVoxels(world);
	if (solidCount == 0u) {
		context.Fail(__LINE__, "Mixed scene must have at least one solid voxel");
		return;
	}
	if (boxes.size() >= solidCount) {
		std::fprintf(
			stderr,
			"Test failure at line %d: greedy merge did not reduce shape count (%zu boxes vs %u voxels)\n",
			__LINE__,
			boxes.size(),
			solidCount);
		++context.failures;
	}
}

void TestFluidAndAirAreIgnored(TestContext &context)
{
	VoxelWorld world = MakeTestWorld(8, 8, 8);
	for (int z = 0; z < 8; ++z) {
		for (int y = 0; y < 8; ++y) {
			for (int x = 0; x < 8; ++x) {
				SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::Fluid, nullptr);
			}
		}
	}
	std::vector<projectv::physics::MergedVoxelBox> boxes;
	projectv::physics::GreedyMergeSolidVoxelsInBounds(
		world,
		Int3{0, 0, 0},
		Int3{8, 8, 8},
		boxes);
	if (!boxes.empty()) {
		context.Fail(__LINE__, "Fluid + Air scene must produce zero merged boxes (no solid materials)");
	}
}

void TestBoundsClamp(TestContext &context)
{
	VoxelWorld world = MakeTestWorld(8, 8, 8);
	for (int z = 0; z < 8; ++z) {
		for (int y = 0; y < 8; ++y) {
			for (int x = 0; x < 8; ++x) {
				SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::FloorWhite, nullptr);
			}
		}
	}
	std::vector<projectv::physics::MergedVoxelBox> boxes;
	projectv::physics::GreedyMergeSolidVoxelsInBounds(
		world,
		Int3{-4, -4, -4},
		Int3{12, 12, 12},
		boxes);
	if (boxes.size() != 1u) {
		context.Fail(__LINE__, "Oversized bounds must clamp to world extents and still merge into 1 box");
	}
}

void TestAllGlassMergesToSingleBox(TestContext &context)
{
	VoxelWorld world = MakeTestWorld(8, 8, 8);
	for (int z = 0; z < 8; ++z) {
		for (int y = 0; y < 8; ++y) {
			for (int x = 0; x < 8; ++x) {
				SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::Glass, nullptr);
			}
		}
	}
	std::vector<projectv::physics::MergedVoxelBox> boxes;
	projectv::physics::GreedyMergeSolidVoxelsInBounds(
		world, Int3{0, 0, 0}, Int3{8, 8, 8}, boxes);
	if (boxes.size() != 1u) {
		context.Fail(__LINE__, "All-Glass world must merge to single box (Glass is solid for physics)");
	}
}

void TestMixedSolidMaterialsMergeTogether(TestContext &context)
{
	VoxelWorld world = MakeTestWorld(8, 8, 8);
	for (int y = 0; y < 8; ++y) {
		for (int x = 0; x < 4; ++x) {
			SetVoxelMaterial(world, {x, y, 0}, VoxelMaterial::FloorWhite, nullptr);
		}
		for (int x = 4; x < 8; ++x) {
			SetVoxelMaterial(world, {x, y, 0}, VoxelMaterial::FloorGray, nullptr);
		}
	}
	std::vector<projectv::physics::MergedVoxelBox> boxes;
	projectv::physics::GreedyMergeSolidVoxelsInBounds(
		world, Int3{0, 0, 0}, Int3{8, 8, 8}, boxes);
	if (boxes.size() != 1u) {
		context.Fail(__LINE__, "Mixed FloorWhite+FloorGray slab must merge to 1 box (physics ignores material type)");
		std::fprintf(stderr, "  got %zu boxes\n", boxes.size());
	}
}

void TestDisjointRegionsProduceMultipleBoxes(TestContext &context)
{
	VoxelWorld world = MakeTestWorld(8, 8, 8);
	SetVoxelMaterial(world, {0, 0, 0}, VoxelMaterial::FloorWhite, nullptr);
	SetVoxelMaterial(world, {7, 7, 7}, VoxelMaterial::FloorWhite, nullptr);
	std::vector<projectv::physics::MergedVoxelBox> boxes;
	projectv::physics::GreedyMergeSolidVoxelsInBounds(
		world, Int3{0, 0, 0}, Int3{8, 8, 8}, boxes);
	if (boxes.size() != 2u) {
		context.Fail(__LINE__, "Two disjoint voxels must produce 2 boxes");
	}
}

void TestThinColumnMerges(TestContext &context)
{
	VoxelWorld world = MakeTestWorld(8, 8, 8);
	for (int y = 0; y < 8; ++y) {
		SetVoxelMaterial(world, {0, y, 0}, VoxelMaterial::FloorWhite, nullptr);
	}
	std::vector<projectv::physics::MergedVoxelBox> boxes;
	projectv::physics::GreedyMergeSolidVoxelsInBounds(
		world, Int3{0, 0, 0}, Int3{8, 8, 8}, boxes);
	if (boxes.size() != 1u) {
		context.Fail(__LINE__, "Thin Y-column must merge to 1 box");
	}
	if (!boxes.empty()) {
		const auto &[minX, minY, minZ, maxX, maxY, maxZ] = boxes[0];
		if (minY != 0 || maxY != 8 || minX != 0 || maxX != 1 || minZ != 0 || maxZ != 1) {
			context.Fail(__LINE__, "Column box extents must be (0,0,0)-(1,8,1)");
		}
	}
}

void TestInteriorHoleProducesCorrectBoxes(TestContext &context)
{
	VoxelWorld world = MakeTestWorld(8, 8, 8);
	for (int z = 0; z < 8; ++z) {
		for (int y = 0; y < 8; ++y) {
			for (int x = 0; x < 8; ++x) {
				if (x > 0 && x < 7 && y > 0 && y < 7 && z > 0 && z < 7) {
					continue;
				}
				SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::FloorWhite, nullptr);
			}
		}
	}
	std::vector<projectv::physics::MergedVoxelBox> boxes;
	projectv::physics::GreedyMergeSolidVoxelsInBounds(
		world, Int3{0, 0, 0}, Int3{8, 8, 8}, boxes);
	const uint32_t solidCount = CountSolidVoxels(world);
	const uint64_t mergedVolume = SumMergedBoxVolumes(boxes);
	if (mergedVolume != solidCount) {
		std::fprintf(stderr, "Test failure at line %d: shell volume %llu != solid count %u\n",
					 __LINE__, static_cast<unsigned long long>(mergedVolume), solidCount);
		++context.failures;
	}
}

void TestZeroSizeBoundsReturnZero(TestContext &context)
{
	VoxelWorld world = MakeTestWorld(8, 8, 8);
	SetVoxelMaterial(world, {0, 0, 0}, VoxelMaterial::FloorWhite, nullptr);
	std::vector<projectv::physics::MergedVoxelBox> boxes;
	const uint32_t count = projectv::physics::GreedyMergeSolidVoxelsInBounds(
		world, Int3{4, 4, 4}, Int3{4, 4, 4}, boxes);
	if (count != 0u || !boxes.empty()) {
		context.Fail(__LINE__, "Zero-size bounds (min == max) must return 0");
	}
}

void TestInvertedBoundsReturnZero(TestContext &context)
{
	VoxelWorld world = MakeTestWorld(8, 8, 8);
	for (int z = 0; z < 8; ++z) {
		for (int y = 0; y < 8; ++y) {
			for (int x = 0; x < 8; ++x) {
				SetVoxelMaterial(world, {x, y, z}, VoxelMaterial::FloorWhite, nullptr);
			}
		}
	}
	std::vector<projectv::physics::MergedVoxelBox> boxes;
	const uint32_t count = projectv::physics::GreedyMergeSolidVoxelsInBounds(
		world, Int3{6, 6, 6}, Int3{2, 2, 2}, boxes);
	if (count != 0u || !boxes.empty()) {
		context.Fail(__LINE__, "Inverted bounds (max < min) must return 0");
	}
}

}  // namespace

int main()
{
	TestContext context{};
	TestEmptyWorldProducesZeroBoxes(context);
	TestSingleVoxelEmitsSingleUnitBox(context);
	TestFullChunkEmitsSingleBox(context);
	TestVolumePreservation(context);
	TestMixedHalfChunkHasReduction(context);
	TestFluidAndAirAreIgnored(context);
	TestBoundsClamp(context);
	TestAllGlassMergesToSingleBox(context);
	TestMixedSolidMaterialsMergeTogether(context);
	TestDisjointRegionsProduceMultipleBoxes(context);
	TestThinColumnMerges(context);
	TestInteriorHoleProducesCorrectBoxes(context);
	TestZeroSizeBoundsReturnZero(context);
	TestInvertedBoundsReturnZero(context);

	if (context.failures > 0) {
		std::fprintf(stderr, "ProjectVPhysicsGreedyMergerTests: %d failure(s)\n", context.failures);
		return 1;
	}
	std::puts("ProjectVPhysicsGreedyMergerTests passed");
	return 0;
}

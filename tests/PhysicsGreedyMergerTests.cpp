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
	for (const projectv::physics::MergedVoxelBox &box : boxes) {
		const uint64_t spanX = static_cast<uint64_t>(box.maxX - box.minX);
		const uint64_t spanY = static_cast<uint64_t>(box.maxY - box.minY);
		const uint64_t spanZ = static_cast<uint64_t>(box.maxZ - box.minZ);
		total += spanX * spanY * spanZ;
	}
	return total;
}

void TestEmptyWorldProducesZeroBoxes(TestContext &context)
{
	VoxelWorld world = MakeTestWorld(8, 8, 8);
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

	if (context.failures > 0) {
		std::fprintf(stderr, "ProjectVPhysicsGreedyMergerTests: %d failure(s)\n", context.failures);
		return 1;
	}
	std::puts("ProjectVPhysicsGreedyMergerTests passed");
	return 0;
}

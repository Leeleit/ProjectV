#include "render/LodDownsampleGpuConsume.hpp"

#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "core/Types.hpp"

namespace {

struct TestContext {
	int failures = 0;
	void Fail(const int line, const std::string_view message)
	{
		std::fprintf(stderr, "Test failure at line %d: %.*s\n", line, static_cast<int>(message.size()), message.data());
		++failures;
	}
};

void TestLodGpuConsumeEnvDefaultOff(TestContext &context)
{
	unsetenv("PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME");
	if (projectv::render::IsLodDownsampledGpuConsumeEnabled()) {
		context.Fail(__LINE__, "PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME unset -> false");
	}
}

void TestLodGpuConsumeEnvOn(TestContext &context)
{
	setenv("PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME", "ON", 1);
	if (!projectv::render::IsLodDownsampledGpuConsumeEnabled()) {
		context.Fail(__LINE__, "PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME=ON -> true");
	}
	unsetenv("PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME");
}

void TestLodGpuConsumeEnvZeroIsOff(TestContext &context)
{
	setenv("PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME", "0", 1);
	if (projectv::render::IsLodDownsampledGpuConsumeEnabled()) {
		context.Fail(__LINE__, "PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME=0 -> false");
	}
	unsetenv("PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME");
}

void TestComputeLodDownsampledPayloadBytesScalesWithChunks(TestContext &context)
{
	const uint32_t small = projectv::render::ComputeLodDownsampledVoxelPayloadBytes(8u, 8u);
	const uint32_t large = projectv::render::ComputeLodDownsampledVoxelPayloadBytes(64u, 8u);
	if (large <= small) {
		context.Fail(__LINE__, "LOD payload bytes must grow with chunk count");
	}
}

void TestComputeChunkLodLevelsCapacityAtLeastOne(TestContext &context)
{
	const uint32_t zero = projectv::render::ComputeChunkLodLevelsCapacity(0u);
	if (zero != 1u) {
		context.Fail(__LINE__, "ComputeChunkLodLevelsCapacity(0) must be at least 1 (capacity floor)");
	}
	const uint32_t many = projectv::render::ComputeChunkLodLevelsCapacity(1234u);
	if (many != 1234u) {
		context.Fail(__LINE__, "ComputeChunkLodLevelsCapacity(1234) must equal 1234");
	}
}

void TestRefreshLodDownsampledBuffersRejectsNullContext(TestContext &context)
{
	RenderState render{};
	VoxelWorld world{};
	if (projectv::render::RefreshLodDownsampledBuffers(nullptr, &render, world)) {
		context.Fail(__LINE__, "RefreshLodDownsampledBuffers(nullptr context) must return false");
	}
}

}  // namespace

int main()
{
	TestContext context{};
	TestLodGpuConsumeEnvDefaultOff(context);
	TestLodGpuConsumeEnvOn(context);
	TestLodGpuConsumeEnvZeroIsOff(context);
	TestComputeLodDownsampledPayloadBytesScalesWithChunks(context);
	TestComputeChunkLodLevelsCapacityAtLeastOne(context);
	TestRefreshLodDownsampledBuffersRejectsNullContext(context);

	if (context.failures > 0) {
		std::fprintf(stderr, "ProjectVLodDownsampleGpuConsumeTests: %d failure(s)\n", context.failures);
		return 1;
	}
	std::puts("ProjectVLodDownsampleGpuConsumeTests passed");
	return 0;
}

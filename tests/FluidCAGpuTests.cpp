#include "render/vulkan/VulkanFluidCaPipeline.hpp"

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {

struct TestContext {
	int failures = 0;
	void Fail(const int line, const std::string_view message)
	{
		std::fprintf(stderr, "Test failure at line %d: %.*s\n", line, static_cast<int>(message.size()), message.data());
		++failures;
	}
};

[[maybe_unused]] void ExpectFalse(TestContext &context, const bool condition, const int line, const std::string_view expr)
{
	if (condition) {
		context.Fail(line, expr);
	}
}

void ExpectEqualUInt(TestContext &context, const uint32_t expected, const uint32_t actual, const int line, const std::string_view expr)
{
	if (expected != actual) {
		std::fprintf(stderr, "Test failure at line %d: %.*s (expected %u, got %u)\n", line, static_cast<int>(expr.size()), expr.data(), expected, actual);
		++context.failures;
	}
}

void TestFluidCaPipelineRequestedDefaultOn(TestContext &context)
{
	unsetenv("PROJECTV_FLUID_CA_GPU");
	const bool requested = projectv::render::IsFluidCaGpuPipelineRequested();
	if (!requested) {
		context.Fail(__LINE__, "PROJECTV_FLUID_CA_GPU unset -> true (per TODO Stage 3.1 Step 2 default flip)");
	}
}

void TestFluidCaPipelineRequestedOptOut(TestContext &context)
{
	setenv("PROJECTV_FLUID_CA_GPU", "0", 1);
	const bool requested = projectv::render::IsFluidCaGpuPipelineRequested();
	unsetenv("PROJECTV_FLUID_CA_GPU");
	if (requested) {
		context.Fail(__LINE__, "PROJECTV_FLUID_CA_GPU=0 -> false (opt-out)");
	}
}

void TestFluidCaPipelineRequestedExplicit(TestContext &context)
{
	setenv("PROJECTV_FLUID_CA_GPU", "1", 1);
	const bool requested = projectv::render::IsFluidCaGpuPipelineRequested();
	unsetenv("PROJECTV_FLUID_CA_GPU");
	if (!requested) {
		context.Fail(__LINE__, "PROJECTV_FLUID_CA_GPU=1 -> true");
	}
}

void TestFluidCaPushConstantsSize(TestContext &context)
{
	(void)context;
	static_assert(sizeof(projectv::render::FluidCaPushConstants) == 48u, "FluidCaPushConstants size expected 48 bytes");
}

void TestFluidCaGpuFrameStatsSize(TestContext &context)
{
	(void)context;
	static_assert(sizeof(projectv::render::FluidCaGpuFrameStats) == 16u, "FluidCaGpuFrameStats size expected 16 bytes");
}

void TestFluidCaPushConstantsPropagation(TestContext &context)
{
	projectv::render::FluidCaPushConstants pc{};
	pc.chunkDimensions = {8u, 8u, 8u, 0u};
	pc.chunkCountAndFlags = {42u, 0u, 0u, 0u};
	pc.fluidTickInterval = 0.05f;
	ExpectEqualUInt(context, 8u, pc.chunkDimensions[0], __LINE__, "chunkDimensions[0] = 8");
	ExpectEqualUInt(context, 42u, pc.chunkCountAndFlags[0], __LINE__, "chunkCountAndFlags[0] = active chunks");
}

}  // namespace

int main()
{
	TestContext context{};
	TestFluidCaPipelineRequestedDefaultOn(context);
	TestFluidCaPipelineRequestedOptOut(context);
	TestFluidCaPipelineRequestedExplicit(context);
	TestFluidCaPushConstantsSize(context);
	TestFluidCaGpuFrameStatsSize(context);
	TestFluidCaPushConstantsPropagation(context);

	if (context.failures > 0) {
		std::fprintf(stderr, "ProjectVFluidCAGpuTests: %d failure(s)\n", context.failures);
		return 1;
	}
	return 0;
}

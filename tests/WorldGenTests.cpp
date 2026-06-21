#include "render/vulkan/VulkanWorldGenPipeline.hpp"

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {

struct WorldGenTestContext {
	int failures = 0;
	void Fail(const int line, const std::string_view message)
	{
		std::fprintf(stderr, "Test failure at line %d: %.*s\n", line, static_cast<int>(message.size()), message.data());
		++failures;
	}
};

void TestEnvGateDefault(WorldGenTestContext &test)
{
	(void)std::getenv("PROJECTV_WORLD_GEN_GPU");
	if (!projectv::render::IsWorldGenGpuPipelineRequested()) {
		test.Fail(__LINE__, "Default env gate must enable world gen pipeline");
	}
}

void TestEnvGateOff(WorldGenTestContext &test)
{
	const int setenvResult = setenv("PROJECTV_WORLD_GEN_GPU", "OFF", 1);
	if (setenvResult != 0) {
		test.Fail(__LINE__, "setenv failed");
		return;
	}
	if (projectv::render::IsWorldGenGpuPipelineRequested()) {
		test.Fail(__LINE__, "env=OFF must disable world gen pipeline");
	}
	const int unsetResult = unsetenv("PROJECTV_WORLD_GEN_GPU");
	(void)unsetResult;
}

void TestEnvGateOn(WorldGenTestContext &test)
{
	const int setenvResult = setenv("PROJECTV_WORLD_GEN_GPU", "ON", 1);
	if (setenvResult != 0) {
		test.Fail(__LINE__, "setenv failed");
		return;
	}
	if (!projectv::render::IsWorldGenGpuPipelineRequested()) {
		test.Fail(__LINE__, "env=ON must enable world gen pipeline");
	}
	const int unsetResult = unsetenv("PROJECTV_WORLD_GEN_GPU");
	(void)unsetResult;
}

}  // namespace

int main()
{
	WorldGenTestContext test{};
	TestEnvGateDefault(test);
	TestEnvGateOff(test);
	TestEnvGateOn(test);

	if (test.failures > 0) {
		std::fprintf(stderr, "ProjectVWorldGenTests: %d failure(s)\n", test.failures);
		return 1;
	}
	std::puts("ProjectVWorldGenTests passed");
	return 0;
}

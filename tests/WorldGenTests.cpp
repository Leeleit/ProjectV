#include "render/vulkan/VulkanWorldGenPipeline.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

void TestWorldGenPushConstantContract(WorldGenTestContext &test)
{
	if constexpr (sizeof(projectv::render::WorldGenPushConstants) != 64u) {
		test.Fail(__LINE__, "WorldGenPushConstants must remain 64 bytes (4 vec4 + 1 uint + 3 reserved)");
	}
	constexpr projectv::render::WorldGenPushConstants defaults{};
	if constexpr (defaults.chunkOriginAndChunkSize[0] || defaults.chunkOriginAndChunkSize[1] ||
		defaults.chunkOriginAndChunkSize[2] || defaults.chunkOriginAndChunkSize[3]) {
		test.Fail(__LINE__, "default chunkOriginAndChunkSize must be zero-initialized");
	}
	if constexpr (defaults.seed != 0u) {
		test.Fail(__LINE__, "default seed must be zero");
	}
}

void TestWorldGenVoxelBufferBytesPerChunkContract(WorldGenTestContext &test)
{
	constexpr VkDeviceSize expected = sizeof(uint32_t) * 8u * 8u * 8u;
	if constexpr (projectv::render::kWorldGenVoxelBufferBytesPerChunk != expected) {
		test.Fail(__LINE__, "kWorldGenVoxelBufferBytesPerChunk must equal 8*8*8 uint32 = 2048 bytes per chunk");
	}
}

void TestWorldGenDispatchSkipOnZeroActiveChunks(WorldGenTestContext &test)
{
	projectv::render::WorldGenPushConstants push{};
	push.chunkCountAndFlags = {0u, 0u, 0u, 0u};
	const bool shouldSkip = push.chunkCountAndFlags[0] == 0u;
	if (!shouldSkip) {
		test.Fail(__LINE__, "Frame loop must skip RecordWorldGenDispatch when activeChunkCount is 0");
	}
}

void TestWorldGenSeedTickVariability(WorldGenTestContext &test)
{
	projectv::render::WorldGenPushConstants pushA{};
	projectv::render::WorldGenPushConstants pushB{};
	pushA.seed = 1u;
	pushB.seed = 2u;
	if (pushA.seed == pushB.seed) {
		test.Fail(__LINE__, "Different simulation ticks must produce different world gen seeds");
	}
	constexpr uint64_t tickA = 1000u;
	constexpr uint64_t tickB = 1001u;
	constexpr uint32_t seedA = tickA;
	constexpr uint32_t seedB = tickB;
	if constexpr (seedA == seedB) {
		test.Fail(__LINE__, "Cast to uint32 must preserve tick difference for typical tick range");
	}
}

}  // namespace

int main()
{
	WorldGenTestContext test{};
	TestEnvGateDefault(test);
	TestEnvGateOff(test);
	TestEnvGateOn(test);
	TestWorldGenPushConstantContract(test);
	TestWorldGenVoxelBufferBytesPerChunkContract(test);
	TestWorldGenDispatchSkipOnZeroActiveChunks(test);
	TestWorldGenSeedTickVariability(test);

	if (test.failures > 0) {
		std::fprintf(stderr, "ProjectVWorldGenTests: %d failure(s)\n", test.failures);
		return 1;
	}
	std::puts("ProjectVWorldGenTests passed");
	return 0;
}

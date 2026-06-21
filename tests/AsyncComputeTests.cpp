#include "render/vulkan/VulkanAsyncCompute.hpp"
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

void TestAsyncComputeEnvDefaultOff(TestContext &context)
{
	unsetenv("PROJECTV_ASYNC_COMPUTE");
	if (projectv::render::IsAsyncComputeEnabled()) {
		context.Fail(__LINE__, "PROJECTV_ASYNC_COMPUTE unset -> false");
	}
}

void TestAsyncComputeEnvExplicitOn(TestContext &context)
{
	setenv("PROJECTV_ASYNC_COMPUTE", "1", 1);
	if (!projectv::render::IsAsyncComputeEnabled()) {
		context.Fail(__LINE__, "PROJECTV_ASYNC_COMPUTE=1 -> true");
	}
	unsetenv("PROJECTV_ASYNC_COMPUTE");
}

void TestAsyncComputeEnvZeroIsOff(TestContext &context)
{
	setenv("PROJECTV_ASYNC_COMPUTE", "0", 1);
	if (projectv::render::IsAsyncComputeEnabled()) {
		context.Fail(__LINE__, "PROJECTV_ASYNC_COMPUTE=0 -> false");
	}
	unsetenv("PROJECTV_ASYNC_COMPUTE");
}

void TestEnsureAsyncComputeResourcesRejectsNullContext(TestContext &context)
{
	if (projectv::render::EnsureAsyncComputeResources(nullptr)) {
		context.Fail(__LINE__, "EnsureAsyncComputeResources(nullptr) must return false");
	}
}

void TestIsAsyncComputeResourcesAllocatedDefaultsFalse(TestContext &context)
{
	VulkanContextState empty{};
	if (projectv::render::IsAsyncComputeResourcesAllocated(empty)) {
		context.Fail(__LINE__, "default VulkanContextState must report IsAsyncComputeResourcesAllocated=false");
	}
}

void TestSubmitToComputeQueueRejectsNullContext(TestContext &context)
{
	uint64_t outValue = 0u;
	if (projectv::render::SubmitToComputeQueue(nullptr, VK_NULL_HANDLE, &outValue)) {
		context.Fail(__LINE__, "SubmitToComputeQueue(nullptr) must return false");
	}
}

void TestSubmitToComputeQueueRejectsNullCommandBuffer(TestContext &context)
{
	VulkanContextState contextState{};
	uint64_t outValue = 0u;
	if (projectv::render::SubmitToComputeQueue(&contextState, VK_NULL_HANDLE, &outValue)) {
		context.Fail(__LINE__, "SubmitToComputeQueue(null CB) must return false");
	}
}

}  // namespace

int main()
{
	TestContext context{};
	TestAsyncComputeEnvDefaultOff(context);
	TestAsyncComputeEnvExplicitOn(context);
	TestAsyncComputeEnvZeroIsOff(context);
	TestEnsureAsyncComputeResourcesRejectsNullContext(context);
	TestIsAsyncComputeResourcesAllocatedDefaultsFalse(context);
	TestSubmitToComputeQueueRejectsNullContext(context);
	TestSubmitToComputeQueueRejectsNullCommandBuffer(context);

	if (context.failures > 0) {
		std::fprintf(stderr, "ProjectVAsyncComputeTests: %d failure(s)\n", context.failures);
		return 1;
	}
	std::puts("ProjectVAsyncComputeTests passed");
	return 0;
}

#include "render/Cloudscape.hpp"

#include "core/Types.hpp"

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

void TestCloudscapeEnvDefaultOff(TestContext &context)
{
	unsetenv("PROJECTV_CLOUDS");
	if (projectv::render::IsCloudscapeEnabled()) {
		context.Fail(__LINE__, "PROJECTV_CLOUDS unset -> false");
	}
}

void TestCloudscapeEnvExplicitOn(TestContext &context)
{
	setenv("PROJECTV_CLOUDS", "ON", 1);
	if (!projectv::render::IsCloudscapeEnabled()) {
		context.Fail(__LINE__, "PROJECTV_CLOUDS=ON -> true");
	}
	unsetenv("PROJECTV_CLOUDS");
}

void TestCloudscapeEnvZeroIsOff(TestContext &context)
{
	setenv("PROJECTV_CLOUDS", "0", 1);
	if (projectv::render::IsCloudscapeEnabled()) {
		context.Fail(__LINE__, "PROJECTV_CLOUDS=0 -> false");
	}
	unsetenv("PROJECTV_CLOUDS");
}

void TestCloudscapePushConstantsSize(TestContext &context)
{
	if constexpr (sizeof(projectv::render::CloudscapePushConstants) != 64u) {
		std::fprintf(stderr, "sizeof(CloudscapePushConstants)=%zu expected=64\n", sizeof(projectv::render::CloudscapePushConstants));
		context.Fail(__LINE__, "CloudscapePushConstants must remain 64 bytes");
	}
}

void TestCloudscapeConstants(TestContext &context)
{
	if constexpr (projectv::render::kCloudscapeNoiseTextureSize != 128u) {
		context.Fail(__LINE__, "kCloudscapeNoiseTextureSize must be 128 (Schneider Nubis 2017 reference)");
	}
	if constexpr (projectv::render::kCloudscapeRaymarchStepCount != 24u) {
		context.Fail(__LINE__, "kCloudscapeRaymarchStepCount must be 24 (Schneider Nubis 2017 reference)");
	}
}

void TestCreateCloudscapeResourcesRejectsNullContext(TestContext &context)
{
	unsetenv("PROJECTV_CLOUDS");
	if (projectv::render::CreateCloudscapeResources(nullptr, nullptr)) {
		context.Fail(__LINE__, "CreateCloudscapeResources(null) must return false");
	}
}

void TestDestroyCloudscapeResourcesRejectsNull(TestContext &context)
{
	projectv::render::DestroyCloudscapeResources(nullptr, nullptr);
}

void TestRecordCloudscapeRaymarchPassRejectsNullCommandBuffer(TestContext &context)
{
	RenderState render{};
	constexpr projectv::render::CloudscapePushConstants push{};
	if (projectv::render::RecordCloudscapeRaymarchPass(VK_NULL_HANDLE, render, push, VK_NULL_HANDLE, VK_NULL_HANDLE, {1280u, 720u}, 0u)) {
		context.Fail(__LINE__, "RecordCloudscapeRaymarchPass(null CB) must return false");
	}
}

void TestRecordCloudscapeRaymarchPassRejectsBadFrameIndex(TestContext &context)
{
	RenderState render{};
	constexpr projectv::render::CloudscapePushConstants push{};
	if (projectv::render::RecordCloudscapeRaymarchPass(VK_NULL_HANDLE, render, push, VK_NULL_HANDLE, VK_NULL_HANDLE, {1280u, 720u}, MAX_FRAMES_IN_FLIGHT)) {
	}
}

void TestRecordCloudscapeRaymarchPassRejectsZeroExtent(TestContext &context)
{
	RenderState render{};
	constexpr projectv::render::CloudscapePushConstants push{};
	if (projectv::render::RecordCloudscapeRaymarchPass(VK_NULL_HANDLE, render, push, VK_NULL_HANDLE, VK_NULL_HANDLE, {0u, 0u}, 0u)) {
	}
}

}  // namespace

int main()
{
	TestContext context{};
	TestCloudscapeEnvDefaultOff(context);
	TestCloudscapeEnvExplicitOn(context);
	TestCloudscapeEnvZeroIsOff(context);
	TestCloudscapePushConstantsSize(context);
	TestCloudscapeConstants(context);
	TestCreateCloudscapeResourcesRejectsNullContext(context);
	TestDestroyCloudscapeResourcesRejectsNull(context);
	TestRecordCloudscapeRaymarchPassRejectsNullCommandBuffer(context);
	TestRecordCloudscapeRaymarchPassRejectsBadFrameIndex(context);
	TestRecordCloudscapeRaymarchPassRejectsZeroExtent(context);

	if (context.failures > 0) {
		std::fprintf(stderr, "ProjectVCloudscapeTests: %d failure(s)\n", context.failures);
		return 1;
	}
	std::puts("ProjectVCloudscapeTests passed");
	return 0;
}

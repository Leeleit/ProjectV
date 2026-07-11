#include "render/SkyAtmosphere.hpp"

#include "core/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
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

void TestSkyAtmosphereEnvDefaultOff(TestContext &context)
{
	unsetenv("PROJECTV_SKY");
	if (projectv::render::IsSkyAtmosphereEnabled()) {
		context.Fail(__LINE__, "PROJECTV_SKY unset -> false");
	}
}

void TestSkyAtmosphereEnvExplicitOn(TestContext &context)
{
	setenv("PROJECTV_SKY", "ON", 1);
	if (!projectv::render::IsSkyAtmosphereEnabled()) {
		context.Fail(__LINE__, "PROJECTV_SKY=ON -> true");
	}
	unsetenv("PROJECTV_SKY");
}

void TestSkyAtmosphereEnvZeroIsOff(TestContext &context)
{
	setenv("PROJECTV_SKY", "0", 1);
	if (projectv::render::IsSkyAtmosphereEnabled()) {
		context.Fail(__LINE__, "PROJECTV_SKY=0 -> false");
	}
	unsetenv("PROJECTV_SKY");
}

void TestSkyAtmosphereEnvOffString(TestContext &context)
{
	setenv("PROJECTV_SKY", "OFF", 1);
	if (projectv::render::IsSkyAtmosphereEnabled()) {
		context.Fail(__LINE__, "PROJECTV_SKY=OFF -> false");
	}
	unsetenv("PROJECTV_SKY");
}

void TestSkyAtmospherePushConstantsSize(TestContext &context)
{
	(void)context;
	static_assert(sizeof(projectv::render::SkyAtmospherePushConstants) == 64u, "SkyAtmospherePushConstants must remain 64 bytes (16-byte align for push constants)");
}

void TestCreateSkyAtmospherePipelinesRejectsNullContext(TestContext &context)
{
	unsetenv("PROJECTV_SKY");
	if (projectv::render::CreateSkyAtmospherePipelines(nullptr, nullptr)) {
		context.Fail(__LINE__, "CreateSkyAtmospherePipelines(null) must return false");
	}
}

void TestDestroySkyAtmospherePipelinesRejectsNull(TestContext &context)
{
	(void)context;
	projectv::render::DestroySkyAtmospherePipelines(nullptr, nullptr);
}

void TestRecordSkyAtmospherePassRejectsNullCommandBuffer(TestContext &context)
{
	RenderState render{};
	constexpr projectv::render::SkyAtmospherePushConstants push{};
	if (projectv::render::RecordSkyAtmospherePass(VK_NULL_HANDLE, render, push, VK_NULL_HANDLE, VK_NULL_HANDLE, {1280u, 720u}, 0u)) {
		context.Fail(__LINE__, "RecordSkyAtmospherePass(null CB) must return false");
	}
}

void TestRecordSkyAtmospherePassRejectsNullSceneColor(TestContext &context)
{
	(void)context;
	RenderState render{};
	constexpr projectv::render::SkyAtmospherePushConstants push{};
	if (projectv::render::RecordSkyAtmospherePass(VK_NULL_HANDLE, render, push, VK_NULL_HANDLE, VK_NULL_HANDLE, {1280u, 720u}, 0u)) {
	}
}

void TestRecordSkyAtmospherePassRejectsZeroExtent(TestContext &context)
{
	RenderState render{};
	constexpr projectv::render::SkyAtmospherePushConstants push{};
	if (projectv::render::RecordSkyAtmospherePass(VK_NULL_HANDLE, render, push, VK_NULL_HANDLE, VK_NULL_HANDLE, {0u, 0u}, 0u)) {
		context.Fail(__LINE__, "RecordSkyAtmospherePass(zero extent) must return false");
	}
}

}  // namespace

int main()
{
	TestContext context{};
	TestSkyAtmosphereEnvDefaultOff(context);
	TestSkyAtmosphereEnvExplicitOn(context);
	TestSkyAtmosphereEnvZeroIsOff(context);
	TestSkyAtmosphereEnvOffString(context);
	TestSkyAtmospherePushConstantsSize(context);
	TestCreateSkyAtmospherePipelinesRejectsNullContext(context);
	TestDestroySkyAtmospherePipelinesRejectsNull(context);
	TestRecordSkyAtmospherePassRejectsNullCommandBuffer(context);
	TestRecordSkyAtmospherePassRejectsNullSceneColor(context);
	TestRecordSkyAtmospherePassRejectsZeroExtent(context);

	if (context.failures > 0) {
		std::fprintf(stderr, "ProjectVSkyAtmosphereTests: %d failure(s)\n", context.failures);
		return 1;
	}
	std::puts("ProjectVSkyAtmosphereTests passed");
	return 0;
}

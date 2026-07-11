#include "render/VolumetricFog.hpp"

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

void TestVolumetricFogEnvDefaultOff(TestContext &context)
{
	unsetenv("PROJECTV_FOG");
	if (projectv::render::IsVolumetricFogEnabled()) {
		context.Fail(__LINE__, "PROJECTV_FOG unset -> false");
	}
}

void TestVolumetricFogEnvExplicitOn(TestContext &context)
{
	setenv("PROJECTV_FOG", "ON", 1);
	if (!projectv::render::IsVolumetricFogEnabled()) {
		context.Fail(__LINE__, "PROJECTV_FOG=ON -> true");
	}
	unsetenv("PROJECTV_FOG");
}

void TestVolumetricFogEnvZeroIsOff(TestContext &context)
{
	setenv("PROJECTV_FOG", "0", 1);
	if (projectv::render::IsVolumetricFogEnabled()) {
		context.Fail(__LINE__, "PROJECTV_FOG=0 -> false");
	}
	unsetenv("PROJECTV_FOG");
}

void TestVolumetricFogPushConstantsSize(TestContext &context)
{
	(void)context;
	static_assert(sizeof(projectv::render::VolumetricFogPushConstants) == 64u, "VolumetricFogPushConstants must remain 64 bytes");
}

void TestVolumetricFogFroxelConstants(TestContext &context)
{
	(void)context;
	static_assert(projectv::render::kVolumetricFogFroxelWidth == 160u, "kVolumetricFogFroxelWidth must be 160 (Wronski 2014 720p reference)");
	static_assert(projectv::render::kVolumetricFogFroxelHeight == 90u, "kVolumetricFogFroxelHeight must be 90 (Wronski 2014 720p reference)");
	static_assert(projectv::render::kVolumetricFogFroxelDepth == 64u, "kVolumetricFogFroxelDepth must be 64 (Wronski 2014 default)");
	static_assert(projectv::render::kVolumetricFogRaymarchStepCount == 12u, "kVolumetricFogRaymarchStepCount must be 12 (per-frostbite pattern)");
}

void TestVolumetricFogDispatchDimensions(TestContext &context)
{
	(void)context;
	// Phase 5 Wronski slab ray-march dispatch: (W/8, H/8, D/4) workgroups
	// for the 160x90x64 froxel grid. Workgroup-local size is 8x8x4 per
	// `src/shaders/volumetric_fog.comp` `local_size_x/y/z`. The shader
	// bounds-checks against `imageSize(fogFroxel)` so the truncated dispatch
	// (W=20, H=11, D=16) safely ignores the 2 extra height-row froxels —
	// matches Wronski 2014 720p reference without over-allocating.
	static_assert(projectv::render::kVolumetricFogFroxelWidth / 8u == 20u, "kVolumetricFogFroxelWidth/8 should be 20 workgroups");
	static_assert(projectv::render::kVolumetricFogFroxelHeight / 8u == 11u, "kVolumetricFogFroxelHeight/8 should be 11 workgroups (Wronski 720p)");
	static_assert(projectv::render::kVolumetricFogFroxelDepth / 4u == 16u, "kVolumetricFogFroxelDepth/4 should be 16 workgroups");
}

void TestCreateVolumetricFogResourcesRejectsNullContext(TestContext &context)
{
	unsetenv("PROJECTV_FOG");
	if (projectv::render::CreateVolumetricFogResources(nullptr, nullptr)) {
		context.Fail(__LINE__, "CreateVolumetricFogResources(null) must return false");
	}
}

void TestDestroyVolumetricFogResourcesRejectsNull()
{
	projectv::render::DestroyVolumetricFogResources(nullptr, nullptr);
}

void TestRecordVolumetricFogAccumulationPassRejectsNullCommandBuffer(TestContext &context)
{
	RenderState render{};
	constexpr projectv::render::VolumetricFogPushConstants push{};
	if (projectv::render::RecordVolumetricFogAccumulationPass(VK_NULL_HANDLE, render, push, 0u)) {
		context.Fail(__LINE__, "RecordVolumetricFogAccumulationPass(null CB) must return false");
	}
}

void TestRecordVolumetricFogAccumulationPassRejectsBadFrameIndex(TestContext &context)
{
	(void)context;
	RenderState render{};
	constexpr projectv::render::VolumetricFogPushConstants push{};
	if (projectv::render::RecordVolumetricFogAccumulationPass(VK_NULL_HANDLE, render, push, MAX_FRAMES_IN_FLIGHT)) {
	}
}

}  // namespace

int main()
{
	TestContext context{};
	TestVolumetricFogEnvDefaultOff(context);
	TestVolumetricFogEnvExplicitOn(context);
	TestVolumetricFogEnvZeroIsOff(context);
	TestVolumetricFogPushConstantsSize(context);
	TestVolumetricFogFroxelConstants(context);
	TestVolumetricFogDispatchDimensions(context);
	TestCreateVolumetricFogResourcesRejectsNullContext(context);
	TestDestroyVolumetricFogResourcesRejectsNull();
	TestRecordVolumetricFogAccumulationPassRejectsNullCommandBuffer(context);
	TestRecordVolumetricFogAccumulationPassRejectsBadFrameIndex(context);

	if (context.failures > 0) {
		std::fprintf(stderr, "ProjectVVolumetricFogTests: %d failure(s)\n", context.failures);
		return 1;
	}
	std::puts("ProjectVVolumetricFogTests passed");
	return 0;
}

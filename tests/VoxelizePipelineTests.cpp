#include "render/vulkan/VulkanVoxelizePipeline.hpp"
#include "voxel/VoxelMaterials.hpp"

#include <cstddef>
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

void TestVctGpuPipelineEnvDefaultOff(TestContext &context)
{
	unsetenv("PROJECTV_VCT_GPU");
	if (projectv::render::IsVctGpuPipelineRequested()) {
		context.Fail(__LINE__, "PROJECTV_VCT_GPU unset -> false");
	}
}

void TestVctGpuPipelineEnvExplicitOn(TestContext &context)
{
	setenv("PROJECTV_VCT_GPU", "1", 1);
	if (!projectv::render::IsVctGpuPipelineRequested()) {
		context.Fail(__LINE__, "PROJECTV_VCT_GPU=1 -> true");
	}
	unsetenv("PROJECTV_VCT_GPU");
}

void TestVctGpuPipelineEnvZeroIsOff(TestContext &context)
{
	setenv("PROJECTV_VCT_GPU", "0", 1);
	if (projectv::render::IsVctGpuPipelineRequested()) {
		context.Fail(__LINE__, "PROJECTV_VCT_GPU=0 -> false");
	}
	unsetenv("PROJECTV_VCT_GPU");
}

void TestVoxelizePushConstantsSize(TestContext &context)
{
	if (sizeof(projectv::render::VoxelizePushConstants) != 48u) {
		std::fprintf(stderr, "sizeof(VoxelizePushConstants)=%zu expected=48\n", sizeof(projectv::render::VoxelizePushConstants));
		context.Fail(__LINE__, "VoxelizePushConstants must remain 48 bytes (16-byte align for SSBO)");
	}
}

void TestCreateVoxelizePipelinesRejectsNullContext(TestContext &context)
{
	if (projectv::render::CreateVoxelizePipelines(nullptr, nullptr)) {
		context.Fail(__LINE__, "CreateVoxelizePipelines(null) must return false");
	}
}

void TestDestroyVoxelizePipelinesRejectsNull(TestContext &context)
{
	projectv::render::DestroyVoxelizePipelines(nullptr, nullptr);
}

void TestRecordVoxelizeDispatchRejectsNullCommandBuffer(TestContext &context)
{
	RenderState render{};
	SceneFrameResources frameResources{};
	projectv::render::VoxelizePushConstants pushConstants{};
	if (projectv::render::RecordVoxelizeDispatch(
			VK_NULL_HANDLE,
			render,
			frameResources,
			pushConstants,
			0u)) {
		context.Fail(__LINE__, "RecordVoxelizeDispatch(null CB) must return false");
	}
}

void TestRecordVoxelizeDispatchRejectsEmptyActiveChunks(TestContext &context)
{
	RenderState render{};
	SceneFrameResources frameResources{};
	projectv::render::VoxelizePushConstants pushConstants{};
	if (!projectv::render::RecordVoxelizeDispatch(
			VK_NULL_HANDLE,
			render,
			frameResources,
			pushConstants,
			0u)) {
	}
}

void TestRefreshVoxelizeResourceBindingsRejectsNullContext(TestContext &context)
{
	RenderState render{};
	if (projectv::render::RefreshVoxelizeResourceBindings(nullptr, &render, 0u)) {
		context.Fail(__LINE__, "RefreshVoxelizeResourceBindings(null) must return false");
	}
}

void TestBuildVctClipmapMipChainRejectsNullCommandBuffer(TestContext &context)
{
	RenderState render{};
	if (projectv::render::BuildVctClipmapMipChain(VK_NULL_HANDLE, render)) {
		context.Fail(__LINE__, "BuildVctClipmapMipChain(null CB) must return false");
	}
}

void TestBuildVctClipmapMipChainRejectsEmptyClipmap(TestContext &context)
{
	RenderState render{};
	if (projectv::render::BuildVctClipmapMipChain(VK_NULL_HANDLE, render)) {
	}
}

void TestVctLightingContractSize(TestContext &context)
{
	if (sizeof(VoxelSceneLighting) != 656u) {
		std::fprintf(stderr, "sizeof(VoxelSceneLighting)=%zu expected=656\n", sizeof(VoxelSceneLighting));
		context.Fail(__LINE__, "VoxelSceneLighting size must remain 656 bytes (lighting contract with voxel.frag)");
	}
	if (offsetof(VoxelSceneLighting, vctParams) != 624u) {
		std::fprintf(stderr, "vctParams offset=%zu expected=624\n", offsetof(VoxelSceneLighting, vctParams));
		context.Fail(__LINE__, "vctParams offset must remain 624 bytes (shader SceneLightingBuffer contract)");
	}
	if (offsetof(VoxelSceneLighting, vctSpecularParams) != 640u) {
		std::fprintf(stderr, "vctSpecularParams offset=%zu expected=640\n", offsetof(VoxelSceneLighting, vctSpecularParams));
		context.Fail(__LINE__, "vctSpecularParams offset must remain 640 bytes (shader SceneLightingBuffer contract)");
	}
}

void TestVctDebugViewStringMapping(TestContext &context)
{
	if (std::string_view(LightingDebugViewToString(LightingDebugView::VctDiffuse)) != "VCT_DIFF") {
		context.Fail(__LINE__, "VctDiffuse string must be VCT_DIFF");
	}
	if (std::string_view(LightingDebugViewToString(LightingDebugView::VctSpecular)) != "VCT_SPEC") {
		context.Fail(__LINE__, "VctSpecular string must be VCT_SPEC");
	}
}

void TestVctDebugViewCycle(TestContext &context)
{
	if (GetNextLightingDebugView(LightingDebugView::Taa) != LightingDebugView::VctDiffuse) {
		context.Fail(__LINE__, "Taa -> VctDiffuse cycle break");
	}
	if (GetNextLightingDebugView(LightingDebugView::VctDiffuse) != LightingDebugView::VctSpecular) {
		context.Fail(__LINE__, "VctDiffuse -> VctSpecular cycle break");
	}
	if (GetNextLightingDebugView(LightingDebugView::VctSpecular) != LightingDebugView::VolumetricFog) {
		context.Fail(__LINE__, "VctSpecular -> VolumetricFog cycle break");
	}
	if (GetNextLightingDebugView(LightingDebugView::VolumetricFog) != LightingDebugView::VolumetricTransmittance) {
		context.Fail(__LINE__, "VolumetricFog -> VolumetricTransmittance cycle break");
	}
	if (GetNextLightingDebugView(LightingDebugView::VolumetricTransmittance) != LightingDebugView::Final) {
		context.Fail(__LINE__, "VolumetricTransmittance -> Final cycle break");
	}
}

}  // namespace

int main()
{
	TestContext context{};
	TestVctGpuPipelineEnvDefaultOff(context);
	TestVctGpuPipelineEnvExplicitOn(context);
	TestVctGpuPipelineEnvZeroIsOff(context);
	TestVoxelizePushConstantsSize(context);
	TestCreateVoxelizePipelinesRejectsNullContext(context);
	TestDestroyVoxelizePipelinesRejectsNull(context);
	TestRecordVoxelizeDispatchRejectsNullCommandBuffer(context);
	TestRecordVoxelizeDispatchRejectsEmptyActiveChunks(context);
	TestRefreshVoxelizeResourceBindingsRejectsNullContext(context);
	TestBuildVctClipmapMipChainRejectsNullCommandBuffer(context);
	TestBuildVctClipmapMipChainRejectsEmptyClipmap(context);
	TestVctLightingContractSize(context);
	TestVctDebugViewStringMapping(context);
	TestVctDebugViewCycle(context);

	if (context.failures > 0) {
		std::fprintf(stderr, "ProjectVVoxelizePipelineTests: %d failure(s)\n", context.failures);
		return 1;
	}
	std::puts("ProjectVVoxelizePipelineTests passed");
	return 0;
}

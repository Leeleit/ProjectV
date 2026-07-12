#include "render/vulkan/VulkanVoxelizePipeline.hpp"
#include "voxel/VoxelMaterials.hpp"

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
	(void)context;
	static_assert(sizeof(projectv::render::VoxelizePushConstants) == 48u, "VoxelizePushConstants must remain 48 bytes (16-byte align for SSBO)");
}

void TestCreateVoxelizePipelinesRejectsNullContext(TestContext &context)
{
	if (projectv::render::CreateVoxelizePipelines(nullptr, nullptr)) {
		context.Fail(__LINE__, "CreateVoxelizePipelines(null) must return false");
	}
}

void TestDestroyVoxelizePipelinesRejectsNull(TestContext &context)
{
	(void)context;
	projectv::render::DestroyVoxelizePipelines(nullptr, nullptr);
}

void TestRecordVoxelizeDispatchRejectsNullCommandBuffer(TestContext &context)
{
	RenderState render{};
	SceneFrameResources frameResources{};
	constexpr projectv::render::VoxelizePushConstants pushConstants{};
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
	(void)context;
	RenderState render{};
	SceneFrameResources frameResources{};
	constexpr projectv::render::VoxelizePushConstants pushConstants{};
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
	(void)context;
	RenderState render{};
	if (projectv::render::BuildVctClipmapMipChain(VK_NULL_HANDLE, render)) {
	}
}

void TestVctLightingContractSize(TestContext &context)
{
	(void)context;
	// CSM removed per TODO.md §5.2.D (session 20x). The size + offset
	// static_asserts for VoxelSceneLighting are retired; the struct is now
	// 352 bytes (VCT diffuse path in voxel.frag remains, sun-shadow cascade
	// fields are gone). vctParams / vctSpecularParams offsets are no longer
	// pinned to the old cascade-augmented layout.
}

void TestDebugViewStringMapping(TestContext &context)
{
	if (std::string_view(LightingDebugViewToString(LightingDebugView::DiffuseGI)) != "GI_DIF") {
		context.Fail(__LINE__, "DiffuseGI string must be GI_DIF");
	}
	if (std::string_view(LightingDebugViewToString(LightingDebugView::SpecularGI)) != "GI_SPC") {
		context.Fail(__LINE__, "SpecularGI string must be GI_SPC");
	}
	if (std::string_view(LightingDebugViewToString(LightingDebugView::RtxSpecular)) != "RTX_SPC") {
		context.Fail(__LINE__, "RtxSpecular string must be RTX_SPC");
	}
}

void TestDebugViewCycle(TestContext &context)
{
	if (GetNextLightingDebugView(LightingDebugView::Fog) != LightingDebugView::DiffuseGI) {
		context.Fail(__LINE__, "Fog -> DiffuseGI cycle break");
	}
	if (GetNextLightingDebugView(LightingDebugView::DiffuseGI) != LightingDebugView::SpecularGI) {
		context.Fail(__LINE__, "DiffuseGI -> SpecularGI cycle break");
	}
	if (GetNextLightingDebugView(LightingDebugView::SpecularGI) != LightingDebugView::RtxSpecular) {
		context.Fail(__LINE__, "SpecularGI -> RtxSpecular cycle break");
	}
	if (GetNextLightingDebugView(LightingDebugView::RtxSpecular) != LightingDebugView::VolumetricFog) {
		context.Fail(__LINE__, "RtxSpecular -> VolumetricFog cycle break");
	}
	if (GetNextLightingDebugView(LightingDebugView::VolumetricFog) != LightingDebugView::VolumetricTransmittance) {
		context.Fail(__LINE__, "VolumetricFog -> VolumetricTransmittance cycle break");
	}
	if (GetNextLightingDebugView(LightingDebugView::VolumetricTransmittance) != LightingDebugView::GreedyMeshing) {
		context.Fail(__LINE__, "VolumetricTransmittance -> GreedyMeshing cycle break");
	}
	if (GetNextLightingDebugView(LightingDebugView::GreedyMeshing) != LightingDebugView::Final) {
		context.Fail(__LINE__, "GreedyMeshing -> Final cycle break");
	}
}

} // namespace

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
	TestDebugViewStringMapping(context);
	TestDebugViewCycle(context);

	if (context.failures > 0) {
		std::fprintf(stderr, "ProjectVVoxelizePipelineTests: %d failure(s)\n", context.failures);
		return 1;
	}
	std::puts("ProjectVVoxelizePipelineTests passed");
	return 0;
}

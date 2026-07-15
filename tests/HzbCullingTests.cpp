#include "render/HizCulling.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <string>
#include <string_view>

namespace {

struct TestContext {
	int failures = 0;
	void Fail(const std::string_view message)
	{
		std::fprintf(stderr, "Test failure: %.*s\n", static_cast<int>(message.size()), message.data());
		++failures;
	}
};

#define EXPECT_FALSE(context, condition, expr) \
	do {                                       \
		if (condition) {                       \
			(context).Fail(expr);              \
		}                                      \
	} while (false)

#define EXPECT_TRUE(context, condition, expr) \
	do {                                      \
		if (!(condition)) {                   \
			(context).Fail(expr);             \
		}                                     \
	} while (false)

#define EXPECT_EQUAL_UINT(context, expected, actual, line, expr)                                                                                                 \
	do {                                                                                                                                                         \
		if ((expected) != (actual)) {                                                                                                                            \
			const std::string_view _expr = (expr);                                                                                                               \
			std::fprintf(stderr, "Test failure at line %d: %.*s (expected %u, got %u)\n", line, static_cast<int>(_expr.size()), _expr.data(), expected, actual); \
			++(context).failures;                                                                                                                                \
		}                                                                                                                                                        \
	} while (false)

void TestHzbDisabledByDefault(TestContext &context)
{
	const bool enabled = projectv::render::IsHzbCullingEnabled();
	EXPECT_FALSE(context, enabled, "HZB culling disabled without env");
}

void TestComputeHzbMipLevelCountSquare(TestContext &context)
{
	const uint32_t levels = projectv::render::ComputeHzbMipLevelCount(1024u, 1024u);
	EXPECT_EQUAL_UINT(context, 11u, levels, __LINE__, "1024x1024 -> 11 mip levels (1+log2(1024))");
}

void TestComputeHzbMipLevelCountNonSquare(TestContext &context)
{
	const uint32_t levels = projectv::render::ComputeHzbMipLevelCount(800u, 600u);
	EXPECT_EQUAL_UINT(context, 10u, levels, __LINE__, "800x600 -> 10 mip levels (1+log2(600))");
}

void TestComputeHzbMipLevelCountTiny(TestContext &context)
{
	const uint32_t levels = projectv::render::ComputeHzbMipLevelCount(1u, 1u);
	EXPECT_EQUAL_UINT(context, 1u, levels, __LINE__, "1x1 -> 1 mip level");
}

void TestHzbApplyModeValues(TestContext &context)
{
	EXPECT_EQUAL_UINT(
		context,
		0u,
		static_cast<uint32_t>(projectv::render::HzbApplyMode::PassA),
		__LINE__,
		"PassA == 0");
	EXPECT_EQUAL_UINT(
		context,
		1u,
		static_cast<uint32_t>(projectv::render::HzbApplyMode::PassB),
		__LINE__,
		"PassB == 1");
	EXPECT_EQUAL_UINT(
		context,
		2u,
		static_cast<uint32_t>(projectv::render::HzbApplyMode::ForceAll),
		__LINE__,
		"ForceAll == 2");
}

std::string ReadShaderSource(const char *shaderName)
{
	const std::filesystem::path shaderPath =
		std::filesystem::path(PROJECTV_TESTS_SOURCE_DIR) / ".." / "src" / "shaders" / shaderName;
	std::ifstream input{shaderPath};
	return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::string ReadProjectSource(const char *relativePath)
{
	const std::filesystem::path sourcePath =
		std::filesystem::path(PROJECTV_TESTS_SOURCE_DIR) / ".." / "src" / relativePath;
	std::ifstream input{sourcePath};
	return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

void TestNormalDepthHzbUsesMaxReduction(TestContext &context)
{
	const std::string minifySource = ReadShaderSource("hiz_minify.comp");
	const std::string cullSource = ReadShaderSource("hzb_cull.comp");
	EXPECT_TRUE(context, !minifySource.empty(), "hiz_minify.comp source is readable");
	EXPECT_TRUE(context, !cullSource.empty(), "hzb_cull.comp source is readable");
	EXPECT_TRUE(
		context,
		minifySource.find("max(max(d00, d10), max(d01, d11))") != std::string::npos,
		"normal-depth Hi-Z reduces each 2x2 block with max");
	EXPECT_TRUE(
		context,
		cullSource.find("max(max(d00, d10), max(d01, d11))") != std::string::npos,
		"normal-depth HZB query uses the farthest sampled depth");
}

void TestHzbUsesSeparateSampledImageAndSampler(TestContext &context)
{
	const std::string cullSource = ReadShaderSource("hzb_cull.comp");
	const std::string minifySource = ReadShaderSource("hiz_minify.comp");
	EXPECT_TRUE(
		context,
		cullSource.find("uniform texture2D hizTexture") != std::string::npos,
		"HZB cull shader declares binding 2 as a separate sampled image");
	EXPECT_TRUE(
		context,
		cullSource.find("sampler2D(hizTexture, hizSampler)") != std::string::npos,
		"HZB cull shader combines its separate image and sampler at lookup");
	EXPECT_TRUE(
		context,
		minifySource.find("uniform texture2D srcTexture") != std::string::npos,
		"HZB depth-copy shader reads its source through a sampled image");
	EXPECT_TRUE(
		context,
		minifySource.find("uniform sampler srcSampler") != std::string::npos,
		"HZB depth-copy shader declares the separate sampler");
}

void TestHzbMipsCoverLongestImageAxis(TestContext &context)
{
	const uint32_t levels = projectv::render::ComputeHzbMipLevelCount(8192u, 1u);
	EXPECT_EQUAL_UINT(context, 14u, levels, __LINE__, "8192x1 -> 14 mip levels (1+log2(8192))");
}

void TestHzbBuildUsesSampledDepthCopy(TestContext &context)
{
	const std::string buildSource = ReadProjectSource("render/HizCullingBuild.cpp");
	EXPECT_TRUE(context, !buildSource.empty(), "HizCullingBuild.cpp source is readable");
	EXPECT_TRUE(
		context,
		buildSource.find("vkCmdBlitImage(") == std::string::npos,
		"HZB never blits a depth image into an R32 color image");
	EXPECT_TRUE(
		context,
		buildSource.find("VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL") != std::string::npos,
		"HZB transitions the depth source for compute sampling");
}

void TestHzbVisibleCountSupportsTransferFill(TestContext &context)
{
	const std::string resourcesSource = ReadProjectSource("render/SceneResourcesFrame.cpp");
	const size_t allocationBegin =
		resourcesSource.find("VmaAllocationInfo hzbVisibleCountAllocationInfo");
	const size_t allocationEnd =
		resourcesSource.find("frameResources.hzbVisibleCountMappedData", allocationBegin);
	EXPECT_TRUE(context, allocationBegin != std::string::npos, "HZB visible-count allocation is present");
	EXPECT_TRUE(context, allocationEnd != std::string::npos, "HZB visible-count allocation has a mapped-data assignment");
	if (allocationBegin == std::string::npos || allocationEnd == std::string::npos) {
		return;
	}
	const std::string allocationBlock =
		resourcesSource.substr(allocationBegin, allocationEnd - allocationBegin);
	EXPECT_TRUE(
		context,
		allocationBlock.find("VK_BUFFER_USAGE_TRANSFER_DST_BIT") != std::string::npos,
		"HZB visible-count buffer supports vkCmdFillBuffer");
}

void TestHzbApplyDescriptorSetIsInitializedOnce(TestContext &context)
{
	const std::string applySource = ReadProjectSource("render/HizCullingApply.cpp");
	EXPECT_TRUE(
		context,
		applySource.find("if (!frameResources.hizApplyDescriptorSetInitialized)") != std::string::npos,
		"HZB apply descriptors are initialized before their first bind only");
}

void TestHzbCullMakesVisibilityAvailableToPassB(TestContext &context)
{
	const std::string dispatchSource = ReadProjectSource("render/HizCullingDispatch.cpp");
	const size_t barrierBegin = dispatchSource.find("VkBufferMemoryBarrier2 cullToApplyBarrier");
	EXPECT_TRUE(
		context,
		barrierBegin != std::string::npos,
		"HZB cull makes visibility-mask writes available before Pass B reads them");
	if (barrierBegin == std::string::npos) {
		return;
	}
	const size_t barrierEnd = dispatchSource.find("VkDependencyInfo cullToApplyDep", barrierBegin);
	EXPECT_TRUE(context, barrierEnd != std::string::npos, "HZB cull-to-apply barrier has a dependency");
	if (barrierEnd == std::string::npos) {
		return;
	}
	const std::string barrierBlock = dispatchSource.substr(barrierBegin, barrierEnd - barrierBegin);
	EXPECT_TRUE(
		context,
		barrierBlock.find("VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT") != std::string::npos,
		"HZB cull-to-apply barrier waits for cull storage writes");
	EXPECT_TRUE(
		context,
		barrierBlock.find("VK_ACCESS_2_SHADER_STORAGE_READ_BIT") != std::string::npos,
		"HZB cull-to-apply barrier exposes the mask to Pass B storage reads");
	EXPECT_TRUE(
		context,
		barrierBlock.find("frameResources.visibilityMaskBuffer") != std::string::npos,
		"HZB cull-to-apply barrier synchronizes the current visibility mask");
}

void TestMeshPreCullHonorsHzbVisibility(TestContext &context)
{
	const std::string preCullSource = ReadShaderSource("voxel_mesh_pre.comp");
	const std::string rendererSource = ReadProjectSource("render/RendererRecordCommands.cpp");
	EXPECT_TRUE(
		context,
		preCullSource.find("binding = 4") != std::string::npos,
		"mesh pre-cull reads the current HZB visibility mask");
	EXPECT_TRUE(
		context,
		preCullSource.find("binding = 5") != std::string::npos,
		"mesh pre-cull reads the previous HZB visibility mask for Pass B");
	EXPECT_TRUE(
		context,
		preCullSource.find("visibilityMaskMode") != std::string::npos,
		"mesh pre-cull selects Pass A, Pass B, or ForceAll visibility semantics");

	const size_t passAApply = rendererSource.find("passAMode);");
	const size_t passBApply = rendererSource.find("HzbApplyMode::PassB);");
	EXPECT_TRUE(context, passAApply != std::string::npos, "HZB Pass A apply is present");
	EXPECT_TRUE(context, passBApply != std::string::npos, "HZB Pass B apply is present");
	if (passAApply == std::string::npos || passBApply == std::string::npos) {
		return;
	}
	const size_t passAPreCull = rendererSource.find("RecordMeshShaderPreCull(", passAApply);
	const size_t passBPreCull = rendererSource.find("RecordMeshShaderPreCull(", passBApply);
	EXPECT_TRUE(
		context,
		passAPreCull != std::string::npos,
		"mesh pre-cull rebuilds the cluster list after HZB Pass A apply");
	EXPECT_TRUE(
		context,
		passBPreCull != std::string::npos,
		"mesh pre-cull rebuilds the disocclusion cluster list after HZB Pass B apply");
}

void TestMeshShaderValidationContracts(TestContext &context)
{
	const std::string meshSetupSource = ReadProjectSource("render/vulkan/VulkanMeshShaderSetup.cpp");
	const std::string bootstrapSource = ReadProjectSource("render/vulkan/VulkanBootstrapFeatures.cpp");
	const std::string rendererSource = ReadProjectSource("render/RendererRecordCommands.cpp");
	EXPECT_TRUE(
		context,
		meshSetupSource.find(
			"meshGraphicsPush.stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT") !=
			std::string::npos,
		"mesh graphics pipeline uses a mesh-and-fragment push-constant range");
	EXPECT_TRUE(
		context,
		meshSetupSource.find("meshCullPush.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT") != std::string::npos,
		"mesh pre-cull pipeline uses a compute-only push-constant range");
	EXPECT_TRUE(
		context,
		bootstrapSource.find(
			"enabled.maintenance4 = selected.features13.maintenance4 ? VK_TRUE : VK_FALSE") !=
			std::string::npos,
		"Vulkan 1.3 maintenance4 is enabled when the selected device supports it");

	const size_t transparentPass = rendererSource.find("PV_PROFILE_GPU_ZONE(render.tracyGraphicsContext, cmd, \"Transparent Pass\")");
	const size_t transparentDraw = rendererSource.find("vkCmdDrawIndirect(", transparentPass);
	EXPECT_TRUE(context, transparentPass != std::string::npos, "transparent pass is present");
	EXPECT_TRUE(context, transparentDraw != std::string::npos, "transparent indirect draw is present");
	if (transparentPass == std::string::npos || transparentDraw == std::string::npos) {
		return;
	}
	const std::string transparentSetup =
		rendererSource.substr(transparentPass, transparentDraw - transparentPass);
	EXPECT_TRUE(
		context,
		transparentSetup.find("vkCmdBindDescriptorSets(") != std::string::npos,
		"transparent packed draw rebinds descriptors after the mesh pipeline");
}

void TestMeshPreCullResetsIndirectOnGpu(TestContext &context)
{
	const std::string meshCullSource = ReadProjectSource("render/vulkan/VulkanMeshShaderCull.cpp");
	const std::string resourcesSource = ReadProjectSource("render/SceneResourcesFrame.cpp");
	EXPECT_TRUE(
		context,
		meshCullSource.find("vkCmdFillBuffer(") != std::string::npos,
		"mesh pre-cull resets its indirect command in GPU command order");
	EXPECT_TRUE(
		context,
		meshCullSource.find("VK_PIPELINE_STAGE_2_TRANSFER_BIT") != std::string::npos,
		"mesh pre-cull makes the GPU reset visible to its compute dispatch");

	const size_t allocationBegin = resourcesSource.find("VmaAllocationInfo meshDrawIndirectAllocationInfo");
	const size_t allocationEnd = resourcesSource.find(
		"frameResources.meshClusters.meshDrawIndirectMappedData",
		allocationBegin);
	EXPECT_TRUE(context, allocationBegin != std::string::npos, "mesh indirect allocation is present");
	EXPECT_TRUE(context, allocationEnd != std::string::npos, "mesh indirect allocation records its mapped pointer");
	if (allocationBegin == std::string::npos || allocationEnd == std::string::npos) {
		return;
	}
	const std::string allocationBlock =
		resourcesSource.substr(allocationBegin, allocationEnd - allocationBegin);
	EXPECT_TRUE(
		context,
		allocationBlock.find("VK_BUFFER_USAGE_TRANSFER_DST_BIT") != std::string::npos,
		"mesh indirect buffer supports GPU command reset");
}

void TestTimestampResultsSkipUnsubmittedSlots(TestContext &context)
{
	const std::string typesSource = ReadProjectSource("core/Types.hpp");
	const std::string drawFrameSource = ReadProjectSource("render/RendererDrawFrame.cpp");
	EXPECT_TRUE(
		context,
		typesSource.find("gpuTimestampQueriesReady") != std::string::npos,
		"render state tracks timestamp-query slots submitted to the GPU");

	const size_t resultsRead = drawFrameSource.find("vkGetQueryPoolResults(");
	const size_t readinessGuard = drawFrameSource.rfind(
		"gpuTimestampQueriesReady[currentFrameIndex]",
		resultsRead);
	EXPECT_TRUE(context, resultsRead != std::string::npos, "GPU timestamp results are queried");
	EXPECT_TRUE(
		context,
		readinessGuard != std::string::npos,
		"GPU timestamp results are read only after that frame slot was submitted");
	EXPECT_TRUE(
		context,
		drawFrameSource.find(
			"gpuTimestampQueriesReady[currentFrameIndex] = true") != std::string::npos,
		"a successful graphics submission marks its timestamp-query slot ready");
}

} // namespace

int main() // NOLINT(*-exception-escape): MSVC STL stream construction may throw; terminating the test process is intended.
{
	TestContext context{};
	TestHzbDisabledByDefault(context);
	TestComputeHzbMipLevelCountSquare(context);
	TestComputeHzbMipLevelCountNonSquare(context);
	TestComputeHzbMipLevelCountTiny(context);
	TestHzbApplyModeValues(context);
	TestNormalDepthHzbUsesMaxReduction(context);
	TestHzbUsesSeparateSampledImageAndSampler(context);
	TestHzbMipsCoverLongestImageAxis(context);
	TestHzbBuildUsesSampledDepthCopy(context);
	TestHzbVisibleCountSupportsTransferFill(context);
	TestHzbApplyDescriptorSetIsInitializedOnce(context);
	TestHzbCullMakesVisibilityAvailableToPassB(context);
	TestMeshPreCullHonorsHzbVisibility(context);
	TestMeshShaderValidationContracts(context);
	TestMeshPreCullResetsIndirectOnGpu(context);
	TestTimestampResultsSkipUnsubmittedSlots(context);
	if (context.failures != 0) {
		return EXIT_FAILURE;
	}
	std::puts("ProjectVHzbCullingTests passed");
	return EXIT_SUCCESS;
}

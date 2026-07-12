#include "render/vulkan/VulkanMeshShaderPipeline.hpp"

#include <cstdio>
#include <cstdlib>
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
	do {                                         \
		if (condition) {                         \
			(context).Fail(expr);                \
		}                                        \
	} while (false)

#define EXPECT_EQUAL_UINT(context, expected, actual, line, expr)                                                                                                 \
	do {                                                                                                                                                         \
		if ((expected) != (actual)) {                                                                                                                            \
			const std::string_view _expr = (expr);                                                                                                               \
			std::fprintf(stderr, "Test failure at line %d: %.*s (expected %u, got %u)\n", line, static_cast<int>(_expr.size()), _expr.data(), expected, actual); \
			++(context).failures;                                                                                                                                \
		}                                                                                                                                                        \
	} while (false)

void TestMeshShaderPipelineRequestedDefaultOff(TestContext &context)
{
	const bool requested = projectv::render::IsMeshShaderPipelineRequested();
	EXPECT_FALSE(context, requested, "PROJECTV_MESH_SHADER_PIPELINE unset -> false");
}

void TestBuildMeshCullPushConstantsDispatchParams(TestContext &context)
{
	ChunkCullingParameters parameters{};
	parameters.cameraPositionAndMaxDistance = {0.0f, 0.0f, 0.0f, 64.0f};
	parameters.cameraForwardAndTanHalfVerticalFov = {0.0f, 0.0f, -1.0f, 0.5f};
	parameters.cameraRightAndTanHalfHorizontalFov = {1.0f, 0.0f, 0.0f, 0.5f};
	parameters.cameraUpAndNearPlane = {0.0f, 1.0f, 0.0f, 0.1f};

	const auto [dispatchParams, frustumPlanes] =
		projectv::render::BuildMeshCullPushConstants(parameters, 256u);
	EXPECT_EQUAL_UINT(context, 256u, dispatchParams[0], __LINE__, "dispatchParams[0] = chunk count");
}

void TestBuildMeshCullPushConstantsFrustumForwardPlane(TestContext &context)
{
	ChunkCullingParameters parameters{};
	parameters.cameraPositionAndMaxDistance = {0.0f, 0.0f, 0.0f, 100.0f};
	parameters.cameraForwardAndTanHalfVerticalFov = {0.0f, 0.0f, -1.0f, 0.0f};
	parameters.cameraRightAndTanHalfHorizontalFov = {1.0f, 0.0f, 0.0f, 0.0f};
	parameters.cameraUpAndNearPlane = {0.0f, 1.0f, 0.0f, 0.5f};

	const auto [dispatchParams, frustumPlanes] =
		projectv::render::BuildMeshCullPushConstants(parameters, 16u);
	(void)dispatchParams;
	const auto &nearPlane = frustumPlanes[4];
	if (nearPlane[0] != 0.0f || nearPlane[1] != 0.0f || nearPlane[2] != -1.0f) {
		std::fprintf(stderr, "Test failure at line %d: near plane normal mismatch (got %f %f %f)\n", __LINE__, nearPlane[0], nearPlane[1], nearPlane[2]);
		++context.failures;
	}
	if (nearPlane[3] != 0.5f) {
		std::fprintf(stderr, "Test failure at line %d: near plane offset expected 0.5 (got %f)\n", __LINE__, nearPlane[3]);
		++context.failures;
	}
}

void TestBuildMeshCullPushConstantsFrustumFarPlane(TestContext &context)
{
	ChunkCullingParameters parameters{};
	parameters.cameraPositionAndMaxDistance = {0.0f, 0.0f, 0.0f, 100.0f};
	parameters.cameraForwardAndTanHalfVerticalFov = {0.0f, 0.0f, -1.0f, 0.0f};
	parameters.cameraRightAndTanHalfHorizontalFov = {1.0f, 0.0f, 0.0f, 0.0f};
	parameters.cameraUpAndNearPlane = {0.0f, 1.0f, 0.0f, 0.5f};

	const auto [dispatchParams, frustumPlanes] =
		projectv::render::BuildMeshCullPushConstants(parameters, 16u);
	(void)dispatchParams;
	const auto &farPlane = frustumPlanes[5];
	if (farPlane[0] != 0.0f || farPlane[1] != 0.0f || farPlane[2] != 1.0f) {
		std::fprintf(stderr, "Test failure at line %d: far plane normal mismatch (got %f %f %f)\n", __LINE__, farPlane[0], farPlane[1], farPlane[2]);
		++context.failures;
	}
}

}  // namespace

int main()
{
	TestContext context{};
	TestMeshShaderPipelineRequestedDefaultOff(context);
	TestBuildMeshCullPushConstantsDispatchParams(context);
	TestBuildMeshCullPushConstantsFrustumForwardPlane(context);
	TestBuildMeshCullPushConstantsFrustumFarPlane(context);
	if (context.failures != 0) {
		return EXIT_FAILURE;
	}
	std::puts("ProjectVMeshShaderTests passed");
	return EXIT_SUCCESS;
}

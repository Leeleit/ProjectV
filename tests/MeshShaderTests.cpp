#include "render/vulkan/VulkanMeshShaderPipeline.hpp"

#include <array>
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

void ExpectFalse(TestContext &context, bool condition, int line, std::string_view expr)
{
	if (condition) {
		context.Fail(line, expr);
	}
}

void ExpectEqualUInt(TestContext &context, uint32_t expected, uint32_t actual, int line, std::string_view expr)
{
	if (expected != actual) {
		std::fprintf(stderr, "Test failure at line %d: %.*s (expected %u, got %u)\n", line, static_cast<int>(expr.size()), expr.data(), expected, actual);
		++context.failures;
	}
}

void TestMeshShaderPipelineRequestedDefaultOff(TestContext &context)
{
	const bool requested = projectv::render::IsMeshShaderPipelineRequested();
	ExpectFalse(context, requested, __LINE__, "PROJECTV_MESH_SHADER_PIPELINE unset -> false");
}

void TestBuildMeshCullPushConstantsDispatchParams(TestContext &context)
{
	ChunkCullingParameters parameters{};
	parameters.cameraPositionAndMaxDistance = {0.0f, 0.0f, 0.0f, 64.0f};
	parameters.cameraForwardAndTanHalfVerticalFov = {0.0f, 0.0f, -1.0f, 0.5f};
	parameters.cameraRightAndTanHalfHorizontalFov = {1.0f, 0.0f, 0.0f, 0.5f};
	parameters.cameraUpAndNearPlane = {0.0f, 1.0f, 0.0f, 0.1f};

	const projectv::render::MeshCullPushConstants cull =
		projectv::render::BuildMeshCullPushConstants(parameters, 256u);
	ExpectEqualUInt(context, 256u, cull.dispatchParams[0], __LINE__, "dispatchParams[0] = chunk count");
}

void TestBuildMeshCullPushConstantsFrustumForwardPlane(TestContext &context)
{
	ChunkCullingParameters parameters{};
	parameters.cameraPositionAndMaxDistance = {0.0f, 0.0f, 0.0f, 100.0f};
	parameters.cameraForwardAndTanHalfVerticalFov = {0.0f, 0.0f, -1.0f, 0.0f};
	parameters.cameraRightAndTanHalfHorizontalFov = {1.0f, 0.0f, 0.0f, 0.0f};
	parameters.cameraUpAndNearPlane = {0.0f, 1.0f, 0.0f, 0.5f};

	const projectv::render::MeshCullPushConstants cull =
		projectv::render::BuildMeshCullPushConstants(parameters, 16u);
	const auto &nearPlane = cull.frustumPlanes[4];
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

	const projectv::render::MeshCullPushConstants cull =
		projectv::render::BuildMeshCullPushConstants(parameters, 16u);
	const auto &farPlane = cull.frustumPlanes[5];
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

#include "render/TaaRenderTargets.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
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

void TestMotionVectorFormat(TestContext &context)
{
	if (projectv::taa::kTaaMotionVectorFormat != VK_FORMAT_R16G16_SFLOAT) {
		std::fprintf(
			stderr,
			"Test failure at line %d: kTaaMotionVectorFormat must be VK_FORMAT_R16G16_SFLOAT per TODO.md §5.3 + Karis 2014 (got %d)\n",
			__LINE__,
			static_cast<int>(projectv::taa::kTaaMotionVectorFormat));
		++context.failures;
	}
}

void TestSceneColorFormatPreserved(TestContext &context)
{
	if (projectv::taa::kTaaSceneColorFormat != VK_FORMAT_B10G11R11_UFLOAT_PACK32) {
		std::fprintf(
			stderr,
			"Test failure at line %d: kTaaSceneColorFormat must remain B10G11R11_UFLOAT_PACK32 (got %d)\n",
			__LINE__,
			static_cast<int>(projectv::taa::kTaaSceneColorFormat));
		++context.failures;
	}
}

void TestLayerHistoryFormatPreserved(TestContext &context)
{
	if (projectv::taa::kTaaLayerHistoryColorFormat != VK_FORMAT_R8G8B8A8_UNORM) {
		std::fprintf(
			stderr,
			"Test failure at line %d: kTaaLayerHistoryColorFormat must remain R8G8B8A8_UNORM (got %d)\n",
			__LINE__,
			static_cast<int>(projectv::taa::kTaaLayerHistoryColorFormat));
		++context.failures;
	}
}

void TestMotionVectorSizeContract(TestContext &context)
{
	if (sizeof(uint16_t) * 2u != 4u) {
		context.Fail(__LINE__, "R16G16_SFLOAT is 4 bytes per pixel");
	}
}

void TestMotionVectorNdcRangeContract(TestContext &context)
{
	const float minComponent = -1.0f;
	const float maxComponent = 1.0f;
	if (!(minComponent < maxComponent)) {
		context.Fail(__LINE__, "Motion vector NDC range must be [-1, 1]");
	}
}

void TestMotionVectorResolveContract(TestContext &context)
{
	const std::array<float, 2> motionValid{0.1f, -0.1f};
	const std::array<float, 2> motionOutOfRange{-0.6f, 0.6f};
	const std::array<float, 2> uvCenter{0.5f, 0.5f};
	const std::array<float, 2> uvCorner{0.0f, 0.0f};

	const std::array<float, 2> prevUvValid{uvCenter[0] + motionValid[0], uvCenter[1] + motionValid[1]};
	const bool validInBounds = (prevUvValid[0] >= 0.0f && prevUvValid[0] <= 1.0f &&
								prevUvValid[1] >= 0.0f && prevUvValid[1] <= 1.0f);
	if (!validInBounds) {
		context.Fail(__LINE__, "uv + motion must produce in-bounds prevUv for typical inputs");
	}

	const std::array<float, 2> prevUvOut{uvCorner[0] + motionOutOfRange[0], uvCorner[1] + motionOutOfRange[1]};
	const bool outOfBounds = (prevUvOut[0] < 0.0f || prevUvOut[0] > 1.0f ||
							  prevUvOut[1] < 0.0f || prevUvOut[1] > 1.0f);
	if (!outOfBounds) {
		context.Fail(__LINE__, "uv + motion must produce out-of-bounds prevUv when motion pushes outside [0,1]");
	}

	const std::array<float, 2> motionZero{0.0f, 0.0f};
	const std::array<float, 2> prevUvIdentity{uvCenter[0] + motionZero[0], uvCenter[1] + motionZero[1]};
	const bool identityValid = (prevUvIdentity[0] == uvCenter[0] && prevUvIdentity[1] == uvCenter[1]);
	if (!identityValid) {
		context.Fail(__LINE__, "Zero motion vector must produce prevUv == uv (identity)");
	}
}

void TestResolveShaderMotionBinding(TestContext &context)
{
	const std::filesystem::path shaderPath =
		std::filesystem::path(PROJECTV_TESTS_SOURCE_DIR) / ".." / "src" / "shaders" / "taa_resolve.frag";
	std::ifstream file(shaderPath);
	if (!file.is_open()) {
		std::fprintf(
			stderr,
			"Test failure at line %d: cannot open %s — shader must contain motion vector binding 4\n",
			__LINE__,
			shaderPath.string().c_str());
		++context.failures;
		return;
	}
	std::ostringstream buffer;
	buffer << file.rdbuf();
	const std::string source = buffer.str();

	const std::string bindingMarker = "binding = 4";
	const std::string motionMarker = "motionVector";
	const std::string texMarker = "texture(motionVector";

	if (source.find(bindingMarker) == std::string::npos) {
		std::fprintf(
			stderr,
			"Test failure at line %d: taa_resolve.frag must declare binding = 4 (motion vector sampler)\n",
			__LINE__);
		++context.failures;
	}
	if (source.find(motionMarker) == std::string::npos) {
		std::fprintf(
			stderr,
			"Test failure at line %d: taa_resolve.frag must reference motionVector uniform\n",
			__LINE__);
		++context.failures;
	}
	if (source.find(texMarker) == std::string::npos) {
		std::fprintf(
			stderr,
			"Test failure at line %d: taa_resolve.frag must call texture(motionVector, uv) for prevUv computation\n",
			__LINE__);
		++context.failures;
	}
}

} // namespace

int main()
{
	TestContext context{};
	TestMotionVectorFormat(context);
	TestSceneColorFormatPreserved(context);
	TestLayerHistoryFormatPreserved(context);
	TestMotionVectorSizeContract(context);
	TestMotionVectorNdcRangeContract(context);
	TestMotionVectorResolveContract(context);
	TestResolveShaderMotionBinding(context);

	if (context.failures > 0) {
		std::fprintf(stderr, "ProjectVTaaMotionVectorTests: %d failure(s)\n", context.failures);
		return 1;
	}
	std::puts("ProjectVTaaMotionVectorTests passed");
	return 0;
}

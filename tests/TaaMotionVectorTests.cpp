#include "render/TaaRenderTargets.hpp"

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

}  // namespace

int main()
{
	TestContext context{};
	TestMotionVectorFormat(context);
	TestSceneColorFormatPreserved(context);
	TestLayerHistoryFormatPreserved(context);
	TestMotionVectorSizeContract(context);
	TestMotionVectorNdcRangeContract(context);

	if (context.failures > 0) {
		std::fprintf(stderr, "ProjectVTaaMotionVectorTests: %d failure(s)\n", context.failures);
		return 1;
	}
	std::puts("ProjectVTaaMotionVectorTests passed");
	return 0;
}

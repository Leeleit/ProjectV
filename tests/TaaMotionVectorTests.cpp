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

}  // namespace

int main()
{
	TestContext context{};
	TestMotionVectorFormat(context);
	TestSceneColorFormatPreserved(context);
	TestLayerHistoryFormatPreserved(context);

	if (context.failures > 0) {
		std::fprintf(stderr, "ProjectVTaaMotionVectorTests: %d failure(s)\n", context.failures);
		return 1;
	}
	std::puts("ProjectVTaaMotionVectorTests passed");
	return 0;
}

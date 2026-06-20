#include "render/HizCulling.hpp"

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

void TestHzbDisabledByDefault(TestContext &context)
{
	const bool enabled = projectv::render::IsHzbCullingEnabled();
	ExpectFalse(context, enabled, __LINE__, "HZB culling disabled without env");
}

void TestComputeHzbMipLevelCountSquare(TestContext &context)
{
	const uint32_t levels = projectv::render::ComputeHzbMipLevelCount(1024u, 1024u);
	ExpectEqualUInt(context, 11u, levels, __LINE__, "1024x1024 -> 11 mip levels (1+log2(1024))");
}

void TestComputeHzbMipLevelCountNonSquare(TestContext &context)
{
	const uint32_t levels = projectv::render::ComputeHzbMipLevelCount(800u, 600u);
	ExpectEqualUInt(context, 10u, levels, __LINE__, "800x600 -> 10 mip levels (1+log2(600))");
}

void TestComputeHzbMipLevelCountTiny(TestContext &context)
{
	const uint32_t levels = projectv::render::ComputeHzbMipLevelCount(1u, 1u);
	ExpectEqualUInt(context, 1u, levels, __LINE__, "1x1 -> 1 mip level");
}

}  // namespace

int main()
{
	TestContext context{};
	TestHzbDisabledByDefault(context);
	TestComputeHzbMipLevelCountSquare(context);
	TestComputeHzbMipLevelCountNonSquare(context);
	TestComputeHzbMipLevelCountTiny(context);
	if (context.failures != 0) {
		return EXIT_FAILURE;
	}
	std::puts("ProjectVHzbCullingTests passed");
	return EXIT_SUCCESS;
}

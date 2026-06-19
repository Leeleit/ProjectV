

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#ifndef PROJECTV_TESTS_SOURCE_DIR
#define PROJECTV_TESTS_SOURCE_DIR "."
#endif

namespace {

struct TestContext {
	int failures = 0;

	void Fail(const int line, const std::string_view message)
	{
		std::fprintf(stderr, "Test failure at line %d: %.*s\n", line, static_cast<int>(message.size()), message.data());
		++failures;
	}
};

#define PV_EXPECT_TRUE(ctx, cond, msg)   \
	do {                                 \
		if (!(cond)) {                   \
			(ctx).Fail(__LINE__, (msg)); \
		}                                \
	} while (0)

void TestGenerateAndValidateHeader(TestContext &ctx)
{
	const std::filesystem::path fixturePath = std::string(PROJECTV_TESTS_SOURCE_DIR) + "/fixtures/box_uv.glb";
	std::ifstream input(fixturePath, std::ios::binary);
	PV_EXPECT_TRUE(
		ctx,
		input.good(),
		"tests/fixtures/box_uv.glb must be present on disk (run GenerateBoxUvFixture first)");

	uint32_t header[3] = {0, 0, 0};
	input.read(reinterpret_cast<char *>(header), sizeof(header));
	PV_EXPECT_TRUE(
		ctx,
		header[0] == 0x46546C67u,
		"GLB header magic must be 0x46546C67 (the 'glTF' ASCII tag)");
	PV_EXPECT_TRUE(
		ctx,
		header[1] == 2u,
		"GLB header version slot must read 2 (was 0); version and length are easy to swap and fastgltf reports the symptom as 'glTF version is not supported'");

	const auto fileSize = static_cast<uint32_t>(std::filesystem::file_size(fixturePath));
	PV_EXPECT_TRUE(
		ctx,
		header[2] == fileSize,
		"GLB header length slot must equal the file size in bytes");
}

void TestFixtureIsAtLeastReasonableSize(TestContext &ctx)
{

	const std::filesystem::path fixturePath = std::string(PROJECTV_TESTS_SOURCE_DIR) + "/fixtures/box_uv.glb";
	const auto fileSize = std::filesystem::file_size(fixturePath);
	PV_EXPECT_TRUE(
		ctx,
		fileSize > 1000,
		"box_uv.glb is suspiciously small; an attribute is probably missing");
}

} // namespace

int main()
{
	TestContext ctx;
	TestGenerateAndValidateHeader(ctx);
	TestFixtureIsAtLeastReasonableSize(ctx);
	if (ctx.failures == 0) {
		std::printf("ProjectVBoxUvFixtureTests: 2/2 passed\n");
		return 0;
	}
	std::fprintf(stderr, "ProjectVBoxUvFixtureTests: %d failure(s)\n", ctx.failures);
	return 1;
}

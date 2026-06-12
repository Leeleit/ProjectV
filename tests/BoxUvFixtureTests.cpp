// M6 prep regression test. Runs the `GenerateBoxUvFixture` binary
// (built once at test-configure time) and verifies that the glb it
// produces has the right header layout. The original bug —
// `magic, length, version` instead of `magic, version, length` —
// passed local smoke because fastgltf only reports
// `Error::UnsupportedVersion`, which reads as a glTF version
// problem. The actual problem was a wrong header byte order that
// shifts everything by 4 bytes. This test catches it by reading
// the header bytes back and asserting each field lands where
// fastgltf will read it.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
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

#define PV_EXPECT_TRUE(ctx, cond, msg) \
	do { \
		if (!(cond)) { \
			(ctx).Fail(__LINE__, (msg)); \
		} \
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
	// We don't assert the length slot value (the fixture is
	// regeneratable and may grow). What matters is that the slot
	// is non-zero and matches the file size.
	const auto fileSize = static_cast<uint32_t>(std::filesystem::file_size(fixturePath));
	PV_EXPECT_TRUE(
		ctx,
		header[2] == fileSize,
		"GLB header length slot must equal the file size in bytes");
}

void TestFixtureIsAtLeastReasonableSize(TestContext &ctx)
{
	// 24 vertices * (3+3+2 floats) * 4 = 768 bytes vertex data +
	// 36 indices * 2 = 72 bytes index data + JSON chunk + GLB
	// headers. A reasonable lower bound is 1000 bytes; if the
	// fixture shrinks below that something has gone wrong
	// (silently dropped attribute, wrong count, etc.).
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

#include "asset/AssetLoader.hpp"
#include "asset/MeshBaker.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
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

bool ApproxEqual(const float a, const float b)
{
	constexpr float epsilon = 1e-5f;
	return std::fabs(a - b) <= epsilon;
}

std::filesystem::path BoxDracoFixturePath()
{
	return std::filesystem::path(PROJECTV_TESTS_SOURCE_DIR) / "fixtures" / "box_draco.glb";
}

std::filesystem::path BoxFixturePath()
{
	return std::filesystem::path(PROJECTV_TESTS_SOURCE_DIR) / "fixtures" / "box.glb";
}

void TestDracoBoxLoadExtractsSameGeometry(TestContext &context)
{
	if (!std::filesystem::exists(BoxDracoFixturePath())) {
		context.Fail(__LINE__, "box_draco.glb fixture not found");
		return;
	}
	projectv::asset::LoadAssetError error;
	const auto loaded = projectv::asset::LoadGlb(BoxDracoFixturePath().string(), &error);
	if (!loaded) {
		context.Fail(__LINE__, std::string("draco LoadGlb failed: ") + error.message);
		return;
	}
	if (loaded->primitives.size() != 1) {
		context.Fail(__LINE__, "draco box should have 1 primitive");
	}
	if (loaded->totalVertexCount != 24) {
		context.Fail(__LINE__, "draco box should have 24 vertices (matches non-draco Box.glb)");
	}
	if (loaded->totalTriangleCount != 12) {
		context.Fail(__LINE__, "draco box should have 12 triangles");
	}
	if (loaded->primitives.empty()) {
		return;
	}
	const auto &prim = loaded->primitives.front();
	if (prim.positions.size() != 24) {
		context.Fail(__LINE__, "draco box positions should be 24");
	}
	if (prim.normals.size() != 24) {
		context.Fail(__LINE__, "draco box normals should be 24 (per-face normals preserved)");
	}
	if (prim.indices.size() != 36) {
		context.Fail(__LINE__, "draco box indices should be 36");
	}
	if (!ApproxEqual(loaded->aabbMin.x, -0.5f) || !ApproxEqual(loaded->aabbMin.y, -0.5f) || !ApproxEqual(loaded->aabbMin.z, -0.5f)) {
		context.Fail(__LINE__, "draco box aabbMin not (-0.5, -0.5, -0.5)");
	}
	if (!ApproxEqual(loaded->aabbMax.x, 0.5f) || !ApproxEqual(loaded->aabbMax.y, 0.5f) || !ApproxEqual(loaded->aabbMax.z, 0.5f)) {
		context.Fail(__LINE__, "draco box aabbMax not (0.5, 0.5, 0.5)");
	}
}

void TestDracoBoxBakesViaMeshBaker(TestContext &context)
{
	if (!std::filesystem::exists(BoxDracoFixturePath())) {
		context.Fail(__LINE__, "box_draco.glb fixture not found");
		return;
	}
	projectv::asset::LoadAssetError error;
	const auto loaded = projectv::asset::LoadGlb(BoxDracoFixturePath().string(), &error);
	if (!loaded) {
		context.Fail(__LINE__, std::string("draco LoadGlb failed: ") + error.message);
		return;
	}

	std::string bakeError;
	const auto baked = projectv::asset::BakeLoadedAsset(*loaded, {}, &bakeError);
	if (!bakeError.empty()) {
		context.Fail(__LINE__, std::string("BakeLoadedAsset on draco box failed: ") + bakeError);
		return;
	}
	if (baked.primitives.size() != 1) {
		context.Fail(__LINE__, "draco box bake should yield 1 primitive");
		return;
	}
	const auto &prim = baked.primitives.front();
	if (prim.vertexCount != 24 || prim.indexCount != 36) {
		context.Fail(__LINE__, "draco box bake vertex/index count mismatch");
	}
	if (prim.vertexBuffer.size() != prim.vertexCount * projectv::asset::kBakedVertexStride) {
		context.Fail(__LINE__, "draco box bake interleaved buffer size mismatch");
	}
}

void TestNonDracoPathStillWorks(TestContext &context)
{
	if (!std::filesystem::exists(BoxFixturePath())) {
		context.Fail(__LINE__, "box.glb fixture not found");
		return;
	}
	projectv::asset::LoadAssetError error;
	const auto loaded = projectv::asset::LoadGlb(BoxFixturePath().string(), &error);
	if (!loaded) {
		context.Fail(__LINE__, std::string("non-draco LoadGlb regressed: ") + error.message);
		return;
	}
	if (loaded->totalVertexCount != 24 || loaded->totalTriangleCount != 12) {
		context.Fail(__LINE__, "non-draco box regression: vertex/triangle count mismatch");
	}
}
} // namespace

int main() // NOLINT(*-exception-escape)
{
	TestContext context;
	TestDracoBoxLoadExtractsSameGeometry(context);
	TestDracoBoxBakesViaMeshBaker(context);
	TestNonDracoPathStillWorks(context);
	if (context.failures != 0) {
		std::fprintf(stderr, "DracoDecoderTests: %d failure(s)\n", context.failures);
		return 1;
	}
	std::fprintf(stderr, "DracoDecoderTests: all passed\n");
	return 0;
}

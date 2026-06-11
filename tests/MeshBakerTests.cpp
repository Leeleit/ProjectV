#include "asset/AssetLoader.hpp"
#include "asset/MeshBaker.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

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

bool ApproxEqual(const float a, const float b, const float epsilon = 1e-5f)
{
	return std::fabs(a - b) <= epsilon;
}

std::filesystem::path BoxFixturePath()
{
	return std::filesystem::path(PROJECTV_TESTS_SOURCE_DIR) / "fixtures" / "box.glb";
}

void TestBakeBoxReducesOrKeepsAcmrNearIdeal(TestContext &context)
{
	if (!std::filesystem::exists(BoxFixturePath())) {
		context.Fail(__LINE__, "box.glb fixture not found");
		return;
	}
	projectv::asset::LoadAssetError error;
	auto loaded = projectv::asset::LoadGlb(BoxFixturePath().string(), &error);
	if (!loaded) {
		context.Fail(__LINE__, std::string("LoadGlb failed: ") + error.message);
		return;
	}

	projectv::asset::BakeConfig config;
	projectv::asset::BakedMesh baked;
	std::string bakeError;
	baked = projectv::asset::BakeLoadedAsset(*loaded, config, &bakeError);

	if (!bakeError.empty()) {
		context.Fail(__LINE__, std::string("BakeLoadedAsset error: ") + bakeError);
		return;
	}
	if (baked.primitives.size() != 1) {
		context.Fail(__LINE__, "expected exactly one baked primitive");
		return;
	}
	const auto &prim = baked.primitives.front();
	if (prim.vertexCount != 24) {
		context.Fail(__LINE__, "expected 24 vertices after dedup");
	}
	if (prim.indexCount != 36) {
		context.Fail(__LINE__, "expected 36 indices");
	}
	if (prim.vertexBuffer.size() != prim.vertexCount * projectv::asset::kBakedVertexStride) {
		context.Fail(__LINE__, "interleaved vertex buffer size mismatch");
	}
	if (prim.overfetch < 0.5f || prim.overfetch > 2.0f) {
		context.Fail(__LINE__, "overfetch out of sane range");
	}

	const float indicesOverVertices = static_cast<float>(prim.indexCount)
		/ (3.0f * static_cast<float>(prim.vertexCount));
	if (indicesOverVertices > 2.0f) {
		context.Fail(__LINE__, "ACMR worse than 2.0 after meshopt pipeline");
	}
}

void TestBakePreservesAllIndicesDistinct(TestContext &context)
{
	if (!std::filesystem::exists(BoxFixturePath())) {
		context.Fail(__LINE__, "box.glb fixture not found");
		return;
	}
	projectv::asset::LoadAssetError error;
	auto loaded = projectv::asset::LoadGlb(BoxFixturePath().string(), &error);
	if (!loaded) {
		context.Fail(__LINE__, std::string("LoadGlb failed: ") + error.message);
		return;
	}
	const auto baked = projectv::asset::BakeLoadedAsset(*loaded);
	const auto &indices = baked.primitives.front().indices;
	for (size_t i = 0; i < indices.size(); i += 3) {
		const uint32_t a = indices[i];
		const uint32_t b = indices[i + 1];
		const uint32_t c = indices[i + 2];
		if (a == b || b == c || a == c) {
			context.Fail(__LINE__, "degenerate triangle in baked mesh");
			return;
		}
		if (a >= baked.primitives.front().vertexCount
			|| b >= baked.primitives.front().vertexCount
			|| c >= baked.primitives.front().vertexCount) {
			context.Fail(__LINE__, "out-of-bounds index after reordering");
			return;
		}
	}
}

void TestBakeDisabledOptimizersProducesSaneBuffer(TestContext &context)
{
	if (!std::filesystem::exists(BoxFixturePath())) {
		context.Fail(__LINE__, "box.glb fixture not found");
		return;
	}
	projectv::asset::LoadAssetError error;
	auto loaded = projectv::asset::LoadGlb(BoxFixturePath().string(), &error);
	if (!loaded) {
		context.Fail(__LINE__, std::string("LoadGlb failed: ") + error.message);
		return;
	}
	projectv::asset::BakeConfig config{};
	config.optimizeVertexCache = false;
	config.optimizeVertexFetch = false;
	const auto baked = projectv::asset::BakeLoadedAsset(*loaded, config);
	const auto &prim = baked.primitives.front();
	if (prim.vertexCount == 0 || prim.indexCount == 0) {
		context.Fail(__LINE__, "non-optimized bake still produced empty mesh");
	}
}

void TestBakeEmptyAssetReportsError(TestContext &context)
{
	projectv::asset::LoadedAsset empty;
	empty.sourcePath = "synthetic://empty";
	std::string error;
	const auto baked = projectv::asset::BakeLoadedAsset(empty, {}, &error);
	if (!baked.primitives.empty()) {
		context.Fail(__LINE__, "baking empty asset should yield zero primitives");
	}
	if (error.empty()) {
		context.Fail(__LINE__, "baking empty asset should populate error message");
	}
}
} // namespace

int main() // NOLINT(*-exception-escape)
{
	TestContext context;
	TestBakeBoxReducesOrKeepsAcmrNearIdeal(context);
	TestBakePreservesAllIndicesDistinct(context);
	TestBakeDisabledOptimizersProducesSaneBuffer(context);
	TestBakeEmptyAssetReportsError(context);
	if (context.failures != 0) {
		std::fprintf(stderr, "MeshBakerTests: %d failure(s)\n", context.failures);
		return 1;
	}
	std::fprintf(stderr, "MeshBakerTests: all passed\n");
	return 0;
}

#include "asset/AssetLoader.hpp"
#include "asset/AssetManifest.hpp"
#include "asset/AssetRegistry.hpp"

#include <array>
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

void TestLoadBoxGlbExtractsOnePrimitive(TestContext &context)
{
	if (!std::filesystem::exists(BoxFixturePath())) {
		context.Fail(__LINE__, "box.glb fixture not found at "
								  + BoxFixturePath().string());
		return;
	}

	projectv::asset::LoadAssetError error;
	auto loaded = projectv::asset::LoadGlb(BoxFixturePath().string(), &error);
	if (!loaded) {
		context.Fail(__LINE__, std::string("LoadGlb returned null: ") + error.message);
		return;
	}

	if (loaded->sourcePath.empty()) {
		context.Fail(__LINE__, "sourcePath is empty");
	}
	if (loaded->primitives.size() != 1) {
		context.Fail(__LINE__, "expected 1 primitive");
	}
	if (loaded->totalVertexCount != 24) {
		context.Fail(__LINE__, "expected 24 vertices (cube with per-face normals)");
	}
	if (loaded->totalTriangleCount != 12) {
		context.Fail(__LINE__, "expected 12 triangles");
	}
	if (loaded->primitives.empty()) {
		return;
	}
	const auto &prim = loaded->primitives.front();
	if (prim.positions.size() != 24) {
		context.Fail(__LINE__, "primitive.positions size mismatch");
	}
	if (prim.normals.size() != 24) {
		context.Fail(__LINE__, "primitive.normals size mismatch");
	}
	if (prim.uvs.size() != 0) {
		context.Fail(__LINE__, "primitive.uvs should be empty for Box.glb (no TEXCOORD_0)");
	}
	if (prim.indices.size() != 36) {
		context.Fail(__LINE__, "primitive.indices size mismatch");
	}
	if (!ApproxEqual(loaded->aabbMin.x, -0.5f) || !ApproxEqual(loaded->aabbMin.y, -0.5f)
		|| !ApproxEqual(loaded->aabbMin.z, -0.5f)) {
		context.Fail(__LINE__, "aabbMin not (-0.5, -0.5, -0.5)");
	}
	if (!ApproxEqual(loaded->aabbMax.x, 0.5f) || !ApproxEqual(loaded->aabbMax.y, 0.5f)
		|| !ApproxEqual(loaded->aabbMax.z, 0.5f)) {
		context.Fail(__LINE__, "aabbMax not (0.5, 0.5, 0.5)");
	}
}

void TestLoadBoxGlbReportsErrorForMissingFile(TestContext &context)
{
	projectv::asset::LoadAssetError error;
	auto loaded = projectv::asset::LoadGlb(
		(std::filesystem::path(PROJECTV_TESTS_SOURCE_DIR) / "fixtures" / "no_such.glb").string(),
		&error);
	if (loaded != nullptr) {
		context.Fail(__LINE__, "LoadGlb on missing file unexpectedly returned a value");
	}
	if (error.message.empty()) {
		context.Fail(__LINE__, "LoadGlb did not populate error message");
	}
	if (projectv::asset::GetAssetLoaderLastErrorMessage().empty()) {
		context.Fail(__LINE__, "GetAssetLoaderLastErrorMessage() returned empty");
	}
}

void TestManifestParsingDefaults(TestContext &context)
{
	using projectv::asset::ManifestEntry;
	using projectv::asset::ParseAssetManifestString;

	const auto empty = ParseAssetManifestString("");
	if (!empty.empty()) {
		context.Fail(__LINE__, "empty input should yield zero entries");
	}

	const auto single = ParseAssetManifestString("path/tree.glb");
	if (single.size() != 1) {
		context.Fail(__LINE__, "single path should yield one entry");
		return;
	}
	if (single[0].path != "path/tree.glb") {
		context.Fail(__LINE__, "single path mismatch");
	}
	if (single[0].id != "tree") {
		context.Fail(__LINE__, "single path default id should be file stem");
	}
	if (single[0].position != glm::vec3(0.0f)) {
		context.Fail(__LINE__, "missing transform should leave position at origin");
	}
	if (single[0].scale != 1.0f) {
		context.Fail(__LINE__, "missing transform should leave scale at 1.0");
	}

	const auto two = ParseAssetManifestString("a.glb; b.glb");
	if (two.size() != 2) {
		context.Fail(__LINE__, "two semi-separated paths should yield two entries");
	}
	if (two[1].id != "b") {
		context.Fail(__LINE__, "second id should default to file stem 'b'");
	}
}

void TestManifestParsingTransforms(TestContext &context)
{
	using projectv::asset::ParseAssetManifestString;

	const auto posOnly = ParseAssetManifestString("a.glb@1,2,3");
	if (posOnly.size() != 1) {
		context.Fail(__LINE__, "x,y,z transform should yield one entry");
		return;
	}
	if (posOnly[0].position != glm::vec3(1.0f, 2.0f, 3.0f)) {
		context.Fail(__LINE__, "x,y,z transform position mismatch");
	}
	if (posOnly[0].scale != 1.0f) {
		context.Fail(__LINE__, "x,y,z transform should leave scale at 1.0");
	}

	const auto fullTransform = ParseAssetManifestString("a.glb@1,2,3,30,45,0,2.5");
	if (fullTransform.size() != 1) {
		context.Fail(__LINE__, "full transform should yield one entry");
		return;
	}
	if (fullTransform[0].position != glm::vec3(1.0f, 2.0f, 3.0f)) {
		context.Fail(__LINE__, "full transform position mismatch");
	}
	if (fullTransform[0].rotationDegrees != glm::vec3(30.0f, 45.0f, 0.0f)) {
		context.Fail(__LINE__, "full transform rotation mismatch");
	}
	if (!ApproxEqual(fullTransform[0].scale, 2.5f)) {
		context.Fail(__LINE__, "full transform scale mismatch");
	}

	const auto bad = ParseAssetManifestString("a.glb@1,2");
	if (!bad.empty()) {
		context.Fail(__LINE__, "malformed transform (2 components) should be dropped");
	}
}

void TestAssetRegistry(TestContext &context)
{
	if (!std::filesystem::exists(BoxFixturePath())) {
		context.Fail(__LINE__, "box.glb fixture not found for registry test");
		return;
	}

	projectv::asset::AssetRegistry registry;
	if (registry.Size() != 0) {
		context.Fail(__LINE__, "registry should start empty");
	}

	if (!registry.Load("box", BoxFixturePath().string())) {
		context.Fail(__LINE__, std::string("registry.Load failed: ")
								  + std::string(projectv::asset::GetAssetLoaderLastErrorMessage()));
		return;
	}
	if (registry.Size() != 1) {
		context.Fail(__LINE__, "registry size should be 1 after one Load");
	}

	const auto *asset = registry.Get("box");
	if (asset == nullptr) {
		context.Fail(__LINE__, "registry.Get('box') returned nullptr");
		return;
	}
	if (asset->primitives.size() != 1) {
		context.Fail(__LINE__, "registry asset should have 1 primitive");
	}
	if (asset->totalVertexCount != 24) {
		context.Fail(__LINE__, "registry asset vertex count mismatch");
	}

	registry.Unload("box");
	if (registry.Size() != 0) {
		context.Fail(__LINE__, "registry should be empty after Unload");
	}
	if (registry.Get("box") != nullptr) {
		context.Fail(__LINE__, "registry.Get after Unload should be null");
	}
}
} // namespace

int main() // NOLINT(*-exception-escape)
{
	TestContext context;
	TestLoadBoxGlbExtractsOnePrimitive(context);
	TestLoadBoxGlbReportsErrorForMissingFile(context);
	TestManifestParsingDefaults(context);
	TestManifestParsingTransforms(context);
	TestAssetRegistry(context);
	if (context.failures != 0) {
		std::fprintf(stderr, "AssetLoaderTests: %d failure(s)\n", context.failures);
		return 1;
	}
	std::fprintf(stderr, "AssetLoaderTests: all passed\n");
	return 0;
}

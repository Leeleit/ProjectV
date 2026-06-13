#include "asset/AssetLoader.hpp"
#include "asset/AssetManifest.hpp"
#include "asset/AssetRegistry.hpp"

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

// `CppDFAConstantParameter` false positive: the DFA reads the
// default `1e-5f` as the only call value, but real call sites
// pass `1e-3f` (glTF float-quantisation tolerance, see
// `TestBakeBoxUvAcceptsRoundingToOnePercent`) and other
// task-specific epsilons.
// noinspection CppDFAConstantParameter
bool ApproxEqual(const float a, const float b, const float epsilon = 1e-5f)
{
	return std::fabs(a - b) <= epsilon;
}

std::filesystem::path BoxFixturePath()
{
	return std::filesystem::path(PROJECTV_TESTS_SOURCE_DIR) / "fixtures" / "box.glb";
}

std::filesystem::path UntitledColonadaFixturePath()
{
	return std::filesystem::path(PROJECTV_TESTS_SOURCE_DIR) / "fixtures" / "Untitled.colonada.glb";
}

void TestLoadBoxGlbExtractsOnePrimitive(TestContext &context)
{
	if (!std::filesystem::exists(BoxFixturePath())) {
		context.Fail(__LINE__, "box.glb fixture not found at " + BoxFixturePath().string());
		return;
	}

	projectv::asset::LoadAssetError error;
	const auto loaded = projectv::asset::LoadGlb(BoxFixturePath().string(), &error);
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
	if (!prim.uvs.empty()) {
		context.Fail(__LINE__, "primitive.uvs should be empty for Box.glb (no TEXCOORD_0)");
	}
	if (prim.indices.size() != 36) {
		context.Fail(__LINE__, "primitive.indices size mismatch");
	}
	if (!ApproxEqual(loaded->aabbMin.x, -0.5f) || !ApproxEqual(loaded->aabbMin.y, -0.5f) || !ApproxEqual(loaded->aabbMin.z, -0.5f)) {
		context.Fail(__LINE__, "aabbMin not (-0.5, -0.5, -0.5)");
	}
	if (!ApproxEqual(loaded->aabbMax.x, 0.5f) || !ApproxEqual(loaded->aabbMax.y, 0.5f) || !ApproxEqual(loaded->aabbMax.z, 0.5f)) {
		context.Fail(__LINE__, "aabbMax not (0.5, 0.5, 0.5)");
	}
}

void TestLoadBoxGlbReportsErrorForMissingFile(TestContext &context)
{
	projectv::asset::LoadAssetError error;
	const auto loaded = projectv::asset::LoadGlb(
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
		context.Fail(__LINE__, std::string("registry.Load failed: ") + std::string(projectv::asset::GetAssetLoaderLastErrorMessage()));
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

void TestComputeGlbDimensionsReportsBoxFixture(TestContext &context)
{
	// Sanity check on the well-known box fixture before reporting
	// dimensions of friend-supplied models. The box is 1x1x1 centered
	// at the origin, so size = (1, 1, 1) and aabbMin = (-0.5, -0.5, -0.5).
	if (!std::filesystem::exists(BoxFixturePath())) {
		context.Fail(__LINE__, "box.glb fixture not found");
		return;
	}
	projectv::asset::LoadAssetError error;
	const auto dims = projectv::asset::ComputeGlbDimensions(BoxFixturePath().string(), &error);
	if (!dims) {
		context.Fail(__LINE__, std::string("ComputeGlbDimensions(box.glb) returned nullopt: ") + error.message);
		return;
	}
	if (!ApproxEqual(dims->size.x, 1.0f) || !ApproxEqual(dims->size.y, 1.0f) || !ApproxEqual(dims->size.z, 1.0f)) {
		context.Fail(__LINE__, "box.glb size is not (1, 1, 1)");
	}
}

void TestComputeGlbDimensionsReportsUntitledColonadaFixture(TestContext &context)
{
	// The friend-supplied lamp-post model — a multi-mesh glTF that
	// uses the node hierarchy for layout (Cylinder + Cube + Sphere
	// nodes, each with its own TRS). The expected AABB after
	// walking the node hierarchy is:
	//   - Cylinder is a horizontal cylinder (-2.32, -1, -3.42)..(3.68, 1.73, 4.71)
	//     in glb space, then the node TRS = T(-0.04, 5.94, 0) * S(1, 5, 1)
	//     lifts the long axis to vertical, so the cylinder's world
	//     AABB is ~6×13.5×8.1.
	//   - Cube is a unit cube scaled (1.26, 0.52, 1.35), translated
	//     to (-0.01, 0.61, 0.07) — the wide base of the lamp post.
	//   - Sphere is a unit sphere scaled 0.61, translated to
	//     (-1.40, 12.49, 3.89) — the lamp head at the top of the arm.
	// The combined AABB after node transforms is roughly
	//   min = (-2.36, 0.08, -3.42)
	//   max = ( 3.64, 14.57,  4.71)
	//   size = (5.99, 14.49, 8.13)  — vertical post 14 voxels tall
	// The pre-M5.1c loader would have returned the Cylinder's
	// raw glb-space AABB of (6.0, 2.7, 8.1) and rendered a
	// horizontal cylinder, not a lamp post. This test would have
	// failed silently — the model would just look wrong in
	// VoxelLab without any unit test catching the regression.
	const auto path = UntitledColonadaFixturePath();
	if (!std::filesystem::exists(path)) {
		context.Fail(__LINE__, "Untitled.colonada.glb fixture not found at " + path.string());
		return;
	}
	projectv::asset::LoadAssetError error;
	const auto dims = projectv::asset::ComputeGlbDimensions(path.string(), &error);
	if (!dims) {
		context.Fail(__LINE__, std::string("ComputeGlbDimensions(Untitled.colonada.glb) returned nullopt: ") + error.message);
		return;
	}
	// After node-hierarchy walk, the lamp post is ~6 wide × ~14.5
	// tall × ~8.1 deep. Width/depth come from the Cylinder's
	// Z (post) and Z (arm) extents. The Y dim is the Cylinder's
	// Y dim (2.73) scaled by node scale 5 = 13.6, plus the Sphere
	// and Cube contributions. We assert 6 ≤ X ≤ 6.1, 14 ≤ Y ≤ 15,
	// 8 ≤ Z ≤ 8.2 to leave room for floating-point tolerance.
	constexpr float xMin = 5.5f, xMax = 6.5f;
	constexpr float yMin = 14.0f, yMax = 15.0f;
	constexpr float zMin = 7.5f, zMax = 8.5f;
	if (dims->size.x < xMin || dims->size.x > xMax) {
		std::fprintf(stderr, "Untitled.colonada X size=%.3f, expected in [%.1f, %.1f]\n", dims->size.x, xMin, xMax);
		context.Fail(__LINE__, "X dim is not ~6 — node hierarchy not applied?");
	}
	if (dims->size.y < yMin || dims->size.y > yMax) {
		std::fprintf(stderr, "Untitled.colonada Y size=%.3f, expected in [%.1f, %.1f] (cylinder scale 5 should make this ~13.6)\n", dims->size.y, yMin, yMax);
		context.Fail(__LINE__, "Y dim is not ~14.5 — node hierarchy not applied?");
	}
	if (dims->size.z < zMin || dims->size.z > zMax) {
		std::fprintf(stderr, "Untitled.colonada Z size=%.3f, expected in [%.1f, %.1f]\n", dims->size.z, zMin, zMax);
		context.Fail(__LINE__, "Z dim is not ~8.1 — node hierarchy not applied?");
	}
	// Print the AABB for human inspection (one-shot debugging —
	// the reusable inspector is `tools/compute-glb-dimensions`).
	std::fprintf(stderr,
				 "ComputeGlbDimensions: path=%s aabbMin=(%.6f %.6f %.6f) aabbMax=(%.6f %.6f %.6f) size=(%.6f %.6f %.6f)\n",
				 path.string().c_str(),
				 dims->aabbMin.x, dims->aabbMin.y, dims->aabbMin.z,
				 dims->aabbMax.x, dims->aabbMax.y, dims->aabbMax.z,
				 dims->size.x, dims->size.y, dims->size.z);
}

void TestComputeVoxelAlignedAabbBoxFixture(TestContext &context)
{
	// 1x1x1 box already on the voxel grid. Identity: no scaling, no
	// shift. The helper should round-trip.
	constexpr glm::vec3 aabbMin(0.0f, 1.0f, 0.0f);
	constexpr glm::vec3 aabbMax(1.0f, 2.0f, 1.0f);
	const auto &[alignedAabbMin, alignedAabbMax] = projectv::asset::ComputeVoxelAlignedAabb(aabbMin, aabbMax, 1.0f);
	if (!ApproxEqual(alignedAabbMin.x, 0.0f) || !ApproxEqual(alignedAabbMin.y, 1.0f) || !ApproxEqual(alignedAabbMin.z, 0.0f)) {
		context.Fail(__LINE__, "box fixture: aligned aabbMin not (0, 1, 0)");
	}
	if (!ApproxEqual(alignedAabbMax.x, 1.0f) || !ApproxEqual(alignedAabbMax.y, 2.0f) || !ApproxEqual(alignedAabbMax.z, 1.0f)) {
		context.Fail(__LINE__, "box fixture: aligned aabbMax not (1, 2, 1)");
	}
}

void TestComputeVoxelAlignedAabbUntitledColonada(TestContext &context)
{
	// The friend-supplied column has src dims 6.0 x 2.7 x 8.1 (X, Y, Z).
	// After auto-scale to integer voxel dims: 6 x 3 x 8. With the
	// operator's integer position (-9, 0, 9), the XZ corner snap
	// is a no-op (round(-9) = -9, round(9) = 9), and the AABB min
	// stays at (-9, 0, 9). The Y is left untouched here because the
	// ground-snap step (which is not part of this pure helper) is
	// what enforces `topVoxelY + 1`.
	constexpr glm::vec3 aabbMin(-9.0f, 0.0f, 9.0f);
	constexpr glm::vec3 aabbMax(-3.0f, 2.7f, 17.1f);
	const auto &[alignedAabbMin, alignedAabbMax] = projectv::asset::ComputeVoxelAlignedAabb(aabbMin, aabbMax, 1.0f);
	const float dimX = alignedAabbMax.x - alignedAabbMin.x;
	const float dimY = alignedAabbMax.y - alignedAabbMin.y;
	const float dimZ = alignedAabbMax.z - alignedAabbMin.z;
	if (!ApproxEqual(dimX, 6.0f) || !ApproxEqual(dimY, 3.0f) || !ApproxEqual(dimZ, 8.0f)) {
		std::fprintf(stderr,
					 "ComputeVoxelAlignedAabb untitled: got dims (%.6f, %.6f, %.6f), expected (6, 3, 8)\n",
					 dimX, dimY, dimZ);
		context.Fail(__LINE__, "untitled colonada: aligned dims are not (6, 3, 8)");
	}
	if (!ApproxEqual(alignedAabbMin.x, -9.0f) || !ApproxEqual(alignedAabbMin.z, 9.0f)) {
		std::fprintf(stderr,
					 "ComputeVoxelAlignedAabb untitled: got aabbMin=(%.6f, %.6f, %.6f), expected X=-9, Z=9\n",
					 alignedAabbMin.x, alignedAabbMin.y, alignedAabbMin.z);
		context.Fail(__LINE__, "untitled colonada: XZ corner snap did not preserve operator's integer position");
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
	TestComputeGlbDimensionsReportsBoxFixture(context);
	TestComputeGlbDimensionsReportsUntitledColonadaFixture(context);
	TestComputeVoxelAlignedAabbBoxFixture(context);
	TestComputeVoxelAlignedAabbUntitledColonada(context);
	if (context.failures != 0) {
		std::fprintf(stderr, "AssetLoaderTests: %d failure(s)\n", context.failures);
		return 1;
	}
	std::fprintf(stderr, "AssetLoaderTests: all passed\n");
	return 0;
}

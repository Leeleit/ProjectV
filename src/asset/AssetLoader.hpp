#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

namespace projectv::asset {

struct PrimitiveData {
	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec2> uvs;
	std::vector<uint32_t> indices;
	std::optional<size_t> materialIndex;
};

struct LoadedAsset {
	std::string sourcePath;
	std::vector<PrimitiveData> primitives;
	glm::vec3 aabbMin{0.0f};
	glm::vec3 aabbMax{0.0f};
	uint32_t totalVertexCount = 0;
	uint32_t totalTriangleCount = 0;
};

struct LoadAssetError {
	std::string message;
};

std::unique_ptr<LoadedAsset> LoadGlb(
	const std::string &path,
	LoadAssetError *outError = nullptr);

std::string_view GetAssetLoaderLastErrorMessage();

/// \brief Reusable dimension calculator for any .glb on disk.
///
/// \details
///  Loads the asset via `LoadGlb` (which decodes draco / extracts POSITION

///  accessors) and returns just the world-space AABB summary. Designed to

///  be called from tests, CLI tools, and runtime "what would this model

///  look like in the voxel grid" inspectors without each consumer having

///  to re-implement the LoadGlb error-handling boilerplate. Returns

///  `std::nullopt` if the file fails to load; on success `outMin`,

///  `outMax`, and `outSize` are populated.

///  This is the canonical entry point for "length / width / height of

///  a model" queries — see `ComputeGlbDimensions` in

///  `tools/compute-glb-dimensions.cpp` for the CLI wrapper and

///  `AssetLoaderTests.cpp::ComputeGlbDimensionsReportsBoxFixture` for

///  the unit test. Used by `SnapModelInstancesAboveGround` to pick the

///  per-axis auto-scale factors when the manifest entry's

///  `autoSnapToVoxelGrid` flag is set (so the model AABB lands on

///  integer voxel boundaries).

struct GlbDimensions {
	glm::vec3 aabbMin{0.0f};
	glm::vec3 aabbMax{0.0f};
	glm::vec3 size{0.0f}; // = aabbMax - aabbMin
};

std::optional<GlbDimensions> ComputeGlbDimensions(
	const std::string &path,
	LoadAssetError *outError = nullptr);

/// \brief Pure helper extracted from `SnapModelInstancesAboveGround` so the
///
/// \details
///  per-axis auto-scale + corner-snap math is unit-testable without a

///  full `VoxelWorld` / `RenderState` setup. Given the world-space

///  AABB of a model, returns the (aabbMin, aabbMax) pair the snap-pass

///  would converge to after the per-axis scale and XZ-round steps —

///  independent of the Y ground-snap (the caller is expected to apply

///  that separately). For a model already on the voxel grid (integer

///  dims, integer min) the result is identical to the input. Lives

///  here (NOT in `ModelManifestLoader.hpp`) so that the helper can be

///  unit-tested from `ProjectVAssetTests` without pulling

///  `core/Types.hpp` (and its `volk.h` chain) into the test target.

struct VoxelAlignedAabb {
	glm::vec3 aabbMin{0.0f};
	glm::vec3 aabbMax{0.0f};
};
VoxelAlignedAabb ComputeVoxelAlignedAabb(
	const glm::vec3 &aabbMin,
	const glm::vec3 &aabbMax,
	float voxelSize = 1.0f);

} // namespace projectv::asset


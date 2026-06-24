#pragma once

#include <cstdint> // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
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

[[nodiscard]] std::unique_ptr<LoadedAsset> LoadGlb(
	const std::string &path,
	LoadAssetError *outError = nullptr);

[[nodiscard]] std::string_view GetAssetLoaderLastErrorMessage();


struct GlbDimensions {
	glm::vec3 aabbMin{0.0f};
	glm::vec3 aabbMax{0.0f};
	glm::vec3 size{0.0f};
};

[[nodiscard]] std::optional<GlbDimensions> ComputeGlbDimensions(
	const std::string &path,
	LoadAssetError *outError = nullptr);


struct VoxelAlignedAabb {
	glm::vec3 aabbMin{0.0f};
	glm::vec3 aabbMax{0.0f};
};
VoxelAlignedAabb ComputeVoxelAlignedAabb(
	const glm::vec3 &aabbMin,
	const glm::vec3 &aabbMax,
	float voxelSize = 1.0f);

} // namespace projectv::asset


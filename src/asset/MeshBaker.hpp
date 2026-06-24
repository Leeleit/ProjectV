#pragma once

#include <cstdint> // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
#include <optional>
#include <string>
#include <vector>

#include "asset/AssetLoader.hpp"

namespace projectv::asset {

constexpr size_t kBakedVertexStride = sizeof(float) * 8;

struct BakedPrimitive {
	std::vector<uint8_t> vertexBuffer;
	std::vector<uint32_t> indices;
	uint32_t vertexCount = 0;
	uint32_t indexCount = 0;
	std::optional<size_t> materialIndex;
	float overfetch = 1.0f;
};

struct BakedMesh {
	std::vector<BakedPrimitive> primitives;
	float acmr = 0.0f;
	float atvr = 0.0f;
};

struct BakeConfig {
	bool optimizeVertexCache = true;
	bool optimizeVertexFetch = true;
};

BakedMesh BakeLoadedAsset(
	const LoadedAsset &asset,
	const BakeConfig &config = {},
	std::string *outError = nullptr);

} // namespace projectv::asset



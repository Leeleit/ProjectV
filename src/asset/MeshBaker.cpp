#include "asset/MeshBaker.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <utility>

#include <glm/glm.hpp>
#include <meshoptimizer.h>

namespace projectv::asset {

namespace {

struct LinearVertex {
	float position[3];
	float normal[3];
	float uv[2];
};

static_assert(sizeof(LinearVertex) == kBakedVertexStride, "baked vertex stride must match");

bool BakePrimitive(
	const PrimitiveData &src,
	const BakeConfig &config,
	BakedPrimitive &out)
{
	const size_t vertexCount = src.positions.size();
	if (vertexCount == 0) {
		return false;
	}
	if (src.indices.empty()) {
		return false;
	}
	if (src.indices.size() % 3 != 0) {
		return false;
	}

	const bool hasNormals = src.normals.size() == vertexCount;
	const bool hasUvs = src.uvs.size() == vertexCount;

	std::vector<LinearVertex> stagingVertices(vertexCount);
	for (size_t i = 0; i < vertexCount; ++i) {
		stagingVertices[i].position[0] = src.positions[i].x;
		stagingVertices[i].position[1] = src.positions[i].y;
		stagingVertices[i].position[2] = src.positions[i].z;
		stagingVertices[i].normal[0] = hasNormals ? src.normals[i].x : 0.0f;
		stagingVertices[i].normal[1] = hasNormals ? src.normals[i].y : 0.0f;
		stagingVertices[i].normal[2] = hasNormals ? src.normals[i].z : 1.0f;
		stagingVertices[i].uv[0] = hasUvs ? src.uvs[i].x : 0.0f;
		stagingVertices[i].uv[1] = hasUvs ? src.uvs[i].y : 0.0f;
	}

	std::vector<uint32_t> remappedIndices = src.indices;
	if (config.optimizeVertexCache) {
		meshopt_optimizeVertexCache(
			remappedIndices.data(),
			remappedIndices.data(),
			remappedIndices.size(),
			vertexCount);
	}

	std::vector<unsigned int> vertexRemap(vertexCount);
	const size_t uniqueCount = meshopt_generateVertexRemap(
		vertexRemap.data(),
		remappedIndices.data(),
		remappedIndices.size(),
		stagingVertices.data(),
		vertexCount,
		sizeof(LinearVertex));

	std::vector<LinearVertex> reorderedVertices(uniqueCount);
	std::vector<uint32_t> finalIndices(remappedIndices.size());
	meshopt_remapVertexBuffer(
		reorderedVertices.data(),
		stagingVertices.data(),
		vertexCount,
		sizeof(LinearVertex),
		vertexRemap.data());
	meshopt_remapIndexBuffer(
		finalIndices.data(),
		remappedIndices.data(),
		remappedIndices.size(),
		vertexRemap.data());

	if (config.optimizeVertexFetch) {
		meshopt_optimizeVertexFetch(
			reorderedVertices.data(),
			finalIndices.data(),
			finalIndices.size(),
			reorderedVertices.data(),
			uniqueCount,
			sizeof(LinearVertex));
	}

	const auto [bytes_fetched, overfetch] = meshopt_analyzeVertexFetch(
		finalIndices.data(),
		finalIndices.size(),
		uniqueCount,
		sizeof(LinearVertex));

	out.vertexBuffer.resize(uniqueCount * sizeof(LinearVertex));
	std::memcpy(out.vertexBuffer.data(), reorderedVertices.data(), out.vertexBuffer.size());
	out.indices = std::move(finalIndices);
	out.vertexCount = static_cast<uint32_t>(uniqueCount);
	out.indexCount = static_cast<uint32_t>(out.indices.size());
	out.materialIndex = src.materialIndex;
	out.overfetch = overfetch;
	return true;
}

} // namespace

BakedMesh BakeLoadedAsset(
	const LoadedAsset &asset,
	const BakeConfig &config,
	std::string *outError)
{
	BakedMesh result;
	if (outError) {
		outError->clear();
	}

	uint64_t totalIndexTriples = 0;
	uint64_t totalUniqueVertices = 0;

	for (size_t i = 0; i < asset.primitives.size(); ++i) {
		BakedPrimitive bakedPrim;
		if (!BakePrimitive(asset.primitives[i], config, bakedPrim)) {
			if (outError) {
				*outError = "BakePrimitive failed for primitive " + std::to_string(i);
			}
			return {};
		}
		totalIndexTriples += bakedPrim.indices.size() / 3;
		totalUniqueVertices += bakedPrim.vertexCount;
		result.primitives.push_back(std::move(bakedPrim));
	}

	if (totalUniqueVertices == 0) {
		if (outError) {
			*outError = "asset has no bakeable primitives";
		}
		return {};
	}

	// ACMR (Average Cache Miss Ratio) = vertex shader invocations / triangle count.
	// Ideal 0.5; well-optimized cube sits around 0.55-0.65 depending on topology.
	result.acmr = static_cast<float>(totalIndexTriples * 3) / static_cast<float>(totalUniqueVertices);
	result.atvr = result.acmr;
	return result;
}

} // namespace projectv::asset

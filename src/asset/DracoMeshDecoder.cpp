#include "asset/DracoMeshDecoder.hpp"

#include <array>
#include <cstring>
#include <string>
#include <utility>

#include <draco/attributes/geometry_attribute.h>
#include <draco/attributes/point_attribute.h>
#include <draco/compression/decode.h>
#include <draco/core/decoder_buffer.h>
#include <draco/mesh/mesh.h>
#include <draco/point_cloud/point_cloud.h>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <glm/glm.hpp>

namespace projectv::asset {

namespace {

bool ExtractFloatAttribute(
	const draco::Mesh &mesh,
	const draco::GeometryAttribute::Type attributeType,
	const std::size_t componentCount,
	std::vector<float> &outFloats,
	std::size_t &outFloatStride,
	std::string *outError)
{
	const auto *attribute = mesh.GetNamedAttribute(attributeType);
	if (attribute == nullptr) {
		outFloats.clear();
		outFloatStride = 0;
		return true;
	}
	if (attribute->num_components() != static_cast<int>(componentCount)) {
		if (outError) {
			*outError = "draco attribute component count mismatch for expected type";
		}
		return false;
	}
	const std::size_t pointCount = static_cast<std::size_t>(mesh.num_points());
	outFloats.resize(pointCount * componentCount);
	outFloatStride = componentCount;
	for (std::size_t i = 0; i < pointCount; ++i) {
		float values[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		attribute->GetMappedValue(draco::PointIndex(static_cast<uint32_t>(i)), values);
		for (std::size_t c = 0; c < componentCount; ++c) {
			outFloats[i * componentCount + c] = values[c];
		}
	}
	return true;
}

void ConvertFloatBufferToVec3(
	const std::vector<float> &source,
	std::vector<glm::vec3> &out)
{
	const std::size_t pointCount = source.size() / 3;
	out.resize(pointCount);
	for (std::size_t i = 0; i < pointCount; ++i) {
		out[i] = glm::vec3(source[i * 3 + 0], source[i * 3 + 1], source[i * 3 + 2]);
	}
}

void ConvertFloatBufferToVec2(
	const std::vector<float> &source,
	std::vector<glm::vec2> &out)
{
	const std::size_t pointCount = source.size() / 2;
	out.resize(pointCount);
	for (std::size_t i = 0; i < pointCount; ++i) {
		out[i] = glm::vec2(source[i * 2 + 0], source[i * 2 + 1]);
	}
}

} // namespace

bool DecodeDracoPrimitive(
	const fastgltf::Asset &asset,
	const fastgltf::Primitive &primitive,
	PrimitiveData &out,
	std::string *outError)
{
	if (outError) {
		outError->clear();
	}
	if (primitive.dracoCompression == nullptr) {
		if (outError) {
			*outError = "primitive is not draco-compressed";
		}
		return false;
	}

	const std::size_t bufferViewIndex = primitive.dracoCompression->bufferView;
	if (bufferViewIndex >= asset.bufferViews.size()) {
		if (outError) {
			*outError = "draco bufferView index out of range";
		}
		return false;
	}

	const fastgltf::DefaultBufferDataAdapter adapter;
	const auto compressedBytes = adapter(asset, bufferViewIndex);
	if (compressedBytes.empty()) {
		if (outError) {
			*outError = "draco bufferView resolved to empty bytes";
		}
		return false;
	}

	draco::DecoderBuffer decoderBuffer;
	decoderBuffer.Init(
		reinterpret_cast<const char *>(compressedBytes.data()),
		compressedBytes.size());

	draco::Decoder decoder;
	auto decodeResult = decoder.DecodeMeshFromBuffer(&decoderBuffer);
	if (!decodeResult.ok()) {
		if (outError) {
			*outError = std::string("draco::Decoder::DecodeMeshFromBuffer failed: ")
				+ decodeResult.status().error_msg_string();
		}
		return false;
	}
	const std::unique_ptr<draco::Mesh> mesh = std::move(decodeResult).value();

	std::vector<float> positionFloats;
	std::vector<float> normalFloats;
	std::vector<float> uvFloats;
	std::size_t positionStride = 0;
	std::size_t normalStride = 0;
	std::size_t uvStride = 0;
	if (!ExtractFloatAttribute(*mesh, draco::GeometryAttribute::POSITION, 3, positionFloats, positionStride, outError)
		|| !ExtractFloatAttribute(*mesh, draco::GeometryAttribute::NORMAL, 3, normalFloats, normalStride, outError)
		|| !ExtractFloatAttribute(*mesh, draco::GeometryAttribute::TEX_COORD, 2, uvFloats, uvStride, outError)) {
		return false;
	}
	if (positionFloats.empty()) {
		if (outError) {
			*outError = "draco mesh has no POSITION attribute";
		}
		return false;
	}
	(void)positionStride;
	(void)normalStride;
	(void)uvStride;

	ConvertFloatBufferToVec3(positionFloats, out.positions);
	if (!normalFloats.empty()) {
		ConvertFloatBufferToVec3(normalFloats, out.normals);
	}
	if (!uvFloats.empty()) {
		ConvertFloatBufferToVec2(uvFloats, out.uvs);
	}

	const std::size_t faceCount = static_cast<std::size_t>(mesh->num_faces());
	out.indices.resize(faceCount * 3);
	for (std::size_t i = 0; i < faceCount; ++i) {
		const auto &face = mesh->face(draco::FaceIndex(static_cast<uint32_t>(i)));
		out.indices[i * 3 + 0] = static_cast<uint32_t>(face[0].value());
		out.indices[i * 3 + 1] = static_cast<uint32_t>(face[1].value());
		out.indices[i * 3 + 2] = static_cast<uint32_t>(face[2].value());
	}

	if (primitive.materialIndex.has_value()) {
		out.materialIndex = *primitive.materialIndex;
	}

	if (out.positions.empty() || out.indices.empty() || (out.indices.size() % 3 != 0)) {
		if (outError) {
			*outError = "decoded draco primitive has degenerate geometry";
		}
		return false;
	}
	return true;
}

} // namespace projectv::asset

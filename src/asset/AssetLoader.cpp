#include "asset/AssetLoader.hpp"

#include "asset/DracoMeshDecoder.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <span>
#include <utility>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <glm/glm.hpp>

namespace projectv::asset {

namespace {

thread_local std::string gLastErrorMessage;

void SetLastError(std::string message)
{
	gLastErrorMessage = std::move(message);
}

bool CopyAccessorToVec3(
	const fastgltf::Asset &asset,
	const fastgltf::Accessor &accessor,
	std::vector<glm::vec3> &out)
{
	if (accessor.type != fastgltf::AccessorType::Vec3) {
		return false;
	}
	out.clear();
	out.reserve(accessor.count);
	fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, accessor, [&](glm::vec3 value, std::size_t) {
		out.push_back(value);
	});
	return true;
}

bool CopyAccessorToVec2(
	const fastgltf::Asset &asset,
	const fastgltf::Accessor &accessor,
	std::vector<glm::vec2> &out)
{
	if (accessor.type != fastgltf::AccessorType::Vec2) {
		return false;
	}
	out.clear();
	out.reserve(accessor.count);
	fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, accessor, [&](glm::vec2 value, std::size_t) {
		out.push_back(value);
	});
	return true;
}

bool CopyIndicesToU32(
	const fastgltf::Asset &asset,
	const fastgltf::Accessor &accessor,
	std::vector<uint32_t> &out)
{
	out.clear();
	out.reserve(accessor.count);
	switch (accessor.componentType) {
	case fastgltf::ComponentType::UnsignedInt: {
		fastgltf::iterateAccessorWithIndex<std::uint32_t>(asset, accessor, [&](std::uint32_t v, std::size_t) {
			out.push_back(v);
		});
		return true;
	}
	case fastgltf::ComponentType::UnsignedShort: {
		fastgltf::iterateAccessorWithIndex<std::uint16_t>(asset, accessor, [&](std::uint16_t v, std::size_t) {
			out.push_back(static_cast<std::uint32_t>(v));
		});
		return true;
	}
	case fastgltf::ComponentType::Byte: {
		fastgltf::iterateAccessorWithIndex<std::int8_t>(asset, accessor, [&](std::int8_t v, std::size_t) {
			out.push_back(static_cast<std::uint32_t>(v));
		});
		return true;
	}
	case fastgltf::ComponentType::UnsignedByte: {
		fastgltf::iterateAccessorWithIndex<std::uint8_t>(asset, accessor, [&](std::uint8_t v, std::size_t) {
			out.push_back(static_cast<std::uint32_t>(v));
		});
		return true;
	}
	default:
		return false;
	}
}

bool LoadPrimitive(
	const fastgltf::Asset &asset,
	const fastgltf::Primitive &primitive,
	PrimitiveData &out)
{
	const auto positionAttr = primitive.findAttribute("POSITION");
	if (positionAttr == primitive.attributes.end()) {
		SetLastError("primitive missing POSITION attribute");
		return false;
	}
	if (!CopyAccessorToVec3(asset, asset.accessors[positionAttr->accessorIndex], out.positions)) {
		SetLastError("POSITION accessor is not Vec3");
		return false;
	}

	if (const auto normalAttr = primitive.findAttribute("NORMAL"); normalAttr != primitive.attributes.end()) {
		if (!CopyAccessorToVec3(asset, asset.accessors[normalAttr->accessorIndex], out.normals)) {
			SetLastError("NORMAL accessor is not Vec3");
			return false;
		}
	}

	if (const auto uvAttr = primitive.findAttribute("TEXCOORD_0"); uvAttr != primitive.attributes.end()) {
		if (!CopyAccessorToVec2(asset, asset.accessors[uvAttr->accessorIndex], out.uvs)) {
			SetLastError("TEXCOORD_0 accessor is not Vec2");
			return false;
		}
	}

	if (primitive.indicesAccessor.has_value()) {
		const auto &indexAccessor = asset.accessors[*primitive.indicesAccessor];
		if (indexAccessor.type != fastgltf::AccessorType::Scalar) {
			SetLastError("indices accessor is not Scalar");
			return false;
		}
		if (!CopyIndicesToU32(asset, indexAccessor, out.indices)) {
			SetLastError("unsupported index component type");
			return false;
		}
	} else {
		out.indices.resize(out.positions.size());
		for (std::size_t i = 0; i < out.positions.size(); ++i) {
			out.indices[i] = static_cast<std::uint32_t>(i);
		}
	}

	if (primitive.materialIndex.has_value()) {
		out.materialIndex = *primitive.materialIndex;
	}

	if (out.positions.empty()) {
		SetLastError("primitive has zero vertices");
		return false;
	}
	if (out.indices.size() % 3 != 0) {
		SetLastError("index count is not a multiple of 3");
		return false;
	}
	return true;
}

void AccumulateAabb(
	const std::vector<glm::vec3> &positions,
	glm::vec3 &inOutMin,
	glm::vec3 &inOutMax,
	bool &hasAny)
{
	for (const auto &p : positions) {
		if (!hasAny) {
			inOutMin = p;
			inOutMax = p;
			hasAny = true;
		} else {
			inOutMin = glm::min(inOutMin, p);
			inOutMax = glm::max(inOutMax, p);
		}
	}
}

} // namespace

std::unique_ptr<LoadedAsset> LoadGlb(
	const std::string &path,
	LoadAssetError *outError)
{
	gLastErrorMessage.clear();

	auto dataBuffer = fastgltf::GltfDataBuffer::FromPath(path);
	if (dataBuffer.error() != fastgltf::Error::None) {
		const std::string message = std::string("GltfDataBuffer::FromPath failed: ")
			+ fastgltf::getErrorMessage(dataBuffer.error());
		SetLastError(message);
		if (outError) {
			outError->message = message;
		}
		return nullptr;
	}

	fastgltf::Parser parser(fastgltf::Extensions::KHR_draco_mesh_compression);
	constexpr fastgltf::Options options = fastgltf::Options::None;
	constexpr fastgltf::Category categories = fastgltf::Category::OnlyRenderable;

	const std::filesystem::path directory = std::filesystem::path(path).parent_path();
	auto assetExpected = parser.loadGltf(dataBuffer.get(), directory, options, categories);
	if (assetExpected.error() != fastgltf::Error::None) {
		const std::string message = std::string("loadGltf failed: ")
			+ fastgltf::getErrorMessage(assetExpected.error());
		SetLastError(message);
		if (outError) {
			outError->message = message;
		}
		return nullptr;
	}
	const fastgltf::Asset &asset = assetExpected.get();

	if (asset.meshes.empty()) {
		SetLastError("asset has no meshes");
		if (outError) {
			outError->message = gLastErrorMessage;
		}
		return nullptr;
	}

	auto loaded = std::make_unique<LoadedAsset>();
	loaded->sourcePath = path;

	bool aabbInited = false;
	glm::vec3 aabbMin{0.0f};
	glm::vec3 aabbMax{0.0f};

	for (const auto &mesh : asset.meshes) {
		for (const auto &primitive : mesh.primitives) {
			if (primitive.type != fastgltf::PrimitiveType::Triangles) {
				continue;
			}

			PrimitiveData primitiveData;
			if (primitive.dracoCompression != nullptr) {
				std::string dracoError;
				if (!DecodeDracoPrimitive(asset, primitive, primitiveData, &dracoError)) {
					SetLastError("draco decode failed: " + dracoError);
					if (outError) {
						outError->message = gLastErrorMessage;
					}
					return nullptr;
				}
			} else if (!LoadPrimitive(asset, primitive, primitiveData)) {
				if (outError) {
					outError->message = gLastErrorMessage;
				}
				return nullptr;
			}
			AccumulateAabb(primitiveData.positions, aabbMin, aabbMax, aabbInited);
			loaded->totalVertexCount += static_cast<uint32_t>(primitiveData.positions.size());
			loaded->totalTriangleCount += static_cast<uint32_t>(primitiveData.indices.size() / 3);
			loaded->primitives.push_back(std::move(primitiveData));
		}
	}

	if (!aabbInited || loaded->primitives.empty()) {
		SetLastError("asset has no triangle primitives");
		if (outError) {
			outError->message = gLastErrorMessage;
		}
		return nullptr;
	}

	loaded->aabbMin = aabbMin;
	loaded->aabbMax = aabbMax;
	return loaded;
}

std::string_view GetAssetLoaderLastErrorMessage()
{
	return gLastErrorMessage;
}

} // namespace projectv::asset

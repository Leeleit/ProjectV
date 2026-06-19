#include "asset/AssetLoader.hpp"

#include "asset/DracoMeshDecoder.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stack>
#include <utility>
#include <vector>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp> // IWYU pragma: keep — ElementTraits<glm::vec3/2/4> specialization needed by CopyAccessorToVec3/2 (see Phase A refactor note)
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

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

	fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, accessor,
												  [&](const glm::vec3 value, std::size_t) {
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
	fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, accessor, [&](const glm::vec2 value, std::size_t) {
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
		fastgltf::iterateAccessorWithIndex<std::uint32_t>(asset, accessor, [&](const std::uint32_t v, std::size_t) {
			out.push_back(v);
		});
		return true;
	}
	case fastgltf::ComponentType::UnsignedShort: {
		fastgltf::iterateAccessorWithIndex<std::uint16_t>(asset, accessor, [&](const std::uint16_t v, std::size_t) {
			out.push_back(v);
		});
		return true;
	}
	case fastgltf::ComponentType::Byte: {
		fastgltf::iterateAccessorWithIndex<std::int8_t>(asset, accessor, [&](const std::int8_t v, std::size_t) {
			out.push_back(static_cast<std::uint32_t>(v));
		});
		return true;
	}
	case fastgltf::ComponentType::UnsignedByte: {
		fastgltf::iterateAccessorWithIndex<std::uint8_t>(asset, accessor, [&](const std::uint8_t v, std::size_t) {
			out.push_back(v);
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

namespace {

glm::mat4 ComposeNodeLocalMatrix(const fastgltf::Node &node)
{
	if (std::holds_alternative<fastgltf::math::fmat4x4>(node.transform)) {
		const auto &m = std::get<fastgltf::math::fmat4x4>(node.transform);

		glm::mat4 result(0.0f);
		for (int col = 0; col < 4; ++col) {
			const auto &srcCol = m[col];
			result[col][0] = srcCol.x();
			result[col][1] = srcCol.y();
			result[col][2] = srcCol.z();
			result[col][3] = srcCol.w();
		}
		return result;
	}
	const auto &[srcTranslation, srcRotation, srcScale] = std::get<fastgltf::TRS>(node.transform);
	const glm::vec3 translation(
		srcTranslation.x(),
		srcTranslation.y(),
		srcTranslation.z());

	const glm::quat rotation(
		srcRotation.w(),
		srcRotation.x(),
		srcRotation.y(),
		srcRotation.z());
	const glm::vec3 scale(
		srcScale.x(),
		srcScale.y(),
		srcScale.z());
	const glm::mat4 t = glm::translate(glm::mat4(1.0f), translation);
	const glm::mat4 r = glm::mat4_cast(rotation);
	const glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
	return t * r * s;
}

bool ApplyNodeHierarchyTransforms(
	const fastgltf::Asset &asset,
	LoadedAsset &loaded)
{
	if (asset.scenes.empty()) {
		SetLastError("asset has no scenes");
		return false;
	}
	const auto &[sceneNodeIndices, sceneName] = asset.scenes.front();
	(void)sceneName;
	if (sceneNodeIndices.empty()) {
		SetLastError("default scene has no root nodes");
		return false;
	}

	std::vector nodeVisited(asset.nodes.size(), false);

	struct Frame {
		size_t nodeIndex;
		glm::mat4 parentGlobal;
	};
	std::stack<Frame> stack;
	for (const size_t rootIdx : sceneNodeIndices) {
		stack.push(Frame{rootIdx, glm::mat4(1.0f)});
	}

	bool aabbInited = false;
	glm::vec3 aabbMin{0.0f};
	glm::vec3 aabbMax{0.0f};

	while (!stack.empty()) {
		const auto &[nodeIndex, parentGlobal] = stack.top();
		stack.pop();
		const fastgltf::Node &node = asset.nodes[nodeIndex];
		if (nodeIndex >= asset.nodes.size()) {
			SetLastError("scene references out-of-range node index");
			return false;
		}
		if (nodeVisited[nodeIndex]) {

			continue;
		}
		nodeVisited[nodeIndex] = true;
		const glm::mat4 local = ComposeNodeLocalMatrix(node);
		const glm::mat4 global = parentGlobal * local;

		if (node.meshIndex.has_value()) {
			const auto meshIdx = *node.meshIndex;
			if (meshIdx >= asset.meshes.size()) {
				SetLastError("node references out-of-range mesh index");
				return false;
			}
			const fastgltf::Mesh &mesh = asset.meshes[meshIdx];

			size_t primitiveOffset = 0;
			for (size_t m = 0; m < meshIdx; ++m) {
				primitiveOffset += asset.meshes[m].primitives.size();
			}
			for (size_t pi = 0; pi < mesh.primitives.size(); ++pi) {
				PrimitiveData &prim = loaded.primitives[primitiveOffset + pi];
				for (glm::vec3 &p : prim.positions) {
					const glm::vec4 transformed = global * glm::vec4(p, 1.0f);
					p = glm::vec3(transformed);
				}
				for (glm::vec3 &n_local : prim.normals) {

					const glm::mat3 normalMatrix = glm::mat3(global);
					const glm::vec3 nTransformed = normalMatrix * n_local;
					const float nLen = glm::length(nTransformed);
					if (nLen > 1e-6f) {
						n_local = nTransformed / nLen;
					}
				}
				AccumulateAabb(prim.positions, aabbMin, aabbMax, aabbInited);
			}
		}

		for (const size_t childIdx : node.children) {
			stack.push(Frame{childIdx, global});
		}
	}

	if (!aabbInited) {
		SetLastError("scene hierarchy references no triangle primitives");
		return false;
	}

	loaded.aabbMin = aabbMin;
	loaded.aabbMax = aabbMax;
	return true;
}

} // namespace

std::unique_ptr<LoadedAsset> LoadGlb(
	const std::string &path,
	LoadAssetError *outError)
{
	gLastErrorMessage.clear();

	auto dataBuffer = fastgltf::GltfDataBuffer::FromPath(path);
	if (dataBuffer.error() != fastgltf::Error::None) {
		std::string message = "GltfDataBuffer::FromPath failed: ";
		message += fastgltf::getErrorMessage(dataBuffer.error());
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
		std::string message = "loadGltf failed: ";
		message += fastgltf::getErrorMessage(assetExpected.error());
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
			loaded->totalVertexCount += static_cast<uint32_t>(primitiveData.positions.size());
			loaded->totalTriangleCount += static_cast<uint32_t>(primitiveData.indices.size() / 3);
			loaded->primitives.push_back(std::move(primitiveData));
		}
	}

	if (loaded->primitives.empty()) {
		SetLastError("asset has no triangle primitives");
		if (outError) {
			outError->message = gLastErrorMessage;
		}
		return nullptr;
	}

	if (!ApplyNodeHierarchyTransforms(asset, *loaded)) {
		if (outError) {
			outError->message = gLastErrorMessage;
		}
		return nullptr;
	}

	return loaded;
}

std::string_view GetAssetLoaderLastErrorMessage()
{
	return gLastErrorMessage;
}

std::optional<GlbDimensions> ComputeGlbDimensions(
	const std::string &path,
	LoadAssetError *outError)
{
	LoadAssetError localError;
	const std::unique_ptr<LoadedAsset> loaded = LoadGlb(path, &localError);
	if (!loaded) {
		if (outError) {
			*outError = std::move(localError);
		}
		return std::nullopt;
	}
	GlbDimensions dims{};
	dims.aabbMin = loaded->aabbMin;
	dims.aabbMax = loaded->aabbMax;
	dims.size = loaded->aabbMax - loaded->aabbMin;
	return dims;
}

VoxelAlignedAabb ComputeVoxelAlignedAabb(
	const glm::vec3 &aabbMin,
	const glm::vec3 &aabbMax,
	const float voxelSize)
{

	(void)voxelSize;

	const float srcX = std::max(aabbMax.x - aabbMin.x, 1e-6f);
	const float srcY = std::max(aabbMax.y - aabbMin.y, 1e-6f);
	const float srcZ = std::max(aabbMax.z - aabbMin.z, 1e-6f);
	const float targetX = std::max(1.0f, std::round(srcX));
	const float targetY = std::max(1.0f, std::round(srcY));
	const float targetZ = std::max(1.0f, std::round(srcZ));
	const float sx = targetX / srcX;
	const float sy = targetY / srcY;
	const float sz = targetZ / srcZ;
	VoxelAlignedAabb result{};
	result.aabbMin = glm::vec3(aabbMin.x * sx, aabbMin.y * sy, aabbMin.z * sz);
	result.aabbMax = glm::vec3(aabbMax.x * sx, aabbMax.y * sy, aabbMax.z * sz);

	result.aabbMin.x = std::round(result.aabbMin.x);
	result.aabbMin.z = std::round(result.aabbMin.z);
	result.aabbMax.x = result.aabbMin.x + targetX;
	result.aabbMax.z = result.aabbMin.z + targetZ;
	return result;
}

} // namespace projectv::asset

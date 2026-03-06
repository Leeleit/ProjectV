// Пример: fastgltf — загрузка glTF/GLB, обход сцены, извлечение меша
// Документация: docs/fastgltf/quickstart.md

#include <cstdio>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <vector>

struct Vertex {
	fastgltf::math::fvec3 position;
	fastgltf::math::fvec2 uv;
};

bool loadAndExtractMesh(std::filesystem::path path)
{
	auto data = fastgltf::GltfDataBuffer::FromPath(path);
	if (data.error() != fastgltf::Error::None)
		return false;

	fastgltf::Parser parser;
	auto asset = parser.loadGltf(data.get(), path.parent_path(), fastgltf::Options::LoadExternalBuffers);
	if (asset.error() != fastgltf::Error::None)
		return false;

	auto sceneIndex = asset->defaultScene.value_or(0);
	if (sceneIndex >= asset->scenes.size())
		return false;

	std::vector<Vertex> vertices;
	std::vector<std::uint32_t> indices;

	fastgltf::iterateSceneNodes(
		asset.get(), sceneIndex, fastgltf::math::fmat4x4{},
		[&](fastgltf::Node &node, const fastgltf::math::fmat4x4 &matrix) {
			if (!node.meshIndex.has_value())
				return;
			auto &mesh = asset->meshes[*node.meshIndex];

			for (auto &primitive : mesh.primitives) {
				auto posAttr = primitive.findAttribute("POSITION");
				auto uvAttr = primitive.findAttribute("TEXCOORD_0");
				if (posAttr == primitive.attributes.cend())
					continue;

				auto &posAccessor = asset->accessors[posAttr->accessorIndex];
				std::size_t vertexCount = posAccessor.count;
				std::size_t baseIdx = vertices.size();
				vertices.resize(baseIdx + vertexCount);

				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
					asset.get(), posAccessor, [&](fastgltf::math::fvec3 pos, std::size_t i) {
						vertices[baseIdx + i].position = pos;
						vertices[baseIdx + i].uv = (uvAttr != primitive.attributes.cend())
													   ? fastgltf::getAccessorElement<fastgltf::math::fvec2>(
															 asset.get(), asset->accessors[uvAttr->accessorIndex], i)
													   : fastgltf::math::fvec2(0, 0);
					});

				if (primitive.indicesAccessor.has_value()) {
					auto &idxAccessor = asset->accessors[*primitive.indicesAccessor];
					std::vector<std::uint32_t> primIndices(idxAccessor.count);
					fastgltf::copyFromAccessor<std::uint32_t>(asset.get(), idxAccessor, primIndices.data());
					for (auto idx : primIndices)
						indices.push_back(static_cast<std::uint32_t>(baseIdx) + idx);
				}
			}
		});

	std::printf("Загружено %zu вершин, %zu индексов\n", vertices.size(), indices.size());
	return true;
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		std::fprintf(stderr, "Использование: %s <path/to/model.gltf|glb>\n", argv[0]);
		return 1;
	}
	return loadAndExtractMesh(argv[1]) ? 0 : 1;
}

// Пример: fastgltf — загрузка текстур и изображений из glTF
// Документация: docs/fastgltf/concepts.md (раздел Images & Textures)
//
// Уровень сложности: 🟢 (базовый)

#include <cstdio>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <vector>

// Простая структура для хранения информации о текстуре
struct TextureInfo {
	std::string name;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t channels = 0;
	std::vector<uint8_t> pixelData;
};

bool loadTexturesFromGltf(const std::filesystem::path &gltfPath)
{
	// 1. Загрузка данных glTF
	auto data = fastgltf::GltfDataBuffer::FromPath(gltfPath);
	if (data.error() != fastgltf::Error::None) {
		std::fprintf(stderr, "Ошибка загрузки файла: %s\n", gltfPath.string().c_str());
		return false;
	}

	// 2. Парсинг glTF
	fastgltf::Parser parser;
	auto asset = parser.loadGltf(data.get(), gltfPath.parent_path(),
								 fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages);

	if (asset.error() != fastgltf::Error::None) {
		std::fprintf(stderr, "Ошибка парсинга glTF: %s\n", fastgltf::to_underlying(asset.error()));
		return false;
	}

	std::vector<TextureInfo> loadedTextures;

	// 3. Обработка изображений
	for (const auto &image : asset->images) {
		TextureInfo texInfo;

		// Получение имени изображения (если есть)
		if (image.name) {
			texInfo.name = *image.name;
		} else {
			texInfo.name = "image_" + std::to_string(&image - &asset->images[0]);
		}

		// 4. Получение данных изображения через DataSource
		std::visit(
			fastgltf::visitor{
				[&](const fastgltf::sources::URI &uri) {
					// Внешний файл изображения
					std::printf("Текстура '%s': внешний файл %s\n", texInfo.name.c_str(), uri.uri.path().c_str());
					// В реальном приложении здесь нужно загрузить файл изображения
					texInfo.width = 0; // Неизвестно без декодирования
					texInfo.height = 0;
					texInfo.channels = 0;
				},
				[&](const fastgltf::sources::BufferView &bufferView) {
					// Изображение встроено в буфер
					const auto &view = asset->bufferViews[bufferView.bufferViewIndex];
					const auto &buffer = asset->buffers[view.bufferIndex];

					std::visit(fastgltf::visitor{[&](const fastgltf::sources::Array &array) {
													 // Данные в памяти
													 const uint8_t *dataPtr = array.bytes.data() + view.byteOffset;
													 size_t dataSize = view.byteLength;

													 std::printf("Текстура '%s': встроенные данные %zu байт\n",
																 texInfo.name.c_str(), dataSize);

													 // В реальном приложении здесь нужно декодировать изображение
													 // (например, с помощью stb_image)
													 texInfo.pixelData.assign(dataPtr, dataPtr + dataSize);
												 },
												 [&](const auto &other) {
													 std::printf("Текстура '%s': другой источник данных\n",
																 texInfo.name.c_str());
												 }},
							   buffer.data);
				},
				[&](const fastgltf::sources::Vector &vec) {
					// Вектор байтов
					std::printf("Текстура '%s': вектор данных %zu байт\n", texInfo.name.c_str(), vec.bytes.size());
					texInfo.pixelData = vec.bytes;
				},
				[](const auto &) {
					// Другие источники данных
				}},
			image.data);

		loadedTextures.push_back(std::move(texInfo));
	}

	// 5. Обработка текстур и материалов
	std::printf("\nЗагружено изображений: %zu\n", asset->images.size());
	std::printf("Загружено текстур: %zu\n", asset->textures.size());
	std::printf("Загружено материалов: %zu\n", asset->materials.size());

	// Пример обработки материалов с текстурами
	for (const auto &material : asset->materials) {
		if (material.pbrData.baseColorTexture) {
			uint32_t texIndex = material.pbrData.baseColorTexture->textureIndex;
			if (texIndex < asset->textures.size()) {
				const auto &texture = asset->textures[texIndex];
				if (texture.imageIndex) {
					std::printf("Материал использует текстуру baseColor: imageIndex=%u\n", *texture.imageIndex);
				}
			}
		}

		if (material.normalTexture) {
			uint32_t texIndex = material.normalTexture->textureIndex;
			if (texIndex < asset->textures.size()) {
				const auto &texture = asset->textures[texIndex];
				if (texture.imageIndex) {
					std::printf("Материал использует normal map: imageIndex=%u\n", *texture.imageIndex);
				}
			}
		}
	}

	return true;
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		std::fprintf(stderr, "Использование: %s <path/to/model.gltf|glb>\n", argv[0]);
		std::fprintf(stderr, "\nПримеры моделей с текстурами:\n");
		std::fprintf(stderr, "  • models/textured-cube.gltf\n");
		std::fprintf(stderr, "  • models/DamagedHelmet.glb\n");
		return 1;
	}

	return loadTexturesFromGltf(argv[1]) ? 0 : 1;
}

# Быстрый старт

🟢 **Уровень 1: Начинающий**

Минимальный рабочий пример загрузки glTF модели.

## Базовый пример

```cpp
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <iostream>

bool loadModel(const std::filesystem::path& path) {
    // 1. Создание парсера
    // Рекомендуется переиспользовать между загрузками, но не между потоками
    fastgltf::Parser parser;

    // 2. Загрузка файла в буфер
    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (data.error() != fastgltf::Error::None) {
        std::cerr << "Error loading file: "
                  << fastgltf::getErrorMessage(data.error()) << "\n";
        return false;
    }

    // 3. Парсинг (автоматически определяет JSON или GLB)
    auto asset = parser.loadGltf(data.get(), path.parent_path(),
                                  fastgltf::Options::None);
    if (asset.error() != fastgltf::Error::None) {
        std::cerr << "Error parsing glTF: "
                  << fastgltf::getErrorMessage(asset.error()) << "\n";
        return false;
    }

    // 4. Доступ к данным
    std::cout << "Loaded model:\n";
    std::cout << "  Meshes: " << asset->meshes.size() << "\n";
    std::cout << "  Nodes: " << asset->nodes.size() << "\n";
    std::cout << "  Materials: " << asset->materials.size() << "\n";

    return true;
}
```

## Выбор источника данных

fastgltf предоставляет несколько классов для загрузки данных:

| Класс                   | Использование              | Особенности                                                     |
|-------------------------|----------------------------|-----------------------------------------------------------------|
| `GltfDataBuffer`        | Стандартная загрузка в RAM | Возвращает `Expected<T>`, самый универсальный                   |
| `GltfFileStream`        | Потоковое чтение           | Обёртка над `std::ifstream`                                     |
| `MappedGltfFile`        | Memory-mapped файл         | Только desktop (Win/Linux/macOS), эффективен для больших файлов |
| `AndroidGltfDataBuffer` | Android APK assets         | Требует `setAndroidAssetManager()`                              |

### GltfDataBuffer

```cpp
// Из файла
auto data = fastgltf::GltfDataBuffer::FromPath("model.gltf");

// Из памяти
auto data = fastgltf::GltfDataBuffer::FromBytes(bytes.data(), bytes.size());

// Из std::span (C++20)
auto data = fastgltf::GltfDataBuffer::FromSpan(span);
```

### GltfFileStream

```cpp
fastgltf::GltfFileStream fileStream("model.gltf");
if (!fileStream.isOpen()) {
    return false;
}
auto asset = parser.loadGltf(fileStream, path.parent_path());
```

### MappedGltfFile

```cpp
auto data = fastgltf::MappedGltfFile::FromPath("large_model.glb");
if (data.error() != fastgltf::Error::None) {
    return false;
}
// Данные не загружаются в RAM, используется memory mapping
```

## Типичная конфигурация

### Статические модели

```cpp
fastgltf::Options options = fastgltf::Options::LoadExternalBuffers;
auto asset = parser.loadGltf(data.get(), basePath, options,
                              fastgltf::Category::OnlyRenderable);
```

### Модели с анимациями

```cpp
fastgltf::Options options = fastgltf::Options::LoadExternalBuffers
                          | fastgltf::Options::LoadExternalImages;
auto asset = parser.loadGltf(data.get(), basePath, options,
                              fastgltf::Category::All);
```

### Прототипирование

```cpp
// Минимальная конфигурация для быстрой проверки
auto asset = parser.loadGltf(data.get(), basePath,
                              fastgltf::Options::None);
```

## Загрузка с расширениями

Для использования расширений их нужно указать при создании парсера:

```cpp
fastgltf::Extensions extensions =
    fastgltf::Extensions::KHR_texture_basisu
    | fastgltf::Extensions::KHR_materials_unlit;

fastgltf::Parser parser(extensions);

// После загрузки проверьте extensionsRequired
auto asset = parser.loadGltf(data.get(), basePath, options);
if (!asset->extensionsRequired.empty()) {
    for (const auto& ext : asset->extensionsRequired) {
        std::cout << "Required extension: " << ext << "\n";
    }
}
```

## Проверка модели

Для отладки можно использовать функцию `validate`:

```cpp
auto asset = parser.loadGltf(data.get(), basePath, options);
if (asset.error() == fastgltf::Error::None) {
    // Строгая проверка по спецификации glTF 2.0
    // Рекомендуется в Debug сборках
    auto validateError = fastgltf::validate(asset.get());
    if (validateError != fastgltf::Error::None) {
        std::cerr << "Validation failed: "
                  << fastgltf::getErrorMessage(validateError) << "\n";
    }
}
```

## Определение типа файла

Если нужно определить тип до загрузки:

```cpp
auto type = fastgltf::determineGltfFileType(data.get());
switch (type) {
    case fastgltf::GltfType::glTF:
        std::cout << "JSON-based glTF\n";
        break;
    case fastgltf::GltfType::GLB:
        std::cout << "Binary glTF (GLB)\n";
        break;
    case fastgltf::GltfType::Invalid:
        std::cerr << "Invalid file\n";
        break;
}

// Или использовать специализированные методы:
auto asset = parser.loadGltfJson(data.get(), basePath, options, category);
auto asset = parser.loadGltfBinary(data.get(), basePath, options, category);
```

## Обработка ошибок

Все функции загрузки возвращают `Expected<T>`:

```cpp
auto asset = parser.loadGltf(data.get(), basePath, options);

if (asset.error() != fastgltf::Error::None) {
    // Краткое имя ошибки
    std::cerr << fastgltf::getErrorName(asset.error()) << "\n";

    // Полное описание
    std::cerr << fastgltf::getErrorMessage(asset.error()) << "\n";

    return;
}

// Доступ к значению
fastgltf::Asset& model = asset.get();
// или
fastgltf::Asset* model = asset.get_if();  // nullptr при ошибке
// или
fastgltf::Asset& model = *asset;  // operator*
```

## Полный пример: Загрузка мешей

Пример загрузки glTF/GLB с извлечением вершин и индексов:

```cpp
// ProjectV Example: fastgltf Mesh Loading
// Description: Загрузка glTF/GLB, обход сцены, извлечение мешей.

#include <cstdint>
#include <cstdio>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <vector>

struct Vertex {
	fastgltf::math::fvec3 position;
	fastgltf::math::fvec2 uv;
};

static bool load_and_extract_mesh(const std::filesystem::path &path)
{
	auto data = fastgltf::GltfDataBuffer::FromPath(path);
	if (data.error() != fastgltf::Error::None) {
		return false;
	}

	fastgltf::Parser parser;
	auto asset = parser.loadGltf(data.get(), path.parent_path(), fastgltf::Options::LoadExternalBuffers);
	if (asset.error() != fastgltf::Error::None) {
		return false;
	}

	auto sceneIndex = asset->defaultScene.value_or(0);
	if (sceneIndex >= asset->scenes.size()) {
		return false;
	}

	std::vector<Vertex> vertices;
	std::vector<std::uint32_t> indices;

	fastgltf::iterateSceneNodes(
		asset.get(), sceneIndex, fastgltf::math::fmat4x4{},
		[&](fastgltf::Node &node, const fastgltf::math::fmat4x4 &matrix) {
			(void)matrix; // Transform not used in this example
			if (!node.meshIndex.has_value()) {
				return;
			}
			auto &mesh = asset->meshes[*node.meshIndex];

			for (auto &primitive : mesh.primitives) {
				auto posAttr = primitive.findAttribute("POSITION");
				auto uvAttr = primitive.findAttribute("TEXCOORD_0");
				if (posAttr == primitive.attributes.cend()) {
					continue;
				}

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
					for (auto idx : primIndices) {
						indices.push_back(static_cast<std::uint32_t>(baseIdx) + idx);
					}
				}
			}
		});

	std::printf("Loaded %zu vertices, %zu indices\n", vertices.size(), indices.size());
	return true;
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		std::fprintf(stderr, "Usage: %s <path/to/model.gltf|glb>\n", argv[0]);
		return 1;
	}
	return load_and_extract_mesh(argv[1]) ? 0 : 1;
}
```

---

## Типичные ошибки

| Ошибка                     | Причина                     | Решение                                                            |
|----------------------------|-----------------------------|--------------------------------------------------------------------|
| `InvalidPath`              | Неверный путь к директории  | Проверьте `path.parent_path()`                                     |
| `MissingExtensions`        | Модель требует расширение   | Добавьте расширение в Parser                                       |
| `UnknownRequiredExtension` | Неподдерживаемое расширение | Используйте `DontRequireValidAssetMember` или найдите альтернативу |
| `InvalidJson`              | Ошибка парсинга JSON        | Проверьте файл валидатором glTF                                    |
| `MissingExternalBuffer`    | Внешний .bin не найден      | Используйте `LoadExternalBuffers`                                  |

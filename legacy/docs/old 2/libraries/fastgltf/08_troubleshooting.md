# Решение проблем fastgltf

🟡 **Уровень 2: Средний**

Диагностика и исправление типичных ошибок.

## Коды ошибок

| Код                          | Причина                         | Решение                                                            |
|------------------------------|---------------------------------|--------------------------------------------------------------------|
| `InvalidPath`                | Неверный путь к директории      | Проверьте `path.parent_path()`                                     |
| `MissingExtensions`          | Расширения не включены в Parser | Добавьте расширения в конструктор Parser                           |
| `UnknownRequiredExtension`   | Неподдерживаемое расширение     | Используйте `DontRequireValidAssetMember` или найдите альтернативу |
| `InvalidJson`                | Ошибка парсинга JSON            | Проверьте файл валидатором glTF                                    |
| `InvalidGltf`                | Невалидные данные glTF          | Проверьте структуру файла                                          |
| `InvalidOrMissingAssetField` | Поле asset отсутствует          | Используйте `DontRequireValidAssetMember`                          |
| `InvalidGLB`                 | Невалидный GLB                  | Проверьте заголовок и чанки                                        |
| `MissingExternalBuffer`      | Внешний буфер не найден         | Используйте `LoadExternalBuffers`                                  |
| `UnsupportedVersion`         | Неподдерживаемая версия glTF    | Конвертируйте в glTF 2.0                                           |
| `InvalidURI`                 | Ошибка парсинга URI             | Проверьте формат URI                                               |
| `InvalidFileData`            | Тип файла не определён          | Проверьте содержимое файла                                         |
| `FailedWritingFiles`         | Ошибка при экспорте             | Проверьте права доступа                                            |
| `FileBufferAllocationFailed` | Не удалось выделить память      | Уменьшите размер файла или используйте streaming                   |

## Диагностика

### Проверка файла перед загрузкой

```cpp
// Определение типа файла
auto type = fastgltf::determineGltfFileType(data.get());
if (type == fastgltf::GltfType::Invalid) {
    std::cerr << "Invalid glTF file\n";
    return;
}

// Валидация после загрузки
auto asset = parser.loadGltf(data.get(), basePath, options);
if (asset.error() == fastgltf::Error::None) {
    auto validateError = fastgltf::validate(asset.get());
    if (validateError != fastgltf::Error::None) {
        std::cerr << "Validation: "
                  << fastgltf::getErrorMessage(validateError) << "\n";
    }
}
```

### Проверка расширений

```cpp
auto asset = parser.loadGltf(data.get(), basePath, options);

// Проверка обязательных расширений
for (const auto& ext : asset->extensionsRequired) {
    std::cout << "Required: " << ext << "\n";

    // Проверка, поддерживает ли ваш код это расширение
    if (!isExtensionSupported(ext)) {
        std::cerr << "Unsupported required extension: " << ext << "\n";
    }
}
```

### Проверка источников данных

```cpp
for (size_t i = 0; i < asset->buffers.size(); ++i) {
    const auto& buffer = asset->buffers[i];

    std::cout << "Buffer " << i << ": ";

    if (std::holds_alternative<fastgltf::sources::ByteView>(buffer.data)) {
        std::cout << "ByteView (GLB/base64)\n";
    } else if (std::holds_alternative<fastgltf::sources::Array>(buffer.data)) {
        std::cout << "Array (embedded)\n";
    } else if (std::holds_alternative<fastgltf::sources::Vector>(buffer.data)) {
        std::cout << "Vector (loaded external)\n";
    } else if (std::holds_alternative<fastgltf::sources::URI>(buffer.data)) {
        const auto& uri = std::get<fastgltf::sources::URI>(buffer.data);
        std::cout << "URI: " << uri.uri.path() << "\n";
        std::cout << "  Need to load manually or use LoadExternalBuffers\n";
    } else if (std::holds_alternative<fastgltf::sources::CustomBuffer>(buffer.data)) {
        std::cout << "CustomBuffer (GPU)\n";
    }
}
```

## Типичные проблемы

### Проблема: MissingExtensions

**Симптом:** Ошибка `MissingExtensions` при загрузке модели.

**Причина:** Модель использует расширение, не включённое в Parser.

**Решение:**

```cpp
// До:
fastgltf::Parser parser;

// После:
fastgltf::Extensions extensions =
    fastgltf::Extensions::KHR_texture_basisu
    | fastgltf::Extensions::KHR_materials_unlit;

fastgltf::Parser parser(extensions);
```

### Проблема: MissingExternalBuffer

**Симптом:** Ошибка `MissingExternalBuffer` при загрузке.

**Причина:** Внешние .bin файлы не загружаются автоматически.

**Решение:**

```cpp
auto asset = parser.loadGltf(
    data.get(),
    basePath,
    fastgltf::Options::LoadExternalBuffers  // Добавьте этот флаг
);
```

### Проблема: Данные accessor недоступны

**Симптом:** `iterateAccessor` или `copyFromAccessor` не работают.

**Причина:** Буфер имеет тип `sources::URI` без загрузки.

**Решение:**

```cpp
// Вариант 1: Использовать LoadExternalBuffers
auto asset = parser.loadGltf(data.get(), basePath,
    fastgltf::Options::LoadExternalBuffers);

// Вариант 2: Кастомный BufferDataAdapter
auto customAdapter = [&](const Asset& asset, size_t bufferViewIdx) {
    // Загрузка из вашего источника
    return yourDataSpan;
};

fastgltf::iterateAccessor<glm::vec3>(asset, accessor,
    [&](glm::vec3 pos) { /* ... */ }, customAdapter);
```

### Проблема: Неверный тип в accessor tools

**Симптом:** Assert или crash при использовании `iterateAccessor`.

**Причина:** Тип в шаблоне не соответствует `AccessorType` accessor.

**Решение:**

```cpp
// Проверка типа accessor
if (accessor.type == fastgltf::AccessorType::Vec3) {
    fastgltf::iterateAccessor<glm::vec3>(asset, accessor, ...);  // OK
}

// Не делайте так:
if (accessor.type == fastgltf::AccessorType::Vec3) {
    fastgltf::iterateAccessor<glm::vec2>(asset, accessor, ...);  // Assert!
}
```

### Проблема: Большой файл не загружается

**Симптом:** `FileBufferAllocationFailed` или out of memory.

**Причина:** Файл слишком большой для загрузки в RAM.

**Решение:**

```cpp
// Используйте memory mapping для больших файлов
auto data = fastgltf::MappedGltfFile::FromPath("large_model.glb");

// Или отключите кастомный memory pool
// В CMake:
// set(FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL ON CACHE BOOL "" FORCE)
```

### Проблема: Матрицы узлов не разложены

**Симптом:** `node.transform` всегда содержит матрицу, а не TRS.

**Причина:** Не указан флаг `DecomposeNodeMatrices`.

**Решение:**

```cpp
auto asset = parser.loadGltf(data.get(), basePath,
    fastgltf::Options::DecomposeNodeMatrices);

// Теперь node.transform может быть TRS
if (std::holds_alternative<fastgltf::TRS>(node.transform)) {
    auto& trs = std::get<fastgltf::TRS>(node.transform);
    // translation, rotation, scale
}
```

## Валидация glTF файлов

### Онлайн валидаторы

- [glTF Validator](https://github.khronos.org/glTF-Validator/) — официальный валидатор Khronos
- [glTF Report](https://github.com/AnalyticalGraphicsInc/gltf-report) — детальный отчёт

### Программная валидация

```cpp
auto asset = parser.loadGltf(data.get(), basePath, options);
if (asset.error() != fastgltf::Error::None) {
    std::cerr << "Load error: "
              << fastgltf::getErrorMessage(asset.error()) << "\n";
    return;
}

auto validateError = fastgltf::validate(asset.get());
if (validateError != fastgltf::Error::None) {
    std::cerr << "Validation error: "
              << fastgltf::getErrorMessage(validateError) << "\n";
    // Файл не соответствует спецификации glTF 2.0
}
```

## Отладка

### Вывод структуры модели

```cpp
void debugAsset(const fastgltf::Asset& asset) {
    std::cout << "=== Asset Debug ===\n";
    std::cout << "Meshes: " << asset.meshes.size() << "\n";
    std::cout << "Nodes: " << asset.nodes.size() << "\n";
    std::cout << "Scenes: " << asset.scenes.size() << "\n";
    std::cout << "Materials: " << asset.materials.size() << "\n";
    std::cout << "Buffers: " << asset.buffers.size() << "\n";
    std::cout << "BufferViews: " << asset.bufferViews.size() << "\n";
    std::cout << "Accessors: " << asset.accessors.size() << "\n";
    std::cout << "Animations: " << asset.animations.size() << "\n";
    std::cout << "Skins: " << asset.skins.size() << "\n";

    if (asset.defaultScene.has_value()) {
        std::cout << "Default scene: " << *asset.defaultScene << "\n";
    }

    for (const auto& ext : asset.extensionsUsed) {
        std::cout << "Extension used: " << ext << "\n";
    }
}
```

### Вывод примитива

```cpp
void debugPrimitive(const fastgltf::Primitive& primitive) {
    std::cout << "Primitive:\n";
    std::cout << "  Type: " << static_cast<int>(primitive.type) << "\n";
    std::cout << "  Attributes:\n";

    for (const auto& attr : primitive.attributes) {
        std::cout << "    " << attr.name << " -> accessor " << attr.accessorIndex << "\n";
    }

    if (primitive.indicesAccessor.has_value()) {
        std::cout << "  Indices: accessor " << *primitive.indicesAccessor << "\n";
    }

    if (primitive.materialIndex.has_value()) {
        std::cout << "  Material: " << *primitive.materialIndex << "\n";
    }

    std::cout << "  Morph targets: " << primitive.targets.size() << "\n";
}
```

## Часто задаваемые вопросы

**Q: Почему fastgltf не загружает изображения?**

A: fastgltf не включает декодер изображений. Используйте stb_image или аналоги:

```cpp
for (const auto& image : asset->images) {
    if (std::holds_alternative<fastgltf::sources::URI>(image.data)) {
        auto& uri = std::get<fastgltf::sources::URI>(image.data);
        // Загрузка через stb_image
        int w, h, channels;
        auto* pixels = stbi_load(uri.uri.path().c_str(), &w, &h, &channels, 4);
    }
}
```

**Q: Как загрузить Draco-сжатую модель?**

A: fastgltf не поддерживает Draco. Используйте tinygltf или конвертируйте модель.

**Q: Как экспортировать glTF?**

A: Используйте `Exporter` или `FileExporter`:

```cpp
fastgltf::FileExporter exporter;
auto error = exporter.writeGltfJson(asset, "output.gltf",
                                     fastgltf::ExportOptions::PrettyPrintJson);
```

**Q: Почему анимации не загружаются?**

A: Проверьте Category:

```cpp
// Неправильно:
auto asset = parser.loadGltf(data.get(), basePath, options,
                              fastgltf::Category::OnlyRenderable);  // Нет анимаций!

// Правильно:
auto asset = parser.loadGltf(data.get(), basePath, options,
                              fastgltf::Category::All);  // Или OnlyAnimations

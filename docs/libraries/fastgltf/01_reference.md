# fastgltf

**fastgltf** — высокопроизводительная библиотека загрузки glTF 2.0 на C++17 с минимальными зависимостями. Использует
SIMD (simdjson) для ускорения парсинга JSON и base64-декодирования. Поддерживает полную спецификацию glTF 2.0 и
множество расширений.

Версия: **0.9.0**
Исходники: [spnda/fastgltf](https://github.com/spnda/fastgltf)

---

## Возможности

- **glTF 2.0** — полная поддержка спецификации (чтение и запись)
- **SIMD-оптимизации** — ускорение парсинга в 2-20 раз по сравнению с аналогами
- **Минимальные зависимости** — только simdjson
- **Accessor tools** — утилиты для чтения данных, включая sparse accessors
- **GPU-ready** — возможность прямой записи в mapped GPU buffers
- **C++20 modules** — опциональная поддержка модулей
- **Android** — нативная загрузка из APK assets

## Сравнение с альтернативами

| Функция                   | cgltf    | tinygltf | fastgltf |
|---------------------------|----------|----------|----------|
| Чтение glTF 2.0           | Да       | Да       | Да       |
| Запись glTF 2.0           | Да       | Да       | Да       |
| Поддержка расширений      | Да       | Частично | Да       |
| Декодирование изображений | Да       | Да       | Нет      |
| Built-in Draco            | Нет      | Да       | Нет      |
| Memory callbacks          | Да       | Нет      | Частично |
| Android assets            | Нет      | Да       | Да       |
| Accessor utilities        | Да       | Нет      | Да       |
| Sparse accessor utilities | Частично | Нет      | Да       |
| Matrix accessor utilities | Частично | Нет      | Да       |
| Node transform utilities  | Да       | Нет      | Да       |

**Когда выбрать fastgltf:**

- Нужна максимальная скорость загрузки
- Требуется современный C++ API (std::variant, std::optional)
- Важна типобезопасность
- Нужны accessor tools для работы с данными

**Когда выбрать альтернативы:**

- **tinygltf** — если нужна встроенная загрузка изображений или Draco-декомпрессия
- **cgltf** — если нужен C API или header-only решение

## Философия

fastgltf следует принципу C++: "you don't pay for what you don't use".

**По умолчанию:**

- Только парсинг JSON
- GLB-буферы загружаются в память (ByteView/Array)
- Внешние буферы — только URI (без загрузки)

**Опционально (через Options):**

- `LoadExternalBuffers` — загрузка внешних .bin файлов
- `LoadExternalImages` — загрузка внешних изображений
- `DecomposeNodeMatrices` — разложение матриц на TRS

> **Для понимания:** fastgltf — это не кухонный комбайн, который пытается сделать всё. Это острый японский нож: делает
> одну вещь — режет glTF — но делает это идеально. Вы сами решаете, какие ингредиенты (буферы, изображения) загружать, а
> какие оставить на тарелке (URI).

## Типобезопасность

Сравнение подходов:

```cpp
// tinygltf: -1 означает "отсутствует"
if (node.mesh != -1) {
    auto& mesh = model.meshes[node.mesh];
}

// fastgltf: Optional с проверкой типа
if (node.meshIndex.has_value()) {
    auto& mesh = asset.meshes[*node.meshIndex];
}
```

## Проекты, использующие fastgltf

- [Fwog](https://github.com/JuanDiegoMontoya/Fwog) — современная абстракция OpenGL 4.6
- [Castor3D](https://github.com/DragonJoker/Castor3D) — мультиплатформенный 3D движок
- [Raz](https://github.com/Razakhel/RaZ) — современный игровой движок на C++17
- [vkguide](https://vkguide.dev) — современный туториал по Vulkan
- [lvgl](https://github.com/lvgl/lvgl) — графическая библиотека для встраиваемых систем
- [OptiX_Apps](https://github.com/NVIDIA/OptiX_Apps) — официальные примеры NVIDIA OptiX
- [vk-gltf-viewer](https://github.com/stripe2933/vk-gltf-viewer) — высокопроизводительный glTF рендерер на Vulkan

## Оригинальная документация

- [fastgltf docs](https://github.com/spnda/fastgltf/tree/main/docs) — официальная документация
- [glTF 2.0 Specification](https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html) — спецификация формата
- [glTF Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf) — краткий справочник от Khronos

---

## Основные понятия fastgltf

Введение в glTF 2.0 и внутреннюю структуру fastgltf.

## glTF 2.0

**glTF** (GL Transmission Format) — открытый формат 3D-моделей от Khronos Group. Версия 2.0 описывает сцены, меши,
материалы, анимации и скиннинг.

Модель состоит из:

- **JSON** — метаданные: структура сцены, ссылки на буферы и изображения
- **Буферы** — сырые байты (вершины, индексы, анимационные ключи)
- **Изображения** — текстуры (PNG, JPEG, KTX2 и др.)

```
glTF модель
├── model.gltf (JSON)
├── model.bin (буфер с геометрией)
└── textures/
    ├── diffuse.png
    └── normal.png
```

### JSON vs GLB

| Формат           | Описание                                                                                |
|------------------|-----------------------------------------------------------------------------------------|
| **JSON (.gltf)** | Файл ссылается на внешние .bin и изображения. Может содержать base64-встроенные данные. |
| **GLB (.glb)**   | Один бинарный файл: JSON-чанк + бинарные чанки. Первый буфер обычно встроен.            |

`Parser::loadGltf()` определяет формат автоматически.

## Структура Asset

Результат парсинга — `fastgltf::Asset`:

```
Asset
├── accessors[]      — типизированные данные (vertices, indices)
├── buffers[]        — сырые байты с DataSource
├── bufferViews[]    — участки буферов
├── images[]         — изображения
├── materials[]      — материалы (PBR)
├── meshes[]         — меши (массивы Primitive)
├── nodes[]          — узлы сцены
├── scenes[]         — сцены (nodeIndices)
├── animations[]     — анимации
├── skins[]          — скиннинг
├── cameras[]        — камеры
├── textures[]       — текстуры
├── samplers[]       — сэмплеры текстур
├── assetInfo        — gltfVersion, generator, copyright
├── defaultScene     — индекс сцены по умолчанию
├── extensionsUsed   — используемые расширения
└── extensionsRequired — обязательные расширения
```

> **Для понимания:** fastgltf::Asset — это коробка с деталями из IKEA. Там есть всё: винтики (буферы), доски (меши),
> инструкция (ноды). Но мы не живем в коробке. Мы достаем детали и собираем из них мебель (сущности в ECS). После сборки
> коробку (Asset) мы выбрасываем (освобождаем память), а мебель (Vulkan буферы) остается.

## Цепочка данных: Buffer → BufferView → Accessor

Данные геометрии идут по цепочке:

```
Buffer (сырые байты)
    ↓
BufferView (участок буфера: offset, length, stride)
    ↓
Accessor (типизированное представление: Vec3 float, Scalar uint16)
    ↓
Primitive (использование в меше)
```

### Buffer

Массив байтов с источником данных (`DataSource`):

```cpp
struct Buffer {
    std::string name;
    size_t byteLength;
    DataSource data;  // variant с источником
};
```

### BufferView

Участок буфера:

```cpp
struct BufferView {
    size_t bufferIndex;      // Индекс в asset.buffers
    size_t byteOffset;       // Смещение в буфере
    size_t byteLength;       // Длина участка
    size_t byteStride;       // 0 = плотная упаковка
    BufferTarget target;     // ArrayBuffer / ElementArrayBuffer
};
```

### Accessor

Типизированное описание данных:

```cpp
struct Accessor {
    size_t byteOffset;           // Смещение в BufferView
    size_t count;                // Количество элементов
    AccessorType type;           // Scalar, Vec2, Vec3, Vec4, Mat2, Mat3, Mat4
    ComponentType componentType; // Byte, UnsignedShort, Float, ...
    bool normalized;             // Нормализация в [0,1] или [-1,1]
    Optional<size_t> bufferViewIndex;
    Optional<SparseAccessor> sparse;
    AccessorBoundsArray min, max;
};
```

> **Для понимания:** Buffer — это длинная колбаса сырого фарша (байты). BufferView — это нарезка этой колбасы на куски.
> А Accessor — это этикетка, которая говорит: "В этом куске лежат vec3 позиции, читать с шагом 12 байт". Не путайте
> этикетку с самой колбасой.

### Вычисление размера

```cpp
size_t elementSize = getNumComponents(type) * getComponentByteSize(componentType);
size_t totalSize = count * elementSize;
```

## Индексация и ссылки

Ссылки между объектами — через `std::size_t` (индекс в векторе):

```cpp
// Scene → Nodes
for (size_t nodeIdx : scene.nodeIndices) {
    auto& node = asset.nodes[nodeIdx];
}

// Node → Mesh
if (node.meshIndex.has_value()) {
    auto& mesh = asset.meshes[*node.meshIndex];
}

// Primitive → Accessor (позиции)
auto it = primitive.findAttribute("POSITION");
if (it != primitive.attributes.cend()) {
    auto& accessor = asset.accessors[it->accessorIndex];
}
```

### Optional

Опциональные ссылки используют `Optional<size_t>`:

```cpp
if (primitive.indicesAccessor.has_value()) {
    auto& accessor = asset.accessors[*primitive.indicesAccessor];
}
```

### Variant

Node::transform — `std::variant<TRS, math::fmat4x4>`:

```cpp
// Матрица
if (std::holds_alternative<fastgltf::math::fmat4x4>(node.transform)) {
    auto& matrix = std::get<fastgltf::math::fmat4x4>(node.transform);
}

// TRS (при Options::DecomposeNodeMatrices)
if (std::holds_alternative<fastgltf::TRS>(node.transform)) {
    auto& trs = std::get<fastgltf::TRS>(node.transform);
}
```

## DataSource

`DataSource` — `std::variant` с источниками данных буфера/изображения:

| Вариант                 | Когда появляется                           | Содержимое                |
|-------------------------|--------------------------------------------|---------------------------|
| `sources::ByteView`     | GLB, base64 без копирования                | `span<const std::byte>`   |
| `sources::Array`        | GLB, встроенные данные                     | `StaticVector<std::byte>` |
| `sources::Vector`       | `LoadExternalBuffers`/`LoadExternalImages` | `std::vector<std::byte>`  |
| `sources::URI`          | Внешний файл без загрузки                  | Путь к файлу              |
| `sources::CustomBuffer` | `setBufferAllocationCallback`              | `customId`                |
| `sources::BufferView`   | Изображение в bufferView                   | Индекс bufferView         |
| `sources::Fallback`     | EXT_meshopt_compression                    | Данные недоступны         |

## Expected и обработка ошибок

Функции загрузки возвращают `fastgltf::Expected<T>`:

```cpp
#include <print>
#include <fastgltf/core.hpp>

auto asset = parser.loadGltf(data.get(), basePath);

// Проверка ошибки через std::expected
if (asset.error() != fastgltf::Error::None) {
    std::println(stderr, "Ошибка загрузки: {}", fastgltf::getErrorMessage(asset.error()));
    return;
}

// Доступ к значению
fastgltf::Asset& model = asset.get();
// или
fastgltf::Asset* model = asset.get_if();  // nullptr при ошибке
```

## Primitive

Часть меша с одним материалом:

```cpp
struct Primitive {
    SmallVector<Attribute, 4> attributes;  // POSITION, NORMAL, TEXCOORD_0, ...
    PrimitiveType type;                     // Points, Lines, Triangles, ...
    Optional<size_t> indicesAccessor;       // Индексы вершин
    Optional<size_t> materialIndex;         // Материал

    // Morph targets
    std::vector<MorphTarget> targets;

    // Методы
    const Attribute* findAttribute(std::string_view name) const;
    const Attribute* findTargetAttribute(size_t targetIndex, std::string_view name) const;
};
```

Доступ к атрибутам:

```cpp
for (const auto& primitive : mesh.primitives) {
    // Позиции
    if (auto* pos = primitive.findAttribute("POSITION")) {
        auto& accessor = asset.accessors[pos->accessorIndex];
        // Чтение вершин...
    }

    // Нормали
    if (auto* norm = primitive.findAttribute("NORMAL")) {
        auto& accessor = asset.accessors[norm->accessorIndex];
        // Чтение нормалей...
    }
}
```

## Node и Scene

### Node

Узел иерархии сцены:

```cpp
struct Node {
    Optional<size_t> meshIndex;
    Optional<size_t> skinIndex;
    Optional<size_t> cameraIndex;
    Optional<size_t> lightIndex;
    std::vector<size_t> children;  // Индексы дочерних узлов

    std::variant<TRS, math::fmat4x4> transform;
    std::string name;
};
```

### Scene

Набор корневых узлов:

```cpp
struct Scene {
    std::vector<size_t> nodeIndices;  // Корневые узлы
    std::string name;
};
```

### Обход иерархии

```cpp
// Используйте iterateSceneNodes из tools.hpp
fastgltf::iterateSceneNodes(asset, sceneIndex, fastgltf::math::fmat4x4(),
    [&](fastgltf::Node& node, fastgltf::math::fmat4x4 transform) {
        if (node.meshIndex.has_value()) {
            drawMesh(*node.meshIndex, transform);
        }
    });
```

## Material

PBR материал:

```cpp
struct Material {
    PBRData pbrData;  // baseColor, metallic, roughness

    Optional<size_t> normalTexture;
    Optional<size_t> occlusionTexture;
    Optional<size_t> emissiveTexture;

    std::array<float, 3> emissiveFactor;
    AlphaMode alphaMode;
    float alphaCutoff;
    bool doubleSided;
    bool unlit;  // KHR_materials_unlit
};
```

### PBRData

```cpp
struct PBRData {
    Optional<size_t> baseColorTexture;
    Optional<size_t> metallicRoughnessTexture;

    std::array<float, 4> baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
};
```

### AlphaMode

```cpp
enum class AlphaMode : std::uint8_t {
    Opaque,      // Непрозрачный
    Mask,        // Маска (alphaCutoff)
    Blend        // Смешивание
};
```

### Текстуры

Текстуры ссылаются на `asset.textures[]`, которые содержат `imageIndex` и `samplerIndex`:

```cpp
struct Texture {
    Optional<size_t> imageIndex;
    Optional<size_t> samplerIndex;
    std::string name;
};
```

### Sampler

```cpp
struct Sampler {
    Optional<Filter> magFilter;   // Увеличение
    Optional<Filter> minFilter;   // Уменьшение
    Optional<Wrap> wrapS;         // U координата
    Optional<Wrap> wrapT;         // V координата
    std::string name;
};
```

### Image

```cpp
struct Image {
    std::string name;
    DataSource data;  // URI, BufferView, ByteView, Vector
    Optional<MimeType> mimeType;
};
```

> **Для понимания:** Material — это рецепт для рендеринга поверхности. PBRData — это список ингредиентов (цвет,
> металличность, шероховатость), текстуры — это фотографии ингредиентов, а sampler — это инструкция по их смешиванию (
> фильтрация, повторение). AlphaMode — это указание, должен ли ингредиент быть прозрачным, а doubleSided — нужно ли
> готовить обе стороны блюда.

## doubleSided и unlit

### doubleSided

```cpp
if (material.doubleSided) {
    // Отключить backface culling
    glDisable(GL_CULL_FACE);
    // или
    rasterizer.cullMode = VK_CULL_MODE_NONE;
}
```

### unlit (KHR_materials_unlit)

```cpp
if (material.unlit) {
    // Использовать unlit шейдер (без освещения)
    // baseColorFactor напрямую как цвет пикселя
}
```

## Утилиты типов

### ComponentType

```cpp
enum class ComponentType : std::uint16_t {
    Byte = 5120,
    UnsignedByte = 5121,
    Short = 5122,
    UnsignedShort = 5123,
    Int = 5124,
    UnsignedInt = 5125,
    Float = 5126,
    Double = 5130  // Только с Options::AllowDouble
};
```

### AccessorType

```cpp
enum class AccessorType : std::uint8_t {
    Scalar,
    Vec2,
    Vec3,
    Vec4,
    Mat2,
    Mat3,
    Mat4
};
```

### PrimitiveType

```cpp
enum class PrimitiveType : std::uint8_t {
    Points,
    Lines,
    LineLoop,
    LineStrip,
    Triangles,
    TriangleStrip,
    TriangleFan
};
```

### BufferTarget

```cpp
enum class BufferTarget : std::uint16_t {
    ArrayBuffer = 34962,
    ElementArrayBuffer = 34963
};
```

## Заключение

Fastgltf предоставляет полную, типобезопасную реализацию glTF 2.0 с акцентом на производительность и современный C++.

### Ключевые особенности

1. **Производительность**: SIMD-оптимизации, memory mapping, минимальные аллокации
2. **Типобезопасность**: `std::optional`, `std::variant`, проверки во время компиляции
3. **Гибкость**: DataSource варианты, кастомные callbacks, расширения
4. **Современный C++**: C++17 минимум, поддержка C++20/23/26 фич

### Структура данных

- **Asset** — корневой контейнер со всеми данными
- **DataSource** — варианты источников данных (URI, BufferView, ByteView, etc.)
- **Accessor chain** — Buffer → BufferView → Accessor → Primitive
- **Иерархия сцены** — Scene → Nodes → Mesh → Primitive

### Обработка ошибок

- **Expected<T>** — современная обработка ошибок
- **getErrorMessage()** — человекочитаемые сообщения
- **getErrorName()** — машинно-читаемые коды

### Расширения

Fastgltf поддерживает большинство расширений glTF через битовые маски в конструкторе Parser.

### Для ProjectV

Fastgltf идеально подходит для ProjectV благодаря:

1. **Data-Oriented Design**: Плоские массивы для преобразования в SoA
2. **GPU-ready**: Прямая запись в mapped GPU buffers
3. **Производительность**: Критична для воксельного рендеринга
4. **Современный C++**: Соответствует требованиям C++26 проекта

### Дальнейшее изучение

- **Интеграция**: См. `02_integration.md` для подключения к ProjectV
- **Продвинутые темы**: См. `03_advanced.md` для сложных сценариев
- **Исходники**: `external/fastgltf/include/` для деталей реализации

Fastgltf — это не просто парсер glTF, это фундамент для высокопроизводительной загрузки 3D контента в современном C++
проекте.

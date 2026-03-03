# Основные понятия fastgltf

🟡 **Уровень 2: Средний**

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
auto asset = parser.loadGltf(data.get(), basePath);

// Проверка ошибки
if (asset.error() != fastgltf::Error::None) {
    std::cerr << fastgltf::getErrorMessage(asset.error()) << "\n";
    return;
}

// Доступ к значению
fastgltf::Asset& model = asset.get();
// или
fastgltf::Asset* model = asset.get_if();  // nullptr при ошибке
// или
fastgltf::Asset& model = *asset;  // UB при ошибке!
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
    bool unlit;
};
```

## Утилиты типов

Функции из `types.hpp`:

| Функция                                   | Описание                          |
|-------------------------------------------|-----------------------------------|
| `getNumComponents(AccessorType)`          | Количество компонентов (Vec3 → 3) |
| `getElementByteSize(type, componentType)` | Размер элемента в байтах          |
| `getComponentByteSize(ComponentType)`     | Размер компонента в байтах        |
| `getComponentBitSize(ComponentType)`      | Размер компонента в битах         |
| `getGLComponentType(ComponentType)`       | OpenGL-константа типа             |
| `getAccessorTypeName(AccessorType)`       | Строковое имя («VEC3», «SCALAR»)  |

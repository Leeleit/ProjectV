## fastgltf

<!-- anchor: 00_overview -->

🟡 **Уровень 2: Средний**

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

## Документация

| Файл                                           | Содержание                                     |
|------------------------------------------------|------------------------------------------------|
| [01_quickstart.md](01_quickstart.md)           | Быстрый старт: минимальный пример загрузки     |
| [02_integration.md](02_integration.md)         | Интеграция: CMake, Options, Extensions         |
| [03_concepts.md](03_concepts.md)               | Основные понятия: Asset, Accessor, DataSource  |
| [04_api-reference.md](04_api-reference.md)     | Справочник API: Parser, Asset, structures      |
| [05_tools.md](05_tools.md)                     | Accessor tools: чтение данных                  |
| [06_performance.md](06_performance.md)         | Производительность и benchmarks                |
| [07_advanced.md](07_advanced.md)               | Продвинутые темы: sparse, animations, skinning |
| [08_troubleshooting.md](08_troubleshooting.md) | Решение проблем: ошибки и диагностика          |
| [09_glossary.md](09_glossary.md)               | Глоссарий терминов                             |

## Требования

- **C++17** или новее (опционально C++20 для модулей)
- **simdjson** (подгружается автоматически через CMake)
- Платформы: Windows, Linux, macOS, Android

## Оригинальная документация

- [fastgltf docs](https://github.com/spnda/fastgltf/tree/main/docs) — официальная документация
- [glTF 2.0 Specification](https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html) — спецификация формата
- [glTF Reference Guide](https://www.khronos.org/files/gltf20-reference-guide.pdf) — краткий справочник от Khronos

---

## Основные понятия fastgltf

<!-- anchor: 03_concepts -->

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

---

## Глоссарий fastgltf

<!-- anchor: 09_glossary -->

🟢 **Уровень 1: Начинающий**

Термины glTF 2.0 и fastgltf.

## Карта связей ключевых терминов

```
Файл .gltf/.glb
      ↓
    Parser
      ↓
    Asset
      ├── buffers[] (DataSource)
      ├── bufferViews[] (byteOffset/Stride)
      ├── accessors[] (Type/ComponentType)
      ├── meshes[] (Primitive[])
      ├── materials[] (PBR)
      ├── nodes[] (transform: TRS/matrix)
      ├── scenes[] (nodeIndices)
      ├── animations[]
      └── skins[]

Buffer → BufferView → Accessor → Primitive
                                     ↓
                                   Mesh → Node → Scene
```

---

## glTF 2.0

| Термин    | Определение                                                                                                          |
|-----------|----------------------------------------------------------------------------------------------------------------------|
| **glTF**  | GL Transmission Format — формат 3D-моделей от Khronos Group. Состоит из JSON-файла с метаданными и бинарных буферов. |
| **GLB**   | Бинарный контейнер glTF: один файл с JSON-чанком и бинарными чанками.                                                |
| **Asset** | Результат парсинга glTF. Содержит все данные модели: меши, материалы, узлы, анимации.                                |

## Цепочка данных

| Термин         | Определение                                                                         | Пример                             |
|----------------|-------------------------------------------------------------------------------------|------------------------------------|
| **Buffer**     | Массив байтов — сырые данные. Источник определяется через `DataSource`.             | `Buffer[0]: 16384 байт`            |
| **BufferView** | Участок буфера: offset, length, stride. Связывает Accessor с Buffer.                | `offset=0, length=8192, stride=12` |
| **Accessor**   | Типизированное описание данных: тип, componentType, count. Ссылается на BufferView. | `Vec3 Float, count=1024`           |

### Формула размера

```
size = count * getNumComponents(type) * getComponentByteSize(componentType)
```

## Геометрия

| Термин        | Определение                                                                                           |
|---------------|-------------------------------------------------------------------------------------------------------|
| **Primitive** | Часть меша: режим отрисовки, атрибуты, индексы, материал. Доступ к атрибутам через `findAttribute()`. |
| **Mesh**      | Набор примитивов. Один меш может содержать несколько примитивов с разными материалами.                |
| **Attribute** | Именованная ссылка на accessor: POSITION, NORMAL, TEXCOORD_0, TANGENT, JOINTS_0, WEIGHTS_0.           |

## Иерархия сцены

| Термин    | Определение                                                                                 |
|-----------|---------------------------------------------------------------------------------------------|
| **Node**  | Узел сцены. Содержит transform (TRS или matrix), ссылки на mesh, skin, camera, children.    |
| **Scene** | Набор корневых узлов через `nodeIndices`. У модели может быть несколько сцен.               |
| **TRS**   | Decomposed transform: Translation, Rotation, Scale. Появляется при `DecomposeNodeMatrices`. |

## Материалы

| Термин          | Определение                                                                           |
|-----------------|---------------------------------------------------------------------------------------|
| **Material**    | PBR материал: baseColor, metallic, roughness, normal, emission.                       |
| **PBR**         | Physically Based Rendering — подход к материалов, основанный на физических свойствах. |
| **TextureInfo** | Ссылка на текстуру: textureIndex, texCoordIndex, опциональный transform.              |
| **Sampler**     | Настройки фильтрации и wrapping для текстуры.                                         |

## Анимации

| Термин               | Определение                                                                            |
|----------------------|----------------------------------------------------------------------------------------|
| **Animation**        | Набор каналов и сэмплеров для анимации свойств узлов.                                  |
| **AnimationChannel** | Связь между target (node + path) и sampler.                                            |
| **AnimationSampler** | Интерполяция между ключевыми кадрами: input (время), output (значения), interpolation. |
| **Interpolation**    | LINEAR, STEP, CUBICSPLINE — метод интерполяции между кадрами.                          |

## Скиннинг

| Термин                  | Определение                                                   |
|-------------------------|---------------------------------------------------------------|
| **Skin**                | Данные для skeletal animation: joints, inverseBindMatrices.   |
| **Joint**               | Кость скелета — ссылка на Node.                               |
| **Inverse Bind Matrix** | Матрица для перевода вершины в пространство кости.            |
| **JOINTS_0**            | Атрибут примитива: индексы костей для каждой вершины (uvec4). |
| **WEIGHTS_0**           | Атрибут примитива: веса влияния костей (vec4).                |

## Morph Targets

| Термин           | Определение                                                                           |
|------------------|---------------------------------------------------------------------------------------|
| **Morph Target** | Blend shape — дельты позиций/нормалей для деформации меша.                            |
| **Weight**       | Вес morph target (0-1) для интерполяции между базовой и целевой формой.               |
| **targets**      | Массив morph targets в Primitive. Каждый target — набор атрибутов (POSITION, NORMAL). |

## Sparse Accessors

| Термин              | Определение                                                                 |
|---------------------|-----------------------------------------------------------------------------|
| **Sparse Accessor** | Accessor с частичным обновлением данных. Хранит только изменённые значения. |
| **Sparse Indices**  | Индексы элементов, которые заменяются.                                      |
| **Sparse Values**   | Новые значения для указанных индексов.                                      |

## Система загрузки fastgltf

| Термин            | Определение                                                                        |
|-------------------|------------------------------------------------------------------------------------|
| **Parser**        | Класс для парсинга glTF/GLB. Не потокобезопасен, переиспользуйте между загрузками. |
| **Expected\<T\>** | Тип возврата функций загрузки: содержит либо T, либо Error.                        |
| **DataSource**    | variant с источником данных буфера/изображения.                                    |

## DataSource варианты

| Вариант          | Когда появляется              | Нужен кастомный адаптер? |
|------------------|-------------------------------|--------------------------|
| **ByteView**     | GLB, base64 без копирования   | Нет                      |
| **Array**        | GLB, встроенные данные        | Нет                      |
| **Vector**       | `LoadExternalBuffers`         | Нет                      |
| **URI**          | Внешний файл без загрузки     | Да                       |
| **CustomBuffer** | `setBufferAllocationCallback` | Да                       |
| **BufferView**   | Изображение в bufferView      | —                        |
| **Fallback**     | EXT_meshopt_compression       | Да                       |

## Options

| Опция                           | Эффект                                                |
|---------------------------------|-------------------------------------------------------|
| **LoadExternalBuffers**         | Загрузка внешних .bin файлов в Vector                 |
| **LoadExternalImages**          | Загрузка внешних изображений                          |
| **DecomposeNodeMatrices**       | Разложение матриц узлов на TRS                        |
| **GenerateMeshIndices**         | Генерация индексов для примитивов без indicesAccessor |
| **AllowDouble**                 | Разрешить GL_DOUBLE как componentType                 |
| **DontRequireValidAssetMember** | Пропустить проверку поля asset                        |

## Category

| Значение           | Что парсится                                       |
|--------------------|----------------------------------------------------|
| **All**            | Всё                                                |
| **OnlyRenderable** | Всё, кроме Animations, Skins                       |
| **OnlyAnimations** | Только Animations, Accessors, BufferViews, Buffers |

## Accessor Types

| Тип        | Компоненты | Пример               |
|------------|------------|----------------------|
| **Scalar** | 1          | float, uint32_t      |
| **Vec2**   | 2          | glm::vec2            |
| **Vec3**   | 3          | glm::vec3            |
| **Vec4**   | 4          | glm::vec4, glm::quat |
| **Mat2**   | 4          | glm::mat2            |
| **Mat3**   | 9          | glm::mat3            |
| **Mat4**   | 16         | glm::mat4            |

## Component Types

| Тип               | Размер           | OpenGL константа  |
|-------------------|------------------|-------------------|
| **Byte**          | 1 byte (signed)  | GL_BYTE           |
| **UnsignedByte**  | 1 byte           | GL_UNSIGNED_BYTE  |
| **Short**         | 2 bytes (signed) | GL_SHORT          |
| **UnsignedShort** | 2 bytes          | GL_UNSIGNED_SHORT |
| **UnsignedInt**   | 4 bytes          | GL_UNSIGNED_INT   |
| **Float**         | 4 bytes          | GL_FLOAT          |

## Термины которые часто путают

| Пара                      | Различие                                                                  |
|---------------------------|---------------------------------------------------------------------------|
| **Buffer vs BufferView**  | Buffer — сырые байты; BufferView — "окно" в Buffer с offset/length/stride |
| **Accessor vs Primitive** | Accessor — описание данных; Primitive — использование этих данных         |
| **Node vs Scene**         | Node — элемент иерархии; Scene — набор корневых Node                      |
| **DataSource vs Buffer**  | DataSource — откуда брать данные; Buffer — сами данные                    |
| **TRS vs Matrix**         | TRS — decomposed transform; Matrix — combined transform                   |

---

Используйте этот глоссарий как справочник при чтении других разделов.
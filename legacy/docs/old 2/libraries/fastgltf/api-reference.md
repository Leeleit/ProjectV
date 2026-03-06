# Справочник API fastgltf 🟡

Краткое описание классов и функций fastgltf. Полные объявления —
в [external/fastgltf/include](../../external/fastgltf/include).

Include по сценарию: см. [Интеграция §6](integration.md#6-include-и-заголовки).

## Какую функцию когда вызывать

| Ситуация                    | Функция / класс                                                   | Пример                                                                  |
|-----------------------------|-------------------------------------------------------------------|-------------------------------------------------------------------------|
| Загрузить glTF/GLB          | `Parser::loadGltf`, `loadGltfJson`, `loadGltfBinary`              | [fastgltf_load_mesh.cpp](../../examples/fastgltf_load_mesh.cpp)         |
| Определить тип файла        | `determineGltfFileType`                                           |                                                                         |
| Проверить Asset             | `validate`                                                        |                                                                         |
| Прочитать вершины/индексы   | `iterateAccessor`, `iterateAccessorWithIndex`, `copyFromAccessor` | [fastgltf_load_mesh.cpp](../../examples/fastgltf_load_mesh.cpp)         |
| Range-based for по accessor | `for (auto e : iterateAccessor<T>(asset, accessor))`              |                                                                         |
| Обойти иерархию сцены       | `iterateSceneNodes`                                               | [fastgltf_load_mesh.cpp](../../examples/fastgltf_load_mesh.cpp)         |
| Получить матрицу узла       | `getLocalTransformMatrix`, `getTransformMatrix`                   |                                                                         |
| Проверить ошибку            | `asset.error()`, `getErrorName`, `getErrorMessage`                | [fastgltf_load_mesh.cpp](../../examples/fastgltf_load_mesh.cpp)         |
| Экспорт в JSON/GLB          | `Exporter::writeGltfJson`, `FileExporter::writeGltfJson`          |                                                                         |
| Загрузка текстур            | Обработка `Image::data` (DataSource)                              | [fastgltf_load_textures.cpp](../../examples/fastgltf_load_textures.cpp) |
| Работа с материалами        | `Material`, `pbrData`, расширения                                 | [fastgltf_materials.cpp](../../examples/fastgltf_materials.cpp)         |
| Анимации                    | `Animation`, `AnimationChannel`, `AnimationSampler`               | [fastgltf_animations.cpp](../../examples/fastgltf_animations.cpp)       |

**Примечание**: Все примеры находятся в [docs/examples/](../../examples/) и компилируются через CMake.

---

## Parser

Класс парсинга glTF. Не потокобезопасен; рекомендуется переиспользовать между загрузками.

### Конструктор

```cpp
explicit Parser(Extensions extensionsToLoad = Extensions::None) noexcept;
```

`extensionsToLoad` — битовая маска расширений, которые модель может использовать.

### Загрузка

| Метод                                                                             | Описание                               |
|-----------------------------------------------------------------------------------|----------------------------------------|
| `Expected<Asset> loadGltf(GltfDataGetter&, path, Options = None, Category = All)` | Определяет тип (JSON/GLB) и загружает. |
| `Expected<Asset> loadGltfJson(GltfDataGetter&, path, Options, Category)`          | Только JSON glTF.                      |
| `Expected<Asset> loadGltfBinary(GltfDataGetter&, path, Options, Category)`        | Только GLB.                            |

### Callbacks

| Метод                                                     | Описание                                  |
|-----------------------------------------------------------|-------------------------------------------|
| `setBufferAllocationCallback(mapCallback, unmapCallback)` | Кастомная аллокация буферов (напр., GPU). |
| `setBase64DecodeCallback(decodeCallback)`                 | Кастомный base64-декодер.                 |
| `setExtrasParseCallback(callback)`                        | Обработка extras при парсинге.            |
| `setUserPointer(pointer)`                                 | Указатель, передаваемый в callbacks.      |

---

## Asset

Результат парсинга. Поля:

| Поле                                   | Тип                   | Описание                             |
|----------------------------------------|-----------------------|--------------------------------------|
| `accessors`                            | `vector<Accessor>`    | Accessors (vertices, indices и т.д.) |
| `buffers`                              | `vector<Buffer>`      | Буферы с `DataSource`                |
| `bufferViews`                          | `vector<BufferView>`  | Участки буферов                      |
| `images`                               | `vector<Image>`       | Изображения                          |
| `materials`                            | `vector<Material>`    | Материалы                            |
| `meshes`                               | `vector<Mesh>`        | Меши (массив Primitive)              |
| `nodes`                                | `vector<Node>`        | Узлы сцены                           |
| `scenes`                               | `vector<Scene>`       | Сцены (`nodeIndices`)                |
| `animations`                           | `vector<Animation>`   | Анимации                             |
| `assetInfo`                            | `Optional<AssetInfo>` | gltfVersion, generator, copyright    |
| `extensionsUsed`, `extensionsRequired` | `vector<string>`      | Расширения                           |
| `defaultScene`                         | `Optional<size_t>`    | Индекс сцены по умолчанию            |
| `availableCategories`                  | `Category`            | Что было распарсено                  |
| `materialVariants`                     | `vector<string>`      | Имена вариантов материалов           |

---

## GltfDataBuffer, GltfFileStream, MappedGltfFile

Реализации `GltfDataGetter`:

| Класс                     | Фабрика / конструктор                                              | Проверка                       | Когда использовать                                                                     |
|---------------------------|--------------------------------------------------------------------|--------------------------------|----------------------------------------------------------------------------------------|
| **GltfDataBuffer**        | `FromPath(path)`, `FromBytes(ptr, size)`, `FromSpan(span)` (C++20) | `Expected` → `error() == None` | Стандартная загрузка в RAM.                                                            |
| **GltfFileStream**        | `GltfFileStream(path)`                                             | `isOpen()`                     | Потоковое чтение. Не возвращает Expected.                                              |
| **MappedGltfFile**        | `FromPath(path)`                                                   | `Expected` → `error() == None` | Memory-mapped. Только при `FASTGLTF_HAS_MEMORY_MAPPED_FILE` (Win/Linux/macOS desktop). |
| **AndroidGltfDataBuffer** | `FromAsset(path, byteOffset=0)`                                    | `Expected`                     | Android APK assets. После `setAndroidAssetManager`.                                    |

---

## Error

Перечисление ошибок. Функции:

- `getErrorName(Error)` — краткое имя.
- `getErrorMessage(Error)` — описание.

| Код                          | Описание                                                     |
|------------------------------|--------------------------------------------------------------|
| `InvalidPath`                | Неверная директория в loadGltf.                              |
| `MissingExtensions`          | Расширения в extensionsRequired не включены в Parser.        |
| `UnknownRequiredExtension`   | Неподдерживаемое расширение.                                 |
| `InvalidJson`                | Ошибка парсинга JSON.                                        |
| `InvalidGltf`                | Невалидные или отсутствующие данные glTF.                    |
| `InvalidOrMissingAssetField` | Поле asset в JSON отсутствует или некорректно.               |
| `InvalidGLB`                 | Невалидный GLB.                                              |
| `MissingExternalBuffer`      | Внешний буфер не найден (при LoadExternalBuffers).           |
| `UnsupportedVersion`         | Неподдерживаемая версия glTF.                                |
| `InvalidURI`                 | Ошибка парсинга URI.                                         |
| `InvalidFileData`            | Тип файла не определён или данные повреждены.                |
| `FailedWritingFiles`         | Экспорт: не удалось записать буферы/изображения.             |
| `FileBufferAllocationFailed` | GltfDataBuffer не смог выделить память (очень большой файл). |

---

## Options

Флаги для `loadGltf` (побитовое ИЛИ). Подробнее — [Интеграция §2](integration.md#2-options).

| Опция                                        | Кратко                              |
|----------------------------------------------|-------------------------------------|
| `None`                                       | Парсинг JSON; GLB в памяти.         |
| `LoadExternalBuffers`                        | Внешние .bin в память.              |
| `LoadExternalImages`                         | Внешние изображения.                |
| `DecomposeNodeMatrices`                      | TRS вместо матриц.                  |
| `GenerateMeshIndices`                        | Индексы для примитивов без indices. |
| `AllowDouble`, `DontRequireValidAssetMember` | См. integration.                    |

`LoadGLBBuffers` — deprecated.

---

## Extensions

Битовая маска расширений. Передаются в конструктор Parser.

---

## Category

Маска того, что парсить. Подробнее — [Интеграция §3](integration.md#3-category).

---

## determineGltfFileType, validate

```cpp
GltfType determineGltfFileType(GltfDataGetter& data);
Error validate(const Asset& asset);
```

`determineGltfFileType` — возвращает `GltfType::glTF`, `GltfType::GLB` или `GltfType::Invalid`. `loadGltf` вызывает
внутри.

`validate` — строгая проверка Asset по спецификации glTF 2.0. Рекомендуется в Debug.

---

## BufferInfo

Структура для `setBufferAllocationCallback`:

- `mappedMemory` — указатель на замапленную память (GPU или RAM).
- `customId` — ваш ID (CustomBufferId), сохраняется в `sources::CustomBuffer`.

---

## Структуры данных (кратко)

### Accessor

`byteOffset`, `count`, `type` (AccessorType), `componentType`, `normalized`, `min`, `max` (AccessorBoundsArray),
`bufferViewIndex`, `sparse` (SparseAccessor).

### BufferView

`bufferIndex`, `byteOffset`, `byteLength`, `byteStride`, `target` (ArrayBuffer/ElementArrayBuffer).

### Primitive

`attributes` (SmallVector<Attribute,4>), `type` (PrimitiveType), `indicesAccessor`, `materialIndex`, `targets` (morph
targets). `findAttribute(name)` → итератор. `findTargetAttribute(targetIndex, name)` — атрибут morph target.

### Node

`meshIndex`, `skinIndex`, `cameraIndex`, `lightIndex`, `children`, `transform` (variant<TRS, fmat4x4>), `name`.

### Scene

`nodeIndices` — массив индексов корневых узлов. `name`.

### Material

`pbrData` (baseColor, metallic, roughness), `normalTexture`, `alphaMode`, `doubleSided`, `unlit`, `alphaCutoff`.
Расширения — `clearcoat`, `transmission`, `volume` и др.

### Image

`name`, `data` (DataSource: URI, BufferView, ByteView, Array, Vector).

### Animation

`name`, `channels` (target node + path), `samplers` (input/output accessors). Ключевые кадры — через accessors.

### Skin

`name`, `joints` (индексы Node), `inverseBindMatrices` (Optional<size_t> accessor), `skeleton`.

### Camera

`name`, `type` (Perspective/Orthographic), `perspective` или `orthographic` (поле fov, aspectRatio и т.д.).

---

## Утилиты расширений

```cpp
std::string_view stringifyExtension(Extensions extensions);  // Имя первого установленного бита
auto stringifyExtensionBits(Extensions extensions) -> vector<string>;  // Список имён
```

Для отладки и логирования.

---

## tools.hpp

### iterateAccessor (две перегрузки)

Возвращает `IterableAccessor` для range-based for:

```cpp
// Range-based for
for (auto elem : fastgltf::iterateAccessor<T>(asset, accessor)) { ... }

// С лямбдой
void iterateAccessor(const Asset& asset, const Accessor& accessor, Functor&& func,
    const BufferDataAdapter& adapter = {});
```

Итерация по элементам accessor с конвертацией типа. Поддерживает sparse accessors. `func(element)` — лямбда.

### iterateAccessorWithIndex

```cpp
void iterateAccessorWithIndex<ElementType>(..., Functor&& func, ...);
```

Лямбда получает `(element, index)`.

### copyFromAccessor

```cpp
template<typename ElementType>
void copyFromAccessor(const Asset& asset, const Accessor& accessor, void* dest,
    const BufferDataAdapter& adapter = {});
```

Копирует данные accessor в массив. При прямом совпадении типов использует memcpy.

### getAccessorElement

```cpp
template<typename ElementType>
ElementType getAccessorElement(const Asset& asset, const Accessor& accessor, size_t index,
    const BufferDataAdapter& adapter = {});
```

Возвращает один элемент по индексу.

### iterateSceneNodes

```cpp
template<typename AssetType, typename Callback>
void iterateSceneNodes(AssetType&& asset, size_t sceneIndex, math::fmat4x4 initial, Callback callback);
```

Обход узлов сцены. `callback(node, matrix)` — для каждого узла с накопленной матрицей трансформации.

### getLocalTransformMatrix / getTransformMatrix

```cpp
math::fmat4x4 getLocalTransformMatrix(const Node& node);
math::fmat4x4 getTransformMatrix(const Node& node, const math::fmat4x4& base = math::fmat4x4());
```

Локальная матрица узла (из TRS или matrix) и произведение с базовой матрицей.

### DefaultBufferDataAdapter

По умолчанию accessor tools используют `DefaultBufferDataAdapter`. Работает только с буферами, у которых `DataSource`
содержит `sources::ByteView`, `sources::Array` или `sources::Vector`. Для `sources::URI` без загрузки нужен кастомный
адаптер — функтор с сигнатурой `span<const std::byte>(const Asset&, size_t bufferViewIdx)`.

### Функции типов (types.hpp)

| Функция                                   | Описание                           |
|-------------------------------------------|------------------------------------|
| `getNumComponents(AccessorType)`          | Количество компонентов (Vec3 → 3). |
| `getElementByteSize(type, componentType)` | Размер элемента в байтах.          |
| `getComponentByteSize(ComponentType)`     | Размер компонента в байтах.        |
| `getComponentBitSize(ComponentType)`      | Размер компонента в битах.         |
| `getGLComponentType(ComponentType)`       | OpenGL-константа типа.             |
| `getAccessorTypeName(AccessorType)`       | Строковое имя («VEC3», «SCALAR»).  |

---

## Exporter / FileExporter

| Класс            | Метод                                   | Описание                            |
|------------------|-----------------------------------------|-------------------------------------|
| **Exporter**     | `writeGltfJson(asset, options)`         | Сериализация в строку JSON.         |
| **Exporter**     | `writeGltfBinary(asset, options)`       | Сериализация в вектор байтов (GLB). |
| **FileExporter** | `writeGltfJson(asset, path, options)`   | Запись в файл + буферы/изображения. |
| **FileExporter** | `writeGltfBinary(asset, path, options)` | Запись GLB в файл.                  |

`ExportOptions`: `ValidateAsset`, `PrettyPrintJson`.

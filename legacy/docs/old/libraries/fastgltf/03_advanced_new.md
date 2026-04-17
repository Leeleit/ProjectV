## Accessor Tools

<!-- anchor: 05_tools -->

🟡 **Уровень 2: Средний**

Утилиты для работы с accessor данными. Заголовок: `fastgltf/tools.hpp`.

## Обзор

Accessor tools упрощают чтение данных из accessors:

- Автоматически обрабатывают sparse accessors
- Конвертируют типы данных
- Поддерживают нормализацию

```cpp
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>  // Для glm типов
```

## ElementTraits

Все функции шаблонные и требуют специализации `ElementTraits`:

```cpp
template <typename T>
struct ElementTraits {
    static constexpr AccessorType AccessorType = ...;
    static constexpr ComponentType ComponentType = ...;
};
```

### Встроенные специализации

- Все типы из `fastgltf/math.hpp` (fvec2, fvec3, fmat4x4, ...)
- `uint8_t`, `uint16_t`, `uint32_t`, `float`, `double`

### glm типы

```cpp
#include <fastgltf/glm_element_traits.hpp>

// Теперь можно использовать:
std::vector<glm::vec3> positions;
fastgltf::copyFromAccessor<glm::vec3>(asset, accessor, positions.data());
```

### Кастомные типы

```cpp
struct MyVec3 {
    float x, y, z;
};

template <>
struct fastgltf::ElementTraits<MyVec3>
    : fastgltf::ElementTraitsBase<MyVec3, AccessorType::Vec3, float> {};
```

## iterateAccessor

Итерация по элементам accessor с лямбдой.

### Сигнатура

```cpp
template <typename ElementType, typename Functor>
void iterateAccessor(const Asset& asset, const Accessor& accessor,
                     Functor&& func,
                     const BufferDataAdapter& adapter = {});
```

### Пример

```cpp
std::vector<glm::vec3> positions;

fastgltf::iterateAccessor<glm::vec3>(asset, accessor,
    [&](glm::vec3 pos) {
        positions.push_back(pos);
    });
```

## iterateAccessorWithIndex

То же, что `iterateAccessor`, но с индексом.

### Сигнатура

```cpp
template <typename ElementType, typename Functor>
void iterateAccessorWithIndex(const Asset& asset, const Accessor& accessor,
                              Functor&& func,
                              const BufferDataAdapter& adapter = {});
```

### Пример

```cpp
std::vector<uint32_t> indices;
indices.resize(accessor.count);

fastgltf::iterateAccessorWithIndex<uint32_t>(asset, accessor,
    [&](uint32_t index, size_t idx) {
        indices[idx] = index;
    });
```

## copyFromAccessor

Копирование данных в массив.

### Сигнатура

```cpp
template <typename ElementType>
void copyFromAccessor(const Asset& asset, const Accessor& accessor,
                      void* dest,
                      const BufferDataAdapter& adapter = {});
```

### Пример

```cpp
std::vector<glm::vec3> positions(accessor.count);
fastgltf::copyFromAccessor<glm::vec3>(asset, accessor, positions.data());

// Для индексов:
std::vector<uint32_t> indices(accessor.count);
fastgltf::copyFromAccessor<uint32_t>(asset, accessor, indices.data());
```

При совпадении типов используется memcpy для максимальной скорости.

## getAccessorElement

Получение одного элемента по индексу.

### Сигнатура

```cpp
template <typename ElementType>
ElementType getAccessorElement(const Asset& asset, const Accessor& accessor,
                               size_t index,
                               const BufferDataAdapter& adapter = {});
```

### Пример

```cpp
glm::vec3 firstVertex = fastgltf::getAccessorElement<glm::vec3>(
    asset, accessor, 0);
```

## Range-based for

`iterateAccessor` возвращает итератор для range-based for:

### Сигнатура

```cpp
template <typename ElementType>
auto iterateAccessor(const Asset& asset, const Accessor& accessor,
                     const BufferDataAdapter& adapter = {})
    -> IterableAccessor<ElementType, BufferDataAdapter>;
```

### Пример

```cpp
for (glm::vec3 pos : fastgltf::iterateAccessor<glm::vec3>(asset, accessor)) {
    // Обработка позиции
}

// С индексом (через external counter)
size_t idx = 0;
for (auto pos : fastgltf::iterateAccessor<glm::vec3>(asset, accessor)) {
    array[idx++] = pos;
}
```

## iterateSceneNodes

Обход иерархии узлов сцены.

### Сигнатура

```cpp
template <typename Callback>
void iterateSceneNodes(const Asset& asset, size_t sceneIndex,
                       math::fmat4x4 initialTransform,
                       Callback callback);
```

### Пример

```cpp
fastgltf::iterateSceneNodes(asset, 0, fastgltf::math::fmat4x4(),
    [&](fastgltf::Node& node, fastgltf::math::fmat4x4 transform) {
        if (node.meshIndex.has_value()) {
            drawMesh(*node.meshIndex, transform);
        }
    });
```

## getLocalTransformMatrix / getTransformMatrix

Получение матрицы трансформации узла.

### Сигнатуры

```cpp
math::fmat4x4 getLocalTransformMatrix(const Node& node);
math::fmat4x4 getTransformMatrix(const Node& node,
                                  const math::fmat4x4& base = math::fmat4x4());
```

### Пример

```cpp
// Локальная матрица (из TRS или matrix)
auto local = fastgltf::getLocalTransformMatrix(node);

// С базовой матрицей (например, родительской)
auto global = fastgltf::getTransformMatrix(node, parentTransform);
```

## BufferDataAdapter

Интерфейс для доступа к данным буферов.

### DefaultBufferDataAdapter

По умолчанию accessor tools используют `DefaultBufferDataAdapter`. Он работает только с:

- `sources::ByteView`
- `sources::Array`
- `sources::Vector`

### Кастомный адаптер

Для `sources::URI` или `sources::CustomBuffer` нужен кастомный адаптер:

```cpp
auto customAdapter = [&](const Asset& asset, size_t bufferViewIdx)
    -> std::span<const std::byte> {

    const auto& bufferView = asset.bufferViews[bufferViewIdx];
    const auto& buffer = asset.buffers[bufferView.bufferIndex];

    // Обработка вашего источника данных
    if (std::holds_alternative<fastgltf::sources::URI>(buffer.data)) {
        // Загрузка из URI
        return loadFromURI(buffer.data);
    }

    // Возврат span на данные
    return yourDataSpan;
};

fastgltf::iterateAccessor<glm::vec3>(asset, accessor,
    [&](glm::vec3 pos) { /* ... */ },
    customAdapter);
```

## Пример: Загрузка примитива

```cpp
void loadPrimitive(const fastgltf::Asset& asset,
                   const fastgltf::Primitive& primitive) {

    // Позиции вершин
    auto* posAttr = primitive.findAttribute("POSITION");
    if (posAttr) {
        const auto& accessor = asset.accessors[posAttr->accessorIndex];
        std::vector<glm::vec3> positions(accessor.count);
        fastgltf::copyFromAccessor<glm::vec3>(asset, accessor,
                                               positions.data());
        // Создание vertex buffer...
    }

    // Нормали
    auto* normAttr = primitive.findAttribute("NORMAL");
    if (normAttr) {
        const auto& accessor = asset.accessors[normAttr->accessorIndex];
        std::vector<glm::vec3> normals(accessor.count);
        fastgltf::copyFromAccessor<glm::vec3>(asset, accessor,
                                               normals.data());
    }

    // Индексы
    if (primitive.indicesAccessor.has_value()) {
        const auto& accessor = asset.accessors[*primitive.indicesAccessor];
        std::vector<uint32_t> indices(accessor.count);

        if (accessor.componentType == fastgltf::ComponentType::UnsignedInt) {
            fastgltf::copyFromAccessor<uint32_t>(asset, accessor,
                                                  indices.data());
        } else if (accessor.componentType ==
                   fastgltf::ComponentType::UnsignedShort) {
            std::vector<uint16_t> shortIndices(accessor.count);
            fastgltf::copyFromAccessor<uint16_t>(asset, accessor,
                                                  shortIndices.data());
            // Конвертация в uint32_t
            for (size_t i = 0; i < accessor.count; ++i) {
                indices[i] = shortIndices[i];
            }
        }
    }
}
```

## Важные замечания

### Проверка типа

Тип в шаблоне должен соответствовать `AccessorType` accessor:

```cpp
// Если accessor.type == AccessorType::Vec3
fastgltf::iterateAccessor<glm::vec3>(asset, accessor, ...);  // OK
fastgltf::iterateAccessor<glm::vec2>(asset, accessor, ...);  // Assert fail!
```

### Sparse accessors

Все функции автоматически обрабатывают sparse accessors:

```cpp
// Sparse данные применяются автоматически
std::vector<glm::vec3> vertices(accessor.count);
fastgltf::copyFromAccessor<glm::vec3>(asset, accessor, vertices.data());
// vertices содержит итоговые данные с применёнными sparse значениями
```

### Нормализация

Если `accessor.normalized == true`, значения автоматически нормализуются:

- Unsigned типы: [0, max] → [0, 1]
- Signed типы: [min, max] → [-1, 1]

---

## Производительность fastgltf

<!-- anchor: 06_performance -->

🟢 **Уровень 1: Начинающий**

Benchmarks и обоснование выбора fastgltf.

## Сравнение с альтернативами

### Тест 1: Embedded buffers (base64)

Модель: **2CylinderEngine** (1.7MB embedded buffer, закодирован в base64)

fastgltf включает оптимизированный base64-декодер с поддержкой AVX2, SSE4 и ARM Neon.

| Библиотека   | Время | Относительно fastgltf |
|--------------|-------|-----------------------|
| **fastgltf** | ~4ms  | 1x (базовая линия)    |
| tinygltf     | ~98ms | 24.5x медленнее       |
| cgltf        | ~30ms | 7.4x медленнее        |

### Тест 2: Большие JSON-файлы

Модель: **Amazon Bistro** (148k строк JSON, конвертирован в glTF 2.0)

Показывает чистую скорость десериализации.

| Библиотека   | Время | Относительно fastgltf |
|--------------|-------|-----------------------|
| **fastgltf** | ~10ms | 1x (базовая линия)    |
| tinygltf     | ~14ms | 1.4x медленнее        |
| cgltf        | ~50ms | 5x медленнее          |

### Выводы

- **Base64-декодирование**: fastgltf лидирует благодаря SIMD
- **JSON-парсинг**: fastgltf быстрее за счёт simdjson
- **Большие модели**: разница критична при загрузке сложных сцен

> **Примечание:** fastgltf следует принципу zero-cost abstractions — по умолчанию выполняется только парсинг JSON.
> См. [02_zero-cost-abstractions.md](../../philosophy/02_zero-cost-abstractions.md).

## Оптимизация загрузки

### Category

Выбор правильного Category ускоряет загрузку:

| Category         | Что загружается                             | Когда использовать              |
|------------------|---------------------------------------------|---------------------------------|
| `OnlyRenderable` | Всё, кроме Animations/Skins                 | Статические модели              |
| `OnlyAnimations` | Animations, Accessors, BufferViews, Buffers | Извлечение анимаций             |
| `All`            | Всё                                         | Анимированные модели, редакторы |

```cpp
// Для статических моделей — быстрее на 30-40%
auto asset = parser.loadGltf(data.get(), basePath, options,
                              fastgltf::Category::OnlyRenderable);
```

### Источники данных

| Класс            | Когда использовать     | Особенности                   |
|------------------|------------------------|-------------------------------|
| `GltfDataBuffer` | Стандартная загрузка   | Универсальный                 |
| `MappedGltfFile` | Большие файлы (>100MB) | Memory mapping, меньше памяти |
| `GltfFileStream` | Streaming              | Потоковое чтение              |

### Переиспользование Parser

```cpp
// Хорошо: переиспользуем parser
fastgltf::Parser parser;  // Создаём один раз

for (const auto& file : files) {
    auto data = fastgltf::GltfDataBuffer::FromPath(file);
    auto asset = parser.loadGltf(data.get(), ...);
    // ...
}

// Плохо: создаём parser для каждого файла
for (const auto& file : files) {
    fastgltf::Parser parser;  // Неэффективно!
    // ...
}
```

### Асинхронная загрузка

Parser не потокобезопасен, но можно использовать отдельные экземпляры:

```cpp
// Каждый поток — свой parser
thread_local fastgltf::Parser threadParser;

// Или пул парсеров
std::vector<std::unique_ptr<fastgltf::Parser>> parserPool;
```

## Профилирование

### Типичные временные затраты

| Операция                      | Время (примерно) | Оптимизации                   |
|-------------------------------|------------------|-------------------------------|
| Парсинг JSON (10MB)           | 5-15ms           | `Category::OnlyRenderable`    |
| Загрузка буферов (50MB)       | 20-50ms          | `LoadExternalBuffers` + async |
| Чтение accessor (100K вершин) | 1-3ms            | Правильный тип в шаблоне      |
| Обход сцены (1000 узлов)      | 0.5-1ms          | `iterateSceneNodes`           |

### Bottlenecks

1. **Base64-декодирование** — используйте GLB вместо embedded buffers
2. **Загрузка внешних файлов** — используйте async загрузку
3. **Конвертация типов** — используйте `copyFromAccessor` с правильным типом

## Рекомендации

### Для статических моделей

```cpp
fastgltf::Parser parser;
auto data = fastgltf::GltfDataBuffer::FromPath(path);
auto asset = parser.loadGltf(
    data.get(),
    path.parent_path(),
    fastgltf::Options::LoadExternalBuffers,
    fastgltf::Category::OnlyRenderable
);
```

### Для больших файлов

```cpp
// Memory mapping для больших GLB
auto data = fastgltf::MappedGltfFile::FromPath("large_model.glb");
```

### Для множества файлов

```cpp
// Переиспользуйте parser и используйте async
fastgltf::Parser parser;  // Один на поток

// Параллельная загрузка
std::vector<std::future<Asset>> futures;
for (const auto& file : files) {
    futures.push_back(std::async([&parser, file] {
        auto data = fastgltf::GltfDataBuffer::FromPath(file);
        return parser.loadGltf(data.get(), ...).get();
    }));
}

---

## Продвинутые темы fastgltf

<!-- anchor: 07_advanced -->

🔴 **Уровень 3: Продвинутый**

Sparse accessors, morph targets, анимации, скиннинг и GPU-driven загрузка.

## Sparse Accessors

### Концепция

Sparse accessors позволяют хранить только изменённые значения вместо полного дублирования буфера.

```

Базовый буфер (или нули)
↓
Инициализация
↓
Sparse indices (какие элементы заменяются)
↓
Sparse values (новые значения)
↓
Итоговый accessor

```

### Применение

- Partial updates вершинных данных
- Сжатие анимационных ключевых кадров
- Динамические изменения ландшафта

### Структура

```cpp
struct SparseAccessor {
    size_t count;                    // Количество изменённых элементов
    size_t indicesAccessor;          // Accessor с индексами
    size_t valuesAccessor;           // Accessor с новыми значениями
    ComponentType indicesComponentType;
    // ...
};
```

### Использование

Accessor tools автоматически обрабатывают sparse:

```cpp
// Автоматическая обработка
std::vector<glm::vec3> vertices(accessor.count);
fastgltf::copyFromAccessor<glm::vec3>(asset, accessor, vertices.data());
// vertices содержит итоговые данные с применёнными sparse значениями
```

### Ручная обработка

```cpp
if (accessor.sparse.has_value()) {
    const auto& sparse = *accessor.sparse;

    // 1. Базовые данные
    if (accessor.bufferViewIndex.has_value()) {
        fastgltf::copyFromAccessor<glm::vec3>(asset, accessor, output.data());
    } else {
        std::fill(output.begin(), output.end(), glm::vec3(0.0f));
    }

    // 2. Индексы
    std::vector<uint32_t> indices(sparse.count);
    fastgltf::copyFromAccessor<uint32_t>(
        asset, asset.accessors[sparse.indicesAccessor], indices.data());

    // 3. Новые значения
    std::vector<glm::vec3> values(sparse.count);
    fastgltf::copyFromAccessor<glm::vec3>(
        asset, asset.accessors[sparse.valuesAccessor], values.data());

    // 4. Применение
    for (size_t i = 0; i < sparse.count; ++i) {
        output[indices[i]] = values[i];
    }
}
```

---

## Morph Targets (Blend Shapes)

### Концепция

Morph targets позволяют деформировать меш путём интерполяции между базовой формой и целевыми формами.

```
vertex_final = vertex_base + Σ(weight[i] * target_offset[i])
```

### Структура в glTF

```
Mesh
├── primitives[]
│   └── Primitive
│       ├── attributes (POSITION, NORMAL, ...)
│       └── targets[] (MorphTarget)
│           ├── POSITION delta
│           ├── NORMAL delta
│           └── TANGENT delta
└── weights[] (веса по умолчанию)
```

### Извлечение morph targets

```cpp
std::vector<glm::vec3> extractMorphDeltas(
    const fastgltf::Asset& asset,
    const fastgltf::Primitive& primitive,
    size_t targetIndex) {

    std::vector<glm::vec3> deltas;

    if (targetIndex >= primitive.targets.size()) {
        return deltas;
    }

    const auto& target = primitive.targets[targetIndex];
    auto it = target.find("POSITION");

    if (it != target.end()) {
        const auto& accessor = asset.accessors[it->second];
        deltas.resize(accessor.count);
        fastgltf::copyFromAccessor<glm::vec3>(asset, accessor, deltas.data());
    }

    return deltas;
}
```

### Применение morph targets

```cpp
void applyMorphTargets(
    std::vector<glm::vec3>& positions,
    const std::vector<glm::vec3>& basePositions,
    const std::vector<std::vector<glm::vec3>>& targetDeltas,
    const std::vector<float>& weights) {

    positions = basePositions;

    for (size_t t = 0; t < targetDeltas.size() && t < weights.size(); ++t) {
        float weight = weights[t];
        const auto& deltas = targetDeltas[t];

        for (size_t v = 0; v < positions.size() && v < deltas.size(); ++v) {
            positions[v] += deltas[v] * weight;
        }
    }
}
```

### GLSL шейдер

```glsl
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;

// Morph target data
layout (binding = 0) buffer MorphPositions { vec3 morphPositions[]; };
layout (binding = 1) buffer MorphNormals { vec3 morphNormals[]; };

uniform float morphWeights[MAX_MORPH_TARGETS];
uniform int morphTargetCount;
uniform int vertexCount;

void main() {
    vec3 position = inPosition;
    vec3 normal = inNormal;

    for (int t = 0; t < morphTargetCount; ++t) {
        int offset = t * vertexCount + gl_VertexIndex;
        position += morphPositions[offset] * morphWeights[t];
        normal += morphNormals[offset] * morphWeights[t];
    }

    normal = normalize(normal);
    // ...
}
```

---

## Анимации

### Структура

```
Animation
├── channels[]
│   └── AnimationChannel
│       ├── target.node
│       ├── target.path (translation/rotation/scale/weights)
│       └── samplerIndex
└── samplers[]
    └── AnimationSampler
        ├── inputAccessor (время ключевых кадров)
        ├── outputAccessor (значения)
        └── interpolation (LINEAR/STEP/CUBICSPLINE)
```

### Интерполяция

| Тип           | Описание              | Использование                  |
|---------------|-----------------------|--------------------------------|
| `LINEAR`      | Линейная интерполяция | Большинство анимаций           |
| `STEP`        | Без интерполяции      | Visibility, discrete           |
| `CUBICSPLINE` | Кубический сплайн     | Camera paths, плавные движения |

### Обработка анимации

```cpp
void processAnimation(
    const fastgltf::Asset& asset,
    const fastgltf::Animation& animation,
    float time,
    std::map<size_t, glm::mat4>& nodeTransforms) {

    for (const auto& channel : animation.channels) {
        if (!channel.nodeIndex.has_value()) continue;

        const auto& sampler = animation.samplers[channel.samplerIndex];
        const auto& inputAccessor = asset.accessors[sampler.inputAccessor];
        const auto& outputAccessor = asset.accessors[sampler.outputAccessor];

        // Временные метки
        std::vector<float> keyframeTimes;
        fastgltf::iterateAccessor<float>(asset, inputAccessor,
            [&](float t) { keyframeTimes.push_back(t); });

        // Поиск текущего ключевого кадра
        size_t keyframeIndex = 0;
        for (size_t i = 0; i < keyframeTimes.size() - 1; ++i) {
            if (time >= keyframeTimes[i] && time < keyframeTimes[i + 1]) {
                keyframeIndex = i;
                break;
            }
        }

        float t0 = keyframeTimes[keyframeIndex];
        float t1 = keyframeTimes[keyframeIndex + 1];
        float alpha = (time - t0) / (t1 - t0);

        size_t nodeIndex = *channel.nodeIndex;

        switch (channel.path) {
            case fastgltf::AnimationPath::Translation: {
                std::vector<glm::vec3> values;
                fastgltf::iterateAccessor<glm::vec3>(asset, outputAccessor,
                    [&](glm::vec3 v) { values.push_back(v); });
                glm::vec3 pos = glm::mix(values[keyframeIndex],
                                         values[keyframeIndex + 1], alpha);
                nodeTransforms[nodeIndex] = glm::translate(
                    nodeTransforms[nodeIndex], pos);
                break;
            }
            case fastgltf::AnimationPath::Rotation: {
                std::vector<glm::quat> values;
                fastgltf::iterateAccessor<glm::quat>(asset, outputAccessor,
                    [&](glm::quat q) { values.push_back(q); });
                glm::quat rot = glm::slerp(values[keyframeIndex],
                                           values[keyframeIndex + 1], alpha);
                nodeTransforms[nodeIndex] *= glm::mat4_cast(rot);
                break;
            }
            case fastgltf::AnimationPath::Scale: {
                std::vector<glm::vec3> values;
                fastgltf::iterateAccessor<glm::vec3>(asset, outputAccessor,
                    [&](glm::vec3 v) { values.push_back(v); });
                glm::vec3 scale = glm::mix(values[keyframeIndex],
                                           values[keyframeIndex + 1], alpha);
                nodeTransforms[nodeIndex] = glm::scale(
                    nodeTransforms[nodeIndex], scale);
                break;
            }
            case fastgltf::AnimationPath::Weights: {
                // Morph target weights
                break;
            }
        }
    }
}
```

### CUBICSPLINE интерполяция

```cpp
glm::vec3 cubicSplineInterpolate(
    const std::vector<glm::vec3>& data,
    size_t keyframeIndex,
    float alpha,
    float deltaTime) {

    // [in-tangent, value, out-tangent] для каждого кадра
    size_t idx = keyframeIndex * 3;
    glm::vec3 inTangent = data[idx];
    glm::vec3 value = data[idx + 1];
    glm::vec3 outTangent = data[idx + 2];

    glm::vec3 nextValue = data[(keyframeIndex + 1) * 3 + 1];
    glm::vec3 nextInTangent = data[(keyframeIndex + 1) * 3];

    float t = alpha;
    float t2 = t * t;
    float t3 = t2 * t;

    return (2.0f * t3 - 3.0f * t2 + 1.0f) * value +
           (t3 - 2.0f * t2 + t) * outTangent * deltaTime +
           (-2.0f * t3 + 3.0f * t2) * nextValue +
           (t3 - t2) * nextInTangent * deltaTime;
}
```

---

## Скиннинг (Skeletal Animation)

### Структура

```
Skin
├── joints[] (индексы Node)
├── inverseBindMatrices (Accessor с матрицами)
└── skeleton (опционально, root node)

Primitive
├── JOINTS_0 (uvec4 — индексы костей)
└── WEIGHTS_0 (vec4 — веса)
```

### Извлечение данных скиннинга

```cpp
struct SkeletonData {
    std::vector<glm::mat4> inverseBindMatrices;
    std::vector<glm::mat4> jointMatrices;
    std::vector<size_t> jointNodeIndices;
};

SkeletonData extractSkinData(const fastgltf::Asset& asset,
                             const fastgltf::Skin& skin) {
    SkeletonData data;
    size_t jointCount = skin.joints.size();

    data.jointNodeIndices = skin.joints;
    data.jointMatrices.resize(jointCount, glm::mat4(1.0f));

    if (skin.inverseBindMatrices.has_value()) {
        const auto& accessor = asset.accessors[*skin.inverseBindMatrices];
        data.inverseBindMatrices.resize(jointCount);
        fastgltf::copyFromAccessor<glm::mat4>(
            asset, accessor, data.inverseBindMatrices.data());
    } else {
        data.inverseBindMatrices.resize(jointCount, glm::mat4(1.0f));
    }

    return data;
}
```

### Обновление joint matrices

```cpp
void updateJointMatrices(
    SkeletonData& skeleton,
    const std::vector<glm::mat4>& nodeGlobalTransforms) {

    for (size_t i = 0; i < skeleton.jointNodeIndices.size(); ++i) {
        size_t nodeIndex = skeleton.jointNodeIndices[i];
        skeleton.jointMatrices[i] =
            nodeGlobalTransforms[nodeIndex] * skeleton.inverseBindMatrices[i];
    }
}
```

### Веса вершин

```cpp
struct VertexSkinData {
    glm::uvec4 jointIndices;
    glm::vec4 weights;
};

std::vector<VertexSkinData> extractVertexSkinData(
    const fastgltf::Asset& asset,
    const fastgltf::Primitive& primitive) {

    auto jointsIt = primitive.findAttribute("JOINTS_0");
    auto weightsIt = primitive.findAttribute("WEIGHTS_0");

    if (jointsIt == primitive.attributes.end() ||
        weightsIt == primitive.attributes.end()) {
        return {};
    }

    size_t vertexCount = asset.accessors[jointsIt->accessorIndex].count;
    std::vector<VertexSkinData> skinData(vertexCount);

    // Извлечение индексов костей
    const auto& jointsAccessor = asset.accessors[jointsIt->accessorIndex];
    if (jointsAccessor.componentType == fastgltf::ComponentType::UnsignedByte) {
        std::vector<glm::u8vec4> temp(vertexCount);
        fastgltf::copyFromAccessor<glm::u8vec4>(asset, jointsAccessor, temp.data());
        for (size_t i = 0; i < vertexCount; ++i) {
            skinData[i].jointIndices = glm::uvec4(temp[i]);
        }
    } else if (jointsAccessor.componentType ==
               fastgltf::ComponentType::UnsignedShort) {
        std::vector<glm::u16vec4> temp(vertexCount);
        fastgltf::copyFromAccessor<glm::u16vec4>(asset, jointsAccessor, temp.data());
        for (size_t i = 0; i < vertexCount; ++i) {
            skinData[i].jointIndices = glm::uvec4(temp[i]);
        }
    }

    // Извлечение весов
    const auto& weightsAccessor = asset.accessors[weightsIt->accessorIndex];
    fastgltf::copyFromAccessor<glm::vec4>(asset, weightsAccessor,
        [&](glm::vec4 weight, size_t idx) {
            skinData[idx].weights = weight;
        });

    // Нормализация
    for (auto& sd : skinData) {
        float sum = sd.weights.x + sd.weights.y + sd.weights.z + sd.weights.w;
        if (sum > 0.0f) sd.weights /= sum;
    }

    return skinData;
}
```

### GLSL шейдер

```glsl
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in uvec4 inJoints;
layout (location = 3) in vec4 inWeights;

layout (binding = 0) readonly buffer JointMatrices {
    mat4 jointMatrices[];
};

void main() {
    mat4 skinMatrix =
        inWeights.x * jointMatrices[inJoints.x] +
        inWeights.y * jointMatrices[inJoints.y] +
        inWeights.z * jointMatrices[inJoints.z] +
        inWeights.w * jointMatrices[inJoints.w];

    vec3 position = (skinMatrix * vec4(inPosition, 1.0)).xyz;
    vec3 normal = normalize((skinMatrix * vec4(inNormal, 0.0)).xyz);

    gl_Position = projection * view * model * vec4(position, 1.0);
}
```

---

## GPU-Driven Загрузка

### Zero-copy через BufferAllocationCallback

```cpp
struct GpuContext {
    // Ваш GPU context (VMA, VkDevice, ...)
};

fastgltf::Parser createGpuParser(GpuContext& ctx) {
    fastgltf::Parser parser;
    parser.setUserPointer(&ctx);

    parser.setBufferAllocationCallback(
        [](void* userPointer, size_t bufferSize,
           fastgltf::BufferAllocateFlags flags) -> fastgltf::BufferInfo {

            auto* ctx = static_cast<GpuContext*>(userPointer);

            // Выделение GPU буфера с mapped памятью
            void* mappedPtr = allocateGpuBuffer(ctx, bufferSize);

            return fastgltf::BufferInfo{
                .mappedMemory = mappedPtr,
                .customId = /* ваш ID */
            };
        },
        nullptr,  // unmap
        nullptr   // deallocate
    );

    return parser;
}
```

### Преимущества

- Нет промежуточных CPU буферов
- Данные записываются сразу в GPU память
- Минимальное копирование

### Требования

- GPU буфер должен быть host-visible
- Или использовать staging buffer с последующим transfer

---

## Резюме

| Тема                 | Ключевые концепции                 | Инструменты fastgltf                  |
|----------------------|------------------------------------|---------------------------------------|
| **Sparse Accessors** | Partial updates, экономия памяти   | `Accessor::sparse`, accessor tools    |
| **Morph Targets**    | Blend shapes, интерполяция         | `Primitive::targets`, `Mesh::weights` |
| **Анимации**         | Keyframes, каналы, интерполяция    | `Animation`, `AnimationSampler`       |
| **Скиннинг**         | Кости, веса, inverse bind matrices | `Skin`, JOINTS_0, WEIGHTS_0           |
| **GPU-Driven**       | Zero-copy, callbacks               | `setBufferAllocationCallback`         |

---

## Решение проблем fastgltf

<!-- anchor: 08_troubleshooting -->

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

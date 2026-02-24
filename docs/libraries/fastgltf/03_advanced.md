## Accessor Tools

<!-- anchor: 05_tools -->

> **Для понимание:** Accessor tools — это как набор профессиональных кухонных ножей для шеф-повара. Каждый нож (функция)
> предназначен для конкретной задачи: один режет мясо (`copyFromAccessor`), другой нарезает овощи (`iterateAccessor`),
> третий чистит рыбу (`getAccessorElement`). Fastgltf даёт вам сырые ингредиенты (данные), а эти инструменты помогают
> приготовить из них блюдо (готовые массивы для рендеринга).

Утилиты для работы с accessor данными. Заголовок: `fastgltf/tools.hpp`.

## Обзор

Accessor tools упрощают чтение данных из accessors:

- Автоматически обрабатывают sparse accessors
- Конвертируют типы данных
- Поддерживают нормализацию

```cpp
#include <print>
#include <expected>
#include <span>
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

> **Для понимание:** Производительность fastgltf — это как спортивный автомобиль против городских малолитражек.
> SIMD-оптимизации — это турбонаддув, memory mapping — это облегчённый кузов, а правильные Category — это выбор
> правильной
> передачи на трассе. Вы не просто загружаете модель, вы участвуете в гонке за минимальное время загрузки.

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
```

---

## Продвинутые темы fastgltf

<!-- anchor: 07_advanced -->

> **Для понимания:** Продвинутые темы fastgltf — это как профессиональный инструментарий для хирурга. Каждый
> инструмент (функция) предназначен для сложных операций: sparse accessors — это микрохирургические инструменты для
> работы
> с повреждёнными тканями, morph targets — это пластическая хирургия для 3D моделей, анимации — это нейрохирургия для
> оживления скелетов, GPU-Driven загрузка — это трансплантология для прямой пересадки данных в GPU. Вы не просто
> загружаете модель, вы проводите сложную операцию по её оживлению в реальном времени.

Расширенные возможности и сложные сценарии использования fastgltf.

## Sparse Accessors

> **Для понимания:** Sparse accessors — это как патч для программного обеспечения. Представьте, что у вас есть полная
> модель (программа), но некоторые её части повреждены или отсутствуют (баги). Вместо перезаписи всей программы, вы
> накладываете патч (sparse данные) только на повреждённые участки. Это экономит память и ускоряет загрузку.

Sparse accessors позволяют эффективно хранить данные, где большинство значений одинаковы или нулевые.

### Структура Sparse Accessor

```cpp
struct SparseAccessor {
    size_t count;                     // Количество sparse значений
    size_t indicesBufferView;         // BufferView с индексами
    size_t valuesBufferView;          // BufferView со значениями
    ComponentType indicesComponentType; // Тип индексов
};
```

### Пример использования

```cpp
#include <print>
#include <expected>
#include <fastgltf/tools.hpp>

void processSparseAccessor(const fastgltf::Asset& asset,
                           const fastgltf::Accessor& accessor) {

    if (accessor.sparse.has_value()) {
        const auto& sparse = *accessor.sparse;

        std::println("Sparse accessor: {} values", sparse.count);

        // Чтение sparse индексов
        std::vector<uint32_t> sparseIndices(sparse.count);
        const auto& indicesView = asset.bufferViews[sparse.indicesBufferView];

        // Чтение sparse значений
        std::vector<glm::vec3> sparseValues(sparse.count);
        const auto& valuesView = asset.bufferViews[sparse.valuesBufferView];

        // Accessor tools автоматически применяют sparse данные
        std::vector<glm::vec3> finalData(accessor.count);
        fastgltf::copyFromAccessor<glm::vec3>(asset, accessor,
                                               finalData.data());
    }
}
```

### Оптимизации

- **Экономия памяти**: Хранение только изменённых значений
- **Быстрая загрузка**: Меньше данных для чтения
- **Автоматическая обработка**: Accessor tools применяют sparse данные автоматически

## Morph Targets

> **Для понимания:** Morph targets — это как пластическая хирургия для 3D моделей. У вас есть базовое лицо модели, а
> morph targets — это набор "операций": улыбка, моргание, нахмуривание. Смешивая эти операции в разных пропорциях, вы
> создаёте выражения лица. Каждый morph target — это дельта (изменение) относительно базовой геометрии.

Morph targets позволяют анимировать вершины меша для создания выражений лица, деформаций и других эффектов.

### Структура Morph Target

```cpp
// В Primitive:
std::vector<std::unordered_map<std::string, size_t>> targets;
// Каждый target — словарь атрибут->accessor
```

### Пример использования

```cpp
#include <print>
#include <span>
#include <fastgltf/tools.hpp>

struct MorphWeights {
    std::vector<float> weights;
};

void processMorphTargets(const fastgltf::Asset& asset,
                         const fastgltf::Primitive& primitive,
                         const MorphWeights& weights) {

    if (!primitive.targets.empty()) {
        std::println("Morph targets: {}", primitive.targets.size());

        // Базовые позиции вершин
        auto* posAttr = primitive.findAttribute("POSITION");
        if (!posAttr) return;

        const auto& baseAccessor = asset.accessors[posAttr->accessorIndex];
        std::vector<glm::vec3> basePositions(baseAccessor.count);
        fastgltf::copyFromAccessor<glm::vec3>(asset, baseAccessor,
                                               basePositions.data());

        // Применяем morph targets с весами
        std::vector<glm::vec3> finalPositions = basePositions;

        for (size_t i = 0; i < primitive.targets.size() && i < weights.weights.size(); ++i) {
            float weight = weights.weights[i];
            if (weight == 0.0f) continue;

            const auto& target = primitive.targets[i];
            auto it = target.find("POSITION");
            if (it == target.end()) continue;

            const auto& targetAccessor = asset.accessors[it->second];
            std::vector<glm::vec3> deltas(targetAccessor.count);
            fastgltf::copyFromAccessor<glm::vec3>(asset, targetAccessor,
                                                   deltas.data());

            // Смешиваем с весом
            for (size_t j = 0; j < finalPositions.size(); ++j) {
                finalPositions[j] += deltas[j] * weight;
            }
        }
    }
}
```

### Оптимизации

- **Интерполяция**: Плавное смешивание между targets
- **Частичное обновление**: Обновлять только изменённые вершины
- **GPU вычисления**: Выполнять смешивание в шейдерах

## Анимации

> **Для понимания:** Анимации в fastgltf — это как музыкальная партитура для 3D моделей. Каждый инструмент (узел) имеет
> свою партию (ключевые кадры), а дирижёр (анимационная система) синхронизирует их во времени. Sampler — это нотная
> запись, Channel — это связь инструмента с партией, Animation — это вся композиция.

Fastgltf загружает анимационные данные, но не выполняет интерполяцию — это задача движка.

### Структура анимации

```cpp
struct Animation {
    std::string name;
    std::vector<AnimationSampler> samplers;
    std::vector<AnimationChannel> channels;
};

struct AnimationSampler {
    size_t inputAccessor;   // Время (float)
    size_t outputAccessor;  // Значения (TRS)
    AnimationInterpolation interpolation;
};

struct AnimationChannel {
    size_t samplerIndex;
    size_t nodeIndex;
    AnimationPath path;     // translation, rotation, scale
};
```

### Пример использования

```cpp
#include <print>
#include <expected>
#include <fastgltf/tools.hpp>
#include <glm/gtc/quaternion.hpp>

struct AnimationState {
    float currentTime = 0.0f;
    std::vector<glm::vec3> translations;
    std::vector<glm::quat> rotations;
    std::vector<glm::vec3> scales;
};

void loadAnimation(const fastgltf::Asset& asset,
                   const fastgltf::Animation& animation,
                   AnimationState& state) {

    std::println("Animation: {} ({} channels)",
                 animation.name, animation.channels.size());

    // Загружаем время для каждого семплера
    std::vector<std::vector<float>> timeData;
    for (const auto& sampler : animation.samplers) {
        const auto& accessor = asset.accessors[sampler.inputAccessor];
        std::vector<float> times(accessor.count);
        fastgltf::copyFromAccessor<float>(asset, accessor, times.data());
        timeData.push_back(std::move(times));
    }

    // Инициализируем состояние
    state.translations.resize(asset.nodes.size(), glm::vec3(0.0f));
    state.rotations.resize(asset.nodes.size(), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    state.scales.resize(asset.nodes.size(), glm::vec3(1.0f));
}
```

### Интерполяция

- **LINEAR**: Линейная интерполяция для translation и scale
- **STEP**: Ступенчатая (без интерполяции)
- **CUBICSPLINE**: Кубическая сплайн-интерполяция (сложнее, но плавнее)

## Скиннинг

> **Для понимания:** Скиннинг — это как марионетка для 3D моделей. Кости (joints) — это ниточки, которые тянут вершины (
> skin). Inverse bind matrices — это начальная поза марионетки на полке, а skin matrices — это текущая поза в руках
> кукловода. Каждая вершина привязана к нескольким костям с разными весами (насколько сильно каждая кость её тянет).

Скиннинг позволяет деформировать меш с помощью иерархии костей.

### Структура скиннинга

```cpp
struct Skin {
    std::optional<size_t> inverseBindMatricesAccessor;
    std::vector<size_t> joints;      // Индексы узлов-костей
    std::optional<size_t> skeleton;  // Корневой узел
};
```

### Пример использования

```cpp
#include <print>
#include <expected>
#include <fastgltf/tools.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct SkinningData {
    std::vector<glm::mat4> inverseBindMatrices;
    std::vector<glm::mat4> jointMatrices;
    std::vector<glm::ivec4> jointIndices;
    std::vector<glm::vec4> jointWeights;
};

void loadSkin(const fastgltf::Asset& asset,
              const fastgltf::Skin& skin,
              SkinningData& data) {

    std::println("Skin with {} joints", skin.joints.size());

    // Загружаем inverse bind matrices
    if (skin.inverseBindMatricesAccessor.has_value()) {
        const auto& accessor = asset.accessors[*skin.inverseBindMatricesAccessor];
        data.inverseBindMatrices.resize(accessor.count);
        fastgltf::copyFromAccessor<glm::mat4>(asset, accessor,
                                               data.inverseBindMatrices.data());
    } else {
        // По умолчанию — identity matrices
        data.inverseBindMatrices.resize(skin.joints.size(), glm::mat4(1.0f));
    }

    // Инициализируем joint matrices
    data.jointMatrices.resize(skin.joints.size(), glm::mat4(1.0f));

    // Для примитива с skin нужно загрузить JOINTS_0 и WEIGHTS_0
}
```

### Вычисление skin matrices

```cpp
void updateSkinMatrices(const fastgltf::Asset& asset,
                        const fastgltf::Skin& skin,
                        const std::vector<glm::mat4>& globalTransforms,
                        SkinningData& data) {

    for (size_t i = 0; i < skin.joints.size(); ++i) {
        size_t nodeIndex = skin.joints[i];
        const glm::mat4& globalTransform = globalTransforms[nodeIndex];
        const glm::mat4& inverseBind = data.inverseBindMatrices[i];

        // Skin matrix = GlobalTransform × InverseBind
        data.jointMatrices[i] = globalTransform * inverseBind;
    }
}
```

## GPU-Driven Загрузка

> **Для понимания:** GPU-Driven загрузка — это как прямая доставка товаров на склад без промежуточных складов. Вместо
> того чтобы везти товары (данные) через центральный склад (CPU память), вы отправляете их прямо в региональный
> распределительный центр (GPU память). Memory mapping — это когда вы просто открываете дверь склада и берёте товары, не
> перетаскивая их.

Оптимизации для прямой загрузки данных в GPU память.

### Memory Mapping

```cpp
#include <print>
#include <expected>
#include <fastgltf/parser.hpp>

void loadWithMemoryMapping(const std::filesystem::path& path) {
    // Memory mapping для больших файлов
    auto mappedFile = fastgltf::MappedGltfFile::FromPath(path);
    if (!mappedFile) {
        std::println("Failed to memory map: {}",
                     fastgltf::getErrorName(mappedFile.error()));
        return;
    }

    fastgltf::Parser parser;
    auto asset = parser.loadGltf(*mappedFile, path.parent_path());

    if (!asset) {
        std::println("Failed to load: {}",
                     fastgltf::getErrorName(asset.error()));
        return;
    }

    std::println("Loaded {} meshes with memory mapping",
                 asset->meshes.size());
}
```

### Zero-Copy Загрузка в Vulkan

```cpp
#include <print>
#include <expected>
#include <fastgltf/parser.hpp>
#include <vulkan/vulkan.h>

struct VulkanBuffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
    void* mapped;
};

void loadDirectToVulkan(const fastgltf::Asset& asset,
                        VulkanBuffer& stagingBuffer) {

    // Callback для прямой записи в Vulkan buffer
    auto mapCallback = [](std::uint64_t bufferSize, void* userPointer)
        -> fastgltf::BufferInfo {

        VulkanBuffer* buffer = static_cast<VulkanBuffer*>(userPointer);

        // Map Vulkan memory
        vkMapMemory(device, buffer->memory, 0, bufferSize, 0, &buffer->mapped);

        return fastgltf::BufferInfo{
            .mappedMemory = buffer->mapped,
            .customId = reinterpret_cast<fastgltf::CustomBufferId>(buffer)
        };
    };

    auto unmapCallback = [](fastgltf::BufferInfo* bufferInfo, void* userPointer) {
        VulkanBuffer* buffer = reinterpret_cast<VulkanBuffer*>(bufferInfo->customId);
        vkUnmapMemory(device, buffer->memory);
    };

    // Настраиваем parser с callbacks
    fastgltf::Parser parser;
    parser.setBufferAllocationCallback(mapCallback, unmapCallback);
    parser.setUserPointer(&stagingBuffer);
}
```

### Оптимизации

- **Memory mapping**: Избегаем копирования в CPU память
- **Direct GPU upload**: Прямая запись в GPU буферы
- **Async загрузка**: Перекрываем загрузку с вычислениями

## Решение проблем

> **Для понимания:** Решение проблем fastgltf — это как диагностика автомобиля: нужно знать коды ошибок (Error enum) и
> как их исправить (рекомендации). У вас есть приборная панель (getErrorName), мануал (getErrorMessage) и набор
> инструментов (решения). Каждая ошибка — это конкретная неисправность, которую можно починить.

Распространённые проблемы и их решения.

### Ошибка: InvalidPath

```cpp
auto asset = parser.loadGltf(data.get(), "invalid/path");
if (!asset) {
    if (asset.error() == fastgltf::Error::InvalidPath) {
        std::println("Исправьте: Укажите правильный путь к директории glTF");
        std::println("Решение: Используйте std::filesystem::absolute()");
    }
}
```

### Ошибка: MissingExternalBuffer

```cpp
// Включите LoadExternalBuffers
auto asset = parser.loadGltf(data.get(), basePath,
                             fastgltf::Options::LoadExternalBuffers);
```

### Ошибка: InvalidGLB

```cpp
// Проверьте, что файл действительно GLB
auto type = fastgltf::determineGltfFileType(data);
if (type != fastgltf::GltfType::GLB) {
    std::println("Файл не является GLB контейнером");
}
```

### Ошибка: InvalidJson

```cpp
// Проверьте целостность JSON
if (asset.error() == fastgltf::Error::InvalidJson) {
    std::println("JSON повреждён или содержит синтаксические ошибки");
    std::println("Решение: Проверьте файл с помощью JSON валидатора");
}
```

### Ошибка: MissingExtensions

```cpp
// Включите необходимые extensions в parser
fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_basisu |
                        fastgltf::Extensions::EXT_meshopt_compression);
```

### Полный список ошибок

| Ошибка                  | Причина                    | Решение                                   |
|-------------------------|----------------------------|-------------------------------------------|
| `InvalidPath`           | Неверный путь к директории | Используйте `std::filesystem::absolute()` |
| `MissingExtensions`     | Требуются extensions       | Включите extensions в конструкторе Parser |
| `InvalidJson`           | Повреждённый JSON          | Проверьте файл валидатором                |
| `InvalidGLB`            | Не GLB файл                | Используйте `determineGltfFileType()`     |
| `MissingExternalBuffer` | Внешний буфер не найден    | Включите `Options::LoadExternalBuffers`   |
| `InvalidURI`            | Некорректный URI           | Проверьте пути к файлам                   |

### Отладка

```cpp
#include <print>
#include <expected>
#include <fastgltf/parser.hpp>

void debugLoad(const std::filesystem::path& path) {
    fastgltf::Parser parser;
    auto data = fastgltf::GltfDataBuffer::FromPath(path);

    if (!data) {
        std::println("Failed to load file: {}",
                     fastgltf::getErrorName(data.error()));
        return;
    }

    auto asset = parser.loadGltf(data.get(), path.parent_path());

    if (!asset) {
        auto error = asset.error();
        std::println("Failed to parse glTF: {}",
                     fastgltf::getErrorName(error));
        std::println("Message: {}",
                     fastgltf::getErrorMessage(error));

        // Дополнительная диагностика
        if (error == fastgltf::Error::InvalidGltf) {
            std::println("Проверьте структуру glTF файла");
        }
    } else {
        std::println("Success! Loaded {} meshes",
                     asset->meshes.size());
    }
}
```

## Заключение

Fastgltf предоставляет мощный набор инструментов для работы с glTF 2.0, оптимизированный для производительности и
современных C++ стандартов. Ключевые преимущества:

1. **Производительность**: SIMD-оптимизации, memory mapping, async загрузка
2. **Современный C++**: C++20/23/26, `std::expected`, `std::span`, `std::print`
3. **Гибкость**: Поддержка extensions, кастомные адаптеры, callbacks
4. **Data-Oriented Design**: Плоские массивы, SoA, эффективная работа с памятью

### Рекомендации для ProjectV

- Используйте `Category::OnlyRenderable` для статических моделей
- Применяйте memory mapping для файлов >100MB
- Реализуйте zero-copy загрузку через Vulkan Memory Allocator
- Преобразуйте иерархические данные в SoA массивы для ECS
- Используйте `std::expected` для обработки ошибок

### Дальнейшее изучение

- Исходный код: `external/fastgltf/include/fastgltf/`
- Примеры: `external/fastgltf/examples/`
- Документация: `docs/libraries/fastgltf/01_reference.md`
- Интеграция: `docs/libraries/fastgltf/02_integration.md`

Fastgltf — это не просто библиотека загрузки glTF, это фундамент для высокопроизводительного рендеринга в ProjectV.
Правильное использование его возможностей позволит достичь максимальной производительности в GPU-driven воксельном
движке.

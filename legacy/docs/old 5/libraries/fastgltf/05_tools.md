# Accessor Tools

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

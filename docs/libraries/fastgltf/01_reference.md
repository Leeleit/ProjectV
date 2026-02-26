# fastgltf: Высокопроизводительный парсер glTF 2.0 для C++26

**fastgltf** — это современная библиотека загрузки glTF 2.0 на C++26, оптимизированная для производительности через
SIMD-парсинг (simdjson) и минимальные аллокации памяти. Библиотека предоставляет типобезопасный API с поддержкой
`std::expected`, `std::span` и других современных C++ фич.

> **Для понимания:** Представьте fastgltf как высокоскоростной конвейер на заводе. На входе — сырьё (glTF файлы), на
> выходе — готовые детали (структурированные данные), которые можно сразу отправлять на сборку (рендеринг). В отличие от
> других библиотек, fastgltf использует SIMD-инструкции процессора, как если бы на конвейере работали не отдельные
> рабочие, а целые бригады, обрабатывающие данные параллельно.

**Стандарты:** C++26, glTF 2.0, SIMD-оптимизации

---

## Возможности

- **Полная поддержка glTF 2.0** — чтение и запись всех спецификаций
- **SIMD-оптимизации** — ускорение парсинга в 2-20 раз через simdjson
- **Минимальные зависимости** — только simdjson, без тяжелых библиотек
- **Типобезопасный API** — `std::optional<size_t>` вместо магических чисел
- **Accessor tools** — утилиты для работы с данными, включая sparse accessors
- **Современный C++** — поддержка C++20/23/26 (`std::expected`, `std::span`, `std::print`)
- **Модульность** — опциональная поддержка C++20 modules

## Сравнение с альтернативами

| Функция                   | cgltf    | tinygltf | fastgltf |
|---------------------------|----------|----------|----------|
| Чтение glTF 2.0           | Да       | Да       | Да       |
| Запись glTF 2.0           | Да       | Да       | Да       |
| Поддержка расширений      | Да       | Частично | Да       |
| SIMD-парсинг              | Нет      | Нет      | Да       |
| Memory callbacks          | Да       | Нет      | Частично |
| Accessor utilities        | Да       | Нет      | Да       |
| Sparse accessor utilities | Частично | Нет      | Да       |
| Типобезопасность          | Нет      | Нет      | Да       |

**Когда выбрать fastgltf:**

- Нужна максимальная скорость загрузки
- Требуется современный C++ API с `std::expected`
- Важна типобезопасность и отсутствие магических чисел
- Нужны accessor tools для работы с данными

**Когда выбрать альтернативы:**

- **tinygltf** — если нужна встроенная загрузка изображений
- **cgltf** — если нужен C API или header-only решение

## Архитектурная философия

fastgltf реализует принцип C++: "you don't pay for what you don't use". Архитектурно библиотека разделена на два уровня:

1. **Ядро парсинга** — минималистичный парсер JSON с SIMD-оптимизациями
2. **Опциональные модули** — загрузка внешних ресурсов, декомпозиция матриц, работа с расширениями

> **Для понимания:** Это как автомобиль с базовой комплектацией и опциями. Базовая версия (ядро) — это экономичный
> двигатель, который едет из точки А в точку Б. Опции (модули) — это кондиционер, навигация, кожаные сиденья. Вы платите
> только за то, что используете.

**Архитектурные принципы по умолчанию:**

- **Минимальный парсинг**: Только JSON метаданные без загрузки внешних ресурсов
- **Zero-copy для GLB**: Использование memory-mapped буферов через `ByteView`
- **Ленивая загрузка**: Внешние буферы остаются как URI до явного запроса

**Опциональные архитектурные расширения:**

- `LoadExternalBuffers` — стратегия загрузки внешних .bin файлов
- `LoadExternalImages` — стратегия загрузки внешних изображений
- `DecomposeNodeMatrices` — архитектурное преобразование матриц в TRS

## Типобезопасность

Сравнение подходов к индексации:

```cpp
// tinygltf: -1 означает "отсутствует" (магическое число)
if (node.mesh != -1) {
    auto& mesh = model.meshes[node.mesh];
}

// fastgltf: Optional с проверкой типа (типобезопасно)
if (node.meshIndex.has_value()) {
    auto& mesh = asset.meshes[*node.meshIndex];
}
```

> **Для понимания:** Представьте, что вы ищете книгу в библиотеке. Tinygltf даёт вам номер полки, но не проверяет,
> существует ли такая полка. Fastgltf даёт вам квитанцию: "Если книга есть, её номер 42". Вы сначала проверяете
> квитанцию,
> и только потом идёте к полке.

## Основные понятия glTF 2.0

**glTF** (GL Transmission Format) — открытый формат 3D-моделей от Khronos Group. Версия 2.0 описывает сцены, меши,
материалы, анимации и скиннинг.

> **Для понимания:** glTF — это как IKEA-инструкция для 3D-моделей. JSON — это список деталей и шагов сборки. Буферы (
> .bin) — это сами детали (деревянные панели, винты). Изображения — это наклейки и отделочные материалы.

Модель состоит из:

- **JSON** — метаданные: структура сцены, ссылки на буферы и изображения
- **Буферы** — сырые байты (вершины, индексы, анимационные ключи)
- **Изображения** — текстуры (PNG, JPEG, KTX2 и др.)

### JSON vs GLB

| Формат           | Описание                                                                                |
|------------------|-----------------------------------------------------------------------------------------|
| **JSON (.gltf)** | Файл ссылается на внешние .bin и изображения. Может содержать base64-встроенные данные. |
| **GLB (.glb)**   | Один бинарный файл: JSON-чанк + бинарные чанки. Первый буфер обычно встроен.            |

`Parser::loadGltf()` определяет формат автоматически.

## Архитектурная модель Asset

Результат парсинга — `fastgltf::Asset`, структура данных, представляющая декомпозицию glTF модели:

```
Asset (корневой контейнер)
├── accessors[]      — типизированные представления данных
├── buffers[]        — сырые байтовые потоки с источниками данных
├── bufferViews[]    — сегменты буферов с параметрами доступа
├── images[]         — графические ресурсы
├── materials[]      — PBR материалы
├── meshes[]         — геометрические примитивы
├── nodes[]          — иерархические узлы сцены
├── scenes[]         — контейнеры сцен
├── animations[]     — временные линии
├── skins[]          — скелетные системы
├── cameras[]        — камеры
├── textures[]       — текстурные ресурсы
├── samplers[]       — параметры сэмплирования
├── assetInfo        — метаданные версии
├── defaultScene     — точка входа по умолчанию
├── extensionsUsed   — расширения, используемые моделью
└── extensionsRequired — обязательные расширения
```

**Архитектурные принципы структуры Asset:**

- **Иммутабельность**: Asset представляет собой неизменяемую структуру после парсинга
- **Ссылочная целостность**: Все индексы гарантированно валидны
- **Ленивая загрузка**: Ресурсы загружаются только при явном запросе
- **Типобезопасность**: Все ссылки используют `std::optional<size_t>`

## Цепочка данных: Buffer → BufferView → Accessor

**Архитектурная метафора:** Представьте себе логистическую цепочку доставки товаров. Buffer — это огромный склад с
сырьём (байтами). BufferView — это конкретная партия товара, упакованная в контейнер (offset, length, stride).
Accessor — это декларация на таможне, описывающая что внутри контейнера (тип данных, количество). Primitive — это
конечный потребитель, который использует товар для производства.

Данные геометрии идут по цепочке:

```
Buffer (сырые байты) → Склад с сырьём
    ↓
BufferView (участок буфера: offset, length, stride) → Контейнер с партией товара
    ↓
Accessor (типизированное представление: Vec3 float, Scalar uint16) → Таможенная декларация
    ↓
Primitive (использование в меше) → Конечный потребитель
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

Типизированное представление данных:

```cpp
struct Accessor {
    size_t byteOffset;           // Смещение в BufferView
    size_t count;                // Количество элементов
    AccessorType type;           // Тип данных: Scalar, Vec2, Vec3, Vec4, Mat2, Mat3, Mat4
    ComponentType componentType; // Тип компонента: Byte, UnsignedShort, Float, ...
    bool normalized;             // Флаг нормализации в [0,1] или [-1,1]
    Optional<size_t> bufferViewIndex; // Ссылка на BufferView
    Optional<SparseAccessor> sparse;  // Поддержка sparse данных
    AccessorBoundsArray min, max;     // Границы для оптимизации
};
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
#include <expected>
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

> **Для понимания:** `std::expected` — это как заказ в ресторане. Вы заказываете стейк (ожидаемый результат), но иногда
> кухня говорит "стейка нет" (ошибка). Вы не получаете невалидный стейк, вы получаете чёткий ответ: либо стейк, либо
> причина, почему его нет.

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

### AlphaMode

```cpp
enum class AlphaMode : std::uint8_t {
    Opaque,      // Непрозрачный
    Mask,        // Маска (alphaCutoff)
    Blend        // Смешивание
};
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

## Архитектурное заключение

Fastgltf представляет собой оптимизированную реализацию glTF 2.0 с фокусом на производительность, типобезопасность и
современные C++ стандарты.

### Архитектурные особенности

1. **Производительность**: SIMD-оптимизации (simdjson), memory mapping, минимальные аллокации
2. **Типобезопасность**: `std::optional<size_t>`, `std::variant`, compile-time проверки
3. **Гибкость**: DataSource варианты, кастомные callbacks, модульные расширения
4. **Современный C++**: C++17 база с поддержкой C++20/23/26 фич

### Архитектурная модель данных

- **Asset**: Иммутабельный корневой контейнер с гарантированной ссылочной целостностью
- **DataSource**: Варианты источников данных (URI, BufferView, ByteView, Vector, CustomBuffer)
- **Accessor chain**: Иерархическая цепочка Buffer → BufferView → Accessor → Primitive

### Архитектура обработки ошибок

- **Expected<T>**: Современная архитектура обработки ошибок с семантикой `std::expected`
- **getErrorMessage()**: Человекочитаемые сообщения об ошибках
- **getErrorName()**: Машинно-читаемые коды ошибок для логирования

### Архитектурные направления дальней

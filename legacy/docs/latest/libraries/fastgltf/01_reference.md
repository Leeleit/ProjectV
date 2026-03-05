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

## Расширения и опции

### Поддерживаемые расширения

Fastgltf поддерживает широкий спектр расширений glTF 2.0 через битовые флаги `fastgltf::Extensions`:

| Расширение                        | Идентификатор                     | Описание                                        |
|-----------------------------------|-----------------------------------|-------------------------------------------------|
| `KHR_texture_transform`           | `KHR_texture_transform`           | Трансформация текстур (сдвиг, масштаб, поворот) |
| `KHR_texture_basisu`              | `KHR_texture_basisu`              | Сжатие текстур Basis Universal                  |
| `MSFT_texture_dds`                | `MSFT_texture_dds`                | Текстуры в формате DDS                          |
| `KHR_mesh_quantization`           | `KHR_mesh_quantization`           | Квантование геометрических данных               |
| `EXT_meshopt_compression`         | `EXT_meshopt_compression`         | Сжатие мешей через MeshOptimizer                |
| `KHR_lights_punctual`             | `KHR_lights_punctual`             | Точечные источники света                        |
| `EXT_texture_webp`                | `EXT_texture_webp`                | Текстуры WebP                                   |
| `KHR_materials_specular`          | `KHR_materials_specular`          | Спекулярные материалы                           |
| `KHR_materials_ior`               | `KHR_materials_ior`               | Индекс преломления материалов                   |
| `KHR_materials_iridescence`       | `KHR_materials_iridescence`       | Иридесценция материалов                         |
| `KHR_materials_volume`            | `KHR_materials_volume`            | Объёмные материалы                              |
| `KHR_materials_transmission`      | `KHR_materials_transmission`      | Прозрачные материалы                            |
| `KHR_materials_clearcoat`         | `KHR_materials_clearcoat`         | Прозрачное покрытие                             |
| `KHR_materials_emissive_strength` | `KHR_materials_emissive_strength` | Сила свечения материалов                        |
| `KHR_materials_sheen`             | `KHR_materials_sheen`             | Шёлковые материалы                              |
| `KHR_materials_unlit`             | `KHR_materials_unlit`             | Немодельные материалы (без освещения)           |
| `KHR_materials_anisotropy`        | `KHR_materials_anisotropy`        | Анизотропные материалы                          |
| `EXT_mesh_gpu_instancing`         | `EXT_mesh_gpu_instancing`         | GPU-инстансинг мешей                            |
| `KHR_draco_mesh_compression`      | `KHR_draco_mesh_compression`      | Сжатие мешей через Draco                        |
| `KHR_accessor_float64`            | `KHR_accessor_float64`            | 64-битные числа с плавающей точкой              |

### Опции загрузки

`fastgltf::Options` — битовые флаги, управляющие поведением парсера:

| Опция                         | Описание                                 |
|-------------------------------|------------------------------------------|
| `AllowDouble`                 | Разрешить тип компонента `double` (5130) |
| `DontRequireValidAssetMember` | Не требовать валидного поля `asset`      |
| `LoadExternalBuffers`         | Загружать внешние буферы в память        |
| `DecomposeNodeMatrices`       | Декомпозировать матрицы узлов в TRS      |
| `LoadExternalImages`          | Загружать внешние изображения в память   |
| `GenerateMeshIndices`         | Генерировать индексы для мешей без них   |

Пример использования опций:

```cpp
auto options = fastgltf::Options::LoadExternalBuffers
             | fastgltf::Options::DecomposeNodeMatrices
             | fastgltf::Options::GenerateMeshIndices;
```

### Категории загрузки

`fastgltf::Category` позволяет загружать только определённые части glTF:

| Категория        | Описание                                                   |
|------------------|------------------------------------------------------------|
| `All`            | Загружать всё                                              |
| `OnlyRenderable` | Только рендеринговая геометрия (меши, материалы, текстуры) |
| `OnlyAnimations` | Только анимации                                            |
| `OnlySkins`      | Только скиннинг                                            |
| `OnlyCameras`    | Только камеры                                              |
| `OnlyLights`     | Только источники света                                     |

## DataSource: архитектура источников данных

`DataSource` — это `std::variant`, представляющий различные источники данных для буферов и изображений. Эта архитектура
позволяет fastgltf работать с данными без лишних копий и поддерживать различные сценарии загрузки.

### Варианты DataSource

| Тип                     | Когда используется                    | Содержимое                | Преимущества                            |
|-------------------------|---------------------------------------|---------------------------|-----------------------------------------|
| `sources::ByteView`     | GLB контейнер, base64 без копирования | `span<const std::byte>`   | Zero-copy, memory-mapped доступ         |
| `sources::Array`        | Встроенные данные GLB                 | `StaticVector<std::byte>` | Быстрый доступ, минимальные аллокации   |
| `sources::Vector`       | Внешние буферы/изображения            | `std::vector<std::byte>`  | Простота использования, полный контроль |
| `sources::URI`          | Внешние файлы без загрузки            | Путь к файлу              | Ленивая загрузка, экономия памяти       |
| `sources::CustomBuffer` | Кастомные аллокаторы                  | `CustomBufferId`          | Интеграция с GPU буферами               |
| `sources::BufferView`   | Изображения в bufferView              | Индекс bufferView         | Эффективное использование памяти        |
| `sources::Fallback`     | EXT_meshopt_compression               | Данные недоступны         | Резервный источник                      |

### Пример работы с DataSource

```cpp
// Проверка типа источника данных
if (std::holds_alternative<fastgltf::sources::ByteView>(buffer.data)) {
    auto& byteView = std::get<fastgltf::sources::ByteView>(buffer.data);
    // Прямой доступ к memory-mapped данным
    process_data(byteView.bytes);
} else if (std::holds_alternative<fastgltf::sources::URI>(buffer.data)) {
    auto& uri = std::get<fastgltf::sources::URI>(buffer.data);
    // Ленивая загрузка по URI
    load_external_file(uri.uri.path());
}
```

## Архитектура обработки ошибок

Fastgltf использует современную систему обработки ошибок через `fastgltf::Expected<T>`, которая предоставляет
типобезопасную альтернативу исключениям и кодам ошибок.

### Коды ошибок

| Ошибка                       | Описание                                 |
|------------------------------|------------------------------------------|
| `None`                       | Успешное выполнение                      |
| `InvalidPath`                | Неверный путь к glTF директории          |
| `MissingExtensions`          | Требуемые расширения не включены         |
| `UnknownRequiredExtension`   | Неподдерживаемое обязательное расширение |
| `InvalidJson`                | Ошибка парсинга JSON                     |
| `InvalidGltf`                | Невалидные или отсутствующие данные glTF |
| `InvalidOrMissingAssetField` | Отсутствует или невалидно поле `asset`   |
| `InvalidGLB`                 | Невалидный GLB контейнер                 |
| `MissingExternalBuffer`      | Внешний буфер не найден                  |
| `UnsupportedVersion`         | Неподдерживаемая версия glTF             |
| `InvalidURI`                 | Ошибка парсинга URI                      |
| `InvalidFileData`            | Невалидные данные файла                  |
| `FailedWritingFiles`         | Ошибка записи файлов при экспорте        |
| `FileBufferAllocationFailed` | Ошибка аллокации буфера файла            |

### Работа с Expected

```cpp
auto asset = parser.loadGltf(data.get(), basePath);
if (asset.error() != fastgltf::Error::None) {
    // Получение человекочитаемого сообщения
    std::string_view message = fastgltf::getErrorMessage(asset.error());

    // Получение машинно-читаемого имени
    std::string_view name = fastgltf::getErrorName(asset.error());

    // Логирование ошибки
    std::println(stderr, "Ошибка загрузки: {} ({})", message, name);
    return;
}

// Безопасный доступ к данным
fastgltf::Asset& model = asset.get();
```

## Архитектурные паттерны fastgltf

### 1. **Strategy Pattern: DataSource варианты**

Различные стратегии загрузки данных (memory-mapped, вектор, URI, кастомные буферы) позволяют адаптироваться к различным
сценариям использования.

### 2. **Visitor Pattern: Обход иерархии glTF**

Типобезопасный обход структур данных через методы `findAttribute()`, доступ к компонентам через `copyFromAccessor()`.

### 3. **Factory Pattern: Создание Asset**

Унифицированный интерфейс `Parser` для создания `Asset` из различных источников данных.

### 4. **Builder Pattern: Экспорт glTF**

Постепенное построение glTF моделей через `Exporter` и `FileExporter`.

### 5. **Observer Pattern: Callbacks**

Кастомные коллбеки для аллокации буферов, декодирования base64 и парсинга extras.

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

### Архитектурные направления дальней разработки

Fastgltf продолжает развиваться с фокусом на следующие направления:

1. **C++26 полная поддержка**: Полная интеграция с новыми возможностями стандарта
2. **GPU-ускоренный парсинг**: Использование compute shaders для декомпрессии данных
3. **Streaming загрузка**: Поддержка progressive loading для больших моделей
4. **WebAssembly поддержка**: Оптимизация для веб-приложений через WASM
5. **Расширенная валидация**: Статический анализ glTF моделей на этапе компиляции

## Заключение

Fastgltf представляет собой современную, высокопроизводительную реализацию парсера glTF 2.0, построенную на принципах:

### Ключевые архитектурные преимущества

1. **Производительность**: SIMD-оптимизации через simdjson, memory-mapped буферы, минимальные аллокации
2. **Типобезопасность**: `std::optional<size_t>`, `std::expected`, compile-time проверки
3. **Гибкость**: Модульная архитектура с опциональными компонентами
4. **Современность**: Полная поддержка C++20/23/26 стандартов

### Архитектурные паттерны

- **Strategy Pattern**: Различные DataSource варианты для разных сценариев загрузки
- **Factory Pattern**: Создание Asset через унифицированный Parser интерфейс
- **Visitor Pattern**: Обход иерархии glTF через type-safe API
- **Builder Pattern**: Постепенное построение glTF моделей при экспорте

### Рекомендации по выбору

**Выбирайте fastgltf когда:**

- Нужна максимальная скорость загрузки glTF моделей
- Важна типобезопасность и современный C++ API
- Требуется минимальное потребление памяти
- Нужны accessor tools для работы с геометрическими данными

**Рассмотрите альтернативы когда:**

- Нужен C API (cgltf)
- Требуется встроенная загрузка изображений (tinygltf)
- Необходима максимальная совместимость со старыми компиляторами

Fastgltf демонстрирует, как современные возможности C++ могут быть использованы для создания высокопроизводительных,
типобезопасных библиотек для работы с 3D графикой, устанавливая новый стандарт для парсеров glTF в экосистеме C++.

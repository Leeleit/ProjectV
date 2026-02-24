# Draco

**Draco** — библиотека сжатия и распаковки 3D геометрических данных (mesh и point cloud) от Google. Спроектирована для
эффективного хранения и передачи 3D-графики с минимальной потерей визуального качества.

**Версия:** 1.5.7 (соответствует коммиту в `external/draco`)
**Исходники:** [google/draco](https://github.com/google/draco)
**Лицензия:** Apache 2.0

---

## Возможности

- **Сжатие mesh** — треугольные меши с connectivity data
- **Сжатие point cloud** — облака точек с произвольными атрибутами
- **Квантование** — контролируемая потеря точности для лучшего сжатия
- **Prediction schemes** — предсказание значений атрибутов по соседним элементам
- **glTF интеграция** — расширение `KHR_draco_mesh_compression`
- **Metadata** — пользовательские данные в сжатом файле
- **Анимации и скиннинг** — через опцию `DRACO_TRANSCODER_SUPPORTED`

## Архитектура сжатия

```
Исходные данные (Mesh / PointCloud)
         ↓
    Quantization (опционально)
         ↓
    Prediction Scheme
         ↓
    Entropy Encoding (rANS)
         ↓
    Draco bitstream (.drc)
```

> **Для понимания:** Представьте, что Draco — это высокотехнологичный архиватор для 3D‑моделей. Как ZIP сжимает
> текстовые файлы, удаляя избыточность, так и Draco анализирует геометрию, предсказывает значения атрибутов и кодирует
> только разницу, экономя до 90% места.

## Compression ratio

| Тип данных            | Без сжатия | Draco (default) | Сжатие |
|-----------------------|------------|-----------------|--------|
| Mesh 100K triangles   | 12.5 MB    | 0.8 MB          | 15×    |
| Point cloud 1M points | 28 MB      | 2.1 MB          | 13×    |
| Skinned mesh          | 18 MB      | 1.4 MB          | 12×    |

> **Примечание:** Результаты зависят от настроек квантования и типа геометрии.

## Сравнение с альтернативами

| Функция                 | Draco    | Open3D  | MeshOptimizer |
|-------------------------|----------|---------|---------------|
| Mesh compression        | Да       | Да      | Да            |
| Point cloud compression | Да       | Да      | Нет           |
| Lossless                | Частично | Да      | Нет           |
| glTF extension          | Да       | Нет     | Да            |
| GPU decoding            | Нет      | Нет     | Да            |
| C++ API                 | Да       | Да      | Да            |
| Decode speed            | Средняя  | Быстрая | Быстрая       |

**Когда выбрать Draco:**

- Нужна интеграция с glTF через `KHR_draco_mesh_compression`
- Требуется сжатие point cloud
- Важен максимальный compression ratio
- Данные передаются по сети

**Когда выбрать альтернативы:**

- **MeshOptimizer** — если нужен GPU decoding или lossless
- **Open3D** — если нужен полный pipeline обработки 3D данных

## Компоненты библиотеки

| Компонент              | Назначение                              |
|------------------------|-----------------------------------------|
| `draco::Decoder`       | Декодирование .drc в Mesh/PointCloud    |
| `draco::Encoder`       | Кодирование с базовыми настройками      |
| `draco::ExpertEncoder` | Детальный контроль над каждым атрибутом |
| `draco::Mesh`          | Треугольный меш с атрибутами            |
| `draco::PointCloud`    | Облако точек с атрибутами               |
| `draco_transcoder`     | CLI инструмент для glTF                 |

## Методы кодирования

### Mesh

| Метод       | Описание           | Compression | Decode speed |
|-------------|--------------------|-------------|--------------|
| Edgebreaker | Connectivity-first | Лучший      | Средняя      |
| Sequential  | Simple traversal   | Хороший     | Быстрая      |

### Point Cloud

| Метод      | Описание             | Compression | Decode speed |
|------------|----------------------|-------------|--------------|
| KD-Tree    | Spatial partitioning | Лучший      | Медленная    |
| Sequential | Linear traversal     | Хороший     | Быстрая      |

## Prediction schemes

| Scheme              | Применение     | Описание                                   |
|---------------------|----------------|--------------------------------------------|
| Parallelogram       | Mesh positions | Предсказание по смежным треугольникам      |
| Multi-parallelogram | Mesh positions | Улучшенный parallelogram                   |
| Geometric normal    | Normals        | Предсказание нормалей по геометрии         |
| Delta               | Generic        | Простая дельта-кодировка                   |
| Tex coords portable | UV             | Специализированно для текстурных координат |

> **Для понимания:** Prediction schemes работают как предсказание следующего слова в предложении. Если вы видите «кошка
> сидит на …», вы ожидаете «окне» или «коврике». Draco анализирует соседние вершины и предсказывает значение атрибута,
> кодируя только разницу между предсказанием и реальным значением. Это резко снижает энтропию и улучшает сжатие.

## Квантование

Draco использует квантование для уменьшения точности float значений:

```
Original: 1.23456789, 2.34567890, 3.45678901
Quantized (11 bit):  1.234,       2.346,      3.457
```

### Квантование позиций

```cpp
#include <draco/compression/encode.h>
#include <print>

draco::Encoder encoder;
encoder.SetAttributeQuantization(
    draco::GeometryAttribute::POSITION,
    14  // 14 бит на компонент
);

// Явное задание bounding box
std::array<float, 3> origin = {0.0f, 0.0f, 0.0f};
float range = 100.0f;  // Диапазон [origin, origin + range]

encoder.SetAttributeExplicitQuantization(
    draco::GeometryAttribute::POSITION,
    14,     // quantization_bits
    3,      // num_dims
    origin.data(),
    range
);
```

### Влияние на размер

| Бит на компонент | Точность | Размер данных |
|------------------|----------|---------------|
| 8                | ~0.4%    | Минимальный   |
| 11               | ~0.05%   | Базовый       |
| 14               | ~0.006%  | Высокий       |
| 16               | ~0.0015% | Максимальный  |

> **Для понимания:** Квантование похоже на уменьшение битрейта аудиофайла. Вы теряете некоторые высокочастотные детали (
> незаметные для глаза), но получаете гораздо меньший размер. 11 бит на компонент достаточно для большинства визуальных
> применений — как MP3 192 kbps для музыки.

### Dequantization при декодировании

```cpp
#include <draco/compression/decode.h>
#include <expected>
#include <print>

std::expected<std::unique_ptr<draco::Mesh>, draco::Status> decode_draco(
    std::span<const std::byte> compressed_data)
{
    draco::DecoderBuffer buffer;
    buffer.Init(reinterpret_cast<const char*>(compressed_data.data()),
                compressed_data.size());

    draco::Decoder decoder;
    // По умолчанию: автоматическая деквантование
    auto result = decoder.DecodeMeshFromBuffer(&buffer);
    if (!result.ok()) {
        return std::unexpected(result.status());
    }
    return std::move(result).value();
}
```

## Требования

- **C++26** (рекомендуется для `std::print`, `std::expected`, `std::span`)
- Совместимость с C++17 для `DRACO_TRANSCODER_SUPPORTED`
- Платформы: Windows, Linux, macOS, Android, iOS
- Зависимости: только стандартная библиотека (опционально `libpng`, `libjpeg` для transcoder)

> **Для понимания:** Draco — это библиотека, которая эволюционировала вместе с C++. Как автомобиль получает новые
> функции с каждой моделью года, так и Draco использует современные возможности C++26 для более безопасного и
> выразительного кода. `std::expected` заменяет исключения для обработки ошибок, `std::span` обеспечивает безопасную
> работу с массивами, а `std::print` делает вывод более читаемым.

## Оригинальная документация

- [Draco README](https://github.com/google/draco/blob/main/README.md)
- [BUILDING.md](https://github.com/google/draco/blob/main/BUILDING.md)
- [glTF EXT_mesh_draco](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_draco_mesh_compression)

---

## Основные понятия

Ключевые концепции Draco: геометрия, атрибуты, квантование.

### Геометрия

#### PointCloud

Базовый класс для хранения облака точек с атрибутами:

```cpp
#include <draco/point_cloud/point_cloud.h>
#include <span>

draco::PointCloud pc;

// Количество точек
pc.set_num_points(1000);

// Добавление атрибутов
draco::GeometryAttribute positionAttribute;
positionAttribute.Init(draco::GeometryAttribute::POSITION,
                       nullptr, 3, draco::DT_FLOAT32, false,
                       sizeof(float) * 3, 0);
int posAttrId = pc.AddAttribute(positionAttribute, true, 1000);

// Доступ к атрибутам
const draco::PointAttribute* attr = pc.GetNamedAttribute(
    draco::GeometryAttribute::POSITION
);
```

#### Mesh

Расширение PointCloud с connectivity data (грани):

```cpp
#include <draco/mesh/mesh.h>

draco::Mesh mesh;

// Количество граней
mesh.SetNumFaces(500);

// Установка грани
draco::Mesh::Face face;
face[0] = draco::PointIndex(0);
face[1] = draco::PointIndex(1);
face[2] = draco::PointIndex(2);
mesh.SetFace(draco::FaceIndex(0), face);

// Доступ к грани
const draco::Mesh::Face& f = mesh.face(draco::FaceIndex(0));
```

#### Отношения между классами

```
draco::PointCloud
├── num_points()
├── attributes[]
└── metadata

draco::Mesh : public PointCloud
├── num_faces()
├── faces[]
└── corner_table (топология)
```

### Атрибуты

#### GeometryAttribute

Базовое описание атрибута (тип, формат данных):

```cpp
#include <draco/attributes/geometry_attribute.h>

draco::GeometryAttribute posAttr;
posAttr.Init(
    draco::GeometryAttribute::POSITION,  // Тип
    nullptr,                              // DataBuffer (позже)
    3,                                    // num_components (x, y, z)
    draco::DT_FLOAT32,                    // data_type
    false,                                // normalized
    sizeof(float) * 3,                    // byte_stride
    0                                     // byte_offset
);
```

#### Типы атрибутов

```cpp
enum Type {
    POSITION = 0,   // Позиции вершин
    NORMAL,         // Нормали
    COLOR,          // Цвета
    TEX_COORD,      // Текстурные координаты
    GENERIC,        // Пользовательские данные
#ifdef DRACO_TRANSCODER_SUPPORTED
    TANGENT,        // Тангенты
    MATERIAL,       // Material indices
    JOINTS,         // Bone indices (skinning)
    WEIGHTS,        // Bone weights (skinning)
#endif
};
```

#### Типы данных

```cpp
enum DataType {
    DT_INT8,
    DT_UINT8,
    DT_INT16,
    DT_UINT16,
    DT_INT32,
    DT_UINT32,
    DT_INT64,
    DT_UINT64,
    DT_FLOAT32,
    DT_FLOAT64,
    DT_BOOL
};
```

#### PointAttribute

Расширяет GeometryAttribute с mapping данных:

```cpp
#include <draco/attributes/point_attribute.h>

// Создание через PointCloud
int attrId = mesh.AddAttribute(
    geomAttr,           // GeometryAttribute
    true,               // identity_mapping (1:1)
    num_values          // количество значений
);

// Доступ
draco::PointAttribute* attr = mesh.attribute(attrId);

// Установка значения
std::array<float, 3> pos = {1.0f, 2.0f, 3.0f};
attr->SetAttributeValue(draco::AttributeValueIndex(0), pos.data());

// Чтение значения
std::array<float, 3> outPos;
attr->GetValue(draco::AttributeValueIndex(0), outPos.data());
```

#### Mapping точек к значениям

Каждая точка может ссылаться на значение атрибута:

```cpp
// Identity mapping: point i → value i
// Используется при AddAttribute(..., true, ...)

// Explicit mapping: точка → произвольное значение
// Используется при AddAttribute(..., false, ...)
for (draco::PointIndex i(0); i < mesh.num_points(); ++i) {
    attr->SetPointMapEntry(i, draco::AttributeValueIndex(i.value() % 3));
}
```

### Индексы

Draco использует типизированные индексы для безопасности:

```cpp
// Типы индексов
draco::PointIndex           // Индекс точки
draco::FaceIndex            // Индекс грани
draco::CornerIndex          // Индекс угла (3 угла на грань)
draco::AttributeValueIndex  // Индекс значения атрибута

// Использование
draco::PointIndex pt(42);
uint32_t value = pt.value();  // Получение числового значения

// Сравнение
if (pt == draco::PointIndex(42)) { /* ... */ }

// Инкремент
for (draco::PointIndex i(0); i < mesh.num_points(); ++i) { /* ... */ }
```

#### Corner Table

Топология меша для навигации по смежным элементам:

```cpp
#include <draco/mesh/corner_table.h>

// Corner = вершина в конкретной грани
// Каждая грань имеет 3 corner: 0, 1, 2

// Получение corner для грани
draco::CornerIndex c0(3 * faceIndex.value() + 0);  // Первый corner
draco::CornerIndex c1(3 * faceIndex.value() + 1);  // Второй corner
draco::CornerIndex c2(3 * faceIndex.value() + 2);  // Третий corner

// Навигация (через CornerTable)
draco::CornerTable ct(mesh);
draco::CornerIndex next = ct.Next(c0);   // Следующий corner в грани
draco::CornerIndex prev = ct.Previous(c0); // Предыдущий corner в грани
draco::CornerIndex opp = ct.Opposite(c0);  // Противоположный corner
```

### BoundingBox

Вычисление границ геометрии:

```cpp
#include <draco/core/bounding_box.h>

draco::BoundingBox bbox = mesh.ComputeBoundingBox();

draco::Vector3f min = bbox.GetMinPoint();
draco::Vector3f max = bbox.GetMaxPoint();

// Размер
draco::Vector3f size = bbox.Size();
```

### Deduplication

Устранение дубликатов:

```cpp
#ifdef DRACO_ATTRIBUTE_INDICES_DEDUPLICATION_SUPPORTED
// Дедупликация точек с одинаковыми атрибутами
mesh.DeduplicatePointIds();
#endif

#ifdef DRACO_ATTRIBUTE_VALUES_DEDUPLICATION_SUPPORTED
// Дедупликация одинаковых значений атрибутов
mesh.DeduplicateAttributeValues();
#endif
```

### Metadata

Пользовательские данные в сжатом файле:

```cpp
#include <draco/metadata/geometry_metadata.h>
#include <memory>

// Добавление metadata в PointCloud
auto metadata = std::make_unique<draco::GeometryMetadata>();
metadata->AddEntryString("name", "MyModel");
metadata->AddEntryInt("version", 1);

pc.AddMetadata(std::move(metadata));

// Metadata для атрибута
auto attrMetadata = std::make_unique<draco::AttributeMetadata>(posAttrId);
attrMetadata->AddEntryString("semantic", "vertex_position");
pc.AddAttributeMetadata(posAttrId, std::move(attrMetadata));

// Чтение metadata
const draco::GeometryMetadata* meta = pc.GetMetadata();
if (meta) {
    std::string name;
    if (meta->GetEntryString("name", &name)) {
        std::println("Model name: {}", name);
    }
}
```

## Формат файла

### Заголовок Draco

```
Offset  Size  Field
------  ----  -----
0       5     "DRACO"
5       1     version_major
6       1     version_minor
7       1     encoder_type (mesh/point cloud)
8       1     encoder_method
9       2     flags
```

### Проверка типа геометрии

```cpp
#include <draco/compression/decode.h>
#include <expected>
#include <print>

std::expected<draco::EncodedGeometryType, draco::Status> get_geometry_type(
    std::span<const std::byte> data)
{
    draco::DecoderBuffer buffer;
    buffer.Init(reinterpret_cast<const char*>(data.data()), data.size());
    return draco::Decoder::GetEncodedGeometryType(&buffer);
}

int main() {
    std::vector<std::byte> data = load_file("model.drc");
    auto type = get_geometry_type(data);
    if (!type) {
        std::println(stderr, "Ошибка: {}", type.error().error_msg());
        return 1;
    }

    switch (*type) {
        case draco::TRIANGULAR_MESH:
            std::println("Mesh");
            break;
        case draco::POINT_CLOUD:
            std::println("Point cloud");
            break;
        case draco::INVALID_GEOMETRY_TYPE:
            std::println("Invalid");
            break;
    }
    return 0;
}
```

---

## Методы кодирования

Описание методов сжатия для mesh и point cloud.

### Обзор

Draco использует разные стратегии для mesh и point cloud:

```
Mesh          Point Cloud
─────         ───────────
Edgebreaker   KD-Tree
Sequential    Sequential
```

### Mesh Encoding

#### Edgebreaker

Оптимальный метод для сжатия connectivity data.

```cpp
#include <draco/compression/encode.h>
#include <print>

draco::Encoder encoder;
encoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);
```

**Принцип работы:**

1. Обход mesh по связности (connectivity traversal)
2. Кодирование topology через клеймы (clers: Create, Left, End, Right, Split)
3. Параллелограммное предсказание позиций
4. Entropy coding для финального сжатия

**Преимущества:**

- Лучший compression ratio для connectivity
- Эффективен для closed meshes
- Поддерживает произвольную топологию

**Недостатки:**

- Медленное декодирование
- Сложность реализации

> **Для понимания:** Edgebreaker напоминает разматывание клубка ниток. Алгоритм «ходит» по поверхности меша, записывая,
> куда он поворачивает (Create, Left, End, Right, Split). Эта последовательность поворотов компактно описывает всю
> топологию.

#### Sequential

Простой линейный обход mesh.

```cpp
draco::Encoder encoder;
encoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
encoder.SetSpeedOptions(10, 10);  // Максимальная скорость
```

**Принцип работы:**

1. Последовательное кодирование вершин
2. Последовательное кодирование граней
3. Delta coding для атрибутов

**Преимущества:**

- Быстрое кодирование и декодирование
- Простая реализация
- Предсказуемое время работы

**Недостатки:**

- Худший compression ratio
- Не использует топологию

#### Сравнение методов для Mesh

| Характеристика | Edgebreaker | Sequential |
|----------------|-------------|------------|
| Compression    | Лучший      | Хороший    |
| Decode speed   | Средняя     | Быстрая    |
| Encode speed   | Медленная   | Быстрая    |
| Сложность      | Высокая     | Низкая     |

#### Выбор метода

```cpp
// Максимальное сжатие
encoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);
encoder.SetSpeedOptions(0, 0);

// Баланс
encoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);
encoder.SetSpeedOptions(5, 5);

// Максимальная скорость
encoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
encoder.SetSpeedOptions(10, 10);
```

### Point Cloud Encoding

#### KD-Tree Encoding

Оптимальный метод для point cloud.

```cpp
draco::Encoder encoder;
encoder.SetEncodingMethod(draco::POINT_CLOUD_KD_TREE_ENCODING);
encoder.SetSpeedOptions(0, 0);  // Лучший compression
```

**Принцип работы:**

1. Построение KD-tree по позициям точек
2. Рекурсивное разбиение пространства
3. Кодирование позиций через tree traversal
4. Кодирование атрибутов с учётом spatial locality

**Преимущества:**

- Лучший compression ratio
- Использует spatial coherence
- Эффективен для плотных облаков

**Недостатки:**

- Медленное кодирование
- Требует квантования позиций

#### Sequential Encoding

Простой линейный обход.

```cpp
draco::Encoder encoder;
encoder.SetEncodingMethod(draco::POINT_CLOUD_SEQUENTIAL_ENCODING);
encoder.SetSpeedOptions(10, 10);
```

**Преимущества:**

- Быстрое кодирование
- Быстрое декодирование
- Не требует квантования

**Недостатки:**

- Худший compression ratio
- Не использует spatial coherence

#### Сравнение методов для Point Cloud

| Характеристика       | KD-Tree   | Sequential |
|----------------------|-----------|------------|
| Compression          | Лучший    | Хороший    |
| Decode speed         | Медленная | Быстрая    |
| Encode speed         | Медленная | Быстрая    |
| Требует quantization | Да        | Нет        |

### Speed Options

Параметр `SetSpeedOptions(encoding_speed, decoding_speed)` контролирует trade-off:

```cpp
// 0 = максимальное сжатие, минимальная скорость
// 10 = минимальное сжатие, максимальная скорость

encoder.SetSpeedOptions(0, 0);   // Максимальный compression
encoder.SetSpeedOptions(5, 5);   // Баланс
encoder.SetSpeedOptions(10, 10); // Максимальная скорость
encoder.SetSpeedOptions(0, 10);  // Медленное кодирование, быстрое декодирование
```

#### Влияние на алгоритмы

| Speed | Prediction schemes | Entropy coding |
|-------|--------------------|----------------|
| 0     | Все доступные      | rANS adaptive  |
| 5     | Основные           | rANS           |
| 10    | Delta only         | Direct         |

### Внутренняя структура encoding

#### Pipeline для Mesh (Edgebreaker)

```
Mesh
  ↓
Connectivity Encoding (Edgebreaker)
  ├── Traversal (depth-first)
  ├── Clers encoding
  └── Hole sealing
  ↓
Attribute Encoding
  ├── Prediction (parallelogram)
  ├── Transform (quantization)
  └── Entropy coding (rANS)
  ↓
Bitstream
```

#### Pipeline для Point Cloud (KD-Tree)

```
PointCloud
  ↓
Position Encoding
  ├── Quantization
  ├── KD-Tree construction
  └── Tree encoding
  ↓
Attribute Encoding
  ├── Prediction (spatial)
  ├── Transform
  └── Entropy coding
  ↓
Bitstream
```

### Traversal методы

Для mesh encoding доступны разные traversal стратегии:

```cpp
// Depth-first traversal (default)
// MESH_TRAVERSAL_DEPTH_FIRST

// Prediction degree traversal
// MESH_TRAVERSAL_PREDICTION_DEGREE
```

Traversal влияет на порядок обработки вершин и качество предсказания.

### Entropy Coding

Draco использует rANS (range Asymmetric Numeral Systems):

```cpp
// Автоматически выбирается на основе speed options
// Низкий speed = rANS adaptive
// Высокий speed = direct bit coding
```

#### Типы entropy coders

| Coder                    | Использование         |
|--------------------------|-----------------------|
| `RAnsBitEncoder`         | Базовый rANS          |
| `AdaptiveRAnsBitEncoder` | Адаптивный rANS       |
| `DirectBitEncoder`       | Без сжатия (speed=10) |
| `SymbolBitEncoder`       | Для symbol data       |

### Connectivity Encoding Variants

Для Edgebreaker доступны варианты:

```cpp
enum MeshEdgebreakerConnectivityEncodingMethod {
    MESH_EDGEBREAKER_STANDARD_ENCODING = 0,
    MESH_EDGEBREAKER_PREDICTIVE_ENCODING = 1,  // Deprecated
    MESH_EDGEBREAKER_VALENCE_ENCODING = 2,     // Лучший compression
};
```

#### Valence Encoding

Использует valence (количество соседних граней) для лучшего сжатия:

```cpp
// Valence encoding включается автоматически при низком speed
encoder.SetSpeedOptions(0, 0);  // Valence encoding активен
```

### Примеры конфигураций

#### Максимальное сжатие (offline)

```cpp
draco::Encoder encoder;
encoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);
encoder.SetSpeedOptions(0, 0);
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 11);
encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, 8);
encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, 10);
```

#### Быстрое кодирование (real-time)

```cpp
draco::Encoder encoder;
encoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
encoder.SetSpeedOptions(10, 10);
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 14);
```

#### Быстрое декодирование (runtime)

```cpp
draco::Encoder encoder;
encoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);
encoder.SetSpeedOptions(5, 10);  // Медленное кодирование, быстрое декодирование
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 12);
```

#### Streaming (point cloud)

```cpp
draco::Encoder encoder;
encoder.SetEncodingMethod(draco::POINT_CLOUD_SEQUENTIAL_ENCODING);
encoder.SetSpeedOptions(10, 10);
// Без квантования для сохранения точности
```

#### Dense point cloud (LiDAR)

```cpp
draco::Encoder encoder;
encoder.SetEncodingMethod(draco::POINT_CLOUD_KD_TREE_ENCODING);
encoder.SetSpeedOptions(0, 0);
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 16);
encoder.SetAttributeQuantization(draco::GeometryAttribute::COLOR, 8);
```

### Compression ratios

Ориентировочные значения для типичных моделей:

#### Mesh

| Модель               | Original | Edgebreaker | Sequential |
|----------------------|----------|-------------|------------|
| Character (50K tris) | 2.8 MB   | 180 KB      | 280 KB     |
| Building (200K tris) | 12 MB    | 600 KB      | 1.1 MB     |
| Terrain (1M tris)    | 58 MB    | 2.5 MB      | 4.8 MB     |

#### Point Cloud

| Модель                   | Original | KD-Tree | Sequential |
|--------------------------|----------|---------|------------|
| LiDAR scan (1M pts)      | 28 MB    | 2.1 MB  | 4.2 MB     |
| Photogrammetry (10M pts) | 280 MB   | 18 MB   | 42 MB      |

> **Примечание:** Результаты сильно зависят от квантования и типа данных.

---

## Prediction Schemes

Методы предсказания значений атрибутов для улучшения сжатия.

### Принцип работы

Prediction schemes предсказывают значение атрибута на основе соседних элементов. Вместо хранения абсолютных значений
кодируется разница (residual) между предсказанием и реальным значением.

```
Original:    [1.0, 1.1, 1.2, 1.3, 1.4]
Predicted:   [---, 1.0, 1.1, 1.2, 1.3]
Residual:    [1.0, 0.1, 0.1, 0.1, 0.1]  ← Меньше энтропия
```

### Доступные schemes

```cpp
enum PredictionSchemeMethod {
    PREDICTION_NONE = -2,
    PREDICTION_UNDEFINED = -1,
    PREDICTION_DIFFERENCE = 0,           // Delta coding
    MESH_PREDICTION_PARALLELOGRAM = 1,   // Параллелограмм
    MESH_PREDICTION_MULTI_PARALLELOGRAM = 2,
    MESH_PREDICTION_TEX_COORDS_PORTABLE = 5,
    MESH_PREDICTION_GEOMETRIC_NORMAL = 6,
};
```

### Delta Coding (PREDICTION_DIFFERENCE)

Базовый метод — предсказание через предыдущее значение.

```cpp
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::GENERIC,
    draco::PREDICTION_DIFFERENCE
);
```

**Использование:**

- Generic атрибуты
- Point cloud данные
- Когда нет topology information

**Пример:**

```
Values:     [100, 102, 105, 103, 108]
Predicted:  [---, 100, 102, 105, 103]
Delta:      [100,   2,   3,  -2,   5]
```

### Parallelogram Prediction

Предсказание позиции вершины через смежные треугольники.

```cpp
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::POSITION,
    draco::MESH_PREDICTION_PARALLELOGRAM
);
```

**Принцип:**

```
    C
   / \
  /   \
 A-----B-----> P (predicted)

P = B + C - A
```

Для нового угла при вершине B предсказание вычисляется из смежного треугольника ABC.

**Использование:**

- Mesh positions
- Только для triangle meshes
- Требует connectivity data

**Точность:**

- Работает хорошо для гладких поверхностей
- Ошибки на sharp edges, creases

### Multi-Parallelogram Prediction

Улучшенный parallelogram с несколькими соседними треугольниками.

```cpp
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::POSITION,
    draco::MESH_PREDICTION_MULTI_PARALLELOGRAM
);
```

**Принцип:**

```
       C       D
      / \     / \
     /   \   /   \
    A-----B-----E-----F
            \
             P (predicted)

P = weighted_average(parallelogram_predictions)
```

**Использование:**

- Mesh positions
- Лучше для валиков (valence > 4)
- Выше точность на гладких поверхностях

**Trade-offs:**

- Лучшее предсказание
- Медленнее encoding/decoding
- Больше overhead

### Constrained Multi-Parallelogram

Вариант с ограничением на количество используемых соседей.

```cpp
// Выбирается автоматически при средних speed values
encoder.SetSpeedOptions(3, 3);
```

Оптимизация между точностью и скоростью.

### Geometric Normal Prediction

Специализированный метод для нормалей.

```cpp
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::NORMAL,
    draco::MESH_PREDICTION_GEOMETRIC_NORMAL
);
```

**Принцип:**

1. Вычисление геометрической нормали из позиций вершин
2. Предсказание нормали атрибута на основе геометрической
3. Кодирование разницы через octahedron transform

**Octahedron Transform:**

```
3D normal (x, y, z) → 2D octahedron coordinates (u, v)

     /\
    /  \     Нормали отображаются
   /    \    на поверхность октаэдра
  /______\
```

> **Для понимания:** Octahedron transform — это как развернуть глобус на плоскую карту. Трёхмерный вектор нормали (
> направление) проецируется на поверхность октаэдра, а затем «разворачивается» в 2D координаты. Это позволяет эффективно
> кодировать нормали с минимальными потерями.

### Tex Coords Portable Prediction

Специализированный метод для UV координат.

```cpp
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::TEX_COORD,
    draco::MESH_PREDICTION_TEX_COORDS_PORTABLE
);
```

**Принцип:**

1. Анализ смежных UV координат в mesh
2. Предсказание через parallelogram prediction в UV space
3. Учёт wrapping (повторение текстур) через modular arithmetic
4. Кодирование разницы с учётом texture atlas boundaries

**Особенности:**

- Работает с texture atlases (разбитыми текстурами)
- Поддерживает UV wrapping (repeat, mirror, clamp)
- Учитывает seams (швы) в UV mapping
- Оптимизирован для real-time применения

**Пример UV prediction:**

```
UV space:
   (0,1)        (1,1)
      ┌──────────┐
      │    C     │
      │   / \    │
      │  /   \   │
      │ A─────B  │
      │          │
      └──────────┘
   (0,0)        (1,0)

Предсказание для B: P_uv = B_uv + C_uv - A_uv
```

> **Для понимания:** Tex Coords Portable Prediction — это как предсказание следующего кадра в видео. Алгоритм
> анализирует, как UV координаты «движутся» по поверхности меша, и предсказывает следующую координату на основе соседних.
> Особенно важно для texture atlases, где UV координаты могут резко меняться на границах атласа.

### Transform Types

Draco поддерживает различные трансформации для residual значений:

```cpp
enum TransformType {
    TRANSFORM_NONE = 0,
    TRANSFORM_QUANTIZATION = 1,
    TRANSFORM_OCTAHEDRON = 2,      // Для нормалей
    TRANSFORM_ATTRIBUTE_INDICES = 3,
};
```

#### Octahedron Transform для нормалей

```cpp
// Автоматически применяется при Geometric Normal Prediction
// Преобразует 3D нормаль в 2D координаты на октаэдре

std::array<float, 2> octahedron_encode(std::array<float, 3> normal) {
    // Нормализация
    float length = std::sqrt(normal[0]*normal[0] +
                            normal[1]*normal[1] +
                            normal[2]*normal[2]);
    normal[0] /= length; normal[1] /= length; normal[2] /= length;

    // Проекция на октаэдр
    float abs_sum = std::abs(normal[0]) + std::abs(normal[1]) + std::abs(normal[2]);
    float u = normal[0] / abs_sum;
    float v = normal[1] / abs_sum;

    // Обработка отрицательных октантов
    if (normal[2] < 0.0f) {
        float temp_u = (1.0f - std::abs(v)) * (u >= 0.0f ? 1.0f : -1.0f);
        float temp_v = (1.0f - std::abs(u)) * (v >= 0.0f ? 1.0f : -1.0f);
        u = temp_u;
        v = temp_v;
    }

    return {u, v};
}
```

### Выбор prediction scheme

#### Автоматический выбор

Draco автоматически выбирает prediction scheme на основе:

- Типа атрибута (position, normal, texcoord, etc.)
- Speed options
- Типа геометрии (mesh vs point cloud)

```cpp
// Автоматический выбор (рекомендуется)
encoder.SetSpeedOptions(5, 5);  // Draco выберет оптимальные схемы

// Ручной выбор для полного контроля
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::POSITION,
    draco::MESH_PREDICTION_MULTI_PARALLELOGRAM
);
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::NORMAL,
    draco::MESH_PREDICTION_GEOMETRIC_NORMAL
);
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::TEX_COORD,
    draco::MESH_PREDICTION_TEX_COORDS_PORTABLE
);
```

#### Рекомендации по выбору

| Атрибут     | Рекомендуемая схема                 | Альтернатива                  |
|-------------|-------------------------------------|-------------------------------|
| POSITION    | MESH_PREDICTION_MULTI_PARALLELOGRAM | MESH_PREDICTION_PARALLELOGRAM |
| NORMAL      | MESH_PREDICTION_GEOMETRIC_NORMAL    | PREDICTION_DIFFERENCE         |
| TEX_COORD   | MESH_PREDICTION_TEX_COORDS_PORTABLE | PREDICTION_DIFFERENCE         |
| COLOR       | PREDICTION_DIFFERENCE               | -                             |
| GENERIC     | PREDICTION_DIFFERENCE               | -                             |
| Point cloud | PREDICTION_DIFFERENCE               | -                             |

### Примеры использования

#### Базовый пример с автоматическим выбором

```cpp
#include <draco/compression/encode.h>
#include <expected>
#include <print>
#include <span>

std::expected<std::vector<std::byte>, draco::Status> encode_mesh_auto(
    const draco::Mesh& mesh)
{
    draco::Encoder encoder;

    // Автоматический выбор оптимальных параметров
    encoder.SetSpeedOptions(5, 5);  // Баланс скорости и сжатия

    // Квантование
    encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 11);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, 8);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, 10);

    // Кодирование
    draco::EncoderBuffer buffer;
    auto status = encoder.EncodeMeshToBuffer(mesh, &buffer);
    if (!status.ok()) {
        return std::unexpected(status);
    }

    // Конвертация в std::byte
    const char* data = buffer.data();
    size_t size = buffer.size();
    std::vector<std::byte> result(size);
    std::memcpy(result.data(), data, size);

    return result;
}
```

#### Расширенный пример с ручной настройкой

```cpp
#include <draco/compression/encode.h>
#include <print>

std::expected<std::vector<std::byte>, draco::Status> encode_mesh_advanced(
    const draco::Mesh& mesh)
{
    draco::Encoder encoder;

    // Метод кодирования
    encoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);

    // Speed options
    encoder.SetSpeedOptions(2, 8);  // Медленное кодирование, быстрое декодирование

    // Prediction schemes
    encoder.SetAttributePredictionScheme(
        draco::GeometryAttribute::POSITION,
        draco::MESH_PREDICTION_MULTI_PARALLELOGRAM
    );
    encoder.SetAttributePredictionScheme(
        draco::GeometryAttribute::NORMAL,
        draco::MESH_PREDICTION_GEOMETRIC_NORMAL
    );
    encoder.SetAttributePredictionScheme(
        draco::GeometryAttribute::TEX_COORD,
        draco::MESH_PREDICTION_TEX_COORDS_PORTABLE
    );

    // Квантование
    encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 12);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, 10);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, 12);

    // Явное bounding box для позиций
    auto bbox = mesh.ComputeBoundingBox();
    std::array<float, 3> origin = {
        bbox.GetMinPoint()[0],
        bbox.GetMinPoint()[1],
        bbox.GetMinPoint()[2]
    };
    float range = std::max({
        bbox.Size()[0],
        bbox.Size()[1],
        bbox.Size()[2]
    });

    encoder.SetAttributeExplicitQuantization(
        draco::GeometryAttribute::POSITION,
        12, 3, origin.data(), range
    );

    // Кодирование
    draco::EncoderBuffer buffer;
    auto status = encoder.EncodeMeshToBuffer(mesh, &buffer);
    if (!status.ok()) {
        return std::unexpected(status);
    }

    std::vector<std::byte> result(buffer.size());
    std::memcpy(result.data(), buffer.data(), buffer.size());
    return result;
}
```

### Влияние на compression

#### Эффективность различных схем

| Схема                               | Compression улучшение | Overhead |
|-------------------------------------|-----------------------|----------|
| PREDICTION_DIFFERENCE               | 1.5×                  | Низкий   |
| MESH_PREDICTION_PARALLELOGRAM       | 2.8×                  | Средний  |
| MESH_PREDICTION_MULTI_PARALLELOGRAM | 3.2×                  | Высокий  |
| MESH_PREDICTION_GEOMETRIC_NORMAL    | 3.5×                  | Средний  |
| MESH_PREDICTION_TEX_COORDS_PORTABLE | 3.0×                  | Высокий  |

> **Примечание:** Улучшение compression измеряется относительно кодирования без prediction.

#### Влияние на скорость

| Схема                               | Encode speed | Decode speed |
|-------------------------------------|--------------|--------------|
| PREDICTION_DIFFERENCE               | Быстрая      | Быстрая      |
| MESH_PREDICTION_PARALLELOGRAM       | Средняя      | Средняя      |
| MESH_PREDICTION_MULTI_PARALLELOGRAM | Медленная    | Медленная    |
| MESH_PREDICTION_GEOMETRIC_NORMAL    | Средняя      | Средняя      |
| MESH_PREDICTION_TEX_COORDS_PORTABLE | Медленная    | Медленная    |

### Диагностика

#### Проверка используемых prediction schemes

```cpp
#include <draco/compression/decode.h>
#include <print>

void analyze_compressed_data(std::span<const std::byte> data) {
    draco::DecoderBuffer buffer;
    buffer.Init(reinterpret_cast<const char*>(data.data()), data.size());

    draco::Decoder decoder;
    auto type_status = decoder.GetEncodedGeometryType(&buffer);
    if (!type_status.ok()) {
        std::println(stderr, "Ошибка определения типа: {}",
                    type_status.status().error_msg());
        return;
    }

    // Сброс буфера для декодирования
    buffer.Init(reinterpret_cast<const char*>(data.data()), data.size());

    // Декодирование с сохранением информации о prediction
    decoder.SetSkipAttributeTransform(draco::POSITION, true);
    decoder.SetSkipAttributeTransform(draco::NORMAL, true);
    decoder.SetSkipAttributeTransform(draco::TEX_COORD, true);

    auto mesh_status = decoder.DecodeMeshFromBuffer(&buffer);
    if (!mesh_status.ok()) {
        std::println(stderr, "Ошибка декодирования: {}",
                    mesh_status.status().error_msg());
        return;
    }

    std::unique_ptr<draco::Mesh> mesh = std::move(mesh_status).value();

    // Анализ атрибутов
    for (int i = 0; i < mesh->num_attributes(); ++i) {
        const draco::PointAttribute* attr = mesh->attribute(i);
        draco::GeometryAttribute::Type type = attr->attribute_type();

        std::string type_name;
        switch (type) {
            case draco::GeometryAttribute::POSITION: type_name = "POSITION"; break;
            case draco::GeometryAttribute::NORMAL: type_name = "NORMAL"; break;
            case draco::GeometryAttribute::TEX_COORD: type_name = "TEX_COORD"; break;
            case draco::GeometryAttribute::COLOR: type_name = "COLOR"; break;
            default: type_name = "GENERIC"; break;
        }

        std::println("Атрибут {}: {} точек, тип {}",
                    i, attr->size(), type_name);
    }
}
```

#### Визуализация prediction errors

```cpp
#include <draco/compression/decode.h>
#include <vector>
#include <print>

std::vector<float> calculate_prediction_errors(
    const draco::Mesh& original,
    const draco::Mesh& decoded)
{
    std::vector<float> errors;

    // Сравнение позиций
    const draco::PointAttribute* orig_pos =
        original.GetNamedAttribute(draco::GeometryAttribute::POSITION);
    const draco::PointAttribute* dec_pos =
        decoded.GetNamedAttribute(draco::GeometryAttribute::POSITION);

    if (orig_pos && dec_pos && orig_pos->size() == dec_pos->size()) {
        for (draco::AttributeValueIndex i(0); i < orig_pos->size(); ++i) {
            std::array<float, 3> orig_val, dec_val;
            orig_pos->GetValue(i, orig_val.data());
            dec_pos->GetValue(i, dec_val.data());

            float error = 0.0f;
            for (int j = 0; j < 3; ++j) {
                float diff = orig_val[j] - dec_val[j];
                error += diff * diff;
            }
            errors.push_back(std::sqrt(error));
        }
    }

    return errors;
}
```

### Best Practices

#### Когда использовать prediction schemes

1. **Используйте автоматический выбор** для большинства случаев:
   ```cpp
   encoder.SetSpeedOptions(5, 5);
   ```

2. **Ручная настройка** когда:
  - Нужен максимальный compression ratio
  - Известны специфические характеристики данных
  - Требуется контроль над speed/quality trade-off

3. **Избегайте сложных схем** для:
  - Point cloud данных (используйте PREDICTION_DIFFERENCE)
  - Real-time кодирования (high speed options)
  - Данных с высокой энтропией (шум)

#### Оптимизация для различных типов данных

| Тип данных      | Рекомендации                                    |
|-----------------|-------------------------------------------------|
| Smooth surfaces | MESH_PREDICTION_MULTI_PARALLELOGRAM             |
| CAD models      | MESH_PREDICTION_PARALLELOGRAM, quantization 14+ |
| Organic shapes  | Все схемы, focus на нормалях и UV               |
| Textured models | MESH_PREDICTION_TEX_COORDS_PORTABLE обязательно |
| Point clouds    | PREDICTION_DIFFERENCE, без квантования          |
| Low-poly models | Sequential encoding, простые схемы              |

#### Отладка проблем

1. **Высокие prediction errors:**
  - Проверьте mesh connectivity
  - Убедитесь в корректности нормалей
  - Проверьте UV mapping на seams

2. **Медленное кодирование:**
  - Увеличьте speed options
  - Используйте Sequential encoding
  - Отключите multi-parallelogram

3. **Плохое сжатие:**
  - Уменьшите speed options
  - Включите более сложные схемы
  - Настройте квантование

> **Для понимания:** Prediction schemes в Draco — это как умный компрессор для 3D данных. Они анализируют структуру
> геометрии и предсказывают значения, уменьшая избыточность. Выбор правильной схемы похож на выбор кодека для видео: для
> плавной анимации нужен один подход, для статичного изображения — другой.

## Draco

<!-- anchor: 00_overview -->


**Draco** — библиотека сжатия и распаковки 3D геометрических данных (mesh и point cloud) от Google. Спроектирована для
эффективного хранения и передачи 3D-графики с минимальной потерей визуального качества.

Версия: **1.5.7**
Исходники: [google/draco](https://github.com/google/draco)

---

## Возможности

- **Сжатие mesh** — треугольные меши с connectivity data
- **Сжатие point cloud** — облака точек с произвольными атрибутами
- **Квантование** — контролируемая потеря точности для лучшего сжатия
- **Prediction schemes** — предсказание значений атрибутов по соседним элементам
- **glTF интеграция** — EXT_mesh_draco расширение
- **Metadata** — пользовательские данные в сжатом файле

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

## Compression ratio

| Тип данных            | Без сжатия | Draco (default) | Сжатие |
|-----------------------|------------|-----------------|--------|
| Mesh 100K triangles   | 12.5 MB    | 0.8 MB          | 15x    |
| Point cloud 1M points | 28 MB      | 2.1 MB          | 13x    |
| Skinned mesh          | 18 MB      | 1.4 MB          | 12x    |

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

- Нужна интеграция с glTF через EXT_mesh_draco
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

## Квантование

Draco использует квантование для уменьшения точности данных:

| Атрибут   | Default (бит) | Рекомендуемый диапазон |
|-----------|---------------|------------------------|
| Position  | 11            | 10-16                  |
| Normal    | 8             | 6-10                   |
| Tex coord | 10            | 8-12                   |
| Color     | 8             | 6-10                   |

> **Примечание:** Большее число бит = выше точность, но хуже сжатие.

## Требования

- **C++11** или новее (C++17 для DRACO_TRANSCODER_SUPPORTED)
- Платформы: Windows, Linux, macOS, Android, iOS

## Оригинальная документация

- [Draco README](https://github.com/google/draco/blob/main/README.md)
- [BUILDING.md](https://github.com/google/draco/blob/main/BUILDING.md)
- [glTF EXT_mesh_draco](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_draco_mesh_compression)

---

## Основные понятия

<!-- anchor: 03_concepts -->


Ключевые концепции Draco: геометрия, атрибуты, квантование.

## Геометрия

### PointCloud

Базовый класс для хранения облака точек с атрибутами:

```cpp
#include <draco/point_cloud/point_cloud.h>

draco::PointCloud pc;

// Количество точек
pc.set_num_points(1000);

// Добавление атрибутов
int posAttrId = pc.AddAttribute(positionAttribute, true, 1000);

// Доступ к атрибутам
const draco::PointAttribute* attr = pc.GetNamedAttribute(
    draco::GeometryAttribute::POSITION
);
```

### Mesh

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

### Отношения между классами

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

## Атрибуты

### GeometryAttribute

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

### Типы атрибутов

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

### Типы данных

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

### PointAttribute

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
attr->SetAttributeValue(draco::AttributeValueIndex(0), &pos);

// Чтение значения
std::array<float, 3> outPos;
attr->GetValue(draco::AttributeValueIndex(0), &outPos);
```

### Mapping точек к значениям

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

## Квантование

### Принцип

Квантование уменьшает точность float значений:

```
Original: 1.23456789, 2.34567890, 3.45678901
Quantized (11 bit):  1.234,       2.346,      3.457
```

### Квантование позиций

```cpp
// При кодировании
encoder.SetAttributeQuantization(
    draco::GeometryAttribute::POSITION,
    14  // 14 бит на компонент
);

// Явное задание bounding box
float origin[3] = {0.0f, 0.0f, 0.0f};
float range = 100.0f;  // Диапазон [origin, origin + range]

encoder.SetAttributeExplicitQuantization(
    draco::GeometryAttribute::POSITION,
    14,     // quantization_bits
    3,      // num_dims
    origin,
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

> **Примечание:** 11 бит достаточно для большинства визуальных применений.

### Dequantization при декодировании

```cpp
// По умолчанию: автоматическая деквантование
draco::Decoder decoder;
auto mesh = decoder.DecodeMeshFromBuffer(&buffer);

// Без деквантования (получаем quantized values)
draco::Decoder decoder;
decoder.SetSkipAttributeTransform(draco::GeometryAttribute::POSITION);
auto mesh = decoder.DecodeMeshFromBuffer(&buffer);

// Доступ к параметрам квантования
const auto* attr = mesh->GetNamedAttribute(draco::GeometryAttribute::POSITION);
// attr содержит AttributeTransformData
```

## Индексы

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

### Corner Table

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

## BoundingBox

Вычисление границ геометрии:

```cpp
#include <draco/core/bounding_box.h>

draco::BoundingBox bbox = mesh.ComputeBoundingBox();

draco::Vector3f min = bbox.GetMinPoint();
draco::Vector3f max = bbox.GetMaxPoint();

// Размер
draco::Vector3f size = bbox.Size();
```

## Deduplication

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

## Metadata

Пользовательские данные в сжатом файле:

```cpp
#include <draco/metadata/geometry_metadata.h>

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
        std::cout << "Model name: " << name << "\n";
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
draco::DecoderBuffer buffer;
buffer.Init(data, size);

auto geomType = draco::Decoder::GetEncodedGeometryType(&buffer);

switch (geomType.value()) {
    case draco::TRIANGULAR_MESH:
        std::cout << "Mesh\n";
        break;
    case draco::POINT_CLOUD:
        std::cout << "Point cloud\n";
        break;
    case draco::INVALID_GEOMETRY_TYPE:
        std::cout << "Invalid\n";
        break;
}

---

## Методы кодирования

<!-- anchor: 05_encoding-methods -->


Описание методов сжатия для mesh и point cloud.

## Обзор

Draco использует разные стратегии для mesh и point cloud:

```

Mesh Point Cloud
───── ───────────
Edgebreaker KD-Tree
Sequential Sequential

```

## Mesh Encoding

### Edgebreaker

Оптимальный метод для сжатия connectivity data.

```cpp
#include <draco/compression/encode.h>

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

### Sequential

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

### Сравнение методов для Mesh

| Характеристика | Edgebreaker | Sequential |
|----------------|-------------|------------|
| Compression    | Лучший      | Хороший    |
| Decode speed   | Средняя     | Быстрая    |
| Encode speed   | Медленная   | Быстрая    |
| Сложность      | Высокая     | Низкая     |

### Выбор метода

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

## Point Cloud Encoding

### KD-Tree Encoding

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

### Sequential Encoding

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

### Сравнение методов для Point Cloud

| Характеристика       | KD-Tree   | Sequential |
|----------------------|-----------|------------|
| Compression          | Лучший    | Хороший    |
| Decode speed         | Медленная | Быстрая    |
| Encode speed         | Медленная | Быстрая    |
| Требует quantization | Да        | Нет        |

## Speed Options

Параметр `SetSpeedOptions(encoding_speed, decoding_speed)` контролирует trade-off:

```cpp
// 0 = максимальное сжатие, минимальная скорость
// 10 = минимальное сжатие, максимальная скорость

encoder.SetSpeedOptions(0, 0);   // Максимальный compression
encoder.SetSpeedOptions(5, 5);   // Баланс
encoder.SetSpeedOptions(10, 10); // Максимальная скорость
encoder.SetSpeedOptions(0, 10);  // Медленное кодирование, быстрое декодирование
```

### Влияние на алгоритмы

| Speed | Prediction schemes | Entropy coding |
|-------|--------------------|----------------|
| 0     | Все доступные      | rANS adaptive  |
| 5     | Основные           | rANS           |
| 10    | Delta only         | Direct         |

## Внутренняя структура encoding

### Pipeline для Mesh (Edgebreaker)

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

### Pipeline для Point Cloud (KD-Tree)

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

## Traversal методы

Для mesh encoding доступны разные traversal стратегии:

```cpp
// Depth-first traversal (default)
// MESH_TRAVERSAL_DEPTH_FIRST

// Prediction degree traversal
// MESH_TRAVERSAL_PREDICTION_DEGREE
```

Traversal влияет на порядок обработки вершин и качество предсказания.

## Entropy Coding

Draco использует rANS (range Asymmetric Numeral Systems):

```cpp
// Автоматически выбирается на основе speed options
// Низкий speed = rANS adaptive
// Высокий speed = direct bit coding
```

### Типы entropy coders

| Coder                    | Использование         |
|--------------------------|-----------------------|
| `RAnsBitEncoder`         | Базовый rANS          |
| `AdaptiveRAnsBitEncoder` | Адаптивный rANS       |
| `DirectBitEncoder`       | Без сжатия (speed=10) |
| `SymbolBitEncoder`       | Для symbol data       |

## Connectivity Encoding Variants

Для Edgebreaker доступны варианты:

```cpp
enum MeshEdgebreakerConnectivityEncodingMethod {
    MESH_EDGEBREAKER_STANDARD_ENCODING = 0,
    MESH_EDGEBREAKER_PREDICTIVE_ENCODING = 1,  // Deprecated
    MESH_EDGEBREAKER_VALENCE_ENCODING = 2,     // Лучший compression
};
```

### Valence Encoding

Использует valence (количество соседних граней) для лучшего сжатия:

```cpp
// Valence encoding включается автоматически при низком speed
encoder.SetSpeedOptions(0, 0);  // Valence encoding активен
```

## Примеры конфигураций

### Максимальное сжатие (offline)

```cpp
draco::Encoder encoder;
encoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);
encoder.SetSpeedOptions(0, 0);
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 11);
encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, 8);
encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, 10);
```

### Быстрое кодирование (real-time)

```cpp
draco::Encoder encoder;
encoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
encoder.SetSpeedOptions(10, 10);
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 14);
```

### Быстрое декодирование (runtime)

```cpp
draco::Encoder encoder;
encoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);
encoder.SetSpeedOptions(5, 10);  // Медленное кодирование, быстрое декодирование
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 12);
```

### Streaming (point cloud)

```cpp
draco::Encoder encoder;
encoder.SetEncodingMethod(draco::POINT_CLOUD_SEQUENTIAL_ENCODING);
encoder.SetSpeedOptions(10, 10);
// Без квантования для сохранения точности
```

### Dense point cloud (LiDAR)

```cpp
draco::Encoder encoder;
encoder.SetEncodingMethod(draco::POINT_CLOUD_KD_TREE_ENCODING);
encoder.SetSpeedOptions(0, 0);
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 16);
encoder.SetAttributeQuantization(draco::GeometryAttribute::COLOR, 8);
```

## Compression ratios

Ориентировочные значения для типичных моделей:

### Mesh

| Модель               | Original | Edgebreaker | Sequential |
|----------------------|----------|-------------|------------|
| Character (50K tris) | 2.8 MB   | 180 KB      | 280 KB     |
| Building (200K tris) | 12 MB    | 600 KB      | 1.1 MB     |
| Terrain (1M tris)    | 58 MB    | 2.5 MB      | 4.8 MB     |

### Point Cloud

| Модель                   | Original | KD-Tree | Sequential |
|--------------------------|----------|---------|------------|
| LiDAR scan (1M pts)      | 28 MB    | 2.1 MB  | 4.2 MB     |
| Photogrammetry (10M pts) | 280 MB   | 18 MB   | 42 MB      |

> **Примечание:** Результаты сильно зависят от квантования и типа данных.

---

## Prediction Schemes

<!-- anchor: 06_prediction-schemes -->


Методы предсказания значений атрибутов для улучшения сжатия.

## Принцип работы

Prediction schemes предсказывают значение атрибута на основе соседних элементов. Вместо хранения абсолютных значений
кодируется разница (residual) между предсказанием и реальным значением.

```
Original:    [1.0, 1.1, 1.2, 1.3, 1.4]
Predicted:   [---, 1.0, 1.1, 1.2, 1.3]
Residual:    [1.0, 0.1, 0.1, 0.1, 0.1]  ← Меньше энтропия
```

## Доступные schemes

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

## Delta Coding (PREDICTION_DIFFERENCE)

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

## Parallelogram Prediction

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

## Multi-Parallelogram Prediction

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

## Constrained Multi-Parallelogram

Вариант с ограничением на количество используемых соседей.

```cpp
// Выбирается автоматически при средних speed values
encoder.SetSpeedOptions(3, 3);
```

Оптимизация между точностью и скоростью.

## Geometric Normal Prediction

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

> **Примечание:** Здесь используется SIMD для оптимизации. См. гайд docs/guides/cpp/14_simd-intrinsics.md

## Tex Coords Portable Prediction

Специализированный метод для UV координат.

```cpp
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::TEX_COORD,
    draco::MESH_PREDICTION_TEX_COORDS_PORTABLE
);
```

**Принцип:**

1. Анализ смежных треугольников
2. Предсказание UV на основе текстурной непрерывности
3. Обработка seams (швов) в UV mapping

**Особенности:**

- Работает с UV seams
- Портативный между платформами
- Учитывает texture wrapping

## Transform Types

Prediction schemes используют transforms для residual coding:

```cpp
enum PredictionSchemeTransformType {
    PREDICTION_TRANSFORM_NONE = -1,
    PREDICTION_TRANSFORM_DELTA = 0,         // Базовый delta
    PREDICTION_TRANSFORM_WRAP = 1,          // Wrap-around для integer
    PREDICTION_TRANSFORM_NORMAL_OCTAHEDRON = 2,        // Для normal
    PREDICTION_TRANSFORM_NORMAL_OCTAHEDRON_CANONICALIZED = 3,
};
```

### Wrap Transform

Для integer атрибутов с ограниченным диапазоном:

```cpp
// Вместо:
// residual = actual - predicted

// Используется wrap:
// residual = wrap(actual - predicted, min_value, max_value)
```

Уменьшает magnitude residuals.

### Normal Octahedron Transform

Для нормалей:

```cpp
// 3D normal → 2D octahedron coordinates
// residuals кодируются в 2D пространстве
```

## Выбор prediction scheme

### По типу атрибута

| Атрибут   | Рекомендуемый scheme                  |
|-----------|---------------------------------------|
| POSITION  | PARALLELOGRAM или MULTI_PARALLELOGRAM |
| NORMAL    | GEOMETRIC_NORMAL                      |
| TEX_COORD | TEX_COORDS_PORTABLE                   |
| COLOR     | DIFFERENCE                            |
| GENERIC   | DIFFERENCE                            |

### По типу геометрии

| Геометрия         | Рекомендации        |
|-------------------|---------------------|
| Smooth mesh       | MULTI_PARALLELOGRAM |
| Hard surface mesh | PARALLELOGRAM       |
| Point cloud       | DIFFERENCE          |
| Organic model     | MULTI_PARALLELOGRAM |

### Автоматический выбор

```cpp
// По умолчанию encoder выбирает оптимальный scheme
draco::Encoder encoder;
encoder.SetSpeedOptions(0, 0);  // Лучший compression
// Prediction schemes выбираются автоматически
```

## Примеры

### Явное указание schemes

```cpp
draco::Encoder encoder;

// Positions: multi-parallelogram
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::POSITION,
    draco::MESH_PREDICTION_MULTI_PARALLELOGRAM
);

// Normals: geometric prediction
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::NORMAL,
    draco::MESH_PREDICTION_GEOMETRIC_NORMAL
);

// UV: portable tex coords
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::TEX_COORD,
    draco::MESH_PREDICTION_TEX_COORDS_PORTABLE
);

// Colors: simple delta
encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::COLOR,
    draco::PREDICTION_DIFFERENCE
);
```

### Через ExpertEncoder

```cpp
draco::ExpertEncoder encoder(mesh);

// Атрибут 0 (position): parallelogram
encoder.SetPredictionSchemeForAttribute(0, draco::MESH_PREDICTION_PARALLELOGRAM);

// Атрибут 1 (normal): geometric normal
encoder.SetPredictionSchemeForAttribute(1, draco::MESH_PREDICTION_GEOMETRIC_NORMAL);

// Атрибут 2 (uv): tex coords portable
encoder.SetPredictionSchemeForAttribute(2, draco::MESH_PREDICTION_TEX_COORDS_PORTABLE);

// Атрибут 3 (generic): no prediction
encoder.SetPredictionSchemeForAttribute(3, draco::PREDICTION_NONE);
```

### Отключение prediction

```cpp
// Иногда prediction ухудшает сжатие
// Например, для случайных данных

encoder.SetAttributePredictionScheme(
    draco::GeometryAttribute::GENERIC,
    draco::PREDICTION_NONE
);
```

## Влияние на compression

Ориентировочное улучшение compression ratio:

| Scheme              | Improvement vs NONE |
|---------------------|---------------------|
| DIFFERENCE          | 10-20%              |
| PARALLELOGRAM       | 20-35%              |
| MULTI_PARALLELOGRAM | 25-40%              |
| GEOMETRIC_NORMAL    | 30-50%              |
| TEX_COORDS_PORTABLE | 15-25%              |

> **Примечание:** Результаты зависят от характеристик модели.

## Диагностика

### Проверка эффективности prediction

```cpp
// После кодирования проверьте размер атрибутов
for (int i = 0; i < mesh.num_attributes(); ++i) {
    const auto* attr = mesh.attribute(i);
    std::cout << "Attribute " << i << ": "
              << attr->size() << " values\n";
}
```

### Когда prediction не помогает

- Случайные данные (noise)
- Высокочастотные изменения
- Несвязанные атрибуты
- Очень маленькие меши (< 100 vertices)

## Внутренняя реализация

### Encoder side

```cpp
// Упрощённая логика
for (each vertex in traversal_order) {
    predicted_value = compute_prediction(vertex, neighbors);
    residual = actual_value - predicted_value;
    encode_residual(residual);
}
```

### Decoder side

```cpp
// Упрощённая логика
for (each vertex in traversal_order) {
    predicted_value = compute_prediction(vertex, neighbors);
    residual = decode_residual();
    actual_value = predicted_value + residual;
}
```

Traversal order должен быть идентичным для encoder и decoder.

---

## Глоссарий

<!-- anchor: 11_glossary -->


Термины и определения, используемые в Draco.

## A

### Attribute (Атрибут)

Данные, связанные с каждой точкой или вершиной геометрии. Примеры: позиция, нормаль, текстурные координаты, цвет.

### AttributeValueIndex

Типизированный индекс для доступа к значениям атрибута. Отличается от PointIndex, так как несколько точек могут
ссылаться на одно значение атрибута.

## B

### Bitstream (Битовый поток)

Сжатое представление геометрии в бинарном формате Draco. Версия bitstream определяет совместимость encoder/decoder.

### BoundingBox (Ограничивающий объём)

Минимальный параллелепипед, содержащий все точки геометрии. Используется для квантования.

### Buffer (Буфер)

Контейнер для бинарных данных. `DecoderBuffer` — для чтения, `EncoderBuffer` — для записи.

## C

### clers

Символы Edgebreaker encoding: **C**reate, **L**eft, **E**nd, **R**ight, **S**plit. Описывают topology mesh при обходе.

### Compression Level (Уровень сжатия)

Параметр от 0 до 10, определяющий баланс между скоростью и качеством сжатия. 0 — лучшее сжатие, 10 — максимальная
скорость.

### Connectivity (Связность)

Информация о том, как вершины соединяются в грани. Отдельно от данных атрибутов.

### Corner (Угол)

Вершина в контексте конкретной грани. Каждая грань имеет 3 corner. Corner позволяет различать разные "экземпляры" одной
вершины в разных гранях.

### Corner Table

Структура данных для навигации по топологии mesh. Позволяет переходить между смежными углами и гранями.

## D

### Deduplication (Дедупликация)

Устранение дублирующихся данных. В Draco: устранение точек с одинаковыми атрибутами или устранение одинаковых значений
атрибутов.

### Delta Coding (Дельта-кодирование)

Метод сжатия, при котором хранится разница между соседними значениями вместо абсолютных значений.

### Decoder (Декодер)

Класс для преобразования сжатого Draco bitstream обратно в геометрию.

### DecoderBuffer

Буфер для последовательного чтения сжатых данных с поддержкой bit-level операций.

### Encoder (Энкодер)

Класс для преобразования геометрии в сжатый Draco bitstream.

### EncoderBuffer

Буфер для последовательной записи сжатых данных.

### Entropy Coding (Энтропийное кодирование)

Финальный этап сжатия, использующий статистику частот символов. В Draco используется rANS.

### ExpertEncoder

Расширенный энкодер с детальным контролем над каждым атрибутом отдельно.

## F

### Face (Грань)

Треугольник в mesh. Определяется тремя индексами точек (PointIndex).

### FaceIndex

Типизированный индекс для доступа к граням mesh.

## G

### Generic Attribute

Пользовательский атрибут без предопределённого типа. Используется для кастомных данных (например, occlusion, custom
weights).

### Geometry (Геометрия)

Общее название для PointCloud и Mesh.

### GeometryAttribute

Базовое описание атрибута: тип, формат данных, компоненты, stride, offset.

### glTF

Graphics Library Transmission Format — стандартный формат для 3D моделей. Draco интегрируется через расширение
KHR_draco_mesh_compression.

## I

### Identity Mapping

Тип связи между точками и значениями атрибутов, при котором point[i] → value[i] (один к одному).

## K

### KD-Tree (K-мерное дерево)

Структура данных для пространственного разбиения. Используется для сжатия point cloud.

### KHR_draco_mesh_compression

Официальное расширение glTF 2.0 для сжатия геометрии Draco.

## M

### Mapping (Отображение)

Связь между PointIndex и AttributeValueIndex. Может быть identity (1:1) или explicit (произвольная).

### Mesh (Меш)

Треугольная сетка: набор вершин с атрибутами и connectivity data (грани).

### MeshAttributeElementType

Тип элемента атрибута: per-vertex, per-corner или per-face.

### Metadata (Метаданные)

Пользовательские данные, хранящиеся вместе с геометрией. Может быть на уровне геометрии или атрибута.

## P

### Parallelogram Prediction

Метод предсказания позиции вершины через смежный треугольник. P = B + C - A.

### Point Cloud (Облако точек)

Набор точек с атрибутами без connectivity data.

### PointIndex

Типизированный индекс для доступа к точкам геометрии.

### PointAttribute

Атрибут с mapping данными. Связывает точки со значениями атрибутов.

### Prediction Scheme (Схема предсказания)

Метод предсказания значения атрибута на основе соседних элементов. Уменьшает энтропию для лучшего сжатия.

## Q

### Quantization (Квантование)

Преобразование float значений в целые с заданной точностью. Основной метод уменьшения размера данных.

### Quantization Bits

Количество бит на компонент при квантовании. Больше бит = выше точность = больше размер.

## R

### rANS (range Asymmetric Numeral Systems)

Алгоритм энтропийного кодирования, используемый в Draco. Более эффективен чем Huffman для больших алфавитов.

### Residual (Остаток)

Разница между реальным значением и предсказанным. Кодируется вместо абсолютного значения.

## S

### Sequential Encoding

Метод кодирования с линейным обходом данных. Простой и быстрый, но менее эффективный по сжатию.

### SetSpeedOptions

Метод настройки баланса скорость/сжатие. Параметры: encoding_speed (0-10), decoding_speed (0-10).

### Skin (Скин)

Данные для скиннинга: joints, weights, inverse bind matrices.

### Sparse Data

Данные с "дырами" — не все элементы заполнены. Поддерживается через sparse accessors в glTF.

### Status / StatusOr

Типы для обработки ошибок. Status — без значения, StatusOr<T> — с значением или ошибка.

## T

### Topology (Топология)

Структура связности mesh: какие вершины образуют грани, как грани соединяются.

### Transcoder

Инструмент для преобразования glTF без Draco в glTF с Draco-сжатием.

### Traversal (Обход)

Порядок обработки вершин/граней при кодировании. Влияет на эффективность prediction schemes.

## V

### Valence (Валентность)

Количество граней, инцидентных вершине. Используется в Valence Encoding для лучшего сжатия connectivity.

### Vertex (Вершина)

Точка в 3D пространстве с набором атрибутов.

## E

### Edgebreaker

Метод кодирования mesh connectivity с высоким compression ratio. Основан на обходе mesh по спирали.

### Explicit Mapping

Тип связи между точками и значениями атрибутов с явным указанием соответствия через mapping table.

### EXT_mesh_draco

Устаревшее название расширения glTF. Текущее название: KHR_draco_mesh_compression.

### EXT_mesh_features

Расширение glTF для хранения feature IDs. Полезно для воксельных данных.

### EXT_structural_metadata

Расширение glTF для структурированных метаданных.

## Сокращения

| Сокращение | Полное название                  |
|------------|----------------------------------|
| cl         | Compression Level                |
| qp         | Quantization Position            |
| qn         | Quantization Normal              |
| qt         | Quantization Tex coord           |
| qc         | Quantization Color               |
| UV         | Texture coordinates              |
| PBR        | Physically Based Rendering       |
| SIMD       | Single Instruction Multiple Data |

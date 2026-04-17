# Основные понятия

🟢 **Уровень 1: Начинающий**

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

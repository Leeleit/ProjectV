# Методы кодирования

🟡 **Уровень 2: Средний**

Описание методов сжатия для mesh и point cloud.

## Обзор

Draco использует разные стратегии для mesh и point cloud:

```
Mesh                Point Cloud
─────               ───────────
Edgebreaker         KD-Tree
Sequential          Sequential
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

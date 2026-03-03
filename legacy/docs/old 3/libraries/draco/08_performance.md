# Производительность

🟡 **Уровень 2: Средний**

Оптимизация compression ratio и decode speed.

## Метрики

### Compression Ratio

```
Compression Ratio = Original Size / Compressed Size
```

| Модель               | Original | Draco  | Ratio |
|----------------------|----------|--------|-------|
| Character (50K tris) | 2.8 MB   | 180 KB | 15.5x |
| Building (200K tris) | 12 MB    | 600 KB | 20x   |
| Terrain (1M tris)    | 58 MB    | 2.5 MB | 23x   |

### Decode Time

Время декодирования на CPU (ms):

| Mesh size | Edgebreaker | Sequential |
|-----------|-------------|------------|
| 10K tris  | 5-10        | 2-5        |
| 100K tris | 50-100      | 20-40      |
| 1M tris   | 500-1000    | 200-400    |

### Encode Time

Время кодирования на CPU (ms):

| Mesh size | Edgebreaker (cl=0) | Edgebreaker (cl=10) |
|-----------|--------------------|---------------------|
| 10K tris  | 50-100             | 10-20               |
| 100K tris | 500-1000           | 100-200             |
| 1M tris   | 5000-10000         | 1000-2000           |

## Факторы влияния

### Quantization

Больше бит = больше размер файла:

| Position bits | Размер       | Точность |
|---------------|--------------|----------|
| 8             | Минимальный  | ~0.4%    |
| 11            | Базовый      | ~0.05%   |
| 14            | Большой      | ~0.006%  |
| 16            | Максимальный | ~0.0015% |

```cpp
// Компромисс: качество vs размер
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 12);  // Баланс
```

### Compression Level

| Level | Размер       | Encode   | Decode |
|-------|--------------|----------|--------|
| 0     | Минимальный  | Медленно | Средне |
| 5     | Средний      | Средне   | Быстро |
| 10    | Максимальный | Быстро   | Быстро |

```cpp
// Для offline preprocessing
encoder.SetSpeedOptions(0, 0);

// Для runtime encoding
encoder.SetSpeedOptions(10, 10);

// Для быстрого декодирования
encoder.SetSpeedOptions(5, 10);
```

### Encoding Method

| Method      | Compression | Decode Speed |
|-------------|-------------|--------------|
| Edgebreaker | Лучший      | Средняя      |
| Sequential  | Хороший     | Быстрая      |

### Prediction Schemes

Влияние на compression ratio:

| Scheme              | Improvement |
|---------------------|-------------|
| NONE                | Baseline    |
| DIFFERENCE          | +10-20%     |
| PARALLELOGRAM       | +20-35%     |
| MULTI_PARALLELOGRAM | +25-40%     |

## Профилирование

### Измерение размера

```cpp
#include <draco/compression/encode.h>

draco::Encoder encoder;
encoder.SetSpeedOptions(0, 0);

draco::EncoderBuffer buffer;
encoder.EncodeMeshToBuffer(mesh, &buffer);

std::cout << "Original vertices: " << mesh.num_points() << "\n";
std::cout << "Compressed size: " << buffer.size() << " bytes\n";
std::cout << "Bytes per vertex: "
          << static_cast<float>(buffer.size()) / mesh.num_points() << "\n";
```

### Измерение времени

```cpp
#include <chrono>

auto start = std::chrono::high_resolution_clock::now();

draco::Decoder decoder;
auto result = decoder.DecodeMeshFromBuffer(&buffer);

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

std::cout << "Decode time: " << duration.count() << " ms\n";
```

### С Tracy

```cpp
#include <tracy/Tracy.hpp>

void decodeWithProfiling(const void* data, size_t size) {
    ZoneScopedN("DracoDecode");

    draco::DecoderBuffer buffer;
    buffer.Init(reinterpret_cast<const char*>(data), size);

    draco::Decoder decoder;

    {
        ZoneScopedN("DecodeMesh");
        auto result = decoder.DecodeMeshFromBuffer(&buffer);
    }

    TracyPlot("MeshSize", static_cast<int64_t>(buffer.decoded_size()));
    FrameMark;
}
```

## Оптимизация decode speed

### Выбор метода

```cpp
// Быстрое декодирование
draco::Encoder encoder;
encoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);  // Быстрее
encoder.SetSpeedOptions(5, 10);  // Оптимизация под decode
```

### Пропуск деквантования

```cpp
// Если квантование приемлемо для вашего use case
draco::Decoder decoder;
decoder.SetSkipAttributeTransform(draco::GeometryAttribute::POSITION);
// Деквантование будет пропущено, decode быстрее
```

### Предварительное выделение памяти

```cpp
// Выделение памяти заранее для больших mesh
draco::EncoderBuffer buffer;
buffer.Reserve(estimated_size);  // Уменьшает реаллокации
```

## Оптимизация compression ratio

### Максимальное сжатие

```cpp
draco::Encoder encoder;
encoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);
encoder.SetSpeedOptions(0, 0);

// Минимальное квантование для вашего use case
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 10);
encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, 6);
encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, 8);
```

### Deduplication перед кодированием

```cpp
// Устранение дубликатов улучшает сжатие
mesh.DeduplicatePointIds();
mesh.DeduplicateAttributeValues();

draco::Encoder encoder;
// ...
```

### Удаление неиспользуемых атрибутов

```cpp
// Удалите ненужные атрибуты перед кодированием
for (int i = mesh.num_attributes() - 1; i >= 0; --i) {
    const auto* attr = mesh.attribute(i);
    if (!isAttributeNeeded(attr->attribute_type())) {
        mesh.DeleteAttribute(i);
    }
}
```

## Memory usage

### Decoder

```cpp
// Пиковое использование памяти при декодировании
// ~2-3x от размера сжатых данных

// Для контроля памяти используйте:
// - Stream processing для больших файлов
// - Chunked loading
```

### Encoder

```cpp
// Пиковое использование памяти при кодировании
// ~3-5x от размера исходных данных

// Edgebreaker требует больше памяти чем Sequential
```

## Benchmarks

### Тестовые модели

| Модель | Vertices | Triangles | Original | Compressed | Ratio |
|--------|----------|-----------|----------|------------|-------|
| Cube   | 8        | 12        | 1 KB     | 200 B      | 5x    |
| Sphere | 482      | 960       | 20 KB    | 3 KB       | 6.7x  |
| Bunny  | 35947    | 69451     | 4 MB     | 280 KB     | 14x   |
| Dragon | 50161    | 100000    | 5 MB     | 350 KB     | 14x   |
| Buddha | 543652   | 1087716   | 55 MB    | 2.8 MB     | 19x   |

### Decode speed по платформам

| Platform      | 100K tris  | 1M tris      |
|---------------|------------|--------------|
| x86-64 (AVX2) | 20-40 ms   | 200-400 ms   |
| x86-64 (SSE)  | 30-60 ms   | 300-600 ms   |
| ARM64 (NEON)  | 25-50 ms   | 250-500 ms   |
| ARM32         | 50-100 ms  | 500-1000 ms  |
| WebAssembly   | 100-200 ms | 1000-2000 ms |

## Практические рекомендации

### Для web

```cpp
// Оптимизация под web loading
encoder.SetSpeedOptions(0, 5);  // Хороший compression, разумный decode
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 12);
```

### Для mobile

```cpp
// Оптимизация под мобильные устройства
encoder.SetSpeedOptions(5, 10);  // Быстрый decode
encoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
```

### Для desktop

```cpp
// Баланс для desktop
encoder.SetSpeedOptions(3, 5);
encoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);
encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 14);
```

### Для streaming

```cpp
// Реальное время, минимальная задержка
encoder.SetSpeedOptions(10, 10);
encoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
```

## Trade-offs summary

| Use case  | Method            | Speed    | Quantization          |
|-----------|-------------------|----------|-----------------------|
| Archive   | Edgebreaker, cl=0 | (0, 0)   | Минимальная           |
| Download  | Edgebreaker, cl=5 | (0, 5)   | Средняя               |
| Streaming | Sequential, cl=10 | (10, 10) | Зависит от требований |
| Real-time | Sequential, cl=10 | (10, 10) | Высокая               |

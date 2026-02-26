# meshoptimizer

> Библиотека для оптимизации 3D-мешей. Повышает производительность GPU-рендеринга через оптимизацию порядка вершин,
> упрощение геометрии и сжатие данных.

---

## Overview

meshoptimizer — это набор алгоритмов для оптимизации геометрии перед рендерингом. Когда GPU рисует треугольники,
различные этапы конвейера обрабатывают данные вершин и индексов. Эффективность этих этапов зависит от того, как данные
организованы.

> **Для понимания:** Представь, что ты работаешь с документами на столе. Если часто используемые бумаги лежат сверху —
> ты быстро их находишь. Если нужный документ в самом низу стопки — приходится рыться каждый раз. GPU работает похоже:
> чем
> ближе нужные вершины друг к другу в памяти, тем быстрее рендеринг.

### Что умеет библиотека

| Категория     | Возможности                                      |
|---------------|--------------------------------------------------|
| **Рендеринг** | Vertex Cache, Overdraw, Vertex Fetch оптимизация |
| **Геометрия** | Индексация, упрощение (LOD), meshlets            |
| **Сжатие**    | Vertex, Index, Meshlet кодирование               |

### Требования

- **C99** / **C++98** или новее (рекомендуется C++26)
- Нет внешних зависимостей
- SIMD: SSE4.1, AVX, AVX2, NEON, WASM SIMD (опционально)

---

## Pipeline Оптимизации

Правильный порядок — критически важен. Каждый этап создаёт основу для следующего.

```
┌─────────────────────────────────────────────────────────────┐
│  1. INDEXING                                                │
│     meshopt_generateVertexRemap                              │
│     ↓                                                        │
│  2. VERTEX CACHE                                            │
│     meshopt_optimizeVertexCache                              │
│     ↓                                                        │
│  3. OVERDRAW (опционально)                                   │
│     meshopt_optimizeOverdraw                                 │
│     ↓                                                        │
│  4. VERTEX FETCH                                             │
│     meshopt_optimizeVertexFetch                              │
│     ↓                                                        │
│  5. QUANTIZATION (опционально)                               │
│     meshopt_quantizeHalf / meshopt_quantizeSnorm             │
│     ↓                                                        │
│  6. COMPRESSION (опционально)                                │
│     meshopt_encodeVertexBuffer / meshopt_encodeIndexBuffer   │
└─────────────────────────────────────────────────────────────┘
```

> **Для пониманию:** Это как приготовление еды. Нельзя сначала подать блюдо, потом его готовить. Каждый шаг зависит от
> предыдущего: сначала нарезаешь ингредиенты (indexing), потом жаришь (optimization), потом украшаешь (compression).

---

## Core API

### Индексация

Большинство алгоритмов требуют наличия индексного буфера без дублирующихся вершин.

```cpp
#include <meshoptimizer.h>
#include <vector>
#include <span>

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

auto generateRemapTable(
    std::span<const uint32_t> indices,
    std::span<const Vertex> vertices
) -> std::vector<uint32_t> {
    std::vector<uint32_t> remap(vertices.size());

    const size_t unique_count = meshopt_generateVertexRemap(
        remap.data(),
        nullptr,              // без исходного индексного буфера
        indices.size(),
        vertices.data(),
        vertices.size(),
        sizeof(Vertex)
    );

    remap.resize(unique_count);
    return remap;
}

auto applyRemap(
    std::span<const Vertex> src_vertices,
    std::span<const uint32_t> src_indices,
    std::span<const uint32_t> remap
) -> std::pair<std::vector<Vertex>, std::vector<uint32_t>> {
    std::vector<Vertex> vertices(remap.size());
    std::vector<uint32_t> indices(src_indices.size());

    meshopt_remapVertexBuffer(
        vertices.data(),
        src_vertices.data(),
        src_vertices.size(),
        sizeof(Vertex),
        remap.data()
    );

    meshopt_remapIndexBuffer(
        indices.data(),
        src_indices.data(),
        src_indices.size(),
        remap.data()
    );

    return {std::move(vertices), std::move(indices)};
}
```

### Vertex Cache Optimization

Оптимизирует порядок треугольников для максимального переиспользования результатов vertex shader.

```cpp
auto optimizeForCache(
    std::span<uint32_t> indices,
    size_t vertex_count
) -> void {
    meshopt_optimizeVertexCache(
        indices.data(),
        indices.data(),
        indices.size(),
        vertex_count
    );
}
```

> **Для понимания:** Представь конвейер на фабрике. Рабочий A обрабатывает деталь и кладёт на конвейер. Если следующему
> рабочему нужна та же деталь — она уже готова. Но если деталь увезли на склад и пришлось делать заново — это потеря
> времени. Vertex Cache работает так же: результаты трансформации вершин кэшируются, и если вершина нужна снова — она не
> пересчитывается.

**FIFO версия** — быстрее, но хуже результат:

```cpp
// ~2x быстрее, немного хуже ACMR
meshopt_optimizeVertexCacheFifo(
    indices.data(),
    indices.data(),
    indices.size(),
    vertex_count,
    16  // размер кэша
);
```

### Overdraw Optimization

Переупорядочивает треугольники для минимизации перерисовки пикселей.

```cpp
auto optimizeOverdraw(
    std::span<uint32_t> indices,
    std::span<const Vertex> vertices,
    float threshold = 1.05f
) -> void {
    meshopt_optimizeOverdraw(
        indices.data(),
        indices.data(),
        indices.size(),
        &vertices[0].px,
        vertices.size(),
        sizeof(Vertex),
        threshold
    );
}
```

> **Для понимания:** Представь, что несколько человек красят одну стену. Один красит всю стену целиком, второй подходит
> и перекрашивает уже окрашенные участки. Это перерасход краски (overdraw). Оптимизация заставляет ближние к камере
> треугольники рисоваться первыми — так дальние перекрываются без лишней работы.

**threshold** контролирует компромисс между overdraw и vertex cache:

- `1.0` — не жертвовать ACMR
- `1.05` — допустить 5% ухудшение ACMR
- `>1.5` — агрессивная оптимизация

### Vertex Fetch Optimization

Оптимизирует расположение вершин в памяти для эффективной загрузки.

```cpp
auto optimizeFetch(
    std::span<Vertex> vertices,
    std::span<uint32_t> indices
) -> void {
    meshopt_optimizeVertexFetch(
        vertices.data(),
        indices.data(),
        indices.size(),
        vertices.data(),
        vertices.size(),
        sizeof(Vertex)
    );
}
```

> **Для понимание:** Если ты идёшь по библиотеке и берёшь книги в порядке их расположения на полках — ты быстро соберёшь
> нужные. Если скачешь туда-сюда — потратишь много времени. GPU загружает вершины пачками, и если вершины идут подряд —
> загрузка одна. Если далеко — приходится загружать снова.

---

## Meshlets

Meshlet — небольшой кластер треугольников (обычно 64–126) для современного mesh shading.

```cpp
struct MeshletData {
    std::vector<meshopt_Meshlet> meshlets;
    std::vector<uint32_t> meshlet_vertices;
    std::vector<uint8_t> meshlet_triangles;
    std::vector<meshopt_Bounds> bounds;
};

auto buildMeshlets(
    std::span<const Vertex> vertices,
    std::span<const uint32_t> indices,
    size_t max_vertices = 64,
    size_t max_triangles = 126,
    float cone_weight = 0.0f
) -> MeshletData {
    MeshletData result;

    // Оценка максимального количества meshlets
    const size_t max_meshlets = meshopt_buildMeshletsBound(
        indices.size(), max_vertices, max_triangles
    );

    result.meshlets.resize(max_meshlets);
    result.meshlet_vertices.resize(max_meshlets * max_vertices);
    result.meshlet_triangles.resize(max_meshlets * max_triangles * 3);

    // Построение meshlets
    const size_t meshlet_count = meshopt_buildMeshlets(
        result.meshlets.data(),
        result.meshlet_vertices.data(),
        result.meshlet_triangles.data(),
        indices.data(),
        indices.size(),
        &vertices[0].px,
        vertices.size(),
        sizeof(Vertex),
        max_vertices,
        max_triangles,
        cone_weight
    );

    result.meshlets.resize(meshlet_count);

    // Обрезаем массивы до реального размера
    const auto& last = result.meshlets.back();
    result.meshlet_vertices.resize(last.vertex_offset + last.vertex_count);
    result.meshlet_triangles.resize(
        last.triangle_offset + last.triangle_count * 3
    );

    // Оптимизация каждого meshlet
    for (auto& m : result.meshlets) {
        meshopt_optimizeMeshlet(
            &result.meshlet_vertices[m.vertex_offset],
            &result.meshlet_triangles[m.triangle_offset],
            m.triangle_count,
            m.vertex_count
        );
    }

    // Вычисление bounds для frustum/cone culling
    result.bounds.resize(meshlet_count);
    for (size_t i = 0; i < meshlet_count; ++i) {
        const auto& m = result.meshlets[i];
        result.bounds[i] = meshopt_computeMeshletBounds(
            &result.meshlet_vertices[m.vertex_offset],
            &result.meshlet_triangles[m.triangle_offset],
            m.triangle_count,
            &vertices[0].px,
            vertices.size(),
            sizeof(Vertex)
        );
    }

    return result;
}
```

> **Для понимания:** Бригада из 10 строителей может построить дом быстрее, чем один строитель. Но если бригада слишком
> большая — они начинают мешать друг другу. Meshlet — это оптимальный размер "бригады" для GPU: достаточно большой для
> эффективности, но не настолько большой, чтобы вызывать проблемы с памятью и синхронизацией.

### Рекомендуемые параметры

| GPU       | max_vertices | max_triangles |
|-----------|--------------|---------------|
| NVIDIA    | 64           | 126           |
| AMD       | 64–128       | 64–128        |
| Universal | 64           | 96            |

### Raytracing Meshlets

Для raytracing используется пространственное разбиение вместо топологического:

```cpp
auto buildRaytracingMeshlets(
    std::span<const Vertex> vertices,
    std::span<const uint32_t> indices
) -> MeshletData {
    const size_t max_vertices = 64;
    const size_t min_triangles = 16;
    const size_t max_triangles = 64;
    const float fill_weight = 0.5f;

    MeshletData result;
    const size_t max_meshlets = meshopt_buildMeshletsBound(
        indices.size(), max_vertices, min_triangles
    );

    result.meshlets.resize(max_meshlets);
    result.meshlet_vertices.resize(max_meshlets * max_vertices);
    result.meshlet_triangles.resize(max_meshlets * max_triangles * 3);

    const size_t count = meshopt_buildMeshletsSpatial(
        result.meshlets.data(),
        result.meshlet_vertices.data(),
        result.meshlet_triangles.data(),
        indices.data(),
        indices.size(),
        &vertices[0].px,
        vertices.size(),
        sizeof(Vertex),
        max_vertices,
        min_triangles,
        max_triangles,
        fill_weight
    );

    result.meshlets.resize(count);
    // ... вычисление bounds аналогично

    return result;
}
```

---

## Simplification (LOD)

Упрощение меша с контролем ошибки через Quadric Error Metrics.

```cpp
struct SimplificationResult {
    std::vector<uint32_t> indices;
    float result_error;
};

auto simplify(
    std::span<const Vertex> vertices,
    std::span<const uint32_t> indices,
    size_t target_index_count,
    float target_error = 1e-2f,
    int options = 0
) -> SimplificationResult {
    SimplificationResult result;
    result.indices.resize(indices.size());

    const size_t simplified_count = meshopt_simplify(
        result.indices.data(),
        indices.data(),
        indices.size(),
        &vertices[0].px,
        vertices.size(),
        sizeof(Vertex),
        target_index_count,
        target_error,
        options,
        &result.result_error
    );

    result.indices.resize(simplified_count);
    return result;
}
```

### Attribute-aware упрощение

Учитывает нормали и UV при упрощении:

```cpp
auto simplifyWithAttributes(
    std::span<const Vertex> vertices,
    std::span<const uint32_t> indices,
    size_t target_count,
    float target_error,
    std::span<const float> attribute_weights
) -> SimplificationResult {
    SimplificationResult result;
    result.indices.resize(indices.size());

    const size_t count = meshopt_simplifyWithAttributes(
        result.indices.data(),
        indices.data(),
        indices.size(),
        &vertices[0].px,
        vertices.size(),
        sizeof(Vertex),
        &vertices[0].nx,           // normals
        sizeof(Vertex),
        attribute_weights.data(),   // {1.0f, 1.0f, 1.0f} для normals
        3,                         // 3 компоненты
        nullptr,                   // vertex_lock
        target_count,
        target_error,
        0,
        &result.result_error
    );

    result.indices.resize(count);
    return result;
}
```

### Опции упрощения

| Опция                           | Описание                                |
|---------------------------------|-----------------------------------------|
| `meshopt_SimplifyLockBorder`    | Не коллапсировать граничные рёбра       |
| `meshopt_SimplifyErrorAbsolute` | Использовать абсолютную ошибку          |
| `meshopt_SimplifySparse`        | Оптимизация для разреженных подмножеств |
| `meshopt_SimplifyPrune`         | Удалять изолированные компоненты        |
| `meshopt_SimplifyRegularize`    | Более регулярные треугольники           |
| `meshopt_SimplifyPermissive`    | Разрешить коллапс через seams           |

> **Для понимания:** QEM работает как оценка "качества потерь". Представь, что ты упрощаешь карту мира. Остров может
> быть убран без большой потери информации (маленькая ошибка), а вот удаление целого континента — это большая ошибка.
> QEM
> вычисляет "стоимость" каждого упрощения и выбирает наименьшую.

---

## Compression

### Vertex Compression

```cpp
auto encodeVertexBuffer(
    std::span<const Vertex> vertices
) -> std::vector<uint8_t> {
    const size_t bound = meshopt_encodeVertexBufferBound(
        vertices.size(),
        sizeof(Vertex)
    );

    std::vector<uint8_t> encoded(bound);
    const size_t actual = meshopt_encodeVertexBuffer(
        encoded.data(),
        bound,
        vertices.data(),
        vertices.size(),
        sizeof(Vertex)
    );

    encoded.resize(actual);
    return encoded;
}

auto decodeVertexBuffer(
    std::span<const uint8_t> encoded,
    size_t vertex_count,
    size_t vertex_size
) -> std::vector<Vertex> {
    std::vector<Vertex> vertices(vertex_count);

    const int result = meshopt_decodeVertexBuffer(
        vertices.data(),
        vertex_count,
        vertex_size,
        encoded.data(),
        encoded.size()
    );

    if (result != 0) {
        throw std::runtime_error("Decode failed: " + std::to_string(result));
    }

    return vertices;
}
```

### Index Compression

```cpp
auto encodeIndexBuffer(
    std::span<const uint32_t> indices,
    size_t vertex_count
) -> std::vector<uint8_t> {
    const size_t bound = meshopt_encodeIndexBufferBound(
        indices.size(),
        vertex_count
    );

    std::vector<uint8_t> encoded(bound);
    const size_t actual = meshopt_encodeIndexBuffer(
        encoded.data(),
        bound,
        indices.data(),
        indices.size()
    );

    encoded.resize(actual);
    return encoded;
}
```

### Квантизация

```cpp
auto quantizeVertex(Vertex v) -> std::array<uint16_t, 3> {
    return {
        meshopt_quantizeHalf(v.px),
        meshopt_quantizeHalf(v.py),
        meshopt_quantizeHalf(v.pz)
    };
}

auto quantizeNormal(Vertex v) -> uint32_t {
    return
        ((meshopt_quantizeSnorm(v.nx, 10) & 1023) << 20) |
        ((meshopt_quantizeSnorm(v.ny, 10) & 1023) << 10) |
        (meshopt_quantizeSnorm(v.nz, 10) & 1023);
}
```

---

## Метрики

### ACMR (Average Cache Miss Ratio)

```
ACMR = vertex_shader_invocations / triangle_count
```

| Значение | Оценка                        |
|----------|-------------------------------|
| 0.5      | Идеально (регулярная сетка)   |
| 1.0      | Хорошо                        |
| 3.0      | Плохо (нет переиспользования) |

### ATVR (Average Transformed Vertex Ratio)

```
ATVR = vertex_shader_invocations / vertex_count
```

| Значение | Оценка                               |
|----------|--------------------------------------|
| 1.0      | Идеально                             |
| >1.0     | Вершины трансформируются многократно |

### Overdraw

```
Overdraw = pixels_shaded / pixels_covered
```

| Значение | Оценка                             |
|----------|------------------------------------|
| 1.0      | Идеально                           |
| 1.5–2.5  | Типично для непрозрачной геометрии |

### Анализ

```cpp
auto analyzeMesh(
    std::span<const uint32_t> indices,
    std::span<const Vertex> vertices
) -> void {
    const auto vc_stats = meshopt_analyzeVertexCache(
        indices.data(),
        indices.size(),
        vertices.size(),
        16,  // cache size
        0,   // transform cache size
        0    // vertex buffer size
    );

    const auto od_stats = meshopt_analyzeOverdraw(
        indices.data(),
        indices.size(),
        &vertices[0].px,
        vertices.size(),
        sizeof(Vertex)
    );

    const auto vf_stats = meshopt_analyzeVertexFetch(
        indices.data(),
        indices.size(),
        vertices.size(),
        sizeof(Vertex)
    );

    std::println("ACMR: {:.3f}, ATVR: {:.3f}", vc_stats.acmr, vc_stats.atvr);
    std::println("Overdraw: {:.2f}", od_stats.overdraw);
    std::println("Overfetch: {:.2f}", vf_stats.overfetch);
}
```

---

## Ключевые структуры

| Структура                       | Описание                                                              |
|---------------------------------|-----------------------------------------------------------------------|
| `meshopt_Stream`                | Поток вершинных атрибутов                                             |
| `meshopt_Meshlet`               | Кластер: vertex_offset, triangle_offset, vertex_count, triangle_count |
| `meshopt_Bounds`                | Bounding sphere + normal cone для culling                             |
| `meshopt_VertexCacheStatistics` | ACMR, ATVR метрики                                                    |
| `meshopt_OverdrawStatistics`    | overdraw ratio                                                        |
| `meshopt_VertexFetchStatistics` | overfetch ratio                                                       |

---

## Spatial Sorting

Сортировка по пространственному положению для улучшения сжатия:

```cpp
auto spatialSort(
    std::span<Vertex> vertices,
    std::span<uint32_t> indices
) -> void {
    std::vector<uint32_t> remap(vertices.size());
    meshopt_spatialSortRemap(
        remap.data(),
        &vertices[0].px,
        vertices.size(),
        sizeof(Vertex)
    );

    meshopt_remapVertexBuffer(
        vertices.data(),
        vertices.data(),
        vertices.size(),
        sizeof(Vertex),
        remap.data()
    );

    meshopt_spatialSortTriangles(
        indices.data(),
        indices.data(),
        indices.size(),
        &vertices[0].px,
        vertices.size(),
        sizeof(Vertex)
    );
}
```

---

## Полный пример Pipeline

```cpp
#include <meshoptimizer.h>
#include <vector>
#include <span>
#include <array>
#include <print>

struct OptimizedMesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    float acmr = 0.0f;
    float atvr = 0.0f;
};

auto optimizeMesh(
    std::span<const Vertex> src_vertices,
    std::span<const uint32_t> src_indices
) -> OptimizedMesh {
    OptimizedMesh result;

    // 1. Генерация remap таблицы
    std::vector<uint32_t> remap(src_vertices.size());
    const size_t unique_count = meshopt_generateVertexRemap(
        remap.data(),
        src_indices.data(),
        src_indices.size(),
        src_vertices.data(),
        src_vertices.size(),
        sizeof(Vertex)
    );

    // 2. Применение remap
    result.indices.resize(src_indices.size());
    result.vertices.resize(unique_count);

    meshopt_remapIndexBuffer(
        result.indices.data(),
        src_indices.data(),
        src_indices.size(),
        remap.data()
    );

    meshopt_remapVertexBuffer(
        result.vertices.data(),
        src_vertices.data(),
        src_vertices.size(),
        sizeof(Vertex),
        remap.data()
    );

    // 3. Vertex cache optimization
    meshopt_optimizeVertexCache(
        result.indices.data(),
        result.indices.data(),
        result.indices.size(),
        unique_count
    );

    // 4. Vertex fetch optimization
    meshopt_optimizeVertexFetch(
        result.vertices.data(),
        result.indices.data(),
        result.indices.size(),
        result.vertices.data(),
        unique_count,
        sizeof(Vertex)
    );

    // 5. Анализ результата
    const auto stats = meshopt_analyzeVertexCache(
        result.indices.data(),
        result.indices.size(),
        unique_count,
        16, 0, 0
    );

    result.acmr = stats.acmr;
    result.atvr = stats.atvr;

    return result;
}
```

---

## Лицензия

MIT License. Copyright (c) 2016-2026 Arseny Kapoulkine.

---

## Ссылки

- [GitHub](https://github.com/zeux/meshoptimizer)
- [EXT_meshopt_compression](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Vendor/EXT_meshopt_compression/README.md)
- [gltfpack](https://github.com/zeux/meshoptimizer/tree/master/gltf) — инструмент для оптимизации glTF

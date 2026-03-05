# Быстрый старт

🟢 **Уровень 1: Базовый**

Полный pipeline оптимизации меши: indexing → vertex cache → overdraw → vertex fetch.

---

## Минимальный пример

```cpp
#include <meshoptimizer.h>
#include <vector>
#include <cstdio>

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float tu, tv;
};

int main() {
    // Исходные данные (неоптимизированный меш)
    std::vector<Vertex> vertices = loadMesh();
    std::vector<unsigned int> indices = loadIndices();

    size_t index_count = indices.size();
    size_t vertex_count = vertices.size();

    // 1. Indexing: удаление дублирующихся вершин
    std::vector<unsigned int> remap(vertex_count);
    size_t unique_vertex_count = meshopt_generateVertexRemap(
        remap.data(),
        indices.data(),
        index_count,
        vertices.data(),
        vertex_count,
        sizeof(Vertex)
    );

    // Применяем remap
    std::vector<unsigned int> optimized_indices(index_count);
    meshopt_remapIndexBuffer(optimized_indices.data(), indices.data(), index_count, remap.data());

    std::vector<Vertex> optimized_vertices(unique_vertex_count);
    meshopt_remapVertexBuffer(optimized_vertices.data(), vertices.data(), vertex_count, sizeof(Vertex), remap.data());

    // Обновляем счётчики
    vertex_count = unique_vertex_count;

    // 2. Vertex Cache Optimization
    meshopt_optimizeVertexCache(optimized_indices.data(), optimized_indices.data(), index_count, vertex_count);

    // 3. Vertex Fetch Optimization
    meshopt_optimizeVertexFetch(optimized_vertices.data(), optimized_indices.data(), index_count,
                                 optimized_vertices.data(), vertex_count, sizeof(Vertex));

    // 4. Анализ результата
    auto stats = meshopt_analyzeVertexCache(optimized_indices.data(), index_count, vertex_count, 16, 0, 0);
    printf("ACMR: %.2f (ideal: 0.5, worst: 3.0)\n", stats.acmr);
    printf("ATVR: %.2f (ideal: 1.0)\n", stats.atvr);

    return 0;
}
```

---

## Шаг 1: Indexing

Удаление дублирующихся вершин на основе бинарного сравнения.

```cpp
// Генерация remap таблицы
std::vector<unsigned int> remap(vertex_count);
size_t unique_count = meshopt_generateVertexRemap(
    remap.data(),           // destination
    indices.data(),         // indices (или NULL для неиндексированного)
    index_count,            // количество индексов
    vertices.data(),        // вершинный буфер
    vertex_count,           // количество вершин
    sizeof(Vertex)          // размер вершины
);

// Применение remap к индексам
std::vector<unsigned int> new_indices(index_count);
meshopt_remapIndexBuffer(new_indices.data(), indices.data(), index_count, remap.data());

// Применение remap к вершинам
std::vector<Vertex> new_vertices(unique_count);
meshopt_remapVertexBuffer(new_vertices.data(), vertices.data(), vertex_count, sizeof(Vertex), remap.data());
```

### Multiple streams

При использовании нескольких вершинных потоков:

```cpp
meshopt_Stream streams[] = {
    {positions.data(), sizeof(float) * 3, sizeof(float) * 3},
    {normals.data(), sizeof(float) * 3, sizeof(float) * 3},
    {uvs.data(), sizeof(float) * 2, sizeof(float) * 2},
};

size_t unique_count = meshopt_generateVertexRemapMulti(
    remap.data(), indices.data(), index_count, vertex_count,
    streams, sizeof(streams) / sizeof(streams[0])
);
```

---

## Шаг 2: Vertex Cache Optimization

Переупорядочивание треугольников для минимизации vertex shader invocations.

```cpp
meshopt_optimizeVertexCache(indices, indices, index_count, vertex_count);
```

### FIFO-версия (быстрее, но менее оптимально)

```cpp
unsigned int cache_size = 16;  // рекомендуется для старых GPU
meshopt_optimizeVertexCacheFifo(indices, indices, index_count, vertex_count, cache_size);
```

### Strip-оптимизация

Если цель — уменьшить размер данных или создать triangle strip:

```cpp
meshopt_optimizeVertexCacheStrip(indices, indices, index_count, vertex_count);
```

---

## Шаг 3: Overdraw Optimization (опционально)

Уменьшение перерисовки пикселей. Требует позиции вершин.

```cpp
float threshold = 1.05f;  // допускаем 5% ухудшение vertex cache
meshopt_optimizeOverdraw(
    indices,                    // destination
    indices,                    // source (уже оптимизированный для vertex cache!)
    index_count,
    &vertices[0].px,            // позиции (float3)
    vertex_count,
    sizeof(Vertex),             // stride
    threshold
);
```

> **Примечание:** Не рекомендуется для tiled deferred rendering (PowerVR, Apple GPU).

---

## Шаг 4: Vertex Fetch Optimization

Переупорядочивание вершин для оптимального доступа к памяти.

```cpp
size_t final_vertex_count = meshopt_optimizeVertexFetch(
    vertices,               // destination
    indices,                // indices (in-out)
    index_count,
    vertices,               // source
    vertex_count,
    sizeof(Vertex)
);
```

### Для multiple streams

```cpp
// Генерация remap вместо прямого переупорядочивания
std::vector<unsigned int> fetch_remap(vertex_count);
meshopt_optimizeVertexFetchRemap(fetch_remap.data(), indices, index_count, vertex_count);

// Применение к каждому потоку
meshopt_remapVertexBuffer(positions, positions, vertex_count, sizeof(float) * 3, fetch_remap.data());
meshopt_remapVertexBuffer(normals, normals, vertex_count, sizeof(float) * 3, fetch_remap.data());
meshopt_remapVertexBuffer(uvs, uvs, vertex_count, sizeof(float) * 2, fetch_remap.data());
meshopt_remapIndexBuffer(indices, indices, index_count, fetch_remap.data());
```

---

## Shadow Index Buffer

Отдельный индексный буфер для depth-only passes (shadow maps, z-prepass).

```cpp
// Использовать только позицию для сравнения вершин
std::vector<unsigned int> shadow_indices(index_count);
meshopt_generateShadowIndexBuffer(
    shadow_indices.data(),
    indices.data(),
    index_count,
    vertices.data(),
    vertex_count,
    sizeof(float) * 3,      // size: только позиция
    sizeof(Vertex)          // stride: полная вершина
);

// Оптимизировать для vertex cache
meshopt_optimizeVertexCache(shadow_indices.data(), shadow_indices.data(), index_count, vertex_count);
```

---

## Анализ производительности

```cpp
// Vertex cache анализ
auto vc_stats = meshopt_analyzeVertexCache(indices, index_count, vertex_count, 16, 32, 128);
printf("ACMR: %.3f, ATVR: %.3f\n", vc_stats.acmr, vc_stats.atvr);

// Overdraw анализ
auto od_stats = meshopt_analyzeOverdraw(indices, index_count, &vertices[0].px, vertex_count, sizeof(Vertex));
printf("Overdraw: %.2f\n", od_stats.overdraw);

// Vertex fetch анализ
auto vf_stats = meshopt_analyzeVertexFetch(indices, index_count, vertex_count, sizeof(Vertex));
printf("Overfetch: %.2f\n", vf_stats.overfetch);
```

---

## Quantization

### Half-precision floats

```cpp
unsigned short px = meshopt_quantizeHalf(vertex.px);
float restored = meshopt_dequantizeHalf(px);
```

### Normalized integers

```cpp
// SNORM [-1, 1] → 10 бит
int nx = meshopt_quantizeSnorm(vertex.nx, 10);

// UNORM [0, 1] → 8 бит
int alpha = meshopt_quantizeUnorm(vertex.alpha, 8);
```

### 10-10-10-2 SNORM normal

```cpp
unsigned int packed_normal =
    ((meshopt_quantizeSnorm(n.x, 10) & 0x3FF) << 20) |
    ((meshopt_quantizeSnorm(n.y, 10) & 0x3FF) << 10) |
    ((meshopt_quantizeSnorm(n.z, 10) & 0x3FF));
```

---

## Типичные ошибки

| Ошибка                                | Причина               | Решение                                               |
|---------------------------------------|-----------------------|-------------------------------------------------------|
| Высокий ACMR после оптимизации        | Меш уже оптимизирован | Проверьте `meshopt_analyzeVertexCache` до оптимизации |
| Падение качества overdraw оптимизации | Большой threshold     | Уменьшите threshold до 1.01–1.05                      |
| Medленная оптимизация                 | Очень большие меши    | Используйте `meshopt_optimizeVertexCacheFifo`         |
| Плохое сжатие                         | Меш не оптимизирован  | Сначала vertex cache + vertex fetch                   |

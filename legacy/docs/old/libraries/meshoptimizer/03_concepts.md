# Концепции

🟡 **Уровень 2: Средний**

Глубокое погружение в алгоритмы meshoptimizer: vertex cache, overdraw, meshlets, simplification.

---

## Vertex Cache Optimization

### Как работает GPU vertex cache

GPU кэширует результаты vertex shader для повторного использования. Когда треугольник ссылается на вершину, которая уже
была обработана, shader не вызывается повторно.

```
Треугольники: [0,1,2] [2,1,3] [3,1,4]
              ↓       ↓       ↓
Вершины:      0,1,2   3       4
              (новые) (1,2 в кэше)
```

### Post-transform cache

Исторически GPU использовали FIFO-кэш фиксированного размера (16–32 вершины). Современные GPU используют более сложные
схемы, но принцип тот же: локальность ссылок на вершины важна.

### ACMR (Average Cache Miss Ratio)

```
ACMR = vertex_shader_invocations / triangle_count
```

- **0.5** — идеально (каждая вершина используется в 2 треугольниках, регулярная сетка)
- **1.0** — хорошо (каждый треугольник добавляет 1 новую вершину)
- **3.0** — худший случай (нет переиспользования вершин)

### ATVR (Average Transformed Vertex Ratio)

```
ATVR = vertex_shader_invocations / vertex_count
```

- **1.0** — идеально (каждая вершина трансформируется один раз)
- **>1.0** — вершины трансформируются многократно

### Алгоритм meshoptimizer

Использует адаптивный алгоритм, который работает хорошо на разных GPU архитектурах:

1. Анализирует топологию меши
2. Строит последовательность треугольников с максимальной локальностью
3. Учитывает различные размеры и типы кэшей

---

## Overdraw Optimization

### Что такое overdraw

Когда несколько треугольников перекрывают один пиксель, pixel shader выполняется для каждого. Это wasteful, если ранние
треугольники полностью перекрыты поздними.

```
Пиксель (100, 100):
  - Треугольник A (дальний) → pixel shader
  - Треугольник B (ближний) → pixel shader
  - Overdraw = 2.0
```

### Depth pre-pass

Один из способов борьбы с overdraw — рендерить только depth в первом проходе. Но это удваивает vertex shader
invocations.

### Overdraw optimization в meshoptimizer

Переупорядочивает треугольники так, чтобы ближние рисовались раньше дальних (в среднем):

```cpp
// Вход: индексы, оптимизированные для vertex cache
// Выход: индексы, сбалансированные между vcache и overdraw
meshopt_optimizeOverdraw(indices, indices, index_count,
    positions, vertex_count, stride, threshold);
```

### Параметр threshold

Контролирует, насколько можно ухудшить ACMR ради overdraw:

- **1.0** — не ухудшать ACMR
- **1.05** — допустить 5% ухудшение ACMR
- **2.0** — агрессивная оптимизация overdraw

### Когда НЕ использовать

- Tiled deferred rendering (PowerVR, Apple GPU)
- Меши с большим количеством вершин (vertex shader тяжёлый)
- Уже отсортированные меши (front-to-back)

---

## Vertex Fetch Optimization

### Кэш памяти GPU

При обработке вершины GPU загружает её атрибуты из памяти. Данные кэшируются, но только в пределах локальности.

### Переупорядочивание вершин

```
До:  индексы [0, 100, 200, 1, 101, 201, ...]
     вершины в памяти: v0, v1, v2, ..., v100, v101, ...

После: индексы [0, 1, 2, 3, 4, 5, ...]
      вершины в памяти: v0, v100, v200, v1, v101, ...
```

Алгоритм помещает вершины в память в порядке использования треугольниками.

### Overfetch

```
Overfetch = bytes_fetched / vertex_buffer_size
```

- **1.0** — идеально (каждый байт загружен один раз)
- **>1.0** — байты загружаются многократно

---

## Meshlets

### Что такое meshlet

Meshlet — небольшой кластер треугольников (обычно 64–126 треугольников), который можно обработать одной группой потоков
на GPU.

```
Меш → Meshlets [0..N]
         │
         ├── meshlet_vertices: индексы вершин меши
         └── meshlet_triangles: micro-indices (0–255)
```

### Структура meshopt_Meshlet

```cpp
struct meshopt_Meshlet {
    unsigned int vertex_offset;    // смещение в meshlet_vertices
    unsigned int triangle_offset;  // смещение в meshlet_triangles
    unsigned int vertex_count;     // количество уникальных вершин
    unsigned int triangle_count;   // количество треугольников
};
```

### Mesh shading pipeline

```
Task Shader (Amplification Shader)
    │
    ├── Выбор видимых meshlets
    │
    ▼
Mesh Shader
    │
    ├── Загрузка meshlet_vertices
    ├── Загрузка meshlet_triangles
    ├── Вычисление позиций вершин
    └── Отправка треугольников на растеризацию
```

### Cone culling

Каждый meshlet имеет bounding cone:

```cpp
struct meshopt_Bounds {
    float center[3];       // центр bounding sphere
    float radius;          // радиус
    float cone_apex[3];    // вершина normal cone
    float cone_axis[3];    // направление normal cone
    float cone_cutoff;     // cos(half_angle)
};
```

Отброс meshlet-а (backface):

```cpp
// Perspective projection
float d = dot(normalize(cone_apex - camera_position), cone_axis);
if (d >= cone_cutoff) {
    // Все треугольники meshlet-а смотрят от камеры
    cull_meshlet();
}
```

### Рекомендуемые параметры

| GPU       | max_vertices | max_triangles |
|-----------|--------------|---------------|
| NVidia    | 64           | 126           |
| AMD       | 64–128       | 64–128        |
| Universal | 64           | 96            |

---

## Simplification

### Quadric Error Metrics

Алгоритм упрощения использует метрику ошибки квадрик:

1. Для каждой вершины вычисляется quadric matrix из плоскостей прилегающих треугольников
2. Ошибка коллапса ребра — это quadric error
3. Ребро с минимальной ошибкой коллапсирует первым

### Относительная ошибка

```cpp
float target_error = 0.01f;  // 1% от размера меши
size_t result_count = meshopt_simplify(
    destination, indices, index_count,
    positions, vertex_count, stride,
    target_index_count, target_error, 0, &result_error
);
```

Ошибка нормализована к `[0..1]` относительно размеров меши.

### Attribute-aware simplification

```cpp
float weights[] = {1.0f, 1.0f, 1.0f};  // nx, ny, nz
meshopt_simplifyWithAttributes(
    destination, indices, index_count,
    positions, vertex_count, stride,
    normals, stride, weights, 3,
    NULL,  // vertex_lock
    target_index_count, target_error, 0, &result_error
);
```

### Опции simplification

| Флаг                            | Описание                                |
|---------------------------------|-----------------------------------------|
| `meshopt_SimplifyLockBorder`    | Не коллапсировать граничные рёбра       |
| `meshopt_SimplifyErrorAbsolute` | Использовать абсолютную ошибку          |
| `meshopt_SimplifySparse`        | Оптимизация для разреженных подмножеств |
| `meshopt_SimplifyPrune`         | Удалять изолированные компоненты        |
| `meshopt_SimplifyRegularize`    | Более регулярные треугольники           |

---

## Compression

### Vertex Buffer Compression

```cpp
// Кодирование
std::vector<unsigned char> encoded(meshopt_encodeVertexBufferBound(vertex_count, sizeof(Vertex)));
encoded.resize(meshopt_encodeVertexBuffer(encoded.data(), encoded.size(),
    vertices, vertex_count, sizeof(Vertex)));

// Декодирование (3–6 GB/s на CPU)
int result = meshopt_decodeVertexBuffer(decoded, vertex_count, sizeof(Vertex),
    encoded.data(), encoded.size());
```

Кодек использует корреляцию между соседними вершинами для сжатия.

### Index Buffer Compression

```cpp
// Кодирование
std::vector<unsigned char> encoded(meshopt_encodeIndexBufferBound(index_count, vertex_count));
encoded.resize(meshopt_encodeIndexBuffer(encoded.data(), encoded.size(),
    indices, index_count));

// Декодирование
int result = meshopt_decodeIndexBuffer(decoded, index_count, sizeof(unsigned int),
    encoded.data(), encoded.size());
```

Типичное сжатие: ~1 байт на треугольник (vs 12 байт для 32-bit indices).

### Требования к данным

Для хорошего сжатия:

1. Vertex cache оптимизация (`meshopt_optimizeVertexCache`)
2. Vertex fetch оптимизация (`meshopt_optimizeVertexFetch`)
3. Квантизация вершин

---

## Spatial Sorting

### Зачем сортировать

Для улучшения сжатия и cache locality:

```cpp
// Сортировка вершин
std::vector<unsigned int> remap(vertex_count);
meshopt_spatialSortRemap(remap.data(), positions, vertex_count, stride);
meshopt_remapVertexBuffer(vertices, vertices, vertex_count, sizeof(Vertex), remap.data());

// Сортировка треугольников
meshopt_spatialSortTriangles(indices, indices, index_count, positions, vertex_count, stride);
```

### Применение

- Point cloud compression
- Улучшение сжатия vertex buffer
- LOD generation

## meshoptimizer

<!-- anchor: 00_overview -->

🟢 **Уровень 1: Базовый**

meshoptimizer — библиотека для оптимизации 3D-мешей, которая улучшает производительность GPU-рендеринга за счёт
оптимизации порядка вершин и индексов, упрощения геометрии и сжатия данных.

---

## Возможности

### Оптимизация рендеринга

- **Vertex Cache Optimization** — минимизация vertex shader invocations
- **Overdraw Optimization** — уменьшение перерисовки пикселей
- **Vertex Fetch Optimization** — оптимизация доступа к вершинным данным

### Работа с геометрией

- **Indexing** — генерация индексных буферов, удаление дублирующихся вершин
- **Simplification** — упрощение мешей с сохранением внешнего вида
- **Meshlets** — разбиение меши на кластеры для mesh shading

### Сжатие

- **Vertex Compression** — кодирование вершинных данных
- **Index Compression** — кодирование индексных буферов
- **Meshlet Compression** — сжатие данных meshlet-ов

---

## Pipeline оптимизации

Правильный порядок оптимизации меши:

```
1. Indexing
   meshopt_generateVertexRemap → meshopt_remapIndexBuffer → meshopt_remapVertexBuffer
         │
         ▼
2. Vertex Cache Optimization
   meshopt_optimizeVertexCache
         │
         ▼
3. Overdraw Optimization (опционально)
   meshopt_optimizeOverdraw
         │
         ▼
4. Vertex Fetch Optimization
   meshopt_optimizeVertexFetch
         │
         ▼
5. Vertex Quantization
   meshopt_quantizeHalf, meshopt_quantizeSnorm, ...
         │
         ▼
6. Compression (опционально)
   meshopt_encodeVertexBuffer, meshopt_encodeIndexBuffer
```

---

## Ключевые структуры

| Структура                       | Описание                                                              |
|---------------------------------|-----------------------------------------------------------------------|
| `meshopt_Stream`                | Поток вершинных атрибутов (data, size, stride)                        |
| `meshopt_Meshlet`               | Кластер: vertex_offset, triangle_offset, vertex_count, triangle_count |
| `meshopt_Bounds`                | Bounding sphere + normal cone для culling                             |
| `meshopt_VertexCacheStatistics` | ACMR, ATVR метрики                                                    |
| `meshopt_OverdrawStatistics`    | Overdraw ratio                                                        |
| `meshopt_VertexFetchStatistics` | Overfetch ratio                                                       |

---

## Метрики производительности

### ACMR (Average Cache Miss Ratio)

Среднее количество vertex shader invocations на треугольник:

- Идеально: 0.5 (каждая вершина используется в 2 треугольниках)
- Плохо: 3.0 (нет переиспользования вершин)
- Типично после оптимизации: 0.6–0.8

### ATVR (Average Transformed Vertex Ratio)

Среднее количество vertex shader invocations на вершину:

- Идеально: 1.0 (каждая вершина трансформируется один раз)
- Плохо: 6.0
- Типично после оптимизации: 1.0–1.2

### Overdraw

Отношение затенённых пикселей к покрытым:

- Идеально: 1.0 (каждый пиксель шейдится один раз)
- Типично для непрозрачной геометрии: 1.5–2.5

---

## Требования

- **C99** / **C++98** или новее
- Нет внешних зависимостей
- SIMD-оптимизации: SSE4.1, AVX, AVX2, NEON, WASM SIMD (опционально)

---

## Лицензия

MIT License. Copyright (c) 2016-2026 Arseny Kapoulkine.

---

## Ссылки

- **Исходный код:** [github.com/zeux/meshoptimizer](https://github.com/zeux/meshoptimizer)
- **glTF extension:
  ** [EXT_meshopt_compression](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Vendor/EXT_meshopt_compression/README.md)
- **gltfpack:** Инструмент для оптимизации glTF файлов

---

## Концепции

<!-- anchor: 03_concepts -->

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

---

## Глоссарий

<!-- anchor: 06_glossary -->

🟢 **Уровень 1: Базовый**

Термины и определения, используемые в meshoptimizer.

---

## A

### ACMR (Average Cache Miss Ratio)

Среднее количество vertex shader invocations на треугольник. Метрика эффективности vertex cache.

```
ACMR = vertex_shader_invocations / triangle_count
```

Диапазон: 0.5 (идеально) — 3.0 (худший случай).

### ATVR (Average Transformed Vertex Ratio)

Среднее количество vertex shader invocations на вершину. Метрика повторной обработки вершин.

```
ATVR = vertex_shader_invocations / vertex_count
```

Диапазон: 1.0 (идеально) — 6.0 (худший случай).

---

## B

### Bounding Cone

Конус, описывающий диапазон нормалей группы треугольников. Используется для backface culling meshlets.

### Bounding Sphere

Сфера, описывающая вокруг группы вершин. Используется для frustum и occlusion culling.

---

## C

### Cache Locality

Свойство данных, при котором близкие по времени обращения адреса находятся близко в памяти. Важно для производительности
GPU.

### Cluster

Небольшая группа треугольников (обычно 64–126). См. Meshlet.

### Cone Culling

Техника отбрасывания meshlets, все треугольники которых смотрят от камеры. Основана на bounding cone нормалей.

### Cone Weight

Параметр (0–1), контролирующий баланс между размером meshlet и эффективностью cone culling при построении meshlets.

---

## D

### DCC (Digital Content Creation)

Программы для создания 3D-контента: Blender, Maya, 3ds Max.

### Decoding

Процесс восстановления сжатых данных в исходный формат. Декодеры meshoptimizer работают на скорости 3–10 GB/s.

### Dedicated Allocation

Выделение отдельного блока VkDeviceMemory для одного ресурса. VMA автоматически выбирает этот режим для больших
ресурсов.

### Deinterleaved

Способ хранения вершинных данных, при котором каждый атрибут находится в отдельном буфере. См. SoA.

---

## E

### Encoding

Процесс сжатия данных для хранения или передачи. Кодировщики meshoptimizer требуют предварительной оптимизации данных.

### EXT_meshopt_compression

glTF расширение для сжатия геометрии с помощью meshoptimizer. Поддерживается большинством glTF-загрузчиков.

---

## F

### FIFO Cache

First-In-First-Out кэш с фиксированным размером. Простейшая модель vertex cache.

### Frustum Culling

Отбрасывание объектов вне поля зрения камеры.

---

## G

### gltfpack

Инструмент командной строки для оптимизации glTF файлов. Входит в состав meshoptimizer.

---

## I

### Index Buffer

Массив индексов, определяющих порядок использования вершин для формирования треугольников.

### Indexing

Процесс создания индексного буфера из неиндексированных вершин с удалением дубликатов.

---

## L

### LOD (Level of Detail)

Техника использования разных версий меши на разных расстояниях от камеры для оптимизации производительности.

### LOD Chain

Последовательность LOD-ов от самого детального до самого грубого.

---

## M

### Mesh Shader

Программируемый шейдер для обработки кластеров треугольников. Современная альтернатива vertex + geometry shaders.

### Meshlet

Небольшой кластер треугольников (обычно 64–126) для обработки в mesh shader. Содержит:

- meshlet_vertices — индексы вершин меши
- meshlet_triangles — micro-indices для треугольников

### Micro-index

8-битный индекс внутри meshlet, ссылающийся на локальную вершину meshlet-а.

---

## N

### Normal Cone

Конус, описывающий разброс нормалей в meshlet. Используется для определения, смотрит ли весь meshlet от камеры.

---

## O

### Occlusion Culling

Отбрасывание объектов, перекрытых другими объектами ближе к камере.

### Overdraw

Отношение количества затенённых пикселей к покрытым. Показывает, сколько раз в среднем шейдится каждый пиксель.

```
Overdraw = pixels_shaded / pixels_covered
```

### Overfetch

Отношение количества загруженных байт к размеру вершинного буфера. Показывает неэффективность vertex fetch.

```
Overfetch = bytes_fetched / vertex_buffer_size
```

---

## P

### Post-transform Cache

Кэш результатов vertex shader на GPU. Позволяет переиспользовать уже трансформированные вершины.

### Primitive Restart

Специальное значение индекса (обычно 0xffff или 0xffffffff), обозначающее разрыв в triangle strip.

### Provoking Vertex

Вершина треугольника, которая определяет атрибуты для всего треугольника (например, flat shading).

---

## Q

### Quadric Error Metrics

Метод оценки ошибки при упрощении мешей. Основан на сумме квадратов расстояний до плоскостей прилегающих треугольников.

### Quantization

Преобразование данных с уменьшением точности для экономии памяти.

---

## R

### Remap Table

Массив, сопоставляющий старые индексы вершин новым. Используется при indexing.

---

## S

### SAH (Surface Area Heuristic)

Эвристика для оптимизации пространственного разбиения. Используется в `meshopt_buildMeshletsSpatial` для
raytracing-оптимизированных meshlets.

### Shadow Index Buffer

Отдельный индексный буфер для depth-only passes (shadow maps, z-prepass). Использует меньше атрибутов для сравнения
вершин.

### Simplification

Процесс уменьшения количества треугольников в меше с сохранением внешнего вида.

### SoA (Structure of Arrays)

Способ организации данных, при котором каждый компонент хранится в отдельном массиве. Контраст с AoS (Array of
Structures).

### Spatial Sort

Сортировка данных по пространственному положению для улучшения cache locality и сжатия.

### Stripify

Преобразование triangle list в triangle strip для уменьшения размера индексного буфера.

---

## T

### Task Shader (Amplification Shader)

Шейдер, запускаемый перед mesh shader для определения, какие meshlets нужно обрабатывать.

### Triangle List

Примитивная топология, где каждые 3 индекса образуют треугольник. Наиболее универсальный формат.

### Triangle Strip

Примитивная топология, где каждый новый индекс образует треугольник с двумя предыдущими.

---

## U

### Unindexed Mesh

Меш без индексного буфера. Вершины дублируются для каждого треугольника.

---

## V

### Vertex Cache

Кэш на GPU для хранения результатов vertex shader. См. Post-transform Cache.

### Vertex Fetch

Процесс загрузки вершинных атрибутов из памяти GPU.

### Vertex Shader

Шейдер, выполняющийся для каждой вершины. Вычисляет позицию и другие атрибуты.

---

## W

### Warp

Группа потоков, выполняющихся параллельно на GPU. 32 потока на NVidia (SIMT), 64 на AMD (wavefront).

---

## Числовые параметры

### max_vertices (meshlet)

Максимальное количество уникальных вершин в meshlet. Ограничено hardware (обычно ≤ 256).

### max_triangles (meshlet)

Максимальное количество треугольников в meshlet. Ограничено hardware (обычно ≤ 512).

### target_error (simplification)

Допустимая ошибка упрощения, нормализованная к размерам меши (0.01 = 1%).

### threshold (overdraw)

Допустимое ухудшение ACMR при overdraw оптимизации (1.05 = до 5%).

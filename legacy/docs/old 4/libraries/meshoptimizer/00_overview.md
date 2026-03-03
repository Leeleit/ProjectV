# meshoptimizer

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

## Документация

| Файл                                           | Уровень | Описание                                                |
|------------------------------------------------|---------|---------------------------------------------------------|
| [01_quickstart.md](01_quickstart.md)           | 🟢      | Полный pipeline оптимизации: indexing → vcache → vfetch |
| [02_installation.md](02_installation.md)       | 🟢      | CMake, header-only, amalgamated build                   |
| [03_concepts.md](03_concepts.md)               | 🟡      | Vertex cache, overdraw, meshlets, simplification        |
| [04_api-reference.md](04_api-reference.md)     | 🟡      | Справочник функций по категориям                        |
| [05_troubleshooting.md](05_troubleshooting.md) | 🟡      | Плохое сжатие, медленная оптимизация                    |
| [06_glossary.md](06_glossary.md)               | 🟢      | ACMR, ATVR, meshlet, SAH, cone culling                  |

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

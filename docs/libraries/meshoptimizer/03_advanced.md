## Устранение неполадок

<!-- anchor: 05_troubleshooting -->


Типичные проблемы при использовании meshoptimizer и их решения.

---

## Проблемы оптимизации

### Высокий ACMR после оптимизации

**Симптомы:** ACMR остаётся > 1.0 после `meshopt_optimizeVertexCache`.

**Причины:**

| Причина                    | Решение                                     |
|----------------------------|---------------------------------------------|
| Меш уже оптимизирован      | Проверьте ACMR до оптимизации               |
| Меш имеет плохую топологию | Треугольники с длинными "хвостами"          |
| Мало вершин                | Маленькие меши не выигрывают от оптимизации |
| Неправильный vertex_count  | Проверьте, что vertex_count корректен       |

**Диагностика:**

```cpp
auto before = meshopt_analyzeVertexCache(indices, index_count, vertex_count, 16, 0, 0);
meshopt_optimizeVertexCache(indices, indices, index_count, vertex_count);
auto after = meshopt_analyzeVertexCache(indices, index_count, vertex_count, 16, 0, 0);

printf("ACMR: %.3f -> %.3f\n", before.acmr, after.acmr);
```

---

### Ухудшение производительности после overdraw оптимизации

**Симптомы:** FPS снизился после `meshopt_optimizeOverdraw`.

**Причины:**

| Причина                   | Решение                                      |
|---------------------------|----------------------------------------------|
| Tiled deferred GPU        | Не используйте overdraw оптимизацию          |
| Тяжёлый vertex shader     | Overdraw оптимизация не стоит ухудшения ACMR |
| Слишком большой threshold | Уменьшите threshold до 1.01–1.02             |

**Когда не использовать overdraw оптимизацию:**

- Apple GPU (PowerVR-подобные)
- Меши со сложными vertex shaders
- Сцены с глубокой иерархией объектов

---

### Медленная оптимизация

**Симптомы:** `meshopt_optimizeVertexCache` работает долго.

**Решения:**

1. **Используйте FIFO-версию:**

```cpp
// Быстрее в ~3 раза, немного хуже результат
meshopt_optimizeVertexCacheFifo(indices, indices, index_count, vertex_count, 16);
```

2. **Разбейте меши на части:**

```cpp
// Оптимизируйте каждую часть отдельно
for (auto& submesh : submeshes) {
    meshopt_optimizeVertexCache(submesh.indices, submesh.indices,
        submesh.index_count, submesh.vertex_count);
}
```

3. **Кэшируйте результаты:**

```cpp
// Сохраняйте оптимизированные меши на диск
saveOptimizedMesh("mesh_optimized.bin", vertices, indices);
```

---

## Проблемы сжатия

### Плохое сжатие vertex buffer

**Симптомы:** Сжатый размер почти равен исходному.

**Причины:**

| Причина                      | Решение                                 |
|------------------------------|-----------------------------------------|
| Нет vertex fetch оптимизации | Выполните `meshopt_optimizeVertexFetch` |
| Нет квантизации              | Квантуйте данные перед сжатием          |
| Неинициализированный padding | Обнулите padding байты                  |
| Случайный порядок вершин     | Выполните spatial sort                  |

**Правильный pipeline:**

```cpp
// 1. Vertex cache
meshopt_optimizeVertexCache(indices, indices, index_count, vertex_count);

// 2. Vertex fetch
meshopt_optimizeVertexFetch(vertices, indices, index_count, vertices, vertex_count, sizeof(Vertex));

// 3. Quantization (опционально)
for (auto& v : vertices) {
    v.px = meshopt_quantizeHalf(v.px);
    // ...
}

// 4. Encode
meshopt_encodeVertexBuffer(buffer, buffer_size, vertices, vertex_count, sizeof(Vertex));
```

---

### Плохое сжатие index buffer

**Симптомы:** Сжатый размер > 2 байт на треугольник.

**Причины:**

| Причина                      | Решение                                 |
|------------------------------|-----------------------------------------|
| Нет vertex cache оптимизации | Выполните `meshopt_optimizeVertexCache` |
| Разреженные индексы          | LOD-ы ссылаются на далёкие вершины      |

**Типичные показатели:**

| Ситуация                     | Размер на треугольник |
|------------------------------|-----------------------|
| Оптимизированный меш         | ~1 байт               |
| Неоптимизированный           | ~2 байта              |
| LOD с разреженными индексами | ~2–4 байта            |

---

### Ошибка декодирования

**Симптомы:** `meshopt_decodeVertexBuffer` возвращает ненулевое значение.

**Причины:**

| Код ошибки | Причина                     |
|------------|-----------------------------|
| -1         | Некорректный заголовок      |
| -2         | Несовпадение размера буфера |
| -3         | Повреждённые данные         |

**Диагностика:**

```cpp
int result = meshopt_decodeVertexBuffer(decoded, vertex_count, vertex_size, encoded, encoded_size);
if (result != 0) {
    printf("Decode error: %d\n", result);
    // Проверить encoded_size, vertex_count, vertex_size
}
```

---

## Проблемы упрощения

### Упрощение останавливается раньше цели

**Симптомы:** `meshopt_simplify` возвращает больше индексов, чем target.

**Причины:**

| Причина                    | Решение                                          |
|----------------------------|--------------------------------------------------|
| Топологические ограничения | Используйте `meshopt_SimplifyLockBorder` = false |
| Маленький target_error     | Увеличьте target_error                           |
| Много attribute seams      | Используйте `meshopt_SimplifyPermissive`         |

---

### Появление дыр в меше

**Симптомы:** После упрощения появляются видимые дыры.

**Решения:**

1. **Используйте attribute-aware simplification:**

```cpp
float weights[] = {1.0f, 1.0f, 1.0f};  // normals
meshopt_simplifyWithAttributes(/* ... */, normals, stride, weights, 3, /* ... */);
```

2. **Заблокируйте границу:**

```cpp
meshopt_simplify(/* ... */, meshopt_SimplifyLockBorder);
```

3. **Уменьшите target_error:**

```cpp
float target_error = 0.005f;  // 0.5% вместо 1%
```

---

### Артефакты на текстурах

**Симптомы:** Текстуры искажаются после упрощения.

**Решение:** Добавьте UV в атрибуты:

```cpp
float weights[] = {1.0f, 1.0f};  // u, v
meshopt_simplifyWithAttributes(/* ... */, uvs, stride, weights, 2, /* ... */);
```

---

## Проблемы meshlets

### Слишком много meshlets

**Симптомы:** Количество meshlets значительно превышает ожидаемое.

**Решения:**

1. **Уменьшите max_vertices/max_triangles:**

```cpp
// Было: 64/126 → много meshlets
// Стало: 64/96 → меньше meshlets
meshopt_buildMeshlets(/* ... */, 64, 96, 0.0f);
```

2. **Увеличьте cone_weight:**

```cpp
// cone_weight = 0.25 для лучшей группировки
meshopt_buildMeshlets(/* ... */, 64, 126, 0.25f);
```

---

### Cone culling не работает

**Симптомы:** Meshlets не отбрасываются при backface culling.

**Диагностика:**

```cpp
auto bounds = meshopt_computeMeshletBounds(/* ... */);

// Проверить, что cone_cutoff < 1.0
printf("Cone cutoff: %.3f\n", bounds.cone_cutoff);

// Проверить направление
printf("Cone axis: (%.2f, %.2f, %.2f)\n",
    bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2]);
```

**Типичные ошибки:**

| Проблема                  | Решение                                         |
|---------------------------|-------------------------------------------------|
| cone_cutoff == 1.0        | Meshlet имеет треугольники во всех направлениях |
| Отрицательный cone_cutoff | Проверьте winding order вершин                  |

---

## Проблемы памяти

### Высокое потребление памяти

**Симптомы:** Большое потребление при оптимизации больших мешей.

**Решения:**

1. **Обрабатывайте по частям:**

```cpp
for (size_t i = 0; i < mesh_count; ++i) {
    optimizeMesh(meshes[i]);  // Освобождает память после каждой
}
```

2. **Используйте кастомный аллокатор:**

```cpp
meshopt_setAllocator(my_allocate, my_deallocate);
```

---

## Отладка

### Проверка корректности данных

```cpp
bool validateIndices(const unsigned int* indices, size_t index_count, size_t vertex_count) {
    for (size_t i = 0; i < index_count; ++i) {
        if (indices[i] >= vertex_count) {
            printf("Invalid index %u at position %zu (vertex_count = %zu)\n",
                indices[i], i, vertex_count);
            return false;
        }
    }
    return true;
}
```

### Логирование pipeline

```cpp
void logPipelineStats(const std::vector<Vertex>& vertices,
                      const std::vector<unsigned int>& indices) {
    size_t vertex_count = vertices.size();
    size_t index_count = indices.size();

    auto vc = meshopt_analyzeVertexCache(indices.data(), index_count, vertex_count, 16, 0, 0);
    auto od = meshopt_analyzeOverdraw(indices.data(), index_count,
        &vertices[0].px, vertex_count, sizeof(Vertex));
    auto vf = meshopt_analyzeVertexFetch(indices.data(), index_count, vertex_count, sizeof(Vertex));

    printf("Vertices: %zu, Triangles: %zu\n", vertex_count, index_count / 3);
    printf("ACMR: %.3f, ATVR: %.3f\n", vc.acmr, vc.atvr);
    printf("Overdraw: %.2f\n", od.overdraw);
    printf("Overfetch: %.2f\n", vf.overfetch);
}

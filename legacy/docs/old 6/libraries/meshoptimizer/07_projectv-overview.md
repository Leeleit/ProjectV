# meshoptimizer в ProjectV

🔴 **Уровень 3: Продвинутый**

Роль meshoptimizer в воксельном движке ProjectV и области применения.

---

## Области применения

### 1. Загрузка и оптимизация моделей

При загрузке glTF-моделей через fastgltf:

```
glTF файл
    │
    ▼
fastgltf::Asset
    │
    ├── Проверка: оптимизирован ли меш?
    │
    ▼ (если нет)
meshoptimizer pipeline:
    ├── generateVertexRemap (удаление дубликатов)
    ├── optimizeVertexCache
    ├── optimizeVertexFetch
    │
    ▼
GPU Buffers (VMA)
```

### 2. LOD Generation

Генерация уровней детализации для distant chunks:

```
Воксельный чанк (mesh)
    │
    ▼
meshopt_simplify → LOD1 (75% треугольников)
    │
    ▼
meshopt_simplify → LOD2 (50% треугольников)
    │
    ▼
meshopt_simplify → LOD3 (25% треугольников)
```

### 3. Meshlet Pipeline

Подготовка геометрии для mesh shading:

```
Оптимизированный меш
    │
    ▼
meshopt_buildMeshlets
    │
    ├── meshlet_vertices[]
    ├── meshlet_triangles[]
    └── meshopt_Meshlet[]
    │
    ▼
meshopt_computeMeshletBounds (bounding spheres + cones)
    │
    ▼
GPU Upload + Cluster Culling
```

### 4. Сжатие для streaming

Сжатие геометрии для быстрой загрузки:

```
Исходный меш
    │
    ▼
meshopt_encodeVertexBuffer
meshopt_encodeIndexBuffer
    │
    ▼
zstd compression (опционально)
    │
    ▼
Сохранение на диск / Передача по сети
```

---

## Интеграция с компонентами ProjectV

### Связь с Vulkan

| Компонент     | meshoptimizer          | Описание                              |
|---------------|------------------------|---------------------------------------|
| Vertex Buffer | `optimizeVertexFetch`  | Оптимальный порядок вершин            |
| Index Buffer  | `optimizeVertexCache`  | Минимизация vertex shader invocations |
| Mesh Shading  | `buildMeshlets`        | Кластеры для mesh shaders             |
| BLAS Building | `buildMeshletsSpatial` | Оптимизация для raytracing            |

### Связь с VMA

```cpp
// Оптимизация меши
meshopt_optimizeVertexCache(indices, indices, index_count, vertex_count);
meshopt_optimizeVertexFetch(vertices, indices, index_count, vertices, vertex_count, sizeof(Vertex));

// Создание буферов через VMA
VkBuffer vertex_buffer;
VmaAllocation vertex_allocation;
vmaCreateBuffer(allocator, &buffer_info, &alloc_info, &vertex_buffer, &vertex_allocation, nullptr);
```

### Связь с fastgltf

fastgltf поддерживает `EXT_meshopt_compression`:

```cpp
// При загрузке glTF с meshopt compression
fastgltf::Asset asset = parser.loadGLTF(buffer);

// Декомпрессия уже выполнена fastgltf
// Но меш может требовать оптимизации для GPU
if (!isOptimizedForGPU(asset.meshes[0])) {
    optimizeMesh(asset.meshes[0]);
}
```

### Связь с flecs ECS

Компоненты для хранения оптимизированной геометрии:

```cpp
struct MeshComponent {
    VkBuffer vertex_buffer;
    VkBuffer index_buffer;
    size_t vertex_count;
    size_t index_count;

    // Метрики оптимизации
    float acmr;
    float atvr;
};

struct MeshletComponent {
    std::vector<meshopt_Meshlet> meshlets;
    std::vector<unsigned int> meshlet_vertices;
    std::vector<unsigned char> meshlet_triangles;
    std::vector<meshopt_Bounds> meshlet_bounds;
};

struct LODComponent {
    std::array<MeshComponent, 4> lods;
    std::array<float, 4> errors;
};
```

---

## Паттерны использования

### Offline (при загрузке уровней)

- Полный pipeline оптимизации
- LOD generation
- Meshlet building + bounds computation
- Compression для хранения

### Runtime (в игре)

- Декомпрессия streaming assets
- Упрощение procedural meshes (если нужно)
- Динамическое обновление meshlets

---

## Ограничения

### Не используется для

| Ситуация                                   | Причина                               |
|--------------------------------------------|---------------------------------------|
| Runtime упрощение воксельных чанков        | Топология меняется, лучше пересоздать |
| Procedural geometry каждый кадр            | Слишком медленно                      |
| Очень маленькие меши (< 100 треугольников) | Накладные расходы превышают выгоду    |

### Производительность

| Операция              | Скорость   | Потокобезопасность    |
|-----------------------|------------|-----------------------|
| `optimizeVertexCache` | O(n log n) | Да (для разных мешей) |
| `simplify`            | O(n log n) | Да                    |
| `buildMeshlets`       | O(n)       | Да                    |
| `decodeVertexBuffer`  | 3–6 GB/s   | Да                    |
| `encodeVertexBuffer`  | 1–2 GB/s   | Да                    |

---

## Стратегия кэширования

### Кэширование на диске

```cpp
struct OptimizedMeshHeader {
    uint32_t version;
    uint32_t vertex_count;
    uint32_t index_count;
    float acmr;
    float atvr;
    uint32_t vertex_data_offset;
    uint32_t index_data_offset;
};
```

### Кэширование в памяти

```cpp
class MeshCache {
    std::unordered_map<MeshID, MeshComponent> cache;

    const MeshComponent& getOrLoad(MeshID id) {
        if (cache.contains(id)) return cache[id];

        MeshComponent mesh = loadAndOptimize(id);
        cache[id] = mesh;
        return cache[id];
    }
};
```

---

## Следующие шаги

- [08_projectv-integration.md](08_projectv-integration.md) — детали интеграции с Vulkan, VMA, fastgltf
- [09_projectv-patterns.md](09_projectv-patterns.md) — паттерны: LOD chain, meshlet culling
- [10_projectv-examples.md](10_projectv-examples.md) — полные примеры кода

## Установка

<!-- anchor: 02_installation -->

🟢 **Уровень 1: Базовый**

Интеграция meshoptimizer в проект: CMake, header-only, amalgamated build.

---

## Варианты интеграции

| Способ      | Описание                            | Рекомендуется          |
|-------------|-------------------------------------|------------------------|
| CMake       | Полная интеграция с системой сборки | Да                     |
| Header-only | Подключение исходников напрямую     | Для простых проектов   |
| Amalgamated | Один .cpp файл                      | Для быстрой интеграции |
| Vcpkg/Conan | Пакетные менеджеры                  | Для Windows            |

---

## CMake

### Как подмодуль

```bash
git submodule add https://github.com/zeux/meshoptimizer.git external/meshoptimizer
```

```cmake
# CMakeLists.txt
add_subdirectory(external/meshoptimizer)

target_link_libraries(YourTarget PRIVATE meshoptimizer)
```

### find_package

```cmake
find_package(meshoptimizer REQUIRED)

target_link_libraries(YourTarget PRIVATE meshoptimizer::meshoptimizer)
```

### Опции CMake

| Опция                       | По умолчанию | Описание                             |
|-----------------------------|--------------|--------------------------------------|
| `MESHOPT_BUILD_SHARED_LIBS` | OFF          | Сборка как shared library            |
| `MESHOPT_BUILD_DEMO`        | OFF          | Сборка демо                          |
| `MESHOPT_BUILD_EXAMPLES`    | OFF          | Сборка примеров                      |
| `MESHOPT_BUILD_TESTS`       | OFF          | Сборка тестов                        |
| `MESHOPT_STABLE_EXPORTS`    | OFF          | Экспортировать только стабильные API |

---

## Header-only стиль

Библиотека состоит из заголовка `meshoptimizer.h` и набора `.cpp` файлов.

```cpp
// meshoptimizer.h — C/C++ заголовок
#include <meshoptimizer.h>

// .cpp файлы (подключить нужные):
// - allocator.cpp
// - clusterizer.cpp
// - indexanalyzer.cpp
// - indexcodec.cpp
// - indexgenerator.cpp
// - meshletcodec.cpp
// - overdrawoptimizer.cpp
// - quantization.cpp
// - simplifier.cpp
// - spatialorder.cpp
// - stripifier.cpp
// - vcacheoptimizer.cpp
// - vertexcodec.cpp
// - vertexfilter.cpp
// - vfetchoptimizer.cpp
```

### Выбор файлов для сборки

| Функциональность | Файлы                                                  |
|------------------|--------------------------------------------------------|
| Indexing         | `indexgenerator.cpp`, `allocator.cpp`                  |
| Vertex Cache     | `vcacheoptimizer.cpp`, `allocator.cpp`                 |
| Overdraw         | `overdrawoptimizer.cpp`, `allocator.cpp`               |
| Vertex Fetch     | `vfetchoptimizer.cpp`, `allocator.cpp`                 |
| Compression      | `indexcodec.cpp`, `vertexcodec.cpp`, `allocator.cpp`   |
| Simplification   | `simplifier.cpp`, `allocator.cpp`                      |
| Meshlets         | `clusterizer.cpp`, `meshletutils.cpp`, `allocator.cpp` |
| Analysis         | `indexanalyzer.cpp`, `rasterizer.cpp`, `allocator.cpp` |

---

## Amalgamated Build

Конкатенация всех файлов в один для упрощения сборки:

```bash
# Linux/macOS
cat src/meshoptimizer.h > meshoptimizer_all.h
echo '#include "meshoptimizer_all.h"' > meshoptimizer_all.cpp
cat src/*.cpp >> meshoptimizer_all.cpp

# Windows PowerShell
Get-Content src/meshoptimizer.h | Set-Content meshoptimizer_all.h
"`#include `"meshoptimizer_all.h`"" | Set-Content meshoptimizer_all.cpp
Get-Content src/*.cpp | Add-Content meshoptimizer_all.cpp
```

Использование:

```cpp
// Только один файл для компиляции
#include "meshoptimizer_all.h"

// meshoptimizer_all.cpp компилируется как часть проекта
```

---

## Vcpkg

```bash
vcpkg install meshoptimizer
```

```cmake
find_package(meshoptimizer REQUIRED)
target_link_libraries(YourTarget PRIVATE meshoptimizer::meshoptimizer)
```

---

## Conan

```bash
conan install meshoptimizer/1.0@
```

---

## SIMD-оптимизации

Библиотека автоматически использует SIMD при доступности:

| Архитектура | SIMD      | Условие                                   |
|-------------|-----------|-------------------------------------------|
| x86-64      | SSE4.1    | Компилятор поддерживает, CPU имеет SSE4.1 |
| x86-64      | AVX/AVX2  | Компилятор поддерживает, CPU имеет AVX    |
| ARM         | NEON      | Компилятор поддерживает                   |
| WebAssembly | WASM SIMD | Компилятор поддерживает                   |

### Явное управление SIMD

```cpp
// Проверка поддержки во время выполнения
// Библиотека автоматически выбирает оптимальный путь
// Нет необходимости в ручной настройке
```

---

## Минимальный CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.10)
project(MeshOptimizerExample)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(external/meshoptimizer)

add_executable(example main.cpp)
target_link_libraries(example PRIVATE meshoptimizer)
```

---

## Требования к компилятору

| Компилятор | Минимальная версия |
|------------|--------------------|
| GCC        | 4.8                |
| Clang      | 3.4                |
| MSVC       | 2015 (1900)        |
| Intel C++  | 16                 |

---

## Проверка установки

```cpp
#include <meshoptimizer.h>
#include <cstdio>

int main() {
    printf("meshoptimizer version: %d\n", MESHOPTIMIZER_VERSION);

    // Тестовая квантизация
    float value = 0.5f;
    unsigned short half = meshopt_quantizeHalf(value);
    float restored = meshopt_dequantizeHalf(half);
    printf("Quantize test: %.3f -> %u -> %.3f\n", value, half, restored);

    return 0;
}

---

## meshoptimizer в ProjectV

<!-- anchor: 07_projectv-overview -->

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

| Компонент | meshoptimizer | Описание |
|-----------|---------------|----------|
| Vertex Buffer | `optimizeVertexFetch` | Оптимальный порядок вершин |
| Index Buffer | `optimizeVertexCache` | Минимизация vertex shader invocations |
| Mesh Shading | `buildMeshlets` | Кластеры для mesh shaders |
| BLAS Building | `buildMeshletsSpatial` | Оптимизация для raytracing |

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

---

## Интеграция с ProjectV

<!-- anchor: 08_projectv-integration -->

🔴 **Уровень 3: Продвинутый**

Детали интеграции meshoptimizer с Vulkan, VMA, fastgltf, flecs.

---

## Интеграция с Vulkan

### Создание оптимизированных буферов

```cpp
#include <meshoptimizer.h>
#include <vk_mem_alloc.h>

struct MeshVertex {
    float position[3];
    float normal[3];
    float uv[2];
};

struct OptimizedMesh {
    VkBuffer vertex_buffer;
    VkBuffer index_buffer;
    VmaAllocation vertex_allocation;
    VmaAllocation index_allocation;
    size_t index_count;
    size_t vertex_count;
};

OptimizedMesh createOptimizedMesh(
    VmaAllocator allocator,
    const std::vector<MeshVertex>& src_vertices,
    const std::vector<uint32_t>& src_indices
) {
    // 1. Indexing
    std::vector<uint32_t> remap(src_vertices.size());
    size_t unique_count = meshopt_generateVertexRemap(
        remap.data(),
        src_indices.data(),
        src_indices.size(),
        src_vertices.data(),
        src_vertices.size(),
        sizeof(MeshVertex)
    );

    std::vector<uint32_t> indices(src_indices.size());
    meshopt_remapIndexBuffer(indices.data(), src_indices.data(), src_indices.size(), remap.data());

    std::vector<MeshVertex> vertices(unique_count);
    meshopt_remapVertexBuffer(vertices.data(), src_vertices.data(), src_vertices.size(),
                              sizeof(MeshVertex), remap.data());

    // 2. Vertex Cache Optimization
    meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), unique_count);

    // 3. Vertex Fetch Optimization
    meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(),
                                vertices.data(), unique_count, sizeof(MeshVertex));

    // 4. Create GPU buffers
    OptimizedMesh result;
    result.index_count = indices.size();
    result.vertex_count = unique_count;

    // Vertex buffer
    VkBufferCreateInfo vb_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    vb_info.size = vertices.size() * sizeof(MeshVertex);
    vb_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo vb_alloc = {};
    vb_alloc.usage = VMA_MEMORY_USAGE_AUTO;
    vb_alloc.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    vmaCreateBuffer(allocator, &vb_info, &vb_alloc,
        &result.vertex_buffer, &result.vertex_allocation, nullptr);

    // Index buffer
    VkBufferCreateInfo ib_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    ib_info.size = indices.size() * sizeof(uint32_t);
    ib_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo ib_alloc = {};
    ib_alloc.usage = VMA_MEMORY_USAGE_AUTO;
    ib_alloc.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    vmaCreateBuffer(allocator, &ib_info, &ib_alloc,
        &result.index_buffer, &result.index_allocation, nullptr);

    // Upload data (через staging buffer)
    uploadToDevice(allocator, result.vertex_buffer, vertices.data(), vb_info.size);
    uploadToDevice(allocator, result.index_buffer, indices.data(), ib_info.size);

    return result;
}
```

### Mesh Shading Pipeline

```cpp
struct MeshletGPU {
    uint32_t vertex_offset;
    uint32_t triangle_offset;
    uint32_t vertex_count;
    uint32_t triangle_count;
};

struct MeshletData {
    VkBuffer meshlets_buffer;
    VkBuffer vertices_buffer;
    VkBuffer triangles_buffer;
    VkBuffer bounds_buffer;

    std::vector<meshopt_Bounds> bounds;
    size_t meshlet_count;
};

MeshletData createMeshlets(
    VmaAllocator allocator,
    const std::vector<MeshVertex>& vertices,
    const std::vector<uint32_t>& indices
) {
    const size_t max_vertices = 64;
    const size_t max_triangles = 126;
    const float cone_weight = 0.25f;

    size_t max_meshlets = meshopt_buildMeshletsBound(indices.size(), max_vertices, max_triangles);

    std::vector<meshopt_Meshlet> meshlets(max_meshlets);
    std::vector<uint32_t> meshlet_vertices(max_meshlets * max_vertices);
    std::vector<uint8_t> meshlet_triangles(max_meshlets * max_triangles * 3);

    size_t meshlet_count = meshopt_buildMeshlets(
        meshlets.data(),
        meshlet_vertices.data(),
        meshlet_triangles.data(),
        indices.data(),
        indices.size(),
        &vertices[0].position[0],
        vertices.size(),
        sizeof(MeshVertex),
        max_vertices,
        max_triangles,
        cone_weight
    );

    // Trim to actual size
    meshlets.resize(meshlet_count);

    // Compute bounds for each meshlet
    std::vector<meshopt_Bounds> bounds(meshlet_count);
    for (size_t i = 0; i < meshlet_count; ++i) {
        const auto& m = meshlets[i];
        bounds[i] = meshopt_computeMeshletBounds(
            &meshlet_vertices[m.vertex_offset],
            &meshlet_triangles[m.triangle_offset],
            m.triangle_count,
            &vertices[0].position[0],
            vertices.size(),
            sizeof(MeshVertex)
        );
    }

    // Create GPU buffers
    MeshletData result;
    result.bounds = std::move(bounds);
    result.meshlet_count = meshlet_count;

    // Upload meshlets, vertices, triangles, bounds to GPU
    // ...

    return result;
}
```

---

## Интеграция с fastgltf

### Загрузка и оптимизация glTF

```cpp
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

struct GLTFMesh {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
};

GLTFMesh loadAndOptimizeGLTFMesh(
    const fastgltf::Asset& asset,
    const fastgltf::Mesh& mesh,
    size_t primitive_index
) {
    const auto& primitive = mesh.primitives[primitive_index];

    // Load positions
    auto* position_attr = primitive.findAttribute("POSITION");
    auto& position_accessor = asset.accessors[position_attr->second];

    std::vector<glm::vec3> positions(position_accessor.count);
    fastgltf::copyFromAccessor(asset, position_accessor, positions.data());

    // Load normals (optional)
    std::vector<glm::vec3> normals;
    if (auto* normal_attr = primitive.findAttribute("NORMAL")) {
        normals.resize(position_accessor.count);
        fastgltf::copyFromAccessor(asset, asset.accessors[normal_attr->second], normals.data());
    }

    // Load UVs (optional)
    std::vector<glm::vec2> uvs;
    if (auto* uv_attr = primitive.findAttribute("TEXCOORD_0")) {
        uvs.resize(position_accessor.count);
        fastgltf::copyFromAccessor(asset, asset.accessors[uv_attr->second], uvs.data());
    }

    // Build vertices
    GLTFMesh result;
    result.vertices.resize(position_accessor.count);

    for (size_t i = 0; i < position_accessor.count; ++i) {
        result.vertices[i].position[0] = positions[i].x;
        result.vertices[i].position[1] = positions[i].y;
        result.vertices[i].position[2] = positions[i].z;

        if (!normals.empty()) {
            result.vertices[i].normal[0] = normals[i].x;
            result.vertices[i].normal[1] = normals[i].y;
            result.vertices[i].normal[2] = normals[i].z;
        }

        if (!uvs.empty()) {
            result.vertices[i].uv[0] = uvs[i].x;
            result.vertices[i].uv[1] = uvs[i].y;
        }
    }

    // Load indices
    if (primitive.indicesAccessor) {
        auto& index_accessor = asset.accessors[primitive.indicesAccessor.value()];
        result.indices.resize(index_accessor.count);
        fastgltf::copyFromAccessor(asset, index_accessor, result.indices.data());
    } else {
        // Generate indices for non-indexed geometry
        result.indices.resize(position_accessor.count);
        for (size_t i = 0; i < position_accessor.count; ++i) {
            result.indices[i] = static_cast<uint32_t>(i);
        }
    }

    // Optimize for GPU
    optimizeMeshInPlace(result.vertices, result.indices);

    return result;
}

void optimizeMeshInPlace(
    std::vector<MeshVertex>& vertices,
    std::vector<uint32_t>& indices
) {
    size_t index_count = indices.size();
    size_t vertex_count = vertices.size();

    // Indexing
    std::vector<uint32_t> remap(vertex_count);
    size_t unique_count = meshopt_generateVertexRemap(
        remap.data(), indices.data(), index_count,
        vertices.data(), vertex_count, sizeof(MeshVertex)
    );

    std::vector<uint32_t> new_indices(index_count);
    meshopt_remapIndexBuffer(new_indices.data(), indices.data(), index_count, remap.data());

    std::vector<MeshVertex> new_vertices(unique_count);
    meshopt_remapVertexBuffer(new_vertices.data(), vertices.data(), vertex_count,
                              sizeof(MeshVertex), remap.data());

    // Optimize
    meshopt_optimizeVertexCache(new_indices.data(), new_indices.data(), index_count, unique_count);
    meshopt_optimizeVertexFetch(new_vertices.data(), new_indices.data(), index_count,
                                new_vertices.data(), unique_count, sizeof(MeshVertex));

    vertices = std::move(new_vertices);
    indices = std::move(new_indices);
}
```

---

## Интеграция с flecs ECS

### Компоненты

```cpp
// ECS компоненты для меши
struct StaticMesh {
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkBuffer index_buffer = VK_NULL_HANDLE;
    VmaAllocation vertex_allocation = VK_NULL_HANDLE;
    VmaAllocation index_allocation = VK_NULL_HANDLE;
    uint32_t index_count = 0;
    uint32_t vertex_count = 0;
};

struct MeshLODs {
    static constexpr size_t MAX_LODS = 4;

    StaticMesh lods[MAX_LODS];
    float errors[MAX_LODS] = {};
    size_t lod_count = 0;
};

struct MeshletGeometry {
    VkBuffer meshlets_buffer;
    VkBuffer vertices_buffer;
    VkBuffer triangles_buffer;
    VkBuffer bounds_buffer;

    std::vector<meshopt_Bounds> cpu_bounds;
    uint32_t meshlet_count;
};

struct MeshMetrics {
    float acmr = 0.0f;
    float atvr = 0.0f;
    float overdraw = 0.0f;
};
```

### Система оптимизации

```cpp
class MeshOptimizationSystem {
public:
    MeshOptimizationSystem(flecs::world& world, VmaAllocator allocator)
        : allocator_(allocator)
    {
        // Система для создания LOD-ов
        world.system<MeshLODs, const StaticMesh>("CreateLODs")
            .kind(flecs::OnLoad)
            .each([this](flecs::entity e, MeshLODs& lods, const StaticMesh& base_mesh) {
                if (lods.lod_count == 0 && base_mesh.index_count > 300) {
                    generateLODs(e, base_mesh);
                }
            });

        // Система для создания meshlets
        world.system<MeshletGeometry, const StaticMesh>("CreateMeshlets")
            .kind(flecs::OnLoad)
            .each([this](flecs::entity e, MeshletGeometry& meshlet, const StaticMesh& mesh) {
                if (meshlet.meshlet_count == 0 && mesh.index_count > 0) {
                    generateMeshlets(e, mesh);
                }
            });
    }

private:
    VmaAllocator allocator_;

    void generateLODs(flecs::entity e, const StaticMesh& base_mesh) {
        // Загрузка данных из GPU (или использование кэша)
        // ...

        // Генерация LOD-ов
        MeshLODs lods;
        lods.lods[0] = base_mesh;
        lods.lod_count = 1;

        // Последовательное упрощение
        std::vector<uint32_t> prev_indices = loadIndices(base_mesh);
        std::vector<MeshVertex> vertices = loadVertices(base_mesh);

        size_t target_counts[] = {
            prev_indices.size() * 3 / 4,   // 75%
            prev_indices.size() * 1 / 2,   // 50%
            prev_indices.size() * 1 / 4    // 25%
        };

        for (size_t lod = 0; lod < 3; ++lod) {
            std::vector<uint32_t> lod_indices(prev_indices.size());
            float error = 0.0f;

            size_t result_count = meshopt_simplify(
                lod_indices.data(),
                prev_indices.data(),
                prev_indices.size(),
                &vertices[0].position[0],
                vertices.size(),
                sizeof(MeshVertex),
                target_counts[lod],
                1e-2f,
                0,
                &error
            );

            if (result_count > 0) {
                lod_indices.resize(result_count);

                // Оптимизировать и загрузить на GPU
                meshopt_optimizeVertexCache(lod_indices.data(), lod_indices.data(),
                                           result_count, vertices.size());

                lods.lods[lods.lod_count] = createMesh(vertices, lod_indices);
                lods.errors[lods.lod_count] = error;
                lods.lod_count++;

                prev_indices = lod_indices;
            }
        }

        e.set<MeshLODs>(std::move(lods));
    }

    void generateMeshlets(flecs::entity e, const StaticMesh& mesh) {
        // Аналогично примеру выше
        // ...
    }
};
```

---

## Интеграция со сжатием

### Сохранение оптимизированной геометрии

```cpp
#include <zstd.h>

struct CompressedMeshData {
    std::vector<uint8_t> encoded_vertices;
    std::vector<uint8_t> encoded_indices;
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t vertex_size;
};

CompressedMeshData compressMesh(
    const std::vector<MeshVertex>& vertices,
    const std::vector<uint32_t>& indices
) {
    CompressedMeshData result;
    result.vertex_count = vertices.size();
    result.index_count = indices.size();
    result.vertex_size = sizeof(MeshVertex);

    // Encode vertex buffer
    size_t vertex_bound = meshopt_encodeVertexBufferBound(vertices.size(), sizeof(MeshVertex));
    result.encoded_vertices.resize(vertex_bound);
    result.encoded_vertices.resize(
        meshopt_encodeVertexBuffer(
            result.encoded_vertices.data(), vertex_bound,
            vertices.data(), vertices.size(), sizeof(MeshVertex)
        )
    );

    // Encode index buffer
    size_t index_bound = meshopt_encodeIndexBufferBound(indices.size(), vertices.size());
    result.encoded_indices.resize(index_bound);
    result.encoded_indices.resize(
        meshopt_encodeIndexBuffer(
            result.encoded_indices.data(), index_bound,
            indices.data(), indices.size()
        )
    );

    // Optional: zstd compression
    // ...

    return result;
}

void decompressMesh(
    const CompressedMeshData& compressed,
    std::vector<MeshVertex>& vertices,
    std::vector<uint32_t>& indices
) {
    vertices.resize(compressed.vertex_count);
    indices.resize(compressed.index_count);

    int vres = meshopt_decodeVertexBuffer(
        vertices.data(), compressed.vertex_count, compressed.vertex_size,
        compressed.encoded_vertices.data(), compressed.encoded_vertices.size()
    );
    assert(vres == 0);

    int ires = meshopt_decodeIndexBuffer(
        indices.data(), compressed.index_count, sizeof(uint32_t),
        compressed.encoded_indices.data(), compressed.encoded_indices.size()
    );
    assert(ires == 0);
}
```

---

## Асинхронная загрузка

```cpp
class AsyncMeshLoader {
public:
    struct LoadRequest {
        std::string path;
        std::promise<OptimizedMesh> promise;
    };

    AsyncMeshLoader(VmaAllocator allocator) : allocator_(allocator) {
        worker_thread_ = std::thread([this] { workerLoop(); });
    }

    ~AsyncMeshLoader() {
        stop_ = true;
        cv_.notify_all();
        worker_thread_.join();
    }

    std::future<OptimizedMesh> loadAsync(const std::string& path) {
        std::promise<OptimizedMesh> promise;
        auto future = promise.get_future();

        {
            std::lock_guard lock(mutex_);
            queue_.push({path, std::move(promise)});
        }
        cv_.notify_one();

        return future;
    }

private:
    void workerLoop() {
        while (!stop_) {
            LoadRequest request;
            {
                std::unique_lock lock(mutex_);
                cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });

                if (stop_) break;

                request = std::move(queue_.front());
                queue_.pop();
            }

            try {
                // Load file
                auto [vertices, indices] = loadMeshFromFile(request.path);

                // Optimize (CPU-intensive, но в фоновом потоке)
                auto mesh = createOptimizedMesh(allocator_, vertices, indices);

                request.promise.set_value(std::move(mesh));
            } catch (...) {
                request.promise.set_exception(std::current_exception());
            }
        }
    }

    VmaAllocator allocator_;
    std::thread worker_thread_;
    std::queue<LoadRequest> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
};

---

## Паттерны использования в ProjectV

<!-- anchor: 09_projectv-patterns -->

🔴 **Уровень 3: Продвинутый**

Типичные паттерны использования meshoptimizer в воксельном движке.

---

## LOD Chain Generation

### Последовательное упрощение

Генерация цепочки LOD-ов от детального к грубому:

```cpp
struct LODChain {
    std::vector<std::vector<uint32_t>> lod_indices;
    std::vector<float> lod_errors;
    size_t base_vertex_count;
};

LODChain generateLODChain(
    const std::vector<MeshVertex>& vertices,
    const std::vector<uint32_t>& base_indices,
    const std::vector<size_t>& target_percentages  // {75, 50, 25, 10}
) {
    LODChain chain;
    chain.base_vertex_count = vertices.size();
    chain.lod_indices.push_back(base_indices);
    chain.lod_errors.push_back(0.0f);

    std::vector<uint32_t> prev_indices = base_indices;

    for (size_t pct : target_percentages) {
        size_t target = (base_indices.size() * pct) / 100;
        target = std::max(target, size_t(3));  // Минимум 1 треугольник

        std::vector<uint32_t> lod_indices(prev_indices.size());
        float error = 0.0f;

        size_t result = meshopt_simplify(
            lod_indices.data(),
            prev_indices.data(),
            prev_indices.size(),
            &vertices[0].position[0],
            vertices.size(),
            sizeof(MeshVertex),
            target,
            FLT_MAX,  // Без ограничения ошибки
            meshopt_SimplifyLockBorder,
            &error
        );

        if (result > 3) {
            lod_indices.resize(result);

            // Оптимизировать для vertex cache
            meshopt_optimizeVertexCache(
                lod_indices.data(),
                lod_indices.data(),
                result,
                vertices.size()
            );

            chain.lod_indices.push_back(std::move(lod_indices));
            chain.lod_errors.push_back(error);
            prev_indices = chain.lod_indices.back();
        }
    }

    return chain;
}
```

### LOD Selection

Выбор LOD на основе расстояния и ошибки:

```cpp
size_t selectLOD(const LODChain& chain, float distance, float screen_height, float fov) {
    // Экранный порог в пикселях
    const float pixel_threshold = 2.0f;

    // Преобразование ошибки в экранное пространство
    // error_screen = error * screen_height / (distance * tan(fov/2))
    float scale = screen_height / (distance * std::tan(fov / 2));

    for (size_t i = 1; i < chain.lod_errors.size(); ++i) {
        float screen_error = chain.lod_errors[i] * scale;

        if (screen_error < pixel_threshold) {
            return i;
        }
    }

    return chain.lod_indices.size() - 1;
}
```

### LOD с shared vertex buffer

Использование одного vertex buffer для всех LOD-ов:

```cpp
struct SharedLODMesh {
    VkBuffer vertex_buffer;
    VmaAllocation vertex_allocation;

    std::vector<VkBuffer> index_buffers;
    std::vector<VmaAllocation> index_allocations;
    std::vector<uint32_t> index_counts;
    std::vector<float> errors;
};

SharedLODMesh createSharedLODMesh(
    VmaAllocator allocator,
    const std::vector<MeshVertex>& vertices,
    const std::vector<uint32_t>& base_indices
) {
    SharedLODMesh result;

    // Создаём vertex buffer один раз
    createVertexBuffer(allocator, vertices, result.vertex_buffer, result.vertex_allocation);

    // Генерируем LOD-ы
    LODChain chain = generateLODChain(vertices, base_indices, {75, 50, 25, 10});

    // Оптимизируем vertex order для всех LOD-ов сразу
    std::vector<uint32_t> combined_remap;
    size_t total_vertices = optimizeVertexFetchForAllLODs(
        vertices, chain.lod_indices, combined_remap
    );

    // Создаём index buffers для каждого LOD
    for (size_t i = 0; i < chain.lod_indices.size(); ++i) {
        VkBuffer ib;
        VmaAllocation ib_alloc;
        createIndexBuffer(allocator, chain.lod_indices[i], ib, ib_alloc);

        result.index_buffers.push_back(ib);
        result.index_allocations.push_back(ib_alloc);
        result.index_counts.push_back(chain.lod_indices[i].size());
        result.errors.push_back(chain.lod_errors[i]);
    }

    return result;
}
```

---

## Meshlet Culling

### Frustum Culling

```cpp
struct Frustum {
    glm::vec4 planes[6];  // left, right, bottom, top, near, far
};

bool isMeshletVisible(const meshopt_Bounds& bounds, const Frustum& frustum) {
    // Bounding sphere test
    for (int i = 0; i < 6; ++i) {
        float distance = glm::dot(glm::vec3(frustum.planes[i]),
                                  glm::vec3(bounds.center[0], bounds.center[1], bounds.center[2]))
                        + frustum.planes[i].w;

        if (distance < -bounds.radius) {
            return false;  // Полностью вне frustum
        }
    }
    return true;
}
```

### Cone Culling (Backface)

```cpp
bool isMeshletBackfacing(
    const meshopt_Bounds& bounds,
    const glm::vec3& camera_position
) {
    glm::vec3 center(bounds.center[0], bounds.center[1], bounds.center[2]);
    glm::vec3 cone_axis(bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2]);

    // Формула для perspective projection
    glm::vec3 view_dir = glm::normalize(center - camera_position);
    float d = glm::dot(view_dir, cone_axis);

    return d >= bounds.cone_cutoff;
}
```

### Occlusion Culling (GPU-driven)

```cpp
// CPU: подготовка indirect draw commands
void prepareMeshletDraws(
    const std::vector<meshopt_Bounds>& bounds,
    const Frustum& frustum,
    const glm::vec3& camera_position,
    std::vector<VkDrawMeshTasksIndirectCommandEXT>& commands
) {
    commands.clear();

    for (size_t i = 0; i < bounds.size(); ++i) {
        if (!isMeshletVisible(bounds[i], frustum)) continue;
        if (isMeshletBackfacing(bounds[i], camera_position)) continue;

        commands.push_back({
            .groupCountX = 1,
            .groupCountY = 1,
            .groupCountZ = 1
        });
    }
}

// GPU: hierarchical z-buffer occlusion culling
// Выполняется в task/amplification shader
```

---

## Streaming Decompression

### Потоковая загрузка сжатых данных

```cpp
class StreamingMeshLoader {
public:
    struct MeshPart {
        std::vector<MeshVertex> vertices;
        std::vector<uint32_t> indices;
    };

    StreamingMeshLoader(size_t max_memory) : max_memory_(max_memory) {}

    std::future<MeshPart> loadCompressed(const std::string& path) {
        return std::async(std::launch::async, [this, path] {
            // 1. Загрузка сжатых данных с диска
            auto compressed = loadCompressedData(path);

            // 2. Декомпрессия
            MeshPart part;
            part.vertices.resize(compressed.vertex_count);
            part.indices.resize(compressed.index_count);

            // Декодирование vertex buffer
            int vres = meshopt_decodeVertexBuffer(
                part.vertices.data(),
                compressed.vertex_count,
                sizeof(MeshVertex),
                compressed.vertex_data.data(),
                compressed.vertex_data.size()
            );

            if (vres != 0) {
                throw std::runtime_error("Vertex decode failed");
            }

            // Декодирование index buffer
            int ires = meshopt_decodeIndexBuffer(
                part.indices.data(),
                compressed.index_count,
                sizeof(uint32_t),
                compressed.index_data.data(),
                compressed.index_data.size()
            );

            if (ires != 0) {
                throw std::runtime_error("Index decode failed");
            }

            return part;
        });
    }

private:
    size_t max_memory_;
    std::mutex mutex_;
};
```

### Приоритетная загрузка

```cpp
class PriorityMeshLoader {
public:
    using MeshID = uint64_t;
    using Priority = float;

    void requestMesh(MeshID id, Priority priority, std::function<void(OptimizedMesh)> callback) {
        std::lock_guard lock(mutex_);

        auto it = std::lower_bound(queue_.begin(), queue_.end(),
            std::make_pair(priority, id),
            [](auto& a, auto& b) { return a.first > b.first; });

        queue_.insert(it, {priority, id, callback});

        if (!loading_) {
            loading_ = true;
            worker_cv_.notify_one();
        }
    }

    void updatePriorities(const std::vector<std::pair<MeshID, Priority>>& updates) {
        std::lock_guard lock(mutex_);

        for (const auto& [id, priority] : updates) {
            // Найти и обновить приоритет
            for (auto& item : queue_) {
                if (std::get<1>(item) == id) {
                    item = {priority, id, std::get<2>(item)};
                    break;
                }
            }
        }

        // Пересортировать
        std::sort(queue_.begin(), queue_.end(),
            [](auto& a, auto& b) { return std::get<0>(a) > std::get<0>(b); });
    }

private:
    std::mutex mutex_;
    std::condition_variable worker_cv_;
    std::vector<std::tuple<Priority, MeshID, std::function<void(OptimizedMesh)>>> queue_;
    bool loading_ = false;
};
```

---

## Chunk Optimization

### Оптимизация воксельных чанков

```cpp
struct VoxelChunkMesh {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;

    // Оптимизированные данные
    OptimizedMesh gpu_mesh;
    std::vector<meshopt_Bounds> meshlet_bounds;
};

VoxelChunkMesh createOptimizedChunkMesh(
    const std::vector<VoxelVertex>& raw_vertices,
    const std::vector<uint32_t>& raw_indices
) {
    VoxelChunkMesh result;

    // 1. Конвертация вершин
    result.vertices.resize(raw_vertices.size());
    for (size_t i = 0; i < raw_vertices.size(); ++i) {
        result.vertices[i].position[0] = static_cast<float>(raw_vertices[i].x);
        result.vertices[i].position[1] = static_cast<float>(raw_vertices[i].y);
        result.vertices[i].position[2] = static_cast<float>(raw_vertices[i].z);
        // ... normals, uvs, etc.
    }

    // 2. Indexing (удаление дубликатов от marching cubes)
    std::vector<uint32_t> remap(result.vertices.size());
    size_t unique_count = meshopt_generateVertexRemap(
        remap.data(),
        raw_indices.data(),
        raw_indices.size(),
        result.vertices.data(),
        result.vertices.size(),
        sizeof(MeshVertex)
    );

    result.indices.resize(raw_indices.size());
    meshopt_remapIndexBuffer(result.indices.data(), raw_indices.data(),
                             raw_indices.size(), remap.data());

    result.vertices.resize(unique_count);
    meshopt_remapVertexBuffer(result.vertices.data(), result.vertices.data(),
                              unique_count, sizeof(MeshVertex), remap.data());

    // 3. Optimization
    meshopt_optimizeVertexCache(result.indices.data(), result.indices.data(),
                                result.indices.size(), unique_count);
    meshopt_optimizeVertexFetch(result.vertices.data(), result.indices.data(),
                                result.indices.size(), result.vertices.data(),
                                unique_count, sizeof(MeshVertex));

    return result;
}
```

### Параллельная генерация чанков

```cpp
class ChunkMeshGenerator {
public:
    ChunkMeshGenerator(size_t thread_count) {
        for (size_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back([this] { workerLoop(); });
        }
    }

    ~ChunkMeshGenerator() {
        stop_ = true;
        cv_.notify_all();
        for (auto& t : workers_) t.join();
    }

    std::future<VoxelChunkMesh> generateAsync(ChunkCoord coord) {
        std::promise<VoxelChunkMesh> promise;
        auto future = promise.get_future();

        {
            std::lock_guard lock(mutex_);
            queue_.push({coord, std::move(promise)});
        }
        cv_.notify_one();

        return future;
    }

private:
    struct WorkItem {
        ChunkCoord coord;
        std::promise<VoxelChunkMesh> promise;
    };

    void workerLoop() {
        while (true) {
            WorkItem item;
            {
                std::unique_lock lock(mutex_);
                cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
                if (stop_) return;

                item = std::move(queue_.front());
                queue_.pop();
            }

            try {
                // Генерация меши чанка
                auto [vertices, indices] = generateChunkGeometry(item.coord);

                // Оптимизация
                auto mesh = createOptimizedChunkMesh(vertices, indices);

                item.promise.set_value(std::move(mesh));
            } catch (...) {
                item.promise.set_exception(std::current_exception());
            }
        }
    }

    std::vector<std::thread> workers_;
    std::queue<WorkItem> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
};
```

---

## Raytracing Optimization

### Meshlets для BLAS

```cpp
struct RaytracingMeshletData {
    std::vector<meshopt_Meshlet> meshlets;
    std::vector<uint32_t> meshlet_vertices;
    std::vector<uint8_t> meshlet_triangles;
    std::vector<meshopt_Bounds> bounds;
};

RaytracingMeshletData createRaytracingMeshlets(
    const std::vector<MeshVertex>& vertices,
    const std::vector<uint32_t>& indices
) {
    const size_t max_vertices = 64;
    const size_t min_triangles = 16;
    const size_t max_triangles = 64;
    const float fill_weight = 0.5f;

    size_t max_meshlets = meshopt_buildMeshletsBound(
        indices.size(), max_vertices, min_triangles);

    RaytracingMeshletData result;
    result.meshlets.resize(max_meshlets);
    result.meshlet_vertices.resize(max_meshlets * max_vertices);
    result.meshlet_triangles.resize(max_meshlets * max_triangles * 3);

    size_t meshlet_count = meshopt_buildMeshletsSpatial(
        result.meshlets.data(),
        result.meshlet_vertices.data(),
        result.meshlet_triangles.data(),
        indices.data(),
        indices.size(),
        &vertices[0].position[0],
        vertices.size(),
        sizeof(MeshVertex),
        max_vertices,
        min_triangles,
        max_triangles,
        fill_weight
    );

    result.meshlets.resize(meshlet_count);

    // Вычислить bounds
    result.bounds.resize(meshlet_count);
    for (size_t i = 0; i < meshlet_count; ++i) {
        const auto& m = result.meshlets[i];
        result.bounds[i] = meshopt_computeMeshletBounds(
            &result.meshlet_vertices[m.vertex_offset],
            &result.meshlet_triangles[m.triangle_offset],
            m.triangle_count,
            &vertices[0].position[0],
            vertices.size(),
            sizeof(MeshVertex)
        );
    }

    return result;
}
```

---

## Memory Budget Management

### Управление памятью при загрузке

```cpp
class MeshMemoryManager {
public:
    MeshMemoryManager(size_t budget) : budget_(budget), used_(0) {}

    bool canAllocate(size_t size) const {
        return used_ + size <= budget_;
    }

    std::optional<OptimizedMesh> tryLoad(
        const std::string& path,
        VmaAllocator allocator
    ) {
        // Оценка размера
        size_t estimated_size = estimateMeshSize(path);

        if (!canAllocate(estimated_size)) {
            // Попробовать освободить память
            evictLRU(estimated_size);
        }

        if (!canAllocate(estimated_size)) {
            return std::nullopt;
        }

        // Загрузка
        auto mesh = loadAndOptimizeMesh(path, allocator);
        used_ += getMeshSize(mesh);

        // Добавить в LRU
        lru_list_.push_front({path, mesh});
        lru_map_[path] = lru_list_.begin();

        return mesh;
    }

    void evictLRU(size_t needed_space) {
        while (used_ + needed_space > budget_ && !lru_list_.empty()) {
            auto& [path, mesh] = lru_list_.back();

            used_ -= getMeshSize(mesh);
            destroyMesh(mesh);

            lru_map_.erase(path);
            lru_list_.pop_back();
        }
    }

private:
    size_t budget_;
    size_t used_;
    std::list<std::pair<std::string, OptimizedMesh>> lru_list_;
    std::unordered_map<std::string, decltype(lru_list_)::iterator> lru_map_;
};

---

## Примеры кода для ProjectV

<!-- anchor: 10_projectv-examples -->

🔴 **Уровень 3: Продвинутый**

Полные примеры использования meshoptimizer в ProjectV.

---

## Пример 1: Полный pipeline оптимизации

```cpp
#include <meshoptimizer.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstdio>

// Структура вершины для ProjectV
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

// Результат оптимизации
struct OptimizedMesh {
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkBuffer index_buffer = VK_NULL_HANDLE;
    VmaAllocation vertex_allocation = VK_NULL_HANDLE;
    VmaAllocation index_allocation = VK_NULL_HANDLE;
    uint32_t index_count = 0;
    uint32_t vertex_count = 0;
    float acmr = 0.0f;
    float atvr = 0.0f;
};

class MeshOptimizer {
public:
    MeshOptimizer(VmaAllocator allocator, VkDevice device, VkQueue queue, VkCommandPool cmd_pool)
        : allocator_(allocator), device_(device), queue_(queue), cmd_pool_(cmd_pool) {}

    OptimizedMesh optimizeAndUpload(
        const std::vector<Vertex>& src_vertices,
        const std::vector<uint32_t>& src_indices
    ) {
        OptimizedMesh result;

        // ========== 1. INDEXING ==========
        printf("[MeshOptimizer] Step 1: Indexing...\n");

        std::vector<uint32_t> remap(src_vertices.size());
        size_t unique_count = meshopt_generateVertexRemap(
            remap.data(),
            src_indices.data(),
            src_indices.size(),
            src_vertices.data(),
            src_vertices.size(),
            sizeof(Vertex)
        );

        printf("  Unique vertices: %zu -> %zu\n", src_vertices.size(), unique_count);

        // Применяем remap
        std::vector<uint32_t> indices(src_indices.size());
        meshopt_remapIndexBuffer(indices.data(), src_indices.data(), src_indices.size(), remap.data());

        std::vector<Vertex> vertices(unique_count);
        meshopt_remapVertexBuffer(vertices.data(), src_vertices.data(), src_vertices.size(),
                                  sizeof(Vertex), remap.data());

        // ========== 2. VERTEX CACHE OPTIMIZATION ==========
        printf("[MeshOptimizer] Step 2: Vertex Cache Optimization...\n");

        auto before_vcache = meshopt_analyzeVertexCache(
            indices.data(), indices.size(), unique_count, 16, 0, 0);

        meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), unique_count);

        auto after_vcache = meshopt_analyzeVertexCache(
            indices.data(), indices.size(), unique_count, 16, 0, 0);

        printf("  ACMR: %.3f -> %.3f\n", before_vcache.acmr, after_vcache.acmr);
        printf("  ATVR: %.3f -> %.3f\n", before_vcache.atvr, after_vcache.atvr);

        result.acmr = after_vcache.acmr;
        result.atvr = after_vcache.atvr;

        // ========== 3. VERTEX FETCH OPTIMIZATION ==========
        printf("[MeshOptimizer] Step 3: Vertex Fetch Optimization...\n");

        auto before_vfetch = meshopt_analyzeVertexFetch(
            indices.data(), indices.size(), unique_count, sizeof(Vertex));

        meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(),
                                    vertices.data(), unique_count, sizeof(Vertex));

        auto after_vfetch = meshopt_analyzeVertexFetch(
            indices.data(), indices.size(), unique_count, sizeof(Vertex));

        printf("  Overfetch: %.3f -> %.3f\n", before_vfetch.overfetch, after_vfetch.overfetch);

        // ========== 4. UPLOAD TO GPU ==========
        printf("[MeshOptimizer] Step 4: Uploading to GPU...\n");

        result.vertex_count = static_cast<uint32_t>(unique_count);
        result.index_count = static_cast<uint32_t>(indices.size());

        // Vertex buffer
        VkDeviceSize vb_size = vertices.size() * sizeof(Vertex);
        createDeviceLocalBuffer(vb_size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            vertices.data(), result.vertex_buffer, result.vertex_allocation);

        // Index buffer
        VkDeviceSize ib_size = indices.size() * sizeof(uint32_t);
        createDeviceLocalBuffer(ib_size,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            indices.data(), result.index_buffer, result.index_allocation);

        printf("  Vertex buffer: %zu bytes\n", (size_t)vb_size);
        printf("  Index buffer: %zu bytes\n", (size_t)ib_size);
        printf("[MeshOptimizer] Done!\n");

        return result;
    }

    void destroy(OptimizedMesh& mesh) {
        if (mesh.vertex_buffer) {
            vmaDestroyBuffer(allocator_, mesh.vertex_buffer, mesh.vertex_allocation);
            mesh.vertex_buffer = VK_NULL_HANDLE;
        }
        if (mesh.index_buffer) {
            vmaDestroyBuffer(allocator_, mesh.index_buffer, mesh.index_allocation);
            mesh.index_buffer = VK_NULL_HANDLE;
        }
    }

private:
    VmaAllocator allocator_;
    VkDevice device_;
    VkQueue queue_;
    VkCommandPool cmd_pool_;

    void createDeviceLocalBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        const void* data,
        VkBuffer& buffer,
        VmaAllocation& allocation
    ) {
        // Staging buffer
        VkBufferCreateInfo staging_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        staging_info.size = size;
        staging_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo staging_alloc = {};
        staging_alloc.usage = VMA_MEMORY_USAGE_AUTO;
        staging_alloc.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                              VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        VkBuffer staging_buffer;
        VmaAllocation staging_allocation;
        VmaAllocationInfo staging_info_result;

        vmaCreateBuffer(allocator_, &staging_info, &staging_alloc,
            &staging_buffer, &staging_allocation, &staging_info_result);

        memcpy(staging_info_result.pMappedData, data, size);

        // Device local buffer
        VkBufferCreateInfo buffer_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        buffer_info.size = size;
        buffer_info.usage = usage;

        VmaAllocationCreateInfo buffer_alloc = {};
        buffer_alloc.usage = VMA_MEMORY_USAGE_AUTO;
        buffer_alloc.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        vmaCreateBuffer(allocator_, &buffer_info, &buffer_alloc,
            &buffer, &allocation, nullptr);

        // Copy command
        VkCommandBuffer cmd = beginSingleTimeCommands();

        VkBufferCopy copy = {};
        copy.srcOffset = 0;
        copy.dstOffset = 0;
        copy.size = size;
        vkCmdCopyBuffer(cmd, staging_buffer, buffer, 1, &copy);

        endSingleTimeCommands(cmd);

        // Cleanup staging
        vmaDestroyBuffer(allocator_, staging_buffer, staging_allocation);
    }

    VkCommandBuffer beginSingleTimeCommands() {
        VkCommandBufferAllocateInfo alloc_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        alloc_info.commandPool = cmd_pool_;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(device_, &alloc_info, &cmd);

        VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmd, &begin_info);
        return cmd;
    }

    void endSingleTimeCommands(VkCommandBuffer cmd) {
        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd;

        vkQueueSubmit(queue_, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue_);

        vkFreeCommandBuffers(device_, cmd_pool_, 1, &cmd);
    }
};
```

---

## Пример 2: Генерация LOD цепочки

```cpp
#include <meshoptimizer.h>
#include <glm/glm.hpp>
#include <vector>
#include <array>

struct LODMesh {
    VkBuffer index_buffer;
    VmaAllocation index_allocation;
    uint32_t index_count;
    float error;
};

struct LODChainMesh {
    // Shared vertex buffer
    VkBuffer vertex_buffer;
    VmaAllocation vertex_allocation;
    uint32_t vertex_count;

    // Per-LOD index buffers
    std::array<LODMesh, 4> lods;
    size_t lod_count;
};

class LODGenerator {
public:
    LODGenerator(VmaAllocator allocator) : allocator_(allocator) {}

    LODChainMesh generateLODs(
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& base_indices,
        const std::vector<float>& lod_ratios = {0.75f, 0.5f, 0.25f, 0.1f}
    ) {
        LODChainMesh result;
        result.vertex_count = static_cast<uint32_t>(vertices.size());
        result.lod_count = 0;

        // Создаём общий vertex buffer
        createVertexBuffer(vertices, result.vertex_buffer, result.vertex_allocation);

        // LOD 0: исходный меш
        result.lods[0].index_count = static_cast<uint32_t>(base_indices.size());
        result.lods[0].error = 0.0f;
        createIndexBuffer(base_indices, result.lods[0].index_buffer, result.lods[0].index_allocation);
        result.lod_count = 1;

        // Генерируем последующие LOD-ы
        std::vector<uint32_t> prev_indices = base_indices;
        float accumulated_error = 0.0f;

        for (size_t i = 1; i < lod_ratios.size(); ++i) {
            size_t target_count = base_indices.size() * lod_ratios[i];
            target_count = std::max(target_count, size_t(12));  // Минимум 4 треугольника

            std::vector<uint32_t> lod_indices(prev_indices.size());
            float lod_error = 0.0f;

            size_t result_count = meshopt_simplify(
                lod_indices.data(),
                prev_indices.data(),
                prev_indices.size(),
                &vertices[0].position.x,
                vertices.size(),
                sizeof(Vertex),
                target_count,
                1e-2f,  // 1% максимальная ошибка
                meshopt_SimplifyLockBorder,
                &lod_error
            );

            if (result_count < 12) break;  // Слишком мало треугольников

            lod_indices.resize(result_count);

            // Оптимизируем для vertex cache
            meshopt_optimizeVertexCache(
                lod_indices.data(),
                lod_indices.data(),
                result_count,
                vertices.size()
            );

            // Создаём index buffer
            result.lods[i].index_count = static_cast<uint32_t>(result_count);
            result.lods[i].error = accumulated_error + lod_error;
            createIndexBuffer(lod_indices, result.lods[i].index_buffer, result.lods[i].index_allocation);

            accumulated_error += lod_error;
            prev_indices = std::move(lod_indices);
            result.lod_count++;
        }

        return result;
    }

    size_t selectLOD(const LODChainMesh& mesh, float distance, float screen_height, float fov) {
        const float pixel_threshold = 1.5f;
        float scale = screen_height / (distance * std::tan(fov / 2));

        for (size_t i = 1; i < mesh.lod_count; ++i) {
            float screen_error = mesh.lods[i].error * scale;
            if (screen_error < pixel_threshold) {
                return i;
            }
        }
        return mesh.lod_count - 1;
    }

private:
    VmaAllocator allocator_;

    void createVertexBuffer(const std::vector<Vertex>& vertices,
                           VkBuffer& buffer, VmaAllocation& allocation) {
        VkDeviceSize size = vertices.size() * sizeof(Vertex);

        VkBufferCreateInfo info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = size;
        info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo alloc = {};
        alloc.usage = VMA_MEMORY_USAGE_AUTO;
        alloc.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        vmaCreateBuffer(allocator_, &info, &alloc, &buffer, &allocation, nullptr);
        // Upload data...
    }

    void createIndexBuffer(const std::vector<uint32_t>& indices,
                          VkBuffer& buffer, VmaAllocation& allocation) {
        VkDeviceSize size = indices.size() * sizeof(uint32_t);

        VkBufferCreateInfo info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = size;
        info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo alloc = {};
        alloc.usage = VMA_MEMORY_USAGE_AUTO;
        alloc.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        vmaCreateBuffer(allocator_, &info, &alloc, &buffer, &allocation, nullptr);
        // Upload data...
    }
};
```

---

## Пример 3: Meshlet Pipeline

```cpp
#include <meshoptimizer.h>
#include <glm/glm.hpp>
#include <vector>

struct MeshletData {
    // CPU data
    std::vector<meshopt_Meshlet> meshlets;
    std::vector<uint32_t> meshlet_vertices;
    std::vector<uint8_t> meshlet_triangles;
    std::vector<meshopt_Bounds> bounds;

    // GPU buffers
    VkBuffer meshlets_buffer;
    VkBuffer vertices_buffer;
    VkBuffer triangles_buffer;
    VkBuffer bounds_buffer;
    VmaAllocation allocations[4];
    uint32_t meshlet_count;
};

class MeshletBuilder {
public:
    MeshletBuilder(VmaAllocator allocator) : allocator_(allocator) {}

    MeshletData buildMeshlets(
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices,
        bool for_raytracing = false
    ) {
        const size_t max_vertices = 64;
        const size_t max_triangles = for_raytracing ? 64 : 126;
        const float cone_weight = 0.25f;

        MeshletData result;

        size_t max_meshlets = meshopt_buildMeshletsBound(
            indices.size(), max_vertices, max_triangles);

        result.meshlets.resize(max_meshlets);
        result.meshlet_vertices.resize(max_meshlets * max_vertices);
        result.meshlet_triangles.resize(max_meshlets * max_triangles * 3);

        size_t meshlet_count;

        if (for_raytracing) {
            meshlet_count = meshopt_buildMeshletsSpatial(
                result.meshlets.data(),
                result.meshlet_vertices.data(),
                result.meshlet_triangles.data(),
                indices.data(),
                indices.size(),
                &vertices[0].position.x,
                vertices.size(),
                sizeof(Vertex),
                max_vertices,
                16,  // min_triangles
                max_triangles,
                0.5f  // fill_weight
            );
        } else {
            meshlet_count = meshopt_buildMeshlets(
                result.meshlets.data(),
                result.meshlet_vertices.data(),
                result.meshlet_triangles.data(),
                indices.data(),
                indices.size(),
                &vertices[0].position.x,
                vertices.size(),
                sizeof(Vertex),
                max_vertices,
                max_triangles,
                cone_weight
            );
        }

        result.meshlets.resize(meshlet_count);
        result.meshlet_count = static_cast<uint32_t>(meshlet_count);

        // Trim arrays
        const auto& last = result.meshlets.back();
        result.meshlet_vertices.resize(last.vertex_offset + last.vertex_count);
        result.meshlet_triangles.resize(last.triangle_offset + last.triangle_count * 3);

        // Optimize each meshlet
        for (auto& m : result.meshlets) {
            meshopt_optimizeMeshlet(
                &result.meshlet_vertices[m.vertex_offset],
                &result.meshlet_triangles[m.triangle_offset],
                m.triangle_count,
                m.vertex_count
            );
        }

        // Compute bounds
        result.bounds.resize(meshlet_count);
        for (size_t i = 0; i < meshlet_count; ++i) {
            const auto& m = result.meshlets[i];
            result.bounds[i] = meshopt_computeMeshletBounds(
                &result.meshlet_vertices[m.vertex_offset],
                &result.meshlet_triangles[m.triangle_offset],
                m.triangle_count,
                &vertices[0].position.x,
                vertices.size(),
                sizeof(Vertex)
            );
        }

        // Upload to GPU
        uploadToGPU(result);

        return result;
    }

    void destroy(MeshletData& data) {
        for (int i = 0; i < 4; ++i) {
            if (data.allocations[i]) {
                vmaDestroyBuffer(allocator_,
                    i == 0 ? data.meshlets_buffer :
                    i == 1 ? data.vertices_buffer :
                    i == 2 ? data.triangles_buffer : data.bounds_buffer,
                    data.allocations[i]);
            }
        }
    }

private:
    VmaAllocator allocator_;

    void uploadToGPU(MeshletData& data) {
        // Создание буферов для meshlets, vertices, triangles, bounds
        // ...
    }
};

// Culling функции
namespace MeshletCulling {

bool frustumCull(const meshopt_Bounds& bounds, const glm::vec4 frustum_planes[6]) {
    glm::vec3 center(bounds.center[0], bounds.center[1], bounds.center[2]);

    for (int i = 0; i < 6; ++i) {
        float distance = glm::dot(glm::vec3(frustum_planes[i]), center) + frustum_planes[i].w;
        if (distance < -bounds.radius) {
            return true;  // Culled
        }
    }
    return false;
}

bool backfaceCull(const meshopt_Bounds& bounds, const glm::vec3& camera_pos) {
    glm::vec3 center(bounds.center[0], bounds.center[1], bounds.center[2]);
    glm::vec3 axis(bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2]);

    glm::vec3 view_dir = glm::normalize(center - camera_pos);
    float d = glm::dot(view_dir, axis);

    return d >= bounds.cone_cutoff;
}

std::vector<uint32_t> getVisibleMeshlets(
    const MeshletData& data,
    const glm::vec4 frustum_planes[6],
    const glm::vec3& camera_pos
) {
    std::vector<uint32_t> visible;
    visible.reserve(data.meshlet_count);

    for (uint32_t i = 0; i < data.meshlet_count; ++i) {
        if (frustumCull(data.bounds[i], frustum_planes)) continue;
        if (backfaceCull(data.bounds[i], camera_pos)) continue;

        visible.push_back(i);
    }

    return visible;
}

}  // namespace MeshletCulling
```

---

## Пример 4: Сжатие и загрузка

```cpp
#include <meshoptimizer.h>
#include <vector>
#include <fstream>

struct CompressedMeshFile {
    // Header
    uint32_t magic = 0x4D455348;  // "MESH"
    uint32_t version = 1;
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t vertex_size;
    uint32_t vertex_data_size;
    uint32_t index_data_size;

    // Data follows after header
};

class MeshCompressor {
public:
    bool saveCompressed(
        const std::string& path,
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices
    ) {
        // Encode vertex buffer
        size_t vb_bound = meshopt_encodeVertexBufferBound(vertices.size(), sizeof(Vertex));
        std::vector<uint8_t> encoded_vertices(vb_bound);
        size_t vb_size = meshopt_encodeVertexBuffer(
            encoded_vertices.data(), vb_bound,
            vertices.data(), vertices.size(), sizeof(Vertex)
        );
        encoded_vertices.resize(vb_size);

        // Encode index buffer
        size_t ib_bound = meshopt_encodeIndexBufferBound(indices.size(), vertices.size());
        std::vector<uint8_t> encoded_indices(ib_bound);
        size_t ib_size = meshopt_encodeIndexBuffer(
            encoded_indices.data(), ib_bound,
            indices.data(), indices.size()
        );
        encoded_indices.resize(ib_size);

        // Write file
        CompressedMeshFile header;
        header.vertex_count = vertices.size();
        header.index_count = indices.size();
        header.vertex_size = sizeof(Vertex);
        header.vertex_data_size = vb_size;
        header.index_data_size = ib_size;

        std::ofstream file(path, std::ios::binary);
        if (!file) return false;

        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        file.write(reinterpret_cast<const char*>(encoded_vertices.data()), vb_size);
        file.write(reinterpret_cast<const char*>(encoded_indices.data()), ib_size);

        printf("Saved: %u vertices, %u indices\n", header.vertex_count, header.index_count);
        printf("Compressed: %zu bytes (vertices) + %zu bytes (indices)\n", vb_size, ib_size);
        printf("Original: %zu bytes (vertices) + %zu bytes (indices)\n",
            vertices.size() * sizeof(Vertex), indices.size() * sizeof(uint32_t));

        return true;
    }

    bool loadCompressed(
        const std::string& path,
        std::vector<Vertex>& vertices,
        std::vector<uint32_t>& indices
    ) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;

        CompressedMeshFile header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        if (header.magic != 0x4D455348 || header.version != 1) {
            return false;
        }

        // Read encoded data
        std::vector<uint8_t> encoded_vertices(header.vertex_data_size);
        std::vector<uint8_t> encoded_indices(header.index_data_size);

        file.read(reinterpret_cast<char*>(encoded_vertices.data()), header.vertex_data_size);
        file.read(reinterpret_cast<char*>(encoded_indices.data()), header.index_data_size);

        // Decode
        vertices.resize(header.vertex_count);
        indices.resize(header.index_count);

        int vres = meshopt_decodeVertexBuffer(
            vertices.data(), header.vertex_count, header.vertex_size,
            encoded_vertices.data(), encoded_vertices.size()
        );

        if (vres != 0) {
            printf("Vertex decode error: %d\n", vres);
            return false;
        }

        int ires = meshopt_decodeIndexBuffer(
            indices.data(), header.index_count, sizeof(uint32_t),
            encoded_indices.data(), encoded_indices.size()
        );

        if (ires != 0) {
            printf("Index decode error: %d\n", ires);
            return false;
        }

        return true;
    }

    void printCompressionStats(
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices
    ) {
        size_t original_vb = vertices.size() * sizeof(Vertex);
        size_t original_ib = indices.size() * sizeof(uint32_t);

        size_t vb_bound = meshopt_encodeVertexBufferBound(vertices.size(), sizeof(Vertex));
        size_t ib_bound = meshopt_encodeIndexBufferBound(indices.size(), vertices.size());

        std::vector<uint8_t> encoded_vb(vb_bound);
        std::vector<uint8_t> encoded_ib(ib_bound);

        size_t vb_size = meshopt_encodeVertexBuffer(
            encoded_vb.data(), vb_bound, vertices.data(), vertices.size(), sizeof(Vertex));
        size_t ib_size = meshopt_encodeIndexBuffer(
            encoded_ib.data(), ib_bound, indices.data(), indices.size());

        printf("=== Compression Stats ===\n");
        printf("Vertices: %zu (%zu bytes)\n", vertices.size(), original_vb);
        printf("Triangles: %zu (%zu bytes)\n", indices.size() / 3, original_ib);
        printf("\n");
        printf("Vertex compression: %zu -> %zu bytes (%.1f%%)\n",
            original_vb, vb_size, 100.0f * vb_size / original_vb);
        printf("Index compression: %zu -> %zu bytes (%.1f%%)\n",
            original_ib, ib_size, 100.0f * ib_size / original_ib);
        printf("Total: %zu -> %zu bytes (%.1f%%)\n",
            original_vb + original_ib, vb_size + ib_size,
            100.0f * (vb_size + ib_size) / (original_vb + original_ib));
    }
};
```

---

## Пример 5: Интеграция с flecs

```cpp
#include <flecs.h>
#include <meshoptimizer.h>
#include <vk_mem_alloc.h>

// Components
ECS_COMPONENT_DECLARE(StaticMesh);
ECS_COMPONENT_DECLARE(MeshLODs);
ECS_COMPONENT_DECLARE(MeshletGeometry);
ECS_COMPONENT_DECLARE(MeshMetrics);

// Systems
void OptimizeMeshSystem(ecs_iter_t* it) {
    StaticMesh* mesh = ecs_field(it, StaticMesh, 1);
    MeshMetrics* metrics = ecs_field(it, MeshMetrics, 2);

    for (int i = 0; i < it->count; i++) {
        if (mesh[i].vertex_buffer == VK_NULL_HANDLE) continue;
        if (metrics[i].acmr > 0) continue;  // Already optimized

        // Get mesh data from GPU (simplified)
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        downloadMeshFromGPU(mesh[i], vertices, indices);

        // Optimize
        meshopt_optimizeVertexCache(
            indices.data(), indices.data(), indices.size(), vertices.size());
        meshopt_optimizeVertexFetch(
            vertices.data(), indices.data(), indices.size(),
            vertices.data(), vertices.size(), sizeof(Vertex));

        // Update metrics
        auto stats = meshopt_analyzeVertexCache(
            indices.data(), indices.size(), vertices.size(), 16, 0, 0);
        metrics[i].acmr = stats.acmr;
        metrics[i].atvr = stats.atvr;

        // Re-upload to GPU
        updateMeshOnGPU(mesh[i], vertices, indices);
    }
}

void GenerateLODSystem(ecs_iter_t* it) {
    StaticMesh* mesh = ecs_field(it, StaticMesh, 1);
    MeshLODs* lods = ecs_field(it, MeshLODs, 2);

    for (int i = 0; i < it->count; i++) {
        if (mesh[i].index_count < 300) continue;
        if (lods[i].lod_count > 0) continue;

        // Generate LODs...
        lods[i].lod_count = 4;
        // ...
    }
}

void MeshletCullingSystem(ecs_iter_t* it) {
    MeshletGeometry* meshlet = ecs_field(it, MeshletGeometry, 1);

    // Get camera data from singleton
    const CameraData* camera = ecs_singleton_get(it->world, CameraData);

    for (int i = 0; i < it->count; i++) {
        // Cull meshlets
        auto visible = MeshletCulling::getVisibleMeshlets(
            meshlet[i],
            camera->frustum_planes,
            camera->position
        );

        // Update draw commands
        meshlet[i].visible_count = visible.size();
    }
}

// Module initialization
void MeshOptimizerModuleImport(ecs_world_t* world) {
    ECS_MODULE(world, MeshOptimizerModule);

    ECS_COMPONENT_DEFINE(world, StaticMesh);
    ECS_COMPONENT_DEFINE(world, MeshLODs);
    ECS_COMPONENT_DEFINE(world, MeshletGeometry);
    ECS_COMPONENT_DEFINE(world, MeshMetrics);

    // Systems
    ECS_SYSTEM(world, OptimizeMeshSystem, EcsOnLoad, StaticMesh, MeshMetrics);
    ECS_SYSTEM(world, GenerateLODSystem, EcsOnLoad, StaticMesh, MeshLODs);
    ECS_SYSTEM(world, MeshletCullingSystem, EcsPreUpdate, MeshletGeometry);
}

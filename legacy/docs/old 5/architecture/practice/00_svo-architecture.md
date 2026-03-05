# SVO Architecture: Sparse Voxel Octree [🔴 Уровень 3]

**🔴 Уровень 3: Продвинутый** — Sparse Voxel Octree как **основная архитектура** для хранения и рендеринга вокселей в
ProjectV.

## Важное объявление

SVO (Sparse Voxel Octree) — это **архитектурный паттерн** ProjectV, который **сосуществует с чанками**, а не заменяет
их. См. раздел [SVO + Chunks Hybrid Architecture](#svo--chunks-hybrid-architecture).

SVO используется для:

- **Хранения воксельного мира** — компактное представление разреженных данных
- **GPU-driven рендеринга** — прямой ray marching по октодереву
- **Автоматического LOD** — иерархическая структура = встроенные уровни детализации
- **Compute/Mesh Shaders** — современный подход к генерации геометрии

---

## Архитектура

### Схема данных

```
┌─────────────────────────────────────────────────────────────────┐
│                    Voxel World (SVO)                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│    [Root Node] ───────────────────────────────────────────────  │
│       │                                                         │
│       ├─ [Node 0] ─┬─ [Leaf: Voxel Data]                       │
│       │            └─ [Node 0.0] ─── [Leaf]                     │
│       ├─ [Node 1] ─── [Leaf: Voxel Data]                       │
│       ├─ [Node 2] ─── Empty (не существует)                     │
│       └─ ...                                                     │
│                                                                 │
│    GPU Buffer: Compact Node Array + Voxel Pool                  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Ключевые компоненты

| Компонент          | Назначение                                   |
|--------------------|----------------------------------------------|
| **SVO Tree**       | Иерархическое разреженное октодерево         |
| **Node Pool**      | GPU буфер для узлов SVO                      |
| **Voxel Pool**     | GPU буфер для данных вокселей                |
| **SVO Builder**    | Построение SVO из исходных данных            |
| **SVO Updater**    | Инкрементальное обновление при изменениях    |
| **Ray Marcher**    | GPU шейдер для трассировки лучей             |
| **Mesh Generator** | Compute/Mesh Shader для извлечения геометрии |

---

## 1. Структура узла SVO

### 1.1 Компактный формат (64 бита)

```cpp
namespace projectv {

// Узел SVO — 64 бита (8 байт)
struct SVONode {
    uint64_t data;

    // Распаковка полей
    [[nodiscard]] uint8_t childMask() const noexcept {
        return static_cast<uint8_t>((data >> 56) & 0xFF);
    }

    [[nodiscard]] uint32_t childPtr() const noexcept {
        return static_cast<uint32_t>((data >> 28) & 0x0FFFFFFF);
    }

    [[nodiscard]] uint32_t voxelPtr() const noexcept {
        return static_cast<uint32_t>(data & 0x0FFFFFFF);
    }

    // Упаковка полей
    void setChildMask(uint8_t mask) noexcept {
        data = (data & ~(0xFFULL << 56)) | (static_cast<uint64_t>(mask) << 56);
    }

    void setChildPtr(uint32_t ptr) noexcept {
        data = (data & ~(0x0FFFFFFFULL << 28)) | (static_cast<uint64_t>(ptr) << 28);
    }

    void setVoxelPtr(uint32_t ptr) noexcept {
        data = (data & ~0x0FFFFFFFULL) | ptr;
    }

    // Проверки
    [[nodiscard]] bool hasChildren() const noexcept {
        return childMask() != 0;
    }

    [[nodiscard]] bool isLeaf() const noexcept {
        return childMask() == 0 && voxelPtr() != 0;
    }

    [[nodiscard]] bool isEmpty() const noexcept {
        return data == 0;
    }

    [[nodiscard]] bool hasChild(uint8_t index) const noexcept {
        return (childMask() & (1 << index)) != 0;
    }
};

static_assert(sizeof(SVONode) == 8, "SVONode must be 8 bytes");

} // namespace projectv
```

### 1.2 Данные вокселя

```cpp
namespace projectv {

// Данные вокселя — 32 бита (4 байта)
struct VoxelData {
    uint16_t materialId;    // ID материала
    uint8_t density;        // Плотность (для физики)
    uint8_t flags;          // Флаги

    struct glaze {
        using T = VoxelData;
        static constexpr auto value = glz::object(
            "materialId", &T::materialId,
            "density", &T::density,
            "flags", &T::flags
        );
    };
};

// Материал вокселя
struct VoxelMaterial {
    glm::vec4 baseColor{1.0f};
    glm::vec4 emissive{0.0f};
    float roughness{0.5f};
    float metallic{0.0f};
    float transmission{0.0f};
    uint32_t flags{0};

    struct glaze {
        using T = VoxelMaterial;
        static constexpr auto value = glz::object(
            "baseColor", &T::baseColor,
            "emissive", &T::emissive,
            "roughness", &T::roughness,
            "metallic", &T::metallic,
            "transmission", &T::transmission
        );
    };
};

} // namespace projectv
```

---

## 2. GPU Representation

### 2.1 SSBO для SVO

```cpp
namespace projectv {

struct SVOGPUData {
    VkBuffer nodeBuffer;        // Узлы SVO
    VmaAllocation nodeAlloc;

    VkBuffer voxelBuffer;       // Данные вокселей
    VmaAllocation voxelAlloc;

    VkBuffer materialBuffer;    // Материалы
    VmaAllocation materialAlloc;

    uint32_t nodeCount;
    uint32_t voxelCount;
    uint32_t materialCount;
    uint32_t maxDepth;

    glm::ivec3 worldSize;       // Размер мира в вокселях
    glm::vec3 worldOrigin;      // Начало координат
};

class SVORenderer {
public:
    std::expected<void, InitError> initialize(VkDevice device, VmaAllocator allocator) {
        device_ = device;
        allocator_ = allocator;

        // Создаём буферы
        auto nodeResult = createBuffer(
            sizeof(SVONode) * INITIAL_NODE_CAPACITY,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY
        );
        if (!nodeResult) return std::unexpected(nodeResult.error());
        gpuData_.nodeBuffer = *nodeResult;

        auto voxelResult = createBuffer(
            sizeof(VoxelData) * INITIAL_VOXEL_CAPACITY,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY
        );
        if (!voxelResult) return std::unexpected(voxelResult.error());
        gpuData_.voxelBuffer = *voxelResult;

        return {};
    }

    void uploadSVO(const std::vector<SVONode>& nodes,
                   const std::vector<VoxelData>& voxels) {
        // Загрузка узлов в GPU
        void* mapped;
        vmaMapMemory(allocator_, gpuData_.nodeAlloc, &mapped);
        std::memcpy(mapped, nodes.data(), nodes.size() * sizeof(SVONode));
        vmaUnmapMemory(allocator_, gpuData_.nodeAlloc);

        // Загрузка вокселей
        vmaMapMemory(allocator_, gpuData_.voxelAlloc, &mapped);
        std::memcpy(mapped, voxels.data(), voxels.size() * sizeof(VoxelData));
        vmaUnmapMemory(allocator_, gpuData_.voxelAlloc);

        gpuData_.nodeCount = static_cast<uint32_t>(nodes.size());
        gpuData_.voxelCount = static_cast<uint32_t>(voxels.size());
    }

private:
    VkDevice device_;
    VmaAllocator allocator_;
    SVOGPUData gpuData_;

    static constexpr uint32_t INITIAL_NODE_CAPACITY = 1024 * 1024;
    static constexpr uint32_t INITIAL_VOXEL_CAPACITY = 4 * 1024 * 1024;
};

} // namespace projectv
```

---

## 3. SVO Builder

### 3.1 Построение из плотного массива

```cpp
namespace projectv {

class SVOBuilder {
public:
    struct BuildResult {
        std::vector<SVONode> nodes;
        std::vector<VoxelData> voxels;
        uint32_t rootIndex;
        uint32_t maxDepth;
    };

    BuildResult build(const std::vector<VoxelData>& denseData,
                      uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ,
                      uint32_t maxDepth = 10) {
        BuildResult result;
        result.maxDepth = maxDepth;

        // Резервируем место для корня
        result.nodes.push_back(SVONode{});
        result.rootIndex = 0;

        // Рекурсивное построение
        buildNode(result, denseData, sizeX, sizeY, sizeZ,
                  0, 0, 0, sizeX, 0, maxDepth);

        return result;
    }

private:
    void buildNode(BuildResult& result,
                   const std::vector<VoxelData>& denseData,
                   uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ,
                   uint32_t x, uint32_t y, uint32_t z,
                   uint32_t size, uint32_t depth, uint32_t maxDepth) {

        // Базовый случай: листовой узел
        if (depth == maxDepth || size == 1) {
            uint32_t voxelIdx = static_cast<uint32_t>(result.voxels.size());
            result.voxels.push_back(denseData[x + y * sizeX + z * sizeX * sizeY]);

            SVONode leaf;
            leaf.setVoxelPtr(voxelIdx);
            result.nodes.push_back(leaf);
            return;
        }

        // Проверяем дочерние октанты
        uint8_t childMask = 0;
        std::array<uint32_t, 8> childIndices{};
        uint32_t childSize = size / 2;

        for (uint32_t i = 0; i < 8; ++i) {
            uint32_t cx = x + ((i & 1) ? childSize : 0);
            uint32_t cy = y + ((i & 2) ? childSize : 0);
            uint32_t cz = z + ((i & 4) ? childSize : 0);

            if (hasVoxelsInRegion(denseData, sizeX, sizeY, sizeZ,
                                   cx, cy, cz, childSize)) {
                childMask |= (1 << i);
                childIndices[i] = static_cast<uint32_t>(result.nodes.size());

                // Резервируем место для дочернего узла
                result.nodes.push_back(SVONode{});

                // Рекурсивное построение
                buildNode(result, denseData, sizeX, sizeY, sizeZ,
                          cx, cy, cz, childSize, depth + 1, maxDepth);
            }
        }

        // Если все дети пустые — не создаём узел
        if (childMask == 0) {
            result.nodes.pop_back();
            return;
        }

        // Создаём внутренний узел
        SVONode node;
        node.setChildMask(childMask);
        node.setChildPtr(childIndices[0]);  // Первый дочерний узел
        result.nodes.push_back(node);
    }

    bool hasVoxelsInRegion(const std::vector<VoxelData>& data,
                           uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ,
                           uint32_t x, uint32_t y, uint32_t z,
                           uint32_t size) const {
        for (uint32_t dx = 0; dx < size; ++dx) {
            for (uint32_t dy = 0; dy < size; ++dy) {
                for (uint32_t dz = 0; dz < size; ++dz) {
                    const auto& voxel = data[(x + dx) + (y + dy) * sizeX + (z + dz) * sizeX * sizeY];
                    if (voxel.materialId != 0) {  // Не воздух
                        return true;
                    }
                }
            }
        }
        return false;
    }
};

} // namespace projectv
```

---

## 4. GPU Ray Marching

### 4.1 Slang шейдер для трассировки

```slang
// svo_ray_marching.slang
module SVORayMarching;

import SVOStructures;

// SVO данные
[[vk::binding(0, 0)]]
RWStructuredBuffer<SVONode> svoNodes;

[[vk::binding(1, 0)]]
StructuredBuffer<VoxelData> voxelData;

[[vk::binding(2, 0)]]
StructuredBuffer<VoxelMaterial> materials;

// Результат
[[vk::binding(3, 0)]]
RWTexture2D<float4> outputImage;

// Параметры
[[vk::push_constant]]
struct RayMarchParams {
    float4x4 invViewProj;
    float3 cameraPos;
    float maxDistance;
    uint rootNode;
    uint maxDepth;
    float voxelSize;
} params;

// Стек для итеративного обхода
struct TraversalStack {
    uint nodeIndex;
    float tMin;
    float tMax;
    uint depth;
};

// Пересечение луча с AABB
bool intersectAABB(float3 origin, float3 dir, float3 boxMin, float3 boxMax,
                   out float tMin, out float tMax) {
    float3 t0 = (boxMin - origin) / dir;
    float3 t1 = (boxMax - origin) / dir;

    tMin = max(max(min(t0.x, t1.x), min(t0.y, t1.y)), min(t0.z, t1.z));
    tMax = min(min(max(t0.x, t1.x), max(t0.y, t1.y)), max(t0.z, t1.z));

    return tMax >= max(tMin, 0.0);
}

// Основной шейдер
[numthreads(8, 8, 1)]
void csMain(uint3 tid : SV_DispatchThreadID) {
    uint2 resolution;
    outputImage.GetDimensions(resolution.x, resolution.y);

    if (any(tid.xy >= resolution)) return;

    // Генерация луча
    float2 uv = (float2(tid.xy) + 0.5) / float2(resolution);
    float2 ndc = uv * 2.0 - 1.0;

    float4 worldPos = mul(params.invViewProj, float4(ndc, 1.0, 1.0));
    worldPos /= worldPos.w;

    float3 ro = params.cameraPos;
    float3 rd = normalize(worldPos.xyz - ro);

    // Инициализация стека
    TraversalStack stack[24];  // Макс глубина SVO
    int stackPtr = 0;

    stack[0].nodeIndex = params.rootNode;
    stack[0].tMin = 0.0;
    stack[0].tMax = params.maxDistance;
    stack[0].depth = 0;

    float3 hitColor = float3(0.1, 0.2, 0.3);  // Цвет фона
    bool hit = false;
    float3 hitNormal;
    VoxelMaterial hitMaterial;

    // Обход SVO
    while (stackPtr >= 0 && !hit) {
        TraversalStack current = stack[stackPtr];
        stackPtr--;

        SVONode node = svoNodes[current.nodeIndex];

        // Листовой узел — проверяем воксель
        if (node.isLeaf()) {
            VoxelData voxel = voxelData[node.voxelPtr()];
            hitMaterial = materials[voxel.materialId];
            hit = true;
            hitColor = hitMaterial.baseColor.rgb;
            break;
        }

        // Внутренний узел — добавляем детей в стек
        if (node.hasChildren()) {
            uint childBase = node.childPtr();
            uint8_t mask = node.childMask();

            for (uint i = 0; i < 8; ++i) {
                if ((mask & (1 << i)) == 0) continue;

                // Вычисляем bounds дочернего узла
                float3 childMin, childMax;
                // ... вычисление bounds на основе depth и index

                float tMin, tMax;
                if (intersectAABB(ro, rd, childMin, childMax, tMin, tMax)) {
                    stackPtr++;
                    stack[stackPtr].nodeIndex = childBase + i;
                    stack[stackPtr].tMin = tMin;
                    stack[stackPtr].tMax = tMax;
                    stack[stackPtr].depth = current.depth + 1;
                }
            }
        }
    }

    // Простое затенение
    if (hit) {
        float3 L = normalize(float3(1, 2, 1));
        float diffuse = max(dot(hitNormal, L), 0.0);
        float ambient = 0.1;

        hitColor = hitColor * (ambient + diffuse);
        hitColor += hitMaterial.emissive.rgb;
    }

    outputImage[tid.xy] = float4(hitColor, 1.0);
}
```

---

## 5. Mesh Shader Generation

### 5.1 Извлечение геометрии из SVO

```slang
// svo_mesh_generator.slang
module SVOMeshGenerator;

import SVOStructures;

[[vk::binding(0, 0)]]
StructuredBuffer<SVONode> svoNodes;

[[vk::binding(1, 0)]]
StructuredBuffer<VoxelData> voxelData;

// Output: meshlets
struct MeshletVertex {
    float3 position;
    float3 normal;
    float2 uv;
    uint materialId;
};

struct Meshlet {
    uint vertexOffset;
    uint indexOffset;
    uint vertexCount;
    uint triangleCount;
};

// Mesh Shader entry point
[shader("mesh")]
[numthreads(32, 1, 1)]
[outputtopology("triangle")]
void msMain(
    uint3 tid : SV_DispatchThreadID,
    out vertices MeshletVertex vertices[64],
    out indices uint3 triangles[124],
    out primitives Meshlet primitives[1]
) {
    // Каждый workgroup обрабатывает один узел SVO
    uint nodeIndex = tid.x;
    if (nodeIndex >= svoNodes.Count()) return;

    SVONode node = svoNodes[nodeIndex];

    // Только листовые узлы генерируют геометрию
    if (!node.isLeaf()) return;

    VoxelData voxel = voxelData[node.voxelPtr()];

    // Генерация вершин для куба вокселя
    // ... (6 граней, 24 вершины, 12 треугольников)

    primitives[0].vertexCount = 24;
    primitives[0].triangleCount = 12;
}

[shader("fragment")]
float4 fsMain(MeshletVertex input) : SV_Target {
    // Простое затенение
    return float4(input.normal * 0.5 + 0.5, 1.0);
}
```

---

## 6. Интеграция с ECS

### 6.1 Компоненты для SVO

```cpp
namespace projectv {

// Компонент для связи сущности с SVO
struct SVONodeRef {
    uint32_t nodeIndex;
    uint32_t depth;
    glm::ivec3 coord;
};

// Компонент для региона SVO
struct SVORegion {
    glm::ivec3 min;
    glm::ivec3 max;
    uint32_t rootNode;
    bool needsRebuild;
};

// Система для обновления SVO
ecs.system<SVORegion>("UpdateSVORegions")
    .kind(flecs::OnUpdate)
    .iter([](flecs::iter& it, SVORegion* regions) {
        auto* svo = it.world().ctx<SVORenderer>();

        for (auto i : it) {
            if (regions[i].needsRebuild) {
                svo->rebuildRegion(regions[i]);
                regions[i].needsRebuild = false;
            }
        }
    });

// Observer для изменений вокселей
ecs.observer<VoxelChange>()
    .event(flecs::OnSet)
    .each([](flecs::entity e, VoxelChange& change) {
        auto* svo = e.world().ctx<SVORenderer>();
        svo->updateVoxel(change.coord, change.newValue);
    });

} // namespace projectv
```

---

## 7. Оптимизации

### 7.1 DAG (Directed Acyclic Graph)

```cpp
namespace projectv {

// SVO → DAG: объединение идентичных поддеревьев
class SVODAGCompressor {
public:
    void compress(std::vector<SVONode>& nodes) {
        std::unordered_map<size_t, uint32_t> hashMap;

        // Проход снизу вверх
        for (int32_t i = static_cast<int32_t>(nodes.size()) - 1; i >= 0; --i) {
            size_t hash = computeNodeHash(nodes, i);

            if (auto it = hashMap.find(hash); it != hashMap.end()) {
                // Найден дубликат — перенаправляем ссылки
                redirectReferences(nodes, i, it->second);
                nodes[i].data = 0;  // Пометить для удаления
            } else {
                hashMap[hash] = static_cast<uint32_t>(i);
            }
        }

        // Компактификация
        compactNodes(nodes);
    }

private:
    size_t computeNodeHash(const std::vector<SVONode>& nodes, uint32_t index) const {
        const SVONode& node = nodes[index];
        size_t h = std::hash<uint64_t>{}(node.data);

        if (node.hasChildren()) {
            uint32_t childBase = node.childPtr();
            for (int i = 0; i < 8; ++i) {
                if (node.hasChild(i)) {
                    h ^= computeNodeHash(nodes, childBase + i) << (i * 4);
                }
            }
        }

        return h;
    }
};

} // namespace projectv
```

### 7.2 Потоковая загрузка

```cpp
namespace projectv {

class StreamingSVO {
public:
    // Загрузка региона по запросу
    std::expected<uint32_t, StreamError> loadRegion(const glm::ivec3& coord) {
        // Проверяем кэш
        auto it = regionCache_.find(coord);
        if (it != regionCache_.end()) {
            touchRegion(it->second);
            return it->second.nodeIndex;
        }

        // Загружаем с диска
        auto data = loadFromDisk(coord);
        if (!data) {
            return std::unexpected(StreamError::NotFound);
        }

        // Интегрируем в основное дерево
        uint32_t nodeIndex = integrateIntoTree(*data);

        // Добавляем в кэш
        regionCache_[coord] = {nodeIndex, getCurrentTime()};

        return nodeIndex;
    }

    // Выгрузка неиспользуемых регионов
    void unloadUnused(float maxAge) {
        auto now = getCurrentTime();

        for (auto it = regionCache_.begin(); it != regionCache_.end(); ) {
            if (now - it->second.lastAccess > maxAge) {
                removeFromTree(it->second.nodeIndex);
                it = regionCache_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    struct CachedRegion {
        uint32_t nodeIndex;
        float lastAccess;
    };

    std::unordered_map<glm::ivec3, CachedRegion> regionCache_;
};

} // namespace projectv
```

---

## 8. Производительность

### Ожидаемые показатели

| Операция                      | Время  | Память              |
|-------------------------------|--------|---------------------|
| Построение SVO (1M вокселей)  | ~50 ms | ~16 MB              |
| Ray Marching (1920x1080)      | ~5 ms  | -                   |
| Mesh Generation (100K voxels) | ~10 ms | -                   |
| DAG сжатие                    | ~20 ms | 50-70% от оригинала |

### Рекомендации

1. **Глубина SVO**: 8-12 уровней для большинства миров
2. **DAG сжатие**: Использовать для статических регионов
3. **Потоковая загрузка**: Для миров > 1 км³
4. **Mesh Shader**: Предпочтительнее ray marching для близких объектов

---

## SVO + Chunks Hybrid Architecture

**🟡 Уровень 2: Средний** — Гибридный подход, объединяющий преимущества SVO и чанков.

### Проблема: Выбор между SVO и Chunks

| Подход     | Преимущества                                  | Недостатки                            |
|------------|-----------------------------------------------|---------------------------------------|
| **SVO**    | Compact storage, встроенный LOD, ray marching | Сложность обновлений, неточная физика |
| **Chunks** | Простота, точная физика, быстрые обновления   | Дублирование данных, нет LOD          |

### Решение: Гибридная архитектура

ProjectV использует **оба подхода одновременно**, каждый для своих задач:

```
┌─────────────────────────────────────────────────────────────────┐
│                    Hybrid Voxel Architecture                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              SVO (Sparse Voxel Octree)                   │   │
│  │                                                         │   │
│  │  ✓ Глобальное освещение (GI)                            │   │
│  │  ✓ Ray Marching для теней и отражений                   │   │
│  │  ✓ Дальний LOD (Level of Detail)                        │   │
│  │  ✓ Compact storage для разреженных миров                │   │
│  │  ✓ Cone Tracing для ambient occlusion                   │   │
│  │                                                         │   │
│  │  Используется для:                                      │   │
│  │  - Визуализация далёких регионов (LOD 2+)               │   │
│  │  - Global Illumination                                  │   │
│  │  - Shadow rays                                          │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                  │
│                              ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              Chunks (32³ voxels)                         │   │
│  │                                                         │   │
│  │  ✓ Ближняя геометрия (LOD 0-1)                          │   │
│  │  ✓ Greedy Meshing для оптимизации                       │   │
│  │  ✓ Точная коллизия с Jolt Physics                       │   │
│  │  ✓ Быстрые локальные изменения                          │   │
│  │  ✓ DDA Raycast для взаимодействия                       │   │
│  │                                                         │   │
│  │  Используется для:                                      │   │
│  │  - Игрок в радиусе N чанков                             │   │
│  │  - Разрушение и строительство                           │   │
│  │  - Физические коллизии                                  │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Принципы гибридной архитектуры

#### 1. Разделение по расстоянию

```cpp
enum class VoxelLODStrategy {
    ChunkMesh,      // 0-50м: классические чанки с Greedy Meshing
    ChunkSVO,       // 50-200м: чанки с упрощённым SVO
    GlobalSVO,      // 200м+: глобальное SVO для ray marching
};

VoxelLODStrategy selectLODStrategy(float distance, const RenderConfig& config) {
    if (distance < config.chunkMeshDistance) {
        return VoxelLODStrategy::ChunkMesh;
    } else if (distance < config.chunkSVODistance) {
        return VoxelLODStrategy::ChunkSVO;
    } else {
        return VoxelLODStrategy::GlobalSVO;
    }
}
```

#### 2. Локальное SVO внутри чанков

Каждый чанк содержит мини-SVO для быстрого ray marching:

```cpp
struct HybridChunk {
    // Классические данные чанка
    std::vector<VoxelData> voxels;       // 32³ = 32768 вокселей
    MeshHandle mesh;                      // Greedy Meshing результат

    // Локальное SVO (опционально)
    std::optional<ChunkSVO> localSVO;    // Мини-дерево для ray casting

    // Метаданные
    glm::ivec3 position;
    uint32_t lodLevel;
    bool needsMeshRebuild;
    bool needsSVORebuild;
};

struct ChunkSVO {
    std::vector<SVONode> nodes;          // ~100-500 узлов для типичного чанка
    uint32_t rootNode;
    uint8_t maxDepth;                     // Обычно 5-6 уровней
};
```

#### 3. Глобальное SVO для дальних регионов

```cpp
class GlobalSVOManager {
public:
    // Построение SVO из чанков
    void buildFromChunks(const std::vector<HybridChunk>& chunks) {
        // 1. Собираем "грубые" данные из чанков
        std::vector<VoxelData> lodVoxels;
        for (const auto& chunk : chunks) {
            // Упрощаем чанк до 16³ или 8³
            auto simplified = simplifyChunk(chunk, 2);  // factor = 2
            lodVoxels.insert(lodVoxels.end(),
                            simplified.begin(), simplified.end());
        }

        // 2. Строим SVO из упрощённых данных
        SVOBuilder builder;
        svo_ = builder.build(lodVoxels, worldSizeX, worldSizeY, worldSizeZ, 8);
    }

    // Обновление при изменении чанка
    void onChunkModified(const glm::ivec3& chunkPos) {
        // Помечаем регион SVO для обновления
        dirtyRegions_.push_back(chunkPos);
    }

    // Инкрементальное обновление
    void updateDirtyRegions() {
        for (const auto& pos : dirtyRegions_) {
            updateSVORegion(pos);
        }
        dirtyRegions_.clear();
    }

private:
    std::optional<SVOBuildResult> svo_;
    std::vector<glm::ivec3> dirtyRegions_;
};
```

### Интеграция с физикой (JoltPhysics)

**Важно**: Физика работает ТОЛЬКО с чанками, не с SVO.

```cpp
class VoxelPhysicsBridge {
public:
    // Создание физической геометрии из чанка
    void createPhysicsBody(const HybridChunk& chunk) {
        // Используем mesh из Greedy Meshing
        const auto& mesh = chunk.mesh;

        // Создаём JoltPhysics mesh shape
        JPH::VertexList vertices;
        JPH::IndexedTriangleList indices;

        for (const auto& vertex : mesh.vertices) {
            vertices.push_back(JPH::Float3(vertex.x, vertex.y, vertex.z));
        }

        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
            JPH::IndexedTriangle tri;
            tri.mIdx[0] = mesh.indices[i];
            tri.mIdx[1] = mesh.indices[i + 1];
            tri.mIdx[2] = mesh.indices[i + 2];
            indices.push_back(tri);
        }

        auto shape = new JPH::MeshShape(vertices, indices);
        auto body = physicsSystem_.CreateBody(shape, chunk.position);

        chunkBodies_[chunk.position] = body;
    }

    // Обновление при разрушении
    void onVoxelDestroyed(const glm::ivec3& worldPos) {
        // 1. Определяем чанк
        auto chunkPos = worldToChunkPos(worldPos);

        // 2. Обновляем меш чанка
        auto& chunk = chunks_[chunkPos];
        chunk.needsMeshRebuild = true;

        // 3. Пересоздаём физическое тело
        removePhysicsBody(chunkPos);
        createPhysicsBody(chunk);
    }

private:
    JPH::PhysicsSystem physicsSystem_;
    std::unordered_map<glm::ivec3, JPH::BodyID> chunkBodies_;
    std::unordered_map<glm::ivec3, HybridChunk> chunks_;
};
```

### Рендеринг гибридного мира

```cpp
class HybridVoxelRenderer {
public:
    void render(VkCommandBuffer cmd, const Camera& camera) {
        // 1. Рендеринг ближних чанков (Greedy Meshing)
        renderNearChunks(cmd, camera);

        // 2. Рендеринг средних чанков (Chunk SVO → Mesh)
        renderMidChunks(cmd, camera);

        // 3. Ray Marching для далёких регионов (Global SVO)
        renderFarSVO(cmd, camera);
    }

private:
    void renderNearChunks(VkCommandBuffer cmd, const Camera& camera) {
        // Стандартный рендеринг мешей
        auto visibleChunks = frustumCull(chunks_, camera);

        for (const auto& chunk : visibleChunks) {
            if (chunk->needsMeshRebuild) {
                rebuildChunkMesh(*chunk);
            }

            // Multi-draw indirect
            addDrawCommand(chunk->mesh);
        }

        executeIndirectDraw(cmd);
    }

    void renderMidChunks(VkCommandBuffer cmd, const Camera& camera) {
        // Извлечение геометрии из локального SVO
        for (const auto& chunk : midRangeChunks_) {
            if (!chunk->localSVO) {
                buildChunkSVO(*chunk);
            }

            // Mesh Shader генерирует геометрию из SVO
            dispatchMeshShader(cmd, *chunk->localSVO);
        }
    }

    void renderFarSVO(VkCommandBuffer cmd, const Camera& camera) {
        // Full-screen ray marching
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rayMarchPipeline_);

        // Bind SVO buffers
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                               layout_, 0, 1, &svoDescriptorSet_, 0, nullptr);

        // Dispatch
        vkCmdDispatch(cmd, width_ / 8, height_ / 8, 1);
    }
};
```

### Синхронизация данных

```cpp
class VoxelWorldSync {
public:
    // Изменение вокселя
    void setVoxel(const glm::ivec3& worldPos, VoxelData value) {
        // 1. Обновляем чанк
        auto chunkPos = worldToChunkPos(worldPos);
        auto localPos = worldToLocalPos(worldPos);

        auto& chunk = chunks_[chunkPos];
        chunk.voxels[localPos] = value;
        chunk.needsMeshRebuild = true;
        chunk.needsSVORebuild = true;

        // 2. Помечаем глобальное SVO для обновления
        globalSVO_.markDirty(chunkPos);

        // 3. Обновляем физику
        physicsBridge_.scheduleRebuild(chunkPos);
    }

    // Обновление всех систем
    void update(float deltaTime) {
        // 1. Перестраиваем меши
        for (auto& [pos, chunk] : chunks_) {
            if (chunk.needsMeshRebuild) {
                meshGenerator_.rebuild(chunk);
                chunk.needsMeshRebuild = false;
            }
        }

        // 2. Обновляем физику (с задержкой)
        physicsBridge_.processPending();

        // 3. Обновляем глобальное SVO (фоновый поток)
        if (svoUpdateTimer_.elapsed() > SVO_UPDATE_INTERVAL) {
            globalSVO_.updateDirtyRegions();
            svoUpdateTimer_.reset();
        }
    }

private:
    std::unordered_map<glm::ivec3, HybridChunk> chunks_;
    GlobalSVOManager globalSVO_;
    VoxelPhysicsBridge physicsBridge_;
    ChunkMeshGenerator meshGenerator_;

    static constexpr float SVO_UPDATE_INTERVAL = 0.5f;  // 2 раза в секунду
};
```

### Преимущества гибридного подхода

| Аспект                 | Только SVO           | Только Chunks        | **Hybrid**      |
|------------------------|----------------------|----------------------|-----------------|
| **Производительность** | Медленно для близких | Медленно для далёких | ✓ Оптимально    |
| **Память**             | Компактно            | Дублирование         | ✓ Баланс        |
| **Физика**             | Неточно              | Точно                | ✓ Точно         |
| **LOD**                | Встроенный           | Нужен код            | ✓ Автоматически |
| **Обновления**         | Медленные            | Быстрые              | ✓ Быстрые       |
| **GI/Тени**            | Ray marching         | Shadow maps          | ✓ Ray marching  |

### Когда использовать гибрид

| Сценарий           | Рекомендация                  |
|--------------------|-------------------------------|
| **MVP (курсовой)** | Только Chunks — проще         |
| **Большой мир**    | Hybrid — нужен LOD            |
| **Разрушаемость**  | Hybrid — чанки для физики     |
| **GI и отражения** | Hybrid — SVO для ray marching |

---

## Ссылки

- [Vulkan Dynamic Rendering](./04_modern-vulkan-guide.md)
- [Compute Shaders](../../libraries/slang/15_projectv-examples.md)
- [VMA Integration](../../libraries/vma/01_quickstart.md)
- [Roadmap & Scope](../00_roadmap_and_scope.md) — MVP vs Vision

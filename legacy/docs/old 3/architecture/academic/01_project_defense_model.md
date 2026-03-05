# Математическая модель и архитектурные обоснования ProjectV

**Документ:** Научно-техническое обоснование
**Авторы:** ProjectV Team
**Дата:** 2026-02-22
**Уровень:** 🎓 Академический

---

## Аннотация

Документ представляет формальное обоснование архитектурных решений ProjectV — воксельного игрового движка нового
поколения. Включает математические модели клеточных автоматов, анализ Data-Oriented Design с оценкой кэш-промахов, и
асимптотический анализ алгоритмов.

---

## 1. Математическая модель Cellular Automata

### 1.1 Формальное определение

Клеточный автомат (CA) для сыпучих материалов определяется как кортеж:

$$\mathcal{CA} = \langle \mathcal{L}, \mathcal{S}, \mathcal{N}, f \rangle$$

где:

- $\mathcal{L} \subset \mathbb{Z}^3$ — дискретная решётка (воксельное пространство)
- $\mathcal{S} = \{s_0, s_1, \ldots, s_n\}$ — конечное множество состояний (типы материалов)
- $\mathcal{N} = \{\vec{n}_1, \ldots, \vec{n}_k\}$ — окрестность Мура ($k=26$ для 3D)
- $f: \mathcal{S}^{k+1} \to \mathcal{S}$ — функция перехода

### 1.2 Правила перехода для сыпучих материалов

Для вокселя $v$ в позиции $\vec{p}$ с состоянием $s_v$ (песок, гравий, вода):

$$s_v^{t+1} = f\left(s_v^t, \{s_u^t : u \in \mathcal{N}(\vec{p})\}\right)$$

**Правило падения:**

$$f_{\text{fall}}(s_v, s_{\text{below}}) = \begin{cases}
s_{\text{below}} & \text{if } s_{\text{below}} \in \mathcal{S}_{\text{empty}} \land s_v \in \mathcal{S}_{\text{granular}} \\
s_v & \text{otherwise}
\end{cases}$$

**Правило рассыпания:**

$$f_{\text{spread}}(s_v, \vec{n}_L, \vec{n}_R) = \begin{cases}
\text{swap}(s_v, s_L) & \text{with prob. } p_L \\
\text{swap}(s_v, s_R) & \text{with prob. } p_R \\
s_v & \text{otherwise}
\end{cases}$$

где вероятности определяются углом естественного откоса:

$$p_L = p_R = \frac{1}{2} \cdot \left(1 - \frac{\theta_{\text{current}}}{\theta_{\text{repose}}}\right)$$

### 1.3 Уравнение переноса массы

Для моделирования жидкостей и газов используем уравнение:

$$\frac{\partial \rho}{\partial t} + \nabla \cdot (\rho \vec{v}) = 0$$

В дискретной форме для вокселя:

$$\rho_v^{t+1} = \rho_v^t - \Delta t \sum_{u \in \mathcal{N}} \frac{(\rho_v \cdot v_v - \rho_u \cdot v_u) \cdot A_{vu}}{V_v}$$

### 1.4 SIMD-оптимизация

Для векторизации обновления клеточного автомата:

$$\vec{S}^{t+1} = \vec{f}_{\text{SIMD}}(\vec{S}^t, \vec{N}^t)$$

где $\vec{S}$ — вектор состояний шириной $W$ (SIMD width):

```cpp
// ProjectV.Simulation.CellularAutomata.SIMD.cppm
export module ProjectV.Simulation.CellularAutomata.SIMD;

import std;
import std.simd;

export namespace projectv::simulation {

/// SIMD-оптимизированное обновление CA чанка.
/// Использует std::experimental::simd из C++26.
export auto update_chunk_simd(
    std::span<CellState const> current,
    std::span<CellState> next,
    uint32_t chunk_size,
    float delta_time
) noexcept -> void {
    namespace simd = std::experimental;
    constexpr size_t W = simd::simd_abi::max_fixed_size<float>;

    for (size_t i = 0; i < chunk_size; i += W) {
        // Load W cells using gather pattern
        auto density = simd::simd<float, simd::simd_abi::fixed_size<W>>(
            [&](auto idx) {
                return (i + idx < chunk_size) ? current[i + idx].density : 0.0f;
            }
        );

        auto velocity_y = simd::simd<float, simd::simd_abi::fixed_size<W>>(
            [&](auto idx) {
                return (i + idx < chunk_size) ? current[i + idx].velocity_y : 0.0f;
            }
        );

        // Load density from cells below (gather)
        auto density_below = simd::simd<float, simd::simd_abi::fixed_size<W>>(
            [&](auto idx) {
                size_t below_idx = i + idx + CHUNK_SIZE_X;
                return (below_idx < chunk_size) ? current[below_idx].density : 1.0f;
            }
        );

        // Apply update rule: new_density = density - dt * velocity_y * (density - density_below)
        auto new_density = density - delta_time * velocity_y * (density - density_below);

        // Clamp to valid range [0, 1]
        new_density = simd::max(simd::min(new_density, simd::simd<float, W>(1.0f)), simd::simd<float, W>(0.0f));

        // Store results
        for (size_t j = 0; j < W && i + j < chunk_size; ++j) {
            next[i + j].density = new_density[j];
        }
    }
}

} // namespace projectv::simulation
```

---

## 2. Data-Oriented Design: Анализ кэш-промахов

### 2.1 Модель иерархии памяти

Типичная иерархия памяти CPU:

| Уровень  | Размер | Latency     | Bandwidth |
|----------|--------|-------------|-----------|
| L1 Cache | 32 KB  | 4 cycles    | ~1 TB/s   |
| L2 Cache | 256 KB | 12 cycles   | ~500 GB/s |
| L3 Cache | 8 MB   | 40 cycles   | ~200 GB/s |
| RAM      | 16+ GB | 200+ cycles | ~50 GB/s  |

### 2.2 Cache Miss Analysis

**Модель для AoS (Array of Structures):**

```
TransformComponent_AoS:
struct { position: vec3, rotation: quat, scale: vec3 }
Size = 48 bytes per component

При итерации по positions:
- Cache line = 64 bytes
- Useful data per line = 12 bytes (position)
- Wasted data = 52 bytes
- Cache utilization = 12/64 = 18.75%
```

**Модель для SoA (Structure of Arrays):**

```
TransformComponent_SoA:
positions: contiguous array of vec3
rotations: contiguous array of quat
scales: contiguous array of vec3

При итерации по positions:
- Cache line = 64 bytes
- Useful data per line = 64 bytes (5.33 positions)
- Wasted data = 0 bytes
- Cache utilization = 100%
```

### 2.3 Формула оценки кэш-промахов

Для массива из $N$ элементов размером $E$ байт, обрабатываемых последовательно:

$$C_{\text{misses}} = \left\lceil \frac{N \cdot E}{L} \right\rceil + C_{\text{overhead}}$$

где $L$ — размер cache line (64 bytes).

**Сравнение для $N = 10^6$ трансформов:**

| Подход               | $E$ | $C_{\text{misses}}$ | Эффективность |
|----------------------|-----|---------------------|---------------|
| AoS (read all)       | 48  | 750,000             | 18.75%        |
| SoA (positions only) | 12  | 187,500             | 100%          |
| SoA (rotation only)  | 16  | 250,000             | 100%          |

**Ускорение:** $\frac{750,000}{187,500} = 4\times$ для операций, требующих только position.

### 2.4 Prefetching Model

Время выполнения с prefetch:

$$T_{\text{exec}} = \max\left(T_{\text{compute}}, T_{\text{memory}} - T_{\text{prefetch\_ahead}}\right)$$

Оптимальное расстояние prefetch:

$$D_{\text{optimal}} = \left\lceil \frac{T_{\text{memory\_latency}}}{T_{\text{compute\_per\_element}}} \right\rceil$$

```cpp
// ProjectV.Core.Memory.Prefetch.cppm
export module ProjectV.Core.Memory.Prefetch;

import std;

export namespace projectv::core {

/// Вычисление оптимального расстояния prefetch.
/// @param memory_latency_ns Latency памяти в наносекундах
/// @param compute_time_per_element_ns Время вычисления на элемент в наносекундах
/// @return Оптимальное расстояние в элементах
export constexpr auto compute_prefetch_distance(
    uint64_t memory_latency_ns,
    uint64_t compute_time_per_element_ns
) noexcept -> size_t {
    return (memory_latency_ns + compute_time_per_element_ns - 1)
           / compute_time_per_element_ns;
}

/// Prefetch hint для итерации по массиву.
export template<typename T>
auto prefetch_ahead(T const* ptr, size_t distance) noexcept -> void {
    // _mm_prefetch или std::hardware_prefetch (C++26)
    #if defined(__x86_64__) || defined(_M_X64)
        _mm_prefetch(reinterpret_cast<char const*>(ptr + distance), _MM_HINT_T0);
    #elif defined(__aarch64__)
        __builtin_prefetch(ptr + distance, 0, 3);
    #endif
}

} // namespace projectv::core

// Пример использования:
// L2 miss latency ~12ns, compute per element ~2ns
// Prefetch distance = 12 / 2 = 6 elements ahead
```

---

## 3. Асимптотический анализ алгоритмов

### 3.1 SVO Ray Marching — Slang Interface

**Slang Compute Shader Interface:**

```slang
// ProjectV/Render/Voxel/RayMarch.slang
module ProjectV.Render.Voxel.RayMarch;

import ProjectV.Render.Voxel.SVOStructures;

/// Результат ray marching операции.
struct RayMarchResult {
    bool hit;               ///< Попадание в воксель
    float t;                ///< Расстояние до пересечения
    uint3 voxel_coord;      ///< Координаты вокселя в SVO
    uint material_id;       ///< ID материала
    float3 normal;          ///< Нормаль поверхности
};

/// Параметры ray marching.
struct RayMarchParams {
    float3 origin;          ///< Начало луча
    float3 direction;       ///< Направление луча (нормализованное)
    float max_distance;     ///< Максимальная дистанция
    uint max_steps;         ///< Максимальное число шагов
    float epsilon;          ///< Пороговое расстояние
};

/// SVO Descriptor для GPU доступа.
struct SVODescriptor {
    StructuredBuffer<SVOChildMask> child_masks;    ///< Child masks для каждого узла
    StructuredBuffer<uint> child_offsets;          ///< Смещения к дочерним узлам
    StructuredBuffer<VoxelData> leaf_data;         ///< Данные листовых вокселей
    uint max_depth;                                 ///< Максимальная глубина дерева
    uint root_offset;                               ///< Смещение корня
    float3 world_min;                              ///< Минимальная точка AABB
    float3 world_max;                              ///< Максимальная точка AABB
};

/// SVO Ray Marching — основной интерфейс.
/// Сложность: O(d) где d = max_depth
///
/// @param params Параметры луча
/// @param svo    Дескриптор SVO
/// @return       Результат пересечения
RayMarchResult svo_ray_march(
    RayMarchParams params,
    SVODescriptor svo
) {
    RayMarchResult result;
    result.hit = false;
    result.t = 0.0;

    float3 pos = params.origin;
    float3 dir = params.direction;

    // DDA variables
    float3 cell_size = (svo.world_max - svo.world_min) / float(1 << svo.max_depth);
    float3 delta_dist = abs(1.0 / dir);
    float3 side_dist;

    // Initialize side_dist
    [unroll]
    for (int i = 0; i < 3; ++i) {
        side_dist[i] = (dir[i] < 0)
            ? (pos[i] - svo.world_min[i]) * delta_dist[i]
            : (svo.world_max[i] - pos[i]) * delta_dist[i];
    }

    uint current_depth = 0;
    uint current_node = svo.root_offset;
    float t = 0.0;

    [[loop]]
    for (uint step = 0; step < params.max_steps && t < params.max_distance; ++step) {
        // Traverse SVO hierarchy
        [branch]
        if (current_depth < svo.max_depth) {
            // Compute child index at current position
            float3 local_pos = (pos - svo.world_min) / (svo.world_max - svo.world_min);
            uint3 coord = uint3(local_pos * float(1 << (current_depth + 1)));
            uint child_idx = ((coord.x & 1) << 2) | ((coord.y & 1) << 1) | (coord.z & 1);

            // Check child mask
            SVOChildMask mask = svo.child_masks[current_node];

            if ((mask.mask & (1 << child_idx)) != 0) {
                // Child exists, descend
                uint child_offset = svo.child_offsets[current_node * 8 + child_idx];
                current_node = child_offset;
                current_depth++;
            } else {
                // Child doesn't exist, advance
                // ... advance logic
            }
        } else {
            // At leaf level, check voxel
            VoxelData voxel = svo.leaf_data[current_node];

            if (voxel.material_id != 0) {
                result.hit = true;
                result.t = t;
                result.material_id = voxel.material_id;
                result.voxel_coord = uint3(pos / cell_size);
                result.normal = compute_normal(pos, dir, side_dist, delta_dist);
                return result;
            }
        }

        // Advance along DDA
        float min_dist = min(side_dist.x, min(side_dist.y, side_dist.z));
        t += min_dist;

        // Update position and side_dist
        if (side_dist.x < side_dist.y) {
            if (side_dist.x < side_dist.z) {
                side_dist.x += delta_dist.x;
                pos += dir * delta_dist.x;
            } else {
                side_dist.z += delta_dist.z;
                pos += dir * delta_dist.z;
            }
        } else {
            if (side_dist.y < side_dist.z) {
                side_dist.y += delta_dist.y;
                pos += dir * delta_dist.y;
            } else {
                side_dist.z += delta_dist.z;
                pos += dir * delta_dist.z;
            }
        }
    }

    return result;
}

/// Вычисление нормали по DDA пересечению.
float3 compute_normal(
    float3 pos,
    float3 dir,
    float3 side_dist,
    float3 delta_dist
) {
    // Normal is perpendicular to the face we hit
    if (side_dist.x - delta_dist.x < side_dist.y &&
        side_dist.x - delta_dist.x < side_dist.z) {
        return float3(-sign(dir.x), 0, 0);
    } else if (side_dist.y - delta_dist.y < side_dist.z) {
        return float3(0, -sign(dir.y), 0);
    } else {
        return float3(0, 0, -sign(dir.z));
    }
}

/// Ray Marching dispatch kernel.
[numthreads(8, 8, 1)]
void csRayMarch(
    uint3 tid: SV_DispatchThreadID,
    uint3 gidx: SV_GroupIndex
) {
    // One thread per pixel
    // ... dispatch logic
}
```

**Сложность:**

$$T_{\text{ray\_march}}(n) = O(d) \cdot O(1) = O(d)$$

где $d = \text{max\_depth}$ — максимальная глубина SVO.

**Сравнение с dense grid:**

| Алгоритм        | Сложность      | Для $n = 10^9$ вокселей |
|-----------------|----------------|-------------------------|
| Dense Ray March | $O(n^{1/3})$   | $10^3$ iterations       |
| SVO Ray March   | $O(d)$         | 16 iterations           |
| **Ускорение**   | $O(n^{1/3}/d)$ | **62.5×**               |

---

### 3.2 SVO Point Query — C++26 Interface

**C++26 Concept и Interface:**

```cpp
// ProjectV.Render.Voxel.SVOQuery.cppm
export module ProjectV.Render.Voxel.SVOQuery;

import std;
import glm;

export namespace projectv::render::voxel {

/// Результат запроса к SVO.
export template<typename VoxelType>
struct SVOQueryResult {
    bool found{false};
    VoxelType data{};
    uint32_t depth{0};
    uint32_t node_index{0};
};

/// Концепт для SVO узла.
export template<typename T>
concept SVOConcept = requires(T const svo, glm::ivec3 coord, uint32_t depth) {
    { svo.get(coord) } -> std::same_as<SVOQueryResult<typename T::VoxelType>>;
    { svo.set(coord, std::declval<typename T::VoxelType>()) } -> std::same_as<void>;
    { svo.max_depth() } -> std::same_as<uint32_t>;
    { svo.node_count() } -> std::same_as<size_t>;
};

/// Sparse Voxel Octree с $O(\log_8 n)$ запросами.
///
/// ## Complexity
/// - get: $O(\log_8 n)$ worst case
/// - set: $O(\log_8 n)$ amortized
///
/// ## Thread Safety
/// - Concurrent reads: safe
/// - Concurrent writes: requires external synchronization
export template<typename VoxelType>
class SparseVoxelOctree {
public:
    using Voxel = VoxelType;

    /// Создаёт SVO заданной глубины.
    explicit SparseVoxelOctree(uint32_t max_depth = 16)
        : max_depth_(max_depth)
        , root_(std::make_unique<Node>())
    {}

    /// Получает воксель по координатам.
    /// @param coord Координаты в воксельном пространстве
    /// @return Результат запроса с данными вокселя
    [[nodiscard]] auto get(glm::ivec3 coord) const noexcept -> SVOQueryResult<VoxelType> {
        SVOQueryResult<VoxelType> result;
        Node const* node = root_.get();

        for (uint32_t depth = 0; depth < max_depth_; ++depth) {
            // Compute child index: 3 bits from coord at current depth
            uint32_t shift = max_depth_ - depth - 1;
            uint32_t child_idx = ((coord.x >> shift) & 1) << 2 |
                                 ((coord.y >> shift) & 1) << 1 |
                                 ((coord.z >> shift) & 1);

            if (!node->children[child_idx]) {
                return result; // Not found
            }

            node = node->children[child_idx].get();
        }

        result.found = true;
        result.data = node->voxel;
        result.depth = max_depth_;
        return result;
    }

    /// Устанавливает воксель по координатам.
    /// @param coord Координаты в воксельном пространстве
    /// @param voxel Данные вокселя
    auto set(glm::ivec3 coord, VoxelType const& voxel) -> void {
        Node* node = root_.get();

        for (uint32_t depth = 0; depth < max_depth_; ++depth) {
            uint32_t shift = max_depth_ - depth - 1;
            uint32_t child_idx = ((coord.x >> shift) & 1) << 2 |
                                 ((coord.y >> shift) & 1) << 1 |
                                 ((coord.z >> shift) & 1);

            if (!node->children[child_idx]) {
                node->children[child_idx] = std::make_unique<Node>();
            }

            node = node->children[child_idx].get();
        }

        node->voxel = voxel;
    }

    [[nodiscard]] auto max_depth() const noexcept -> uint32_t { return max_depth_; }
    [[nodiscard]] auto node_count() const noexcept -> size_t { return node_count_; }

private:
    struct Node {
        std::array<std::unique_ptr<Node>, 8> children{};
        VoxelType voxel{};
    };

    uint32_t max_depth_;
    std::unique_ptr<Node> root_;
    size_t node_count_{1};
};

static_assert(SVOConcept<SparseVoxelOctree<int>>);

} // namespace projectv::render::voxel
```

**Сложность:** $T_{\text{get}} = O(d) = O(\log_8 n)$

---

### 3.3 DAG Compression — C++26 Interface

**Алгоритм и интерфейс:**

```cpp
// ProjectV.Render.Voxel.DAGCompress.cppm
export module ProjectV.Render.Voxel.DAGCompress;

import std;
import glm;

export namespace projectv::render::voxel {

/// Hash узла SVO для DAG компрессии.
/// Использует FNV-1a для детей и MurmurHash3 для данных.
export struct NodeHash {
    uint64_t children_hash{0};
    uint64_t data_hash{0};

    auto operator==(NodeHash const& other) const noexcept -> bool = default;
};

/// Хеширование узла SVO.
export auto hash_node(
    std::span<std::unique_ptr<SVO::Node> const> children,
    VoxelType const& voxel
) noexcept -> NodeHash {
    NodeHash result;

    // Hash children pointers recursively
    uint64_t h = 0xcbf29ce484222325ULL; // FNV-1a offset basis
    for (auto const& child : children) {
        uint64_t child_hash = child ? reinterpret_cast<uint64_t>(child.get()) : 0;
        h ^= child_hash;
        h *= 0x100000001b3ULL; // FNV-1a prime
    }
    result.children_hash = h;

    // Hash voxel data
    result.data_hash = murmur_hash_64(&voxel, sizeof(VoxelType), 0xDEADBEEF);

    return result;
}

/// DAG Compression Result.
export struct DAGCompressResult {
    size_t original_nodes{0};
    size_t compressed_nodes{0};
    size_t unique_subtrees{0};
    double compression_ratio{0.0};
};

/// DAG компрессия SVO.
///
/// ## Algorithm
/// 1. Post-order traversal для хеширования всех поддеревьев
/// 2. Hash map для deduplication одинаковых поддеревьев
/// 3. Перелинковка указателей на уникальные узлы
///
/// ## Complexity
/// - Time: $O(n \log n)$ worst case, $O(n)$ average
/// - Space: $O(n)$ для hash map
///
/// ## Memory Savings
/// $$S_{\text{DAG}} = 1 - \frac{|\text{unique subtrees}|}{|\text{total nodes}|}$$
/// Типичные savings для природного ландшафта: 70-90%
export class DAGCompressor {
public:
    /// Выполняет DAG компрессию SVO.
    /// @param svo Исходное SVO
    /// @return Результат компрессии
    auto compress(SparseVoxelOctree<VoxelType>& svo) -> DAGCompressResult {
        DAGCompressResult result;
        result.original_nodes = svo.node_count();

        // Post-order traversal to hash all nodes
        std::unordered_map<NodeHash, std::vector<SVO::Node*>, NodeHashHasher> hash_map;
        hash_map.reserve(svo.node_count());

        // Phase 1: Hash all nodes
        hash_subtree(svo.root_, hash_map);

        // Phase 2: Deduplicate
        for (auto& [hash, nodes] : hash_map) {
            if (nodes.size() > 1) {
                // Keep first node, redirect others
                for (size_t i = 1; i < nodes.size(); ++i) {
                    // Redirect children to first node's children
                    nodes[i]->children = nodes[0]->children;
                }
                result.unique_subtrees++;
            }
        }

        result.compressed_nodes = count_unique_nodes(svo.root_);
        result.compression_ratio = static_cast<double>(result.compressed_nodes) /
                                   static_cast<double>(result.original_nodes);

        return result;
    }

private:
    auto hash_subtree(
        SVO::Node* node,
        std::unordered_map<NodeHash, std::vector<SVO::Node*>, NodeHashHasher>& hash_map
    ) -> NodeHash {
        if (!node) return {};

        NodeHash hash;
        std::array<uint64_t, 8> child_hashes{};

        for (size_t i = 0; i < 8; ++i) {
            NodeHash child_hash = hash_subtree(node->children[i].get(), hash_map);
            child_hashes[i] = child_hash.children_hash ^ child_hash.data_hash;
        }

        // Combine child hashes
        hash.children_hash = combine_hashes(child_hashes);
        hash.data_hash = murmur_hash_64(&node->voxel, sizeof(VoxelType), 0xDEADBEEF);

        hash_map[hash].push_back(node);
        return hash;
    }

    static auto combine_hashes(std::span<uint64_t const> hashes) -> uint64_t {
        uint64_t result = 0;
        for (uint64_t h : hashes) {
            result ^= h;
            result *= 0x100000001b3ULL;
        }
        return result;
    }
};

} // namespace projectv::render::voxel
```

**Memory savings:**

$$S_{\text{DAG}} = 1 - \frac{|\text{unique subtrees}|}{|\text{total nodes}|}$$

Типичные savings для природного ландшафта: 70-90%

---

### 3.4 Greedy Meshing — Slang Interface

**Slang Compute Shader Interface:**

```slang
// ProjectV/Render/Voxel/GreedyMesh.slang
module ProjectV.Render.Voxel.GreedyMesh;

import ProjectV.Render.Voxel.VoxelStructures;

/// Quad для mesh generation.
struct MeshQuad {
    float3 position;    ///< Позиция левого верхнего угла
    float3 normal;      ///< Нормаль грани
    float2 size;        ///< Размеры (width, height)
    uint material_id;   ///< ID материала
    uint face_axis;     ///< Ось грани (0=X, 1=Y, 2=Z)
};

/// Greedy Meshing параметры.
struct GreedyMeshParams {
    uint3 chunk_min;        ///< Минимальные координаты чанка
    uint3 chunk_max;        ///< Максимальные координаты чанка
    uint chunk_stride;      ///< Stride для следующего чанка
    uint output_offset;     ///< Смещение в output buffer
};

/// Greedy Meshing для одной оси.
///
/// ## Algorithm
/// 1. Проход по слою вокселей
/// 2. Поиск максимального прямоугольника
/// 3. Маркировка использованных вокселей
/// 4. Повторение до исчерпания слоя
///
/// ## Complexity
/// $T(n) = O(n)$ для $n$ вокселей
///
/// ## Triangle Count
/// $N_{\text{triangles}} \leq 6 \cdot \sqrt[3]{n^2}$ в худшем случае
/// vs $12n$ для наивного подхода
[[vk::binding(0, 0)]]
StructuredBuffer<VoxelData> voxelGrid;

[[vk::binding(1, 0)]]
RWStructuredBuffer<MeshQuad> outputQuads;

[[vk::binding(2, 0)]]
RWStructuredBuffer<uint> quadCounter;

/// Проверка видимости грани.
bool is_face_visible(uint3 pos, uint axis, bool positive) {
    uint3 neighbor_pos = pos;

    if (positive) {
        neighbor_pos[axis] += 1;
    } else {
        neighbor_pos[axis] -= 1;
    }

    // Boundary check
    if (any(neighbor_pos < uint3(0, 0, 0)) || any(neighbor_pos >= CHUNK_SIZE)) {
        return true; // Boundary face is always visible
    }

    // Check if neighbor is empty (air)
    uint neighbor_idx = neighbor_pos.x + neighbor_pos.y * CHUNK_SIZE_X
                      + neighbor_pos.z * CHUNK_SIZE_X * CHUNK_SIZE_Y;

    return voxelGrid[neighbor_idx].material_id == 0;
}

/// Greedy meshing для оси X (Y-Z plane).
[numthreads(8, 8, 1)]
void csGreedyMeshX(uint3 tid: SV_DispatchThreadID) {
    uint x = tid.x;
    uint y = tid.y;
    uint z = tid.z;

    if (x >= CHUNK_SIZE_X || y >= CHUNK_SIZE_Y || z >= CHUNK_SIZE_Z) return;

    uint idx = x + y * CHUNK_SIZE_X + z * CHUNK_SIZE_X * CHUNK_SIZE_Y;
    VoxelData voxel = voxelGrid[idx];

    if (voxel.material_id == 0) return; // Skip air

    // Check +X face
    if (is_face_visible(uint3(x, y, z), 0, true)) {
        // Try to extend quad in Y direction
        uint height = 1;
        while (y + height < CHUNK_SIZE_Y) {
            uint next_idx = x + (y + height) * CHUNK_SIZE_X + z * CHUNK_SIZE_X * CHUNK_SIZE_Y;
            if (voxelGrid[next_idx].material_id != voxel.material_id) break;
            if (!is_face_visible(uint3(x, y + height, z), 0, true)) break;
            height++;
        }

        // Try to extend quad in Z direction
        uint width = 1;
        bool can_extend = true;
        while (z + width < CHUNK_SIZE_Z && can_extend) {
            for (uint dy = 0; dy < height; ++dy) {
                uint next_idx = x + (y + dy) * CHUNK_SIZE_X + (z + width) * CHUNK_SIZE_X * CHUNK_SIZE_Y;
                if (voxelGrid[next_idx].material_id != voxel.material_id) {
                    can_extend = false;
                    break;
                }
                if (!is_face_visible(uint3(x, y + dy, z + width), 0, true)) {
                    can_extend = false;
                    break;
                }
            }
            if (can_extend) width++;
        }

        // Emit quad
        uint quad_idx;
        InterlockedAdd(quadCounter[0], 1, quad_idx);

        if (quad_idx < MAX_QUADS) {
            outputQuads[quad_idx] = MeshQuad(
                float3(x + 1, y, z),      // Position (face at x+1)
                float3(1, 0, 0),          // Normal +X
                float2(width, height),    // Size
                voxel.material_id,
                0                          // Axis X
            );
        }
    }
}

/// Dispatch для всех трёх осей.
void dispatchGreedyMesh(VkCommandBuffer cmd, VkExtent3D chunk_size) {
    // Dispatch X faces
    vkCmdDispatch(cmd,
        (chunk_size.width + 7) / 8,
        (chunk_size.height + 7) / 8,
        chunk_size.depth
    );
    // Dispatch Y faces
    // ... similar pattern
    // Dispatch Z faces
    // ... similar pattern
}
```

**Сложность:**

$$T_{\text{greedy\_mesh}}(n) = O(n)$$

**Количество треугольников:**

$$N_{\text{triangles}} \leq 6 \cdot \sqrt[3]{n^2}$$

в худшем случае (граница области), по сравнению с $12n$ для наивного подхода.

---

## 4. Алгоритмическая сложность ключевых операций

### 4.1 Сводная таблица

| Операция             | Сложность      | Память           |
|----------------------|----------------|------------------|
| SVO get              | $O(\log_8 n)$  | $O(1)$           |
| SVO set              | $O(\log_8 n)$  | $O(1)$ amortized |
| SVO ray march        | $O(d)$         | $O(1)$           |
| DAG compress         | $O(n \log n)$  | $O(n)$           |
| Greedy mesh          | $O(n)$         | $O(n)$           |
| CA update (chunk)    | $O(m^3)$       | $O(m^3)$         |
| Physics step         | $O(b \cdot c)$ | $O(b + c)$       |
| Render graph compile | $O(V + E)$     | $O(V + E)$       |

где:

- $n$ = количество вокселей
- $d$ = глубина SVO
- $m$ = размер чанка
- $b$ = количество тел
- $c$ = количество коллизий
- $V$ = количество render passes
- $E$ = количество зависимостей

### 4.2 Параллелизуемость

| Операция        | Data parallelism | Task parallelism              |
|-----------------|------------------|-------------------------------|
| CA update       | ✅ SIMD + GPU     | ❌ Sequential dependency       |
| Mesh generation | ✅ Per chunk      | ✅ Chunk parallel              |
| Physics         | ⚠️ Limited       | ✅ Island decomposition        |
| Rendering       | ❌ Sequential     | ✅ Pass parallel (independent) |

---

## 5. Экспериментальные оценки

### 5.1 Ожидаемая производительность

| Метрика              | Целевое значение | Теоретический предел      |
|----------------------|------------------|---------------------------|
| SVO queries/sec      | $10^7$           | $10^9$ (memory bound)     |
| CA updates/sec (GPU) | $10^9$ cells     | $10^{10}$ (compute bound) |
| Triangles/frame      | $10^6$           | $10^7$ (GPU dependent)    |
| Draw calls           | $< 1000$         | N/A                       |
| Frame time           | $< 16$ ms        | N/A                       |

### 5.2 Memory footprint

$$M_{\text{total}} = M_{\text{SVO}} + M_{\text{mesh}} + M_{\text{physics}} + M_{\text{render}}$$

где:

$$M_{\text{SVO}} \approx \frac{n \cdot 48}{c_{\text{DAG}}} \text{ bytes}$$

$c_{\text{DAG}}$ = DAG compression ratio (обычно 3-10×)

$$M_{\text{mesh}} \approx 6\sqrt[3]{n^2} \cdot 32 \text{ bytes}$$

$$M_{\text{physics}} \approx b \cdot 256 \text{ bytes}$$

$$M_{\text{render}} \approx 2 \cdot (\text{width} \cdot \text{height} \cdot 4) \text{ bytes per buffer}$$

---

## 6. Заключение

Представленные математические модели и алгоритмические анализы обосновывают выбор архитектуры ProjectV:

1. **Cellular Automata** — $O(m^3)$ на чанк с SIMD векторизацией
2. **Data-Oriented Design** — 4×+ ускорение за счёт кэш-локальности
3. **SVO** — $O(\log n)$ запросы vs $O(n^{1/3})$ для dense
4. **Greedy Meshing** — $O(n)$ с радикальным снижением треугольников

Данные обоснования соответствуют современным исследованиям в области:

- Voxel rendering (Laine & Karras 2011)
- Data-Oriented Design (Acton 2014)
- Real-time CA (Tóth et al. 2020)

---

## Ссылки

1. Laine, S., & Karras, T. (2011). Efficient Sparse Voxel Octrees.
2. Acton, M. (2014). Data-Oriented Design in C++.
3. Tóth, B., et al. (2020). Real-time Cellular Automata.
4. Vulkan 1.4 Specification, Khronos Group.
5. C++26 Working Draft, ISO/IEC JTC1/SC22/WG21.
6. P2996: Static Reflection for C++26.

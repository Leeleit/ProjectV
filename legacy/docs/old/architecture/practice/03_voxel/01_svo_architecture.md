# Спецификация SVO (Sparse Voxel Octree) ProjectV

---

## Обзор

SVO (Sparse Voxel Octree) — **архитектурный паттерн** ProjectV, который **сосуществует с чанками**:

| Применение | SVO                    | Chunks         |
|------------|------------------------|----------------|
| Хранение   | Compact, разреженное   | Плотное, 32³   |
| Рендеринг  | Ray marching (далёкие) | Mesh (близкие) |
| Физика     | Нет                    | JoltPhysics    |
| LOD        | Встроенный             | Требует код    |
| Обновления | Медленные              | Быстрые        |

---

## Интерфейсы SVO Data

```cpp
// ProjectV.Voxel.SVO.Data.cppm
export module ProjectV.Voxel.SVO.Data;

import std;
import glm;

export namespace projectv::voxel::svo {

/// Компактный узел SVO (64 бита).
export struct SVONode {
    uint64_t data{0};

    // Bit layout:
    // [63:56] child_mask - какие дети существуют (8 бит)
    // [55:28] child_ptr - указатель на детей (28 бит)
    // [27:0]  voxel_ptr - указатель на воксель (28 бит)

    [[nodiscard]] auto child_mask() const noexcept -> uint8_t;
    [[nodiscard]] auto child_ptr() const noexcept -> uint32_t;
    [[nodiscard]] auto voxel_ptr() const noexcept -> uint32_t;

    auto set_child_mask(uint8_t mask) noexcept -> void;
    auto set_child_ptr(uint32_t ptr) noexcept -> void;
    auto set_voxel_ptr(uint32_t ptr) noexcept -> void;

    [[nodiscard]] auto has_children() const noexcept -> bool;
    [[nodiscard]] auto is_leaf() const noexcept -> bool;
    [[nodiscard]] auto is_empty() const noexcept -> bool;
    [[nodiscard]] auto has_child(uint8_t index) const noexcept -> bool;
};

static_assert(sizeof(SVONode) == 8, "SVONode must be 8 bytes");

/// Данные вокселя в SVO (32 бита).
export struct SVOVoxelData {
    uint16_t material_id{0};
    uint8_t density{0};
    uint8_t flags{0};
};

/// Конфигурация SVO.
export struct SVOConfig {
    uint32_t max_depth{10};             ///< Максимальная глубина (1024³ max)
    uint32_t initial_node_capacity{1024 * 1024};
    uint32_t initial_voxel_capacity{4 * 1024 * 1024};
    bool enable_dag_compression{true};  ///< Объединение одинаковых subtree
};

} // namespace projectv::voxel::svo
```

---

## Интерфейсы SVO Tree

```cpp
// ProjectV.Voxel.SVO.Tree.cppm
export module ProjectV.Voxel.SVO.Tree;

import std;
import glm;
import ProjectV.Voxel.Data;
import ProjectV.Voxel.SVO.Data;
import ProjectV.Render.Vulkan;

export namespace projectv::voxel::svo {

/// Коды ошибок SVO.
export enum class SVOError : uint8_t {
    InvalidConfig,
    AllocationFailed,
    BuildFailed,
    UpdateFailed,
    GPULUploadFailed,
    Canceled
};

/// Результат построения SVO.
export struct SVOBuildResult {
    std::vector<SVONode> nodes;
    std::vector<SVOVoxelData> voxels;
    uint32_t root_index{0};
    uint32_t max_depth{0};
    size_t memory_usage{0};
};

/// Sparse Voxel Octree.
export class SVOTree {
public:
    /// Создаёт пустое SVO.
    explicit SVOTree(SVOConfig const& config = {}) noexcept;

    ~SVOTree() noexcept;

    SVOTree(SVOTree&&) noexcept;
    SVOTree& operator=(SVOTree&&) noexcept;
    SVOTree(const SVOTree&) = delete;
    SVOTree& operator=(const SVOTree&) = delete;

    // ========== Build API ==========

    /// Строит SVO из плотного массива.
    [[nodiscard]] auto build(
        std::span<VoxelData const> data,
        glm::uvec3 extent
    ) noexcept -> std::expected<SVOBuildResult, SVOError>;

    /// Строит SVO асинхронно.
    [[nodiscard]] auto build_async(
        std::span<VoxelData const> data,
        glm::uvec3 extent
    ) noexcept -> std::future<std::expected<SVOBuildResult, SVOError>>;

    // ========== Query API ==========

    /// Получает воксель по координатам.
    [[nodiscard]] auto get(glm::ivec3 coord) const noexcept
        -> std::expected<SVOVoxelData, SVOError>;

    /// Устанавливает воксель.
    [[nodiscard]] auto set(glm::ivec3 coord, SVOVoxelData voxel) noexcept
        -> std::expected<void, SVOError>;

    /// Проверяет существование вокселя.
    [[nodiscard]] auto exists(glm::ivec3 coord) const noexcept -> bool;

    // ========== Compression ==========

    /// Выполняет DAG сжатие (объединение одинаковых subtree).
    [[nodiscard]] auto compress_dag() noexcept -> size_t;

    /// Получает коэффициент сжатия.
    [[nodiscard]] auto compression_ratio() const noexcept -> float;

    // ========== GPU ==========

    /// Загружает на GPU.
    [[nodiscard]] auto upload_to_gpu(
        VkDevice device,
        VmaAllocator allocator
    ) noexcept -> std::expected<void, SVOError>;

    /// Получает GPU буфер узлов.
    [[nodiscard]] auto node_buffer() const noexcept -> VkBuffer;

    /// Получает GPU буфер вокселей.
    [[nodiscard]] auto voxel_buffer() const noexcept -> VkBuffer;

    // ========== Stats ==========

    [[nodiscard]] auto node_count() const noexcept -> size_t;
    [[nodiscard]] auto voxel_count() const noexcept -> size_t;
    [[nodiscard]] auto memory_usage() const noexcept -> size_t;
    [[nodiscard]] auto max_depth() const noexcept -> uint32_t;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::voxel::svo
```

---

## Интерфейсы Async Rebuild

```cpp
// ProjectV.Voxel.SVO.Async.cppm
export module ProjectV.Voxel.SVO.Async;

import std;
import glm;
import ProjectV.Voxel.Data;
import ProjectV.Voxel.SVO.Tree;

export namespace projectv::voxel::svo {

/// Асинхронный перестроитель SVO.
export class AsyncSVORebuilder {
public:
    /// Создаёт перестроитель.
    [[nodiscard]] static auto create(
        uint32_t max_nodes,
        uint32_t max_voxels
    ) noexcept -> std::expected<AsyncSVORebuilder, SVOError>;

    ~AsyncSVORebuilder() noexcept;

    AsyncSVORebuilder(AsyncSVORebuilder&&) noexcept;
    AsyncSVORebuilder& operator=(AsyncSVORebuilder&&) noexcept;
    AsyncSVORebuilder(const AsyncSVORebuilder&) = delete;
    AsyncSVORebuilder& operator=(const AsyncSVORebuilder&) = delete;

    /// Запускает асинхронную перестройку (немедленно возвращается).
    auto start_rebuild(
        std::span<VoxelData const> source,
        glm::uvec3 extent
    ) noexcept -> void;

    /// Проверяет завершение (non-blocking).
    [[nodiscard]] auto is_complete() const noexcept -> bool;

    /// Пытается завершить rebuild (non-blocking).
    [[nodiscard]] auto try_complete() noexcept -> std::optional<SVOBuildResult>;

    /// Блокирующее ожидание.
    [[nodiscard]] auto wait_complete() noexcept -> std::expected<SVOBuildResult, SVOError>;

    /// Отменяет rebuild.
    auto cancel() noexcept -> void;

    /// Проверяет отмену.
    [[nodiscard]] auto was_cancelled() const noexcept -> bool;

private:
    AsyncSVORebuilder() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Double-buffered GPU manager.
export class SVODoubleBufferManager {
public:
    /// Создаёт manager.
    [[nodiscard]] static auto create(
        VkDevice device,
        VmaAllocator allocator,
        uint32_t max_nodes,
        uint32_t max_voxels
    ) noexcept -> std::expected<SVODoubleBufferManager, SVOError>;

    ~SVODoubleBufferManager() noexcept;

    SVODoubleBufferManager(SVODoubleBufferManager&&) noexcept;
    SVODoubleBufferManager& operator=(SVODoubleBufferManager&&) noexcept;
    SVODoubleBufferManager(const SVODoubleBufferManager&) = delete;
    SVODoubleBufferManager& operator=(const SVODoubleBufferManager&) = delete;

    /// Запускает асинхронную перестройку.
    auto start_async_rebuild(
        std::span<VoxelData const> source,
        glm::uvec3 extent
    ) noexcept -> void;

    /// Обновление каждый кадр.
    /// @returns true если произошло обновление буферов
    [[nodiscard]] auto update(VkCommandBuffer cmd) noexcept -> bool;

    /// Получает текущие GPU буферы.
    [[nodiscard]] auto get_current_buffers() const noexcept
        -> std::pair<VkBuffer, VkBuffer>;

private:
    SVODoubleBufferManager() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::voxel::svo
```

---

## Интерфейсы SVO Ray Tracing

```cpp
// ProjectV.Voxel.SVO.RayTracing.cppm
export module ProjectV.Voxel.SVO.RayTracing;

import std;
import glm;
import ProjectV.Voxel.SVO.Tree;
import ProjectV.Render.Vulkan;

export namespace projectv::voxel::svo {

/// Результат трассировки луча.
export struct SVORayResult {
    bool hit{false};
    glm::vec3 hit_point{0.0f};
    glm::vec3 hit_normal{0.0f};
    float distance{0.0f};
    SVOVoxelData voxel{};
};

/// GPU Ray Tracer для SVO.
export class SVORayTracer {
public:
    /// Создаёт ray tracer.
    [[nodiscard]] static auto create(
        VkDevice device,
        SVOTree const& svo
    ) noexcept -> std::expected<SVORayTracer, SVOError>;

    ~SVORayTracer() noexcept;

    SVORayTracer(SVORayTracer&&) noexcept;
    SVORayTracer& operator=(SVORayTracer&&) noexcept;
    SVORayTracer(const SVORayTracer&) = delete;
    SVORayTracer& operator=(const SVORayTracer&) = delete;

    /// CPU трассировка луча.
    [[nodiscard]] auto trace_ray(
        glm::vec3 origin,
        glm::vec3 direction,
        float max_distance
    ) const noexcept -> SVORayResult;

    /// GPU dispatch трассировки (full-screen).
    auto dispatch_trace(
        VkCommandBuffer cmd,
        VkImageView output_image,
        glm::vec3 camera_position,
        glm::mat4 const& inv_view_proj
    ) noexcept -> void;

    /// GPU trace для набора лучей.
    auto dispatch_trace_rays(
        VkCommandBuffer cmd,
        VkBuffer ray_buffer,
        VkBuffer result_buffer,
        uint32_t ray_count
    ) noexcept -> void;

    /// Получает pipeline.
    [[nodiscard]] auto pipeline() const noexcept -> VkPipeline;

private:
    SVORayTracer() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::voxel::svo
```

---

## Интерфейсы Hybrid Architecture

```cpp
// ProjectV.Voxel.Hybrid.cppm
export module ProjectV.Voxel.Hybrid;

import std;
import glm;
import ProjectV.Voxel.Data;
import ProjectV.Voxel.Storage;
import ProjectV.Voxel.SVO.Tree;

export namespace projectv::voxel {

/// Стратегия LOD для гибридного мира.
export enum class LODStrategy : uint8_t {
    ChunkMesh,      ///< 0-50m: классические чанки с Greedy Meshing
    ChunkSVO,       ///< 50-200m: чанки с упрощённым SVO
    GlobalSVO       ///< 200m+: глобальное SVO для ray marching
};

/// Гибридный чанк.
export struct HybridChunk {
    ChunkStorage storage;                   ///< 32³ вокселей
    svo::SVOTree local_svo;                 ///< Локальное SVO (опционально)
    glm::ivec3 position{0};
    uint8_t lod_level{0};
    bool needs_mesh_rebuild{true};
    bool needs_svo_rebuild{false};
};

/// Глобальный SVO менеджер.
export class GlobalSVOManager {
public:
    /// Создаёт manager.
    [[nodiscard]] static auto create(
        svo::SVOConfig const& config = {}
    ) noexcept -> std::expected<GlobalSVOManager, SVOError>;

    ~GlobalSVOManager() noexcept;

    GlobalSVOManager(GlobalSVOManager&&) noexcept;
    GlobalSVOManager& operator=(GlobalSVOManager&&) noexcept;
    GlobalSVOManager(const GlobalSVOManager&) = delete;
    GlobalSVOManager& operator=(const GlobalSVOManager&) = delete;

    /// Строит SVO из чанков.
    auto build_from_chunks(std::span<HybridChunk const> chunks) noexcept -> void;

    /// Помечает регион для обновления.
    auto mark_dirty(glm::ivec3 chunk_pos) noexcept -> void;

    /// Обновляет грязные регионы.
    auto update_dirty_regions() noexcept -> void;

    /// Получает SVO.
    [[nodiscard]] auto svo() const noexcept -> svo::SVOTree const&;

    /// Получает dirty регионы.
    [[nodiscard]] auto dirty_regions() const noexcept -> std::span<glm::ivec3 const>;

private:
    GlobalSVOManager() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Выбирает стратегию по расстоянию.
[[nodiscard]] auto select_lod_strategy(
    float distance,
    float chunk_mesh_distance = 50.0f,
    float chunk_svo_distance = 200.0f
) noexcept -> LODStrategy;

} // namespace projectv::voxel
```

---

## Диаграммы

### Hybrid Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Hybrid Voxel Architecture                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Distance    0m ─────────────── 50m ─────────── 200m ──────────── ∞    │
│                                                                          │
│  Strategy    ChunkMesh            ChunkSVO         GlobalSVO            │
│              ──────────           ─────────        ──────────           │
│              Greedy Mesh          Local SVO        Ray Marching         │
│              JoltPhysics          Simplified       Shadows/GI           │
│              Fast Updates         LOD 1-2          LOD 3+               │
│                                                                          │
│  Memory      High                 Medium           Low                  │
│  Update      Fast                 Medium           Slow                 │
│  Physics     Yes                  No               No                   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### SVO Node Layout

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         SVONode (64 bits)                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  [63:56] child_mask (8 bit)                                             │
│  │                                                                      │
│  │   Bit 0: +X+Y+Z  Bit 4: +X+Y-Z                                      │
│  │   Bit 1: -X+Y+Z  Bit 5: -X+Y-Z                                      │
│  │   Bit 2: +X-Y+Z  Bit 6: +X-Y-Z                                      │
│  │   Bit 3: -X-Y+Z  Bit 7: -X-Y-Z                                      │
│  │                                                                      │
│  [55:28] child_ptr (28 bit) - индекс первого дочернего узла            │
│  │                                                                      │
│  [27:0]  voxel_ptr (28 bit) - индекс данных вокселя (для leaf)         │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Критерии реализации

### Обязательные требования

- [ ] SVONode 64-bit compact format
- [ ] SVOTree с build/query API
- [ ] Async rebuild без блокировки main thread
- [ ] GPU upload с double buffering

### Опциональные (GPU-dependent)

- [ ] DAG compression (50-70% memory reduction)
- [ ] GPU ray tracing через compute shaders
- [ ] Global SVO для дальних регионов

---

## Метрики производительности

| Операция              | Целевое время |
|-----------------------|---------------|
| Build (1M voxels)     | < 50 ms       |
| Async Build Start     | < 1 ms        |
| GPU Upload            | < 5 ms        |
| Ray Trace (1920x1080) | < 5 ms        |
| DAG Compression       | < 20 ms       |

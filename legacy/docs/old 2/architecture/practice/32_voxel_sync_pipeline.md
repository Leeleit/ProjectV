# Amortized Voxel Updates (SVO ↔ Mesh ↔ Physics Sync)

**Статус:** Technical Specification
**Уровень:** 🔴 Продвинутый
**Дата:** 2026-02-22
**Версия:** 1.0

---

## Обзор

Документ описывает **пайплайн синхронизации** между тремя представлениями воксельных данных:

- **SVO** (Sparse Voxel Octree) — основное хранилище
- **Mesh** (Greedy Meshing) — визуальное представление
- **Physics** (Jolt Collision) — физическое представление

Ключевая задача — **избежать фризов (stutters)** основного потока при масштабных разрушениях.

---

## 1. Проблема масштабного разрушения

### 1.1 Сценарий: Взрыв радиусом 20 вокселей

При взрыве с радиусом $r = 20$ вокселей:

$$N_{\text{affected}} = \frac{4}{3}\pi r^3 \cdot \rho \approx 33500 \text{ вокселей}$$

При плотности материала $\rho = 0.8$ (камень с пустотами):

$$N_{\text{chunks}} = \left\lceil \frac{2r}{16} \right\rceil^3 = 3^3 = 27 \text{ чанков (worst case)}$$

При типичном распределении: **8 чанков** на границе взрыва.

### 1.2 Стоимость обновления

| Операция        | Время на чанк | Total (8 чанков) |
|-----------------|---------------|------------------|
| SVO update      | 0.5 ms        | 4 ms             |
| Physics rebuild | 2.0 ms        | 16 ms            |
| Mesh rebuild    | 1.5 ms        | 12 ms            |
| **Total**       | 4.0 ms        | **32 ms**        |

**Проблема:** 32 ms на одном кадре = **19 FPS drop** (stutter).

### 1.3 Решение: Амортизированное обновление

Распределяем работу на несколько кадров с бюджетом:

| Phase           | Budget per frame | Frames (8 chunks) |
|-----------------|------------------|-------------------|
| Physics rebuild | 4 ms             | 4 frames          |
| Mesh rebuild    | 4 ms             | 3 frames          |
| SVO rebuild     | 2 ms             | 2 frames          |

**Результат:** каждый кадр ≤ 10 ms, без фризов.

---

## 2. Chunk State Machine

### 2.1 Диаграмма состояний

```
                    ┌─────────────────────────────────────────────────────────────┐
                    │                    Chunk State Machine                       │
                    └─────────────────────────────────────────────────────────────┘

                              ┌───────────────┐
                              │               │
                              │     CLEAN     │◄──────────────────────────────────┐
                              │               │                                  │
                              └───────┬───────┘                                  │
                                      │                                          │
                                      │ voxel_modified()                         │
                                      ▼                                          │
                              ┌───────────────┐                                  │
                              │               │                                  │
                              │     DIRTY     │                                  │
                              │               │                                  │
                              └───────┬───────┘                                  │
                                      │                                          │
                    ┌─────────────────┼─────────────────┐                        │
                    │                 │                 │                        │
                    │ physics_first   │ mesh_first      │ svo_first              │
                    ▼                 ▼                 ▼                        │
            ┌───────────────┐ ┌───────────────┐ ┌───────────────┐                │
            │               │ │               │ │               │                │
            │  PHYSICS_     │ │   MESH_       │ │    SVO_       │                │
            │  REBUILDING   │ │  REBUILDING   │ │  REBUILDING   │                │
            │               │ │               │ │               │                │
            └───────┬───────┘ └───────┬───────┘ └───────┬───────┘                │
                    │                 │                 │                        │
                    │ async           │ async           │ background             │
                    ▼                 ▼                 ▼                        │
            ┌───────────────┐ ┌───────────────┐ ┌───────────────┐                │
            │               │ │               │ │               │                │
            │  PHYSICS_     │ │   MESH_       │ │    SVO_       │                │
            │   READY       │ │   READY       │ │   READY       │                │
            │               │ │               │ │               │                │
            └───────┬───────┘ └───────┬───────┘ └───────┬───────┘                │
                    │                 │                 │                        │
                    └─────────────────┴─────────────────┘                        │
                                      │                                          │
                                      │ all_ready()                              │
                                      ▼                                          │
                              ┌───────────────┐                                  │
                              │               │                                  │
                              │   COMMITTED   │──────────────────────────────────┘
                              │               │        swap_buffers()
                              └───────────────┘
```

### 2.2 Определение состояний

```cpp
// ProjectV.Voxel.ChunkState.cppm
export module ProjectV.Voxel.ChunkState;

import std;

export namespace projectv::voxel {

/// Состояние чанка в пайплайне синхронизации.
export enum class ChunkState : uint8_t {
    /// Чанк чист, все представления синхронизированы.
    Clean = 0,

    /// Воксели изменены, требуется пересборка.
    Dirty = 1,

    /// Физика пересобирается (асинхронно).
    PhysicsRebuilding = 2,

    /// Физика готова, ожидает коммита.
    PhysicsReady = 3,

    /// Mesh пересобирается (асинхронно).
    MeshRebuilding = 4,

    /// Mesh готов, ожидает коммита.
    MeshReady = 5,

    /// SVO пересобирается (в фоне).
    SVORebuilding = 6,

    /// SVO готов, ожидает коммита.
    SVOReady = 7,

    /// Все готово, ждём swap buffers.
    Committed = 8
};

/// Флаги модификации чанка.
export enum class ChunkDirtyFlags : uint8_t {
    None = 0,
    Voxels = 1 << 0,      ///< Изменены воксели
    Physics = 1 << 1,     ///< Требуется пересборка физики
    Mesh = 1 << 2,        ///< Требуется пересборка mesh
    SVO = 1 << 3,         ///< Требуется пересборка SVO
    All = Voxels | Physics | Mesh | SVO
};

/// Приоритет обновления чанка.
export enum class ChunkPriority : uint8_t {
    Low = 0,      ///< Фоновое обновление
    Normal = 1,   ///< Обычное обновление
    High = 2,     ///< Игрок взаимодействует
    Critical = 3  ///< Взрыв, разрушение
};

/// Метаданные чанка для пайплайна.
export struct ChunkMetadata {
    ChunkState state{ChunkState::Clean};
    ChunkDirtyFlags dirty_flags{ChunkDirtyFlags::None};
    ChunkPriority priority{ChunkPriority::Normal};

    uint64_t frame_modified{0};       ///< Frame number когда изменён
    uint64_t frame_rebuild_start{0};  ///< Frame number начала rebuild
    uint64_t version{0};              ///< Версия данных (инкрементируется)

    uint32_t physics_build_id{0};     ///< ID задачи physics rebuild
    uint32_t mesh_build_id{0};        ///< ID задачи mesh rebuild
    uint32_t svo_build_id{0};         ///< ID задачи SVO rebuild

    float distance_to_camera{0.0f};   ///< Для приоритизации
    bool is_visible{true};            ///< Frustum culling
};

/// Проверяет, требует ли чанк обработки.
[[nodiscard]] export auto needs_rebuild(ChunkMetadata const& meta) noexcept -> bool {
    return meta.state != ChunkState::Clean &&
           meta.state != ChunkState::Committed;
}

/// Проверяет, готов ли чанк к коммиту.
[[nodiscard]] export auto ready_for_commit(ChunkMetadata const& meta) noexcept -> bool {
    return meta.state == ChunkState::Committed;
}

} // namespace projectv::voxel
```

### 2.3 ECS компоненты

```cpp
// ProjectV.ECS.VoxelComponents.cppm
export module ProjectV.ECS.VoxelComponents;

import std;
import glm;
import ProjectV.Voxel.ChunkState;

export namespace projectv::ecs {

/// Компонент чанка вокселей.
export struct VoxelChunkComponent {
    /// Позиция в grid координатах.
    glm::ivec3 grid_position{0};

    /// Размер чанка (обычно 16³).
    uint8_t chunk_size{16};

    /// Метаданные состояния.
    projectv::voxel::ChunkMetadata metadata;

    /// Указатель на данные чанка (PIMPL).
    /// Реальные данные в ChunkStorage.
    uint32_t chunk_index{0};
};

/// Компонент для отслеживания rebuild задач.
export struct ChunkRebuildComponent {
    uint32_t physics_task_id{0};
    uint32_t mesh_task_id{0};
    uint32_t svo_task_id{0};

    bool physics_done{false};
    bool mesh_done{false};
    bool svo_done{false};

    float physics_time_ms{0.0f};
    float mesh_time_ms{0.0f};
    float svo_time_ms{0.0f};
};

/// Компонент для взрывов и разрушений.
export struct DestructionComponent {
    glm::vec3 center{0.0f};
    float radius{0.0f};
    float force{0.0f};
    uint64_t frame_created{0};
    bool processed{false};
};

} // namespace projectv::ecs
```

---

## 3. Chunk Rebuild Pipeline

### 3.1 Реестр чанков

```cpp
// ProjectV.Voxel.ChunkRegistry.cppm
export module ProjectV.Voxel.ChunkRegistry;

import std;
import glm;
import flecs;
import ProjectV.Voxel.ChunkState;
import ProjectV.ECS.VoxelComponents;

export namespace projectv::voxel {

/// Конфигурация rebuild пайплайна.
export struct RebuildConfig {
    uint32_t max_physics_per_frame{2};    ///< Максимум physics rebuilds на кадр
    uint32_t max_mesh_per_frame{4};       ///< Максимум mesh rebuilds на кадр
    uint32_t max_svo_per_frame{1};        ///< Максимум SVO rebuilds на кадр

    float physics_budget_ms{4.0f};        ///< Бюджет на physics rebuild
    float mesh_budget_ms{4.0f};           ///< Бюджет на mesh rebuild
    float svo_budget_ms{2.0f};            ///< Бюджет на SVO rebuild

    bool parallel_physics{true};          ///< Параллельная сборка физики
    bool parallel_mesh{true};             ///< Параллельная генерация mesh
};

/// Реестр чанков с пайплайном синхронизации.
///
/// ## Thread Safety
/// - register_chunk: main thread only
/// - mark_dirty: thread-safe
/// - process_rebuilds: main thread
///
/// ## Invariants
/// - Чанк в состоянии Ready имеет валидные данные
/// - Swap происходит только между кадрами
export class ChunkRegistry {
public:
    /// Создаёт реестр.
    [[nodiscard]] static auto create(RebuildConfig const& config = {}) noexcept
        -> std::expected<ChunkRegistry, VoxelError>;

    ~ChunkRegistry() noexcept = default;

    ChunkRegistry(ChunkRegistry&&) noexcept = default;
    ChunkRegistry& operator=(ChunkRegistry&&) noexcept = default;
    ChunkRegistry(const ChunkRegistry&) = delete;
    ChunkRegistry& operator=(const ChunkRegistry&) = delete;

    /// Регистрирует чанк.
    auto register_chunk(glm::ivec3 const& grid_pos) noexcept
        -> std::expected<uint32_t, VoxelError>;

    /// Удаляет чанк.
    auto unregister_chunk(glm::ivec3 const& grid_pos) noexcept -> void;

    /// Помечает чанк как dirty.
    auto mark_dirty(
        glm::ivec3 const& grid_pos,
        ChunkDirtyFlags flags = ChunkDirtyFlags::All,
        ChunkPriority priority = ChunkPriority::Normal
    ) noexcept -> void;

    /// Помечает область как dirty (для взрывов).
    auto mark_region_dirty(
        glm::vec3 const& center,
        float radius,
        ChunkPriority priority = ChunkPriority::Critical
    ) noexcept -> void;

    /// Обрабатывает rebuild queue.
    /// Вызывается каждый кадр из ECS system.
    ///
    /// @param world ECS world
    /// @param pool Thread pool для async задач
    /// @return Количество обработанных чанков
    auto process_rebuilds(flecs::world& world, jobs::ThreadPool& pool) noexcept
        -> uint32_t;

    /// Коммитит готовые чанки.
    /// Swap buffers для готовых чанков.
    ///
    /// @param cmd Vulkan command buffer для GPU sync
    auto commit_ready_chunks(VkCommandBuffer cmd) noexcept -> void;

    /// Получает статистику.
    struct Stats {
        uint32_t total_chunks{0};
        uint32_t dirty_chunks{0};
        uint32_t physics_rebuilding{0};
        uint32_t mesh_rebuilding{0};
        uint32_t svo_rebuilding{0};
        uint32_t ready_for_commit{0};
    };

    [[nodiscard]] auto stats() const noexcept -> Stats;

private:
    ChunkRegistry() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::voxel
```

### 3.2 ECS System для rebuild

```cpp
// ProjectV.ECS.ChunkRebuildSystem.cppm
export module ProjectV.ECS.ChunkRebuildSystem;

import std;
import glm;
import flecs;
import ProjectV.Voxel.ChunkRegistry;
import ProjectV.ECS.VoxelComponents;
import ProjectV.Core.Jobs.ThreadPool;

export namespace projectv::ecs {

/// Система обработки rebuild чанков.
///
/// ## Execution Order
/// 1. Process destruction events → mark chunks dirty
/// 2. Start async rebuilds (physics, mesh, SVO)
/// 3. Check completion
/// 4. Commit ready chunks
///
/// ## Budget Management
/// Система следит за бюджетом времени на кадр.
export class ChunkRebuildSystem {
public:
    /// Регистрирует систему в ECS world.
    static auto register_system(
        flecs::world& world,
        voxel::ChunkRegistry& registry,
        jobs::ThreadPool& pool
    ) noexcept -> flecs::system;

    /// System callback — вызывается каждый кадр.
    static auto update(
        flecs::iter& it,
        VoxelChunkComponent* chunks,
        ChunkRebuildComponent* rebuilds
    ) noexcept -> void;

private:
    static voxel::ChunkRegistry* registry_;
    static jobs::ThreadPool* pool_;
};

/// Система обработки разрушений.
export class DestructionSystem {
public:
    static auto register_system(
        flecs::world& world,
        voxel::ChunkRegistry& registry
    ) noexcept -> flecs::system;

    static auto update(
        flecs::iter& it,
        DestructionComponent* destructions,
        VoxelChunkComponent* chunks
    ) noexcept -> void;
};

} // namespace projectv::ecs
```

### 3.3 Реализация системы

```cpp
// ProjectV.ECS.ChunkRebuildSystem.cpp
module ProjectV.ECS.ChunkRebuildSystem;

#include <flecs.h>

import std;
import glm;
import ProjectV.Voxel.ChunkRegistry;
import ProjectV.ECS.VoxelComponents;
import ProjectV.Core.Jobs.ThreadPool;
import ProjectV.ECS.ChunkRebuildSystem;

namespace projectv::ecs {

voxel::ChunkRegistry* ChunkRebuildSystem::registry_ = nullptr;
jobs::ThreadPool* ChunkRebuildSystem::pool_ = nullptr;

auto ChunkRebuildSystem::register_system(
    flecs::world& world,
    voxel::ChunkRegistry& registry,
    jobs::ThreadPool& pool
) noexcept -> flecs::system {
    registry_ = &registry;
    pool_ = &pool;

    return world.system<VoxelChunkComponent, ChunkRebuildComponent>("ChunkRebuildSystem")
        .kind(flecs::OnUpdate)
        .iter(update);
}

auto ChunkRebuildSystem::update(
    flecs::iter& it,
    VoxelChunkComponent* chunks,
    ChunkRebuildComponent* rebuilds
) noexcept -> void {
    ZoneScopedN("ChunkRebuildSystem");

    uint64_t current_frame = it.world().get_frame();

    // Phase 1: Start new rebuilds for dirty chunks
    uint32_t physics_started = 0;
    uint32_t mesh_started = 0;
    uint32_t svo_started = 0;

    for (auto i : it) {
        auto& chunk = chunks[i];
        auto& rebuild = rebuilds[i];

        if (chunk.metadata.state == voxel::ChunkState::Dirty) {
            // Determine rebuild order based on priority and visibility
            bool visible = chunk.metadata.is_visible;
            bool high_priority = chunk.metadata.priority >= voxel::ChunkPriority::High;

            if (visible || high_priority) {
                // Start physics rebuild first for visible chunks
                if (physics_started < registry_->config().max_physics_per_frame) {
                    rebuild.physics_task_id = start_physics_rebuild(chunk);
                    chunk.metadata.state = voxel::ChunkState::PhysicsRebuilding;
                    physics_started++;
                }
            } else {
                // Background: start all rebuilds together
                rebuild.physics_task_id = start_physics_rebuild(chunk);
                rebuild.mesh_task_id = start_mesh_rebuild(chunk);
                rebuild.svo_task_id = start_svo_rebuild(chunk);
                chunk.metadata.state = voxel::ChunkState::PhysicsRebuilding;
            }
        }
    }

    // Phase 2: Check completion and advance states
    for (auto i : it) {
        auto& chunk = chunks[i];
        auto& rebuild = rebuilds[i];

        switch (chunk.metadata.state) {
            case voxel::ChunkState::PhysicsRebuilding:
                if (is_task_complete(rebuild.physics_task_id)) {
                    rebuild.physics_done = true;
                    chunk.metadata.state = voxel::ChunkState::PhysicsReady;

                    // Start mesh rebuild
                    rebuild.mesh_task_id = start_mesh_rebuild(chunk);
                    chunk.metadata.state = voxel::ChunkState::MeshRebuilding;
                }
                break;

            case voxel::ChunkState::MeshRebuilding:
                if (is_task_complete(rebuild.mesh_task_id)) {
                    rebuild.mesh_done = true;
                    chunk.metadata.state = voxel::ChunkState::MeshReady;

                    // Start SVO rebuild (background)
                    rebuild.svo_task_id = start_svo_rebuild(chunk);
                    chunk.metadata.state = voxel::ChunkState::SVORebuilding;
                }
                break;

            case voxel::ChunkState::SVORebuilding:
                if (is_task_complete(rebuild.svo_task_id)) {
                    rebuild.svo_done = true;
                    chunk.metadata.state = voxel::ChunkState::SVOReady;

                    // All ready, mark for commit
                    chunk.metadata.state = voxel::ChunkState::Committed;
                }
                break;

            default:
                break;
        }
    }

    // Phase 3: Process rebuild queue for next frame
    registry_->process_rebuilds(it.world(), *pool_);
}

auto ChunkRebuildSystem::start_physics_rebuild(VoxelChunkComponent const& chunk) noexcept
    -> uint32_t {

    using namespace stdexec;

    auto task = schedule(pool_->scheduler())
              | then([&chunk]() {
                    ZoneScopedN("PhysicsRebuild");

                    auto start = std::chrono::high_resolution_clock::now();

                    // Build collision mesh from voxel data
                    auto collision_mesh = build_collision_mesh(chunk.chunk_index);

                    auto end = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration<float, std::milli>(end - start);

                    return CollisionBuildResult{
                        .mesh = std::move(collision_mesh),
                        .build_time_ms = duration.count()
                    };
                });

    return submit_async_task(std::move(task));
}

auto ChunkRebuildSystem::start_mesh_rebuild(VoxelChunkComponent const& chunk) noexcept
    -> uint32_t {

    using namespace stdexec;

    auto task = schedule(pool_->scheduler())
              | then([&chunk]() {
                    ZoneScopedN("MeshRebuild");

                    auto start = std::chrono::high_resolution_clock::now();

                    // Generate greedy mesh
                    auto mesh = generate_greedy_mesh(chunk.chunk_index);

                    auto end = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration<float, std::milli>(end - start);

                    return MeshBuildResult{
                        .mesh = std::move(mesh),
                        .build_time_ms = duration.count()
                    };
                });

    return submit_async_task(std::move(task));
}

auto ChunkRebuildSystem::start_svo_rebuild(VoxelChunkComponent const& chunk) noexcept
    -> uint32_t {

    using namespace stdexec;

    auto task = schedule(pool_->scheduler())
              | then([&chunk]() {
                    ZoneScopedN("SVORebuild");

                    // SVO rebuild can be spread across multiple frames
                    // using incremental updates
                    return update_svo_subtree(chunk.chunk_index);
                });

    return submit_async_task(std::move(task));
}

// DestructionSystem implementation
auto DestructionSystem::register_system(
    flecs::world& world,
    voxel::ChunkRegistry& registry
) noexcept -> flecs::system {

    return world.system<DestructionComponent>("DestructionSystem")
        .kind(flecs::PreUpdate)
        .iter([&registry](flecs::iter& it, DestructionComponent* destructions) {
            ZoneScopedN("DestructionSystem");

            for (auto i : it) {
                auto& dest = destructions[i];

                if (!dest.processed) {
                    // Mark affected chunks as dirty
                    registry.mark_region_dirty(
                        dest.center,
                        dest.radius,
                        voxel::ChunkPriority::Critical
                    );

                    dest.processed = true;

                    // Remove destruction component after processing
                    it.entity(i).remove<DestructionComponent>();
                }
            }
        });
}

} // namespace projectv::ecs
```

---

## 4. Vulkan Buffer Swap без разрыва кадра

### 4.1 Проблема

При обновлении чанка необходимо:

1. **Не прерывать текущий кадр** — старый mesh должен дорисоваться
2. **Заменить данные** — новый mesh должен появиться в следующем кадре
3. **Синхронизировать GPU** — избежать data races

### 4.2 Решение: Ring Buffer с BDA

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Chunk Buffer Ring Architecture                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Chunk Mesh Buffers (Triple Buffering per Chunk)                         │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                     Buffer 0 (Front)                             │    │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐                │    │
│  │  │   Chunk 0   │ │   Chunk 1   │ │   Chunk N   │                │    │
│  │  │   (Active)  │ │   (Active)  │ │   (Active)  │                │    │
│  │  └─────────────┘ └─────────────┘ └─────────────┘                │    │
│  │  BDA: 0x1000     BDA: 0x2000     BDA: 0x3000                    │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                     Buffer 1 (Back)                              │    │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐                │    │
│  │  │   Chunk 0   │ │   Chunk 1   │ │   Chunk N   │                │    │
│  │  │ (Building)  │ │  (Ready)    │ │   (Active)  │                │    │
│  │  └─────────────┘ └─────────────┘ └─────────────┘                │    │
│  │  BDA: 0x10000    BDA: 0x11000    BDA: 0x12000                   │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                     Buffer 2 (Staging)                           │    │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐                │    │
│  │  │   Chunk 0   │ │   Chunk 1   │ │   Chunk N   │                │    │
│  │  │  (Old)      │ │ (Writing)   │ │   (Old)     │                │    │
│  │  └─────────────┘ └─────────────┘ └─────────────┘                │    │
│  │  BDA: 0x20000    BDA: 0x21000    BDA: 0x22000                   │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 4.3 Vulkan Implementation

```cpp
// ProjectV.Render.Voxel.ChunkBufferManager.cppm
export module ProjectV.Render.Voxel.ChunkBufferManager;

import std;
import glm;
import vulkan;
import ProjectV.Render.Vulkan.Context;
import ProjectV.Render.Vulkan.Synchronization;
import ProjectV.Voxel.ChunkState;

export namespace projectv::render::vulkan {

/// Дескриптор буфера чанка.
export struct ChunkBufferDescriptor {
    VkDeviceAddress vertex_address{0};
    VkDeviceAddress index_address{0};
    uint32_t vertex_count{0};
    uint32_t index_count{0};
    uint32_t version{0};           ///< Версия данных
    uint32_t buffer_index{0};      ///< Индекс в ring buffer
};

/// Менеджер буферов чанков с triple buffering.
///
/// ## Architecture
/// - Triple buffering для каждого чанка
/// - BDA (Buffer Device Address) для bindless access
/// - Lock-free swap через versioning
///
/// ## Vulkan 1.4 Features
/// - Buffer Device Address (core)
/// - Synchronization2 для barriers
export class ChunkBufferManager {
public:
    /// Создаёт менеджер.
    [[nodiscard]] static auto create(
        VulkanContext const& ctx,
        uint32_t max_chunks,
        uint32_t max_vertices_per_chunk = 65536,
        uint32_t max_indices_per_chunk = 131072
    ) noexcept -> std::expected<ChunkBufferManager, VulkanError>;

    ~ChunkBufferManager() noexcept;

    ChunkBufferManager(ChunkBufferManager&&) noexcept;
    ChunkBufferManager& operator=(ChunkBufferManager&&) noexcept;
    ChunkBufferManager(const ChunkBufferManager&) = delete;
    ChunkBufferManager& operator=(const ChunkBufferManager&) = delete;

    /// Выделяет буфер для нового чанка.
    [[nodiscard]] auto allocate_chunk(uint32_t chunk_id) noexcept
        -> std::expected<ChunkBufferDescriptor, VulkanError>;

    /// Освобождает буфер чанка.
    auto free_chunk(uint32_t chunk_id) noexcept -> void;

    /// Получает адрес для записи новых данных.
    /// Возвращает staging buffer address.
    [[nodiscard]] auto get_write_address(uint32_t chunk_id) noexcept
        -> ChunkBufferDescriptor;

    /// Коммитит новые данные.
    /// Записывает staging → back buffer, планирует swap.
    ///
    /// @param cmd Command buffer
    /// @param chunk_id ID чанка
    /// @param new_version Новая версия данных
    auto commit(
        VkCommandBuffer cmd,
        uint32_t chunk_id,
        uint32_t new_version
    ) noexcept -> void;

    /// Выполняет swap buffers для готовых чанков.
    /// Вызывается между кадрами.
    ///
    /// @param cmd Command buffer
    /// @param frame_index Индекс кадра
    auto swap_buffers(VkCommandBuffer cmd, uint64_t frame_index) noexcept -> void;

    /// Получает текущий (front) дескриптор чанка.
    [[nodiscard]] auto get_current(uint32_t chunk_id) const noexcept
        -> ChunkBufferDescriptor;

    /// Получает BDA таблицу для всех чанков.
    /// Используется в shader для bindless access.
    [[nodiscard]] auto get_bda_table() const noexcept
        -> std::span<ChunkBufferDescriptor const>;

private:
    ChunkBufferManager() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::render::vulkan
```

### 4.4 Реализация Buffer Swap

```cpp
// ProjectV.Render.Voxel.ChunkBufferManager.cpp
module ProjectV.Render.Voxel.ChunkBufferManager;

import std;
import glm;
import vulkan;
import VMA;
import ProjectV.Render.Vulkan.Context;
import ProjectV.Render.Vulkan.Synchronization;
import ProjectV.Render.Vulkan.ChunkBufferManager;

namespace projectv::render::vulkan {

struct ChunkBufferManager::Impl {
    static constexpr uint32_t BUFFER_COUNT = 3;  // Triple buffering

    struct PerChunkBuffers {
        std::array<VkBuffer, BUFFER_COUNT> vertex_buffers{};
        std::array<VkBuffer, BUFFER_COUNT> index_buffers{};
        std::array<VkDeviceAddress, BUFFER_COUNT> vertex_addresses{};
        std::array<VkDeviceAddress, BUFFER_COUNT> index_addresses{};

        uint32_t front_buffer{0};      ///< Текущий для рендера
        uint32_t back_buffer{1};       ///< Для записи
        uint32_t staging_buffer{2};    ///< Staging

        uint32_t version{0};
        bool pending_swap{false};
    };

    VulkanContext const* ctx{nullptr};
    VmaAllocator allocator{VK_NULL_HANDLE};

    uint32_t max_chunks{0};
    uint32_t max_vertices{0};
    uint32_t max_indices{0};

    std::vector<PerChunkBuffers> chunk_buffers;
    std::vector<ChunkBufferDescriptor> bda_table;  // For GPU access

    VkBuffer staging_buffer{VK_NULL_HANDLE};
    VmaAllocation staging_allocation{VK_NULL_HANDLE};
    VkDeviceAddress staging_address{0};

    TimelineSemaphore swap_semaphore;
};

auto ChunkBufferManager::create(
    VulkanContext const& ctx,
    uint32_t max_chunks,
    uint32_t max_vertices_per_chunk,
    uint32_t max_indices_per_chunk
) noexcept -> std::expected<ChunkBufferManager, VulkanError> {

    ChunkBufferManager result;
    result.impl_ = std::make_unique<Impl>();

    result.impl_->ctx = &ctx;
    result.impl_->allocator = ctx.allocator();
    result.impl_->max_chunks = max_chunks;
    result.impl_->max_vertices = max_vertices_per_chunk;
    result.impl_->max_indices = max_indices_per_chunk;

    // Create timeline semaphore for swap synchronization
    auto sem_result = TimelineSemaphore::create(ctx.device(), 0);
    if (!sem_result) {
        return std::unexpected(sem_result.error());
    }
    result.impl_->swap_semaphore = std::move(*sem_result);

    // Pre-allocate chunk buffers
    result.impl_->chunk_buffers.resize(max_chunks);
    result.impl_->bda_table.resize(max_chunks);

    // Create staging buffer
    VkDeviceSize staging_size = max_vertices_per_chunk * sizeof(Vertex)
                              + max_indices_per_chunk * sizeof(uint32_t);

    VkBufferCreateInfo staging_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = staging_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };

    VmaAllocationCreateInfo alloc_info{
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    };

    VkResult vr = vmaCreateBuffer(
        result.impl_->allocator,
        &staging_info,
        &alloc_info,
        &result.impl_->staging_buffer,
        &result.impl_->staging_allocation,
        nullptr
    );

    if (vr != VK_SUCCESS) {
        return std::unexpected(VulkanError::BufferCreationFailed);
    }

    // Get device address
    VkBufferDeviceAddressInfo addr_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = result.impl_->staging_buffer,
    };
    result.impl_->staging_address = vkGetBufferDeviceAddress(ctx.device(), &addr_info);

    return result;
}

ChunkBufferManager::~ChunkBufferManager() noexcept {
    if (!impl_) return;

    for (auto& chunk : impl_->chunk_buffers) {
        for (uint32_t i = 0; i < Impl::BUFFER_COUNT; ++i) {
            if (chunk.vertex_buffers[i]) {
                vmaDestroyBuffer(impl_->allocator,
                    chunk.vertex_buffers[i], nullptr);
            }
            if (chunk.index_buffers[i]) {
                vmaDestroyBuffer(impl_->allocator,
                    chunk.index_buffers[i], nullptr);
            }
        }
    }

    if (impl_->staging_buffer) {
        vmaDestroyBuffer(impl_->allocator,
            impl_->staging_buffer, impl_->staging_allocation);
    }
}

auto ChunkBufferManager::get_write_address(uint32_t chunk_id) noexcept
    -> ChunkBufferDescriptor {

    auto& chunk = impl_->chunk_buffers[chunk_id];
    uint32_t staging = chunk.staging_buffer;

    return {
        .vertex_address = chunk.vertex_addresses[staging],
        .index_address = chunk.index_addresses[staging],
        .version = chunk.version + 1,
        .buffer_index = staging
    };
}

auto ChunkBufferManager::commit(
    VkCommandBuffer cmd,
    uint32_t chunk_id,
    uint32_t new_version
) noexcept -> void {

    auto& chunk = impl_->chunk_buffers[chunk_id];

    // Copy from staging to back buffer
    uint32_t staging = chunk.staging_buffer;
    uint32_t back = chunk.back_buffer;

    VkBufferCopy vertex_copy{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = impl_->max_vertices * sizeof(Vertex),
    };

    vkCmdCopyBuffer(cmd,
        chunk.vertex_buffers[staging],
        chunk.vertex_buffers[back],
        1, &vertex_copy
    );

    // Barrier: transfer → vertex shader read
    BarrierBuilder barrier;
    barrier.add_buffer_barrier(
        chunk.vertex_buffers[back],
        VK_PIPELINE_STAGE_2_COPY_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT
    );
    barrier.execute(cmd);

    chunk.version = new_version;
    chunk.pending_swap = true;
}

auto ChunkBufferManager::swap_buffers(
    VkCommandBuffer cmd,
    uint64_t frame_index
) noexcept -> void {

    ZoneScopedN("ChunkBufferSwap");

    uint32_t swaps_this_frame = 0;
    constexpr uint32_t MAX_SWAPS_PER_FRAME = 8;

    for (size_t i = 0; i < impl_->chunk_buffers.size() &&
                       swaps_this_frame < MAX_SWAPS_PER_FRAME; ++i) {

        auto& chunk = impl_->chunk_buffers[i];

        if (!chunk.pending_swap) continue;

        // Rotate: front → staging, back → front, staging → back
        uint32_t old_front = chunk.front_buffer;
        chunk.front_buffer = chunk.back_buffer;
        chunk.back_buffer = chunk.staging_buffer;
        chunk.staging_buffer = old_front;

        chunk.pending_swap = false;
        swaps_this_frame++;

        // Update BDA table
        impl_->bda_table[i] = {
            .vertex_address = chunk.vertex_addresses[chunk.front_buffer],
            .index_address = chunk.index_addresses[chunk.front_buffer],
            .version = chunk.version,
            .buffer_index = chunk.front_buffer
        };
    }

    // Pipeline barrier for new front buffers
    if (swaps_this_frame > 0) {
        BarrierBuilder barrier;
        barrier.add_memory_barrier(
            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            VK_ACCESS_2_MEMORY_WRITE_BIT,
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
            VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT,
            VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT
        );
        barrier.execute(cmd);
    }
}

auto ChunkBufferManager::get_current(uint32_t chunk_id) const noexcept
    -> ChunkBufferDescriptor {

    auto& chunk = impl_->chunk_buffers[chunk_id];
    return {
        .vertex_address = chunk.vertex_addresses[chunk.front_buffer],
        .index_address = chunk.index_addresses[chunk.front_buffer],
        .version = chunk.version,
        .buffer_index = chunk.front_buffer
    };
}

auto ChunkBufferManager::get_bda_table() const noexcept
    -> std::span<ChunkBufferDescriptor const> {
    return impl_->bda_table;
}

} // namespace projectv::render::vulkan
```

### 4.5 Slang Shader для BDA доступа

```slang
// ProjectV/Render/Voxel/ChunkMeshBindless.slang
module ProjectV.Render.Voxel.ChunkMeshBindless;

import ProjectV.Render.Core.Bindless;

/// Chunk mesh descriptor (matches C++ struct)
struct ChunkMeshDescriptor {
    uint64_t vertex_address;
    uint64_t index_address;
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t version;
    uint32_t buffer_index;
};

/// Bindless chunk mesh table
[[vk::binding(0, 0)]]
StructuredBuffer<ChunkMeshDescriptor> chunk_meshes;

/// Vertex structure
struct Vertex {
    float3 position;
    float3 normal;
    float2 uv;
    uint material_id;
};

/// Fetch vertex by chunk ID and vertex index
Vertex fetch_chunk_vertex(uint chunk_id, uint vertex_index) {
    ChunkMeshDescriptor desc = chunk_meshes[chunk_id];

    // Direct memory access via BDA
    DevicePointer<Vertex> vertices = devicePointerFromAddress<Vertex>(desc.vertex_address);
    return vertices[vertex_index];
}

/// Fetch index by chunk ID and index index
uint fetch_chunk_index(uint chunk_id, uint index_index) {
    ChunkMeshDescriptor desc = chunk_meshes[chunk_id];

    DevicePointer<uint> indices = devicePointerFromAddress<uint>(desc.index_address);
    return indices[index_index];
}

/// Mesh shader for bindless chunk rendering
[numthreads(64, 1, 1)]
[outputtopology("triangle")]
void msChunkMesh(
    uint3 tid: SV_DispatchThreadID,
    uint3 gidx: SV_GroupIndex,

    // Push constants
    in uint chunk_id: SV_GroupID,

    // Output
    out indices uint3 triangles[64],
    out vertices Vertex verts[192]
) {
    ChunkMeshDescriptor desc = chunk_meshes[chunk_id];

    // Each threadgroup processes one chunk
    // Calculate how many triangles this group outputs
    uint triangle_count = desc.index_count / 3;
    uint vertex_count = desc.vertex_count;

    SetMeshOutputCounts(vertex_count, triangle_count);

    // Fetch vertices using BDA
    for (uint i = gidx.x; i < vertex_count; i += 64) {
        verts[i] = fetch_chunk_vertex(chunk_id, i);
    }

    // Generate triangle indices
    for (uint i = gidx.x; i < triangle_count; i += 64) {
        uint i0 = fetch_chunk_index(chunk_id, i * 3 + 0);
        uint i1 = fetch_chunk_index(chunk_id, i * 3 + 1);
        uint i2 = fetch_chunk_index(chunk_id, i * 3 + 2);
        triangles[i] = uint3(i0, i1, i2);
    }
}
```

---

## 5. CPU-GPU Pipeline Timeline

### 5.1 Диаграмма синхронизации

```
Timeline Diagram: Explosion Event (8 chunks affected)

Frame N (Explosion):
┌────────────────────────────────────────────────────────────────────────┐
│ Main Thread                                                             │
│ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐                              │
│ │Input│→│ ECS │→│Mark │→│Start│→│Render│                              │
│ │     │ │     │ │Dirty│ │Async│ │     │                              │
│ └─────┘ └─────┘ └─────┘ └─────┘ └─────┘                              │
│                      │         │                                       │
│                      │         ▼                                       │
│                      │   ┌─────────────────────────────────┐           │
│                      │   │ Job System (stdexec)            │           │
│                      │   │                                 │           │
│                      │   │  ┌─────────────────────────┐   │           │
│                      │   │  │ Physics Rebuild Chunks  │   │           │
│                      │   │  │ 0, 1, 2, 3 (parallel)   │   │           │
│                      │   │  └─────────────────────────┘   │           │
│                      │   │         │                       │           │
│                      │   │         ▼                       │           │
│                      │   │  ┌─────────────────────────┐   │           │
│                      │   │  │ Mesh Rebuild Chunks     │   │           │
│                      │   │  │ 0, 1, 2, 3 (parallel)   │   │           │
│                      │   │  └─────────────────────────┘   │           │
│                      │   │                                 │           │
│                      │   └─────────────────────────────────┘           │
│                      │                                                 │
│                      ▼                                                 │
│               ┌─────────────┐                                          │
│               │   GPU       │                                          │
│               │   Render    │                                          │
│               │   Frame N   │                                          │
│               └─────────────┘                                          │
└────────────────────────────────────────────────────────────────────────┘

Frame N+1 (Physics Ready):
┌────────────────────────────────────────────────────────────────────────┐
│ Main Thread                                                             │
│ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐                              │
│ │Input│→│ ECS │→│Check│→│Start│→│Render│                              │
│ │     │ │     │ │Done │ │Next │ │     │                              │
│ └─────┘ └─────┘ └─────┘ └─────┘ └─────┘                              │
│                      │         │                                       │
│                      │         ▼                                       │
│                      │   ┌─────────────────────────────────┐           │
│                      │   │ Job System (stdexec)            │           │
│                      │   │                                 │           │
│                      │   │  ┌─────────────────────────┐   │           │
│                      │   │  │ Physics Rebuild Chunks  │   │           │
│                      │   │  │ 4, 5, 6, 7 (parallel)   │   │           │
│                      │   │  └─────────────────────────┘   │           │
│                      │   │         │                       │           │
│                      │   │         ▼                       │           │
│                      │   │  ┌─────────────────────────┐   │           │
│                      │   │  │ Mesh Rebuild Chunks     │   │           │
│                      │   │  │ 4, 5, 6, 7 (parallel)   │   │           │
│                      │   │  └─────────────────────────┘   │           │
│                      │   │                                 │           │
│                      │   │  ┌─────────────────────────┐   │           │
│                      │   │  │ Commit Chunks 0-3       │   │           │
│                      │   │  │ (Buffer Swap)           │   │           │
│                      │   │  └─────────────────────────┘   │           │
│                      │   │                                 │           │
│                      │   └─────────────────────────────────┘           │
│                      │                                                 │
│                      ▼                                                 │
│               ┌─────────────┐                                          │
│               │   GPU       │                                          │
│               │   Render    │                                          │
│               │   Frame N+1 │                                          │
│               │   (Chunks   │                                          │
│               │   0-3 new)  │                                          │
│               └─────────────┘                                          │
└────────────────────────────────────────────────────────────────────────┘

Frame N+2 (All Ready):
┌────────────────────────────────────────────────────────────────────────┐
│ Main Thread                                                             │
│ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐                              │
│ │Input│→│ ECS │→│Check│→│SVO  │→│Render│                              │
│ │     │ │     │ │Done │ │Bg   │ │     │                              │
│ └─────┘ └─────┘ └─────┘ └─────┘ └─────┘                              │
│                      │         │                                       │
│                      │         ▼                                       │
│                      │   ┌─────────────────────────────────┐           │
│                      │   │ Background Tasks                │           │
│                      │   │                                 │           │
│                      │   │  ┌─────────────────────────┐   │           │
│                      │   │  │ SVO Rebuild (spread    │   │           │
│                      │   │  │ across multiple frames)│   │           │
│                      │   │  └─────────────────────────┘   │           │
│                      │   │                                 │           │
│                      │   │  ┌─────────────────────────┐   │           │
│                      │   │  │ Commit Chunks 4-7       │   │           │
│                      │   │  │ (Buffer Swap)           │   │           │
│                      │   │  └─────────────────────────┘   │           │
│                      │   │                                 │           │
│                      │   └─────────────────────────────────┘           │
│                      │                                                 │
│                      ▼                                                 │
│               ┌─────────────┐                                          │
│               │   GPU       │                                          │
│               │   Render    │                                          │
│               │   Frame N+2 │                                          │
│               │   (All 8    │                                          │
│               │   updated)  │                                          │
│               └─────────────┘                                          │
└────────────────────────────────────────────────────────────────────────┘
```

### 5.2 vkCmdPipelineBarrier2 Sequence

```cpp
// Sequence of barriers for chunk buffer swap

// 1. Before starting rebuild: wait for previous GPU use
BarrierBuilder before_rebuild;
before_rebuild.add_buffer_barrier(
    chunk_buffer,
    VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
    VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
    VK_ACCESS_2_TRANSFER_WRITE_BIT
);
before_rebuild.execute(cmd);

// 2. Copy new data to staging
vkCmdCopyBuffer(cmd, staging_buffer, back_buffer, ...);

// 3. After copy: make data visible to vertex shader
BarrierBuilder after_copy;
after_copy.add_buffer_barrier(
    back_buffer,
    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
    VK_ACCESS_2_TRANSFER_WRITE_BIT,
    VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
    VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT
);
after_copy.execute(cmd);

// 4. Before render: ensure all chunks are visible
BarrierBuilder before_render;
before_render.add_memory_barrier(
    VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    VK_ACCESS_2_MEMORY_WRITE_BIT,
    VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT,
    VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT
);
before_render.execute(render_cmd);
```

---

## 6. Performance Analysis

### 6.1 Memory Footprint

$$M_{\text{chunk}} = (V_{\text{max}} \cdot 32 + I_{\text{max}} \cdot 4) \cdot 3$$

При $V_{\text{max}} = 65536$, $I_{\text{max}} = 131072$:

$$M_{\text{chunk}} \approx 6.3 \text{ MB per chunk (triple buffered)}$$

Для 1024 чанков: $M_{\text{total}} \approx 6.4 \text{ GB}$

### 6.2 Expected Performance

| Scenario                | Chunks Affected | Frames to Update | Max Frame Time |
|-------------------------|-----------------|------------------|----------------|
| Single block break      | 1               | 1                | < 2 ms         |
| Small explosion (r=5)   | 2               | 1                | < 6 ms         |
| Medium explosion (r=10) | 4               | 2                | < 8 ms         |
| Large explosion (r=20)  | 8               | 3                | < 10 ms        |
| Massive (r=50)          | 64              | 20               | < 10 ms        |

### 6.3 Tracy Profiling Marks

```cpp
// Expected Tracy output for explosion scenario
Frame N:
  [InputSystem]         0.5 ms
  [DestructionSystem]   0.3 ms
  [ChunkRebuildSystem]  1.2 ms  (mark dirty, start async)
  [PhysicsRebuild]      4.0 ms  (4 chunks parallel on job system)
  [MeshRebuild]         3.5 ms  (4 chunks parallel on job system)
  [RenderSystem]        8.0 ms
  [BufferSwap]          0.5 ms
  Total:               18.0 ms  (within 16ms budget with overlap)

Frame N+1:
  [InputSystem]         0.5 ms
  [ChunkRebuildSystem]  0.8 ms  (check completion, start next batch)
  [PhysicsRebuild]      4.0 ms  (remaining 4 chunks)
  [MeshRebuild]         3.5 ms  (remaining 4 chunks)
  [RenderSystem]        8.0 ms
  [BufferSwap]          0.5 ms  (commit first 4 chunks)
  Total:               17.3 ms

Frame N+2:
  [InputSystem]         0.5 ms
  [ChunkRebuildSystem]  0.5 ms
  [SVORebuild]          2.0 ms  (background)
  [RenderSystem]        8.0 ms
  [BufferSwap]          0.5 ms  (commit remaining chunks)
  Total:               11.5 ms
```

---

## Статус

| Компонент                | Статус         | Приоритет |
|--------------------------|----------------|-----------|
| Chunk State Machine      | Специфицирован | P0        |
| Chunk Rebuild Pipeline   | Специфицирован | P0        |
| Buffer Swap (Vulkan 1.4) | Специфицирован | P0        |
| BDA Bindless Access      | Специфицирован | P1        |
| CPU-GPU Timeline         | Специфицирован | P1        |

---

## Ссылки

- [Job System P2300 Spec](./31_job_system_p2300_spec.md)
- [Vulkan 1.4 Spec](./04_vulkan_spec.md)
- [CA-Physics Bridge](./30_ca_physics_bridge.md)
- [Voxel Pipeline](./03_voxel-pipeline.md)

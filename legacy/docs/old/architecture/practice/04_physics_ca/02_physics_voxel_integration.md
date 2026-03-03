# Physics-Voxel Integration Specification

---

## Обзор

Документ определяет спецификацию интеграции разрушаемых воксельных чанков с JoltPhysics. Ключевые принципы:

1. **MeshShape на основе Greedy Meshing** — оптимальный баланс производительности и точности
2. **Асинхронная перестройка коллизий** — минимум влияния на главный поток
3. **Batch-обновления** — группировка изменений для эффективности
4. **ECS-driven синхронизация** — автоматические триггеры при изменении данных

---

## Approach Analysis

### Сравнение подходов

| Подход                         | Преимущества                                                                        | Недостатки                                                          | Применимость                   |
|--------------------------------|-------------------------------------------------------------------------------------|---------------------------------------------------------------------|--------------------------------|
| **MeshShape (Greedy Meshing)** | ✅ Оптимальное кол-во треугольников<br>✅ Быстрое построение<br>✅ Хорошее кэширование | ⚠️ Неточные concave boundaries<br>⚠️ Требует rebuild при изменениях | **РЕКОМЕНДУЕТСЯ** для MVP      |
| **HeightField**                | ✅ Очень быстрый для terrain<br>✅ Low memory overhead                                | ❌ Только 2.5D (одна высота на x,z)<br>❌ Не подходит для пещер       | Только для surface terrain     |
| **MutableCompoundShape**       | ✅ Частичные обновления<br>✅ Хорош для мелких объектов                               | ❌ Высокий overhead на batch updates<br>❌ Сложная синхронизация      | Отдельные destructible objects |
| **Custom Jolt Shape**          | ✅ Максимальная точность<br>✅ Прямой доступ к voxel data                             | ❌ Сложная реализация<br>❌ Требует глубокого знания Jolt             | Post-MVP оптимизация           |
| **Voxel Grid Shape**           | ✅ Идеально для cubic voxels<br>✅ O(1) ray casts                                     | ❌ Нетривиальная реализация<br>❌ Overhead на sparse data             | Future consideration           |

### Обоснование выбора

**Для MVP выбран: MeshShape с Greedy Meshing**

Причины:

1. **Простота реализации** — один алгоритм для всех типов воксельных структур
2. **Хорошая производительность** — greedy meshing снижает количество полигонов в 4-10 раз
3. **Совместимость с Jolt** — MeshShape поддерживает concave geometry
4. **Масштабируемость** — async rebuild не блокирует main thread

---

## Memory Layout

### VoxelCollisionProxy

```
VoxelCollisionProxy (PIMPL)
┌─────────────────────────────────────────────────────────────┐
│  std::unique_ptr<Impl> (8 bytes)                            │
│  └── Impl:                                                  │
│      ├── shape_: JPH::RefConst<JPH::Shape> (8 bytes)        │
│      ├── body_id_: JPH::BodyID (4 bytes)                    │
│      ├── chunk_ref_: ChunkReference (16 bytes)              │
│      ├── version_: uint32_t (4 bytes)                       │
│      ├── state_: CollisionState (1 byte)                    │
│      ├── pending_rebuild_: std::atomic<bool> (1 byte)       │
│      └── padding: 6 bytes                                   │
│  Total: 8 bytes (external) + 48 bytes (internal)            │
└─────────────────────────────────────────────────────────────┘

CollisionSyncTask
┌─────────────────────────────────────────────────────────────┐
│  chunk_id: uint64_t (8 bytes)                               │
│  voxel_changes: std::vector<VoxelDelta> (24 bytes)          │
│  priority: RebuildPriority (1 byte)                         │
│  state: TaskState (1 byte)                                  │
│  submit_frame: uint64_t (8 bytes)                           │
│  callback: std::function<void(Result)> (32 bytes)           │
│  Total: 80 bytes                                            │
└─────────────────────────────────────────────────────────────┘

VoxelDelta
┌─────────────────────────────────────────────────────────────┐
│  local_x: uint8_t (1 byte)                                  │
│  local_y: uint8_t (1 byte)                                  │
│  local_z: uint8_t (1 byte)                                  │
│  old_material: uint8_t (1 byte)                             │
│  new_material: uint8_t (1 byte)                             │
│  padding: 3 bytes                                           │
│  Total: 8 bytes                                             │
└─────────────────────────────────────────────────────────────┘

GreedyMeshData
┌─────────────────────────────────────────────────────────────┐
│  vertices: std::vector<JPH::Float3> (24 bytes)              │
│  indices: std::vector<uint32_t> (24 bytes)                  │
│  face_count: uint32_t (4 bytes)                             │
│  vertex_count: uint32_t (4 bytes)                           │
│  material_ranges: std::vector<MaterialRange> (24 bytes)     │
│  Total: 80 bytes (without data)                             │
└─────────────────────────────────────────────────────────────┘
```

---

## State Machine

### Collision State

```
CollisionState
       ┌────────────┐
       │   IDLE     │ ←── Initial/Up-to-date
       └─────┬──────┘
             │ voxels_changed()
             ▼
       ┌────────────┐
       │   DIRTY    │ ←── Changes pending
       └─────┬──────┘
             │ queue_rebuild()
             ▼
       ┌────────────┐
       │  QUEUED    │ ←── In rebuild queue
       └─────┬──────┘
             │ begin_rebuild()
             ▼
       ┌────────────┐
       │ REBUILDING │ ←── Async mesh generation
       └─────┬──────┘
             │
    ┌────────┴────────┐
    ▼                 ▼
┌─────────┐     ┌──────────┐
│ READY   │     │  FAILED  │
│ (swap)  │     │  (retry) │
└────┬────┘     └──────────┘
     │ swap_complete()
     ▼
┌────────────┐
│   IDLE     │
└────────────┘
```

### Rebuild Pipeline

```
Rebuild Pipeline (Async)
─────────────────────────────────────────────────────────────────

[Main Thread]              [Worker Thread]              [Jolt Thread]
     │                           │                           │
     │  1. Voxel Changed         │                           │
     ├──────────────────────────►│                           │
     │                           │                           │
     │  2. Queue Task            │                           │
     ├──────────────────────────►│                           │
     │                           │                           │
     │                           │  3. Extract Voxel Data    │
     │                           ├──────────────────────────►│
     │                           │                           │
     │                           │  4. Greedy Meshing        │
     │                           ├──────────────────────────►│
     │                           │                           │
     │                           │  5. Build JPH::MeshShape  │
     │                           ├──────────────────────────►│
     │                           │                           │
     │  6. Callback (swap)       │                           │
     │◄──────────────────────────┤                           │
     │                           │                           │
     │  7. Update Body Shape     │                           │
     ├──────────────────────────────────────────────────────►│
     │                           │                           │
     ▼                           ▼                           ▼
```

---

## API Contracts

### VoxelCollisionManager

```cpp
// ProjectV.Voxel.Collision.cppm
export module ProjectV.Voxel.Collision;

import std;
import glm;
import ProjectV.Physics.Jolt;
import ProjectV.Voxel.Chunk;

export namespace projectv::voxel {

/// Приоритет перестройки коллизии.
export enum class RebuildPriority : uint8_t {
    Low = 0,       ///< Фоновые обновления
    Normal = 1,    ///< Обычные изменения
    High = 2,      ///< Взрывы, разрушения
    Critical = 3   ///< Немедленная обработка
};

/// Состояние коллизии.
export enum class CollisionState : uint8_t {
    Idle,          ///< Актуальное состояние
    Dirty,         ///< Есть изменения
    Queued,        ///< В очереди на rebuild
    Rebuilding,    ///< Перестраивается
    Ready,         ///< Готов к swap
    Failed         ///< Ошибка rebuild
};

/// Результат перестройки коллизии.
export struct RebuildResult {
    uint64_t chunk_id{0};
    bool success{false};
    uint32_t face_count{0};
    uint32_t vertex_count{0};
    std::chrono::milliseconds duration{0};
    std::string error_message;
};

/// Конфигурация collision manager.
export struct CollisionConfig {
    uint32_t max_queue_size{256};        ///< Максимальный размер очереди
    uint32_t worker_threads{2};          ///< Количество worker потоков
    uint32_t batch_size{16};             ///< Размер batch для обработки
    bool enable_greedy_meshing{true};    ///< Использовать greedy meshing
    float simplification_threshold{0.0f}; ///< Порог упрощения (0 = отключено)
};

/// Voxel Collision Manager — управление коллизиями воксельных чанков.
///
/// ## Architecture
/// - Асинхронная перестройка MeshShape
/// - Приоритизация задач
/// - Batch-обработка для эффективности
///
/// ## Thread Safety
/// - mark_dirty(): thread-safe
/// - process_queue(): main thread
/// - rebuild_worker(): worker threads
///
/// ## Invariants
/// - Не более одного активного rebuild на chunk
/// - Priority queue обрабатывается корректно
/// - Body shape обновляется только на main thread
export class VoxelCollisionManager {
public:
    /// Создаёт Collision Manager.
    ///
    /// @param physics JoltPhysics system
    /// @param config Конфигурация
    [[nodiscard]] static auto create(
        physics::PhysicsSystem& physics,
        CollisionConfig const& config = {}
    ) noexcept -> std::expected<VoxelCollisionManager, CollisionError>;

    ~VoxelCollisionManager() noexcept;

    VoxelCollisionManager(VoxelCollisionManager&&) noexcept;
    VoxelCollisionManager& operator=(VoxelCollisionManager&&) noexcept;
    VoxelCollisionManager(const VoxelCollisionManager&) = delete;
    VoxelCollisionManager& operator=(const VoxelCollisionManager&) = delete;

    /// Регистрирует chunk для отслеживания коллизий.
    ///
    /// @param chunk_id ID чанка
    /// @param chunk_reference Ссылка на данные чанка
    ///
    /// @pre chunk_id не зарегистрирован
    /// @post chunk имеет collision body
    [[nodiscard]] auto register_chunk(
        uint64_t chunk_id,
        ChunkReference const& chunk_reference
    ) noexcept -> std::expected<void, CollisionError>;

    /// Отменяет регистрацию chunk.
    ///
    /// @param chunk_id ID чанка
    /// @post Body удалён из физического мира
    auto unregister_chunk(uint64_t chunk_id) noexcept -> void;

    /// Помечает chunk как dirty.
    ///
    /// @param chunk_id ID чанка
    /// @param delta Изменение вокселя
    /// @param priority Приоритет
    ///
    /// @post Chunk добавлен в rebuild queue
    auto mark_dirty(
        uint64_t chunk_id,
        VoxelDelta const& delta,
        RebuildPriority priority = RebuildPriority::Normal
    ) noexcept -> void;

    /// Помечает chunk как dirty (batch).
    ///
    /// @param chunk_id ID чанка
    /// @param deltas Изменения вокселей
    /// @param priority Приоритет
    auto mark_dirty_batch(
        uint64_t chunk_id,
        std::span<VoxelDelta const> deltas,
        RebuildPriority priority = RebuildPriority::Normal
    ) noexcept -> void;

    /// Обрабатывает очередь перестроек.
    ///
    /// @param max_tasks Максимум задач за вызов
    ///
    /// @return Количество обработанных задач
    auto process_queue(uint32_t max_tasks = UINT32_MAX) noexcept -> uint32_t;

    /// Получает состояние коллизии chunk.
    [[nodiscard]] auto get_state(uint64_t chunk_id) const noexcept
        -> CollisionState;

    /// Получает BodyID для chunk.
    [[nodiscard]] auto get_body_id(uint64_t chunk_id) const noexcept
        -> std::expected<JPH::BodyID, CollisionError>;

    /// Принудительно завершает все pending rebuilds.
    auto flush_queue() noexcept -> void;

    /// Ожидает завершения rebuild для chunk.
    auto wait_for_chunk(uint64_t chunk_id,
                        uint64_t timeout_ms = UINT64_MAX) noexcept -> bool;

    /// Получает размер очереди.
    [[nodiscard]] auto queue_size() const noexcept -> size_t;

    /// Получает количество активных rebuilds.
    [[nodiscard]] auto active_rebuild_count() const noexcept -> size_t;

private:
    VoxelCollisionManager() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;

    /// Worker function для rebuild.
    auto rebuild_worker() noexcept -> void;

    /// Генерация greedy mesh.
    [[nodiscard]] auto generate_greedy_mesh(
        ChunkData const& chunk_data
    ) const noexcept -> GreedyMeshData;

    /// Создание Jolt MeshShape.
    [[nodiscard]] auto create_mesh_shape(
        GreedyMeshData const& mesh_data
    ) const noexcept -> std::expected<JPH::RefConst<JPH::Shape>, CollisionError>;

    /// Обновление body shape.
    auto update_body_shape(
        JPH::BodyID body_id,
        JPH::RefConst<JPH::Shape> new_shape
    ) noexcept -> void;
};

} // namespace projectv::voxel
```

### ECS Sync System

```cpp
// ProjectV.Voxel.CollisionSync.cppm
export module ProjectV.Voxel.CollisionSync;

import std;
import glm;
import ProjectV.ECS.Flecs;
import ProjectV.Voxel.Chunk;
import ProjectV.Voxel.Collision;

export namespace projectv::voxel::sync {

/// ECS компонент для связи с collision.
export struct VoxelCollisionComponent {
    uint64_t chunk_id{0};
    CollisionState state{CollisionState::Idle};
    uint32_t pending_changes{0};
    uint64_t last_rebuild_frame{0};
};

/// ECS компонент для физического тела.
export struct VoxelPhysicsBodyComponent {
    JPH::BodyID body_id;
    bool is_static{true};
    bool is_active{true};
};

/// ECS система синхронизации Voxel ↔ Collision.
///
/// ## Registration
/// world.system<VoxelChunkComponent, VoxelCollisionComponent>("VoxelCollisionSyncSystem")
///     .kind(flecs::OnSet)
///     .each(sync_voxel_to_collision);
export class VoxelCollisionSyncSystem {
public:
    /// Регистрирует системы в ECS world.
    static auto register_systems(
        flecs::world& world,
        VoxelCollisionManager& collision_manager
    ) noexcept -> void;

private:
    /// Система: Voxel Changed → Collision Dirty.
    static auto on_voxel_changed(
        flecs::entity e,
        VoxelChunkComponent& chunk,
        VoxelCollisionComponent& collision
    ) noexcept -> void;

    /// Система: Обработка rebuild queue.
    static auto process_collision_queue(
        flecs::iter& it,
        VoxelCollisionManager& manager
    ) noexcept -> void;

    /// Система: Swap completed shapes.
    static auto swap_collision_shapes(
        flecs::iter& it,
        VoxelCollisionManager& manager
    ) noexcept -> void;
};

} // namespace projectv::voxel::sync
```

---

## Greedy Meshing Algorithm

### Алгоритм (C++ Implementation)

```cpp
// ProjectV.Voxel.GreedyMesh.cppm
export module ProjectV.Voxel.GreedyMesh;

import std;
import glm;
import ProjectV.Voxel.Chunk;

export namespace projectv::voxel {

/// Направление грани.
export enum class FaceDirection : uint8_t {
    PositiveX,  ///< Right
    NegativeX,  ///< Left
    PositiveY,  ///< Up
    NegativeY,  ///< Down
    PositiveZ,  ///< Front
    NegativeZ   ///< Back
};

/// Грань для greedy meshing.
export struct GreedyFace {
    uint16_t x, y, z;       ///< Позиция
    uint16_t width, height; ///< Размеры в плоскости
    FaceDirection direction;
    uint16_t material_id;
};

/// Greedy Meshing Generator.
///
/// ## Algorithm
/// 1. Для каждого направления (6 faces)
/// 2. Slice-by-slice scan
/// 3. Merge adjacent same-material voxels
/// 4. Output quads
export class GreedyMeshGenerator {
public:
    /// Генерирует quads для chunk.
    ///
    /// @param chunk_data Данные вокселей
    /// @param chunk_size Размер чанка (обычно 16)
    ///
    /// @return Вектор граней
    [[nodiscard]] auto generate_quads(
        ChunkData const& chunk_data,
        uint32_t chunk_size = 16
    ) const noexcept -> std::vector<GreedyFace>;

    /// Конвертирует quads в triangle mesh.
    ///
    /// @param quads Грани
    /// @param chunk_offset Мировой offset чанка
    ///
    /// @return Vertices и indices для Jolt MeshShape
    [[nodiscard]] auto quads_to_mesh(
        std::vector<GreedyFace> const& quads,
        glm::ivec3 chunk_offset
    ) const noexcept -> GreedyMeshData;

private:
    /// Проверяет, видим ли voxel с направления.
    [[nodiscard]] auto is_face_visible(
        ChunkData const& data,
        uint32_t x, uint32_t y, uint32_t z,
        FaceDirection dir,
        uint32_t chunk_size
    ) const noexcept -> bool;

    /// Находит материал voxel.
    [[nodiscard]] auto get_material(
        ChunkData const& data,
        uint32_t x, uint32_t y, uint32_t z
    ) const noexcept -> uint16_t;

    /// Scan line для greedy merge.
    auto scan_plane(
        ChunkData const& data,
        uint32_t slice,
        FaceDirection dir,
        uint32_t chunk_size,
        std::vector<std::vector<bool>>& mask
    ) const noexcept -> std::vector<GreedyFace>;
};

// === Implementation ===

auto GreedyMeshGenerator::generate_quads(
    ChunkData const& chunk_data,
    uint32_t chunk_size
) const noexcept -> std::vector<GreedyFace> {

    std::vector<GreedyFace> quads;
    quads.reserve(chunk_size * chunk_size * 6);  // Estimate

    // Для каждого из 6 направлений
    for (uint32_t dir = 0; dir < 6; ++dir) {
        auto face_dir = static_cast<FaceDirection>(dir);

        // Маска для текущего slice
        std::vector<std::vector<bool>> mask(
            chunk_size,
            std::vector<bool>(chunk_size, false)
        );

        // Проходим по всем slices
        for (uint32_t slice = 0; slice < chunk_size; ++slice) {
            // Заполняем маску
            for (uint32_t y = 0; y < chunk_size; ++y) {
                for (uint32_t x = 0; x < chunk_size; ++x) {
                    uint32_t vx, vy, vz;

                    switch (face_dir) {
                        case FaceDirection::PositiveX:
                        case FaceDirection::NegativeX:
                            vx = slice; vy = y; vz = x;
                            break;
                        case FaceDirection::PositiveY:
                        case FaceDirection::NegativeY:
                            vx = x; vy = slice; vz = y;
                            break;
                        case FaceDirection::PositiveZ:
                        case FaceDirection::NegativeZ:
                            vx = x; vy = y; vz = slice;
                            break;
                    }

                    mask[y][x] = is_face_visible(
                        chunk_data, vx, vy, vz, face_dir, chunk_size);
                }
            }

            // Greedy merge в маске
            for (uint32_t y = 0; y < chunk_size; ) {
                for (uint32_t x = 0; x < chunk_size; ) {
                    if (!mask[y][x]) {
                        ++x;
                        continue;
                    }

                    // Находим ширину
                    uint32_t width = 1;
                    while (x + width < chunk_size && mask[y][x + width]) {
                        ++width;
                    }

                    // Находим высоту
                    uint32_t height = 1;
                    bool can_extend = true;
                    while (y + height < chunk_size && can_extend) {
                        for (uint32_t w = 0; w < width; ++w) {
                            if (!mask[y + height][x + w]) {
                                can_extend = false;
                                break;
                            }
                        }
                        if (can_extend) ++height;
                    }

                    // Создаём quad
                    uint32_t vx, vy, vz;
                    switch (face_dir) {
                        case FaceDirection::PositiveX:
                            vx = slice; vy = y; vz = x;
                            break;
                        case FaceDirection::NegativeX:
                            vx = slice + 1; vy = y; vz = x;
                            break;
                        case FaceDirection::PositiveY:
                            vx = x; vy = slice; vz = y;
                            break;
                        case FaceDirection::NegativeY:
                            vx = x; vy = slice + 1; vz = y;
                            break;
                        case FaceDirection::PositiveZ:
                            vx = x; vy = y; vz = slice;
                            break;
                        case FaceDirection::NegativeZ:
                            vx = x; vy = y; vz = slice + 1;
                            break;
                    }

                    auto material = get_material(chunk_data,
                        vx, vy, vz);

                    quads.push_back({
                        .x = static_cast<uint16_t>(vx),
                        .y = static_cast<uint16_t>(vy),
                        .z = static_cast<uint16_t>(vz),
                        .width = static_cast<uint16_t>(width),
                        .height = static_cast<uint16_t>(height),
                        .direction = face_dir,
                        .material_id = material
                    });

                    // Очищаем mask
                    for (uint32_t dy = 0; dy < height; ++dy) {
                        for (uint32_t dx = 0; dx < width; ++dx) {
                            mask[y + dy][x + dx] = false;
                        }
                    }

                    x += width;
                }
                ++y;
            }
        }
    }

    return quads;
}

auto GreedyMeshGenerator::quads_to_mesh(
    std::vector<GreedyFace> const& quads,
    glm::ivec3 chunk_offset
) const noexcept -> GreedyMeshData {

    GreedyMeshData result;

    // Каждый quad = 2 triangles = 6 indices
    result.indices.reserve(quads.size() * 6);
    result.vertices.reserve(quads.size() * 4);

    for (auto const& quad : quads) {
        uint32_t base_vertex = static_cast<uint32_t>(result.vertices.size());

        // Генерируем 4 вершины quad
        float x = static_cast<float>(quad.x + chunk_offset.x);
        float y = static_cast<float>(quad.y + chunk_offset.y);
        float z = static_cast<float>(quad.z + chunk_offset.z);

        float w = static_cast<float>(quad.width);
        float h = static_cast<float>(quad.height);

        switch (quad.direction) {
            case FaceDirection::PositiveX:
                result.vertices.push_back({x, y, z});
                result.vertices.push_back({x, y + h, z});
                result.vertices.push_back({x, y + h, z + w});
                result.vertices.push_back({x, y, z + w});
                break;
            case FaceDirection::NegativeX:
                result.vertices.push_back({x, y, z});
                result.vertices.push_back({x, y, z + w});
                result.vertices.push_back({x, y + h, z + w});
                result.vertices.push_back({x, y + h, z});
                break;
            case FaceDirection::PositiveY:
                result.vertices.push_back({x, y, z});
                result.vertices.push_back({x + w, y, z});
                result.vertices.push_back({x + w, y, z + h});
                result.vertices.push_back({x, y, z + h});
                break;
            case FaceDirection::NegativeY:
                result.vertices.push_back({x, y, z});
                result.vertices.push_back({x, y, z + h});
                result.vertices.push_back({x + w, y, z + h});
                result.vertices.push_back({x + w, y, z});
                break;
            case FaceDirection::PositiveZ:
                result.vertices.push_back({x, y, z});
                result.vertices.push_back({x, y + h, z});
                result.vertices.push_back({x + w, y + h, z});
                result.vertices.push_back({x + w, y, z});
                break;
            case FaceDirection::NegativeZ:
                result.vertices.push_back({x, y, z});
                result.vertices.push_back({x + w, y, z});
                result.vertices.push_back({x + w, y + h, z});
                result.vertices.push_back({x, y + h, z});
                break;
        }

        // 2 triangles (CCW winding)
        result.indices.push_back(base_vertex + 0);
        result.indices.push_back(base_vertex + 1);
        result.indices.push_back(base_vertex + 2);

        result.indices.push_back(base_vertex + 0);
        result.indices.push_back(base_vertex + 2);
        result.indices.push_back(base_vertex + 3);
    }

    result.vertex_count = static_cast<uint32_t>(result.vertices.size());
    result.face_count = static_cast<uint32_t>(result.indices.size() / 3);

    return result;
}

auto GreedyMeshGenerator::is_face_visible(
    ChunkData const& data,
    uint32_t x, uint32_t y, uint32_t z,
    FaceDirection dir,
    uint32_t chunk_size
) const noexcept -> bool {

    // Проверка границ
    if (x >= chunk_size || y >= chunk_size || z >= chunk_size) {
        return false;
    }

    // Текущий voxel должен быть solid
    auto current = get_material(data, x, y, z);
    if (current == 0) {  // Air
        return false;
    }

    // Соседний voxel должен быть air или outside chunk
    uint32_t nx = x, ny = y, nz = z;
    switch (dir) {
        case FaceDirection::PositiveX: ++nx; break;
        case FaceDirection::NegativeX: if (x == 0) return true; --nx; break;
        case FaceDirection::PositiveY: ++ny; break;
        case FaceDirection::NegativeY: if (y == 0) return true; --ny; break;
        case FaceDirection::PositiveZ: ++nz; break;
        case FaceDirection::NegativeZ: if (z == 0) return true; --nz; break;
    }

    if (nx >= chunk_size || ny >= chunk_size || nz >= chunk_size) {
        return true;  // Edge of chunk
    }

    auto neighbor = get_material(data, nx, ny, nz);
    return neighbor == 0;  // Air neighbor
}

auto GreedyMeshGenerator::get_material(
    ChunkData const& data,
    uint32_t x, uint32_t y, uint32_t z
) const noexcept -> uint16_t {
    // Implementation depends on ChunkData structure
    // Assuming data.voxels[x][y][z].material_id
    return data.get_voxel(x, y, z).material_id;
}

} // namespace projectv::voxel
```

---

## ECS Integration

### Flecs Systems

```cpp
// ProjectV.Voxel.CollisionSync.cpp
module ProjectV.Voxel.CollisionSync;

import std;
import glm;
import ProjectV.ECS.Flecs;
import ProjectV.Voxel.Chunk;
import ProjectV.Voxel.Collision;

namespace projectv::voxel::sync {

auto VoxelCollisionSyncSystem::register_systems(
    flecs::world& world,
    VoxelCollisionManager& collision_manager
) noexcept -> void {

    // Observer: Voxel Changed → Queue Collision Update
    world.observer<VoxelChunkComponent, VoxelCollisionComponent>("VoxelChangeObserver")
        .event(flecs::OnSet)
        .each([&collision_manager](
            flecs::entity e,
            VoxelChunkComponent& chunk,
            VoxelCollisionComponent& collision
        ) {
            if (collision.state == CollisionState::Idle ||
                collision.state == CollisionState::Dirty) {

                collision.state = CollisionState::Dirty;
                ++collision.pending_changes;

                // Queue rebuild with appropriate priority
                auto priority = RebuildPriority::Normal;

                // Check if destruction is happening
                // if (auto* destructible = e.get<DestructibleComponent>()) {
                //     if (destructible->is_fractured) {
                //         priority = RebuildPriority::High;
                //     }
                // }

                collision_manager.mark_dirty(
                    collision.chunk_id,
                    {},  // Delta will be extracted from chunk
                    priority
                );
            }
        });

    // System: Process Collision Queue (every frame)
    world.system<>("ProcessCollisionQueueSystem")
        .kind(flecs::PreUpdate)
        .singleton()
        .iter([&collision_manager](flecs::iter& it) {
            collision_manager.process_queue();
        });

    // System: Update Collision State (after physics step)
    world.system<VoxelCollisionComponent>("UpdateCollisionStateSystem")
        .kind(flecs::PostUpdate)
        .each([&collision_manager](
            flecs::entity e,
            VoxelCollisionComponent& collision
        ) {
            auto new_state = collision_manager.get_state(collision.chunk_id);
            collision.state = new_state;

            if (new_state == CollisionState::Idle) {
                collision.pending_changes = 0;
            }
        });

    // System: Sync Physics Body Transform
    world.system<VoxelCollisionComponent, VoxelPhysicsBodyComponent>("SyncPhysicsTransformSystem")
        .kind(flecs::PostUpdate)
        .each([&collision_manager](
            flecs::entity e,
            VoxelCollisionComponent& collision,
            VoxelPhysicsBodyComponent& body
        ) {
            if (body.is_active) {
                // Update transform from physics body
                // auto transform = physics::get_body_transform(body.body_id);
                // e.set<TransformComponent>(transform);
            }
        });
}

} // namespace projectv::voxel::sync
```

---

## Background Task Queue

### Async Rebuild Pipeline

```cpp
// ProjectV.Voxel.CollisionWorker.cpp
module ProjectV.Voxel.Collision;

import std;
import glm;
import ProjectV.Physics.Jolt;
import ProjectV.Voxel.Chunk;
import ProjectV.Voxel.GreedyMesh;

namespace projectv::voxel {

struct VoxelCollisionManager::Impl {
    physics::PhysicsSystem* physics{nullptr};
    CollisionConfig config;

    // Registered chunks
    std::unordered_map<uint64_t, VoxelCollisionProxy> proxies;
    std::shared_mutex proxies_mutex;

    // Rebuild queue (priority)
    struct RebuildTask {
        uint64_t chunk_id;
        RebuildPriority priority;
        uint64_t submit_frame;
        std::vector<VoxelDelta> deltas;
        std::promise<RebuildResult> promise;
    };

    std::priority_queue<
        RebuildTask,
        std::vector<RebuildTask>,
        [](RebuildTask const& a, RebuildTask const& b) {
            return static_cast<uint8_t>(a.priority) <
                   static_cast<uint8_t>(b.priority);
        }
    > rebuild_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;

    // Worker threads
    std::vector<std::thread> workers;
    std::atomic<bool> running{true};

    // Completed tasks (for main thread processing)
    std::queue<std::pair<uint64_t, JPH::RefConst<JPH::Shape>>> completed;
    std::mutex completed_mutex;

    // Greedy mesh generator
    GreedyMeshGenerator mesh_generator;
};

auto VoxelCollisionManager::rebuild_worker() noexcept -> void {
    TracySetThreadName("CollisionRebuild");

    while (impl_->running.load(std::memory_order_acquire)) {
        RebuildTask task;

        {
            std::unique_lock lock(impl_->queue_mutex);
            impl_->queue_cv.wait(lock, [this]() {
                return !impl_->rebuild_queue.empty() ||
                       !impl_->running.load(std::memory_order_acquire);
            });

            if (!impl_->running.load(std::memory_order_acquire)) {
                break;
            }

            if (impl_->rebuild_queue.empty()) {
                continue;
            }

            task = std::move(const_cast<RebuildTask&>(impl_->rebuild_queue.top()));
            impl_->rebuild_queue.pop();
        }

        // Update state to rebuilding
        {
            std::shared_lock lock(impl_->proxies_mutex);
            if (auto it = impl_->proxies.find(task.chunk_id);
                it != impl_->proxies.end()) {
                it->second.state_ = CollisionState::Rebuilding;
            }
        }

        auto start = std::chrono::steady_clock::now();

        // Generate mesh
        auto mesh_data = impl_->mesh_generator.quads_to_mesh(
            impl_->mesh_generator.generate_quads(/* chunk data */),
            {}  // chunk offset
        );

        // Create Jolt shape
        auto shape_result = create_mesh_shape(mesh_data);

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start
        );

        RebuildResult result{
            .chunk_id = task.chunk_id,
            .success = shape_result.has_value(),
            .face_count = mesh_data.face_count,
            .vertex_count = mesh_data.vertex_count,
            .duration = duration
        };

        if (shape_result) {
            // Queue for main thread processing
            std::lock_guard lock(impl_->completed_mutex);
            impl_->completed.push({task.chunk_id, *shape_result});

            TracyPlot("CollisionShapesRebuilt", 1.0f);
        } else {
            result.error_message = "Failed to create MeshShape";

            // Mark as failed
            std::shared_lock lock(impl_->proxies_mutex);
            if (auto it = impl_->proxies.find(task.chunk_id);
                it != impl_->proxies.end()) {
                it->second.state_ = CollisionState::Failed;
            }
        }

        task.promise.set_value(result);
    }
}

auto VoxelCollisionManager::process_queue(uint32_t max_tasks) noexcept -> uint32_t {
    uint32_t processed = 0;

    // Process completed tasks
    while (true) {
        std::pair<uint64_t, JPH::RefConst<JPH::Shape>> completed;
        {
            std::lock_guard lock(impl_->completed_mutex);
            if (impl_->completed.empty()) break;
            completed = std::move(impl_->completed.front());
            impl_->completed.pop();
        }

        // Update body shape (must be on main thread)
        std::shared_lock lock(impl_->proxies_mutex);
        if (auto it = impl_->proxies.find(completed.first);
            it != impl_->proxies.end()) {
            update_body_shape(it->second.body_id_, completed.second);
            it->second.state_ = CollisionState::Idle;
        }

        ++processed;
        if (processed >= max_tasks) break;
    }

    return processed;
}

auto VoxelCollisionManager::create_mesh_shape(
    GreedyMeshData const& mesh_data
) const noexcept -> std::expected<JPH::RefConst<JPH::Shape>, CollisionError> {

    // Jolt MeshShape requires indexed triangles
    JPH::MeshShapeSettings settings;

    settings.mVertices.resize(mesh_data.vertex_count);
    for (uint32_t i = 0; i < mesh_data.vertex_count; ++i) {
        settings.mVertices[i] = JPH::Float3{
            mesh_data.vertices[i].x,
            mesh_data.vertices[i].y,
            mesh_data.vertices[i].z
        };
    }

    settings.mTriangles.resize(mesh_data.face_count);
    for (uint32_t i = 0; i < mesh_data.face_count; ++i) {
        uint32_t base = i * 3;
        settings.mTriangles[i] = JPH::IndexedTriangle{
            static_cast<int>(mesh_data.indices[base + 0]),
            static_cast<int>(mesh_data.indices[base + 1]),
            static_cast<int>(mesh_data.indices[base + 2])
        };
    }

    auto result = settings.Create();
    if (result.IsValid()) {
        return result.Get();
    }

    return std::unexpected(CollisionError::ShapeCreationFailed);
}

auto VoxelCollisionManager::update_body_shape(
    JPH::BodyID body_id,
    JPH::RefConst<JPH::Shape> new_shape
) noexcept -> void {

    auto& body_interface = impl_->physics->GetBodyInterface();

    // Get current transform
    auto position = body_interface.GetCenterOfMassPosition(body_id);
    auto rotation = body_interface.GetRotation(body_id);

    // Create new body with new shape
    JPH::BodyCreationSettings settings(
        new_shape,
        position,
        rotation,
        JPH::EMotionType::Static,
        JPH::ObjectLayer::Static
    );

    // Note: Jolt doesn't support direct shape replacement
    // Must create new body and remove old one

    auto new_body_id = body_interface.CreateBody(settings);
    if (new_body_id != JPH::BodyID::cInvalidBodyID) {
        body_interface.RemoveBody(body_id);
        body_interface.DestroyBody(body_id);
        body_interface.AddBody(new_body_id, JPH::EActivation::DontActivate);
    }
}

} // namespace projectv::voxel
```

---

## Performance Metrics

| Метрика                  | Цель        | Измерение        |
|--------------------------|-------------|------------------|
| Rebuild Time (16³ chunk) | < 5ms       | Tracy CPU        |
| Rebuild Time (32³ chunk) | < 20ms      | Tracy CPU        |
| Greedy Mesh Reduction    | 4-10x       | Face count       |
| Queue Processing         | < 1ms/frame | Tracy CPU        |
| Memory per Proxy         | < 1KB       | Custom allocator |

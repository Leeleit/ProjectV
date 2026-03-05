# Спецификация динамических воксельных объектов [🔴 Уровень 3]

**Статус:** Technical Specification
**Уровень:** 🔴 Продвинутый
**Дата:** 2026-02-23
**Версия:** 1.0

---

## Обзор

Документ описывает архитектуру **динамических воксельных объектов** (Dynamic Voxel Entities) для ProjectV. В отличие от
статичного ландшафта (глобальная сетка чанков), динамические объекты (танки, боты, транспорт) должны свободно двигаться,
вращаться и разрушаться в мировом пространстве, сохраняя воксельную структуру.

---

## 1. Архитектура ECS

### 1.1 Концептуальная модель

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Dynamic Voxel Entity Architecture                     │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Static World (Terrain)              Dynamic Entities (Vehicles)        │
│  ┌─────────────────────┐            ┌─────────────────────────┐         │
│  │ Global Chunk Grid   │            │ Per-Entity Voxel Space  │         │
│  │                     │            │                         │         │
│  │  Chunk[0,0,0]       │            │  ┌─────────────────┐    │         │
│  │  Chunk[1,0,0]       │◄──Collision──│  Local SVO/Grid  │    │         │
│  │  Chunk[0,1,0]       │            │  │  32³ voxels     │    │         │
│  │  ...                │            │  └─────────────────┘    │         │
│  │                     │            │         │               │         │
│  │  World Space        │            │    Model Matrix         │         │
│  │  (Immutable refs)   │            │    (Transforms to       │         │
│  │                     │            │     World Space)        │         │
│  └─────────────────────┘            └─────────────────────────┘         │
│                                                                          │
│  Rendering:                          Rendering:                          │
│  - Chunk LOD system                  - Instance rendering                │
│  - Height-based culling              - Per-object model matrix           │
│  - Frustum culling                   - Dynamic LOD based on distance     │
│                                                                          │
│  Physics:                            Physics:                            │
│  - Static heightfield                - JPH::MotionType::Dynamic          │
│  - Triangle mesh colliders           - Mass/inertia recalc on damage     │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 1.2 ECS Components

```cpp
// ProjectV.Gameplay.DynamicVoxel.cppm
export module ProjectV.Gameplay.DynamicVoxel;

import std;
import glm;
import ProjectV.Voxel.SVO;

export namespace projectv::gameplay {

// ============================================================================
// Component: DynamicVoxelModel
// ============================================================================
// Хранит локальное воксельное представление объекта.
// Может быть SVO (разреженный) или DenseGrid (плотный).

/// Тип хранения вокселей для динамического объекта.
export enum class VoxelStorageType : uint8_t {
    DenseGrid,  ///< 32³ или 64³ плотный grid — для мелких объектов
    SVO,        ///< Sparse Voxel Octree — для крупных/разреженных
    Hybrid      ///< SVO + Dense regions для детализации
};

/// Локальные координаты вокселя в пространстве объекта.
export struct LocalVoxelCoord {
    uint16_t x, y, z;

    [[nodiscard]] static constexpr auto invalid() -> LocalVoxelCoord {
        return {UINT16_MAX, UINT16_MAX, UINT16_MAX};
    }

    [[nodiscard]] constexpr auto is_valid() const -> bool {
        return x != UINT16_MAX;
    }
};

/// Информация о материале вокселя для физики.
export struct VoxelMaterialInfo {
    uint16_t material_id{0};      ///< ID материала
    float density_kg_m3{1000.0f}; ///< Плотность в кг/м³
    float strength_pa{1e6f};      ///< Прочность в Па (для разрушения)
    uint8_t flags{0};             ///< Флаги (flammable, conductive, etc.)
};

/// Компонент: Воксельная модель динамического объекта.
///
/// ## Memory Layout
/// - DenseGrid: 32³ × 4 bytes = 128 KB
/// - SVO: variable, typically 10-50 KB
///
/// ## Thread Safety
/// - Read operations: thread-safe
/// - Write operations: requires mutex or single-threaded access
export struct DynamicVoxelModel {
    /// Тип хранения.
    VoxelStorageType storage_type{VoxelStorageType::DenseGrid};

    /// Размер грида (для DenseGrid).
    uint8_t grid_size_log2{5};  // 2^5 = 32

    /// SVO дерево (для SVO/Hybrid).
    std::unique_ptr<voxel::SVOTree> svo;

    /// Dense grid данные (для DenseGrid).
    /// Каждый voxel: 4 bytes (material_id:16 + flags:8 + health:8)
    std::vector<uint32_t> dense_grid;

    /// Таблица материалов.
    std::vector<VoxelMaterialInfo> materials;

    /// Грязные регионы (нужен remesh).
    std::vector<glm::ivec3> dirty_regions;

    /// Флаг: нужна перегенерация mesh.
    bool mesh_dirty{true};

    /// Флаг: нужен перерасчёт физики.
    bool physics_dirty{true};

    /// GPU buffer handle (opaque).
    uint64_t gpu_voxel_buffer{0};

    /// GPU mesh buffer handle.
    uint64_t gpu_mesh_buffer{0};

    // === Methods ===

    /// Получает воксель в локальных координатах.
    [[nodiscard]] auto get_voxel(LocalVoxelCoord coord) const noexcept
        -> std::expected<uint32_t, void>;

    /// Устанавливает воксель.
    auto set_voxel(LocalVoxelCoord coord, uint32_t data) noexcept -> void;

    /// Удаляет воксель (set to air).
    auto remove_voxel(LocalVoxelCoord coord) noexcept -> void;

    /// Подсчитывает ненулевые воксели.
    [[nodiscard]] auto count_solid_voxels() const noexcept -> size_t;

    /// Вычисляет общий bounding box.
    [[nodiscard]] auto local_bounds() const noexcept -> glm::vec3;

    /// Получает плотность материала.
    [[nodiscard]] auto get_material_density(uint16_t material_id) const noexcept -> float;
};

// ============================================================================
// Component: DynamicVoxelTransform
// ============================================================================
// Transform для динамических объектов с model matrix.

/// Компонент: Трансформация динамического воксельного объекта.
///
/// ## Memory Layout
/// - 128 bytes total
/// - 64-byte aligned for SIMD
export struct alignas(64) DynamicVoxelTransform {
    /// Позиция в world space (center of mass).
    glm::vec3 position{0.0f};

    /// Вращение (quaternion).
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};

    /// Масштаб (обычно 1.0).
    glm::vec3 scale{1.0f};

    /// Model matrix (cached).
    glm::mat4 model_matrix{1.0f};

    /// Inverse model matrix (cached).
    glm::mat4 inverse_model_matrix{1.0f};

    /// Нормаль matrix (for lighting).
    glm::mat3 normal_matrix{1.0f};

    /// Флаг: matrix dirty.
    bool dirty{true};

    /// World-space AABB (cached).
    glm::vec3 aabb_min{0.0f};
    glm::vec3 aabb_max{0.0f};

    // === Methods ===

    /// Обновляет матрицы.
    auto update_matrices() noexcept -> void;

    /// Преобразует local point в world.
    [[nodiscard]] auto local_to_world(glm::vec3 local) const noexcept -> glm::vec3;

    /// Преобразует world point в local.
    [[nodiscard]] auto world_to_local(glm::vec3 world) const noexcept -> glm::vec3;

    /// Обновляет AABB на основе local bounds.
    auto update_world_aabb(glm::vec3 local_bounds_min,
                           glm::vec3 local_bounds_max) noexcept -> void;
};

static_assert(sizeof(DynamicVoxelTransform) == 128);
static_assert(alignof(DynamicVoxelTransform) == 64);

// ============================================================================
// Component: DynamicVoxelPhysics
// ============================================================================
// Физические свойства для Jolt Physics.

/// Компонент: Физика динамического воксельного объекта.
///
/// ## Jolt Integration
/// - JPH::BodyID хранится как uint64_t
/// - Mass/inertia пересчитываются при разрушении
export struct DynamicVoxelPhysics {
    /// Jolt Body ID (opaque).
    uint64_t jolt_body_id{0};

    /// Общая масса в кг.
    float total_mass_kg{0.0f};

    /// Центр масс в локальных координатах.
    glm::vec3 center_of_mass_local{0.0f};

    /// Тензор инерции (диагональ).
    glm::vec3 inertia_diagonal{0.0f};

    /// Объём в м³.
    float volume_m3{0.0f};

    /// Средняя плотность кг/м³.
    float average_density{0.0f};

    /// Motion type.
    enum class MotionType : uint8_t {
        Static,
        Kinematic,
        Dynamic
    } motion_type{MotionType::Dynamic};

    /// Motion quality (for fast objects).
    enum class MotionQuality : uint8_t {
        Discrete,
        LinearCast  // Better for fast objects
    } motion_quality{MotionQuality::Discrete};

    /// Linear damping.
    float linear_damping{0.05f};

    /// Angular damping.
    float angular_damping{0.05f};

    /// Флаг: нужен перерасчёт массы/инерции.
    bool needs_recalc{true};

    /// Время последнего обновления физики.
    float last_physics_update{0.0f};

    // === Methods ===

    /// Проверяет валидность Body ID.
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return jolt_body_id != 0;
    }
};

// ============================================================================
// Component: DynamicVoxelDamage
// ============================================================================
// Состояние повреждений и разрушения.

/// Регион повреждения (для частичного разрушения).
export struct DamageRegion {
    glm::ivec3 center{0};       ///< Центр повреждения (local)
    float radius{0.0f};         ///< Радиус повреждения
    float damage_amount{0.0f};  ///< Величина урона
    float timestamp{0.0f};      ///< Время нанесения
};

/// Компонент: Повреждения динамического воксельного объекта.
export struct DynamicVoxelDamage {
    /// Текущее здоровье (0.0 - 1.0 от максимального).
    float health{1.0f};

    /// Максимальное здоровье.
    float max_health{100.0f};

    /// Активные регионы повреждений.
    std::vector<DamageRegion> active_damage_regions;

    /// Отделённые фрагменты (для визуального эффекта).
    std::vector<std::vector<LocalVoxelCoord>> detached_fragments;

    /// Флаг: объект уничтожен.
    bool is_destroyed{false};

    /// Время уничтожения (для эффектов).
    float destruction_time{0.0f};

    /// Тип разрушения.
    enum class DestructionType : uint8_t {
        None,
        Fragment,     // Распадается на куски
        Explode,      // Взрыв
        Collapse,     // Обрушение
        Dissolve      // Растворение
    } destruction_type{DestructionType::Fragment};

    // === Methods ===

    /// Наносит урон в точке.
    auto apply_damage(
        glm::vec3 local_point,
        float radius,
        float damage,
        float timestamp
    ) noexcept -> void;

    /// Обрабатывает разрушение.
    auto process_destruction(float current_time) noexcept -> void;

    /// Проверяет, можно ли разрушить.
    [[nodiscard]] auto can_be_destroyed() const noexcept -> bool {
        return health <= 0.0f && !is_destroyed;
    }
};

// ============================================================================
// Component: DynamicVoxelRenderData
// ============================================================================
// Данные для рендеринга (GPU).

/// Компонент: Данные рендеринга динамического объекта.
export struct DynamicVoxelRenderData {
    /// Instance ID в instance buffer.
    uint32_t instance_id{0};

    /// LOD уровень (0 = highest).
    uint8_t lod_level{0};

    /// Флаг: виден в текущем кадре.
    bool is_visible{true};

    /// Расстояние до камеры.
    float distance_to_camera{0.0f};

    /// Индirect draw command index.
    uint32_t indirect_draw_index{0};

    /// Meshlet count.
    uint32_t meshlet_count{0};

    /// Vertex count.
    uint32_t vertex_count{0};

    /// Index count.
    uint32_t index_count{0};
};

} // namespace projectv::gameplay
```

### 1.3 ECS Systems

```cpp
// ProjectV.Gameplay.DynamicVoxelSystems.cppm
export module ProjectV.Gameplay.DynamicVoxelSystems;

import std;
import glm;
import flecs;
import ProjectV.Gameplay.DynamicVoxel;
import ProjectV.Physics.Jolt;
import ProjectV.Render.Vulkan;

export namespace projectv::gameplay::systems {

// ============================================================================
// System: DynamicVoxelTransformSystem
// ============================================================================
// Обновляет model matrices для всех динамических объектов.

export auto register_transform_system(flecs::world& world) -> void {
    world.system<DynamicVoxelTransform, DynamicVoxelModel>(
        "DynamicVoxelTransformSystem"
    ).each([](flecs::entity e, DynamicVoxelTransform& transform,
              DynamicVoxelModel const& model) {
        if (transform.dirty) {
            transform.update_matrices();

            // Update world AABB
            auto local_bounds = model.local_bounds();
            transform.update_world_aabb(
                -local_bounds * 0.5f,
                local_bounds * 0.5f
            );

            transform.dirty = false;
        }
    });
}

// ============================================================================
// System: DynamicVoxelMeshSystem
// ============================================================================
// Перегенерирует mesh для "грязных" объектов.

export auto register_mesh_system(flecs::world& world) -> void {
    world.system<DynamicVoxelModel, DynamicVoxelTransform, DynamicVoxelRenderData>(
        "DynamicVoxelMeshSystem"
    ).kind(flecs::OnStore).each([](flecs::entity e,
        DynamicVoxelModel& model,
        DynamicVoxelTransform const& transform,
        DynamicVoxelRenderData& render_data) {

        if (model.mesh_dirty) {
            // Generate mesh on worker thread
            generate_voxel_mesh_async(model, transform, render_data);
            model.mesh_dirty = false;
        }
    });
}

// ============================================================================
// System: DynamicVoxelPhysicsSystem
// ============================================================================
// Обновляет физические свойства при изменении вокселей.

export auto register_physics_system(flecs::world& world, physics::JoltPhysics* physics) -> void {
    world.system<DynamicVoxelModel, DynamicVoxelPhysics, DynamicVoxelTransform>(
        "DynamicVoxelPhysicsSystem"
    ).kind(flecs::PreUpdate).each([physics](flecs::entity e,
        DynamicVoxelModel& model,
        DynamicVoxelPhysics& physics_comp,
        DynamicVoxelTransform& transform) {

        if (physics_comp.needs_recalc && physics_comp.is_valid()) {
            recalculate_physics_properties(model, physics_comp, transform);
            physics_comp.needs_recalc = false;
        }
    });
}

// ============================================================================
// System: DynamicVoxelDamageSystem
// ============================================================================
// Обрабатывает повреждения и разрушение.

export auto register_damage_system(flecs::world& world, float current_time) -> void {
    world.system<DynamicVoxelModel, DynamicVoxelPhysics, DynamicVoxelDamage>(
        "DynamicVoxelDamageSystem"
    ).each([current_time](flecs::entity e,
        DynamicVoxelModel& model,
        DynamicVoxelPhysics& physics,
        DynamicVoxelDamage& damage) {

        if (damage.can_be_destroyed()) {
            // Process destruction
            damage.process_destruction(current_time);

            // Create detached fragments as new entities
            for (auto& fragment : damage.detached_fragments) {
                create_debris_entity(e, model, fragment);
            }

            // Mark entity for removal or destruction
            if (damage.is_destroyed) {
                e.add<DestroyedTag>();
            }
        }
    });
}

// ============================================================================
// System: DynamicVoxelCullingSystem
// ============================================================================
// Frustum и distance culling.

export auto register_culling_system(flecs::world& world, render::Frustum const& frustum) -> void {
    world.system<DynamicVoxelTransform, DynamicVoxelRenderData>(
        "DynamicVoxelCullingSystem"
    ).kind(flecs::OnValidate).each([&frustum](flecs::entity e,
        DynamicVoxelTransform const& transform,
        DynamicVoxelRenderData& render_data) {

        // Frustum culling
        render_data.is_visible = frustum.intersects_aabb(
            transform.aabb_min,
            transform.aabb_max
        );

        // LOD selection based on distance
        if (render_data.is_visible) {
            float distance = render_data.distance_to_camera;

            if (distance < 50.0f) {
                render_data.lod_level = 0;
            } else if (distance < 100.0f) {
                render_data.lod_level = 1;
            } else if (distance < 200.0f) {
                render_data.lod_level = 2;
            } else {
                render_data.lod_level = 3;
            }
        }
    });
}

} // namespace projectv::gameplay::systems
```

---

## 2. Рендеринг (Slang Task + Mesh Shaders)

### 2.1 Структура данных GPU

```cpp
// ProjectV.Render.DynamicVoxelGPU.cppm
export module ProjectV.Render.DynamicVoxelGPU;

import std;
import glm;
import vulkan;

export namespace projectv::render {

/// Instance data для динамических воксельных объектов.
/// Записывается в instance buffer.
export struct alignas(16) DynamicVoxelInstanceData {
    glm::mat4 model_matrix;           // 64 bytes
    glm::mat4 inverse_model_matrix;   // 64 bytes
    glm::mat3 normal_matrix;          // 48 bytes (padded to 64)
    glm::vec3 aabb_min;               // 12 bytes (padded to 16)
    float _pad0;
    glm::vec3 aabb_max;               // 12 bytes (padded to 16)
    uint32_t voxel_buffer_address_lo; // BDA for voxel data
    uint32_t voxel_buffer_address_hi;
    uint32_t mesh_buffer_address_lo;  // BDA for mesh data
    uint32_t mesh_buffer_address_hi;
    uint32_t voxel_count;             // Number of solid voxels
    uint32_t meshlet_count;           // Number of meshlets
    uint32_t lod_level;               // Current LOD
    uint32_t flags;                   // Visibility flags
};

static_assert(sizeof(DynamicVoxelInstanceData) == 256);

/// Meshlet для динамического объекта.
export struct alignas(16) DynamicVoxelMeshlet {
    uint32_t vertex_offset;      // Offset in vertex buffer
    uint32_t index_offset;       // Offset in index buffer
    uint32_t vertex_count;       // Number of vertices
    uint32_t index_count;        // Number of indices
    glm::vec3 center;            // Bounding sphere center
    float radius;                // Bounding sphere radius
    uint32_t flags;              // Meshlet flags
    uint32_t _pad[3];
};

static_assert(sizeof(DynamicVoxelMeshlet) == 48);

/// Vertex для динамического воксельного объекта.
export struct alignas(16) DynamicVoxelVertex {
    glm::vec3 position;          // 12 bytes
    float _pad0;
    glm::vec3 normal;            // 12 bytes
    float _pad1;
    glm::vec2 uv;                // 8 bytes
    uint32_t material_id;        // 4 bytes
    uint32_t color;              // 4 bytes (RGBA8)
};

static_assert(sizeof(DynamicVoxelVertex) == 48);

} // namespace projectv::render
```

### 2.2 Slang Task Shader

```slang
// shaders/dynamic_voxel_task.slang

#extension GL_EXT_mesh_shader : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

// ============================================================================
// Buffer References (BDA)
// ============================================================================

layout(buffer_reference, scalar) buffer InstanceBuffer {
    DynamicVoxelInstanceData instances[];
};

layout(buffer_reference, scalar) buffer MeshletBuffer {
    DynamicVoxelMeshlet meshlets[];
};

// ============================================================================
// Structures
// ============================================================================

struct DynamicVoxelInstanceData {
    mat4 model_matrix;
    mat4 inverse_model_matrix;
    mat3 normal_matrix;
    vec3 aabb_min;
    float _pad0;
    vec3 aabb_max;
    uint voxel_buffer_address_lo;
    uint voxel_buffer_address_hi;
    uint mesh_buffer_address_lo;
    uint mesh_buffer_address_hi;
    uint voxel_count;
    uint meshlet_count;
    uint lod_level;
    uint flags;
};

struct DynamicVoxelMeshlet {
    uint vertex_offset;
    uint index_offset;
    uint vertex_count;
    uint index_count;
    vec3 center;
    float radius;
    uint flags;
    uint _pad[3];
};

// ============================================================================
// Push Constants
// ============================================================================

layout(push_constant) uniform PushConstants {
    InstanceBuffer instance_buffer;
    MeshletBuffer meshlet_buffer;
    vec3 camera_position;
    uint instance_count;
    vec3 view_dir;
    float time;
    mat4 view_projection_matrix;
    vec4 frustum_planes[6];
} pc;

// ============================================================================
// Task Shader
// ============================================================================

#ifdef TASK_SHADER

// Workgroup size for task shader
// Each workgroup processes one instance
layout(local_size_x = 32) in;

// Task payload to mesh shader
taskPayloadSharedEXT uint meshlet_indices[32];

void main() {
    uint instance_id = gl_WorkGroupID.x;

    if (instance_id >= pc.instance_count) {
        return;
    }

    // Load instance data
    DynamicVoxelInstanceData instance = pc.instance_buffer.instances[instance_id];

    // Frustum culling
    bool visible = true;
    for (int i = 0; i < 6; ++i) {
        vec4 plane = pc.frustum_planes[i];
        vec3 aabb_min = instance.aabb_min;
        vec3 aabb_max = instance.aabb_max;

        // AABB vs plane test
        vec3 positive_vertex = mix(aabb_min, aabb_max,
            greaterThan(plane.xyz, vec3(0.0)));

        if (dot(positive_vertex, plane.xyz) + plane.w < 0.0) {
            visible = false;
            break;
        }
    }

    if (!visible) {
        return;
    }

    // Distance-based LOD selection
    vec3 world_center = (instance.model_matrix * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    float distance = length(world_center - pc.camera_position);

    uint target_lod;
    if (distance < 50.0) {
        target_lod = 0;
    } else if (distance < 100.0) {
        target_lod = 1;
    } else if (distance < 200.0) {
        target_lod = 2;
    } else {
        target_lod = 3;
    }

    // Early exit if LOD mismatch (will be processed by correct LOD pass)
    if (target_lod != instance.lod_level) {
        return;
    }

    // Count meshlets for this instance
    uint meshlet_count = instance.meshlet_count;

    // Distribute meshlets among mesh shader workgroups
    uint meshlets_per_task = 32; // mesh shader workgroup size
    uint task_count = (meshlet_count + meshlets_per_task - 1) / meshlets_per_task;

    // Emit tasks for mesh shader
    for (uint i = 0; i < task_count; ++i) {
        uint start_meshlet = i * meshlets_per_task;
        uint end_meshlet = min(start_meshlet + meshlets_per_task, meshlet_count);

        // Store meshlet indices in payload
        for (uint j = start_meshlet; j < end_meshlet; ++j) {
            meshlet_indices[j - start_meshlet] = j;
        }

        // Emit one task
        EmitMeshTasksEXT(1, instance_id);
    }
}

#endif // TASK_SHADER
```

### 2.3 Slang Mesh Shader

```slang
// shaders/dynamic_voxel_mesh.slang

#extension GL_EXT_mesh_shader : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

// ============================================================================
// Buffer References
// ============================================================================

layout(buffer_reference, scalar) buffer VertexBuffer {
    DynamicVoxelVertex vertices[];
};

layout(buffer_reference, scalar) buffer IndexBuffer {
    uint indices[];
};

layout(buffer_reference, scalar) buffer MeshletBuffer {
    DynamicVoxelMeshlet meshlets[];
};

layout(buffer_reference, scalar) buffer InstanceBuffer {
    DynamicVoxelInstanceData instances[];
};

// ============================================================================
// Structures
// ============================================================================

struct DynamicVoxelVertex {
    vec3 position;
    float _pad0;
    vec3 normal;
    float _pad1;
    vec2 uv;
    uint material_id;
    uint color;
};

struct DynamicVoxelMeshlet {
    uint vertex_offset;
    uint index_offset;
    uint vertex_count;
    uint index_count;
    vec3 center;
    float radius;
    uint flags;
    uint _pad[3];
};

struct DynamicVoxelInstanceData {
    mat4 model_matrix;
    mat4 inverse_model_matrix;
    mat3 normal_matrix;
    vec3 aabb_min;
    float _pad0;
    vec3 aabb_max;
    uint voxel_buffer_address_lo;
    uint voxel_buffer_address_hi;
    uint mesh_buffer_address_lo;
    uint mesh_buffer_address_hi;
    uint voxel_count;
    uint meshlet_count;
    uint lod_level;
    uint flags;
};

// ============================================================================
// Push Constants
// ============================================================================

layout(push_constant) uniform PushConstants {
    InstanceBuffer instance_buffer;
    MeshletBuffer meshlet_buffer;
    VertexBuffer vertex_buffer;
    IndexBuffer index_buffer;
    vec3 camera_position;
    uint instance_count;
    mat4 view_projection_matrix;
} pc;

// ============================================================================
// Mesh Shader Output
// ============================================================================

// Maximum vertices and primitives per mesh workgroup
layout(max_vertices = 64, max_primitives = 126) out;

// Output variables
layout(location = 0) out vec4 out_position[];    // World position
layout(location = 1) out vec3 out_normal[];      // World normal
layout(location = 2) out vec2 out_uv[];          // UV
layout(location = 3) out flat uint out_material_id[];  // Material ID
layout(location = 4) out flat uint out_instance_id[];  // Instance ID

// Indices for triangles
layout(triangles) out;

// ============================================================================
// Mesh Shader
// ============================================================================

#ifdef MESH_SHADER

// Task payload from task shader
taskPayloadSharedEXT uint meshlet_indices[32];

layout(local_size_x = 32) in;

void main() {
    uint local_invocation_id = gl_LocalInvocationID.x;
    uint instance_id = gl_WorkGroupID.y; // Instance from task shader

    // Load instance data
    DynamicVoxelInstanceData instance = pc.instance_buffer.instances[instance_id];

    // Get meshlet index for this invocation
    uint meshlet_index = meshlet_indices[gl_WorkGroupID.x];

    if (meshlet_index >= instance.meshlet_count) {
        return;
    }

    // Load meshlet
    uint64_t mesh_buffer_addr =
        (uint64_t(instance.mesh_buffer_address_hi) << 32) |
        instance.mesh_buffer_address_lo;
    MeshletBuffer meshlet_buf = MeshletBuffer(mesh_buffer_addr);
    DynamicVoxelMeshlet meshlet = meshlet_buf.meshlets[meshlet_index];

    // Culling: meshlet bounding sphere vs frustum
    vec3 world_center = (instance.model_matrix * vec4(meshlet.center, 1.0)).xyz;
    float world_radius = meshlet.radius * length(instance.model_matrix[0].xyz);

    // Simple distance culling
    if (distance(world_center, pc.camera_position) > 500.0) {
        return;
    }

    // Set mesh output counts
    SetMeshOutputsEXT(meshlet.vertex_count, meshlet.index_count / 3);

    // Output vertices
    uint64_t vertex_addr = mesh_buffer_addr; // Same buffer
    VertexBuffer vbuf = VertexBuffer(vertex_addr);

    for (uint i = 0; i < meshlet.vertex_count; ++i) {
        if (local_invocation_id == i % 32) {
            uint vertex_idx = meshlet.vertex_offset + i;
            DynamicVoxelVertex vertex = vbuf.vertices[vertex_idx];

            // Transform to world space
            vec4 world_pos = instance.model_matrix * vec4(vertex.position, 1.0);
            vec3 world_normal = normalize(instance.normal_matrix * vertex.normal);

            // Output vertex
            gl_MeshVerticesEXT[i].gl_Position =
                pc.view_projection_matrix * world_pos;

            out_position[i] = world_pos;
            out_normal[i] = world_normal;
            out_uv[i] = vertex.uv;
            out_material_id[i] = vertex.material_id;
            out_instance_id[i] = instance_id;
        }
    }

    // Output indices
    uint64_t index_addr = mesh_buffer_addr + 1024 * 1024; // Separate region
    IndexBuffer ibuf = IndexBuffer(index_addr);

    for (uint i = 0; i < meshlet.index_count / 3; ++i) {
        if (local_invocation_id == i % 32) {
            uint idx_offset = meshlet.index_offset + i * 3;
            uint i0 = ibuf.indices[idx_offset + 0];
            uint i1 = ibuf.indices[idx_offset + 1];
            uint i2 = ibuf.indices[idx_offset + 2];

            gl_MeshPrimitivesEXT[i] = uvec3(i0, i1, i2);
        }
    }
}

#endif // MESH_SHADER
```

### 2.4 Fragment Shader

```slang
// shaders/dynamic_voxel_fragment.slang

#extension GL_EXT_scalar_block_layout : require

// ============================================================================
// Input
// ============================================================================

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in flat uint in_material_id;
layout(location = 4) in flat uint in_instance_id;

// ============================================================================
// Descriptor Sets
// ============================================================================

layout(set = 0, binding = 0) uniform SceneData {
    mat4 view_matrix;
    mat4 projection_matrix;
    mat4 view_projection_matrix;
    vec3 camera_position;
    float time;
    vec3 sun_direction;
    float sun_intensity;
    vec3 sun_color;
    float ambient_intensity;
} scene;

layout(set = 1, binding = 0, scalar) buffer MaterialBuffer {
    MaterialData materials[];
};

struct MaterialData {
    vec4 albedo_color;
    vec4 emissive_color;
    float metallic;
    float roughness;
    float ao;
    uint flags;
    uint albedo_texture;
    uint normal_texture;
    uint metallic_roughness_texture;
    uint ao_texture;
    uint _pad;
};

layout(set = 2, binding = 0) uniform texture2D textures[];
layout(set = 2, binding = 1) uniform sampler sampler_linear;

// ============================================================================
// Output
// ============================================================================

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_material;

// ============================================================================
// Fragment Shader
// ============================================================================

void main() {
    // Load material
    MaterialData material = materials[in_material_id];

    // Base color
    vec3 base_color = material.albedo_color.rgb;

    // Sample textures if available
    if (material.albedo_texture != 0xFFFFFFFF) {
        vec4 tex_color = texture(sampler2D(textures[material.albedo_texture], sampler_linear), in_uv);
        base_color *= tex_color.rgb;
    }

    // Normal mapping
    vec3 normal = normalize(in_normal);
    if (material.normal_texture != 0xFFFFFFFF) {
        vec3 tangent_normal = texture(sampler2D(textures[material.normal_texture], sampler_linear), in_uv).xyz * 2.0 - 1.0;
        // TODO: TBN matrix calculation
        normal = normalize(normal + tangent_normal * 0.5);
    }

    // Simple directional lighting
    vec3 light_dir = normalize(scene.sun_direction);
    float ndl = max(dot(normal, light_dir), 0.0);

    vec3 ambient = base_color * scene.ambient_intensity;
    vec3 diffuse = base_color * ndl * scene.sun_color * scene.sun_intensity;

    vec3 final_color = ambient + diffuse;

    // Output
    out_color = vec4(final_color, 1.0);
    out_normal = vec4(normal * 0.5 + 0.5, 1.0);
    out_material = vec4(material.metallic, material.roughness, material.ao, 0.0);
}
```

---

## 3. Физика и разрушение

### 3.1 Алгоритм пересчёта центра масс

```cpp
// ProjectV.Physics.DynamicVoxelPhysics.cppm
export module ProjectV.Physics.DynamicVoxelPhysics;

import std;
import glm;
import ProjectV.Gameplay.DynamicVoxel;
import ProjectV.Physics.Jolt;

// Jolt Physics headers (internal)
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/CompoundShape.h>

export namespace projectv::physics {

/// Результат пересчёта физических свойств.
export struct PhysicsPropertiesResult {
    float total_mass_kg{0.0f};
    glm::vec3 center_of_mass_local{0.0f};
    glm::vec3 inertia_diagonal{0.0f};
    glm::mat3 inertia_tensor{0.0f};
    float volume_m3{0.0f};
    float average_density{0.0f};
};

/// Вычисляет физические свойства динамического воксельного объекта.
///
/// ## Algorithm
///
/// **Center of Mass:**
/// $$\vec{r}_{cm} = \frac{\sum_{i} m_i \vec{r}_i}{\sum_{i} m_i}$$
///
/// **Inertia Tensor (about COM):**
/// $$I = \sum_{i} m_i \left[ (\vec{r}_i - \vec{r}_{cm})^2 \cdot \mathbf{E} -
///     (\vec{r}_i - \vec{r}_{cm}) \otimes (\vec{r}_i - \vec{r}_{cm}) \right]$$
///
/// **Parallel Axis Theorem (if computing about origin):**
/// $$I_{cm} = I_0 - M \cdot (\vec{r}_{cm}^2 \cdot \mathbf{E} -
///     \vec{r}_{cm} \otimes \vec{r}_{cm})$$
///
/// ## Performance
/// - O(n) where n = number of solid voxels
/// - SIMD-optimized for dense grids
/// - Incremental update for partial destruction
export auto calculate_physics_properties(
    DynamicVoxelModel const& model
) noexcept -> PhysicsPropertiesResult;

/// Инкрементально обновляет физические свойства при удалении вокселей.
///
/// ## Algorithm
/// Instead of full recalculation, uses incremental update:
///
/// **New Mass:**
/// $$M_{new} = M_{old} - \sum_{i \in removed} m_i$$
///
/// **New COM:**
/// $$\vec{r}_{cm,new} = \frac{M_{old} \cdot \vec{r}_{cm,old} -
///     \sum_{i \in removed} m_i \vec{r}_i}{M_{new}}$$
///
/// **New Inertia:**
/// Subtract contribution of removed voxels, then apply parallel axis theorem.
///
/// ## Complexity
/// O(k) where k = number of removed voxels (typically small)
export auto update_physics_incremental(
    PhysicsPropertiesResult const& old_props,
    DynamicVoxelModel const& model,
    std::span<LocalVoxelCoord const> removed_voxels
) noexcept -> PhysicsPropertiesResult;

/// Реализация вычисления физических свойств.
export class VoxelPhysicsCalculator {
public:
    /// Размер одного вокселя в метрах.
    static constexpr float VOXEL_SIZE_M = 0.25f;

    /// Объём одного вокселя в м³.
    static constexpr float VOXEL_VOLUME_M3 = VOXEL_SIZE_M * VOXEL_SIZE_M * VOXEL_SIZE_M;

    /// Вычисляет свойства для DenseGrid.
    static auto calculate_dense_grid(
        std::span<uint32_t const> grid,
        uint32_t grid_size,
        std::span<VoxelMaterialInfo const> materials
    ) noexcept -> PhysicsPropertiesResult;

    /// Вычисляет свойства для SVO.
    static auto calculate_svo(
        voxel::SVOTree const& svo,
        std::span<VoxelMaterialInfo const> materials
    ) noexcept -> PhysicsPropertiesResult;

private:
    /// Вспомогательная структура для аккумуляции.
    struct Accumulator {
        float total_mass{0.0f};
        glm::dvec3 weighted_position{0.0};  // double for precision
        glm::dmat3 inertia_tensor{0.0};
        uint64_t voxel_count{0};
    };

    /// Обрабатывает один воксель.
    static auto process_voxel(
        Accumulator& acc,
        glm::vec3 local_pos,
        float density_kg_m3,
        float voxel_volume
    ) noexcept -> void;

    /// Финализирует вычисления (COM, inertia about COM).
    static auto finalize(
        Accumulator const& acc,
        float voxel_volume
    ) noexcept -> PhysicsPropertiesResult;
};

// ============================================================================
// Implementation
// ============================================================================

auto VoxelPhysicsCalculator::calculate_dense_grid(
    std::span<uint32_t const> grid,
    uint32_t grid_size,
    std::span<VoxelMaterialInfo const> materials
) noexcept -> PhysicsPropertiesResult {

    Accumulator acc;
    float half_size = static_cast<float>(grid_size) * VOXEL_SIZE_M * 0.5f;

    for (uint32_t z = 0; z < grid_size; ++z) {
        for (uint32_t y = 0; y < grid_size; ++y) {
            for (uint32_t x = 0; x < grid_size; ++x) {
                uint32_t voxel_data = grid[x + y * grid_size + z * grid_size * grid_size];

                if (voxel_data == 0) {
                    continue;  // Air
                }

                // Extract material ID
                uint16_t material_id = static_cast<uint16_t>(voxel_data & 0xFFFF);

                // Get material density
                float density = 1000.0f;  // Default
                if (material_id < materials.size()) {
                    density = materials[material_id].density_kg_m3;
                }

                // Local position (center of voxel)
                glm::vec3 local_pos(
                    x * VOXEL_SIZE_M - half_size + VOXEL_SIZE_M * 0.5f,
                    y * VOXEL_SIZE_M - half_size + VOXEL_SIZE_M * 0.5f,
                    z * VOXEL_SIZE_M - half_size + VOXEL_SIZE_M * 0.5f
                );

                process_voxel(acc, local_pos, density, VOXEL_VOLUME_M3);
            }
        }
    }

    return finalize(acc, VOXEL_VOLUME_M3);
}

auto VoxelPhysicsCalculator::process_voxel(
    Accumulator& acc,
    glm::vec3 local_pos,
    float density_kg_m3,
    float voxel_volume
) noexcept -> void {

    float voxel_mass = density_kg_m3 * voxel_volume;

    acc.total_mass += voxel_mass;
    acc.weighted_position += glm::dvec3(local_pos) * static_cast<double>(voxel_mass);
    acc.voxel_count += 1;

    // Inertia contribution about origin (will shift to COM later)
    // For a cube: I = (1/6) * m * a^2 where a = side length
    // For voxel cube: I_diag = (1/6) * m * VOXEL_SIZE_M^2
    float voxel_inertia = (1.0f / 6.0f) * voxel_mass * VOXEL_SIZE_M * VOXEL_SIZE_M;

    // Parallel axis theorem: add m * r^2 for each axis
    float r_sq = dot(local_pos, local_pos);

    acc.inertia_tensor[0][0] += voxel_inertia + voxel_mass * (r_sq - local_pos.x * local_pos.x);
    acc.inertia_tensor[1][1] += voxel_inertia + voxel_mass * (r_sq - local_pos.y * local_pos.y);
    acc.inertia_tensor[2][2] += voxel_inertia + voxel_mass * (r_sq - local_pos.z * local_pos.z);

    // Off-diagonal terms (negative because of tensor definition)
    acc.inertia_tensor[0][1] -= voxel_mass * local_pos.x * local_pos.y;
    acc.inertia_tensor[0][2] -= voxel_mass * local_pos.x * local_pos.z;
    acc.inertia_tensor[1][2] -= voxel_mass * local_pos.y * local_pos.z;
}

auto VoxelPhysicsCalculator::finalize(
    Accumulator const& acc,
    float voxel_volume
) noexcept -> PhysicsPropertiesResult {

    PhysicsPropertiesResult result;

    if (acc.total_mass <= 0.0f || acc.voxel_count == 0) {
        return result;  // No mass
    }

    result.total_mass_kg = acc.total_mass;
    result.volume_m3 = static_cast<float>(acc.voxel_count) * voxel_volume;
    result.average_density = acc.total_mass / result.volume_m3;

    // Center of mass
    result.center_of_mass_local = glm::vec3(
        acc.weighted_position / static_cast<double>(acc.total_mass)
    );

    // Shift inertia tensor to center of mass using parallel axis theorem
    // I_com = I_origin - M * [r_cm^2 * E - r_cm ⊗ r_cm]
    glm::vec3 r_cm = result.center_of_mass_local;
    float r_cm_sq = glm::dot(r_cm, r_cm);

    glm::mat3 shift;
    shift[0][0] = acc.total_mass * (r_cm_sq - r_cm.x * r_cm.x);
    shift[1][1] = acc.total_mass * (r_cm_sq - r_cm.y * r_cm.y);
    shift[2][2] = acc.total_mass * (r_cm_sq - r_cm.z * r_cm.z);
    shift[0][1] = -acc.total_mass * r_cm.x * r_cm.y;
    shift[0][2] = -acc.total_mass * r_cm.x * r_cm.z;
    shift[1][2] = -acc.total_mass * r_cm.y * r_cm.z;
    shift[1][0] = shift[0][1];
    shift[2][0] = shift[0][2];
    shift[2][1] = shift[1][2];

    result.inertia_tensor = glm::mat3(acc.inertia_tensor) - shift;

    // Extract diagonal for Jolt (assumes approximately aligned principal axes)
    result.inertia_diagonal = glm::vec3(
        result.inertia_tensor[0][0],
        result.inertia_tensor[1][1],
        result.inertia_tensor[2][2]
    );

    return result;
}

} // namespace projectv::physics
```

### 3.2 Интеграция с Jolt Physics

```cpp
// ProjectV.Physics.DynamicVoxelJolt.cpp
module ProjectV.Physics.DynamicVoxelPhysics;

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/CompoundShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

import std;
import glm;
import ProjectV.Physics.DynamicVoxelPhysics;
import ProjectV.Gameplay.DynamicVoxel;

namespace projectv::physics {

/// Обновляет Jolt Body после изменения вокселей.
auto update_jolt_body(
    JPH::BodyInterface& body_interface,
    JPH::BodyID body_id,
    PhysicsPropertiesResult const& props,
    DynamicVoxelPhysics& physics_comp
) -> void {

    if (!body_id.IsValid()) {
        return;
    }

    JPH::BodyLockWrite lock(body_interface, body_id);
    if (!lock.Succeeded()) {
        return;
    }

    JPH::Body& body = lock.GetBody();

    // Update mass properties
    JPH::MassProperties mass_props;
    mass_props.mMass = props.total_mass_kg;

    // Set inertia tensor
    JPH::Vec3 inertia(
        props.inertia_diagonal.x,
        props.inertia_diagonal.y,
        props.inertia_diagonal.z
    );

    // For simplicity, use diagonal inertia (works well for roughly symmetric objects)
    mass_props.mInertia = JPH::Mat44::sIdentity();
    mass_props.mInertia(0, 0) = inertia[0];
    mass_props.mInertia(1, 1) = inertia[1];
    mass_props.mInertia(2, 2) = inertia[2];

    body.SetMassProperties(JPH::EAllowedDOFs::All, mass_props);

    // Update center of mass offset in shape
    // Note: This requires recreating the shape in Jolt
    // For now, we adjust the body position to account for COM shift

    // Store updated properties
    physics_comp.total_mass_kg = props.total_mass_kg;
    physics_comp.center_of_mass_local = props.center_of_mass_local;
    physics_comp.inertia_diagonal = props.inertia_diagonal;
    physics_comp.volume_m3 = props.volume_m3;
    physics_comp.average_density = props.average_density;
}

/// Создаёт Jolt collision shape для воксельного объекта.
auto create_voxel_collision_shape(
    DynamicVoxelModel const& model
) -> JPH::RefConst<JPH::Shape> {

    // For small objects, use compound of boxes
    // For large objects, use convex decomposition or approximate with OBB

    uint32_t grid_size = 1u << model.grid_size_log2;

    if (model.storage_type == VoxelStorageType::DenseGrid) {
        // Count solid voxels
        size_t solid_count = 0;
        for (auto voxel : model.dense_grid) {
            if (voxel != 0) {
                ++solid_count;
            }
        }

        // If too many voxels, use approximate shape
        if (solid_count > 1000) {
            // Use bounding box or convex hull
            auto bounds = model.local_bounds();
            JPH::AABox aabb;
            aabb.mMin = JPH::Vec3(-bounds.x * 0.5f, -bounds.y * 0.5f, -bounds.z * 0.5f);
            aabb.mMax = JPH::Vec3(bounds.x * 0.5f, bounds.y * 0.5f, bounds.z * 0.5f);

            return new JPH::BoxShape(aabb.GetExtent() * 0.5f);
        }

        // Create compound shape from individual voxels
        JPH::CompoundShapeSettings compound_settings;

        float half_size = static_cast<float>(grid_size) * 0.125f; // VOXEL_SIZE_M * 0.5

        for (uint32_t i = 0; i < model.dense_grid.size(); ++i) {
            if (model.dense_grid[i] == 0) continue;

            uint32_t x = i % grid_size;
            uint32_t y = (i / grid_size) % grid_size;
            uint32_t z = i / (grid_size * grid_size);

            JPH::Vec3 position(
                (static_cast<float>(x) - grid_size * 0.5f + 0.5f) * 0.25f,
                (static_cast<float>(y) - grid_size * 0.5f + 0.5f) * 0.25f,
                (static_cast<float>(z) - grid_size * 0.5f + 0.5f) * 0.25f
            );

            auto box_shape = new JPH::BoxShape(JPH::Vec3(0.125f, 0.125f, 0.125f));
            compound_settings.AddShape(position, JPH::Quat::sIdentity(), box_shape);
        }

        return compound_settings.Create().Get();
    }

    // For SVO, use bounding box for now
    auto bounds = model.local_bounds();
    return new JPH::BoxShape(JPH::Vec3(
        bounds.x * 0.125f,
        bounds.y * 0.125f,
        bounds.z * 0.125f
    ));
}

} // namespace projectv::physics
```

### 3.3 Обработка разрушения

```cpp
// ProjectV.Gameplay.DynamicVoxelDestruction.cppm
export module ProjectV.Gameplay.DynamicVoxelDestruction;

import std;
import glm;
import flecs;
import ProjectV.Gameplay.DynamicVoxel;
import ProjectV.Physics.DynamicVoxelPhysics;

export namespace projectv::gameplay {

/// Результат обработки повреждения.
export struct DamageResult {
    std::vector<LocalVoxelCoord> destroyed_voxels;
    std::vector<std::vector<LocalVoxelCoord>> detached_fragments;
    bool should_destroy_entity{false};
};

/// Обрабатывает повреждение воксельного объекта.
///
/// ## Algorithm
/// 1. Find voxels within damage radius
/// 2. Apply damage based on distance and material strength
/// 3. Check for disconnected fragments (flood fill)
/// 4. Create debris entities for fragments
export class VoxelDamageProcessor {
public:
    /// Применяет взрывное повреждение.
    static auto apply_explosion_damage(
        DynamicVoxelModel& model,
        DynamicVoxelDamage& damage,
        glm::vec3 local_epicenter,
        float radius,
        float damage_amount
    ) noexcept -> DamageResult;

    /// Применяет повреждение от попадания.
    static auto apply_impact_damage(
        DynamicVoxelModel& model,
        DynamicVoxelDamage& damage,
        glm::vec3 local_point,
        float radius,
        float damage_amount
    ) noexcept -> DamageResult;

    /// Находит отключённые фрагменты (flood fill).
    static auto find_detached_fragments(
        DynamicVoxelModel const& model,
        std::span<LocalVoxelCoord const> destroyed_voxels
    ) noexcept -> std::vector<std::vector<LocalVoxelCoord>>;

    /// Создаёт entity для обломков.
    static auto create_debris_entity(
        flecs::world& world,
        DynamicVoxelModel const& source_model,
        std::vector<LocalVoxelCoord> const& fragment_voxels,
        glm::vec3 world_position,
        glm::quat world_rotation
    ) -> flecs::entity;

private:
    /// Проверяет, связан ли воксель с "якором" (например, нижний слой).
    static auto is_connected_to_anchor(
        DynamicVoxelModel const& model,
        LocalVoxelCoord start,
        std::span<LocalVoxelCoord const> removed_voxels
    ) noexcept -> bool;
};

// ============================================================================
// Implementation
// ============================================================================

auto VoxelDamageProcessor::apply_explosion_damage(
    DynamicVoxelModel& model,
    DynamicVoxelDamage& damage,
    glm::vec3 local_epicenter,
    float radius,
    float damage_amount
) noexcept -> DamageResult {

    DamageResult result;

    uint32_t grid_size = 1u << model.grid_size_log2;
    float voxel_size = 0.25f;

    // Find voxels within radius
    for (uint32_t i = 0; i < model.dense_grid.size(); ++i) {
        uint32_t voxel_data = model.dense_grid[i];
        if (voxel_data == 0) continue;  // Air

        uint32_t x = i % grid_size;
        uint32_t y = (i / grid_size) % grid_size;
        uint32_t z = i / (grid_size * grid_size);

        glm::vec3 voxel_pos(
            (static_cast<float>(x) - grid_size * 0.5f + 0.5f) * voxel_size,
            (static_cast<float>(y) - grid_size * 0.5f + 0.5f) * voxel_size,
            (static_cast<float>(z) - grid_size * 0.5f + 0.5f) * voxel_size
        );

        float distance = glm::length(voxel_pos - local_epicenter);

        if (distance > radius) continue;

        // Damage falloff (linear)
        float damage_factor = 1.0f - (distance / radius);
        float effective_damage = damage_amount * damage_factor;

        // Get material strength
        uint16_t material_id = static_cast<uint16_t>(voxel_data & 0xFFFF);
        float strength = 1e6f;  // Default
        if (material_id < model.materials.size()) {
            strength = model.materials[material_id].strength_pa;
        }

        // Check if destroyed
        if (effective_damage > strength * 0.1f) {  // Threshold
            result.destroyed_voxels.push_back({uint16_t(x), uint16_t(y), uint16_t(z)});
            model.dense_grid[i] = 0;  // Set to air
        }
    }

    // Find detached fragments
    if (!result.destroyed_voxels.empty()) {
        result.detached_fragments = find_detached_fragments(model, result.destroyed_voxels);
    }

    // Mark for physics update
    model.physics_dirty = true;
    model.mesh_dirty = true;

    return result;
}

auto VoxelDamageProcessor::find_detached_fragments(
    DynamicVoxelModel const& model,
    std::span<LocalVoxelCoord const> destroyed_voxels
) noexcept -> std::vector<std::vector<LocalVoxelCoord>> {

    std::vector<std::vector<LocalVoxelCoord>> fragments;

    uint32_t grid_size = 1u << model.grid_size_log2;

    // Create a set of destroyed positions for quick lookup
    std::unordered_set<uint32_t> destroyed_set;
    for (auto const& coord : destroyed_voxels) {
        destroyed_set.insert(coord.x + coord.y * grid_size + coord.z * grid_size * grid_size);
    }

    // Track visited voxels
    std::vector<bool> visited(model.dense_grid.size(), false);

    // Flood fill from bottom layer (anchor)
    std::queue<LocalVoxelCoord> queue;

    // Add all bottom-layer voxels as potential anchors
    for (uint32_t x = 0; x < grid_size; ++x) {
        for (uint32_t z = 0; z < grid_size; ++z) {
            LocalVoxelCoord coord{uint16_t(x), 0, uint16_t(z)};
            uint32_t idx = coord.x + coord.y * grid_size + coord.z * grid_size * grid_size;

            if (model.dense_grid[idx] != 0 && destroyed_set.find(idx) == destroyed_set.end()) {
                queue.push(coord);
                visited[idx] = true;
            }
        }
    }

    // Flood fill to mark connected voxels
    constexpr int16_t neighbors[6][3] = {
        {1, 0, 0}, {-1, 0, 0},
        {0, 1, 0}, {0, -1, 0},
        {0, 0, 1}, {0, 0, -1}
    };

    while (!queue.empty()) {
        LocalVoxelCoord current = queue.front();
        queue.pop();

        for (auto const& [dx, dy, dz] : neighbors) {
            int16_t nx = current.x + dx;
            int16_t ny = current.y + dy;
            int16_t nz = current.z + dz;

            if (nx < 0 || nx >= int16_t(grid_size) ||
                ny < 0 || ny >= int16_t(grid_size) ||
                nz < 0 || nz >= int16_t(grid_size)) {
                continue;
            }

            uint32_t idx = nx + ny * grid_size + nz * grid_size * grid_size;

            if (visited[idx] || model.dense_grid[idx] == 0 ||
                destroyed_set.find(idx) != destroyed_set.end()) {
                continue;
            }

            visited[idx] = true;
            queue.push({uint16_t(nx), uint16_t(ny), uint16_t(nz)});
        }
    }

    // Collect unvisited voxels as separate fragments
    std::vector<LocalVoxelCoord> unvisited;
    for (uint32_t i = 0; i < model.dense_grid.size(); ++i) {
        if (model.dense_grid[i] != 0 && !visited[i] &&
            destroyed_set.find(i) == destroyed_set.end()) {
            uint32_t x = i % grid_size;
            uint32_t y = (i / grid_size) % grid_size;
            uint32_t z = i / (grid_size * grid_size);
            unvisited.push_back({uint16_t(x), uint16_t(y), uint16_t(z)});
        }
    }

    // Group unvisited into connected fragments
    // (Simplified: treat all as one fragment)
    if (!unvisited.empty()) {
        fragments.push_back(std::move(unvisited));
    }

    return fragments;
}

} // namespace projectv::gameplay
```

---

## 4. Пример использования

### 4.1 Создание динамического объекта

```cpp
// Пример создания танка как динамического воксельного объекта

auto create_tank_entity(flecs::world& world, glm::vec3 position) -> flecs::entity {
    auto e = world.entity("Tank");

    // Model
    auto& model = e.set<DynamicVoxelModel>({
        .storage_type = VoxelStorageType::DenseGrid,
        .grid_size_log2 = 5,  // 32³ = 32768 voxels
        .dense_grid(32 * 32 * 32, 0)
    });

    // Load voxel data from file or procedural generation
    load_tank_voxels(model);

    // Transform
    e.set<DynamicVoxelTransform>({
        .position = position,
        .rotation = glm::quat(1, 0, 0, 0),
        .scale = glm::vec3(1.0f)
    });

    // Physics
    e.set<DynamicVoxelPhysics>({
        .motion_type = DynamicVoxelPhysics::MotionType::Dynamic,
        .motion_quality = DynamicVoxelPhysics::MotionQuality::LinearCast,
        .linear_damping = 0.3f,
        .angular_damping = 0.5f
    });

    // Damage
    e.set<DynamicVoxelDamage>({
        .health = 100.0f,
        .max_health = 100.0f,
        .destruction_type = DynamicVoxelDamage::DestructionType::Explode
    });

    // Render data
    e.set<DynamicVoxelRenderData>({});

    return e;
}
```

---

## Статус

| Компонент                 | Статус         | Приоритет |
|---------------------------|----------------|-----------|
| ECS Components            | Специфицирован | P0        |
| Task/Mesh Shaders (Slang) | Специфицирован | P0        |
| Physics Calculator        | Специфицирован | P0        |
| Jolt Integration          | Специфицирован | P1        |
| Destruction System        | Специфицирован | P1        |

---

## Ссылки

- [00_engine-structure.md](../01_core/01_engine_structure.md)
- [30_ca_physics_bridge.md](../04_physics_ca/06_ca_physics_bridge.md)
- [31_job_system_p2300_spec.md](../01_core/05_job_system.md)
- [Jolt Physics](https://github.com/jrouwe/JoltPhysics)

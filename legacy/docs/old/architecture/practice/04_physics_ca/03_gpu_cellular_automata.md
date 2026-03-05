# GPU Cellular Automata Specification

---

## Обзор

Документ описывает систему **GPU Cellular Automata (CA)** для симуляции:

- Жидкостей (вода, лава, нефть)
- Сыпучих материалов (песок, гравий, снег)
- Газов и дыма
- Распространения огня

CA работает полностью на GPU через compute shaders и синхронизируется с JoltPhysics для коллизий.

---

## 1. Data Layout

### 1.1 Cell State (GPU-aligned)

```cpp
// Спецификация интерфейса
export module ProjectV.Simulation.CellularAutomata;

import std;
import glm;

export namespace projectv::simulation {

/// Состояние ячейки CA (16 байт, std430 aligned)
struct alignas(16) CellState {
    float density{0.0f};        ///< Плотность материала [0-1]
    float velocity_x{0.0f};     ///< Скорость по X
    float velocity_y{0.0f};     ///< Скорость по Y (Y-up)
    float velocity_z{0.0f};     ///< Скорость по Z
    uint32_t material_type{0};  ///< Тип материала (air, water, sand, etc.)
    uint32_t flags{0};          ///< Флаги (burning, source, drain)
    float temperature{0.0f};    ///< Температура для огня/лавы
    float pressure{0.0f};       ///< Давление для газов
};

static_assert(sizeof(CellState) == 32);
static_assert(alignof(CellState) == 16);

/// Типы материалов для CA
export enum class CellMaterial : uint32_t {
    Air = 0,
    Water,
    Oil,
    Lava,
    Sand,
    Gravel,
    Snow,
    Smoke,
    Fire,
    Steam,
    Acid,
    Custom  // Для пользовательских материалов
};

/// Флаги ячейки
export enum class CellFlags : uint32_t {
    None = 0,
    Burning = 1 << 0,       ///< Горит (огонь)
    Source = 1 << 1,       ///< Источник (бесконечный spawning)
    Drain = 1 << 2,        ///< Сток (удаляет материал)
    Static = 1 << 3,       ///< Статичная (не двигается)
    Explosive = 1 << 4,    ///< Взрывоопасная
    Corrosive = 1 << 5,    ///< Разъедает окружение
};

} // namespace projectv::simulation
```

### 1.2 Double Buffer for GPU

```cpp
export namespace projectv::simulation {

/// Двойная буферизация для CA симуляции
class CellularAutomataGrid {
public:
    /// Создаёт сетку заданного размера.
    /// Размер должен быть кратен 8 для выравнивания workgroups.
    [[nodiscard]] static auto create(
        uint32_t size_x,
        uint32_t size_y,
        uint32_t size_z,
        GPUAllocator const& allocator
    ) noexcept -> std::expected<CellularAutomataGrid, CAError>;

    ~CellularAutomataGrid() noexcept;

    // Move-only
    CellularAutomataGrid(CellularAutomataGrid&&) noexcept;
    CellularAutomataGrid& operator=(CellularAutomataGrid&&) noexcept;
    CellularAutomataGrid(const CellularAutomataGrid&) = delete;
    CellularAutomataGrid& operator=(const CellularAutomataGrid&) = delete;

    /// Переключает буферы (current ↔ next).
    auto swap_buffers() noexcept -> void;

    /// Очищает сетку.
    auto clear() noexcept -> void;

    /// Устанавливает ячейку (CPU → GPU upload).
    auto set_cell(uint32_t x, uint32_t y, uint32_t z, CellState const& state) noexcept -> void;

    /// Получает ячейку (GPU → CPU download).
    [[nodiscard]] auto get_cell(uint32_t x, uint32_t y, uint32_t z) const noexcept
        -> std::expected<CellState, CAError>;

    // --- GPU Buffers ---

    [[nodiscard]] auto current_buffer() const noexcept -> VkBuffer { return buffers_[current_].buffer; }
    [[nodiscard]] auto next_buffer() const noexcept -> VkBuffer { return buffers_[1 - current_].buffer; }

    [[nodiscard]] auto size() const noexcept -> glm::uvec3 { return size_; }
    [[nodiscard]] auto cell_count() const noexcept -> size_t {
        return size_t(size_.x) * size_.y * size_.z;
    }

private:
    explicit CellularAutomataGrid(
        glm::uvec3 size,
        std::array<BufferAllocation, 2> buffers
    ) noexcept;

    glm::uvec3 size_{0};
    std::array<BufferAllocation, 2> buffers_;
    uint32_t current_{0};
};

} // namespace projectv::simulation
```

---

## 2. Compute Shader Dispatch

### 2.1 Dispatch Parameters (UBO)

```cpp
export namespace projectv::simulation {

/// Параметры симуляции CA (UBO, std140 aligned)
struct alignas(16) CASimulationParams {
    uint32_t grid_size_x;
    uint32_t grid_size_y;
    uint32_t grid_size_z;
    float delta_time;

    float gravity{-9.81f};           ///< Гравитация
    float viscosity{0.1f};           ///< Вязкость для жидкостей
    float friction{0.5f};            ///< Трение для сыпучих
    float diffusion{0.01f};          ///< Диффузия

    uint32_t step_count{1};          ///< Под-шагов за кадр
    uint32_t material_count;         ///< Количество материалов
    float temperature_decay{0.1f};   ///< Остывание
    float burn_rate{0.05f};          ///< Скорость горения

    // Boundary conditions
    glm::ivec3 boundary_min{0, 0, 0};
    glm::ivec3 boundary_max{255, 255, 255};
};

/// Конфигурация dispatch
struct CADispatchConfig {
    uint32_t workgroup_size_x{8};
    uint32_t workgroup_size_y{8};
    uint32_t workgroup_size_z{8};

    [[nodiscard]] auto dispatch_size(glm::uvec3 grid_size) const noexcept -> glm::uvec3 {
        return {
            (grid_size.x + workgroup_size_x - 1) / workgroup_size_x,
            (grid_size.y + workgroup_size_y - 1) / workgroup_size_y,
            (grid_size.z + workgroup_size_z - 1) / workgroup_size_z
        };
    }
};

} // namespace projectv::simulation
```

### 2.2 Slang Shader: Liquid Simulation

```slang
// voxel/VoxelCA.slang
module VoxelCA;

import SVOStructures;

// Double buffers
[[vk::binding(0, 0)]]
StructuredBuffer<CellState> currentCells;

[[vk::binding(1, 0)]]
RWStructuredBuffer<CellState> nextCells;

// Parameters (UBO)
[[vk::binding(2, 0)]]
uniform CASimulationParams {
    uint3 grid_size;
    float delta_time;
    float gravity;
    float viscosity;
    float friction;
    float diffusion;
    uint step_count;
    uint material_count;
    float temperature_decay;
    float burn_rate;
    int3 boundary_min;
    int3 boundary_max;
} params;

// Material properties lookup
[[vk::binding(3, 0)]]
StructuredBuffer<MaterialProperties> materials;

struct MaterialProperties {
    float density;        // kg/m³
    float viscosity;
    float flammability;
    float conductivity;
    uint type;
    uint flags;
};

// Cell index calculation
uint cellIndex(uint3 pos) {
    return pos.x + pos.y * params.grid_size.x + pos.z * params.grid_size.x * params.grid_size.y;
}

bool inBounds(int3 pos) {
    return all(pos >= params.boundary_min) && all(pos <= params.boundary_max);
}

// Liquid simulation step
[numthreads(8, 8, 8)]
void csLiquidStep(uint3 tid: SV_DispatchThreadID) {
    if (any(tid >= params.grid_size)) return;

    uint idx = cellIndex(tid);
    CellState current = currentCells[idx];

    // Skip empty or static cells
    if (current.density <= 0.0 || (current.flags & STATIC_CELL)) {
        nextCells[idx] = current;
        return;
    }

    // Apply gravity
    current.velocity_y += params.gravity * params.delta_time;

    // Compute flow direction
    float3 flow_dir = float3(0, 0, 0);

    // Check neighbors for pressure gradient
    int3 neighbors[6] = {
        int3(1, 0, 0), int3(-1, 0, 0),
        int3(0, 1, 0), int3(0, -1, 0),
        int3(0, 0, 1), int3(0, 0, -1)
    };

    float total_pressure = current.pressure;
    int valid_neighbors = 0;

    for (int i = 0; i < 6; ++i) {
        int3 neighbor_pos = int3(tid) + neighbors[i];
        if (!inBounds(neighbor_pos)) continue;

        uint neighbor_idx = cellIndex(uint3(neighbor_pos));
        CellState neighbor = currentCells[neighbor_idx];

        if (neighbor.material_type == AIR_MATERIAL) {
            // Flow toward empty cells
            flow_dir += float3(neighbors[i]) * (1.0 - neighbor.density);
        } else if (neighbor.material_type == current.material_type) {
            // Equalize pressure
            float pressure_diff = current.pressure - neighbor.pressure;
            flow_dir += float3(neighbors[i]) * pressure_diff * 0.5;
        }

        total_pressure += neighbor.pressure;
        valid_neighbors++;
    }

    // Normalize flow
    float flow_len = length(flow_dir);
    if (flow_len > 0.001) {
        flow_dir /= flow_len;
    }

    // Apply velocity
    current.velocity_x += flow_dir.x * params.diffusion * params.delta_time;
    current.velocity_y += flow_dir.y * params.diffusion * params.delta_time;
    current.velocity_z += flow_dir.z * params.diffusion * params.delta_time;

    // Damping (viscosity)
    current.velocity_x *= (1.0 - params.viscosity);
    current.velocity_y *= (1.0 - params.viscosity);
    current.velocity_z *= (1.0 - params.viscosity);

    // Update pressure
    current.pressure = total_pressure / max(valid_neighbors, 1);

    // Temperature decay
    current.temperature *= (1.0 - params.temperature_decay * params.delta_time);

    // Write result
    nextCells[idx] = current;
}

// Sand/granular simulation step
[numthreads(8, 8, 8)]
void csGranularStep(uint3 tid: SV_DispatchThreadID) {
    if (any(tid >= params.grid_size)) return;

    uint idx = cellIndex(tid);
    CellState current = currentCells[idx];

    if (current.density <= 0.0 || (current.flags & STATIC_CELL)) {
        nextCells[idx] = current;
        return;
    }

    // Granular materials fall straight down
    current.velocity_y += params.gravity * params.delta_time;

    // Check if can fall down
    int3 below_pos = int3(tid) + int3(0, -1, 0);
    if (inBounds(below_pos)) {
        uint below_idx = cellIndex(uint3(below_pos));
        CellState below = currentCells[below_idx];

        if (below.density > 0.5 && below.material_type != AIR_MATERIAL) {
            // Can't fall, try diagonal
            int3 diag_positions[4] = {
                int3(1, -1, 0), int3(-1, -1, 0),
                int3(0, -1, 1), int3(0, -1, -1)
            };

            for (int i = 0; i < 4; ++i) {
                int3 diag_pos = int3(tid) + diag_positions[i];
                if (!inBounds(diag_pos)) continue;

                uint diag_idx = cellIndex(uint3(diag_pos));
                if (currentCells[diag_idx].density < 0.5) {
                    // Move diagonally
                    current.velocity_x = float(diag_positions[i].x) * 2.0;
                    current.velocity_z = float(diag_positions[i].z) * 2.0;
                    break;
                }
            }
        }
    }

    // Apply friction
    current.velocity_x *= (1.0 - params.friction);
    current.velocity_z *= (1.0 - params.friction);

    nextCells[idx] = current;
}

// Fire spreading step
[numthreads(8, 8, 8)]
void csFireStep(uint3 tid: SV_DispatchThreadID) {
    if (any(tid >= params.grid_size)) return;

    uint idx = cellIndex(tid);
    CellState current = currentCells[idx];

    if (current.flags & BURNING_FLAG) {
        // Reduce density (fuel consumption)
        current.density -= params.burn_rate * params.delta_time;
        current.temperature = min(current.temperature + 10.0, 1500.0);

        // Spread fire to neighbors
        int3 neighbors[6] = {
            int3(1, 0, 0), int3(-1, 0, 0),
            int3(0, 1, 0), int3(0, -1, 0),
            int3(0, 0, 1), int3(0, 0, -1)
        };

        for (int i = 0; i < 6; ++i) {
            int3 neighbor_pos = int3(tid) + neighbors[i];
            if (!inBounds(neighbor_pos)) continue;

            uint neighbor_idx = cellIndex(uint3(neighbor_pos));
            CellState neighbor = currentCells[neighbor_idx];

            MaterialProperties mat = materials[neighbor.material_type];
            if (mat.flammability > 0.5 && neighbor.temperature > 200.0) {
                // Ignite
                neighbor.flags |= BURNING_FLAG;
                nextCells[neighbor_idx] = neighbor;
            }
        }

        // Burn out
        if (current.density <= 0.0) {
            current.material_type = AIR_MATERIAL;
            current.flags = 0;
            current.density = 0.0;
        }
    }

    nextCells[idx] = current;
}

// Constants
static const uint AIR_MATERIAL = 0;
static const uint WATER_MATERIAL = 1;
static const uint SAND_MATERIAL = 4;
static const uint FIRE_MATERIAL = 8;
static const uint STATIC_CELL = 1 << 3;
static const uint BURNING_FLAG = 1 << 0;
```

---

## 3. Jolt Physics Integration

### 3.1 Collision Sync

```cpp
export namespace projectv::simulation {

/// Точка синхронизации между CA и физикой
struct CACollisionSyncPoint {
    glm::ivec3 cell_position;
    float penetration_depth;
    glm::vec3 normal;
    uint64_t body_id;  // Jolt BodyID
};

/// Синхронизатор CA ↔ Jolt
class CACollisionBridge {
public:
    /// Создаёт мост между CA и физикой.
    [[nodiscard]] static auto create(
        CellularAutomataGrid& grid,
        PhysicsWorld& physics
    ) noexcept -> std::expected<CACollisionBridge, CACollisionError>;

    /// Собирает коллизии из Jolt и обновляет CA.
    /// Вызывается после physics.step(), до CA.step().
    auto sync_from_physics(float delta_time) noexcept -> void;

    /// Применяет силы от CA к телам в Jolt.
    /// Вызывается после CA.step().
    auto sync_to_physics(float delta_time) noexcept -> void;

    /// Регистрирует динамическое тело для взаимодействия с CA.
    auto register_body(uint64_t body_id, float radius) noexcept -> void;

    /// Удаляет тело из симуляции CA.
    auto unregister_body(uint64_t body_id) noexcept -> void;

private:
    CellularAutomataGrid* grid_;
    PhysicsWorld* physics_;

    struct RegisteredBody {
        uint64_t body_id;
        float radius;
        glm::vec3 last_position;
    };

    std::vector<RegisteredBody> registered_bodies_;
};

} // namespace projectv::simulation
```

### 3.2 Integration Pattern

```cpp
// Main loop integration
auto update_simulation(
    float delta_time,
    PhysicsWorld& physics,
    CellularAutomataGrid& ca_grid,
    CACollisionBridge& bridge,
    VkCommandBuffer cmd,
    VkPipeline ca_pipeline
) -> void {

    // 1. Step physics
    physics.step(delta_time);

    // 2. Sync collisions from physics to CA
    bridge.sync_from_physics(delta_time);

    // 3. Dispatch CA compute shaders
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ca_pipeline);

    // Liquid step
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            ca_layout, 0, 1, &ca_descriptor_set, 0, nullptr);

    glm::uvec3 dispatch_size = CADispatchConfig{}.dispatch_size(ca_grid.size());
    vkCmdDispatch(cmd, dispatch_size.x, dispatch_size.y, dispatch_size.z);

    // 4. Memory barrier between steps
    VkMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
    };
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr
    );

    // 5. Swap buffers
    ca_grid.swap_buffers();

    // 6. Apply forces from CA to physics
    bridge.sync_to_physics(delta_time);
}
```

---

## 4. Integration with Voxel World

### 4.1 Voxel ↔ CA Mapping

```cpp
export namespace projectv::simulation {

/// Маппинг между воксельным миром и CA сеткой
class VoxelCAMapping {
public:
    /// Размер ячейки CA в вокселях (например, 4³ вокселя = 1 CA ячейка)
    static constexpr uint32_t VOXELS_PER_CELL = 4;

    /// Конвертирует координаты вокселя в координаты CA.
    [[nodiscard]] static auto voxel_to_ca(glm::ivec3 voxel_pos) noexcept -> glm::ivec3 {
        return voxel_pos / static_cast<int>(VOXELS_PER_CELL);
    }

    /// Конвертирует координаты CA в диапазон вокселей.
    [[nodiscard]] static auto ca_to_voxel_range(glm::ivec3 ca_pos) noexcept
        -> std::pair<glm::ivec3, glm::ivec3> {
        glm::ivec3 min = ca_pos * static_cast<int>(VOXELS_PER_CELL);
        glm::ivec3 max = min + static_cast<int>(VOXELS_PER_CELL) - 1;
        return {min, max};
    }

    /// Создаёт CA ячейку из воксельных данных.
    [[nodiscard]] static auto voxels_to_cell(
        std::span<VoxelData const> voxels,
        glm::uvec3 voxel_extent
    ) noexcept -> CellState;

    /// Применяет CA состояние к вокселям.
    static auto cell_to_voxels(
        CellState const& cell,
        std::span<VoxelData> voxels,
        glm::uvec3 voxel_extent
    ) noexcept -> void;
};

} // namespace projectv::simulation
```

---

## Статус

| Компонент               | Статус         | Приоритет |
|-------------------------|----------------|-----------|
| CellState (GPU-aligned) | Специфицирован | P0        |
| CellularAutomataGrid    | Специфицирован | P0        |
| CASimulationParams      | Специфицирован | P0        |
| Liquid Shader           | Специфицирован | P0        |
| Granular Shader         | Специфицирован | P1        |
| Fire Shader             | Специфицирован | P1        |
| Jolt Integration        | Специфицирован | P1        |
| Voxel Mapping           | Специфицирован | P2        |

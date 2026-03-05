# Cellular Automata ↔ Physics Bridge Specification

**Статус:** Technical Specification
**Уровень:** 🔴 Продвинутый
**Дата:** 2026-02-22
**Версия:** 1.0

---

## Обзор

Документ описывает архитектуру **моста между клеточными автоматами (CA) и физической симуляцией** для реализации:

- **Жидкости** — вода, лава, нефть с физическим поведением
- **Газы** — дым, токсичные газы с диффузией
- **Гравий/песок** — сыпучие материалы с физикой
- **Разрушения** — динамическое взаимодействие CA и rigid bodies

---

## 1. Математическая модель

### 1.1 Определение CA-Physics Bridge

Пусть $\mathcal{C}$ — клеточный автомат с состоянием $S(x, y, z, t)$, где $(x, y, z)$ — координаты ячейки, $t$ — время.

Мост $\mathcal{B}$ определяет отображение:

$$\mathcal{B}: S(x, y, z, t) \leftrightarrow \mathcal{P}$$

где $\mathcal{P}$ — множество физических тел.

### 1.2 Условия конвертации

**CA → Physics:**
$$\text{toPhysics}(S) = \begin{cases}
\text{RigidBody} & \text{if } S.\text{density} > \rho_{\text{threshold}} \\
\text{Particle} & \text{if } S.\text{mass} < m_{\text{threshold}} \\
\text{None} & \text{otherwise}
\end{cases}$$

**Physics → CA:**
$$\text{toCA}(\text{Body}) = \begin{cases}
S(x, y, z) & \text{if } \text{Body} \cap \text{Cell}(x, y, z) \neq \emptyset \\
\text{unchanged} & \text{otherwise}
\end{cases}$$

### 1.3 Материальные свойства

Для материала $m$ определяются:

| Свойство              | Обозначение | Единицы  |
|-----------------------|-------------|----------|
| Плотность             | $\rho_m$    | kg/m³    |
| Вязкость              | $\nu_m$     | Pa·s     |
| Теплоёмкость          | $c_m$       | J/(kg·K) |
| Температура плавления | $T_{melt}$  | K        |
| Коэффициент трения    | $\mu_m$     | —        |

---

## 2. CA State Representation

### 2.1 Cell Data Structure

```cpp
// ProjectV.Voxel.CAState.cppm
export module ProjectV.Voxel.CAState;

import std;
import glm;

export namespace projectv::voxel {

/// Тип материала CA.
export enum class CellType : uint8_t {
    Empty = 0,
    Solid,        ///< Неподвижный блок
    Powder,       ///< Сыпучий (песок, гравий)
    Liquid,       ///< Жидкость (вода, лава)
    Gas,          ///< Газ (дым, пар)
    Plasma,       ///< Плазма (огонь, лава)
    Organic,      ///< Органика (трава, листья)
    RigidBody,    ///< Конвертирован в физику
    Count
};

/// Флаги ячейки.
export enum class CellFlags : uint8_t {
    None = 0,
    Updated = 1 << 0,      ///< Обновлена в этом кадре
    Sleeping = 1 << 1,     ///< "Спит" — не обновляется
    Hot = 1 << 2,          ///< Горячая
    Burning = 1 << 3,      ///< Горит
    Wet = 1 << 4,          ///< Мокрая
    Conducts = 1 << 5,     ///< Проводит электричество
};

/// Состояние ячейки CA (8 bytes).
export struct alignas(8) CellState {
    CellType type{CellType::Empty};
    uint8_t material_id{0};      ///< ID материала
    uint8_t temperature{0};      ///< 0-255 → 0-1000K
    uint8_t pressure{0};         ///< Давление (для газов)
    uint16_t velocity_x{0};      ///< Скорость X (fixed-point)
    uint16_t velocity_y{0};      ///< Скорость Y
    uint16_t velocity_z{0};      ///< Скорость Z
    uint8_t flags{0};            ///< CellFlags
    uint8_t lifetime{0};         ///< Время жизни (для газов)
};

static_assert(sizeof(CellState) == 12, "CellState must be 12 bytes");

/// Свойства материала.
export struct MaterialProperties {
    uint32_t material_id{0};
    std::string name;
    CellType default_type{CellType::Solid};

    float density{1000.0f};           ///< kg/m³
    float viscosity{0.0f};            ///< Pa·s (для жидкостей)
    float friction{0.5f};             ///< Коэффициент трения
    float restitution{0.3f};          ///< Упругость

    float melting_point{1000.0f};     ///< K
    float boiling_point{2000.0f};     ///< K
    float thermal_conductivity{1.0f}; ///< W/(m·K)

    uint8_t color_r{128};
    uint8_t color_g{128};
    uint8_t color_b{128};
    uint8_t color_a{255};

    /// Проверяет, является ли материал жидким при температуре.
    [[nodiscard]] auto is_liquid_at(float temperature) const noexcept -> bool;

    /// Проверяет, является ли материал газообразным.
    [[nodiscard]] auto is_gas_at(float temperature) const noexcept -> bool;
};

} // namespace projectv::voxel
```

### 2.2 CA Grid Structure

```cpp
// ProjectV.Voxel.CAGrid.cppm
export module ProjectV.Voxel.CAGrid;

import std;
import glm;
import ProjectV.Voxel.CAState;

export namespace projectv::voxel {

/// 3D Grid для клеточного автомата.
export class CAGrid {
public:
    /// Создаёт grid заданного размера.
    [[nodiscard]] static auto create(
        glm::ivec3 dimensions,
        float cell_size = 0.25f
    ) noexcept -> std::expected<CAGrid, CAError>;

    ~CAGrid() noexcept = default;

    CAGrid(CAGrid&&) noexcept = default;
    CAGrid& operator=(CAGrid&&) noexcept = default;
    CAGrid(const CAGrid&) = delete;
    CAGrid& operator=(const CAGrid&) = delete;

    /// Получает ячейку по координатам.
    [[nodiscard]] auto get(glm::ivec3 const& pos) const noexcept
        -> CellState const&;

    /// Устанавливает ячейку.
    auto set(glm::ivec3 const& pos, CellState const& state) noexcept -> void;

    /// Получает диапазон ячеек для итерации.
    [[nodiscard]] auto cells() const noexcept
        -> std::span<CellState const>;

    /// Получает размер grid.
    [[nodiscard]] auto dimensions() const noexcept -> glm::ivec3;

    /// Получает размер ячейки в метрах.
    [[nodiscard]] auto cell_size() const noexcept -> float;

    /// Конвертирует мировые координаты в координаты grid.
    [[nodiscard]] auto world_to_grid(glm::vec3 const& world_pos) const noexcept
        -> glm::ivec3;

    /// Конвертирует координаты grid в мировые.
    [[nodiscard]] auto grid_to_world(glm::ivec3 const& grid_pos) const noexcept
        -> glm::vec3;

private:
    explicit CAGrid(glm::ivec3 dims, float cell_size) noexcept;

    glm::ivec3 dimensions_;
    float cell_size_;
    std::vector<CellState> cells_;
};

} // namespace projectv::voxel
```

---

## 3. CA Rules Engine

### 3.1 Правила переходов

```cpp
// ProjectV.Voxel.CARules.cppm
export module ProjectV.Voxel.CARules;

import std;
import glm;
import ProjectV.Voxel.CAState;

export namespace projectv::voxel {

/// Контекст для выполнения правил.
export struct CARuleContext {
    CAGrid const& grid;
    glm::ivec3 position;
    CellState const& current;
    CellState& next;
    MaterialProperties const& material;
    float delta_time;
};

/// Результат применения правила.
export enum class RuleResult : uint8_t {
    NoChange,       ///< Ячейка не изменилась
    Updated,        ///< Ячейка обновлена
    Moved,          ///< Ячейка переместилась
    Converted,      ///< Тип изменился (плавление, испарение)
    ToPhysics,      ///< Требует конвертации в физику
};

/// Интерфейс правила CA.
export struct ICARule {
    virtual ~ICARule() = default;

    /// Проверяет применимость правила.
    [[nodiscard]] virtual auto is_applicable(CARuleContext const& ctx) const noexcept
        -> bool = 0;

    /// Применяет правило.
    virtual auto apply(CARuleContext& ctx) noexcept -> RuleResult = 0;

    /// Приоритет правила (ниже = раньше).
    [[nodiscard]] virtual auto priority() const noexcept -> int = 0;
};

/// Правило для жидкостей.
export struct LiquidRule : ICARule {
    [[nodiscard]] auto is_applicable(CARuleContext const& ctx) const noexcept
        -> bool override {
        return ctx.current.type == CellType::Liquid;
    }

    auto apply(CARuleContext& ctx) noexcept -> RuleResult override;
    [[nodiscard]] auto priority() const noexcept -> int override { return 10; }
};

/// Правило для порошков.
export struct PowderRule : ICARule {
    [[nodiscard]] auto is_applicable(CARuleContext const& ctx) const noexcept
        -> bool override {
        return ctx.current.type == CellType::Powder;
    }

    auto apply(CARuleContext& ctx) noexcept -> RuleResult override;
    [[nodiscard]] auto priority() const noexcept -> int override { return 20; }
};

/// Правило для газов.
export struct GasRule : ICARule {
    [[nodiscard]] auto is_applicable(CARuleContext const& ctx) const noexcept
        -> bool override {
        return ctx.current.type == CellType::Gas;
    }

    auto apply(CARuleContext& ctx) noexcept -> RuleResult override;
    [[nodiscard]] auto priority() const noexcept -> int override { return 30; }
};

/// Правило теплопередачи.
export struct ThermalRule : ICARule {
    [[nodiscard]] auto is_applicable(CARuleContext const& ctx) const noexcept
        -> bool override {
        return ctx.current.temperature > 0;
    }

    auto apply(CARuleContext& ctx) noexcept -> RuleResult override;
    [[nodiscard]] auto priority() const noexcept -> int override { return 5; }
};

} // namespace projectv::voxel
```

### 3.2 CA Simulator

```cpp
// ProjectV.Voxel.CASimulator.cppm
export module ProjectV.Voxel.CASimulator;

import std;
import glm;
import ProjectV.Voxel.CAGrid;
import ProjectV.Voxel.CARules;

export namespace projectv::voxel {

/// Результат симуляции кадра.
export struct CASimulationResult {
    uint32_t cells_updated{0};
    uint32_t cells_moved{0};
    uint32_t cells_converted{0};
    uint32_t cells_to_physics{0};
    float simulation_time_ms{0.0f};
};

/// Симулятор клеточного автомата.
export class CASimulator {
public:
    /// Создаёт симулятор.
    [[nodiscard]] static auto create(
        CAGrid& grid,
        uint32_t max_threads = 4
    ) noexcept -> std::expected<CASimulator, CAError>;

    ~CASimulator() noexcept = default;

    CASimulator(CASimulator&&) noexcept = default;
    CASimulator& operator=(CASimulator&&) noexcept = default;
    CASimulator(const CASimulator&) = delete;
    CASimulator& operator=(const CASimulator&) = delete;

    /// Регистрирует правило.
    auto register_rule(std::unique_ptr<ICARule> rule) noexcept -> void;

    /// Выполняет один шаг симуляции.
    auto step(float delta_time) noexcept -> CASimulationResult;

    /// Устанавливает область симуляции.
    auto set_simulation_region(glm::ivec3 const& min, glm::ivec3 const& max) noexcept -> void;

    /// Включает/выключает многопоточность.
    auto set_multithreaded(bool enabled) noexcept -> void;

private:
    explicit CASimulator(CAGrid* grid, uint32_t max_threads) noexcept;

    CAGrid* grid_{nullptr};
    std::vector<std::unique_ptr<ICARule>> rules_;
    bool multithreaded_{true};
    uint32_t max_threads_{4};
    glm::ivec3 region_min_{0};
    glm::ivec3 region_max_{INT_MAX};
};

} // namespace projectv::voxel
```

---

## 4. CA-Physics Bridge

### 4.1 Bridge Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        CA-Physics Bridge Architecture                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────┐                      ┌─────────────────┐            │
│  │   CA Grid       │                      │  Jolt Physics   │            │
│  │                 │                      │                 │            │
│  │  CellState[]    │                      │  RigidBodies    │            │
│  │  Materials[]    │                      │  Particles      │            │
│  │                 │                      │                 │            │
│  └────────┬────────┘                      └────────┬────────┘            │
│           │                                        │                      │
│           │         ┌───────────────────┐          │                      │
│           │         │   CA-Physics      │          │                      │
│           └────────►│   Bridge          │◄─────────┘                      │
│                     │                   │                                 │
│                     │  - toPhysics()    │                                 │
│                     │  - toCA()         │                                 │
│                     │  - sync()         │                                 │
│                     │                   │                                 │
│                     └───────────────────┘                                 │
│                              │                                           │
│                              ▼                                           │
│                     ┌───────────────────┐                                 │
│                     │   Conversion      │                                 │
│                     │   Queue           │                                 │
│                     │                   │                                 │
│                     │  Pending:         │                                 │
│                     │  - CA→Physics     │                                 │
│                     │  - Physics→CA     │                                 │
│                     └───────────────────┘                                 │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Bridge Implementation

```cpp
// ProjectV.Physics.CABridge.cppm
export module ProjectV.Physics.CABridge;

import std;
import glm;
import Jolt;
import ProjectV.Voxel.CAGrid;
import ProjectV.Voxel.CAState;

export namespace projectv::physics {

/// Событие конвертации.
export struct ConversionEvent {
    enum class Direction : uint8_t {
        CAToPhysics,
        PhysicsToCA
    };

    Direction direction;
    glm::ivec3 ca_position{0};
    JPH::BodyID body_id;
    uint32_t material_id{0};
    float mass{0.0f};
    glm::vec3 velocity{0.0f};
};

/// Конфигурация моста.
export struct CABridgeConfig {
    float density_threshold{500.0f};    ///< Порог плотности для CA→Physics
    float mass_threshold{10.0f};        ///< Порог массы для Physics→CA
    float velocity_threshold{0.1f};     ///< Порог скорости для Physics→CA
    uint32_t max_conversions_per_frame{64};
    bool enable_debris{true};           ///< Создавать debris при разрушении
    float debris_min_size{0.1f};        ///< Минимальный размер debris
};

/// Мост между CA и физикой.
///
/// ## Thread Safety
/// - CA operations on CA thread
/// - Physics operations on physics thread
/// - Bridge sync on main thread
///
/// ## Invariants
/// - Mass is conserved during conversions
/// - Momentum is preserved during transitions
export class CAPhysicsBridge {
public:
    /// Создаёт мост.
    [[nodiscard]] static auto create(
        voxel::CAGrid& grid,
        JPH::PhysicsSystem& physics,
        CABridgeConfig const& config = {}
    ) noexcept -> std::expected<CAPhysicsBridge, BridgeError>;

    ~CAPhysicsBridge() noexcept = default;

    CAPhysicsBridge(CAPhysicsBridge&&) noexcept = default;
    CAPhysicsBridge& operator=(CAPhysicsBridge&&) noexcept = default;
    CAPhysicsBridge(const CAPhysicsBridge&) = delete;
    CAPhysicsBridge& operator=(const CAPhysicsBridge&) = delete;

    /// Синхронизирует CA и Physics.
    /// Вызывается после CA step и Physics step.
    auto sync(float delta_time) noexcept -> std::vector<ConversionEvent>;

    /// Конвертирует область CA в физические тела.
    auto ca_to_physics(
        glm::ivec3 const& min,
        glm::ivec3 const& max
    ) noexcept -> std::vector<ConversionEvent>;

    /// Конвертирует физическое тело в CA.
    auto physics_to_ca(JPH::BodyID body_id) noexcept
        -> std::expected<ConversionEvent, BridgeError>;

    /// Обрабатывает столкновение тела с CA grid.
    auto handle_collision(
        JPH::BodyID body_id,
        glm::vec3 const& contact_point,
        glm::vec3 const& contact_normal,
        float impulse
    ) noexcept -> void;

    /// Создаёт debris при разрушении.
    auto create_debris(
        glm::vec3 const& position,
        glm::vec3 const& velocity,
        uint32_t material_id,
        float total_mass
    ) noexcept -> std::vector<JPH::BodyID>;

    /// Получает конфигурацию.
    [[nodiscard]] auto config() const noexcept -> CABridgeConfig const&;

    /// Устанавливает конфигурацию.
    auto set_config(CABridgeConfig const& config) noexcept -> void;

private:
    explicit CAPhysicsBridge(
        voxel::CAGrid* grid,
        JPH::PhysicsSystem* physics,
        CABridgeConfig const& config
    ) noexcept;

    voxel::CAGrid* grid_{nullptr};
    JPH::PhysicsSystem* physics_{nullptr};
    CABridgeConfig config_;

    std::vector<ConversionEvent> pending_conversions_;
    std::unordered_map<JPH::BodyID, glm::ivec3> body_to_ca_pos_;
};

} // namespace projectv::physics
```

### 4.3 Conversion Logic

```cpp
// ProjectV.Physics.CABridge.cpp
module ProjectV.Physics.CABridge;

import std;
import glm;
import Jolt;
import ProjectV.Voxel.CAGrid;
import ProjectV.Voxel.CAState;
import ProjectV.Voxel.CAMaterials;

namespace projectv::physics {

auto CAPhysicsBridge::sync(float delta_time) noexcept
    -> std::vector<ConversionEvent> {

    std::vector<ConversionEvent> conversions;
    conversions.reserve(config_.max_conversions_per_frame);

    // Process CA→Physics conversions
    auto const& cells = grid_->cells();
    auto dims = grid_->dimensions();

    for (size_t i = 0; i < cells.size() && conversions.size() < config_.max_conversions_per_frame; ++i) {
        auto const& cell = cells[i];

        // Check if cell should be converted
        if (cell.type == voxel::CellType::Empty ||
            cell.type == voxel::CellType::RigidBody) {
            continue;
        }

        // Get material properties
        auto const& material = voxel::get_material(cell.material_id);

        // Check density threshold
        if (material.density >= config_.density_threshold) {
            // Check velocity - if moving fast, convert to physics
            float vx = fixed_to_float(cell.velocity_x);
            float vy = fixed_to_float(cell.velocity_y);
            float vz = fixed_to_float(cell.velocity_z);
            float speed = std::sqrt(vx*vx + vy*vy + vz*vz);

            if (speed > config_.velocity_threshold) {
                // Convert to physics body
                glm::ivec3 pos{
                    static_cast<int>(i % dims.x),
                    static_cast<int>((i / dims.x) % dims.y),
                    static_cast<int>(i / (dims.x * dims.y))
                };

                auto event = convert_cell_to_body(pos, cell, material);
                if (event.has_value()) {
                    conversions.push_back(*event);
                }
            }
        }
    }

    // Process Physics→CA conversions
    auto& body_interface = physics_->GetBodyInterface();

    for (auto& [body_id, ca_pos] : body_to_ca_pos_) {
        if (!body_interface.IsActive(body_id)) continue;

        JPH::RVec3 pos = body_interface.GetPosition(body_id);
        JPH::RVec3 vel = body_interface.GetLinearVelocity(body_id);

        float speed = std::sqrt(
            vel.GetX() * vel.GetX() +
            vel.GetY() * vel.GetY() +
            vel.GetZ() * vel.GetZ()
        );

        // If body is slow enough, convert back to CA
        if (speed < config_.velocity_threshold * 0.5f) {
            auto event = convert_body_to_cell(body_id, ca_pos);
            if (event.has_value()) {
                conversions.push_back(*event);
            }
        }
    }

    return conversions;
}

auto CAPhysicsBridge::convert_cell_to_body(
    glm::ivec3 const& pos,
    voxel::CellState const& cell,
    voxel::MaterialProperties const& material
) noexcept -> std::expected<ConversionEvent, BridgeError> {

    // Calculate mass
    float volume = grid_->cell_size() * grid_->cell_size() * grid_->cell_size();
    float mass = material.density * volume;

    // Create sphere shape for simplicity
    float radius = grid_->cell_size() * 0.5f;
    JPH::SphereShapeSettings shape_settings(radius);
    auto shape_result = shape_settings.Create();
    if (shape_result.HasError()) {
        return std::unexpected(BridgeError::ShapeCreationFailed);
    }

    // Create body
    glm::vec3 world_pos = grid_->grid_to_world(pos);

    JPH::BodyCreationSettings body_settings(
        shape_result.Get(),
        JPH::RVec3(world_pos.x, world_pos.y, world_pos.z),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic,
        JPH::ObjectLayer::Moving
    );

    body_settings.mMassPropertiesOverride.mMass = mass;
    body_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateMassAndInertia;

    auto& body_interface = physics_->GetBodyInterface();
    JPH::Body* body = body_interface.CreateBody(body_settings);
    if (!body) {
        return std::unexpected(BridgeError::BodyCreationFailed);
    }

    // Set velocity from CA
    float vx = fixed_to_float(cell.velocity_x);
    float vy = fixed_to_float(cell.velocity_y);
    float vz = fixed_to_float(cell.velocity_z);
    body->SetLinearVelocity(JPH::RVec3(vx, vy, vz));

    body_interface.AddBody(body->GetID());

    // Mark CA cell as converted
    grid_->set(pos, voxel::CellState{.type = voxel::CellType::RigidBody});

    // Track mapping
    body_to_ca_pos_[body->GetID()] = pos;

    return ConversionEvent{
        .direction = ConversionEvent::Direction::CAToPhysics,
        .ca_position = pos,
        .body_id = body->GetID(),
        .material_id = cell.material_id,
        .mass = mass,
        .velocity = glm::vec3(vx, vy, vz)
    };
}

} // namespace projectv::physics
```

---

## 5. GPU-Accelerated CA

### 5.1 Compute Shader Interface

```slang
// ProjectV/Voxel/CA/ComputeCA.slang
module ProjectV.Voxel.CA.ComputeCA;

import ProjectV.Voxel.CAState;

/// CA Grid buffer.
RWStructuredBuffer<CellState> ca_grid: register(u0);

/// Configuration.
cbuffer CAConfig : register(b0) {
    uint3 grid_dimensions;
    float cell_size;
    float delta_time;
    uint max_iterations;
};

/// Получение индекса ячейки.
uint get_cell_index(uint3 pos) {
    return pos.x + pos.y * grid_dimensions.x + pos.z * grid_dimensions.x * grid_dimensions.y;
}

/// Проверка границ.
bool is_in_bounds(uint3 pos) {
    return pos.x < grid_dimensions.x &&
           pos.y < grid_dimensions.y &&
           pos.z < grid_dimensions.z;
}

/// Получение соседа.
CellState get_neighbor(uint3 pos, int3 offset) {
    int3 neighbor_pos = int3(pos) + offset;

    if (neighbor_pos.x < 0 || neighbor_pos.x >= int(grid_dimensions.x) ||
        neighbor_pos.y < 0 || neighbor_pos.y >= int(grid_dimensions.y) ||
        neighbor_pos.z < 0 || neighbor_pos.z >= int(grid_dimensions.z)) {
        return CellState{.type = CellType_Solid};
    }

    return ca_grid[get_cell_index(uint3(neighbor_pos))];
}

/// Правило для жидкостей.
[numthreads(8, 8, 8)]
void csLiquidRule(uint3 tid: SV_DispatchThreadID) {
    if (!is_in_bounds(tid)) return;

    uint idx = get_cell_index(tid);
    CellState cell = ca_grid[idx];

    if (cell.type != CellType_Liquid) return;

    // Проверка падения вниз
    if (tid.y > 0) {
        CellState below = get_neighbor(tid, int3(0, -1, 0));
        if (below.type == CellType_Empty) {
            // Падение
            uint below_idx = get_cell_index(tid - uint3(0, 1, 0));
            ca_grid[below_idx] = cell;
            ca_grid[idx] = CellState{.type = CellType_Empty};
            return;
        }
    }

    // Проверка растекания
    // ... (horizontal flow logic)
}

/// Правило для порошков.
[numthreads(8, 8, 8)]
void csPowderRule(uint3 tid: SV_DispatchThreadID) {
    if (!is_in_bounds(tid)) return;

    uint idx = get_cell_index(tid);
    CellState cell = ca_grid[idx];

    if (cell.type != CellType_Powder) return;

    // Проверка падения вниз
    if (tid.y > 0) {
        CellState below = get_neighbor(tid, int3(0, -1, 0));
        if (below.type == CellType_Empty) {
            uint below_idx = get_cell_index(tid - uint3(0, 1, 0));
            ca_grid[below_idx] = cell;
            ca_grid[idx] = CellState{.type = CellType_Empty};
            return;
        }

        // Скольжение по диагонали
        uint dir = tid.x % 2; // Randomize direction
        int3 offsets[2] = {int3(-1, -1, 0), int3(1, -1, 0)};

        for (uint i = 0; i < 2; ++i) {
            int3 offset = offsets[(dir + i) % 2];
            CellState diagonal = get_neighbor(tid, offset);
            if (diagonal.type == CellType_Empty) {
                uint diag_idx = get_cell_index(uint3(int3(tid) + offset));
                ca_grid[diag_idx] = cell;
                ca_grid[idx] = CellState{.type = CellType_Empty};
                return;
            }
        }
    }
}

/// Теплопередача.
[numthreads(8, 8, 8)]
void csThermalRule(uint3 tid: SV_DispatchThreadID) {
    if (!is_in_bounds(tid)) return;

    uint idx = get_cell_index(tid);
    CellState cell = ca_grid[idx];

    if (cell.temperature == 0) return;

    // Усреднение температуры с соседями
    float total_temp = float(cell.temperature);
    uint count = 1;

    for (int dz = -1; dz <= 1; ++dz) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0 && dz == 0) continue;

                CellState neighbor = get_neighbor(tid, int3(dx, dy, dz));
                if (neighbor.type != CellType_Empty && neighbor.temperature > 0) {
                    total_temp += float(neighbor.temperature);
                    count++;
                }
            }
        }
    }

    // Применение теплопроводности
    float avg_temp = total_temp / float(count);
    cell.temperature = uint8_t(lerp(float(cell.temperature), avg_temp, 0.1 * delta_time));

    ca_grid[idx] = cell;
}
```

### 5.2 GPU CA Manager

```cpp
// ProjectV.Voxel.CAGPU.cppm
export module ProjectV.Voxel.CAGPU;

import std;
import glm;
import vulkan;
import ProjectV.Voxel.CAState;

export namespace projectv::voxel {

/// GPU-accelerated CA simulator.
export class GPUCASimulator {
public:
    /// Создаёт GPU симулятор.
    [[nodiscard]] static auto create(
        VkDevice device,
        VmaAllocator allocator,
        glm::ivec3 dimensions,
        uint32_t max_iterations = 8
    ) noexcept -> std::expected<GPUCASimulator, CAError>;

    ~GPUCASimulator() noexcept;

    GPUCASimulator(GPUCASimulator&&) noexcept;
    GPUCASimulator& operator=(GPUCASimulator&&) noexcept;
    GPUCASimulator(const GPUCASimulator&) = delete;
    GPUCASimulator& operator=(const GPUCASimulator&) = delete;

    /// Загружает данные на GPU.
    auto upload(CAGrid const& grid) noexcept -> void;

    /// Выполняет симуляцию на GPU.
    auto simulate(VkCommandBuffer cmd, float delta_time) noexcept -> void;

    /// Выгружает данные с GPU.
    auto download(CAGrid& grid) noexcept -> void;

    /// Получает storage buffer для indirect access.
    [[nodiscard]] auto storage_buffer() const noexcept -> VkBuffer;

private:
    GPUCASimulator() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::voxel
```

---

## 6. ECS Integration

### 6.1 ECS Components

```cpp
// ProjectV.ECS.CAComponents.cppm
export module ProjectV.ECS.CAComponents;

import std;
import glm;
import ProjectV.Voxel.CAState;

export namespace projectv::ecs {

/// Компонент CA области.
export struct CARegionComponent {
    glm::ivec3 min{0};
    glm::ivec3 max{16};
    uint32_t grid_id{0};
    bool is_active{true};
    bool gpu_accelerated{false};
};

/// Компонент CA эмиттера.
export struct CAEmitterComponent {
    glm::ivec3 position{0};
    uint32_t material_id{0};
    float emit_rate{10.0f};         ///< Ячеек в секунду
    float emit_velocity{1.0f};
    glm::vec3 emit_direction{0, -1, 0};
    bool is_active{true};
};

/// Компонент CA сенсора.
export struct CASensorComponent {
    glm::ivec3 min{0};
    glm::ivec3 max{0};
    uint32_t target_material{0};
    float detection_threshold{0.5f};
    bool is_triggered{false};
    uint32_t trigger_count{0};
};

} // namespace projectv::ecs
```

### 6.2 CA System

```cpp
// ProjectV.ECS.CASystem.cppm
export module ProjectV.ECS.CASystem;

import std;
import glm;
import flecs;
import ProjectV.Voxel.CAGrid;
import ProjectV.Voxel.CASimulator;
import ProjectV.Physics.CABridge;
import ProjectV.ECS.CAComponents;

export namespace projectv::ecs {

/// Система клеточных автоматов.
export class CASystem {
public:
    /// Регистрирует систему в world.
    static auto register_system(
        flecs::world& world,
        physics::CAPhysicsBridge& bridge
    ) noexcept -> flecs::system;

    /// Обновление симуляции.
    static auto update(
        flecs::iter& it,
        CARegionComponent* regions,
        CAEmitterComponent* emitters
    ) noexcept -> void;

private:
    static physics::CAPhysicsBridge* bridge_;
    static std::unordered_map<uint32_t, std::unique_ptr<voxel::CAGrid>> grids_;
    static std::unordered_map<uint32_t, std::unique_ptr<voxel::CASimulator>> simulators_;
};

} // namespace projectv::ecs
```

---

## 7. Performance Analysis

### 7.1 Сложность

| Операция              | CPU      | GPU            |
|-----------------------|----------|----------------|
| Single cell update    | $O(1)$   | $O(1)$         |
| Full grid update      | $O(n^3)$ | $O(n^3 / 512)$ |
| CA→Physics conversion | $O(k)$   | $O(k)$         |
| Physics→CA conversion | $O(1)$   | $O(1)$         |

где $n$ — размер grid, $k$ — количество конвертируемых ячеек.

### 7.2 Memory

$$M_{\text{CA}} = n^3 \cdot 12 \text{ bytes}$$

$$M_{\text{materials}} = N_m \cdot 64 \text{ bytes}$$

Для grid 256³: $M_{\text{CA}} \approx 200 \text{ MB}$

### 7.3 Benchmarks (Expected)

| Grid Size | CPU (ms) | GPU (ms) | Speedup |
|-----------|----------|----------|---------|
| 64³       | 2.5      | 0.1      | 25x     |
| 128³      | 20       | 0.5      | 40x     |
| 256³      | 160      | 4        | 40x     |

---

## Статус

| Компонент       | Статус         | Приоритет |
|-----------------|----------------|-----------|
| CellState       | Специфицирован | P0        |
| CAGrid          | Специфицирован | P0        |
| CARules         | Специфицирован | P0        |
| CASimulator     | Специфицирован | P0        |
| CAPhysicsBridge | Специфицирован | P0        |
| GPUCASimulator  | Специфицирован | P1        |
| CASystem (ECS)  | Специфицирован | P1        |

---

## Ссылки

- [Voxel Pipeline](./03_voxel-pipeline.md)
- [Jolt-Vulkan Bridge](./06_jolt-vulkan-bridge.md)
- [Voxel Data Philosophy](../philosophy/07_voxel-data-philosophy.md)

# Неевклидова геометрия (SCP Mechanics) Specification

**Статус:** Technical Specification
**Уровень:** 🔴 Продвинутый
**Дата:** 2026-02-22
**Версия:** 1.0

---

## Обзор

Документ описывает архитектуру **неевклидовой геометрии** для реализации SCP-механик:

- **Бесконечные пространства** в ограниченном объёме
- **Порталы** без загрузочных экранов
- **Неправильная геометрия** — комнаты больше снаружи, чем внутри
- **Бесконечные коридоры** — замкнутые.loop-пространства

---

## 1. Математическая модель

### 1.1 Определение неевклидова пространства

Пусть $\mathcal{W}$ — мировое пространство с евклидовой метрикой $d_E$.

Неевклидово пространство $\mathcal{N}$ определяется через функцию перехода $T$:

$$T: \mathcal{W} \times \mathcal{P} \to \mathcal{W}$$

где $\mathcal{P}$ — множество порталов.

Для точки $\vec{p} \in \mathcal{W}$ и портала $\pi \in \mathcal{P}$:

$$T(\vec{p}, \pi) = M_\pi \cdot \vec{p} + \vec{t}_\pi$$

где $M_\pi$ — матрица трансформации портала, $\vec{t}_\pi$ — вектор смещения.

### 1.2 Топология порталов

Портал $\pi$ определяется как пара:

$$\pi = (S_{in}, S_{out})$$

где $S_{in}, S_{out} \subset \mathbb{R}^3$ — входная и выходная поверхности.

Условие связности:

$$\forall \vec{p} \in S_{in}: T(\vec{p}, \pi) \in S_{out}$$

### 1.3 Рекурсивные порталы

Для портала, смотрящего в себя:

$$\vec{p}_n = T(\vec{p}_{n-1}, \pi), \quad n = 1, 2, \ldots, N_{max}$$

Остановка рекурсии при:

$$|\vec{p}_n - \vec{p}_{n-1}| < \epsilon \quad \text{или} \quad n \geq N_{max}$$

### 1.4 Метрика неевклидова пространства

Расстояние между точками $\vec{a}, \vec{b}$ через портал $\pi$:

$$d_N(\vec{a}, \vec{b}) = \min \left( d_E(\vec{a}, \vec{b}), \min_{\pi} \left( d_E(\vec{a}, S_{in}^\pi) + d_E(T(\vec{a}, \pi), \vec{b}) \right) \right)$$

---

## 2. Render-Side: Portal Rendering

### 2.1 Stencil Buffer Architecture

```
Stencil Buffer Layout (8-bit)
┌─────────────────────────────────────────────────────────────┐
│  Bits 0-3: Portal recursion depth (0-15)                    │
│  Bit 4: Inside portal flag                                   │
│  Bit 5: Portal visibility flag                               │
│  Bits 6-7: Reserved                                          │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Portal Rendering Pipeline

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          Portal Rendering Pass                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Pass 1: Render Portal Surfaces to Stencil                              │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │  glStencilFunc(GL_ALWAYS, depth+1, 0xFF)                        │    │
│  │  glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE)                      │    │
│  │  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE)            │    │
│  │  glDepthMask(GL_FALSE)                                          │    │
│  │  draw_portal_surface(portal)                                    │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                    │                                     │
│                                    ▼                                     │
│  Pass 2: Render Scene Through Portal                                    │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │  glStencilFunc(GL_EQUAL, depth+1, 0xFF)                         │    │
│  │  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP)                         │    │
│  │  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE)                │    │
│  │  glDepthMask(GL_TRUE)                                           │    │
│  │  view_matrix = portal.transform * view_matrix                   │    │
│  │  render_scene(recursion_depth + 1)                              │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                    │                                     │
│                                    ▼                                     │
│  Pass 3: Clear Stencil & Render Portal Frame                            │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │  glStencilFunc(GL_ALWAYS, 0, 0xFF)                              │    │
│  │  render_portal_frame(portal)                                    │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.3 Slang Shader Interface

```slang
// ProjectV/Render/Portal/PortalRendering.slang
module ProjectV.Render.Portal.PortalRendering;

import ProjectV.Render.Core.ViewData;

/// Portal data structure.
struct PortalData {
    float4x4 transform;         ///< Transform from entry to exit
    float4x4 inverse_transform; ///< Inverse transform
    float3 position;            ///< Portal center position
    float3 normal;              ///< Portal surface normal
    float2 half_size;           ///< Portal half-extents
    uint target_portal_id;      ///< Connected portal ID
    uint flags;                 ///< Portal flags
};

/// Portal view transformation.
float4x4 compute_portal_view(
    float4x4 view_matrix,
    PortalData portal
) {
    // Transform view matrix through portal
    return mul(view_matrix, portal.transform);
}

/// Check if point is inside portal frustum.
bool is_inside_portal_frustum(
    float3 world_pos,
    PortalData portal,
    float4x4 view_proj
) {
    // Transform to portal local space
    float3 local_pos = world_pos - portal.position;

    // Check if in front of portal plane
    float dist = dot(local_pos, portal.normal);
    if (dist < 0) return false;

    // Check if within portal bounds
    float3 right = normalize(cross(portal.normal, float3(0, 1, 0)));
    float3 up = cross(right, portal.normal);

    float x = dot(local_pos, right);
    float y = dot(local_pos, up);

    return abs(x) <= portal.half_size.x && abs(y) <= portal.half_size.y;
}

/// Portal surface vertex shader.
float4 vsPortalSurface(
    float3 position: POSITION,
    uint portal_id: INSTANCE_ID
) : SV_Position {
    PortalData portal = portal_buffer[portal_id];

    // Transform vertex to world space
    float3 right = normalize(cross(portal.normal, float3(0, 1, 0)));
    float3 up = cross(right, portal.normal);

    float3 world_pos = portal.position
                     + position.x * right * portal.half_size.x
                     + position.y * up * portal.half_size.y;

    return mul(view_proj, float4(world_pos, 1.0));
}

/// Portal clipping in fragment shader.
void clipPortalSurface(
    float3 world_pos,
    PortalData portal
) {
    // Clip fragments outside portal bounds
    float3 local_pos = world_pos - portal.position;

    float3 right = normalize(cross(portal.normal, float3(0, 1, 0)));
    float3 up = cross(right, portal.normal);

    float x = dot(local_pos, right) / portal.half_size.x;
    float y = dot(local_pos, up) / portal.half_size.y;

    clip(1.0 - abs(x));
    clip(1.0 - abs(y));
}
```

### 2.4 C++26 API Contract

```cpp
// ProjectV.Render.Portal.Renderer.cppm
export module ProjectV.Render.Portal.Renderer;

import std;
import glm;
import ProjectV.Render.Vulkan.Context;

export namespace projectv::render::portal {

/// Данные портала для GPU.
export struct alignas(16) PortalDataGPU {
    glm::mat4 transform{1.0f};           ///< Transform от входа к выходу
    glm::mat4 inverse_transform{1.0f};   ///< Обратная трансформация
    glm::vec3 position{0.0f};            ///< Центр портала
    float _pad0{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};   ///< Нормаль поверхности
    float _pad1{0.0f};
    glm::vec2 half_size{1.0f, 1.0f};     ///< Половинные размеры
    uint32_t target_portal_id{0};        ///< ID связанного портала
    uint32_t flags{0};                   ///< Флаги
};

static_assert(sizeof(PortalDataGPU) == 176, "PortalDataGPU must be 176 bytes");

/// Флаги портала.
export enum class PortalFlags : uint32_t {
    None = 0,
    Visible = 1 << 0,        ///< Портал видим
    Active = 1 << 1,         ///< Портал активен
    Recursive = 1 << 2,      ///< Рекурсивный портал (смотрит в себя)
    Bidirectional = 1 << 3,  ///< Двунаправленный
    Mirror = 1 << 4,         ///< Зеркало (специальный случай)
};

/// Рендерер порталов.
export class PortalRenderer {
public:
    /// Создаёт рендерер порталов.
    [[nodiscard]] static auto create(
        VulkanContext const& ctx,
        uint32_t max_portals = 16,
        uint32_t max_recursion_depth = 4
    ) noexcept -> std::expected<PortalRenderer, PortalError>;

    ~PortalRenderer() noexcept;

    // Move-only
    PortalRenderer(PortalRenderer&&) noexcept;
    PortalRenderer& operator=(PortalRenderer&&) noexcept;
    PortalRenderer(const PortalRenderer&) = delete;
    PortalRenderer& operator=(const PortalRenderer&) = delete;

    /// Регистрирует портал.
    auto register_portal(PortalDataGPU const& data) noexcept
        -> std::expected<uint32_t, PortalError>;

    /// Обновляет данные портала.
    auto update_portal(uint32_t portal_id, PortalDataGPU const& data) noexcept -> void;

    /// Удаляет портал.
    auto unregister_portal(uint32_t portal_id) noexcept -> void;

    /// Рендерит сцену через порталы.
    auto render(
        VkCommandBuffer cmd,
        glm::mat4 const& view_matrix,
        glm::mat4 const& proj_matrix,
        std::function<void(glm::mat4 const&, uint32_t)> render_scene_callback
    ) noexcept -> void;

    /// Устанавливает максимальную глубину рекурсии.
    auto set_max_recursion_depth(uint32_t depth) noexcept -> void;

private:
    PortalRenderer() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::render::portal
```

---

## 3. Physics-Side: Jolt Integration

### 3.1 Трансформация тел через портал

```cpp
// ProjectV.Physics.PortalBridge.cppm
export module ProjectV.Physics.PortalBridge;

import std;
import glm;
import Jolt;
import ProjectV.Render.Portal.Renderer;

export namespace projectv::physics {

/// Событие перехода через портал.
export struct PortalTransitionEvent {
    JPH::BodyID body_id;           ///< ID тела
    uint32_t source_portal_id;     ///< ID исходного портала
    uint32_t target_portal_id;     ///< ID целевого портала
    glm::vec3 entry_position;      ///< Точка входа
    glm::vec3 exit_position;       ///< Точка выхода
    glm::quat rotation_delta;      ///< Изменение ориентации
    float velocity_scale{1.0f};    ///< Масштаб скорости
};

/// Мост между порталами и физикой.
///
/// ## Thread Safety
/// - All methods must be called from physics thread
///
/// ## Invariants
/// - Teleportation preserves velocity direction relative to portal
export class PortalPhysicsBridge {
public:
    /// Создаёт мост.
    [[nodiscard]] static auto create(
        JPH::PhysicsSystem& physics,
        render::portal::PortalRenderer& portals
    ) noexcept -> std::expected<PortalPhysicsBridge, PortalError>;

    ~PortalPhysicsBridge() noexcept = default;

    // Move-only
    PortalPhysicsBridge(PortalPhysicsBridge&&) noexcept = default;
    PortalPhysicsBridge& operator=(PortalPhysicsBridge&&) noexcept = default;
    PortalPhysicsBridge(const PortalPhysicsBridge&) = delete;
    PortalPhysicsBridge& operator=(const PortalPhysicsBridge&) = delete;

    /// Проверяет и обрабатывает пересечения тел с порталами.
    /// Вызывается после physics.step().
    auto process_portal_transitions(float delta_time) noexcept
        -> std::vector<PortalTransitionEvent>;

    /// Регистрирует тело для отслеживания порталов.
    auto register_body(JPH::BodyID body_id) noexcept -> void;

    /// Удаляет тело из отслеживания.
    auto unregister_body(JPH::BodyID body_id) noexcept -> void;

    /// Телепортирует тело через портал.
    auto teleport_body(
        JPH::BodyID body_id,
        render::portal::PortalDataGPU const& source_portal,
        render::portal::PortalDataGPU const& target_portal
    ) noexcept -> PortalTransitionEvent;

private:
    explicit PortalPhysicsBridge(
        JPH::PhysicsSystem* physics,
        render::portal::PortalRenderer* portals
    ) noexcept;

    JPH::PhysicsSystem* physics_{nullptr};
    render::portal::PortalRenderer* portals_{nullptr};

    struct TrackedBody {
        JPH::BodyID body_id;
        glm::vec3 last_position;
        uint32_t last_portal_id{UINT32_MAX};
        float cooldown{0.0f};
    };

    std::vector<TrackedBody> tracked_bodies_;
};

} // namespace projectv::physics
```

### 3.2 Реализация телепортации

```cpp
// ProjectV.Physics.PortalBridge.cpp
module ProjectV.Physics.PortalBridge;

import std;
import glm;
import Jolt;

namespace projectv::physics {

auto PortalPhysicsBridge::process_portal_transitions(float delta_time) noexcept
    -> std::vector<PortalTransitionEvent> {

    std::vector<PortalTransitionEvent> events;

    for (auto& tracked : tracked_bodies_) {
        // Update cooldown
        tracked.cooldown = std::max(0.0f, tracked.cooldown - delta_time);

        if (tracked.cooldown > 0.0f) continue;

        // Get body
        auto& body_interface = physics_->GetBodyInterface();
        if (!body_interface.IsActive(tracked.body_id)) continue;

        JPH::Body* body = body_interface.GetBody(tracked.body_id);
        if (!body) continue;

        // Get current position
        JPH::RVec3 pos = body->GetPosition();
        glm::vec3 current_pos = glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());

        // Check intersection with each portal
        // ... (portal intersection logic)

        tracked.last_position = current_pos;
    }

    return events;
}

auto PortalPhysicsBridge::teleport_body(
    JPH::BodyID body_id,
    render::portal::PortalDataGPU const& source_portal,
    render::portal::PortalDataGPU const& target_portal
) noexcept -> PortalTransitionEvent {

    auto& body_interface = physics_->GetBodyInterface();

    // Get current state
    JPH::RVec3 pos = body_interface.GetPosition(body_id);
    JPH::Quat rot = body_interface.GetRotation(body_id);
    JPH::RVec3 vel = body_interface.GetLinearVelocity(body_id);
    JPH::Vec3 ang_vel = body_interface.GetAngularVelocity(body_id);

    // Convert to glm
    glm::vec3 position(pos.GetX(), pos.GetY(), pos.GetZ());
    glm::quat rotation(rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW());
    glm::vec3 velocity(vel.GetX(), vel.GetY(), vel.GetZ());

    // Calculate transform through portal
    glm::mat4 portal_transform = target_portal.transform * source_portal.inverse_transform;

    // Transform position
    glm::vec4 new_pos_h = portal_transform * glm::vec4(position, 1.0f);
    glm::vec3 new_position = glm::vec3(new_pos_h);

    // Transform rotation
    glm::mat3 rot_matrix = glm::mat3(portal_transform);
    glm::quat rotation_delta = glm::quat_cast(rot_matrix);
    glm::quat new_rotation = rotation_delta * rotation;

    // Transform velocity
    glm::vec3 new_velocity = rot_matrix * velocity;

    // Apply new state
    body_interface.SetPositionAndRotation(
        body_id,
        JPH::RVec3(new_position.x, new_position.y, new_position.z),
        JPH::Quat(new_rotation.x, new_rotation.y, new_rotation.z, new_rotation.w),
        JPH::EActivation::Activate
    );

    body_interface.SetLinearVelocity(
        body_id,
        JPH::RVec3(new_velocity.x, new_velocity.y, new_velocity.z)
    );

    return PortalTransitionEvent{
        .body_id = body_id,
        .source_portal_id = 0, // Set from caller
        .target_portal_id = 0, // Set from caller
        .entry_position = position,
        .exit_position = new_position,
        .rotation_delta = rotation_delta
    };
}

} // namespace projectv::physics
```

### 3.3 Ghost Bodies на границах порталов

```cpp
// ProjectV.Physics.GhostBodies.cppm
export module ProjectV.Physics.GhostBodies;

import std;
import glm;
import Jolt;

export namespace projectv::physics {

/// Ghost Body — фантомное тело для порталов.
///
/// ## Purpose
/// Создаёт "призрачную" копию тела на другой стороне портала
/// для корректной обработки коллизий при переходе.
export class GhostBodyManager {
public:
    /// Создаёт manager.
    [[nodiscard]] static auto create(
        JPH::PhysicsSystem& physics
    ) noexcept -> std::expected<GhostBodyManager, PortalError>;

    /// Создаёт ghost body для портала.
    /// @param source_body Исходное тело
    /// @param portal_transform Трансформация портала
    /// @return ID ghost body
    auto create_ghost(
        JPH::BodyID source_body,
        glm::mat4 const& portal_transform
    ) noexcept -> std::expected<JPH::BodyID, PortalError>;

    /// Обновляет позицию ghost body.
    auto update_ghost(
        JPH::BodyID ghost_id,
        JPH::BodyID source_body,
        glm::mat4 const& portal_transform
    ) noexcept -> void;

    /// Удаляет ghost body.
    auto destroy_ghost(JPH::BodyID ghost_id) noexcept -> void;

    /// Синхронизирует все ghost bodies.
    auto sync_all_ghosts() noexcept -> void;

private:
    JPH::PhysicsSystem* physics_{nullptr};

    struct GhostPair {
        JPH::BodyID source_body;
        JPH::BodyID ghost_body;
        uint32_t portal_id;
    };

    std::vector<GhostPair> ghost_pairs_;
};

} // namespace projectv::physics
```

---

## 4. ECS-Side: NonEuclideanTransform

### 4.1 Компоненты ECS

```cpp
// ProjectV.ECS.NonEuclidean.cppm
export module ProjectV.ECS.NonEuclidean;

import std;
import glm;
import flecs;

export namespace projectv::ecs {

/// Компонент портала.
export struct PortalComponent {
    uint32_t portal_id{0};           ///< ID в PortalRenderer
    uint32_t target_portal_id{0};    ///< ID связанного портала
    glm::vec3 position{0.0f};        ///< Позиция в мире
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec2 size{2.0f, 3.0f};      ///< Размер портала
    bool is_bidirectional{false};
    bool is_active{true};
};

/// Компонент неевклидовой трансформации.
export struct NonEuclideanTransform {
    glm::vec3 base_position{0.0f};       ///< Позиция в базовом пространстве
    glm::quat base_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    uint32_t current_space_id{0};        ///< ID текущего пространства
    uint32_t last_portal_id{UINT32_MAX}; ///< Последний пройденный портал
    float portal_cooldown{0.0f};         ///< Задержка перед повторным проходом
};

/// Компонент неевклидова пространства.
export struct NonEuclideanSpace {
    uint32_t space_id{0};
    glm::vec3 world_offset{0.0f};        ///< Смещение относительно мира
    glm::quat world_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 bounds_min{0.0f};
    glm::vec3 bounds_max{100.0f};
    bool is_infinite{false};             ///< Бесконечное пространство (loop)
};

} // namespace projectv::ecs
```

### 4.2 Система порталов

```cpp
// ProjectV.ECS.PortalSystem.cppm
export module ProjectV.ECS.PortalSystem;

import std;
import glm;
import flecs;
import ProjectV.ECS.NonEuclidean;
import ProjectV.Render.Portal.Renderer;
import ProjectV.Physics.PortalBridge;

export namespace projectv::ecs {

/// Система обработки порталов.
export class PortalSystem {
public:
    /// Регистрирует систему в ECS world.
    static auto register_system(
        flecs::world& world,
        render::portal::PortalRenderer& renderer,
        physics::PortalPhysicsBridge& bridge
    ) noexcept -> flecs::system;

    /// Обновляет порталы.
    static auto update(
        flecs::iter& it,
        PortalComponent* portals,
        NonEuclideanTransform* transforms
    ) noexcept -> void;

    /// Проверяет переход сущности через портал.
    static auto check_portal_transition(
        flecs::entity e,
        PortalComponent const& portal,
        NonEuclideanTransform& transform
    ) noexcept -> bool;

private:
    static render::portal::PortalRenderer* renderer_;
    static physics::PortalPhysicsBridge* bridge_;
};

} // namespace projectv::ecs
```

### 4.3 Flecs System Registration

```cpp
// ProjectV.ECS.PortalSystem.cpp
module ProjectV.ECS.PortalSystem;

import std;
import glm;
import flecs;
import ProjectV.ECS.NonEuclidean;

namespace projectv::ecs {

render::portal::PortalRenderer* PortalSystem::renderer_{nullptr};
physics::PortalPhysicsBridge* PortalSystem::bridge_{nullptr};

auto PortalSystem::register_system(
    flecs::world& world,
    render::portal::PortalRenderer& renderer,
    physics::PortalPhysicsBridge& bridge
) noexcept -> flecs::system {

    renderer_ = &renderer;
    bridge_ = &bridge;

    return world.system<PortalComponent, NonEuclideanTransform>("PortalSystem")
        .kind(flecs::OnUpdate)
        .iter(update);
}

auto PortalSystem::update(
    flecs::iter& it,
    PortalComponent* portals,
    NonEuclideanTransform* transforms
) noexcept -> void {

    float delta_time = it.delta_time();

    for (auto i : it) {
        auto& portal = portals[i];
        auto& transform = transforms[i];

        // Update cooldown
        transform.portal_cooldown = std::max(0.0f,
            transform.portal_cooldown - delta_time);

        // Update portal in renderer
        render::portal::PortalDataGPU data{
            .position = portal.position,
            .normal = portal.rotation * glm::vec3(0, 0, 1),
            .half_size = portal.size * 0.5f,
            .target_portal_id = portal.target_portal_id,
            .flags = portal.is_active ?
                static_cast<uint32_t>(render::portal::PortalFlags::Active) : 0
        };

        renderer_->update_portal(portal.portal_id, data);
    }

    // Process physics transitions
    auto events = bridge_->process_portal_transitions(delta_time);

    // Apply transitions to ECS
    for (auto const& event : events) {
        // Find entity by body ID and update transform
        // ...
    }
}

} // namespace projectv::ecs
```

---

## 5. Graph Scene — NonEuclidean Graph

### 5.1 Структура графа

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        Non-Euclidean Scene Graph                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────┐                      ┌─────────────────┐            │
│  │   Space A       │                      │   Space B       │            │
│  │   (Main World)  │                      │   (SCP-XXXX)    │            │
│  │                 │                      │                 │            │
│  │  ┌───────┐      │      Portal 1        │  ┌───────┐      │            │
│  │  │Entity │──────┼──────────────────────┼──│Entity'│      │            │
│  │  └───────┘      │                      │  └───────┘      │            │
│  │                 │                      │                 │            │
│  │  ┌───────┐      │      Portal 2        │  ┌───────┐      │            │
│  │  │Portal │──────┼──────────────────────┼──│Portal │      │            │
│  │  └───────┘      │                      │  └───────┘      │            │
│  │                 │                      │                 │            │
│  └─────────────────┘                      └─────────────────┘            │
│         │                                       │                        │
│         │ Portal 3 (Recursive)                  │                        │
│         ▼                                       │                        │
│  ┌─────────────────┐                            │                        │
│  │   Space A'      │                            │                        │
│  │   (Recursive)   │                            │                        │
│  │                 │                            │                        │
│  │  Infinite Loop  │                            │                        │
│  └─────────────────┘                            │                        │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 5.2 API для Graph Management

```cpp
// ProjectV.Scene.NonEuclideanGraph.cppm
export module ProjectV.Scene.NonEuclideanGraph;

import std;
import glm;
import ProjectV.ECS.NonEuclidean;

export namespace projectv::scene {

/// Узел графа неевклидова пространства.
export struct SpaceNode {
    uint32_t space_id;
    std::string name;
    glm::mat4 world_transform{1.0f};
    std::vector<uint32_t> portal_ids;      ///< Порталы в этом пространстве
    std::vector<uint32_t> connected_spaces; ///< Связанные пространства
};

/// Ребро графа — портал между пространствами.
export struct PortalEdge {
    uint32_t portal_id;
    uint32_t source_space_id;
    uint32_t target_space_id;
    glm::mat4 transform{1.0f};             ///< Трансформация между пространствами
};

/// Граф неевклидова сцены.
export class NonEuclideanGraph {
public:
    /// Добавляет пространство.
    auto add_space(ecs::NonEuclideanSpace const& space) noexcept
        -> uint32_t;

    /// Удаляет пространство.
    auto remove_space(uint32_t space_id) noexcept -> void;

    /// Добавляет портал между пространствами.
    auto add_portal(
        uint32_t source_space_id,
        uint32_t target_space_id,
        glm::mat4 const& transform
    ) noexcept -> uint32_t;

    /// Получает путь между пространствами.
    auto find_path(uint32_t from_space, uint32_t to_space) const noexcept
        -> std::expected<std::vector<uint32_t>, GraphError>;

    /// Вычисляет эффективное расстояние через порталы.
    auto compute_distance(
        glm::vec3 from_pos,
        uint32_t from_space,
        glm::vec3 to_pos,
        uint32_t to_space
    ) const noexcept -> float;

    /// Получает узел по ID.
    [[nodiscard]] auto get_space(uint32_t space_id) const noexcept
        -> SpaceNode const*;

    /// Получает все пространства.
    [[nodiscard]] auto spaces() const noexcept
        -> std::unordered_map<uint32_t, SpaceNode> const&;

private:
    std::unordered_map<uint32_t, SpaceNode> spaces_;
    std::unordered_map<uint32_t, PortalEdge> portals_;
    uint32_t next_space_id_{1};
    uint32_t next_portal_id_{1};
};

} // namespace projectv::scene
```

---

## 6. Performance Considerations

### 6.1 Ограничения рекурсии

| Глубина | Draw Calls | Stencil Buffer | Рекомендация        |
|---------|------------|----------------|---------------------|
| 1       | 2          | 1 bit          | Минимум             |
| 2       | 4          | 2 bits         | Стандарт            |
| 4       | 8          | 3 bits         | Максимум для mobile |
| 8       | 16         | 4 bits         | High-end GPU        |

### 6.2 Memory Footprint

$$M_{\text{portals}} = N_{\text{portals}} \cdot (176 \text{ bytes} + M_{\text{ghost\_bodies}})$$

$$M_{\text{ghost\_bodies}} = N_{\text{tracked}} \cdot 2 \cdot M_{\text{body}}$$

### 6.3 Время рендеринга

$$T_{\text{portal\_render}} = T_{\text{base}} \cdot (1 + N_{\text{visible\_portals}} \cdot D_{\text{recursion}})$$

где $D_{\text{recursion}}$ — средняя глубина рекурсии.

---

## Статус

| Компонент             | Статус         | Приоритет |
|-----------------------|----------------|-----------|
| PortalRenderer        | Специфицирован | P0        |
| PortalPhysicsBridge   | Специфицирован | P0        |
| GhostBodyManager      | Специфицирован | P1        |
| NonEuclideanTransform | Специфицирован | P0        |
| PortalSystem (ECS)    | Специфицирован | P0        |
| NonEuclideanGraph     | Специфицирован | P1        |

---

## Ссылки

- [Jolt-Vulkan Bridge](./06_jolt-vulkan-bridge.md)
- [Render Graph Specification](./28_render_graph_spec.md)
- [ECS Architecture](../adr/0003-ecs-architecture.md)

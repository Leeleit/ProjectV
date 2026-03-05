# ADR-0003: ECS Architecture (Flecs Integration)

---

## Контекст

ProjectV требует архитектуры для:

- Игровой логики с тысячами сущностей (NPC, предметы, частицы)
- Синхронизации с физическим движком JoltPhysics
- Репликации состояния для будущего мультиплеера
- Модульности и расширяемости

## Решение

Принята архитектура **Entity-Component-System (ECS)** на базе библиотеки **Flecs** с DOD-ориентированными компонентами.

---

## 1. Entity & Component Model

### 1.1 Entity ID

```cpp
// Спецификация интерфейса
export module ProjectV.ECS.Entity;

import std;
import flecs;

export namespace projectv::ecs {

/// Обёртка над flecs::entity с типобезопасностью.
/// Entity = ID + archetype metadata (managed by Flecs).
class Entity final {
public:
    /// Создаёт null entity.
    explicit Entity() noexcept = default;

    /// Создаёт entity из flecs::entity.
    explicit Entity(flecs::entity entity) noexcept : entity_(entity) {}

    /// Проверяет валидность entity.
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return entity_.is_alive();
    }

    /// Возвращает ID entity.
    [[nodiscard]] auto id() const noexcept -> uint64_t {
        return entity_.id();
    }

    /// Добавляет компонент к entity.
    template<typename T>
    auto add() noexcept -> Entity& {
        entity_.add<T>();
        return *this;
    }

    /// Устанавливает компонент.
    template<typename T>
    auto set(T const& component) noexcept -> Entity& {
        entity_.set<T>(component);
        return *this;
    }

    /// Получает компонент (mutable).
    template<typename T>
    [[nodiscard]] auto get_mut() noexcept -> T* {
        return entity_.get_mut<T>();
    }

    /// Получает компонент (const).
    template<typename T>
    [[nodiscard]] auto get() const noexcept -> T const* {
        return entity_.get<T>();
    }

    /// Удаляет компонент.
    template<typename T>
    auto remove() noexcept -> Entity& {
        entity_.remove<T>();
        return *this;
    }

    /// Удаляет entity.
    auto destroy() noexcept -> void {
        entity_.destruct();
    }

    /// Возвращает нативный flecs::entity.
    [[nodiscard]] auto native() const noexcept -> flecs::entity {
        return entity_;
    }

    auto operator<=>(Entity const&) const = default;

private:
    flecs::entity entity_{};
};

} // namespace projectv::ecs
```

### 1.2 Component Types (Data-Only)

```cpp
// Спецификация интерфейса
export module ProjectV.ECS.Components;

import std;
import glm;
import glaze;

export namespace projectv::ecs {

// ============================================================================
// Transform Components
// ============================================================================

/// Позиция в мире (float3)
struct TransformComponent {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // w, x, y, z
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    /// Вычисляет модельную матрицу.
    [[nodiscard]] auto to_matrix() const noexcept -> glm::mat4 {
        auto t = glm::translate(glm::mat4(1.0f), position);
        auto r = glm::mat4_cast(rotation);
        auto s = glm::scale(glm::mat4(1.0f), scale);
        return t * r * s;
    }

    struct glaze {
        using T = TransformComponent;
        static constexpr auto value = glz::object(
            "position", &T::position,
            "rotation", &T::rotation,
            "scale", &T::scale
        );
    };
};

/// Скорость (для physics integration)
struct VelocityComponent {
    glm::vec3 linear{0.0f, 0.0f, 0.0f};
    glm::vec3 angular{0.0f, 0.0f, 0.0f};

    struct glaze {
        using T = VelocityComponent;
        static constexpr auto value = glz::object(
            "linear", &T::linear,
            "angular", &T::angular
        );
    };
};

// ============================================================================
// Physics Components
// ============================================================================

/// Ссылка на физическое тело в Jolt
struct PhysicsBodyComponent {
    uint64_t body_id{0};        ///< JPH::BodyID
    bool is_dynamic{true};
    bool is_kinematic{false};
    float mass{1.0f};

    struct glaze {
        using T = PhysicsBodyComponent;
        static constexpr auto value = glz::object(
            "body_id", &T::body_id,
            "is_dynamic", &T::is_dynamic,
            "is_kinematic", &T::is_kinematic,
            "mass", &T::mass
        );
    };
};

/// Коллайдер (shape для Jolt)
struct ColliderComponent {
    enum class ShapeType : uint8_t {
        Box,
        Sphere,
        Capsule,
        Cylinder,
        Mesh,
        VoxelChunk
    };

    ShapeType shape_type{ShapeType::Box};
    glm::vec3 half_extents{0.5f, 0.5f, 0.5f};  // Для box
    float radius{0.5f};                         // Для sphere/capsule
    float height{1.0f};                         // Для capsule/cylinder

    struct glaze {
        using T = ColliderComponent;
        static constexpr auto value = glz::object(
            "shape_type", &T::shape_type,
            "half_extents", &T::half_extents,
            "radius", &T::radius,
            "height", &T::height
        );
    };
};

// ============================================================================
// Rendering Components
// ============================================================================

/// Визуальная геометрия
struct MeshComponent {
    uint64_t mesh_id{0};        ///< ID в MeshManager
    uint64_t material_id{0};    ///< ID в MaterialManager
    bool cast_shadows{true};
    bool receive_shadows{true};
    bool visible{true};

    struct glaze {
        using T = MeshComponent;
        static constexpr auto value = glz::object(
            "mesh_id", &T::mesh_id,
            "material_id", &T::material_id,
            "cast_shadows", &T::cast_shadows,
            "receive_shadows", &T::receive_shadows,
            "visible", &T::visible
        );
    };
};

/// Bounds для frustum culling
struct BoundsComponent {
    glm::vec3 min{-0.5f, -0.5f, -0.5f};
    glm::vec3 max{0.5f, 0.5f, 0.5f};
    float radius{0.866f};  // sqrt(3) / 2

    [[nodiscard]] auto contains(glm::vec3 point) const noexcept -> bool {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }
};

/// Видимость для frustum culling (tag + result)
struct VisibleComponent {
    bool in_frustum{true};
    uint32_t lod_level{0};
    float distance_to_camera{0.0f};
};

// ============================================================================
// Voxel Components
// ============================================================================

/// Ссылка на чанк в мире
struct VoxelChunkComponent {
    int32_t chunk_x{0};
    int32_t chunk_y{0};
    int32_t chunk_z{0};
    uint64_t svo_root_id{0};
    bool needs_mesh_rebuild{true};
    bool needs_svo_rebuild{true};

    [[nodiscard]] auto chunk_coord() const noexcept -> glm::ivec3 {
        return {chunk_x, chunk_y, chunk_z};
    }
};

/// Изменение вокселя (для events)
struct VoxelChangeComponent {
    int32_t x{0};
    int32_t y{0};
    int32_t z{0};
    uint16_t old_material{0};
    uint16_t new_material{0};
    uint64_t timestamp{0};
};

// ============================================================================
// Gameplay Components
// ============================================================================

/// Игрок
struct PlayerComponent {
    uint32_t player_id{0};
    float move_speed{5.0f};
    float jump_force{8.0f};
    float mouse_sensitivity{0.1f};
    bool is_grounded{false};
    bool is_sprinting{false};
    bool is_crouching{false};
};

/// AI агент
struct AIComponent {
    uint64_t target_entity{0};
    glm::vec3 target_position{0.0f};
    float aggro_range{10.0f};
    float attack_range{2.0f};
    float attack_cooldown{0.0f};
    float patrol_radius{5.0f};
    uint8_t state{0};  // Idle, Patrol, Chase, Attack
};

/// Здоровье
struct HealthComponent {
    float current{100.0f};
    float max{100.0f};
    float regeneration{0.0f};
    bool invulnerable{false};

    [[nodiscard]] auto is_dead() const noexcept -> bool {
        return current <= 0.0f && !invulnerable;
    }

    auto apply_damage(float damage) noexcept -> void {
        if (!invulnerable) {
            current = std::max(0.0f, current - damage);
        }
    }

    struct glaze {
        using T = HealthComponent;
        static constexpr auto value = glz::object(
            "current", &T::current,
            "max", &T::max,
            "regeneration", &T::regeneration,
            "invulnerable", &T::invulnerable
        );
    };
};

/// Тег для имени (debug purposes)
struct NameComponent {
    std::string name;

    struct glaze {
        using T = NameComponent;
        static constexpr auto value = glz::object("name", &T::name);
    };
};

} // namespace projectv::ecs
```

---

## 2. System Interface

```cpp
// Спецификация интерфейса
export module ProjectV.ECS.System;

import std;
import flecs;
import ProjectV.ECS.Components;

export namespace projectv::ecs {

/// Базовый интерфейс для систем.
/// Системы в Flecs — это функции, применяемые к архетипам.
class SystemBase {
public:
    virtual ~SystemBase() = default;
    virtual auto register_system(flecs::world& world) -> void = 0;
    virtual auto name() const -> std::string_view = 0;
};

/// Регистратор всех систем.
class SystemRegistry final {
public:
    /// Регистрирует все системы в world.
    static auto register_all(flecs::world& world) -> void;

    /// Регистрирует одну систему.
    template<std::derived_from<SystemBase> T>
    static auto register_system(flecs::world& world) -> void {
        T system;
        system.register_system(world);
    }
};

// ============================================================================
// Примеры систем
// ============================================================================

/// Система обновления transform на основе velocity.
class TransformSystem final : public SystemBase {
public:
    auto name() const -> std::string_view override { return "TransformSystem"; }

    auto register_system(flecs::world& world) -> void override {
        world.system<TransformComponent, VelocityComponent>("UpdateTransforms")
            .kind(flecs::OnUpdate)
            .each([](TransformComponent& t, VelocityComponent const& v) {
                t.position += v.linear * 0.016f; // TODO: delta_time
                // TODO: rotation integration
            });
    }
};

/// Система синхронизации физики.
class PhysicsSyncSystem final : public SystemBase {
public:
    auto name() const -> std::string_view override { return "PhysicsSyncSystem"; }

    auto register_system(flecs::world& world) -> void override {
        // Синхронизация Transform ← Physics (после шага физики)
        world.system<TransformComponent, PhysicsBodyComponent>("SyncPhysicsToTransform")
            .kind(flecs::PreStore)  // После физики, до рендеринга
            .iter([](flecs::iter& it, TransformComponent* t, PhysicsBodyComponent const* p) {
                auto* physics = it.world().ctx<PhysicsWorld>();
                if (!physics) return;

                for (auto i : it) {
                    if (p[i].is_dynamic) {
                        auto pos = physics->get_body_position(p[i].body_id);
                        auto rot = physics->get_body_rotation(p[i].body_id);
                        t[i].position = pos;
                        t[i].rotation = rot;
                    }
                }
            });
    }
};

/// Система frustum culling.
class FrustumCullingSystem final : public SystemBase {
public:
    auto name() const -> std::string_view override { return "FrustumCullingSystem"; }

    auto register_system(flecs::world& world) -> void override {
        world.system<TransformComponent, BoundsComponent, VisibleComponent>("FrustumCull")
            .kind(flecs::PreUpdate)
            .iter([](flecs::iter& it, TransformComponent const* t, BoundsComponent const* b, VisibleComponent* v) {
                auto* camera = it.world().ctx<Camera>();
                if (!camera) return;

                auto frustum = camera->get_frustum();

                for (auto i : it) {
                    // Transform bounds to world space
                    auto world_bounds = transform_bounds(b[i], t[i]);

                    // Test frustum intersection
                    v[i].in_frustum = frustum.intersects(world_bounds);
                    v[i].distance_to_camera = glm::distance(t[i].position, camera->position());

                    // Calculate LOD
                    v[i].lod_level = calculate_lod(v[i].distance_to_camera);
                }
            });
    }

private:
    static auto calculate_lod(float distance) -> uint32_t {
        if (distance < 50.0f) return 0;
        if (distance < 100.0f) return 1;
        if (distance < 200.0f) return 2;
        return 3;
    }
};

/// Система здоровья.
class HealthSystem final : public SystemBase {
public:
    auto name() const -> std::string_view override { return "HealthSystem"; }

    auto register_system(flecs::world& world) -> void override {
        // Регенерация здоровья
        world.system<HealthComponent>("HealthRegen")
            .kind(flecs::OnUpdate)
            .each([](HealthComponent& h) {
                if (h.regeneration > 0.0f && h.current < h.max) {
                    h.current = std::min(h.max, h.current + h.regeneration * 0.016f);
                }
            });

        // Обработка смерти
        world.system<HealthComponent>("HandleDeath")
            .kind(flecs::PostUpdate)
            .iter([](flecs::iter& it, HealthComponent* h) {
                for (auto i : it) {
                    if (h[i].is_dead()) {
                        // Emit death event
                        it.world().entity(it.entity(i)).add<DeadTag>();
                    }
                }
            });
    }
};

/// Тег для мёртвых сущностей.
struct DeadTag {};

} // namespace projectv::ecs
```

---

## 3. World & Context

```cpp
// Спецификация интерфейса
export module ProjectV.ECS.World;

import std;
import flecs;
import ProjectV.ECS.Components;
import ProjectV.ECS.System;

export namespace projectv::ecs {

/// Конфигурация ECS мира
struct WorldConfig {
    uint32_t initial_entity_count{1000};
    bool enable_hierarchy{true};      // Parent-child relationships
    bool enable_threads{true};        // Multi-threading
    uint32_t thread_count{0};         // 0 = auto
};

/// ECS World — корневой контейнер для всех сущностей.
class ECSWorld final {
public:
    /// Создаёт ECS мир с конфигурацией.
    [[nodiscard]] static auto create(WorldConfig const& config = {}) noexcept
        -> std::expected<ECSWorld, ECSError>;

    ~ECSWorld() noexcept = default;

    // Move-only
    ECSWorld(ECSWorld&&) noexcept = default;
    ECSWorld& operator=(ECSWorld&&) noexcept = default;
    ECSWorld(const ECSWorld&) = delete;
    ECSWorld& operator=(const ECSWorld&) = delete;

    // --- Entity Creation ---

    /// Создаёт новую entity.
    [[nodiscard]] auto create_entity() noexcept -> Entity;

    /// Создаёт entity с именем.
    [[nodiscard]] auto create_entity(std::string_view name) noexcept -> Entity;

    /// Создаёт entity из prefab.
    [[nodiscard]] auto create_from_prefab(std::string_view prefab_name) noexcept
        -> std::expected<Entity, ECSError>;

    // --- Queries ---

    /// Находит entity по имени.
    [[nodiscard]] auto find_entity(std::string_view name) const noexcept
        -> std::optional<Entity>;

    /// Находит entity по ID.
    [[nodiscard]] auto get_entity(uint64_t id) const noexcept -> Entity;

    // --- Context (Singletons) ---

    /// Устанавливает контекстный объект (singleton).
    template<typename T>
    auto set_context(T* ptr) noexcept -> void {
        world_.set<T*>(ptr);
    }

    /// Получает контекстный объект.
    template<typename T>
    [[nodiscard]] auto get_context() const noexcept -> T* {
        return world_.get<T*>();
    }

    // --- Simulation ---

    /// Выполняет один шаг симуляции.
    /// @param delta_time Время в секундах
    auto progress(float delta_time) noexcept -> void;

    /// Возвращает количество entity.
    [[nodiscard]] auto entity_count() const noexcept -> size_t;

    /// Возвращает нативный flecs::world.
    [[nodiscard]] auto native() noexcept -> flecs::world& { return world_; }
    [[nodiscard]] auto native() const noexcept -> flecs::world const& { return world_; }

private:
    explicit ECSWorld(flecs::world&& world) noexcept;

    flecs::world world_;
};

/// Коды ошибок ECS
export enum class ECSError : uint8_t {
    PrefabNotFound,         ///< Prefab не найден
    InvalidEntity,          ///< Entity не валидна
    ComponentNotFound,      ///< Компонент не найден
    SystemRegistrationFailed ///< Ошибка регистрации системы
};

} // namespace projectv::ecs
```

---

## 4. Event System

```cpp
// Спецификация интерфейса
export module ProjectV.ECS.Events;

import std;
import flecs;
import ProjectV.ECS.Components;

export namespace projectv::ecs {

// ============================================================================
// Event Types
// ============================================================================

/// Событие создания entity
struct EntityCreatedEvent {
    uint64_t entity_id;
    std::string_view name;
};

/// Событие уничтожения entity
struct EntityDestroyedEvent {
    uint64_t entity_id;
};

/// Событие изменения здоровья
struct HealthChangedEvent {
    uint64_t entity_id;
    float old_health;
    float new_health;
    float damage;
};

/// Событие смерти
struct EntityDiedEvent {
    uint64_t entity_id;
    uint64_t killer_id;  // 0 = unknown
};

/// Событие изменения вокселя
struct VoxelChangedEvent {
    int32_t x, y, z;
    uint16_t old_material;
    uint16_t new_material;
    uint64_t entity_id;  // Кто изменил
};

// ============================================================================
// Event Manager
// ============================================================================

/// Менеджер событий.
/// Использует Flecs observers для событийной модели.
class EventManager final {
public:
    /// Регистрирует обработчик события создания entity.
    template<typename Func>
    static auto on_entity_created(flecs::world& world, Func&& callback) -> void {
        world.observer<EntityCreatedEvent>()
            .event(flecs::OnAdd)
            .each(std::forward<Func>(callback));
    }

    /// Регистрирует обработчик изменения компонента.
    template<typename T, typename Func>
    static auto on_component_changed(flecs::world& world, Func&& callback) -> void {
        world.observer<T>()
            .event(flecs::OnSet)
            .each(std::forward<Func>(callback));
    }

    /// Эмитит событие.
    template<typename T>
    static auto emit(flecs::world& world, T const& event) -> void {
        world.event<T>().emit(event);
    }

    /// Эмитит событие для entity.
    template<typename T>
    static auto emit_for(flecs::world& world, uint64_t entity_id, T const& event) -> void {
        world.event<T>().entity(world.entity(entity_id)).emit(event);
    }
};

} // namespace projectv::ecs
```

---

## 5. Prefab System

```cpp
// Спецификация интерфейса
export module ProjectV.ECS.Prefab;

import std;
import flecs;
import ProjectV.ECS.Components;
import glaze;

export namespace projectv::ecs {

/// Определение prefab'а
struct PrefabDefinition {
    std::string name;
    std::vector<std::string> components;  // Component type names
    std::string json_data;                 // Serialized component data

    struct glaze {
        using T = PrefabDefinition;
        static constexpr auto value = glz::object(
            "name", &T::name,
            "components", &T::components,
            "data", &T::json_data
        );
    };
};

/// Менеджер prefab'ов.
class PrefabManager final {
public:
    /// Загружает prefab из JSON.
    [[nodiscard]] auto load_prefab(std::string_view json_path) noexcept
        -> std::expected<void, PrefabError>;

    /// Регистрирует prefab.
    auto register_prefab(PrefabDefinition const& definition) noexcept -> void;

    /// Создаёт entity из prefab.
    [[nodiscard]] auto instantiate(
        flecs::world& world,
        std::string_view prefab_name
    ) const noexcept -> std::expected<flecs::entity, PrefabError>;

    /// Проверяет существование prefab.
    [[nodiscard]] auto has_prefab(std::string_view name) const noexcept -> bool;

    /// Возвращает все имена prefab'ов.
    [[nodiscard]] auto prefab_names() const noexcept -> std::vector<std::string_view>;

private:
    std::unordered_map<std::string, PrefabDefinition> prefabs_;
};

export enum class PrefabError : uint8_t {
    NotFound,           ///< Prefab не найден
    InvalidJSON,        ///< Ошибка парсинга JSON
    ComponentNotFound,  ///< Компонент не зарегистрирован
    InstantiationFailed ///< Ошибка создания entity
};

// ============================================================================
// Builtin Prefabs
// ============================================================================

/// Создаёт prefab игрока.
auto create_player_prefab(flecs::world& world) -> flecs::entity;

/// Создаёт prefab воксельного чанка.
auto create_chunk_prefab(flecs::world& world) -> flecs::entity;

/// Создаёт prefab NPC.
auto create_npc_prefab(flecs::world& world) -> flecs::entity;

} // namespace projectv::ecs
```

---

## 6. Integration with JoltPhysics

```cpp
// Спецификация интерфейса
export module ProjectV.ECS.PhysicsIntegration;

import std;
import flecs;
import Jolt;
import ProjectV.ECS.Components;

export namespace projectv::ecs {

/// Контекст физики для ECS.
class PhysicsWorld {
public:
    /// Инициализирует физический мир.
    [[nodiscard]] static auto create() noexcept
        -> std::expected<PhysicsWorld, PhysicsError>;

    ~PhysicsWorld() noexcept;

    // --- Body Management ---

    /// Создаёт физическое тело из компонента.
    [[nodiscard]] auto create_body(
        TransformComponent const& transform,
        ColliderComponent const& collider,
        PhysicsBodyComponent& body
    ) noexcept -> std::expected<uint64_t, PhysicsError>;

    /// Удаляет физическое тело.
    auto destroy_body(uint64_t body_id) noexcept -> void;

    // --- Simulation ---

    /// Выполняет шаг физики.
    auto step(float delta_time) noexcept -> void;

    // --- Queries ---

    [[nodiscard]] auto get_body_position(uint64_t body_id) const noexcept -> glm::vec3;
    [[nodiscard]] auto get_body_rotation(uint64_t body_id) const noexcept -> glm::quat;
    auto set_body_position(uint64_t body_id, glm::vec3 pos) noexcept -> void;
    auto set_body_velocity(uint64_t body_id, glm::vec3 vel) noexcept -> void;

    // --- Raycasts ---

    struct RaycastHit {
        uint64_t body_id;
        glm::vec3 point;
        glm::vec3 normal;
        float distance;
    };

    [[nodiscard]] auto raycast(
        glm::vec3 origin,
        glm::vec3 direction,
        float max_distance
    ) const noexcept -> std::optional<RaycastHit>;

private:
    PhysicsWorld() noexcept = default;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

export enum class PhysicsError : uint8_t {
    InitializationFailed,
    BodyCreationFailed,
    InvalidBodyID,
    ShapeCreationFailed
};

} // namespace projectv::ecs
```

---

## Статус

| Компонент                             | Статус         | Приоритет |
|---------------------------------------|----------------|-----------|
| Entity                                | Специфицирован | P0        |
| Components (Transform, Physics, Mesh) | Специфицирован | P0        |
| System Interface                      | Специфицирован | P0        |
| ECSWorld                              | Специфицирован | P0        |
| Event System                          | Специфицирован | P1        |
| Prefab System                         | Специфицирован | P1        |
| Physics Integration                   | Специфицирован | P0        |

---

## Последствия

### Положительные:

- DOD-ориентированные компоненты (данные отдельно от логики)
- Автоматическая многопоточность через Flecs
- Событийная модель через observers
- Простая интеграция с JoltPhysics

### Риски:

- Flecs — C-библиотека, требует Global Module Fragment
- Prefab-система требует регистрации типов компонентов

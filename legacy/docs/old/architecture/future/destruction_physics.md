# Destruction Physics Specification

---

## Обзор

Документ описывает систему **разрушаемой физики** для ProjectV. Система позволит:

- Разрушать воксельные структуры в реальном времени
- Создавать реалистичные обломки (debris)
- Симулировать цепные разрушения
- Интегрироваться с JoltPhysics для физики обломков

---

## 1. Архитектура

### 1.1 Принципы

| Принцип               | Описание                                      |
|-----------------------|-----------------------------------------------|
| **Performance-first** | Разрушения не должны просаживать FPS          |
| **Deterministic**     | Одинаковые разрушения при одинаковых условиях |
| **Scalable**          | От мелких блоков до огромных строений         |
| **GPU-accelerated**   | Вычисления на GPU где возможно                |

### 1.2 Компоненты

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Destruction Physics System                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌───────────────┐    ┌───────────────┐    ┌───────────────┐           │
│  │   Voxel       │───▶│   Fracture    │───▶│   Debris      │           │
│  │   Structure   │    │   Analysis    │    │   Generation  │           │
│  │   (SVO)       │    │   (GPU)       │    │   (Jolt)      │           │
│  └───────────────┘    └───────────────┘    └───────────────┘           │
│         │                    │                    │                     │
│         │                    │                    │                     │
│         ▼                    ▼                    ▼                     │
│  ┌───────────────┐    ┌───────────────┐    ┌───────────────┐           │
│  │   Structural  │    │   Crack       │    │   Collision   │           │
│  │   Integrity   │    │   Propagation │    │   Response    │           │
│  │   (CPU)       │    │   (GPU)       │    │   (Jolt)      │           │
│  └───────────────┘    └───────────────┘    └───────────────┘           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Structural Integrity System

### 2.1 Connectivity Graph

```cpp
export module ProjectV.Destruction.StructuralIntegrity;

import std;
import glm;
import ProjectV.Voxel.SVO;

export namespace projectv::destruction {

/// Узел графа связности
struct SupportNode {
    glm::ivec3 position;
    uint16_t materialId;
    float supportValue{0.0f};       ///< Сила поддержки [0-1]
    float stressValue{0.0f};        ///< Нагрузка на блок
    uint8_t connectedNeighbors{0};  /// Количество связанных соседей
    bool isSupported{false};        ///< Имеет путь к "земле"
    bool isStressed{false};         ///< Превышает порог стресса
};

/// Граф структурной целостности
class StructuralIntegrityGraph {
public:
    /// Создаёт граф для области вокселей
    [[nodiscard]] static auto build(
        SVO const& svo,
        glm::ivec3 const& minBound,
        glm::ivec3 const& maxBound,
        float groundLevel
    ) noexcept -> std::expected<StructuralIntegrityGraph, GraphError>;

    /// Обновляет поддержку после удаления блока
    auto update_support(glm::ivec3 const& removedPosition) noexcept -> void;

    /// Находит все неподдерживаемые блоки
    [[nodiscard]] auto find_unsupported() const noexcept
        -> std::vector<glm::ivec3>;

    /// Находит сильно нагруженные блоки
    [[nodiscard]] auto find_overstressed(float threshold) const noexcept
        -> std::vector<glm::ivec3>;

    /// Вычисляет нагрузку на блок
    [[nodiscard]] auto calculate_stress(glm::ivec3 const& pos) const noexcept
        -> float;

    /// Получает информацию об узле
    [[nodiscard]] auto get_node(glm::ivec3 const& pos) const noexcept
        -> SupportNode const*;

private:
    std::unordered_map<glm::ivec3, SupportNode, IVec3Hash> nodes_;
    std::unordered_map<glm::ivec3, std::vector<glm::ivec3>, IVec3Hash> adjacency_;
    glm::ivec3 minBound_;
    glm::ivec3 maxBound_;
    float groundLevel_;

    /// BFS для определения поддержки
    auto propagate_support() noexcept -> void;

    /// Вычисление нагрузки снизу вверх
    auto calculate_load_distribution() noexcept -> void;
};

} // namespace projectv::destruction
```

### 2.2 Material Strength Properties

```cpp
export namespace projectv::destruction {

/// Свойства материала для разрушений
struct MaterialStrengthProps {
    float tensileStrength{1.0f};      ///< Прочность на растяжение
    float compressiveStrength{10.0f}; ///< Прочность на сжатие
    float shearStrength{2.0f};        ///< Прочность на сдвиг
    float density{1.0f};              ///< Плотность (влияет на вес)
    float brittleness{0.5f};          ///< Хрупкость (0=пластичный, 1=хрупкий)
    float fragmentationFactor{1.0f};  ///< Множитель количества осколков

    /// Предельная нагрузка перед разрушением
    [[nodiscard]] auto breaking_point() const noexcept -> float {
        return (tensileStrength + compressiveStrength + shearStrength) / 3.0f;
    }
};

/// Регистр свойств материалов
class MaterialStrengthRegistry {
public:
    /// Регистрирует свойства материала
    auto register_material(uint16_t materialId, MaterialStrengthProps const& props)
        noexcept -> void;

    /// Получает свойства материала
    [[nodiscard]] auto get_props(uint16_t materialId) const noexcept
        -> MaterialStrengthProps const&;

    /// Предустановленные материалы
    static auto initialize_defaults() noexcept -> MaterialStrengthRegistry {
        MaterialStrengthRegistry registry;

        // Камень
        registry.register_material(1, {
            .tensileStrength = 4.0f,
            .compressiveStrength = 50.0f,
            .shearStrength = 10.0f,
            .density = 2.5f,
            .brittleness = 0.7f,
            .fragmentationFactor = 1.0f
        });

        // Дерево
        registry.register_material(2, {
            .tensileStrength = 10.0f,
            .compressiveStrength = 5.0f,
            .shearStrength = 8.0f,
            .density = 0.6f,
            .brittleness = 0.2f,
            .fragmentationFactor = 0.5f
        });

        // Металл
        registry.register_material(3, {
            .tensileStrength = 50.0f,
            .compressiveStrength = 50.0f,
            .shearStrength = 30.0f,
            .density = 7.8f,
            .brittleness = 0.1f,
            .fragmentationFactor = 0.3f
        });

        // Стекло
        registry.register_material(4, {
            .tensileStrength = 0.5f,
            .compressiveStrength = 10.0f,
            .shearStrength = 0.5f,
            .density = 2.5f,
            .brittleness = 0.95f,
            .fragmentationFactor = 3.0f
        });

        return registry;
    }

private:
    std::unordered_map<uint16_t, MaterialStrengthProps> props_;
    static MaterialStrengthProps defaultProps_;
};

} // namespace projectv::destruction
```

---

## 3. Fracture Analysis (GPU)

### 3.1 Compute Shader

```slang
// Destruction/FractureAnalysis.slang
module FractureAnalysis;

// Входные данные
[[vk::binding(0, 0)]]
StructuredBuffer<SupportNode> supportNodes;

[[vk::binding(1, 0)]]
StructuredBuffer<MaterialStrengthProps> materialProps;

// Выходные данные
[[vk::binding(2, 0)]]
RWStructuredBuffer<FractureResult> fractureResults;

// Параметры
struct FractureParams {
    uint3 grid_size;
    float explosion_radius;
    float3 explosion_center;
    float explosion_force;
    float time_delta;
    uint node_count;
};

[[vk::binding(3, 0)]]
uniform FractureParams params;

/// Результат анализа разрушения
struct FractureResult {
    float damage;           ///< Накопленный урон [0-1]
    uint should_break;      ///< Флаг разрушения
    uint crack_direction;   ///< Направление трещины (encoded)
    float debris_count;     ///< Количество обломков
};

/// Анализ разрушения от взрыва
[numthreads(8, 8, 1)]
void csExplosionDamage(uint3 tid: SV_DispatchThreadID) {
    if (tid.x >= params.node_count) return;

    SupportNode node = supportNodes[tid.x];
    MaterialStrengthProps mat = materialProps[node.materialId];

    // Расстояние до центра взрыва
    float3 toCenter = float3(node.position) - params.explosion_center;
    float distance = length(toCenter);

    if (distance > params.explosion_radius) {
        fractureResults[tid.x].damage = 0.0f;
        fractureResults[tid.x].should_break = 0;
        return;
    }

    // Урон убывает с расстоянием
    float falloff = 1.0f - (distance / params.explosion_radius);
    falloff = falloff * falloff;  // Quadratic falloff

    // Базовый урон от взрыва
    float baseDamage = params.explosion_force * falloff;

    // Модификаторы материала
    float materialMod = 1.0f / mat.breaking_point();
    float stressMod = 1.0f + node.stressValue * 0.5f;

    // Итоговый урон
    float totalDamage = baseDamage * materialMod * stressMod;

    // Накопление урона
    fractureResults[tid.x].damage = min(totalDamage, 1.0f);

    // Проверка разрушения
    fractureResults[tid.x].should_break =
        (totalDamage > 1.0f - mat.brittleness * 0.5f) ? 1 : 0;

    // Направление трещины (от центра взрыва)
    if (distance > 0.001f) {
        float3 crackDir = normalize(toCenter);
        // Encode direction into uint (6 bits per component)
        fractureResults[tid.x].crack_direction =
            (uint(crackDir.x * 31.0f + 31.0f) << 0) |
            (uint(crackDir.y * 31.0f + 31.0f) << 6) |
            (uint(crackDir.z * 31.0f + 31.0f) << 12);
    }

    // Количество обломков
    fractureResults[tid.x].debris_count =
        ceil(mat.fragmentationFactor * totalDamage * 5.0f);
}

/// Анализ разрушения от нагрузки
[numthreads(8, 8, 1)]
void csStressDamage(uint3 tid: SV_DispatchThreadID) {
    if (tid.x >= params.node_count) return;

    SupportNode node = supportNodes[tid.x];
    MaterialStrengthProps mat = materialProps[node.materialId];

    // Блоки без поддержки разрушаются
    if (!node.isSupported && node.position.y > 0) {
        fractureResults[tid.x].should_break = 1;
        fractureResults[tid.x].damage = 1.0f;
        fractureResults[tid.x].debris_count = 1.0f;
        return;
    }

    // Проверка перегрузки
    float breakingPoint = mat.breaking_point();
    if (node.stressValue > breakingPoint) {
        fractureResults[tid.x].should_break = 1;
        fractureResults[tid.x].damage = node.stressValue / breakingPoint;
        fractureResults[tid.x].debris_count =
            ceil(mat.fragmentationFactor * (node.stressValue / breakingPoint));
    }
}
```

---

## 4. Debris Generation

### 4.1 Debris System

```cpp
export module ProjectV.Destruction.Debris;

import std;
import glm;
import ProjectV.Physics.Jolt;

export namespace projectv::destruction {

/// Данные обломка
struct DebrisData {
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 velocity;
    glm::vec3 angularVelocity;
    glm::vec3 halfExtents;     ///< Размер обломка
    uint16_t materialId;
    float mass;
    float lifetime;            ///< Время до исчезновения
    bool isActive;
};

/// Генератор обломков
class DebrisGenerator {
public:
    /// Генерирует обломки из разрушенного блока
    [[nodiscard]] auto generate_from_block(
        glm::ivec3 const& blockPos,
        uint16_t materialId,
        glm::vec3 const& impactDirection,
        float impactForce,
        MaterialStrengthProps const& matProps
    ) noexcept -> std::vector<DebrisData>;

    /// Генерирует обломки из области разрушения
    [[nodiscard]] auto generate_from_region(
        std::vector<glm::ivec3> const& destroyedBlocks,
        glm::vec3 const& explosionCenter,
        float explosionForce
    ) noexcept -> std::vector<DebrisData>;

    /// Устанавливает максимальное количество активных обломков
    auto set_max_debris(size_t maxCount) noexcept -> void {
        maxDebrisCount_ = maxCount;
    }

private:
    size_t maxDebrisCount_{1000};
    std::mt19937 rng_{std::random_device{}()};

    /// Генерация случайной формы обломка
    [[nodiscard]] auto random_debris_shape(
        MaterialStrengthProps const& props
    ) noexcept -> glm::vec3;

    /// Вычисление начальной скорости
    [[nodiscard]] auto calculate_initial_velocity(
        glm::vec3 const& debrisPos,
        glm::vec3 const& impactPos,
        glm::vec3 const& impactDir,
        float force
    ) noexcept -> glm::vec3;
};

/// Менеджер обломков
class DebrisManager {
public:
    /// Создаёт менеджер обломков
    [[nodiscard]] static auto create(
        PhysicsSystem& physics,
        size_t maxDebris = 1000
    ) noexcept -> std::expected<DebrisManager, DebrisError>;

    /// Добавляет обломки в мир
    auto spawn_debris(std::vector<DebrisData> const& debris) noexcept -> void;

    /// Обновляет обломки (удаляет старые)
    auto update(float deltaTime) noexcept -> void;

    /// Получает все активные обломки
    [[nodiscard]] auto get_active_debris() const noexcept
        -> std::vector<DebrisData const*>;

    /// Очищает все обломки
    auto clear() noexcept -> void;

private:
    PhysicsSystem* physics_;
    DebrisGenerator generator_;
    std::vector<DebrisData> activeDebris_;
    std::unordered_map<size_t, BodyID> physicsBodies_;  // debris index -> Jolt body
    size_t nextIndex_{0};
};

} // namespace projectv::destruction
```

### 4.2 Debris Pooling

```cpp
export namespace projectv::destruction {

/// Пул обломков для оптимизации
class DebrisPool {
public:
    /// Получает обломок из пула или создаёт новый
    [[nodiscard]] auto acquire() noexcept -> DebrisData* {
        if (!pool_.empty()) {
            DebrisData* debris = pool_.back();
            pool_.pop_back();
            return debris;
        }

        active_.push_back({});
        return &active_.back();
    }

    /// Возвращает обломок в пул
    auto release(DebrisData* debris) noexcept -> void {
        debris->isActive = false;
        debris->lifetime = 0.0f;
        pool_.push_back(debris);
    }

    /// Предварительное выделение
    auto preallocate(size_t count) noexcept -> void {
        active_.reserve(count);
        pool_.reserve(count / 4);
    }

private:
    std::vector<DebrisData> active_;
    std::vector<DebrisData*> pool_;
};

} // namespace projectv::destruction
```

---

## 5. Explosion System

### 5.1 Explosion Definition

```cpp
export module ProjectV.Destruction.Explosion;

import std;
import glm;
import ProjectV.Voxel.SVO;
import ProjectV.Destruction.StructuralIntegrity;

export namespace projectv::destruction {

/// Тип взрыва
export enum class ExplosionType : uint8_t {
    Spherical,      ///< Сферический (TNT)
    Directional,    ///< Направленный (кумулятивный)
    Chain,          ///< Цепной (передаётся по блокам)
    Implosion       ///< Имплозия (втягивает внутрь)
};

/// Параметры взрыва
struct ExplosionParams {
    glm::vec3 center;
    float radius{5.0f};
    float force{1.0f};
    ExplosionType type{ExplosionType::Spherical};
    float chainDelay{0.0f};         ///< Задержка для цепных взрывов
    bool createFire{false};         ///< Создать огонь
    bool destroyGround{false};      ///< Разрушать ли "землю"

    /// Материалы, устойчивые к взрыву
    std::vector<uint16_t> resistantMaterials;
};

/// Система взрывов
class ExplosionSystem {
public:
    /// Создаёт взрыв
    auto create_explosion(
        ExplosionParams const& params,
        SVO& svo,
        StructuralIntegrityGraph& graph
    ) noexcept -> std::vector<glm::ivec3>;

    /// Обновляет цепные взрывы
    auto update(float deltaTime) noexcept -> void;

    /// Устанавливает callback для разрушенных блоков
    auto set_destruction_callback(
        std::function<void(std::vector<glm::ivec3> const&)> callback
    ) noexcept -> void {
        destructionCallback_ = std::move(callback);
    }

private:
    struct ActiveExplosion {
        ExplosionParams params;
        float elapsedTime{0.0f};
        std::vector<glm::ivec3> pendingDestructions;
    };

    std::vector<ActiveExplosion> activeExplosions_;
    std::function<void(std::vector<glm::ivec3> const&)> destructionCallback_;

    /// Вычисляет повреждённые блоки
    [[nodiscard]] auto calculate_affected_blocks(
        ExplosionParams const& params,
        SVO const& svo
    ) const noexcept -> std::vector<glm::ivec3>;

    /// Применяет урон к блокам
    auto apply_damage(
        ExplosionParams const& params,
        std::vector<glm::ivec3> const& affected,
        SVO& svo,
        StructuralIntegrityGraph& graph
    ) noexcept -> std::vector<glm::ivec3>;
};

} // namespace projectv::destruction
```

---

## 6. Integration with Main Loop

### 6.1 Destruction Pipeline

```cpp
// Интеграция в главный цикл
auto process_destruction(
    float deltaTime,
    SVO& svo,
    StructuralIntegrityGraph& graph,
    ExplosionSystem& explosions,
    DebrisManager& debris,
    PhysicsSystem& physics
) -> void {

    // 1. Обновление активных взрывов
    explosions.update(deltaTime);

    // 2. Обновление графа структурной целостности
    // (только для изменённых областей)
    // ...

    // 3. Проверка на разрушения от нагрузки
    auto overstressed = graph.find_overstressed(STRESS_THRESHOLD);
    if (!overstressed.empty()) {
        // Удаляем перенапряжённые блоки
        for (auto const& pos : overstressed) {
            svo.set_voxel(pos, VoxelData{});
        }

        // Генерируем обломки
        auto newDebris = debris.generator_.generate_from_region(
            overstressed,
            glm::vec3{0.0f},
            0.0f
        );
        debris.spawn_debris(newDebris);
    }

    // 4. Обновление физики обломков
    debris.update(deltaTime);

    // 5. Проверка на цепные разрушения
    auto unsupported = graph.find_unsupported();
    if (!unsupported.empty()) {
        // Обрушение неподдерживаемых блоков
        handle_collapse(unsupported, svo, graph, debris);
    }
}
```

---

## 7. Performance Considerations

### 7.1 Optimizations

| Оптимизация          | Описание                        |
|----------------------|---------------------------------|
| **LOD for debris**   | Далёкие обломки — простые формы |
| **Debris pooling**   | Переиспользование объектов      |
| **GPU fracture**     | Вычисления на GPU               |
| **Deferred updates** | Отложенное обновление графа     |

### 7.2 Limits

```cpp
/// Конфигурация разрушений
struct DestructionConfig {
    /// Максимальное количество активных обломков
    size_t maxActiveDebris{1000};

    /// Максимальный радиус взрыва (в блоках)
    float maxExplosionRadius{32.0f};

    /// Частота обновления графа целостности (в кадрах)
    uint32_t graphUpdateFrequency{4};

    /// Порог стресса для разрушения
    float stressThreshold{1.0f};

    /// Время жизни обломков (секунды)
    float debrisLifetime{30.0f};

    /// Максимальное количество одновременных взрывов
    size_t maxActiveExplosions{10};
};
```

---

## Статус

| Компонент                | Статус         | Приоритет |
|--------------------------|----------------|-----------|
| StructuralIntegrityGraph | Специфицирован | P0        |
| MaterialStrengthProps    | Специфицирован | P0        |
| Fracture Analysis (GPU)  | Специфицирован | P1        |
| DebrisGenerator          | Специфицирован | P0        |
| DebrisManager            | Специфицирован | P0        |
| ExplosionSystem          | Специфицирован | P1        |
| Debris Pooling           | Специфицирован | P1        |

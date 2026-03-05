# Спецификация MVP Демо-сцены: "The Voxel Laboratory"

**Документ:** Техническое задание на демо-сцену
**Цель:** Tech Showcase к маю 2026 года для защиты проекта в вузе
**Дата:** 2026-02-22
**Уровень:** 🎓 Академический
**Эстетика:** Frutiger Aero / Liminal Spaces

---

## Аннотация

Документ описывает спецификацию **Tech Showcase** демо-сцены **"The Voxel Laboratory"** в эстетике Frutiger Aero —
яркой, футуристичной, "снотворческой" визуальной стилистике начала 2000-х. Сцена демонстрирует ключевые технические
решения ProjectV: SVO, Vulkan Mesh Shaders, Jolt Physics, Cellular Automata.

---

## 1. Цели и ограничения

### 1.1 Концепция "Tech Showcase"

| Аспект                   | Описание                                                                     |
|--------------------------|------------------------------------------------------------------------------|
| **Визуальный стиль**     | Frutiger Aero: яркие цвета, глянцевые поверхности, стекло, вода, небо        |
| **Атмосфера**            | Liminal Spaces: пустота, сновидческое пространство, "laboratory in the void" |
| **Демонстрация**         | SVO rendering, Physics destruction, Fluid simulation, Performance metrics    |
| **Эмоциональный отклик** | "Wow, это работает в реальном времени?"                                      |

### 1.2 Цели демо-сцены

| Цель                   | Описание                                                       |
|------------------------|----------------------------------------------------------------|
| **Визуализация**       | The Voxel Laboratory — белоснежная сфера в пустом пространстве |
| **Взаимодействие**     | Projectile → Destruction → Fluid flow → Structural collapse    |
| **Производительность** | Доказательство < 16ms frame time при сложных эффектах          |
| **Архитектура**        | Демонстрация SVO + Mesh Shaders + Jolt + CA integration        |

### 1.3 Ограничения MVP

| Аспект      | В MVP                               | НЕ в MVP                  |
|-------------|-------------------------------------|---------------------------|
| Размер мира | 32×16×32 чанков (одна сцена)        | Бесконечный мир           |
| Генерация   | Процедурная лаборатория             | Природный ландшафт, биомы |
| Материалы   | 3 типа (стекло, жидкость, пол)      | 100+ материалов           |
| Освещение   | Directional + GTAO + Volumetric Fog | PBR, shadows, ray tracing |
| Физика      | Projectile, debris, fluid, collapse | Полная destruction        |
| UI          | Tracy overlay + crosshair           | Игровое UI                |

### 1.4 Платформа

| Параметр     | Значение                                   |
|--------------|--------------------------------------------|
| ОС           | Windows 11                                 |
| Graphics API | Vulkan 1.4                                 |
| Min GPU      | NVIDIA RTX 3060 / AMD RX 6600              |
| RAM          | 16 GB                                      |
| CPU          | 8+ cores (Intel 12th gen / AMD Ryzen 5000) |

---

## 2. The Voxel Laboratory: Процедурная генерация

### 2.1 Описание сцены

```
The Voxel Laboratory
─────────────────────────────────────────────────────────────

     ┌─────────────────────────────────────────────────────┐
     │                    SKY (Void)                        │
     │                    ☀ Sun Light                       │
     │                     ◯                                │
     │                   ╱     ╲                            │
     │                 ╱    ◯    ╲      ← Sphere (R=64)    │
     │               ╱   fluid    ╲     Material: Glass    │
     │              │   (blue)    │    Center: (0,100,0)   │
     │               ╲           ╱                          │
     │                 ╲_______╱                            │
     │                                                      │
     │                                                      │
     ├──────────────────────────────────────────────────────┤
     │     □ ■ □ ■ □ ■ □ ■ □ ■ □ ■ □ ■ □ ■ □ ■ □ ■ □ ■     │
     │     ■ □ ■ □ ■ □ ■ □ ■ ■ □ ■ □ ■ □ ■ □ ■ □ ■ □ ■     │
     │              CHECKERBOARD FLOOR                      │
     │            512×512 voxels (white/light gray)        │
     │            Y=0 (ground level)                        │
     └─────────────────────────────────────────────────────┘

Scene Dimensions:
─────────────────
  Floor:    512 × 1 × 512 voxels (checkerboard)
  Sphere:   Radius 64 voxels, hollow shell
  Fluid:    Inside sphere, fill level ~70%
  Void:     Empty space above and around

Total Voxels: ~1.5M (optimized via SVO)
Active Chunks: ~64 (smaller world for showcase)
Memory Estimate: ~256 MB (SVO + Mesh + Physics)
```

### 2.2 Материалы

| ID | Название    | Цвет (RGB)      | Свойства                         |
|----|-------------|-----------------|----------------------------------|
| 0  | Air         | —               | Прозрачный                       |
| 1  | Glass/White | (240, 245, 255) | Глянцевый, отражающий, breakable |
| 2  | Fluid       | (0, 150, 255)   | Прозрачность 0.7, CA-активный    |
| 3  | Floor White | (255, 255, 255) | Матовый                          |
| 4  | Floor Gray  | (200, 205, 210) | Матовый                          |

### 2.3 Функция генерации сцены (C++26)

```cpp
// ProjectV.Demo.VoxelLab.cppm
export module ProjectV.Demo.VoxelLab;

import std;
import glm;
import ProjectV.Voxel.World;
import ProjectV.Voxel.SVO;

export namespace projectv::demo {

/// Материалы для The Voxel Laboratory.
export enum class LabMaterial : uint8_t {
    Air = 0,
    Glass = 1,       ///< Внешняя оболочка сферы
    Fluid = 2,       ///< Жидкость внутри сферы
    FloorWhite = 3,  ///< Белые клетки пола
    FloorGray = 4,   ///< Серые клетки пола
};

/// Конфигурация Voxel Laboratory.
export struct LabConfig {
    uint32_t floor_size{512};         ///< Размер пола (воксели)
    uint32_t sphere_radius{64};       ///< Радиус сферы
    glm::ivec3 sphere_center{0, 100, 0}; ///< Центр сферы
    uint32_t shell_thickness{2};      ///< Толщина оболочки
    float fluid_fill_level{0.7f};     ///< Уровень жидкости (0-1)
    int32_t floor_y{0};               ///< Высота пола
};

/// Генерирует сцену "The Voxel Laboratory".
///
/// ## Scene Layout
/// 1. Checkerboard floor: 512×512 voxels
/// 2. Hollow sphere: radius 64, glass material
/// 3. Fluid inside sphere: blue voxels, fill 70%
///
/// ## Algorithm
/// - Floor: O(n²) where n = floor_size
/// - Sphere shell: O(r³) where r = sphere_radius
/// - Fluid fill: O(r³) interior scan
export auto generate_voxel_lab(
    VoxelWorld& world,
    LabConfig const& config = {}
) noexcept -> void {

    ZoneScopedN("GenerateVoxelLab");

    auto const& c = config;
    int32_t r = static_cast<int32_t>(c.sphere_radius);
    int32_t r_inner = r - static_cast<int32_t>(c.shell_thickness);
    int32_t fluid_max_y = static_cast<int32_t>(
        c.sphere_center.y - r + 2 * r * c.fluid_fill_level
    );

    // === PHASE 1: Checkerboard Floor ===
    for (int32_t z = -static_cast<int32_t>(c.floor_size / 2);
         z < static_cast<int32_t>(c.floor_size / 2); ++z) {
        for (int32_t x = -static_cast<int32_t>(c.floor_size / 2);
             x < static_cast<int32_t>(c.floor_size / 2); ++x) {

            // Checkerboard pattern
            bool is_white = ((x + z) & 1) == 0;
            uint8_t material = is_white
                ? static_cast<uint8_t>(LabMaterial::FloorWhite)
                : static_cast<uint8_t>(LabMaterial::FloorGray);

            world.set_voxel({x, c.floor_y, z}, VoxelData{.material_id = material});
        }
    }

    // === PHASE 2: Hollow Sphere with Fluid ===
    for (int32_t dz = -r; dz <= r; ++dz) {
        for (int32_t dy = -r; dy <= r; ++dy) {
            for (int32_t dx = -r; dx <= r; ++dx) {

                int32_t dist_sq = dx * dx + dy * dy + dz * dz;
                int32_t dist_sq_inner = (dx * dx + dy * dy + dz * dz);

                // Adjust for inner radius
                int32_t dx_adj = dx, dy_adj = dy, dz_adj = dz;
                int32_t inner_dist_sq = dx_adj * dx_adj + dy_adj * dy_adj + dz_adj * dz_adj;

                glm::ivec3 pos = c.sphere_center + glm::ivec3(dx, dy, dz);

                // Outer sphere boundary
                if (dist_sq <= r * r) {
                    // Inside outer sphere

                    if (dist_sq > r_inner * r_inner) {
                        // Shell: between inner and outer radius
                        world.set_voxel(pos, VoxelData{
                            .material_id = static_cast<uint8_t>(LabMaterial::Glass)
                        });
                    } else {
                        // Interior: check if should be fluid
                        int32_t world_y = pos.y;
                        int32_t fluid_level = c.sphere_center.y - r_inner +
                            static_cast<int32_t>(2 * r_inner * c.fluid_fill_level);

                        if (world_y <= fluid_level) {
                            world.set_voxel(pos, VoxelData{
                                .material_id = static_cast<uint8_t>(LabMaterial::Fluid)
                            });
                        }
                        // Else: air (leave empty)
                    }
                }
            }
        }
    }

    TracyMessageL("Voxel Laboratory generated successfully");
}

/// Альтернативная генерация с оптимизацией для SVO.
/// Использует mid-point circle algorithm для сферы.
export auto generate_voxel_lab_optimized(
    VoxelWorld& world,
    LabConfig const& config = {}
) noexcept -> void {

    ZoneScopedN("GenerateVoxelLabOptimized");

    // Phase 1: Floor (same as above)
    generate_checkerboard_floor(world, config);

    // Phase 2: Sphere using iterative refinement
    // For each Y slice, draw a circle
    auto const& c = config;
    int32_t r = static_cast<int32_t>(c.sphere_radius);
    int32_t r_inner = r - static_cast<int32_t>(c.shell_thickness);

    for (int32_t dy = -r; dy <= r; ++dy) {
        int32_t slice_radius = static_cast<int32_t>(
            std::sqrt(static_cast<float>(r * r - dy * dy))
        );
        int32_t slice_radius_inner = static_cast<int32_t>(
            std::sqrt(static_cast<float>(r_inner * r_inner - dy * dy))
        );

        int32_t world_y = c.sphere_center.y + dy;

        // Draw circle at this Y level
        for (int32_t dz = -slice_radius; dz <= slice_radius; ++dz) {
            for (int32_t dx = -slice_radius; dx <= slice_radius; ++dx) {
                int32_t dist_sq = dx * dx + dz * dz;

                glm::ivec3 pos = c.sphere_center + glm::ivec3(dx, dy, dz);

                if (dist_sq <= slice_radius * slice_radius) {
                    if (dist_sq > slice_radius_inner * slice_radius_inner) {
                        // Shell
                        world.set_voxel(pos, VoxelData{
                            .material_id = static_cast<uint8_t>(LabMaterial::Glass)
                        });
                    } else {
                        // Interior: fluid or air
                        int32_t fluid_level = c.sphere_center.y - r_inner +
                            static_cast<int32_t>(2 * r_inner * config.fluid_fill_level);

                        if (world_y <= fluid_level) {
                            world.set_voxel(pos, VoxelData{
                                .material_id = static_cast<uint8_t>(LabMaterial::Fluid)
                            });
                        }
                    }
                }
            }
        }
    }
}

/// Генерирует только checkerboard пол.
auto generate_checkerboard_floor(
    VoxelWorld& world,
    LabConfig const& config
) noexcept -> void {

    ZoneScopedN("GenerateCheckerboardFloor");

    int32_t half = static_cast<int32_t>(config.floor_size / 2);

    for (int32_t z = -half; z < half; ++z) {
        for (int32_t x = -half; x < half; ++x) {
            bool is_white = ((x + z) & 1) == 0;

            world.set_voxel(
                {x, config.floor_y, z},
                VoxelData{
                    .material_id = is_white
                        ? static_cast<uint8_t>(LabMaterial::FloorWhite)
                        : static_cast<uint8_t>(LabMaterial::FloorGray)
                }
            );
        }
    }
}

} // namespace projectv::demo
```

### 2.4 Диаграмма инициализации

```
Voxel Laboratory Initialization Sequence
─────────────────────────────────────────────────────────────

Frame 0 (Startup):
┌─────────────────────────────────────────────────────────────┐
│ 1. Create Vulkan Context                                     │
│    - Vulkan 1.4 device creation                              │
│    - VMA allocator initialization                            │
│    - Timeline semaphores                                     │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. Create ECS World                                          │
│    - Flecs initialization                                    │
│    - Register components                                     │
│    - Register systems                                        │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. Create Job System                                         │
│    - stdexec::static_thread_pool (6 workers)                 │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ 4. Create Physics World (Jolt)                               │
│    - PhysicsSystem initialization                            │
│    - Static floor collision                                  │
└─────────────────────────────────────────────────────────────┘

Frame 1 (Scene Generation):
┌─────────────────────────────────────────────────────────────┐
│ generate_voxel_lab(world)                                    │
│                                                              │
│ Phase 1: Checkerboard Floor                                  │
│   ████████████████████ 100% (262K voxels)                   │
│   Time: ~50 ms                                               │
│                                                              │
│ Phase 2: Hollow Sphere                                       │
│   ████████████████████ 100% (~1M voxels)                    │
│   Time: ~100 ms                                              │
│                                                              │
│ Phase 3: Fluid Fill                                          │
│   ████████████████████ 100% (~300K voxels)                  │
│   Time: ~30 ms                                               │
│                                                              │
│ Total Generation Time: < 200 ms                              │
│ SVO Build + Mesh: < 500 ms                                   │
└─────────────────────────────────────────────────────────────┘

Frame 2+ (Ready):
┌─────────────────────────────────────────────────────────────┐
│ The Voxel Laboratory Ready!                                  │
│                                                              │
│ Statistics:                                                  │
│   - 64 active chunks                                         │
│   - ~1.5M voxels total                                       │
│   - Sphere: 64³ voxels, glass shell + fluid interior        │
│   - Floor: 512×512 checkerboard                              │
│   - Physics: 1 static body (floor)                          │
│                                                              │
│ Camera spawns at (0, 150, 200)                               │
│ Looking at sphere center...                                   │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Механика взаимодействия

### 3.1 Spectator Mode (Полёт камерой)

**Управление:**

| Клавиша     | Действие                           |
|-------------|------------------------------------|
| W/A/S/D     | Движение вперёд/влево/назад/вправо |
| Space       | Движение вверх                     |
| Shift       | Движение вниз                      |
| Mouse       | Поворот камеры                     |
| Mouse Wheel | Изменение скорости                 |

### 3.2 ЛКМ — Projectile → Destruction

**Система снарядов:**

```cpp
// ProjectV.Demo.ProjectileSystem.cppm
export module ProjectV.Demo.ProjectileSystem;

import std;
import glm;
import flecs;
import Jolt;
import ProjectV.ECS.Components;
import ProjectV.Physics.World;
import ProjectV.Voxel.ChunkRegistry;

export namespace projectv::demo {

/// Конфигурация снаряда.
export struct ProjectileConfig {
    float speed{50.0f};              ///< m/s
    float radius{3.0f};              ///< Радиус взрыва при попадании
    float mass{10.0f};               ///< Масса снаряда
    glm::vec3 color{1.0f, 0.5f, 0.0f}; ///< Оранжевый
};

/// Система снарядов.
export class ProjectileSystem {
public:
    static auto register_system(
        flecs::world& world,
        physics::PhysicsWorld& physics,
        voxel::ChunkRegistry& registry,
        ProjectileConfig const& config = {}
    ) noexcept -> flecs::system;

private:
    static auto update(
        flecs::iter& it,
        TransformComponent const* transforms,
        CameraComponent const* cameras
    ) noexcept -> void;

    static auto spawn_projectile(
        glm::vec3 const& origin,
        glm::vec3 const& direction
    ) noexcept -> physics::BodyId;

    static auto on_collision(
        physics::BodyId projectile,
        glm::vec3 const& hit_point
    ) noexcept -> void;

    static physics::PhysicsWorld* physics_;
    static voxel::ChunkRegistry* registry_;
    static ProjectileConfig config_;
};

// Implementation
auto ProjectileSystem::update(
    flecs::iter& it,
    TransformComponent const* transforms,
    CameraComponent const* cameras
) noexcept -> void {
    ZoneScopedN("ProjectileSystem");

    if (!input::is_mouse_clicked(input::MouseButton::Left)) {
        return;
    }

    for (auto i : it) {
        glm::vec3 origin = transforms[i].position;
        glm::vec3 dir = transforms[i].rotation * glm::vec3(0, 0, -1);

        // Spawn projectile
        auto body = spawn_projectile(origin, dir);

        // Register collision callback
        physics_->on_collision(body, [](physics::CollisionEvent const& e) {
            on_collision(e.body_a, e.contact_point);
        });

        break;
    }
}

auto ProjectileSystem::spawn_projectile(
    glm::vec3 const& origin,
    glm::vec3 const& direction
) noexcept -> physics::BodyId {

    glm::vec3 velocity = direction * config_.speed;

    auto body = physics_->create_body({
        .position = origin + direction * 2.0f,  // Offset from camera
        .rotation = glm::quat(1, 0, 0, 0),
        .shape_type = physics::ShapeType::Sphere,
        .shape_size = glm::vec3(0.5f),
        .motion_type = physics::MotionType::Dynamic,
        .linear_velocity = velocity,
        .mass = config_.mass,
        .restitution = 0.1f,
        .friction = 0.5f,
        .is_projectile = true
    });

    return body.value();
}

auto ProjectileSystem::on_collision(
    physics::BodyId projectile,
    glm::vec3 const& hit_point
) noexcept -> void {

    ZoneScopedN("ProjectileImpact");

    // Create explosion
    perform_explosion(hit_point, config_.radius);

    // Create debris
    create_debris(hit_point, config_.radius, 30);

    // Mark chunks dirty
    registry_->mark_region_dirty(hit_point, config_.radius * 2);

    // Remove projectile
    physics_->destroy_body(projectile);

    // Check for sphere breach (fluid release)
    check_sphere_breach(hit_point);

    TracyMessageL(("Projectile impact at: " +
        std::to_string(hit_point.x) + ", " +
        std::to_string(hit_point.y) + ", " +
        std::to_string(hit_point.z)).c_str());
}

} // namespace projectv::demo
```

### 3.3 Sphere Breach Detection

```cpp
/// Проверяет пробитие сферы и активирует истечение жидкости.
auto check_sphere_breach(glm::vec3 const& impact_point) noexcept -> void {

    // Check if impact is within sphere bounds
    glm::vec3 sphere_center{0.0f, 100.0f, 0.0f};
    float sphere_radius = 64.0f;

    float dist_to_center = glm::length(impact_point - sphere_center);

    if (dist_to_center <= sphere_radius + 5.0f) {
        // Impact near or on sphere - activate fluid in nearby chunks
        // Fluid will flow through the breach via CA simulation

        TracyMessageL("Sphere breached! Fluid release activated.");
    }
}
```

### 3.4 Cellular Automata для жидкости

```cpp
// ProjectV.Demo.FluidCA.cppm
export module ProjectV.Demo.FluidCA;

import std;
import glm;
import ProjectV.Voxel.CA;

export namespace projectv::demo {

/// CA для жидкости в Voxel Laboratory.
///
/// ## Behavior
/// - Fluid падает вниз под действием гравитации
/// - При достижении пола растекается в стороны
/// - Собирается в лужи на полу
export class FluidCASystem {
public:
    /// Обновляет один шаг CA для жидкости.
    static auto update_fluid(
        voxel::ChunkData& chunk,
        glm::ivec3 const& pos
    ) noexcept -> bool;

private:
    static constexpr uint8_t FLUID_ID = static_cast<uint8_t>(LabMaterial::Fluid);
    static constexpr uint8_t AIR_ID = static_cast<uint8_t>(LabMaterial::Air);
    static constexpr uint8_t FLOOR_WHITE = static_cast<uint8_t>(LabMaterial::FloorWhite);
    static constexpr uint8_t FLOOR_GRAY = static_cast<uint8_t>(LabMaterial::FloorGray);
};

auto FluidCASystem::update_fluid(
    voxel::ChunkData& chunk,
    glm::ivec3 const& pos
) noexcept -> bool {

    auto& voxel = chunk.get(pos);

    if (voxel.material_id != FLUID_ID) {
        return false;
    }

    // Try to fall down
    if (pos.y > 0) {
        glm::ivec3 below_pos(pos.x, pos.y - 1, pos.z);
        auto& below = chunk.get(below_pos);

        if (below.material_id == AIR_ID) {
            std::swap(voxel, below);
            return true;
        }
    }

    // Try to flow horizontally (lower priority)
    static thread_local std::mt19937 gen(std::random_device{}());
    std::array<glm::ivec3, 4> offsets = {{
        {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}
    }};
    std::shuffle(offsets.begin(), offsets.end(), gen);

    for (auto const& offset : offsets) {
        glm::ivec3 neighbor = pos + offset;

        if (neighbor.x >= 0 && neighbor.x < 16 &&
            neighbor.z >= 0 && neighbor.z < 16) {

            auto& neighbor_voxel = chunk.get(neighbor);

            if (neighbor_voxel.material_id == AIR_ID) {
                // Check if there's floor below neighbor
                if (neighbor.y > 0) {
                    glm::ivec3 below_neighbor(neighbor.x, neighbor.y - 1, neighbor.z);
                    auto& floor = chunk.get(below_neighbor);

                    if (floor.material_id == FLOOR_WHITE ||
                        floor.material_id == FLOOR_GRAY) {
                        std::swap(voxel, neighbor_voxel);
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

} // namespace projectv::demo
```

---

## 4. Сценарий демонстрации: 4 Фазы

### 4.1 Диаграмма фаз

```
Tech Showcase Demo Script (5 minutes)
─────────────────────────────────────────────────────────────

┌─────────────────────────────────────────────────────────────┐
│ PHASE 1: STATIC (0:00 - 1:15)                                │
│ "Camera Orbit - SVO Performance Showcase"                    │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│   Camera Path:                                               │
│   ┌─────────────────────────────────────────────────────┐   │
│   │                     ◯ Sphere                        │   │
│   │                   ╱    ╲                            │   │
│   │      START ──▶  ╱        ╲  ◀── END                │   │
│   │     (0,150,200)╲          ╱    (0,200,-150)        │   │
│   │                  ╲______╱                          │   │
│   │                    ▼                                │   │
│   │              □ ■ □ ■ □ Floor                        │   │
│   └─────────────────────────────────────────────────────┘   │
│                                                              │
│   Actions:                                                   │
│   - Smooth camera orbit around sphere                       │
│   - Show glass material reflections                         │
│   - Show fluid shimmer inside                               │
│   - Tracy metrics: Frame Time ~8ms (120+ FPS)               │
│                                                              │
│   Commentary:                                                │
│   "SVO rendering with Vulkan Mesh Shaders -                 │
│    1.5M voxels at 120 FPS"                                  │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ PHASE 2: PHYSICS DESTRUCTION (1:15 - 2:30)                   │
│ "Projectile Impact - Jolt Physics Integration"               │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│   Before:                    After:                         │
│   ┌─────────────────┐       ┌─────────────────┐            │
│   │      ◯         │       │      ◯          │            │
│   │    ╱   ╲       │       │    ╱   ╲        │            │
│   │   │fluid│      │  ──▶  │   │    💥│     │            │
│   │    ╲___╱       │       │    ╲___╱ debris │            │
│   │                 │       │       * * *    │            │
│   └─────────────────┘       └─────────────────┘            │
│                                                              │
│   Actions:                                                   │
│   1. Player fires projectile (LMB)                          │
│   2. Projectile hits sphere shell                           │
│   3. Explosion creates breach                               │
│   4. Glass debris spawns (Jolt Physics)                     │
│   5. Tracy spike: Frame Time 15-20ms                        │
│                                                              │
│   Commentary:                                                │
│   "Jolt Physics handles debris simulation -                 │
│    30 rigid bodies per impact"                              │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ PHASE 3: FLUID SIMULATION (2:30 - 3:45)                      │
│ "Cellular Automata - Fluid Dynamics"                         │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│   Breach Created:            Fluid Flowing:                 │
│   ┌─────────────────┐       ┌─────────────────┐            │
│   │      ◯         │       │      ◯          │            │
│   │    ╱   ╲       │       │    ╱   ╲        │            │
│   │   │~~~~│ breach│  ──▶  │   │    ╲~~~     │            │
│   │    ╲___╱       │       │    ╲___╱        │            │
│   │                 │       │      ~~~~       │            │
│   │   □ ■ □ ■ □    │       │   □ ■~■ □ ■    │            │
│   └─────────────────┘       └─────────────────┘            │
│                                                              │
│   Actions:                                                   │
│   1. Fluid detects breach in shell                          │
│   2. Fluid flows through hole (gravity)                     │
│   3. Fluid falls to floor                                   │
│   4. Fluid spreads on checkerboard (CA)                     │
│   5. Tracy: CA Update < 2ms per frame                       │
│                                                              │
│   Commentary:                                                │
│   "Cellular Automata simulates fluid -                      │
│    < 2ms per frame for 300K active voxels"                  │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ PHASE 4: STRESS TEST (3:45 - 5:00)                           │
│ "Structural Collapse - Full System Integration"              │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│   Before:                    After:                         │
│   ┌─────────────────┐       ┌─────────────────┐            │
│   │      ◯         │       │                 │            │
│   │    ╱   ╲       │       │     debris      │            │
│   │   │~~~~│      │  ──▶  │    * * * *     │            │
│   │    ╲___╱       │       │   * SPLASH *   │            │
│   │                 │       │  ~~~~~~~~~~~~  │            │
│   │   □ ■ □ ■ □    │       │   □ ■~■~■ ■    │            │
│   └─────────────────┘       └─────────────────┘            │
│                                                              │
│   Actions:                                                   │
│   1. Player shoots sphere base                              │
│   2. Entire shell becomes debris (Jolt activation island)   │
│   3. Massive debris falls into fluid                        │
│   4. Splash effects, fluid displacement                     │
│   5. Tracy peak: Frame Time 25-30ms (still < 33ms)          │
│                                                              │
│   Commentary:                                                │
│   "Full destruction: 200+ debris objects,                   │
│    fluid simulation, no frame drops below 30 FPS"           │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 Детальный скрипт

```
Defense Demo Script (5 minutes)
─────────────────────────────────────────────────────────────

[0:00 - 0:30] Startup & Scene Generation
─────────────────────────────────────────────────────────────
- Launch application
- Loading screen: "Generating The Voxel Laboratory..."
- Tracy overlay visible from start
- Display progress:
  * Floor: ████████████████████ 100%
  * Sphere: ████████████████████ 100%
  * Fluid: ████████████████████ 100%

[0:30 - 1:15] PHASE 1: Static Orbit
─────────────────────────────────────────────────────────────
- Camera auto-orbits around sphere
- Show glass material with reflections
- Show fluid shimmer inside sphere
- Tracy metrics:
  * Frame time: ~8 ms (120+ FPS)
  * Memory: ~256 MB
  * Draw calls: ~10 (mesh shader batches)

[1:15 - 2:30] PHASE 2: Physics Destruction
─────────────────────────────────────────────────────────────
- Switch to player control
- Crosshair appears
- Player fires 2-3 projectiles at sphere
- Each impact:
  * Explosion effect
  * Glass debris spawns
  * Breach visible in shell
- Tracy shows frame spikes (15-20ms) with quick recovery

[2:30 - 3:45] PHASE 3: Fluid Simulation
─────────────────────────────────────────────────────────────
- Breach in sphere visible
- Blue fluid begins flowing through hole
- Fluid falls to floor
- Fluid spreads on checkerboard
- Tracy: CA Update zone visible (~1.5ms)
- No visible frame drops

[3:45 - 5:00] PHASE 4: Stress Test - Structural Collapse
─────────────────────────────────────────────────────────────
- Player targets sphere base/support
- Large explosion destroys structural integrity
- Entire sphere shell collapses (Jolt activation island)
- Massive debris falls into fluid pool
- Splash and displacement effects
- Tracy peak: 25-30ms (acceptable worst case)
- Recovery to normal frame time

[5:00] Final Metrics Summary
─────────────────────────────────────────────────────────────
- Freeze frame
- Display Tracy summary overlay:
  * Average frame time: < 16 ms ✓
  * CA update: < 2 ms ✓
  * Physics step: < 3 ms ✓
  * Memory: < 512 MB ✓
- "60 FPS maintained throughout demo"
```

---

## 5. Technical Art: Frutiger Aero Lighting

### 5.1 Эстетика Frutiger Aero

**Характерные черты:**

- Яркие, чистые цвета
- Глянцевые, отражающие поверхности
- Стекло и прозрачность
- Вода и жидкости
- Небо и облака
- Футуристичная, "снотворческая" атмосфера

**Цветовая палитра:**

| Элемент     | Цвет                                | Описание                                 |
|-------------|-------------------------------------|------------------------------------------|
| Glass Shell | RGB(240, 245, 255)                  | Холодный белый с лёгким голубым оттенком |
| Fluid       | RGB(0, 150, 255) → RGB(0, 100, 200) | Ярко-синий, градиент глубины             |
| Floor White | RGB(255, 255, 255)                  | Чистый белый                             |
| Floor Gray  | RGB(200, 205, 210)                  | Холодный серый                           |
| Sky/Ambient | RGB(135, 206, 250)                  | Светло-голубой (Light Sky Blue)          |
| Sun Light   | RGB(255, 250, 240)                  | Тёплый белый                             |

### 5.2 Освещение

```cpp
// ProjectV.Demo.Lighting.cppm
export module ProjectV.Demo.Lighting;

import std;
import glm;
import ProjectV.Render.Lighting;

export namespace projectv::demo {

/// Конфигурация освещения Frutiger Aero.
export struct FrutigerAeroLightingConfig {
    /// Направленное освещение (Sun).
    struct Sun {
        glm::vec3 direction{0.3f, -0.8f, 0.5f};  ///< Направление
        glm::vec3 color{1.0f, 0.98f, 0.94f};     ///< Цвет (тёплый белый)
        float intensity{1.5f};                    ///< Интенсивность
    } sun;

    /// Ambient Occlusion (GTAO).
    struct AmbientOcclusion {
        bool enabled{true};
        float radius{2.0f};          ///< Радиус AO в метрах
        float intensity{0.8f};       ///< Интенсивность затенения
        float power{1.5f};           ///< Контраст AO
    } gtao;

    /// Volumetric Fog.
    struct VolumetricFog {
        bool enabled{true};
        glm::vec3 color{0.85f, 0.92f, 1.0f};  ///< Цвет тумана (светло-голубой)
        float density{0.002f};                 ///< Плотность
        float height_falloff{0.01f};           ///< Затухание по высоте
        float scattering{0.5f};                ///< Рассеяние света
    } fog;

    /// Ambient light.
    glm::vec3 ambient_color{0.53f, 0.81f, 0.98f};  ///< Light Sky Blue
    float ambient_intensity{0.4f};
};

/// Применяет освещение Frutiger Aero к сцене.
export auto apply_frutiger_aero_lighting(
    render::LightingSystem& lighting,
    FrutigerAeroLightingConfig const& config = {}
) noexcept -> void {

    // 1. Directional Sun Light
    lighting.set_directional_light({
        .direction = config.sun.direction,
        .color = config.sun.color,
        .intensity = config.sun.intensity,
        .cast_shadows = false  // MVP: без теней
    });

    // 2. Ambient Light
    lighting.set_ambient({
        .color = config.ambient_color,
        .intensity = config.ambient_intensity
    });

    // 3. GTAO (Ground Truth Ambient Occlusion)
    if (config.gtao.enabled) {
        lighting.enable_gtao({
            .radius = config.gtao.radius,
            .intensity = config.gtao.intensity,
            .power = config.gtao.power
        });
    }

    // 4. Volumetric Fog (for "dream" atmosphere)
    if (config.fog.enabled) {
        lighting.enable_volumetric_fog({
            .color = config.fog.color,
            .density = config.fog.density,
            .height_falloff = config.fog.height_falloff,
            .scattering = config.fog.scattering
        });
    }
}

} // namespace projectv::demo
```

### 5.3 Материалы для Frutiger Aero

```slang
// ProjectV/Render/Materials/FrutigerAero.slang

/// Glass material для сферы.
struct GlassMaterial {
    float3 base_color;
    float roughness;
    float metalness;
    float transparency;
    float ior;  // Index of refraction
};

/// Fluid material для жидкости.
struct FluidMaterial {
    float3 base_color;
    float transparency;
    float refraction_strength;
    float caustics_intensity;
};

// Material presets
static const GlassMaterial kSphereGlass = {
    .base_color = float3(0.94, 0.96, 1.0),
    .roughness = 0.05,      // Very smooth
    .metalness = 0.0,
    .transparency = 0.3,    // Slightly transparent
    .ior = 1.5              // Glass IOR
};

static const FluidMaterial kFluid = {
    .base_color = float3(0.0, 0.6, 1.0),
    .transparency = 0.7,
    .refraction_strength = 0.5,
    .caustics_intensity = 0.3
};
```

---

## 6. Целевые метрики (Tracy Profiler)

### 6.1 Основные метрики

| Метрика             | Целевое  | Критическое | Метод измерения    |
|---------------------|----------|-------------|--------------------|
| **Frame Time**      | < 16 ms  | > 33 ms     | Tracy frame marker |
| **CA Update**       | < 2 ms   | > 5 ms      | Tracy zone         |
| **Physics Step**    | < 3 ms   | > 6 ms      | Tracy zone         |
| **Mesh Generation** | < 4 ms   | > 8 ms      | Tracy zone         |
| **Render**          | < 8 ms   | > 15 ms     | Tracy zone         |
| **ACMR**            | < 0.8    | > 1.2       | GPU timer query    |
| **Draw Calls**      | < 20     | > 100       | Tracy counter      |
| **Memory**          | < 512 MB | > 1 GB      | Tracy memory       |

### 6.2 Tracy Zones для демонстрации

```cpp
// Ключевые зоны для Tracy profiling

// Main loop
DEMO_ZONE("Frame");
DEMO_COUNTER("Frame Time (ms)", delta_time * 1000.0f);

// Systems
DEMO_ZONE("ProjectileSystem");
DEMO_ZONE("FluidCASystem");
DEMO_ZONE("PhysicsStep");
DEMO_ZONE("RenderSystem");

// Counters
DEMO_COUNTER("Active Fluid Voxels", fluid_count);
DEMO_COUNTER("Debris Count", debris_count);
DEMO_COUNTER("Draw Calls", render_stats.draw_calls);
```

### 6.3 Tracy Display Layout

```
Tracy Profiler Window Layout (Tech Showcase)
─────────────────────────────────────────────────────────────

┌─────────────────────────────────────────────────────────────┐
│ Frame Time Graph                                            │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ ▁▂▃▄▅▆▇█▇▆▅▄▃▂▁▁▂▃▄▅▆▇█▇▆▅▄▃▂▁ 60 FPS stable        │ │
│ │              ▲   ▲   ▲                                   │ │
│ │         Projectile impacts                               │ │
│ └─────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│ Zone Statistics                                             │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ Zone Name              │ Time (ms) │ % of Frame │ Calls │ │
│ ├─────────────────────────┼───────────┼─────────────┼───────┤ │
│ │ InputSystem            │    0.10   │    0.6%     │   1   │ │
│ │ ProjectileSystem       │    0.80   │    5.0%     │   1   │ │
│ │ FluidCASystem          │    1.50   │    9.4%     │   1   │ │
│ │ PhysicsStep            │    2.20   │   13.8%     │   1   │ │
│ │ RenderSystem           │    6.00   │   37.5%     │   1   │ │
│ │ BufferSwap             │    0.40   │    2.5%     │   1   │ │
│ ├─────────────────────────┼───────────┼─────────────┼───────┤ │
│ │ TOTAL                  │   11.00   │   68.8%     │   6   │ │
│ └─────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│ GPU Counters                                                │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ Draw Calls: 12 (mesh shader batches)                    │ │
│ │ Triangles: ~500,000                                     │ │
│ │ ACMR: 0.65                                              │ │
│ │ GPU Time: 7.2 ms                                        │ │
│ └─────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│ Demo Counters                                               │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ Active Fluid Voxels: 45,230                             │ │
│ │ Debris Objects: 87                                      │ │
│ │ Sphere Integrity: 78%                                   │ │
│ └─────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│ Memory Usage                                                │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ SVO Data:      180 MB                                   │ │
│ │ Mesh Buffers:  120 MB                                   │ │
│ │ Physics:        64 MB                                   │ │
│ │ TOTAL:         364 MB                                   │ │
│ └─────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

---

## 7. Чеклист готовности к защите

### 7.1 Функциональность

- [ ] Vulkan 1.4 контекст инициализируется
- [ ] Сцена "Voxel Laboratory" генерируется за < 500 ms
- [ ] Checkerboard пол отображается корректно
- [ ] Стеклянная сфера рендерится с отражениями
- [ ] Жидкость внутри сферы видна
- [ ] Spectator camera управляется плавно
- [ ] Projectile system работает (ЛКМ)
- [ ] При попадании создаётся breach в сфере
- [ ] Debris разлетается физично (Jolt)
- [ ] Жидкость вытекает через breach
- [ ] Жидкость растекается по полу
- [ ] Полное обрушение сферы работает

### 7.2 Производительность

- [ ] Frame time < 16 ms (среднее)
- [ ] Frame time < 33 ms (пиковое при collapse)
- [ ] CA update < 2 ms
- [ ] Physics step < 3 ms
- [ ] ACMR < 0.8
- [ ] Draw calls < 20
- [ ] Memory < 512 MB

### 7.3 Визуальное качество (Frutiger Aero)

- [ ] Яркое, чистое освещение
- [ ] Glass материал с отражениями
- [ ] Fluid с прозрачностью
- [ ] Мягкий AO на углах
- [ ] Лёгкий fog для атмосферы
- [ ] Цветовая палитра соответствует стилю

### 7.4 Инструментарий

- [ ] Tracy работает
- [ ] Tracy overlay отображается
- [ ] Все зоны профилируются
- [ ] Memory tracking работает

---

## Ссылки

- [Project Defense Model](./01_project_defense_model.md)
- [Job System P2300 Spec](../practice/31_job_system_p2300_spec.md)
- [Voxel Sync Pipeline](../practice/32_voxel_sync_pipeline.md)
- [Vulkan 1.4 Spec](../practice/04_vulkan_spec.md)
- [CA-Physics Bridge](../practice/30_ca_physics_bridge.md)

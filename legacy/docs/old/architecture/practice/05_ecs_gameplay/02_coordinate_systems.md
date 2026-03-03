# Спецификация систем координат ProjectV

---

## Обзор

ProjectV использует **Floating Origin** архитектуру для поддержки гигантских миров (64+ км) с точностью до миллиметра.

### Ключевые решения

| Решение                | Обоснование                                   |
|------------------------|-----------------------------------------------|
| **Floating Origin**    | Камера всегда в (0,0,0), мир двигается вокруг |
| **JoltPhysics Y-Up**   | Основная система для игровой логики           |
| **Vulkan Y-Down**      | Только для финального рендеринга              |
| **SVO max_depth = 16** | 2^16 = 65536³ вокселей = 64км³ при 1м/воксель |

---

## Floating Origin Architecture

### Проблема больших миров

```
IEEE 754 Single Precision (float):
- 23 бита мантиссы
- Точность: ~1/2^23 ≈ 0.000000119

При позиции X = 100,000 (100 км от центра):
- Точность: 100,000 × 0.000000119 ≈ 0.012 метров
- На 1000 км: точность ≈ 0.12 метров (12 см!)

Результат: Z-fighting, jitter, некорректная физика
```

### Решение: Floating Origin

```
Камера всегда в (0, 0, 0)
Мир сдвигается относительно камеры

Frame N:                          Frame N+1:
Камера в (0, 0, 0)                Камера в (0, 0, 0)
Объект A в (100, 50, 200)         Объект A в (95, 50, 195)
Origin offset: (0, 0, 0)          Origin offset: (5, 0, 5)

Физическая позиция объекта (абсолютная):
(5, 50, 5) + (95, 50, 195) = (100, 50, 200) — неизменна
```

---

## Memory Layout

### WorldCoordinate

```
WorldCoordinate (16 bytes, 128-bit)
┌─────────────────────────────────────────────────────────────┐
│  sector_x: int32_t (4 bytes)    — сектор мира X             │
│  sector_y: int32_t (4 bytes)    — сектор мира Y             │
│  sector_z: int32_t (4 bytes)    — сектор мира Z             │
│  local_pos: glm::vec3 (12 bytes) — позиция внутри сектора   │
│  padding: 4 bytes                                            │
│  Total: 28 bytes                                             │
└─────────────────────────────────────────────────────────────┘

Sector Size = 4096 метров (relocatable unit)
Sector Coordinates: int32_t (±2^15 секторов = ±134 миллионов км)
```

### FloatingOrigin

```
FloatingOrigin (PIMPL)
┌─────────────────────────────────────────────────────────────┐
│  std::unique_ptr<Impl> (8 bytes)                            │
│  └── Impl:                                                  │
│      ├── current_sector_: SectorCoord (12 bytes)            │
│      ├── local_offset_: glm::vec3 (12 bytes)                │
│      ├── accumulated_offset_: glm::dvec3 (24 bytes)         │
│      ├── threshold_: float (4 bytes) = 1024.0f              │
│      └── on_origin_shift_: callback (32 bytes)              │
│  Total: 8 bytes (external) + ~100 bytes (internal)          │
└─────────────────────────────────────────────────────────────┘
```

### Transform (GPU-aligned)

```
TransformComponent (64 bytes, GPU-aligned)
┌─────────────────────────────────────────────────────────────┐
│  position: glm::vec3 (12 bytes)    — локальная позиция      │
│  padding0: 4 bytes                                           │
│  rotation: glm::quat (16 bytes)    — вращение               │
│  scale: glm::vec3 (12 bytes)       — масштаб                │
│  sector_delta: int32_t[3] (12 bytes) — смещение сектора     │
│  padding1: 4 bytes                                           │
│  Total: 64 bytes (cache-line friendly)                      │
└─────────────────────────────────────────────────────────────┘
```

---

## State Machines

### Origin Shift State

```
Origin Shift Decision

    ┌──────────────────┐
    │  CHECK_POSITION  │ ←── Каждый кадр
    └────────┬─────────┘
             │ |local_pos| > threshold?
             │ YES
             ▼
    ┌──────────────────┐
    │   CALCULATE_NEW  │
    │     ORIGIN       │
    └────────┬─────────┘
             │
             ▼
    ┌──────────────────┐
    │ SHIFT_ALL_ENTITIES│ ←── Вычесть offset из всех позиций
    └────────┬─────────┘
             │
             ▼
    ┌──────────────────┐
    │ UPDATE_SECTOR    │ ←── Обновить current_sector_
    └────────┬─────────┘
             │
             ▼
    ┌──────────────────┐
    │ NOTIFY_SYSTEMS   │ ←── Callback для физики, рендера
    └──────────────────┘
```

### Sector Loading State

```
Sector Lifecycle

    ┌─────────────┐
    │   UNLOADED  │ ←── Сектор не существует
    └──────┬──────┘
           │ player enters load radius
           ▼
    ┌─────────────┐
    │   LOADING   │ ←── Генерация/загрузка данных
    └──────┬──────┘
           │ data ready
           ▼
    ┌─────────────┐
    │   ACTIVE    │ ←── Сектор полностью загружен
    └──────┬──────┘
           │ player exits unload radius
           ▼
    ┌─────────────┐
    │  UNLOADING  │ ←── Сохранение/освобождение
    └──────┬──────┘
           │ cleanup complete
           ▼
    ┌─────────────┐
    │   UNLOADED  │ ←── Сектор удалён из памяти
    └─────────────┘
```

---

## API Contracts

### WorldCoordinate

```cpp
// ProjectV.World.Coordinate.cppm
export module ProjectV.World.Coordinate;

import std;
import glm;

export namespace projectv::world {

/// Размер сектора в метрах.
export constexpr float SECTOR_SIZE = 4096.0f;

/// Порог сдвига origin.
export constexpr float ORIGIN_SHIFT_THRESHOLD = 1024.0f;

/// Максимальная глубина SVO.
export constexpr uint32_t SVO_MAX_DEPTH = 16;

/// Координаты сектора.
export struct SectorCoord {
    int32_t x{0};
    int32_t y{0};
    int32_t z{0};

    /// Хеш для unordered_map.
    [[nodiscard]] auto hash() const noexcept -> size_t;

    /// Сравнение.
    [[nodiscard]] auto operator==(SectorCoord const&) const noexcept -> bool = default;
    [[nodiscard]] auto operator<=>(SectorCoord const&) const noexcept = default;

    /// Конвертация в мировые координаты.
    [[nodiscard]] auto to_world_position() const noexcept -> glm::dvec3;

    /// Из мировых координат.
    [[nodiscard]] static auto from_world_position(glm::dvec3 const& pos) noexcept -> SectorCoord;
};

/// Мировые координаты с двойной точностью.
///
/// ## Precision
/// - Сектор: int32_t → ±2^15 секторов = ±134M км
/// - Локальная позиция: float → точность < 1мм при SECTOR_SIZE=4096
///
/// ## Invariants
/// - local_position всегда в пределах [-SECTOR_SIZE/2, SECTOR_SIZE/2]
export struct WorldCoordinate {
    SectorCoord sector;
    glm::vec3 local_position{0.0f};

    /// Получает позицию как double (абсолютную).
    [[nodiscard]] auto to_absolute() const noexcept -> glm::dvec3;

    /// Создаёт из абсолютной позиции.
    [[nodiscard]] static auto from_absolute(glm::dvec3 const& pos) noexcept -> WorldCoordinate;

    /// Вычисляет относительную позицию к другому сектору.
    [[nodiscard]] auto relative_to(SectorCoord const& origin_sector) const noexcept -> glm::vec3;

    /// Нормализует local_position (держит в пределах сектора).
    auto normalize() noexcept -> void;
};

} // namespace projectv::world
```

---

### FloatingOrigin

```cpp
// ProjectV.World.FloatingOrigin.cppm
export module ProjectV.World.FloatingOrigin;

import std;
import glm;
import ProjectV.World.Coordinate;
import ProjectV.ECS.Flecs;

export namespace projectv::world {

/// Callback для уведомления о сдвиге origin.
export using OriginShiftCallback = std::move_only_function<void(glm::vec3 const& offset)>;

/// Floating Origin Manager.
///
/// ## Purpose
/// Поддерживает камеру в (0, 0, 0), сдвигая все объекты при необходимости.
///
/// ## Thread Safety
/// - update() вызывается из main thread
/// - callbacks вызываются синхронно
///
/// ## Invariants
/// - Камера всегда в local_position (0, 0, 0)
/// - current_sector_ отражает сектор камеры
/// - accumulated_offset_ для отладки/визуализации
export class FloatingOrigin {
public:
    FloatingOrigin() noexcept;
    ~FloatingOrigin() noexcept;

    FloatingOrigin(FloatingOrigin&&) noexcept;
    FloatingOrigin& operator=(FloatingOrigin&&) noexcept;
    FloatingOrigin(const FloatingOrigin&) = delete;
    FloatingOrigin& operator=(const FloatingOrigin&) = delete;

    /// Обновляет origin при необходимости.
    ///
    /// @param camera_local_position Позиция камеры в локальных координатах
    /// @return true если произошёл сдвиг origin
    ///
    /// @pre camera_local_position — позиция относительно текущего origin
    /// @post Если |position| > threshold, все позиции сдвинуты
    auto update(glm::vec3 const& camera_local_position) noexcept -> bool;

    /// Регистрирует callback для сдвига origin.
    ///
    /// @param callback Функция, вызываемая при сдвиге
    auto on_origin_shift(OriginShiftCallback callback) noexcept -> void;

    /// Получает текущий сектор.
    [[nodiscard]] auto current_sector() const noexcept -> SectorCoord;

    /// Получает накопленный offset (для отладки).
    [[nodiscard]] auto accumulated_offset() const noexcept -> glm::dvec3;

    /// Устанавливает порог сдвига.
    auto set_threshold(float threshold) noexcept -> void;

    /// Принудительно сдвигает origin.
    auto force_shift(glm::vec3 const& offset) noexcept -> void;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::world
```

---

### Coordinate Transform

```cpp
// ProjectV.World.Transform.cppm
export module ProjectV.World.Transform;

import std;
import glm;
import ProjectV.World.Coordinate;

export namespace projectv::world {

/// JoltPhysics Y-Up → Vulkan Y-Down conversion.
export namespace coord_convert {

/// Матрица преобразования Jolt → Vulkan.
[[nodiscard]] inline auto jolt_to_vulkan_matrix() noexcept -> glm::mat4 {
    static const glm::mat4 matrix = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
    return matrix;
}

/// Преобразует позицию Jolt → Vulkan.
[[nodiscard]] inline auto jolt_to_vulkan(glm::vec3 const& pos) noexcept -> glm::vec3 {
    return {pos.x, -pos.y, pos.z};
}

/// Преобразует позицию Vulkan → Jolt.
[[nodiscard]] inline auto vulkan_to_jolt(glm::vec3 const& pos) noexcept -> glm::vec3 {
    return {pos.x, -pos.y, pos.z};
}

/// Преобразует quaternion Jolt → Vulkan.
[[nodiscard]] auto jolt_to_vulkan_quat(glm::quat const& q) noexcept -> glm::quat;

/// Создаёт Vulkan-совместимую projection matrix.
///
/// @param fov Field of view в радианах
/// @param aspect Aspect ratio
/// @param near Ближняя плоскость
/// @param far Дальняя плоскость
/// @return Projection matrix для Vulkan NDC
[[nodiscard]] inline auto vulkan_projection(
    float fov,
    float aspect,
    float near,
    float far
) noexcept -> glm::mat4 {
    glm::mat4 proj = glm::perspective(fov, aspect, near, far);
    // Vulkan NDC: Y-down, Z [0,1]
    glm::mat4 const correction = {
        1.0f,  0.0f,  0.0f,  0.0f,
        0.0f, -1.0f,  0.0f,  0.0f,
        0.0f,  0.0f,  0.5f,  0.0f,
        0.0f,  0.0f,  0.5f,  1.0f
    };
    return correction * proj;
}

} // namespace coord_convert

} // namespace projectv::world
```

---

## Coordinate Systems Integration

### Systems Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Coordinate Systems in ProjectV                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                    Absolute World Space                           │    │
│  │  WorldCoordinate { sector: int32_t[3], local: vec3 }             │    │
│  │  Precision: ~1mm within ±134 million km from origin              │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                    │                                     │
│                                    ▼                                     │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                    Floating Origin Space                          │    │
│  │  Camera always at (0, 0, 0)                                       │    │
│  │  All entities positioned relative to camera                      │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                    │                                     │
│           ┌────────────────────────┼────────────────────────┐           │
│           ▼                        ▼                        ▼           │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐          │
│  │ Physics (Jolt)  │  │ Gameplay (ECS)  │  │ UI (ImGui)      │          │
│  │ Y-Up, Right-handed│ Y-Up, Right-handed│ Screen coords    │          │
│  │ Sector-relative │ │ Sector-relative │ │ Pixels          │          │
│  └────────┬────────┘  └────────┬────────┘  └─────────────────┘          │
│           │                    │                                        │
│           │                    │                                        │
│           ▼                    ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                    Render Transform                               │    │
│  │  Y-flip for Vulkan NDC                                           │    │
│  │  FrontFace = CLOCKWISE (due to Y-flip)                           │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                    │                                     │
│                                    ▼                                     │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                    Vulkan NDC                                     │    │
│  │  X: [-1, 1] left → right                                         │    │
│  │  Y: [-1, 1] top → bottom (Y-DOWN!)                               │    │
│  │  Z: [0, 1] near → far                                            │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### Transformation Pipeline

```
Entity Position Flow:

1. Storage (ECS):
   WorldCoordinate { sector, local_position }

2. Physics Update (JoltPhysics):
   position = local_position (sector-relative)
   physics_world.set_origin(current_sector)

3. Origin Shift Check:
   if (|camera_position| > threshold):
       shift_all_entities(-camera_position)
       current_sector += camera_position / SECTOR_SIZE

4. Render Preparation:
   view_matrix = lookAt(camera_position, target, {0, -1, 0})  // Y-down for Vulkan
   model_matrix = entity_transform * jolt_to_vulkan_matrix()

5. GPU Submission:
   MVP = projection * view * model
   gl_Position = MVP * vertex_position
```

---

## SVO Integration (max_depth = 16)

### World Scale

```
SVO Configuration:
- max_depth = 16
- max_extent = 2^16 = 65536 voxels
- voxel_size = 1.0 meter
- total_extent = 65.536 km per axis

With Floating Origin:
- SVO covers one sector (4096m = 2^12 voxels)
- SVO depth per sector = 12
- Entities can span multiple sectors
- Seamless transitions via sector loading
```

### Sector-based SVO

```cpp
/// SVO Configuration for sectors.
export struct SectorSVOConfig {
    uint32_t depth{12};           ///< Depth for one sector (4096 voxels)
    float voxel_size{1.0f};       ///< 1 meter per voxel
    bool enable_dag_compression{true};

    /// Вычисляет физический размер сектора.
    [[nodiscard]] auto sector_size() const noexcept -> float {
        return static_cast<float>(1 << depth) * voxel_size;
    }
};

/// Sector-based voxel storage.
export class SectorVoxelStorage {
public:
    /// Получает SVO для сектора.
    [[nodiscard]] auto get_svo(SectorCoord const& coord) noexcept -> SVOTree*;

    /// Загружает сектор.
    auto load_sector(SectorCoord const& coord) noexcept -> void;

    /// Выгружает сектор.
    auto unload_sector(SectorCoord const& coord) noexcept -> void;

    /// Получает воксель по мировым координатам.
    [[nodiscard]] auto get_voxel(WorldCoordinate const& coord) const noexcept
        -> std::expected<VoxelData, SVOError>;

    /// Устанавливает воксель по мировым координатам.
    auto set_voxel(WorldCoordinate const& coord, VoxelData const& data) noexcept
        -> std::expected<void, SVOError>;

private:
    std::unordered_map<SectorCoord, SVOTree, SectorCoordHash> sectors_;
    SectorSVOConfig config_;
};
```

---

## Winding Order (Front Face)

### Critical Issue

При Y-flip инвертируется winding order:

```
Исходный треугольник (Y-Up):       После Y-flip (Y-Down):
     A (top)                           A (bottom)
    /\                                /\
   /  \                              /  \
  B----C                            B----C

Winding: A→B→C = CCW                 Winding: A→B→C = CW
```

### Vulkan Configuration

```cpp
VkPipelineRasterizationStateCreateInfo rasterization{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_CLOCKWISE,  // Из-за Y-flip!
    .lineWidth = 1.0f
};
```

---

## Transformation Tables

### System Conventions

| Система       | Y-ось | Z-ось    | Winding         |
|---------------|-------|----------|-----------------|
| JoltPhysics   | Up    | Forward  | CCW             |
| GLM (default) | Up    | Backward | CCW             |
| Vulkan NDC    | Down  | Forward  | CW (after flip) |
| glTF          | Up    | Forward  | CCW             |

### Conversion Matrices

| Преобразование  | Матрица                                                     |
|-----------------|-------------------------------------------------------------|
| Jolt → Vulkan   | `scale(1, -1, 1)`                                           |
| Vulkan → Jolt   | `scale(1, -1, 1)` (инволюция)                               |
| OpenGL → Vulkan | `scale(1, -1, 1) × translate(0, 0, 0.5) × scale(1, 1, 0.5)` |

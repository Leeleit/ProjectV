# СТВ-CPP-004: Границы PIMPL и защита Data-Oriented Design

---

## 1. Область применения

Настоящий стандарт определяет **строгие границы** использования паттерна PIMPL (Pointer to Implementation) в ProjectV.
Неправильное применение PIMPL может нарушить принципы Data-Oriented Design и привести к критическим проблемам
производительности.

---

## 3. Классификация типов данных

### 3.1 Категории типов

Все типы данных в ProjectV классифицируются на три категории:

| Категория           | PIMPL         | Heap Allocation | Примеры                              |
|---------------------|---------------|-----------------|--------------------------------------|
| **Hot Path Types**  | ❌ Запрещён    | ❌ Запрещён      | Transform, VoxelData, math types     |
| **System Types**    | ✅ Обязателен  | ✅ Разрешён      | RenderDevice, PhysicsWorld, ECSWorld |
| **Cold Path Types** | ⚠️ Опционален | ✅ Разрешён      | Config, AssetMetadata, UI State      |

### 3.2 Определения

**Hot Path Types** — типы данных, которые:

- Обрабатываются в ECS системах каждый кадр
- Итерируются в массивах (SoA или AoS)
- Используются в SIMD-операциях
- Сериализуются массово

**System Types** — типы данных, которые:

- Инкапсулируют внешние библиотеки (Vulkan, Jolt, Flecs)
- Имеют сложный внутренний state
- Создаются один раз при инициализации
- Не копируются и не перемещаются часто

**Cold Path Types** — типы данных, которые:

- Используются редко (загрузка, конфигурация)
- Не влияют на кадр
- Могут быть выделены в куче

---

## 4. PIMPL: Обязательное применение

### 4.1 Когда PIMPL ОБЯЗАТЕЛЕН

PIMPL обязателен для всех типов, которые:

1. **Инкапсулируют внешние C++ библиотеки**
  - `RenderDevice` → Vulkan types
  - `PhysicsWorld` → Jolt types
  - `ECSWorld` → Flecs types
  - `AudioEngine` → miniaudio types

2. **Содержат типы, несовместимые с модулями**
  - Шаблоны с макросами из external libraries
  - Типы с `#include` зависимостями

3. **Имеют платформо-зависимую реализацию**
  - `PlatformSubsystem` → SDL types
  - `FileSystem` → platform-specific handles

### 4.2 Пример: RenderDevice с PIMPL

```cpp
// ProjectV.Render.Vulkan.Device.cppm
export module ProjectV.Render.Vulkan.Device;

import std;
import vulkan;  // C-библиотека, разрешена в GMF

export namespace projectv::render::vulkan {

/// Render Device — PIMPL обязателен.
///
/// ## Why PIMPL?
/// - Скрывает VkDevice, VkPhysicalDevice, VmaAllocator
/// - Изолирует от изменений Vulkan API
/// - Уменьшает время компиляции зависимого кода
///
/// ## Thread Safety
/// - create(): main thread only
/// - destroy(): main thread only
/// - operations: thread-safe per Vulkan rules
export class RenderDevice {
public:
    /// Конфигурация устройства.
    export struct Config {
        std::string application_name{"ProjectV"};
        bool enable_validation{true};
        bool prefer_discrete_gpu{true};
    };

    /// Создаёт RenderDevice.
    [[nodiscard]] static auto create(Config const& config = {}) noexcept
        -> std::expected<RenderDevice, DeviceError>;

    ~RenderDevice() noexcept;

    // Move-only (PIMPL)
    RenderDevice(RenderDevice&&) noexcept;
    RenderDevice& operator=(RenderDevice&&) noexcept;
    RenderDevice(const RenderDevice&) = delete;
    RenderDevice& operator=(const RenderDevice&) = delete;

    /// Получает VkDevice (для низкоуровневых операций).
    /// Использовать только в критических случаях.
    [[nodiscard]] auto native_device() const noexcept -> VkDevice;

    /// Получает allocator.
    [[nodiscard]] auto allocator() const noexcept -> VmaAllocator;

    /// Ожидает завершения всех операций.
    auto wait_idle() const noexcept -> void;

private:
    RenderDevice() noexcept = default;

    // PIMPL — всегда unique_ptr
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::render::vulkan
```

```cpp
// ProjectV.Render.Vulkan.Device.cpp
module ProjectV.Render.Vulkan.Device;

// Vulkan и VMA — C-библиотеки, включаем здесь
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

import std;
import ProjectV.Render.Vulkan.Device;

namespace projectv::render::vulkan {

// Полное определение Impl в .cpp файле
struct RenderDevice::Impl {
    VkInstance instance{VK_NULL_HANDLE};
    VkPhysicalDevice physical_device{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    VmaAllocator allocator{VK_NULL_HANDLE};
    uint32_t graphics_queue_family{UINT32_MAX};
    VkQueue graphics_queue{VK_NULL_HANDLE};

    // Все Vulkan-specific данные здесь
    std::vector<VkSemaphore> semaphores_;
    std::vector<VkFence> fences_;
    VkPipelineCache pipeline_cache{VK_NULL_HANDLE};

    ~Impl() noexcept {
        if (pipeline_cache) {
            vkDestroyPipelineCache(device, pipeline_cache, nullptr);
        }
        if (allocator) {
            vmaDestroyAllocator(allocator);
        }
        if (device) {
            vkDestroyDevice(device, nullptr);
        }
        if (instance) {
            vkDestroyInstance(instance, nullptr);
        }
    }
};

auto RenderDevice::create(Config const& config) noexcept
    -> std::expected<RenderDevice, DeviceError> {

    RenderDevice result;
    result.impl_ = std::make_unique<Impl>();

    // Vulkan initialization code...
    // (implementation details omitted for brevity)

    return result;
}

RenderDevice::~RenderDevice() noexcept = default;
RenderDevice::RenderDevice(RenderDevice&&) noexcept = default;
RenderDevice& RenderDevice::operator=(RenderDevice&&) noexcept = default;

auto RenderDevice::native_device() const noexcept -> VkDevice {
    return impl_->device;
}

auto RenderDevice::allocator() const noexcept -> VmaAllocator {
    return impl_->allocator;
}

auto RenderDevice::wait_idle() const noexcept -> void {
    if (impl_ && impl_->device) {
        vkDeviceWaitIdle(impl_->device);
    }
}

} // namespace projectv::render::vulkan
```

### 4.3 Пример: PhysicsWorld с PIMPL

```cpp
// ProjectV.Physics.World.cppm
export module ProjectV.Physics.World;

import std;
import glm;

export namespace projectv::physics {

/// Physics World — PIMPL обязателен.
///
/// ## Why PIMPL?
/// - Скрывает Jolt Physics types
/// - Jolt использует макросы и шаблоны
/// - Изолирует зависимости от остального кода
export class PhysicsWorld {
public:
    export struct Config {
        glm::vec3 gravity{0.0f, -9.81f, 0.0f};
        uint32_t max_bodies{65536};
        uint32_t max_body_pairs{65536};
        uint32_t max_contact_constraints{10240};
    };

    [[nodiscard]] static auto create(Config const& config = {}) noexcept
        -> std::expected<PhysicsWorld, PhysicsError>;

    ~PhysicsWorld() noexcept;

    PhysicsWorld(PhysicsWorld&&) noexcept;
    PhysicsWorld& operator=(PhysicsWorld&&) noexcept;
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    /// Выполняет шаг симуляции.
    auto step(float delta_time) noexcept -> void;

    /// Создаёт rigid body.
    [[nodiscard]] auto create_body(RigidBodyDesc const& desc) noexcept
        -> std::expected<BodyId, PhysicsError>;

    /// Удаляет rigid body.
    auto destroy_body(BodyId id) noexcept -> void;

    /// Получает позицию тела.
    [[nodiscard]] auto get_body_position(BodyId id) const noexcept
        -> std::expected<glm::vec3, PhysicsError>;

private:
    PhysicsWorld() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::physics
```

```cpp
// ProjectV.Physics.World.cpp
module ProjectV.Physics.World;

// Jolt headers — ТОЛЬКО в .cpp файле!
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>

import std;
import glm;
import ProjectV.Physics.World;

namespace projectv::physics {

struct PhysicsWorld::Impl {
    JPH::PhysicsSystem physics_system;
    JPH::BodyInterface* body_interface{nullptr};

    // Broad phase layer interface
    std::unique_ptr<JPH::BroadPhaseLayerInterface> bp_layer_interface;
    std::unique_ptr<JPH::ObjectLayerPairFilter> object_layer_filter;
    std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilter> bp_layer_filter;
};

auto PhysicsWorld::create(Config const& config) noexcept
    -> std::expected<PhysicsWorld, PhysicsError> {

    PhysicsWorld result;
    result.impl_ = std::make_unique<Impl>();

    // Initialize Jolt Physics
    // (implementation details omitted)

    return result;
}

// ... implementation methods

} // namespace projectv::physics
```

---

## 5. PIMPL: Категорически запрещён

### 5.1 Когда PIMPL ЗАПРЕЩЁН

PIMPL запрещён для всех типов, которые:

1. **Обрабатываются в ECS системах**
  - Компоненты
  - Теги

2. **Итерируются в массивах**
  - Mesh vertices
  - Particle data
  - Voxel data

3. **Используются в SIMD операциях**
  - Math types (vec3, mat4, quat)
  - Bounding volumes

4. **Требуют trivially copyable**
  - GPU buffer data
  - Network packets

### 5.2 Математическое обоснование

Для типа $T$ с размером $|T|$ байт и $N$ экземпляров:

**Без PIMPL (direct data):**
$$T_{\text{access}} = N \cdot T_{\text{cache\_miss}} \cdot \frac{|T|}{L}$$

где $L$ — размер cache line (64 bytes).

**С PIMPL (pointer indirection):**
$$T_{\text{access}} = N \cdot T_{\text{cache\_miss}} \cdot (1 + \frac{|T|}{L})$$

Дополнительный cache miss на каждый элемент из-за pointer chasing.

**Penalty:** до **2× замедление** для hot path кода.

### 5.3 Концепт HotPathComponent

```cpp
// ProjectV.ECS.Concepts.cppm
export module ProjectV.ECS.Concepts;

import std;

export namespace projectv::ecs {

/// Концепт: Hot Path Component.
///
/// ## Requirements
/// - Trivially copyable — можно копировать memcpy
/// - Trivially destructible — нет деструктора
/// - Alignment ≤ 64 bytes — влезает в cache line
/// - No virtual functions — нет vtable overhead
///
/// ## Why?
/// Эти требования гарантируют:
/// - SoA layout без overhead
/// - SIMD-friendly data
/// - Cache-efficient iteration
export template<typename T>
concept HotPathComponent =
    std::is_trivially_copyable_v<T> &&
    std::is_trivially_destructible_v<T> &&
    alignof(T) <= 64 &&
    !std::is_polymorphic_v<T>;

/// Концепт: ECS Component (может иметь деструктор).
export template<typename T>
concept ECSComponent =
    std::is_trivially_copyable_v<T> &&
    alignof(T) <= 64 &&
    !std::is_polymorphic_v<T>;

/// Концепт: Tag Component (пустой, для marking entities).
export template<typename T>
concept TagComponent =
    std::is_empty_v<T> &&
    std::is_trivially_copyable_v<T>;

/// Static assertions для проверки компонентов
#define STATIC_ASSERT_HOT_PATH_COMPONENT(T) \
    static_assert(projectv::ecs::HotPathComponent<T>, \
        #T " must be trivially copyable, trivially destructible, " \
        "aligned to cache line, and non-polymorphic for hot path use")

} // namespace projectv::ecs
```

### 5.4 Примеры правильных компонентов

```cpp
// ProjectV.ECS.Components.cppm
export module ProjectV.ECS.Components;

import std;
import glm;
import ProjectV.ECS.Concepts;

export namespace projectv::ecs {

// ============================================================================
// HOT PATH COMPONENTS — PIMPL ЗАПРЕЩЁН
// ============================================================================

/// Transform component — обрабатывается каждый кадр.
/// Размер: 48 bytes (3× vec3 + padding)
export struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};  // 16 bytes
    glm::vec3 scale{1.0f};
    uint8_t dirty{0};
    uint8_t _padding[3];
};
STATIC_ASSERT_HOT_PATH_COMPONENT(TransformComponent);

/// Velocity component.
export struct VelocityComponent {
    glm::vec3 linear{0.0f};
    glm::vec3 angular{0.0f};
};
STATIC_ASSERT_HOT_PATH_COMPONENT(VelocityComponent);

/// Voxel chunk data component.
export struct VoxelChunkComponent {
    glm::ivec3 grid_position{0};
    uint32_t chunk_index{0};          ///< Index in ChunkStorage
    uint64_t version{0};
    uint8_t state{0};
    uint8_t flags{0};
    uint8_t _padding[6];
};
STATIC_ASSERT_HOT_PATH_COMPONENT(VoxelChunkComponent);

/// Mesh instance component (for rendering).
export struct MeshInstanceComponent {
    uint32_t mesh_id{0};
    uint32_t material_id{0};
    uint32_t vertex_buffer_offset{0};
    uint32_t index_buffer_offset{0};
    uint32_t vertex_count{0};
    uint32_t index_count{0};
};
STATIC_ASSERT_HOT_PATH_COMPONENT(MeshInstanceComponent);

/// Physics body reference component.
export struct PhysicsBodyComponent {
    uint64_t body_id{0};              ///< Jolt BodyID
    uint32_t collision_layer{0};
    uint32_t flags{0};
};
STATIC_ASSERT_HOT_PATH_COMPONENT(PhysicsBodyComponent);

/// Bounding box component.
export struct BoundingBoxComponent {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
};
STATIC_ASSERT_HOT_PATH_COMPONENT(BoundingBoxComponent);

// ============================================================================
// TAG COMPONENTS — пустые, для marking entities
// ============================================================================

/// Tag: entity is visible this frame.
export struct VisibleTag {};
static_assert(TagComponent<VisibleTag>);

/// Tag: entity needs cleanup.
export struct NeedsCleanupTag {};
static_assert(TagComponent<NeedsCleanupTag>);

/// Tag: entity is player-controlled.
export struct PlayerControlledTag {};
static_assert(TagComponent<PlayerControlledTag>);

// ============================================================================
// COLD PATH COMPONENTS — PIMPL опционален
// ============================================================================

/// Metadata component — используется редко.
export struct MetadataComponent {
    std::string name;                  // NOT trivially copyable
    std::string description;
    uint64_t created_at{0};
    uint64_t modified_at{0};
};
// NOT a HotPathComponent — std::string has destructor

/// Script component — Lua/WASM reference.
export struct ScriptComponent {
    uint32_t script_id{0};
    void* script_state{nullptr};       // Opaque pointer to script VM
    // NOT trivially destructible if script_state needs cleanup
};

} // namespace projectv::ecs
```

---

## 6. Запрет heap allocation в Hot Path

### 6.1 Правило

**В Hot Path коде (ECS systems, render loop, physics step) ЗАПРЕЩЕНО:**

1. `new` / `delete`
2. `std::make_unique` / `std::make_shared`
3. `std::vector::push_back` (с reallocation)
4. `std::string` конкатенация
5. Любой код, который может выделить память в куче

### 6.2 Концепт NoHeapAllocation

```cpp
// ProjectV.Core.Concepts.cppm
export module ProjectV.Core.Concepts;

import std;

export namespace projectv::core {

/// Концепт: функция не выделяет память в куче.
///
/// ## Usage
/// Помечать функции, которые должны быть allocation-free.
export template<typename F, typename... Args>
concept NoHeapAllocation =
    std::is_nothrow_invocable_v<F, Args...> &&
    requires(F&& f, Args&&... args) {
        // Проверка через static analysis (compiler-dependent)
        // В идеале использовать clang-tidy или static analyzer
        { std::forward<F>(f)(std::forward<Args>(args)...) } noexcept;
    };

/// Атрибут для документирования (C++26)
#define NO_HEAP_ALLOCATION \
    [[nodiscard, gnu::malloc, gnu::nothrow]]

/// Концепт: Allocator-aware type.
export template<typename T>
concept AllocatorAware = requires {
    typename T::allocator_type;
};

} // namespace projectv::core
```

### 6.3 Примеры Hot Path кода

```cpp
// ProjectV.ECS.Systems.TransformSystem.cppm
export module ProjectV.ECS.Systems.TransformSystem;

import std;
import glm;
import flecs;
import ProjectV.ECS.Components;
import ProjectV.ECS.Concepts;

export namespace projectv::ecs {

/// Transform System — Hot Path.
///
/// ## Constraints
/// - NO heap allocation
/// - NO syscalls
/// - NO blocking operations
/// - Time budget: 0.5 ms per frame
export class TransformSystem {
public:
    static auto register_system(flecs::world& world) noexcept -> flecs::system {
        return world.system<TransformComponent, VelocityComponent const>("TransformSystem")
            .kind(flecs::OnUpdate)
            .iter(update);
    }

private:
    /// Update callback — NO HEAP ALLOCATION.
    static auto update(
        flecs::iter& it,
        TransformComponent* transforms,
        VelocityComponent const* velocities
    ) noexcept -> void {
        ZoneScopedN("TransformSystem");  // Tracy zone

        float dt = it.delta_time();
        size_t count = it.count();

        // HOT PATH: линейный проход, SIMD-friendly
        for (size_t i = 0; i < count; ++i) {
            transforms[i].position += velocities[i].linear * dt;

            // Интеграция rotation (shortest path)
            glm::quat dq = glm::quat(velocities[i].angular * dt * 0.5f,
                                      glm::vec3(0.0f));
            transforms[i].rotation = glm::normalize(transforms[i].rotation * dq);

            transforms[i].dirty = 1;
        }
    }
};

} // namespace projectv::ecs
```

### 6.4 Arena Allocator для временных данных

Когда временные данные необходимы в hot path, используйте arena allocator:

```cpp
// ProjectV.Core.Memory.Arena.cppm
export module ProjectV.Core.Memory.Arena;

import std;

export namespace projectv::core::memory {

/// Linear Arena Allocator — O(1) allocate, NO individual free.
///
/// ## Usage
/// ```cpp
/// ArenaAllocator arena(64 * 1024);  // 64 KB scratch buffer
///
/// void hot_path_function(ArenaAllocator& arena) {
///     // Allocate temporary data
///     auto* temp_data = arena.allocate<glm::vec3>(1024);
///
///     // Use temp_data...
///
///     // Reset arena at end of frame (not individual free!)
///     arena.reset();
/// }
/// ```
///
/// ## Thread Safety
/// NOT thread-safe. Each thread should have its own arena.
///
/// ## Memory Model
/// - Pre-allocated buffer
/// - Linear bump allocation
/// - Reset frees all at once
export class ArenaAllocator {
public:
    /// Создаёт arena заданного размера.
    explicit ArenaAllocator(size_t size) noexcept
        : buffer_(static_cast<std::byte*>(std::aligned_alloc(64, size)))
        , capacity_(size)
        , offset_(0)
    {}

    ~ArenaAllocator() noexcept {
        std::free(buffer_);
    }

    ArenaAllocator(ArenaAllocator&&) noexcept = default;
    ArenaAllocator& operator=(ArenaAllocator&&) noexcept = default;
    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    /// Выделяет память — O(1), noexcept.
    /// NEVER returns nullptr (terminates on OOM).
    [[nodiscard]] auto allocate(size_t size, size_t alignment = 16) noexcept
        -> void* {

        size_t aligned_offset = align_up(offset_, alignment);
        size_t new_offset = aligned_offset + size;

        if (new_offset > capacity_) {
            // Arena overflow — critical error
            std::terminate();
        }

        offset_ = new_offset;
        return buffer_ + aligned_offset;
    }

    /// Выделяет массив типа T.
    template<typename T>
    [[nodiscard]] auto allocate(size_t count) noexcept -> T* {
        return static_cast<T*>(allocate(count * sizeof(T), alignof(T)));
    }

    /// Сбрасывает arena — O(1).
    /// Вся память становится доступна для переиспользования.
    auto reset() noexcept -> void {
        offset_ = 0;
    }

    /// Получает использованный размер.
    [[nodiscard]] auto used() const noexcept -> size_t {
        return offset_;
    }

    /// Получает общий размер.
    [[nodiscard]] auto capacity() const noexcept -> size_t {
        return capacity_;
    }

private:
    static constexpr auto align_up(size_t offset, size_t alignment) noexcept -> size_t {
        return (offset + alignment - 1) & ~(alignment - 1);
    }

    std::byte* buffer_;
    size_t capacity_;
    size_t offset_;
};

/// Thread-local scratch arena для hot path.
/// Размер: 1 MB per thread.
export thread_local inline ArenaAllocator scratch_arena{1024 * 1024};

/// Получает scratch arena для текущего потока.
[[nodiscard]] export auto get_scratch_arena() noexcept -> ArenaAllocator& {
    return scratch_arena;
}

} // namespace projectv::core::memory
```

---

## 7. Fast PIMPL Pattern

### 7.1 Когда применим

Fast PIMPL — альтернатива для случаев, когда:

- Нужна изоляция реализации
- Но размер Impl известен и фиксирован
- И накладные расходы unique_ptr недопустимы

### 7.2 Реализация

```cpp
// ProjectV.Core.FastPimpl.cppm
export module ProjectV.Core.FastPimpl;

import std;

export namespace projectv::core {

/// Fast PIMPL — хранит Impl inline вместо heap allocation.
///
/// ## Advantages
/// - No heap allocation
/// - No pointer indirection (same cache line)
/// - Known memory footprint
///
/// ## Disadvantages
/// - Fixed size
/// - Requires manual alignment
/// - Header must know size (breaks complete encapsulation)
///
/// ## Usage
/// ```cpp
/// class MyType {
///     // 64 bytes storage, 16-byte aligned
///     FastPimpl<64, 16> impl_;
/// };
/// ```
export template<size_t Size, size_t Alignment = alignof(std::max_align_t)>
class FastPimpl {
public:
    FastPimpl() noexcept = default;

    /// Конструктор от Impl типа.
    template<typename T, typename... Args>
        requires (sizeof(T) <= Size && alignof(T) <= Alignment)
    explicit FastPimpl(std::in_place_type_t<T>, Args&&... args) noexcept {
        new (storage_) T(std::forward<Args>(args)...);
    }

    FastPimpl(FastPimpl const&) = delete;
    FastPimpl& operator=(FastPimpl const&) = delete;

    FastPimpl(FastPimpl&& other) noexcept {
        std::memcpy(storage_, other.storage_, Size);
        std::memset(other.storage_, 0, Size);
    }

    FastPimpl& operator=(FastPimpl&& other) noexcept {
        std::memcpy(storage_, other.storage_, Size);
        std::memset(other.storage_, 0, Size);
        return *this;
    }

    ~FastPimpl() noexcept = default;

    /// Получает указатель на Impl.
    template<typename T>
        requires (sizeof(T) <= Size && alignof(T) <= Alignment)
    [[nodiscard]] auto get() noexcept -> T* {
        return std::launder(reinterpret_cast<T*>(storage_));
    }

    template<typename T>
        requires (sizeof(T) <= Size && alignof(T) <= Alignment)
    [[nodiscard]] auto get() const noexcept -> T const* {
        return std::launder(reinterpret_cast<T const*>(storage_));
    }

    /// Destruct текущий Impl.
    template<typename T>
    auto destruct() noexcept -> void {
        get<T>()->~T();
    }

private:
    alignas(Alignment) std::byte storage_[Size];
};

} // namespace projectv::core
```

### 7.3 Пример использования

```cpp
// Пример: SmallSystem с Fast PIMPL

export class SmallSystem {
public:
    SmallSystem() noexcept {
        // Конструируем Impl inline
        new (impl_.get<Impl>()) Impl{};
    }

    ~SmallSystem() noexcept {
        impl_.destruct<Impl>();
    }

    auto do_work() noexcept -> void {
        impl_.get<Impl>()->do_work_internal();
    }

private:
    struct Impl {
        int data[4];
        void* handle;

        auto do_work_internal() noexcept -> void {
            // Implementation
        }
    };

    // 64 bytes достаточно для Impl
    static_assert(sizeof(Impl) <= 64);

    FastPimpl<64, 16> impl_;
};
```

---

## 8. Правила и Checklist

### 8.1 Checklist для нового типа

При создании нового типа ответьте на вопросы:

1. **Будет ли тип обрабатываться в ECS system?**
  - Да → PIMPL ЗАПРЕЩЁН, используйте trivially copyable struct

2. **Тип инкапсулирует внешнюю C++ библиотеку?**
  - Да → PIMPL ОБЯЗАТЕЛЕН

3. **Тип будет итерироваться в массивах по тысячам элементов?**
  - Да → PIMPL ЗАПРЕЩЁН

4. **Тип создаётся один раз при старте?**
  - Да → PIMPL РЕКОМЕНДОВАН

5. **Тип содержит std::string, std::vector или другие heap-allocated данные?**
  - Да → либо PIMPL, либо вынести в отдельный cold component

### 8.2 Static Assertions

Добавьте в каждый компонент:

```cpp
// Для hot path компонентов
STATIC_ASSERT_HOT_PATH_COMPONENT(MyComponent);

// Для обычных ECS компонентов
static_assert(projectv::ecs::ECSComponent<MyComponent>);

// Для tag компонентов
static_assert(projectv::ecs::TagComponent<MyTag>);
```

### 8.3 Clang-Tidy Rules

```yaml
# .clang-tidy
Checks: >
  -*,
  performance-*,
  cppcoreguidelines-*,
  -cppcoreguidelines-avoid-magic-numbers,
  -cppcoreguidelines-pro-bounds-pointer-arithmetic

# Запрет virtual functions в components
WarnOnVirtualFunctionsInComponents: true

# Запрет heap allocation в hot path
WarnOnHeapAllocationInHotPath: true
```

---

## 9. Таблица решений

| Тип                     | PIMPL? | Heap? | Rationale                      |
|-------------------------|--------|-------|--------------------------------|
| `TransformComponent`    | ❌      | ❌     | Hot path, ECS component        |
| `VelocityComponent`     | ❌      | ❌     | Hot path, ECS component        |
| `VoxelChunkComponent`   | ❌      | ❌     | Hot path, mass iteration       |
| `MeshInstanceComponent` | ❌      | ❌     | Hot path, rendering data       |
| `PhysicsBodyComponent`  | ❌      | ❌     | Hot path, physics data         |
| `glm::vec3`             | ❌      | ❌     | Value type, SIMD               |
| `glm::mat4`             | ❌      | ❌     | Value type, SIMD               |
| `BoundingBox`           | ❌      | ❌     | Value type, trivially copyable |
| `RenderDevice`          | ✅      | ✅     | Vulkan wrapper, system type    |
| `PhysicsWorld`          | ✅      | ✅     | Jolt wrapper, system type      |
| `ECSWorld`              | ✅      | ✅     | Flecs wrapper, system type     |
| `AudioEngine`           | ✅      | ✅     | miniaudio wrapper, system type |
| `PlatformSubsystem`     | ✅      | ✅     | SDL wrapper, system type       |
| `Config`                | ⚠️     | ✅     | Cold path, optional            |
| `AssetMetadata`         | ⚠️     | ✅     | Cold path, optional            |
| `std::string`           | N/A    | ✅     | Not a component                |
| `std::vector`           | N/A    | ✅     | Not a component                |

---

## 10. Требования соответствия

### 10.1 Обязательные требования

1. Все ECS компоненты ДОЛЖНЫ быть `trivially copyable`
2. Все hot path типы ДОЛЖНЫ иметь `alignof(T) <= 64`
3. Все system типы (wrappers) ДОЛЖНЫ использовать PIMPL
4. В hot path коде ЗАПРЕЩЕНЫ heap allocations

### 10.2 Запрещённые практики

1. `std::unique_ptr<Impl>` в ECS компонентах
2. `std::string` в hot path компонентах
3. `std::vector` в hot path компонентах
4. `virtual` функции в компонентах
5. `new`/`delete` в ECS systems
6. PIMPL для математических типов

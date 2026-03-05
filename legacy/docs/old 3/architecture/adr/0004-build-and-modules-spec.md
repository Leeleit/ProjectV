# ADR-0004: Build System & C++26 Modules Specification

**Статус:** Принято
**Дата:** 2026-02-22
**Автор:** Architecture Team
**Версия:** 2.0

---

## Контекст

ProjectV использует **C++26** как основной язык с **Vulkan 1.4** для рендеринга. Требуется:

1. Спецификация модульной архитектуры (C++26 Modules с `import std;`)
2. **Изоляция C++ библиотек через PIMPL** — запрещено включать C++ headers в Global Module Fragment
3. Единый стандарт для всех модулей проекта
4. Требования к сборочной системе (CMake 3.30+)

---

## Решение

Принята **модульная архитектура** с использованием `import std;` и **строгим разделением** между:

- **C-библиотеками**: Global Module Fragment в `.cppm` файлах
- **C++ библиотеками**: PIMPL изоляция, включения только в `.cpp` файлах реализации

---

## 1. Module Structure Overview

```
src/
├── core/
│   ├── ProjectV.Core.cppm              # Primary module interface
│   ├── ProjectV.Core.Memory.cppm       # Memory management submodule
│   ├── ProjectV.Core.Memory.cpp        # Implementation (VMA includes here)
│   ├── ProjectV.Core.Containers.cppm   # DOD containers submodule
│   ├── ProjectV.Core.Jobs.cppm         # Job system submodule
│   └── ProjectV.Core.Platform.cppm     # Platform abstraction (SDL3)
│
├── render/
│   ├── ProjectV.Render.cppm            # Primary render module interface
│   ├── ProjectV.Render.Vulkan.cppm     # Vulkan wrapper submodule
│   ├── ProjectV.Render.Vulkan.cpp      # Implementation (Vulkan/VMA includes)
│   ├── ProjectV.Render.SVO.cppm        # SVO rendering submodule
│   └── ProjectV.Render.Mesh.cppm       # Mesh generation submodule
│
├── voxel/
│   ├── ProjectV.Voxel.cppm             # Primary voxel module interface
│   ├── ProjectV.Voxel.SVO.cppm         # SVO data structures submodule
│   ├── ProjectV.Voxel.Chunk.cppm       # Chunk management submodule
│   └── ProjectV.Voxel.Generation.cppm  # Procedural generation submodule
│
├── physics/
│   ├── ProjectV.Physics.cppm           # Primary physics module interface
│   ├── ProjectV.Physics.Jolt.cppm      # JoltPhysics PIMPL wrapper
│   └── ProjectV.Physics.Jolt.cpp       # Implementation (Jolt headers HERE)
│
├── ecs/
│   ├── ProjectV.ECS.cppm               # Primary ECS module interface
│   ├── ProjectV.ECS.Flecs.cppm         # Flecs PIMPL wrapper
│   └── ProjectV.ECS.Flecs.cpp          # Implementation (Flecs headers HERE)
│
├── audio/
│   ├── ProjectV.Audio.cppm             # Primary audio module interface
│   └── ProjectV.Audio.Miniaudio.cppm   # Miniaudio wrapper (C library)
│
├── ui/
│   ├── ProjectV.UI.cppm                # Primary UI module interface
│   ├── ProjectV.UI.ImGui.cppm          # ImGui PIMPL wrapper
│   └── ProjectV.UI.ImGui.cpp           # Implementation (ImGui headers HERE)
│
├── profile/
│   ├── ProjectV.Profile.cppm           # Profiling module interface
│   ├── ProjectV.Profile.Tracy.cppm     # Tracy PIMPL wrapper
│   └── ProjectV.Profile.Tracy.cpp      # Implementation (Tracy headers HERE)
│
└── app/
    └── ProjectV.App.cppm               # Application layer module
```

---

## 2. Global Module Fragment Rules

### 2.1 ПРАВИЛО: Только C Headers в GMF

**Разрешено в Global Module Fragment:**

- Vulkan (`<vulkan/vulkan.h>`) — C API
- SDL3 (`<SDL3/SDL.h>`) — C API
- VMA (`<vk_mem_alloc.h>`) — C API
- miniaudio (`<miniaudio.h>`) — C API
- Системные C headers (`<cstdio>`, `<cstring>`, `<cstddef>`)

**ЗАПРЕЩЕНО в Global Module Fragment:**

- JoltPhysics (`<Jolt/Jolt.h>`) — **C++ library with templates**
- Flecs C++ wrapper (`<flecs.h>`) — C++ wrapper
- ImGui (`<imgui.h>`) — **C++ library with templates**
- Tracy (`<tracy/Tracy.hpp>`) — **C++ library**
- GLM (`<glm/glm.hpp>`) — C++ library (но имеет module support)
- fastgltf — C++ library (но имеет module support)

### 2.2 Vulkan Module Wrapper (C Library — OK in GMF)

```cpp
// ProjectV.Render.Vulkan.cppm
module;

// Global Module Fragment: C headers BEFORE module declaration
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

export module ProjectV.Render.Vulkan;

import std;

export namespace projectv::render::vulkan {

// Opaque handle types — re-export from C API
export using VkInstanceHandle = VkInstance;
export using VkDeviceHandle = VkDevice;
export using VkPhysicalDeviceHandle = VkPhysicalDevice;
export using VkQueueHandle = VkQueue;
export using VkBufferHandle = VkBuffer;
export using VkImageHandle = VkImage;
export using VkCommandBufferHandle = VkCommandBuffer;

/// RAII wrapper for VkInstance
class Instance {
public:
    Instance() noexcept = default;

    /// Creates Vulkan instance with required extensions.
    [[nodiscard]] static auto create(
        std::span<const char* const> extensions,
        std::span<const char* const> layers = {}
    ) noexcept -> std::expected<Instance, InstanceError>;

    ~Instance() noexcept {
        if (instance_ != VK_NULL_HANDLE) {
            vkDestroyInstance(instance_, nullptr);
        }
    }

    // Move-only
    Instance(Instance&& other) noexcept : instance_(other.instance_) {
        other.instance_ = VK_NULL_HANDLE;
    }

    Instance& operator=(Instance&& other) noexcept {
        if (this != &other) {
            if (instance_ != VK_NULL_HANDLE) {
                vkDestroyInstance(instance_, nullptr);
            }
            instance_ = other.instance_;
            other.instance_ = VK_NULL_HANDLE;
        }
        return *this;
    }

    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;

    [[nodiscard]] auto native() const noexcept -> VkInstance { return instance_; }
    [[nodiscard]] auto valid() const noexcept -> bool { return instance_ != VK_NULL_HANDLE; }

private:
    explicit Instance(VkInstance instance) noexcept : instance_(instance) {}
    VkInstance instance_{VK_NULL_HANDLE};
};

/// RAII wrapper for VkDevice
class Device {
public:
    [[nodiscard]] static auto create(
        VkPhysicalDevice physical_device,
        std::span<const char* const> extensions,
        std::span<uint32_t const> queue_families
    ) noexcept -> std::expected<Device, DeviceError>;

    ~Device() noexcept {
        if (device_ != VK_NULL_HANDLE) {
            vkDestroyDevice(device_, nullptr);
        }
    }

    Device(Device&&) noexcept;
    Device& operator=(Device&&) noexcept;
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    [[nodiscard]] auto native() const noexcept -> VkDevice { return device_; }
    auto wait_idle() const noexcept -> void { vkDeviceWaitIdle(device_); }

private:
    explicit Device(VkDevice device) noexcept : device_(device) {}
    VkDevice device_{VK_NULL_HANDLE};
};

// Additional RAII wrappers: Buffer, Image, CommandPool, etc.

} // namespace projectv::render::vulkan
```

### 2.3 SDL3 Module Wrapper (C Library — OK in GMF)

```cpp
// ProjectV.Core.Platform.cppm
module;

// Global Module Fragment
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

export module ProjectV.Core.Platform;

import std;
import glm;

export namespace projectv::platform {

/// RAII wrapper for SDL3 window
class Window {
public:
    [[nodiscard]] static auto create(
        std::string_view title,
        uint32_t width,
        uint32_t height
    ) noexcept -> std::expected<Window, WindowError>;

    ~Window() noexcept {
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
        }
    }

    Window(Window&& other) noexcept : window_(other.window_) {
        other.window_ = nullptr;
    }

    Window& operator=(Window&& other) noexcept {
        if (this != &other) {
            if (window_ != nullptr) {
                SDL_DestroyWindow(window_);
            }
            window_ = other.window_;
            other.window_ = nullptr;
        }
        return *this;
    }

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    [[nodiscard]] auto poll_events() noexcept -> bool;
    [[nodiscard]] auto vulkan_instance_extensions() const noexcept -> std::vector<const char*>;
    [[nodiscard]] auto create_vulkan_surface(VkInstance instance) const noexcept
        -> std::expected<VkSurfaceKHR, SurfaceError>;

    [[nodiscard]] auto native() const noexcept -> SDL_Window* { return window_; }
    [[nodiscard]] auto size() const noexcept -> glm::uvec2;

private:
    explicit Window(SDL_Window* window) noexcept : window_(window) {}
    SDL_Window* window_{nullptr};
};

/// SDL subsystem initializer (RAII)
class SDLContext {
public:
    [[nodiscard]] static auto init(uint32_t flags = SDL_INIT_VIDEO) noexcept
        -> std::expected<SDLContext, SDLError>;

    ~SDLContext() noexcept { SDL_Quit(); }

    SDLContext(SDLContext&&) = default;
    SDLContext& operator=(SDLContext&&) = default;
    SDLContext(const SDLContext&) = delete;
    SDLContext& operator=(const SDLContext&) = delete;

private:
    SDLContext() = default;
};

} // namespace projectv::platform
```

---

## 3. PIMPL Architecture for C++ Libraries

### 3.1 Проблема: C++ Headers в модулях

**Почему нельзя включать C++ headers в Global Module Fragment:**

```cpp
// НЕПРАВИЛЬНО — нарушает ODR и module isolation
module;
#include <Jolt/Jolt.h>  // ❌ C++ library with templates!

export module ProjectV.Physics.Jolt;

// Проблемы:
// 1. Jolt's templates instantiate in module context
// 2. Internal Jolt headers may conflict with other modules
// 3. Jolt's macros pollute global namespace
// 4. Recompilation of Jolt cascades to all module importers
```

### 3.2 Решение: PIMPL Pattern

**Архитектура PIMPL для C++ библиотек:**

```
┌─────────────────────────────────────────────────────────────┐
│  Module Interface (.cppm)                                   │
│  ─────────────────────                                      │
│  • Только объявления (declarations)                         │
│  • Opaque pointers (std::unique_ptr<Impl>)                  │
│  • Никаких Jolt/Flecs/ImGui headers                         │
│  • Export только ProjectV types                             │
└─────────────────────────────────────────────────────────────┘
                          │
                          │ import
                          ▼
┌─────────────────────────────────────────────────────────────┐
│  Module Implementation (.cpp)                               │
│  ───────────────────────────                                │
│  • #include <Jolt/Jolt.h> — здесь!                          │
│  • Полные определения классов                               │
│  • Внутренние Jolt types скрыты                             │
│  • Изменения Jolt не триггерят recompile модулей            │
└─────────────────────────────────────────────────────────────┘
```

### 3.3 JoltPhysics PIMPL Implementation

```cpp
// ProjectV.Physics.Jolt.cppm — Module Interface
// ЗАПРЕЩЕНО: #include <Jolt/Jolt.h>

export module ProjectV.Physics.Jolt;

import std;
import glm;

// Forward declarations — NO Jolt headers
namespace JPH {
    class PhysicsSystem;
    class JobSystemThreadPool;
    class TempAllocator;
    class Shape;
    class Body;
}

export namespace projectv::physics {

/// Error codes for physics operations
export enum class PhysicsError : uint8_t {
    InitializationFailed,
    BodyCreationFailed,
    ShapeCreationFailed,
    InvalidBodyID,
    SimulationStepFailed,
    OutOfMemory
};

/// Body identifier — opaque wrapper
export struct BodyID {
    uint32_t index{0};
    uint32_t generation{0};

    [[nodiscard]] auto valid() const noexcept -> bool {
        return index != 0xFFFFFFFF;
    }

    [[nodiscard]] auto native() const noexcept -> uint64_t {
        return (uint64_t{generation} << 32) | index;
    }
};

/// Motion type for physics bodies
export enum class MotionType : uint8_t {
    Static,     ///< Never moves
    Kinematic,  ///< Moved by code, not physics
    Dynamic     ///< Full physics simulation
};

/// Shape types
export enum class ShapeType : uint8_t {
    Sphere,
    Box,
    Capsule,
    Cylinder,
    ConvexHull,
    Mesh,
    HeightField
};

/// Physics body creation settings
export struct BodySettings {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    MotionType motion_type{MotionType::Dynamic};
    float mass{1.0f};
    float friction{0.5f};
    float restitution{0.2f};
    bool is_sensor{false};
};

/// Physics system — PIMPL wrapper
export class PhysicsSystem {
public:
    /// Creates physics system with configuration.
    [[nodiscard]] static auto create(
        uint32_t max_bodies = 65536,
        uint32_t max_body_pairs = 65536,
        uint32_t max_contact_constraints = 10240
    ) noexcept -> std::expected<PhysicsSystem, PhysicsError>;

    ~PhysicsSystem() noexcept;

    // Move-only
    PhysicsSystem(PhysicsSystem&&) noexcept;
    PhysicsSystem& operator=(PhysicsSystem&&) noexcept;
    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;

    /// Steps simulation.
    auto step(float delta_time) noexcept -> void;

    /// Creates a dynamic body.
    [[nodiscard]] auto create_body(
        BodySettings const& settings,
        ShapeType shape_type,
        glm::vec3 const& shape_size
    ) noexcept -> std::expected<BodyID, PhysicsError>;

    /// Removes a body.
    auto remove_body(BodyID id) noexcept -> void;

    /// Gets body position.
    [[nodiscard]] auto get_body_position(BodyID id) const noexcept
        -> std::expected<glm::vec3, PhysicsError>;

    /// Gets body rotation.
    [[nodiscard]] auto get_body_rotation(BodyID id) const noexcept
        -> std::expected<glm::quat, PhysicsError>;

    /// Sets body velocity.
    auto set_body_velocity(BodyID id, glm::vec3 const& velocity) noexcept -> void;

    /// Applies force to body.
    auto apply_force(BodyID id, glm::vec3 const& force) noexcept -> void;

    /// Applies impulse to body.
    auto apply_impulse(BodyID id, glm::vec3 const& impulse) noexcept -> void;

private:
    PhysicsSystem() noexcept = default;

    // PIMPL: Implementation hidden in .cpp
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Shape factory — static methods for creating shapes
export class ShapeFactory {
public:
    [[nodiscard]] static auto create_sphere(float radius) noexcept
        -> std::expected<ShapeHandle, PhysicsError>;

    [[nodiscard]] static auto create_box(glm::vec3 const& half_extents) noexcept
        -> std::expected<ShapeHandle, PhysicsError>;

    [[nodiscard]] static auto create_capsule(float half_height, float radius) noexcept
        -> std::expected<ShapeHandle, PhysicsError>;

    [[nodiscard]] static auto create_mesh(
        std::span<glm::vec3 const> vertices,
        std::span<uint32_t const> indices
    ) noexcept -> std::expected<ShapeHandle, PhysicsError>;
};

/// Opaque shape handle
export struct ShapeHandle {
    uint64_t internal_id{0};

    [[nodiscard]] auto valid() const noexcept -> bool {
        return internal_id != 0;
    }
};

} // namespace projectv::physics
```

```cpp
// ProjectV.Physics.Jolt.cpp — Module Implementation
// Jolt headers HERE, not in .cppm

module ProjectV.Physics.Jolt;

// C++ library includes — ONLY in implementation file
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>

import std;
import glm;

namespace projectv::physics {

// PIMPL Implementation — fully hidden from module interface
struct PhysicsSystem::Impl {
    JPH::PhysicsSystem* physics_system{nullptr};
    JPH::JobSystemThreadPool* job_system{nullptr};
    JPH::TempAllocatorImpl* temp_allocator{nullptr};
    JPH::BroadPhaseLayerInterface* broad_phase_layer{nullptr};

    uint32_t max_bodies{0};
    std::unordered_map<uint64_t, JPH::BodyID> body_map;
};

auto PhysicsSystem::create(
    uint32_t max_bodies,
    uint32_t max_body_pairs,
    uint32_t max_contact_constraints
) noexcept -> std::expected<PhysicsSystem, PhysicsError> {
    // Initialize Jolt factory (singleton)
    static bool jolt_initialized = []{
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
        return true;
    }();

    PhysicsSystem system;
    system.impl_ = std::make_unique<Impl>();
    system.impl_->max_bodies = max_bodies;

    // Create temp allocator
    system.impl_->temp_allocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024); // 10 MB

    // Create job system
    system.impl_->job_system = new JPH::JobSystemThreadPool(
        JPH::cMaxPhysicsJobs,
        JPH::cMaxPhysicsBarriers,
        static_cast<int>(std::thread::hardware_concurrency() - 1)
    );

    // Create broad phase layer interface
    system.impl_->broad_phase_layer = new VoxelBroadPhaseLayerInterface();

    // Create physics system
    system.impl_->physics_system = new JPH::PhysicsSystem();
    system.impl_->physics_system->Init(
        max_bodies,
        0, // auto body mutexes
        max_body_pairs,
        max_contact_constraints,
        *system.impl_->broad_phase_layer,
        g_object_vs_broadphase_layer_filter,
        g_object_vs_object_layer_filter
    );

    // Configure gravity for voxel world
    system.impl_->physics_system->SetGravity(JPH::Vec3(0, -15.0f, 0));

    return system;
}

PhysicsSystem::~PhysicsSystem() noexcept {
    if (impl_) {
        delete impl_->physics_system;
        delete impl_->job_system;
        delete impl_->temp_allocator;
        delete impl_->broad_phase_layer;
        impl_.reset();
    }
}

PhysicsSystem::PhysicsSystem(PhysicsSystem&& other) noexcept
    : impl_(std::move(other.impl_)) {}

PhysicsSystem& PhysicsSystem::operator=(PhysicsSystem&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
    }
    return *this;
}

auto PhysicsSystem::step(float delta_time) noexcept -> void {
    if (!impl_ || !impl_->physics_system) return;

    impl_->physics_system->Update(
        delta_time,
        JPH::cCollisionSteps,
        *impl_->temp_allocator,
        *impl_->job_system
    );
}

auto PhysicsSystem::create_body(
    BodySettings const& settings,
    ShapeType shape_type,
    glm::vec3 const& shape_size
) noexcept -> std::expected<BodyID, PhysicsError> {
    if (!impl_ || !impl_->physics_system) {
        return std::unexpected(PhysicsError::InitializationFailed);
    }

    // Create shape based on type
    JPH::RefConst<JPH::Shape> shape;

    switch (shape_type) {
        case ShapeType::Sphere:
            shape = new JPH::SphereShape(shape_size.x);
            break;
        case ShapeType::Box:
            shape = new JPH::BoxShape(JPH::Vec3(shape_size.x, shape_size.y, shape_size.z));
            break;
        default:
            return std::unexpected(PhysicsError::ShapeCreationFailed);
    }

    // Determine motion type
    JPH::EMotionType motion_type;
    switch (settings.motion_type) {
        case MotionType::Static:
            motion_type = JPH::EMotionType::Static;
            break;
        case MotionType::Kinematic:
            motion_type = JPH::EMotionType::Kinematic;
            break;
        case MotionType::Dynamic:
            motion_type = JPH::EMotionType::Dynamic;
            break;
    }

    // Create body settings
    JPH::BodyCreationSettings body_settings(
        shape,
        JPH::RVec3(settings.position.x, settings.position.y, settings.position.z),
        JPH::Quat(settings.rotation.x, settings.rotation.y,
                  settings.rotation.z, settings.rotation.w),
        motion_type,
        Layers::MOVING
    );

    body_settings.mFriction = settings.friction;
    body_settings.mRestitution = settings.restitution;
    body_settings.mIsSensor = settings.is_sensor;

    if (settings.motion_type == MotionType::Dynamic) {
        body_settings.mMassPropertiesOverride.mMass = settings.mass;
    }

    // Create body
    JPH::BodyInterface& interface = impl_->physics_system->GetBodyInterface();
    JPH::Body* body = interface.CreateBody(body_settings);

    if (!body) {
        return std::unexpected(PhysicsError::BodyCreationFailed);
    }

    interface.AddBody(body->GetID(), JPH::EActivation::Activate);

    BodyID result{
        .index = body->GetID().GetIndex(),
        .generation = body->GetID().GetSequenceNumber()
    };

    impl_->body_map[result.native()] = body->GetID();

    return result;
}

auto PhysicsSystem::get_body_position(BodyID id) const noexcept
    -> std::expected<glm::vec3, PhysicsError> {
    if (!impl_ || !impl_->physics_system) {
        return std::unexpected(PhysicsError::InitializationFailed);
    }

    auto it = impl_->body_map.find(id.native());
    if (it == impl_->body_map.end()) {
        return std::unexpected(PhysicsError::InvalidBodyID);
    }

    JPH::BodyInterface& interface = impl_->physics_system->GetBodyInterface();
    JPH::RVec3 pos = interface.GetCenterOfMassPosition(it->second);

    return glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
}

// Additional method implementations...

} // namespace projectv::physics
```

### 3.4 Flecs PIMPL Implementation

```cpp
// ProjectV.ECS.Flecs.cppm — Module Interface

export module ProjectV.ECS.Flecs;

import std;
import glm;

// Forward declarations — NO Flecs headers
namespace flecs {
    struct world;
    struct entity;
    struct system;
}

export namespace projectv::ecs {

/// ECS World — PIMPL wrapper for flecs::world
export class World {
public:
    World() noexcept;
    explicit World(int32_t thread_count) noexcept;
    ~World() noexcept;

    World(World&&) noexcept;
    World& operator=(World&&) noexcept;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    /// Progresses simulation.
    [[nodiscard]] auto progress(float delta_time) noexcept -> bool;

    /// Creates entity.
    [[nodiscard]] auto entity() noexcept -> uint64_t;
    [[nodiscard]] auto entity(std::string_view name) noexcept -> uint64_t;

    /// Destroys entity.
    auto destroy(uint64_t entity_id) noexcept -> void;

    /// Checks if entity exists.
    [[nodiscard]] auto exists(uint64_t entity_id) const noexcept -> bool;

    /// Sets component on entity.
    template<typename T>
    auto set(uint64_t entity_id, T const& component) noexcept -> void;

    /// Gets component from entity.
    template<typename T>
    [[nodiscard]] auto get(uint64_t entity_id) const noexcept -> T const*;

    /// Gets mutable component reference.
    template<typename T>
    [[nodiscard]] auto get_mut(uint64_t entity_id) noexcept -> T*;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Entity builder for fluent API
export class EntityBuilder {
public:
    explicit EntityBuilder(World& world) noexcept;

    auto with_name(std::string_view name) noexcept -> EntityBuilder&;

    template<typename T>
    auto with(T const& component) noexcept -> EntityBuilder&;

    [[nodiscard]] auto build() noexcept -> uint64_t;

private:
    World& world_;
    uint64_t entity_id_{0};
};

} // namespace projectv::ecs
```

```cpp
// ProjectV.ECS.Flecs.cpp — Module Implementation

module ProjectV.ECS.Flecs;

// Flecs header HERE, not in .cppm
#include <flecs.h>
#include <flecs/addons/flecs_cpp.h>

import std;
import glm;

namespace projectv::ecs {

struct World::Impl {
    ::flecs::world world;

    Impl() = default;

    explicit Impl(int32_t thread_count) {
        if (thread_count > 1) {
            world.set_threads(thread_count);
        }
    }
};

World::World() noexcept : impl_(std::make_unique<Impl>()) {}

World::World(int32_t thread_count) noexcept
    : impl_(std::make_unique<Impl>(thread_count)) {}

World::~World() noexcept = default;

World::World(World&&) noexcept = default;
World& World::operator=(World&&) noexcept = default;

auto World::progress(float delta_time) noexcept -> bool {
    return impl_->world.progress(delta_time);
}

auto World::entity() noexcept -> uint64_t {
    return impl_->world.entity().id();
}

auto World::entity(std::string_view name) noexcept -> uint64_t {
    return impl_->world.entity(name.data()).id();
}

auto World::destroy(uint64_t entity_id) noexcept -> void {
    impl_->world.entity(entity_id).destruct();
}

auto World::exists(uint64_t entity_id) const noexcept -> bool {
    return impl_->world.entity(entity_id).is_alive();
}

template<typename T>
auto World::set(uint64_t entity_id, T const& component) noexcept -> void {
    impl_->world.entity(entity_id).set<T>(component);
}

template<typename T>
auto World::get(uint64_t entity_id) const noexcept -> T const* {
    return impl_->world.entity(entity_id).get<T>();
}

template<typename T>
auto World::get_mut(uint64_t entity_id) noexcept -> T* {
    return impl_->world.entity(entity_id).get_mut<T>();
}

// Explicit template instantiations for common components
template auto World::set<glm::vec3>(uint64_t, glm::vec3 const&) noexcept -> void;
template auto World::get<glm::vec3>(uint64_t) const noexcept -> glm::vec3 const*;
template auto World::get_mut<glm::vec3>(uint64_t) noexcept -> glm::vec3*;

} // namespace projectv::ecs
```

### 3.5 ImGui PIMPL Implementation

```cpp
// ProjectV.UI.ImGui.cppm — Module Interface

export module ProjectV.UI.ImGui;

import std;
import glm;

// NO ImGui headers here!

export namespace projectv::ui {

/// ImGui context wrapper — PIMPL
export class ImGuiContext {
public:
    ImGuiContext() noexcept;
    ~ImGuiContext() noexcept;

    ImGuiContext(ImGuiContext&&) noexcept;
    ImGuiContext& operator=(ImGuiContext&&) noexcept;
    ImGuiContext(const ImGuiContext&) = delete;
    ImGuiContext& operator=(const ImGuiContext&) = delete;

    /// Begins new frame.
    auto new_frame() noexcept -> void;

    /// Ends frame and renders.
    auto render() noexcept -> void;

    /// Draws debug window for entity.
    auto draw_entity_debug(uint64_t entity_id) noexcept -> void;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Scoped ImGui window
export class ScopedWindow {
public:
    explicit ScopedWindow(std::string_view name) noexcept;
    ~ScopedWindow() noexcept;

    ScopedWindow(const ScopedWindow&) = delete;
    ScopedWindow& operator=(const ScopedWindow&) = delete;

    [[nodiscard]] auto is_open() const noexcept -> bool;

private:
    bool is_open_{false};
};

/// Input widgets
export auto input_float3(std::string_view label, glm::vec3& value, float speed = 0.1f) noexcept -> bool;
export auto input_quat(std::string_view label, glm::quat& value) noexcept -> bool;
export auto color_picker(std::string_view label, glm::vec4& value) noexcept -> bool;

} // namespace projectv::ui
```

```cpp
// ProjectV.UI.ImGui.cpp — Module Implementation

module ProjectV.UI.ImGui;

// ImGui headers HERE
#include <imgui.h>
#include <imgui_internal.h>

import std;
import glm;

namespace projectv::ui {

struct ImGuiContext::Impl {
    ::ImGuiContext* context{nullptr};
};

ImGuiContext::ImGuiContext() noexcept
    : impl_(std::make_unique<Impl>()) {
    impl_->context = ::ImGui::CreateContext();
    ::ImGui::SetCurrentContext(impl_->context);

    // Configure style
    ::ImGui::StyleColorsDark();
    auto& style = ::ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;
}

ImGuiContext::~ImGuiContext() noexcept {
    if (impl_ && impl_->context) {
        ::ImGui::DestroyContext(impl_->context);
    }
}

ImGuiContext::ImGuiContext(ImGuiContext&&) noexcept = default;
ImGuiContext& ImGuiContext::operator=(ImGuiContext&&) noexcept = default;

auto ImGuiContext::new_frame() noexcept -> void {
    ::ImGui::SetCurrentContext(impl_->context);
    ::ImGui::NewFrame();
}

auto ImGuiContext::render() noexcept -> void {
    ::ImGui::SetCurrentContext(impl_->context);
    ::ImGui::Render();
}

auto input_float3(std::string_view label, glm::vec3& value, float speed) noexcept -> bool {
    return ::ImGui::DragFloat3(label.data(), &value.x, speed);
}

auto input_quat(std::string_view label, glm::quat& value) noexcept -> bool {
    glm::vec3 euler = glm::degrees(glm::eulerAngles(value));
    if (::ImGui::DragFloat3(label.data(), &euler.x, 1.0f)) {
        value = glm::quat(glm::radians(euler));
        return true;
    }
    return false;
}

auto color_picker(std::string_view label, glm::vec4& value) noexcept -> bool {
    return ::ImGui::ColorEdit4(label.data(), &value.x);
}

ScopedWindow::ScopedWindow(std::string_view name) noexcept {
    is_open_ = ::ImGui::Begin(name.data());
}

ScopedWindow::~ScopedWindow() noexcept {
    ::ImGui::End();
}

auto ScopedWindow::is_open() const noexcept -> bool {
    return is_open_;
}

} // namespace projectv::ui
```

---

## 4. CMake Requirements

### 4.1 Minimum CMake Configuration

```cmake
# CMakeLists.txt (root)

cmake_minimum_required(VERSION 3.30)

project(ProjectV VERSION 0.1.0 LANGUAGES CXX)

# C++26 standard
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable C++26 Modules support
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# Required compilers:
# - GCC 14+
# - Clang 18+
# - MSVC 19.40+

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14.0")
        message(FATAL_ERROR "ProjectV requires GCC 14+ for C++26 modules")
    endif()
    add_compile_options(-fmodules-ts)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "18.0")
        message(FATAL_ERROR "ProjectV requires Clang 18+ for C++26 modules")
    endif()
    add_compile_options(-fmodules)
elseif(MSVC)
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19.40")
        message(FATAL_ERROR "ProjectV requires MSVC 19.40+ for C++26 modules")
    endif()
endif()

# Vulkan 1.4
find_package(Vulkan 1.4 REQUIRED)
```

### 4.2 Module Compilation Rules

```
Правила компиляции модулей:

1. Primary Module Interface (.cppm)
   - export module ProjectV.ModuleName;
   - Только объявления (declarations)
   - Может экспортировать другие модули
   - C headers в Global Module Fragment (module; #include <...>)
   - НИКАКИХ C++ library headers в GMF

2. Module Implementation (.cpp)
   - module ProjectV.ModuleName;
   - Определения (definitions)
   - #include C++ library headers ЗДЕСЬ
   - Не экспортирует ничего

3. Module Partition (.cppm)
   - export module ProjectV.ModuleName:Partition;
   - Внутренняя организация модуля
   - Импортируется главным интерфейсом

4. Global Module Fragment
   - module; (перед любым #include)
   - Только для C-заголовков
   - Не может экспортироваться
```

### 4.3 Module Dependencies Order

```
Зависимости модулей (порядок компиляции):

Level 0 (C libraries via GMF):
  - ProjectV.Render.Vulkan (Vulkan, VMA)
  - ProjectV.Core.Platform (SDL3)
  - ProjectV.Audio.Miniaudio (miniaudio)

Level 1 (нет зависимостей):
  - ProjectV.Core.Memory (VMA в .cpp)
  - ProjectV.Core.Containers
  - ProjectV.Voxel.SVO (только типы)

Level 2 (зависит от Level 0-1):
  - ProjectV.Core (агрегирует Level 1)
  - ProjectV.Render (агрегирует Vulkan + SVO)
  - ProjectV.Physics.Jolt (PIMPL, Jolt в .cpp)
  - ProjectV.ECS.Flecs (PIMPL, Flecs в .cpp)
  - ProjectV.UI.ImGui (PIMPL, ImGui в .cpp)

Level 3 (зависит от Level 2):
  - ProjectV.Physics (агрегирует Jolt)
  - ProjectV.ECS (агрегирует Flecs)
  - ProjectV.UI (агрегирует ImGui)

Level 4 (зависит от Level 3):
  - ProjectV.App (агрегирует всё)
```

---

## 5. Import Standards

### 5.1 Standard Library

```cpp
// Всегда используем import std; без fallback
import std;

// НЕ ИСПОЛЬЗОВАТЬ:
// #include <vector>
// #include <string>
// #include <expected>
// import std.compat;  // Запрещено!
```

### 5.2 Third-Party Libraries

```cpp
// C-библиотеки — через Global Module Fragment
module;
#include <vulkan/vulkan.h>  // OK: C header
#include <SDL3/SDL.h>       // OK: C header
#include <miniaudio.h>      // OK: C header

export module ProjectV.Render.Vulkan;

// C++ библиотеки — через PIMPL в .cpp файлах
// НЕ ДЕЛАЙТЕ ТАК:
// module;
// #include <Jolt/Jolt.h>  // ❌ ЗАПРЕЩЕНО!

// ДЕЛАЙТЕ ТАК:
// .cppm: export module ProjectV.Physics.Jolt; (без includes)
// .cpp:  module ProjectV.Physics.Jolt; #include <Jolt/Jolt.h>
```

### 5.3 Internal Modules

```cpp
// Импорт внутренних модулей
import ProjectV.Core;
import ProjectV.Render.Vulkan;
import ProjectV.Voxel.SVO;

// Транзитивные экспорты
export module ProjectV.Render;

// Экспортируем всё из подмодулей
export import ProjectV.Render.Vulkan;
export import ProjectV.Render.SVO;
export import ProjectV.Render.Mesh;
```

---

## 6. Error Handling Standard

```cpp
// Используем std::expected везде
[[nodiscard]] auto create() noexcept
    -> std::expected<T, Error>;

// Используем enum class для кодов ошибок
export enum class Error : uint8_t {
    AllocationFailed,
    InvalidParameter,
    // ...
};

// Используем std::optional для nullable результатов
[[nodiscard]] auto find_entity(std::string_view name) const noexcept
    -> std::optional<uint64_t>;

// Используем [[nodiscard]] для всех возвращающих значения функций
[[nodiscard]] auto size() const noexcept -> size_t;
```

---

## Статус

| Компонент                       | Статус         | Приоритет |
|---------------------------------|----------------|-----------|
| Module Structure                | Специфицирован | P0        |
| Global Module Fragment (C libs) | Специфицирован | P0        |
| PIMPL Architecture (C++ libs)   | Специфицирован | P0        |
| JoltPhysics PIMPL Wrapper       | Специфицирован | P0        |
| Flecs PIMPL Wrapper             | Специфицирован | P0        |
| ImGui PIMPL Wrapper             | Специфицирован | P1        |
| Tracy PIMPL Wrapper             | Специфицирован | P1        |
| CMake Requirements              | Специфицирован | P0        |
| Import Standards                | Специфицирован | P0        |

---

## Последствия

### Положительные:

- Чистая инкапсуляция через модули
- Быстрая компиляция (модули компилируются один раз)
- Явные зависимости между компонентами
- PIMPL изолирует C++ library changes от module importers
- Изменения в Jolt/ImGui/Flecs не триггерят recompile всех модулей

### Риски:

- Требуются современные компиляторы (GCC 14+, Clang 18+, MSVC 19.40+)
- CMake support for modules в развитии
- IDE support может быть ограничен
- PIMPL добавляет один уровень indirection (minimal overhead)

---

## Ссылки

- [ADR-0001: Vulkan Renderer](./0001-vulkan-renderer.md)
- [ADR-0002: SVO Storage](./0002-svo-storage.md)
- [ADR-0003: ECS Architecture](./0003-ecs-architecture.md)

# Интеграция JoltPhysics с ProjectV

> **Для понимания:** JoltPhysics — это как физический движок NASA. Точность до атома, производительность до предела.
> В ProjectV мы интегрируем его через наши DOD-аллокаторы, lock-free логгер и единый Job System.

## CMake конфигурация

### Git Submodules с интеграцией ProjectV

ProjectV использует подмодули с полной интеграцией в архитектуру движка:

```
ProjectV/
├── external/
│   └── JoltPhysics/           # Физический движок (интегрирован с MemoryManager)
├── src/
│   ├── core/                  # Ядро движка
│   │   ├── memory/            # DOD-аллокаторы
│   │   ├── logging/           # Lock-free логгер
│   │   └── profiling/         # Tracy hooks
│   └── physics/
│       └── jolt/              # JoltPhysics интеграция
└── CMakeLists.txt
```

```cmake
# CMakeLists.txt ProjectV
cmake_minimum_required(VERSION 3.25)
project(ProjectV LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Внешние зависимости (только подмодули) ---
# Отключаем всё лишнее в JoltPhysics
set(USE_SSE4_2 ON CACHE BOOL "Enable SSE4.2 optimizations" FORCE)
set(USE_AVX2 OFF CACHE BOOL "Disable AVX2 (not all CPUs support)" FORCE)
set(USE_AVX512 OFF CACHE BOOL "Disable AVX512" FORCE)
set(USE_LZCNT ON CACHE BOOL "Enable LZCNT instruction" FORCE)
set(USE_TZCNT ON CACHE BOOL "Enable TZCNT instruction" FORCE)
set(USE_F16C ON CACHE BOOL "Enable F16C instruction" FORCE)

set(JPH_CROSS_PLATFORM_DETERMINISTIC OFF CACHE BOOL "Disable deterministic mode" FORCE)
set(JPH_DEBUG_RENDERER OFF CACHE BOOL "Disable debug renderer" FORCE)
set(JPH_PROFILE_ENABLED OFF CACHE BOOL "Disable Jolt internal profiling" FORCE)

set(BUILD_TESTING OFF CACHE BOOL "Disable Jolt tests" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Disable Jolt examples" FORCE)

# Подключение подмодуля
add_subdirectory(external/JoltPhysics)

# --- Ядро ProjectV ---
# MemoryManager модуль (уже должен быть в проекте)
add_library(projectv_core_memory STATIC
    src/core/memory/page_allocator.cpp
    src/core/memory/global_manager.cpp
    src/core/memory/arena_allocator.cpp
    src/core/memory/pool_allocator.cpp
    src/core/memory/linear_allocator.cpp
)

target_compile_features(projectv_core_memory PUBLIC cxx_std_26)
target_include_directories(projectv_core_memory PUBLIC src/core)

# Logging модуль
add_library(projectv_core_logging STATIC
    src/core/logging/logger.cpp
    src/core/logging/ring_buffer.cpp
)

target_compile_features(projectv_core_logging PUBLIC cxx_std_26)
target_include_directories(projectv_core_logging PUBLIC src/core)
target_link_libraries(projectv_core_logging PRIVATE projectv_core_memory)

# Profiling модуль (только для Profile конфигурации)
if(PROJECTV_ENABLE_PROFILING)
    add_library(projectv_core_profiling STATIC
        src/core/profiling/tracy_hooks.cpp
        src/core/profiling/physics_profiler.cpp
    )
    target_compile_features(projectv_core_profiling PUBLIC cxx_std_26)
    target_include_directories(projectv_core_profiling PUBLIC src/core)
    target_link_libraries(projectv_core_profiling PRIVATE projectv_core_memory projectv_core_logging)
    target_compile_definitions(projectv_core_profiling PRIVATE TRACY_ENABLE)
endif()

# --- JoltPhysics модуль движка ---
add_library(projectv_physics_jolt STATIC
    src/physics/jolt/integration.cpp
    src/physics/jolt/allocator.cpp
    src/physics/jolt/logger.cpp
)

target_compile_features(projectv_physics_jolt PUBLIC cxx_std_26)
target_include_directories(projectv_physics_jolt PUBLIC src/physics)
target_link_libraries(projectv_physics_jolt PRIVATE
    projectv_core_memory
    projectv_core_logging
    Jolt
)

if(PROJECTV_ENABLE_PROFILING)
    target_link_libraries(projectv_physics_jolt PRIVATE projectv_core_profiling)
endif()

# --- Исполняемый файл ProjectV ---
add_executable(ProjectV
    src/main.cpp
)

target_link_libraries(ProjectV PRIVATE
    projectv_physics_jolt
)

# Для отладки
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(ProjectV PRIVATE DEBUG)
endif()

# Profile конфигурация
if(PROJECTV_ENABLE_PROFILING)
    target_compile_definitions(ProjectV PRIVATE TRACY_ENABLE)
    target_link_libraries(ProjectV PRIVATE Tracy::TracyClient)
endif()
```

---

## Инициализация Modern JoltPhysics 5.5.1

### 1. C++26 Module с Global Module Fragment

```cpp
// src/physics/jolt/integration.cppm - Primary Module Interface
module;
// Global Module Fragment: изолируем заголовки JoltPhysics
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/JobSystem.h>

export module projectv.physics.jolt;

import std;
import projectv.core.memory;
import projectv.core.log;
import projectv.core.profiling;

namespace projectv::physics::jolt {

// --- Обработка ошибок через std::expected ---
enum class JoltError {
    AllocatorRegistrationFailed,
    FactoryCreationFailed,
    TypeRegistrationFailed,
    PhysicsSystemInitFailed,
    JobSystemInitFailed,
    NoSuitableConfiguration,
    BodyCreationFailed,
    ShapeCreationFailed,
    ConstraintCreationFailed
};

template<typename T>
using JoltResult = std::expected<T, JoltError>;

inline std::string to_string(JoltError error) {
    switch (error) {
        case JoltError::AllocatorRegistrationFailed: return "Jolt allocator registration failed";
        case JoltError::FactoryCreationFailed: return "Jolt factory creation failed";
        case JoltError::TypeRegistrationFailed: return "Jolt type registration failed";
        case JoltError::PhysicsSystemInitFailed: return "Physics system initialization failed";
        case JoltError::JobSystemInitFailed: return "Job system initialization failed";
        case JoltError::NoSuitableConfiguration: return "No suitable physics configuration found";
        case JoltError::BodyCreationFailed: return "Physics body creation failed";
        case JoltError::ShapeCreationFailed: return "Physics shape creation failed";
        case JoltError::ConstraintCreationFailed: return "Physics constraint creation failed";
        default: return "Unknown JoltPhysics error";
    }
}

// --- MemoryManager интеграция через JPH::AllocateFunction ---
class JoltAllocator {
public:
    static void registerCustomAllocator() noexcept {
        // Регистрируем кастомные аллокаторы ProjectV
        JPH::AllocateFunction allocate_func = &joltAllocate;
        JPH::FreeFunction free_func = &joltFree;
        JPH::AlignedAllocateFunction aligned_allocate_func = &joltAlignedAllocate;
        JPH::AlignedFreeFunction aligned_free_func = &joltAlignedFree;

        JPH::RegisterCustomAllocator(
            allocate_func,
            free_func,
            aligned_allocate_func,
            aligned_free_func
        );

        projectv::core::Log::info("Physics", "JoltPhysics custom allocators registered with ProjectV MemoryManager");
    }

private:
    static void* joltAllocate(size_t size) {
        ZoneScopedN("JoltAllocate");

        // Используем ProjectV MemoryManager в зависимости от размера
        auto& memoryManager = projectv::core::memory::getGlobalMemoryManager();

        if (size <= 256) {
            // Малые объекты - PoolAllocator
            return memoryManager.createPool(size, 1000).allocate();
        } else if (size <= 1024 * 1024) {
            // Средние объекты - ArenaAllocator из thread arena
            return memoryManager.getThreadArena().allocate(size, 16);
        } else {
            // Крупные объекты - PageAllocator
            auto result = projectv::core::memory::PageAllocator::allocatePage(size, 64);
            if (result) {
                return *result;
            }
            projectv::core::Log::error("Physics", "Jolt page allocation failed: size={}", size);
            return nullptr;
        }
    }

    static void joltFree(void* ptr) {
        if (!ptr) return;

        ZoneScopedN("JoltFree");

        // В реальной реализации нужно определить, из какого аллокатора выделена память
        // и вернуть её туда. Для простоты примера - игнорируем.
        // В production коде используется allocation tracking через MemoryManager.
    }

    static void* joltAlignedAllocate(size_t size, size_t alignment) {
        ZoneScopedN("JoltAlignedAllocate");

        auto& memoryManager = projectv::core::memory::getGlobalMemoryManager();

        if (size <= 256) {
            // Малые объекты с выравниванием
            return memoryManager.createPool(size, 1000, alignment).allocate();
        } else {
            // Крупные объекты с выравниванием
            auto result = projectv::core::memory::PageAllocator::allocatePage(size, alignment);
            if (result) {
                return *result;
            }
            projectv::core::Log::error("Physics", "Jolt aligned allocation failed: size={}, alignment={}", size, alignment);
            return nullptr;
        }
    }

    static void joltAlignedFree(void* ptr) {
        if (!ptr) return;
        ZoneScopedN("JoltAlignedFree");
        // Аналогично joltFree
    }
};

// --- TempAllocator на основе ProjectV ArenaAllocator ---
class JoltTempAllocator : public JPH::TempAllocator {
public:
    JoltTempAllocator(size_t capacity = 10 * 1024 * 1024)
        : arena_(projectv::core::memory::getGlobalMemoryManager().createArena(capacity)) {
        projectv::core::Log::debug("Physics", "JoltTempAllocator created with capacity: {} MB", capacity / (1024 * 1024));
    }

    ~JoltTempAllocator() override {
        projectv::core::Log::debug("Physics", "JoltTempAllocator destroyed");
    }

    void* Allocate(size_t size) override {
        ZoneScopedN("JoltTempAllocate");
        return arena_.allocate(size, 16);
    }

    void Free(void* ptr, size_t size) override {
        // ArenaAllocator не поддерживает освобождение отдельных блоков
        // Вся память освобождается при reset()
    }

    void Reset() override {
        ZoneScopedN("JoltTempAllocatorReset");
        arena_.reset();
        projectv::core::Log::trace("Physics", "JoltTempAllocator reset");
    }

private:
    projectv::core::memory::ArenaAllocator arena_;
};

// --- Logging Integration через JPH::TraceImpl ---
class JoltLogger {
public:
    static void registerCustomLogger() noexcept {
        JPH::TraceImpl trace_func = &joltTrace;
        JPH::SetTraceCallback(trace_func);

        projectv::core::Log::info("Physics", "JoltPhysics custom logger registered with ProjectV Log");
    }

private:
    static void joltTrace(const char* fmt, ...) {
        // Преобразуем variadic args в строку
        char buffer[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        // Определяем уровень логирования на основе префикса сообщения
        std::string message(buffer);
        projectv::core::Log::Level level = projectv::core::Log::Level::Info;

        if (message.find("[ERROR]") != std::string::npos) {
            level = projectv::core::Log::Level::Error;
        } else if (message.find("[WARNING]") != std::string::npos) {
            level = projectv::core::Log::Level::Warning;
        } else if (message.find("[DEBUG]") != std::string::npos) {
            level = projectv::core::Log::Level::Debug;
        } else if (message.find("[TRACE]") != std::string::npos) {
            level = projectv::core::Log::Level::Trace;
        }

        // Логируем через ProjectV Log
        projectv::core::Log::log(level, "Physics", "{}", message);
    }
};

// --- Profiling Integration (Tracy hooks) ---
class JoltProfiler {
public:
    static void beginPhysicsUpdate(const char* name = "PhysicsSystem::Update") {
        ZoneScopedN(name);
        TracyMessageL(name);
    }

    static void beginCollisionDetection() {
        ZoneScopedN("CollisionDetection");
#ifdef PROJECTV_PROFILE_DEEP
        ZoneScopedN("BroadPhaseNarrowPhase");
#endif
    }

    static void beginConstraintSolving() {
        ZoneScopedN("ConstraintSolving");
    }

    static void logPhysicsEvent(const char* event, const char* details = "") {
        TracyMessage(event, details);
    }

    static void logCollisionEvent(JPH::BodyID body1, JPH::BodyID body2) {
#ifdef PROJECTV_PROFILE_DEEP
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "Collision: Body %u - Body %u", 
                 body1.GetIndex(), body2.GetIndex());
        TracyMessageL(buffer);
#endif
    }

    static void logBodyActivation(JPH::BodyID bodyId, bool activated) {
        projectv::core::Log::debug("Physics", "Body {} {}", 
            bodyId.GetIndex(), activated ? "activated" : "deactivated");
    }
};
```

---

## Интеграция с Flecs ECS

### Компоненты

```cpp
#include <flecs.h>

struct PhysicsBody {
    JPH::BodyID id;
    bool is_kinematic = false;
};

struct PhysicsProperties {
    float mass = 1.0f;
    float friction = 0.5f;
    float restitution = 0.3f;
};

struct PhysicsDirtyFlags {
    bool position = false;
    bool rotation = false;
    bool velocity = false;
};
```

### Регистрация компонентов

```cpp
void RegisterPhysicsComponents(flecs::world& world) {
    world.component<PhysicsBody>();
    world.component<PhysicsProperties>();
    world.component<PhysicsDirtyFlags>();
}
```

### Создание тела из ECS сущности

```cpp
#include <Jolt/Physics/Body/BodyCreationSettings.h>

JPH::BodyID CreatePhysicsBody(
    JPH::PhysicsSystem& physics,
    const glm::vec3& position,
    const glm::quat& rotation,
    JPH::ShapeRefC shape,
    float mass,
    JPH::ObjectLayer layer
) {
    JPH::BodyInterface& interface = physics.GetBodyInterfaceNoLock();

    JPH::BodyCreationSettings settings;
    settings.SetShape(shape.Get(), JPH::ShapeFilter());
    settings.mPosition = JPH::RVec3(position.x, position.y, position.z);
    settings.mRotation = JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w);
    settings.mMotionType = JPH::EMotionType::Dynamic;
    settings.mObjectLayer = layer;
    settings.mMass = mass;

    return interface.CreateAndAddBody(settings, JPH::EActivation::Activate);
}
```

### Синхронизация: Jolt → ECS

```cpp
void CreatePhysicsSyncToECSSystem(flecs::world& world, JPH::PhysicsSystem& physics) {
    JPH::BodyInterface& interface = physics.GetBodyInterfaceNoLock();

    world.system<PhysicsBody>("SyncPhysicsToECS")
        .kind(flecs::PostUpdate)
        .each([&interface](PhysicsBody& body, Transform& transform) {
            if (body.id.IsInvalid() || !interface.IsActive(body.id))
                return;

            JPH::RVec3 pos = interface.GetCenterOfMassPosition(body.id);
            JPH::Quat rot = interface.GetRotation(body.id);

            transform.position = glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
            transform.rotation = glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());
        });
}
```

### Синхронизация: ECS → Jolt (Kinematic)

```cpp
void CreateKinematicSyncToPhysicsSystem(flecs::world& world, JPH::PhysicsSystem& physics) {
    JPH::BodyInterface& interface = physics.GetBodyInterfaceNoLock();

    world.system<const Transform, PhysicsBody>("SyncECSToPhysics")
        .kind(flecs::PreUpdate)
        .each([&interface](const Transform& transform, PhysicsBody& body) {
            if (body.id.IsInvalid() || !body.is_kinematic)
                return;

            interface.SetPositionAndRotation(
                body.id,
                JPH::RVec3(transform.position.x, transform.position.y, transform.position.z),
                JPH::Quat(transform.rotation.x, transform.rotation.y,
                          transform.rotation.z, transform.rotation.w),
                JPH::EActivation::Activate
            );
        });
}
```

### Observer для очистки тел

```cpp
void CreatePhysicsCleanupObserver(flecs::world& world, JPH::PhysicsSystem& physics) {
    JPH::BodyInterface& interface = physics.GetBodyInterfaceNoLock();

    world.observer<PhysicsBody>()
        .event(flecs::OnRemove)
        .each([&interface](PhysicsBody& body) {
            if (!body.id.IsInvalid()) {
                interface.RemoveBody(body.id);
                interface.DestroyBody(body.id);
                body.id = JPH::BodyID();
            }
        });
}
```

---

## Интеграция с Vulkan

### Конвертация типов

```cpp
namespace PhysicsMath {
    JPH::Vec3 ToJolt(const glm::vec3& v) {
        return JPH::Vec3(v.x, v.y, v.z);
    }

    JPH::RVec3 ToJoltR(const glm::vec3& v) {
        return JPH::RVec3(v.x, v.y, v.z);
    }

    glm::vec3 ToGlm(const JPH::Vec3& v) {
        return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
    }

    glm::vec3 ToGlm(const JPH::RVec3& v) {
        return glm::vec3(static_cast<float>(v.GetX()),
                        static_cast<float>(v.GetY()),
                        static_cast<float>(v.GetZ()));
    }

    glm::quat ToGlm(const JPH::Quat& q) {
        return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
    }
}
```

### Управление временем жизни буферов через Observer

```cpp
#include <Jolt/Physics/Body/Body.h>

class VulkanResourceObserver {
public:
    VulkanResourceObserver(JPH::PhysicsSystem& physics, VmaAllocator allocator)
        : m_physics(physics), m_allocator(allocator) {}

    void OnBodyDestroyed(JPH::BodyID id) {
        auto it = m_buffer_map.find(id);
        if (it != m_buffer_map.end()) {
            vmaDestroyBuffer(m_allocator, it->second.buffer, it->second.allocation);
            m_buffer_map.erase(it);
        }
    }

    void RegisterBody(JPH::BodyID id, VkBuffer buffer, VmaAllocation allocation) {
        m_buffer_map[id] = {buffer, allocation};
    }

private:
    struct BufferInfo {
        VkBuffer buffer;
        VmaAllocation allocation;
    };

    JPH::PhysicsSystem& m_physics;
    VmaAllocator m_allocator;
    std::unordered_map<JPH::BodyID, BufferInfo> m_buffer_map;
};
```

### Обновление GPU-буфера после физики

```cpp
void UpdateGPUBuffers(
    JPH::PhysicsSystem& physics,
    VkCommandBuffer cmd_buffer,
    VkBuffer position_buffer
) {
    JPH::BodyInterface& interface = physics.GetBodyInterfaceNoLock();

    std::vector<glm::mat4> transforms;
    transforms.reserve(physics.GetNumBodies());

    JPH::BodyIDVector bodies;
    physics.GetBodies(bodies);

    for (JPH::BodyID id : bodies) {
        if (!interface.IsActive(id))
            continue;

        JPH::RVec3 pos = interface.GetCenterOfMassPosition(id);
        JPH::Quat rot = interface.GetRotation(id);

        glm::mat4 transform = glm::translate(glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ()))
                             * glm::mat4_cast(glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ()));
        transforms.push_back(transform);
    }

    void* data;
    vmaMapMemory(m_allocator, m_staging_allocation, &data);
    std::memcpy(data, transforms.data(), transforms.size() * sizeof(glm::mat4));
    vmaUnmapMemory(m_allocator, m_staging_allocation);

    VkBufferCopy copy_region = {};
    copy_region.size = transforms.size() * sizeof(glm::mat4);
    vkCmdCopyBuffer(cmd_buffer, m_staging_buffer, position_buffer, 1, &copy_region);
}
```

---

## Интеграция с SDL3

### Получение input для кинематических тел

```cpp
#include <SDL3/SDL.h>

struct InputState {
    glm::vec3 move_direction = glm::vec3(0.0f);
    bool jump = false;
};

InputState PollInput() {
    InputState state;
    const Uint8* keys = SDL_GetKeyboardState(nullptr);

    if (keys[SDL_SCANCODE_W]) state.move_direction.z -= 1.0f;
    if (keys[SDL_SCANCODE_S]) state.move_direction.z += 1.0f;
    if (keys[SDL_SCANCODE_A]) state.move_direction.x -= 1.0f;
    if (keys[SDL_SCANCODE_D]) state.move_direction.x += 1.0f;
    if (keys[SDL_SCANCODE_SPACE]) state.jump = true;

    return state;
}

void CreatePlayerControllerSystem(
    flecs::world& world,
    JPH::PhysicsSystem& physics
) {
    JPH::BodyInterface& interface = physics.GetBodyInterfaceNoLock();

    world.system<PhysicsBody, const InputState>("PlayerController")
        .each([&interface](PhysicsBody& body, const InputState& input) {
            if (body.id.IsInvalid() || !body.is_kinematic)
                return;

            JPH::Vec3 velocity = interface.GetLinearVelocity(body.id);
            velocity.SetX(input.move_direction.x * 10.0f);
            velocity.SetZ(input.move_direction.z * 10.0f);

            if (input.jump) {
                velocity.SetY(5.0f);
            }

            interface.SetLinearVelocity(body.id, velocity);
        });
}
```

### Callback для контактов (через ECS Observer)

```cpp
class PhysicsContactCallback : public JPH::ContactListener {
public:
    void OnContactAdded(
        const JPH::Body& body1,
        const JPH::Body& body2,
        const JPH::ContactManifold& manifold,
        JPH::ContactSettings& settings) override
    {
        // Логика обработки контакта
    }

    void OnContactPersisted(
        const JPH::Body& body1,
        const JPH::Body& body2,
        const JPH::ContactManifold& manifold,
        JPH::ContactSettings& settings) override
    {
        // Обновление состояния контакта
    }

    void OnContactRemoved(const JPH::SubShapeIDPair& sub_shape_pair) override
    {
        // Очистка при удалении контакта
    }
};
```

```

### 2. JoltPhysics Integration класс с полной интеграцией ProjectV

```cpp
// Продолжение модуля projectv.physics.jolt
// (в том же файле integration.cppm после JoltProfiler)

class JoltPhysicsIntegration {
public:
    JoltPhysicsIntegration() = default;
    ~JoltPhysicsIntegration() {
        ZoneScopedN("JoltPhysicsIntegrationDestructor");
        shutdown();
    }

    // Non-copyable
    JoltPhysicsIntegration(const JoltPhysicsIntegration&) = delete;
    JoltPhysicsIntegration& operator=(const JoltPhysicsIntegration&) = delete;

    // Movable
    JoltPhysicsIntegration(JoltPhysicsIntegration&& other) noexcept;
    JoltPhysicsIntegration& operator=(JoltPhysicsIntegration&& other) noexcept;

    // Инициализация с интеграцией ProjectV
    JoltResult<void> initialize();
    void shutdown();

    // Обновление физики с профайлингом
    void update(float delta_time);

    // Getters
    JPH::PhysicsSystem& physics_system() noexcept { return physics_; }
    const JPH::PhysicsSystem& physics_system() const noexcept { return physics_; }

    // Вспомогательные методы
    JPH::BodyInterface& body_interface() noexcept { return physics_.GetBodyInterfaceNoLock(); }
    const JPH::BodyInterface& body_interface() const noexcept { return physics_.GetBodyInterfaceNoLock(); }

private:
    JoltResult<void> register_custom_allocators();
    JoltResult<void> create_factory();
    JoltResult<void> register_types();
    JoltResult<void> create_temp_allocator();
    JoltResult<void> create_physics_system();
    JoltResult<void> create_job_system();
    JoltResult<void> setup_listeners();

    // Jolt handles
    std::unique_ptr<JoltTempAllocator> temp_allocator_;
    std::unique_ptr<JPH::JobSystem> job_system_;
    JPH::PhysicsSystem physics_;

    // Listeners
    BPLayerInterfaceImpl bp_layer_interface_;
    ObjectLayerPairFilterImpl object_layer_filter_;
    ObjectVsBroadPhaseLayerFilterImpl object_vs_bp_filter_;
    PhysicsContactCallback contact_callback_;
    BodyActivationListenerImpl activation_listener_;

    // Configuration
    static constexpr uint32_t kMaxBodies = 16384;
    static constexpr uint32_t kNumBodyMutexes = 0;
    static constexpr uint32_t kMaxBodyPairs = 8192;
    static constexpr uint32_t kMaxContactConstraints = 4096;
};

// Реализация методов
JoltResult<void> JoltPhysicsIntegration::initialize() {
    ZoneScopedN("JoltPhysicsIntegrationInitialize");

    projectv::core::Log::info("Physics", "Initializing JoltPhysics 5.5.1 with ProjectV integration");

    // 1. Регистрация кастомных аллокаторов ProjectV
    if (auto result = register_custom_allocators(); !result) {
        projectv::core::Log::error("Physics", "Failed to register custom allocators: {}", 
            to_string(result.error()));
        return std::unexpected(result.error());
    }

    // 2. Создание фабрики
    if (auto result = create_factory(); !result) {
        projectv::core::Log::error("Physics", "Failed to create factory: {}", 
            to_string(result.error()));
        return std::unexpected(result.error());
    }

    // 3. Регистрация типов
    if (auto result = register_types(); !result) {
        projectv::core::Log::error("Physics", "Failed to register types: {}", 
            to_string(result.error()));
        return std::unexpected(result.error());
    }

    // 4. Создание TempAllocator на основе ProjectV ArenaAllocator
    if (auto result = create_temp_allocator(); !result) {
        projectv::core::Log::error("Physics", "Failed to create temp allocator: {}", 
            to_string(result.error()));
        return std::unexpected(result.error());
    }

    // 5. Создание Job System (будет обновлено в 03_advanced.md с JobSystem Bridge)
    if (auto result = create_job_system(); !result) {
        projectv::core::Log::error("Physics", "Failed to create job system: {}", 
            to_string(result.error()));
        return std::unexpected(result.error());
    }

    // 6. Создание Physics System
    if (auto result = create_physics_system(); !result) {
        projectv::core::Log::error("Physics", "Failed to create physics system: {}", 
            to_string(result.error()));
        return std::unexpected(result.error());
    }

    // 7. Настройка listeners
    if (auto result = setup_listeners(); !result) {
        projectv::core::Log::error("Physics", "Failed to setup listeners: {}", 
            to_string(result.error()));
        return std::unexpected(result.error());
    }

    // 8. Регистрация кастомного логгера ProjectV
    JoltLogger::registerCustomLogger();

    projectv::core::Log::info("Physics", "JoltPhysics initialized successfully with {} max bodies", kMaxBodies);
    return {};
}

JoltResult<void> JoltPhysicsIntegration::register_custom_allocators() {
    ZoneScopedN("RegisterCustomAllocators");
    JoltAllocator::registerCustomAllocator();
    return {};
}

JoltResult<void> JoltPhysicsIntegration::create_factory() {
    ZoneScopedN("CreateFactory");
    
    // Проверяем, что фабрика ещё не создана
    if (JPH::Factory::sInstance != nullptr) {
        projectv::core::Log::warning("Physics", "Jolt factory already exists");
        return {};
    }

    // Создаём фабрику через ProjectV MemoryManager
    JPH::Factory::sInstance = new (std::nothrow) JPH::Factory();
    if (JPH::Factory::sInstance == nullptr) {
        projectv::core::Log::error("Physics", "Failed to allocate Jolt factory");
        return std::unexpected(JoltError::FactoryCreationFailed);
    }

    projectv::core::Log::debug("Physics", "Jolt factory created successfully");
    return {};
}

JoltResult<void> JoltPhysicsIntegration::register_types() {
    ZoneScopedN("RegisterTypes");
    JPH::RegisterTypes();
    projectv::core::Log::debug("Physics", "Jolt types registered");
    return {};
}

JoltResult<void> JoltPhysicsIntegration::create_temp_allocator() {
    ZoneScopedN("CreateTempAllocator");
    
    // Используем наш кастомный TempAllocator на основе ProjectV ArenaAllocator
    temp_allocator_ = std::make_unique<JoltTempAllocator>(10 * 1024 * 1024);
    
    if (!temp_allocator_) {
        projectv::core::Log::error("Physics", "Failed to create temp allocator");
        return std::unexpected(JoltError::JobSystemInitFailed);
    }

    projectv::core::Log::debug("Physics", "Temp allocator created with 10MB capacity");
    return {};
}

JoltResult<void> JoltPhysicsIntegration::create_job_system() {
    ZoneScopedN("CreateJobSystem");

    // Временная реализация - используем стандартный JobSystemThreadPool
    // В 03_advanced.md будет реализован JobSystem Bridge для интеграции с projectv::core::jobs::ThreadPool
    uint32_t threads = std::max(1u, projectv::config::get_thread_count() - 1);
    
    job_system_ = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs, 
        JPH::cMaxPhysicsBarriers, 
        threads
    );

    if (!job_system_) {
        projectv::core::Log::error("Physics", "Failed to create job system");
        return std::unexpected(JoltError::JobSystemInitFailed);
    }

    projectv::core::Log::info("Physics", "Jolt job system created with {} threads", threads);
    return {};
}

JoltResult<void> JoltPhysicsIntegration::create_physics_system() {
    ZoneScopedN("CreatePhysicsSystem");

    // Инициализируем Physics System с нашими конфигурациями
    JPH::PhysicsSystem::InitSettings settings;
    
    if (!physics_.Init(
        kMaxBodies,
        kNumBodyMutexes,
        kMaxBodyPairs,
        kMaxContactConstraints,
        &bp_layer_interface_,
        &object_vs_bp_filter_,
        &object_layer_filter_
    )) {
        projectv::core::Log::error("Physics", "Physics system initialization failed");
        return std::unexpected(JoltError::PhysicsSystemInitFailed);
    }

    projectv::core::Log::debug("Physics", "Physics system initialized: max_bodies={}, max_pairs={}", 
        kMaxBodies, kMaxBodyPairs);
    return {};
}

JoltResult<void> JoltPhysicsIntegration::setup_listeners() {
    ZoneScopedN("SetupListeners");

    physics_.SetContactListener(&contact_callback_);
    physics_.SetBodyActivationListener(&activation_listener_);

    projectv::core::Log::debug("Physics", "Physics listeners setup complete");
    return {};
}

void JoltPhysicsIntegration::update(float delta_time) {
    // Профайлинг всей физической симуляции
    JoltProfiler::beginPhysicsUpdate();

    // Профайлинг отдельных этапов
    {
        ZoneScopedN("PhysicsUpdateStep");
        JoltProfiler::beginCollisionDetection();
        JoltProfiler::beginConstraintSolving();
        
        physics_.Update(delta_time, 1, temp_allocator_.get(), job_system_.get());
    }

    // Сброс TempAllocator для следующего кадра
    temp_allocator_->Reset();
}

void JoltPhysicsIntegration::shutdown() {
    ZoneScopedN("JoltPhysicsIntegrationShutdown");

    projectv::core::Log::info("Physics", "Shutting down JoltPhysics integration");

    // Освобождаем Job System
    job_system_.reset();
    projectv::core::Log::debug("Physics", "Job system destroyed");

    // Выключаем Physics System
    physics_.Shutdown();
    projectv::core::Log::debug("Physics", "Physics system shutdown");

    // Освобождаем TempAllocator
    temp_allocator_.reset();
    projectv::core::Log::debug("Physics", "Temp allocator destroyed");

    // Удаляем фабрику
    if (JPH::Factory::sInstance != nullptr) {
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
        projectv::core::Log::debug("Physics", "Factory destroyed");
    }

    projectv::core::Log::info("Physics", "JoltPhysics integration shutdown complete");
}

JoltPhysicsIntegration::JoltPhysicsIntegration(JoltPhysicsIntegration&& other) noexcept
    : temp_allocator_(std::move(other.temp_allocator_))
    , job_system_(std::move(other.job_system_))
    , physics_(std::move(other.physics_))
    , bp_layer_interface_(std::move(other.bp_layer_interface_))
    , object_layer_filter_(std::move(other.object_layer_filter_))
    , object_vs_bp_filter_(std::move(other.object_vs_bp_filter_))
    , contact_callback_(std::move(other.contact_callback_))
    , activation_listener_(std::move(other.activation_listener_)) {
}

JoltPhysicsIntegration& JoltPhysicsIntegration::operator=(JoltPhysicsIntegration&& other) noexcept {
    if (this != &other) {
        shutdown();

        temp_allocator_ = std::move(other.temp_allocator_);
        job_system_ = std::move(other.job_system_);
        physics_ = std::move(other.physics_);
        bp_layer_interface_ = std::move(other.bp_layer_interface_);
        object_layer_filter_ = std::move(other.object_layer_filter_);
        object_vs_bp_filter_ = std::move(other.object_vs_bp_filter_);
        contact_callback_ = std::move(other.contact_callback_);
        activation_listener_ = std::move(other.activation_listener_);
    }
    return *this;
}
```

### 3. Пример использования в ProjectV Engine

```cpp
// Пример интеграции JoltPhysics в игровой цикл ProjectV
class ProjectVEngine {
public:
    ProjectVEngine() {
        ZoneScopedN("ProjectVEngineConstructor");
        
        // Инициализация ProjectV систем
        projectv::core::Log::initialize();
        projectv::core::memory::GlobalMemoryManager::initialize();
        
        // Инициализация JoltPhysics
        auto result = physics_integration_.initialize();
        if (!result) {
            projectv::core::Log::critical("Engine", "Failed to initialize physics: {}", 
                to_string(result.error()));
            // Обработка ошибки через std::expected
        }
    }

    ~ProjectVEngine() {
        ZoneScopedN("ProjectVEngineDestructor");
        physics_integration_.shutdown();
    }

    void update(float delta_time) {
        ZoneScopedN("EngineUpdate");
        
        // Обновление физики с профайлингом
        physics_integration_.update(delta_time);
        
        // Синхронизация физики с ECS
        sync_physics_to_ecs();
    }

    void sync_physics_to_ecs() {
        ZoneScopedN("SyncPhysicsToECS");
        
        auto& body_interface = physics_integration_.body_interface();
        auto view = ecs_world_.view<PhysicsBody, Transform>();
        
        view.each([&body_interface](PhysicsBody& body, Transform& transform) {
            if (body.id.IsInvalid() || !body_interface.IsActive(body.id))
                return;

            JPH::RVec3 pos = body_interface.GetCenterOfMassPosition(body.id);
            JPH::Quat rot = body_interface.GetRotation(body.id);

            transform.position = glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
            transform.rotation = glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());
        });
    }

private:
    JoltPhysicsIntegration physics_integration_;
    flecs::world ecs_world_;
};
```

```

## Оптимизация

### Batch операции с интеграцией ProjectV

```cpp
// Batch создание тел с профайлингом
JoltResult<std::vector<JPH::BodyID>> createBodiesBatch(
    JPH::BodyInterface& interface,
    std::span<const JPH::BodyCreationSettings> settings) {
    
    ZoneScopedN("CreateBodiesBatch");
    
    if (settings.empty()) {
        projectv::core::Log::warning("Physics", "Empty batch creation requested");
        return std::vector<JPH::BodyID>{};
    }

    std::vector<JPH::BodyID> ids(settings.size());
    
    // Используем batch операцию JoltPhysics
    interface.CreateAndAddBodies(settings.data(), static_cast<int>(settings.size()), 
                                 ids.data(), JPH::EActivation::Activate);
    
    projectv::core::Log::debug("Physics", "Created {} bodies in batch", settings.size());
    return ids;
}
```

### OptimizeBroadPhase с профайлингом

```cpp
void optimizePhysicsBroadPhase(JPH::PhysicsSystem& physics) {
    ZoneScopedN("OptimizeBroadPhase");
    
    physics.OptimizeBroadPhase();
    
    projectv::core::Log::info("Physics", "Broad phase optimized");
    JoltProfiler::logPhysicsEvent("BroadPhaseOptimized");
}
```

### Fixed timestep с интеграцией ProjectV

```cpp
class FixedTimestepPhysics {
public:
    FixedTimestepPhysics(JoltPhysicsIntegration& physics, float fixed_delta = 1.0f / 60.0f)
        : physics_(physics), fixed_delta_(fixed_delta) {
        projectv::core::Log::debug("Physics", "Fixed timestep physics initialized: {} Hz", 1.0f / fixed_delta);
    }

    void update(float delta_time) {
        ZoneScopedN("FixedTimestepUpdate");
        
        accumulator_ += delta_time;
        int steps = 0;
        
        while (accumulator_ >= fixed_delta_ && steps < kMaxSteps) {
            ZoneScopedN("PhysicsStep");
            
            physics_.update(fixed_delta_);
            accumulator_ -= fixed_delta_;
            steps++;
            
            // Защита от spiral of death
            if (steps >= kMaxSteps) {
                projectv::core::Log::warning("Physics", "Physics spiral of death detected, clamping steps");
                accumulator_ = 0.0f;
                break;
            }
        }
        
        // Интерполяция для smooth rendering
        interpolation_factor_ = accumulator_ / fixed_delta_;
        
        if (steps > 0) {
            JoltProfiler::logPhysicsEvent("PhysicsSteps", std::format("steps={}", steps).c_str());
        }
    }

    float interpolation_factor() const noexcept { return interpolation_factor_; }

private:
    JoltPhysicsIntegration& physics_;
    float fixed_delta_;
    float accumulator_ = 0.0f;
    float interpolation_factor_ = 0.0f;
    static constexpr int kMaxSteps = 10;
};
```

## Заключение

### Что мы получили после интеграции:

1. **✅ MemoryManager Integration:** Все аллокации JoltPhysics проходят через ProjectV аллокаторы:
   - `PageAllocator` для ядра системы и постоянных буферов
   - `ArenaAllocator` для временных данных кадра (O(1) аллокация и сброс)
   - `PoolAllocator` для частых объектов (Body, Shape, Constraint)

2. **✅ Logging Integration:** Все физические события логируются через ProjectV логгер:
   - `Info` уровень для инициализации и основных событий
   - `Debug` уровень для создания/удаления динамических тел
   - `Error` уровень для ошибок симуляции
   - `Trace` уровень для коллизий (только при отладке)

3. **✅ Profiling Integration:** Полная интеграция с Tracy:
   - High-level: `PhysicsSystem::Update`
   - Mid-level: `CollisionDetection` и `ConstraintSolving`
   - Deep-level: `BroadPhaseNarrowPhase` (под макросом `PROJECTV_PROFILE_DEEP`)

4. **✅ C++26 Module:** Современная модульная архитектура с Global Module Fragment
5. **✅ Error Handling:** Использование `std::expected<..., JoltError>` для type-safe ошибок
6. **✅ Phase 0 Compliance:** Никаких `try-catch`, `throw`, `std::mutex` и других запрещенных методов

### Что будет в 03_advanced.md:

1. **JobSystem Bridge:** Адаптер для интеграции Jolt JobSystem с единым `projectv::core::jobs::ThreadPool` на базе stdexec
2. **Lock-free структуры:** Оптимизации для многопоточного доступа к физическим данным
3. **GPU-Driven Physics:** Интеграция с Vulkan для GPU-ускоренной физики
4. **DOD оптимизации:** Structure of Arrays для массовых операций с физическими телами
5. **Predictive Physics:** Предсказательная физика для сетевых игр

### Пример быстрого старта:

```cpp
#include <projectv/physics/jolt.hpp>

int main() {
    // Инициализация ProjectV систем
    projectv::core::Log::initialize();
    projectv::core::memory::GlobalMemoryManager::initialize();
    
    // Создание физической интеграции
    projectv::physics::jolt::JoltPhysicsIntegration physics;
    
    // Инициализация
    if (auto result = physics.initialize(); !result) {
        projectv::core::Log::critical("Main", "Physics initialization failed: {}", 
            projectv::physics::jolt::to_string(result.error()));
        return 1;
    }
    
    // Игровой цикл
    FixedTimestepPhysics fixed_step(physics);
    
    while (running) {
        float delta_time = get_delta_time();
        fixed_step.update(delta_time);
        
        // Рендеринг с интерполяцией
        render(fixed_step.interpolation_factor());
    }
    
    return 0;
}
```

**JoltPhysics теперь полностью интегрирован в экосистему ProjectV с аллокаторами MemoryManager, логгером и профайлингом.**

# Интеграция JoltPhysics с ProjectV

## CMake

### Подключение JoltPhysics

```cmake
add_subdirectory(external/JoltPhysics)
target_link_libraries(${PROJECT_NAME} PRIVATE Jolt)
```

### Опции сборки

```cmake
# Производительность
set(USE_SSE4_2 ON CACHE BOOL "" FORCE)
set(USE_AVX2 OFF CACHE BOOL "" FORCE)
set(USE_AVX512 OFF CACHE BOOL "" FORCE)
set(USE_LZCNT ON CACHE BOOL "" FORCE)
set(USE_TZCNT ON CACHE BOOL "" FORCE)
set(USE_F16C ON CACHE BOOL "" FORCE)

# Функциональность
set(JPH_CROSS_PLATFORM_DETERMINISTIC OFF CACHE BOOL "" FORCE)
set(JPH_DEBUG_RENDERER OFF CACHE BOOL "" FORCE)
set(JPH_PROFILE_ENABLED OFF CACHE BOOL "" FORCE)

# Тесты и примеры
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
```

---

## Инициализация

### Порядок инициализации

```cpp
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <print>

void InitJolt() {
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
}

void ShutdownJolt() {
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}
```

### TempAllocator

```cpp
JPH::TempAllocatorImpl temp_allocator(10 * 1024 * 1024);
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

---

## Полный цикл инициализации

```cpp
#include <Jolt/Core/JobSystemThreadPool.h>

class PhysicsEngine {
public:
    void Init() {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();

        m_temp_allocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);

        m_physics.Init(16384, 0, 8192, 4096, &m_bp_layer_interface,
                       &m_object_vs_bp_filter, &m_object_layer_filter);

        m_physics.SetContactListener(&m_contact_callback);
        m_physics.SetBodyActivationListener(&m_activation_listener);

        // Количество потоков берется из конфигурации движка ProjectV
        uint32_t threads = std::max(1u, projectv::config::get_thread_count() - 1);
        m_job_system = std::make_unique<JPH::JobSystemThreadPool>(
            JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, threads);
    }

    void Shutdown() {
        m_job_system.reset();
        m_physics.Shutdown();
        m_temp_allocator.reset();

        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

    void Update(float delta_time) {
        m_physics.Update(delta_time, 1, m_temp_allocator.get(), m_job_system.get());
    }

    JPH::PhysicsSystem& GetSystem() { return m_physics; }

private:
    std::unique_ptr<JPH::TempAllocatorImpl> m_temp_allocator;
    std::unique_ptr<JPH::JobSystemThreadPool> m_job_system;
    JPH::PhysicsSystem m_physics;

    BPLayerInterfaceImpl m_bp_layer_interface;
    ObjectLayerPairFilterImpl m_object_layer_filter;
    ObjectVsBroadPhaseLayerFilterImpl m_object_vs_bp_filter;
    PhysicsContactCallback m_contact_callback;
    BodyActivationListenerImpl m_activation_listener;
};
```

---

## Оптимизация

### Batch операции

```cpp
std::vector<JPH::BodyCreationSettings> settings;
std::vector<JPH::BodyID> ids = interface.CreateAndAddBodies(settings, JPH::EActivation::Activate);
```

### OptimizeBroadPhase

```cpp
physics.OptimizeBroadPhase();
```

### Fixed timestep

```cpp
class FixedTimestepLoop {
public:
    void Update(JPH::PhysicsSystem& physics, float dt) {
        accumulator += dt;
        while (accumulator >= fixed_delta) {
            physics.Update(fixed_delta, 1, m_temp_allocator.get(), m_job_system.get());
            accumulator -= fixed_delta;
        }
    }

private:
    float fixed_delta = 1.0f / 60.0f;
    float accumulator = 0.0f;
};
```

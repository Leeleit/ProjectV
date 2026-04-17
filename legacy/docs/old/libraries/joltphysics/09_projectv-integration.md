# Интеграция JoltPhysics с ProjectV

🔴 **Уровень 3: Продвинутый**

## Обзор

Этот документ описывает интеграцию JoltPhysics с компонентами ProjectV: flecs ECS, Vulkan рендерер, Tracy профилировщик,
glm математика.

---

## ECS интеграция (flecs)

### Компоненты

```cpp
#include <flecs.h>

// Компонент физического тела
struct PhysicsBody {
    JPH::BodyID body_id;
    bool is_kinematic = false;
};

// Компонент свойств физики
struct PhysicsProperties {
    float mass = 1.0f;
    float friction = 0.5f;
    float restitution = 0.3f;
};

// Компонент для синхронизации
struct PhysicsDirty {
    bool position = false;
    bool rotation = false;
    bool velocity = false;
};
```

### Регистрация компонентов

```cpp
void RegisterPhysicsComponents(flecs::world& world)
{
    world.component<PhysicsBody>();
    world.component<PhysicsProperties>();
    world.component<PhysicsDirty>();
}
```

### Система синхронизации Jolt → ECS

```cpp
void CreatePhysicsSyncSystem(flecs::world& world, JPH::PhysicsSystem& physics_system)
{
    JPH::BodyInterface& interface = physics_system.GetBodyInterfaceNoLock();

    // Система PostUpdate: синхронизация Jolt → ECS
    world.system<PhysicsBody, Transform>("SyncPhysicsToECS")
        .kind(flecs::PostUpdate)
        .each([&interface](PhysicsBody& body, Transform& transform) {
            if (body.body_id.IsInvalid() || !interface.IsActive(body.body_id))
                return;

            JPH::RVec3 pos = interface.GetCenterOfMassPosition(body.body_id);
            JPH::Quat rot = interface.GetRotation(body.body_id);

            transform.position = glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
            transform.rotation = glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());
        });
}
```

### Система синхронизации ECS → Jolt

```cpp
void CreateKinematicSyncSystem(flecs::world& world, JPH::PhysicsSystem& physics_system)
{
    JPH::BodyInterface& interface = physics_system.GetBodyInterfaceNoLock();

    // Система PreUpdate: синхронизация ECS → Jolt (для кинематических тел)
    world.system<const Transform, PhysicsBody>("SyncECSToPhysics")
        .kind(flecs::PreUpdate)
        .each([&interface](const Transform& transform, PhysicsBody& body) {
            if (body.body_id.IsInvalid() || !body.is_kinematic)
                return;

            interface.SetPositionAndRotation(
                body.body_id,
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
void CreatePhysicsCleanupObserver(flecs::world& world, JPH::PhysicsSystem& physics_system)
{
    JPH::BodyInterface& interface = physics_system.GetBodyInterfaceNoLock();

    world.observer<PhysicsBody>()
        .event(flecs::OnRemove)
        .each([&interface](PhysicsBody& body) {
            if (!body.body_id.IsInvalid()) {
                interface.RemoveBody(body.body_id);
                interface.DestroyBody(body.body_id);
                body.body_id = JPH::BodyID();
            }
        });
}
```

---

## Интеграция с glm

### Конвертация векторов

```cpp
namespace PhysicsMath
{
    // glm → Jolt
    JPH::Vec3 ToJolt(const glm::vec3& v) {
        return JPH::Vec3(v.x, v.y, v.z);
    }

    JPH::RVec3 ToJoltR(const glm::vec3& v) {
        return JPH::RVec3(v.x, v.y, v.z);
    }

    // Jolt → glm
    glm::vec3 ToGlm(const JPH::Vec3& v) {
        return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
    }

    glm::vec3 ToGlm(const JPH::RVec3& v) {
        return glm::vec3(static_cast<float>(v.GetX()),
                        static_cast<float>(v.GetY()),
                        static_cast<float>(v.GetZ()));
    }
}
```

### Конвертация кватернионов

```cpp
namespace PhysicsMath
{
    // glm → Jolt
    JPH::Quat ToJolt(const glm::quat& q) {
        return JPH::Quat(q.x, q.y, q.z, q.w);
    }

    // Jolt → glm
    glm::quat ToGlm(const JPH::Quat& q) {
        return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
    }
}
```

### Конвертация матриц

```cpp
namespace PhysicsMath
{
    glm::mat4 ToGlm(const JPH::Mat44& m)
    {
        glm::mat4 result;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result[i][j] = m(i, j);
            }
        }
        return result;
    }

    JPH::Mat44 ToJolt(const glm::mat4& m)
    {
        JPH::Mat44 result;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result(i, j) = m[i][j];
            }
        }
        return result;
    }
}
```

---

## Интеграция с Tracy

### Профилирование обновления физики

```cpp
#include <tracy/Tracy.hpp>

void UpdatePhysics(float delta_time)
{
    ZoneScopedN("Physics::Update");

    {
        ZoneScopedN("Physics::Step");
        physics_system.Update(delta_time, collision_steps, &temp_allocator, &job_system);
    }

    {
        ZoneScopedN("Physics::SyncToECS");
        SyncPhysicsToECS();
    }

    // Статистика
    TracyPlot("Physics/ActiveBodies", physics_system.GetNumActiveBodies());
    TracyPlot("Physics/TotalBodies", physics_system.GetNumBodies());
}
```

### Профилирование создания тел

```cpp
JPH::BodyID CreatePhysicsBody(const JPH::BodyCreationSettings& settings)
{
    ZoneScopedN("Physics::CreateBody");

    auto& interface = physics_system.GetBodyInterfaceNoLock();
    JPH::BodyID body_id = interface.CreateAndAddBody(settings, JPH::EActivation::Activate);

    TracyPlot("Physics/TotalBodies", physics_system.GetNumBodies());
    return body_id;
}
```

---

## Интеграция с Vulkan

### Storage Buffer для трансформаций

```cpp
struct PhysicsTransformData {
    glm::mat4 model_matrix;
    uint32_t body_id;
    uint32_t flags;
    float _pad[2];
};

class PhysicsVulkanBridge
{
public:
    void UpdateTransformBuffer(VmaAllocator allocator, VkBuffer buffer)
    {
        ZoneScopedN("Physics::UpdateTransformBuffer");

        std::vector<PhysicsTransformData> transform_data;

        JPH::BodyInterface& interface = physics_system.GetBodyInterfaceNoLock();
        JPH::BodyIDVector body_ids;
        physics_system.GetBodies(body_ids);

        for (const JPH::BodyID& id : body_ids) {
            if (!interface.IsAdded(id)) continue;

            PhysicsTransformData data;
            data.model_matrix = CalculateModelMatrix(interface, id);
            data.body_id = id.GetIndexAndSequenceNumber();
            data.flags = 0;

            transform_data.push_back(data);
        }

        // Обновление GPU буфера
        void* mapped;
        vmaMapMemory(allocator, transform_allocation, &mapped);
        memcpy(mapped, transform_data.data(), transform_data.size() * sizeof(PhysicsTransformData));
        vmaUnmapMemory(allocator, transform_allocation);
    }

private:
    glm::mat4 CalculateModelMatrix(JPH::BodyInterface& interface, const JPH::BodyID& id)
    {
        JPH::RVec3 pos = interface.GetPosition(id);
        JPH::Quat rot = interface.GetRotation(id);
        JPH::Vec3 scale = JPH::Vec3::sReplicate(1.0f);  // Или из компонента

        JPH::Mat44 jolt_matrix = JPH::Mat44::sRotationTranslation(rot, pos);

        return PhysicsMath::ToGlm(jolt_matrix);
    }
};
```

### Интерполяция для плавного рендеринга

```cpp
class PhysicsInterpolator
{
public:
    void RecordState()
    {
        previous_transforms = current_transforms;
        current_transforms.clear();

        JPH::BodyInterface& interface = physics_system.GetBodyInterfaceNoLock();
        JPH::BodyIDVector body_ids;
        physics_system.GetBodies(body_ids);

        for (const JPH::BodyID& id : body_ids) {
            current_transforms[id.GetIndex()] = {
                .position = interface.GetPosition(id),
                .rotation = interface.GetRotation(id)
            };
        }
    }

    glm::mat4 GetInterpolatedTransform(uint32_t body_index, float alpha)
    {
        auto prev_it = previous_transforms.find(body_index);
        auto curr_it = current_transforms.find(body_index);

        if (prev_it == previous_transforms.end() || curr_it == current_transforms.end()) {
            return glm::mat4(1.0f);
        }

        const auto& prev = prev_it->second;
        const auto& curr = curr_it->second;

        // Интерполяция позиции
        JPH::RVec3 pos = prev.position + (curr.position - prev.position) * alpha;

        // Сферическая интерполяция вращения
        JPH::Quat rot = JPH::Quat::sSlerp(prev.rotation, curr.rotation, alpha);

        return PhysicsMath::ToGlm(JPH::Mat44::sRotationTranslation(rot, pos));
    }

private:
    struct TransformState {
        JPH::RVec3 position;
        JPH::Quat rotation;
    };

    std::unordered_map<uint32_t, TransformState> previous_transforms;
    std::unordered_map<uint32_t, TransformState> current_transforms;
};
```

---

## Полный класс интеграции

```cpp
class ProjectVPhysicsSystem
{
public:
    void Init(flecs::world& world)
    {
        // Инициализация Jolt
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();

        // Создание системы
        physics_system.Init(
            16384,  // max_bodies
            0,      // num_body_mutexes
            8192,   // max_body_pairs
            4096,   // max_contact_constraints
            &bp_layer_interface,
            &object_vs_bp_layer_filter,
            &object_layer_pair_filter
        );

        physics_system.SetGravity(JPH::Vec3(0.0f, -15.0f, 0.0f));

        // Временный аллокатор
        temp_allocator = std::make_unique<JPH::TempAllocatorImpl>(32 * 1024 * 1024);

        // Регистрация ECS систем
        RegisterPhysicsComponents(world);
        CreatePhysicsSyncSystem(world, physics_system);
        CreateKinematicSyncSystem(world, physics_system);
        CreatePhysicsCleanupObserver(world, physics_system);
    }

    void Update(float delta_time)
    {
        ZoneScopedN("Physics::Update");

        // Fixed timestep
        accumulator += delta_time;

        while (accumulator >= fixed_dt) {
            physics_system.Update(fixed_dt, collision_steps, temp_allocator.get(), nullptr);
            accumulator -= fixed_dt;
        }

        // Интерполяция для рендеринга
        interpolator.RecordState();
    }

    void Shutdown()
    {
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

private:
    JPH::PhysicsSystem physics_system;
    std::unique_ptr<JPH::TempAllocatorImpl> temp_allocator;

    float fixed_dt = 1.0f / 120.0f;
    float accumulator = 0.0f;
    int collision_steps = 1;

    PhysicsInterpolator interpolator;
};
```

---

## Координатные системы

### Y-up vs Y-down

JoltPhysics использует Y-up (Y направлен вверх). Vulkan по умолчанию использует Y-down для NDC.

```cpp
// Конвертация для рендеринга
glm::mat4 GetRenderTransform(JPH::BodyInterface& interface, const JPH::BodyID& id)
{
    JPH::RVec3 pos = interface.GetPosition(id);
    JPH::Quat rot = interface.GetRotation(id);

    // Матрица модели
    glm::mat4 model = glm::translate(glm::mat4(1.0f), PhysicsMath::ToGlm(pos));
    model *= glm::mat4_cast(PhysicsMath::ToGlm(rot));

    return model;
}
```

---

## Пример использования

```cpp
// Создание сущности с физикой
flecs::entity CreatePhysicsEntity(
    flecs::world& world,
    JPH::PhysicsSystem& physics_system,
    const glm::vec3& position,
    const glm::quat& rotation)
{
    // Создание формы
    JPH::BoxShapeSettings shape_settings(JPH::Vec3(0.5f, 0.5f, 0.5f));
    JPH::ShapeRefC shape = shape_settings.Create().Get();

    // Настройки тела
    JPH::BodyCreationSettings body_settings(
        shape,
        PhysicsMath::ToJoltR(position),
        PhysicsMath::ToJolt(rotation),
        JPH::EMotionType::Dynamic,
        0  // object layer
    );

    // Создание тела
    JPH::BodyID body_id = physics_system.GetBodyInterfaceNoLock().CreateAndAddBody(
        body_settings, JPH::EActivation::Activate
    );

    // Создание сущности
    return world.entity()
        .set<PhysicsBody>({body_id})
        .set<Transform>({position, rotation})
        .set<PhysicsProperties>({1.0f, 0.5f, 0.3f});
}

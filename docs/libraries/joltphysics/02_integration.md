## Интеграция JoltPhysics

<!-- anchor: 03_integration -->


## CMake

### Базовое подключение

```cmake
add_subdirectory(external/JoltPhysics)
target_link_libraries(YourApp PRIVATE Jolt)
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

### Опции для разных конфигураций

```cmake
# Debug
set(JPH_DEBUG_RENDERER ON CACHE BOOL "" FORCE)
set(JPH_PROFILE_ENABLED ON CACHE BOOL "" FORCE)

# Release
set(JPH_DEBUG_RENDERER OFF CACHE BOOL "" FORCE)
set(JPH_PROFILE_ENABLED OFF CACHE BOOL "" FORCE)
```

---

## Инициализация

### Порядок инициализации

```cpp
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/PhysicsSystem.h>

void InitJolt() {
    // 1. Регистрация аллокатора (обязательно первой)
    JPH::RegisterDefaultAllocator();

    // 2. Создание фабрики (обязательно второй)
    JPH::Factory::sInstance = new JPH::Factory();

    // 3. Регистрация типов (обязательно третьей)
    JPH::RegisterTypes();
}
```

### Кастомный аллокатор

```cpp
// Опционально: использовать свой аллокатор
void* MyAllocate(size_t inSize) {
    return_aligned_alloc(64, inSize);  // 64-байтное выравнивание
}

void MyFree(void* inBlock) {
    aligned_free(inBlock);
}

// Установить до RegisterDefaultAllocator()
JPH::Allocate = MyAllocate;
JPH::Free = MyFree;
```

### Очистка

```cpp
void ShutdownJolt() {
    // Удаление фабрики
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}
```

---

## TempAllocator

### Назначение

`TempAllocator` выделяет временную память для симуляции. Память освобождается после каждого кадра.

### TempAllocatorImpl

```cpp
#include <Jolt/Core/TempAllocator.h>

// Фиксированный размер (например, 10 MB)
JPH::TempAllocatorImpl temp_allocator(10 * 1024 * 1024);

physics_system.Update(delta_time, 1, &temp_allocator, nullptr);
```

### Рекомендуемые размеры

| Сложность сцены        | Размер аллокатора |
|------------------------|-------------------|
| Простая (< 100 тел)    | 1 MB              |
| Средняя (100-1000 тел) | 10 MB             |
| Сложная (> 1000 тел)   | 32 MB             |

### TempAllocatorMalloc

Для тестирования (использует malloc/free):

```cpp
JPH::TempAllocatorMalloc temp_allocator;
```

---

## Полная интеграция со слоями

### Определение слоёв

```cpp
// Object Layers (логические слои)
namespace Layers
{
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
}

// BroadPhase Layers (физические слои для оптимизации)
namespace BroadPhaseLayers
{
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr uint32_t NUM_LAYERS(2);
}
```

### Реализация интерфейсов

```cpp
// Фильтр столкновений между объектными слоями
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
{
public:
    bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
    {
        switch (inObject1)
        {
        case Layers::NON_MOVING:
            return inObject2 == Layers::MOVING;
        case Layers::MOVING:
            return true;
        default:
            return false;
        }
    }
};

// Интерфейс BroadPhase слоёв
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
    BPLayerInterfaceImpl()
    {
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    uint32_t GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
    {
        return mObjectToBroadPhase[inLayer];
    }

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

// Фильтр столкновений объект vs BroadPhase
class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
    {
        switch (inLayer1)
        {
        case Layers::NON_MOVING:
            return inLayer2 == BroadPhaseLayers::MOVING;
        case Layers::MOVING:
            return true;
        default:
            return false;
        }
    }
};
```

### Инициализация PhysicsSystem

```cpp
BPLayerInterfaceImpl bp_layer_interface;
ObjectLayerPairFilterImpl object_layer_pair_filter;
ObjectVsBroadPhaseLayerFilterImpl object_vs_bp_layer_filter;
JPH::TempAllocatorImpl temp_allocator(10 * 1024 * 1024);

JPH::PhysicsSystem physics_system;
physics_system.Init(
    1024,                        // max_bodies
    0,                           // num_body_mutexes
    1024,                        // max_body_pairs
    1024,                        // max_contact_constraints
    &bp_layer_interface,
    &object_vs_bp_layer_filter,
    &object_layer_pair_filter
);
```

---

## JobSystem

### Однопоточный режим

Для простых сцен и отладки:

```cpp
physics_system.Update(delta_time, 1, &temp_allocator, nullptr);
```

### Многопоточный режим

```cpp
#include <Jolt/Core/JobSystemThreadPool.h>

uint32_t num_threads = std::max(1u, std::thread::hardware_concurrency() - 1);
JPH::JobSystemThreadPool job_system(
    JPH::cMaxPhysicsJobs,
    JPH::cMaxPhysicsBarriers,
    num_threads
);

physics_system.Update(delta_time, 1, &temp_allocator, &job_system);
```

### Кастомный JobSystem

Для интеграции с собственным движком:

```cpp
class MyJobSystem : public JPH::JobSystem
{
public:
    JobHandle CreateJob(const char* inName, ColorArg inColor,
                       const JobFunction& inJobFunction,
                       uint32_t inNumDependencies) override
    {
        // Создать задачу в вашем планировщике
    }

    bool IsJobFinished(JobHandle inJob) const override
    {
        // Проверить завершение
    }

    void WaitForJob(JobHandle inJob) override
    {
        // Дождаться завершения
    }
};
```

---

## Цикл симуляции

### Базовый цикл

```cpp
void update(float delta_time) {
    physics_system.Update(
        delta_time,
        1,  // collision_steps
        &temp_allocator,
        &job_system
    );
}
```

### Fixed timestep

```cpp
class PhysicsLoop
{
public:
    void update(float delta_time)
    {
        accumulator += delta_time;

        while (accumulator >= fixed_delta_time)
        {
            physics_system.Update(fixed_delta_time, 1, &temp_allocator, &job_system);
            accumulator -= fixed_delta_time;
        }
    }

private:
    float fixed_delta_time = 1.0f / 60.0f;
    float accumulator = 0.0f;
};
```

### Collision steps

Для стабильности при быстрых движениях:

```cpp
// Больше шагов = точнее, но медленнее
physics_system.Update(delta_time, 4, &temp_allocator, &job_system);
```

---

## BodyInterface

### Получение интерфейса

```cpp
// Без блокировки (в однопоточном режиме)
JPH::BodyInterface& interface = physics_system.GetBodyInterfaceNoLock();

// С блокировкой (в многопоточном режиме)
JPH::BodyInterface& interface = physics_system.GetBodyInterface();
```

### Основные операции

```cpp
// Создание тела
JPH::BodyID body_id = interface.CreateBody(body_settings);

// Добавление в мир
interface.AddBody(body_id, JPH::EActivation::Activate);

// Создание и добавление за один вызов
JPH::BodyID body_id = interface.CreateAndAddBody(body_settings, JPH::EActivation::Activate);

// Удаление
interface.RemoveBody(body_id);
interface.DestroyBody(body_id);

// Активация/деактивация
interface.ActivateBody(body_id);
interface.DeactivateBody(body_id);
```

### Получение данных

```cpp
// Позиция и вращение
JPH::RVec3 position = interface.GetCenterOfMassPosition(body_id);
JPH::RVec3 position = interface.GetPosition(body_id);  // Без учёта центра масс
JPH::Quat rotation = interface.GetRotation(body_id);

// Скорости
JPH::Vec3 linear_velocity = interface.GetLinearVelocity(body_id);
JPH::Vec3 angular_velocity = interface.GetAngularVelocity(body_id);

// Свойства
float mass = interface.GetMass(body_id);
JPH::AABox bounds = interface.GetWorldSpaceBounds(body_id);
bool is_active = interface.IsActive(body_id);
```

### Установка данных

```cpp
// Позиция и вращение
interface.SetPosition(body_id, JPH::RVec3(0, 10, 0), JPH::EActivation::Activate);
interface.SetRotation(body_id, JPH::Quat::sRotation(JPH::Vec3::sAxisY(), JPH::JPH_PI), JPH::EActivation::Activate);
interface.SetPositionAndRotation(body_id, position, rotation, JPH::EActivation::Activate);

// Скорости
interface.SetLinearVelocity(body_id, JPH::Vec3(0, 10, 0));
interface.SetAngularVelocity(body_id, JPH::Vec3(0, 1, 0));

// Силы и импульсы
interface.AddForce(body_id, JPH::Vec3(0, 100, 0));
interface.AddImpulse(body_id, JPH::Vec3(0, 10, 0));
interface.AddTorque(body_id, JPH::Vec3(0, 1, 0));
interface.AddAngularImpulse(body_id, JPH::Vec3(0, 0.5f, 0));
```

---

## Оптимизация

### Batch операции

```cpp
// Добавление нескольких тел
std::vector<JPH::BodyCreationSettings> settings;
JPH::BodyIDVector body_ids = interface.CreateAndAddBodies(settings, JPH::EActivation::Activate);

// Удаление нескольких тел
interface.RemoveBodies(body_ids);
interface.DestroyBodies(body_ids);
```

### OptimizeBroadPhase

```cpp
// Вызвать после загрузки уровня
physics_system.OptimizeBroadPhase();
```

### Параметры для сложных сцен

```cpp
physics_system.Init(
    16384,   // max_bodies
    0,       // num_body_mutexes
    8192,    // max_body_pairs
    4096,    // max_contact_constraints
    ...
);

---

## Интеграция JoltPhysics с ProjectV

<!-- anchor: 09_projectv-integration -->


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

---

## Воксельные паттерны JoltPhysics для ProjectV

<!-- anchor: 10_projectv-patterns -->


## Обзор

Паттерны использования JoltPhysics для воксельного движка: HeightField для ландшафта, чанкирование физики, динамические
воксели, жидкости.

---

## HeightField для воксельного ландшафта

### Создание HeightField из чанка

```cpp
JPH::ShapeRefC CreateHeightFieldFromChunk(const VoxelChunk& chunk)
{
    const uint32_t size = chunk.GetSize();
    std::vector<float> heights(size * size);

    // Извлечение высот из воксельных данных
    for (uint32_t z = 0; z < size; ++z) {
        for (uint32_t x = 0; x < size; ++x) {
            float height = chunk.GetHighestVoxel(x, z);
            heights[z * size + x] = height;
        }
    }

    JPH::HeightFieldShapeSettings settings(
        heights.data(),
        JPH::Vec3(chunk.GetWorldPosition().x, 0, chunk.GetWorldPosition().z),
        JPH::Vec3(chunk.GetVoxelSize(), 1.0f, chunk.GetVoxelSize()),
        size
    );

    // Оптимизация для дальних чанков
    settings.mBlockSize = 4;
    settings.mBitsPerSample = 8;

    return settings.Create().Get();
}
```

### Дыры в HeightField (пещеры)

```cpp
void AddCaveEntrance(std::vector<float>& heights, uint32_t size, uint32_t x, uint32_t z, float radius)
{
    const float NO_COLLISION = JPH::HeightFieldShapeConstants::cNoCollisionValue;

    for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dz * dz <= radius * radius) {
                uint32_t px = x + dx;
                uint32_t pz = z + dz;
                if (px < size && pz < size) {
                    heights[pz * size + px] = NO_COLLISION;
                }
            }
        }
    }
}
```

---

## Чанкирование физики

### Структура чанка физики

```cpp
struct PhysicsChunk
{
    glm::ivec3 coords;
    JPH::BodyID terrain_body;
    std::vector<JPH::BodyID> dynamic_bodies;
    bool is_active = false;
    float distance_to_player = 0.0f;

    // LOD для физики
    enum class LOD { Full, Simplified, BoundsOnly };
    LOD current_lod = LOD::Full;
};
```

### Управление чанками

```cpp
class PhysicsChunkManager
{
public:
    void Update(const glm::vec3& player_position)
    {
        // Обновление дистанций
        for (auto& [coords, chunk] : chunks) {
            chunk.distance_to_player = glm::distance(
                glm::vec3(coords) * chunk_size,
                player_position
            );
        }

        // Активация/деактивация
        for (auto& [coords, chunk] : chunks) {
            if (chunk.distance_to_player < active_radius && !chunk.is_active) {
                ActivateChunk(chunk);
            } else if (chunk.distance_to_player > active_radius * 1.5f && chunk.is_active) {
                DeactivateChunk(chunk);
            }
        }

        // LOD обновление
        for (auto& [coords, chunk] : chunks) {
            PhysicsChunk::LOD new_lod = CalculateLOD(chunk.distance_to_player);
            if (new_lod != chunk.current_lod) {
                UpdateChunkLOD(chunk, new_lod);
            }
        }
    }

private:
    std::unordered_map<glm::ivec3, PhysicsChunk> chunks;
    float chunk_size = 32.0f;
    float active_radius = 100.0f;

    void ActivateChunk(PhysicsChunk& chunk)
    {
        auto& interface = physics_system.GetBodyInterfaceNoLock();
        interface.AddBody(chunk.terrain_body, JPH::EActivation::DontActivate);

        for (auto& body_id : chunk.dynamic_bodies) {
            interface.AddBody(body_id, JPH::EActivation::Activate);
        }

        chunk.is_active = true;
    }

    void DeactivateChunk(PhysicsChunk& chunk)
    {
        auto& interface = physics_system.GetBodyInterfaceNoLock();
        interface.RemoveBody(chunk.terrain_body);

        for (auto& body_id : chunk.dynamic_bodies) {
            interface.RemoveBody(body_id);
        }

        chunk.is_active = false;
    }

    PhysicsChunk::LOD CalculateLOD(float distance)
    {
        if (distance < 50.0f) return PhysicsChunk::LOD::Full;
        if (distance < 100.0f) return PhysicsChunk::LOD::Simplified;
        return PhysicsChunk::LOD::BoundsOnly;
    }
};
```

---

## Динамические воксели

### Разрушаемые объекты

```cpp
class DestructibleVoxelObject
{
public:
    void Initialize(const std::vector<Voxel>& voxels, float voxel_size)
    {
        // Создание составной формы
        JPH::StaticCompoundShapeSettings compound_settings;

        for (const auto& voxel : voxels) {
            JPH::BoxShapeSettings box_settings(JPH::Vec3(voxel_size * 0.5f));
            auto box_result = box_settings.Create();

            if (box_result.IsValid()) {
                compound_settings.AddShape(
                    JPH::Vec3(voxel.position.x, voxel.position.y, voxel.position.z),
                    JPH::Quat::sIdentity(),
                    box_result.Get()
                );
            }
        }

        auto shape_result = compound_settings.Create();
        if (!shape_result.IsValid()) return;

        // Создание тела
        JPH::BodyCreationSettings body_settings(
            shape_result.Get(),
            JPH::RVec3::sZero(),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic,
            Layers::MOVING
        );

        body_id = physics_system.GetBodyInterfaceNoLock().CreateAndAddBody(
            body_settings, JPH::EActivation::Activate
        );
    }

    void ApplyExplosion(const glm::vec3& center, float force)
    {
        // Удаление старого тела
        auto& interface = physics_system.GetBodyInterfaceNoLock();
        JPH::RVec3 current_pos = interface.GetPosition(body_id);
        interface.RemoveBody(body_id);
        interface.DestroyBody(body_id);

        // Создание отдельных тел для каждого вокселя
        for (const auto& voxel : affected_voxels) {
            glm::vec3 world_pos = current_world_pos + voxel.position;

            JPH::BoxShapeSettings box_settings(JPH::Vec3(voxel_size * 0.5f));
            auto shape = box_settings.Create().Get();

            JPH::BodyCreationSettings debris_settings(
                shape,
                JPH::RVec3(world_pos.x, world_pos.y, world_pos.z),
                JPH::Quat::sIdentity(),
                JPH::EMotionType::Dynamic,
                Layers::MOVING
            );

            JPH::BodyID debris_id = interface.CreateAndAddBody(
                debris_settings, JPH::EActivation::Activate
            );

            // Импульс от взрыва
            glm::vec3 dir = glm::normalize(world_pos - center);
            interface.AddImpulse(debris_id, JPH::Vec3(dir.x, dir.y, dir.z) * force);
        }
    }

private:
    JPH::BodyID body_id;
    std::vector<Voxel> affected_voxels;
    float voxel_size;
};
```

---

## Character Controller для воксельного мира

### Кастомный controller

```cpp
class VoxelCharacterController
{
public:
    void Initialize(JPH::PhysicsSystem* system)
    {
        JPH::CharacterVirtualSettings settings;
        settings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
        settings.mMaxStrength = 100.0f;
        settings.mMass = 70.0f;

        // Капсула для персонажа
        settings.mShape = JPH::RotatedTranslatedShapeSettings(
            JPH::Vec3(0, 0.9f, 0),  // Смещение для центра масс
            JPH::Quat::sIdentity(),
            new JPH::CapsuleShape(0.5f, 0.3f)
        ).Create().Get();

        character = new JPH::CharacterVirtual(
            &settings,
            JPH::RVec3::sZero(),
            JPH::Quat::sIdentity(),
            system
        );
    }

    void Update(float delta_time, const glm::vec3& movement_input, bool jump)
    {
        // Применение движения
        JPH::Vec3 desired_velocity = JPH::Vec3(movement_input.x, 0, movement_input.z) * move_speed;

        // Прыжок
        if (jump && character->IsOnGround()) {
            desired_velocity.SetY(jump_velocity);
        } else {
            desired_velocity.SetY(character->GetLinearVelocity().GetY());
        }

        character->SetLinearVelocity(desired_velocity);

        // Обновление
        character->Update(
            delta_time,
            JPH::Vec3(0, -20.0f, 0),  // Гравитация
            broad_phase_filter,
            object_layer_filter,
            body_filter,
            shape_filter,
            *temp_allocator
        );
    }

    glm::vec3 GetPosition() const
    {
        JPH::RVec3 pos = character->GetPosition();
        return glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
    }

private:
    JPH::CharacterVirtual* character;
    float move_speed = 5.0f;
    float jump_velocity = 8.0f;
};
```

---

## Асинхронное обновление коллайдеров

### Система очереди обновлений

```cpp
class AsyncColliderUpdater
{
public:
    struct UpdateTask
    {
        glm::ivec3 chunk_coords;
        std::vector<VoxelModification> modifications;
        uint32_t priority;
    };

    void ScheduleUpdate(const glm::ivec3& coords, const std::vector<VoxelModification>& mods, uint32_t priority)
    {
        std::lock_guard<std::mutex> lock(queue_mutex);

        UpdateTask task{coords, mods, priority};
        update_queue.push(task);
    }

    void ProcessUpdates()
    {
        std::lock_guard<std::mutex> lock(queue_mutex);

        while (!update_queue.empty()) {
            auto task = update_queue.top();
            update_queue.pop();

            // Обновление HeightField для чанка
            UpdateChunkCollider(task);
        }
    }

private:
    std::priority_queue<UpdateTask> update_queue;
    std::mutex queue_mutex;

    void UpdateChunkCollider(const UpdateTask& task)
    {
        // Пересоздание HeightField с учётом модификаций
        auto chunk = GetChunk(task.chunk_coords);
        auto new_shape = CreateHeightFieldFromChunk(chunk);

        auto& interface = physics_system.GetBodyInterfaceNoLock();
        interface.SetShape(
            chunk->terrain_body,
            new_shape,
            false,
            JPH::EActivation::DontActivate
        );
    }
};
```

---

## Оптимизация для больших миров

### Приоритизация по расстоянию

```cpp
uint32_t CalculateUpdatePriority(const glm::ivec3& chunk_coords, const glm::vec3& player_pos)
{
    float distance = glm::distance(glm::vec3(chunk_coords) * 32.0f, player_pos);

    uint32_t priority = 0;

    if (distance < 30.0f) priority += 10;  // Очень близко
    else if (distance < 60.0f) priority += 7;
    else if (distance < 100.0f) priority += 4;
    else if (distance < 200.0f) priority += 1;

    return priority;
}
```

### Batch обновления

```cpp
void BatchProcessModifications()
{
    // Группировка по чанкам
    std::unordered_map<glm::ivec3, std::vector<VoxelModification>> batched;

    for (const auto& mod : pending_modifications) {
        batched[mod.chunk_coords].push_back(mod);
    }

    // Создание задач
    for (const auto& [coords, mods] : batched) {
        uint32_t priority = CalculateUpdatePriority(coords, player_position);
        collider_updater.ScheduleUpdate(coords, mods, priority);
    }

    pending_modifications.clear();
}
```

---

## Детерминированная симуляция для мультиплеера

```cpp
class DeterministicPhysics
{
public:
    void Initialize()
    {
        // Включение детерминизма (требует JPH_CROSS_PLATFORM_DETERMINISTIC)
        physics_system.Init(...);

        // Фиксированный seed
        physics_system.SetDeterministicSimulationSeed(0x12345678);

        // Fixed timestep обязателен
        fixed_dt = 1.0f / 60.0f;
    }

    void Update(float delta_time)
    {
        accumulator += delta_time;

        while (accumulator >= fixed_dt) {
            // Детерминированный шаг
            physics_system.Update(fixed_dt, 1, temp_allocator, nullptr);
            accumulator -= fixed_dt;

            // Сохранение состояния для сетевой синхронизации
            RecordPhysicsState();
        }
    }

    struct PhysicsSnapshot
    {
        uint32_t frame;
        std::vector<BodyState> bodies;

        uint64_t CalculateHash() const
        {
            uint64_t hash = frame;
            for (const auto& body : bodies) {
                hash ^= body.CalculateHash();
            }
            return hash;
        }
    };

private:
    float fixed_dt;
    float accumulator = 0.0f;

    void RecordPhysicsState()
    {
        PhysicsSnapshot snapshot;
        snapshot.frame = current_frame++;

        // Сбор состояний всех тел
        JPH::BodyIDVector body_ids;
        physics_system.GetBodies(body_ids);

        auto& interface = physics_system.GetBodyInterfaceNoLock();
        for (const auto& id : body_ids) {
            BodyState state;
            state.id = id.GetIndexAndSequenceNumber();
            state.position = interface.GetPosition(id);
            state.rotation = interface.GetRotation(id);
            state.velocity = interface.GetLinearVelocity(id);

            snapshot.bodies.push_back(state);
        }

        // Хеш для проверки согласованности
        uint64_t hash = snapshot.CalculateHash();
        SendHashToServer(hash);
    }
};
```

---

## Слои для воксельного мира

```cpp
namespace VoxelLayers
{
    // Object Layers
    static constexpr JPH::ObjectLayer TERRAIN = 0;      // Статический ландшафт
    static constexpr JPH::ObjectLayer DYNAMIC = 1;      // Динамические объекты
    static constexpr JPH::ObjectLayer PLAYER = 2;       // Игрок
    static constexpr JPH::ObjectLayer SENSOR = 3;       // Триггеры
    static constexpr JPH::ObjectLayer DEBRIS = 4;       // Обломки
    static constexpr JPH::ObjectLayer NUM_LAYERS = 5;

    // BroadPhase Layers
    static constexpr JPH::BroadPhaseLayer BP_TERRAIN(0);
    static constexpr JPH::BroadPhaseLayer BP_DYNAMIC(1);
    static constexpr JPH::BroadPhaseLayer BP_PLAYER(2);
    static constexpr uint32_t BP_NUM_LAYERS = 3;
}

// Фильтр коллизий
class VoxelLayerFilter : public JPH::ObjectLayerPairFilter
{
public:
    bool ShouldCollide(JPH::ObjectLayer layer1, JPH::ObjectLayer layer2) const override
    {
        switch (layer1) {
        case VoxelLayers::TERRAIN:
            return layer2 != VoxelLayers::TERRAIN;  // Terrain не коллизит сам с собой

        case VoxelLayers::DYNAMIC:
        case VoxelLayers::DEBRIS:
            return true;  // Сталкивается со всем

        case VoxelLayers::PLAYER:
            return layer2 == VoxelLayers::TERRAIN ||
                   layer2 == VoxelLayers::DYNAMIC ||
                   layer2 == VoxelLayers::SENSOR;

        case VoxelLayers::SENSOR:
            return layer2 == VoxelLayers::PLAYER;  // Только с игроком

        default:
            return false;
        }
    }
};

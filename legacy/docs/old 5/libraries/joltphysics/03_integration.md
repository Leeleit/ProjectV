# Интеграция JoltPhysics

🟡 **Уровень 2: Средний**

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

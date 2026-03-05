# Быстрый старт JoltPhysics

**🟢 Уровень 1: Начинающий**

## Шаг 1: CMake

```cmake
add_subdirectory(external/JoltPhysics)
target_link_libraries(YourApp PRIVATE Jolt)
```

## Шаг 2: Инициализация

```cpp
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>

// Регистрация аллокаторов и типов (обязательно)
JPH::RegisterDefaultAllocator();
JPH::Factory::sInstance = new JPH::Factory();
JPH::RegisterTypes();

// Создание системы
JPH::PhysicsSystem physics_system;
physics_system.Init(...);
```

## Шаг 3: Создание тела

```cpp
// Настройки формы
JPH::BoxShapeSettings box_settings(JPH::Vec3(1.0f, 1.0f, 1.0f));
JPH::ShapeRefC box_shape = box_settings.Create().Get();

// Настройки тела
JPH::BodyCreationSettings body_settings(
    box_shape,
    JPH::Vec3(0, 10, 0),
    JPH::Quat::sIdentity(),
    JPH::EMotionType::Dynamic,
    Layers::MOVING
);

// Создание и добавление
JPH::BodyID body_id = interface.CreateAndAddBody(body_settings, JPH::EActivation::Activate);
```

## Шаг 4: Шаг симуляции

```cpp
physics_system.Update(deltaTime, 1, &temp_allocator, &job_system);
```

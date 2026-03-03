# Интеграция JoltPhysics

**🟡 Уровень 2: Средний**

## Оглавление

- [1. CMake](#1-cmake)
- [2. Инициализация памяти](#2-инициализация-памяти)
- [3. Слои (Layers)](#3-слои-layers)
- [4. JobSystem](#4-jobsystem)

---

## 1. CMake

```cmake
# Опции
set(USE_SSE4_2 ON CACHE BOOL "" FORCE)
add_subdirectory(external/JoltPhysics)
target_link_libraries(YourApp PRIVATE Jolt)
```

## 2. Инициализация памяти

Jolt требует регистрации аллокаторов:

```cpp
JPH::RegisterDefaultAllocator();
```

Или свои: `JPH::Allocate = MyAlloc; JPH::Free = MyFree;`.

## 3. Слои (Layers)

Необходимо реализовать `JPH::BroadPhaseLayerInterface` и `JPH::ObjectVsBroadPhaseLayerFilter` для разделения статических
и динамических объектов.

## 4. JobSystem

Для многопоточности создайте `JPH::JobSystemThreadPool`.

```cpp
JPH::JobSystemThreadPool job_system(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, num_threads);
```

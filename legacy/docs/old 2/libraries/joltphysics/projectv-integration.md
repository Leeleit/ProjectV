# Интеграция с ProjectV

**🟡 Уровень 2: Средний**

## На этой странице

- [Архитектура интеграции](#архитектура-интеграции)
- [Воксельная физика и HeightFieldShape](#воксельная-физика-и-heightfieldshape)
- [Интеграция с ECS (flecs)](#интеграция-с-ecs-flecs)
- [Оптимизации для воксельного движка](#оптимизации-для-воксельного-движка)
- [Производительность и настройка](#производительность-и-настройка)
- [Решение проблем (ProjectV-specific)](#решение-проблем-projectv-specific)
- [Терминология для воксельной физики](#терминология-для-воксельной-физики)

---

## Архитектура интеграции

### Общий подход

ProjectV использует JoltPhysics как основной физический движок для симуляции воксельного мира. Интеграция построена на
следующих принципах:

1. **Разделение ответственности**: JoltPhysics обрабатывает коллизии и симуляцию, ProjectV управляет воксельными данными
   и рендерингом.
2. **Слои для вокселей**: Специализированные Object Layers для разных типов воксельных объектов.
3. **ECS-ориентированность**: Полная интеграция с фреймворком flecs для управления сущностями и компонентами.
4. **Детерминированность**: Обязательное требование для сетевой синхронизации воксельного мира.

### Системные слои для ProjectV

```cpp
namespace Layers
{
    static constexpr ObjectLayer VOXEL_TERRAIN = 0;   // Статический воксельный ландшафт
    static constexpr ObjectLayer VOXEL_DYNAMIC = 1;   // Динамические воксельные объекты
    static constexpr ObjectLayer VOXEL_FLUID   = 2;   // Жидкости/сыпучие материалы
    static constexpr ObjectLayer PLAYER        = 3;   // Игрок и NPC
    static constexpr ObjectLayer SENSOR        = 4;   // Триггеры и датчики
    static constexpr ObjectLayer NUM_LAYERS    = 5;
};
```

### Поток данных

```
[Voxel Data] → [HeightField/Mesh Shape] → [JoltPhysics Body] → [ECS Component] → [Vulkan Renderer]
     ↑                                      ↓                    ↓
[Voxel Updates] ← [Collision Results] ← [Physics Simulation] ← [Input/Forces]
```

---

## Воксельная физика и HeightFieldShape

### HeightFieldShape для ландшафта

Для статического ландшафта используйте `HeightFieldShape`. Это экономит память по сравнению с мешами.

```cpp
// Создание HeightField из данных чанка
JPH::HeightFieldShapeSettings settings;
settings.mSampleCount = CHUNK_SIZE;  // Квадратная сетка (например, 32x32)
settings.mHeightSamples = heights.data();  // Массив размером CHUNK_SIZE * CHUNK_SIZE
settings.mScale = JPH::Vec3(voxelSize, 1.0f, voxelSize);
settings.mOffset = JPH::Vec3(chunkX * CHUNK_SIZE * voxelSize, 0, chunkZ * CHUNK_SIZE * voxelSize);

JPH::ShapeSettings::ShapeResult result = settings.Create();
if (result.IsValid()) {
    JPH::BodyCreationSettings body_settings(
        result.Get(),
        JPH::RVec3::sZero(),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Static,
        (JPH::uint8)Layers::VOXEL_TERRAIN
    );

    JPH::BodyID body_id = body_interface.CreateBody(body_settings);
    body_interface.AddBody(body_id, JPH::EActivation::DontActivate);
}
```

### Динамические воксельные объекты

Для разрушаемых объектов или построек используйте `MeshShape` или `MutableCompoundShape`. При изменении вокселей
пересоздавайте форму (асинхронно).

### Параметры HeightFieldShape для вокселей

Настройте `HeightFieldShape` для оптимальной работы с воксельным ландшафтом:

```cpp
JPH::HeightFieldShapeSettings settings;

// Критически важные параметры
settings.mHeightSamples = height_samples;  // Массив sample_count * sample_count значений
settings.mSampleCount = sample_count;      // Количество сэмплов по одной оси (квадратная сетка)
settings.mOffset = JPH::Vec3(0, min_height, 0);  // Смещение всей высотной карты
settings.mScale = JPH::Vec3(voxel_size, vertical_scale, voxel_size);  // Масштаб

// Оптимизация и качество
settings.mBlockSize = 2;                   // Размер блока [2, 8], влияет на память/производительность
settings.mBitsPerSample = 8;               // Битов на сэмпл [1, 8], влияет на точность
settings.mActiveEdgeCosThresholdAngle = 0.996195f;  // cos(5 градусов) по умолчанию

// Материалы (опционально)
if (!material_indices.empty()) {
    settings.mMaterialIndices = material_indices.data();
    settings.mMaterials = material_list;
}

// Минимальная/максимальная высота для компрессии
settings.mMinHeightValue = min_height;     // Искусственный минимум
settings.mMaxHeightValue = max_height;     // Искусственный максимум

// Создание формы с проверкой ошибок
JPH::ShapeResult result = settings.Create();
if (!result.IsValid()) {
    SDL_LogError("Failed to create HeightFieldShape: %s", result.GetError().c_str());
}
```

### Слои и фильтры

ProjectV использует `ObjectLayer` для разделения статики (ландшафт) и динамики (игрок, предметы).

```cpp
namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
}

// BPLayerInterface мапит ObjectLayer на BroadPhaseLayer
```

### Character Controller

Используйте `JPH::CharacterVirtual` для игрока. Это позволяет реализовать плавное движение по воксельным ступенькам (
step height) и корректное взаимодействие с физикой.

```cpp
character->Update(deltaTime, gravity, layerFilter, objectFilter, bodyFilter, shapeFilter, allocator);
```

---

## Интеграция с ECS (flecs)

Для связи физического мира Jolt и игрового мира flecs используются компоненты-обертки.

### Компоненты

```cpp
struct JoltBody {
    JPH::BodyID id;
};

// При удалении компонента удаляем тело из физ. мира
world.observer<JoltBody>().event(flecs::OnRemove)
    .each([physics](JoltBody& b) {
        physics->GetBodyInterface().RemoveBody(b.id);
        physics->GetBodyInterface().DestroyBody(b.id);
    });
```

### Синхронизация трансформаций

Система `PreUpdate` копирует данные из ECS в Jolt (для кинематических тел), а `PostUpdate` — из Jolt в ECS (для
динамических).

```cpp
// Jolt -> ECS
world.system<Position, Rotation, const JoltBody>("SyncPhysicsToECS")
    .kind(flecs::PostUpdate)
    .each([interface](Position& p, Rotation& r, const JoltBody& b) {
        if (interface->IsActive(b.id)) {
            JPH::Vec3 pos = interface->GetPosition(b.id);
            JPH::Quat rot = interface->GetRotation(b.id);
            p = {pos.GetX(), pos.GetY(), pos.GetZ()};
            r = {rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW()};
        }
    });

// ECS -> Jolt (для кинематических тел)
world.system<const Position, const Rotation, const JoltBody>("SyncECSToPhysics")
    .kind(flecs::PreUpdate)
    .each([interface](const Position& p, const Rotation& r, const JoltBody& b) {
        if (interface->GetMotionType(b.id) == JPH::EMotionType::Kinematic) {
            interface->SetPositionAndRotation(
                b.id,
                JPH::Vec3(p.x, p.y, p.z),
                JPH::Quat(r.x, r.y, r.z, r.w),
                JPH::EActivation::Activate
            );
        }
    });
```

### Полная система интеграции

```cpp
class PhysicsSystem {
public:
    void Init(flecs::world& world) {
        // Инициализация JoltPhysics
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();

        // Создание PhysicsSystem
        physics_system.Init(max_bodies, num_body_mutexes, max_body_pairs, max_contact_constraints);

        // Регистрация компонентов
        world.component<JoltBody>();
        world.component<PhysicsProperties>();

        // Создание систем
        CreateSyncSystems(world);

        // Настройка слушателей
        physics_system.SetContactListener(&contact_listener);
        physics_system.SetBodyActivationListener(&activation_listener);
    }

    void Update(float delta_time) {
        physics_system.Update(delta_time, JPH::cCollisionSteps, temp_allocator, job_system);
    }

private:
    JPH::PhysicsSystem physics_system;
    JPH::TempAllocatorImpl temp_allocator;
    JPH::JobSystemThreadPool job_system;
};
```

---

## Оптимизации для воксельного движка

### HeightFieldShape для воксельного ландшафта

Идеально подходит для представления воксельного ландшафта:

```cpp
// Создание высотного поля из массива вокселей
HeightFieldShapeSettings heightfield_settings;
heightfield_settings.mHeightSamples = voxel_heights;      // Массив высот
heightfield_settings.mSize = IVec3(width, 1, depth);      // Размеры
heightfield_settings.mScale = Vec3(voxel_size, height_scale, voxel_size);
heightfield_settings.mOffset = Vec3(0, min_height, 0);    // Смещение

ShapeRefC heightfield_shape = heightfield_settings.Create().Get();
```

### Voxel-based коллайдеры

Для динамических воксельных объектов (разрушаемые стены, кучи материалов):

- **ConvexHullShape**: Для групп вокселей
- **CompoundShape**: Для разреженных воксельных структур
- **MeshShape**: Для статических воксельных мешей

### Cellular Automata и жидкости

JoltPhysics поддерживает мягкие тела, которые можно адаптировать для симуляции жидкостей и сыпучих материалов через:

- **SoftBodyShape**: Для деформируемых объектов
- **Edge constraints**, **Volume constraints**: Для сохранения формы
- **Pressure constraints**: Для симуляции газов/жидкостей

### Чанкирование физики

Для больших воксельных миров используйте чанкирование:

```cpp
struct VoxelChunkPhysics {
    JPH::BodyID terrain_body;
    std::vector<JPH::BodyID> dynamic_bodies;
    bool is_active = true;

    void Activate() {
        if (!is_active) {
            physics_system.GetBodyInterface().AddBody(terrain_body, JPH::EActivation::Activate);
            is_active = true;
        }
    }

    void Deactivate() {
        if (is_active) {
            physics_system.GetBodyInterface().RemoveBody(terrain_body);
            is_active = false;
        }
    }
};
```

---

## Производительность и настройка

### Критические параметры для воксельного мира

```cpp
physics_system.Init(
    max_bodies = 16384,          // Увеличить для больших воксельных миров
    num_body_mutexes = 0,        // Автоматический выбор
    max_body_pairs = 8192,       // Увеличить для сложных сцен
    max_contact_constraints = 4096, // Увеличить для множества контактов
    ... // фильтры
);

// Настройка гравитации для вокселей
physics_system.SetGravity(JPH::Vec3(0, -15.0f, 0));  // Усиленная гравитация для лучшей стабильности
```

### Оптимизации для ProjectV

1. **Используйте слои**: Разделяйте статические и динамические воксели для оптимизации широкой фазы
2. **Batch добавление тел**: Добавляйте воксельные чанки группами, а не по одному
3. **Оптимизируйте HeightField**: Используйте `mBlockSize = 4-8` для далёких чанков, `mBlockSize = 2` для близких
4. **Настройте Sleep**: Используйте `SetSleepSettings()` для контроля активации воксельных объектов
5. **Кэшируйте формы**: Повторно используйте формы для одинаковых воксельных структур

### Профилирование воксельной физики

Включите `JPH_PROFILE_ENABLED` для сбора статистики специфичной для вокселей:

```cpp
void LogVoxelPhysicsStats() {
    uint32_t stats[static_cast<int>(JPH::EPhysicsStatistics::NumStatistics)];
    physics_system.GetPhysicsStatistics(stats);

    SDL_Log("Voxel Physics Statistics:");
    SDL_Log("  Total bodies: %u", stats[static_cast<int>(JPH::EPhysicsStatistics::NumBodies)]);
    SDL_Log("  Active voxel bodies: %u", GetActiveVoxelBodyCount());
    SDL_Log("  HeightField collisions: %u", GetHeightFieldCollisionCount());
    SDL_Log("  Voxel-voxel contacts: %u", GetVoxelVoxelContactCount());
}
```

---

## Решение проблем (ProjectV-specific)

### Проблема: HeightFieldShape для вокселей не работает

**Симптомы:**

- Воксельный ландшафт не коллизится
- Объекты падают сквозь землю
- Неправильные коллизии с террейном
- Ошибки при создании HeightFieldShape
- Отсутствие коллизий в определённых областях (дыры)

**Решение:**

1. **Проверить данные высот (основные требования):**
   ```cpp
   void ValidateHeightFieldData(const float* samples, uint32 sample_count) {
       if (!samples) {
           SDL_LogError("HeightField samples pointer is null");
           return;
       }

       if (sample_count == 0 || sample_count > 16384) {  // Практический лимит
           SDL_LogError("Invalid sample count: %u (must be 1-16384)", sample_count);
           return;
       }

       // Проверить наличие значений "no collision"
       bool has_collision = false;
       for (uint32 i = 0; i < sample_count * sample_count; ++i) {
           if (samples[i] != HeightFieldShapeConstants::cNoCollisionValue) {
               has_collision = true;
               break;
           }
       }

       if (!has_collision) {
           SDL_LogError("HeightField has no collision samples (all values are cNoCollisionValue)");
       }

       // Проверить требования к размеру блока
       if (sample_count % settings.mBlockSize != 0) {
           SDL_LogError("Sample count (%u) must be divisible by block size (%u)",
                        sample_count, settings.mBlockSize);
       }
   }
   ```

2. **Распространённые ошибки и их решения:**

   **Ошибка 1: Неправильный размер массива samples**
   ```cpp
   // ❌ Неправильно: думать, что samples - это одномерный массив размером sample_count
   // ✅ Правильно: samples - это квадратный массив размером sample_count * sample_count
   uint32 total_samples = sample_count * sample_count;
   std::vector<float> samples(total_samples);

   // Заполнение: samples[y * sample_count + x] = height_at(x, y)
   ```

   **Ошибка 2: Использование неквадратной сетки**
   ```cpp
   // ❌ JoltPhysics поддерживает только квадратные высотные поля
   // ✅ Используйте sample_count для обеих осей, интерполируйте данные если нужно
   ```

   **Ошибка 3: Неправильные значения "no collision"**
   ```cpp
   // Использовать константу из HeightFieldShapeConstants
   const float no_collision = HeightFieldShapeConstants::cNoCollisionValue;  // FLT_MAX

   // Установить дыру в высотном поле
   samples[y * sample_count + x] = no_collision;
   ```

   **Ошибка 4: Слишком большой block size для мелких деталей**
   ```cpp
   // Для детализированного террейна используйте меньший block size
   settings.mBlockSize = 2;  // Лучшая детализация, больше памяти
   // vs
   settings.mBlockSize = 8;  // Меньше памяти, меньше детализации
   ```

3. **Отладка HeightFieldShape:**

   ```cpp
   void DebugHeightFieldShape(JPH::HeightFieldShape* shape) {
       if (!shape) return;

       // Основные параметры
       uint32 sample_count = shape->GetSampleCount();
       uint32 block_size = shape->GetBlockSize();
       float min_height = shape->GetMinHeightValue();
       float max_height = shape->GetMaxHeightValue();

       SDL_Log("HeightFieldShape: %ux%u samples, block size: %u, height range: %.2f - %.2f",
               sample_count, sample_count, block_size, min_height, max_height);

       // Проверить конкретные точки
       for (uint32 y = 0; y < sample_count; y += sample_count / 4) {
           for (uint32 x = 0; x < sample_count; x += sample_count / 4) {
               bool no_collision = shape->IsNoCollision(x, y);
               JPH::Vec3 pos = shape->GetPosition(x, y);

               SDL_Log("  Sample (%u, %u): %s at (%.2f, %.2f, %.2f)",
                       x, y,
                       no_collision ? "NO COLLISION" : "collision",
                       pos.GetX(), pos.GetY(), pos.GetZ());
           }
       }
   }
   ```

### Проблема: Интеграция с flecs ECS

**Симптомы:**

- Сущности не имеют физики
- Физические тела не синхронизируются с сущностями
- Утечки при удалении сущностей

**Решение:**

1. **Правильная система обновления:**
   ```cpp
   void PhysicsUpdateSystem(flecs::iter& it, float dt) {
       auto bodies = it.field<PhysicsBody>(1);
       auto transforms = it.field<Transform>(2);

       for (auto i : it) {
           if (bodies[i].id != JPH::BodyID::cInvalidBodyID) {
               // Обновить трансформацию сущности из физики
               JPH::BodyInterface& interface = physics_system.GetBodyInterface();
               JPH::Vec3 pos = interface.GetCenterOfMassPosition(bodies[i].id);
               JPH::Quat rot = interface.GetRotation(bodies[i].id);

               transforms[i].position = ToGlmVec3(pos);
               transforms[i].rotation = ToGlmQuat(rot);
           }
       }
   }
   ```

2. **Очистка при удалении сущности:**
   ```cpp
   void OnEntityDestroyed(flecs::entity e) {
       PhysicsBody* body = e.get<PhysicsBody>();
       if (body && body->id != JPH::BodyID::cInvalidBodyID) {
           physics_system.GetBodyInterface().DestroyBody(body->id);
           body->id = JPH::BodyID::cInvalidBodyID;
       }
   }
   ```

3. **Синхронизация компонентов:**
   ```cpp
   // Использовать флаги обновления
   struct PhysicsDirty {
       bool position = false;
       bool rotation = false;
       bool properties = false;
   };

   // Проверять флаги в системе
   ```

### Проблема: Рассинхронизация с рендерером

**Симптомы:**

- Объекты рендерятся не там, где находятся физически
- Дрожание/мерцание
- Задержка в отображении

**Решение:**

1. **Двойная буферизация трансформаций:**
   ```cpp
   struct TransformBuffer {
       glm::mat4 current;
       glm::mat4 previous;
       bool updated = false;
   };

   std::vector<TransformBuffer> transform_buffers;

   void UpdateRenderTransforms() {
       for (auto& buf : transform_buffers) {
           if (buf.updated) {
               // Интерполировать между previous и current
               glm::mat4 interpolated = glm::mix(buf.previous, buf.current, alpha);
               // Отправить в Vulkan буфер
               buf.updated = false;
           }
       }
   }
   ```

2. **Интерполяция кадров:**
   ```cpp
   void InterpolatePhysics(float alpha) {
       // alpha от 0 (предыдущий кадр) до 1 (текущий кадр)
       for (each body) {
           glm::vec3 pos = glm::mix(prev_position, curr_position, alpha);
           glm::quat rot = glm::slerp(prev_rotation, curr_rotation, alpha);
           // Обновить трансформацию рендеринга
       }
   }
   ```

3. **Синхронизация с частотой обновления:**
   ```cpp
   // Физика обновляется с фиксированной частотой (например, 60 Гц)
   // Рендеринг может быть с переменной частотой
   // Интерполировать между физическими кадрами
   ```

---

## Терминология для воксельной физики

| Термин                                     | Объяснение                                                                                                             |
|--------------------------------------------|------------------------------------------------------------------------------------------------------------------------|
| **Воксельный коллайдер**                   | Коллайдер, представляющий воксельную геометрию. Может быть динамически обновляемым.                                    |
| **Сыпучие материалы (Granular Materials)** | Физика песка, гравия, сыпучих веществ. Моделируется через множество мелких частиц.                                     |
| **Жидкости (Fluids)**                      | Симуляция жидкостей в воксельном мире. Может использовать smoothed-particle hydrodynamics (SPH) или cellular automata. |
| **Деформация (Deformation)**               | Изменение формы воксельного ландшафта под воздействием сил (копание, взрывы).                                          |
| **Устойчивость (Stability)**               | Способность воксельных структур выдерживать нагрузки без разрушения.                                                   |
| **Разрушение (Fracture)**                  | Процесс разделения воксельной структуры на части при превышении предела прочности.                                     |
| **Пористость (Porosity)**                  | Свойство материала, определяющее наличие пустот. Влияет на физику жидкостей и прочность.                               |
| **Адгезия (Adhesion)**                     | Сила сцепления между вокселями разных материалов.                                                                      |

---

## Специальные термины для воксельной физики

| Термин                             | Объяснение                                                                                                         |
|------------------------------------|--------------------------------------------------------------------------------------------------------------------|
| **HeightFieldShape**               | Специальная форма JoltPhysics для представления воксельного ландшафта. Эффективно использует разреженность данных. |
| **Voxel-based коллайдер**          | Коллайдер, построенный на основе воксельных данных. Может быть аппроксимирован HeightField или voxel grid.         |
| **ECS компоненты физики**          | Компоненты flecs для интеграции физики: `PhysicsBody`, `PhysicsProperties`, `PhysicsForces`.                       |
| **Синхронизация трансформаций**    | Процесс обновления трансформаций рендеринга (Vulkan) на основе результатов физической симуляции.                   |
| **Transform буфер**                | GPU буфер (Vulkan storage buffer) для передачи матриц трансформации из физики в рендерер.                          |
| **Object Layers для вокселей**     | Специализированные слои для воксельного мира: `VOXEL_TERRAIN`, `VOXEL_DYNAMIC`, `VOXEL_FLUID`.                     |
| **Детерминированная симуляция**    | Критично для воксельных миров, где изменения должны быть воспроизводимы.                                           |
| **Многопоточная обработка чанков** | Параллельная симуляция разных чанков воксельного мира через JobSystem.                                             |

---

## Примеры использования

### Минимальный пример интеграции

```cpp
#include "Jolt/Jolt.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include <flecs.h>

class VoxelPhysicsSystem {
public:
    void Init() {
        // Инициализация JoltPhysics
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();

        // Создание PhysicsSystem с параметрами для вокселей
        physics_system.Init(16384, 0, 8192, 4096,
                           &broad_phase_layer_interface,
                           &object_vs_broadphase_layer_filter,
                           &object_vs_object_layer_filter);

        // Настройка гравитации
        physics_system.SetGravity(JPH::Vec3(0, -15.0f, 0));
    }

    void CreateVoxelTerrain(flecs::world& world, const VoxelChunk& chunk) {
        // Создание HeightFieldShape из данных чанка
        JPH::HeightFieldShapeSettings settings;
        settings.mSampleCount = chunk.GetSize();
        settings.mHeightSamples = chunk.GetHeightData();
        settings.mScale = JPH::Vec3(chunk.GetVoxelSize(), 1.0f, chunk.GetVoxelSize());
        settings.mOffset = JPH::Vec3(chunk.GetPosition().x, 0, chunk.GetPosition().z);

        JPH::ShapeRefC shape = settings.Create().Get();

        // Создание тела
        JPH::BodyCreationSettings body_settings(
            shape,
            JPH::RVec3::sZero(),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Static,
            (JPH::uint8)Layers::VOXEL_TERRAIN
        );

        JPH::BodyID body_id = physics_system.GetBodyInterface().CreateBody(body_settings);
        physics_system.GetBodyInterface().AddBody(body_id, JPH::EActivation::DontActivate);

        // Создание сущности flecs
        flecs::entity entity = world.entity()
            .set<JoltBody>({body_id})
            .set<Transform>({chunk.GetPosition(), glm::quat(1, 0, 0, 0)});
    }

private:
    JPH::PhysicsSystem physics_system;
};
```

### Динамическое обновление вокселей

```cpp
void UpdateVoxelPhysics(VoxelChunk& chunk, uint32 x, uint32 y, uint32 z, bool add_voxel) {
    // Получить HeightFieldShape из тела
    JPH::BodyID body_id = chunk.GetPhysicsBody();
    JPH::BodyLockRead lock(physics_system.GetBodyLockInterface(), body_id);

    if (lock.Succeeded()) {
        const JPH::Body& body = lock.GetBody();
        JPH::HeightFieldShape* heightfield = static_cast<JPH::HeightFieldShape*>(body.GetShape().GetPtr());

        // Обновить высоту в конкретной точке
        float new_height = add_voxel ?
            std::max(heightfield->GetHeight(x, z), (float)y * chunk.GetVoxelSize()) :
            HeightFieldShapeConstants::cNoCollisionValue;

        // Обновить данные высотного поля
        UpdateHeightField(heightfield, x, z, new_height);
    }
}
```

---

## Детерминированная симуляция для сетевых воксельных миров

Воксельные миры ProjectV часто требуют сетевой синхронизации, где детерминированность физической симуляции критически
важна.

### Принципы детерминированной физики

```cpp
class DeterministicPhysicsSystem {
public:
    void Init() {
        // Включение детерминированного режима
        physics_system.Init(
            16384, 0, 8192, 4096,
            &broad_phase_layer_interface,
            &object_vs_broadphase_layer_filter,
            &object_vs_object_layer_filter,
            JPH::EPhysicsUpdateError::ReportError  // Всегда сообщать об ошибках
        );

        // Фиксированный сид для воспроизводимости
        physics_system.SetDeterministicSimulationSeed(0x12345678);

        // Фиксированная частота обновления (60 Гц)
        fixed_delta_time = 1.0f / 60.0f;
        accumulator = 0.0f;
    }

    void Update(float delta_time) {
        // Fixed timestep для детерминированности
        accumulator += delta_time;

        while (accumulator >= fixed_delta_time) {
            physics_system.Update(fixed_delta_time,
                                 JPH::cCollisionSteps,
                                 temp_allocator,
                                 job_system);
            accumulator -= fixed_delta_time;

            // Сохраняем состояние для rollback
            SavePhysicsState();
        }
    }

private:
    float fixed_delta_time;
    float accumulator;

    void SavePhysicsState() {
        // Сохранение состояния всех тел для сетевой синхронизации
        // ...
    }
};
```

### Сетевая синхронизация воксельной физики

```cpp
struct NetworkedVoxelPhysics {
    struct PhysicsSnapshot {
        uint32_t frame_number;
        std::vector<BodyState> body_states;
        std::vector<VoxelModification> voxel_changes;

        // Хеш для проверки согласованности
        uint64_t CalculateHash() const {
            // Детерминированный хеш состояния
            // ...
        }
    };

    // Отправка snapshots по сети
    void SendSnapshot(const PhysicsSnapshot& snapshot) {
        // Сжатие и отправка
        // ...
    }

    // Применение полученных изменений
    void ApplyRemoteSnapshot(const PhysicsSnapshot& snapshot) {
        if (snapshot.frame_number > current_frame) {
            // Rollback и пересимуляция
            RollbackAndResimulate(snapshot.frame_number);
        }

        // Синхронизация тел
        for (const auto& body_state : snapshot.body_states) {
            physics_system.GetBodyInterface().SetPositionAndRotation(
                body_state.body_id,
                body_state.position,
                body_state.rotation,
                JPH::EActivation::Activate
            );
        }

        // Применение изменений вокселей
        for (const auto& voxel_change : snapshot.voxel_changes) {
            ApplyVoxelChange(voxel_change);
        }
    }
};
```

### Оптимизации для детерминированной симуляции

1. **Фиксированный таймстеп**: Обязательно для воспроизводимости
2. **Детерминированные сиды**: Одинаковые начальные условия
3. **Порядок обработки**: Фиксированный порядок обработки чанков и тел
4. **Сжатие состояний**: Эффективное сетевое взаимодействие
5. **Rollback система**: Для компенсации сетевой задержки

---

## Интеграция с системой чанков ProjectV

### Глобальная система координат для физики

```cpp
class VoxelWorldPhysics {
public:
    struct ChunkPhysicsData {
        JPH::BodyID terrain_body;
        std::vector<JPH::BodyID> dynamic_bodies;
        glm::ivec3 chunk_coords;
        bool is_loaded = false;

        // Связь с рендерингом (интеграция с docs/vulkan/projectv-integration.md)
        VkBuffer transform_buffer;  // GPU буфер для трансформаций
        VmaAllocation transform_allocation;

        // Связь с ECS (интеграция с docs/flecs/projectv-integration.md)
        flecs::entity chunk_entity;
    };

    // Загрузка чанка с физикой
    void LoadChunk(const glm::ivec3& coords, const VoxelChunkData& voxel_data) {
        ChunkPhysicsData chunk_data;
        chunk_data.chunk_coords = coords;

        // Создание HeightFieldShape из данных вокселей
        auto heightfield = CreateHeightFieldFromVoxels(voxel_data);

        // Создание тела
        JPH::BodyCreationSettings body_settings(
            heightfield,
            ChunkToWorldPosition(coords),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Static,
            (JPH::uint8)Layers::VOXEL_TERRAIN
        );

        chunk_data.terrain_body = physics_system.GetBodyInterface().CreateBody(body_settings);
        physics_system.GetBodyInterface().AddBody(chunk_data.terrain_body,
                                                 JPH::EActivation::DontActivate);

        // Создание GPU буфера для трансформаций (интеграция с Vulkan)
        chunk_data.transform_buffer = CreateTransformBuffer(coords);

        // Создание сущности ECS
        chunk_data.chunk_entity = world.entity()
            .set<ChunkComponent>({coords})
            .set<PhysicsChunkComponent>({chunk_data.terrain_body});

        loaded_chunks[coords] = std::move(chunk_data);
    }

private:
    std::unordered_map<glm::ivec3, ChunkPhysicsData> loaded_chunks;
};
```

### LOD система для физики

```cpp
// Разные уровни детализации для физики
class PhysicsLODSystem {
public:
    enum class PhysicsLOD {
        Full,      // Полная точность (близкие чанки)
        Simplified, // Упрощённые коллайдеры (дальние чанки)
        BoundsOnly  // Только bounding box (очень дальние чанки)
    };

    PhysicsLOD CalculatePhysicsLOD(glm::vec3 camera_pos, glm::ivec3 chunk_coords) {
        float distance = glm::distance(camera_pos, ChunkToWorldPosition(chunk_coords));

        if (distance < close_distance) return PhysicsLOD::Full;
        if (distance < medium_distance) return PhysicsLOD::Simplified;
        return PhysicsLOD::BoundsOnly;
    }

    // Упрощение HeightField для дальних чанков
    JPH::ShapeRefC CreateSimplifiedHeightField(const VoxelChunkData& voxel_data,
                                               PhysicsLOD lod) {
        switch (lod) {
            case PhysicsLOD::Full:
                return CreateFullHeightField(voxel_data);
            case PhysicsLOD::Simplified:
                return CreateSimplifiedHeightField(voxel_data, 0.5f); // 50% детализации
            case PhysicsLOD::BoundsOnly:
                return CreateBoundingBoxShape(voxel_data);
        }
        return nullptr;
    }
};
```

### Динамическое обновление чанков

```cpp
// Обработка изменений вокселей в реальном времени
void ProcessVoxelModification(glm::ivec3 chunk_coords,
                             glm::uvec3 voxel_local_pos,
                             bool add_voxel) {

    auto& chunk_data = GetChunkPhysicsData(chunk_coords);

    if (!chunk_data.is_loaded) return;

    // Получение текущего HeightFieldShape
    JPH::BodyLockRead lock(physics_system.GetBodyLockInterface(),
                          chunk_data.terrain_body);

    if (lock.Succeeded()) {
        const JPH::Body& body = lock.GetBody();
        JPH::HeightFieldShape* heightfield =
            static_cast<JPH::HeightFieldShape*>(body.GetShape().GetPtr());

        // Обновление высотного поля
        UpdateHeightField(heightfield, voxel_local_pos, add_voxel);

        // Пометить чанк как изменённый для обновления рендеринга
        MarkChunkDirty(chunk_coords);

        // Обновление GPU буфера трансформаций если нужно
        UpdatePhysicsTransformBuffer(chunk_data);
    }
}
```

---

## Типичные сценарии воксельной физики

### Сценарий 1: Разрушаемые стены

```cpp
class DestructibleVoxelWall {
public:
    void Initialize(const VoxelChunkData& wall_data) {
        // Создание CompoundShape из отдельных вокселей
        JPH::StaticCompoundShapeSettings compound_settings;

        for (const auto& voxel : wall_data.GetSolidVoxels()) {
            JPH::BoxShapeSettings box_settings(
                JPH::Vec3(0.5f, 0.5f, 0.5f) * voxel_size
            );
            box_settings.SetEmbedded();

            JPH::ShapeSettings::ShapeResult result = box_settings.Create();
            if (result.IsValid()) {
                compound_settings.AddShape(
                    VoxelToWorldPosition(voxel.position),
                    JPH::Quat::sIdentity(),
                    result.Get()
                );
            }
        }

        // Создание тела
        JPH::BodyCreationSettings body_settings(
            compound_settings.Create().Get(),
            JPH::RVec3::sZero(),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic,
            (JPH::uint8)Layers::VOXEL_DYNAMIC
        );

        body_settings.mRestitution = 0.2f;
        body_settings.mFriction = 0.8f;

        wall_body = physics_system.GetBodyInterface().CreateBody(body_settings);
        physics_system.GetBodyInterface().AddBody(wall_body, JPH::EActivation::Activate);
    }

    void ApplyDamage(glm::vec3 impact_point, float force) {
        // Определение затронутых вокселей
        auto affected_voxels = FindAffectedVoxels(impact_point, force);

        // Удаление вокселей из CompoundShape
        RemoveVoxelsFromShape(affected_voxels);

        // Применение импульса
        JPH::Vec3 impulse(force * 10.0f, 0, 0);
        physics_system.GetBodyInterface().AddImpulse(wall_body, impulse);
    }

private:
    JPH::BodyID wall_body;
};
```

### Сценарий 2: Физика жидкостей и сыпучих материалов

```cpp
class VoxelFluidPhysics {
public:
    void SimulateFluid(float delta_time) {
        // SPH (Smoothed Particle Hydrodynamics) симуляция
        for (auto& fluid_chunk : fluid_chunks) {
            // Вычисление плотности
            CalculateDensity(fluid_chunk);

            // Вычисление давления
            CalculatePressure(fluid_chunk);

            // Вычисление сил
            CalculateForces(fluid_chunk);

            // Интеграция
            Integrate(fluid_chunk, delta_time);

            // Обработка коллизий с террейном
            HandleTerrainCollisions(fluid_chunk);
        }

        // Обновление воксельного представления
        UpdateVoxelRepresentation();
    }

private:
    struct FluidParticle {
        glm::vec3 position;
        glm::vec3 velocity;
        glm::vec3 acceleration;
        float density;
        float pressure;
        float mass;
    };

    std::vector<std::vector<FluidParticle>> fluid_chunks;

    void HandleTerrainCollisions(std::vector<FluidParticle>& particles) {
        for (auto& particle : particles) {
            // Проверка коллизии с HeightField террейном
            JPH::RayCastResult result;
            if (physics_system.GetNarrowPhaseQuery().CastRay(
                ToJoltVec3(particle.position),
                ToJoltVec3(particle.position + particle.velocity * 0.1f),
                result)) {

                // Отражение от поверхности
                particle.velocity = glm::reflect(particle.velocity,
                                                ToGlmVec3(result.mPenetrationAxis));
                particle.velocity *= 0.7f; // Коэффициент отскока
            }
        }
    }
};
```

### Сценарий 3: Взаимодействие игрока с воксельным миром

```cpp
class VoxelPlayerController {
public:
    void Initialize() {
        // Создание Character Controller
        JPH::CharacterVirtualSettings settings;
        settings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
        settings.mMaxStrength = 100.0f;
        settings.mShape = JPH::RotatedTranslatedShapeSettings(
            JPH::Vec3(0, 1.0f, 0),  // Смещение для центра масс
            JPH::Quat::sIdentity(),
            new JPH::CapsuleShape(0.5f, 1.0f)  // Капсула для игрока
        ).Create().Get();

        character = new JPH::CharacterVirtual(
            &settings,
            JPH::RVec3::sZero(),
            JPH::Quat::sIdentity(),
            &physics_system
        );

        character->SetLayer(Layers::PLAYER);
    }

    void Update(float delta_time, const PlayerInput& input) {
        // Вычисление движения
        JPH::Vec3 movement = CalculateMovement(input);

        // Обновление Character Controller
        character->Update(
            delta_time,
            physics_system.GetGravity(),
            character->GetLayerFilter(),
            character->GetBodyFilter(),
            character->GetShapeFilter(),
            temp_allocator
        );

        // Обработка взаимодействия с вокселями
        if (input.mining) {
            RaycastForMining(character->GetPosition(), character->GetRotation());
        }

        if (input.building) {
            RaycastForBuilding(character->GetPosition(), character->GetRotation());
        }
    }

private:
    JPH::CharacterVirtual* character;

    void RaycastForMining(JPH::RVec3 position, JPH::Quat rotation) {
        // Луч для определения цели копания
        JPH::Vec3 forward = rotation.RotateAxisX();
        JPH::RayCastResult result;

        if (physics_system.GetNarrowPhaseQuery().CastRay(
            position,
            position + forward * 5.0f,
            result)) {

            // Определение чанка и позиции вокселя
            auto [chunk_coords, voxel_pos] =
                WorldToVoxelCoordinates(ToGlmVec3(result.mContactPoint));

            // Удаление вокселя
            RemoveVoxel(chunk_coords, voxel_pos);

            // Обновление физики
            UpdateChunkPhysics(chunk_coords);
        }
    }
};
```

---

## Интеграция с другими компонентами ProjectV

### Связь с Vulkan GPU-Driven рендерингом

```cpp
// Подготовка данных физики для GPU-Driven пайплайна
struct PhysicsDrawData {
    glm::mat4 transform;
    glm::vec4 bounds;
    uint32_t body_id;
    uint32_t flags;
};

std::vector<PhysicsDrawData> PreparePhysicsDrawData() {
    std::vector<PhysicsDrawData> draw_data;

    // Сбор данных всех физических тел
    JPH::BodyInterface& interface = physics_system.GetBodyInterface();
    JPH::BodyIDVector body_ids;
    physics_system.GetBodies(body_ids);

    for (JPH::BodyID body_id : body_ids) {
        if (interface.IsAdded(body_id)) {
            PhysicsDrawData data;

            // Трансформация
            data.transform = ToGlmMat4(interface.GetWorldTransform(body_id));

            // Bounding box
            JPH::AABox aabb = interface.GetTransformedShape(body_id).GetWorldSpaceBounds();
            data.bounds = glm::vec4(
                aabb.mMin.GetX(), aabb.mMin.GetY(), aabb.mMin.GetZ(),
                aabb.mMax.GetX() - aabb.mMin.GetX()  // width как w компонент
            );

            // Идентификатор и флаги
            data.body_id = body_id.GetIndexAndSequenceNumber();
            data.flags = CalculatePhysicsFlags(body_id);

            draw_data.push_back(data);
        }
    }

    return draw_data;
}

// Обновление Vulkan storage buffer
void UpdatePhysicsVulkanBuffer(VkDevice device,
                              VmaAllocator allocator,
                              VkBuffer physics_buffer,
                              const std::vector<PhysicsDrawData>& draw_data) {
    // Копирование данных в GPU буфер (интеграция с docs/vulkan/projectv-integration.md)
    // ...
}
```

### Связь с glm математикой

```cpp
// Конвертация между JoltPhysics и glm
namespace PhysicsMath {
    glm::vec3 ToGlmVec3(const JPH::Vec3& v) {
        return {v.GetX(), v.GetY(), v.GetZ()};
    }

    JPH::Vec3 ToJoltVec3(const glm::vec3& v) {
        return {v.x, v.y, v.z};
    }

    glm::quat ToGlmQuat(const JPH::Quat& q) {
        return {q.GetW(), q.GetX(), q.GetY(), q.GetZ()};
    }

    JPH::Quat ToJoltQuat(const glm::quat& q) {
        return {q.x, q.y, q.z, q.w};
    }

    glm::mat4 ToGlmMat4(const JPH::Mat44& m) {
        // Конвертация матрицы 4x4
        // ...
    }

    // Быстрое преобразование массовых данных
    void TransformVoxelPositionsToPhysics(const std::vector<glm::vec3>& positions,
                                         std::vector<JPH::Vec3>& physics_positions) {
        physics_positions.resize(positions.size());

        // SIMD оптимизированная конвертация (интеграция с docs/glm/projectv-integration.md)
        #ifdef GLM_ENABLE_SIMD_SSE2
        // SIMD версия
        #else
        // Скалярная версия
        for (size_t i = 0; i < positions.size(); ++i) {
            physics_positions[i] = ToJoltVec3(positions[i]);
        }
        #endif
    }
};
```

### Связь с Tracy профилированием

```cpp
// Профилирование воксельной физики
void ProfileVoxelPhysics() {
    ZoneScopedN("VoxelPhysics");

    {
        ZoneScopedN("HeightFieldUpdates");
        // Обновление HeightField коллайдеров
        UpdateAllHeightFields();
    }

    {
        ZoneScopedN("PhysicsSimulation");
        // Основная симуляция
        physics_system.Update(delta_time, JPH::cCollisionSteps,
                             temp_allocator, job_system);
    }

    {
        ZoneScopedN("ECSSync");
        // Синхронизация с ECS
        SyncPhysicsToECS();
    }

    TracyPlot("ActivePhysicsBodies", GetActiveBodyCount());
    TracyPlot("VoxelCollisions", GetVoxelCollisionCount());
    TracyPlot("PhysicsMemoryMB", GetPhysicsMemoryUsage() / (1024 * 1024));
}
```

---

## Заключение

Интеграция JoltPhysics с ProjectV предоставляет мощную основу для физической симуляции воксельного мира. Ключевые
аспекты:

1. **HeightFieldShape** — оптимальное решение для статического воксельного ландшафта
2. **ECS интеграция** — чистый и производительный способ связи физики с игровой логикой
3. **Слои и фильтры** — эффективное управление коллизиями в сложных воксельных сценах
4. **Оптимизации** — чанкирование, кэширование форм, адаптивная детализация
5. **Детерминированность** — обязательное требование для сетевых воксельных миров

Для дальнейшего изучения смотрите:

- [Быстрый старт](quickstart.md) — базовые примеры использования JoltPhysics
- [Справочник API](api-reference.md) — полная документация классов и функций
- [Основные понятия](concepts.md) — архитектурные концепции физической симуляции

← [На главную документации](../README.md)

🟡 **Уровень 2: Средний**

# Устранение неполадок JoltPhysics

Руководство по диагностике и решению распространённых проблем при работе с JoltPhysics в ProjectV.

## На этой странице

- [Общие принципы отладки](#общие-принципы-отладки)
- [Инициализация и настройка](#инициализация-и-настройка)
- [Проблемы с коллизиями](#проблемы-с-коллизиями)
- [Производительность](#производительность)
- [Память и утечки](#память-и-утечки)
- [Интеграция с ProjectV](#интеграция-с-projectv)
- [Отладка и инструменты](#отладка-и-инструменты)
- [Часто задаваемые вопросы](#часто-задаваемые-вопросы)

---

## Общие принципы отладки

### Шаги диагностики

1. **Включить отладочную информацию**:
   ```cpp
   #define JPH_DEBUG_RENDERER
   #define JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
   #define JPH_ASSERTION_ENABLED
   ```

2. **Проверить порядок инициализации**:
   ```cpp
   // Правильный порядок:
   JPH::RegisterDefaultAllocator();
   JPH::Factory::sInstance = new JPH::Factory();
   JPH::RegisterTypes();
   // Создать TempAllocator, JobSystem, PhysicsSystem
   ```

3. **Валидировать состояние системы**:
   ```cpp
   void ValidatePhysicsSystem() {
       if (!JPH::Factory::sInstance) {
           SDL_LogError("Factory not initialized");
       }
       
       uint32_t num_bodies = physics_system.GetNumBodies();
       uint32_t num_active = physics_system.GetNumActiveBodies();
       SDL_Log("Bodies: %u total, %u active", num_bodies, num_active);
   }
   ```

### Логирование ошибок

```cpp
class PhysicsErrorCallback : public JPH::ContactListener {
public:
    void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2,
                       const JPH::ContactManifold& inManifold) override {
        SDL_Log("Contact added between body %u and %u", 
                inBody1.GetID().GetIndex(), inBody2.GetID().GetIndex());
    }
    
    void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2,
                           const JPH::ContactManifold& inManifold) override {
        // Логирование persistent контактов
    }
    
    void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override {
        SDL_Log("Contact removed");
    }
};
```

---

## Инициализация и настройка

### Проблема: Ошибка при инициализации JoltPhysics

**Симптомы:**

- Креши при вызове `JPH::RegisterTypes()`
- Физические тела не создаются
- Ошибки ассертов типа `JPH_ASSERT(IsAligned(this, JPH_CACHE_LINE_SIZE))`
- Сегфолты или access violation

**Причины и решения:**

1. **Неправильный порядок инициализации (самая частая ошибка):**
   ```cpp
   // ❌ Неправильно - приведёт к крешу:
   JPH::Factory::sInstance = new JPH::Factory();
   JPH::RegisterDefaultAllocator();  // Должно быть ПЕРВЫМ!
   
   // ✅ Правильный порядок (обязательно в этой последовательности):
   JPH::RegisterDefaultAllocator();      // 1. Аллокатор - всегда первый!
   JPH::Factory::sInstance = new JPH::Factory();  // 2. Фабрика
   JPH::RegisterTypes();                 // 3. Регистрация типов
   ```

2. **Отсутствие фабрики перед регистрацией типов:**
   ```cpp
   // Проверить наличие фабрики
   if (!JPH::Factory::sInstance) {
       SDL_LogError("JoltPhysics factory is null - creating new instance");
       JPH::Factory::sInstance = new JPH::Factory();
       // Не забудьте вызвать RegisterTypes() после создания фабрики!
       JPH::RegisterTypes();
   } else if (!JPH::Factory::sInstance->IsTypeRegistered(JPH::EShapeSubType::Box)) {
       // Проверить, зарегистрированы ли типы
       SDL_Log("Types not registered - calling RegisterTypes()");
       JPH::RegisterTypes();
   }
   ```

3. **Многократная инициализация (double initialization):**
   ```cpp
   // Использовать флаг для предотвращения повторной инициализации
   static std::once_flag init_flag;
   std::call_once(init_flag, []() {
       JPH::RegisterDefaultAllocator();
       JPH::Factory::sInstance = new JPH::Factory();
       JPH::RegisterTypes();
       SDL_Log("JoltPhysics initialized successfully");
   });
   ```

4. **Проблемы с выравниванием памяти:**
   ```cpp
   // JoltPhysics требует выравнивание по JPH_CACHE_LINE_SIZE (обычно 64 байта)
   // Убедитесь, что выделяете память с правильным выравниванием
   void* aligned_memory = JPH::Allocate(JPH_CACHE_LINE_SIZE, JPH_CACHE_LINE_SIZE);
   
   // Или используйте стандартные аллокаторы с выравниванием
   struct alignas(JPH_CACHE_LINE_SIZE) AlignedStruct {
       // данные
   };
   ```

5. **Исключения при инициализации:**
   ```cpp
   try {
       JPH::RegisterDefaultAllocator();
       JPH::Factory::sInstance = new JPH::Factory();
       JPH::RegisterTypes();
   } catch (const std::exception& e) {
       SDL_LogError("JoltPhysics initialization failed: %s", e.what());
       // Обработка ошибки
   }
   ```

**Отладочные макросы для диагностики:**

```cpp
#define JPH_DEBUG_RENDERER              // Включить отладочный рендерер
#define JPH_FLOATING_POINT_EXCEPTIONS_ENABLED  // Проверка деления на ноль и т.д.
#define JPH_ASSERTION_ENABLED           // Включить ассерты
#define JPH_ENABLE_ASSERTS              // Альтернативное название

// Включить все проверки для отладки
#ifdef _DEBUG
    #define JPH_DEBUG
    #define JPH_PROFILE_ENABLED
#endif
```

**Проверка состояния системы:**

```cpp
void ValidateJoltPhysicsState() {
    if (!JPH::Factory::sInstance) {
        SDL_LogError("ERROR: Factory is null - JoltPhysics not initialized");
        return;
    }
    
    // Проверить регистрацию ключевых типов форм
    bool box_registered = JPH::Factory::sInstance->IsTypeRegistered(JPH::EShapeSubType::Box);
    bool sphere_registered = JPH::Factory::sInstance->IsTypeRegistered(JPH::EShapeSubType::Sphere);
    
    SDL_Log("JoltPhysics state: Box registered=%s, Sphere registered=%s",
            box_registered ? "YES" : "NO",
            sphere_registered ? "YES" : "NO");
    
    if (!box_registered || !sphere_registered) {
        SDL_LogError("ERROR: Some shape types not registered. Call RegisterTypes()");
    }
}
```

### Проблема: PhysicsSystem не обновляется

**Симптомы:**

- Тела создаются, но не движутся
- Не применяются силы
- Коллизии не обрабатываются

**Решение:**

```cpp
// Проверить вызов Update
void UpdatePhysics(float delta_time) {
    // Ограничить delta time для стабильности
    delta_time = std::min(delta_time, 0.033f);
    
    // Убедиться, что все параметры корректны
    if (temp_allocator && job_system) {
        physics_system.Update(
            delta_time,
            JPH::cCollisionSteps,  // Обычно 1
            temp_allocator,
            job_system
        );
    } else {
        SDL_LogError("TempAllocator or JobSystem is null");
    }
}
```

---

## Проблемы с коллизиями

### Проблема: Тела не сталкиваются

**Симптомы:**

- Объекты проходят друг сквозь друга
- Нет реакции на столкновения
- Контакты не создаются

**Диагностика:**

1. **Проверить фильтры слоёв**:
   ```cpp
   class DebugLayerFilter : public JPH::ObjectVsObjectLayerFilter {
   public:
       bool ShouldCollide(JPH::ObjectLayer layer1, JPH::ObjectLayer layer2) override {
           bool result = original_filter.ShouldCollide(layer1, layer2);
           if (!result) {
               SDL_Log("Collision filtered: %u vs %u", layer1, layer2);
           }
           return result;
       }
   private:
       ObjectVsObjectLayerFilter& original_filter;
   };
   ```

2. **Проверить формы тел**:
   ```cpp
   void DebugBodyShapes(JPH::BodyID body_id) {
       JPH::BodyLockRead lock(physics_system.GetBodyLockInterface(), body_id);
       if (lock.Succeeded()) {
           const JPH::Body& body = lock.GetBody();
           JPH::ShapeRefC shape = body.GetShape();
           
           // Получить AABB
           JPH::AABox aabb = shape->GetWorldSpaceBounds(
               body.GetCenterOfMassTransform(),
               JPH::Vec3::sReplicate(1.0f)
           );
           
           SDL_Log("Body %u AABB: min=(%f,%f,%f), max=(%f,%f,%f)",
                   body_id.GetIndex(),
                   aabb.mMin.GetX(), aabb.mMin.GetY(), aabb.mMin.GetZ(),
                   aabb.mMax.GetX(), aabb.mMax.GetY(), aabb.mMax.GetZ());
       }
   }
   ```

3. **Проверить типы движения**:
   ```cpp
   void DebugBodyMotionType(JPH::BodyID body_id) {
       JPH::BodyInterface& body_interface = physics_system.GetBodyInterface();
       JPH::EMotionType motion_type = body_interface.GetMotionType(body_id);
       
       const char* type_str = "Unknown";
       switch (motion_type) {
           case JPH::EMotionType::Static: type_str = "Static"; break;
           case JPH::EMotionType::Kinematic: type_str = "Kinematic"; break;
           case JPH::EMotionType::Dynamic: type_str = "Dynamic"; break;
       }
       
       SDL_Log("Body %u motion type: %s", body_id.GetIndex(), type_str);
   }
   ```

**Распространённые причины:**

1. **Оба тела Static**:
   ```cpp
   // Static тела никогда не сталкиваются друг с другом
   // Решение: сделать одно тело Dynamic или Kinematic
   ```

2. **Неправильные слои**:
   ```cpp
   // Проверить фильтр
   if (!layer_filter.ShouldCollide(layer1, layer2)) {
       // Изменить слои или фильтр
   }
   ```

3. **Тела слишком далеко**:
   ```cpp
   // Проверить дистанцию
   float distance = (pos1 - pos2).Length();
   if (distance > 100.0f) {
       // Увеличить размер AABB или проверить wide phase
   }
   ```

### Проблема: Туннелирование быстрых объектов

**Симптомы:**

- Быстрые пули/объекты проходят сквозь стены
- Пропущенные коллизии

**Решение:**

1. **Включить спекулятивные контакты**:
   ```cpp
   JPH::PhysicsSettings settings;
   settings.mSpeculativeContactDistance = 0.1f;  // Увеличить расстояние
   physics_system.SetPhysicsSettings(settings);
   ```

2. **Использовать Continuous Collision Detection**:
   ```cpp
   JPH::BodyCreationSettings body_settings;
   body_settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
   ```

3. **Увеличить частоту обновления физики**:
   ```cpp
   // Обновлять физику с фиксированным шагом
   const float physics_dt = 1.0f / 120.0f;  // 120 Hz
   accumulator += delta_time;
   while (accumulator >= physics_dt) {
       physics_system.Update(physics_dt, ...);
       accumulator -= physics_dt;
   }
   ```

### Проблема: Нестабильные коллизии

**Симптомы:**

- Дрожание объектов при контакте
- Объекты "прыгают" или "толкаются"
- Непредсказуемое поведение

**Решение:**

1. **Настроить PhysicsSettings**:
   ```cpp
   JPH::PhysicsSettings settings;
   settings.mBaumgarte = 0.2f;            // Увеличить для большей стабильности
   settings.mPenetrationSlop = 0.01f;     // Увеличить допуск проникновения
   settings.mLinearCastThreshold = 0.05f; // Уменьшить порог
   settings.mManifoldToleranceSq = 1.0e-5f; // Увеличить толерантность
   
   physics_system.SetPhysicsSettings(settings);
   ```

2. **Увеличить массу объектов**:
   ```cpp
   // Лёгкие объекты более нестабильны
   body_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
   body_settings.mMassPropertiesOverride.mMass = 10.0f;  // Увеличить массу
   ```

3. **Добавить демпфирование**:
   ```cpp
   body_settings.mLinearDamping = 0.1f;   // Линейное демпфирование
   body_settings.mAngularDamping = 0.1f;  // Угловое демпфирование
   ```

---

## Производительность

### Проблема: Высокое использование CPU

**Симптомы:**

- Низкий FPS при включённой физике
- Высокая загрузка CPU
- Задержки в обновлении

**Диагностика:**

```cpp
void ProfilePhysicsUpdate() {
    auto start = std::chrono::high_resolution_clock::now();
    
    physics_system.Update(delta_time, ...);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<float>(end - start).count();
    
    SDL_Log("Physics update: %.3f ms", duration * 1000.0f);
    
    // Статистика системы
    uint32_t num_active = physics_system.GetNumActiveBodies();
    uint32_t num_islands = physics_system.GetNumActiveIslands();
    
    SDL_Log("Active bodies: %u, Islands: %u", num_active, num_islands);
}
```

**Оптимизации:**

1. **Уменьшить количество тел**:
   ```cpp
   // Использовать пулы объектов
   // Удалять неиспользуемые тела
   // Объединять статические объекты
   ```

2. **Оптимизировать формы**:
   ```cpp
   // Использовать простые формы (кубы, сферы)
   // Кэшировать формы
   // Использовать составные формы вместо множества отдельных
   ```

3. **Настроить JobSystem**:
   ```cpp
   // Использовать оптимальное количество потоков
   uint32_t num_threads = std::max(1u, std::thread::hardware_concurrency() - 1);
   JPH::JobSystemThreadPool job_system(
       JPH::cMaxPhysicsJobs,
       JPH::cMaxPhysicsBarriers,
       num_threads
   );
   ```

4. **Использовать сон тел**:
   ```cpp
   // Тела автоматически засыпают при отсутствии движения
   // Можно принудительно усыпить:
   body_interface.DeactivateBody(body_id);
   ```

### Проблема: Просадки производительности

**Симптомы:**

- Внезапные лаги
- Неравномерная производительность
- Пики использования CPU

**Решение:**

1. **Ограничить сложность сцены**:
   ```cpp
   // Динамически добавлять/удалять тела
   // Использовать LOD для физики
   // Отключать физику для далёких объектов
   ```

2. **Оптимизировать широкую фазу**:
   ```cpp
   // Проверить настройки широкой фазы
   // Использовать пространственное разбиение
   // Настроить размеры AABB
   ```

3. **Использовать фиксированный временной шаг**:
   ```cpp
   // Избегать переменного delta time
   const float fixed_dt = 1.0f / 60.0f;
   // Использовать накопление времени
   ```

---

## Память и утечки

### Проблема: Утечки памяти

**Симптомы:**

- Растущее использование памяти
- Со временем замедление работы
- Креши при нехватке памяти

**Диагностика:**

```cpp
void CheckMemoryUsage() {
    // Проверить использование TempAllocator
    uint64_t used = temp_allocator.GetUsed();
    uint64_t capacity = temp_allocator.GetCapacity();
    
    SDL_Log("TempAllocator: %llu / %llu bytes (%.1f%%)",
            used, capacity, (float)used / capacity * 100.0f);
    
    // Проверить количество тел
    uint32_t num_bodies = physics_system.GetNumBodies();
    SDL_Log("Total bodies: %u", num_bodies);
    
    // Проверить на наличие "зомби" тел
    for (uint32_t i = 0; i < num_bodies; ++i) {
        JPH::BodyID body_id(i);
        if (physics_system.GetBodyInterface().IsAdded(body_id)) {
            // Тело активно
        }
    }
}
```

**Решение:**

1. **Уничтожать тела правильно**:
   ```cpp
   // ❌ Неправильно:
   // Просто забыть про тело
   
   // ✅ Правильно:
   body_interface.RemoveBody(body_id);
   body_interface.DestroyBody(body_id);
   ```

2. **Очищать TempAllocator**:
   ```cpp
   // TempAllocator автоматически очищается после каждого Update
   // Но можно принудительно сбросить:
   temp_allocator.Clear();
   ```

3. **Использовать пулы объектов**:
   ```cpp
   class PhysicsBodyPool {
   public:
       JPH::BodyID CreateBody() {
           if (!free_ids.empty()) {
               JPH::BodyID id = free_ids.back();
               free_ids.pop_back();
               return id;
           }
           // Создать новое тело
       }
       
       void DestroyBody(JPH::BodyID id) {
           body_interface.DestroyBody(id);
           free_ids.push_back(id);
       }
   private:
       std::vector<JPH::BodyID> free_ids;
   };
   ```

### Проблема: Фрагментация памяти

**Симптомы:**

- Медленное создание тел со временем
- Увеличение времени обновления
- Нестабильная производительность

**Решение:**

1. **Использовать VMA (Vulkan Memory Allocator) аналоги**:
   ```cpp
   // JoltPhysics имеет внутренний аллокатор
   // Но можно использовать собственные пулы
   ```

2. **Переиспользовать формы**:
   ```cpp
   std::unordered_map<std::string, JPH::ShapeRefC> shape_cache;
   
   JPH::ShapeRefC GetOrCreateBox(const JPH::Vec3& size) {
       std::string key = fmt::format("box_{}_{}_{}", size.GetX(), size.GetY(), size.GetZ());
       auto it = shape_cache.find(key);
       if (it != shape_cache.end()) return it->second;
       
       JPH::BoxShapeSettings settings(size * 0.5f);
       JPH::ShapeRefC shape = settings.Create().Get();
       shape_cache[key] = shape;
       return shape;
   }
   ```

3. **Периодическая дефрагментация**:
   ```cpp
   // JoltPhysics автоматически дефрагментирует память
   // Но можно принудительно вызвать оптимизацию:
   physics_system.OptimizeBroadPhase();
   ```

---

## Проблемы, специфичные для ProjectV

> ⚠️ **Контент перемещён**
>
> Вся информация об интеграции JoltPhysics с ProjectV, включая проблемы с HeightFieldShape, интеграцию с ECS и
> синхронизацию с рендерером, была перемещена в общий файл интеграции для лучшей организации и избежания дублирования.

### Где найти информацию?

Все материалы по проблемам интеграции JoltPhysics с ProjectV теперь доступны
в [projectv-integration.md](projectv-integration.md):

- [Решение проблем (ProjectV-specific)](projectv-integration.md#решение-проблем-projectv-specific)
- [HeightFieldShape для вокселей не работает](projectv-integration.md#проблема-heightfieldshape-для-вокселей-не-работает)
- [Интеграция с flecs ECS](projectv-integration.md#проблема-интеграция-с-flecs-ecs)
- [Рассинхронизация с рендерером](projectv-integration.md#проблема-рассинхронизация-с-рендерером)

### Общие рекомендации

Если вы испытываете проблемы с интеграцией JoltPhysics в ProjectV:

1. **Сначала проверьте общие проблемы** в этом руководстве
2. **Обратитесь к интеграционной документации** для ProjectV-specific решений
3. **Используйте отладку и профилирование** для диагностики
4. **Сверьтесь с официальной документацией** JoltPhysics для API-специфичных вопросов

---

## Отладка и инструменты

### Визуализация отладки

```cpp
void RenderPhysicsDebug() {
#ifdef JPH_DEBUG_RENDERER
    JPH::DebugRenderer* debug = physics_system.GetDebugRenderer();
    if (debug) {
        // Рендерить AABB всех тел
        physics_system.DrawBodies(JPH::DebugRenderer::EBodyDrawMode::BoundingBox);
        
        // Рендерить контакты
        physics_system.DrawContacts();
        
        // Рендерить ограничения
        physics_system.DrawConstraints();
    }
#endif
}
```

### Статистика и метрики

```cpp
void LogPhysicsStats() {
    uint32_t stats[static_cast<int>(JPH::EPhysicsStatistics::NumStatistics)];
    physics_system.GetPhysicsStatistics(stats);
    
    SDL_Log("Physics Statistics:");
    SDL_Log("  Bodies: %u", stats[static_cast<int>(JPH::EPhysicsStatistics::NumBodies)]);
    SDL_Log("  Active Bodies: %u", stats[static_cast<int>(JPH::EPhysicsStatistics::NumActiveBodies)]);
    SDL_Log("  Contacts: %u", stats[static_cast<int>(JPH::EPhysicsStatistics::NumContacts)]);
    SDL_Log("  Constraints: %u", stats[static_cast<int>(JPH::EPhysicsStatistics::NumConstraints)]);
    SDL_Log("  Islands: %u", stats[static_cast<int>(JPH::EPhysicsStatistics::NumIslands)]);
    SDL_Log("  Broad Phase Pairs: %u", stats[static_cast<int>(JPH::EPhysicsStatistics::NumBroadPhasePairs)]);
}
```

### Интеграция с Tracy

```cpp
#include <tracy/Tracy.hpp>

void UpdatePhysicsWithProfiling(float dt) {
    ZoneScopedN("PhysicsUpdate");
    
    {
        ZoneScopedN("PhysicsStep");
        physics_system.Update(dt, ...);
    }
    
    {
        ZoneScopedN("UpdateTransforms");
        UpdateRenderTransforms();
    }
    
    FrameMark;  // Отметить кадр в Tracy
}
```

---

## Часто задаваемые вопросы

### Q: Почему тела падают слишком медленно/быстро?

**A:** Проверить гравитацию:

```cpp
JPH::Vec3 gravity = physics_system.GetGravity();
SDL_Log("Gravity: (%f, %f, %f)", gravity.GetX(), gravity.GetY(), gravity.GetZ());

// Изменить гравитацию
physics_system.SetGravity(JPH::Vec3(0, -9.81f, 0));  // Земная гравитация
```

### Q: Как сделать тело невесомым?

**A:** Установить множитель гравитации:

```cpp
body_settings.mGravityFactor = 0.0f;  // Без гравитации
// или
body_interface.SetGravityFactor(body_id, 0.0f);
```

### Q: Почему вращение тела странное?

**A:** Проверить тензор инерции:

```cpp
// Автоматически вычисляется из формы
// Или задать вручную:
body_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
body_settings.mMassPropertiesOverride.mInertia = JPH::Mat44::sScale(JPH::Vec3(1, 1, 1));
```

### Q: Как сделать триггер (датчик)?

**A:** Использовать слой датчика:

```cpp
body_settings.mObjectLayer = (JPH::uint8)ObjectLayer::SENSOR;
// и настроить фильтр, чтобы датчики не создавали сил
```

### Q: Как синхронизировать физику в сетевой игре?

**A:** Использовать детерминированную симуляцию:

```cpp
// 1. Фиксированный временной шаг
// 2. Детерминированные начальные условия
// 3. Синхронизированные seed для рандома
// 4. Отправлять только входные данные, а не состояния
```

### Q: Как оптимизировать физику для мобильных устройств?

**A:**

1. Уменьшить количество тел (макс 100-200)
2. Использовать простые формы
3. Уменьшить частоту обновления (30 Гц вместо 60)
4. Отключать физику для далёких объектов
5. Использовать упрощённые коллайдеры

---

## Получение дополнительной помощи

1. **Официальная документация**: [JoltPhysics GitHub](https://github.com/jrouwe/JoltPhysics)
2. **Примеры кода**: `external/JoltPhysics/HelloWorld/`
3. **Форумы и сообщества**: Discord каналы игровой разработки
4. **Исходный код**: Изучение `external/JoltPhysics/Jolt/`

**Перед обращением за помощью подготовьте:**

- Минимальный воспроизводимый пример
- Версию JoltPhysics (5.5.1 в ProjectV)
- Логи ошибок и ассертов
- Конфигурацию системы (ОС, CPU, память)
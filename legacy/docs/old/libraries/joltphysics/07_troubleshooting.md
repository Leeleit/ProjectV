# Решение проблем JoltPhysics

🟡 **Уровень 2: Средний**

## Проблемы инициализации

### Фабрика не создана

**Симптомы:**

- Крэш при создании формы
- Ошибка "Factory not initialized"

**Решение:**

```cpp
// Правильный порядок инициализации
JPH::RegisterDefaultAllocator();        // 1. Аллокатор
JPH::Factory::sInstance = new JPH::Factory();  // 2. Фабрика
JPH::RegisterTypes();                   // 3. Типы
```

### Типы не зарегистрированы

**Симптомы:**

- Ошибка "Type not registered"
- Крэш при создании определённых форм

**Решение:**

```cpp
// Регистрация всех типов
JPH::RegisterTypes();

// Проверка регистрации
if (!JPH::Factory::sInstance->IsRegistered(JPH::ETagShapeSubType::Box)) {
    // Тип не зарегистрирован
}
```

---

## Проблемы с телами

### Тела не сталкиваются

**Возможные причины:**

1. **Одинаковые слои без коллизии:**

```cpp
// Проверьте ObjectLayerPairFilter
bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override
{
    // Static vs Static не сталкиваются
    if (inObject1 == Layers::NON_MOVING && inObject2 == Layers::NON_MOVING)
        return false;
    return true;
}
```

2. **Тело не добавлено в мир:**

```cpp
// Неправильно
JPH::BodyID id = interface.CreateBody(settings);
// Тело создано, но не в мире!

// Правильно
JPH::BodyID id = interface.CreateAndAddBody(settings, JPH::EActivation::Activate);
```

3. **Тело деактивировано:**

```cpp
if (!interface.IsActive(body_id)) {
    interface.ActivateBody(body_id);
}
```

### Тела проваливаются друг сквозь друга

**Причины:**

1. **Слишком большая скорость:**

```cpp
// Увеличьте количество collision steps
physics_system.Update(delta_time, 4, &temp_allocator, &job_system);
```

2. **Тонкие объекты:**

```cpp
// Используйте Continuous Collision Detection
body_settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
```

3. **Неправильный convex radius:**

```cpp
// Convex radius должен быть меньше половины наименьшего размера
JPH::BoxShapeSettings box(JPH::Vec3(0.5f, 0.5f, 0.5f), 0.1f);  // OK
JPH::BoxShapeSettings box(JPH::Vec3(0.5f, 0.5f, 0.5f), 0.6f);  // ОШИБКА!
```

### Тела "дрожат" или нестабильны

**Решения:**

1. **Уменьшите массу:**

```cpp
// Слишком маленькая масса = нестабильность
body_settings.mMass = 1.0f;  // Минимум для стабильности
```

2. **Настройте damping:**

```cpp
body_settings.mLinearDamping = 0.1f;
body_settings.mAngularDamping = 0.2f;
```

3. **Увеличьте итерации solver:**

```cpp
physics_system.Init(
    1024, 0, 1024, 1024,
    ...
);
// Или через collision steps
physics_system.Update(delta_time, 2, &temp_allocator, &job_system);
```

---

## Проблемы с формами

### HeightFieldShape не создаётся

**Симптомы:**

- Create() возвращает ошибку
- Invalid HeightFieldShape

**Проверки:**

```cpp
// 1. Размер массива
uint32_t total_samples = sample_count * sample_count;
if (heights.size() != total_samples) {
    // Ошибка: неправильный размер
}

// 2. Делимость на block size
if (sample_count % settings.mBlockSize != 0) {
    // Ошибка: sample_count должен делиться на mBlockSize
}

// 3. Валидация значений
for (float h : heights) {
    if (std::isnan(h) || std::isinf(h)) {
        // Ошибка: невалидное значение высоты
    }
}
```

### MeshShape вызывает крэш

**Причины:**

1. **Дегенеративные треугольники:**

```cpp
// Проверка треугольников
for (const auto& tri : triangles) {
    JPH::Vec3 v0(tri.mV[0].x, tri.mV[0].y, tri.mV[0].z);
    JPH::Vec3 v1(tri.mV[1].x, tri.mV[1].y, tri.mV[1].z);
    JPH::Vec3 v2(tri.mV[2].x, tri.mV[2].y, tri.mV[2].z);

    JPH::Vec3 normal = (v1 - v0).Cross(v2 - v0);
    if (normal.LengthSq() < 1e-10f) {
        // Дегенеративный треугольник (площадь ≈ 0)
    }
}
```

2. **Слишком много треугольников:**

```cpp
// MeshShape для статики только
if (body_settings.mMotionType != JPH::EMotionType::Static) {
    // MeshShape не поддерживает динамику!
    // Используйте ConvexHullShape или CompoundShape
}
```

### ConvexHullShape не выпуклый

```cpp
// Ошибка: точки должны образовывать выпуклую оболочку
JPH::ConvexHullShapeSettings settings(points.data(), points.size());
auto result = settings.Create();

if (!result.IsValid()) {
    std::cout << "Error: " << result.GetError() << std::endl;
    // Возможная причина: точки на одной плоскости или вырожденный набор
}
```

---

## Проблемы производительности

### Низкий FPS

**Диагностика:**

```cpp
// Включите профилирование
#define JPH_PROFILE_ENABLED

// Получите статистику
uint32_t num_bodies = physics_system.GetNumBodies();
uint32_t num_active = physics_system.GetNumActiveBodies();
uint32_t num_constraints = physics_system.GetNumConstraints();

std::cout << "Bodies: " << num_bodies << ", Active: " << num_active << std::endl;
```

**Решения:**

1. **Уменьшите активные тела:**

```cpp
// Настройте sleep
JPH::BodyCreationSettings settings;
settings.mAllowSleeping = true;
```

2. **Оптимизируйте broad phase:**

```cpp
// После загрузки уровня
physics_system.OptimizeBroadPhase();
```

3. **Используйте JobSystem:**

```cpp
JPH::JobSystemThreadPool job_system(
    JPH::cMaxPhysicsJobs,
    JPH::cMaxPhysicsBarriers,
    std::thread::hardware_concurrency() - 1
);
```

### Утечки памяти

**Проверка:**

```cpp
// Подсчёт ссылок на формы
JPH::ShapeRefC shape = settings.Create().Get();
// shape будет автоматически удалён когда счётчик = 0

// Проверка живых тел
JPH::BodyIDVector body_ids;
physics_system.GetBodies(body_ids);
std::cout << "Active bodies: " << body_ids.size() << std::endl;
```

**Частые причины:**

```cpp
// Неправильно: забыли удалить тело
interface.CreateBody(settings);
// Нет вызова DestroyBody!

// Правильно
JPH::BodyID id = interface.CreateBody(settings);
interface.AddBody(id, JPH::EActivation::Activate);
// ...
interface.RemoveBody(id);
interface.DestroyBody(id);
```

---

## Проблемы с ограничениями

### Ограничения не работают

**Проверки:**

```cpp
// 1. Ограничение добавлено в систему?
physics_system.AddConstraint(constraint);

// 2. Ограничение активно?
if (!constraint->IsActive()) {
    // Возможно, тела слишком далеко друг от друга
}

// 3. Ограничение включено?
constraint->SetEnabled(true);
```

### Ragdoll разваливается

**Решения:**

1. **Увеличьте итерации:**

```cpp
physics_system.Init(
    ...,
    max_contact_constraints * 2  // Больше контактов для ragdoll
);
```

2. **Настройте damping:**

```cpp
body_settings.mLinearDamping = 0.2f;
body_settings.mAngularDamping = 0.3f;
```

3. **Проверьте лимиты суставов:**

```cpp
// Лимиты должны быть реалистичными
settings.mTwistMinAngle = -0.5f;  // Не слишком большие углы
settings.mTwistMaxAngle = 0.5f;
```

---

## Многопоточные проблемы

### Data races

**Симптомы:**

- Случайные крэши
- Несогласованные результаты

**Решение:**

```cpp
// Используйте правильный интерфейс
// Неправильно в многопоточном режиме:
JPH::BodyInterface& interface = physics_system.GetBodyInterfaceNoLock();

// Правильно в многопоточном режиме:
JPH::BodyInterface& interface = physics_system.GetBodyInterface();
```

### Deadlock

**Причины:**

- Блокировка мьютексов в неправильном порядке
- Обращение к телам из ContactListener

**Решение:**

```cpp
// В ContactListener не вызывайте GetBodyInterface()
class MyContactListener : public JPH::ContactListener
{
    void OnContactAdded(...) override
    {
        // Неправильно:
        // interface.GetPosition(body_id);  // Может вызвать deadlock!

        // Правильно: используйте inBody1, inBody2
        JPH::RVec3 pos = inBody1.GetPosition();
    }
};
```

---

## Debug инструменты

### Визуализация

```cpp
// Включите debug renderer
#define JPH_DEBUG_RENDERER

// Реализуйте JPH::DebugRenderer
class MyDebugRenderer : public JPH::DebugRenderer
{
    void DrawLine(RVec3Arg inFrom, RVec3Arg inTo, ColorArg inColor) override;
    void DrawTriangle(RVec3Arg inV1, RVec3Arg inV2, RVec3Arg inV3, ColorArg inColor) override;
    void DrawText3D(RVec3Arg inPosition, const string_view& inString, ColorArg inColor) override;
};

// Отрисовка
MyDebugRenderer renderer;
physics_system.DrawBodies(JPH::BodyManager::DrawSettings(), &renderer);
```

### Validation

```cpp
// Проверка целостности физической системы
#ifdef JPH_DEBUG
    physics_system.ValidateContactCache();
#endif
```

### Логирование

```cpp
// Включите трассировку
#define JPH_TRACE_ENABLED

// Проверка ошибок при Update
JPH::EPhysicsUpdateError errors = physics_system.Update(...);
if (errors != JPH::EPhysicsUpdateError::None) {
    if (errors & JPH::EPhysicsUpdateError::ManifoldCacheFull)
        std::cout << "Manifold cache full!" << std::endl;
    if (errors & JPH::EPhysicsUpdateError::BodyPairCacheFull)
        std::cout << "Body pair cache full!" << std::endl;
    if (errors & JPH::EPhysicsUpdateError::ContactConstraintsFull)
        std::cout << "Contact constraints full!" << std::endl;
}
```

---

## Частые ошибки

| Ошибка                  | Причина                             | Решение                           |
|-------------------------|-------------------------------------|-----------------------------------|
| Factory not initialized | Не создана фабрика                  | `new JPH::Factory()`              |
| Types not registered    | Не вызваны `RegisterTypes()`        | `JPH::RegisterTypes()`            |
| Invalid shape           | Некорректные параметры формы        | Проверить размеры, точки          |
| Body not in world       | Тело не добавлено                   | `AddBody()`                       |
| Thread safety violation | Несинхронизированный доступ         | Использовать `GetBodyInterface()` |
| Out of memory           | Недостаточно памяти в TempAllocator | Увеличить размер                  |

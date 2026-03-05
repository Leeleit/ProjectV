# Основные понятия JoltPhysics

🟡 **Уровень 2: Средний**

## Слои объектов (Object Layers)

### Назначение

Каждому телу присваивается **Object Layer** — целое число от 0 до 31 (или до 255). Слои позволяют управлять тем, какие
объекты могут сталкиваться друг с другом.

```cpp
namespace Layers
{
    static constexpr JPH::ObjectLayer NON_MOVING = 0;  // Статические объекты
    static constexpr JPH::ObjectLayer MOVING = 1;      // Динамические объекты
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
}
```

### BroadPhase Layers

Каждый BroadPhase Layer создаёт отдельное дерево ограничивающих объёмов (AABB tree):

- **Статические объекты** — дерево не обновляется часто
- **Динамические объекты** — дерево обновляется каждый кадр

```cpp
namespace BroadPhaseLayers
{
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr uint32_t NUM_LAYERS(2);
}
```

### Фильтры коллизий

Для управления столкновениями между слоями используются три интерфейса:

#### 1. ObjectLayerPairFilter

Определяет, могут ли два Object Layer'а столкнуться:

```cpp
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
{
public:
    bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
    {
        switch (inObject1)
        {
        case Layers::NON_MOVING:
            return inObject2 == Layers::MOVING;  // Static только с dynamic
        case Layers::MOVING:
            return true;  // Dynamic со всеми
        default:
            return false;
        }
    }
};
```

#### 2. BroadPhaseLayerInterface

Маппит Object Layer → Broad Phase Layer:

```cpp
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
    BPLayerInterfaceImpl()
    {
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    uint32_t GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
    {
        return mObjectToBroadPhase[inLayer];
    }

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};
```

#### 3. ObjectVsBroadPhaseLayerFilter

Определяет, может ли Object Layer столкнуться с BroadPhase Layer:

```cpp
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

---

## Фазы коллизий

### Broad Phase (Широкая фаза)

1. **Назначение**: Быстро определить, какие пары тел *могут* пересекаться
2. **Реализация**: Деревья AABB для каждого BroadPhase Layer
3. **Результат**: Список пар тел для Narrow Phase

### Narrow Phase (Узкая фаза)

1. **Назначение**: Точная проверка столкновений между конкретными формами
2. **Реализация**: Алгоритмы GJK/EPA для выпуклых форм
3. **Результат**: Контактные многообразия с точками, нормалями, глубиной

### Конвейер

```
Broad Phase          Narrow Phase         Solver
     ↓                     ↓                 ↓
AABB test       →    GJK/EPA test    →   Resolve contacts
     ↓                     ↓                 ↓
Candidate pairs      Contact points      Updated positions
```

---

## Типы движения тел

### Static (Статические)

- Не двигаются (бесконечная масса)
- Не требуют обновления в broad phase дереве
- Примеры: пол, стены, ландшафт

### Kinematic (Кинематические)

- Движение задаётся вручную (позиция/скорость)
- Толкают динамические объекты при столкновении
- Не подвержены силам и гравитации
- Примеры: движущиеся платформы, лифты, двери

### Dynamic (Динамические)

- Подчиняются законам физики (силы, импульсы)
- Могут спать (деактивироваться) для экономии ресурсов
- Примеры: кубы, сферы, персонажи (как ragdoll)

```cpp
enum class EMotionType
{
    Static,      // Неподвижное тело
    Kinematic,   // Движение задаётся вручную
    Dynamic      // Подчиняется физике
}
```

---

## Детерминированная симуляция

### Что это?

При одинаковых входных данных симуляция даёт **бит-в-бит одинаковый** результат на любом оборудовании.

### Зачем нужно?

1. **Сетевая синхронизация** — передавать только входные данные, а не полное состояние
2. **Воспроизводимость** — багрепорты, демозаписи, тесты
3. **Отладка** — одинаковые результаты при повторных запусках

### Включение

```cmake
# При сборке JoltPhysics
set(JPH_CROSS_PLATFORM_DETERMINISTIC ON CACHE BOOL "" FORCE)
```

### Ограничения

- Требует `JPH_CROSS_PLATFORM_DETERMINISTIC` при компиляции
- Порядок добавления/удаления тел влияет на результат
- Многопоточность требует фиксированного seed

---

## Многопоточность и JobSystem

> **Примечание:** Подробное объяснение паттернов многопоточности в C++ см.
> в [docs/guides/cpp/08_multithreading.md](../../guides/cpp/08_multithreading.md).

### Архитектура

Jolt использует систему задач (jobs) для параллелизации:

| Job тип             | Назначение                 |
|---------------------|----------------------------|
| PhysicsUpdateJob    | Основное обновление физики |
| BroadPhaseJob       | Обновление широкой фазы    |
| NarrowPhaseJob      | Обновление узкой фазы      |
| SolveConstraintsJob | Решение ограничений        |
| BodyUpdateJob       | Обновление позиций тел     |

### JobSystemThreadPool

```cpp
#include <Jolt/Core/JobSystemThreadPool.h>

// Создание пула потоков
uint32_t num_threads = std::thread::hardware_concurrency() - 1;
JPH::JobSystemThreadPool job_system(
    JPH::cMaxPhysicsJobs,        // Макс. задач
    JPH::cMaxPhysicsBarriers,    // Макс. барьеров
    num_threads                   // Рабочие потоки
);

// Использование
physics_system.Update(delta_time, 1, &temp_allocator, &job_system);
```

### Когда использовать JobSystem?

| Количество тел | Рекомендация                          |
|----------------|---------------------------------------|
| < 100          | Однопоточный режим (nullptr)          |
| 100-500        | Опционально, измерить профилировщиком |
| > 500          | Рекомендуется JobSystem               |

---

## Контакты и слушатели

### ContactListener

```cpp
class MyContactListener : public JPH::ContactListener
{
public:
    // Перед созданием контакта (можно отклонить)
    JPH::ValidateResult OnContactValidate(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        JPH::RVec3Arg inBaseOffset,
        const JPH::CollideShapeResult& inCollisionResult) override
    {
        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    // При добавлении контакта
    void OnContactAdded(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        const JPH::ContactManifold& inManifold,
        JPH::ContactSettings& ioSettings) override {}

    // При сохранении контакта
    void OnContactPersisted(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        const JPH::ContactManifold& inManifold,
        JPH::ContactSettings& ioSettings) override {}

    // При удалении контакта
    void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override {}
};
```

### BodyActivationListener

```cpp
class MyBodyActivationListener : public JPH::BodyActivationListener
{
public:
    void OnBodyActivated(const JPH::BodyID& inBodyID, uint64_t inBodyUserData) override {}
    void OnBodyDeactivated(const JPH::BodyID& inBodyID, uint64_t inBodyUserData) override {}
};
```

### Установка слушателей

```cpp
MyContactListener contact_listener;
MyBodyActivationListener activation_listener;

physics_system.SetContactListener(&contact_listener);
physics_system.SetBodyActivationListener(&activation_listener);
```

---

## Производительность

### Критические параметры Init()

```cpp
physics_system.Init(
    max_bodies = 1024,           // Максимум тел
    num_body_mutexes = 0,        // 0 = автоматически
    max_body_pairs = 1024,       // Максимум пар в broad phase
    max_contact_constraints = 1024  // Максимум контактов
);
```

### Рекомендации

1. **Используйте слои** — разделяйте статические и динамические объекты
2. **Batch добавление** — добавляйте тела группами, а не по одному
3. **OptimizeBroadPhase()** — вызовите после загрузки уровня
4. **Настройте Sleep** — используйте `SetSleepSettings()` для контроля деактивации
5. **TempAllocator** — предварительно выделяйте память

### Профилирование

Включите `JPH_PROFILE_ENABLED` для сбора статистики:

- Время в каждой фазе
- Количество активных тел, контактов, ограничений
- Использование памяти

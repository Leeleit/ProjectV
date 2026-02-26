# JoltPhysics: Чистый справочник для студентов

**JoltPhysics** — современный физический движок с поддержкой детерминированной симуляции, оптимизированный для
многопоточности и разработанный для высокой производительности в игровых проектах.

> **Для понимания:** Представьте JoltPhysics как "умный конструктор LEGO для физики". Вы берёте готовые блоки (формы,
> тела, ограничения), соединяете их по правилам, и получаете реалистичную физическую симуляцию без необходимости писать
> сложные алгоритмы с нуля.

## Основные возможности

- **Детерминированная симуляция** — бит-в-бит воспроизводимые результаты на разных платформах
- **Многопоточность** — встроенная система задач (JobSystem) для распараллеливания
- **Современный C++** — требует C++17 или новее
- **Широкий набор форм** — примитивы, выпуклые оболочки, меши, высотные поля
- **Система ограничений** — шарниры, пружины, моторы, ragdoll
- **Character controller** — виртуальный персонаж для игр

## Типичное применение

- Игровые движки (AAA и indie)
- Симуляторы транспортных средств
- VR/AR приложения
- Физические головоломки
- Сетевые игры (детерминированная физика)

## Архитектура

### Основные компоненты

| Компонент         | Назначение                                 |
|-------------------|--------------------------------------------|
| **PhysicsSystem** | Центральный объект, управляет симуляцией   |
| **BodyInterface** | Интерфейс для создания и управления телами |
| **Shape**         | Геометрические формы для коллизий          |
| **Constraint**    | Ограничения между телами                   |
| **JobSystem**     | Система задач для многопоточности          |

### Конвейер симуляции

```
Broad Phase → Narrow Phase → Solver → Position Update
     ↓              ↓           ↓
  AABB test     GJK/EPA    Constraints
```

> **Для понимания:** Физический движок — это как фабрика по производству. На конвейере сначала грубо сортируют детали (
> Broad Phase), потом точно подгоняют (Narrow Phase), затем собирают (Solver), и наконец упаковывают (Position Update).

---

## Слои объектов (Object Layers)

### Назначение

Каждому телу присваивается **Object Layer** — целое число от 0 до 31 (или до 255). Слои позволяют управлять тем, какие
объекты могут сталкиваться друг с другом.

```cpp
namespace Layers
{
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
}
```

> **Для понимания:** Object Layers — это как "цветные наклейки" на объектах. Красные объекты сталкиваются только с
> синими, зелёные — со всеми, и т.д. Это позволяет создавать сложные правила взаимодействия без проверки каждого объекта
> отдельно.

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
            return inObject2 == Layers::MOVING;
        case Layers::MOVING:
            return true;
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

> **Для понимания:** Broad Phase — это как "быстрый поиск в телефонной книге". Вместо того чтобы звонить каждому
> человеку (проверять каждую пару тел), вы сначала смотрите на букву фамилии (AABB-дерево), чтобы сузить круг поиска.

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
    Static,
    Kinematic,
    Dynamic
}
```

> **Для понимания:** Представьте три типа объектов в парке аттракционов: Static — это здания (не двигаются), Kinematic —
> это карусель (движется по заданному пути), Dynamic — это мячики в бассейне с шариками (летают куда хотят по законам
> физики).

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
set(JPH_CROSS_PLATFORM_DETERMINISTIC ON CACHE BOOL "" FORCE)
```

### Ограничения

- Требует `JPH_CROSS_PLATFORM_DETERMINISTIC` при компиляции
- Порядок добавления/удаления тел влияет на результат
- Многопоточность требует фиксированного seed

---

## Многопоточность и JobSystem

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

uint32_t num_threads = std::thread::hardware_concurrency() - 1;
JPH::JobSystemThreadPool job_system(
    JPH::cMaxPhysicsJobs,
    JPH::cMaxPhysicsBarriers,
    num_threads
);

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
    JPH::ValidateResult OnContactValidate(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        JPH::RVec3Arg inBaseOffset,
        const JPH::CollideShapeResult& inCollisionResult) override
    {
        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    void OnContactAdded(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        const JPH::ContactManifold& inManifold,
        JPH::ContactSettings& ioSettings) override {}

    void OnContactPersisted(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        const JPH::ContactManifold& inManifold,
        JPH::ContactSettings& ioSettings) override {}

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

---

## Формы (Shapes)

### BoxShape

```cpp
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

JPH::BoxShapeSettings box_settings(JPH::Vec3(1.0f, 1.0f, 1.0f));
JPH::ShapeRefC box_shape = box_settings.Create().Get();
```

### SphereShape

```cpp
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

JPH::SphereShapeSettings sphere_settings(1.0f);
JPH::ShapeRefC sphere_shape = sphere_settings.Create().Get();
```

### CapsuleShape

```cpp
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>

JPH::CapsuleShapeSettings capsule_settings(1.0f, 0.5f);
JPH::ShapeRefC capsule_shape = capsule_settings.Create().Get();
```

### ConvexHullShape

```cpp
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>

std::vector<JPH::Vec3> points = {
    JPH::Vec3(0, 0, 0),
    JPH::Vec3(1, 0, 0),
    JPH::Vec3(0, 1, 0),
    JPH::Vec3(0, 0, 1)
};

JPH::ConvexHullShapeSettings hull_settings(points.data(), points.size(), 0.05f);
JPH::ShapeRefC hull_shape = hull_settings.Create().Get();
```

### HeightFieldShape

```cpp
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>

const uint32_t sample_count = 32;
std::vector<float> heights(sample_count * sample_count);

JPH::HeightFieldShapeSettings heightfield_settings(
    heights.data(),
    JPH::Vec3(0, 0, 0),
    JPH::Vec3(1.0f, 1.0f, 1.0f),
    sample_count
);

JPH::ShapeRefC heightfield_shape = heightfield_settings.Create().Get();
```

### MeshShape

```cpp
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

JPH::MeshShapeSettings mesh_settings;
JPH::Triangle triangle;
triangle.mV[0] = JPH::Float3(0, 0, 0);
triangle.mV[1] = JPH::Float3(1, 0, 0);
triangle.mV[2] = JPH::Float3(0, 1, 0);
mesh_settings.AddTriangle(triangle);

JPH::ShapeRefC mesh_shape = mesh_settings.Create().Get();
```

---

## Ограничения (Constraints)

### FixedConstraint

```cpp
#include <Jolt/Physics/Constraints/FixedConstraint.h>

JPH::FixedConstraintSettings settings;
settings.mAutoDetectPoint = true;

JPH::Constraint* constraint = settings.Create(body1, body2);
physics_system.AddConstraint(constraint);
```

### HingeConstraint

```cpp
#include <Jolt/Physics/Constraints/HingeConstraint.h>

JPH::HingeConstraintSettings settings;
settings.mPoint1 = JPH::RVec3(0, 1, 0);
settings.mPoint2 = JPH::RVec3(0, 1, 0);
settings.mHingeAxis1 = JPH::Vec3::sAxisY();
settings.mHingeAxis2 = JPH::Vec3::sAxisY();

JPH::Constraint* hinge = settings.Create(body1, body2);
physics_system.AddConstraint(hinge);
```

### DistanceConstraint

```cpp
#include <Jolt/Physics/Constraints/DistanceConstraint.h>

JPH::DistanceConstraintSettings settings;
settings.mPoint1 = JPH::RVec3(0, 0, 0);
settings.mPoint2 = JPH::RVec3(0, 2, 0);
settings.mDistance = 2.0f;

JPH::Constraint* constraint = settings.Create(body1, body2);
physics_system.AddConstraint(constraint);
```

### SwingTwistConstraint

```cpp
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>

JPH::SwingTwistConstraintSettings settings;
settings.mPosition1 = JPH::RVec3(0, 0, 0);
settings.mPosition2 = JPH::RVec3(0, 0, 0);
settings.mTwistAxis1 = JPH::Vec3::sAxisY();
settings.mTwistAxis2 = JPH::Vec3::sAxisY();
settings.mTwistMinAngle = -JPH::JPH_PI * 0.5f;
settings.mTwistMaxAngle = JPH::JPH_PI * 0.5f;

JPH::Constraint* constraint = settings.Create(body1, body2);
physics_system.AddConstraint(constraint);
```

---

## Производительность

### Критические параметры Init()

```cpp
physics_system.Init(
    1024,   // max_bodies
    0,      // num_body_mutexes
    1024,   // max_body_pairs
    1024    // max_contact_constraints
);
```

### Рекомендации

1. **Используйте слои** — разделяйте статические и динамические объекты
2. **Batch добавление** — добавляйте тела группами
3. **OptimizeBroadPhase()** — вызовите после загрузки уровня
4. **TempAllocator** — предварительно выделяйте память

### TempAllocator

```cpp
JPH::TempAllocatorImpl temp_allocator(10 * 1024 * 1024);
```

| Сложность сцены        | Размер аллокатора |
|------------------------|-------------------|
| Простая (< 100 тел)    | 1 MB              |
| Средняя (100-1000 тел) | 10 MB             |
| Сложная (> 1000 тел)   | 32 MB             |

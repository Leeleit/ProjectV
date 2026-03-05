# Основные понятия

**🟡 Уровень 2: Средний**

## На этой странице

- [Слои объектов и фильтры коллизий](#слои-объектов-и-фильтры-коллизий)
- [Широкие и узкие фазы](#широкие-и-узкие-фазы)
- [Типы движения тел](#типы-движения-тел)
- [Формы (Shapes)](#формы-shapes)
- [Ограничения (Constraints)](#ограничения-constraints)
- [Детерминированная симуляция](#детерминированная-симуляция)
- [Многопоточность и JobSystem](#многопоточность-и-jobsystem)
- [Контакты и слушатели](#контакты-и-слушатели)
- [Оптимизации для воксельного движка](#оптимизации-для-воксельного-движка)
- [Производительность и настройка](#производительность-и-настройка)

---

## Слои объектов и фильтры коллизий

### Object Layers

Каждому телу присваивается **Object Layer** — целое число от 0 до 31 (или до 255, зависит от настроек). Слои позволяют
управлять тем, какие объекты могут сталкиваться друг с другом.

```cpp
namespace Layers
{
    static constexpr ObjectLayer NON_MOVING = 0;  // Статические объекты
    static constexpr ObjectLayer MOVING = 1;      // Динамические объекты
    static constexpr ObjectLayer SENSOR = 2;      // Сенсоры/триггеры
    static constexpr ObjectLayer NUM_LAYERS = 3;
};
```

### Фильтры коллизий

Для управления столкновениями между слоями используются три типа фильтров:

1. **ObjectLayerPairFilter** — определяет, могут ли два Object Layer'а столкнуться.
2. **BroadPhaseLayerInterface** — маппит Object Layer → Broad Phase Layer.
3. **ObjectVsBroadPhaseLayerFilter** — определяет, может ли Object Layer столкнуться с BroadPhase Layer.

#### Пример полной реализации фильтров из HelloWorld.cpp

Вот полная реализация всех трёх фильтров из официального примера JoltPhysics:

```cpp
// ObjectLayerPairFilter - определяет столкновения между объектными слоями
class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter
{
public:
    virtual bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override
    {
        switch (inObject1)
        {
        case Layers::NON_MOVING:
            return inObject2 == Layers::MOVING; // Non moving only collides with moving
        case Layers::MOVING:
            return true; // Moving collides with everything
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

// BroadPhaseLayerInterface - отображает объектные слои на broad phase слои
class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface
{
public:
    BPLayerInterfaceImpl()
    {
        // Create a mapping table from object to broad phase layer
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    virtual uint GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    virtual BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override
    {
        JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char *GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override
    {
        switch ((BroadPhaseLayer::Type)inLayer)
        {
        case (BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:
            return "NON_MOVING";
        case (BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:
            return "MOVING";
        default:
            JPH_ASSERT(false);
            return "INVALID";
        }
    }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
    BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

// ObjectVsBroadPhaseLayerFilter - определяет столкновения между объектным слоем и broad phase слоем
class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter
{
public:
    virtual bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override
    {
        switch (inLayer1)
        {
        case Layers::NON_MOVING:
            return inLayer2 == BroadPhaseLayers::MOVING;
        case Layers::MOVING:
            return true;
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};
```

#### Определение слоёв

```cpp
namespace Layers
{
    static constexpr ObjectLayer NON_MOVING = 0;
    static constexpr ObjectLayer MOVING = 1;
    static constexpr ObjectLayer NUM_LAYERS = 2;
};

namespace BroadPhaseLayers
{
    static constexpr BroadPhaseLayer NON_MOVING(0);
    static constexpr BroadPhaseLayer MOVING(1);
    static constexpr uint NUM_LAYERS(2);
};
```

### Зачем нужны отдельные BroadPhase Layers?

Каждый BroadPhase Layer создаёт отдельное дерево ограничивающих объёмов (bounding volume tree). Это оптимизация:

- **Статические объекты** в одном дереве (не обновляется часто)
- **Динамические объекты** в другом дереве (обновляется каждый кадр)

## Широкие и узкие фазы

### Broad Phase (Широкая фаза)

1. **Назначение**: Быстро определить, какие пары тел *могут* пересекаться.
2. **Реализация**: Деревья ограничивающих объёмов (AABB trees) для каждого BroadPhase Layer.
3. **Результат**: Список пар тел для передачи в Narrow Phase.

### Narrow Phase (Узкая фаза)

1. **Назначение**: Точная проверка столкновений между конкретными формами.
2. **Реализация**: Алгоритмы GJK/EPA для выпуклых форм, специальные алгоритмы для примитивов.
3. **Результат**: Контактные многообразия (contact manifolds) с точками контакта, нормалями, глубиной проникновения.

### Конвейер коллизий

```
Broad Phase → Список пар тел → Narrow Phase → Контакты → Решатель ограничений → Обновление позиций
```

## Типы движения тел

### Static (Статические)

- Не двигаются (бесконечная масса)
- Не требуют обновления в дереве broad phase
- Пример: пол, стены, ландшафт

### Kinematic (Кинематические)

- Двигаются по заданному пути (скорость/позиция задаются вручную)
- Толкают динамические объекты при столкновении
- Пример: движущиеся платформы, лифты

### Dynamic (Динамические)

- Подчиняются законам физики (силы, импульсы)
- Могут спать (деактивироваться) для экономии ресурсов
- Пример: сфера, куб, персонаж

```cpp
enum class EMotionType
{
    Static,      // Неподвижное тело
    Kinematic,   // Движение задаётся вручную
    Dynamic      // Подчиняется физике
};
```

## Формы (Shapes)

### Примитивные формы

- **BoxShape**: Параллелепипед
- **SphereShape**: Сфера
- **CapsuleShape**: Капсула (цилиндр с полусферами на концах)
- **TaperedCapsuleShape**: Сужающаяся капсула
- **CylinderShape**: Цилиндр
- **TaperedCylinderShape**: Сужающийся цилиндр

### Сложные формы

- **ConvexHullShape**: Выпуклая оболочка набора точек
- **MeshShape**: Треугольная сетка (статическая)
- **HeightFieldShape**: Высотное поле (heightfield) для ландшафта
- **CompoundShape**: Композитная форма из нескольких под-форм

### Свойства форм

- **Center of Mass**: Центр масс (автоматически вычисляется)
- **Inertia**: Момент инерции (автоматически вычисляется для примитивов)
- **Convex Radius**: Радиус выпуклости (для закругления углов)
- **Density**: Плотность (для расчёта массы)

```cpp
// Создание бокса
BoxShapeSettings box_settings(Vec3(1.0f, 2.0f, 1.0f)); // Размеры
box_settings.SetDensity(1000.0f); // Плотность в кг/м³

// Создание выпуклой оболочки
ConvexHullShapeSettings hull_settings(points, num_points);
hull_settings.SetMaxConvexRadius(0.05f); // Закругление углов
```

## Ограничения (Constraints)

### Типы ограничений

1. **Fixed**: Жёсткое соединение двух тел
2. **Point**: Шарнирное соединение (сфера + сфера)
3. **Hinge**: Петлевое соединение (вращение вокруг одной оси)
4. **Slider**: Призматическое соединение (движение вдоль одной оси)
5. **Cone**: Конусное ограничение (например, для шеи)
6. **SwingTwist**: Ограничение качания-скручивания (для плеч)
7. **Distance**: Ограничение расстояния (с пружиной)
8. **Gear**: Зубчатая передача
9. **RackAndPinion**: Реечно-шестерёнчатая передача
10. **Pulley**: Блочная система

### Мотора (Motors)

Ограничения могут иметь моторы для управления движением:

- **Position Motor**: Достижение целевой позиции/угла
- **Velocity Motor**: Достижение целевой скорости
- **Force/Torque Motor**: Приложение силы/крутящего момента

```cpp
// Создание hinge constraint
HingeConstraintSettings hinge_settings;
hinge_settings.mPoint1 = hinge_settings.mPoint2 = Vec3(0, 1, 0); // Точка крепления
hinge_settings.mHingeAxis1 = hinge_settings.mHingeAxis2 = Vec3(0, 0, 1); // Ось вращения
hinge_settings.mNormalAxis1 = hinge_settings.mNormalAxis2 = Vec3(1, 0, 0); // Нормальная ось

Constraint *constraint = hinge_settings.Create(body1, body2);
physics_system.AddConstraint(constraint);
```

## Детерминированная симуляция

### Что такое детерминированная симуляция?

При одинаковых входных данных (начальные состояния, входные воздействия) симуляция даёт **бит-в-бит одинаковый**
результат на любом оборудовании и в любом окружении.

### Зачем это нужно?

1. **Сетевая синхронизация**: Передавать только входные данные, а не полное состояние
2. **Воспроизводимость**: Багрепорты, демозаписи, тесты
3. **Многопоточность**: Без race conditions

### Ограничения детерминированности в Jolt

- Требует `JPH_CROSS_PLATFORM_DETERMINISTIC` при компиляции
- Зависит от точности математики (float/double)
- Порядок добавления/удаления тел может влиять
- Многопоточность с фиксированным seed для RNG

### Использование в ProjectV

```cpp
// В CMake: добавьте -DJPH_CROSS_PLATFORM_DETERMINISTIC=ON
// В коде: проверяйте флаг
#ifdef JPH_CROSS_PLATFORM_DETERMINISTIC
    // Детерминированный режим
#endif
```

## Многопоточность и JobSystem

### Архитектура JobSystem

Jolt использует систему задач (jobs) для параллелизации:

- **PhysicsUpdateJob**: Основное обновление физики
- **BroadPhaseJob**: Обновление широкой фазы
- **NarrowPhaseJob**: Обновление узкой фазы
- **SolveConstraintsJob**: Решение ограничений
- **BodyUpdateJob**: Обновление позиций тел

### JobSystemThreadPool

Готовый пул потоков для использования:

```cpp
// Создание пула потоков
JobSystemThreadPool job_system(cMaxPhysicsJobs,        // Максимальное количество задач
                               cMaxPhysicsBarriers,    // Максимальное количество барьеров
                               hardware_concurrency - 1); // Количество рабочих потоков

// Использование в Update
physics_system.Update(delta_time, collision_steps, &temp_allocator, &job_system);
```

### Интеграция с собственным JobSystem

Можно реализовать `JobSystem` интерфейс для интеграции с собственным планировщиком задач (например, из игрового движка).

## Контакты и слушатели

### ContactListener

Получает уведомления о контактах между телами:

```cpp
class MyContactListener : public ContactListener
{
public:
    // Вызывается перед созданием контакта (можно отклонить)
    virtual ValidateResult OnContactValidate(const Body &inBody1, const Body &inBody2,
                                             RVec3Arg inBaseOffset,
                                             const CollideShapeResult &inCollisionResult) override;
    
    // Вызывается при добавлении контакта
    virtual void OnContactAdded(const Body &inBody1, const Body &inBody2,
                                const ContactManifold &inManifold,
                                ContactSettings &ioSettings) override;
    
    // Вызывается при сохранении контакта (на следующем кадре)
    virtual void OnContactPersisted(const Body &inBody1, const Body &inBody2,
                                    const ContactManifold &inManifold,
                                    ContactSettings &ioSettings) override;
    
    // Вызывается при удалении контакта
    virtual void OnContactRemoved(const SubShapeIDPair &inSubShapePair) override;
};
```

### BodyActivationListener

Уведомляет об активации/деактивации тел (пробуждение/сон):

```cpp
class MyBodyActivationListener : public BodyActivationListener
{
public:
    virtual void OnBodyActivated(const BodyID &inBodyID, uint64 inBodyUserData) override;
    virtual void OnBodyDeactivated(const BodyID &inBodyID, uint64 inBodyUserData) override;
};
```

### Использование

```cpp
MyContactListener contact_listener;
MyBodyActivationListener activation_listener;

physics_system.SetContactListener(&contact_listener);
physics_system.SetBodyActivationListener(&activation_listener);
```

## Оптимизации для воксельного движка

> ⚠️ **Контент перемещён**
>
> Информация об оптимизациях для воксельного движка была перемещена в общий файл интеграции для лучшей организации.

### Где найти информацию?

Все материалы по оптимизациям для воксельного движка теперь доступны
в [projectv-integration.md](projectv-integration.md):

- [HeightFieldShape для воксельного ландшафта](projectv-integration.md#воксельная-физика-и-heightfieldshape)
- [Voxel-based коллайдеры](projectv-integration.md#оптимизации-для-воксельного-движка)
- [Cellular Automata и жидкости](projectv-integration.md#оптимизации-для-воксельного-движка)
- [Чанкирование физики](projectv-integration.md#оптимизации-для-воксельного-движка)

### Краткое описание

JoltPhysics предоставляет специализированные возможности для эффективной работы с воксельными мирами, включая
оптимизированные формы, системы слоёв и интеграцию с ECS. Для подробностей обратитесь к полной документации интеграции.

## Производительность и настройка

### Критические параметры PhysicsSystem.Init()

```cpp
physics_system.Init(
    max_bodies = 1024,          // Максимум тел (увеличить для больших сцен)
    num_body_mutexes = 0,       // Мьютексы для тел (0 = auto)
    max_body_pairs = 1024,      // Максимум пар в broad phase
    max_contact_constraints = 1024, // Максимум контактов
    ... // фильтры
);
```

### Оптимизации

1. **Используйте слои**: Разделяйте статические и динамические объекты
2. **Batch добавление тел**: Добавляйте тела группами, а не по одному
3. **Оптимизируйте Broad Phase**: Вызывайте `OptimizeBroadPhase()` после загрузки уровня
4. **Настройте Sleep**: Используйте `SetSleepSettings()` для контроля активации
5. **Используйте TempAllocator**: Предварительное выделение уменьшает аллокации во время симуляции

### Профилирование

Включите `JPH_PROFILE_ENABLED` для сбора статистики:

- Время, проведённое в каждой фазе
- Количество активных тел, контактов, ограничений
- Использование памяти

---

**См. также:** [Быстрый старт](quickstart.md) — практический пример, [Интеграция](integration.md) — настройка для
ProjectV.

← [На главную документации](../README.md)
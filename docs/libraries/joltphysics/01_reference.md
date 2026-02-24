## Обзор JoltPhysics

<!-- anchor: 00_overview -->


**JoltPhysics** — современный физический движок с поддержкой детерминированной симуляции, оптимизированный для
многопоточности и разработанный для высокой производительности в игровых проектах.

Версия: **5.5.1+**
Исходники: [jrouwe/JoltPhysics](https://github.com/jrouwe/JoltPhysics)

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

## Структура библиотеки

```
JoltPhysics/
├── Jolt.h                   # Главный заголовок
├── Physics/
│   ├── PhysicsSystem.h      # Основная система
│   ├── Body/                # Тела и их свойства
│   ├── Collision/           # Коллизии и формы
│   └── Constraints/         # Ограничения
├── Core/
│   ├── JobSystem.h          # Многопоточность
│   └── TempAllocator.h      # Временные аллокации
└── Geometry/                # Математика и геометрия
```

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

## Основные понятия JoltPhysics

<!-- anchor: 02_concepts -->

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
> в C++ guide.

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

---

## Глоссарий JoltPhysics

<!-- anchor: 08_glossary -->

## Основные термины

### PhysicsSystem

**Физическая система** — центральный объект, управляющий всей симуляцией. Хранит тела, ограничения, обрабатывает
коллизии.

### Body

**Тело** — объект в физическом мире. Имеет форму, позицию, вращение, массу, скорость. Три типа движения:

- **Static** — неподвижное (пол, стены)
- **Kinematic** — движется по заданному пути (платформы)
- **Dynamic** — подчиняется физике (предметы)

### BodyInterface

**Интерфейс тел** — класс для работы с телами (создание, удаление, получение/установка свойств).

### Shape

**Форма** — геометрия коллизий тела. Определяет, как тело сталкивается с другими.

---

## Формы (Shapes)

### BoxShape

**Бокс** — параллелепипед. Определяется половиной размеров по X, Y, Z.

### SphereShape

**Сфера** — шар. Определяется радиусом.

### CapsuleShape

**Капсула** — цилиндр с полусферами на концах. Часто используется для персонажей.

### CylinderShape

**Цилиндр** — цилиндрическая форма.

### ConvexHullShape

**Выпуклая оболочка** — форма, построенная из набора точек. Все точки "натянуты" на поверхность.

### HeightFieldShape

**Высотное поле** — сетка высот для ландшафта. Эффективно для больших территорий.

### MeshShape

**Меш** — треугольная сетка. Только для статических тел.

### CompoundShape

**Составная форма** — комбинация нескольких подформ.

---

## Слои (Layers)

### Object Layer

**Объектный слой** — логическая группа тел (0-31 или 0-255). Используется для фильтрации коллизий.

### BroadPhase Layer

**Слой широкой фазы** — физическая группа для оптимизации. Каждая группа имеет своё AABB-дерево.

### ObjectLayerPairFilter

**Фильтр пар слоёв** — определяет, могут ли два объектных слоя сталкиваться.

### BroadPhaseLayerInterface

**Интерфейс BP-слоёв** — маппит объектные слои на слои широкой фазы.

---

## Фазы коллизий

### Broad Phase

**Широкая фаза** — быстрое определение потенциально сталкивающихся пар тел через AABB-деревья.

### Narrow Phase

**Узкая фаза** — точная проверка столкновений между парами тел из широкой фазы.

### GJK

**Gilbert-Johnson-Keerthi** — алгоритм определения расстояния между выпуклыми формами.

### EPA

**Expanding Polytope Algorithm** — алгоритм нахождения точки контакта и нормали после GJK.

---

## Ограничения (Constraints)

### Constraint

**Ограничение** — связь между двумя телами, ограничивающая их относительное движение.

### FixedConstraint

**Фиксированное ограничение** — жёсткое соединение двух тел.

### PointConstraint

**Точечное ограничение** — тела вращаются вокруг общей точки.

### HingeConstraint

**Шарнирное ограничение** — вращение вокруг одной оси (как дверная петля).

### SliderConstraint

**Ползунковое ограничение** — движение вдоль одной оси (как поршень).

### SwingTwistConstraint

**Swing-Twist ограничение** — сложное ограничение для суставов (плечо, шея).

### Motor

**Мотор** — механизм в ограничении для управления движением (позиция, скорость, сила).

---

## Симуляция

### Delta Time

**Дельта-тайм** — время между кадрами симуляции.

### Collision Steps

**Шаги коллизии** — количество проверок коллизий за кадр. Больше = точнее, но медленнее.

### Fixed Timestep

**Фиксированный шаг** — постоянное время симуляции для стабильности.

### Interpolation

**Интерполяция** — сглаживание между кадрами физики для рендеринга.

---

## Состояния тел

### Active

**Активное** — тело участвует в симуляции, движется.

### Sleeping

**Спящее** — тело деактивировано для экономии ресурсов. Просыпается при столкновении.

### Deactivated

**Деактивированное** — тело временно не участвует в симуляции.

---

## Память

### TempAllocator

**Временный аллокатор** — выделяет память на кадр симуляции. Освобождается автоматически.

### JobSystem

**Система задач** — управляет параллельным выполнением работы в нескольких потоках.

### ShapeRefC

**Ссылка на форму** — умный указатель с подсчётом ссылок. Форма удаляется когда счётчик = 0.

---

## Коллизии

### AABB

**Axis-Aligned Bounding Box** — выровненный по осям ограничивающий параллелепипед.

### Contact Manifold

**Контактное многообразие** — набор точек контакта между двумя телами.

### Contact Normal

**Контактная нормаль** — направление, в котором нужно раздвинуть тела.

### Penetration Depth

**Глубина проникновения** — насколько одно тело проникло в другое.

---

## Физические свойства

### Mass

**Масса** — количество вещества тела (кг). Определяет инерцию.

### Inertia Tensor

**Тензор инерции** — матрица, описывающая сопротивление вращению по осям.

### Friction

**Трение** — сопротивление скольжению (0 = лёд, 1 = резина).

### Restitution

**Упругость** — коэффициент отскока (0 = пластилин, 1 = супер-мяч).

### Damping

**Затухание** — потеря энергии со временем (линейное и угловое).

### Center of Mass

**Центр масс** — точка равновесия тела.

---

## Слушатели (Listeners)

### ContactListener

**Слушатель контактов** — получает события о столкновениях тел.

### BodyActivationListener

**Слушатель активации** — получает события о пробуждении/усыплении тел.

---

## Типы данных

### Vec3

**Vec3** — 3D вектор с одинарной точностью (float).

### RVec3

**RVec3** — 3D вектор с двойной точностью (double). Используется для позиций в больших мирах.

### Quat

**Quat** — кватернион для представления вращения.

### Mat44

**Mat44** — матрица 4x4 для трансформаций.

### BodyID

**BodyID** — уникальный идентификатор тела в системе.

---

## Персонажи

### CharacterVirtual

**Виртуальный персонаж** — специальный класс для управления персонажем без физического тела.

### Step Height

**Высота ступеньки** — максимальная высота, которую персонаж может преодолеть.

### Slope Angle

**Угол склона** — максимальный наклон поверхности, на которой персонаж может стоять.

---

## Детерминизм

### Deterministic

**Детерминированный** — одинаковые входные данные дают одинаковый результат.

### Cross-platform Deterministic

**Кроссплатформенный детерминизм** — одинаковый результат на разных платформах.

---

## Отладка

### Debug Renderer

**Отладочный рендерер** — интерфейс для визуализации физики (коллайдеры, контакты, ограничения).

### Validation

**Валидация** — проверка целостности физической системы.

---

## Краткий справочник

| Термин       | Значение                        |
|--------------|---------------------------------|
| Body         | Физическое тело                 |
| Shape        | Геометрия коллизий              |
| Constraint   | Ограничение между телами        |
| Layer        | Слой для фильтрации коллизий    |
| Broad Phase  | Широкая фаза (быстрая проверка) |
| Narrow Phase | Узкая фаза (точная проверка)    |
| Motor        | Мотор ограничения               |
| Sleeping     | Спящее состояние тела           |
| AABB         | Ограничивающий объём            |
| Contact      | Контакт между телами            |

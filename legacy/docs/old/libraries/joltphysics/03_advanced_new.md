## Формы (Shapes) в JoltPhysics

<!-- anchor: 05_shapes -->

🟢 **Уровень 1: Начинающий**

## Обзор

Формы (Shapes) определяют геометрию коллизий тел. JoltPhysics поддерживает:

- **Примитивы** — Box, Sphere, Capsule, Cylinder
- **Составные** — ConvexHull, Compound
- **Поверхности** — HeightField, Mesh

---

## Примитивные формы

### BoxShape

Параллелепипед. Определяется половиной размеров по каждой оси.

```cpp
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

// Куб 2x2x2 (половина размера = 1)
JPH::BoxShapeSettings box_settings(JPH::Vec3(1.0f, 1.0f, 1.0f));
JPH::ShapeRefC box_shape = box_settings.Create().Get();
```

#### Параметры

| Параметр        | Описание                             |
|-----------------|--------------------------------------|
| `mHalfExtent`   | Половина размеров по X, Y, Z         |
| `mConvexRadius` | Радиус скругления углов (0 = острые) |

```cpp
// Прямоугольник 4x2x1 с закруглёнными углами
JPH::BoxShapeSettings box_settings(JPH::Vec3(2.0f, 1.0f, 0.5f), 0.1f);
```

### SphereShape

Сфера. Определяется радиусом.

```cpp
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

JPH::SphereShapeSettings sphere_settings(1.0f);  // Радиус = 1
JPH::ShapeRefC sphere_shape = sphere_settings.Create().Get();
```

### CapsuleShape

Капсула: цилиндр с полусферами на концах.

```cpp
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>

// Капсула: высота цилиндра = 2, радиус = 0.5
// Общая высота = 2 * half_height + 2 * radius = 2 + 1 = 3
JPH::CapsuleShapeSettings capsule_settings(1.0f, 0.5f);
JPH::ShapeRefC capsule_shape = capsule_settings.Create().Get();
```

#### Параметры

| Параметр                | Описание                             |
|-------------------------|--------------------------------------|
| `mHalfHeightOfCylinder` | Половина высоты цилиндрической части |
| `mRadius`               | Радиус капсулы                       |

### CylinderShape

Цилиндр.

```cpp
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>

// Цилиндр: высота = 2, радиус = 0.5
JPH::CylinderShapeSettings cylinder_settings(1.0f, 0.5f);
JPH::ShapeRefC cylinder_shape = cylinder_settings.Create().Get();
```

### TaperedCapsuleShape

Сужающаяся капсула (разные радиусы сверху и снизу).

```cpp
#include <Jolt/Physics/Collision/Shape/TaperedCapsuleShape.h>

// Сужающаяся капсула: half_height = 1, top_radius = 0.3, bottom_radius = 0.5
JPH::TaperedCapsuleShapeSettings tapered_settings(1.0f, 0.3f, 0.5f);
```

---

## Выпуклые формы

### ConvexHullShape

Выпуклая оболочка набора точек.

```cpp
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>

// Набор точек
std::vector<JPH::Vec3> points = {
    JPH::Vec3(0, 0, 0),
    JPH::Vec3(1, 0, 0),
    JPH::Vec3(0, 1, 0),
    JPH::Vec3(0, 0, 1),
    JPH::Vec3(1, 1, 1)
};

JPH::ConvexHullShapeSettings hull_settings(points.data(), points.size(), 0.05f);
JPH::ShapeRefC hull_shape = hull_settings.Create().Get();
```

#### Параметры

| Параметр           | Описание                       |
|--------------------|--------------------------------|
| `mPoints`          | Массив точек                   |
| `mConvexRadius`    | Радиус скругления              |
| `mMaxConvexRadius` | Максимальный радиус скругления |

### Создание из массива

```cpp
// Добавление точек по одной
JPH::ConvexHullShapeSettings hull_settings;
hull_settings.AddPoint(JPH::Vec3(0, 0, 0));
hull_settings.AddPoint(JPH::Vec3(1, 0, 0));
hull_settings.AddPoint(JPH::Vec3(0, 1, 0));
hull_settings.mConvexRadius = 0.02f;

JPH::ShapeRefC shape = hull_settings.Create().Get();
```

---

## Составные формы

### StaticCompoundShape

Композитная форма из нескольких подформ. Неизменяемая после создания.

```cpp
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

JPH::StaticCompoundShapeSettings compound_settings;

// Добавление подформ с позицией и вращением
compound_settings.AddShape(
    JPH::Vec3(0, 0, 0),           // Позиция
    JPH::Quat::sIdentity(),       // Вращение
    new JPH::BoxShape(JPH::Vec3(1, 1, 1))  // Форма
);

compound_settings.AddShape(
    JPH::Vec3(0, 2, 0),
    JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), JPH::JPH_PI * 0.25f),
    new JPH::SphereShape(0.5f)
);

JPH::ShapeRefC compound_shape = compound_settings.Create().Get();
```

### MutableCompoundShape

Композитная форма, которую можно изменять после создания.

```cpp
#include <Jolt/Physics/Collision/Shape/MutableCompoundShape.h>

JPH::MutableCompoundShapeSettings mutable_settings;

// Добавление подформ
mutable_settings.AddShape(JPH::Vec3(0, 0, 0), JPH::Quat::sIdentity(), box_shape);

JPH::ShapeRefC shape = mutable_settings.Create().Get();
```

---

## HeightFieldShape

Высотное поле для ландшафта.

### Создание

```cpp
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>

// Данные высот (квадратная сетка)
const uint32_t sample_count = 32;
std::vector<float> heights(sample_count * sample_count);

for (uint32_t z = 0; z < sample_count; ++z) {
    for (uint32_t x = 0; x < sample_count; ++x) {
        heights[z * sample_count + x] = CalculateHeight(x, z);
    }
}

JPH::HeightFieldShapeSettings heightfield_settings(
    heights.data(),                                    // Данные высот
    JPH::Vec3(0, 0, 0),                               // Смещение
    JPH::Vec3(1.0f, 1.0f, 1.0f),                      // Масштаб (x, height, z)
    sample_count                                       // Размер сетки
);

JPH::ShapeRefC heightfield_shape = heightfield_settings.Create().Get();
```

### Параметры

| Параметр         | Описание                                   |
|------------------|--------------------------------------------|
| `mHeightSamples` | Массив высот (sample_count * sample_count) |
| `mSampleCount`   | Размер сетки по одной оси                  |
| `mOffset`        | Смещение всей высотной карты               |
| `mScale`         | Масштаб по X, Y (высота), Z                |
| `mBlockSize`     | Размер блока для оптимизации (2-8)         |
| `mBitsPerSample` | Бит на сэмпл для сжатия (1-8)              |

### Дыры в HeightField

```cpp
// FLT_MAX означает "нет коллизии"
const float NO_COLLISION = JPH::HeightFieldShapeConstants::cNoCollisionValue;

// Создание дыры
heights[z * sample_count + x] = NO_COLLISION;
```

### Оптимизация

```cpp
// Высокая детализация (больше памяти, точнее коллизии)
settings.mBlockSize = 2;
settings.mBitsPerSample = 8;

// Низкая детализация (меньше памяти, менее точно)
settings.mBlockSize = 8;
settings.mBitsPerSample = 4;
```

---

## MeshShape

Треугольная сетка. Только для статических тел.

### Создание

```cpp
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

JPH::MeshShapeSettings mesh_settings;

// Добавление треугольников
JPH::Triangle triangle;
triangle.mV[0] = JPH::Float3(0, 0, 0);
triangle.mV[1] = JPH::Float3(1, 0, 0);
triangle.mV[2] = JPH::Float3(0, 1, 0);
mesh_settings.AddTriangle(triangle);

JPH::ShapeRefC mesh_shape = mesh_settings.Create().Get();
```

### Из массива треугольников

```cpp
std::vector<JPH::Triangle> triangles;

// Добавление треугольников
triangles.push_back(JPH::Triangle(
    JPH::Float3(0, 0, 0),
    JPH::Float3(1, 0, 0),
    JPH::Float3(0, 1, 0)
));

JPH::MeshShapeSettings mesh_settings(triangles);
JPH::ShapeRefC mesh_shape = mesh_settings.Create().Get();
```

---

## RotatedTranslatedShape

Обёртка для смещения и вращения формы.

```cpp
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

// Капсула, смещённая вверх на 1 метр
JPH::RotatedTranslatedShapeSettings rotated_settings(
    JPH::Vec3(0, 1.0f, 0),              // Смещение
    JPH::Quat::sIdentity(),             // Вращение
    new JPH::CapsuleShape(0.5f, 0.25f)  // Внутренняя форма
);

JPH::ShapeRefC shape = rotated_settings.Create().Get();
```

---

## Сравнение форм

| Форма       | Использование     | Производительность | Память             |
|-------------|-------------------|--------------------|--------------------|
| Box         | Стены, ящики      | Высокая            | Низкая             |
| Sphere      | Шары, мячи        | Высокая            | Низкая             |
| Capsule     | Персонажи         | Высокая            | Низкая             |
| Cylinder    | Колёса, столбы    | Средняя            | Низкая             |
| ConvexHull  | Сложные объекты   | Средняя            | Средняя            |
| HeightField | Ландшафт          | Высокая            | Зависит от размера |
| Mesh        | Сложная статики   | Низкая             | Высокая            |
| Compound    | Составные объекты | Средняя            | Средняя            |

---

## Создание и использование

### Паттерн создания

```cpp
// 1. Создать настройки формы
JPH::BoxShapeSettings settings(JPH::Vec3(1, 1, 1));

// 2. Создать форму
JPH::ShapeSettings::ShapeResult result = settings.Create();

// 3. Проверить результат
if (!result.IsValid()) {
    std::cerr << "Error: " << result.GetError() << std::endl;
    return;
}

// 4. Получить ссылку на форму
JPH::ShapeRefC shape = result.Get();

// 5. Использовать в теле
JPH::BodyCreationSettings body_settings(
    shape,
    JPH::RVec3(0, 10, 0),
    JPH::Quat::sIdentity(),
    JPH::EMotionType::Dynamic,
    0
);
```

### Повторное использование форм

```cpp
// Формы можно использовать для нескольких тел
JPH::ShapeRefC shared_shape = JPH::BoxShapeSettings(JPH::Vec3(1, 1, 1)).Create().Get();

// Создаём несколько тел с одной формой
for (int i = 0; i < 100; ++i) {
    JPH::BodyCreationSettings body_settings(
        shared_shape,
        JPH::RVec3(i * 2.0f, 10, 0),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic,
        0
    );

    physics_system.GetBodyInterfaceNoLock().CreateAndAddBody(body_settings, JPH::EActivation::Activate);
}
```

---

## Свойства материалов

### PhysicsMaterial

```cpp
#include <Jolt/Physics/Collision/PhysicsMaterial.h>

class MyMaterial : public JPH::PhysicsMaterial
{
public:
    MyMaterial(float inFriction, float inRestitution)
        : mFriction(inFriction), mRestitution(inRestitution) {}

    float mFriction;
    float mRestitution;
};

// Создание материала
JPH::RefConst<JPH::PhysicsMaterial> material = new MyMaterial(0.5f, 0.3f);

// Назначение форме
mesh_settings.mMaterials.push_back(material);
```

### Трение и упругость

```cpp
// Трение (0-1): 0 = лёд, 1 = резина
body_settings.mFriction = 0.8f;

// Упругость (0-1): 0 = пластилин, 1 = супер-мяч
body_settings.mRestitution = 0.5f;

---

## Ограничения (Constraints) в JoltPhysics

<!-- anchor: 06_constraints -->

🟡 **Уровень 2: Средний**

## Обзор

Ограничения (Constraints) связывают два тела, ограничивая их относительное движение.

---

## Типы ограничений

| Тип               | Описание                 | Примеры            |
|-------------------|--------------------------|--------------------|
| **Fixed**         | Жёсткое соединение       | Сварка, склейка    |
| **Point**         | Шарнир в точке           | Канат, цепь        |
| **Hinge**         | Вращение вокруг оси      | Дверь, колёсo      |
| **Slider**        | Движение вдоль оси       | Поршень, лифт      |
| **Cone**          | Ограничение конусом      | Плечевой сустав    |
| **SwingTwist**    | Качание и скручивание    | Ragdoll            |
| **Distance**      | Фиксированное расстояние | Пружина, трос      |
| **Gear**          | Зубчатая передача        | Шестерни           |
| **RackAndPinion** | Рейка и шестерня         | Рулевое управление |

---

## FixedConstraint

Жёсткое соединение двух тел.

```cpp
#include <Jolt/Physics/Constraints/FixedConstraint.h>

JPH::FixedConstraintSettings settings;

// Точка крепления в мировых координатах
settings.mAutoDetectPoint = true;  // Автоопределение точки между телами

// Или указать явно
settings.mPoint1 = JPH::RVec3(0, 0, 0);
settings.mPoint2 = JPH::RVec3(0, 0, 0);

JPH::Constraint* constraint = settings.Create(body1, body2);
physics_system.AddConstraint(constraint);
```

---

## PointConstraint

Шарнирное соединение — тела могут вращаться вокруг общей точки.

```cpp
#include <Jolt/Physics/Constraints/PointConstraint.h>

JPH::PointConstraintSettings settings;

// Точки крепления (в локальных координатах тел)
settings.mPoint1 = JPH::Vec3(0, 0, 0);  // На первом теле
settings.mPoint2 = JPH::Vec3(0, 0, 0);  // На втором теле

JPH::Constraint* constraint = settings.Create(body1, body2);
physics_system.AddConstraint(constraint);
```

---

## HingeConstraint

Вращение вокруг одной оси (как дверная петля).

```cpp
#include <Jolt/Physics/Constraints/HingeConstraint.h>

JPH::HingeConstraintSettings settings;

// Точка крепления
settings.mPoint1 = JPH::RVec3(0, 1, 0);
settings.mPoint2 = JPH::RVec3(0, 1, 0);

// Ось вращения
settings.mHingeAxis1 = JPH::Vec3::sAxisY();  // Ось на первом теле
settings.mHingeAxis2 = JPH::Vec3::sAxisY();  // Ось на втором теле

// Нормальная ось (определяет начальный угол)
settings.mNormalAxis1 = JPH::Vec3::sAxisX();
settings.mNormalAxis2 = JPH::Vec3::sAxisX();

// Лимиты угла (опционально)
settings.mLimitsMin = -JPH::JPH_PI * 0.5f;  // -90 градусов
settings.mLimitsMax = JPH::JPH_PI * 0.5f;   // +90 градусов

JPH::HingeConstraint* constraint = static_cast<JPH::HingeConstraint*>(settings.Create(body1, body2));
physics_system.AddConstraint(constraint);
```

### Мотор HingeConstraint

```cpp
// Включить мотор
constraint->SetMotorState(JPH::EMotorState::Position);

// Целевой угол
constraint->SetTargetAngle(JPH::JPH_PI * 0.25f);  // 45 градусов

// Параметры мотора
constraint->GetMotorSettings().mFrequency = 2.0f;   // Частота (Hz)
constraint->GetMotorSettings().mDamping = 1.0f;     // Затухание

// Скоростной мотор
constraint->SetMotorState(JPH::EMotorState::Velocity);
constraint->SetTargetAngularVelocity(JPH::JPH_PI);  // 180 град/с
```

---

## SliderConstraint

Движение вдоль одной оси (как поршень).

```cpp
#include <Jolt/Physics/Constraints/SliderConstraint.h>

JPH::SliderConstraintSettings settings;

// Точка крепления
settings.mPoint1 = JPH::RVec3(0, 0, 0);
settings.mPoint2 = JPH::RVec3(0, 0, 0);

// Ось скольжения
settings.mSliderAxis1 = JPH::Vec3::sAxisY();
settings.mSliderAxis2 = JPH::Vec3::sAxisY();

// Нормальная ось
settings.mNormalAxis1 = JPH::Vec3::sAxisX();
settings.mNormalAxis2 = JPH::Vec3::sAxisX();

// Лимиты
settings.mLimitsMin = -1.0f;  // Минимальное смещение
settings.mLimitsMax = 1.0f;   // Максимальное смещение

JPH::SliderConstraint* constraint = static_cast<JPH::SliderConstraint*>(settings.Create(body1, body2));
physics_system.AddConstraint(constraint);
```

### Мотор SliderConstraint

```cpp
// Позиционный мотор
constraint->SetMotorState(JPH::EMotorState::Position);
constraint->SetTargetPosition(0.5f);

// Скоростной мотор
constraint->SetMotorState(JPH::EMotorState::Velocity);
constraint->SetTargetVelocity(2.0f);  // 2 м/с

// Силовой мотор
constraint->SetMotorState(JPH::EMotorState::Force);
constraint->SetMaxForce(100.0f);
```

---

## DistanceConstraint

Поддержание фиксированного расстояния между точками.

```cpp
#include <Jolt/Physics/Constraints/DistanceConstraint.h>

JPH::DistanceConstraintSettings settings;

// Точки крепления
settings.mPoint1 = JPH::RVec3(0, 0, 0);
settings.mPoint2 = JPH::RVec3(0, 2, 0);

// Расстояние (авто = текущее расстояние)
settings.mDistance = -1.0f;  // -1 = автоматически

// Или указать явно
settings.mDistance = 2.0f;

// Пружина
settings.mLimitsSpringSettings.mFrequency = 5.0f;
settings.mLimitsSpringSettings.mDamping = 0.5f;

JPH::Constraint* constraint = settings.Create(body1, body2);
physics_system.AddConstraint(constraint);
```

---

## ConeConstraint

Ограничение в форме конуса — для плечевых суставов.

```cpp
#include <Jolt/Physics/Constraints/ConeConstraint.h>

JPH::ConeConstraintSettings settings;

// Точка крепления
settings.mPoint1 = JPH::RVec3(0, 0, 0);
settings.mPoint2 = JPH::RVec3(0, 0, 0);

// Ось конуса (направление ограничения)
settings.mConeAxis1 = JPH::Vec3::sAxisY();
settings.mConeAxis2 = JPH::Vec3::sAxisY();

// Угол конуса (половина угла раскрытия)
settings.mHalfConeAngle = JPH::JPH_PI * 0.25f;  // 45 градусов

JPH::Constraint* constraint = settings.Create(body1, body2);
physics_system.AddConstraint(constraint);
```

---

## SwingTwistConstraint

Сложное ограничение для ragdoll — разделяет swing и twist вращения.

```cpp
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>

JPH::SwingTwistConstraintSettings settings;

// Точка крепления
settings.mPosition1 = JPH::RVec3(0, 0, 0);
settings.mPosition2 = JPH::RVec3(0, 0, 0);

// Ось твиста (вращение вокруг оси)
settings.mTwistAxis1 = JPH::Vec3::sAxisY();
settings.mTwistAxis2 = JPH::Vec3::sAxisY();

// Плоскость swing (качание)
settings.mPlaneAxis1 = JPH::Vec3::sAxisX();
settings.mPlaneAxis2 = JPH::Vec3::sAxisX();

// Лимиты твиста (вращение вокруг оси)
settings.mTwistMinAngle = -JPH::JPH_PI * 0.5f;
settings.mTwistMaxAngle = JPH::JPH_PI * 0.5f;

// Лимиты swing (качание)
settings.mSwingYMaxAngle = JPH::JPH_PI * 0.25f;
settings.mSwingZMaxAngle = JPH::JPH_PI * 0.25f;

JPH::SwingTwistConstraint* constraint = static_cast<JPH::SwingTwistConstraint*>(settings.Create(body1, body2));
physics_system.AddConstraint(constraint);
```

---

## GearConstraint

Зубчатая передача между двумя hinge ограничениями.

```cpp
#include <Jolt/Physics/Constraints/GearConstraint.h>

JPH::GearConstraintSettings settings;

// Два hinge ограничения
settings.mHinge1 = hinge_constraint1;
settings.mHinge2 = hinge_constraint2;

// Передаточное число
settings.mRatio = 2.0f;  // Вторая шестерня вращается в 2 раза быстрее

JPH::Constraint* constraint = settings.Create(body1, body2);
physics_system.AddConstraint(constraint);
```

---

## RackAndPinionConstraint

Рейка и шестерня.

```cpp
#include <Jolt/Physics/Constraints/RackAndPinionConstraint.h>

JPH::RackAndPinionConstraintSettings settings;

// Hinge (шестерня) и Slider (рейка)
settings.mHinge = hinge_constraint;
settings.mSlider = slider_constraint;

// Передаточное число
settings.mRatio = 1.0f;

JPH::Constraint* constraint = settings.Create(body1, body2);
physics_system.AddConstraint(constraint);
```

---

## PulleyConstraint

Блочная система.

```cpp
#include <Jolt/Physics/Constraints/PulleyConstraint.h>

JPH::PulleyConstraintSettings settings;

// Точки крепления на телах
settings.mBodyPoint1 = JPH::Vec3(0, 0, 0);
settings.mBodyPoint2 = JPH::Vec3(0, 0, 0);

// Точки крепления блоков (в мировых координатах)
settings.mFixedPoint1 = JPH::RVec3(0, 5, 0);
settings.mFixedPoint2 = JPH::RVec3(2, 5, 0);

// Длина троса
settings.mRatio = 1.0f;  // Отношение длин

JPH::Constraint* constraint = settings.Create(body1, body2);
physics_system.AddConstraint(constraint);
```

---

## Управление ограничениями

### Добавление и удаление

```cpp
// Добавление
physics_system.AddConstraint(constraint);

// Удаление
physics_system.RemoveConstraint(constraint);

// Проверка активности
bool active = constraint->IsActive();
```

### Включение/отключение

```cpp
// Отключить ограничение
constraint->SetEnabled(false);

// Включить ограничение
constraint->SetEnabled(true);

// Проверить состояние
bool enabled = constraint->GetEnabled();
```

### Получение информации

```cpp
// Связанные тела
JPH::Body* body1 = constraint->GetBody1();
JPH::Body* body2 = constraint->GetBody2();

// Тип ограничения
JPH::EConstraintType type = constraint->GetType();
JPH::EConstraintSubType sub_type = constraint->GetSubType();

// UserData
constraint->SetUserData(42);
uint64 data = constraint->GetUserData();
```

---

## Моторы

### EMotorState

```cpp
enum class EMotorState
{
    Off,        // Мотор выключен
    Position,   // Достижение целевой позиции
    Velocity,   // Достижение целевой скорости
    Force       // Приложение постоянной силы
};
```

### MotorSettings

```cpp
JPH::MotorSettings motor_settings;
motor_settings.mFrequency = 2.0f;     // Частота пружины (Hz)
motor_settings.mDamping = 1.0f;       // Затухание
motor_settings.mMinForceLimit = -1000.0f;
motor_settings.mMaxForceLimit = 1000.0f;
motor_settings.mMinTorqueLimit = -100.0f;
motor_settings.mMaxTorqueLimit = 100.0f;

constraint->SetMotorSettings(motor_settings);
```

### Примеры использования

```cpp
// Позиционный мотор — переместить в позицию
hinge->SetMotorState(JPH::EMotorState::Position);
hinge->SetTargetAngle(JPH::JPH_PI * 0.5f);

// Скоростной мотор — вращать с постоянной скоростью
hinge->SetMotorState(JPH::EMotorState::Velocity);
hinge->SetTargetAngularVelocity(JPH::JPH_PI);  // 180 град/с

// Силовой мотор — приложить силу
hinge->SetMotorState(JPH::EMotorState::Force);
hinge->SetMaxTorque(50.0f);
```

---

## Ограничение сил

### Soft-locked ограничения

```cpp
// Пружина для мягкого ограничения
JPH::SpringSettings spring;
spring.mFrequency = 5.0f;   // Частота колебаний
spring.mDamping = 0.5f;     // Затухание

slider_constraint->SetLimitsSpringSettings(spring);
```

---

## Ragdoll пример

```cpp
// Создание ragdoll из SwingTwistConstraint
void CreateRagdoll(JPH::PhysicsSystem& physics_system)
{
    // Тело: торс, голова, руки, ноги
    JPH::Body* torso = CreateBody(physics_system, ...);
    JPH::Body* head = CreateBody(physics_system, ...);
    JPH::Body* upper_arm_L = CreateBody(physics_system, ...);
    JPH::Body* lower_arm_L = CreateBody(physics_system, ...);

    // Шея (торс - голова)
    JPH::SwingTwistConstraintSettings neck_settings;
    neck_settings.mPosition1 = JPH::RVec3(0, 0.5f, 0);  // Верх торса
    neck_settings.mPosition2 = JPH::RVec3(0, -0.1f, 0); // Низ головы
    neck_settings.mTwistAxis1 = JPH::Vec3::sAxisY();
    neck_settings.mTwistAxis2 = JPH::Vec3::sAxisY();
    neck_settings.mTwistMinAngle = -0.3f;
    neck_settings.mTwistMaxAngle = 0.3f;
    neck_settings.mSwingYMaxAngle = 0.5f;
    neck_settings.mSwingZMaxAngle = 0.5f;

    JPH::Constraint* neck = neck_settings.Create(torso, head);
    physics_system.AddConstraint(neck);

    // Плечо (торс - upper_arm_L)
    JPH::SwingTwistConstraintSettings shoulder_settings;
    shoulder_settings.mPosition1 = JPH::RVec3(0.3f, 0.4f, 0);
    shoulder_settings.mPosition2 = JPH::RVec3(0, 0.3f, 0);
    shoulder_settings.mTwistAxis1 = JPH::Vec3::sAxisY();
    shoulder_settings.mTwistAxis2 = JPH::Vec3::sAxisY();
    shoulder_settings.mTwistMinAngle = -1.0f;
    shoulder_settings.mTwistMaxAngle = 1.0f;
    shoulder_settings.mSwingYMaxAngle = JPH::JPH_PI * 0.5f;
    shoulder_settings.mSwingZMaxAngle = JPH::JPH_PI * 0.5f;

    JPH::Constraint* shoulder = shoulder_settings.Create(torso, upper_arm_L);
    physics_system.AddConstraint(shoulder);

    // Локоть (upper_arm_L - lower_arm_L)
    JPH::HingeConstraintSettings elbow_settings;
    elbow_settings.mPoint1 = JPH::RVec3(0, -0.3f, 0);
    elbow_settings.mPoint2 = JPH::RVec3(0, 0.3f, 0);
    elbow_settings.mHingeAxis1 = JPH::Vec3::sAxisZ();
    elbow_settings.mHingeAxis2 = JPH::Vec3::sAxisZ();
    elbow_settings.mLimitsMin = 0.0f;
    elbow_settings.mLimitsMax = JPH::JPH_PI * 0.75f;

    JPH::Constraint* elbow = elbow_settings.Create(upper_arm_L, lower_arm_L);
    physics_system.AddConstraint(elbow);
}

---

## Решение проблем JoltPhysics

<!-- anchor: 07_troubleshooting -->

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
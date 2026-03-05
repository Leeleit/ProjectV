# Ограничения (Constraints) в JoltPhysics

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

🟡 **Уровень 2: Средний**

# Справочник API JoltPhysics

Краткое описание функций, классов и структур JoltPhysics, необходимых для интеграции в ProjectV. Полная
документация: [JoltPhysics Documentation](https://github.com/jrouwe/JoltPhysics). Исходники:
`external/JoltPhysics/Jolt/`.

## На этой странице

- [Иерархия классов](#иерархия-классов)
- [Инициализация](#инициализация)
- [Physics System](#physics-system)
- [Body Interface](#body-interface)
- [Формы (Shapes)](#формы-shapes)
- [Слои и фильтры коллизий](#слои-и-фильтры-коллизий)
- [Ограничения (Constraints)](#ограничения-constraints)
- [Job System](#job-system)
- [Вспомогательные классы](#вспомогательные-классы)
- [Утилиты преобразования](#утилиты-преобразования)

---

## Иерархия классов

```
JPH::PhysicsSystem           // Основная физическая система
├── JPH::BodyInterface      // Интерфейс для работы с телами
├── JPH::BroadPhaseLayer    // Слои для широкой фазы
├── JPH::ObjectLayer        // Слои объектов для фильтрации
└── JPH::ConstraintManager  // Менеджер ограничений

JPH::Shape                  // Базовый класс всех форм
├── JPH::BoxShape          // Куб
├── JPH::SphereShape       // Сфера
├── JPH::CapsuleShape      // Капсула
├── JPH::CylinderShape     // Цилиндр
├── JPH::ConvexHullShape   // Выпуклая оболочка
├── JPH::MeshShape         // Полигональная сетка
├── JPH::HeightFieldShape  // Высотное поле (для террейна)
└── JPH::CompoundShape     // Составная форма

JPH::JobSystem             // Система задач для многопоточности
```

---

## Инициализация

### JPH::RegisterDefaultAllocator

```cpp
void JPH::RegisterDefaultAllocator();
```

Регистрирует аллокатор по умолчанию для JoltPhysics. **Должен быть вызван первым**, перед любыми другими вызовами Jolt.

---

### JPH::Factory::sInstance

```cpp
JPH::Factory* JPH::Factory::sInstance = new JPH::Factory();
```

Создание фабрики для создания форм и тел. Должно быть создано до регистрации типов.

---

### JPH::RegisterTypes

```cpp
void JPH::RegisterTypes();
```

Регистрирует все стандартные типы форм и ограничений. Вызывать после создания фабрики.

---

### JPH::TempAllocatorImpl

```cpp
class JPH::TempAllocatorImpl : public JPH::TempAllocator
```

Аллокатор временной памяти для физических расчётов. Используется в `PhysicsSystem::Update`.

**Параметры конструктора:**

- `size_t inSize` - размер выделяемой памяти в байтах (рекомендуется 10-100 МБ)

**Пример:**

```cpp
JPH::TempAllocatorImpl temp_allocator(10 * 1024 * 1024); // 10 МБ
```

---

## Physics System

### JPH::PhysicsSystem

```cpp
class JPH::PhysicsSystem
```

Основной класс физической системы. Управляет всеми телами, коллизиями и симуляцией.

#### Init

```cpp
void Init(
    uint inMaxBodies,                    // Максимальное количество тел
    uint inNumBodyMutexes,              // Количество мьютексов для тел
    uint inMaxBodyPairs,                // Максимальное количество пар тел
    uint inMaxContactConstraints,       // Максимальное количество контактов
    BroadPhaseLayerInterface& inBroadPhaseLayerInterface,      // Интерфейс слоёв широкой фазы
    ObjectVsBroadPhaseLayerFilter& inObjectVsBroadPhaseLayerFilter, // Фильтр слоёв
    ObjectVsObjectLayerFilter& inObjectVsObjectLayerFilter    // Фильтр объектов
);
```

Инициализация физической системы.

**Параметры:**

- `inMaxBodies` - обычно 65536 (16-битные ID)
- `inNumBodyMutexes` - количество мьютексов (обычно 0 для однопоточности)
- `inMaxBodyPairs` - зависит от сложности сцены (например, 65536)
- `inMaxContactConstraints` - зависит от сложности сцены (например, 10240)

#### Update

```cpp
void Update(
    float inDeltaTime,                  // Время с предыдущего обновления
    int inCollisionSteps,               // Количество шагов коллизий
    JPH::TempAllocator* inTempAllocator, // Аллокатор временной памяти
    JPH::JobSystem* inJobSystem         // Система задач
);
```

Обновление физической симуляции.

**Параметры:**

- `inDeltaTime` - дельта времени (обычно ограничивать до 0.033 для стабильности)
- `inCollisionSteps` - количество шагов коллизий (обычно 1-4)
- `inTempAllocator` - аллокатор временной памяти
- `inJobSystem` - система задач для многопоточности

#### GetBodyInterface

```cpp
JPH::BodyInterface& GetBodyInterface();
```

Возвращает интерфейс для работы с телами.

---

### JPH::PhysicsSettings

```cpp
struct JPH::PhysicsSettings
```

Настройки физической системы.

| Параметр                      | Тип     | Описание                               | Значение по умолчанию |
|-------------------------------|---------|----------------------------------------|-----------------------|
| `mBaumgarte`                  | `float` | Коэффициент Baumgarte для стабильности | 0.2f                  |
| `mSpeculativeContactDistance` | `float` | Дистанция спекулятивных контактов      | 0.02f                 |
| `mPenetrationSlop`            | `float` | Допуск проникновения                   | 0.01f                 |
| `mLinearCastThreshold`        | `float` | Порог линейного каста                  | 0.1f                  |
| `mManifoldToleranceSq`        | `float` | Толерантность многообразий             | 1.0e-6f               |
| `mMaxLinearVelocity`          | `float` | Максимальная линейная скорость         | 500.0f                |
| `mMaxAngularVelocity`         | `float` | Максимальная угловая скорость          | 50.0f * JPH_PI        |

---

## Body Interface

### JPH::BodyInterface

```cpp
class JPH::BodyInterface
```

Интерфейс для создания, модификации и удаления тел.

#### CreateBody

```cpp
JPH::BodyID CreateBody(const JPH::BodyCreationSettings& inSettings);
```

Создаёт тело с заданными настройками.

**Возвращает:** `BodyID` созданного тела или `BodyID::cInvalidBodyID` при ошибке.

#### AddBody

```cpp
void AddBody(JPH::BodyID inBodyID, JPH::EActivation inActivationMode);
```

Добавляет тело в физическую систему.

**Параметры:**

- `inBodyID` - ID тела
- `inActivationMode` - `EActivation::Activate` или `EActivation::DontActivate`

#### RemoveBody

```cpp
void RemoveBody(JPH::BodyID inBodyID);
```

Удаляет тело из физической системы (но не уничтожает его).

#### DestroyBody

```cpp
void DestroyBody(JPH::BodyID inBodyID);
```

Полностью уничтожает тело (удаляет из системы и освобождает память).

#### SetPositionAndRotation

```cpp
void SetPositionAndRotation(
    JPH::BodyID inBodyID,
    const JPH::Vec3& inPosition,
    const JPH::Quat& inRotation,
    JPH::EActivation inActivationMode
);
```

Устанавливает позицию и вращение тела.

#### GetPosition / GetRotation

```cpp
JPH::Vec3 GetPosition(JPH::BodyID inBodyID) const;
JPH::Quat GetRotation(JPH::BodyID inBodyID) const;
```

Возвращает позицию и вращение тела.

#### AddForce / AddImpulse

```cpp
void AddForce(JPH::BodyID inBodyID, const JPH::Vec3& inForce, JPH::EActivation inActivationMode);
void AddImpulse(JPH::BodyID inBodyID, const JPH::Vec3& inImpulse, JPH::EActivation inActivationMode);
```

Применяет силу или импульс к телу.

#### SetLinearVelocity / SetAngularVelocity

```cpp
void SetLinearVelocity(JPH::BodyID inBodyID, const JPH::Vec3& inVelocity);
void SetAngularVelocity(JPH::BodyID inBodyID, const JPH::Vec3& inVelocity);
```

Устанавливает линейную и угловую скорость.

---

### JPH::BodyCreationSettings

```cpp
struct JPH::BodyCreationSettings
```

Настройки для создания тела.

| Поле                  | Тип                   | Описание                                           |
|-----------------------|-----------------------|----------------------------------------------------|
| `mPosition`           | `JPH::Vec3`           | Начальная позиция                                  |
| `mRotation`           | `JPH::Quat`           | Начальное вращение                                 |
| `mMotionType`         | `JPH::EMotionType`    | Тип движения: `Static`, `Kinematic`, `Dynamic`     |
| `mObjectLayer`        | `JPH::ObjectLayer`    | Слой объекта для фильтрации коллизий               |
| `mFriction`           | `float`               | Коэффициент трения (0.0-1.0)                       |
| `mRestitution`        | `float`               | Упругость (0.0-1.0)                                |
| `mLinearDamping`      | `float`               | Линейное затухание                                 |
| `mAngularDamping`     | `float`               | Угловое затухание                                  |
| `mMaxLinearVelocity`  | `float`               | Максимальная линейная скорость                     |
| `mMaxAngularVelocity` | `float`               | Максимальная угловая скорость                      |
| `mGravityFactor`      | `float`               | Множитель гравитации (1.0 = нормальная гравитация) |
| `mMotionQuality`      | `JPH::EMotionQuality` | Качество движения: `Discrete`, `LinearCast`        |

---

### JPH::EMotionType

```cpp
enum class JPH::EMotionType : uint8
```

Тип движения тела.

| Значение    | Описание                                        |
|-------------|-------------------------------------------------|
| `Static`    | Статическое тело, не движется                   |
| `Kinematic` | Кинематическое тело, движется по заданному пути |
| `Dynamic`   | Динамическое тело, реагирует на силы и коллизии |

---

## Формы (Shapes)

### Базовые классы

#### JPH::ShapeSettings

```cpp
class JPH::ShapeSettings
```

Базовый класс для всех настроек форм. Содержит общие параметры и метод для создания формы.

**Методы:**

```cpp
virtual ShapeResult Create() const = 0;  // Создает форму из настроек
```

**Поля:**

- `mUserData` - пользовательские данные для связи с приложением
- `mMaterial` - указатель на материал (физические свойства поверхности)

#### JPH::ConvexShapeSettings

```cpp
class JPH::ConvexShapeSettings : public JPH::ShapeSettings
```

Базовый класс для настроек выпуклых форм (сферы, кубы, капсулы и т.д.).

**Конструктор:**

```cpp
explicit ConvexShapeSettings(const PhysicsMaterial* inMaterial = nullptr);
```

#### JPH::Shape

```cpp
class JPH::Shape
```

Базовый класс всех форм. Формы неизменяемы (immutable) и могут использоваться несколькими телами одновременно для
экономии памяти.

**Основные методы:**

```cpp
virtual AABox GetLocalBounds() const = 0;  // Локальные границы формы
virtual AABox GetWorldSpaceBounds(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale) const = 0;  // Глобальные границы
virtual float GetVolume() const = 0;  // Объем формы
virtual MassProperties GetMassProperties() const = 0;  // Массовые свойства
virtual bool IsValidScale(Vec3Arg inScale) const = 0;  // Проверка допустимости масштаба
virtual Vec3 MakeScaleValid(Vec3Arg inScale) const = 0;  // Коррекция масштаба
```

### Простые формы (Primitive Shapes)

#### BoxShape - Куб

**Настройки (JPH::BoxShapeSettings):**

```cpp
class JPH::BoxShapeSettings : public JPH::ConvexShapeSettings
```

**Конструкторы:**

```cpp
explicit BoxShapeSettings(Vec3Arg inHalfExtent, float inConvexRadius = 0.0f, 
                          const PhysicsMaterial* inMaterial = nullptr);
```

**Параметры:**

- `inHalfExtent` - половина размера по каждой оси (Vec3)
- `inConvexRadius` - радиус выпуклости для скругления ребер (по умолчанию 0)
- `inMaterial` - физический материал (по умолчанию nullptr)

**Класс формы (JPH::BoxShape):**

```cpp
class JPH::BoxShape : public JPH::ConvexShape
```

**Конструкторы:**

```cpp
explicit BoxShape(Vec3Arg inHalfExtent, float inConvexRadius = 0.0f,
                  const PhysicsMaterial* inMaterial = nullptr);
```

**Основные методы:**

```cpp
Vec3 GetHalfExtent() const;  // Возвращает половину размера
float GetConvexRadius() const;  // Возвращает радиус выпуклости
```

#### SphereShape - Сфера

**Настройки (JPH::SphereShapeSettings):**

```cpp
class JPH::SphereShapeSettings : public JPH::ConvexShapeSettings
```

**Конструкторы:**

```cpp
explicit SphereShapeSettings(float inRadius, const PhysicsMaterial* inMaterial = nullptr);
```

**Параметры:**

- `inRadius` - радиус сферы (должен быть > 0)
- `inMaterial` - физический материал (по умолчанию nullptr)

**Класс формы (JPH::SphereShape):**

```cpp
class JPH::SphereShape : public JPH::ConvexShape
```

**Конструкторы:**

```cpp
explicit SphereShape(float inRadius, const PhysicsMaterial* inMaterial = nullptr);
```

**Основные методы:**

```cpp
float GetRadius() const;  // Возвращает радиус сферы
float GetInnerRadius() const;  // Внутренний радиус (равен mRadius для сферы)
```

#### CapsuleShape - Капсула

**Настройки (JPH::CapsuleShapeSettings):**

```cpp
class JPH::CapsuleShapeSettings : public JPH::ConvexShapeSettings
```

**Конструкторы:**

```cpp
CapsuleShapeSettings(float inHalfHeightOfCylinder, float inRadius,
                     const PhysicsMaterial* inMaterial = nullptr);
```

**Параметры:**

- `inHalfHeightOfCylinder` - половина высоты цилиндрической части (должна быть ≥ 0)
- `inRadius` - радиус капсулы (должен быть > 0)
- `inMaterial` - физический материал (по умолчанию nullptr)

**Вспомогательные методы (в Settings):**

```cpp
bool IsValid() const;  // Проверяет, являются ли параметры валидными
bool IsSphere() const;  // Проверяет, является ли капсула сферой (inHalfHeightOfCylinder == 0)
```

**Класс формы (JPH::CapsuleShape):**

```cpp
class JPH::CapsuleShape : public JPH::ConvexShape
```

**Конструкторы:**

```cpp
CapsuleShape(float inHalfHeightOfCylinder, float inRadius,
             const PhysicsMaterial* inMaterial = nullptr);
```

**Основные методы:**

```cpp
float GetRadius() const;  // Возвращает радиус капсулы
float GetHalfHeightOfCylinder() const;  // Возвращает половину высоты цилиндрической части
```

#### CylinderShape - Цилиндр

**Настройки (JPH::CylinderShapeSettings):**

```cpp
class JPH::CylinderShapeSettings : public JPH::ConvexShapeSettings
```

**Конструкторы:**

```cpp
CylinderShapeSettings(float inHalfHeight, float inRadius,
                      float inConvexRadius = cDefaultConvexRadius,
                      const PhysicsMaterial* inMaterial = nullptr);
```

**Параметры:**

- `inHalfHeight` - половина высоты цилиндра
- `inRadius` - радиус цилиндра
- `inConvexRadius` - радиус выпуклости для скругления ребер (по умолчанию cDefaultConvexRadius)
- `inMaterial` - физический материал (по умолчанию nullptr)

**Класс формы (JPH::CylinderShape):**

```cpp
class JPH::CylinderShape : public JPH::ConvexShape
```

**Конструкторы:**

```cpp
CylinderShape(float inHalfHeight, float inRadius,
              float inConvexRadius = cDefaultConvexRadius,
              const PhysicsMaterial* inMaterial = nullptr);
```

**Основные методы:**

```cpp
float GetHalfHeight() const;  // Возвращает половину высоты цилиндра
float GetRadius() const;  // Возвращает радиус цилиндра
float GetConvexRadius() const;  // Возвращает радиус выпуклости
```

### Сложные формы (Complex Shapes)

#### ConvexHullShape - Выпуклая оболочка

**Настройки (JPH::ConvexHullShapeSettings):**

```cpp
class JPH::ConvexHullShapeSettings : public JPH::ConvexShapeSettings
```

**Конструкторы:**

```cpp
ConvexHullShapeSettings(const Vec3* inPoints, uint inNumPoints,
                        float inMaxConvexRadius = cDefaultConvexRadius,
                        const PhysicsMaterial* inMaterial = nullptr);
```

**Параметры:**

- `inPoints` - массив точек для построения выпуклой оболочки
- `inNumPoints` - количество точек (должно быть ≥ 4)
- `inMaxConvexRadius` - максимальный радиус выпуклости
- `inMaterial` - физический материал (по умолчанию nullptr)

#### MeshShape - Полигональная сетка (невыпуклая)

**Настройки (JPH::MeshShapeSettings):**

```cpp
class JPH::MeshShapeSettings : public JPH::ShapeSettings
```

**Конструкторы:**

```cpp
explicit MeshShapeSettings(const TriangleList& inTriangles,
                           const PhysicsMaterialList& inMaterials = PhysicsMaterialList());
```

**Параметры:**

- `inTriangles` - список треугольников
- `inMaterials` - список материалов для треугольников (по умолчанию пустой)

**Особенности:**

- MeshShape не является выпуклой формой
- Используется для статических коллайдеров сложной геометрии
- Не поддерживает некоторые операции, доступные для выпуклых форм

#### HeightFieldShape - Высотное поле (террейн)

**Константы HeightFieldShapeConstants:**

```cpp
namespace JPH::HeightFieldShapeConstants {
    constexpr float cNoCollisionValue = FLT_MAX;  // Значение, используемое для создания дыр/пропусков в высотном поле
    constexpr uint16 cNoCollisionValue16 = 0xffff; // 16-битное значение для "нет коллизии"
    constexpr uint16 cMaxHeightValue16 = 0xfffe;   // Максимальное допустимое значение высоты в 16-битном формате
}
```

**Настройки (JPH::HeightFieldShapeSettings):**

```cpp
class JPH::HeightFieldShapeSettings : public JPH::ShapeSettings
```

**Конструкторы:**

```cpp
// Основной конструктор
HeightFieldShapeSettings(const float* inSamples, 
                        Vec3Arg inOffset, 
                        Vec3Arg inScale, 
                        uint32 inSampleCount, 
                        const uint8* inMaterialIndices = nullptr, 
                        const PhysicsMaterialList& inMaterialList = PhysicsMaterialList());
```

**Параметры конструктора:**

- `inSamples` - массив высотных сэмплов размером `inSampleCount * inSampleCount`
- `inOffset` - смещение всей высотной карты в мировом пространстве
- `inScale` - масштаб по осям X, Y, Z
- `inSampleCount` - количество сэмплов в каждом измерении (должно быть кратно `mBlockSize`)
- `inMaterialIndices` - индексы материалов для квадратов `(inSampleCount - 1)^2` (опционально)
- `inMaterialList` - список материалов (опционально)

**Основные поля HeightFieldShapeSettings:**

- `mOffset` (Vec3) - смещение: `mOffset + mScale * (x, height, y)`
- `mScale` (Vec3) - масштаб: по умолчанию `Vec3::sOne()`
- `mSampleCount` (uint32) - количество сэмплов в каждом измерении
- `mMinHeightValue` (float) - минимальное значение высоты (для компрессии, по умолчанию `cLargeFloat`)
- `mMaxHeightValue` (float) - максимальное значение высоты (для компрессии, по умолчанию `-cLargeFloat`)
- `mMaterialsCapacity` (uint32) - емкость списка материалов (для предотвращения реаллокаций)
- `mBlockSize` (uint32) - размер блока для оптимизации (2-8, эффективный блок = `mBlockSize / 2`)
- `mBitsPerSample` (uint32) - бит на сэмпл (1-8, больше бит = выше точность)
- `mHeightSamples` (Array<float>) - массив высотных сэмплов `mSampleCount^2` в row-major порядке
- `mMaterialIndices` (Array<uint8>) - индексы материалов `(mSampleCount - 1)^2`
- `mMaterials` (PhysicsMaterialList) - список материалов
- `mActiveEdgeCosThresholdAngle` (float) - косинус порогового угла для активных ребер (по умолчанию cos(5°) = 0.996195f)

**Методы HeightFieldShapeSettings:**

```cpp
// Определяет минимальное и максимальное значение в mHeightSamples (игнорирует cNoCollisionValue)
void DetermineMinAndMaxSample(float& outMinValue, float& outMaxValue, float& outQuantizationScale) const;

// Рассчитывает количество бит на сэмпл для достижения максимальной ошибки inMaxError
uint32 CalculateBitsPerSampleForError(float inMaxError) const;

// Создает HeightFieldShape (переопределение ShapeSettings::Create())
virtual ShapeResult Create() const override;
```

**Класс формы (JPH::HeightFieldShape):**

```cpp
class JPH::HeightFieldShape final : public Shape
```

**Конструкторы:**

```cpp
// Конструктор для создания из настроек
HeightFieldShape(const HeightFieldShapeSettings& inSettings, ShapeResult& outResult);
```

**Основные методы HeightFieldShape:**

```cpp
// Основные геттеры
uint GetSampleCount() const;    // Возвращает количество сэмплов (округленное до кратного mBlockSize)
uint GetBlockSize() const;      // Возвращает размер блока

// Работа с высотными данными
Vec3 GetPosition(uint inX, uint inY) const;              // Возвращает позицию в точке (x, y)
bool IsNoCollision(uint inX, uint inY) const;           // Проверяет, есть ли коллизия в точке
bool ProjectOntoSurface(Vec3Arg inLocalPosition,        // Проецирует точку на поверхность
                       Vec3& outSurfacePosition, 
                       SubShapeID& outSubShapeID) const;

// Получение/установка высот
void GetHeights(uint inX, uint inY, uint inSizeX, uint inSizeY, 
                float* outHeights, intptr_t inHeightsStride) const;
void SetHeights(uint inX, uint inY, uint inSizeX, uint inSizeY,
                const float* inHeights, intptr_t inHeightsStride,
                TempAllocator& inAllocator,
                float inActiveEdgeCosThresholdAngle = 0.996195f);

// Получение/установка материалов
const PhysicsMaterialList& GetMaterialList() const;
void GetMaterials(uint inX, uint inY, uint inSizeX, uint inSizeY,
                  uint8* outMaterials, intptr_t inMaterialsStride) const;
bool SetMaterials(uint inX, uint inY, uint inSizeX, uint inSizeY,
                  const uint8* inMaterials, intptr_t inMaterialsStride,
                  const PhysicsMaterialList* inMaterialList,
                  TempAllocator& inAllocator);

// Клонирование (для избежания race conditions при модификации во время запросов)
Ref<HeightFieldShape> Clone() const;

// Диапазон высот
float GetMinHeightValue() const;  // Минимальная кодируемая высота
float GetMaxHeightValue() const;  // Максимальная кодируемая высота

// Обязательно статическая форма
virtual bool MustBeStatic() const override { return true; }
```

**Пример создания HeightFieldShape:**

```cpp
// Создание данных высотного поля (8x8 сэмплов)
constexpr uint32 sample_count = 8;
float height_samples[sample_count * sample_count] = {
    0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 1.5f, 1.0f, 0.5f,
    0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 2.0f, 1.5f, 1.0f,
    // ... остальные сэмплы
};

// Создание настроек
JPH::HeightFieldShapeSettings settings(
    height_samples,                    // массив высот
    JPH::Vec3(0.0f, 0.0f, 0.0f),      // смещение
    JPH::Vec3(1.0f, 0.5f, 1.0f),      // масштаб (Y сжат для более пологих холмов)
    sample_count,                      // количество сэмплов
    nullptr,                           // индексы материалов (опционально)
    {}                                 // список материалов (опционально)
);

// Настройка параметров
settings.mBlockSize = 4;              // Блок 4x4 для баланса памяти/производительности
settings.mBitsPerSample = 6;          // 6 бит на сэмпл для компромисса точности/памяти
settings.mActiveEdgeCosThresholdAngle = 0.984808f; // cos(10°) - более агрессивное сглаживание

// Создание формы
JPH::ShapeResult result = settings.Create();
if (result.IsValid()) {
    JPH::HeightFieldShape* height_field = static_cast<JPH::HeightFieldShape*>(result.Get());
    
    // Использование высотного поля
    JPH::Vec3 position = height_field->GetPosition(3, 4);  // Позиция в точке (3, 4)
    bool has_collision = height_field->IsNoCollision(5, 5); // Проверка коллизии
}
```

**Особенности и рекомендации для ProjectV:**

1. **Воксельный ландшафт**: HeightFieldShape идеально подходит для рендеринга воксельного террейна с физической
   коллизией.
2. **Динамическая модификация**: Используйте `SetHeights()` для изменения ландшафта во время выполнения (создавая race
   conditions, используйте `Clone()` для безопасной модификации).
3. **Оптимизация памяти**: Увеличивайте `mBlockSize` (2-8) для уменьшения памяти, уменьшайте `mBitsPerSample` (1-8) для
   компрессии.
4. **Материалы**: Используйте `mMaterialIndices` для разных типов поверхности (трава, камень, песок и т.д.).
5. **Статичность**: HeightFieldShape всегда статичен (`MustBeStatic() == true`), используйте для неподвижного ландшафта.

#### CompoundShape - Составная форма

**Настройки (JPH::CompoundShapeSettings):**

```cpp
class JPH::CompoundShapeSettings : public JPH::ShapeSettings
```

**Конструкторы:**

```cpp
CompoundShapeSettings();
```

**Методы добавления под-форм:**

```cpp
void AddShape(Vec3Arg inPosition, QuatArg inRotation, const ShapeSettings* inShape);
void AddShape(Vec3Arg inPosition, QuatArg inRotation, const Shape* inShape);
```

**Особенности:**

- Объединяет несколько форм в одну
- Каждая под-форма имеет собственные позицию и вращение
- Эффективно для создания сложных коллайдеров из простых примитивов

### Примеры создания форм

#### Создание через Settings (рекомендуемый способ):

```cpp
// Создание куба
JPH::BoxShapeSettings box_settings(JPH::Vec3(1.0f, 2.0f, 1.0f), 0.05f);
JPH::ShapeResult box_result = box_settings.Create();
if (box_result.IsValid()) {
    JPH::BoxShape* box_shape = static_cast<JPH::BoxShape*>(box_result.Get());
    // Использовать форму
}

// Создание сферы
JPH::SphereShapeSettings sphere_settings(1.5f);
JPH::ShapeResult sphere_result = sphere_settings.Create();
// Проверка результата аналогично кубу

// Создание капсулы
JPH::CapsuleShapeSettings capsule_settings(1.0f, 0.5f);
JPH::ShapeResult capsule_result = capsule_settings.Create();

// Создание цилиндра
JPH::CylinderShapeSettings cylinder_settings(1.5f, 0.8f);
JPH::ShapeResult cylinder_result = cylinder_settings.Create();
```

#### Прямое создание (для простых случаев):

```cpp
// Прямое создание сферы (без Settings)
JPH::SphereShape sphere_shape(1.5f);

// Прямое создание куба
JPH::BoxShape box_shape(JPH::Vec3(1.0f, 2.0f, 1.0f), 0.05f);

// Прямое создание капсулы
JPH::CapsuleShape capsule_shape(1.0f, 0.5f);

// Прямое создание цилиндра
JPH::CylinderShape cylinder_shape(1.5f, 0.8f);
```

#### Создание составной формы:

```cpp
JPH::CompoundShapeSettings compound_settings;

// Добавление под-форм
compound_settings.AddShape(
    JPH::Vec3(0.0f, 0.5f, 0.0f),  // Позиция
    JPH::Quat::sIdentity(),        // Вращение
    new JPH::BoxShapeSettings(JPH::Vec3(1.0f, 0.5f, 1.0f))
);

compound_settings.AddShape(
    JPH::Vec3(0.0f, -0.5f, 0.0f),
    JPH::Quat::sIdentity(),
    new JPH::SphereShapeSettings(0.8f)
);

JPH::ShapeResult compound_result = compound_settings.Create();
if (compound_result.IsValid()) {
    JPH::CompoundShape* compound_shape = static_cast<JPH::CompoundShape*>(compound_result.Get());
    // Использовать составную форму
}
```

### Общие советы по использованию форм

1. **Неизменяемость**: Формы являются неизменяемыми (immutable) после создания. Это позволяет повторно использовать одну
   форму для многих тел.

2. **Конвекс-радиус**: Для BoxShape и CylinderShape используйте конвекс-радиус > 0 для улучшения стабильности
   симуляции (скругление ребер).

3. **Проверка результата**: Всегда проверяйте `ShapeResult::IsValid()` после создания формы через Settings.

4. **Масштабирование**: Используйте `IsValidScale()` и `MakeScaleValid()` для проверки и коррекции масштабирования форм.

5. **Выбор формы**: Для статических объектов используйте MeshShape или HeightFieldShape, для динамических - выпуклые
   формы (Box, Sphere, Capsule, Cylinder, ConvexHull).

---

## Слои и фильтры коллизий

### JPH::ObjectLayer

```cpp
typedef uint8 JPH::ObjectLayer;
```

8-битный идентификатор слоя объекта. Определяет, какие объекты могут сталкиваться.

### JPH::BroadPhaseLayer

```cpp
typedef uint8 JPH::BroadPhaseLayer;
```

8-битный идентификатор слоя широкой фазы. Используется для оптимизации коллизий.

### JPH::BroadPhaseLayerInterface

```cpp
class JPH::BroadPhaseLayerInterface
```

Абстрактный класс для определения слоёв широкой фазы.

**Методы:**

```cpp
virtual uint GetNumBroadPhaseLayers() const = 0;
virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const = 0;
```

### JPH::ObjectVsBroadPhaseLayerFilter

```cpp
class JPH::ObjectVsBroadPhaseLayerFilter
```

Фильтр для определения, может ли объект сталкиваться с слоем широкой фазы.

**Методы:**

```cpp
virtual bool ShouldCollide(
    JPH::ObjectLayer inLayer1,
    JPH::BroadPhaseLayer inLayer2
) const = 0;
```

### JPH::ObjectVsObjectLayerFilter

```cpp
class JPH::ObjectVsObjectLayerFilter
```

Фильтр для определения, могут ли два объекта сталкиваться.

**Методы:**

```cpp
virtual bool ShouldCollide(
    JPH::ObjectLayer inLayer1,
    JPH::ObjectLayer inLayer2
) const = 0;
```

**Пример реализации для ProjectV:**

```cpp
class ObjectLayerPairFilter : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) override {
        // Персонажи не сталкиваются друг с другом
        if (inObject1 == (JPH::uint8)ObjectLayer::CHARACTER &&
            inObject2 == (JPH::uint8)ObjectLayer::CHARACTER) {
            return false;
        }
        
        // Датчики не сталкиваются ни с чем
        if (inObject1 == (JPH::uint8)ObjectLayer::SENSOR ||
            inObject2 == (JPH::uint8)ObjectLayer::SENSOR) {
            return false;
        }
        
        return true;
    }
};
```

---

## Ограничения (Constraints)

### JPH::ConstraintSettings

Базовый класс для настроек ограничений.

### JPH::FixedConstraintSettings

```cpp
class JPH::FixedConstraintSettings : public JPH::TwoBodyConstraintSettings
```

Настройки для фиксированного соединения двух тел.

### JPH::PointConstraintSettings

```cpp
class JPH::PointConstraintSettings : public JPH::TwoBodyConstraintSettings
```

Настройки для точечного соединения (шарнир).

### JPH::HingeConstraintSettings

```cpp
class JPH::HingeConstraintSettings : public JPH::TwoBodyConstraintSettings
```

Настройки для шарнирного соединения.

### JPH::SliderConstraintSettings

```cpp
class JPH::SliderConstraintSettings : public JPH::TwoBodyConstraintSettings
```

Настройки для линейного соединения (ползунок).

### JPH::ConeConstraintSettings

```cpp
class JPH::ConeConstraintSettings : public JPH::TwoBodyConstraintSettings
```

Настройки для конического соединения.

### JPH::DistanceConstraintSettings

```cpp
class JPH::DistanceConstraintSettings : public JPH::TwoBodyConstraintSettings
```

Настройки для соединения с фиксированным расстоянием.

### Создание ограничения

```cpp
JPH::Constraint* constraint = settings.Create(
    body1.GetID(),
    body2.GetID()
);
physics_system.AddConstraint(constraint);
```

---

## Job System

### JPH::JobSystemThreadPool

```cpp
class JPH::JobSystemThreadPool : public JPH::JobSystem
```

Пул потоков для выполнения задач.

**Конструктор:**

```cpp
JobSystemThreadPool(
    uint inMaxJobs,          // Максимальное количество задач
    uint inMaxBarriers,      // Максимальное количество барьеров
    int inNumThreads         // Количество потоков (-1 = все ядра)
);
```

**Пример:**

```cpp
JPH::JobSystemThreadPool job_system(
    JPH::cMaxPhysicsJobs,           // 65536
    JPH::cMaxPhysicsBarriers,       // 1024
    std::thread::hardware_concurrency() - 1  // Все ядра кроме одного
);
```

### JPH::JobHandle

```cpp
class JPH::JobHandle
```

Дескриптор задачи для отслеживания выполнения.

### Добавление задачи

```cpp
JPH::JobHandle handle = job_system.CreateJob(
    "PhysicsUpdate",         // Имя задачи (для отладки)
    JPH::Color::sRed,        // Цвет (для визуализации)
    [](JPH::JobSystem* inJobSystem, uint inThreadIndex) {
        // Код задачи
    },
    JPH::JOB_PRIORITY_HIGH   // Приоритет
);

job_system.AddJob(handle);
job_system.WaitForJobs(handle);  // Ожидание завершения
```

---

## Вспомогательные классы

### JPH::Vec3

```cpp
class JPH::Vec3
```

3-х мерный вектор.

**Методы:**

```cpp
static JPH::Vec3 sZero();                    // Нулевой вектор
static JPH::Vec3 sAxisX();                   // Ось X
static JPH::Vec3 sAxisY();                   // Ось Y
static JPH::Vec3 sAxisZ();                   // Ось Z

float GetX(), GetY(), GetZ() const;         // Получение компонент
void SetX(float inX), SetY(float inY), SetZ(float inZ); // Установка компонент

float Length() const;                       // Длина вектора
float LengthSq() const;                     // Квадрат длины
JPH::Vec3 Normalized() const;               // Нормализованный вектор

// Арифметические операции
JPH::Vec3 operator+(const JPH::Vec3& inRHS) const;
JPH::Vec3 operator-(const JPH::Vec3& inRHS) const;
JPH::Vec3 operator*(float inRHS) const;
JPH::Vec3 operator/(float inRHS) const;

// Точечное и векторное произведение
float Dot(const JPH::Vec3& inRHS) const;
JPH::Vec3 Cross(const JPH::Vec3& inRHS) const;
```

### JPH::Quat

```cpp
class JPH::Quat
```

Кватернион для представления вращения.

**Методы:**

```cpp
static JPH::Quat sIdentity();                // Единичный кватернион
static JPH::Quat sRotation(JPH::Vec3Arg inAxis, float inAngle); // Вращение вокруг оси

JPH::Vec3 GetAxis() const;                  // Ось вращения
float GetAngle() const;                     // Угол вращения

JPH::Quat Conjugated() const;               // Сопряжённый кватернион
JPH::Quat Inversed() const;                 // Обратный кватернион

// Умножение кватернионов
JPH::Quat operator*(const JPH::Quat& inRHS) const;

// Вращение вектора
JPH::Vec3 operator*(JPH::Vec3Arg inRHS) const;
```

### JPH::Mat44

```cpp
class JPH::Mat44
```

Матрица 4x4 для преобразований.

**Методы:**

```cpp
static JPH::Mat44 sIdentity();              // Единичная матрица
static JPH::Mat44 sTranslation(JPH::Vec3Arg inTranslation); // Матрица переноса
static JPH::Mat44 sRotation(JPH::QuatArg inRotation);       // Матрица вращения
static JPH::Mat44 sScale(JPH::Vec3Arg inScale);             // Матрица масштабирования

JPH::Vec3 GetTranslation() const;           // Получение переноса
JPH::Quat GetRotation() const;              // Получение вращения

// Умножение матриц
JPH::Mat44 operator*(const JPH::Mat44& inRHS) const;

// Умножение на вектор
JPH::Vec3 operator*(JPH::Vec3Arg inRHS) const;
```

---

## Утилиты преобразования

### Преобразование между Jolt и glm

```cpp
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Vec3: Jolt -> glm
inline glm::vec3 ToGlmVec3(const JPH::Vec3& v) {
    return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
}

// Vec3: glm -> Jolt
inline JPH::Vec3 ToJoltVec3(const glm::vec3& v) {
    return JPH::Vec3(v.x, v.y, v.z);
}

// Quat: Jolt -> glm
inline glm::quat ToGlmQuat(const JPH::Quat& q) {
    return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
}

// Quat: glm -> Jolt
inline JPH::Quat ToJoltQuat(const glm::quat& q) {
    return JPH::Quat(q.x, q.y, q.z, q.w);
}
```

### Преобразование между Jolt и Vulkan

```cpp
// Преобразование системы координат: Jolt (Y-up) -> Vulkan (Y-down)
inline JPH::Vec3 ToVulkanCoordinates(const JPH::Vec3& jolt_pos) {
    return JPH::Vec3(jolt_pos.GetX(), -jolt_pos.GetY(), jolt_pos.GetZ());
}

inline JPH::Quat ToVulkanRotation(const JPH::Quat& jolt_rot) {
    // Для переворота по оси Y нужен специальный кватернион
    JPH::Quat flip_y = JPH::Quat::sRotation(JPH::Vec3::sAxisX(), JPH_PI);
    return flip_y * jolt_rot;
}
```

---

## Константы

### Лимиты системы

```cpp
namespace JPH {
    constexpr uint32 cMaxBodies = 65536;           // Максимальное количество тел
    constexpr uint32 cNumBodyMutexes = 0;          // Количество мьютексов тел
    constexpr uint32 cMaxBodyPairs = 65536;        // Максимальное количество пар тел
    constexpr uint32 cMaxContactConstraints = 10240; // Максимальное количество контактов
    constexpr int cCollisionSteps = 1;             // Количество шагов коллизий
    constexpr uint32 cMaxPhysicsJobs = 65536;      // Максимальное количество задач
    constexpr uint32 cMaxPhysicsBarriers = 1024;   // Максимальное количество барьеров
}
```

### Версия библиотеки

```cpp
#define JPH_VERSION_MAJOR 5    // Основная версия
#define JPH_VERSION_MINOR 5    // Минорная версия  
#define JPH_VERSION_PATCH 1    // Патч-версия
```

---

## Смотрите также

- [Quickstart Guide](quickstart.md) - Быстрый старт с JoltPhysics
- [Concepts](concepts.md) - Основные концепции JoltPhysics
- [Integration](integration.md) - Интеграция в ProjectV
- [Voxel Physics](voxel-physics.md) - Специализированная физика для вокселей
- [ECS Integration](flecs-integration.md) - Интеграция с flecs ECS
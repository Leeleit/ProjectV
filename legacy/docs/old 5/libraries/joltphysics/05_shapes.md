# Формы (Shapes) в JoltPhysics

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

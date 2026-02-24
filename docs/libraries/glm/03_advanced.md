## Практические рецепты GLM

<!-- anchor: 05_tools -->


Готовые решения для типичных задач: трансформации, камера, кватернионы.

## MVP-трансформация

Полный пример вычисления матрицы Model-View-Projection:

```cpp
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

glm::mat4 computeMVP(
    glm::vec3 position,      // Позиция объекта
    glm::vec3 rotation,      // Углы Эйлера (радианы)
    glm::vec3 scale,         // Масштаб
    glm::vec3 cameraPos,     // Позиция камеры
    glm::vec3 cameraTarget,  // Точка взгляда
    float fov,               // Поле зрения (радианы)
    float aspect,            // Соотношение сторон
    float nearPlane,         // Ближняя плоскость
    float farPlane           // Дальняя плоскость
) {
    // Model: локальные → мировые координаты
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, rotation.x, glm::vec3(1, 0, 0));  // Pitch
    model = glm::rotate(model, rotation.y, glm::vec3(0, 1, 0));  // Yaw
    model = glm::rotate(model, rotation.z, glm::vec3(0, 0, 1));  // Roll
    model = glm::scale(model, scale);

    // View: мировые → координаты камеры
    glm::mat4 view = glm::lookAt(
        cameraPos,
        cameraTarget,
        glm::vec3(0, 1, 0)  // Вектор "вверх"
    );

    // Projection: 3D → 2D экран
    glm::mat4 proj = glm::perspective(fov, aspect, nearPlane, farPlane);

    return proj * view * model;
}
```

---

## FPS-камера

Камера от первого лица с управлением через WASD + мышь:

```cpp
class FPSCamera {
public:
    glm::vec3 position{0, 0, 5};
    glm::vec3 front{0, 0, -1};
    glm::vec3 up{0, 1, 0};
    glm::vec3 right{1, 0, 0};
    glm::vec3 worldUp{0, 1, 0};

    float yaw{-90.0f};      // Горизонтальный угол (радианы)
    float pitch{0.0f};      // Вертикальный угол (радианы)
    float speed{2.5f};      // Скорость перемещения
    float sensitivity{0.1f}; // Чувствительность мыши

    void processMouse(float xoffset, float yoffset) {
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        yaw   += xoffset;
        pitch += yoffset;

        // Ограничение по вертикали
        pitch = glm::clamp(pitch, -89.0f, 89.0f);

        updateVectors();
    }

    void processKeyboard(const char direction, float dt) {
        float velocity = speed * dt;
        switch (direction) {
            case 'W': position += front * velocity; break;
            case 'S': position -= front * velocity; break;
            case 'A': position -= right * velocity; break;
            case 'D': position += right * velocity; break;
        }
    }

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, position + front, up);
    }

private:
    void updateVectors() {
        glm::vec3 newFront;
        newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        newFront.y = sin(glm::radians(pitch));
        newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(newFront);
        right = glm::normalize(glm::cross(front, worldUp));
        up    = glm::normalize(glm::cross(right, front));
    }
};
```

---

## Orbit-камера

Камера, вращающаяся вокруг цели:

```cpp
class OrbitCamera {
public:
    glm::vec3 target{0, 0, 0};
    float distance{5.0f};
    float minDistance{1.0f};
    float maxDistance{50.0f};

    float yaw{0.0f};
    float pitch{30.0f};

    void rotate(float dYaw, float dPitch) {
        yaw += dYaw;
        pitch = glm::clamp(pitch + dPitch, -89.0f, 89.0f);
    }

    void zoom(float delta) {
        distance = glm::clamp(distance - delta, minDistance, maxDistance);
    }

    glm::mat4 getViewMatrix() const {
        glm::vec3 position;
        position.x = target.x + distance * cos(glm::radians(pitch)) * cos(glm::radians(yaw));
        position.y = target.y + distance * sin(glm::radians(pitch));
        position.z = target.z + distance * cos(glm::radians(pitch)) * sin(glm::radians(yaw));

        return glm::lookAt(position, target, glm::vec3(0, 1, 0));
    }

    glm::vec3 getPosition() const {
        glm::vec3 pos;
        pos.x = target.x + distance * cos(glm::radians(pitch)) * cos(glm::radians(yaw));
        pos.y = target.y + distance * sin(glm::radians(pitch));
        pos.z = target.z + distance * cos(glm::radians(pitch)) * sin(glm::radians(yaw));
        return pos;
    }
};
```

---

## Плавные повороты с кватернионами

Интерполяция между двумя ориентациями:

```cpp
#include <glm/gtc/quaternion.hpp>

class SmoothRotation {
    glm::quat current{1.0f, 0.0f, 0.0f, 0.0f};  // Текущая ориентация
    glm::quat target{1.0f, 0.0f, 0.0f, 0.0f};   // Целевая ориентация
    float speed{2.0f};                           // Скорость поворота

public:
    void setTarget(glm::vec3 eulerAngles) {
        target = glm::quat(eulerAngles);
    }

    void setTargetAxisAngle(float angle, glm::vec3 axis) {
        target = glm::angleAxis(angle, axis);
    }

    void setTargetDirection(glm::vec3 direction, glm::vec3 up = glm::vec3(0, 1, 0)) {
        target = glm::quatLookAt(glm::normalize(direction), up);
    }

    void update(float dt) {
        float t = glm::clamp(dt * speed, 0.0f, 1.0f);
        current = glm::slerp(current, target, t);
        current = glm::normalize(current);
    }

    glm::mat4 getMatrix() const {
        return glm::mat4_cast(current);
    }

    glm::quat getQuat() const {
        return current;
    }
};
```

---

## LookAt для объектов

Направить объект в сторону цели:

```cpp
glm::mat4 lookAtObject(glm::vec3 position, glm::vec3 target, glm::vec3 up = glm::vec3(0, 1, 0)) {
    glm::vec3 forward = glm::normalize(target - position);
    glm::vec3 right = glm::normalize(glm::cross(forward, up));
    glm::vec3 newUp = glm::cross(right, forward);

    glm::mat4 rotation(1.0f);
    rotation[0] = glm::vec4(right, 0.0f);
    rotation[1] = glm::vec4(newUp, 0.0f);
    rotation[2] = glm::vec4(-forward, 0.0f);  // Отрицательный forward

    glm::mat4 translation(1.0f);
    translation[3] = glm::vec4(position, 1.0f);

    return translation * rotation;
}
```

---

## Билборды

Поворот объекта так, чтобы он всегда смотрел на камеру:

```cpp
glm::mat4 createBillboard(glm::vec3 objectPos, glm::vec3 cameraPos, glm::vec3 up = glm::vec3(0, 1, 0)) {
    glm::vec3 toCamera = cameraPos - objectPos;
    toCamera.y = 0;  // Y-билборд (только вращение вокруг Y)
    toCamera = glm::normalize(toCamera);

    glm::vec3 right = glm::normalize(glm::cross(up, toCamera));
    glm::vec3 forward = glm::normalize(glm::cross(right, up));

    glm::mat4 model(1.0f);
    model[0] = glm::vec4(right, 0);
    model[1] = glm::vec4(up, 0);
    model[2] = glm::vec4(forward, 0);
    model[3] = glm::vec4(objectPos, 1);

    return model;
}
```

---

## Преобразование координат

### Локальные → мировые

```cpp
glm::vec3 localToWorld(glm::vec3 localPos, glm::mat4 modelMatrix) {
    glm::vec4 worldPos = modelMatrix * glm::vec4(localPos, 1.0f);
    return glm::vec3(worldPos);
}
```

### Мировые → локальные

```cpp
glm::vec3 worldToLocal(glm::vec3 worldPos, glm::mat4 modelMatrix) {
    glm::mat4 invModel = glm::inverse(modelMatrix);
    glm::vec4 localPos = invModel * glm::vec4(worldPos, 1.0f);
    return glm::vec3(localPos);
}
```

### Экранные → мировые (Ray casting)

```cpp
glm::vec3 screenToWorldRay(
    glm::vec2 screenPos,      // Позиция мыши в пикселях
    glm::vec2 screenSize,     // Размер экрана
    glm::mat4 viewMatrix,
    glm::mat4 projMatrix
) {
    // Нормализованные координаты устройства (NDC)
    glm::vec2 ndc(
        (2.0f * screenPos.x) / screenSize.x - 1.0f,
        1.0f - (2.0f * screenPos.y) / screenSize.y
    );

    // Clip-координаты (z = -1 для ближней плоскости)
    glm::vec4 clipCoords(ndc.x, ndc.y, -1.0f, 1.0f);

    // Eye-координаты
    glm::mat4 invProj = glm::inverse(projMatrix);
    glm::vec4 eyeCoords = invProj * clipCoords;
    eyeCoords = glm::vec4(eyeCoords.x, eyeCoords.y, -1.0f, 0.0f);

    // World-координаты
    glm::mat4 invView = glm::inverse(viewMatrix);
    glm::vec4 worldDir = invView * eyeCoords;

    return glm::normalize(glm::vec3(worldDir));
}
```

---

## Проверки и утилиты

### Проверка видимости (Frustum culling)

```cpp
bool isPointInFrustum(glm::vec3 point, const glm::mat4& vpMatrix) {
    glm::vec4 clip = vpMatrix * glm::vec4(point, 1.0f);

    // Проверка NDC границ: [-1, 1] для OpenGL, [0, 1] для Vulkan (z)
    return (clip.x >= -clip.w && clip.x <= clip.w) &&
           (clip.y >= -clip.w && clip.y <= clip.w) &&
           (clip.z >= 0.0f    && clip.z <= clip.w);  // Vulkan: [0, 1]
}
```

### Расстояние до плоскости

```cpp
float distanceToPlane(glm::vec3 point, glm::vec3 planePoint, glm::vec3 planeNormal) {
    return glm::dot(point - planePoint, planeNormal);
}
```

### Проекция точки на плоскость

```cpp
glm::vec3 projectPointOnPlane(glm::vec3 point, glm::vec3 planePoint, glm::vec3 planeNormal) {
    float distance = glm::dot(point - planePoint, planeNormal);
    return point - planeNormal * distance;
}
```

### Отражение вектора скорости

```cpp
glm::vec3 reflectVelocity(glm::vec3 velocity, glm::vec3 normal, float restitution = 1.0f) {
    return velocity - (1.0f + restitution) * glm::dot(velocity, normal) * normal;
}

---

## Производительность GLM

<!-- anchor: 06_performance -->


Оптимизации GLM: SIMD, выравнивание, организация данных.

## SIMD оптимизации

GLM поддерживает автоматическое использование SIMD инструкций (SSE, AVX).

### Включение SIMD

```cpp
// До включения заголовков GLM:
#define GLM_FORCE_INTRINSICS    // Включить SIMD
#define GLM_FORCE_SSE2          // Минимум SSE2
// Или: GLM_FORCE_AVX, GLM_FORCE_AVX2

#include <glm/glm.hpp>
```

### Автоматическое определение

Без явного указания GLM автоматически определяет доступные инструкции:

```cpp
// GLM автоматически использует SSE2/AVX если доступно
// При GLM_FORCE_INTRINSICS
```

### Проверка SIMD в runtime

```cpp
#include <glm/glm.hpp>
#include <iostream>

int main() {
#ifdef GLM_FORCE_SSE2
    std::cout << "SSE2 включен" << std::endl;
#endif
#ifdef GLM_FORCE_AVX
    std::cout << "AVX включен" << std::endl;
#endif
    return 0;
}
```

---

## Выравнивание данных

### Упакованные vs выровненные типы

По умолчанию GLM использует **упакованные (packed)** типы:

```cpp
struct PackedData {
    glm::vec3 a;  // 12 байт
    float b;      // 4 байт
    glm::vec3 c;  // 12 байт
    // Всего: 28 байт (без padding)
};
```

Для SIMD нужны **выровненные (aligned)** типы:

```cpp
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>

struct AlignedData {
    glm::vec3 a;  // 16 байт (выравнивание)
    float b;      // 4 байт
    glm::vec3 c;  // 16 байт (выравнивание)
    // Всего: 36 байт
};
```

### Ручное управление выравниванием

```cpp
#define GLM_FORCE_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/type_aligned.hpp>

using vec4a = glm::aligned_vec4;  // Выровненный (16 байт)
using vec4p = glm::packed_vec4;   // Упакованный (16 байт для vec4)
```

### alignas для структур

```cpp
struct AlignedUBO {
    alignas(16) glm::mat4 model;      // 64 байта
    alignas(16) glm::vec4 color;      // 16 байт
    alignas(16) glm::vec3 position;   // 16 байт (vec3 → vec4 для GPU)
};
```

---

## Организация данных

GLM-типы можно использовать в обеих схемах:

```cpp
// AoS — простой код, но хуже локальность кэша
struct ParticleAoS {
    glm::vec3 position;
    glm::vec3 velocity;
    float lifetime;
};

// SoA — лучше локальность кэша для выборочной обработки
struct ParticleSoA {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> velocities;
    std::vector<float> lifetimes;
};
```

Для массовых вычислений SoA предпочтительнее — SIMD работает эффективнее с последовательными данными.

---

## Относительная стоимость операций

| Операция                 | Относительное время | Оптимизация                       |
|--------------------------|---------------------|-----------------------------------|
| `dot(a, b)`              | 1.0×                | Базовая                           |
| `normalize(v)`           | 1.0×                | Используйте SIMD                  |
| `cross(a, b)`            | 1.2×                | Избегайте в горячих путях         |
| `translate/rotate/scale` | 2.0×                | Кэшируйте матрицы                 |
| `perspective/lookAt`     | 3.0×                | Вычисляйте только при изменении   |
| `inverse(m)`             | 4.0×                | Избегайте в runtime               |
| `slerp(q1, q2, t)`       | 2.5×                | Используйте `mix` для приближения |

---

## Кэширование матриц

### Предвычисление постоянных матриц

```cpp
class Camera {
    glm::mat4 viewMatrix;
    glm::mat4 projMatrix;
    glm::mat4 viewProjMatrix;  // Кэшированное произведение
    bool viewDirty = true;
    bool projDirty = true;

public:
    void setPosition(glm::vec3 pos) {
        position = pos;
        viewDirty = true;
    }

    void setFOV(float fov) {
        this->fov = fov;
        projDirty = true;
    }

    glm::mat4 getViewProjMatrix() {
        if (viewDirty) {
            viewMatrix = glm::lookAt(position, target, up);
            viewDirty = false;
            viewProjMatrix = projMatrix * viewMatrix;  // Пересчитать
        }
        if (projDirty) {
            projMatrix = glm::perspective(fov, aspect, near, far);
            projDirty = false;
            viewProjMatrix = projMatrix * viewMatrix;  // Пересчитать
        }
        return viewProjMatrix;
    }
};
```

### Грязный флаг для иерархий

```cpp
class Transform {
    glm::mat4 localMatrix;
    glm::mat4 worldMatrix;
    Transform* parent = nullptr;
    bool dirty = true;

public:
    void setLocalPosition(glm::vec3 pos) {
        localMatrix = glm::translate(glm::mat4(1.0f), pos);
        markDirty();
    }

    glm::mat4 getWorldMatrix() {
        if (dirty) {
            if (parent) {
                worldMatrix = parent->getWorldMatrix() * localMatrix;
            } else {
                worldMatrix = localMatrix;
            }
            dirty = false;
        }
        return worldMatrix;
    }

    void markDirty() {
        dirty = true;
        // Уведомить детей...
    }
};
```

---

## Пакетная обработка

### Трансформация массива векторов

```cpp
void transformArray(
    const std::vector<glm::vec3>& input,
    const glm::mat4& matrix,
    std::vector<glm::vec3>& output
) {
    output.resize(input.size());

    // SIMD автоматически применяется в GLM
    for (size_t i = 0; i < input.size(); ++i) {
        output[i] = glm::vec3(matrix * glm::vec4(input[i], 1.0f));
    }
}
```

### Вычисление нормалей

```cpp
void computeNormals(
    const std::vector<glm::vec3>& vertices,
    const std::vector<uint32_t>& indices,
    std::vector<glm::vec3>& normals
) {
    normals.assign(vertices.size(), glm::vec3(0.0f));

    // Накопление нормалей по граням
    for (size_t i = 0; i < indices.size(); i += 3) {
        glm::vec3 v0 = vertices[indices[i]];
        glm::vec3 v1 = vertices[indices[i + 1]];
        glm::vec3 v2 = vertices[indices[i + 2]];

        glm::vec3 normal = glm::cross(v1 - v0, v2 - v0);

        normals[indices[i]]     += normal;
        normals[indices[i + 1]] += normal;
        normals[indices[i + 2]] += normal;
    }

    // Нормализация
    for (auto& n : normals) {
        n = glm::normalize(n);
    }
}
```

---

## Быстрые приближения

### Быстрый обратный квадратный корень

```cpp
// GTX расширение (экспериментальное)
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/fast_square_root.hpp>

float fastInvSqrt = glm::fastInverseSqrt(length2);  // Быстрее, но менее точно
```

### Длина без квадратного корня

```cpp
// Сравнение расстояний без sqrt
float dist2 = glm::distance2(a, b);  // Квадрат расстояния
if (dist2 < radius * radius) {
    // Внутри сферы
}
```

### Приближённый slerp

```cpp
// mix вместо slerp для небольших углов
glm::quat fastSlerp = glm::mix(q1, q2, t);  // NLERP: быстрее, менее точно
glm::quat accurateSlerp = glm::slerp(q1, q2, t);  // SLERP: медленнее, точно
```

---

## Рекомендации

| Ситуация                  | Рекомендация                            |
|---------------------------|-----------------------------------------|
| Массовые вычисления       | SoA, SIMD, пакетная обработка           |
| Частые матричные операции | Кэширование, грязные флаги              |
| Сравнение расстояний      | `distance2` вместо `distance`           |
| Повороты каждый кадр      | Кватернионы, `mix` вместо `slerp`       |
| Передача в GPU            | `value_ptr`, выравнивание `alignas(16)` |
| Много объектов            | Инстансинг, массивы матриц              |

---

## Решение проблем GLM

<!-- anchor: 07_troubleshooting -->


Частые ошибки и способы их исправления.

## Ошибки компиляции

### Не найдены `translate`, `rotate`, `perspective`

**Причина:** Не подключен заголовок расширений.

**Решение:**

```cpp
#include <glm/gtc/matrix_transform.hpp>
```

### Не найдены `quat`, `slerp`

**Причина:** Кватернионы находятся в отдельном модуле.

**Решение:**

```cpp
#include <glm/gtc/quaternion.hpp>
```

### Ошибки с макросами `min` / `max` (Windows)

**Причина:** Конфликт с `<windows.h>`.

**Решение:** Определить `NOMINMAX` перед включением системных заголовков:

```cpp
#define NOMINMAX
#include <windows.h>

#include <glm/glm.hpp>
```

### Не найдены функции GTX

**Причина:** Экспериментальные расширения требуют отдельного макроса.

**Решение:**

```cpp
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
```

### Долгая компиляция

**Причина:** Использование `<glm/glm.hpp>` или `<glm/ext.hpp>` подключает всё.

**Решение:** Использовать разделённые заголовки:

```cpp
// Вместо:
#include <glm/glm.hpp>

// Используйте:
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
```

---

## Ошибки при выполнении

### Изображение перевёрнуто

**Причина:** Различие систем координат OpenGL и Vulkan.

**Решение:**

```cpp
// Для Vulkan (до включения glm)
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
```

Или инвертируйте Y в матрице проекции:

```cpp
glm::mat4 proj = glm::perspective(fov, aspect, near, far);
proj[1][1] *= -1;  // Инвертировать Y
```

### Проблемы с выравниванием (std140)

**Симптом:** Данные в шейдере читаются некорректно.

**Причина:** В `std140` `vec3` занимает 16 байт (как `vec4`), а в C++ — 12 байт.

**Решение:**

```cpp
// Неправильно:
struct UBO {
    glm::vec3 pos;    // 12 байт в C++, но 16 в std140
    float scale;
};

// Правильно:
struct UBO {
    alignas(16) glm::vec3 pos;  // 16 байт с выравниванием
    float scale;
};

// Или используйте vec4:
struct UBO {
    glm::vec4 pos;    // 16 байт
    float scale;
};
```

### Матрицы передаются неправильно

**Симптом:** Объекты отображаются некорректно.

**Причина:** Неправильный порядок умножения.

**Решение:**

```cpp
// Правильный порядок: proj * view * model
glm::mat4 mvp = proj * view * model;

// Неправильно:
glm::mat4 mvp = model * view * proj;  // Ошибка!
```

### NaN в результатах

**Симптом:** Появление NaN в вычислениях.

**Причины и решения:**

1. **Нормализация нулевого вектора:**

```cpp
glm::vec3 zero(0, 0, 0);
glm::vec3 normalized = glm::normalize(zero);  // NaN!

// Решение: проверка длины
if (glm::length(v) > 0.0001f) {
    normalized = glm::normalize(v);
}
```

2. **Обратная матрица вырожденной матрицы:**

```cpp
glm::mat4 singular;  // Нулевая матрица
glm::mat4 inv = glm::inverse(singular);  // NaN!

// Решение: проверка определителя
if (glm::determinant(m) > epsilon) {
    inv = glm::inverse(m);
}
```

3. **Деление на ноль:**

```cpp
float result = a / b;  // Если b == 0, NaN

// Решение:
float result = (std::abs(b) > epsilon) ? a / b : 0.0f;
```

---

## Проблемы производительности

### Медленные матричные операции

**Причина:** Не включены SIMD оптимизации.

**Решение:**

```cpp
#define GLM_FORCE_INTRINSICS
#define GLM_FORCE_SSE2  // или AVX, AVX2

#include <glm/glm.hpp>
```

### Частые аллокации

**Причина:** Создание временных объектов в цикле.

**Решение:** Выносите вычисления за циклы, используйте ссылочные параметры:

```cpp
// Плохо:
for (auto& obj : objects) {
    glm::mat4 mvp = proj * view * obj.model;  // Временные объекты
}

// Лучше:
glm::mat4 vp = proj * view;
for (auto& obj : objects) {
    glm::mat4 mvp = vp * obj.model;
}
```

---

## Проблемы совместимости

### Разные результаты на разных платформах

**Причина:** Различия в реализации SIMD.

**Решение:** Явно указывайте точность:

```cpp
#define GLM_FORCE_PRECISION_HIGHP_FLOAT
#include <glm/glm.hpp>
```

### Ошибки с C++20/23 модулями

**Причина:** GLM не полностью поддерживает модули.

**Решение:** Используйте традиционные `#include`, или дождитесь официальной поддержки.

---

## Отладочные техники

### Вывод значений

```cpp
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

glm::vec3 v(1, 2, 3);
std::cout << glm::to_string(v) << std::endl;

glm::mat4 m(1.0f);
std::cout << glm::to_string(m) << std::endl;
```

### Проверка матрицы

```cpp
bool isIdentity(const glm::mat4& m, float epsilon = 0.0001f) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float expected = (i == j) ? 1.0f : 0.0f;
            if (std::abs(m[i][j] - expected) > epsilon) {
                return false;
            }
        }
    }
    return true;
}
```

### Проверка ортогональности

```cpp
bool isOrthonormal(const glm::mat4& m, float epsilon = 0.0001f) {
    glm::mat4 identity = m * glm::transpose(m);
    return isIdentity(identity, epsilon);
}
```

---

## Быстрая диагностика

| Симптом                       | Вероятная причина            | Решение                                   |
|-------------------------------|------------------------------|-------------------------------------------|
| `translate` не найден         | Нет заголовка GTC            | `#include <glm/gtc/matrix_transform.hpp>` |
| Изображение перевёрнуто       | Диапазон Z                   | `GLM_FORCE_DEPTH_ZERO_TO_ONE`             |
| NaN в результатах             | Деление на ноль              | Проверяйте входные данные                 |
| Неправильные данные в шейдере | Выравнивание std140          | `alignas(16)` или `vec4`                  |
| Долгая компиляция             | Глобальные заголовки         | Разделённые заголовки                     |
| Конфликт с `windows.h`        | Макросы min/max              | `#define NOMINMAX`                        |
| GTX функции недоступны        | Экспериментальные расширения | `#define GLM_ENABLE_EXPERIMENTAL`         |

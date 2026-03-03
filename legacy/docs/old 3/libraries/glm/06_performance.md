# Производительность GLM

🔴 **Уровень 3: Продвинутый**

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

> **Примечание:** Подробное объяснение SoA vs AoS и влияние на кэш процессора см.
> в [docs/guides/cpp/05_dod-practice.md](../../guides/cpp/05_dod-practice.md).

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

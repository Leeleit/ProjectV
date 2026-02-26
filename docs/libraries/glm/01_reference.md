# GLM: Математическая библиотека для графики

> **Для понимания:** GLM — это "переводчик" с языка математики на язык C++. Как GLSL (язык шейдеров) описывает векторы и
> матрицы, так и GLM делает это в C++. Представьте, что вы строите 3D-дом: GLM даёт вам чертежи (векторы — направления,
> матрицы — преобразования), а Vulkan строит по ним.

**GLM (OpenGL Mathematics)** — header-only математическая библиотека для C++, повторяющая типы и функции GLSL.

## Основные возможности

- **GLSL-совместимые типы** — vec2/3/4, mat2/3/4, кватернионы
- **Функции трансформации** — translate, rotate, scale, perspective, lookAt
- **Математические функции** — dot, cross, normalize, inverse, determinant
- **Header-only** — не требует компиляции
- **Без внешних зависимостей**

## Структура библиотеки

```
glm/
├── core/       # Ядро: GLSL-подобные типы
├── ext/        # Стабильные расширения
├── gtc/        # Рекомендуемые расширения (стабильные)
└── gtx/        # Экспериментальные расширения
```

## Основные типы

### Векторы

| Тип    | Компоненты | Использование                     |
|--------|------------|-----------------------------------|
| `vec2` | x, y       | UV-координаты, размеры            |
| `vec3` | x, y, z    | Позиции, направления, нормали     |
| `vec4` | x, y, z, w | Цвета RGBA, однородные координаты |

### Матрицы

| Тип    | Размер | Использование                  |
|--------|--------|--------------------------------|
| `mat2` | 2×2    | 2D преобразования              |
| `mat3` | 3×3    | Повороты/масштабы без переноса |
| `mat4` | 4×4    | Полные 3D преобразования       |

### Кватернионы

| Тип    | Использование            |
|--------|--------------------------|
| `quat` | Повороты без gimbal lock |

## Создание и доступ

```cpp
glm::vec3 v(1.0f, 2.0f, 3.0f);
float x = v.x;       // 1.0f
float y = v[1];      // 2.0f (индексация)
float z = v.r;       // 3.0f (альтернативно: r, g, b, a)
```

## Базовые операции

```cpp
glm::vec3 a(1, 2, 3), b(4, 5, 6);

glm::vec3 sum = a + b;              // (5, 7, 9)
glm::vec3 diff = a - b;             // (-3, -3, -3)
glm::vec3 scaled = a * 2.0f;        // (2, 4, 6)

float dotProduct = glm::dot(a, b);  // 32
glm::vec3 crossProd = glm::cross(a, b);
float length = glm::length(a);
glm::vec3 normalized = glm::normalize(a);
```

## Три основные матрицы

### Model (модельная)

```cpp
glm::mat4 model = glm::translate(glm::mat4(1.0f), position)
                * glm::rotate(glm::mat4(1.0f), angle, axis)
                * glm::scale(glm::mat4(1.0f), scale);
```

### View (видовая)

```cpp
glm::mat4 view = glm::lookAt(cameraPos, target, glm::vec3(0, 1, 0));
```

### Projection (проекционная)

```cpp
// Перспективная
glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

// Ортографическая
glm::mat4 ortho = glm::ortho(left, right, bottom, top, near, far);
```

## Порядок операций

`MVP = Projection * View * Model`

```cpp
glm::mat4 mvp = proj * view * model;
glm::vec4 finalPos = mvp * vertex;
```

## Кватернионы

```cpp
#include <glm/gtc/quaternion.hpp>

// Из угла и оси
glm::quat q = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0));

// Интерполяция
glm::quat slerped = glm::slerp(q1, q2, t);

// В матрицу
glm::mat4 rotation = glm::mat4_cast(q);
```

## Конфигурация для Vulkan

```cpp
#define GLM_FORCE_DEPTH_ZERO_TO_ONE  // Vulkan диапазон [0, 1]
#define GLM_FORCE_LEFT_HANDED         // Левая система координат
#define GLM_FORCE_RADIANS             // Все углы в радианах
```

## Глоссарий

### vec2, vec3, vec4

Векторы из 2, 3, 4 компонент.

### mat4

Матрица 4×4 для преобразования вершин.

### quat

Кватернион — представление поворота.

### MVP

Model-View-Projection: комбинация трёх матриц.

### Column-major

Порядок хранения матрицы по столбцам (как в GLSL).

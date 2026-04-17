# Основные понятия GLM

🟡 **Уровень 2: Средний**

Краткое введение в математику для графики с использованием GLM.

## Вектор (vec2, vec3, vec4)

Вектор — набор из 2, 3 или 4 чисел. Векторы в GLM хранят компоненты последовательно в памяти, поддерживают SIMD
оптимизации.

### Основные типы векторов

| Тип                       | Компоненты | Использование                                |
|---------------------------|------------|----------------------------------------------|
| `vec2`                    | x, y       | UV-координаты, размеры экрана                |
| `vec3`                    | x, y, z    | Позиции, направления, цвета RGB, нормали     |
| `vec4`                    | x, y, z, w | Цвета с альфой (RGBA), однородные координаты |
| `ivec2`, `ivec3`, `ivec4` | int        | Индексы, целочисленные координаты            |
| `uvec2`, `uvec3`, `uvec4` | unsigned   | Хеши, флаги, беззнаковые индексы             |
| `dvec2`, `dvec3`, `dvec4` | double     | Высокая точность                             |

### Создание и доступ

```cpp
glm::vec3 v(1.0f, 2.0f, 3.0f);
float x = v.x;      // 1.0f
float y = v[1];     // 2.0f (индексация с 0)
float z = v.r;      // 3.0f (альтернативные имена: r, g, b, a)
```

### Базовые операции

```cpp
glm::vec3 a(1, 2, 3), b(4, 5, 6);

glm::vec3 sum = a + b;              // (5, 7, 9)
glm::vec3 diff = a - b;             // (-3, -3, -3)
glm::vec3 scaled = a * 2.0f;        // (2, 4, 6)
glm::vec3 componentMul = a * b;     // (4, 10, 18) поэлементно

float dotProduct = glm::dot(a, b);  // 32 (скалярное произведение)
glm::vec3 crossProd = glm::cross(a, b);  // Векторное произведение
float length = glm::length(a);      // ≈ 3.741
glm::vec3 normalized = glm::normalize(a);  // Длина = 1
```

---

## Матрица (mat4)

Матрица 4×4 кодирует геометрическое преобразование: перенос, поворот, масштаб. В GLM матрицы хранятся в column-major
порядке.

### Типы матриц

| Тип     | Размер       | Использование                                |
|---------|--------------|----------------------------------------------|
| `mat2`  | 2×2          | 2D преобразования                            |
| `mat3`  | 3×3          | Повороты/масштабирования в 3D (без переноса) |
| `mat4`  | 4×4          | Полные 3D преобразования                     |
| `dmat4` | 4×4 (double) | Высокая точность                             |

### Три основных матрицы в графике

**1. Model (модельная)** — переход из локальных координат объекта в мировые:

```cpp
glm::mat4 model = glm::translate(glm::mat4(1.0f), position)
                * glm::rotate(glm::mat4(1.0f), angle, axis)
                * glm::scale(glm::mat4(1.0f), scale);
```

**2. View (видовая)** — переход из мировых координат в пространство камеры:

```cpp
glm::mat4 view = glm::lookAt(cameraPos, target, glm::vec3(0, 1, 0));
```

**3. Projection (проекционная)** — проекция 3D пространства на 2D экран:

```cpp
// Перспективная проекция
glm::mat4 proj = glm::perspective(glm::radians(45.0f), 16.0f/9.0f, 0.1f, 100.0f);

// Ортографическая проекция
glm::mat4 ortho = glm::ortho(0.0f, 1920.0f, 0.0f, 1080.0f, 0.1f, 100.0f);
```

---

## Порядок операций

Стандартная формула: `MVP = Projection * View * Model`

```cpp
glm::mat4 mvp = proj * view * model;          // Обратный порядок умножения!
glm::vec4 vertex = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
glm::vec4 finalPos = mvp * vertex;            // Умножение справа
```

### Почему такой порядок

1. Умножаем вершину на Model (получаем мировую позицию)
2. Умножаем на View (позиция относительно камеры)
3. Умножаем на Projection (координаты на экране)

---

## Радианы

GLM использует радианы для всех углов.

```cpp
float degrees = 45.0f;
float radians = glm::radians(degrees);  // 0.785398... (π/4)
float backToDegrees = glm::degrees(radians);  // 45.0f

// ПРАВИЛЬНО: угол в радианах
glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0, 1, 0));
```

---

## Кватернион (quat)

Кватернион — компактное представление поворота (x, y, z, w).

### Преимущества перед углами Эйлера

- Нет проблемы "складывания рамок" (gimbal lock)
- Плавная интерполяция (slerp)
- Эффективная композиция поворотов

### Создание кватернионов

```cpp
#include <glm/gtc/quaternion.hpp>

// Из угла и оси (угол в радианах)
glm::quat q1 = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0));

// Из углов Эйлера (yaw, pitch, roll)
glm::quat q2 = glm::quat(glm::vec3(
    glm::radians(30.0f),
    glm::radians(45.0f),
    0.0f
));

// Идентичный кватернион (без поворота)
glm::quat identity = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
```

### Операции с кватернионами

```cpp
glm::quat a = ..., b = ...;

// Композиция поворотов (справа налево)
glm::quat combined = b * a;  // сначала a, потом b

// Векторный поворот
glm::vec3 point(1, 0, 0);
glm::vec3 rotated = a * point;

// Интерполяция
float t = 0.5f;
glm::quat lerped = glm::mix(a, b, t);     // Линейная (быстро)
glm::quat slerped = glm::slerp(a, b, t);  // Сферическая (корректно)

// Конвертация в матрицу
glm::mat4 rotationMatrix = glm::mat4_cast(a);
```

---

## Системы координат

### Правая vs левая

- **Правая (OpenGL)**: Большой палец = X, указательный = Y, средний = Z
- **Левая (DirectX)**: Большой палец = X, указательный = Y, средний = -Z

Переключение: `#define GLM_FORCE_LEFT_HANDED` перед включением GLM.

### Диапазон глубины (Depth Range)

- **OpenGL**: [-1, 1]
- **Vulkan**: [0, 1]

Переключение: `#define GLM_FORCE_DEPTH_ZERO_TO_ONE` для Vulkan.

---

## Column-major порядок

GLM следует GLSL: матрицы хранятся по столбцам (column-major).

```cpp
glm::mat4 model = ...;
glm::vec4 point = ...;
glm::vec4 transformed = model * point;  // matrix * vector (column-major)
```

При передаче в API через `value_ptr` данные уже в правильном порядке.

---

## Общая схема

```
Данные:
vec3 → позиции, направления, цвета
mat4 → преобразования (model, view, projection)
quat → повороты

Вычисления:
vertex → model → view → projection → NDC

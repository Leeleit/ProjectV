# Основные понятия

**🟡 Уровень 2: Средний**

Краткое введение в математику для графики с использованием glm. Примеры кода: [docs/examples/](../../examples/).

---

## Вектор (vec2, vec3, vec4)

**Вектор** — набор из 2, 3 или 4 чисел (`float`). Векторы в glm хранят компоненты последовательно в памяти, поддерживают
SIMD оптимизации.

### Основные типы векторов

| Тип                       | Компоненты | Использование                                           | Пример создания                            |
|---------------------------|------------|---------------------------------------------------------|--------------------------------------------|
| `vec2`                    | x, y       | UV-координаты текстур, размеры экрана                   | `glm::vec2 uv(0.5f, 0.5f);`                |
| `vec3`                    | x, y, z    | Позиции в пространстве, направления, цвета RGB, нормали | `glm::vec3 position(1.0f, 2.0f, 3.0f);`    |
| `vec4`                    | x, y, z, w | Цвета с альфой (RGBA), однородные координаты, плоскости | `glm::vec4 color(1.0f, 0.0f, 0.0f, 1.0f);` |
| `ivec2`, `ivec3`, `ivec4` | int        | Индексы, координаты вокселей, целочисленные размеры     | `glm::ivec3 voxelCoord(10, 20, 30);`       |
| `uvec2`, `uvec3`, `uvec4` | unsigned   | Хеши, флаги, беззнаковые индексы                        | `glm::uvec2 textureSize(1024, 1024);`      |
| `bvec2`, `bvec3`, `bvec4` | bool       | Маски, результаты сравнений                             | `glm::bvec3 mask(true, false, true);`      |
| `dvec2`, `dvec3`, `dvec4` | double     | Высокая точность (научные расчёты)                      | `glm::dvec3 precise(1e-10, 2e-10, 3e-10);` |

### Доступ к компонентам

```cpp
glm::vec3 v(1.0f, 2.0f, 3.0f);
float x = v.x;          // 1.0f
float y = v[1];         // 2.0f (индексация с 0)
float z = v.r;          // 3.0f (альтернативные имена: r,g,b,a)
float w = v.s;          // для vec4: s,t,p,q

// Swizzle (требует GLM_FORCE_SWIZZLE)
// glm::vec3 swizzled = v.zyx; // (3.0f, 2.0f, 1.0f)
```

### Базовые операции

```cpp
glm::vec3 a(1, 2, 3), b(4, 5, 6);

glm::vec3 sum = a + b;              // (5, 7, 9)
glm::vec3 diff = a - b;             // (-3, -3, -3)
glm::vec3 scaled = a * 2.0f;        // (2, 4, 6)
glm::vec3 componentMul = a * b;     // (4, 10, 18) поэлементно
float dotProduct = glm::dot(a, b);  // 1*4 + 2*5 + 3*6 = 32
glm::vec3 crossProd = glm::cross(a, b); // Векторное произведение
float length = glm::length(a);      // √(1²+2²+3²) ≈ 3.741
glm::vec3 normalized = glm::normalize(a); // Длина = 1
```

**Важно:** glm следует соглашениям GLSL: операции выполняются поэлементно.

---

## Матрица (mat4)

**Матрица 4×4** кодирует геометрическое преобразование: перенос, поворот, масштаб. В glm матрицы хранятся в *
*column-major** порядке (как в OpenGL/Vulkan).

### Типы матриц

| Тип                     | Размер       | Использование                                    |
|-------------------------|--------------|--------------------------------------------------|
| `mat2`                  | 2×2          | 2D преобразования                                |
| `mat3`                  | 3×3          | Повороты/масштабирования в 3D (без переноса)     |
| `mat4`                  | 4×4          | Полные 3D преобразования (модель, вид, проекция) |
| `mat2x3`, `mat3x2`, ... | различные    | Специализированные преобразования                |
| `dmat4`                 | 4×4 (double) | Высокая точность                                 |

### Три основных матрицы в графике

1. **Model (модельная)** — переход из локальных координат объекта в мировые:
   ```cpp
   glm::mat4 model = glm::translate(glm::mat4(1.0f), position)
                   * glm::rotate(glm::mat4(1.0f), angle, axis)
                   * glm::scale(glm::mat4(1.0f), scale);
   ```

2. **View (видовая)** — переход из мировых координат в пространство камеры:
   ```cpp
   glm::mat4 view = glm::lookAt(cameraPos, target, glm::vec3(0, 1, 0));
   ```

3. **Projection (проекционная)** — проекция 3D пространства на 2D экран:
   ```cpp
   // Перспективная проекция
   glm::mat4 proj = glm::perspective(glm::radians(45.0f), 16.0f/9.0f, 0.1f, 100.0f);
   
   // Ортографическая проекция
   glm::mat4 ortho = glm::ortho(0.0f, 1920.0f, 0.0f, 1080.0f, 0.1f, 100.0f);
   ```

---

## Порядок операций

Стандартная формула: `MVP = Projection * View * Model`.

### Почему именно такой порядок?

1. Умножаем вершину на Model (получаем мировую позицию).
2. Умножаем на View (позиция относительно камеры).
3. Умножаем на Projection (координаты на экране).

В коде:

```cpp
glm::mat4 mvp = proj * view * model;          // Обратный порядок умножения!
glm::vec4 vertex = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
glm::vec4 finalPos = mvp * vertex;            // Умножение справа
```

**Важно:** Умножение матриц в C++ выполняется слева направо, поэтому порядок `proj * view * model` соответствует
`model → view → proj` при умножении на вектор справа.

### Полный пример трансформации

```cpp
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Исходная точка в локальных координатах объекта
glm::vec4 localPoint(0.0f, 0.5f, 0.0f, 1.0f);

// Матрицы преобразования
glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f));
glm::mat4 view = glm::lookAt(glm::vec3(0,0,5), glm::vec3(0,0,0), glm::vec3(0,1,0));
glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1.33f, 0.1f, 100.0f);

// Последовательное преобразование
glm::vec4 worldPoint = model * localPoint;     // В мировые координаты
glm::vec4 viewPoint = view * worldPoint;       // В координаты камеры
glm::vec4 clipPoint = proj * viewPoint;        // В clip space
// После деления на w получаем NDC (Normalized Device Coordinates)
glm::vec3 ndc = glm::vec3(clipPoint) / clipPoint.w;
```

---

## Радианы

glm использует **радианы** для всех углов. GLSL также работает с радианами.

### Конвертация

```cpp
float degrees = 45.0f;
float radians = glm::radians(degrees);  // 0.785398... (π/4)
float backToDegrees = glm::degrees(radians); // 45.0f
```

### Пример с поворотом

```cpp
// ПРАВИЛЬНО: угол в радианах
glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0,1,0));

// НЕПРАВИЛЬНО: угол в градусах (слишком большой поворот)
// glm::mat4 wrong = glm::rotate(glm::mat4(1.0f), 90.0f, glm::vec3(0,1,0));
```

---

## Кватернион (quat)

**Кватернион** — компактное представление поворота (x, y, z, w). Преимущества перед углами Эйлера:

- Нет проблемы "складывания рамок" (gimbal lock)
- Плавная интерполяция (slerp)
- Эффективная композиция поворотов

### Создание кватернионов

```cpp
#include <glm/gtc/quaternion.hpp>

// Из угла и оси (угол в радианах!)
glm::quat q1 = glm::angleAxis(glm::radians(90.0f), glm::vec3(0,1,0));

// Из углов Эйлера (yaw, pitch, roll)
glm::quat q2 = glm::quat(glm::vec3(glm::radians(30.0f), glm::radians(45.0f), 0.0f));

// Идентичный кватернион (без поворота)
glm::quat identity = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
```

### Операции с кватернионами

```cpp
glm::quat a = ..., b = ...;

// Композиция поворотов (справа налево)
glm::quat combined = b * a;  // сначала a, потом b

// Векторный поворот
glm::vec3 point(1,0,0);
glm::vec3 rotated = a * point;  // ротация точки

// Интерполяция
float t = 0.5f;
glm::quat lerped = glm::mix(a, b, t);     // Линейная интерполяция (быстро)
glm::quat slerped = glm::slerp(a, b, t);  // Сферическая интерполяция (корректно)

// Конвертация в матрицу
glm::mat4 rotationMatrix = glm::mat4_cast(a);
```

### Преимущества для воксельного движка

1. **Накопление поворотов** без потери точности
2. **Интерполяция анимаций** (slerp для плавного вращения камеры)
3. **Компактное хранение** (4 float вместо 9/16 для матрицы)

---

## Системы координат

### Правая vs левая

- **Правая (OpenGL)**: Большой палец = X, указательный = Y, средний = Z
- **Левая (DirectX)**: Большой палец = X, указательный = Y, средний = -Z

Переключение: `#define GLM_FORCE_LEFT_HANDED` перед включением glm.

### Диапазон глубины (Depth Range)

- **OpenGL**: [-1, 1] (глубина от -1 до 1)
- **Vulkan**: [0, 1] (глубина от 0 до 1)

Переключение: `#define GLM_FORCE_DEPTH_ZERO_TO_ONE` для Vulkan.

---

## См. также

- [Быстрый старт](quickstart.md) — практический пример.
- [Справочник API](api-reference.md) — полное описание функций.
- [Интеграция с Vulkan](integration.md) — использование glm в Vulkan (UBO, push constants).
- [Примеры кода](../../examples/) — `glm_transform.cpp`, `glm_voxel_chunk.cpp`.

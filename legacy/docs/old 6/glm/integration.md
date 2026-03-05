# Интеграция glm

**🟡 Уровень 2: Средний**

Пошаговое руководство по подключению и настройке glm с учётом производительности и особенностей C++ компиляции.

## Оглавление

- [1. CMake](#1-cmake)
- [2. Три способа подключения заголовков](#2-три-способа-подключения-заголовков)
- [3. Модули](#3-модули)
- [4. Макросы конфигурации](#4-макросы-конфигурации)
- [5. SIMD оптимизации и выравнивание](#5-simd-оптимизации-и-выравнивание)
- [6. Соглашения: column-major и handedness](#6-соглашения-column-major-и-handedness)
- [7. Интеграция с Vulkan (uniform-буферы)](#7-интеграция-с-vulkan-uniform-буферы)

---

## 1. CMake

### Добавление glm как подпроекта

```cmake
add_subdirectory(external/glm)
target_link_libraries(YourApp PRIVATE glm::glm)
```

glm — header-only библиотека: линкуется только интерфейс (include-пути).

### CMake-опции glm

Опции из `CMakeLists.txt` библиотеки:

| Опция                        | По умолчанию | Описание                                            |
|------------------------------|--------------|-----------------------------------------------------|
| `GLM_BUILD_LIBRARY`          | `ON`         | Сборка библиотеки (для header-only обычно не нужна) |
| `GLM_BUILD_TESTS`            | `OFF`        | Сборка тестов glm                                   |
| `GLM_ENABLE_CXX_17`          | `OFF`        | Принудительно C++17                                 |
| `GLM_ENABLE_SIMD_SSE2`       | `OFF`        | SIMD-оптимизации (требуют `GLM_FORCE_INTRINSICS`)   |
| `GLM_DISABLE_AUTO_DETECTION` | `OFF`        | Отключить автоопределение платформы                 |

**Совет:** Для максимальной производительности включите `GLM_ENABLE_SIMD_SSE2` (или AVX/AVX2) и используйте
`GLM_FORCE_DEFAULT_ALIGNED_GENTYPES`.

---

## 2. Три способа подключения заголовков

GLM предлагает три подхода для баланса удобства и времени компиляции.

### 2.1. Глобальные заголовки (быстро, но тяжёлая компиляция)

```cpp
#include <glm/glm.hpp>  // Ядро GLM (vec2/3/4, mat2/3/4, базовые функции)
#include <glm/ext.hpp>  // Все расширения (translate, rotate, perspective и др.)
```

**Плюсы:** Удобно, одна строка.
**Минусы:** Значительно увеличивает время компиляции, подключает весь код GLM.

### 2.2. Разделённые заголовки (баланс)

Подключайте только необходимые модули ядра GLSL:

```cpp
// Ядро GLSL
#include <glm/vec2.hpp>               // vec2
#include <glm/vec3.hpp>               // vec3
#include <glm/mat4x4.hpp>             // mat4
#include <glm/trigonometric.hpp>      // radians

// Расширения
#include <glm/ext/matrix_transform.hpp>     // translate, rotate, scale
#include <glm/ext/matrix_clip_space.hpp>    // perspective
```

**Преимущество:** Сокращает время компиляции на 30-50% по сравнению с глобальными заголовками.

### 2.3. Заголовки расширений (максимальная производительность компиляции)

Наиболее гранулярный подход — подключайте конкретные типы:

```cpp
// Конкретные типы векторов
#include <glm/ext/vector_float2.hpp>          // vec2
#include <glm/ext/vector_float3.hpp>          // vec3
#include <glm/ext/vector_trigonometric.hpp>   // radians

// Конкретные типы матриц
#include <glm/ext/matrix_float4x4.hpp>        // mat4
#include <glm/ext/matrix_transform.hpp>       // translate, rotate
#include <glm/ext/matrix_clip_space.hpp>      // perspective
```

**Преимущество:** Минимальное время компиляции, только используемый код.
**Недостаток:** Нужно знать точные пути заголовков.

---

## 3. Модули

GLM организована в три категории модулей:

### 3.1. Ядро (GLSL совместимость)

| Заголовок                           | Содержание                               |
|-------------------------------------|------------------------------------------|
| `glm/vec2.hpp`...`glm/vec4.hpp`     | Векторы (float, double, int, uint, bool) |
| `glm/mat2x2.hpp`...`glm/mat4x4.hpp` | Матрицы всех размеров                    |
| `glm/trigonometric.hpp`             | `radians`, `cos`, `sin`, `asin` и др.    |
| `glm/exponential.hpp`               | `pow`, `log`, `exp2`, `sqrt`             |
| `glm/geometric.hpp`                 | `dot`, `cross`, `normalize`, `reflect`   |
| `glm/matrix.hpp`                    | `transpose`, `inverse`, `determinant`    |
| `glm/common.hpp`                    | `min`, `max`, `mix`, `abs`, `sign`       |
| `glm/packing.hpp`                   | `packUnorm4x8`, `unpackHalf2x16`         |
| `glm/integer.hpp`                   | `findMSB`, `bitfieldExtract`             |

### 3.2. Стабильные расширения (GTC — рекомендованные)

| Заголовок                      | Содержание                                              |
|--------------------------------|---------------------------------------------------------|
| `glm/gtc/matrix_transform.hpp` | `translate`, `rotate`, `scale`, `perspective`, `lookAt` |
| `glm/gtc/type_ptr.hpp`         | `value_ptr`, `make_mat4`, `make_vec3`                   |
| `glm/gtc/quaternion.hpp`       | `quat`, `slerp`, `mat4_cast`, `eulerAngles`             |
| `glm/gtc/constants.hpp`        | `pi`, `epsilon`, `infinity`                             |
| `glm/gtc/random.hpp`           | `linearRand`, `gaussRand`, `diskRand`                   |

### 3.3. Экспериментальные расширения (GTX — нестабильные)

Требуют макрос `GLM_ENABLE_EXPERIMENTAL`:

```cpp
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>    // Дополнительные функции для кватернионов
#include <glm/gtx/euler_angles.hpp>  // Преобразования углов Эйлера
```

**Внимание:** API GTX может меняться между версиями GLM.

---

## 4. Макросы конфигурации

Определять **до** первого `#include` glm.

### 4.1. Системы координат и углы

| Макрос                        | Описание                             | Рекомендация для Vulkan   |
|-------------------------------|--------------------------------------|---------------------------|
| `GLM_FORCE_RADIANS`           | Использовать радианы для всех углов  | ✅ Обязательно             |
| `GLM_FORCE_DEPTH_ZERO_TO_ONE` | NDC по оси Z в [0, 1] (а не [-1, 1]) | ✅ Обязательно             |
| `GLM_FORCE_LEFT_HANDED`       | Левая система координат              | ⚠️ Обычно правая (OpenGL) |

### 4.2. Отладка и сообщения

| Макрос                      | Описание                                 |
|-----------------------------|------------------------------------------|
| `GLM_FORCE_MESSAGES`        | Выводить конфигурацию в лог сборки       |
| `GLM_FORCE_SILENT_WARNINGS` | Отключить warnings о language extensions |

### 4.3. Контроль точности

| Макрос                              | Тип     | Описание                        |
|-------------------------------------|---------|---------------------------------|
| `GLM_FORCE_PRECISION_HIGHP_FLOAT`   | `float` | Высокая точность (по умолчанию) |
| `GLM_FORCE_PRECISION_MEDIUMP_FLOAT` | `float` | Средняя точность                |
| `GLM_FORCE_PRECISION_LOWP_FLOAT`    | `float` | Низкая точность                 |
| `GLM_FORCE_PRECISION_HIGHP_INT`     | `int`   | Высокая точность для целых      |

### 4.4. Другие важные макросы

| Макрос                           | Описание                                                   |
|----------------------------------|------------------------------------------------------------|
| `GLM_FORCE_EXPLICIT_CTOR`        | Запретить неявные преобразования типов                     |
| `GLM_FORCE_SIZE_T_LENGTH`        | `.length()` возвращает `size_t` вместо `int`               |
| `GLM_FORCE_UNRESTRICTED_GENTYPE` | Разрешить смешанные типы в функциях                        |
| `GLM_FORCE_SWIZZLE`              | Включить swizzle-операторы (сильно замедляет компиляцию)   |
| `GLM_FORCE_XYZW_ONLY`            | Только компоненты x, y, z, w (без r, g, b, a и s, t, p, q) |

---

## 5. SIMD оптимизации и выравнивание

### 5.1. Включение SIMD

GLM автоматически определяет доступные инструкции процессора. Для явного указания:

```cpp
#define GLM_FORCE_INTRINSICS           // Включить intrinsics
#define GLM_FORCE_SSE2                 // или SSE3, SSSE3, SSE41, SSE42, AVX, AVX2, AVX512
#include <glm/glm.hpp>
```

**Важно:** SIMD требует выровненных типов (см. ниже).

### 5.2. Выровненные vs упакованные типы

По умолчанию GLM использует **упакованные (packed)** типы для экономии памяти:

```cpp
struct PackedData {
    glm::vec3 a;  // 12 байт
    float b;      // 4 байт
    glm::vec3 c;  // 12 байт
    // Всего: 28 байт (без padding)
};
```

Для SIMD оптимизаций нужны **выровненные (aligned)** типы:

```cpp
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>

struct AlignedData {
    glm::vec3 a;  // 12 байт + 4 байта padding
    float b;      // 4 байта
    glm::vec3 c;  // 12 байт + 4 байта padding
    // Всего: 36 байт
};
```

**Компромисс:** Выровненные типы быстрее благодаря SIMD, но используют больше памяти.

### 5.3. Ручное управление выравниванием

```cpp
#define GLM_FORCE_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/type_aligned.hpp>

typedef glm::aligned_vec4 vec4a;  // Выровненный
typedef glm::packed_vec4 vec4p;   // Упакованный
```

---

## 6. Соглашения: column-major и handedness

### 6.1. Column-major матрицы

GLM следует GLSL: матрицы хранятся по **столбцам (column-major)**. Умножение:

```cpp
glm::mat4 model = ...;
glm::vec4 point = ...;
glm::vec4 transformed = model * point;  // matrix * vector (column-major)
```

**Важно:** При передаче в Vulkan через `value_ptr` данные уже в правильном порядке.

### 6.2. Системы координат

- **По умолчанию:** Правая (right-handed) как в OpenGL
- **Для Vulkan:** Используйте `GLM_FORCE_DEPTH_ZERO_TO_ONE` для Z [0, 1]
- **Для DirectX:** Добавьте `GLM_FORCE_LEFT_HANDED`

---

## 7. Интеграция с Vulkan (uniform-буферы)

### 7.1. Передача матриц и векторов

```cpp
#include <glm/gtc/type_ptr.hpp>

glm::mat4 model = glm::mat4(1.0f);
glm::vec3 position = glm::vec3(1.0f, 2.0f, 3.0f);

// Копирование в отображённую память VMA
memcpy(mappedData, glm::value_ptr(model), sizeof(glm::mat4));
memcpy(mappedData + sizeof(glm::mat4), glm::value_ptr(position), sizeof(glm::vec3));
```

### 7.2. Выравнивание для std140 (UBO)

Стандарт `std140` имеет строгие правила выравнивания:

- `vec3` выравнивается как `vec4` (16 байт)
- Матрицы выравниваются по столбцам (каждый столбец как `vec4`)

**Правильное объявление структур:**

```cpp
struct UBO {
    alignas(16) glm::mat4 mvp;      // 64 байта, выравнивание 16
    alignas(16) glm::vec3 cameraPos; // 16 байт (не 12!)
    alignas(4)  float time;         // 4 байта
    // Итого: 84 байта, но Vulkan требует кратность 16 → 96 байт с padding
};
```

**Альтернатива:** Используйте `glm::vec4` вместо `glm::vec3` для избежания проблем.

### 7.3. Чтение данных из GPU

```cpp
float* gpuData = ...;  // Указатель на отображённую память

glm::mat4 model = glm::make_mat4(gpuData);
glm::vec3 position = glm::make_vec3(gpuData + 16);  // Пропустить mat4 (16 floats)
```

### 7.4. Push constants

Для push constants (малый объём данных, передаваемых напрямую в команду):

```cpp
struct PushConstants {
    alignas(16) glm::mat4 model;
    alignas(16) glm::vec4 color;
};

PushConstants pc;
pc.model = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f));
pc.color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                   0, sizeof(PushConstants), &pc);
```

### 7.5. Инстансинг с массивами матриц

Для рендеринга множества объектов с разными трансформациями:

```cpp
std::vector<glm::mat4> instanceMatrices;
// ... заполнение матриц

// Передача в storage buffer
VkDeviceSize bufferSize = instanceMatrices.size() * sizeof(glm::mat4);
// Используйте glm::value_ptr(instanceMatrices[0]) для получения указателя на данные
```

---

## Практические рекомендации

1. **Для Vulkan:** Используйте `GLM_FORCE_RADIANS` и `GLM_FORCE_DEPTH_ZERO_TO_ONE`
2. **Для производительности:** Включите SIMD и выровненные типы
3. **Для быстрой компиляции:** Используйте разделённые заголовки
4. **Для отладки:** Включите `GLM_FORCE_MESSAGES`
5. **Для Vulkan:** Всегда используйте `alignas(16)` для структур UBO

Полный список макросов см.
в [руководстве GLM](https://github.com/g-truc/glm/blob/master/manual.md#2-preprocessor-configurations).

# Интеграция GLM

🟡 **Уровень 2: Средний**

Подключение и настройка GLM: CMake, заголовки, макросы конфигурации.

## CMake

### Добавление как подпроекта

```cmake
add_subdirectory(external/glm)
target_link_libraries(YourApp PRIVATE glm::glm)
```

GLM — header-only библиотека: линкуется только интерфейс (include-пути).

### CMake-опции

| Опция                        | По умолчанию | Описание                                            |
|------------------------------|--------------|-----------------------------------------------------|
| `GLM_BUILD_LIBRARY`          | `ON`         | Сборка библиотеки (для header-only обычно не нужна) |
| `GLM_BUILD_TESTS`            | `OFF`        | Сборка тестов GLM                                   |
| `GLM_ENABLE_CXX_17`          | `OFF`        | Принудительно C++17                                 |
| `GLM_ENABLE_SIMD_SSE2`       | `OFF`        | SIMD-оптимизации (требуют `GLM_FORCE_INTRINSICS`)   |
| `GLM_DISABLE_AUTO_DETECTION` | `OFF`        | Отключить автоопределение платформы                 |

---

## Способы подключения заголовков

### Глобальные заголовки (удобно, но медленная компиляция)

```cpp
#include <glm/glm.hpp>  // Ядро GLM (vec2/3/4, mat2/3/4, базовые функции)
#include <glm/ext.hpp>  // Все расширения (translate, rotate, perspective и др.)
```

### Разделённые заголовки (баланс)

```cpp
// Ядро GLSL
#include <glm/vec2.hpp>               // vec2
#include <glm/vec3.hpp>               // vec3
#include <glm/mat4x4.hpp>             // mat4

// Расширения
#include <glm/ext/matrix_transform.hpp>     // translate, rotate, scale
#include <glm/ext/matrix_clip_space.hpp>    // perspective
```

### Гранулярные заголовки (максимальная скорость компиляции)

```cpp
#include <glm/ext/vector_float3.hpp>          // vec3
#include <glm/ext/matrix_float4x4.hpp>        // mat4
#include <glm/ext/matrix_transform.hpp>       // translate, rotate
```

---

## Модули GLM

### Ядро (GLSL совместимость)

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

### Стабильные расширения (GTC — рекомендованные)

| Заголовок                      | Содержание                                              |
|--------------------------------|---------------------------------------------------------|
| `glm/gtc/matrix_transform.hpp` | `translate`, `rotate`, `scale`, `perspective`, `lookAt` |
| `glm/gtc/type_ptr.hpp`         | `value_ptr`, `make_mat4`, `make_vec3`                   |
| `glm/gtc/quaternion.hpp`       | `quat`, `slerp`, `mat4_cast`, `eulerAngles`             |
| `glm/gtc/constants.hpp`        | `pi`, `epsilon`, `infinity`                             |
| `glm/gtc/random.hpp`           | `linearRand`, `gaussRand`, `diskRand`                   |

### Экспериментальные расширения (GTX — нестабильные)

Требуют макрос `GLM_ENABLE_EXPERIMENTAL`:

```cpp
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>    // Дополнительные функции для кватернионов
#include <glm/gtx/euler_angles.hpp>  // Преобразования углов Эйлера
```

---

## Макросы конфигурации

Определять **до** первого `#include` GLM.

### Системы координат и углы

| Макрос                        | Описание                             | Рекомендация для Vulkan |
|-------------------------------|--------------------------------------|-------------------------|
| `GLM_FORCE_RADIANS`           | Использовать радианы для всех углов  | Рекомендуется           |
| `GLM_FORCE_DEPTH_ZERO_TO_ONE` | NDC по оси Z в [0, 1] (а не [-1, 1]) | Обязательно для Vulkan  |
| `GLM_FORCE_LEFT_HANDED`       | Левая система координат              | Для DirectX             |

### Отладка и сообщения

| Макрос                      | Описание                                 |
|-----------------------------|------------------------------------------|
| `GLM_FORCE_MESSAGES`        | Выводить конфигурацию в лог сборки       |
| `GLM_FORCE_SILENT_WARNINGS` | Отключить warnings о language extensions |

### Контроль точности

| Макрос                              | Тип     | Описание                        |
|-------------------------------------|---------|---------------------------------|
| `GLM_FORCE_PRECISION_HIGHP_FLOAT`   | `float` | Высокая точность (по умолчанию) |
| `GLM_FORCE_PRECISION_MEDIUMP_FLOAT` | `float` | Средняя точность                |
| `GLM_FORCE_PRECISION_LOWP_FLOAT`    | `float` | Низкая точность                 |

### Другие важные макросы

| Макрос                           | Описание                                                   |
|----------------------------------|------------------------------------------------------------|
| `GLM_FORCE_EXPLICIT_CTOR`        | Запретить неявные преобразования типов                     |
| `GLM_FORCE_SIZE_T_LENGTH`        | `.length()` возвращает `size_t` вместо `int`               |
| `GLM_FORCE_UNRESTRICTED_GENTYPE` | Разрешить смешанные типы в функциях                        |
| `GLM_FORCE_SWIZZLE`              | Включить swizzle-операторы (сильно замедляет компиляцию)   |
| `GLM_FORCE_XYZW_ONLY`            | Только компоненты x, y, z, w (без r, g, b, a и s, t, p, q) |

---

## Пример конфигурации для Vulkan

```cpp
// Конфигурация GLM для Vulkan (до включения заголовков)
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_MESSAGES  // Опционально: вывод конфигурации при сборке

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
```

---

## Передача данных в GPU

### value_ptr для uniform-буферов

```cpp
#include <glm/gtc/type_ptr.hpp>

glm::mat4 model = glm::mat4(1.0f);
glm::vec3 position = glm::vec3(1.0f, 2.0f, 3.0f);

// Копирование в отображённую память
memcpy(mappedData, glm::value_ptr(model), sizeof(glm::mat4));
memcpy(mappedData + sizeof(glm::mat4), glm::value_ptr(position), sizeof(glm::vec3));
```

### make_* для чтения из GPU

```cpp
float* gpuData = ...;  // Указатель на отображённую память

glm::mat4 model = glm::make_mat4(gpuData);
glm::vec3 position = glm::make_vec3(gpuData + 16);  // Пропустить mat4 (16 floats)
```

---

## Выравнивание для std140

Стандарт `std140` имеет строгие правила выравнивания:

- `vec3` выравнивается как `vec4` (16 байт)
- Матрицы выравниваются по столбцам (каждый столбец как `vec4`)

```cpp
struct UBO {
    alignas(16) glm::mat4 mvp;       // 64 байта, выравнивание 16
    alignas(16) glm::vec3 cameraPos; // 16 байт (не 12!)
    alignas(4)  float time;          // 4 байта
};
```

**Альтернатива:** Используйте `glm::vec4` вместо `glm::vec3` для избежания проблем.

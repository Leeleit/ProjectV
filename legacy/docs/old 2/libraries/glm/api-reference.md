# Справочник API GLM

**🟡 Уровень 2: Средний**

Краткое описание основных функций и типов GLM (OpenGL Mathematics). GLM — header-only библиотека математики для графики,
совместимая с GLSL. Полный список функций: [manual.md](https://github.com/g-truc/glm/blob/master/manual.md) (
внешний), [Конфигурация макросов](integration.md#макросы-конфигурации). Примеры кода: [docs/examples/](../../examples/).

## Карта заголовков

| Файл                                      | Назначение                                                                                                  | Основные функции                                       |
|-------------------------------------------|-------------------------------------------------------------------------------------------------------------|--------------------------------------------------------|
| `<glm/glm.hpp>`                           | Всё ядро GLSL + расширения                                                                                  | `vec2`, `mat4`, `radians`, `normalize`, `dot`, `cross` |
| `<glm/ext.hpp>`                           | Все стабильные расширения                                                                                   | `perspective`, `translate`, `rotate`, `lookAt`         |
| **Векторы**                               |                                                                                                             |                                                        |
| `<glm/vec2.hpp>`                          | vec2 (float), ivec2, uvec2, bvec2                                                                           |                                                        |
| `<glm/vec3.hpp>`                          | vec3 (float), ivec3, uvec3, bvec3                                                                           |                                                        |
| `<glm/vec4.hpp>`                          | vec4 (float), ivec4, uvec4, bvec4                                                                           |                                                        |
| `<glm/ext/vector_float*.hpp>`             | Отдельные заголовки для оптимизации сборки                                                                  |                                                        |
| **Матрицы**                               |                                                                                                             |                                                        |
| `<glm/mat2x2.hpp>` ... `<glm/mat4x4.hpp>` | Матрицы 2×2 … 4×4 (float)                                                                                   |                                                        |
| `<glm/ext/matrix_float*.hpp>`             | Отдельные заголовки                                                                                         |                                                        |
| **Функции ядра**                          |                                                                                                             |                                                        |
| `<glm/common.hpp>`                        | Общие функции: `abs`, `min`, `max`, `clamp`, `mix`, `step`, `smoothstep`, `isnan`, `isfinite`, `fma`        |                                                        |
| `<glm/exponential.hpp>`                   | Экспоненциальные: `pow`, `exp`, `log`, `exp2`, `log2`, `sqrt`, `inversesqrt`                                |                                                        |
| `<glm/geometric.hpp>`                     | Геометрические: `length`, `distance`, `dot`, `cross`, `normalize`, `faceforward`, `reflect`, `refract`      |                                                        |
| `<glm/matrix.hpp>`                        | Матричные: `transpose`, `determinant`, `inverse` (ядро)                                                     |                                                        |
| `<glm/packing.hpp>`                       | Упаковка: `packUnorm4x8`, `unpackUnorm4x8`, `packHalf2x16`, `unpackHalf2x16`                                |                                                        |
| `<glm/trigonometric.hpp>`                 | Тригонометрические: `radians`, `degrees`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan` (2 аргумента) |                                                        |
| `<glm/vector_relational.hpp>`             | Векторные реляционные: `lessThan`, `greaterThan`, `equal`, `any`, `all`, `not`                              |                                                        |
| **Расширения (GTC — стабильные)**         |                                                                                                             |                                                        |
| `<glm/gtc/matrix_transform.hpp>`          | Матричные трансформации: `translate`, `rotate`, `scale`, `lookAt`, `perspective`, `ortho`, `frustum`        |                                                        |
| `<glm/gtc/quaternion.hpp>`                | Кватернионы: `quat`, `angleAxis`, `mix`, `slerp`, `conjugate`, `inverse`, `mat4_cast`                       |                                                        |
| `<glm/gtc/type_ptr.hpp>`                  | Указатели на данные: `value_ptr`, `make_mat4`, `make_vec3` (для Vulkan/OpenGL)                              |                                                        |
| `<glm/gtc/constants.hpp>`                 | Константы: `pi`, `epsilon`, `infinity`                                                                      |                                                        |
| `<glm/gtc/random.hpp>`                    | Случайные числа: `linearRand`, `circularRand`, `sphericalRand`, `gaussRand`                                 |                                                        |
| `<glm/gtc/noise.hpp>`                     | Шум: `perlin`, `simplex`                                                                                    |                                                        |
| **Расширения (GTX — экспериментальные)**  | Требуют `#define GLM_ENABLE_EXPERIMENTAL`                                                                   |                                                        |
| `<glm/gtx/compatibility.hpp>`             | Совместимость с DirectX/HLSL                                                                                |                                                        |
| `<glm/gtx/euler_angles.hpp>`              | Углы Эйлера                                                                                                 |                                                        |
| `<glm/gtx/fast_square_root.hpp>`          | Быстрые приближения `fastSqrt`, `fastInverseSqrt`                                                           |                                                        |
| `<glm/gtx/norm.hpp>`                      | Нормы: `length2`, `distance2` (без квадратного корня)                                                       |                                                        |
| `<glm/gtx/string_cast.hpp>`               | Преобразование в строку: `to_string`                                                                        |                                                        |

## Когда что использовать

| Задача                             | Функция / тип                                                           | Заголовок                        | Пример                                                    |
|------------------------------------|-------------------------------------------------------------------------|----------------------------------|-----------------------------------------------------------|
| Создать вектор/матрицу             | `glm::vec3(1.0f, 2.0f, 3.0f)`                                           | `<glm/glm.hpp>`                  | [glm_transform.cpp](../../examples/glm_transform.cpp)     |
|                                    | `glm::mat4(1.0f)` (единичная)                                           | `<glm/glm.hpp>`                  |                                                           |
| Нормализовать вектор               | `glm::normalize(v)`                                                     | `<glm/geometric.hpp>`            |                                                           |
| Скалярное произведение             | `glm::dot(a, b)`                                                        | `<glm/geometric.hpp>`            |                                                           |
| Векторное произведение             | `glm::cross(a, b)` (только vec3)                                        | `<glm/geometric.hpp>`            |                                                           |
| Транспонировать матрицу            | `glm::transpose(m)`                                                     | `<glm/matrix.hpp>`               |                                                           |
| Обратная матрица                   | `glm::inverse(m)`                                                       | `<glm/matrix.hpp>`               |                                                           |
| Перевести/повернуть/масштабировать | `glm::translate(m, v)`                                                  | `<glm/gtc/matrix_transform.hpp>` | [glm_transform.cpp](../../examples/glm_transform.cpp)     |
|                                    | `glm::rotate(m, angle, axis)`                                           | `<glm/gtc/matrix_transform.hpp>` |                                                           |
|                                    | `glm::scale(m, v)`                                                      | `<glm/gtc/matrix_transform.hpp>` |                                                           |
| Перспективная проекция             | `glm::perspective(fov, aspect, near, far)`                              | `<glm/gtc/matrix_transform.hpp>` |                                                           |
| Ортографическая проекция           | `glm::ortho(left, right, bottom, top, near, far)`                       | `<glm/gtc/matrix_transform.hpp>` |                                                           |
| Матрица вида камеры                | `glm::lookAt(eye, center, up)`                                          | `<glm/gtc/matrix_transform.hpp>` |                                                           |
| Кватернионы                        | `glm::quat(w, x, y, z)`, `glm::angleAxis(angle, axis)`                  | `<glm/gtc/quaternion.hpp>`       |                                                           |
| Интерполяция кватернионов          | `glm::mix(q1, q2, t)` (линейная), `glm::slerp(q1, q2, t)` (сферическая) | `<glm/gtc/quaternion.hpp>`       |                                                           |
| Указатель на данные для Vulkan     | `glm::value_ptr(mat)`                                                   | `<glm/gtc/type_ptr.hpp>`         | [glm_voxel_chunk.cpp](../../examples/glm_voxel_chunk.cpp) |
| Случайные векторы                  | `glm::linearRand(min, max)`                                             | `<glm/gtc/random.hpp>`           |                                                           |
| Шум Перлина/симплекс               | `glm::perlin(p)`, `glm::simplex(p)`                                     | `<glm/gtc/noise.hpp>`            |                                                           |
| Преобразование в строку (debug)    | `glm::to_string(v)`                                                     | `<glm/gtx/string_cast.hpp>`      |                                                           |

**Примечание**: Все примеры находятся в [docs/examples/](../../examples/) и компилируются через CMake. Для GTX
расширений требуется `#define GLM_ENABLE_EXPERIMENTAL` перед включением заголовков.

---

## Векторные функции

### Основные операции

Заголовок: `<glm/glm.hpp>` или `<glm/geometric.hpp>`

| Функция       | Сигнатура                                     | Описание                                   | Пример                                             |
|---------------|-----------------------------------------------|--------------------------------------------|----------------------------------------------------|
| `length`      | `float length(vec3 v)`                        | Длина вектора                              | `float len = glm::length(vec3(1,2,3)); // ≈3.741`  |
| `distance`    | `float distance(vec3 p1, vec3 p2)`            | Расстояние между точками                   | `float d = glm::distance(a, b);`                   |
| `dot`         | `float dot(vec3 a, vec3 b)`                   | Скалярное произведение                     | `float dp = glm::dot(dir, normal);`                |
| `cross`       | `vec3 cross(vec3 a, vec3 b)`                  | Векторное произведение (только 3D)         | `vec3 normal = glm::cross(edge1, edge2);`          |
| `normalize`   | `vec3 normalize(vec3 v)`                      | Нормализованный вектор (единичная длина)   | `vec3 dir = glm::normalize(vec3(1,2,3));`          |
| `faceforward` | `vec3 faceforward(vec3 N, vec3 I, vec3 Nref)` | Ориентирует N в сторону от I               |                                                    |
| `reflect`     | `vec3 reflect(vec3 I, vec3 N)`                | Отражение вектора I относительно нормали N | `vec3 reflected = glm::reflect(incident, normal);` |
| `refract`     | `vec3 refract(vec3 I, vec3 N, float eta)`     | Преломление (закон Снеллиуса)              |                                                    |

### Компонентные операции

Заголовок: `<glm/common.hpp>`, `<glm/exponential.hpp>`, `<glm/trigonometric.hpp>`

| Функция                                     | Описание                                      | Применение                                     |
|---------------------------------------------|-----------------------------------------------|------------------------------------------------|
| `abs`, `sign`                               | Абсолютное значение, знак                     | `vec3 vabs = glm::abs(v);`                     |
| `floor`, `ceil`, `round`                    | Округление вниз/вверх/ближайшее               |                                                |
| `fract`                                     | Дробная часть                                 | `vec3 frac = glm::fract(v);`                   |
| `mod`                                       | Остаток от деления                            |                                                |
| `min`, `max`, `clamp`                       | Ограничение значений                          | `vec3 clamped = glm::clamp(v, 0.0f, 1.0f);`    |
| `mix`                                       | Линейная интерполяция                         | `vec3 lerp = glm::mix(a, b, t);`               |
| `step`, `smoothstep`                        | Пороговая и сглаженная ступенчатая функции    |                                                |
| `pow`, `exp`, `log`                         | Степень, экспонента, логарифм                 |                                                |
| `sqrt`, `inversesqrt`                       | Квадратный корень, обратный квадратный корень | `vec3 invLen = glm::inversesqrt(v);`           |
| `sin`, `cos`, `tan`, `asin`, `acos`, `atan` | Тригонометрические (радианы)                  |                                                |
| `radians`, `degrees`                        | Преобразование градусы ↔ радианы              | `float rad = glm::radians(45.0f); // 0.785398` |

### Реляционные и логические

Заголовок: `<glm/vector_relational.hpp>`

| Функция                                                        | Возвращает | Описание                                                       |
|----------------------------------------------------------------|------------|----------------------------------------------------------------|
| `lessThan`, `greaterThan`, `lessThanEqual`, `greaterThanEqual` | `bvec*`    | Поэлементное сравнение                                         |
| `equal`, `notEqual`                                            | `bvec*`    | Равенство с epsilon (если указан)                              |
| `any`                                                          | `bool`     | Истина, если хотя бы один компонент истинен                    |
| `all`                                                          | `bool`     | Истина, если все компоненты истинны                            |
| `not_`                                                         | `bvec*`    | Логическое НЕ (подчёркивание, т.к. `not` — ключевое слово C++) |

**Пример:**

```cpp
glm::vec3 a(1,2,3), b(1,2,4);
glm::bvec3 cmp = glm::lessThan(a, b); // (false, false, true)
bool anyTrue = glm::any(cmp); // true
bool allTrue = glm::all(cmp); // false
```

---

## Матричные функции

### Трансформации (GTC)

Заголовок: `<glm/gtc/matrix_transform.hpp>`

| Функция       | Сигнатура                                                           | Описание                                       | Пример                                                                               |
|---------------|---------------------------------------------------------------------|------------------------------------------------|--------------------------------------------------------------------------------------|
| `translate`   | `mat4 translate(mat4 m, vec3 v)`                                    | Матрица переноса                               | `glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(1,2,3));`                   |
| `rotate`      | `mat4 rotate(mat4 m, float angle, vec3 axis)`                       | Матрица поворота (угол в радианах)             | `glm::mat4 R = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0,1,0));` |
| `scale`       | `mat4 scale(mat4 m, vec3 v)`                                        | Матрица масштабирования                        | `glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(2,2,2));`                       |
| `lookAt`      | `mat4 lookAt(vec3 eye, vec3 center, vec3 up)`                       | Матрица вида камеры                            | `glm::mat4 view = glm::lookAt(cameraPos, target, glm::vec3(0,1,0));`                 |
| `perspective` | `mat4 perspective(float fovY, float aspect, float near, float far)` | Перспективная проекция (fovY в радианах)       | `glm::mat4 proj = glm::perspective(glm::radians(45.0f), 16.0f/9.0f, 0.1f, 100.0f);`  |
| `ortho`       | `mat4 ortho(float left, right, bottom, top, near, far)`             | Ортографическая проекция                       |                                                                                      |
| `frustum`     | `mat4 frustum(float left, right, bottom, top, near, far)`           | Усечённая пирамида (как glFrustum)             |                                                                                      |
| `pickMatrix`  | `mat4 pickMatrix(vec2 center, vec2 delta, ivec4 viewport)`          | Матрица выбора (как gluPickMatrix)             |                                                                                      |
| `project`     | `vec3 project(vec3 obj, mat4 model, mat4 proj, ivec4 viewport)`     | Преобразование объекта в окно (как gluProject) |                                                                                      |
| `unProject`   | `vec3 unProject(vec3 win, mat4 model, mat4 proj, ivec4 viewport)`   | Обратное преобразование (как gluUnProject)     |                                                                                      |

### Ядро матриц

Заголовок: `<glm/matrix.hpp>`

| Функция        | Описание                                | Пример                                      |
|----------------|-----------------------------------------|---------------------------------------------|
| `transpose`    | Транспонирование                        | `glm::mat4 transposed = glm::transpose(m);` |
| `determinant`  | Определитель                            | `float det = glm::determinant(m);`          |
| `inverse`      | Обратная матрица (ядро)                 | `glm::mat4 inv = glm::inverse(m);`          |
| `outerProduct` | Внешнее произведение векторов → матрица | `glm::mat2x3 M = glm::outerProduct(a, b);`  |

### Дополнительные обратные матрицы (GTC)

Заголовок: `<glm/gtc/matrix_inverse.hpp>`

| Функция            | Описание                                                   |
|--------------------|------------------------------------------------------------|
| `affineInverse`    | Обратная для аффинной матрицы (быстрее, чем общая inverse) |
| `inverseTranspose` | Транспонированная обратная (для нормалей)                  |

---

## Кватернионы

Заголовок: `<glm/gtc/quaternion.hpp>`

### Создание

| Функция      | Сигнатура                                  | Описание                                   |
|--------------|--------------------------------------------|--------------------------------------------|
| `quat`       | `quat(float w, float x, float y, float z)` | Прямое задание                             |
| `angleAxis`  | `quat angleAxis(float angle, vec3 axis)`   | Из угла и оси (угол в радианах)            |
| `quatLookAt` | `quat quatLookAt(vec3 direction, vec3 up)` | Из направления (как lookAt, но кватернион) |

### Операции

| Функция               | Возвращает      | Описание                                       |
|-----------------------|-----------------|------------------------------------------------|
| `dot`                 | `float`         | Скалярное произведение кватернионов            |
| `length`, `normalize` | `float`, `quat` | Длина и нормализация                           |
| `conjugate`           | `quat`          | Сопряжённый кватернион                         |
| `inverse`             | `quat`          | Обратный (для единичного = conjugate)          |
| `mix`                 | `quat`          | Линейная интерполяция (NLERP)                  |
| `slerp`               | `quat`          | Сферическая линейная интерполяция (правильная) |
| `mat4_cast`           | `mat4`          | Преобразование в матрицу 4×4                   |
| `toMat4`              | `mat4`          | То же, что `mat4_cast`                         |

**Пример:**

```cpp
glm::quat q = glm::angleAxis(glm::radians(90.0f), glm::vec3(0,1,0));
glm::mat4 rotMat = glm::mat4_cast(q); // матрица поворота на 90° вокруг Y
```

---

## Указатели на данные (для Vulkan/OpenGL)

Заголовок: `<glm/gtc/type_ptr.hpp>`

| Функция     | Сигнатура                          | Описание                            | Использование в Vulkan                                           |
|-------------|------------------------------------|-------------------------------------|------------------------------------------------------------------|
| `value_ptr` | `float* value_ptr(vec3&)`          | Указатель на сырые данные           | `vkCmdPushConstants(cmd, ..., glm::value_ptr(pushConst));`       |
| `value_ptr` | `float* value_ptr(mat4&)`          | Указатель на матрицу (column-major) | `memcpy(uniformBufferMapped, glm::value_ptr(ubo), sizeof(ubo));` |
| `make_mat4` | `mat4 make_mat4(float const* ptr)` | Создание матрицы из указателя       |                                                                  |
| `make_vec3` | `vec3 make_vec3(float const* ptr)` | Создание вектора из указателя       |                                                                  |

**Важно**: Матрицы в GLM хранятся в column-major порядке (как в OpenGL). Для Vulkan это соответствует
`VK_FORMAT_R32G32B32A32_SFLOAT` и т.д.

**Пример для Vulkan UBO:**

```cpp
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

void updateUniformBuffer(UniformBufferObject& ubo, void* mapped) {
    ubo.model = glm::rotate(glm::mat4(1.0f), time, glm::vec3(0,1,0));
    ubo.view = glm::lookAt(...);
    ubo.proj = glm::perspective(...);
    memcpy(mapped, glm::value_ptr(ubo), sizeof(ubo));
}
```

---

## Расширения GTC (стабильные)

### Константы

Заголовок: `<glm/gtc/constants.hpp>`

| Константа             | Значение (float)   | Описание                   |
|-----------------------|--------------------|----------------------------|
| `pi<float>()`         | 3.141592653589793  | π                          |
| `two_pi<float>()`     | 6.283185307179586  | 2π                         |
| `half_pi<float>()`    | 1.5707963267948966 | π/2                        |
| `quarter_pi<float>()` | 0.7853981633974483 | π/4                        |
| `root_two<float>()`   | 1.4142135623730951 | √2                         |
| `root_three<float>()` | 1.7320508075688772 | √3                         |
| `epsilon<float>()`    | 1.192092896e-07f   | Машинный epsilon для float |
| `zero<float>()`       | 0.0                |                            |
| `one<float>()`        | 1.0                |                            |

### Случайные числа

Заголовок: `<glm/gtc/random.hpp>`

| Функция                       | Возвращает          | Описание                             |
|-------------------------------|---------------------|--------------------------------------|
| `linearRand(T min, T max)`    | `T` (скаляр/вектор) | Равномерное распределение [min, max) |
| `circularRand(float radius)`  | `vec2`              | Точка на окружности                  |
| `sphericalRand(float radius)` | `vec3`              | Точка на сфере (равномерно)          |
| `diskRand(float radius)`      | `vec2`              | Точка в круге                        |
| `ballRand(float radius)`      | `vec3`              | Точка в шаре                         |
| `gaussRand(T mean, T stdDev)` | `T`                 | Нормальное распределение             |

### Шум

Заголовок: `<glm/gtc/noise.hpp>`

| Функция   | Сигнатура               | Описание        |
|-----------|-------------------------|-----------------|
| `perlin`  | `float perlin(vec2 p)`  | Шум Перлина 2D  |
| `perlin`  | `float perlin(vec3 p)`  | Шум Перлина 3D  |
| `perlin`  | `float perlin(vec4 p)`  | Шум Перлина 4D  |
| `simplex` | `float simplex(vec2 p)` | Симплекс-шум 2D |
| `simplex` | `float simplex(vec3 p)` | Симплекс-шум 3D |
| `simplex` | `float simplex(vec4 p)` | Симплекс-шум 4D |

---

## Интеграция с ProjectV

Для воксельного движка ProjectV особо полезны:

1. **Быстрые приближения** (`<glm/gtx/fast_square_root.hpp>`): `fastSqrt`, `fastInverseSqrt` для массовых вычислений
   расстояний.
2. **Нормы без корня** (`<glm/gtx/norm.hpp>`): `length2`, `distance2` для сравнения расстояний без извлечения корня.
3. **Упаковка** (`<glm/packing.hpp>`): `packUnorm4x8` для сжатия цветов вокселей в 32 бита.
4. **Шум** (`<glm/gtc/noise.hpp>`): Генерация процедурного мира.

Подробнее см. [projectv-integration.md](projectv-integration.md).

---

## См. также

- [Конфигурация макросов GLM](integration.md#макросы-конфигурации) — `GLM_FORCE_*` настройки
- [Основные понятия](concepts.md) — математические основы
- [Интеграция с Vulkan](integration.md#интеграция-с-vulkan) — UBO, push constants, std140
- [Примеры кода](../../examples/) — `glm_transform.cpp`, `glm_voxel_chunk.cpp`
- [Оригинальная документация GLM](https://github.com/g-truc/glm/blob/master/manual.md)
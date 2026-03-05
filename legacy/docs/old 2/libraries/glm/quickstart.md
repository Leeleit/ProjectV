# Быстрый старт

**🟢 Уровень 1: Начинающий**

Минимальный рабочий пример: подключить glm и посчитать матрицу трансформации (перенос, поворот, масштаб). Все примеры
компилируются через CMake проекта ProjectV.

**Примеры кода в репозитории:**

- [glm_transform.cpp](../examples/glm_transform.cpp) — базовые трансформации
- [glm_voxel_chunk.cpp](../examples/glm_voxel_chunk.cpp) — воксельная математика
- [Другие примеры](../../examples/) — полный список

## Что нужно перед началом

- CMake 3.25+
- Компилятор C++ (MSVC, GCC, Clang) с поддержкой C++17+
- glm как подмодуль в [external/glm](../../external/glm/)
- Проект ProjectV уже настроен (см. корневой CMakeLists.txt)

## Шаг 1: CMakeLists.txt

Подключение glm как подпроекта в вашем приложении ProjectV:

```cmake
# В корневом CMakeLists.txt ProjectV glm уже подключен
# В вашем подпроекте просто линкуйтесь с glm::glm

add_executable(my_app src/main.cpp)
target_link_libraries(my_app PRIVATE glm::glm)
```

Или если вы создаёте отдельный пример:

```cmake
# docs/examples/CMakeLists.txt показывает как добавлять примеры
add_executable(glm_transform glm_transform.cpp)
target_link_libraries(glm_transform PRIVATE glm::glm)
```

## Шаг 2: Базовый пример трансформации

Создайте файл `main.cpp`:

```cpp
#include <glm/glm.hpp>               // Векторы, матрицы, базовые функции
#include <glm/gtc/matrix_transform.hpp> // translate, rotate, scale, perspective
#include <glm/gtc/constants.hpp>     // Константы (pi, epsilon)
#include <iostream>

int main() {
    // 1. Создаём единичную матрицу 4×4 (начальная трансформация)
    glm::mat4 model = glm::mat4(1.0f);
    
    // 2. Перенос на (1, 0, 0) в мировых координатах
    model = glm::translate(model, glm::vec3(1.0f, 0.0f, 0.0f));
    
    // 3. Поворот на 45 градусов вокруг оси Y
    //    Угол ДОЛЖЕН быть в радианах!
    model = glm::rotate(model, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    
    // 4. Масштабирование в 2 раза по всем осям
    model = glm::scale(model, glm::vec3(2.0f, 2.0f, 2.0f));
    
    // 5. Исходная точка в локальных координатах объекта (w=1 для позиций)
    glm::vec4 localPoint(0.0f, 0.5f, 0.0f, 1.0f);
    
    // 6. Применяем все трансформации к точке
    glm::vec4 transformed = model * localPoint;
    
    std::cout << "Исходная точка: (" 
              << localPoint.x << ", " << localPoint.y << ", " 
              << localPoint.z << ")" << std::endl;
    std::cout << "После трансформации: (" 
              << transformed.x << ", " << transformed.y << ", " 
              << transformed.z << ")" << std::endl;
    
    // 7. Пример вычисления расстояния между точками
    glm::vec3 a(0.0f, 0.0f, 0.0f);
    glm::vec3 b(1.0f, 2.0f, 3.0f);
    float distance = glm::distance(a, b);
    std::cout << "Расстояние между a и b: " << distance << std::endl;
    
    // 8. Пример с камерой и проекцией
    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 5.0f),   // Позиция камеры
        glm::vec3(0.0f, 0.0f, 0.0f),   // Точка взгляда
        glm::vec3(0.0f, 1.0f, 0.0f)    // Вектор "вверх"
    );
    
    glm::mat4 proj = glm::perspective(
        glm::radians(45.0f),  // Поле зрения (FOV) в радианах
        16.0f / 9.0f,         // Соотношение сторон (16:9)
        0.1f,                 // Ближняя плоскость отсечения
        100.0f                // Дальняя плоскость отсечения
    );
    
    // 9. Полная матрица MVP (Model-View-Projection)
    glm::mat4 mvp = proj * view * model;
    
    std::cout << "Матрица MVP создана успешно!" << std::endl;
    return 0;
}
```

Полный работающий пример: [glm_transform.cpp](../examples/glm_transform.cpp)

## Что происходит на каждом шаге

| Строка                                    | Действие                      | Объяснение                                                                  |
|-------------------------------------------|-------------------------------|-----------------------------------------------------------------------------|
| `#include <glm/glm.hpp>`                  | Подключает ядро GLM           | vec2/3/4, mat2/3/4, базовые математические функции (dot, cross, normalize). |
| `#include <glm/gtc/matrix_transform.hpp>` | Добавляет трансформации       | `translate`, `rotate`, `scale`, `perspective`, `lookAt`, `ortho`.           |
| `glm::mat4(1.0f)`                         | Единичная матрица             | Начальная матрица для накопления трансформаций.                             |
| `glm::translate(model, vec3)`             | Матрица переноса              | Сдвигает все точки на указанный вектор.                                     |
| `glm::rotate(model, angle, axis)`         | Матрица поворота              | Вращает вокруг оси на угол (в радианах!).                                   |
| `glm::radians(45.0f)`                     | Конвертация градусы → радианы | Обязательно для всех функций GLM.                                           |
| `glm::scale(model, vec3)`                 | Матрица масштабирования       | Увеличивает/уменьшает по осям.                                              |
| `model * localPoint`                      | Применение трансформации      | Умножение матрицы на вектор (column-major порядок).                         |
| `glm::distance(a, b)`                     | Расстояние между точками      | Евклидово расстояние: √((x₁-x₂)² + (y₁-y₂)² + (z₁-z₂)²).                    |
| `glm::lookAt(...)`                        | Матрица вида камеры           | Преобразует мировые координаты в координаты камеры.                         |
| `glm::perspective(...)`                   | Перспективная проекция        | Проецирует 3D сцену на 2D экран с перспективой.                             |
| `proj * view * model`                     | Композиция матриц             | Порядок важен: сначала модель, потом вид, потом проекция.                   |

## Шаг 3: Пример для воксельного движка

Для ProjectV особенно полезны операции с целочисленными векторами и быстрые вычисления:

```cpp
#include <glm/glm.hpp>
#include <glm/gtx/fast_square_root.hpp> // Быстрые приближения
#include <glm/gtx/norm.hpp>              // Нормы без квадратного корня

// Координаты вокселя в чанке (0-31)
glm::ivec3 voxelCoord(10, 20, 30);

// Размер чанка (32³ вокселя)
glm::ivec3 chunkSize(32, 32, 32);

// Проверка, находится ли воксель внутри чанка
bool inside = glm::all(glm::greaterThanEqual(voxelCoord, glm::ivec3(0))) &&
              glm::all(glm::lessThan(voxelCoord, chunkSize));

// Линейный индекс вокселя в 1D массиве (DOD)
int linearIndex = voxelCoord.x + voxelCoord.y * chunkSize.x + 
                  voxelCoord.z * chunkSize.x * chunkSize.y;

// Расстояние между двумя вокселями (для проверки видимости)
glm::vec3 voxelPosA = glm::vec3(voxelCoord);
glm::vec3 voxelPosB = glm::vec3(15, 25, 5);
float distanceSquared = glm::distance2(voxelPosA, voxelPosB); // Без sqrt!

// Быстрый обратный квадратный корень (как в Quake)
float fastInvSqrt = glm::fastInverseSqrt(distanceSquared);
```

Полный пример: [glm_voxel_chunk.cpp](../examples/glm_voxel_chunk.cpp)

## Шаг 4: Передача данных в Vulkan

Для интеграции с Vulkan используйте `value_ptr` для записи матриц в uniform буферы:

```cpp
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>  // value_ptr

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

void updateUBO(UniformBufferObject& ubo, void* mappedMemory) {
    // ... вычисляем матрицы ...
    
    // Копируем в mapped память Vulkan
    memcpy(mappedMemory, glm::value_ptr(ubo), sizeof(ubo));
}

// Или для push constants:
vkCmdPushConstants(commandBuffer, ..., glm::value_ptr(pushConstantMatrix));
```

Подробнее: [Интеграция с Vulkan](integration.md#интеграция-с-vulkan-uniform-буферы).

## Шаг 5: Следующие шаги

1. **Изучите основы**: [Основные понятия](concepts.md) — векторы, матрицы, кватернионы, системы координат.
2. **Используйте справочник**: [Справочник API](api-reference.md) — полный список функций с примерами.
3. **Настройте под проект**: [Интеграция](integration.md) — макросы конфигурации, SIMD оптимизации, Vulkan.
4. **Посмотрите примеры**: [Все примеры кода](../../examples/) — `glm_transform.cpp`, `glm_voxel_chunk.cpp` и другие.
5. **Оптимизируйте**: Для воксельного движка используйте быстрые приближения (`fastSqrt`, `fastInverseSqrt`) и
   целочисленные векторы (`ivec3`).

## Частые ошибки

1. **Углы в градусах**: Всегда используйте `glm::radians()` для преобразования.
2. **Порядок умножения**: `proj * view * model`, не `model * view * proj`.
3. **Выравнивание в UBO**: Используйте `alignas(16)` для `vec3` в структурах std140.
4. **Подключение заголовков**: Для кватернионов нужен `<glm/gtc/quaternion.hpp>`, для трансформаций —
   `<glm/gtc/matrix_transform.hpp>`.

## Дополнительные примеры

- **Кватернионы**: [Концепты](concepts.md#кватернион-quat) — вращение без gimbal lock.
- **Шум**: [Справочник API](api-reference.md#шум) — `glm::perlin`, `glm::simplex` для процедурной генерации.
- **Случайные числа**: [Справочник API](api-reference.md#случайные-числа) — `glm::linearRand`, `glm::sphericalRand`.

---

## Быстрая справка

| Задача              | Решение                                        | Заголовок                                                    |
|---------------------|------------------------------------------------|--------------------------------------------------------------|
| Создать вектор      | `glm::vec3(1,2,3)`                             | `<glm/glm.hpp>`                                              |
| Перенести объект    | `glm::translate(mat, vec3)`                    | `<glm/gtc/matrix_transform.hpp>`                             |
| Повернуть объект    | `glm::rotate(mat, radians, axis)`              | `<glm/gtc/matrix_transform.hpp>`                             |
| Проекция камеры     | `glm::perspective(radians, aspect, near, far)` | `<glm/gtc/matrix_transform.hpp>`                             |
| Матрица вида        | `glm::lookAt(eye, center, up)`                 | `<glm/gtc/matrix_transform.hpp>`                             |
| Кватернионы         | `glm::quat`, `glm::slerp`                      | `<glm/gtc/quaternion.hpp>`                                   |
| Указатель на данные | `glm::value_ptr(mat)`                          | `<glm/gtc/type_ptr.hpp>`                                     |
| Быстрый sqrt        | `glm::fastSqrt`, `glm::fastInverseSqrt`        | `<glm/gtx/fast_square_root.hpp>` + `GLM_ENABLE_EXPERIMENTAL` |
| Норма без sqrt      | `glm::length2`, `glm::distance2`               | `<glm/gtx/norm.hpp>` + `GLM_ENABLE_EXPERIMENTAL`             |

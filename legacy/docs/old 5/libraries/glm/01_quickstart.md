# Быстрый старт GLM

🟢 **Уровень 1: Начинающий**

Минимальный рабочий пример: подключить GLM и посчитать матрицу трансформации.

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.10)
project(MyApp)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(external/glm)

add_executable(my_app src/main.cpp)
target_link_libraries(my_app PRIVATE glm::glm)
```

## main.cpp

```cpp
#include <glm/glm.hpp>                 // Векторы, матрицы, базовые функции
#include <glm/gtc/matrix_transform.hpp> // translate, rotate, scale, perspective
#include <glm/gtc/constants.hpp>       // Константы (pi, epsilon)
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

## Результат

```
Исходная точка: (0, 0.5, 0)
После трансформации: (1, 1, 0.707107)
Расстояние между a и b: 3.74166
Матрица MVP создана успешно!
```

## Ключевые моменты

| Концепт         | Пример                                                 |
|-----------------|--------------------------------------------------------|
| **Matrix**      | `glm::mat4(1.0f)` — единичная матрица                  |
| **Vector**      | `glm::vec3(1.0f, 2.0f, 3.0f)` — 3D вектор              |
| **Translate**   | `glm::translate(mat, vec3)` — перенос                  |
| **Rotate**      | `glm::rotate(mat, radians, axis)` — поворот            |
| **Scale**       | `glm::scale(mat, vec3)` — масштабирование              |
| **Perspective** | `glm::perspective(fov, aspect, near, far)` — проекция  |
| **LookAt**      | `glm::lookAt(eye, center, up)` — матрица вида          |
| **Radians**     | `glm::radians(45.0f)` — конвертация градусов в радианы |

## Порядок операций

Стандартная формула: `MVP = Projection * View * Model`

```cpp
glm::mat4 mvp = proj * view * model;  // Обратный порядок умножения!
glm::vec4 result = mvp * vertex;       // Умножение справа
```

Почему такой порядок:

1. Умножаем вершину на Model (получаем мировую позицию)
2. Умножаем на View (позиция относительно камеры)
3. Умножаем на Projection (координаты на экране)

## Частые ошибки

| Ошибка                         | Решение                                         |
|--------------------------------|-------------------------------------------------|
| Углы в градусах                | Всегда используйте `glm::radians()`             |
| Неправильный порядок умножения | `proj * view * model`, не `model * view * proj` |
| Не найден `translate`          | `#include <glm/gtc/matrix_transform.hpp>`       |
| Не найден `quat`               | `#include <glm/gtc/quaternion.hpp>`             |

## Быстрая справка

| Задача              | Функция                           | Заголовок                        |
|---------------------|-----------------------------------|----------------------------------|
| Создать вектор      | `glm::vec3(1, 2, 3)`              | `<glm/glm.hpp>`                  |
| Перенести объект    | `glm::translate(mat, vec3)`       | `<glm/gtc/matrix_transform.hpp>` |
| Повернуть объект    | `glm::rotate(mat, radians, axis)` | `<glm/gtc/matrix_transform.hpp>` |
| Масштабировать      | `glm::scale(mat, vec3)`           | `<glm/gtc/matrix_transform.hpp>` |
| Проекция камеры     | `glm::perspective(...)`           | `<glm/gtc/matrix_transform.hpp>` |
| Матрица вида        | `glm::lookAt(...)`                | `<glm/gtc/matrix_transform.hpp>` |
| Кватернионы         | `glm::quat`, `glm::slerp`         | `<glm/gtc/quaternion.hpp>`       |
| Указатель на данные | `glm::value_ptr(mat)`             | `<glm/gtc/type_ptr.hpp>`         |

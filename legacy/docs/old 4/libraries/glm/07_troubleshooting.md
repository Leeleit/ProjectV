# Решение проблем GLM

🟡 **Уровень 2: Средний**

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

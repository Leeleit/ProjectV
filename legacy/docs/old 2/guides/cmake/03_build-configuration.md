# Конфигурация сборки

**🟢 Уровень 1: Начинающий** — Типы сборки, стандарты языка, флаги компилятора.

---

## 1. Типы сборки (Build Types)

### Основные типы

| Тип                | Флаги                  | Назначение               |
|--------------------|------------------------|--------------------------|
| **Debug**          | `-O0 -g` / `/Od /Zi`   | Отладка, без оптимизаций |
| **Release**        | `-O3 -DNDEBUG` / `/O2` | Максимальная скорость    |
| **RelWithDebInfo** | `-O2 -g` / `/O2 /Zi`   | Профилирование           |
| **MinSizeRel**     | `-Os` / `/O1`          | Минимальный размер       |

### Выбор типа сборки

```bash
# Для Ninja/Make (single-config)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Для Visual Studio (multi-config)
cmake ..
cmake --build . --config Release
```

---

## 2. Стандарты C++

```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # c++20, не gnu++20

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
```

---

## 3. Флаги компилятора

Используйте `target_compile_options` для конкретных целей, а не глобально.

### Предупреждения

```cmake
if(MSVC)
    target_compile_options(MyGame PRIVATE
        /W4           # Уровень предупреждений 4
        /permissive-  # Строгое соответствие стандартам
    )
else()
    target_compile_options(MyGame PRIVATE
        -Wall -Wextra -Wpedantic
        -Werror=return-type
    )
endif()
```

### Generator Expressions (условная логика)

```cmake
target_compile_options(MyGame PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
    $<$<CONFIG:Debug>:-DDEBUG_MODE>
)
```

---

## 4. Препроцессорные определения

```cmake
target_compile_definitions(MyGame PRIVATE
    _USE_MATH_DEFINES  # M_PI в Windows
    NOMINMAX           # Отключить min/max макросы
    PROJECT_VERSION="${PROJECT_VERSION}"
)
```

---

## 5. Специфика для ProjectV

> **Связь с C++ гайдом:** ProjectV запрещает исключения и RTTI.
> См. [11_banned-features.md](../cpp/11_banned-features.md) для обоснования.

### Отключение исключений и RTTI (КРИТИЧНО)

```cmake
# ProjectV: Отключение исключений и RTTI
# Это закон движка — без исключений!

if(MSVC)
    target_compile_options(ProjectV PRIVATE
        /EHs-c-    # Отключить исключения (no C++ exceptions)
        /GR-       # Отключить RTTI (no runtime type info)
    )
else()
    target_compile_options(ProjectV PRIVATE
        -fno-exceptions
        -fno-rtti
    )
endif()
```

**Почему это важно:**

| Аспект           | С исключениями | Без исключений  |
|------------------|----------------|-----------------|
| Binary size      | +10-20%        | Baseline        |
| Runtime overhead | Таблицы unwind | Нет             |
| Predictability   | Непредсказуемо | Детерминировано |

**Альтернативы для обработки ошибок:**

```cpp
// Вместо исключений используйте:
std::optional<T>          // Для опциональных значений
std::expected<T, E>       // C++23, для ошибок
bool / enum Result        // Классический подход
```

### SIMD для воксельной математики

```cmake
if(MSVC)
    target_compile_options(ProjectV PRIVATE /arch:AVX2)
else()
    target_compile_options(ProjectV PRIVATE -mavx2 -mfma)
endif()
```

### Vulkan оптимизации

```cmake
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(ProjectV PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/O2 /fp:fast>
        $<$<CXX_COMPILER_ID:GNU>:-O3 -ffast-math>
    )
endif()
```

### DOD/ECS архитектура

```cmake
target_compile_definitions(ProjectV PRIVATE
    USE_DATA_ORIENTED_DESIGN
    USE_ENTITY_COMPONENT_SYSTEM
)
```

---

## 6. Быстрый справочник

| Задача              | Команда                                            |
|---------------------|----------------------------------------------------|
| Установить стандарт | `set(CMAKE_CXX_STANDARD 20)`                       |
| Добавить флаги      | `target_compile_options(target PRIVATE flags)`     |
| Добавить макрос     | `target_compile_definitions(target PRIVATE MACRO)` |
| Условные флаги      | `$<$<CONFIG:Debug>:-g>`                            |

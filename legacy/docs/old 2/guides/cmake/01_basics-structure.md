# Основы и структура CMake

**🟢 Уровень 1: Начинающий** — Modern CMake, targets и структура ProjectV.

Это руководство поможет понять, как устроен процесс сборки ProjectV. Мы используем **Modern CMake** (версия 3.20+), что
означает отказ от глобальных переменных в пользу настройки конкретных целей (`targets`).

---

## 1. Минимальный CMakeLists.txt

Каждый проект начинается с корневого файла `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)

project(ProjectV
    VERSION 0.1.0
    LANGUAGES CXX C
)

# Стандарт C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Добавляем поддиректории
add_subdirectory(src)
add_subdirectory(tests)
```

---

## 2. Основные понятия: Targets

В Modern CMake всё вращается вокруг **Targets** (целей). Цель — это исполняемый файл или библиотека.

### Исполняемый файл (Executable)

```cmake
add_executable(GameApp
    main.cpp
    engine.cpp
)

target_link_libraries(GameApp PRIVATE
    SDL3::SDL3
    Vulkan::Vulkan
)

target_include_directories(GameApp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
```

### Библиотека (Library)

```cmake
add_library(PhysicsEngine STATIC
    physics.cpp
    collision.cpp
)

target_include_directories(PhysicsEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
```

### Область видимости (PRIVATE/PUBLIC/INTERFACE)

| Спецификатор | Значение                                  |
|--------------|-------------------------------------------|
| `PRIVATE`    | Нужно только этой цели                    |
| `PUBLIC`     | Нужно этой цели и тем, кто её использует  |
| `INTERFACE`  | Нужно только тем, кто использует эту цель |

---

## 3. Структура проекта ProjectV

```
ProjectV/
├── CMakeLists.txt          # Корневой конфиг
├── src/
│   ├── main.cpp
│   └── engine/
├── include/                # Публичные заголовки
├── external/               # Сторонние библиотеки (подмодули)
│   ├── SDL/
│   ├── volk/
│   ├── VMA/
│   └── ...
└── tests/
    └── CMakeLists.txt
```

### Подключение подпапок

```cmake
add_subdirectory(src)
add_subdirectory(external)
```

---

## 4. Anti-patterns: чего избегать

**❌ Плохо (Old CMake):**

```cmake
include_directories(include)  # Глобально!
link_libraries(mylib)         # Глобально!
add_definitions(-DDEBUG)      # Глобально!
```

**✅ Хорошо (Modern CMake):**

```cmake
target_include_directories(MyTarget PRIVATE include)
target_link_libraries(MyTarget PRIVATE mylib)
target_compile_definitions(MyTarget PRIVATE DEBUG)
```

Это изолирует настройки разных частей проекта и избегает конфликтов.

---

## 5. Быстрый справочник

| Команда                                      | Назначение                 |
|----------------------------------------------|----------------------------|
| `add_executable(name sources...)`            | Создать исполняемый файл   |
| `add_library(name STATIC/SHARED sources...)` | Создать библиотеку         |
| `target_link_libraries(target libs...)`      | Подключить библиотеки      |
| `target_include_directories(target dirs...)` | Добавить пути к заголовкам |
| `target_compile_definitions(target defs...)` | Добавить макросы           |
| `target_compile_options(target opts...)`     | Добавить флаги компиляции  |
| `add_subdirectory(dir)`                      | Добавить подпроект         |

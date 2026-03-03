# СТВ-CMAKE-002: Стандарт структуры проекта CMake

---

## 1. Область применения

Настоящий стандарт определяет обязательную структуру проектов CMake в ProjectV. Все файлы CMakeLists.txt и связанные
конфигурации сборки ДОЛЖНЫ соответствовать данной спецификации.

---

## 3. Принципы современного CMake

### 3.1 Конфигурация на основе целевых объектов

Вся конфигурация сборки ДОЛЖНА выражаться через свойства целевых объектов, а не через команды уровня директории.

**Обязательный паттерн:**

```cmake
# ПРАВИЛЬНО: Конфигурация на основе целевого объекта
target_compile_features(MyTarget PRIVATE cxx_std_26)
target_include_directories(MyTarget PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(MyTarget PRIVATE PROJECTV_DEBUG)

# ЗАПРЕЩЕНО: Конфигурация уровня директории
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)  # ЗАПРЕЩЕНО
add_definitions(-DPROJECTV_DEBUG)  # ЗАПРЕЩЕНО
```

### 3.2 Ключевые слова видимости

Все команды для целевых объектов ДОЛЖНЫ использовать явные ключевые слова видимости:

| Ключевое слово | Область действия           | Применение                   |
|----------------|----------------------------|------------------------------|
| `PRIVATE`      | Только целевой объект      | Детали внутренней реализации |
| `PUBLIC`       | Целевой объект + зависимые | Требования интерфейса        |
| `INTERFACE`    | Только зависимые           | Header-only библиотеки       |

---

## 4. Структура CMakeLists.txt

### 4.1 Обязательные разделы

Все файлы CMakeLists.txt ДОЛЖНЫ соответствовать следующей структуре:

```cmake
# =============================================================================
# Раздел 1: Заголовок и документация
# =============================================================================
# Модуль ProjectV: <имя_модуля>
# Описание: <краткое_описание>
# Зависимости: <список_зависимостей>
# =============================================================================

# =============================================================================
# Раздел 2: Минимальная версия CMake и объявление проекта
# =============================================================================
cmake_minimum_required(VERSION 3.30)

project(<ИмяПроекта>
    VERSION <major>.<minor>.<patch>
    LANGUAGES CXX
    DESCRIPTION "<описание проекта>"
)

# =============================================================================
# Раздел 3: Параметры конфигурации
# =============================================================================
option(PROJECTV_BUILD_TESTS "Сборка набора тестов" ON)
option(PROJECTV_ENABLE_SANITIZERS "Включить санитайзеры" OFF)

# =============================================================================
# Раздел 4: Зависимости
# =============================================================================
find_package(Vulkan 1.4 REQUIRED)

# =============================================================================
# Раздел 5: Определение целевого объекта
# =============================================================================
add_library(ProjectV.<ИмяМодуля>)

# =============================================================================
# Раздел 6: Исходные файлы
# =============================================================================
target_sources(ProjectV.<ИмяМодуля>
    PUBLIC FILE_SET CXX_MODULES FILES
        <интерфейсные_файлы_модулей>
    PRIVATE
        <файлы_реализации>
)

# =============================================================================
# Раздел 7: Конфигурация компиляции
# =============================================================================
target_compile_features(ProjectV.<ИмяМодуля> PUBLIC cxx_std_26)

# =============================================================================
# Раздел 8: Конфигурация линковки
# =============================================================================
target_link_libraries(ProjectV.<ИмяМодуля>
    PUBLIC
        <публичные_зависимости>
    PRIVATE
        <приватные_зависимости>
)

# =============================================================================
# Раздел 9: Установка (если применимо)
# =============================================================================
install(TARGETS ProjectV.<ИмяМодуля>
    EXPORT ProjectVTargets
    FILE_SET CXX_MODULES
)
```

### 4.2 Шаблон заголовка

```cmake
# =============================================================================
# ProjectV - Проект воксельного движка
# Модуль: <ИмяМодуля>
# Файл: CMakeLists.txt
#
# Copyright (c) 2026 Участники ProjectV
# SPDX-License-Identifier: <лицензия>
# =============================================================================
```

---

## 5. Организация исходных файлов

### 5.1 Интерфейсные файлы модулей (.cppm)

Интерфейсные файлы модулей ДОЛЖНЫ объявляться с использованием `FILE_SET CXX_MODULES`:

```cmake
target_sources(ProjectV.Core
    PUBLIC FILE_SET CXX_MODULES FILES
        src/core/ProjectV.Core.cppm
        src/core/ProjectV.Core.Memory.cppm
        src/core/ProjectV.Core.Containers.cppm
        src/core/ProjectV.Math.cppm
)
```

### 5.2 Файлы реализации (.cpp)

Файлы реализации ДОЛЖНЫ объявляться как `PRIVATE` источники:

```cmake
target_sources(ProjectV.Core
    PRIVATE
        src/core/ProjectV.Core.Memory.cpp
        src/core/ProjectV.Core.Containers.cpp
        src/core/ProjectV.Math.cpp
)
```

### 5.3 Заголовочные файлы

Для обёрток C-библиотек и совместимости с legacy-кодом:

```cmake
target_sources(ProjectV.Core
    PUBLIC FILE_SET HEADERS FILES
        include/projectv/core/export.h
        include/projectv/core/types.h
)
```

---

## 6. Свойства целевых объектов

### 6.1 Обязательные свойства целевого объекта

```cmake
# Стандарт C++ (обязательно)
set_target_properties(ProjectV.Core PROPERTIES
    CXX_STANDARD 26
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Свойства модулей
set_target_properties(ProjectV.Core PROPERTIES
    CXX_SCAN_FOR_MODULES ON
)

# Выходные директории
set_target_properties(ProjectV.Core PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
```

### 6.2 Специфичные для Clang свойства

```cmake
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set_target_properties(ProjectV.Core PROPERTIES
        COMPILE_FLAGS "-fmodules -stdlib=libc++"
        LINK_FLAGS "-stdlib=libc++"
    )
endif()
```

---

## 7. Условная компиляция

### 7.1 Определение платформы

```cmake
# Определение платформы
if(WIN32)
    target_compile_definitions(ProjectV.Core PRIVATE PROJECTV_PLATFORM_WINDOWS)
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(ProjectV.Core PRIVATE PROJECTV_PLATFORM_LINUX)
elseif(APPLE)
    target_compile_definitions(ProjectV.Core PRIVATE PROJECTV_PLATFORM_MACOS)
endif()
```

### 7.2 Конфигурация сборки

```cmake
# Специфичная для Debug конфигурация
target_compile_definitions(ProjectV.Core
    PRIVATE
        $<$<CONFIG:Debug>:PROJECTV_DEBUG>
        $<$<CONFIG:Release>:PROJECTV_RELEASE>
)

# Конфигурация санитайзеров
if(PROJECTV_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ProjectV.Core
        PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
    )
    target_link_options(ProjectV.Core
        PRIVATE
            -fsanitize=address,undefined
    )
endif()
```

---

## 8. Генерируемые файлы

### 8.1 Генерируемые заголовки

```cmake
# Генерация заголовка версии
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/projectv/version.h"
    @ONLY
)

target_include_directories(ProjectV.Core
    PUBLIC
        "${CMAKE_CURRENT_BINARY_DIR}/include"
)
```

### 8.2 Шаблон версии

```cmake
# cmake/version.h.in
#pragma once

#define PROJECTV_VERSION_MAJOR @PROJECTV_VERSION_MAJOR@
#define PROJECTV_VERSION_MINOR @PROJECTV_VERSION_MINOR@
#define PROJECTV_VERSION_PATCH @PROJECTV_VERSION_PATCH@
#define PROJECTV_VERSION_STRING "@PROJECTV_VERSION@"

#define PROJECTV_VULKAN_VERSION @Vulkan_VERSION@
```

---

## 9. Управление подкаталогами

### 9.1 Паттерн добавления подкаталогов

```cmake
# Структура подкаталогов
add_subdirectory(core)       # Уровень 1: Фундамент
add_subdirectory(math)       # Уровень 1: Математика
add_subdirectory(render)     # Уровень 2: Рендеринг
add_subdirectory(voxel)      # Уровень 2: Воксельная система
add_subdirectory(physics)    # Уровень 3: Физика
add_subdirectory(ecs)        # Уровень 3: ECS
add_subdirectory(ui)         # Уровень 3: UI
add_subdirectory(app)        # Уровень 4: Приложение
```

### 9.2 Пропригация зависимостей

Родительские директории НЕ ДОЛЖНЫ напрямую управлять зависимостями дочерних:

```cmake
# ПРАВИЛЬНО: Дочерний модуль управляет своими зависимостями
# В src/physics/CMakeLists.txt:
target_link_libraries(ProjectV.Physics
    PUBLIC
        ProjectV.Core
        ProjectV.Math
    PRIVATE
        ProjectV.Physics.Jolt  # PIMPL
)

# ЗАПРЕЩЕНО: Родитель управляет зависимостями дочернего модуля
# В src/CMakeLists.txt:
target_link_libraries(ProjectV.Physics PRIVATE Jolt)  # ЗАПРЕЩЕНО
```

---

## 10. Конфигурация тестирования

### 10.1 Структура целевого объекта тестов

```cmake
# tests/CMakeLists.txt
enable_testing()

# Исполняемый файл тестов
add_executable(ProjectV.Tests)

target_sources(ProjectV.Tests
    PRIVATE
        main.cpp
        test_memory.cpp
        test_containers.cpp
        test_math.cpp
)

target_link_libraries(ProjectV.Tests
    PRIVATE
        ProjectV.Core
        doctest::doctest
)

# Обнаружение тестов
include(GoogleTest)
gtest_discover_tests(ProjectV.Tests)
```

### 10.2 Регистрация тестов

```cmake
# Регистрация теста в CTest
add_test(NAME ProjectV.Core.Tests
    COMMAND ProjectV.Tests --test-suite=Core
)

# Свойства теста
set_tests_properties(ProjectV.Core.Tests PROPERTIES
    TIMEOUT 60
    LABELS "unit;core"
)
```

---

## 11. Конфигурация установки

### 11.1 Установка целевого объекта

```cmake
install(TARGETS ProjectV.Core
    EXPORT ProjectVExport
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    FILE_SET CXX_MODULES DESTINATION lib/cmake/ProjectV/modules
    FILE_SET HEADERS DESTINATION include
)
```

### 11.2 Конфигурация экспорта

```cmake
install(EXPORT ProjectVExport
    FILE ProjectVTargets.cmake
    NAMESPACE ProjectV::
    DESTINATION lib/cmake/ProjectV
)

# Конфигурация пакета
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ProjectVConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ProjectVConfig.cmake"
    INSTALL_DESTINATION lib/cmake/ProjectV
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ProjectVConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
```

---

## 12. Требования соответствия

### 12.1 Обязательные требования

1. Все файлы CMakeLists.txt ДОЛЖНЫ соответствовать структуре, определённой в разделе 4
2. Конфигурация на основе целевых объектов ДОЛЖНА использоваться исключительно
3. Ключевые слова видимости ДОЛЖНЫ быть явными для всех команд целевых объектов
4. Интерфейсные файлы модулей ДОЛЖНЫ использовать `FILE_SET CXX_MODULES`
5. Файлы реализации ДОЛЖНЫ быть объявлены как `PRIVATE` источники

### 12.2 Запрещённые практики

1. `include_directories()` уровня директории — использовать `target_include_directories()`
2. `add_definitions()` уровня директории — использовать `target_compile_definitions()`
3. `add_compile_options()` уровня директории — использовать `target_compile_options()`
4. `aux_source_directory()` — явно перечислять все исходные файлы
5. `file(GLOB ...)` для исходных файлов — требуется явное перечисление источников

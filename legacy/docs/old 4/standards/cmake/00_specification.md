# СТВ-CMAKE-001: Спецификация системы сборки CMake

**Идентификатор документа:** СТВ-CMAKE-001
**Версия:** 1.0.0
**Статус:** Утверждён
**Дата введения:** 22.02.2026
**Классификация:** Технический стандарт

---

## 1. Область применения

Настоящая спецификация определяет обязательные требования к конфигурации системы сборки CMake в проекте ProjectV. Все
скрипты сборки, toolchain-файлы и модули CMake ДОЛЖНЫ соответствовать настоящему стандарту.

---

## 2. Нормативные ссылки

- ISO/IEC 14882:2026 (C++26)
- Документация CMake 3.30+
- Требования к компилятору Clang 18+
- Спецификация Vulkan 1.4

---

## 3. Требования к компилятору

### 3.1 Обязательный компилятор

ProjectV устанавливает **Clang 18.0+** в качестве эталонного компилятора. Другие компиляторы (GCC 14+, MSVC 19.40+)
допускаются для тестирования совместимости, но НЕ ДОЛЖНЫ использовать специфичные для них расширения.

### 3.2 Конфигурация стандарта C++

```cmake
# ОБЯЗАТЕЛЬНО: Стандарт C++26 со строгим соответствием
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ОБЯЗАТЕЛЬНО: Флаги Clang для модулей C++26
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(
        -std=c++26           # Стандарт C++26
        -stdlib=libc++       # Стандартная библиотека LLVM
        -fmodules            # Включение модулей C++26
        -fmodule-file=std=std.pcm  # Модуль стандартной библиотеки
        -Wall                # Все предупреждения
        -Wextra              # Дополнительные предупреждения
        -Wpedantic           # Строгое соответствие ISO
        -Werror              # Предупреждения как ошибки
        -Wno-unused-command-line-argument  # Обход для модулей Clang
    )

    # Линковка с libc++
    add_link_options(-stdlib=libc++ -lc++abi)
endif()
```

### 3.3 Флаги компиляции модулей

```cmake
# Поддержка модулей C++26 в Clang 18+
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# Флаги прекомпиляции модулей
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprebuilt-module-path=${CMAKE_BINARY_DIR}/modules")

# Путь к модулю стандартной библиотеки
set(STD_MODULE_PATH "${CMAKE_BINARY_DIR}/std.pcm")
```

---

## 4. Требования к структуре проекта

### 4.1 Структура директорий

```
ProjectV/
├── CMakeLists.txt              # Корневая конфигурация
├── cmake/                      # Модули и toolchain-файлы CMake
│   ├── CompilerOptions.cmake   # Конфигурация флагов компилятора
│   ├── FindVulkan.cmake        # Обнаружение Vulkan 1.4
│   ├── ModuleSupport.cmake     # Утилиты для модулей C++26
│   └── toolchains/
│       └── Clang.cmake         # Toolchain для Clang 18+
├── src/                        # Исходный код
│   ├── CMakeLists.txt
│   ├── core/
│   ├── render/
│   ├── voxel/
│   └── ...
├── external/                   # Git-сабмодули
└── tests/
    └── CMakeLists.txt
```

### 4.2 Структура корневого CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.30)

project(ProjectV
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "Voxel Engine Project"
)

# ОБЯЗАТЕЛЬНО: Стандарт C++26
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ОБЯЗАТЕЛЬНО: Сканирование модулей
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# Подключение модулей CMake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(ModuleSupport)

# Требование Vulkan 1.4
find_package(Vulkan 1.4 REQUIRED)

# Добавление подкаталогов
add_subdirectory(src)
add_subdirectory(tests)
```

---

## 5. Конфигурация сборки модулей

### 5.1 Объявление модульного целевого объекта

```cmake
# Объявление библиотеки с модулями C++26
add_library(ProjectV.Core)

# Интерфейс модулей
target_sources(ProjectV.Core PUBLIC
    FILE_SET CXX_MODULES FILES
        src/core/ProjectV.Core.cppm
        src/core/ProjectV.Core.Memory.cppm
        src/core/ProjectV.Core.Containers.cppm
)

# Файлы реализации (паттерн PIMPL)
target_sources(ProjectV.Core PRIVATE
    src/core/ProjectV.Core.Memory.cpp
    src/core/ProjectV.Core.Containers.cpp
)

# Зависимости модуля
target_link_libraries(ProjectV.Core
    PUBLIC
        Vulkan::Vulkan
    PRIVATE
        vma::vma
)
```

### 5.2 Порядок компиляции модулей

Модули ДОЛЖНЫ компилироваться в порядке зависимостей. Система сборки ДОЛЖНА обеспечить следующую последовательность:

| Уровень | Модуль                   | Зависимости                      |
|---------|--------------------------|----------------------------------|
| 0       | ProjectV.Render.Vulkan   | Vulkan, VMA (C-библиотеки в GMF) |
| 0       | ProjectV.Core.Platform   | SDL3 (C-библиотека в GMF)        |
| 1       | ProjectV.Core.Memory     | Уровень 0                        |
| 1       | ProjectV.Core.Containers | Нет                              |
| 1       | ProjectV.Math            | Нет (C++26 <simd>)               |
| 2       | ProjectV.Physics.Jolt    | Уровень 1 (PIMPL, Jolt в .cpp)   |
| 2       | ProjectV.ECS.Flecs       | Уровень 1 (PIMPL, Flecs в .cpp)  |
| 2       | ProjectV.UI.ImGui        | Уровень 1 (PIMPL, ImGui в .cpp)  |
| 3       | ProjectV.Physics         | Уровень 2                        |
| 3       | ProjectV.ECS             | Уровень 2                        |
| 4       | ProjectV.App             | Уровень 3                        |

---

## 6. Управление зависимостями

### 6.1 Git-сабмодули (предпочтительный способ)

Внешние зависимости ДОЛЖНЫ размещаться в директории `external/`:

```cmake
# Интеграция сабмодулей
add_subdirectory(external/volk EXCLUDE_FROM_ALL)
add_subdirectory(external/VMA EXCLUDE_FROM_ALL)

# Header-only библиотеки
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE external/glm)
```

### 6.2 FetchContent (альтернативный способ)

Для зависимостей, не подходящих для сабмодулей:

```cmake
include(FetchContent)

FetchContent_Declare(
    volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG        master
)

FetchContent_MakeAvailable(volk)
```

### 6.3 find_package (системные библиотеки)

Для системно установленных библиотек:

```cmake
# Vulkan SDK 1.4
find_package(Vulkan 1.4 REQUIRED)

# Использование
target_link_libraries(MyTarget PRIVATE Vulkan::Vulkan)
```

---

## 7. Конфигурации сборки

### 7.1 Стандартные типы сборки

| Тип сборки       | Назначение          | Оптимизация | Отладочная информация |
|------------------|---------------------|-------------|-----------------------|
| `Debug`          | Разработка, отладка | `-O0`       | Полная                |
| `Release`        | Продакшн            | `-O3`       | Нет                   |
| `RelWithDebInfo` | Профилирование      | `-O2`       | Полная                |
| `MinSizeRel`     | Дистрибутив         | `-Os`       | Нет                   |

### 7.2 Специфичные флаги конфигурации

```cmake
# Конфигурация Debug
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -fsanitize=address,undefined")

# Конфигурация Release
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -flto=thin")

# Специфичные для Clang оптимизации
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native")
endif()
```

---

## 8. Интеграция с Vulkan

### 8.1 Требования к Vulkan SDK

```cmake
# ОБЯЗАТЕЛЬНО: Vulkan 1.4
find_package(Vulkan 1.4 REQUIRED)

# Проверка версии Vulkan
if(Vulkan_VERSION VERSION_LESS "1.4")
    message(FATAL_ERROR "ProjectV требует Vulkan 1.4+, обнаружена версия ${Vulkan_VERSION}")
endif()

# Статический загрузчик volk
add_subdirectory(external/volk)
target_link_libraries(MyTarget PRIVATE volk::volk)

# Аллокатор VMA
add_subdirectory(external/VMA)
target_link_libraries(MyTarget PRIVATE GPUOpen::VulkanMemoryAllocator)
```

### 8.2 Требуемые расширения Vulkan

```cmake
# Обязательные расширения Vulkan
set(VULKAN_REQUIRED_FEATURES
    VK_KHR_dynamic_rendering
    VK_KHR_synchronization2
    VK_EXT_descriptor_buffer
    VK_EXT_mesh_shader
    VK_KHR_timeline_semaphore
    VK_KHR_spirv_1_4
)

# Верификация расширений на этапе конфигурации
foreach(FEATURE ${VULKAN_REQUIRED_FEATURES})
    message(STATUS "Требуемое расширение Vulkan: ${FEATURE}")
endforeach()
```

---

## 9. Toolchain-файл Clang

### 9.1 Clang.cmake

```cmake
# cmake/toolchains/Clang.cmake
# Конфигурация toolchain для Clang 18+

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# C++26 с libc++
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++26 -stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")

# Поддержка модулей
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmodules")

# Конфигурация предупреждений
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror")

# Интеграция clang-tidy
set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-config-file=${CMAKE_SOURCE_DIR}/.clang-tidy")

# Интеграция clang-format
set(CMAKE_CXX_CLANG_FORMAT "clang-format;-style=file")
```

---

## 10. Требования соответствия

### 10.1 Обязательные требования

1. Версия CMake ДОЛЖНА быть 3.30 или выше
2. Стандарт C++ ДОЛЖЕН быть C++26 с `CMAKE_CXX_EXTENSIONS OFF`
3. Сканирование модулей ДОЛЖНО быть включено через `CMAKE_CXX_SCAN_FOR_MODULES`
4. Все внешние C++ библиотеки ДОЛЖНЫ быть изолированы через PIMPL в файлах `.cpp`
5. Clang 18+ ДОЛЖЕН использоваться как эталонный компилятор

### 10.2 Запрещённые практики

1. `CMAKE_CXX_EXTENSIONS ON` — совместимость с Microsoft ABI НЕ требуется
2. Глобальные `include_directories()` — использовать `target_include_directories()`
3. Глобальные `add_definitions()` — использовать `target_compile_definitions()`
4. Ручное управление файлами `.pcm` — CMake должен управлять компиляцией модулей
5. `#include` в интерфейсных файлах модулей (`.cppm`) для C++ библиотек

---

## 11. История редакций

| Версия | Дата       | Автор                 | Изменения                   |
|--------|------------|-----------------------|-----------------------------|
| 1.0.0  | 22.02.2026 | Архитектурная команда | Первоначальная спецификация |

---

## 12. Связанные документы

- [СТВ-CPP-001: Стандарт языка C++](../cpp/00_language-standard.md)
- [СТВ-GIT-001: Стандарт контроля версий](../git/00_version-control-standard.md)
- [ADR-0004: Спецификация сборки и модулей](../../architecture/adr/0004-build-and-modules-spec.md)

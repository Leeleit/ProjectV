# СТВ-CMAKE-003: Стандарт управления зависимостями

---

## 1. Область применения

Настоящий стандарт определяет обязательные требования к управлению внешними зависимостями в ProjectV. Все сторонние
библиотеки и внешние модули ДОЛЖНЫ интегрироваться согласно данной спецификации.

---

## 3. Классификация зависимостей

### 3.1 Методы интеграции

| Метод         | Приоритет            | Применение                          | Совместимость с модулями |
|---------------|----------------------|-------------------------------------|--------------------------|
| Git-сабмодули | 1 (предпочтительный) | Библиотеки с поддержкой CMake       | Полная                   |
| FetchContent  | 2                    | Небольшие библиотеки без сабмодулей | Полная                   |
| find_package  | 3                    | Системные библиотеки, SDK           | Требует обёртку PIMPL    |

### 3.2 Типы библиотек

| Тип                                | Global Module Fragment | Требуется PIMPL | Пример                           |
|------------------------------------|------------------------|-----------------|----------------------------------|
| **C-библиотека**                   | Разрешён               | Нет             | Vulkan, SDL3, VMA, miniaudio     |
| **C++ библиотека (header-only)**   | Запрещён               | Нет             | glm, glaze                       |
| **C++ библиотека (компилируемая)** | Запрещён               | Да              | JoltPhysics, Flecs, ImGui, Tracy |

---

## 4. Git-сабмодули

### 4.1 Структура сабмодулей

Все внешние зависимости ДОЛЖНЫ располагаться в директории `external/`:

```
external/
├── SDL/              # SDL3 — C-библиотека
├── volk/             # Загрузчик Vulkan — C-библиотека
├── VMA/              # Vulkan Memory Allocator — C-библиотека
├── glm/              # Математическая библиотека — header-only C++
├── glaze/            # JSON-сериализация — header-only C++
├── flecs/            # ECS — C++ библиотека (требуется PIMPL)
├── JoltPhysics/      # Физика — C++ библиотека (требуется PIMPL)
├── imgui/            # UI — C++ библиотека (требуется PIMPL)
├── tracy/            # Профилирование — C++ библиотека (требуется PIMPL)
├── fastgltf/         # Загрузчик glTF — C++ библиотека (требуется PIMPL)
├── miniaudio/        # Аудио — C-библиотека
└── doctest/          # Тестирование — header-only C++
```

### 4.2 Команды управления сабмодулями

```bash
# Добавление сабмодуля
git submodule add https://github.com/libsdl-org/SDL external/SDL

# Инициализация всех сабмодулей
git submodule update --init --recursive

# Обновление сабмодуля до последней версии
git submodule update --remote external/SDL

# Удаление сабмодуля
git submodule deinit -f external/SDL
rm -rf .git/modules/external/SDL
git rm -f external/SDL
```

### 4.3 Интеграция с CMake

```cmake
# C-библиотека — прямая интеграция (Global Module Fragment разрешён)
add_subdirectory(external/SDL EXCLUDE_FROM_ALL)
add_subdirectory(external/volk EXCLUDE_FROM_ALL)
add_subdirectory(external/VMA EXCLUDE_FROM_ALL)

# Header-only C++ — интерфейсная библиотека
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE
    ${CMAKE_SOURCE_DIR}/external/glm
)

# C++ библиотека, требующая PIMPL
add_subdirectory(external/flecs EXCLUDE_FROM_ALL)
add_subdirectory(external/JoltPhysics EXCLUDE_FROM_ALL)
```

---

## 5. FetchContent

### 5.1 Объявление FetchContent

```cmake
include(FetchContent)

# C-библиотека
FetchContent_Declare(
    volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)

# Header-only C++
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)

# C++ библиотека
FetchContent_Declare(
    flecs
    GIT_REPOSITORY https://github.com/SanderMertens/flecs.git
    GIT_TAG        v4.0.0
    GIT_SHALLOW    TRUE
)
```

### 5.2 Активация FetchContent

```cmake
# Активация всех объявленных зависимостей
FetchContent_MakeAvailable(volk glm flecs)

# Или индивидуальная активация
FetchContent_GetProperties(volk)
if(NOT volk_POPULATED)
    FetchContent_Populate(volk)
    add_subdirectory(${volk_SOURCE_DIR} ${volk_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
```

### 5.3 Переопределение конфигурации

```cmake
# Переопределение опций перед FetchContent_MakeAvailable
set(FLECS_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(flecs)
```

---

## 6. Интеграция системных пакетов

### 6.1 Vulkan SDK

```cmake
# ОБЯЗАТЕЛЬНО: Vulkan 1.4
find_package(Vulkan 1.4 REQUIRED)

# Проверка версии
if(Vulkan_VERSION VERSION_LESS "1.4.0")
    message(FATAL_ERROR "ProjectV требует Vulkan 1.4+, обнаружена версия ${Vulkan_VERSION}")
endif()

# Использование
target_link_libraries(MyTarget PRIVATE Vulkan::Vulkan)

# Путь к Vulkan SDK
set(VULKAN_SDK $ENV{VULKAN_SDK})
if(NOT VULKAN_SDK)
    message(FATAL_ERROR "Переменная окружения VULKAN_SDK не установлена")
endif()
```

### 6.2 Пользовательские модули поиска

```cmake
# cmake/FindVulkan.cmake (расширенный)
find_package(Vulkan QUIET)

if(Vulkan_FOUND)
    # Проверка требуемых расширений
    include(CheckVulkanExtensions)
    check_vulkan_extensions(
        REQUIRED
            VK_KHR_dynamic_rendering
            VK_KHR_synchronization2
            VK_EXT_descriptor_buffer
    )
endif()
```

---

## 7. Паттерн интеграции PIMPL

### 7.1 Постановка проблемы

C++ библиотеки с шаблонами и макросами НЕ ДОЛЖНЫ включаться в интерфейсные файлы модулей (`.cppm`). Такие включения
нарушают изоляцию модулей и могут привести к нарушениям ODR.

### 7.2 Архитектура PIMPL

```
┌─────────────────────────────────────────────────────────────┐
│  Интерфейс модуля (.cppm)                                   │
│  ─────────────────────                                      │
│  • export module ProjectV.ModuleName;                       │
│  • Только предварительные объявления                        │
│  • struct Impl; std::unique_ptr<Impl> impl_;                │
│  • НИКАКИХ #include для C++ библиотек                       │
└─────────────────────────────────────────────────────────────┘
                          │
                          │ import
                          ▼
┌─────────────────────────────────────────────────────────────┐
│  Реализация модуля (.cpp)                                   │
│  ─────────────────────                                      │
│  • module ProjectV.ModuleName;                              │
│  • #include <Jolt/Jolt.h>                                   │
│  • #include <flecs.h>                                       │
│  • Полное определение struct Impl                           │
└─────────────────────────────────────────────────────────────┘
```

### 7.3 Пример интеграции Flecs

```cmake
# CMakeLists.txt для обёртки Flecs
add_library(ProjectV.ECS.Flecs)

target_sources(ProjectV.ECS.Flecs
    PUBLIC FILE_SET CXX_MODULES FILES
        src/ecs/ProjectV.ECS.Flecs.cppm
    PRIVATE
        src/ecs/ProjectV.ECS.Flecs.cpp
)

target_link_libraries(ProjectV.ECS.Flecs
    PRIVATE
        flecs::flecs_static
)

# PIMPL: Заголовки Flecs видны только в .cpp
target_include_directories(ProjectV.ECS.Flecs
    PRIVATE
        ${CMAKE_SOURCE_DIR}/external/flecs
)
```

### 7.4 Пример интеграции JoltPhysics

```cmake
# CMakeLists.txt для обёртки Jolt
add_library(ProjectV.Physics.Jolt)

target_sources(ProjectV.Physics.Jolt
    PUBLIC FILE_SET CXX_MODULES FILES
        src/physics/ProjectV.Physics.Jolt.cppm
    PRIVATE
        src/physics/ProjectV.Physics.Jolt.cpp
)

target_link_libraries(ProjectV.Physics.Jolt
    PRIVATE
        Jolt
)

# PIMPL: Заголовки Jolt видны только в .cpp
target_include_directories(ProjectV.Physics.Jolt
    PRIVATE
        ${CMAKE_SOURCE_DIR}/external/JoltPhysics
)
```

### 7.5 Пример интеграции ImGui

```cmake
# CMakeLists.txt для обёртки ImGui
add_library(ProjectV.UI.ImGui)

target_sources(ProjectV.UI.ImGui
    PUBLIC FILE_SET CXX_MODULES FILES
        src/ui/ProjectV.UI.ImGui.cppm
    PRIVATE
        src/ui/ProjectV.UI.ImGui.cpp
        ${CMAKE_SOURCE_DIR}/external/imgui/imgui.cpp
        ${CMAKE_SOURCE_DIR}/external/imgui/imgui_draw.cpp
        ${CMAKE_SOURCE_DIR}/external/imgui/imgui_tables.cpp
        ${CMAKE_SOURCE_DIR}/external/imgui/imgui_widgets.cpp
)

# PIMPL: Заголовки ImGui видны только в .cpp
target_include_directories(ProjectV.UI.ImGui
    PRIVATE
        ${CMAKE_SOURCE_DIR}/external/imgui
)
```

---

## 8. Интеграция Glaze (рефлексия/сериализация)

### 8.1 Требования к изоляции Glaze

Glaze использует макросы для рефлексии (`glz::object`, `glz::meta`). Эти макросы ДОЛЖНЫ быть изолированы в файлах
реализации `.cpp` для предотвращения загрязнения модулей.

### 8.2 Архитектура интеграции Glaze

```cmake
# CMakeLists.txt для модуля сериализации
add_library(ProjectV.Core.Serialization)

target_sources(ProjectV.Core.Serialization
    PUBLIC FILE_SET CXX_MODULES FILES
        src/core/ProjectV.Core.Serialization.cppm
    PRIVATE
        src/core/ProjectV.Core.Serialization.cpp
)

# Glaze как header-only библиотека
target_include_directories(ProjectV.Core.Serialization
    PRIVATE
        ${CMAKE_SOURCE_DIR}/external/glaze/include
)
```

### 8.3 Паттерн PIMPL для Glaze

```cpp
// ProjectV.Core.Serialization.cppm
export module ProjectV.Core.Serialization;

import std;

// Предварительные объявления — БЕЗ glaze includes
namespace glz {
    struct context;
}

export namespace projectv::core {

/// Тип результата сериализации
export template<typename T>
using SerializeResult = std::expected<std::string, SerializationError>;

/// Тип результата десериализации
export template<typename T>
using DeserializeResult = std::expected<T, SerializationError>;

/// JSON-сериализатор — обёртка PIMPL
export class JsonSerializer {
public:
    JsonSerializer() noexcept;
    ~JsonSerializer() noexcept;

    JsonSerializer(JsonSerializer&&) noexcept;
    JsonSerializer& operator=(JsonSerializer&&) noexcept;
    JsonSerializer(const JsonSerializer&) = delete;
    JsonSerializer& operator=(const JsonSerializer&) = delete;

    template<typename T>
    [[nodiscard]] auto serialize(T const& value) const noexcept
        -> SerializeResult<T>;

    template<typename T>
    [[nodiscard]] auto deserialize(std::string_view json) const noexcept
        -> DeserializeResult<T>;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::core
```

```cpp
// ProjectV.Core.Serialization.cpp
module ProjectV.Core.Serialization;

// Заголовок Glaze ЗДЕСЬ — не в .cppm
#include <glaze/glaze.hpp>

import std;

namespace projectv::core {

struct JsonSerializer::Impl {
    glz::context ctx;
};

JsonSerializer::JsonSerializer() noexcept
    : impl_(std::make_unique<Impl>()) {}

JsonSerializer::~JsonSerializer() noexcept = default;

JsonSerializer::JsonSerializer(JsonSerializer&&) noexcept = default;
JsonSerializer& JsonSerializer::operator=(JsonSerializer&&) noexcept = default;

template<typename T>
auto JsonSerializer::serialize(T const& value) const noexcept
    -> SerializeResult<T> {
    std::string buffer;
    auto err = glz::write_json(value, buffer);
    if (err) {
        return std::unexpected(SerializationError{
            .code = static_cast<uint32_t>(err),
            .message = "Ошибка сериализации"
        });
    }
    return buffer;
}

template<typename T>
auto JsonSerializer::deserialize(std::string_view json) const noexcept
    -> DeserializeResult<T> {
    T value;
    auto err = glz::read_json(value, json);
    if (err) {
        return std::unexpected(SerializationError{
            .code = static_cast<uint32_t>(err),
            .message = "Ошибка десериализации"
        });
    }
    return value;
}

// Явные инстанциации шаблонов
template auto JsonSerializer::serialize<glm::vec3>(glm::vec3 const&) const noexcept
    -> SerializeResult<glm::vec3>;
template auto JsonSerializer::deserialize<glm::vec3>(std::string_view) const noexcept
    -> DeserializeResult<glm::vec3>;

} // namespace projectv::core
```

### 8.4 Сериализация компонентов с Glaze

```cpp
// Определения компонентов с метаданными (в файле .cpp)
module ProjectV.Gameplay.Components;

#include <glaze/glaze.hpp>  // ТОЛЬКО в .cpp

import std;
import glm;

namespace projectv::gameplay {

// Компонент определён в модуле
export struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

} // namespace projectv::gameplay

// Метаданные Glaze в том же файле .cpp (не экспортируются)
namespace glz {

template<>
struct meta<projectv::gameplay::TransformComponent> {
    using T = projectv::gameplay::TransformComponent;
    static constexpr auto value = object(
        "position", &T::position,
        "rotation", &T::rotation,
        "scale", &T::scale
    );
};

} // namespace glz
```

---

## 9. Требования к версиям зависимостей

### 9.1 Минимальные версии

| Зависимость | Минимальная версия | Тип             |
|-------------|--------------------|-----------------|
| SDL3        | 3.0.0              | C-библиотека    |
| volk        | 1.3.280            | C-библиотека    |
| VMA         | 3.1.0              | C-библиотека    |
| glm         | 1.0.0              | Header-only C++ |
| glaze       | 2.0.0              | Header-only C++ |
| flecs       | 4.0.0              | C++ (PIMPL)     |
| JoltPhysics | 5.0.0              | C++ (PIMPL)     |
| ImGui       | 1.90.0             | C++ (PIMPL)     |
| Tracy       | 0.10.0             | C++ (PIMPL)     |
| fastgltf    | 0.8.0              | C++ (PIMPL)     |
| miniaudio   | 0.11.0             | C-библиотека    |
| doctest     | 2.4.0              | Header-only C++ |

### 9.2 Верификация версий

```cmake
# Функция верификации версии зависимости
function(verify_dependency_version NAME MIN_VERSION ACTUAL_VERSION)
    if(ACTUAL_VERSION VERSION_LESS MIN_VERSION)
        message(FATAL_ERROR
            "Для ${NAME} требуется версия ${MIN_VERSION}+, обнаружена ${ACTUAL_VERSION}"
        )
    endif()
    message(STATUS "${NAME}: версия ${ACTUAL_VERSION} (OK)")
endfunction()

# Использование
verify_dependency_version("Vulkan" "1.4" ${Vulkan_VERSION})
```

---

## 10. Требования соответствия

### 10.1 Обязательные требования

1. Все внешние зависимости ДОЛЖНЫ располагаться в директории `external/`
2. C++ библиотеки с шаблонами ДОЛЖНЫ использовать паттерн PIMPL
3. `#include` для C++ библиотек НЕ ДОЛЖНЫ появляться в файлах `.cppm`
4. Макросы Glaze ДОЛЖНЫ быть изолированы в файлах реализации `.cpp`
5. Версии зависимостей ДОЛЖНЫ соответствовать минимальным требованиям

### 10.2 Запрещённые практики

1. `#include <Jolt/Jolt.h>` в файлах `.cppm`
2. `#include <flecs.h>` в файлах `.cppm`
3. `#include <imgui.h>` в файлах `.cppm`
4. `#include <glaze/glaze.hpp>` в файлах `.cppm`
5. Прямое раскрытие типов C++ библиотек в интерфейсах модулей

---

## 11. Конфигурация флагов Clang для C++26 экспериментальных фич

### 11.1 Обязательные флаги компилятора

```cmake
# cmake/CompilerOptions.cmake

# === C++26 Standard Configuration ===
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Базовые флаги C++26
    add_compile_options(
        -std=c++26                    # Стандарт C++26
        -stdlib=libc++                # LLVM стандартная библиотека
    )

    # Модули C++26
    add_compile_options(
        -fmodules                     # Включение поддержки модулей
        -fmodule-file=std=std.pcm     # Предкомпилированный модуль std
        -fprebuilt-module-path=${CMAKE_BINARY_DIR}/modules  # Путь к PCM файлам
    )

    # Предупреждения
    add_compile_options(
        -Wall                         # Все базовые предупреждения
        -Wextra                       # Дополнительные предупреждения
        -Wpedantic                    # Строгое соответствие ISO C++
        -Werror                       # Предупреждения как ошибки
        -Wno-unused-command-line-argument  # Обход для модулей
    )

    # Экспериментальные фичи C++26
    add_compile_options(
        -fexperimental-library        # Экспериментные возможности libc++
        -freflection                  # P2996 Static Reflection (экспериментально)
    )

    # Линковка
    add_link_options(
        -stdlib=libc++
        -lc++abi
    )
endif()
```

### 11.2 Конфигурация для Debug/Release

```cmake
# Debug-специфичные флаги
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -O0")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer")

# Release-специфичные флаги
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto=thin")

# Clang-специфичные оптимизации
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ffast-math")
endif()
```

### 11.3 Предкомпиляция модуля std

```cmake
# cmake/StdModule.cmake

function(precompile_std_module)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        return()
    endif()

    set(STD_MODULE_SOURCE "${CMAKE_BINARY_DIR}/std_module.cppm")

    # Генерация файла модуля std
    file(WRITE "${STD_MODULE_SOURCE}"
"module;
#include <std.h>
export module std;
"
    )

    # Компиляция PCM
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/std.pcm"
        COMMAND ${CMAKE_CXX_COMPILER}
            -std=c++26
            -stdlib=libc++
            -fmodules
            -c "${STD_MODULE_SOURCE}"
            -o "${CMAKE_BINARY_DIR}/std.pcm"
        DEPENDS "${STD_MODULE_SOURCE}"
        COMMENT "Precompiling std module"
        VERBATIM
    )

    add_custom_target(std_module
        DEPENDS "${CMAKE_BINARY_DIR}/std.pcm"
    )
endfunction()
```

---

## 12. Полный CMakeLists.txt манифест

### 12.1 Корневой CMakeLists.txt

```cmake
# CMakeLists.txt (root)
cmake_minimum_required(VERSION 3.30)

# === Project Definition ===
project(ProjectV
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "Voxel Engine with Vulkan 1.4"
)

# === C++26 Configuration ===
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# === Module Support ===
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS "clang-scan-deps")

# === Build Type ===
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# === CMake Modules ===
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(StdModule)
include(ModuleSupport)

# === Dependencies ===
# Vulkan 1.4 (required)
find_package(Vulkan 1.4 REQUIRED)

# Git submodules (external/)
add_subdirectory(external/volk EXCLUDE_FROM_ALL)
add_subdirectory(external/VMA EXCLUDE_FROM_ALL)
add_subdirectory(external/SDL EXCLUDE_FROM_ALL)
add_subdirectory(external/flecs EXCLUDE_FROM_ALL)
add_subdirectory(external/JoltPhysics EXCLUDE_FROM_ALL)
add_subdirectory(external/glm EXCLUDE_FROM_ALL)
add_subdirectory(external/glaze EXCLUDE_FROM_ALL)
add_subdirectory(external/tracy EXCLUDE_FROM_ALL)
add_subdirectory(external/fastgltf EXCLUDE_FROM_ALL)
add_subdirectory(external/draco EXCLUDE_FROM_ALL)
add_subdirectory(external/meshoptimizer EXCLUDE_FROM_ALL)

# === Precompile std module ===
precompile_std_module()

# === Source Directories ===
add_subdirectory(src)

# === Optional: Tests ===
option(PROJECTV_BUILD_TESTS "Build test suite" OFF)
if(PROJECTV_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# === Install Rules ===
install(TARGETS ProjectV.Runtime
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
```

### 12.2 CMakeLists.txt для модуля

```cmake
# src/core/CMakeLists.txt

add_library(ProjectV.Core)

# === Module Interface Files ===
target_sources(ProjectV.Core PUBLIC
    FILE_SET CXX_MODULES FILES
        ProjectV.Core.cppm
        ProjectV.Core.Memory.cppm
        ProjectV.Core.Containers.cppm
        ProjectV.Core.Jobs.cppm
        ProjectV.Core.Platform.cppm
)

# === Implementation Files (PIMPL) ===
target_sources(ProjectV.Core PRIVATE
    ProjectV.Core.Memory.cpp
    ProjectV.Core.Containers.cpp
    ProjectV.Core.Jobs.cpp
    ProjectV.Core.Platform.cpp
)

# === C-библиотеки: Global Module Fragment разрешён ===
# SDL3 — C-библиотека, можно включать в GMF
target_link_libraries(ProjectV.Core
    PUBLIC
        SDL3::SDL3-static
)

# === C++ библиотеки: PIMPL обязателен ===
# Flecs, Jolt, ImGui — ONLY в .cpp файлах!

# === Compile Definitions ===
target_compile_definitions(ProjectV.Core
    PRIVATE
        $<$<CONFIG:Debug>:TRACY_ENABLE>
)

# === Include Directories ===
target_include_directories(ProjectV.Core
    PRIVATE
        ${CMAKE_SOURCE_DIR}/external/tracy/public/tracy
)

# === Module Dependencies ===
# Зависимости от других модулей ProjectV
# target_link_libraries(ProjectV.Core PUBLIC ProjectV.Math)
```

### 12.3 Иерархия модулей

```
Module Dependency Graph
─────────────────────────────────────────────────────────────
Level 0 (Foundation):
    ProjectV.Core.Platform    ── SDL3 (C)
    ProjectV.Render.Vulkan    ── Vulkan, VMA, volk (C)

Level 1 (Core):
    ProjectV.Core.Memory      ── Level 0
    ProjectV.Core.Containers
    ProjectV.Math             ── glm (header-only)

Level 2 (Wrappers — PIMPL required):
    ProjectV.ECS.Flecs        ── flecs (C++, PIMPL)
    ProjectV.Physics.Jolt     ── JoltPhysics (C++, PIMPL)
    ProjectV.UI.ImGui         ── imgui (C++, PIMPL)
    ProjectV.Profile.Tracy    ── tracy (C++, PIMPL)

Level 3 (Domain):
    ProjectV.Voxel.SVO
    ProjectV.Voxel.Chunk
    ProjectV.Render.SVO

Level 4 (Application):
    ProjectV.App              ── Level 3
─────────────────────────────────────────────────────────────
```

---

## 13. Техники изоляции макросов при линковке к модулям

### 13.1 Проблема

Макросы из C++ библиотек (Flecs макросы для компонентов, Jolt макросы регистрации, ImGui ID generation) не должны "
утекать" в интерфейс модуля (`.cppm`), так как:

1. Нарушают изоляцию модуля
2. Могут конфликтовать с другими макросами
3. Усложняют бинарную совместимость

### 13.2 Решение: PIMPL с wrapper-функциями

```cpp
// === ProjectV.ECS.Flecs.cppm (Interface) ===
export module ProjectV.ECS.Flecs;

import std;

// НИКАКИХ #include <flecs.h> здесь!

export namespace projectv::ecs {

/// Opaque entity ID.
export using EntityId = uint64_t;

/// Forward declarations only.
export class World;
export class Entity;
export class SystemBuilder;

/// World — PIMPL wrapper для flecs::world.
export class World {
public:
    World() noexcept;
    explicit World(uint32_t thread_count) noexcept;
    ~World() noexcept;

    World(World&&) noexcept;
    World& operator=(World&&) noexcept;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    /// Создаёт сущность.
    [[nodiscard]] auto entity() noexcept -> EntityId;

    /// Создаёт именованную сущность.
    [[nodiscard]] auto entity(std::string_view name) noexcept -> EntityId;

    /// Уничтожает сущность.
    auto destroy(EntityId id) noexcept -> void;

    /// Выполняет шаг симуляции.
    [[nodiscard]] auto progress(float delta_time) noexcept -> bool;

    /// Устанавливает компонент (type-erased internally).
    template<typename T>
    auto set(EntityId id, T const& component) noexcept -> void;

    /// Получает компонент.
    template<typename T>
    [[nodiscard]] auto get(EntityId id) const noexcept -> T const*;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::ecs
```

```cpp
// === ProjectV.ECS.Flecs.cpp (Implementation) ===
module ProjectV.ECS.Flecs;

// Flecs headers ЗДЕСЬ — не в .cppm!
#include <flecs.h>
#include <flecs/addons/cpp/world.hpp>
#include <flecs/addons/cpp/entity.hpp>

import std;

namespace projectv::ecs {

// Внутренняя реализация
struct World::Impl {
    flecs::world world;

    Impl() = default;
    explicit Impl(uint32_t thread_count)
        : world(flecs::world().set_threads(thread_count))
    {}
};

World::World() noexcept
    : impl_(std::make_unique<Impl>()) {}

World::World(uint32_t thread_count) noexcept
    : impl_(std::make_unique<Impl>(thread_count)) {}

World::~World() noexcept = default;

World::World(World&&) noexcept = default;
World& World::operator=(World&&) noexcept = default;

auto World::entity() noexcept -> EntityId {
    return impl_->world.entity().id();
}

auto World::entity(std::string_view name) noexcept -> EntityId {
    return impl_->world.entity(name.data()).id();
}

auto World::destroy(EntityId id) noexcept -> void {
    impl_->world.entity(static_cast<flecs::entity_t>(id)).destruct();
}

auto World::progress(float delta_time) noexcept -> bool {
    return impl_->world.progress(delta_time);
}

// Явные инстанциации для известных типов компонентов
template auto World::set<TransformComponent>(EntityId, TransformComponent const&) noexcept -> void;
template auto World::get<TransformComponent>(EntityId) const noexcept -> TransformComponent const*;

} // namespace projectv::ecs
```

### 13.3 Техника: Global Module Fragment для C-библиотек

```cpp
// === ProjectV.Render.Vulkan.cppm ===
module;

// Global Module Fragment — для C-библиотек
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

export module ProjectV.Render.Vulkan;

import std;
import glm;

// Vulkan и VMA — C-библиотеки, макросы не утекают
// (Vulkan использует vk-префикс, VMA использует vma-префикс)

export namespace projectv::render::vulkan {

export class VulkanContext {
    // VkHandle типы безопасны для использования в интерфейсе
    // так как это typedef для uint64_t/void*
};

} // namespace projectv::render::vulkan
```

### 13.4 Техника: Compile Definitions Isolation

```cmake
# Изоляция макросов через target_compile_definitions

# Flecs требует FLECS_NO_CPP17 для совместимости
# Но мы не хотим "утечь" этот макрос в другие модули

target_compile_definitions(ProjectV.ECS.Flecs
    PRIVATE
        FLECS_NO_CPP17=1
        FLECS_USE_OS_ALLOC=0
)

# Tracy требует TRACY_ENABLE
target_compile_definitions(ProjectV.Profile.Tracy
    PRIVATE
        TRACY_ENABLE=1
)

# Jolt требует определённые макросы
target_compile_definitions(ProjectV.Physics.Jolt
    PRIVATE
        JPH_DEBUG_RENDERER=0
        JPH_PROFILE_ENABLED=1
)
```

### 13.5 Техника: Wrapper Functions для макро-генерируемого кода

```cpp
// === Проблема: Flecs макросы для компонентов ===
// В Flecs компоненты регистрируются макросами:
// ECS_COMPONENT(world, TransformComponent)

// === Решение: Регистрация в .cpp файле ===

// ProjectV.ECS.Flecs.cpp
module ProjectV.ECS.Flecs;

#include <flecs.h>

import std;
import ProjectV.Gameplay.Components;  // Наш модуль с компонентами

namespace projectv::ecs {

// Внутренняя функция регистрации
auto register_builtin_components(flecs::world& world) -> void {
    // Регистрация через Flecs API (не макросы)
    world.component<TransformComponent>()
        .member<glm::vec3>("position")
        .member<glm::quat>("rotation")
        .member<glm::vec3>("scale");

    world.component<VelocityComponent>()
        .member<glm::vec3>("linear")
        .member<glm::vec3>("angular");
}

// Export wrapper для клиентского кода
auto World::register_component_type(std::string_view name,
                                     std::vector<ComponentMember> const& members)
    -> void {
    // Wrapper для динамической регистрации
    impl_->world.component(name.data());
    for (auto const& m : members) {
        // ...
    }
}

} // namespace projectv::ecs
```

---

## 14. Пример интеграции SDL3 с Global Module Fragment

### 14.1 Модуль Platform

```cpp
// === ProjectV.Core.Platform.cppm ===
module;

// Global Module Fragment — SDL3 это C-библиотека
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

export module ProjectV.Core.Platform;

import std;
import glm;

export namespace projectv::platform {

/// Коды ошибок платформы.
export enum class PlatformError : uint8_t {
    SDLInitFailed,
    WindowCreationFailed,
    VulkanSurfaceFailed,
    EventQueueOverflow
};

/// Конфигурация окна.
export struct WindowConfig {
    std::string title{"ProjectV"};
    uint32_t width{1920};
    uint32_t height{1080};
    bool fullscreen{false};
    bool vsync{true};
    bool resizable{true};
};

/// Window handle (opaque).
export struct WindowHandle {
    SDL_Window* native{nullptr};

    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return native != nullptr;
    }
};

/// Platform subsystem — wrapper для SDL3.
///
/// ## Invariants
/// - SDL инициализирован после успешного init()
/// - window_ валиден после успешного create_window()
///
/// ## Thread Safety
/// - event processing должен вызываться из main thread
/// - window operations могут вызываться из любого потока (SDL thread-safe)
export class PlatformSubsystem {
public:
    PlatformSubsystem() noexcept;
    ~PlatformSubsystem() noexcept;

    PlatformSubsystem(PlatformSubsystem&&) noexcept;
    PlatformSubsystem& operator=(PlatformSubsystem&&) noexcept;
    PlatformSubsystem(const PlatformSubsystem&) = delete;
    PlatformSubsystem& operator=(const PlatformSubsystem&) = delete;

    /// Инициализирует SDL3.
    ///
    /// @pre SDL не инициализирован
    /// @post SDL готов к использованию
    [[nodiscard]] auto init() noexcept
        -> std::expected<void, PlatformError>;

    /// Создаёт окно.
    ///
    /// @pre init() успешно вызван
    /// @post window() возвращает валидный handle
    [[nodiscard]] auto create_window(WindowConfig const& config) noexcept
        -> std::expected<WindowHandle, PlatformError>;

    /// Обрабатывает события.
    ///
    /// @return true если приложение должно продолжаться
    [[nodiscard]] auto process_events() noexcept -> bool;

    /// Получает Vulkan extensions для SDL.
    ///
    /// @pre create_window() успешно вызван
    [[nodiscard]] auto get_vulkan_extensions() const noexcept
        -> std::expected<std::vector<char const*>, PlatformError>;

    /// Создаёт Vulkan surface.
    ///
    /// @param instance Vulkan instance
    /// @pre create_window() успешно вызван
    [[nodiscard]] auto create_vulkan_surface(VkInstance instance) noexcept
        -> std::expected<VkSurfaceKHR, PlatformError>;

    /// Получает размер окна.
    [[nodiscard]] auto window_size() const noexcept -> glm::ivec2;

    /// Получает native window handle.
    [[nodiscard]] auto native_window_handle() const noexcept -> void*;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace projectv::platform
```

### 14.2 Реализация Platform

```cpp
// === ProjectV.Core.Platform.cpp ===
module ProjectV.Core.Platform;

// SDL3 уже включён в .cppm через Global Module Fragment
// Но можем включить дополнительные заголовки здесь
#include <SDL3/SDL_syswm.h>

import std;
import glm;

namespace projectv::platform {

struct PlatformSubsystem::Impl {
    SDL_Window* window{nullptr};
    bool sdl_initialized{false};
    bool should_quit{false};
};

PlatformSubsystem::PlatformSubsystem() noexcept
    : impl_(std::make_unique<Impl>()) {}

PlatformSubsystem::~PlatformSubsystem() noexcept {
    if (impl_->window) {
        SDL_DestroyWindow(impl_->window);
    }
    if (impl_->sdl_initialized) {
        SDL_Quit();
    }
}

PlatformSubsystem::PlatformSubsystem(PlatformSubsystem&&) noexcept = default;
PlatformSubsystem& PlatformSubsystem::operator=(PlatformSubsystem&&) noexcept = default;

auto PlatformSubsystem::init() noexcept
    -> std::expected<void, PlatformError> {

    if (impl_->sdl_initialized) {
        return {};
    }

    // SDL3 initialization
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        return std::unexpected(PlatformError::SDLInitFailed);
    }

    impl_->sdl_initialized = true;
    return {};
}

auto PlatformSubsystem::create_window(WindowConfig const& config) noexcept
    -> std::expected<WindowHandle, PlatformError> {

    if (!impl_->sdl_initialized) {
        return std::unexpected(PlatformError::SDLInitFailed);
    }

    SDL_WindowFlags flags = SDL_WINDOW_VULKAN;
    if (config.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    if (config.fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }

    impl_->window = SDL_CreateWindow(
        config.title.c_str(),
        static_cast<int>(config.width),
        static_cast<int>(config.height),
        flags
    );

    if (!impl_->window) {
        return std::unexpected(PlatformError::WindowCreationFailed);
    }

    return WindowHandle{impl_->window};
}

auto PlatformSubsystem::process_events() noexcept -> bool {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                impl_->should_quit = true;
                return false;

            case SDL_EVENT_WINDOW_RESIZE:
                // Handle resize
                break;

            default:
                break;
        }
    }
    return !impl_->should_quit;
}

auto PlatformSubsystem::get_vulkan_extensions() const noexcept
    -> std::expected<std::vector<char const*>, PlatformError> {

    if (!impl_->window) {
        return std::unexpected(PlatformError::WindowCreationFailed);
    }

    Uint32 count;
    char const* const* extensions = SDL_Vulkan_GetInstanceExtensions(&count);

    if (!extensions) {
        return std::unexpected(PlatformError::VulkanSurfaceFailed);
    }

    return std::vector<char const*>(extensions, extensions + count);
}

auto PlatformSubsystem::create_vulkan_surface(VkInstance instance) noexcept
    -> std::expected<VkSurfaceKHR, PlatformError> {

    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(impl_->window, instance, nullptr, &surface)) {
        return std::unexpected(PlatformError::VulkanSurfaceFailed);
    }

    return surface;
}

auto PlatformSubsystem::window_size() const noexcept -> glm::ivec2 {
    int w, h;
    SDL_GetWindowSize(impl_->window, &w, &h);
    return {w, h};
}

auto PlatformSubsystem::native_window_handle() const noexcept -> void* {
    SDL_SysWMinfo wmInfo;
    SDL_GetWindowWMInfo(impl_->window, &wmInfo);

#if defined(_WIN32)
    return reinterpret_cast<void*>(wmInfo.info.win.hwnd);
#elif defined(__linux__)
    return reinterpret_cast<void*>(wmInfo.info.x11.window);
#else
    return nullptr;
#endif
}

} // namespace projectv::platform
```

### 14.3 CMakeLists.txt для Platform

```cmake
# src/core/platform/CMakeLists.txt

add_library(ProjectV.Core.Platform)

target_sources(ProjectV.Core.Platform PUBLIC
    FILE_SET CXX_MODULES FILES
        ProjectV.Core.Platform.cppm
    PRIVATE
        ProjectV.Core.Platform.cpp
)

# SDL3 — C-библиотека, безопасна для Global Module Fragment
target_link_libraries(ProjectV.Core.Platform
    PUBLIC
        SDL3::SDL3-static
)

# Include directories для SDL3
target_include_directories(ProjectV.Core.Platform
    PRIVATE
        ${CMAKE_SOURCE_DIR}/external/SDL/include
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(ProjectV.Core.Platform PRIVATE
        user32
        gdi32
        winmm
        imm32
        ole32
        oleaut32
        version
        setupapi
    )
elseif(UNIX AND NOT APPLE)
    target_link_libraries(ProjectV.Core.Platform PRIVATE
        dl
        pthread
    )
endif()
```

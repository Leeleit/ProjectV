# СТВ-CMAKE-006: Стандарт кросс-платформенной сборки

**Идентификатор документа:** СТВ-CMAKE-006
**Версия:** 1.0.0
**Статус:** Утверждён
**Дата введения:** 22.02.2026
**Классификация:** Технический стандарт

---

## 1. Область применения

Настоящий стандарт определяет требования к кросс-платформенной сборке ProjectV. Все платформенно-зависимые конфигурации,
toolchain-файлы и вопросы переносимости ДОЛЖНЫ соответствовать данной спецификации.

---

## 2. Нормативные ссылки

- ISO/IEC 14882:2026 (C++26)
- Документация CMake 3.30
- Руководство по компилятору Clang 18+
- Спецификация Vulkan 1.4

---

## 3. Поддерживаемые платформы

### 3.1 Основные платформы

| Платформа             | Архитектура   | Статус    | Приоритет |
|-----------------------|---------------|-----------|-----------|
| Windows 11            | x86_64        | Основная  | P0        |
| Linux (Ubuntu 24.04+) | x86_64        | Основная  | P0        |
| macOS 14+             | ARM64 (M1/M2) | Вторичная | P1        |

### 3.2 Определение платформы

```cmake
# Идентификация платформы
if(WIN32)
    set(PROJECTV_PLATFORM "windows")
    set(PROJECTV_PLATFORM_WINDOWS TRUE)
elseif(UNIX AND NOT APPLE)
    set(PROJECTV_PLATFORM "linux")
    set(PROJECTV_PLATFORM_LINUX TRUE)
elseif(APPLE)
    set(PROJECTV_PLATFORM "macos")
    set(PROJECTV_PLATFORM_MACOS TRUE)
endif()

# Определение архитектуры
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    set(PROJECTV_ARCH "x86_64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
    set(PROJECTV_ARCH "arm64")
endif()
```

---

## 4. Требования к компилятору по платформам

### 4.1 Windows (Clang 18+)

```cmake
# Конфигурация Clang для Windows
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Совместимость с MSVC ABI не требуется
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++26 -stdlib=libc++")

    # Специфичные для Windows определения
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )

    # Линковка с libc++
    add_link_options(-stdlib=libc++ -lc++abi)
endif()
```

### 4.2 Linux (Clang 18+)

```cmake
# Конфигурация Clang для Linux
if(UNIX AND NOT APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++26 -stdlib=libc++")

    # Специфичные для Linux флаги
    add_compile_options(
        -fPIC
        -pthread
    )

    # Линковка с libc++
    add_link_options(-stdlib=libc++ -lc++abi)

    # Требуемые библиотеки
    target_link_libraries(ProjectV.Core PUBLIC dl pthread)
endif()
```

### 4.3 macOS (Clang 18+)

```cmake
# Конфигурация Clang для macOS
if(APPLE)
    # Минимальная версия macOS
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0")

    # Архитектура
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)

    # libc++ используется по умолчанию на macOS
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++26")

    # Специфичные для macOS фреймворки
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)

    target_link_libraries(ProjectV.Core
        PUBLIC
            ${FOUNDATION_FRAMEWORK}
    )
endif()
```

---

## 5. Конфигурация Vulkan по платформам

### 5.1 Vulkan для Windows

```cmake
if(WIN32)
    # Vulkan SDK
    find_package(Vulkan 1.4 REQUIRED)

    # Volk для статической линковки
    add_subdirectory(external/volk)

    target_link_libraries(ProjectV.Render
        PRIVATE
            volk::volk
            Vulkan::Vulkan
    )

    # Специфичные для Windows определения Vulkan
    target_compile_definitions(ProjectV.Render PRIVATE
        VK_USE_PLATFORM_WIN32_KHR
    )
endif()
```

### 5.2 Vulkan для Linux

```cmake
if(UNIX AND NOT APPLE)
    # Vulkan SDK
    find_package(Vulkan 1.4 REQUIRED)

    # Volk для статической линковки
    add_subdirectory(external/volk)

    # Поддержка X11/Wayland
    find_package(X11 QUIET)
    find_package(Wayland QUIET)

    if(X11_FOUND)
        target_compile_definitions(ProjectV.Render PRIVATE
            VK_USE_PLATFORM_XLIB_KHR
        )
        target_link_libraries(ProjectV.Render PRIVATE ${X11_LIBRARIES})
    endif()

    if(Wayland_FOUND)
        target_compile_definitions(ProjectV.Render PRIVATE
            VK_USE_PLATFORM_WAYLAND_KHR
        )
        target_link_libraries(ProjectV.Render PRIVATE Wayland::client)
    endif()

    target_link_libraries(ProjectV.Render PRIVATE volk::volk Vulkan::Vulkan)
endif()
```

### 5.3 Vulkan для macOS (MoltenVK)

```cmake
if(APPLE)
    # Vulkan SDK с MoltenVK
    find_package(Vulkan 1.4 REQUIRED)

    # Конфигурация MoltenVK
    target_compile_definitions(ProjectV.Render PRIVATE
        VK_USE_PLATFORM_MACOS_MVK
        VK_ENABLE_BETA_EXTENSIONS
    )

    target_link_libraries(ProjectV.Render PRIVATE Vulkan::Vulkan)
endif()
```

---

## 6. Конфигурация SDL3 по платформам

### 6.1 Платформенно-зависимая настройка SDL

```cmake
# Конфигурация SDL3
add_subdirectory(external/SDL EXCLUDE_FROM_ALL)

if(WIN32)
    # SDL для Windows
    target_compile_definitions(ProjectV.Core.Platform PRIVATE
        SDL_PLATFORM_WINDOWS
    )
elseif(UNIX AND NOT APPLE)
    # SDL для Linux
    target_compile_definitions(ProjectV.Core.Platform PRIVATE
        SDL_PLATFORM_LINUX
    )
elseif(APPLE)
    # SDL для macOS
    target_compile_definitions(ProjectV.Core.Platform PRIVATE
        SDL_PLATFORM_MACOS
    )
endif()

target_link_libraries(ProjectV.Core.Platform PRIVATE SDL3::SDL3)
```

---

## 7. Toolchain-файлы

### 7.1 Toolchain Clang (Linux/macOS)

```cmake
# cmake/toolchains/Clang.cmake
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# Стандарт C++26
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# libc++
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")

# Поддержка модулей
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmodules")
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# Предупреждения
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror")
```

### 7.2 Toolchain Clang (Windows)

```cmake
# cmake/toolchains/Clang-Windows.cmake
set(CMAKE_C_COMPILER clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)

# Стандарт C++26
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Использование линковщика lld
set(CMAKE_LINKER lld-link)

# Поддержка модулей
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmodules")
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# Специфичные для Windows настройки
add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
```

---

## 8. Платформенно-зависимые исходные файлы

### 8.1 Условный выбор источников

```cmake
# Платформенно-зависимые файлы реализации
set(PLATFORM_SOURCES)

if(WIN32)
    list(APPEND PLATFORM_SOURCES
        src/core/platform/windows/window_win32.cpp
        src/core/platform/windows/thread_win32.cpp
    )
elseif(UNIX AND NOT APPLE)
    list(APPEND PLATFORM_SOURCES
        src/core/platform/linux/window_linux.cpp
        src/core/platform/linux/thread_linux.cpp
    )
elseif(APPLE)
    list(APPEND PLATFORM_SOURCES
        src/core/platform/macos/window_macos.cpp
        src/core/platform/macos/thread_macos.cpp
    )
endif()

target_sources(ProjectV.Core.Platform PRIVATE ${PLATFORM_SOURCES})
```

---

## 9. Пути установки

### 9.1 Платформенно-зависимые пути

```cmake
if(WIN32)
    # Пути установки для Windows
    install(TARGETS ProjectV.Core
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION bin
        ARCHIVE DESTINATION lib
    )
elseif(UNIX)
    # Пути установки для Linux/macOS (соответствие FHS)
    include(GNUInstallDirs)

    install(TARGETS ProjectV.Core
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()
```

---

## 10. Кросс-компиляция

### 10.1 Поддержка кросс-компиляции

```cmake
# Обнаружение кросс-компиляции
if(CMAKE_CROSSCOMPILING)
    message(STATUS "Кросс-компиляция для ${CMAKE_SYSTEM_NAME} (${CMAKE_SYSTEM_PROCESSOR})")

    # Специфичные для кросс-компиляции настройки
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
endif()
```

### 10.2 Toolchain для кросс-компиляции

```cmake
# cmake/toolchains/Linux-ARM64.cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-clang)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-clang++)

set(CMAKE_SYSROOT /usr/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
```

---

## 11. Требования соответствия

### 11.1 Обязательные требования

1. Все платформы ДОЛЖНЫ использовать Clang 18+ в качестве компилятора
2. Все платформы ДОЛЖНЫ использовать libc++ в качестве стандартной библиотеки
3. Определение платформы ДОЛЖНО использовать встроенные переменные CMake
4. Vulkan 1.4 ДОЛЖЕН поддерживаться на всех основных платформах

### 11.2 Запрещённые практики

1. Платформенно-зависимые `#ifdef` в интерфейсных файлах модулей
2. Жёстко заданные пути для внешних зависимостей
3. Зависимость от платформенно-зависимых расширений компилятора
4. Условная компиляция на основе идентификатора компилятора (использовать определение возможностей)

---

## 12. История редакций

| Версия | Дата       | Автор                 | Изменения                   |
|--------|------------|-----------------------|-----------------------------|
| 1.0.0  | 22.02.2026 | Архитектурная команда | Первоначальная спецификация |

---

## 13. Связанные документы

- [СТВ-CMAKE-001: Спецификация системы сборки CMake](00_specification.md)
- [СТВ-CMAKE-002: Стандарт структуры проекта CMake](01_basics-structure.md)
- [ADR-0001: Рендерер Vulkan](../../architecture/adr/0001-vulkan-renderer.md)

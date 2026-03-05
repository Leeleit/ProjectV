# СТВ-CMAKE-004: Стандарт конфигурации сборки

**Идентификатор документа:** СТВ-CMAKE-004
**Версия:** 1.0.0
**Статус:** Утверждён
**Дата введения:** 22.02.2026
**Классификация:** Технический стандарт

---

## 1. Область применения

Настоящий стандарт определяет обязательные настройки конфигурации сборки для ProjectV. Все типы сборки, флаги
компилятора и параметры оптимизации ДОЛЖНЫ соответствовать данной спецификации.

---

## 2. Нормативные ссылки

- ISO/IEC 14882:2026 (C++26)
- Документация CMake 3.30
- Руководство по компилятору Clang 18+
- СТВ-CMAKE-001: Спецификация системы сборки CMake

---

## 3. Типы сборки

### 3.1 Стандартные типы сборки

ProjectV распознаёт четыре стандартных типа сборки:

| Тип сборки     | Идентификатор    | Назначение                       | Оптимизация |
|----------------|------------------|----------------------------------|-------------|
| Debug          | `Debug`          | Разработка, отладка              | `-O0`       |
| Release        | `Release`        | Продакшн                         | `-O3`       |
| RelWithDebInfo | `RelWithDebInfo` | Профилирование, диагностика      | `-O2`       |
| MinSizeRel     | `MinSizeRel`     | Дистрибутив, оптимизация размера | `-Os`       |

### 3.2 Выбор типа сборки

```cmake
# Тип сборки по умолчанию
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Тип сборки" FORCE)
endif()

# Валидация типа сборки
set(ALLOWED_BUILD_TYPES Debug Release RelWithDebInfo MinSizeRel)
if(NOT CMAKE_BUILD_TYPE IN_LIST ALLOWED_BUILD_TYPES)
    message(FATAL_ERROR "Некорректный CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
endif()
```

### 3.3 Пользовательские типы сборки

Пользовательские типы сборки НЕ допускаются. Все конфигурации ДОЛЖНЫ использовать стандартные типы CMake.

---

## 4. Флаги компилятора

### 4.1 Базовые флаги (все конфигурации)

```cmake
# ОБЯЗАТЕЛЬНО: Базовые флаги компилятора для Clang 18+
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(
        -std=c++26           # Стандарт C++26
        -stdlib=libc++       # Стандартная библиотека LLVM
        -fmodules            # Поддержка модулей C++26
        -Wall                # Все предупреждения
        -Wextra              # Дополнительные предупреждения
        -Wpedantic           # Строгое соответствие ISO
        -Werror              # Предупреждения как ошибки
        -Wno-unused-command-line-argument  # Обход для модулей Clang
    )
endif()
```

### 4.2 Конфигурация Debug

```cmake
# Флаги конфигурации Debug
set(CMAKE_CXX_FLAGS_DEBUG ""
    CACHE STRING "Флаги Debug" FORCE
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS_DEBUG
        "-O0 -g3 -gsplit-dwarf"
        CACHE STRING "Флаги Debug" FORCE
    )

    # AddressSanitizer и UndefinedBehaviorSanitizer
    option(ENABLE_ASAN "Включить AddressSanitizer" ON)
    if(ENABLE_ASAN)
        add_compile_options(-fsanitize=address,undefined)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()
```

### 4.3 Конфигурация Release

```cmake
# Флаги конфигурации Release
set(CMAKE_CXX_FLAGS_RELEASE ""
    CACHE STRING "Флаги Release" FORCE
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS_RELEASE
        "-O3 -DNDEBUG -flto=thin -ffunction-sections -fdata-sections"
        CACHE STRING "Флаги Release" FORCE
    )

    # Оптимизация времени линковки
    add_link_options(-flto=thin)

    # Архитектурно-зависимые оптимизации
    option(ENABLE_NATIVE_ARCH "Включить -march=native" ON)
    if(ENABLE_NATIVE_ARCH)
        add_compile_options(-march=native)
    endif()
endif()
```

### 4.4 Конфигурация RelWithDebInfo

```cmake
# Флаги конфигурации RelWithDebInfo
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO ""
    CACHE STRING "Флаги RelWithDebInfo" FORCE
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO
        "-O2 -g2 -DNDEBUG"
        CACHE STRING "Флаги RelWithDebInfo" FORCE
    )
endif()
```

### 4.5 Конфигурация MinSizeRel

```cmake
# Флаги конфигурации MinSizeRel
set(CMAKE_CXX_FLAGS_MINSIZEREL ""
    CACHE STRING "Флаги MinSizeRel" FORCE
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS_MINSIZEREL
        "-Os -DNDEBUG -ffunction-sections -fdata-sections"
        CACHE STRING "Флаги MinSizeRel" FORCE
    )
endif()
```

---

## 5. Флаги линковщика

### 5.1 Базовые флаги линковщика

```cmake
# ОБЯЗАТЕЛЬНО: Линковка с libc++
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_link_options(
        -stdlib=libc++
        -lc++abi
    )
endif()
```

### 5.2 Платформенно-зависимые флаги линковщика

```cmake
# Windows
if(WIN32)
    add_link_options(
        -Wl,--gc-sections
    )
endif()

# Linux
if(UNIX AND NOT APPLE)
    add_link_options(
        -Wl,--gc-sections
        -Wl,--as-needed
    )
endif()

# macOS
if(APPLE)
    add_link_options(
        -Wl,-dead_strip
    )
endif()
```

---

## 6. Определения препроцессора

### 6.1 Глобальные определения

```cmake
# Определения версии
target_compile_definitions(ProjectV.Core
    PUBLIC
        PROJECTV_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        PROJECTV_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        PROJECTV_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# Определения платформы
if(WIN32)
    target_compile_definitions(ProjectV.Core PUBLIC PROJECTV_PLATFORM_WINDOWS)
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(ProjectV.Core PUBLIC PROJECTV_PLATFORM_LINUX)
elseif(APPLE)
    target_compile_definitions(ProjectV.Core PUBLIC PROJECTV_PLATFORM_MACOS)
endif()

# Версия Vulkan
target_compile_definitions(ProjectV.Core
    PUBLIC
        VK_API_VERSION_MAJOR=1
        VK_API_VERSION_MINOR=4
        VK_API_VERSION_PATCH=0
)
```

### 6.2 Специфичные для конфигурации определения

```cmake
# Определения для Debug
target_compile_definitions(ProjectV.Core
    PRIVATE
        $<$<CONFIG:Debug>:PROJECTV_DEBUG PROJECTV_ENABLE_ASSERTIONS>
)

# Определения для Release
target_compile_definitions(ProjectV.Core
    PRIVATE
        $<$<CONFIG:Release>:PROJECTV_RELEASE PROJECTV_DISABLE_ASSERTIONS>
)
```

---

## 7. Настройки оптимизации

### 7.1 Link-Time Optimization (LTO)

```cmake
# Конфигурация LTO
option(ENABLE_LTO "Включить Link-Time Optimization" ON)

if(ENABLE_LTO AND CMAKE_BUILD_TYPE STREQUAL "Release")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        include(CheckIPOSupported)
        check_ipo_supported(RESULT LTO_SUPPORTED OUTPUT LTO_ERROR)

        if(LTO_SUPPORTED)
            set_target_properties(ProjectV.Core PROPERTIES
                INTERPROCEDURAL_OPTIMIZATION ON
            )
        else()
            message(WARNING "LTO не поддерживается: ${LTO_ERROR}")
        endif()
    endif()
endif()
```

### 7.2 Profile-Guided Optimization (PGO)

```cmake
# Конфигурация PGO
option(ENABLE_PGO "Включить Profile-Guided Optimization" OFF)

if(ENABLE_PGO)
    set(PGO_STAGE "generate" CACHE STRING "Стадия PGO: generate или use")

    if(PGO_STAGE STREQUAL "generate")
        add_compile_options(-fprofile-generate=${CMAKE_BINARY_DIR}/pgo)
        add_link_options(-fprofile-generate=${CMAKE_BINARY_DIR}/pgo)
    elseif(PGO_STAGE STREQUAL "use")
        add_compile_options(-fprofile-use=${CMAKE_BINARY_DIR}/pgo)
        add_link_options(-fprofile-use=${CMAKE_BINARY_DIR}/pgo)
    endif()
endif()
```

---

## 8. Санитайзеры

### 8.1 AddressSanitizer (ASan)

```cmake
option(ENABLE_ASAN "Включить AddressSanitizer" OFF)

if(ENABLE_ASAN)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(WARNING "AddressSanitizer рекомендуется только для сборок Debug")
    endif()

    add_compile_options(
        -fsanitize=address
        -fno-omit-frame-pointer
        -fno-optimize-sibling-calls
    )
    add_link_options(-fsanitize=address)
endif()
```

### 8.2 UndefinedBehaviorSanitizer (UBSan)

```cmake
option(ENABLE_UBSAN "Включить UndefinedBehaviorSanitizer" OFF)

if(ENABLE_UBSAN)
    add_compile_options(
        -fsanitize=undefined
        -fno-omit-frame-pointer
    )
    add_link_options(-fsanitize=undefined)
endif()
```

### 8.3 ThreadSanitizer (TSan)

```cmake
option(ENABLE_TSAN "Включить ThreadSanitizer" OFF)

if(ENABLE_TSAN)
    if(ENABLE_ASAN)
        message(FATAL_ERROR "ThreadSanitizer несовместим с AddressSanitizer")
    endif()

    add_compile_options(
        -fsanitize=thread
        -fno-omit-frame-pointer
    )
    add_link_options(-fsanitize=thread)
endif()
```

### 8.4 MemorySanitizer (MSan)

```cmake
option(ENABLE_MSAN "Включить MemorySanitizer" OFF)

if(ENABLE_MSAN)
    if(ENABLE_ASAN OR ENABLE_TSAN)
        message(FATAL_ERROR "MemorySanitizer несовместим с другими санитайзерами")
    endif()

    add_compile_options(
        -fsanitize=memory
        -fno-omit-frame-pointer
    )
    add_link_options(-fsanitize=memory)
endif()
```

---

## 9. Покрытие кода

### 9.1 Конфигурация покрытия

```cmake
option(ENABLE_COVERAGE "Включить покрытие кода" OFF)

if(ENABLE_COVERAGE)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(WARNING "Покрытие рекомендуется только для сборок Debug")
    endif()

    add_compile_options(
        -fprofile-instr-generate
        -fcoverage-mapping
    )
    add_link_options(
        -fprofile-instr-generate
        -fcoverage-mapping
    )
endif()
```

---

## 10. Unity-сборки

### 10.1 Конфигурация Unity-сборки

Unity-сборки объединяют несколько единиц трансляции, снижая накладные расходы компиляции и обеспечивая межмодульную
оптимизацию.

```cmake
option(ENABLE_UNITY_BUILD "Включить unity-сборку" OFF)

if(ENABLE_UNITY_BUILD)
    set_target_properties(ProjectV.Core PROPERTIES
        UNITY_BUILD ON
        UNITY_BUILD_BATCH_SIZE 16
        UNITY_BUILD_CODE_BEFORE_INCLUDE "// NOLINT: Unity build"
        UNITY_BUILD_CODE_AFTER_INCLUDE ""
    )

    # Исключение специфичных файлов из unity-сборки
    set_source_files_properties(
        src/core/special_file.cpp
        PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
    )
endif()
```

---

## 11. Интеграция CCache

### 11.1 Конфигурация CCache

```cmake
option(ENABLE_CCACHE "Включить ccache" ON)

if(ENABLE_CCACHE)
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        message(STATUS "Использование ccache: ${CCACHE_PROGRAM}")
        set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
        set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    else()
        message(WARNING "ccache не найден")
    endif()
endif()
```

---

## 12. Требования соответствия

### 12.1 Обязательные требования

1. Тип сборки ДОЛЖЕН быть одним из: Debug, Release, RelWithDebInfo, MinSizeRel
2. Флаги компилятора ДОЛЖНЫ включать `-std=c++26 -stdlib=libc++ -fmodules`
3. Сборки Debug ДОЛЖНЫ включать отладочные символы (`-g`)
4. Сборки Release ДОЛЖНЫ использовать оптимизацию (`-O3`)
5. LTO ДОЛЖНО быть включено для сборок Release

### 12.2 Запрещённые практики

1. Пользовательские типы сборки без одобрения Архитектурного совета
2. `-O0` для сборок Release
3. Отключение предупреждений в любом типе сборки
4. Санитайзеры в сборках Release
5. `-fno-exceptions` или `-fno-rtti` (исключения и RTTI обязательны)

---

## 13. История редакций

| Версия | Дата       | Автор                 | Изменения                   |
|--------|------------|-----------------------|-----------------------------|
| 1.0.0  | 22.02.2026 | Архитектурная команда | Первоначальная спецификация |

---

## 14. Связанные документы

- [СТВ-CMAKE-001: Спецификация системы сборки CMake](00_specification.md)
- [СТВ-CMAKE-002: Стандарт структуры проекта CMake](01_basics-structure.md)
- [СТВ-CMAKE-003: Стандарт управления зависимостями](02_dependencies.md)

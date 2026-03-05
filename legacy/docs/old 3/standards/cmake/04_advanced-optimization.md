# СТВ-CMAKE-005: Стандарт расширенной оптимизации

**Идентификатор документа:** СТВ-CMAKE-005
**Версия:** 1.0.0
**Статус:** Утверждён
**Дата введения:** 22.02.2026
**Классификация:** Технический стандарт

---

## 1. Область применения

Настоящий стандарт определяет методы расширенной оптимизации для сборок ProjectV. Все конфигурации оптимизации,
выходящие за рамки стандартных флагов компилятора, ДОЛЖНЫ соответствовать данной спецификации.

---

## 2. Нормативные ссылки

- ISO/IEC 14882:2026 (C++26)
- Документация CMake 3.30
- Руководство по оптимизации Clang 18+
- СТВ-CMAKE-004: Стандарт конфигурации сборки

---

## 3. Link-Time Optimization (LTO)

### 3.1 Конфигурация ThinLTO

ProjectV устанавливает ThinLTO как обязательный метод для сборок Release. ThinLTO обеспечивает параллельную оптимизацию
времени линковки с пониженным потреблением памяти.

```cmake
# Конфигурация ThinLTO для сборок Release
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT LTO_SUPPORTED OUTPUT LTO_ERROR)

    if(LTO_SUPPORTED)
        set_target_properties(ProjectV.Core PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION ON
        )

        # Специфичные для ThinLTO флаги
        target_compile_options(ProjectV.Core PRIVATE
            -flto=thin
            -ffunction-sections
            -fdata-sections
        )

        target_link_options(ProjectV.Core PRIVATE
            -flto=thin
            -Wl,--gc-sections
        )
    else()
        message(WARNING "ThinLTO не поддерживается: ${LTO_ERROR}")
    endif()
endif()
```

### 3.2 Конфигурация кэша LTO

```cmake
# Кэш LTO для ускорения инкрементальных сборок
set(LTO_CACHE_DIR "${CMAKE_BINARY_DIR}/lto-cache")
file(MAKE_DIRECTORY ${LTO_CACHE_DIR})

target_link_options(ProjectV.Core PRIVATE
    -Wl,--thinlto-cache-dir=${LTO_CACHE_DIR}
    -Wl,--thinlto-cache-policy,prune_after=7d:prune_interval=1d
)
```

---

## 4. Profile-Guided Optimization (PGO)

### 4.1 Рабочий процесс PGO

Profile-Guided Optimization требует трёхэтапного процесса сборки:

1. **Инструментированная сборка**: Генерация данных профилирования
2. **Профилирующий запуск**: Выполнение типичной рабочей нагрузки
3. **Оптимизированная сборка**: Использование данных профилирования для оптимизации

### 4.2 Конфигурация инструментированной сборки

```cmake
# Этап 1: Инструментированная сборка
option(PGO_GENERATE "Генерировать данные PGO-профилирования" OFF)

if(PGO_GENERATE)
    set(PGO_DIR "${CMAKE_BINARY_DIR}/pgo-data")
    file(MAKE_DIRECTORY ${PGO_DIR})

    target_compile_options(ProjectV.Core PRIVATE
        -fprofile-generate=${PGO_DIR}
        -fcoverage-mapping
    )

    target_link_options(ProjectV.Core PRIVATE
        -fprofile-generate=${PGO_DIR}
    )
endif()
```

### 4.3 Конфигурация оптимизированной сборки

```cmake
# Этап 3: Использование данных PGO-профилирования
option(PGO_USE "Использовать данные PGO-профилирования" OFF)

if(PGO_USE)
    set(PGO_DIR "${CMAKE_BINARY_DIR}/pgo-data")

    if(NOT EXISTS "${PGO_DIR}")
        message(FATAL_ERROR "Данные PGO-профилирования не найдены в ${PGO_DIR}")
    endif()

    target_compile_options(ProjectV.Core PRIVATE
        -fprofile-use=${PGO_DIR}
        -fprofile-correction
    )

    target_link_options(ProjectV.Core PRIVATE
        -fprofile-use=${PGO_DIR}
    )
endif()
```

### 4.4 Скрипт сборки PGO

```bash
#!/bin/bash
# scripts/pgo_build.sh

set -e

BUILD_DIR="${1:-build-pgo}"

# Этап 1: Инструментированная сборка
cmake -B ${BUILD_DIR} -DCMAKE_BUILD_TYPE=Release -DPGO_GENERATE=ON
cmake --build ${BUILD_DIR} --parallel

# Этап 2: Профилирующий запуск
${BUILD_DIR}/bin/ProjectV --benchmark --exit-after 60

# Этап 3: Оптимизированная сборка
cmake -B ${BUILD_DIR} -DPGO_GENERATE=OFF -DPGO_USE=ON
cmake --build ${BUILD_DIR} --parallel
```

---

## 5. Автовекторизация

### 5.1 Конфигурация SIMD

```cmake
# Флаги автовекторизации
target_compile_options(ProjectV.Core PRIVATE
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-ffast-math>
    $<$<CONFIG:Release>:-fslp-vectorize>
    $<$<CONFIG:Release>:-fvectorize>
)

# Архитектурно-зависимая векторизация
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_options(ProjectV.Core PRIVATE
        -msse4.2
        -mavx2
        $<$<CONFIG:Release>:-march=native>
    )
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
    target_compile_options(ProjectV.Core PRIVATE
        -march=armv8-a+simd
    )
endif()
```

### 5.2 Явный SIMD через C++26 <simd>

```cpp
// Модуль ProjectV.Math использует C++26 <simd>
export module ProjectV.Math;

import std;

export namespace projectv::math {

template<typename T, std::size_t N>
using simd_vec = std::simd<T, std::simd_abi::fixed_size<N>>;

using vec4f = std::simd<float, std::simd_abi::fixed_size<4>>;
using vec8f = std::simd<float, std::simd_abi::fixed_size<8>>;
using vec4i = std::simd<int32_t, std::simd_abi::fixed_size<4>>;

export [[nodiscard]] auto dot(vec4f const& a, vec4f const& b) noexcept -> float {
    return std::reduce(a * b);
}

export [[nodiscard]] auto cross(vec4f const& a, vec4f const& b) noexcept -> vec4f {
    return {
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
        0.0f
    };
}

} // namespace projectv::math
```

---

## 6. Unity-сборки

### 6.1 Конфигурация Unity-сборки

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

### 6.2 Размер пакета Unity-сборки

```cmake
# Рекомендуемые размеры пакетов в зависимости от размера проекта
set(UNITY_BATCH_SIZE_SMALL 8)    # < 50 файлов
set(UNITY_BATCH_SIZE_MEDIUM 16)  # 50-200 файлов
set(UNITY_BATCH_SIZE_LARGE 32)   # > 200 файлов

# Автоопределение размера пакета
file(GLOB_RECURSE SOURCES "src/*.cpp")
list(LENGTH SOURCES SOURCE_COUNT)

if(SOURCE_COUNT LESS 50)
    set(UNITY_BATCH_SIZE ${UNITY_BATCH_SIZE_SMALL})
elseif(SOURCE_COUNT LESS 200)
    set(UNITY_BATCH_SIZE ${UNITY_BATCH_SIZE_MEDIUM})
else()
    set(UNITY_BATCH_SIZE ${UNITY_BATCH_SIZE_LARGE})
endif()

set_target_properties(ProjectV.Core PROPERTIES
    UNITY_BUILD_BATCH_SIZE ${UNITY_BATCH_SIZE}
)
```

---

## 7. Предкомпилированные заголовки

### 7.1 Конфигурация PCH

Предкомпилированные заголовки сокращают время компиляции часто используемых заголовков.

```cmake
option(ENABLE_PCH "Включить предкомпилированные заголовки" ON)

if(ENABLE_PCH)
    target_precompile_headers(ProjectV.Core
        PRIVATE
            <algorithm>
            <array>
            <atomic>
            <chrono>
            <cmath>
            <cstddef>
            <cstdint>
            <expected>
            <functional>
            <memory>
            <optional>
            <span>
            <string>
            <string_view>
            <thread>
            <type_traits>
            <utility>
            <vector>
    )
endif()
```

### 7.2 Повторное использование PCH между целевыми объектами

```cmake
# Совместное использование PCH между несколькими целевыми объектами
target_precompile_headers(ProjectV.Core PUBLIC
    <algorithm>
    <array>
    # ... общие заголовки
)

# Повторное использование PCH из ProjectV.Core
target_precompile_headers(ProjectV.Render REUSE_FROM ProjectV.Core)
target_precompile_headers(ProjectV.Voxel REUSE_FROM ProjectV.Core)
```

---

## 8. Параллелизм сборки

### 8.1 Параллельная компиляция

```cmake
# Параллельные задачи компиляции
include(ProcessorCount)
ProcessorCount(NPROC)

if(NPROC GREATER 0)
    math(EXPR NJOBS "${NPROC} * 2")
    set(CMAKE_JOB_POOL_COMPILE compile_pool)
    set(CMAKE_JOB_POOL_LINK link_pool)
    set(CMAKE_JOB_POOLS
        "compile_pool=${NJOBS}"
        "link_pool=${NPROC}"
    )
endif()
```

### 8.2 Оптимизация генератора Ninja

```cmake
# Специфичные для Ninja оптимизации
if(CMAKE_GENERATOR STREQUAL "Ninja")
    # Отображение прогресса компиляции
    set(CMAKE_MAKE_PROGRAM "${CMAKE_MAKE_PROGRAM} -v")

    # Параллельная линковка
    set(CMAKE_NINJA_FORCE_RESPONSE_FILE ON)
endif()
```

---

## 9. Оптимизация памяти

### 9.1 Пулы памяти

```cmake
# Пользовательские пулы памяти для VMA
target_compile_definitions(ProjectV.Render PRIVATE
    VMA_DEBUG_INITIALIZE_ALLOCATIONS=0
    VMA_DEBUG_MARGIN=0
    VMA_DEBUG_DETECT_CORRUPTION=0
)

# Оптимизация памяти только для Release
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(ProjectV.Core PRIVATE
        PROJECTV_DISABLE_MEMORY_TRACKING
    )
endif()
```

---

## 10. Оптимизация времени компиляции

### 10.1 Оптимизация constexpr

```cmake
# Включение расширенного constexpr
target_compile_options(ProjectV.Core PRIVATE
    -fconstexpr-ops-limit=10000000000
    -fconstexpr-steps=10000000000
)
```

### 10.2 Инстанциация шаблонов

```cmake
# Явное управление инстанциацией шаблонов
target_compile_options(ProjectV.Core PRIVATE
    -ftemplate-depth=1024
    -ftemplate-backtrace-limit=0
)
```

---

## 11. Мониторинг производительности

### 11.1 Измерение времени сборки

```cmake
# Включение измерения времени сборки
option(ENABLE_BUILD_TIMING "Включить измерение времени сборки" ON)

if(ENABLE_BUILD_TIMING)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CMAKE_COMMAND} -E time")
endif()
```

### 11.2 База данных компиляции

```cmake
# Генерация compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

---

## 12. Требования соответствия

### 12.1 Обязательные требования

1. ThinLTO ДОЛЖНО быть включено для сборок Release
2. `-ffunction-sections` и `-fdata-sections` ДОЛЖНЫ использоваться для сборок Release
3. Сборки PGO ДОЛЖНЫ следовать трёхэтапному рабочему процессу
4. Unity-сборки ДОЛЖНЫ иметь явную конфигурацию размера пакета

### 12.2 Запрещённые практики

1. `-ffast-math` в сборках Debug
2. `-O0` в сборках Release
3. Отключение LTO в сборках Release без одобрения Архитектурного совета
4. Размеры пакетов, превышающие 64 для unity-сборок

---

## 13. История редакций

| Версия | Дата       | Автор                 | Изменения                   |
|--------|------------|-----------------------|-----------------------------|
| 1.0.0  | 22.02.2026 | Архитектурная команда | Первоначальная спецификация |

---

## 14. Связанные документы

- [СТВ-CMAKE-001: Спецификация системы сборки CMake](00_specification.md)
- [СТВ-CMAKE-004: Стандарт конфигурации сборки](03_build-configuration.md)
- [СТВ-CPP-001: Стандарт языка C++](../cpp/00_language-standard.md)

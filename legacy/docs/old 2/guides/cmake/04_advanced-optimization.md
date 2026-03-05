# Продвинутые оптимизации сборки

**🔴 Уровень 3: Продвинутый** — Unity Builds, PCH, CCache и оптимизации для ProjectV.

> **Связь с философией:** Unity Builds и PCH — часть стратегии оптимизации времени компиляции.
> См. [05_optimization-philosophy.md](../../philosophy/05_optimization-philosophy.md).

---

## 1. Unity Builds (Jumbo Builds)

**Проблема:** Множество мелких `.cpp` файлов компилируется долго.
**Решение:** Unity Build объединяет несколько файлов в один перед компиляцией.

```cmake
set_target_properties(ProjectV PROPERTIES
    UNITY_BUILD ON
    UNITY_BUILD_BATCH_SIZE 16  # Оптимально 8-32
)

# Исключить проблемные файлы
set_source_files_properties(special_case.cpp PROPERTIES
    SKIP_UNITY_BUILD_INCLUSION ON
)
```

**Результат:** Ускорение сборки в 2-5 раз.

**Проблемы Unity Build:**

```cpp
// Проблема: Конфликты имён в anonymous namespace
// Файл A.cpp
namespace { int counter = 0; }  // Конфликт!

// Решение: уникальные имена
namespace file_a_detail { int counter = 0; }
```

---

## 2. CCache (Compiler Cache)

Кэширует результаты компиляции. Если исходник не изменился — берёт из кэша.

```cmake
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()
```

**Результат:** Пересборка почти мгновенная.

---

## 3. Precompiled Headers (PCH)

Предкомпиляция часто используемых заголовков.

```cmake
target_precompile_headers(ProjectV PRIVATE
    <vector>
    <string>
    <vulkan/vulkan.h>
    <glm/glm.hpp>
    <flecs.h>
)
```

### Общий PCH для нескольких целей

```cmake
# Создание общего PCH
add_library(projectv_pch INTERFACE)
target_precompile_headers(projectv_pch INTERFACE
    <vector>
    <string>
    <vulkan/vulkan.h>
)

# Использование в целях
target_precompile_headers(projectv_core REUSE_FROM projectv_pch)
target_precompile_headers(projectv_voxel REUSE_FROM projectv_pch)
```

**Результат:** Ускорение компиляции в 2-4 раза.

---

## 4. Оптимизации для ProjectV

### DOD/ECS структура проекта

```cmake
# Отдельные цели для слоёв архитектуры
add_library(projectv_core STATIC
    src/core/ecs.cpp
    src/core/components.cpp
)

add_library(projectv_voxel STATIC
    src/voxel/chunk.cpp
    src/voxel/meshing.cpp
)

add_library(projectv_render STATIC
    src/render/vulkan.cpp
)

# Основное приложение
add_executable(ProjectV src/main.cpp)
target_link_libraries(ProjectV PRIVATE
    projectv_core
    projectv_voxel
    projectv_render
)
```

### SIMD для воксельной математики

```cmake
if(MSVC)
    target_compile_options(projectv_voxel PRIVATE /arch:AVX2)
else()
    target_compile_options(projectv_voxel PRIVATE
        -mavx2 -mfma -march=native
    )
endif()

# Выравнивание для cache-friendly структур
target_compile_options(projectv_voxel PRIVATE
    -falign-functions=32 -falign-loops=32
)
```

### PCH для воксельного модуля

```cmake
target_precompile_headers(projectv_voxel PRIVATE
    <cstdint>
    <cstring>
    <immintrin.h>    # AVX/AVX2
    <xmmintrin.h>    # SSE
    <glm/glm.hpp>
    "voxel/chunk_types.hpp"
)
```

### PCH для Vulkan рендеринга

```cmake
target_precompile_headers(projectv_render PRIVATE
    <vulkan/vulkan.h>
    <vk_mem_alloc.h>
    <tracy/Tracy.hpp>
)
```

---

## 5. Профиль Development

Дополнительный профиль для баланса отладки и производительности:

```cmake
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Development" CACHE STRING "" FORCE)

if(CMAKE_BUILD_TYPE STREQUAL "Development")
    set(CMAKE_CXX_FLAGS_DEVELOPMENT "-O2 -g -DTRACY_ENABLE")

    # Санитайзеры
    target_compile_options(ProjectV PRIVATE
        -fsanitize=address
        -fsanitize=undefined
    )
    target_link_options(ProjectV PRIVATE
        -fsanitize=address
        -fsanitize=undefined
    )
endif()
```

---

## 6. Tracy профилирование

```cmake
option(USE_TRACY "Enable Tracy profiling" ON)

if(USE_TRACY)
    target_compile_definitions(ProjectV PRIVATE TRACY_ENABLE)
    target_link_libraries(ProjectV PRIVATE Tracy::TracyClient)

    # Vulkan интеграция
    target_compile_definitions(projectv_render PRIVATE TRACY_VULKAN)
endif()
```

---

## 7. Сравнение производительности

| Конфигурация         | Полная сборка | Инкрементальная |
|----------------------|---------------|-----------------|
| Без оптимизаций      | 3m 45s        | 15s             |
| Unity batch=16       | 1m 20s        | 45s             |
| Unity + PCH          | 55s           | 40s             |
| Unity + PCH + CCache | 50s           | 5s              |

**Рекомендация:** Используйте `UNITY_BUILD_BATCH_SIZE=16` + PCH + CCache.

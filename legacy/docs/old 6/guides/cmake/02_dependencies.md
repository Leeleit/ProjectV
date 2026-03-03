# Управление зависимостями

**🟡 Уровень 2: Средний** — Подключение библиотек для воксельного движка.

Ключевой аспект разработки игрового движка — использование библиотек (Vulkan, SDL, glm, fastgltf). CMake предлагает
несколько способов их подключения.

---

## 1. FetchContent (рекомендуется для большинства библиотек)

`FetchContent` — встроенный модуль CMake, который скачивает исходный код зависимости во время конфигурации.

**Преимущества:**

- Не нужен `git submodule`
- Всегда актуальная версия (можно зафиксировать тег)
- Кроссплатформенно

```cmake
include(FetchContent)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt
    GIT_TAG        10.1.1  # Фиксируйте версию!
)

FetchContent_MakeAvailable(fmt)

target_link_libraries(MyGame PRIVATE fmt::fmt)
```

---

## 2. find_package (для системных SDK)

Для тяжёлых SDK (Vulkan, Boost) лучше использовать системные версии.

```cmake
find_package(Vulkan REQUIRED)

if(Vulkan_FOUND)
    message(STATUS "Vulkan SDK: ${Vulkan_INCLUDE_DIRS}")
    target_link_libraries(MyGame PRIVATE Vulkan::Vulkan)
endif()
```

> **Примечание:** `find_package` ищет `.cmake` конфиги в системе. На Windows может искать в `CMAKE_PREFIX_PATH` или
> переменных среды (`VULKAN_SDK`).

---

## 3. Git Submodules (для контроля версий)

Если нужен полный контроль над версией или патчинг библиотеки.

**Структура:**

```
ProjectV/
├── external/
│   ├── SDL/        # git submodule
│   ├── volk/       # git submodule
│   └── VMA/        # git submodule
└── CMakeLists.txt
```

**CMakeLists.txt:**

```cmake
add_subdirectory(external/SDL)
add_subdirectory(external/volk)
add_subdirectory(external/VMA)

target_link_libraries(ProjectV PRIVATE
    SDL3::SDL3
    volk
    GPUOpen::VulkanMemoryAllocator
)
```

---

## 4. Смешанный подход для ProjectV

| Тип библиотеки        | Метод                         | Примеры            |
|-----------------------|-------------------------------|--------------------|
| Тяжёлые SDK           | `find_package`                | Vulkan SDK         |
| Header-only           | `FetchContent` или submodules | glm, stb           |
| Сложные библиотеки    | `FetchContent` с опциями      | flecs, JoltPhysics |
| Контролируемые версии | Git submodules                | SDL3, volk, VMA    |

---

## 5. Примеры для ProjectV

### Vulkan (системный SDK)

```cmake
find_package(Vulkan REQUIRED)
target_link_libraries(ProjectV PRIVATE Vulkan::Vulkan)
```

### SDL3 (git submodule)

```cmake
add_subdirectory(external/SDL)
target_link_libraries(ProjectV PRIVATE SDL3::SDL3)
```

### volk (статическая линковка)

```cmake
set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
add_subdirectory(external/volk)
target_link_libraries(ProjectV PRIVATE volk)
```

### VMA (Vulkan Memory Allocator)

```cmake
add_subdirectory(external/VMA)
target_link_libraries(ProjectV PRIVATE GPUOpen::VulkanMemoryAllocator)
```

### flecs (через FetchContent)

```cmake
include(FetchContent)
FetchContent_Declare(
    flecs
    GIT_REPOSITORY https://github.com/SanderMertens/flecs
    GIT_TAG        v3.2.4
)
FetchContent_MakeAvailable(flecs)
target_link_libraries(ProjectV PRIVATE flecs::flecs_static)
```

### JoltPhysics (с опциями)

```cmake
set(USE_SSE4_2 ON CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY https://github.com/jolt-physics/jolt-physics
    GIT_TAG        v5.0.0
)
FetchContent_MakeAvailable(JoltPhysics)

target_link_libraries(ProjectV PRIVATE Jolt)
```

---

## 6. Быстрый справочник

| Задача               | Команда                                                   |
|----------------------|-----------------------------------------------------------|
| Скачать библиотеку   | `FetchContent_Declare()` + `FetchContent_MakeAvailable()` |
| Найти системную      | `find_package(Name REQUIRED)`                             |
| Подключить локальную | `add_subdirectory(external/name)`                         |
| Указать путь поиска  | `cmake -DCMAKE_PREFIX_PATH=/path ..`                      |

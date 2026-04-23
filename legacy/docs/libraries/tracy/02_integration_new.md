## Установка Tracy

<!-- anchor: 02_installation -->

**🟢 Уровень 1: Начинающий** — Интеграция Tracy в проект через CMake и пакетные менеджеры.

---

## CMake интеграция

### Простейшая интеграция

```cmake
add_subdirectory(external/tracy)
target_link_libraries(YourApp PRIVATE Tracy::TracyClient)
```

### Условное включение

```cmake
option(TRACY_ENABLE "Enable Tracy profiling" ON)

if(TRACY_ENABLE)
    add_subdirectory(external/tracy)
    target_compile_definitions(YourApp PRIVATE TRACY_ENABLE)
    target_link_libraries(YourApp PRIVATE Tracy::TracyClient)
endif()
```

### Конфигурация для Debug/Release

```cmake
if(TRACY_ENABLE)
    add_subdirectory(external/tracy)
    target_compile_definitions(YourApp PRIVATE TRACY_ENABLE)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # Debug: полное профилирование
        target_compile_definitions(YourApp PRIVATE
            TRACY_CALLSTACK=8
            TRACY_MEMORY
            TRACY_VERBOSE
        )
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        # Release: минимальный overhead
        target_compile_definitions(YourApp PRIVATE
            TRACY_ONLY_FRAME
            TRACY_NO_CALLSTACK
            TRACY_LOW_OVERHEAD
        )
    endif()

    target_link_libraries(YourApp PRIVATE Tracy::TracyClient)
endif()
```

---

## Пакетные менеджеры

### vcpkg

```cmake
find_package(Tracy CONFIG REQUIRED)
target_link_libraries(YourApp PRIVATE Tracy::TracyClient)
```

Установка пакета:

```bash
vcpkg install tracy
```

### Conan

В `conanfile.txt`:

```ini
[requires]
tracy/0.11

[generators]
CMakeDeps
CMakeToolchain
```

В `CMakeLists.txt`:

```cmake
find_package(tracy REQUIRED)
target_link_libraries(YourApp PRIVATE tracy::tracy)
```

---

## Конфигурационные макросы

### Основные макросы

| Макрос                       | По умолчанию | Описание                                      |
|------------------------------|--------------|-----------------------------------------------|
| `TRACY_ENABLE`               | Не определён | Включить Tracy                                |
| `TRACY_ON_DEMAND`            | 0            | Профилирование по запросу                     |
| `TRACY_NO_FRAME_IMAGE`       | 0            | Отключить захват изображений кадра            |
| `TRACY_NO_VSYNC_CAPTURE`     | 0            | Отключить захват vsync                        |
| `TRACY_NO_CONTEXT_SWITCH`    | 0            | Отключить отслеживание переключений контекста |
| `TRACY_NO_SAMPLING`          | 0            | Отключить sampling                            |
| `TRACY_NO_BROADCAST`         | 0            | Отключить broadcast                           |
| `TRACY_NO_CALLSTACK`         | 0            | Отключить стек вызовов                        |
| `TRACY_NO_CALLSTACK_INLINES` | 0            | Отключить inline-функции в стеке              |
| `TRACY_NO_EXIT`              | 0            | Не завершать приложение при ошибках           |
| `TRACY_NO_SYSTEM_TRACING`    | 0            | Отключить системную трассировку               |
| `TRACY_NO_CODE_TRANSFER`     | 0            | Отключить передачу кода                       |
| `TRACY_NO_STATISTICS`        | 0            | Отключить статистику                          |

### Расширенные макросы

| Макрос                  | По умолчанию | Описание                         |
|-------------------------|--------------|----------------------------------|
| `TRACY_MANUAL_LIFETIME` | 0            | Ручное управление временем жизни |
| `TRACY_DELAYED_INIT`    | 0            | Отложенная инициализация         |
| `TRACY_FIBERS`          | 0            | Поддержка fibers                 |
| `TRACY_CALLSTACK`       | 0            | Глубина стека (0-62)             |
| `TRACY_ONLY_FRAME`      | 0            | Только отметки кадров            |
| `TRACY_LOW_OVERHEAD`    | 0            | Режим низкого overhead           |
| `TRACY_VERBOSE`         | 0            | Подробные сообщения              |

### Пример конфигурации

```cpp
// В коде или через CMake
#define TRACY_ENABLE
#define TRACY_CALLSTACK 8      // Глубина стека
#define TRACY_MEMORY          // Memory profiling
#define TRACY_NO_FRAME_IMAGE  // Экономия памяти
#define TRACY_LOW_OVERHEAD    // Минимальный overhead
```

---

## Платформо-специфичные настройки

### Windows

```cmake
if(WIN32)
    target_compile_definitions(YourApp PRIVATE
        _WIN32_WINNT=0x0601  # Windows 7+
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()
```

### Linux

```cmake
if(UNIX AND NOT APPLE)
    # Tracy требует libatomic на некоторых архитектурах
    find_library(ATOMIC_LIBRARY atomic)
    if(ATOMIC_LIBRARY)
        target_link_libraries(YourApp PRIVATE ${ATOMIC_LIBRARY})
    endif()

    # Sampling требует прав root
    target_compile_definitions(YourApp PRIVATE TRACY_NO_SAMPLING)
endif()
```

### macOS

```cmake
if(APPLE)
    # Sampling требует root права
    target_compile_definitions(YourApp PRIVATE TRACY_NO_SAMPLING)
endif()
```

### Android

```cmake
if(ANDROID)
    target_compile_definitions(YourApp PRIVATE
        TRACY_NO_SAMPLING
        TRACY_NO_CONTEXT_SWITCH
        TRACY_NO_SYSTEM_TRACING
    )

    find_library(log-lib log)
    target_link_libraries(YourApp PRIVATE ${log-lib})
endif()
```

---

## Заголовочные файлы

### Основные

```cpp
#include "tracy/Tracy.hpp"    // C++ интерфейс
#include "tracy/TracyC.h"     // C интерфейс
```

### GPU профилирование

```cpp
#include "tracy/TracyVulkan.hpp"   // Vulkan
#include "tracy/TracyOpenGL.hpp"   // OpenGL
#include "tracy/TracyD3D11.hpp"    // Direct3D 11
#include "tracy/TracyD3D12.hpp"    // Direct3D 12
#include "tracy/TracyMetal.hmm"    // Metal
#include "tracy/TracyCUDA.hpp"     // CUDA
#include "tracy/TracyOpenCL.hpp"   // OpenCL
```

### Языковые биндинги

```cpp
#include "tracy/TracyLua.hpp"      // Lua
```

---

## Сборка сервера Tracy

### Зависимости

**Windows:**

- Visual Studio 2019+ или MSVC
- Windows SDK

**Linux:**

- GCC 9+ или Clang 10+
- libpthread, libdl
- libgtk-3-dev, libtbb-dev (опционально)

**macOS:**

- Xcode 12+
- Homebrew зависимости

### Сборка

```bash
cd external/tracy/profiler
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Альтернативы

**Web-версия:**

```bash
# Требует Emscripten
cd external/tracy/profiler
emcmake cmake .
emmake make
```

---

## Проверка установки

### Код для проверки

```cpp
#include "tracy/Tracy.hpp"

int main() {
    ZoneScoped;

    FrameMark;
    TracyMessageL("Tracy is working!");

    return 0;
}
```

### Компиляция и запуск

```bash
# Сборка
cmake --build build

# Запуск приложения
./build/YourApp &

# Запуск сервера
./tracy-profiler
```

Если подключение прошло успешно, в интерфейсе Tracy появятся зоны и сообщения.

---

## Tracy в ProjectV: Обзор

<!-- anchor: 08_projectv-overview -->

**🟡 Уровень 2: Средний** — Интеграция Tracy в воксельный движок ProjectV.

---

## Почему Tracy для ProjectV

### Требования к профилированию воксельного движка

Воксельные движки имеют специфичные требования к профилированию:

1. **Большое количество данных** — тысячи чанков, миллионы вокселей
2. **Много уровней абстракции** — от генерации мира до рендеринга
3. **GPU-интенсивные операции** — compute shaders, indirect drawing
4. **Многопоточная архитектура** — ECS системы, streaming, физика

### Преимущества Tracy для ProjectV

| Требование             | Решение Tracy                       |
|------------------------|-------------------------------------|
| Наносекундная точность | Высокое разрешение таймеров         |
| GPU профилирование     | Vulkan поддержка из коробки         |
| Многопоточность        | Автоматическое отслеживание потоков |
| Memory tracking        | VMA интеграция                      |
| Реальное время         | Live profiling без остановки        |

---

## Архитектура интеграции

### Компоненты ProjectV с Tracy

```
ProjectV
├── SDL3 Events        ──► Tracy FrameMark
├── flecs ECS          ──► Tracy Zone (каждая система)
├── Vulkan Renderer    ──► Tracy GPU Zones
├── VMA Memory         ──► Tracy Memory tracking
├── JoltPhysics        ──► Tracy Zones
├── Chunk Streaming    ──► Tracy Plots
└── Audio (miniaudio)  ──► Tracy Zones
```

### Цветовое кодирование для ProjectV

| Подсистема    | Цвет       | HEX        |
|---------------|------------|------------|
| SDL Events    | Голубой    | `0x00FFFF` |
| ECS Update    | Зелёный    | `0x00FF00` |
| Vulkan Render | Синий      | `0x0000FF` |
| GPU Compute   | Фиолетовый | `0x8800FF` |
| Physics       | Оранжевый  | `0xFF8800` |
| Chunk Gen     | Жёлтый     | `0xFFFF00` |
| Streaming     | Красный    | `0xFF0000` |
| Audio         | Розовый    | `0xFF00FF` |

---

## Ключевые точки профилирования

### Главный игровой цикл

```cpp
void Game::run() {
    while (m_running) {
        FrameMark;  // Основной кадр

        {
            ZoneScopedNC("SDL_Events", 0x00FFFF);
            processEvents();
        }

        {
            ZoneScopedNC("ECS_Update", 0x00FF00);
            m_world.progress(deltaTime);
        }

        {
            ZoneScopedNC("Render", 0x0000FF);
            m_renderer.render();
        }
    }
}
```

### ECS системы

```cpp
void MovementSystem::update(float dt) {
    ZoneScopedNC("MovementSystem", 0x00FF00);

    auto view = world.each<Position, Velocity>();
    for (auto entity : view) {
        // ...
    }

    TracyPlot("EntityCount", (int64_t)view.count());
}
```

### Vulkan рендеринг

```cpp
void VulkanRenderer::renderFrame(VkCommandBuffer cmd) {
    TracyVkZone(m_tracyCtx, cmd, "RenderFrame");

    {
        TracyVkZone(m_tracyCtx, cmd, "ShadowPass");
        renderShadows(cmd);
    }

    {
        TracyVkZone(m_tracyCtx, cmd, "GeometryPass");
        renderGeometry(cmd);
    }

    {
        TracyVkZone(m_tracyCtx, cmd, "VoxelCompute");
        computeVoxels(cmd);
    }
}
```

### VMA Memory tracking

```cpp
void allocateBuffer(VmaAllocator allocator, size_t size) {
    VkBuffer buffer;
    VmaAllocation allocation;
    // ... allocation ...

    TracyAllocN(allocation, size, "VMA_Buffer");
}
```

---

## Метрики ProjectV

### Рекомендуемые Plots

```cpp
void updateMetrics() {
    // FPS и время кадра
    TracyPlot("FPS", (int64_t)m_fps);
    TracyPlot("FrameTime", m_frameTime);

    // ECS метрики
    TracyPlot("EntityCount", (int64_t)m_entityCount);
    TracyPlot("SystemCount", (int64_t)m_systemCount);

    // Чанки
    TracyPlot("LoadedChunks", (int64_t)m_loadedChunks);
    TracyPlot("VisibleChunks", (int64_t)m_visibleChunks);

    // Память
    TracyPlot("VoxelMemoryMB", m_voxelMemory / (1024.0 * 1024.0));
    TracyPlot("GPUMemoryMB", m_gpuMemory / (1024.0 * 1024.0));

    // Физика
    TracyPlot("PhysicsBodies", (int64_t)m_physicsBodies);
}
```

---

## Связь с другими библиотеками

### SDL3 + Tracy

```cpp
#include "tracy/Tracy.hpp"

void processEvents() {
    ZoneScoped;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                m_running = false;
                TracyMessageL("Quit event received");
                break;
        }
    }
}
```

### flecs + Tracy

```cpp
#include "tracy/Tracy.hpp"

// Автоматическое профилирование систем
world.set_threads(4);  // Многопоточность автоматически отслеживается

// Ручное профилирование
world.system<Position, Velocity>("Movement")
    .each([](flecs::entity e, Position& p, Velocity& v) {
        ZoneScopedNC("MovementEach", 0x00FF00);
        // ...
    });
```

### VMA + Tracy

```cpp
#include "tracy/Tracy.hpp"
#include "vk_mem_alloc.h"

// Кастомные аллокаторы с Tracy
void* vmaAllocate(size_t size, void* userdata) {
    void* ptr = malloc(size);
    TracyAllocN(ptr, size, "VMA");
    return ptr;
}

void vmaDeallocate(void* ptr, void* userdata) {
    TracyFreeN(ptr, "VMA");
    free(ptr);
}
```

---

## Режимы работы

### Development (Debug)

```cmake
target_compile_definitions(ProjectV PRIVATE
    TRACY_ENABLE
    TRACY_CALLSTACK=16
    TRACY_MEMORY
    TRACY_VERBOSE
)
```

Полное профилирование со стеками, memory tracking, подробные сообщения.

### Release

```cmake
target_compile_definitions(ProjectV PRIVATE
    TRACY_ENABLE
    TRACY_ON_DEMAND
    TRACY_CALLSTACK=4
    TRACY_NO_SAMPLING
    TRACY_NO_SYSTEM_TRACING
)
```

Минимальный overhead, профилирование по запросу.

### Distribution

```cmake
# TRACY_ENABLE не определён — все макросы становятся no-op
```

Без профилирования для финальных билдов.

---

## Следующие шаги

| Файл                                                     | Описание                    |
|----------------------------------------------------------|-----------------------------|
| [09_projectv-integration.md](09_projectv-integration.md) | CMake конфигурация ProjectV |
| [10_projectv-patterns.md](10_projectv-patterns.md)       | Паттерны профилирования     |
| [11_projectv-examples.md](11_projectv-examples.md)       | Примеры кода                |

---

## Tracy в ProjectV: Интеграция

<!-- anchor: 09_projectv-integration -->

**🟡 Уровень 2: Средний** — CMake конфигурация и техническая интеграция Tracy в ProjectV.

---

## Структура в ProjectV

```
ProjectV/
├── external/
│   └── tracy/           # Git submodule
│       ├── public/
│       │   ├── tracy/
│       │   │   ├── Tracy.hpp
│       │   │   └── TracyVulkan.hpp
│       │   └── TracyClient.cpp
│       └── ...
├── src/
│   ├── core/
│   │   └── Profiling.hpp  # Обёртки для Tracy
│   └── ...
└── CMakeLists.txt
```

---

## CMake конфигурация

### Базовая интеграция

```cmake
# CMakeLists.txt

option(TRACY_ENABLE "Enable Tracy profiling" ON)

if(TRACY_ENABLE)
    add_subdirectory(external/tracy)

    target_compile_definitions(ProjectV PRIVATE
        TRACY_ENABLE
    )

    target_link_libraries(ProjectV PRIVATE
        Tracy::TracyClient
    )
endif()
```

### Полная конфигурация с профилями

```cmake
# cmake/TracyConfig.cmake

option(TRACY_ENABLE "Enable Tracy profiling" ON)
option(TRACY_ON_DEMAND "On-demand profiling" OFF)
option(TRACY_LOW_OVERHEAD "Minimize profiling overhead" OFF)

if(TRACY_ENABLE)
    add_subdirectory(${CMAKE_SOURCE_DIR}/external/tracy)

    target_compile_definitions(ProjectV PRIVATE TRACY_ENABLE)

    # Debug конфигурация
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(ProjectV PRIVATE
            TRACY_CALLSTACK=16
            TRACY_MEMORY
            TRACY_VERBOSE
        )
    endif()

    # Release конфигурация
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_definitions(ProjectV PRIVATE
            TRACY_CALLSTACK=4
        )

        if(TRACY_ON_DEMAND)
            target_compile_definitions(ProjectV PRIVATE TRACY_ON_DEMAND)
        endif()

        if(TRACY_LOW_OVERHEAD)
            target_compile_definitions(ProjectV PRIVATE
                TRACY_NO_SAMPLING
                TRACY_NO_SYSTEM_TRACING
                TRACY_NO_CONTEXT_SWITCH
            )
        endif()
    endif()

    target_link_libraries(ProjectV PRIVATE Tracy::TracyClient)
endif()
```

### Условное профилирование

```cmake
# Позволяет отключить Tracy для distribution билдов
if(NOT TRACY_ENABLE)
    target_compile_definitions(ProjectV PRIVATE
        TRACY_ENABLE=0
    )
endif()
```

---

## Заголовочный файл обёртки

### Profiling.hpp

```cpp
// src/core/Profiling.hpp
#pragma once

#ifdef TRACY_ENABLE
    #include "tracy/Tracy.hpp"
    #include "tracy/TracyVulkan.hpp"
#else
    // Все макросы становятся no-op если Tracy отключён
    #define ZoneScoped
    #define ZoneScopedN(name)
    #define ZoneScopedC(color)
    #define ZoneScopedNC(name, color)
    #define FrameMark
    #define FrameMarkNamed(name)
    #define TracyPlot(name, val)
    #define TracyMessageL(txt)
    #define TracyMessageLC(txt, color)
    #define TracyAlloc(ptr, size)
    #define TracyFree(ptr)
    #define TracyAllocN(ptr, size, name)
    #define TracyFreeN(ptr, name)
#endif

// Цвета для ProjectV подсистем
namespace ProjectV::ProfilingColors {
    constexpr uint32_t SDLEvents    = 0x00FFFF;  // Голубой
    constexpr uint32_t ECS          = 0x00FF00;  // Зелёный
    constexpr uint32_t Render       = 0x0000FF;  // Синий
    constexpr uint32_t GPUCompute   = 0x8800FF;  // Фиолетовый
    constexpr uint32_t Physics      = 0xFF8800;  // Оранжевый
    constexpr uint32_t ChunkGen     = 0xFFFF00;  // Жёлтый
    constexpr uint32_t Streaming    = 0xFF0000;  // Красный
    constexpr uint32_t Audio        = 0xFF00FF;  // Розовый
    constexpr uint32_t Memory       = 0xFFFFFF;  // Белый
    constexpr uint32_t Loading      = 0x88FF88;  // Светло-зелёный
}

// Удобные макросы для ProjectV
#define PV_PROFILE_FRAME()       FrameMark
#define PV_PROFILE_SDL()         ZoneScopedNC("SDL", ProjectV::ProfilingColors::SDLEvents)
#define PV_PROFILE_ECS(name)     ZoneScopedNC(name, ProjectV::ProfilingColors::ECS)
#define PV_PROFILE_RENDER(name)  ZoneScopedNC(name, ProjectV::ProfilingColors::Render)
#define PV_PROFILE_GPU(name)     ZoneScopedNC(name, ProjectV::ProfilingColors::GPUCompute)
#define PV_PROFILE_PHYSICS()     ZoneScopedNC("Physics", ProjectV::ProfilingColors::Physics)
#define PV_PROFILE_CHUNK()       ZoneScopedNC("ChunkGen", ProjectV::ProfilingColors::ChunkGen)
#define PV_PROFILE_STREAMING()   ZoneScopedNC("Streaming", ProjectV::ProfilingColors::Streaming)
#define PV_PROFILE_AUDIO()       ZoneScopedNC("Audio", ProjectV::ProfilingColors::Audio)
```

---

## Инициализация Tracy

### Vulkan контекст

```cpp
// src/renderer/VulkanTracy.hpp
#pragma once

#ifdef TRACY_ENABLE
    #include "tracy/TracyVulkan.hpp"
#endif

namespace projectv {

class VulkanTracy {
#ifdef TRACY_ENABLE
    tracy::VkCtx* m_context = nullptr;
#endif

public:
    void init(VkPhysicalDevice physicalDevice, VkDevice device,
              VkQueue queue, VkCommandBuffer initCmd) {
#ifdef TRACY_ENABLE
        m_context = tracy::CreateVkContext(physicalDevice, device, queue, initCmd);
#endif
    }

    void destroy() {
#ifdef TRACY_ENABLE
        if (m_context) {
            tracy::DestroyVkContext(m_context);
            m_context = nullptr;
        }
#endif
    }

    void zone(VkCommandBuffer cmd, const char* name) {
#ifdef TRACY_ENABLE
        TracyVkZone(m_context, cmd, name);
#endif
    }

    void collect(VkQueue queue) {
#ifdef TRACY_ENABLE
        TracyVkCollect(m_context, queue);
#endif
    }

#ifdef TRACY_ENABLE
    tracy::VkCtx* getContext() { return m_context; }
#endif
};

} // namespace projectv
```

### Интеграция в Renderer

```cpp
// src/renderer/Renderer.cpp

void Renderer::init() {
    // Инициализация Vulkan...

    // Инициализация Tracy для Vulkan
#ifdef TRACY_ENABLE
    VkCommandBuffer initCmd = beginSingleTimeCommands();
    m_tracy.init(m_physicalDevice, m_device, m_graphicsQueue, initCmd);
    endSingleTimeCommands(initCmd);
#endif
}

void Renderer::render() {
    VkCommandBuffer cmd = beginFrame();

    m_tracy.zone(cmd, "Frame");

    {
        m_tracy.zone(cmd, "Geometry");
        renderGeometry(cmd);
    }

    {
        m_tracy.zone(cmd, "Voxels");
        renderVoxels(cmd);
    }

    endFrame();

#ifdef TRACY_ENABLE
    m_tracy.collect(m_graphicsQueue);
#endif
}

void Renderer::cleanup() {
    m_tracy.destroy();
}
```

---

## Интеграция с flecs

### Профилирование ECS систем

```cpp
// src/ecs/SystemProfiler.hpp
#pragma once

#include "flecs.h"
#include "core/Profiling.hpp"

namespace projectv {

// Базовый класс для систем с профилированием
class ProfiledSystem {
protected:
    const char* m_name;
    uint32_t m_color;

public:
    ProfiledSystem(const char* name, uint32_t color = ProfilingColors::ECS)
        : m_name(name), m_color(color) {}

    void profileBegin() {
        ZoneScopedNC(m_name, m_color);
    }

    void profileEnd() {
        // Автоматически при выходе из scope
    }
};

// Макрос для создания профилируемой системы
#define PV_SYSTEM(name, color) \
    struct name##System : public ProfiledSystem { \
        name##System() : ProfiledSystem(#name, color) {} \
        void update(flecs::world& world, float dt); \
    };

} // namespace projectv
```

### Использование

```cpp
// Автоматическое профилирование системы
world.system<Position, Velocity>("Movement")
    .iter([](flecs::iter& it, Position* p, Velocity* v) {
        ZoneScopedNC("MovementSystem", ProfilingColors::ECS);

        for (auto i : it) {
            p[i].x += v[i].x * it.delta_time();
            p[i].y += v[i].y * it.delta_time();
        }

        TracyPlot("MovementEntities", (int64_t)it.count());
    });
```

---

## Интеграция с VMA

### Memory tracking

```cpp
// src/memory/VMAProfiling.hpp
#pragma once

#include "vk_mem_alloc.h"
#include "core/Profiling.hpp"

namespace projectv {

class VMAProfiling {
public:
    static void trackAllocation(VmaAllocation allocation, size_t size, const char* name) {
#ifdef TRACY_ENABLE
        TracyAllocN(allocation, size, name);
#endif
    }

    static void trackFree(VmaAllocation allocation, const char* name) {
#ifdef TRACY_ENABLE
        TracyFreeN(allocation, name);
#endif
    }
};

// RAII обёртка
class VMABuffer {
    VkBuffer m_buffer;
    VmaAllocation m_allocation;
    size_t m_size;
    const char* m_name;

public:
    VMABuffer(VmaAllocator allocator, size_t size, VkBufferUsageFlags usage,
              VmaMemoryUsage memoryUsage, const char* name = "Buffer")
        : m_size(size), m_name(name)
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = memoryUsage;

        vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
                       &m_buffer, &m_allocation, nullptr);

        VMAProfiling::trackAllocation(m_allocation, size, name);
    }

    ~VMABuffer() {
        VMAProfiling::trackFree(m_allocation, m_name);
        // vmaDestroyBuffer вызывается внешне
    }
};

} // namespace projectv
```

---

## Сборка сервера Tracy

### Скрипт для Windows

```batch
@echo off
cd external\tracy\profiler
mkdir build 2>nul
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -A x64
cmake --build . --config Release
copy Release\tracy-profiler.exe ..\..\..\
```

### Скрипт для Linux

```bash
#!/bin/bash
cd external/tracy/profiler
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cp tracy-profiler ../../../
```

---

## Запуск профилирования

### Development режим

```bash
# Запуск сервера
./tracy-profiler

# Запуск приложения
./build/ProjectV
```

### Remote профилирование

```bash
# На целевой машине
./ProjectV &

# На машине разработчика
./tracy-profiler
# Подключение к IP целевой машины
```

### Сохранение трейса

1. В Tracy сервере: File → Save
2. Формат: `.tracy`
3. Открытие: File → Open

---

## Tracy в ProjectV: Паттерны

<!-- anchor: 10_projectv-patterns -->

**🔴 Уровень 3: Продвинутый** — Паттерны профилирования для воксельного движка.

---

## Паттерн 1: Профилирование ECS систем

> **См. также:** [ECS философия](../../philosophy/04_ecs-philosophy.md) — принципы проектирования систем.

### Проблема

ECS системы выполняются каждый кадр и обрабатывают множество сущностей. Нужно отслеживать как общее время системы, так и
обработку отдельных сущностей.

### Решение: Иерархическое профилирование

```cpp
void MovementSystem::update(flecs::world& world, float dt) {
    // Уровень 1: Общее время системы
    ZoneScopedNC("MovementSystem", ProfilingColors::ECS);

    auto view = world.each<Position, Velocity>();
    size_t count = 0;

    // Уровень 2: Обработка batch'ей (если много сущностей)
    for (auto it = view.begin(); it != view.end(); ++it) {
        ZoneScopedNC("MovementBatch", ProfilingColors::ECS);

        for (auto i : *it) {
            auto [pos, vel] = it->get<Position, Velocity>(i);
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
        }
        count += it->count();
    }

    // Метрики
    TracyPlot("MovementSystem_Count", (int64_t)count);
}
```

### Паттерн: Условное профилирование для горячих систем

```cpp
void HotSystem::update(flecs::world& world, float dt) {
    // Профилируем только если подключены
    ZoneNamed(zone, TracyIsConnected);

    // Основной код...

    if (ZoneIsActiveV(zone)) {
        ZoneTextV(zone, "Details", 8);
        ZoneValueV(zone, entityCount);
    }
}
```

---

## Паттерн 2: Профилирование генерации чанков

### Проблема

Генерация чанков — одна из самых затратных операций в воксельном движке. Требуется детальное профилирование всех этапов.

### Решение: Детальное профилирование пайплайна

```cpp
class ChunkGenerator {
public:
    void generate(Chunk& chunk, const ChunkCoord& coord) {
        ZoneScopedNC("ChunkGeneration", ProfilingColors::ChunkGen);

        auto startTime = std::chrono::high_resolution_clock::now();

        // Этап 1: Noise generation
        {
            ZoneScopedNC("NoiseGen", ProfilingColors::ChunkGen);
            generateNoise(chunk, coord);
        }

        // Этап 2: Terrain carving
        {
            ZoneScopedNC("TerrainCarve", ProfilingColors::ChunkGen);
            carveTerrain(chunk);
        }

        // Этап 3: Feature placement
        {
            ZoneScopedNC("Features", ProfilingColors::ChunkGen);
            placeFeatures(chunk);
        }

        // Этап 4: Mesh building
        {
            ZoneScopedNC("MeshBuild", ProfilingColors::ChunkGen);
            buildMesh(chunk);
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration<float, std::milli>(endTime - startTime).count();

        TracyPlot("ChunkGen_Time", ms);
        TracyPlot("ChunkGen_Voxels", (int64_t)chunk.voxelCount);
    }
};
```

### Метрики для генерации чанков

```cpp
void updateChunkMetrics() {
    TracyPlot("Chunks_Total", (int64_t)m_totalChunks);
    TracyPlot("Chunks_Generating", (int64_t)m_generatingChunks);
    TracyPlot("Chunks_Queued", (int64_t)m_queuedChunks);
    TracyPlot("Chunks_Visible", (int64_t)m_visibleChunks);
    TracyPlot("Chunks_MemoryMB", m_chunkMemory / (1024.0 * 1024.0));
}
```

---

## Паттерн 3: Профилирование GPU compute

### Проблема

Compute shaders для воксельной обработки требуют отдельного профилирования на GPU.

### Решение: GPU зоны для compute dispatch

```cpp
class VoxelCompute {
    tracy::VkCtx* m_tracyCtx;

public:
    void dispatch(VkCommandBuffer cmd, const ComputeParams& params) {
        // CPU зона для подготовки
        ZoneScopedNC("VoxelCompute", ProfilingColors::GPUCompute);

        // GPU зона для dispatch
        TracyVkZone(m_tracyCtx, cmd, "VoxelCompute_Dispatch");

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
        vkCmdDispatch(cmd, params.groupCountX, params.groupCountY, params.groupCountZ);

        // Memory barrier
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &barrier, 0, nullptr, 0, nullptr);
    }

    void dispatchChain(VkCommandBuffer cmd) {
        TracyVkZone(m_tracyCtx, cmd, "ComputeChain");

        {
            TracyVkZone(m_tracyCtx, cmd, "Culling");
            dispatchCulling(cmd);
        }

        {
            TracyVkZone(m_tracyCtx, cmd, "LOD");
            dispatchLOD(cmd);
        }

        {
            TracyVkZone(m_tracyCtx, cmd, "MeshGen");
            dispatchMeshGen(cmd);
        }
    }
};
```

---

## Паттерн 4: Профилирование streaming

### Проблема

Streaming чанков происходит в фоновых потоках. Требуется отслеживание очередей и производительности.

### Решение: Профилирование очередей и потоков

```cpp
class ChunkStreamer {
    TracyLockable(std::mutex, m_queueMutex);
    std::queue<ChunkLoadRequest> m_loadQueue;
    std::queue<ChunkUnloadRequest> m_unloadQueue;

public:
    void update() {
        ZoneScopedNC("Streaming", ProfilingColors::Streaming);

        // Захват mutex с профилированием
        std::unique_lock lock(m_queueMutex);
        LockMark(m_queueMutex);

        size_t loadCount = m_loadQueue.size();
        size_t unloadCount = m_unloadQueue.size();

        lock.unlock();

        // Обработка load очереди
        {
            ZoneScopedNC("ProcessLoadQueue", ProfilingColors::Streaming);
            processLoadQueue();
        }

        // Обработка unload очереди
        {
            ZoneScopedNC("ProcessUnloadQueue", ProfilingColors::Streaming);
            processUnloadQueue();
        }

        // Метрики
        TracyPlot("Streaming_LoadQueue", (int64_t)loadCount);
        TracyPlot("Streaming_UnloadQueue", (int64_t)unloadCount);
    }
};
```

### Профилирование worker потоков

```cpp
void streamingWorker(ThreadSafeQueue<ChunkLoadRequest>& queue) {
    // Tracy автоматически отслеживает потоки
    TracyMessageL("Streaming worker started");

    while (running) {
        auto request = queue.pop();

        if (request) {
            ZoneScopedNC("LoadChunk", ProfilingColors::Streaming);

            Chunk chunk = loadChunkFromDisk(request->coord);

            {
                ZoneScopedNC("Decompress", ProfilingColors::Streaming);
                decompressChunk(chunk);
            }

            {
                ZoneScopedNC("Upload", ProfilingColors::Streaming);
                uploadToGPU(chunk);
            }
        }
    }
}
```

---

## Паттерн 5: Профилирование VMA аллокаций

### Проблема

VMA аллокации для воксельных данных — критическая точка. Нужна категоризация и отслеживание.

### Решение: Именованные memory pools

```cpp
// Категории памяти
namespace MemoryCategories {
    constexpr const char* VertexBuffer = "VMA_Vertex";
    constexpr const char* IndexBuffer = "VMA_Index";
    constexpr const char* StagingBuffer = "VMA_Staging";
    constexpr const char* UniformBuffer = "VMA_Uniform";
    constexpr const char* StorageBuffer = "VMA_Storage";
    constexpr const char* ChunkData = "VMA_Chunk";
    constexpr const char* Texture = "VMA_Texture";
}

class MemoryManager {
    VmaAllocator m_allocator;

public:
    VkBuffer createVertexBuffer(size_t size) {
        VkBuffer buffer;
        VmaAllocation allocation;

        VmaAllocationCreateInfo createInfo{};
        createInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        vmaCreateBuffer(m_allocator, /* ... */, &buffer, &allocation, nullptr);

        // Отслеживание с именем категории
        TracyAllocN(allocation, size, MemoryCategories::VertexBuffer);

        return buffer;
    }

    void destroyBuffer(VkBuffer buffer, VmaAllocation allocation, const char* category) {
        TracyFreeN(allocation, category);
        vmaDestroyBuffer(m_allocator, buffer, allocation);
    }
};
```

### RAII обёртка с автоматическим tracking

```cpp
template<const char* Category>
class TrackedBuffer {
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    size_t m_size = 0;
    VmaAllocator m_allocator;

public:
    TrackedBuffer(VmaAllocator allocator, size_t size, VkBufferUsageFlags usage)
        : m_allocator(allocator), m_size(size)
    {
        VmaAllocationCreateInfo createInfo{};
        createInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;

        vmaCreateBuffer(allocator, &bufferInfo, &createInfo,
                       &m_buffer, &m_allocation, nullptr);

        TracyAllocN(m_allocation, size, Category);
    }

    ~TrackedBuffer() {
        if (m_allocation) {
            TracyFreeN(m_allocation, Category);
            vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
        }
    }

    // Non-copyable, movable
    TrackedBuffer(const TrackedBuffer&) = delete;
    TrackedBuffer& operator=(const TrackedBuffer&) = delete;

    TrackedBuffer(TrackedBuffer&& other) noexcept
        : m_buffer(std::exchange(other.m_buffer, VK_NULL_HANDLE))
        , m_allocation(std::exchange(other.m_allocation, VK_NULL_HANDLE))
        , m_size(other.m_size)
        , m_allocator(other.m_allocator)
    {}

    VkBuffer get() const { return m_buffer; }
    size_t size() const { return m_size; }
};

// Использование
constexpr const char VERTEX_BUFFER[] = "VMA_Vertex";
using VertexBuffer = TrackedBuffer<VERTEX_BUFFER>;
```

---

## Паттерн 6: Frame-in-frame профилирование

### Проблема

Несколько независимых "кадров" в одном приложении: игровой цикл, рендер, физика.

### Решение: Именованные frames

```cpp
class Game {
public:
    void run() {
        while (m_running) {
            // Главный кадр
            FrameMark;

            // Обновление физики (несколько шагов)
            for (int i = 0; i < m_physicsSteps; i++) {
                FrameMarkNamed("PhysicsStep");
                updatePhysics();
            }

            // Обновление рендера
            {
                ZoneScopedNC("RenderFrame", ProfilingColors::Render);
                m_renderer.render();
            }

            // Проверка производительности
            if (TracyIsConnected) {
                TracyPlot("GameLoop_MS", calculateFrameTime());
            }
        }
    }
};
```

---

## Паттерн 7: Профилирование с порогами

### Проблема

Не нужно профилировать быстрые операции, только те, что превышают порог.

### Решение: Условные зоны с измерением времени

```cpp
class ThresholdProfiler {
    std::chrono::high_resolution_clock::time_point m_start;
    const char* m_name;
    float m_thresholdMs;

public:
    explicit ThresholdProfiler(const char* name, float thresholdMs = 1.0f)
        : m_name(name), m_thresholdMs(thresholdMs)
    {
        m_start = std::chrono::high_resolution_clock::now();
    }

    ~ThresholdProfiler() {
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration<float, std::milli>(end - m_start).count();

        if (ms >= m_thresholdMs) {
            TracyMessageLC(fmt::format("{}: {:.2f}ms (slow!)", m_name, ms).c_str(), 0xFF0000);
        }

        TracyPlot(m_name, ms);
    }
};

// Использование
void potentialSlowOperation() {
    ThresholdProfiler profiler("SlowOp", 5.0f);  // Порог 5ms
    // ... код ...
}
```

---

## Паттерн 8: Профилирование ImGui

### Проблема

ImGui может добавлять overhead, требуется отслеживание времени рендеринга UI.

### Решение: Выделение UI в отдельную зону

```cpp
void renderUI() {
    ZoneScopedNC("ImGui", 0xFF00FF);

    ImGui::Begin("Stats");

    // Отображение метрик Tracy в UI
    ImGui::Text("FPS: %.1f", m_fps);
    ImGui::Text("Frame: %.2fms", m_frameTime);

    // Вложенные UI зоны
    {
        ZoneScopedNC("ImGui_DebugWindow", 0xFF00FF);
        renderDebugWindow();
    }

    {
        ZoneScopedNC("ImGui_Console", 0xFF00FF);
        renderConsole();
    }

    ImGui::End();

    // Финальный рендер ImGui
    {
        ZoneScopedNC("ImGui_Render", 0xFF00FF);
        ImGui::Render();
    }
}
```

---

## Рекомендации по оптимизации

### Минимизация overhead

1. **Не профилируйте каждый draw call** — группируйте
2. **Используйте TracyIsConnected** — проверка перед дорогими операциями
3. **Ограничьте глубину стека** — `TRACY_CALLSTACK=4` для release
4. **Избегайте ZoneText в hot paths** — форматирование строки дорого

### Организация кода

```cpp
// Хорошо: плоская структура зон
void update() {
    ZoneScoped;
    updateA();
    updateB();
    updateC();
}

// Плохо: глубокая вложенность
void update() {
    ZoneScoped;
    {
        ZoneScoped;
        {
            ZoneScoped;  // Слишком глубоко!
            // ...
        }
    }
}
```

### Консистентность имён

```cpp
// Плохо: разные стили имён
ZoneScopedN("UpdatePhysics");
ZoneScopedN("update_rendering");
ZoneScopedN("AudioSystem");

// Хорошо: консистентный стиль
ZoneScopedN("Physics_Update");
ZoneScopedN("Rendering_Update");
ZoneScopedN("Audio_Update");

---

## Tracy в ProjectV: Примеры

<!-- anchor: 11_projectv-examples -->

**🟡 Уровень 2: Средний** — Готовые примеры кода для интеграции Tracy в ProjectV.

---

## Пример 1: Полный игровой цикл

```cpp
// src/main.cpp
#include "tracy/Tracy.hpp"
#include "Game.hpp"

int main() {
    TracyMessageL("ProjectV starting...");

    projectv::Game game;
    game.init();

    TracyMessageL("Game initialized");

    while (game.isRunning()) {
        FrameMark;

        {
            ZoneScopedNC("Events", projectv::ProfilingColors::SDLEvents);
            game.processEvents();
        }

        {
            ZoneScopedNC("Update", projectv::ProfilingColors::ECS);
            game.update();
        }

        {
            ZoneScopedNC("Render", projectv::ProfilingColors::Render);
            game.render();
        }

        TracyPlot("FPS", (int64_t)game.getFPS());
        TracyPlot("FrameTime", game.getFrameTime());
    }

    TracyMessageL("ProjectV shutting down...");
    game.cleanup();

    return 0;
}
```

---

## Пример 2: Vulkan рендерер с Tracy

```cpp
// src/renderer/VulkanRenderer.hpp
#pragma once

#include "tracy/Tracy.hpp"
#include "tracy/TracyVulkan.hpp"
#include <vulkan/vulkan.h>

namespace projectv {

class VulkanRenderer {
    VkPhysicalDevice m_physicalDevice;
    VkDevice m_device;
    VkQueue m_graphicsQueue;
    VkCommandPool m_commandPool;

    tracy::VkCtx* m_tracyCtx = nullptr;

public:
    void init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device) {
        m_physicalDevice = physicalDevice;
        m_device = device;

        // Получить graphics queue
        vkGetDeviceQueue(device, 0, 0, &m_graphicsQueue);

        // Создать command pool
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = 0;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkCreateCommandPool(device, &poolInfo, nullptr, &m_commandPool);

        // Инициализация Tracy для Vulkan
        VkCommandBuffer initCmd = allocateCommandBuffer();
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(initCmd, &beginInfo);

        m_tracyCtx = tracy::CreateVkContext(physicalDevice, device, m_graphicsQueue, initCmd);

        vkEndCommandBuffer(initCmd);

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &initCmd;
        vkQueueSubmit(m_graphicsQueue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_graphicsQueue);

        vkFreeCommandBuffers(device, m_commandPool, 1, &initCmd);

        TracyMessageL("Vulkan Tracy context initialized");
    }

    void render() {
        ZoneScopedNC("VulkanRender", ProfilingColors::Render);

        VkCommandBuffer cmd = allocateCommandBuffer();

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cmd, &beginInfo);

        TracyVkZone(m_tracyCtx, cmd, "Frame");

        // Shadow pass
        {
            TracyVkZone(m_tracyCtx, cmd, "ShadowPass");
            renderShadows(cmd);
        }

        // Geometry pass
        {
            TracyVkZone(m_tracyCtx, cmd, "GeometryPass");
            renderGeometry(cmd);
        }

        // Voxel compute
        {
            TracyVkZone(m_tracyCtx, cmd, "VoxelCompute");
            computeVoxels(cmd);
        }

        // Post-process
        {
            TracyVkZone(m_tracyCtx, cmd, "PostProcess");
            renderPostProcess(cmd);
        }

        vkEndCommandBuffer(cmd);

        // Submit
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        vkQueueSubmit(m_graphicsQueue, 1, &submit, VK_NULL_HANDLE);

        // Собрать данные Tracy
        TracyVkCollect(m_tracyCtx, m_graphicsQueue);

        vkQueueWaitIdle(m_graphicsQueue);
        vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmd);
    }

    void cleanup() {
        if (m_tracyCtx) {
            tracy::DestroyVkContext(m_tracyCtx);
            m_tracyCtx = nullptr;
        }
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    }

private:
    VkCommandBuffer allocateCommandBuffer() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(m_device, &allocInfo, &cmd);
        return cmd;
    }

    void renderShadows(VkCommandBuffer cmd) { /* ... */ }
    void renderGeometry(VkCommandBuffer cmd) { /* ... */ }
    void computeVoxels(VkCommandBuffer cmd) { /* ... */ }
    void renderPostProcess(VkCommandBuffer cmd) { /* ... */ }
};

} // namespace projectv
```

---

## Пример 3: ECS система с профилированием

```cpp
// src/ecs/systems/MovementSystem.cpp
#include "tracy/Tracy.hpp"
#include "MovementSystem.hpp"

namespace projectv::systems {

void MovementSystem::update(flecs::world& world, float dt) {
    ZoneScopedNC("MovementSystem", ProfilingColors::ECS);

    size_t processedEntities = 0;

    world.each([&](flecs::entity e, Position& pos, const Velocity& vel) {
        // Пропускаем неподвижные сущности
        if (vel.x == 0.0f && vel.y == 0.0f && vel.z == 0.0f) {
            return;
        }

        // Обновление позиции
        pos.x += vel.x * dt;
        pos.y += vel.y * dt;
        pos.z += vel.z * dt;

        processedEntities++;
    });

    TracyPlot("Movement_Entities", (int64_t)processedEntities);
}

} // namespace projectv::systems
```

---

## Пример 4: Менеджер памяти с VMA и Tracy

```cpp
// src/memory/GPUMemoryManager.hpp
#pragma once

#include "tracy/Tracy.hpp"
#include "vk_mem_alloc.h"
#include <unordered_map>

namespace projectv {

class GPUMemoryManager {
    VmaAllocator m_allocator;
    std::unordered_map<VmaAllocation, const char*> m_allocationNames;

public:
    void init(VkPhysicalDevice physicalDevice, VkDevice device, VkInstance instance) {
        VmaAllocatorCreateInfo createInfo{};
        createInfo.physicalDevice = physicalDevice;
        createInfo.device = device;
        createInfo.instance = instance;
        createInfo.vulkanApiVersion = VK_API_VERSION_1_4;

        vmaCreateAllocator(&createInfo, &m_allocator);

        TracyMessageL("VMA allocator created");
    }

    struct BufferResult {
        VkBuffer buffer;
        VmaAllocation allocation;
        void* mappedData;
    };

    BufferResult createBuffer(size_t size, VkBufferUsageFlags usage,
                              VmaMemoryUsage memoryUsage, const char* name) {
        ZoneScopedNC("CreateBuffer", ProfilingColors::Memory);

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = memoryUsage;

        if (memoryUsage == VMA_MEMORY_USAGE_CPU_TO_GPU ||
            memoryUsage == VMA_MEMORY_USAGE_GPU_TO_CPU) {
            allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }

        BufferResult result;
        VmaAllocationInfo allocationInfo;
        vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo,
                       &result.buffer, &result.allocation, &allocationInfo);

        result.mappedData = allocationInfo.pMappedData;

        // Отслеживание в Tracy
        TracyAllocN(result.allocation, size, name);
        m_allocationNames[result.allocation] = name;

        TracyPlot(name, (int64_t)size);

        return result;
    }

    void destroyBuffer(VkBuffer buffer, VmaAllocation allocation) {
        auto it = m_allocationNames.find(allocation);
        if (it != m_allocationNames.end()) {
            TracyFreeN(allocation, it->second);
            m_allocationNames.erase(it);
        }

        vmaDestroyBuffer(m_allocator, buffer, allocation);
    }

    void cleanup() {
        // Освободить все оставшиеся аллокации в Tracy
        for (const auto& [allocation, name] : m_allocationNames) {
            TracyFreeN(allocation, name);
        }
        m_allocationNames.clear();

        vmaDestroyAllocator(m_allocator);
    }

    size_t getTotalAllocated() const {
        VmaStats stats;
        vmaCalculateStats(m_allocator, &stats);
        return stats.total.bytesAllocated;
    }
};

} // namespace projectv
```

---

## Пример 5: Генератор чанков

```cpp
// src/world/ChunkGenerator.cpp
#include "tracy/Tracy.hpp"
#include "ChunkGenerator.hpp"

namespace projectv {

Chunk ChunkGenerator::generateChunk(const ChunkCoord& coord) {
    ZoneScopedNC("GenerateChunk", ProfilingColors::ChunkGen);

    Chunk chunk;
    chunk.coord = coord;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Этап 1: Генерация шума
    {
        ZoneScopedNC("Noise", ProfilingColors::ChunkGen);
        generateNoise(chunk, coord);
    }

    // Этап 2: Карвинг (пещеры)
    {
        ZoneScopedNC("Carving", ProfilingColors::ChunkGen);
        carveCaves(chunk);
    }

    // Этап 3: Размещение объектов
    {
        ZoneScopedNC("Features", ProfilingColors::ChunkGen);
        placeFeatures(chunk);
    }

    // Этап 4: Построение меша
    {
        ZoneScopedNC("MeshBuild", ProfilingColors::ChunkGen);
        buildMesh(chunk);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration<float, std::milli>(endTime - startTime).count();

    // Метрики
    TracyPlot("ChunkGen_TimeMS", ms);
    TracyPlot("ChunkGen_Voxels", (int64_t)chunk.voxels.size());
    TracyPlot("ChunkGen_Vertices", (int64_t)chunk.vertices.size());

    if (ms > 10.0f) {
        TracyMessageLC(fmt::format("Slow chunk gen at ({},{},{}): {:.1f}ms",
                                   coord.x, coord.y, coord.z, ms).c_str(), 0xFFFF00);
    }

    return chunk;
}

void ChunkGenerator::generateNoise(Chunk& chunk, const ChunkCoord& coord) {
    ZoneScopedNC("GenerateNoise", ProfilingColors::ChunkGen);

    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                float noise = m_noiseGenerator.noise(
                    (coord.x * CHUNK_SIZE + x) * 0.01f,
                    (coord.y * CHUNK_SIZE + y) * 0.01f,
                    (coord.z * CHUNK_SIZE + z) * 0.01f
                );

                chunk.voxels[x][y][z] = noise > 0.5f ? 1 : 0;
            }
        }
    }
}

} // namespace projectv
```

---

## Пример 6: Streaming система

```cpp
// src/world/ChunkStreamer.cpp
#include "tracy/Tracy.hpp"
#include "ChunkStreamer.hpp"

namespace projectv {

ChunkStreamer::ChunkStreamer(size_t workerCount) {
    ZoneScopedNC("StreamerInit", ProfilingColors::Streaming);

    m_running = true;

    for (size_t i = 0; i < workerCount; i++) {
        m_workers.emplace_back(&ChunkStreamer::workerThread, this);
    }

    TracyMessageL(fmt::format("ChunkStreamer started with {} workers", workerCount).c_str());
}

ChunkStreamer::~ChunkStreamer() {
    m_running = false;
    m_condition.notify_all();

    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ChunkStreamer::requestLoad(const ChunkCoord& coord, LoadPriority priority) {
    ZoneScopedNC("RequestLoad", ProfilingColors::Streaming);

    {
        std::lock_guard lock(m_queueMutex);
        m_loadQueue.push({coord, priority});
    }

    m_condition.notify_one();

    TracyPlot("Streaming_PendingLoads", (int64_t)m_loadQueue.size());
}

void ChunkStreamer::workerThread() {
    TracyMessageL("Streaming worker started");

    while (m_running) {
        ChunkLoadRequest request;

        {
            std::unique_lock lock(m_queueMutex);
            m_condition.wait(lock, [this] {
                return !m_loadQueue.empty() || !m_running;
            });

            if (!m_running) break;

            if (m_loadQueue.empty()) continue;

            request = m_loadQueue.top();
            m_loadQueue.pop();
        }

        // Обработка запроса
        ZoneScopedNC("LoadChunk", ProfilingColors::Streaming);

        Chunk chunk = m_generator.generateChunk(request.coord);

        {
            ZoneScopedNC("UploadToGPU", ProfilingColors::Streaming);
            uploadChunkToGPU(chunk);
        }

        {
            std::lock_guard lock(m_loadedMutex);
            m_loadedChunks.push(chunk);
        }

        TracyPlot("Streaming_LoadedChunks", (int64_t)m_loadedChunks.size());
    }

    TracyMessageL("Streaming worker stopped");
}

} // namespace projectv
```

---

## Пример 7: Профилирование с ImGui интеграцией

```cpp
// src/ui/ProfilerWindow.cpp
#include "tracy/Tracy.hpp"
#include "imgui.h"
#include "ProfilerWindow.hpp"

namespace projectv::ui {

void ProfilerWindow::render() {
    ZoneScopedNC("ProfilerWindow", 0xFF00FF);

    if (!m_visible) return;

    ImGui::Begin("Profiler", &m_visible);

    // FPS график
    ImGui::Text("Performance");
    ImGui::Separator();

    ImGui::Text("FPS: %.1f", m_fps);
    ImGui::Text("Frame: %.2f ms", m_frameTime);
    ImGui::Text("GPU: %.2f ms", m_gpuTime);

    ImGui::Spacing();

    // Memory usage
    ImGui::Text("Memory");
    ImGui::Separator();

    ImGui::Text("Voxel Data: %.1f MB", m_voxelMemory / (1024.0 * 1024.0));
    ImGui::Text("GPU Memory: %.1f MB", m_gpuMemory / (1024.0 * 1024.0));
    ImGui::Text("ECS Memory: %.1f MB", m_ecsMemory / (1024.0 * 1024.0));

    ImGui::Spacing();

    // Chunk stats
    ImGui::Text("Chunks");
    ImGui::Separator();

    ImGui::Text("Total: %zu", m_totalChunks);
    ImGui::Text("Visible: %zu", m_visibleChunks);
    ImGui::Text("Loading: %zu", m_loadingChunks);

    ImGui::Spacing();

    // Tracy status
    ImGui::Text("Tracy");
    ImGui::Separator();

    bool connected = TracyIsConnected;
    ImGui::Text("Status: %s", connected ? "Connected" : "Disconnected");

    if (ImGui::Button(connected ? "Disconnect" : "Connect")) {
        // Tracy управляет подключением автоматически
    }

    if (ImGui::Button("Save Trace")) {
        TracyMessageL("Save trace requested from UI");
    }

    ImGui::End();

    // Обновление Plots
    TracyPlot("UI_WindowCount", (int64_t)m_windowCount);
}

} // namespace projectv::ui
```

---

## Пример 8: Конфигурация CMakeLists.txt

```cmake
# CMakeLists.txt (фрагмент для Tracy)

# Опции Tracy
option(TRACY_ENABLE "Enable Tracy profiling" ON)
option(TRACY_ON_DEMAND "Enable on-demand profiling" OFF)

if(TRACY_ENABLE)
    # Добавить Tracy как subdirectory
    add_subdirectory(${CMAKE_SOURCE_DIR}/external/tracy)

    # Определения для Tracy
    target_compile_definitions(ProjectV PRIVATE
        TRACY_ENABLE
    )

    # Debug конфигурация
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(ProjectV PRIVATE
            TRACY_CALLSTACK=16
            TRACY_MEMORY
            TRACY_VERBOSE
        )
    endif()

    # Release конфигурация с минимальным overhead
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_definitions(ProjectV PRIVATE
            TRACY_CALLSTACK=4
        )

        if(TRACY_ON_DEMAND)
            target_compile_definitions(ProjectV PRIVATE
                TRACY_ON_DEMAND
            )
        endif()

        # Отключить системное профилирование для release
        target_compile_definitions(ProjectV PRIVATE
            TRACY_NO_SAMPLING
            TRACY_NO_SYSTEM_TRACING
            TRACY_NO_CONTEXT_SWITCH
        )
    endif()

    # Линковка
    target_link_libraries(ProjectV PRIVATE
        Tracy::TracyClient
    )

    # Include директории
    target_include_directories(ProjectV PRIVATE
        ${CMAKE_SOURCE_DIR}/external/tracy/public
    )

    message(STATUS "Tracy profiling: ENABLED")
else()
    message(STATUS "Tracy profiling: DISABLED")
endif()
```

---

## Пример использования

```cpp
// Полный пример использования
#include "tracy/Tracy.hpp"
#include "tracy/TracyVulkan.hpp"

int main() {
    // Информация о приложении
    TracyAppInfo("ProjectV v0.0.1", 14);

    // Инициализация Vulkan и Tracy
    // ...

    while (running) {
        FrameMark;

        {
            ZoneScopedNC("Update", 0x00FF00);

            // Обновление мира
            world.update(dt);

            TracyPlot("EntityCount", (int64_t)world.entityCount());
        }

        {
            ZoneScopedNC("Render", 0x0000FF);

            // Рендеринг
            TracyVkZone(tracyCtx, cmd, "GPU_Frame");
            renderer.render(cmd);

            TracyVkCollect(tracyCtx, queue);
        }
    }

    return 0;
}
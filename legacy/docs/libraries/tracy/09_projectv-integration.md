# Tracy в ProjectV: Интеграция

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
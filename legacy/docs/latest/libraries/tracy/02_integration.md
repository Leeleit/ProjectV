# Tracy в ProjectV: Интеграция

Документ описывает интеграцию Tracy в воксельный движок ProjectV. Рассматриваются: CMake-конфигурация, Vulkan 1.4, Flecs
ECS, VMA, SDL3. OpenGL и DirectX нам не нужны — только современный стек.

## CMake-конфигурация

### Базовая интеграция

```cmake
# CMakeLists.txt (фрагмент)
option(TRACY_ENABLE "Enable Tracy profiling" ON)

if(TRACY_ENABLE)
    add_subdirectory(${CMAKE_SOURCE_DIR}/external/tracy)

    target_compile_definitions(ProjectV PRIVATE TRACY_ENABLE)

    target_link_libraries(ProjectV PRIVATE Tracy::TracyClient)

    target_include_directories(ProjectV PRIVATE
        ${CMAKE_SOURCE_DIR}/external/tracy/public
    )

    message(STATUS "Tracy profiling: ENABLED")
else()
    message(STATUS "Tracy profiling: DISABLED")
endif()
```

### Профили Debug/Release

```cmake
# cmake/TracyConfig.cmake

option(TRACY_ENABLE "Enable Tracy profiling" ON)
option(TRACY_ON_DEMAND "On-demand profiling" OFF)

if(TRACY_ENABLE)
    add_subdirectory(${CMAKE_SOURCE_DIR}/external/tracy)

    target_compile_definitions(ProjectV PRIVATE TRACY_ENABLE)

    # Debug: полное профилирование
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(ProjectV PRIVATE
            TRACY_CALLSTACK=16
            TRACY_MEMORY
            TRACY_VERBOSE
        )
    endif()

    # Release: минимальный overhead
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_definitions(ProjectV PRIVATE
            TRACY_CALLSTACK=4
        )

        if(TRACY_ON_DEMAND)
            target_compile_definitions(ProjectV PRIVATE TRACY_ON_DEMAND)
        endif()

        # Отключаем системное профилирование
        target_compile_definitions(ProjectV PRIVATE
            TRACY_NO_SAMPLING
            TRACY_NO_SYSTEM_TRACING
            TRACY_NO_CONTEXT_SWITCH
        )
    endif()

    target_link_libraries(ProjectV PRIVATE Tracy::TracyClient)
endif()
```

### Условное отключение для distribution

```cmake
# Для финальных билдов — без профилирования
option(TRACY_DISTRIBUTION_BUILD "Disable profiling for distribution" OFF)

if(TRACY_DISTRIBUTION_BUILD)
    target_compile_definitions(ProjectV PRIVATE TRACY_ENABLE=0)
endif()
```

## Единая точка включения

### Profiling.hpp

```cpp
// src/core/Profiling.hpp
#pragma once

#ifdef TRACY_ENABLE
    #include "tracy/Tracy.hpp"
    #include "tracy/TracyVulkan.hpp"
#else
    // No-op макросы когда Tracy отключён
    #define ZoneScoped
    #define ZoneScopedN(name)
    #define ZoneScopedC(color)
    #define ZoneScopedNC(name, color)
    #define ZoneNamed(name, active)
    #define ZoneTransient(name, active)
    #define ZoneTransientN(name, str, active)
    #define ZoneText(zone, text, size)
    #define ZoneTextF(zone, fmt, ...)
    #define ZoneValue(zone, val)
    #define FrameMark
    #define FrameMarkNamed(name)
    #define FrameMarkStart(name)
    #define FrameMarkEnd(name)
    #define TracyPlot(name, val)
    #define TracyMessage(text, size)
    #define TracyMessageL(text)
    #define TracyMessageC(text, size, color)
    #define TracyMessageLC(text, color)
    #define TracyAlloc(ptr, size)
    #define TracyFree(ptr)
    #define TracyAllocN(ptr, size, name)
    #define TracyFreeN(ptr, name)
    #define TracySecureAlloc(ptr, size)
    #define TracySecureFree(ptr)
    #define TracyIsConnected 0
#endif

// Цветовая схема для подсистем ProjectV
namespace ProjectV::ProfilingColors {
    constexpr uint32_t SDLEvents    = 0x00FFFF;  // Голубой — ввод
    constexpr uint32_t ECS          = 0x00FF00;  // Зелёный — ECS системы
    constexpr uint32_t Render       = 0x0000FF;  // Синий — рендеринг
    constexpr uint32_t GPUCompute  = 0x8800FF;  // Фиолетовый — compute shaders
    constexpr uint32_t Physics     = 0xFF8800;  // Оранжевый — физика
    constexpr uint32_t ChunkGen    = 0xFFFF00;  // Жёлтый — генерация чанков
    constexpr uint32_t Streaming   = 0xFF0000;  // Красный — стриминг
    constexpr uint32_t Audio       = 0xFF00FF;  // Розовый — аудио
    constexpr uint32_t Memory      = 0xFFFFFF;  // Белый — память
    constexpr uint32_t Loading     = 0x88FF88;  // Светло-зелёный — загрузка
}

// Удобные макросы для ProjectV
#define PV_PROFILE_FRAME()          FrameMark
#define PV_PROFILE_SDL()            ZoneScopedNC("SDL_Events", ProjectV::ProfilingColors::SDLEvents)
#define PV_PROFILE_ECS(name)        ZoneScopedNC(name, ProjectV::ProfilingColors::ECS)
#define PV_PROFILE_RENDER(name)     ZoneScopedNC(name, ProjectV::ProfilingColors::Render)
#define PV_PROFILE_GPU(name)        ZoneScopedNC(name, ProjectV::ProfilingColors::GPUCompute)
#define PV_PROFILE_PHYSICS()        ZoneScopedNC("Physics", ProjectV::ProfilingColors::Physics)
#define PV_PROFILE_CHUNK()          ZoneScopedNC("ChunkGen", ProjectV::ProfilingColors::ChunkGen)
#define PV_PROFILE_STREAMING()      ZoneScopedNC("Streaming", ProjectV::ProfilingColors::Streaming)
#define PV_PROFILE_AUDIO()         ZoneScopedNC("Audio", ProjectV::ProfilingColors::Audio)
#define PV_PROFILE_MEMORY(name)     ZoneScopedNC(name, ProjectV::ProfilingColors::Memory)
```

## Vulkan 1.4 интеграция

### Класс-обёртка

```cpp
// src/renderer/TracyVulkan.hpp
#pragma once

#ifdef TRACY_ENABLE
    #include "tracy/TracyVulkan.hpp"
#endif

#include <vulkan/vulkan.h>
#include <expected>

namespace projectv {

class TracyVulkan {
#ifdef TRACY_ENABLE
    tracy::VkCtx* m_context = nullptr;
#else
    void* m_dummy = nullptr;
#endif

public:
    [[nodiscard]] constexpr bool isEnabled() const noexcept {
#ifdef TRACY_ENABLE
        return m_context != nullptr;
#else
        return false;
#endif
    }

    [[nodiscard]] std::expected<void, std::error_code> init(
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        VkQueue queue,
        VkCommandBuffer initCmd
    ) noexcept {
#ifdef TRACY_ENABLE
        m_context = tracy::CreateVkContext(physicalDevice, device, queue, initCmd);

        if (m_context == nullptr) {
            return std::unexpected(std::errc::operation_failed);
        }

        TracyMessageL("Tracy Vulkan context initialized");
        return {};
#else
        (void)physicalDevice;
        (void)device;
        (void)queue;
        (void)initCmd;
        return {};
#endif
    }

    void zone(VkCommandBuffer cmd, const char* name) const noexcept {
#ifdef TRACY_ENABLE
        TracyVkZone(m_context, cmd, name);
#endif
    }

    void zone(VkCommandBuffer cmd, const char* name, uint32_t color) const noexcept {
#ifdef TRACY_ENABLE
        TracyVkZoneC(m_context, cmd, name, color);
#endif
    }

    template<std::size_t N>
    void zone(VkCommandBuffer cmd, const char (&name)[N]) const noexcept {
        zone(cmd, static_cast<const char*>(name));
    }

    void collect(VkQueue queue) const noexcept {
#ifdef TRACY_ENABLE
        TracyVkCollect(m_context, queue);
#endif
    }

    void destroy(VkDevice device) noexcept {
#ifdef TRACY_ENABLE
        if (m_context != nullptr) {
            tracy::DestroyVkContext(m_context);
            m_context = nullptr;
        }
#endif
        (void)device;
    }

#ifdef TRACY_ENABLE
    [[nodiscard]] tracy::VkCtx* context() noexcept { return m_context; }
    [[nodiscard]] const tracy::VkCtx* context() const noexcept { return m_context; }
#endif
};

} // namespace projectv
```

### Интеграция в Renderer

```cpp
// src/renderer/VulkanRenderer.cpp (фрагмент)
#include "core/Profiling.hpp"
#include "renderer/TracyVulkan.hpp"

void VulkanRenderer::init() {
    // ... инициализация Vulkan ...

    // Tracy инициализация
    VkCommandBuffer initCmd = beginSingleTimeCommands();

    auto result = m_tracy.init(
        m_physicalDevice,
        m_device,
        m_graphicsQueue,
        initCmd
    );

    if (!result) {
        std::println(stderr, "Warning: Tracy init failed, profiling disabled");
    }

    endSingleTimeCommands(initCmd);
}

void VulkanRenderer::render(VkCommandBuffer cmd) {
    m_tracy.zone(cmd, "RenderFrame");

    // Shadow pass
    {
        m_tracy.zone(cmd, "ShadowPass");
        renderShadows(cmd);
    }

    // Geometry pass
    {
        m_tracy.zone(cmd, "GeometryPass");
        renderGeometry(cmd);
    }

    // Voxel compute
    {
        m_tracy.zone(cmd, "VoxelCompute");
        dispatchVoxelCompute(cmd);
    }

    // Post-process
    {
        m_tracy.zone(cmd, "PostProcess");
        renderPostProcess(cmd);
    }
}

void VulkanRenderer::present() {
    // ... submit ...

    // Сбор GPU данных послеQueueSubmit
    m_tracy.collect(m_graphicsQueue);
}

void VulkanRenderer::cleanup() {
    m_tracy.destroy(m_device);
}
```

## Flecs ECS интеграция

### Профилирование систем

```cpp
// src/ecs/SystemProfiler.hpp
#pragma once

#include "flecs.h"
#include "core/Profiling.hpp"

namespace projectv::ecs {

// Макрос для создания профилируемой ECS системы
#define PV_ECS_SYSTEM(name, color) \
    struct name##System { \
        static constexpr const char* k_Name = #name; \
        static constexpr uint32_t k_Color = color; \
        void update(flecs::world& world, float dt); \
    };

// Базовый класс профилируемой системы
class ProfiledSystem {
protected:
    const char* m_name;
    uint32_t m_color;

public:
    constexpr ProfiledSystem(const char* name, uint32_t color) noexcept
        : m_name(name), m_color(color) {}

    constexpr auto name() const noexcept { return m_name; }
    constexpr auto color() const noexcept { return m_color; }

    void beginProfile() noexcept { ZoneScopedNC(m_name, m_color); }
    // Деструктор ZoneScoped автоматически завершает зону
};

// Профилируемая система с итератором - C++26 Deducing This паттерн
// Заменяет виртуальные методы на статический полиморфизм для zero-cost abstraction
template<typename Components>
class ProfiledIterSystem : public ProfiledSystem {
public:
    using ProfiledSystem::ProfiledSystem;

    // Deducing This - C++26 позволяет вызывать без virtual
    void run(this auto&& self, flecs::iter& it) const noexcept {
        ZoneScopedNC(self.m_name, self.m_color);

        Components* comps = it.get<Components>();
        for (auto i : it) {
            self.processEntity(it, i, comps[i]);
        }
    }

    // Реализация должна быть в конкретном типе
    template<typename Self>
    void processEntity(this Self&& self, flecs::iter& it, size_t i, Components& comps);
};

} // namespace projectv::ecs
```

### Пример системы

```cpp
// src/ecs/systems/MovementSystem.cpp
#include "ecs/SystemProfiler.hpp"

namespace projectv::systems {

struct Position {
    float x, y, z;
};

struct Velocity {
    float x, y, z;
};

PV_ECS_SYSTEM(Movement, ProfilingColors::ECS);

void MovementSystem::update(flecs::world& world, float dt) {
    ZoneScopedNC("MovementSystem", ProfilingColors::ECS);

    size_t processed = 0;

    world.each([&](flecs::entity e, Position& pos, const Velocity& vel) {
        // Пропуск статичных сущностей
        if (vel.x == 0.0f && vel.y == 0.0f && vel.z == 0.0f) {
            return;
        }

        pos.x += vel.x * dt;
        pos.y += vel.y * dt;
        pos.z += vel.z * dt;

        processed++;
    });

    TracyPlot("Movement_Entities", static_cast<int64_t>(processed));
}

} // namespace projectv::systems
```

### Регистрация систем

```cpp
void Game::initECS() {
    flecs::world& world = m_world;

    // Регистрация систем с профилированием
    world.system<Position, Velocity>("Movement")
        .kind(flecs::PreUpdate)
        .each([](flecs::entity e, Position& p, const Velocity& v) {
            ZoneScopedNC("Movement_Each", ProfilingColors::ECS);
            p.x += v.x * 0.016f;
            p.y += v.y * 0.016f;
            p.z += v.z * 0.016f;
        });

    world.system<Position, Renderable>("RenderSync")
        .kind(flecs::PostUpdate)
        .each([](Position& p, Renderable& r) {
            ZoneScopedNC("RenderSync", ProfilingColors::Render);
            r.worldMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(p.x, p.y, p.z));
        });
}
```

## VMA Memory интеграция

### RAII-обёртка для буферов

```cpp
// src/memory/TrackedBuffer.hpp
#pragma once

#include "vk_mem_alloc.h"
#include "core/Profiling.hpp"

#include <span>
#include <expected>
#include <error_code>

namespace projectv::memory {

// Категории аллокаций
namespace Category {
    constexpr const char* VertexBuffer = "VMA_VertexBuffer";
    constexpr const char* IndexBuffer = "VMA_IndexBuffer";
    constexpr const char* StagingBuffer = "VMA_StagingBuffer";
    constexpr const char* UniformBuffer = "VMA_UniformBuffer";
    constexpr const char* StorageBuffer = "VMA_StorageBuffer";
    constexpr const char* ChunkData = "VMA_ChunkData";
    constexpr const char* Texture = "VMA_Texture";
    constexpr const char* IndirectBuffer = "VMA_IndirectBuffer";
}

template<const char* CategoryName>
class TrackedBuffer {
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VmaAllocator m_allocator = nullptr;
    VkDeviceSize m_size = 0;

public:
    TrackedBuffer() noexcept = default;

    TrackedBuffer(VmaAllocator allocator, VkDeviceSize size,
                  VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) noexcept
        : m_allocator(allocator), m_size(size)
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = memoryUsage;

        vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
                       &m_buffer, &m_allocation, nullptr);

#ifdef TRACY_ENABLE
        TracyAllocN(m_allocation, static_cast<size_t>(size), CategoryName);
#endif
    }

    // Non-copyable
    TrackedBuffer(const TrackedBuffer&) = delete;
    TrackedBuffer& operator=(const TrackedBuffer&) = delete;

    // Movable
    TrackedBuffer(TrackedBuffer&& other) noexcept
        : m_buffer(std::exchange(other.m_buffer, VK_NULL_HANDLE))
        , m_allocation(std::exchange(other.m_allocation, VK_NULL_HANDLE))
        , m_allocator(other.m_allocator)
        , m_size(other.m_size)
    {}

    TrackedBuffer& operator=(TrackedBuffer&& other) noexcept {
        if (this != &other) {
            destroy();
            m_buffer = std::exchange(other.m_buffer, VK_NULL_HANDLE);
            m_allocation = std::exchange(other.m_allocation, VK_NULL_HANDLE);
            m_allocator = other.m_allocator;
            m_size = other.m_size;
        }
        return *this;
    }

    ~TrackedBuffer() { destroy(); }

    void destroy() noexcept {
        if (m_allocation != VK_NULL_HANDLE) {
#ifdef TRACY_ENABLE
            TracyFreeN(m_allocation, CategoryName);
#endif
            vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
            m_buffer = VK_NULL_HANDLE;
            m_allocation = VK_NULL_HANDLE;
        }
    }

    [[nodiscard]] VkBuffer buffer() const noexcept { return m_buffer; }
    [[nodiscard]] VmaAllocation allocation() const noexcept { return m_allocation; }
    [[nodiscard]] VkDeviceSize size() const noexcept { return m_size; }
    [[nodiscard]] explicit operator bool() const noexcept {
        return m_buffer != VK_NULL_HANDLE;
    }
};

// Типизированные буферы
using VertexBuffer = TrackedBuffer<Category::VertexBuffer>;
using IndexBuffer = TrackedBuffer<Category::IndexBuffer>;
using UniformBuffer = TrackedBuffer<Category::UniformBuffer>;
using StorageBuffer = TrackedBuffer<Category::StorageBuffer>;
using ChunkBuffer = TrackedBuffer<Category::ChunkData>;
using IndirectBuffer = TrackedBuffer<Category::IndirectBuffer>;

} // namespace projectv::memory
```

### Memory Manager

```cpp
// src/memory/MemoryManager.cpp
#include "memory/TrackedBuffer.hpp"

namespace projectv::memory {

class MemoryManager {
    VmaAllocator m_allocator = nullptr;

public:
    void init(VkPhysicalDevice physicalDevice, VkDevice device,
               VkInstance instance) noexcept {
        VmaAllocatorCreateInfo createInfo{};
        createInfo.physicalDevice = physicalDevice;
        createInfo.device = device;
        createInfo.instance = instance;
        createInfo.vulkanApiVersion = VK_API_VERSION_1_4;

        vmaCreateAllocator(&createInfo, &m_allocator);
        TracyMessageL("VMA allocator initialized");
    }

    void cleanup() noexcept {
        vmaDestroyAllocator(m_allocator);
        m_allocator = nullptr;
    }

    // Создание буфера с автотрекингом
    template<const char* CategoryName>
    [[nodiscard]] TrackedBuffer<CategoryName> createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VmaMemoryUsage memoryUsage
    ) noexcept {
        return TrackedBuffer<CategoryName>(m_allocator, size, usage, memoryUsage);
    }

    // Метрики
    void updateMetrics() noexcept {
        VmaStats stats{};
        vmaCalculateStats(m_allocator, &stats);

        TracyPlot("VMA_TotalAllocatedMB",
                  static_cast<int64_t>(stats.total.bytesAllocated / (1024 * 1024)));
        TracyPlot("VMA_TotalUsedMB",
                  static_cast<int64_t>(stats.total.bytesUsed / (1024 * 1024)));
        TracyPlot("VMA_BufferCount",
                  static_cast<int64_t>(stats.bufferCount));
        TracyPlot("VMA_AllocationCount",
                  static_cast<int64_t>(stats.allocationCount));
    }
};

} // namespace projectv::memory
```

## SDL3 callback интеграция

### Профилирование event processing

```cpp
// src/sdl/EventProcessor.cpp
#include "core/Profiling.hpp"

namespace project {

classv::sdl EventProcessor {
public:
    void processEvents() noexcept {
        PV_PROFILE_SDL();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    TracyMessageL("SDL_EVENT_QUIT");
                    m_running = false;
                    break;

                case SDL_EVENT_KEY_DOWN:
                    handleKeyDown(event.key);
                    break;

                case SDL_EVENT_MOUSE_MOTION:
                    handleMouseMotion(event.motion);
                    break;

                // ... другие события
            }
        }
    }

private:
    bool m_running = true;

    void handleKeyDown(const SDL_KeyboardEvent& event) noexcept {
        ZoneScopedNC("Input_KeyDown", ProfilingColors::SDLEvents);
        // Обработка ввода...
    }

    void handleMouseMotion(const SDL_MouseMotionEvent& event) noexcept {
        ZoneScopedNC("Input_MouseMotion", ProfilingColors::SDLEvents);
        // Обработка движения мыши...
    }
};

} // namespace projectv::sdl
```

## Главный игровой цикл

```cpp
// src/main.cpp
#include "core/Profiling.hpp"
#include "Game.hpp"

int main() {
    Tracy_APPINFO("ProjectV", 8);

    Tracy_MESSAGE("Starting ProjectV...");

    projectv::Game game;

    auto initResult = game.init();
    if (!initResult) {
        std::println(stderr, "Game init failed: {}", initResult.error().message());
        return 1;
    }

    Tracy_MESSAGE("Game initialized, entering main loop");

    while (game.isRunning()) {
        FrameMark;

        // SDL Events
        {
            PV_PROFILE_SDL();
            game.processEvents();
        }

        // ECS Update
        {
            PV_PROFILE_ECS("ECS_Update");
            float dt = game.deltaTime();
            game.ecsWorld().progress(dt);
        }

        // Render
        {
            PV_PROFILE_RENDER("Render");
            game.render();
        }

        // Метрики
        TracyPlot("FPS", static_cast<int64_t>(game.fps()));
        TracyPlot("FrameTime", game.frameTimeMs());
        TracyPlot("EntityCount", static_cast<int64_t>(game.entityCount()));
    }

    Tracy_MESSAGE("Shutting down...");
    game.cleanup();

    return 0;
}
```

## Сборка Tracy Profiler

### Windows

```batch
@echo off
cd external\tracy\profiler
if not exist build mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -A x64
cmake --build . --config Release
copy Release\tracy-profiler.exe ..\..\..\bin\
```

### Linux

```bash
#!/bin/bash
cd external/tracy/profiler
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cp tracy-profiler ../../../bin/
```

## Устранение проблем

### Компиляция

**Ошибка: undefined reference to tracy**

```cmake
# Убедитесь что линкуется TracyClient
target_link_libraries(ProjectV PRIVATE Tracy::TracyClient)
```

**Ошибка: multiple definition**

```cmake
# Не добавляйте TracyClient.cpp вручную
# Используйте только цель Tracy::TracyClient
```

### Подключение

**Сервер не видит приложение**

1. Проверьте брандмауэр (порт 8947)
2. Убедитесь что сервер и клиент в одной сети
3. Запустите Tracy от имени администратора

### Производительность

**Высокий overhead**

```cmake
# В CMake для Release
target_compile_definitions(ProjectV PRIVATE
    TRACY_CALLSTACK=4
    TRACY_NO_SAMPLING
    TRACY_NO_SYSTEM_TRACING
)
```

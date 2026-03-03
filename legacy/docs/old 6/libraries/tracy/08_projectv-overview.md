# Tracy в ProjectV: Обзор

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

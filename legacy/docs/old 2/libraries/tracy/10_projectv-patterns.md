# Tracy в ProjectV: Паттерны

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

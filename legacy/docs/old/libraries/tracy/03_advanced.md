# Tracy: Хардкорные оптимизации

Документ описывает продвинутые техники профилирования в ProjectV: DOD-паттерны, Job System интеграцию, SoA для метрик,
zero-overhead профилирование и GPU-driven паттерны. Всё в контексте воксельного движка с тысячами сущностей и compute
shaders.

> **Для понимания:** Стандартное профилирование — как врач с фонендоскопом. Он слышит сердце (основной поток), но не
> слышит работу почек (background jobs), не видит микротрещины в костях (GPU stalls). Хардкорное профилирование — как
> полное МРТ всего тела: видно всё, но оборудование дорогое, а процедура долгая. Наша задача: получить максимум
> информации
> с минимальным ущербом для производительности.

## DOD-паттерны для профилирования

### SoA vs AoS для метрик

Традиционный подход (AoS) — массив структур:

```cpp
// ❌ Плохо: Array of Structures
struct FrameMetric {
    float frameTime;
    size_t entityCount;
    size_t drawCalls;
    float gpuTime;
};

std::vector<FrameMetric> metrics;
```

**Проблема:** При обновлении `frameTime` мы читаем/пишем 24 байта (весь кэш-ли),нию хотя нужно только 4.

**Решение:** Structure of Arrays (SoA):

```cpp
// ✅ Хорошо: Structure of Arrays
alignas(64) struct FrameMetricsSoA {
    std::vector<float> frameTimes;       // 4 байта/элемент
    std::vector<size_t> entityCounts;    // 8 байт/элемент
    std::vector<size_t> drawCalls;       // 8 байт/элемент
    std::vector<float> gpuTimes;          // 4 байта/элемент
};

class ProfilerMetrics {
    static constexpr std::size_t k_HistorySize = 256;

    alignas(64) float     m_frameTimes[k_HistorySize];
    alignas(64) size_t    m_entityCounts[k_HistorySize];
    alignas(64) size_t    m_drawCalls[k_HistorySize];
    alignas(64) float     m_gpuTimes[k_HistorySize];

    std::atomic<std::size_t> m_index{0};

public:
    void record(float frameTime, size_t entities, size_t draws, float gpuTime) noexcept {
        std::size_t idx = m_index.fetch_add(1) % k_HistorySize;

        // Каждое поле —独立的 кэш-линия (или кратный блок)
        m_frameTimes[idx] = frameTime;
        m_entityCounts[idx] = entities;
        m_drawCalls[idx] = draws;
        m_gpuTimes[idx] = gpuTime;
    }

    // Tracy integration
    void pushToTracy() noexcept {
        std::size_t idx = (m_index.load() - 1) % k_HistorySize;

        TracyPlot("FPS", static_cast<int64_t>(1000.0f / m_frameTimes[idx]));
        TracyPlot("FrameTime", m_frameTimes[idx]);
        TracyPlot("EntityCount", static_cast<int64_t>(m_entityCounts[idx]));
        TracyPlot("DrawCalls", static_cast<int64_t>(m_drawCalls[idx]));
        TracyPlot("GPUTime", m_gpuTimes[idx]);
    }
};
```

> **Для понимания:** SoA — как склады с раздельными секциями. На одном складе (память) лежат все инструменты (
> структура), но когда нужен молоток — приходится перебирать весь ящик. SoA — это отдельные полки для молотков,
> отвёрток,
> гаечных ключей. Берёшь сразу то, что нужно, без лишних движений.

### Hot/Cold Data Separation

```cpp
// ❌ Плохо: всё в одной структуре
struct SystemStats {
    float avgUpdateTime;      // Hot — читается каждый кадр
    size_t entityCount;       // Hot
    std::string debugInfo;    // Cold — редко нужно
    std::vector<size_t> history;  // Cold
};

// ✅ Хорошо: раздельно
alignas(64) struct HotSystemStats {
    float avgUpdateTime = 0.0f;
    size_t entityCount = 0;
    float minTime = std::numeric_limits<float>::max();
    float maxTime = 0.0f;
    std::atomic<uint64_t> totalIterations{0};
};

alignas(64) struct ColdSystemStats {
    // Lazy-инициализируемые данные для отладки
    std::string lastSlowEntity;
    std::vector<float> perEntityTimes;
};
```

## Job System интеграция

### Tracy в Job System без std::thread

Главное правило: **никаких `std::thread` внутри кадра**. Job System должен переиспользовать потоки.

```cpp
// src/jobs/JobSystem.hpp
#pragma once

#include "core/Profiling.hpp"

#include <coroutine>
#include <functional>
#include <queue>
#include <array>
#include <atomic>

namespace projectv::jobs {

// Идентификатор job'а для Tracy
using JobId = uint32_t;

// Job с профилированием - C++26 Deducing This паттерн
// Заменяет виртуальные методы на статический полиморфизм
class ProfiledJob {
    const char* m_name;
    uint32_t m_color;
    // ... остальные поля job'а

public:
    ProfiledJob(const char* name, uint32_t color) noexcept
        : m_name(name), m_color(color) {}

    // Deducing This - C++26 позволяет вызывать без virtual
    void execute(this auto&& self) noexcept {
        ZoneScopedNC(self.m_name, self.m_color);
        self.run();
    }

    // Реализация должна быть в конкретном типе
    template<typename Self>
    void run(this Self&& self) noexcept;
};

// Job queue с Tracy
class JobQueue {
    static constexpr std::size_t k_QueueSize = 256;

    alignas(64) std::array<JobId, k_QueueSize> m_jobs;
    alignas(64) std::atomic<std::size_t> m_head{0};
    std::atomic<std::size_t> m_tail{0};

public:
    bool enqueue(JobId job) noexcept {
        std::size_t head = m_head.load(std::memory_order_relaxed);
        std::size_t nextHead = (head + 1) % k_QueueSize;

        if (nextHead == m_tail.load(std::memory_order_acquire)) {
            return false;  // Queue full
        }

m_jobs[head] = job;
        m_head.store(nextHead, std::memory_order_release);
        return true;
    }

    bool dequeue(JobId& job) noexcept {
        std::size_t tail = m_tail.load(std::memory_order_relaxed);

        if (tail == m_head.load(std::memory_order_acquire)) {
            return false;  // Queue empty
        }

        job = m_jobs[tail];
        m_tail.store((tail + 1) % k_QueueSize, std::memory_order_release);
        return true;
    }
};

// Worker на основе stdexec — НЕ создаёт потоки!
class StdexecWorker {
    JobQueue& m_queue;
    stdexec::scheduler auto scheduler_;

public:
    explicit StdexecWorker(JobQueue& queue, stdexec::scheduler auto scheduler = stdexec::get_default_scheduler()) 
        : m_queue(queue), scheduler_(scheduler) {
        
        // Запускаем обработку задач через stdexec
        startProcessing();
    }

private:
    void startProcessing() {
        // Создаем sender для обработки задач
        auto processTask = stdexec::schedule(scheduler_)
                         | stdexec::then([this]() -> bool {
                               JobId job;
                               if (m_queue.dequeue(job)) {
                                   executeJob(job);
                                   return true;
                               }
                               return false;
                           })
                         | stdexec::then([this](bool hadWork) {
                               // Если была работа, продолжаем немедленно
                               // Если нет, ждем немного
                               if (hadWork) {
                                   return stdexec::schedule(scheduler_);
                               } else {
                                   return stdexec::schedule_after(scheduler_, std::chrono::microseconds(100));
                               }
                           });

        // Запускаем циклическую обработку
        auto loopTask = stdexec::schedule(scheduler_)
                      | stdexec::then([this, processTask = std::move(processTask)]() mutable {
                            // Рекурсивно перезапускаем задачу
                            return processTask
                                 | stdexec::then([this, processTask = std::move(processTask)]() mutable {
                                       return loopTask;
                                   });
                        });

        // Запускаем асинхронно
        stdexec::start_detached(std::move(loopTask));
    }

    void executeJob(JobId job) noexcept {
        ZoneScopedNC("JobExecution", 0x00FF00);

        // Execute job...
        TracyPlot("PendingJobs", static_cast<int64_t>(m_queue.pendingCount()));
    }
};
```

### Zero-Copy профилирование Job'ов

```cpp
// ✅ Правильно: job регистрирует себя, Tracy пишет напрямую
class ZeroCopyJobProfiler {
    struct alignas(64) JobData {
        std::atomic<uint64_t> startTime{0};
        std::atomic<uint64_t> endTime{0};
        std::atomic<const char*> name{nullptr};
        std::atomic<uint32_t> color{0};
    };

    static constexpr std::size_t k_MaxJobs = 1024;
    alignas(64) std::array<JobData, k_MaxJobs> m_jobs;
    std::atomic<std::size_t> m_nextId{0};

public:
    JobId startJob(const char* name, uint32_t color) noexcept {
        JobId id = m_nextId.fetch_add(1) % k_MaxJobs;

        m_jobs[id].name.store(name, std::memory_order_relaxed);
        m_jobs[id].color.store(color, std::memory_order_relaxed);
        m_jobs[id].startTime.store(
            std::chrono::steady_clock::now().time_since_epoch().count(),
            std::memory_order_relaxed
        );

        return id;
    }

    void endJob(JobId id) noexcept {
        m_jobs[id].endTime.store(
            std::chrono::steady_clock::now().time_since_epoch().count(),
            std::memory_order_relaxed
        );

        // Tracy plot уже в job system, не в каждом job!
    }
};

// Job без Tracy overhead
class MinimalJob {
public:
    void execute() noexcept {
        // Zero Tracy overhead — профилирование на уровне job system
        // Tracy видит только агрегированные данные
        doWork();
    }
};
```

## On-Demand профилирование

### Условное профилирование с TracyIsConnected

```cpp
class AdaptiveProfiler {
    std::atomic<bool> m_profilingActive{false};

public:
    // Дешёвая проверка без overhead в hot path
    void update() noexcept {
        if (TracyIsConnected && !m_profilingActive.load()) {
            m_profilingActive.store(true);
            enableDetailedProfiling();
        } else if (!TracyIsConnected && m_profilingActive.load()) {
            m_profilingActive.store(false);
            disableDetailedProfiling();
        }
    }

    // Только когда Tracy подключен
    void profileExpensiveOperation() noexcept {
        if (m_profilingActive.load(std::memory_order_relaxed)) {
            ZoneScopedN("ExpensiveOperation");
            // Детальная работа...
        } else {
            // Без профилирования
            doWork();
        }
    }
};
```

### Threshold-based профилирование

```cpp
class ThresholdProfiler {
    static constexpr float k_SlowThresholdMs = 1.0f;
    static constexpr float k_VerySlowThresholdMs = 5.0f;

    std::chrono::steady_clock::time_point m_start;

public:
    explicit ThresholdProfiler(const char* name) noexcept {
        m_start = std::chrono::steady_clock::now();
    }

    ~ThresholdProfiler() noexcept {
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration<float, std::milli>(end - m_start).count();

        TracyPlot("SlowOps", ms);  // Всегда

        if (ms >= k_SlowThresholdMs) {
            TracyMessageLC("Slow operation detected", 0xFFFF00);
        }

        if (ms >= k_VerySlowThresholdMs) {
            TracyMessageLC("CRITICAL: Very slow operation!", 0xFF0000);
        }
    }
};

// Использование — zero overhead когда не нужно
void processChunks() {
    ThresholdProfiler timer("ChunkProcessing");  // RAII — автоматически

    // Работа...
    // Если >1ms — Tracy получит уведомление
}
```

## GPU-Driven паттерны

### Indirect Drawing с Tracy

```cpp
// src/renderer/VulkanRenderer.hpp (фрагмент)
#include "core/Profiling.hpp"

class VoxelRenderer {
    VkBuffer m_indirectBuffer = VK_NULL_HANDLE;
    VmaAllocation m_indirectAllocation = VK_NULL_HANDLE;

    // GPU-driven: indirect buffer обновляется на GPU
    // Tracy CPU-side показывает только high-level пайплайн
    struct IndirectCommand {
        uint32_t vertexCount;
        uint32_t instanceCount;
        uint32_t firstVertex;
        uint32_t firstInstance;
    };

public:
    void renderIndirect(VkCommandBuffer cmd, VkBuffer indirectBuffer) noexcept {
        // CPU зона: подготовка
        ZoneScopedNC("RenderIndirect", 0x8800FF);

        // GPU зона: исполнение
        TracyVkZone(m_tracyContext, cmd, "IndirectDraw");

        vkCmdDrawIndirect(cmd, indirectBuffer, 0,
                        m_drawCount, sizeof(IndirectCommand));
    }

    void updateIndirectBuffer(VkCommandBuffer cmd) noexcept {
        ZoneScopedNC("UpdateIndirect", 0x8800FF);

        // Обновление indirect commands на GPU через compute
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_cullPipeline);
        vkCmdDispatch(cmd, m_chunkCount / 256, 1, 1);
    }
};
```

### Multi-Queue профилирование

```cpp
// Несколько очередей — Tracy может отслеживать каждую
class MultiQueueRenderer {
    tracy::VkCtx* m_tracyCtx = nullptr;
    VkQueue m_graphicsQueue;
    VkQueue m_computeQueue;
    VkQueue m_transferQueue;

public:
    void init(tracy::VkCtx* ctx, VkQueue g, VkQueue c, VkQueue t) noexcept {
        m_tracyCtx = ctx;
        m_graphicsQueue = g;
        m_computeQueue = c;
        m_transferQueue = t;
    }

    void renderFrame() noexcept {
        // Graphics queue
        TracyVkZone(m_tracyCtx, m_graphicsCmd, "Graphics");

        // Compute queue — отдельная зона!
        TracyVkZone(m_tracyCtx, m_computeCmd, "ComputeCulling");

        // Transfer queue — асинхронная загрузка
        TracyVkZone(m_tracyCtx, m_transferCmd, "Transfer");
    }

    void collectAll() noexcept {
        TracyVkCollect(m_tracyCtx, m_graphicsQueue);
        TracyVkCollect(m_tracyCtx, m_computeQueue);
        TracyVkCollect(m_tracyCtx, m_transferQueue);
    }
};
```

## Lock-free метрики

### SPSC queue для метрик

```cpp
// Single Producer Single Consumer queue для метрик
// Никаких lock — минимальный overhead
template<typename T, std::size_t N>
class alignas(64) SpscMetricQueue {
    static_assert((N & (N - 1)) == 0, "N must be power of 2");

    alignas(64) std::array<T, N> m_buffer;
    alignas(64) std::atomic<std::size_t> m_writeIdx{0};
    char m_pad1[64];
    alignas(64) std::atomic<std::size_t> m_readIdx{0};
    char m_pad2[64];

public:
    bool push(const T& value) noexcept {
        std::size_t write = m_writeIdx.load(std::memory_order_relaxed);
        std::size_t nextWrite = (write + 1) & (N - 1);

        if (nextWrite == m_readIdx.load(std::memory_order_acquire)) {
            return false;  // Full
        }

        m_buffer[write] = value;
        m_writeIdx.store(nextWrite, std::memory_order_release);

        return true;
    }

    bool pop(T& value) noexcept {
        std::size_t read = m_readIdx.load(std::memory_order_relaxed);

        if (read == m_writeIdx.load(std::memory_order_acquire)) {
            return false;  // Empty
        }

        value = m_buffer[read];
        m_readIdx.store((read + 1) & (N - 1), std::memory_order_release);

        return true;
    }
};

// Использование: background thread -> main thread
struct FrameMetric {
    float frameTime;
    size_t entityCount;
    size_t drawCalls;
    float gpuTime;
};

alignas(64) SpscMetricQueue<FrameMetric, 64> g_metricQueue;

// Background job пишет метрики
void backgroundMetricsWriter() {
    FrameMetric metric;
    metric.frameTime = measureFrameTime();
    metric.entityCount = countEntities();
    metric.drawCalls = countDrawCalls();

    g_metricQueue.push(metric);  // Lock-free!
}

// Main thread читает метрики
void pushMetricsToTracy() {
    FrameMetric metric;
    while (g_metricQueue.pop(metric)) {
        TracyPlot("FPS", static_cast<int64_t>(1000.0f / metric.frameTime));
    }
}
```

## Frame allocator для Tracy данных

```cpp
// Frame allocator — память выделяемая каждый кадр, сбрасывается в конце
// Никаких долгосрочных аллокаций

class FrameProfilerData {
    struct Metric {
        const char* name;
        float value;
        uint32_t color;
    };

    static constexpr std::size_t k_MaxMetrics = 64;

    std::array<Metric, k_MaxMetrics> m_metrics;
    std::size_t m_count = 0;

    // Временный буфер для имён — frame allocator style
    static constexpr std::size_t k_MaxNameLen = 256;
    alignas(8) char m_nameBuffer[k_MaxNameLen];
    std::size_t m_nameOffset = 0;

public:
    // bump allocator для имён
    const char* copyName(const char* name) noexcept {
        std::size_t len = std::strlen(name) + 1;

        if (m_nameOffset + len > k_MaxNameLen) {
            return name;  // Fallback
        }

        const char* result = &m_nameBuffer[m_nameOffset];
        std::memcpy(result, name, len);
        m_nameOffset += len;

        return result;
    }

    void addMetric(const char* name, float value, uint32_t color) noexcept {
        if (m_count < k_MaxMetrics) {
            m_metrics[m_count++] = {copyName(name), value, color};
        }
    }

    void pushToTracy() noexcept {
        for (std::size_t i = 0; i < m_count; i++) {
            TracyPlot(m_metrics[i].name,
                     static_cast<int64_t>(m_metrics[i].value));
        }
        m_count = 0;
        m_nameOffset = 0;  // Reset для следующего кадра
    }
};

// RAII frame scoped
class FrameProfilerScope {
    FrameProfilerData& m_data;
    std::chrono::steady_clock::time_point m_start;

public:
    FrameProfilerScope(FrameProfilerData& data, const char* name, float value) noexcept
        : m_data(data), m_start(std::chrono::steady_clock::now())
    {
        // Push start
    }

    ~FrameProfilerScope() noexcept {
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration<float, std::milli>(end - m_start).count();

        m_data.addMetric("Scope", ms, 0);
        // Не делаем TracyPlot здесь — push в конце кадра!
    }
};
```

## Cache-line aligned Tracy данные

### False sharing avoidance

```cpp
// ❌ Плохо: false sharing между потоками
struct SharedStats {
    float frameTime;
    size_t entityCount;
};

std::vector<SharedStats> statsPerThread(8);  // 8 потоков
// Поток 0 пишет в [0], поток 1 в [1]...
// Но они на одной кэш-линии → constant cache invalidation!

// ✅ Хорошо: каждый поток на своей кэш-линии
struct alignas(64) PerThreadStats {
    float frameTime = 0.0f;
    size_t entityCount = 0;
    float reserved[7];  // Заполнение до 64 байт
};

alignas(64) std::array<PerThreadStats, 8> g_threadStats;
// Теперь каждый поток работает со своей кэш-линией

// Tracy-интеграция: агрегация в конце кадра
void pushThreadStatsToTracy() noexcept {
    float totalFrameTime = 0.0f;
    size_t totalEntities = 0;

    for (const auto& stats : g_threadStats) {
        totalFrameTime += stats.frameTime;
        totalEntities += stats.entityCount;
    }

    TracyPlot("ThreadSum_FrameTime", totalFrameTime);
    TracyPlot("ThreadSum_Entities", static_cast<int64_t>(totalEntities));
}
```

## Tracy + Vulkan 1.4 Bindless

### Профилирование bindless ресурсов

```cpp
// Bindless — индексы текстур/буферов вместо дескрипторов
// Tracy может отслеживать использование

class BindlessProfiler {
    static constexpr std::size_t k_MaxResources = 16384;

    // Hot: счётчики использования
    alignas(64) std::array<uint32_t, k_MaxResources> m_bindCounts;
    alignas(64) std::array<uint32_t, k_MaxResources> m_unbindCounts;

    // Cold: метаданные (загружаются по требованию)
    std::vector<std::string> m_resourceNames;
    std::vector<size_t> m_resourceSizes;

public:
    void onBind(VkImageView view, std::size_t index) noexcept {
        // Атомарный инкремент — но только если Tracy подключен!
        if (TracyIsConnected) {
            m_bindCounts[index].fetch_add(1, std::memory_order_relaxed);
        }
    }

    void onUnbind(std::size_t index) noexcept {
        if (TracyIsConnected) {
            m_unbindCounts[index].fetch_add(1, std::memory_order_relaxed);
        }
    }

    // Вызывается раз в секунду, не каждый кадр
    void reportBindlessStats() noexcept {
        // Агрегируем топ-N используемых ресурсов
        // TracyMessage для top используемых текстур
    }
};
```

## Интеграция с Tracy Profiler API

### Кастомный source locations

```cpp
// Продвинутое: кастомные source locations для сложных систем

void registerCustomSourceLocations() {
    // Для воксельного движка — специфичные локации

    // Chunk generation stages
    TracySourceLocationRegister(
        "NoiseGen",
        __FILE__, __LINE__, 0,
        TracyColor_Green
    );

    TracySourceLocationRegister(
        "MeshBuild",
        __FILE__, __LINE__, 0,
        TracyColor_Yellow
    );

    TracySourceLocationRegister(
        "GPUCulling",
        __FILE__, __LINE__, 0,
        TracyColor_Purple
    );
}

// Использование
void chunkGeneration() {
    TracyNamedZone(
        TracySourceLocationGet("NoiseGen"),
        "NoiseGen", true
    );
    // Работа...
}
```

## Метрики воксельного движка

### Специфичные графики для Voxel Engine

```cpp
class VoxelEngineMetrics {
public:
    // Per-frame метрики
    static void pushFrameMetrics(
        size_t loadedChunks,
        size_t visibleChunks,
        size_t culledChunks,
        size_t drawCalls,
        float frameTimeMs,
        float gpuTimeMs
    ) noexcept {
        TracyPlot("Voxel_LoadedChunks", static_cast<int64_t>(loadedChunks));
        TracyPlot("Voxel_VisibleChunks", static_cast<int64_t>(visibleChunks));
        TracyPlot("Voxel_CulledChunks", static_cast<int64_t>(culledChunks));
        TracyPlot("Voxel_DrawCalls", static_cast<int64_t>(drawCalls));
        TracyPlot("Voxel_FrameTime", frameTimeMs);
        TracyPlot("Voxel_GPUTime", gpuTimeMs);
    }

    // Memory метрики
    static void pushMemoryMetrics(
        size_t cpuMemoryBytes,
        size_t gpuMemoryBytes,
        size_t chunkMemoryBytes,
        size_t textureMemoryBytes
    ) noexcept {
        TracyPlot("Memory_CPU_MB",
                 static_cast<int64_t>(cpuMemoryBytes / (1024 * 1024)));
        TracyPlot("Memory_GPU_MB",
                 static_cast<int64_t>(gpuMemoryBytes / (1024 * 1024)));
        TracyPlot("Memory_Chunks_MB",
                 static_cast<int64_t>(chunkMemoryBytes / (1024 * 1024)));
        TracyPlot("Memory_Textures_MB",
                 static_cast<int64_t>(textureMemoryBytes / (1024 * 1024)));
    }

    // Compute метрики
    static void pushComputeMetrics(
        size_t dispatchCount,
        size_t activeThreads,
        float computeTimeMs
    ) noexcept {
        TracyPlot("Compute_DispatchCount", static_cast<int64_t>(dispatchCount));
        TracyPlot("Compute_ActiveThreads", static_cast<int64_t>(activeThreads));
        TracyPlot("Compute_TimeMs", computeTimeMs);
    }

    // Streaming метрики
    static void pushStreamingMetrics(
        size_t queuedLoads,
        size_t queuedUnloads,
        size_t activeWorkers,
        float streamingTimeMs
    ) noexcept {
        TracyPlot("Streaming_QueuedLoads", static_cast<int64_t>(queuedLoads));
        TracyPlot("Streaming_QueuedUnloads", static_cast<int64_t>(queuedUnloads));
        TracyPlot("Streaming_ActiveWorkers", static_cast<int64_t>(activeWorkers));
        TracyPlot("Streaming_TimeMs", streamingTimeMs);
    }
};
```

## Сводка рекомендаций

| Теника                 | Overhead     | Когда использовать            |
|------------------------|--------------|-------------------------------|
| `TRACY_ON_DEMAND`      | Минимальный  | Production билды              |
| Threshold profiling    | Низкий       | Debugging slow paths          |
| SPSC queue             | Очень низкий | Background → Main thread      |
| SoA для метрик         | Низкий       | Много данных                  |
| Cache-line alignment   | Низкий       | Multi-threaded профилирование |
| Frame allocator        | Низкий       | Temporary Tracy данные        |
| TracyIsConnected check | ~1 ns        | Условное профилирование       |

### Главные правила

1. **Никогда не создавай `std::thread` в кадре** — job system переиспользует потоки
2. **Агрегируй данные, прежде чем отправлять в Tracy** — меньше overhead
3. **Используй `alignas(64)`** — избегай false sharing
4. **SoA > AoS** — для любых данных, которые читаются отдельно
5. **On-demand по умолчанию** — Tracy включается только для отладки

> **Для понимания:** Профилирование хардкорного воксельного движка — как космическая программа NASA. Инженеры не ставят
> датчики на каждый винт (overhead!), но ключевые системы (двигатели, связь, навигация) отслеживаются постоянно. Данные
> агрегируются, анализируются, и только аномалии вызывают детальное расследование. Твой движок работает на 60 FPS —
> датчики должны быть умнее операций.

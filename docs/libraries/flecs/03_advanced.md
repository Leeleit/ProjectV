# Flecs в ProjectV: Злые хаки и хардкор оптимизации

> **Для понимания:** Это не просто "оптимизации" — это война за каждый наносекунд. В воксельном движке, где
> обрабатываются миллионы блоков, стандартные подходы не работают. Здесь мы используем трюки уровня "прямой доступ к
> кэшу
> процессора" и "обход ограничений драйвера".

## 🚀 Data-Oriented Design на максимум

### SoA хранение компонентов для вокселей

```cpp
#include <flecs.h>
#include <print>
#include <span>
#include <mdspan>

// ТРАДИЦИОННЫЙ ПОДХОД (AoS) — МЕДЛЕННО:
struct ChunkAoS {
    alignas(16) glm::vec3 position;
    alignas(4)  uint32_t blockData[16*16*16];
    alignas(4)  uint8_t lightLevels[16*16*16];
    alignas(4)  bool dirtyFlags[16*16*16];
};

// ПРАВИЛЬНЫЙ ПОДХОД (SoA) — БЫСТРО:
struct ChunkSoA {
    // Отдельные массивы для каждого типа данных
    alignas(64) std::array<glm::vec3, MAX_CHUNKS> positions;      // 64-байтное выравнивание для кэш-линий
    alignas(64) std::array<std::array<uint32_t, 4096>, MAX_CHUNKS> blockData;
    alignas(64) std::array<std::array<uint8_t, 4096>, MAX_CHUNKS> lightLevels;
    alignas(64) std::array<std::array<bool, 4096>, MAX_CHUNKS> dirtyFlags;

    // Использование mdspan для многомерного доступа
    auto getBlockData(size_t chunkIndex) {
        return std::mdspan(blockData[chunkIndex].data(),
                          std::extents<size_t, 16, 16, 16>());
    }
};

// Компонент Flecs с SoA хранением
struct alignas(64) VoxelChunkStorage {
    ChunkSoA data;
    size_t chunkCount{0};

    // Batch операции над всеми чанками
    void updateAllChunks(std::function<void(size_t, glm::vec3&)> processor) {
        for (size_t i = 0; i < chunkCount; ++i) {
            processor(i, data.positions[i]);
        }
    }
};
```

### Выравнивание для избежания false sharing

```cpp
#include <flecs.h>
#include <print>

// ❌ ПЛОХО: Разные потоки пишут в одну cache line
struct BadAlignment {
    uint64_t counter1;  // Поток 1
    uint64_t counter2;  // Поток 2 — в той же cache line!
    uint64_t counter3;  // Поток 3
    uint64_t counter4;  // Поток 4
};

// ✅ ХОРОШО: Каждый counter в отдельной cache line
struct alignas(64) GoodAlignment {
    alignas(64) std::atomic<uint64_t> counter1;  // 64 байта = размер cache line
    alignas(64) std::atomic<uint64_t> counter2;
    alignas(64) std::atomic<uint64_t> counter3;
    alignas(64) std::atomic<uint64_t> counter4;

    // Каждый поток работает со своей cache line
    // Нет false sharing, нет инвалидации кэша
};

// Компонент для многопоточных систем
struct alignas(64) ThreadLocalStats {
    alignas(64) uint64_t trianglesProcessed{0};
    alignas(64) uint64_t voxelsGenerated{0};
    alignas(64) uint64_t memoryAllocated{0};
    alignas(64) uint64_t cacheMisses{0};

    // Сбор статистики со всех потоков
    static ThreadLocalStats collect(const std::vector<ThreadLocalStats>& all) {
        ThreadLocalStats total;
        for (const auto& stat : all) {
            total.trianglesProcessed += stat.trianglesProcessed;
            total.voxelsGenerated += stat.voxelsGenerated;
            total.memoryAllocated += stat.memoryAllocated;
            total.cacheMisses += stat.cacheMisses;
        }
        return total;
    }
};
```

## ⚡ Многопоточность: M:N Job System вместо std::thread

### Почему std::thread — антипаттерн для ProjectV

```cpp
// ❌ АНТИПАТТЕРН: Создание потока для каждой задачи
void badMultithreading() {
    std::vector<std::thread> threads;

    for (int i = 0; i < 1000; ++i) {
        threads.emplace_back([]() {
            // Задача
        });
    }

    for (auto& t : threads) {
        t.join();
    }
    // Проблемы:
    // 1. Overhead на создание/уничтожение потоков
    // 2. Нет контроля над количеством потоков
    // 3. Cache thrashing при миграции между ядрами
    // 4. Системные вызовы на каждую задачу
}

// ✅ ПРАВИЛЬНЫЙ ПОДХОД: M:N Job System
class JobSystem {
private:
    std::vector<std::thread> workers;          // N hardware threads
    moodycamel::ConcurrentQueue<Job> jobQueue; // Lock-free очередь
    std::atomic<bool> running{true};

public:
    JobSystem(size_t threadCount = std::thread::hardware_concurrency()) {
        workers.reserve(threadCount);
        for (size_t i = 0; i < threadCount; ++i) {
            workers.emplace_back([this, i]() { workerThread(i); });
        }
    }

    ~JobSystem() {
        running = false;
        for (auto& t : workers) {
            if (t.joinable()) t.join();
        }
    }

    template<typename F>
    void schedule(F&& job) {
        jobQueue.enqueue(std::forward<F>(job));
    }

    template<typename F>
    void parallelFor(size_t count, F&& func) {
        const size_t chunkSize = std::max<size_t>(1, count / workers.size());
        std::atomic<size_t> completed{0};

        for (size_t start = 0; start < count; start += chunkSize) {
            size_t end = std::min(start + chunkSize, count);
            schedule([start, end, &func, &completed]() {
                for (size_t i = start; i < end; ++i) {
                    func(i);
                }
                completed.fetch_add(1, std::memory_order_release);
            });
        }

        // Ожидание завершения всех задач
        while (completed.load(std::memory_order_acquire) <
               ((count + chunkSize - 1) / chunkSize)) {
            std::this_thread::yield();
        }
    }

private:
    void workerThread(size_t threadId) {
        // Привязка потока к конкретному CPU ядру
        setThreadAffinity(threadId);

        Job job;
        while (running) {
            if (jobQueue.try_dequeue(job)) {
                job();
            } else {
                std::this_thread::yield();
            }
        }
    }
};
```

### Интеграция Job System с Flecs

```cpp
#include <flecs.h>

// Компонент для хранения Job System
struct JobSystemComponent {
    std::unique_ptr<JobSystem> jobSystem;

    JobSystemComponent()
        : jobSystem(std::make_unique<JobSystem>()) {}
};

// Система, использующая Job System для параллельной обработки
ecs.system<VoxelData>("ProcessVoxelsParallel")
    .multi_threaded()  // Flecs распределяет сущности по потокам
    .iter([](flecs::iter& it, VoxelData* data) {
        auto* jobSystem = it.world().ctx<JobSystemComponent>()->jobSystem.get();

        // Разделяем работу на chunks для Job System
        jobSystem->parallelFor(it.count(), [&](size_t i) {
            processVoxelChunk(data[i]);
        });

        // Flecs ждёт завершения всех задач перед переходом к следующей системе
    });

// Специальная система для тяжёлых вычислений
ecs.system<HeavyComputationData>("HeavyCompute")
    .iter([](flecs::iter& it, HeavyComputationData* data) {
        // Эта система НЕ использует multi_threaded()
        // Вместо этого она использует Job System для fine-grained параллелизма

        auto* jobSystem = it.world().ctx<JobSystemComponent>()->jobSystem.get();
        std::vector<std::future<void>> futures;

        for (auto i : it) {
            futures.push_back(jobSystem->scheduleAsync([&data, i]() {
                // Тяжёлые вычисления в отдельной задаче
                performHeavyComputation(data[i]);
            }));
        }

        // Ожидание завершения всех асинхронных задач
        for (auto& future : futures) {
            future.wait();
        }
    });
```

## 🎯 Unsafe доступ и zero-copy оптимизации

### Unsafe доступ для hot paths

```cpp
#include <flecs.h>

// ❌ БЕЗОПАСНЫЙ ДОСТУП (медленно)
ecs.system<Position, Velocity>("MoveSafe")
    .each([](flecs::entity e, Position& pos, Velocity& vel) {
        // Каждый вызов .each() — это виртуальный вызов + проверки
        pos.x += vel.dx;
        pos.y += vel.dy;
    });

// ✅ UNSAFE ДОСТУП (быстро, но опасно)
ecs.system<Position, Velocity>("MoveUnsafe")
    .iter([](flecs::iter& it, Position* pos, Velocity* vel) {
        // Прямой доступ к массивам, без проверок
        // ТОЛЬКО если уверены, что данные валидны!

        const size_t count = it.count();
        Position* posArray = pos;
        const Velocity* velArray = vel;

        // Векторизация через SIMD (компилятор может оптимизировать)
        for (size_t i = 0; i < count; ++i) {
            posArray[i].x += velArray[i].dx;
            posArray[i].y += velArray[i].dy;
        }

        // Ещё быстрее: ручная векторизация
        #ifdef __AVX2__
        processWithAVX2(posArray, velArray, count);
        #endif
    });

// Компонент с ручным управлением памятью
struct alignas(64) HighPerformanceComponent {
    // Raw указатели для максимальной производительности
    float* positionsX{nullptr};
    float* positionsY{nullptr};
    float* positionsZ{nullptr};
    size_t capacity{0};
    size_t count{0};

    HighPerformanceComponent(size_t initialCapacity = 1024) {
        capacity = initialCapacity;

        // Выделяем выровненную память
        positionsX = static_cast<float*>(aligned_alloc(64, capacity * sizeof(float)));
        positionsY = static_cast<float*>(aligned_alloc(64, capacity * sizeof(float)));
        positionsZ = static_cast<float*>(aligned_alloc(64, capacity * sizeof(float)));

        // Инициализация
        std::fill_n(positionsX, capacity, 0.0f);
        std::fill_n(positionsY, capacity, 0.0f);
        std::fill_n(positionsZ, capacity, 0.0f);
    }

    ~HighPerformanceComponent() {
        if (positionsX) aligned_free(positionsX);
        if (positionsY) aligned_free(positionsY);
        if (positionsZ) aligned_free(positionsZ);
    }

    // Batch операции с SIMD
    void translateAll(float dx, float dy, float dz) {
        const size_t simdWidth = 8; // AVX2: 8 floats
        const size_t alignedCount = count - (count % simdWidth);

        // Векторизованная часть
        for (size_t i = 0; i < alignedCount; i += simdWidth) {
            // SIMD операции
            _mm256_store_ps(&positionsX[i],
                _mm256_add_ps(_mm256_load_ps(&positionsX[i]),
                             _mm256_set1_ps(dx)));
            // ... аналогично для Y и Z
        }

        // Скалярная часть для остатка
        for (size_t i = alignedCount; i < count; ++i) {
            positionsX[i] += dx;
            positionsY[i] += dy;
            positionsZ[i] += dz;
        }
    }
};
```

### Zero-copy передача данных в GPU

```cpp
#include <flecs.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

// Структура для zero-copy буферов
struct ZeroCopyBuffer {
    VkBuffer buffer{VK_NULL_HANDLE};
    VmaAllocation allocation{nullptr};
    void* mappedData{nullptr};  // CPU-видимый указатель
    VkDeviceSize size{0};

    // Создание persistently mapped buffer
    static std::expected<ZeroCopyBuffer, std::string> create(
        VmaAllocator allocator,
        VkDeviceSize size,
        VkBufferUsageFlags usage) {

        ZeroCopyBuffer result;
        result.size = size;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        VmaAllocationInfo allocationInfo{};

        VkResult vkResult = vmaCreateBuffer(
            allocator,
            &bufferInfo,
            &allocInfo,
            &result.buffer,
            &result.allocation,
            &allocationInfo
        );

        if (vkResult != VK_SUCCESS) {
            return std::unexpected("Failed to create zero-copy buffer");
        }

        result.mappedData = allocationInfo.pMappedData;
        return result;
    }

    // Прямая запись данных (без копирования)
    template<typename T>
    void writeDirect(size_t offset, const T& data) {
        if (!mappedData || offset + sizeof(T) > size) return;

        // Просто записываем в память — VMA обеспечивает когерентность
        *reinterpret_cast<T*>(static_cast<char*>(mappedData) + offset) = data;

        // Флаш не нужен — VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
        // гарантирует когерентность
    }

    ~ZeroCopyBuffer() {
        if (buffer != VK_NULL_HANDLE) {
            // VMA автоматически анмапит память при уничтожении
        }
    }
};

// Компонент для zero-copy рендеринга вокселей
struct alignas(64) VoxelRenderData {
    ZeroCopyBuffer vertexBuffer;
    ZeroCopyBuffer indexBuffer;
    ZeroCopyBuffer instanceBuffer;  // Для instanced rendering

    // Прямое обновление данных без копирования
    void updateVerticesDirect(const std::vector<Vertex>& vertices) {
        if (vertices.empty()) return;

        const size_t dataSize = vertices.size() * sizeof(Vertex);
        if (dataSize > vertexBuffer.size) {
            // Реаллокация с новым размером
            vertexBuffer = ZeroCopyBuffer::create(/*...*/).value();
        }

        // Zero-copy запись
        std::memcpy(vertexBuffer.mappedData, vertices.data(), dataSize);
    }

    // Batch обновление инстансов
    template<typename InstanceData>
    void updateInstancesDirect(const std::span<InstanceData> instances) {
        if (instances.empty()) return;

        const size_t dataSize = instances.size() * sizeof(InstanceData);
        if (dataSize > instanceBuffer.size) {
            instanceBuffer = ZeroCopyBuffer::create(/*...*/).value();
        }

        // Прямая запись
        std::memcpy(instanceBuffer.mappedData, instances.data(), dataSize);
    }
};
```

## 🔥 GPU-Driven Rendering через Flecs

### Compute shaders и indirect drawing

```cpp
#include <flecs.h>
#include <vulkan/vulkan.h>

// Компонент для GPU-driven рендеринга
struct GPUDrivenRendering {
    VkBuffer indirectCommands{VK_NULL_HANDLE};
    VmaAllocation indirectCommandsAlloc{nullptr};

    VkBuffer instanceData{VK_NULL_HANDLE

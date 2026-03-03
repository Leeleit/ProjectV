# Интеграция Flecs в Vulkan-ориентированный движок

> **Для понимание:** Flecs — это центральная нервная система высокопроизводительного движка. Она связывает все
> компоненты: SDL3
> обрабатывает ввод, Vulkan рендерит графику, JoltPhysics вычисляет столкновения, а Flecs координирует их работу через
> ECS-архитектуру.

## 🏗️ Архитектура интеграции

### Общая схема

```
Vulkan-ориентированная архитектура:
┌─────────────────────────────────────────────────────────┐
│                    SDL3 Callback API                     │
│  (SDL_AppInit → SDL_AppIterate → SDL_AppEvent → SDL_AppQuit) │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│                  Flecs World (ECS Core)                  │
│  ┌─────────────────────────────────────────────────┐  │
│  │  Systems Pipeline:                              │  │
│  │  • OnLoad: Загрузка ресурсов                   │  │
│  │  • PreUpdate: Ввод + Физика                    │  │
│  │  • OnUpdate: Игровая логика                    │  │
│  │  • PostUpdate: Синхронизация                   │  │
│  │  • OnStore: Рендеринг (Vulkan)                 │  │
│  └─────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
         │              │              │              │
         ▼              ▼              ▼              ▼
┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐
│   Vulkan     │ │   VMA        │ │   JoltPhysics│ │   Job System │
│   Rendering  │ │   Memory     │ │   Collision  │ │   Parallel   │
│              │ │   Management │ │   Detection  │ │   Processing │
└──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘
```

## 📦 Инициализация Flecs в Vulkan-ориентированном движке

### Базовая инициализация

```cpp
#include <flecs.h>
#include <print>
#include <expected>

// Контекст приложения
struct AppContext {
    flecs::world ecs;
    // Другие компоненты: SDL_Window*, VkDevice, VmaAllocator и т.д.
};

// Инициализация ECS с настройками для Vulkan-ориентированного движка
std::expected<AppContext, std::string> initECS() {
    AppContext ctx;

    // Инициализация ECS без исключений, используя std::expected
    // Количество потоков берется из конфигурации движка
    if (auto thread_count = config::get_thread_count(); thread_count > 0) {
        ctx.ecs.set_threads(thread_count);
    } else {
        return std::unexpected("Invalid thread count from config");
    }

    ctx.ecs.set_target_fps(144.0f);  // 144 FPS для плавного рендеринга

    // Включение отладки в development builds
    #ifdef DEBUG
    ctx.ecs.set<flecs::Rest>({});  // Для Flecs Explorer
    #endif

    std::println("[ECS] World initialized with {} threads",
                 ctx.ecs.get_threads());

    return ctx;
}
```

### Интеграция с SDL3 Callback API

```cpp
#include <SDL3/SDL.h>
#include <flecs.h>
#include <print>

struct AppState {
    flecs::world ecs;
    SDL_Window* window;
    bool running;
};

// SDL3 callback: инициализация
SDL_AppResult SDL_AppInit(void** appstate) {
    auto* state = new AppState();
    *appstate = state;

    // Инициализация SDL3
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::println(stderr, "SDL initialization failed");
        delete state;
        return SDL_APP_FAILURE;
    }

    // Создание окна с Vulkan support
    state->window = SDL_CreateWindow("Vulkan Engine", 1920, 1080, SDL_WINDOW_VULKAN);
    if (!state->window) {
        std::println(stderr, "Window creation failed: {}", SDL_GetError());
        delete state;
        return SDL_APP_FAILURE;
    }

    // Инициализация ECS
    // Количество потоков берется из конфигурации движка
    state->ecs.set_threads(config::get_thread_count());
    state->running = true;

    std::println("[Vulkan Engine] Initialized with SDL3 + Flecs");
    return SDL_APP_CONTINUE;
}

// SDL3 callback: основной цикл
SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* state = static_cast<AppState*>(appstate);

    if (!state->running) {
        return SDL_APP_SUCCESS;
    }

    // Выполнение одного кадра ECS pipeline
    // delta_time вычисляется автоматически Flecs
    if (!state->ecs.progress()) {
        state->running = false;
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

// SDL3 callback: обработка событий
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* state = static_cast<AppState*>(appstate);

    switch (event->type) {
        case SDL_EVENT_QUIT:
            state->ecs.quit();  // Сигнал Flecs на завершение
            return SDL_APP_SUCCESS;

        case SDL_EVENT_KEY_DOWN:
            // Передача ввода в ECS системы
            handleInputEvent(state->ecs, event->key);
            break;

        case SDL_EVENT_MOUSE_MOTION:
            handleMouseEvent(state->ecs, event->motion);
            break;
    }

    return SDL_APP_CONTINUE;
}

// SDL3 callback: завершение
void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    auto* state = static_cast<AppState*>(appstate);

    std::println("[Vulkan Engine] Shutting down...");

    // 1. Остановить все системы
    state->ecs.quit();

    // 2. Уничтожить окно
    if (state->window) {
        SDL_DestroyWindow(state->window);
    }

    // 3. Очистить ECS (вызовет деструкторы всех компонентов)
    delete state;

    // 4. Завершить SDL
    SDL_Quit();

    std::println("[Vulkan Engine] Shutdown complete");
}
```

## 🔧 Pipeline для воксельного движка

### Кастомные фазы для воксельного движка

```cpp
#include <flecs.h>

// Определение фаз для воксельного движка
struct VoxelEnginePhases {
    // Стандартные фазы Flecs
    static constexpr flecs::entity_t OnLoad = flecs::OnLoad;
    static constexpr flecs::entity_t PreUpdate = flecs::PreUpdate;
    static constexpr flecs::entity_t OnUpdate = flecs::OnUpdate;
    static constexpr flecs::entity_t PostUpdate = flecs::PostUpdate;
    static constexpr flecs::entity_t OnStore = flecs::OnStore;

    // Кастомные фазы для воксельного движка
    flecs::entity_t VoxelGeneration;
    flecs::entity_t MeshBuilding;
    flecs::entity_t Physics;
    flecs::entity_t Rendering;

    VoxelEnginePhases(flecs::world& ecs) {
        // Фаза генерации вокселей (после загрузки)
        VoxelGeneration = ecs.entity("Phase::VoxelGeneration")
            .add(flecs::Phase)
            .add(flecs::DependsOn, OnLoad);

        // Фаза построения мешей (после генерации)
        MeshBuilding = ecs.entity("Phase::MeshBuilding")
            .add(flecs::Phase)
            .add(flecs::DependsOn, VoxelGeneration);

        // Фаза физики (перед обновлением)
        Physics = ecs.entity("Phase::Physics")
            .add(flecs::Phase)
            .add(flecs::DependsOn, PreUpdate);

        // Фаза рендеринга (после всех обновлений)
        Rendering = ecs.entity("Phase::Rendering")
            .add(flecs::Phase)
            .add(flecs::DependsOn, OnStore);
    }
};
```

### Настройка системного pipeline

```cpp
void setupVoxelEnginePipeline(flecs::world& ecs) {
    VoxelEnginePhases phases(ecs);

    // 1. Загрузка ресурсов (OnLoad)
    ecs.system<>("LoadResources")
        .kind(phases.OnLoad)
        .iter([](flecs::iter& it) {
            std::println("[Pipeline] Loading resources...");
            // Загрузка текстур, шейдеров, моделей
        });

    // 2. Генерация воксельных чанков
    ecs.system<ChunkPosition, ChunkData>("GenerateVoxels")
        .kind(phases.VoxelGeneration)
        .with<NeedsGeneration>()
        .multi_threaded()
        .iter([](flecs::iter& it, ChunkPosition* pos, ChunkData* data) {
            for (auto i : it) {
                generateVoxelChunk(pos[i], data[i]);
                it.entity(i).remove<NeedsGeneration>();
            }
        });

    // 3. Построение мешей
    ecs.system<ChunkData, MeshComponent>("BuildMeshes")
        .kind(phases.MeshBuilding)
        .with<Dirty>()
        .iter([](flecs::iter& it, ChunkData* data, MeshComponent* mesh) {
            auto* gpu = it.world().ctx<GPUContext>();
            for (auto i : it) {
                buildMeshFromVoxels(gpu, data[i], mesh[i]);
                it.entity(i).remove<Dirty>();
            }
        });

    // 4. Физика (PreUpdate)
    ecs.system<PhysicsBody, Position>("PhysicsStep")
        .kind(phases.Physics)
        .iter([](flecs::iter& it, PhysicsBody* body, Position* pos) {
            auto* physics = it.world().ctx<PhysicsSystem>();
            physics->step(it.delta_time());

            // Синхронизация позиций
            for (auto i : it) {
                pos[i] = body[i].getWorldPosition();
            }
        });

    // 5. Основная логика (OnUpdate)
    ecs.system<PlayerInput, Velocity>("ProcessInput")
        .kind(phases.OnUpdate)
        .each([](PlayerInput& input, Velocity& vel) {
            vel.dx = input.move_x * 5.0f;
            vel.dz = input.move_z * 5.0f;
        });

    // 6. Рендеринг (OnStore) - однопоточный для Vulkan
    ecs.system<MeshComponent, Position>("Render")
        .kind(phases.Rendering)
        .with<Visible>()
        .iter([](flecs::iter& it, MeshComponent* mesh, Position* pos) {
            auto* renderer = it.world().ctx<VulkanRenderer>();
            renderer->beginFrame();

            for (auto i : it) {
                renderer->drawMesh(mesh[i], pos[i]);
            }

            renderer->endFrame();
        });
}
```

## 🎮 Управление Vulkan ресурсами через Flecs

### Компоненты с GPU ресурсами

```cpp
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <flecs.h>
#include <print>

// Контекст GPU для доступа к Vulkan
struct GPUContext {
    VkDevice device;
    VmaAllocator allocator;
    VkQueue graphicsQueue;
    uint32_t graphicsQueueFamily;
};

// Компонент с Vulkan буфером
struct alignas(16) GPUBuffer {
    VkBuffer buffer{VK_NULL_HANDLE};
    VmaAllocation allocation{nullptr};
    VkDeviceSize size{0};
    VkBufferUsageFlags usage{0};

    // RAII деструктор
    ~GPUBuffer() {
        if (buffer != VK_NULL_HANDLE) {
            auto* ctx = flecs::world().ctx<GPUContext>();
            if (ctx && ctx->allocator) {
                vmaDestroyBuffer(ctx->allocator, buffer, allocation);
            }
        }
    }
};

// Компонент с мешем для рендеринга
struct alignas(16) MeshComponent {
    GPUBuffer vertexBuffer;
    GPUBuffer indexBuffer;
    uint32_t vertexCount{0};
    uint32_t indexCount{0};
    VkPrimitiveTopology topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

    // Флаги состояния
    bool needsUpload{true};
    bool isVisible{true};
};
```

### Observers для управления временем жизни ресурсов

```cpp
void setupResourceObservers(flecs::world& ecs) {
    // Observer для создания GPU ресурсов при добавлении MeshComponent
    ecs.observer<MeshComponent>("CreateMeshResources")
        .event(flecs::OnAdd)
        .each([](flecs::entity e, MeshComponent& mesh) {
            auto* ctx = e.world().ctx<GPUContext>();
            if (!ctx) {
                std::println(stderr, "GPUContext not available for mesh creation");
                return;
            }

            // Создание vertex buffer
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = mesh.vertexCount * sizeof(Vertex);
            bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

            VkResult result = vmaCreateBuffer(
                ctx->allocator,
                &bufferInfo,
                &allocInfo,
                &mesh.vertexBuffer.buffer,
                &mesh.vertexBuffer.allocation,
                nullptr
            );

            if (result != VK_SUCCESS) {
                std::println(stderr, "Failed to create vertex buffer: {}", result);
            }

            mesh.vertexBuffer.size = bufferInfo.size;
            mesh.vertexBuffer.usage = bufferInfo.usage;
        });

    // Observer для освобождения GPU ресурсов при удалении MeshComponent
    ecs.observer<MeshComponent>("DestroyMeshResources")
        .event(flecs::OnRemove)
        .each([](flecs::entity e, MeshComponent& mesh) {
            // Деструкторы GPUBuffer автоматически вызовут vmaDestroyBuffer
            // через RAII в деструкторе GPUBuffer
            std::println("[ECS] Mesh resources released for entity {}", e.id());
        });

    // Observer для загрузки данных в GPU при изменении меша
    ecs.observer<MeshComponent>("UploadMeshData")
        .event(flecs::OnSet)
        .each([](flecs::entity e, MeshComponent& mesh) {
            if (!mesh.needsUpload) return;

            auto* ctx = e.world().ctx<GPUContext>();
            if (!ctx) return;

            // Загрузка данных через staging buffer
            uploadMeshToGPU(ctx, mesh);
            mesh.needsUpload = false;

            std::println("[ECS] Mesh data uploaded for entity {}", e.id());
        });
}
```

### Синхронизация с Vulkan timeline semaphores

```cpp
#include <flecs.h>

// Компонент для синхронизации GPU-CPU
struct alignas(16) GPUSync {
    VkSemaphore timelineSemaphore{VK_NULL_HANDLE};
    uint64_t currentValue{0};
    uint64_t lastCompletedValue{0};

    // Сигнал о завершении GPU работы
    void signalCompletion(uint64_t value) {
        currentValue = value;
    }

    // Проверка, завершена ли работа
    bool isCompleted(uint64_t value) const {
        return value <= lastCompletedValue;
    }
};

// Система для управления синхронизацией
void setupSynchronizationSystems(flecs::world& ecs) {
    // Система для обновления состояния синхронизации
    ecs.system<GPUSync>("UpdateSyncState")
        .kind(flecs::PostUpdate)
        .each([](GPUSync& sync) {
            auto* ctx = ecs.ctx<GPUContext>();
            if (!ctx || !sync.timelineSemaphore) return;

            // Запрос текущего значения timeline semaphore
            uint64_t completedValue;
            vkGetSemaphoreCounterValue(
                ctx->device,
                sync.timelineSemaphore,
                &completedValue
            );

            sync.lastCompletedValue = completedValue;
        });

    // Система для ожидания завершения GPU работы
    ecs.system<GPUSync>("WaitForGPU")
        .kind(flecs::PreUpdate)
        .each([](GPUSync& sync) {
            auto* ctx = flecs::world().ctx<GPUContext>();
            if (!ctx || !sync.timelineSemaphore) return;

            VkSemaphoreWaitInfo waitInfo = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
                .semaphoreCount = 1,
                .pSemaphores = &sync.timelineSemaphore,
                .pValues = &sync.currentValue
            };

            // Ждем, пока GPU закончит работу над этим кадром
            vkWaitSemaphores(ctx->device, &waitInfo, UINT64_MAX);
        });
```

## 🧵 Многопоточность в Vulkan-ориентированном движке

### Настройка многопоточности Flecs

```cpp
#include <flecs.h>
#include <print>
#include <thread>

// Конфигурация потоков для Vulkan-ориентированного движка
struct ThreadConfig {
    int32_t worker_threads;
    int32_t io_threads;
    bool enable_hyperthreading;
};

void configureFlecsThreading(flecs::world& ecs, const ThreadConfig& config) {
    // Общее количество потоков = worker + IO + main
    int32_t total_threads = config.worker_threads + config.io_threads + 1;

    if (config.enable_hyperthreading) {
        // Используем hyperthreading для лучшего использования CPU
        total_threads = std::thread::hardware_concurrency();
    }

    ecs.set_threads(total_threads);

    // Настройка приоритетов потоков
    #ifdef _WIN32
    // Windows: установка приоритетов
    ecs.observer<>("SetThreadPriority")
        .event(flecs::OnAdd)
        .each([](flecs::entity e) {
            if (e.has<flecs::System>()) {
                // Системы получают нормальный приоритет
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
            }
        });
    #endif

    std::println("[ECS] Configured with {} threads ({} workers, {} IO)",
                 total_threads, config.worker_threads, config.io_threads);
}
```

### Thread-safe системы

```cpp
#include <flecs.h>
#include <atomic>
#include <mutex>

// Потокобезопасный компонент с мьютексом
struct ThreadSafeCounter {
    std::atomic<int64_t> value{0};
    std::mutex mutex;

    void increment() {
        std::lock_guard lock(mutex);
        ++value;
    }

    int64_t get() const {
        return value.load(std::memory_order_acquire);
    }
};

// Система с thread-local данными
struct ThreadLocalData {
    thread_local static inline int32_t local_counter = 0;

    int32_t get_and_reset() {
        int32_t val = local_counter;
        local_counter = 0;
        return val;
    }
};

void setupThreadSafeSystems(flecs::world& ecs) {
    // 1. Многопоточная система с атомарными операциями
    ecs.system<ThreadSafeCounter>("AtomicCounterSystem")
        .multi_threaded()
        .each([](ThreadSafeCounter& counter) {
            counter.increment();
        });

    // 2. Система с thread-local данными
    ecs.system<>("ThreadLocalSystem")
        .multi_threaded()
        .iter([](flecs::iter& it) {
            // Каждый поток имеет свою копию local_counter
            ThreadLocalData::local_counter += it.count();
        });

    // 3. Система с синхронизацией через барьеры
    ecs.system<>("BarrierSystem")
        .multi_threaded()
        .iter([](flecs::iter& it) {
            // Барьер для синхронизации между потоками
            it.world().barrier();

            // Критическая секция
            static std::mutex critical_mutex;
            {
                std::lock_guard lock(critical_mutex);
                // Работа с общими данными
            }
        });
}
```

### Job System интеграция

```cpp
#include <flecs.h>
#include "job_system.h"

// Компонент для job-based обработки
struct VoxelChunkJob {
    JobHandle job_handle;
    bool completed{false};
    std::vector<uint8_t> voxel_data;
};

// Система, которая создает jobs
void setupJobSystemIntegration(flecs::world& ecs) {
    auto* job_system = ecs.ctx<JobSystem>();

    if (!job_system) {
        std::println(stderr, "JobSystem not found in context");
        return;
    }

    // Система для создания jobs
    ecs.system<VoxelChunkJob>("CreateVoxelJobs")
        .with<NeedsProcessing>()
        .iter([](flecs::iter& it, VoxelChunkJob* jobs) {
            auto* job_system = it.world().ctx<JobSystem>();

            for (auto i : it) {
                // Создаем job для обработки вокселей
                jobs[i].job_handle = job_system->create_job([&jobs, i]() {
                    processVoxelChunk(jobs[i].voxel_data);
                    jobs[i].completed = true;
                });

                // Запускаем job
                job_system->run(jobs[i].job_handle);

                it.entity(i).remove<NeedsProcessing>();
            }
        });

    // Система для проверки завершения jobs
    ecs.system<VoxelChunkJob>("CheckJobCompletion")
        .iter([](flecs::iter& it, VoxelChunkJob* jobs) {
            auto* job_system = it.world().ctx<JobSystem>();

            for (auto i : it) {
                if (jobs[i].completed) {
                    // Job завершен, можно использовать результаты
                    it.entity(i).add<ProcessingComplete>();

                    // Освобождаем job
                    job_system->wait(jobs[i].job_handle);
                    jobs[i].job_handle = {};
                }
            }
        });
}
```

## 💾 Управление памятью с VMA

### Интеграция VMA с Flecs компонентами

```cpp
#include <flecs.h>
#include <vk_mem_alloc.h>
#include <print>

// Контекст Vulkan Memory Allocator
struct VMAContext {
    VmaAllocator allocator;
    VkDevice device;

    // Статистика использования памяти
    struct Stats {
        size_t allocated_bytes{0};
        size_t peak_allocated_bytes{0};
        uint32_t allocation_count{0};
    } stats;
};

// Компонент с VMA-аллоцированной памятью
struct VMAAllocatedBuffer {
    VkBuffer buffer{VK_NULL_HANDLE};
    VmaAllocation allocation{nullptr};
    VmaAllocationInfo alloc_info{};
    VkDeviceSize size{0};

    // Деструктор с RAII
    ~VMAAllocatedBuffer() {
        if (buffer != VK_NULL_HANDLE) {
            auto* ctx = flecs::world().ctx<VMAContext>();
            if (ctx && ctx->allocator) {
                vmaDestroyBuffer(ctx->allocator, buffer, allocation);

                // Обновляем статистику
                ctx->stats.allocated_bytes -= alloc_info.size;
                ctx->stats.allocation_count--;
            }
        }
    }

    // Создание буфера через VMA
    bool create(VkBufferCreateInfo* buffer_info,
                VmaAllocationCreateInfo* alloc_info,
                VMAContext* ctx) {
        if (!ctx || !ctx->allocator) return false;

        VkResult result = vmaCreateBuffer(
            ctx->allocator,
            buffer_info,
            alloc_info,
            &buffer,
            &allocation,
            &this->alloc_info
        );

        if (result == VK_SUCCESS) {
            size = buffer_info->size;
            ctx->stats.allocated_bytes += size;
            ctx->stats.allocation_count++;
            ctx->stats.peak_allocated_bytes =
                std::max(ctx->stats.peak_allocated_bytes,
                        ctx->stats.allocated_bytes);
            return true;
        }

        return false;
    }
};

// Компонент с staging буфером для загрузки данных
struct StagingBuffer {
    VMAAllocatedBuffer buffer;
    void* mapped_data{nullptr};
    bool is_mapped{false};

    // Отображение памяти для записи
    bool map(VMAContext* ctx) {
        if (!ctx || !buffer.allocation || is_mapped) return false;

        VkResult result = vmaMapMemory(
            ctx->allocator,
            buffer.allocation,
            &mapped_data
        );

        is_mapped = (result == VK_SUCCESS);
        return is_mapped;
    }

    // Отмена отображения
    void unmap(VMAContext* ctx) {
        if (!ctx || !buffer.allocation || !is_mapped) return;

        vmaUnmapMemory(ctx->allocator, buffer.allocation);
        mapped_data = nullptr;
        is_mapped = false;
    }

    // Копирование данных в буфер
    template<typename T>
    bool upload(const std::vector<T>& data, VMAContext* ctx) {
        if (!map(ctx)) return false;

        size_t data_size = data.size() * sizeof(T);
        if (data_size > buffer.size) {
            unmap(ctx);
            return false;
        }

        memcpy(mapped_data, data.data(), data_size);
        unmap(ctx);
        return true;
    }
};
```

### Observers для управления памятью

```cpp
void setupVMAObservers(flecs::world& ecs) {
    // Observer для создания буферов при добавлении компонента
    ecs.observer<VMAAllocatedBuffer>("CreateVKBuffer")
        .event(flecs::OnAdd)
        .each([](flecs::entity e, VMAAllocatedBuffer& buffer) {
            auto* ctx = e.world().ctx<VMAContext>();
            if (!ctx) {
                std::println(stderr, "VMAContext not available");
                return;
            }

            // Стандартные настройки для vertex buffer
            VkBufferCreateInfo buffer_info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = buffer.size,
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };

            VmaAllocationCreateInfo alloc_info = {
                .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
            };

            if (!buffer.create(&buffer_info, &alloc_info, ctx)) {
                std::println(stderr, "Failed to create VK buffer for entity {}", e.id());
            }
        });

    // Observer для дефрагментации памяти
    ecs.observer<>("DefragmentMemory")
        .event(flecs::OnStore)  // В конце кадра
        .each([](flecs::entity e) {
            auto* ctx = e.world().ctx<VMAContext>();
            if (!ctx) return;

            // Проверяем, нужно ли дефрагментировать
            VmaStats stats;
            vmaCalculateStats(ctx->allocator, &stats);

            if (stats.total.usedBytes > stats.total.unusedBytes * 2) {
                // Много фрагментированной памяти, запускаем дефрагментацию
                VmaDefragmentationInfo defrag_info = {};
                VmaDefragmentationStats defrag_stats;
                VmaDefragmentationContext defrag_ctx;

                vmaDefragment(ctx->allocator, nullptr,
                             &defrag_info, &defrag_stats, &defrag_ctx);

                std::println("[VMA] Defragmentation saved {} bytes",
                            defrag_stats.bytesMoved);
            }
        });
}
```

## 🎮 Интеграция с JoltPhysics

### Компоненты для физики

```cpp
#include <flecs.h>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/Body.h>

// Контекст Jolt Physics
struct JoltPhysicsContext {
    JPH::PhysicsSystem* physics_system{nullptr};
    JPH::JobSystem* job_system{nullptr};
    float fixed_timestep{1.0f / 60.0f};
    float accumulated_time{0.0f};
};

// Компонент физического тела
struct PhysicsBody {
    JPH::BodyID body_id;
    JPH::BodyCreationSettings creation_settings;
    bool is_active{true};

    // Получение указателя на тело
    JPH::Body* get(JoltPhysicsContext* ctx) const {
        if (!ctx || !ctx->physics_system || !body_id.IsValid()) {
            return nullptr;
        }
        return ctx->physics_system->GetBodyInterface().GetBody(body_id);
    }

    // Активация/деактивация тела
    void set_active(JoltPhysicsContext* ctx, bool active) {
        if (auto* body = get(ctx)) {
            if (active && !is_active) {
                ctx->physics_system->GetBodyInterface().ActivateBody(body_id);
            } else if (!active && is_active) {
                ctx->physics_system->GetBodyInterface().DeactivateBody(body_id);
            }
            is_active = active;
        }
    }
};

// Компонент для синхронизации позиции
struct PhysicsSync {
    bool sync_to_physics{true};   // Позиция → физика
    bool sync_from_physics{true}; // Физика → позиция
    float interpolation_factor{0.1f};
};
```

### Системы физики

```cpp
void setupPhysicsSystems(flecs::world& ecs) {
    // Система создания физических тел
    ecs.observer<PhysicsBody>("CreatePhysicsBody")
        .event(flecs::OnAdd)
        .each([](flecs::entity e, PhysicsBody& body) {
            auto* ctx = e.world().ctx<JoltPhysicsContext>();
            if (!ctx || !ctx->physics_system) return;

            // Создаем тело в Jolt
            auto& body_iface = ctx->physics_system->GetBodyInterface();
            body.body_id = body_iface.CreateBody(body.creation_settings);

            if (body.body_id.IsValid()) {
                body_iface.AddBody(body.body_id,
                                  body.is_active ?
                                  JPH::EActivation::Activate :
                                  JPH::EActivation::DontActivate);

                std::println("[Physics] Body created for entity {}", e.id());
            }
        });

    // Система удаления физических тел
    ecs.observer<PhysicsBody>("DestroyPhysicsBody")
        .event(flecs::OnRemove)
        .each([](flecs::entity e, PhysicsBody& body) {
            auto* ctx = e.world().ctx<JoltPhysicsContext>();
            if (!ctx || !ctx->physics_system || !body.body_id.IsValid()) return;

            auto& body_iface = ctx->physics_system->GetBodyInterface();
            body_iface.RemoveBody(body.body_id);
            body_iface.DestroyBody(body.body_id);

            std::println("[Physics] Body destroyed for entity {}", e.id());
        });

    // Система шага физики (fixed timestep)
    ecs.system<JoltPhysicsContext>("PhysicsStep")
        .kind(flecs::PreUpdate)
        .iter([](flecs::iter& it, JoltPhysicsContext* ctx) {
            if (!ctx || !ctx->physics_system) return;

            // Накопление времени
            ctx->accumulated_time += it.delta_time();

            // Fixed timestep
            while (ctx->accumulated_time >= ctx->fixed_timestep) {
                ctx->physics_system->Update(
                    ctx->fixed_timestep,
                    1,  // Коллизии
                    1,  // Интеграция
                    ctx->job_system
                );
                ctx->accumulated_time -= ctx->fixed_timestep;
            }
        });

    // Система синхронизации позиций
    ecs.system<PhysicsBody, Position, PhysicsSync>("SyncPhysicsPositions")
        .kind(flecs::PostUpdate)
        .each([](PhysicsBody& body, Position& pos, PhysicsSync& sync) {
            auto* ctx = flecs::world().ctx<JoltPhysicsContext>();
            if (!ctx || !sync.sync_from_physics) return;

            if (auto* jolt_body = body.get(ctx)) {
                // Интерполяция позиции для плавности
                JPH::Vec3 jolt_pos = jolt_body->GetPosition();
                pos.x = pos.x * (1.0f - sync.interpolation_factor) +
                       jolt_pos.GetX() * sync.interpolation_factor;
                pos.y = pos.y * (1.0f - sync.interpolation_factor) +
                       jolt_pos.GetY() * sync.interpolation_factor;
                pos.z = pos.z * (1.0f - sync.interpolation_factor) +
                       jolt_pos.GetZ() * sync.interpolation_factor;
            }
        });

    // Система применения сил
    ecs.system<PhysicsBody, const Force>("ApplyForces")
        .kind(flecs::OnUpdate)
        .each([](PhysicsBody& body, const Force& force) {
            auto* ctx = flecs::world().ctx<JoltPhysicsContext>();
            if (!ctx) return;

            if (auto* jolt_body = body.get(ctx)) {
                auto& body_iface = ctx->physics_system->GetBodyInterface();
                body_iface.AddForce(
                    body.body_id,
                    JPH::Vec3(force.x, force.y, force.z)
                );
            }
        });
}
```

## 📊 Мониторинг и отладка

### Компоненты для сбора статистики

```cpp
#include <flecs.h>
#include <print>
#include <chrono>

// Статистика производительности
struct PerformanceStats {
    struct FrameStats {
        float frame_time_ms{0};
        float system_time_ms{0};
        float physics_time_ms{0};
        float render_time_ms{0};
        int32_t entity_count{0};
        int32_t system_count{0};
    };

    std::vector<FrameStats> frame_history;
    size_t max_history{300};  // 5 секунд при 60 FPS

    void add_frame(const FrameStats& stats) {
        frame_history.push_back(stats);
        if (frame_history.size() > max_history) {
            frame_history.erase(frame_history.begin());
        }
    }

    FrameStats get_average() const {
        if (frame_history.empty()) return {};

        FrameStats avg{};
        for (const auto& frame : frame_history) {
            avg.frame_time_ms += frame.frame_time_ms;
            avg.system_time_ms += frame.system_time_ms;
            avg.physics_time_ms += frame.physics_time_ms;
            avg.render_time_ms += frame.render_time_ms;
            avg.entity_count += frame.entity_count;
            avg.system_count += frame.system_count;
        }

        float count = static_cast<float>(frame_history.size());
        avg.frame_time_ms /= count;
        avg.system_time_ms /= count;
        avg.physics_time_ms /= count;
        avg.render_time_ms /= count;
        avg.entity_count /= static_cast<int32_t>(frame_history.size());
        avg.system_count /= static_cast<int32_t>(frame_history.size());

        return avg;
    }
};

// Система сбора статистики
void setupPerformanceMonitoring(flecs::world& ecs) {
    auto stats = ecs.entity("PerformanceStats")
        .set<PerformanceStats>({});

    // Система для сбора статистики каждого кадра
    ecs.system<>("CollectStats")
        .kind(flecs::OnStore)  // В самом конце кадра
        .iter([](flecs::iter& it) {
            auto* stats = it.world().get<PerformanceStats>("PerformanceStats");
            if (!stats) return;

            PerformanceStats::FrameStats frame_stats;

            // Время кадра
            frame_stats.frame_time_ms = it.delta_time() * 1000.0f;

            // Количество сущностей
            frame_stats.entity_count = it.world().count<flecs::Name>();

            // Количество систем
            frame_stats.system_count = it.world().count<flecs::System>();

            // Время систем (нужно включить профилирование)
            #ifdef FLECS_SYSTEM_MONITOR
            frame_stats.system_time_ms = it.world().get_system_time();
            #endif

            stats->add_frame(frame_stats);
        });

    // Система для вывода статистики (раз в секунду)
    ecs.system<PerformanceStats>("PrintStats")
        .interval(1.0f)  // Раз в секунду
        .each([](PerformanceStats& stats) {
            auto avg = stats.get_average();

            std::println("\n=== Performance Stats ({} frames) ===",
                        stats.frame_history.size());
            std::println("Frame Time: {:.2f} ms ({:.1f} FPS)",
                        avg.frame_time_ms, 1000.0f / avg.frame_time_ms);
            std::println("System Time: {:.2f} ms", avg.system_time_ms);
            std::println("Physics Time: {:.2f} ms", avg.physics_time_ms);
            std::println("Render Time: {:.2f} ms", avg.render_time_ms);
            std::println("Entities: {}", avg.entity_count);
            std::println("Systems: {}", avg.system_count);
            std::println("====================================\n");
        });
}
```

### Отладка с Flecs Explorer

```cpp
#include <flecs.h>

void setupFlecsExplorer(flecs::world& ecs) {
    // Включение REST API для Flecs Explorer
    ecs.import<flecs::rest>();

    // Настройка порта
    ecs.set<flecs::Rest>({.port = 8080});

    // Добавление кастомных компонентов для отладки
    ecs.observer<>("DebugObserver")
        .event(flecs::OnAdd)
        .each([](flecs::entity e) {
    // Логирование создания сущностей в debug mode
    #ifdef DEBUG
    if (e.has<flecs::Name>()) {
        std::println("[DEBUG] Entity created: {}", e.name());
    }
    #endif
        });

    // Система для проверки инвариантов
    ecs.system<>("InvariantCheck")
        .interval(5.0f)  // Каждые 5 секунд
        .iter([](flecs::iter& it) {
            // Проверка инвариантов движка
            checkEngineInvariants(it.world());
        });
}
```

## 🎯 Лучшие практики для Vulkan-ориентированного движка

### 1. Организация компонентов

```cpp
// Хорошо: Логическая группировка
namespace transform {
    struct Position { float x, y, z; };
    struct Rotation { float x, y, z, w; };
    struct Scale { float x, y, z; };
}

namespace render {
    struct Mesh { uint32_t handle; };
    struct Material { uint32_t texture_id; };
    struct Visible {};
}

namespace physics {
    struct RigidBody { JPH::BodyID body_id; };
    struct Collider { JPH::ShapeRefC shape; };
    struct Force { float x, y, z; };
}

// Плохо: Разбросанные компоненты без namespace
struct Pos { float x, y, z; };
struct Rot { float x, y, z, w; };
struct MeshHandle { uint32_t h; };
```

### 2. Оптимизация запросов

```cpp
void optimizeQueries(flecs::world& ecs) {
    // Хорошо: Кэшированные запросы для часто используемых паттернов
    static auto render_query = ecs.query_builder<transform::Position, render::Mesh>()
        .with<render::Visible>()
        .without<render::Hidden>()
        .cache()  // Включение кэширования
        .build();

    // Хорошо: Использование singleton для глобальных данных
    ecs.singleton<GlobalSettings>()
        .set<GlobalSettings>({
            .max_fps = 144,
            .vsync = true,
            .render_distance = 1000.0f
        });

    // Плохо: Создание запроса каждый кадр
    // auto q = ecs.query<Position, Mesh>(); // В цикле - медленно!
}
```

### 3. Управление памятью

```cpp
struct MemoryManagement {
    // Использование пулов для часто создаваемых/удаляемых компонентов
    static inline flecs::component<Bullet> bullet_component;

    // Pre-allocation для известного количества сущностей
    void preallocateEntities(flecs::world& ecs, int32_t count) {
        ecs.dim(count);  // Предварительное выделение памяти
    }

    // Использование bulk operations
    void createBullets(flecs::world& ecs, const std::vector<Position>& positions) {
        ecs.bulk_new<Bullet>(positions.size(), [&](Bullet* bullets, int32_t count) {
            for (int32_t i = 0; i < count; ++i) {
                bullets[i].position = positions[i];
                bullets[i].velocity = {0, 0, -10};
            }
        });
    }
};
```

### 4. Обработка ошибок

```cpp
#include <expected>
#include <print>

std::expected<flecs::entity, std::string> createPlayer(
    flecs::world& ecs,
    const std::string& name,
    const transform::Position& pos
) {
    if (name.empty()) {
        return std::unexpected("Player name cannot be empty");
    }

    if (ecs.lookup(name.c_str())) {
        return std::unexpected("Player with this name already exists");
    }

    try {
        auto player = ecs.entity(name.c_str())
            .set<transform::Position>(pos)
            .set<PlayerComponent>({100, 100});

        return player;
    } catch (const std::exception& e) {
        return std::unexpected(std::string("Failed to create player: ") + e.what());
    }
}

// Использование
auto result = createPlayer(ecs, "Hero", {0, 0, 0});
if (result) {
    std::println("Player created: {}", result->name());
} else {
    std::println(stderr, "Error: {}", result.error());
}
```

## 🚀 Заключение

Flecs предоставляет мощную и гибкую основу для построения высокопроизводительного воксельного движка. Ключевые
преимущества:

1. **Производительность**: SoA хранение, batch processing, многопоточность
2. **Гибкость**: Пары, модули, рефлексия, кастомные pipeline
3. **Интегрируемость**: Легкая интеграция с Vulkan, JoltPhysics, SDL3
4. **Отладка**: Flecs Explorer, встроенная статистика, мониторинг
5. **Безопасность**: RAII для ресурсов, обработка ошибок через std::expected

Используя эти паттерны и лучшие практики, Vulkan-ориентированный движок может достичь высокой производительности при
сохранении чистоты кода и удобства разработки.

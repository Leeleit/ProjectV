# Интеграция Flecs в ProjectV

> **Для понимания:** Flecs в ProjectV — это центральная нервная система движка. Она связывает все компоненты: SDL3
> обрабатывает ввод, Vulkan рендерит графику, JoltPhysics вычисляет столкновения, а Flecs координирует их работу через
> ECS-архитектуру.

## 🏗️ Архитектура интеграции

### Общая схема

```
ProjectV Architecture:
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

## 📦 Инициализация Flecs в ProjectV

### Базовая инициализация

```cpp
#include <flecs.h>
#include <print>
#include <expected>

// Контекст приложения ProjectV
struct AppContext {
    flecs::world ecs;
    // Другие компоненты: SDL_Window*, VkDevice, VmaAllocator и т.д.
};

// Инициализация ECS с настройками для ProjectV
std::expected<AppContext, std::string> initECS() {
    AppContext ctx;

    // Инициализация ECS без исключений, используя std::expected
    // Количество потоков берется из конфигурации движка ProjectV
    if (auto thread_count = projectv::config::get_thread_count(); thread_count > 0) {
        ctx.ecs.set_threads(thread_count);
    } else {
        return std::unexpected("Invalid thread count from ProjectV config");
    }
    
    ctx.ecs.set_target_fps(144.0f);  // 144 FPS для плавного рендеринга

    // Включение отладки в development builds
    #ifdef PROJECTV_DEBUG
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
    state->window = SDL_CreateWindow("ProjectV", 1920, 1080, SDL_WINDOW_VULKAN);
    if (!state->window) {
        std::println(stderr, "Window creation failed: {}", SDL_GetError());
        delete state;
        return SDL_APP_FAILURE;
    }

    // Инициализация ECS
    // Количество потоков берется из конфигурации движка ProjectV
    state->ecs.set_threads(projectv::config::get_thread_count());
    state->running = true;

    std::println("[ProjectV] Initialized with SDL3 + Flecs");
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

    std::println("[ProjectV] Shutting down...");

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

    std::println("[ProjectV] Shutdown complete");
}
```

## 🔧 Pipeline для воксельного движка

### Кастомные фазы ProjectV

```cpp
#include <flecs.h>

// Определение фаз для воксельного движка
struct ProjectVPhases {
    // Стандартные фазы Flecs
    static constexpr flecs::entity_t OnLoad = flecs::OnLoad;
    static constexpr flecs::entity_t PreUpdate = flecs::PreUpdate;
    static constexpr flecs::entity_t OnUpdate = flecs::OnUpdate;
    static constexpr flecs::entity_t PostUpdate = flecs::PostUpdate;
    static constexpr flecs::entity_t OnStore = flecs::OnStore;

    // Кастомные фазы для ProjectV
    flecs::entity_t VoxelGeneration;
    flecs::entity_t MeshBuilding;
    flecs::entity_t Physics;
    flecs::entity_t Rendering;

    ProjectVPhases(flecs::world& ecs) {
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
void setupProjectVPipeline(flecs::world& ecs) {
    ProjectVPhases phases(ecs);

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
}

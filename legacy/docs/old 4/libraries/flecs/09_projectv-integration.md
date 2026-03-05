# Интеграция flecs с ProjectV

🔴 **Уровень 3: Продвинутый**

Связка flecs с Vulkan, SDL3, VMA и JoltPhysics в ProjectV.

## Обзор интеграции

ProjectV использует flecs как основу ECS-архитектуры для воксельного движка. Ключевые особенности:

- **SDL3 callback API** — главный цикл управляется SDL
- **Vulkan handles в компонентах** — требуется правильное управление ресурсами
- **VMA** — выделение GPU памяти
- **JoltPhysics** — физика через интеграцию с flecs

---

## SDL3 Callback API

SDL3 использует callback-архитектуру вместо традиционного цикла:

### Инициализация

```cpp
#include <SDL3/SDL.h>
#include <flecs.h>

struct AppState {
    flecs::world ecs;
    SDL_Window* window;
    // ... Vulkan ресурсы
};

SDL_AppResult SDL_AppInit(void** appstate) {
    auto* state = new AppState();
    *appstate = state;
    
    // Инициализация SDL
    SDL_Init(SDL_INIT_VIDEO);
    state->window = SDL_CreateWindow("ProjectV", 800, 600, SDL_WINDOW_VULKAN);
    
    // Инициализация flecs
    setup_systems(state->ecs);
    create_entities(state->ecs);
    
    return SDL_APP_CONTINUE;
}
```

### Основной цикл

```cpp
SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* state = static_cast<AppState*>(appstate);
    
    // ECS update
    if (!state->ecs.progress(1.0f / 60.0f)) {
        return SDL_APP_SUCCESS;  // ecs.quit() был вызван
    }
    
    // Рендеринг
    render_frame(state);
    
    return SDL_APP_CONTINUE;
}
```

### Обработка событий

```cpp
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* state = static_cast<AppState*>(appstate);
    
    switch (event->type) {
        case SDL_EVENT_QUIT:
            state->ecs.quit();
            return SDL_APP_SUCCESS;
            
        case SDL_EVENT_KEY_DOWN:
            // Передать в систему ввода
            handle_input(state->ecs, event->key);
            break;
    }
    
    return SDL_APP_CONTINUE;
}
```

### Завершение

```cpp
void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    auto* state = static_cast<AppState*>(appstate);
    
    // 1. Подождать завершения GPU
    vkQueueWaitIdle(queue);
    
    // 2. Уничтожить swapchain и связанные ресурсы
    destroy_swapchain(state);
    
    // 3. Уничтожить world — вызовет OnRemove observers
    //    для освобождения GPU ресурсов в компонентах
    delete state;
}
```

---

## Vulkan Handles в компонентах

### Проблема

При удалении entity или компонента Vulkan handles остаются неосвобождёнными.

### Решение: Observer OnRemove

```cpp
struct MeshComponent {
    VkBuffer vertex_buffer;
    VmaAllocation vertex_allocation;
    VkBuffer index_buffer;
    VmaAllocation index_allocation;
    uint32_t index_count;
};

// Observer для освобождения GPU ресурсов
ecs.observer<MeshComponent>()
    .event(flecs::OnRemove)
    .ctx(&gpu_context)  // VkDevice, VmaAllocator
    .each([](flecs::entity e, MeshComponent& mesh) {
        auto* ctx = e.world().ctx<GPUContext>();
        
        if (mesh.vertex_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(ctx->allocator, 
                mesh.vertex_buffer, 
                mesh.vertex_allocation);
        }
        
        if (mesh.index_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(ctx->allocator,
                mesh.index_buffer,
                mesh.index_allocation);
        }
    });
```

### Порядок уничтожения

```
1. vkQueueWaitIdle(queue)     — ждать завершения GPU работы
2. destroy_swapchain()        — framebuffers, image views
3. world.~world() / ecs_fini — вызывает OnRemove observers
4. vkDestroyDevice()          — после освобождения всех buffers
5. vkDestroyInstance()        — финал
```

---

## VMA интеграция

### Компонент с GPU данными

```cpp
struct GPUContext {
    VkDevice device;
    VmaAllocator allocator;
    VkQueue graphics_queue;
};

// Сохранить как singleton
ecs.set<GPUContext>({device, allocator, queue});
```

### Создание ресурса

```cpp
ecs.observer<MeshComponent>()
    .event(flecs::OnSet)
    .each([](flecs::entity e, MeshComponent& mesh) {
        auto* ctx = e.world().ctx<GPUContext>();
        
        // Создать vertex buffer
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = mesh.vertex_data_size;
        buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        
        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        
        vmaCreateBuffer(ctx->allocator,
            &buffer_info, &alloc_info,
            &mesh.vertex_buffer,
            &mesh.vertex_allocation,
            nullptr);
    });
```

---

## JoltPhysics интеграция

### Компоненты физики

```cpp
struct PhysicsBody {
    JPH::BodyID body_id;
    // Jolt body создаётся в observer
};

struct PhysicsSettings {
    JPH::PhysicsSystem* system;
    JPH::TempAllocator* temp_allocator;
};
```

### Система симуляции

```cpp
ecs.system<PhysicsSettings>("PhysicsStep")
    .kind(flecs::PreUpdate)
    .iter([](flecs::iter& it, PhysicsSettings* settings) {
        settings->system->Update(
            it.delta_time(),
            1,  // collision steps
            settings->temp_allocator
        );
    });
```

### Синхронизация Transform

```cpp
// Из физики в ECS
ecs.system<PhysicsBody, Position>("SyncFromPhysics")
    .kind(flecs::PostUpdate)
    .each([](PhysicsBody& body, Position& pos) {
        auto* settings = ecs.get<PhysicsSettings>();
        auto& transform = settings->system->GetBodyInterface().GetWorldTransform(body.body_id);
        pos.x = transform.GetTranslation().GetX();
        pos.y = transform.GetTranslation().GetY();
        pos.z = transform.GetTranslation().GetZ();
    });
```

---

## Многопоточность

### Ограничения

- **Vulkan:** Запись в CommandBuffer не потокобезопасна
- **JoltPhysics:** Требует синхронизации для body creation/deletion

### Паттерн

```cpp
// Многопоточные системы (без Vulkan/Jolt)
ecs.system<Position, Velocity>("Move")
    .multi_threaded()
    .each([](Position& p, const Velocity& v) {
        p.x += v.x;
    });

// Однопоточные системы (Vulkan, Physics)
ecs.system<RenderData>("RecordCommands")
    .kind(flecs::OnStore)  // После всех обновлений
    .each([](RenderData& r) {
        // Запись command buffer — только в одном потоке
    });
```

---

## Архитектура систем ProjectV

### Порядок фаз

```
OnLoad
  └── Загрузка ресурсов (текстуры, модели)

PreUpdate
  └── Обработка ввода
  └── Физика (Jolt step)

OnUpdate
  └── Логика игры
  └── Движение
  └── AI

PostUpdate
  └── Синхронизация физика → transform
  └── Frustum culling

OnStore
  └── Запись command buffers
  └── Отправка на GPU
```

### Пример структуры

```cpp
void setup_systems(flecs::world& ecs) {
    // Физика
    ecs.system<PhysicsSettings>("PhysicsStep")
        .kind(flecs::PreUpdate)
        .iter(physics_step);
    
    ecs.system<PhysicsBody, Transform>("SyncFromPhysics")
        .kind(flecs::PostUpdate)
        .each(sync_from_physics);
    
    // Движение
    ecs.system<Transform, Velocity>("Move")
        .kind(flecs::OnUpdate)
        .multi_threaded()
        .each(move_entity);
    
    // Рендеринг
    ecs.system<RenderData>("RecordCommands")
        .kind(flecs::OnStore)
        .iter(record_commands);
}
```

---

## Обработка выхода

### Из системы

```cpp
ecs.system<GameState>("CheckQuit")
    .kind(flecs::OnUpdate)
    .each([](flecs::entity e, GameState& state) {
        if (state.should_quit) {
            e.world().quit();
        }
    });
```

### Из SDL callback

```cpp
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* state = static_cast<AppState*>(appstate);
    
    if (event->type == SDL_EVENT_QUIT) {
        state->ecs.quit();
        return SDL_APP_SUCCESS;
    }
    
    return SDL_APP_CONTINUE;
}
```

---

## Отладка

### FLECS_DEBUG

```cmake
target_compile_definitions(ProjectV PRIVATE FLECS_DEBUG)
```

### Tracy интеграция

```cpp
#define FLECS_TRACY 1
#include <flecs.h>
```

### Vulkan validation layers

Включить через Vulkan SDK для отладки проблем с handles.
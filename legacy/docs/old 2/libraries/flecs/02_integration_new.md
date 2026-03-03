## Интеграция flecs

<!-- anchor: 03_integration -->

🟡 **Уровень 2: Средний**

Подключение flecs к проекту, CMake, модули и addons.

## CMake

### Добавление как подпроект

```cmake
cmake_minimum_required(VERSION 3.10)
project(MyApp)

set(CMAKE_CXX_STANDARD 17)

# Добавляем flecs как подмодуль Git
add_subdirectory(external/flecs)

add_executable(MyApp src/main.cpp)
target_link_libraries(MyApp PRIVATE flecs::flecs_static)
```

### Цели

| Цель                  | Описание                       |
|-----------------------|--------------------------------|
| `flecs::flecs_static` | Статическая библиотека         |
| `flecs::flecs`        | Статическая библиотека (алиас) |
| `flecs::flecs_shared` | Динамическая библиотека        |

### Зависимости

Flecs автоматически подтягивает платформенные зависимости:

- Windows: `ws2_32`, `dbghelp`
- Linux: `pthread`

---

## Include

Для C++ и C API достаточно одного заголовка:

```cpp
#include <flecs.h>
```

При включении в C++ автоматически подтягивается C++ API из `flecs/addons/cpp/flecs.hpp`.

---

## Модули

Модули позволяют организовать код в переиспользуемые блоки.

### C++ модуль

```cpp
struct game_module {
    game_module(flecs::world& ecs) {
        ecs.module<game_module>();

        // Регистрация компонентов
        ecs.component<Position>();
        ecs.component<Velocity>();

        // Регистрация систем
        ecs.system<Position, Velocity>("Move")
            .each([](Position& p, const Velocity& v) {
                p.x += v.x;
                p.y += v.y;
            });
    }
};

// Импорт модуля
int main() {
    flecs::world ecs;
    ecs.import<game_module>();

    // Теперь доступны компоненты и системы модуля
    ecs.entity().set<Position>({0, 0}).set<Velocity>({1, 0});

    while (ecs.progress()) {}
}
```

### C модуль

```c
// MyModule.h
void MyModuleImport(ecs_world_t *world);

// MyModule.c
#include "MyModule.h"

void MyModuleImport(ecs_world_t *world) {
    ECS_MODULE(world, MyModule);

    ECS_COMPONENT_DEFINE(world, Position);
    ECS_COMPONENT_DEFINE(world, Velocity);

    ECS_SYSTEM(world, Move, EcsOnUpdate, Position, Velocity);
}

// main.c
#include "MyModule.h"

int main() {
    ecs_world_t *world = ecs_init();
    ECS_IMPORT(world, MyModule);

    while (ecs_progress(world, 0)) {}
    ecs_fini(world);
}
```

---

## Addons

Flecs включает дополнительные модули (addons):

| Addon             | Описание                           |
|-------------------|------------------------------------|
| `flecs::c`        | C API                              |
| `flecs::cpp`      | C++ API                            |
| `flecs::rest`     | REST API для удалённого управления |
| `flecs::meta`     | Метаданные компонентов             |
| `flecs::json`     | Сериализация в JSON                |
| `flecs::units`    | Единицы измерения                  |
| `flecs::timer`    | Таймеры и интервалы                |
| `flecs::system`   | Системы                            |
| `flecs::pipeline` | Pipeline                           |
| `flecs::query`    | Queries                            |

### Кастомная сборка

Для уменьшения размера можно собрать только нужные addons:

```cmake
set(FLECS_CUSTOM_BUILD ON CACHE BOOL "" FORCE)
add_subdirectory(external/flecs)
```

Подробнее: [BuildingFlecs.md](../../external/flecs/docs/BuildingFlecs.md)

---

## Порядок вызовов

### Типичный жизненный цикл

1. Создать `flecs::world`
2. Зарегистрировать компоненты и системы
3. Создать entities
4. В цикле: `world.progress(delta_time)`
5. При выходе: world уничтожается автоматически

```cpp
int main() {
    // 1. Создание world
    flecs::world ecs;

    // 2. Регистрация систем
    ecs.system<Position, Velocity>().each([](Position& p, const Velocity& v) {
        p.x += v.x;
    });

    // 3. Создание entities
    ecs.entity().set<Position>({0, 0}).set<Velocity>({1, 0});

    // 4. Игровой цикл
    while (ecs.progress(1.0f / 60.0f)) {
        // Выход по ecs.quit()
    }

    // 5. World уничтожается автоматически (RAII)
    return 0;
}
```

### Выход из цикла

```cpp
// В любом месте
ecs.quit();

// Или через условие
while (should_run && ecs.progress()) {}
```

---

## Интеграция с внешним циклом

Flecs можно интегрировать в любой цикл обновления:

### Пример: пользовательский цикл

```cpp
flecs::world ecs;
setup_systems(ecs);
create_entities(ecs);

while (!should_exit) {
    // Внешние обновления
    process_input();

    // ECS обновление
    ecs.progress(delta_time);

    // Внешняя отрисовка
    render();
}
```

### Пример: фиксированный timestep

```cpp
flecs::world ecs;
double accumulator = 0.0;
const double fixed_dt = 1.0 / 60.0;

while (!should_exit) {
    double frame_time = get_frame_time();
    accumulator += frame_time;

    while (accumulator >= fixed_dt) {
        ecs.progress(fixed_dt);
        accumulator -= fixed_dt;
    }

    render();
}
```

---

## Многопоточность

### Включение многопоточности

```cpp
flecs::world ecs;

// Включить пул потоков
ecs.set_threads(4);

// Система с параллельным выполнением
ecs.system<Position, Velocity>()
    .multi_threaded()
    .each([](Position& p, const Velocity& v) {
        p.x += v.x;
    });
```

### Ограничения

- Системы с `immediate = true` не могут быть `multi_threaded`
- Доступ к общим данным требует синхронизации
- Изменение структуры ECS (создание/удаление entities) должно выполняться в основном потоке или через `ecs_defer`

---

## Отладка

### FLECS_DEBUG

Включает дополнительные проверки и assert'ы:

```cmake
target_compile_definitions(MyApp PRIVATE FLECS_DEBUG)
```

### Flecs Explorer

Веб-инструмент для просмотра состояния ECS:

- Entities и компоненты
- Иерархии
- Systems и queries
- Производительность

Требует REST addon: [flecs.dev/explorer](https://flecs.dev/explorer)

---

## Интеграция flecs с ProjectV

<!-- anchor: 09_projectv-integration -->

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

---

## Паттерны flecs для ProjectV

<!-- anchor: 10_projectv-patterns -->

🔴 **Уровень 3: Продвинутый**

Архитектурные паттерны для воксельного движка.

## Воксельные чанки

### Компоненты чанка

```cpp
struct ChunkPosition {
    int32_t x, y, z;  // Координаты в сетке чанков
};

struct ChunkData {
    static constexpr size_t SIZE = 16 * 16 * 16;
    uint8_t blocks[SIZE];  // ID блоков
    uint8_t dirty_mask;    // Какие секции изменились
};

struct ChunkMesh {
    VkBuffer vertex_buffer;
    VmaAllocation vertex_allocation;
    uint32_t vertex_count;
    bool needs_rebuild;
};

struct ChunkGPU {
    VkBuffer storage_buffer;  // Для compute shaders
    VmaAllocation storage_allocation;
};
```

### Иерархия чанков

```cpp
// World entity как корень
auto world_entity = ecs.entity("World");

// Чанки как дети
auto chunk = ecs.entity()
    .child_of(world_entity)
    .set<ChunkPosition>({0, 0, 0})
    .set<ChunkData>{}
    .set<ChunkMesh>{{}, 0, false};
```

### Генерация чанка

```cpp
ecs.system<ChunkPosition, ChunkData>("GenerateChunk")
    .kind(flecs::OnLoad)
    .without<ChunkMesh>()  // Только новые чанки
    .each([](ChunkPosition& pos, ChunkData& data) {
        // Процедурная генерация
        for (size_t i = 0; i < ChunkData::SIZE; i++) {
            data.blocks[i] = generate_block(pos, i);
        }
        data.dirty_mask = 0xFF;  // Весь чанк грязный
    });
```

### Перестроение меша

```cpp
ecs.system<ChunkData, ChunkMesh>("RebuildChunkMesh")
    .kind(flecs::OnUpdate)
    .iter([](flecs::iter& it, ChunkData* data, ChunkMesh* mesh) {
        auto* ctx = it.world().ctx<GPUContext>();
        
        for (auto i : it) {
            if (!mesh[i].needs_rebuild && data[i].dirty_mask == 0) {
                continue;
            }
            
            // Генерация меша
            MeshVertices vertices = generate_chunk_mesh(data[i]);
            
            // Обновление GPU buffer
            if (mesh[i].vertex_buffer != VK_NULL_HANDLE) {
                vmaDestroyBuffer(ctx->allocator, 
                    mesh[i].vertex_buffer, 
                    mesh[i].vertex_allocation);
            }
            
            create_vertex_buffer(ctx, vertices, mesh[i]);
            mesh[i].needs_rebuild = false;
            data[i].dirty_mask = 0;
        }
    });
```

---

## GPU-Driven Rendering

### Compute Pipeline для чанков

```cpp
struct ComputePipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkDescriptorSetLayout set_layout;
};

// Singleton
ecs.set<ComputePipeline>({pipeline, layout, set_layout});

// Система для dispatch
ecs.system<ChunkGPU, ChunkData>("DispatchChunkCompute")
    .kind(flecs::OnUpdate)
    .iter([](flecs::iter& it, ChunkGPU* gpu, ChunkData* data) {
        auto* pipeline = it.world().ctx<ComputePipeline>();
        auto* ctx = it.world().ctx<GPUContext>();
        
        VkCommandBuffer cmd = begin_compute_commands(ctx);
        
        for (auto i : it) {
            // Записать данные чанка в SSBO
            update_ssbo(ctx, gpu[i], data[i]);
            
            // Dispatch compute
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, 
                pipeline->pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                pipeline->layout, 0, 1, &descriptor_set, 0, nullptr);
            vkCmdDispatch(cmd, 16 / 8, 16 / 8, 16 / 8);
        }
        
        end_compute_commands(ctx, cmd);
    });
```

---

## Frustum Culling

### Компонент для видимости

```cpp
struct Visible {
    bool in_frustum;
    float distance_to_camera;
};

struct BoundingBox {
    glm::vec3 min;
    glm::vec3 max;
};
```

### Система culling

```cpp
ecs.system<ChunkPosition, BoundingBox, Visible>("FrustumCull")
    .kind(flecs::PreUpdate)
    .iter([](flecs::iter& it, ChunkPosition* pos, BoundingBox* bb, Visible* vis) {
        auto* camera = it.world().ctx<Camera>();
        Frustum frustum = camera->get_frustum();
        
        for (auto i : it) {
            glm::vec3 world_pos(
                pos[i].x * 16.0f,
                pos[i].y * 16.0f,
                pos[i].z * 16.0f
            );
            
            vis[i].in_frustum = frustum.intersects(bb[i], world_pos);
            vis[i].distance_to_camera = glm::distance(
                world_pos, 
                camera->position
            );
        }
    });
```

### Рендеринг только видимых

```cpp
ecs.query_builder<ChunkMesh, Visible>()
    .term_at(1).optional()  // Visible может отсутствовать
    .build()
    .iter([](flecs::iter& it, ChunkMesh* mesh, Visible* vis) {
        for (auto i : it) {
            // Пропустить невидимые
            if (vis && !vis[i].in_frustum) {
                continue;
            }
            
            // Рендеринг
            render_chunk_mesh(mesh[i]);
        }
    });
```

---

## LOD (Level of Detail)

### Компонент LOD

```cpp
struct LODState {
    uint8_t current_lod;  // 0, 1, 2, 3
    float last_distance;
};

struct LODMeshes {
    ChunkMesh meshes[4];  // Один на каждый LOD
};
```

### Выбор LOD

```cpp
ecs.system<Visible, LODState>("UpdateLOD")
    .kind(flecs::PreUpdate)
    .iter([](flecs::iter& it, Visible* vis, LODState* lod) {
        for (auto i : it) {
            float d = vis[i].distance_to_camera;
            
            if (d < 50.0f) lod[i].current_lod = 0;
            else if (d < 100.0f) lod[i].current_lod = 1;
            else if (d < 200.0f) lod[i].current_lod = 2;
            else lod[i].current_lod = 3;
            
            lod[i].last_distance = d;
        }
    });
```

---

## Управление памятью чанков

### Выгрузка дальних чанков

```cpp
ecs.system<ChunkPosition, Visible>("UnloadDistantChunks")
    .kind(flecs::OnStore)
    .iter([](flecs::iter& it, ChunkPosition* pos, Visible* vis) {
        constexpr float MAX_DISTANCE = 300.0f;
        
        // Внимание: НЕ нужно вызывать defer_begin()/defer_end() внутри систем!
        // Flecs автоматически обрабатывает staging для операций в системах.
        // defer_begin/end нужны только ВНЕ систем (например, в main loop).
        
        for (auto i : it) {
            if (vis[i].distance_to_camera > MAX_DISTANCE) {
                it.entity(i).destruct();  // OnRemove освободит GPU ресурсы
            }
        }
    });
```

### Загрузка новых чанков

```cpp
ecs.system<Camera>("LoadNearbyChunks")
    .kind(flecs::OnLoad)
    .iter([](flecs::iter& it, Camera* cam) {
        constexpr int32_t LOAD_RADIUS = 10;
        
        auto world_entity = it.world().lookup("World");
        
        for (int32_t x = -LOAD_RADIUS; x <= LOAD_RADIUS; x++) {
            for (int32_t z = -LOAD_RADIUS; z <= LOAD_RADIUS; z++) {
                // Проверить существование чанка
                int32_t chunk_x = cam->chunk_x + x;
                int32_t chunk_z = cam->chunk_z + z;
                
                auto path = fmt::format("World::Chunk_{}_{}", chunk_x, chunk_z);
                if (it.world().lookup(path.c_str())) {
                    continue;  // Уже существует
                }
                
                // Создать новый чанк
                it.world().entity(path.c_str())
                    .child_of(world_entity)
                    .set<ChunkPosition>({chunk_x, 0, chunk_z})
                    .set<ChunkData>{}
                    .add<NeedsGeneration>();
            }
        }
    });
```

---

## Синхронизация с физикой

### Статические коллайдеры чанков

```cpp
struct ChunkCollider {
    JPH::Ref<JPH::Shape> shape;
    JPH::BodyID body_id;
};

ecs.observer<ChunkMesh>()
    .event(flecs::OnSet)
    .ctx(&physics_system)
    .each([](flecs::entity e, ChunkMesh& mesh) {
        auto* settings = e.world().ctx<PhysicsSettings>();
        
        // Создать triangle mesh collider
        auto shape = create_chunk_shape(mesh);
        
        JPH::BodyCreationSettings body_settings(
            shape,
            JPH::RVec3(chunk_x * 16, 0, chunk_z * 16),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Static,
            JPH::ObjectLayer::Static
        );
        
        auto body = settings->system->GetBodyInterface().CreateBody(body_settings);
        settings->system->GetBodyInterface().AddBody(body->GetID(), JPH::EActivation::DontActivate);
        
        e.set<ChunkCollider>({shape, body->GetID()});
    });
```

---

## Batch рендеринг

### Индиректная отрисовка

```cpp
struct IndirectBatch {
    VkBuffer indirect_buffer;
    uint32_t draw_count;
};

ecs.system<ChunkMesh, Visible>("PrepareIndirectBatch")
    .kind(flecs::OnStore)
    .iter([](flecs::iter& it, ChunkMesh* mesh, Visible* vis) {
        auto* ctx = it.world().ctx<GPUContext>();
        
        std::vector<VkDrawIndirectCommand> commands;
        
        for (auto i : it) {
            if (vis && !vis[i].in_frustum) continue;
            
            commands.push_back({
                .vertexCount = mesh[i].vertex_count,
                .instanceCount = 1,
                .firstVertex = 0,
                .firstInstance = 0
            });
        }
        
        // Обновить indirect buffer
        update_indirect_buffer(ctx, commands);
    });
```

---

## Threading модель

### Job System вместо std::thread

ProjectV использует **M:N Job System** для параллелизма, а не `std::thread` или `std::async`.

> **Важно:** Создание `std::thread` для каждой задачи — это anti-pattern:
> - Overhead на создание/уничтожение потоков
> - Нет контроля над количеством потоков
> - Cache thrashing при миграции потоков между ядрами

### Многопоточные системы в Flecs

```cpp
// Многопоточные системы (чистые вычисления)
// Flecs сам распределяет работу по доступным потокам
ecs.system<ChunkData>("UpdateBlocks")
    .multi_threaded()  // Flecs управляет потоками
    .each([](ChunkData& data) { 
        // Потокобезопасный код
        // Не обращайтесь к глобальному состоянию!
    });

ecs.system<Position, Velocity>("MoveEntities")
    .multi_threaded()
    .each([](Position& p, Velocity& v) { 
        p.value += v.value * 0.016f;
    });
```

### Однопоточные системы (GPU, физика)

```cpp
// Системы с GPU доступом — однопоточные
ecs.system<ChunkMesh>("UploadToGPU")
    .kind(flecs::OnStore)
    .iter([](flecs::iter& it, ChunkMesh* mesh) { 
        // Vulkan calls — только однопоточные!
        // Vulkan command buffers не потокобезопасны
    });

// Физика — однопоточная (JoltPhysics имеет внутренний параллелизм)
ecs.system<PhysicsSettings>("StepPhysics")
    .kind(flecs::PreUpdate)
    .iter([](flecs::iter& it, PhysicsSettings* s) { 
        s->system->Update(delta_time, 1, 1, nullptr, nullptr);
    });
```

### Job System интеграция

```cpp
// Job System для задач вне ECS
class JobSystem {
public:
    // M:N threading: M задач на N потоков (обычно N = hardware_concurrency)
    void schedule(std::function<void()> task) {
        // Задача добавляется в очередь
        // Свободный worker thread её заберёт
    }
    
    // Параллельное выполнение диапазона
    void parallelFor(size_t count, std::function<void(size_t)> func) {
        // Разбивает диапазон на chunks
        // Каждый chunk выполняется на отдельном worker
    }
    
    // Ожидание завершения всех задач
    void wait() {
        // Блокирует до завершения всех scheduled задач
    }
};

// Использование для генерации чанков
void generateChunksParallel(JobSystem& jobs, 
                            const std::vector<ChunkCoord>& coords) {
    jobs.parallelFor(coords.size(), [&](size_t i) {
        generateChunk(coords[i]);
    });
    jobs.wait();  // Ждём завершения
}
```

### Defer и многопоточность

```cpp
// ПРАВИЛЬНО: defer_outside системы
void mainLoop(flecs::world& ecs) {
    // Defer можно использовать ВНЕ систем
    ecs.defer_begin();
    
    // Пакетное создание сущностей
    for (int i = 0; i < 100; ++i) {
        ecs.entity().set<Position>({{i, 0, 0}});
    }
    
    ecs.defer_end();  // Все изменения применяются разом
}

// НЕПРАВИЛЬНО: defer_inside .multi_threaded() системы
ecs.system<Position>("BadSystem")
    .multi_threaded()
    .iter([](flecs::iter& it, Position* pos) {
        it.world().defer_begin();  // ❌ НЕ ДЕЛАЙТЕ ЭТОГО!
        // ...
        it.world().defer_end();    // ❌ Deadlock или data race!
    });

// ПРАВИЛЬНО: внутри систем defer не нужен
ecs.system<Position>("GoodSystem")
    .multi_threaded()
    .iter([](flecs::iter& it, Position* pos) {
        // Flecs автоматически staged changes для вас
        for (auto i : it) {
            it.entity(i).set<Velocity>({{1, 0, 0}});  // ✅ Автоматически отложено
        }
    });
```

---

## Паттерны Prefab для блоков

```cpp
// Prefab для типа блока
auto Stone = ecs.prefab("BlockStone")
    .set<BlockType>{BlockType::STONE}
    .set<BlockHardness>{3.0f}
    .set<BlockColor>{{0.5f, 0.5f, 0.5f}};

auto Dirt = ecs.prefab("BlockDirt")
    .set<BlockType>{BlockType::DIRT}
    .set<BlockHardness>{1.0f}
    .set<BlockColor>{{0.4f, 0.3f, 0.2f}};

// Instance блока в мире
auto block = ecs.entity()
    .is_a(Stone)
    .set<WorldPosition>({10, 20, 5});
```

---

## Отладка

### Визуализация чанков

```cpp
#ifdef DEBUG
ecs.system<ChunkPosition, BoundingBox, Visible>("DebugDrawChunks")
    .kind(flecs::OnStore)
    .iter([](flecs::iter& it, ChunkPosition* pos, BoundingBox* bb, Visible* vis) {
        for (auto i : it) {
            glm::vec3 color = vis[i].in_frustum 
                ? glm::vec3(0, 1, 0)   // Зелёный — видимый
                : glm::vec3(1, 0, 0);  // Красный — невидимый
            
            debug_draw_aabb(bb[i], pos[i], color);
        }
    });
#endif
```

### Статистика

```cpp
struct ChunkStats {
    uint32_t total_chunks;
    uint32_t visible_chunks;
    uint32_t meshes_uploaded;
};

ecs.system<Visible>("CollectChunkStats")
    .kind(flecs::OnStore)
    .iter([](flecs::iter& it, Visible* vis) {
        auto* stats = it.world().get_mut<ChunkStats>();
        stats->visible_chunks = 0;
        
        for (auto i : it) {
            if (vis[i].in_frustum) {
                stats->visible_chunks++;
            }
        }
    });
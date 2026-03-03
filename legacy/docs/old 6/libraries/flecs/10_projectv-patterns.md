# Паттерны flecs для ProjectV

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
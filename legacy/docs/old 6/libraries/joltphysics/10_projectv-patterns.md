# Воксельные паттерны JoltPhysics для ProjectV

🔴 **Уровень 3: Продвинутый**

## Обзор

Паттерны использования JoltPhysics для воксельного движка: HeightField для ландшафта, чанкирование физики, динамические
воксели, жидкости.

---

## HeightField для воксельного ландшафта

### Создание HeightField из чанка

```cpp
JPH::ShapeRefC CreateHeightFieldFromChunk(const VoxelChunk& chunk)
{
    const uint32_t size = chunk.GetSize();
    std::vector<float> heights(size * size);

    // Извлечение высот из воксельных данных
    for (uint32_t z = 0; z < size; ++z) {
        for (uint32_t x = 0; x < size; ++x) {
            float height = chunk.GetHighestVoxel(x, z);
            heights[z * size + x] = height;
        }
    }

    JPH::HeightFieldShapeSettings settings(
        heights.data(),
        JPH::Vec3(chunk.GetWorldPosition().x, 0, chunk.GetWorldPosition().z),
        JPH::Vec3(chunk.GetVoxelSize(), 1.0f, chunk.GetVoxelSize()),
        size
    );

    // Оптимизация для дальних чанков
    settings.mBlockSize = 4;
    settings.mBitsPerSample = 8;

    return settings.Create().Get();
}
```

### Дыры в HeightField (пещеры)

```cpp
void AddCaveEntrance(std::vector<float>& heights, uint32_t size, uint32_t x, uint32_t z, float radius)
{
    const float NO_COLLISION = JPH::HeightFieldShapeConstants::cNoCollisionValue;

    for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dz * dz <= radius * radius) {
                uint32_t px = x + dx;
                uint32_t pz = z + dz;
                if (px < size && pz < size) {
                    heights[pz * size + px] = NO_COLLISION;
                }
            }
        }
    }
}
```

---

## Чанкирование физики

### Структура чанка физики

```cpp
struct PhysicsChunk
{
    glm::ivec3 coords;
    JPH::BodyID terrain_body;
    std::vector<JPH::BodyID> dynamic_bodies;
    bool is_active = false;
    float distance_to_player = 0.0f;

    // LOD для физики
    enum class LOD { Full, Simplified, BoundsOnly };
    LOD current_lod = LOD::Full;
};
```

### Управление чанками

```cpp
class PhysicsChunkManager
{
public:
    void Update(const glm::vec3& player_position)
    {
        // Обновление дистанций
        for (auto& [coords, chunk] : chunks) {
            chunk.distance_to_player = glm::distance(
                glm::vec3(coords) * chunk_size,
                player_position
            );
        }

        // Активация/деактивация
        for (auto& [coords, chunk] : chunks) {
            if (chunk.distance_to_player < active_radius && !chunk.is_active) {
                ActivateChunk(chunk);
            } else if (chunk.distance_to_player > active_radius * 1.5f && chunk.is_active) {
                DeactivateChunk(chunk);
            }
        }

        // LOD обновление
        for (auto& [coords, chunk] : chunks) {
            PhysicsChunk::LOD new_lod = CalculateLOD(chunk.distance_to_player);
            if (new_lod != chunk.current_lod) {
                UpdateChunkLOD(chunk, new_lod);
            }
        }
    }

private:
    std::unordered_map<glm::ivec3, PhysicsChunk> chunks;
    float chunk_size = 32.0f;
    float active_radius = 100.0f;

    void ActivateChunk(PhysicsChunk& chunk)
    {
        auto& interface = physics_system.GetBodyInterfaceNoLock();
        interface.AddBody(chunk.terrain_body, JPH::EActivation::DontActivate);

        for (auto& body_id : chunk.dynamic_bodies) {
            interface.AddBody(body_id, JPH::EActivation::Activate);
        }

        chunk.is_active = true;
    }

    void DeactivateChunk(PhysicsChunk& chunk)
    {
        auto& interface = physics_system.GetBodyInterfaceNoLock();
        interface.RemoveBody(chunk.terrain_body);

        for (auto& body_id : chunk.dynamic_bodies) {
            interface.RemoveBody(body_id);
        }

        chunk.is_active = false;
    }

    PhysicsChunk::LOD CalculateLOD(float distance)
    {
        if (distance < 50.0f) return PhysicsChunk::LOD::Full;
        if (distance < 100.0f) return PhysicsChunk::LOD::Simplified;
        return PhysicsChunk::LOD::BoundsOnly;
    }
};
```

---

## Динамические воксели

### Разрушаемые объекты

```cpp
class DestructibleVoxelObject
{
public:
    void Initialize(const std::vector<Voxel>& voxels, float voxel_size)
    {
        // Создание составной формы
        JPH::StaticCompoundShapeSettings compound_settings;

        for (const auto& voxel : voxels) {
            JPH::BoxShapeSettings box_settings(JPH::Vec3(voxel_size * 0.5f));
            auto box_result = box_settings.Create();

            if (box_result.IsValid()) {
                compound_settings.AddShape(
                    JPH::Vec3(voxel.position.x, voxel.position.y, voxel.position.z),
                    JPH::Quat::sIdentity(),
                    box_result.Get()
                );
            }
        }

        auto shape_result = compound_settings.Create();
        if (!shape_result.IsValid()) return;

        // Создание тела
        JPH::BodyCreationSettings body_settings(
            shape_result.Get(),
            JPH::RVec3::sZero(),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic,
            Layers::MOVING
        );

        body_id = physics_system.GetBodyInterfaceNoLock().CreateAndAddBody(
            body_settings, JPH::EActivation::Activate
        );
    }

    void ApplyExplosion(const glm::vec3& center, float force)
    {
        // Удаление старого тела
        auto& interface = physics_system.GetBodyInterfaceNoLock();
        JPH::RVec3 current_pos = interface.GetPosition(body_id);
        interface.RemoveBody(body_id);
        interface.DestroyBody(body_id);

        // Создание отдельных тел для каждого вокселя
        for (const auto& voxel : affected_voxels) {
            glm::vec3 world_pos = current_world_pos + voxel.position;

            JPH::BoxShapeSettings box_settings(JPH::Vec3(voxel_size * 0.5f));
            auto shape = box_settings.Create().Get();

            JPH::BodyCreationSettings debris_settings(
                shape,
                JPH::RVec3(world_pos.x, world_pos.y, world_pos.z),
                JPH::Quat::sIdentity(),
                JPH::EMotionType::Dynamic,
                Layers::MOVING
            );

            JPH::BodyID debris_id = interface.CreateAndAddBody(
                debris_settings, JPH::EActivation::Activate
            );

            // Импульс от взрыва
            glm::vec3 dir = glm::normalize(world_pos - center);
            interface.AddImpulse(debris_id, JPH::Vec3(dir.x, dir.y, dir.z) * force);
        }
    }

private:
    JPH::BodyID body_id;
    std::vector<Voxel> affected_voxels;
    float voxel_size;
};
```

---

## Character Controller для воксельного мира

### Кастомный controller

```cpp
class VoxelCharacterController
{
public:
    void Initialize(JPH::PhysicsSystem* system)
    {
        JPH::CharacterVirtualSettings settings;
        settings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
        settings.mMaxStrength = 100.0f;
        settings.mMass = 70.0f;

        // Капсула для персонажа
        settings.mShape = JPH::RotatedTranslatedShapeSettings(
            JPH::Vec3(0, 0.9f, 0),  // Смещение для центра масс
            JPH::Quat::sIdentity(),
            new JPH::CapsuleShape(0.5f, 0.3f)
        ).Create().Get();

        character = new JPH::CharacterVirtual(
            &settings,
            JPH::RVec3::sZero(),
            JPH::Quat::sIdentity(),
            system
        );
    }

    void Update(float delta_time, const glm::vec3& movement_input, bool jump)
    {
        // Применение движения
        JPH::Vec3 desired_velocity = JPH::Vec3(movement_input.x, 0, movement_input.z) * move_speed;

        // Прыжок
        if (jump && character->IsOnGround()) {
            desired_velocity.SetY(jump_velocity);
        } else {
            desired_velocity.SetY(character->GetLinearVelocity().GetY());
        }

        character->SetLinearVelocity(desired_velocity);

        // Обновление
        character->Update(
            delta_time,
            JPH::Vec3(0, -20.0f, 0),  // Гравитация
            broad_phase_filter,
            object_layer_filter,
            body_filter,
            shape_filter,
            *temp_allocator
        );
    }

    glm::vec3 GetPosition() const
    {
        JPH::RVec3 pos = character->GetPosition();
        return glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
    }

private:
    JPH::CharacterVirtual* character;
    float move_speed = 5.0f;
    float jump_velocity = 8.0f;
};
```

---

## Асинхронное обновление коллайдеров

### Система очереди обновлений

```cpp
class AsyncColliderUpdater
{
public:
    struct UpdateTask
    {
        glm::ivec3 chunk_coords;
        std::vector<VoxelModification> modifications;
        uint32_t priority;
    };

    void ScheduleUpdate(const glm::ivec3& coords, const std::vector<VoxelModification>& mods, uint32_t priority)
    {
        std::lock_guard<std::mutex> lock(queue_mutex);

        UpdateTask task{coords, mods, priority};
        update_queue.push(task);
    }

    void ProcessUpdates()
    {
        std::lock_guard<std::mutex> lock(queue_mutex);

        while (!update_queue.empty()) {
            auto task = update_queue.top();
            update_queue.pop();

            // Обновление HeightField для чанка
            UpdateChunkCollider(task);
        }
    }

private:
    std::priority_queue<UpdateTask> update_queue;
    std::mutex queue_mutex;

    void UpdateChunkCollider(const UpdateTask& task)
    {
        // Пересоздание HeightField с учётом модификаций
        auto chunk = GetChunk(task.chunk_coords);
        auto new_shape = CreateHeightFieldFromChunk(chunk);

        auto& interface = physics_system.GetBodyInterfaceNoLock();
        interface.SetShape(
            chunk->terrain_body,
            new_shape,
            false,
            JPH::EActivation::DontActivate
        );
    }
};
```

---

## Оптимизация для больших миров

### Приоритизация по расстоянию

```cpp
uint32_t CalculateUpdatePriority(const glm::ivec3& chunk_coords, const glm::vec3& player_pos)
{
    float distance = glm::distance(glm::vec3(chunk_coords) * 32.0f, player_pos);

    uint32_t priority = 0;

    if (distance < 30.0f) priority += 10;  // Очень близко
    else if (distance < 60.0f) priority += 7;
    else if (distance < 100.0f) priority += 4;
    else if (distance < 200.0f) priority += 1;

    return priority;
}
```

### Batch обновления

```cpp
void BatchProcessModifications()
{
    // Группировка по чанкам
    std::unordered_map<glm::ivec3, std::vector<VoxelModification>> batched;

    for (const auto& mod : pending_modifications) {
        batched[mod.chunk_coords].push_back(mod);
    }

    // Создание задач
    for (const auto& [coords, mods] : batched) {
        uint32_t priority = CalculateUpdatePriority(coords, player_position);
        collider_updater.ScheduleUpdate(coords, mods, priority);
    }

    pending_modifications.clear();
}
```

---

## Детерминированная симуляция для мультиплеера

```cpp
class DeterministicPhysics
{
public:
    void Initialize()
    {
        // Включение детерминизма (требует JPH_CROSS_PLATFORM_DETERMINISTIC)
        physics_system.Init(...);

        // Фиксированный seed
        physics_system.SetDeterministicSimulationSeed(0x12345678);

        // Fixed timestep обязателен
        fixed_dt = 1.0f / 60.0f;
    }

    void Update(float delta_time)
    {
        accumulator += delta_time;

        while (accumulator >= fixed_dt) {
            // Детерминированный шаг
            physics_system.Update(fixed_dt, 1, temp_allocator, nullptr);
            accumulator -= fixed_dt;

            // Сохранение состояния для сетевой синхронизации
            RecordPhysicsState();
        }
    }

    struct PhysicsSnapshot
    {
        uint32_t frame;
        std::vector<BodyState> bodies;

        uint64_t CalculateHash() const
        {
            uint64_t hash = frame;
            for (const auto& body : bodies) {
                hash ^= body.CalculateHash();
            }
            return hash;
        }
    };

private:
    float fixed_dt;
    float accumulator = 0.0f;

    void RecordPhysicsState()
    {
        PhysicsSnapshot snapshot;
        snapshot.frame = current_frame++;

        // Сбор состояний всех тел
        JPH::BodyIDVector body_ids;
        physics_system.GetBodies(body_ids);

        auto& interface = physics_system.GetBodyInterfaceNoLock();
        for (const auto& id : body_ids) {
            BodyState state;
            state.id = id.GetIndexAndSequenceNumber();
            state.position = interface.GetPosition(id);
            state.rotation = interface.GetRotation(id);
            state.velocity = interface.GetLinearVelocity(id);

            snapshot.bodies.push_back(state);
        }

        // Хеш для проверки согласованности
        uint64_t hash = snapshot.CalculateHash();
        SendHashToServer(hash);
    }
};
```

---

## Слои для воксельного мира

```cpp
namespace VoxelLayers
{
    // Object Layers
    static constexpr JPH::ObjectLayer TERRAIN = 0;      // Статический ландшафт
    static constexpr JPH::ObjectLayer DYNAMIC = 1;      // Динамические объекты
    static constexpr JPH::ObjectLayer PLAYER = 2;       // Игрок
    static constexpr JPH::ObjectLayer SENSOR = 3;       // Триггеры
    static constexpr JPH::ObjectLayer DEBRIS = 4;       // Обломки
    static constexpr JPH::ObjectLayer NUM_LAYERS = 5;

    // BroadPhase Layers
    static constexpr JPH::BroadPhaseLayer BP_TERRAIN(0);
    static constexpr JPH::BroadPhaseLayer BP_DYNAMIC(1);
    static constexpr JPH::BroadPhaseLayer BP_PLAYER(2);
    static constexpr uint32_t BP_NUM_LAYERS = 3;
}

// Фильтр коллизий
class VoxelLayerFilter : public JPH::ObjectLayerPairFilter
{
public:
    bool ShouldCollide(JPH::ObjectLayer layer1, JPH::ObjectLayer layer2) const override
    {
        switch (layer1) {
        case VoxelLayers::TERRAIN:
            return layer2 != VoxelLayers::TERRAIN;  // Terrain не коллизит сам с собой

        case VoxelLayers::DYNAMIC:
        case VoxelLayers::DEBRIS:
            return true;  // Сталкивается со всем

        case VoxelLayers::PLAYER:
            return layer2 == VoxelLayers::TERRAIN ||
                   layer2 == VoxelLayers::DYNAMIC ||
                   layer2 == VoxelLayers::SENSOR;

        case VoxelLayers::SENSOR:
            return layer2 == VoxelLayers::PLAYER;  // Только с игроком

        default:
            return false;
        }
    }
};

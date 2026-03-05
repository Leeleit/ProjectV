# JoltPhysics: Продвинутые оптимизации и хардкор

## Data-Oriented Design

### SoA vs AoS

Традиционный подход (AoS — Array of Structures) хранит данные тела вместе:

```cpp
struct PhysicsBodyAoS {
    JPH::BodyID id;
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 velocity;
    float mass;
    bool is_active;
};
```

SoA (Structure of Arrays) разделяет данные для кэш-линейности:

```cpp
alignas(64) struct PhysicsBodySoA {
    std::vector<JPH::BodyID> ids;
    std::vector<glm::vec3> positions;
    std::vector<glm::quat> rotations;
    std::vector<glm::vec3> velocities;
    std::vector<float> masses;
    std::vector<uint8_t> active_flags;
};
```

### Hot/Cold Separation

Горячие данные (читаются каждый кадр):

- Позиция, вращение, скорости
- Состояние тела (активно/спит)

Холодные данные (читаются редко):

- Масса, трение, упругость
- ID формы, пользовательские данные

```cpp
alignas(64) struct alignas(64) HotPhysicsData {
    std::vector<JPH::RVec3> positions;
    std::vector<JPH::Quat> rotations;
    std::vector<JPH::Vec3> linear_velocities;
    std::vector<JPH::Vec3> angular_velocities;
    std::vector<uint32_t> active_states;
};

struct ColdPhysicsData {
    std::vector<float> masses;
    std::vector<float> friction;
    std::vector<float> restitution;
    std::vector<JPH::ShapeRefC> shapes;
};
```

### Cache Line Alignment

```cpp
alignas(64) struct AlignedBodyData {
    JPH::Vec3 position;
    float padding1;
    JPH::Vec3 velocity;
    float padding2;
};

static_assert(sizeof(AlignedBodyData) == 32, "Cache line aligned");
```

---

## Многопоточность

### M:N Job System

JoltPhysics использует M:N модель: M рабочих потоков обрабатывают N физических задач.

```cpp
#include <Jolt/Core/JobSystemThreadPool.h>

class PhysicsJobSystem {
public:
    void Init(uint32_t num_threads) {
        m_job_system = std::make_unique<JPH::JobSystemThreadPool>(
            JPH::cMaxPhysicsJobs,
            JPH::cMaxPhysicsBarriers,
            num_threads
        );
    }

    void Update(JPH::PhysicsSystem& physics, float dt) {
        physics.Update(dt, 1, m_temp_allocator.get(), m_job_system.get());
    }

private:
    std::unique_ptr<JPH::JobSystemThreadPool> m_job_system;
    std::unique_ptr<JPH::TempAllocatorImpl> m_temp_allocator;
};
```

### Thread-Local аллокация

```cpp
class ThreadLocalTempAllocator {
public:
    ThreadLocalTempAllocator() {
        // Количество потоков берется из конфигурации движка ProjectV
        m_allocators.resize(projectv::config::get_thread_count());
        for (auto& alloc : m_allocators) {
            alloc = std::make_unique<JPH::TempAllocatorImpl>(1024 * 1024);
        }
    }

    JPH::TempAllocator* Get() {
        return m_allocators[get_current_thread_index()].get();
    }

private:
    std::vector<std::unique_ptr<JPH::TempAllocatorImpl>> m_allocators;
};
```

### False Sharing Avoidance

```cpp
alignas(64) struct ThreadLocalCounter {
    std::atomic<uint32_t> active_bodies{0};
    char pad[64 - sizeof(std::atomic<uint32_t>)];
};

class AvoidFalseSharing {
public:
    void Increment(size_t thread_id) {
        m_counters[thread_id].active_bodies.fetch_add(1, std::memory_order_relaxed);
    }

private:
    std::vector<ThreadLocalCounter> m_counters;
};
```

---

## Zero-Copy загрузка

### Memory Mapping для HeightField

```cpp
#include <sys/mman.h>
#include <fcntl.h>

class MappedHeightField {
public:
    bool Load(const std::string& path) {
        m_file = open(path.c_str(), O_RDONLY);
        if (m_file < 0) return false;

        struct stat sb;
        fstat(m_file, &sb);
        m_size = sb.st_size;

        m_data = mmap(nullptr, m_size, PROT_READ, MAP_PRIVATE, m_file, 0);
        if (m_data == MAP_FAILED) return false;

        madvise(m_data, m_size, MADV_SEQUENTIAL);
        return true;
    }

    JPH::HeightFieldShapeSettings CreateSettings() const {
        const float* heights = static_cast<const float*>(m_data);
        uint32_t sample_count = static_cast<uint32_t>(std::sqrt(m_size / sizeof(float)));

        return JPH::HeightFieldShapeSettings(
            heights,
            JPH::Vec3(0, 0, 0),
            JPH::Vec3(1.0f, 1.0f, 1.0f),
            sample_count
        );
    }

    ~MappedHeightField() {
        if (m_data) munmap(m_data, m_size);
        if (m_file >= 0) close(m_file);
    }

private:
    int m_file = -1;
    size_t m_size = 0;
    void* m_data = nullptr;
};
```

### Bulk Body Creation

```cpp
std::vector<JPH::BodyID> CreateBulkBodies(
    JPH::PhysicsSystem& physics,
    const std::span<const glm::mat4> transforms,
    JPH::ShapeRefC shape,
    float mass
) {
    JPH::BodyInterface& interface = physics.GetBodyInterfaceNoLock();

    std::vector<JPH::BodyCreationSettings> settings;
    settings.reserve(transforms.size());

    for (const auto& transform : transforms) {
        JPH::BodyCreationSettings s;
        s.SetShape(shape.Get());
        s.mPosition = JPH::RVec3(transform[3].x, transform[3].y, transform[3].z);

        glm::quat rot = glm::quat_cast(transform);
        s.mRotation = JPH::Quat(rot.x, rot.y, rot.z, rot.w);

        s.mMotionType = JPH::EMotionType::Dynamic;
        s.mMass = mass;

        settings.push_back(s);
    }

    return interface.CreateAndAddBodies(settings, JPH::EActivation::Activate);
}
```

---

## GPU-Driven физика

### Transform буфер для GPU

```cpp
class GPUTransformBuffer {
public:
    GPUTransformBuffer(VmaAllocator allocator, uint32_t max_bodies)
        : m_allocator(allocator), m_max_bodies(max_bodies)
    {
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = max_bodies * sizeof(glm::mat4);
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        vmaCreateBuffer(m_allocator, &buffer_info, &alloc_info, &m_buffer, &m_allocation, nullptr);
    }

    void Update(JPH::PhysicsSystem& physics, VkCommandBuffer cmd, VkBuffer staging) {
        JPH::BodyInterface& interface = physics.GetBodyInterfaceNoLock();

        JPH::BodyIDVector bodies;
        physics.GetBodies(bodies);

        std::vector<glm::mat4> transforms;
        transforms.reserve(bodies.size());

        for (JPH::BodyID id : bodies) {
            if (!interface.IsActive(id)) continue;

            JPH::RVec3 pos = interface.GetCenterOfMassPosition(id);
            JPH::Quat rot = interface.GetRotation(id);

            transforms.push_back(
                glm::translate(glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ()))
                * glm::mat4_cast(glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ()))
            );
        }

        void* data;
        vmaMapMemory(m_allocator, m_staging_allocation, &data);
        std::memcpy(data, transforms.data(), transforms.size() * sizeof(glm::mat4));
        vmaUnmapMemory(m_allocator, m_staging_allocation);

        VkBufferCopy region = {.size = transforms.size() * sizeof(glm::mat4)};
        vkCmdCopyBuffer(cmd, m_staging_buffer, m_buffer, 1, &region);
    }

    VkBuffer GetBuffer() const { return m_buffer; }

private:
    VmaAllocator m_allocator;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    uint32_t m_max_bodies;
};
```

### Indirect Drawing

```cpp
class PhysicsIndirectRenderer {
public:
    void RecordDrawCommands(VkCommandBuffer cmd, const GPUTransformBuffer& transforms) {
        VkBufferAddress addr = transforms.GetBufferAddress();

        VkDrawIndirectCommand draw = {};
        draw.vertexCount = 3;
        draw.instanceCount = m_active_count;
        draw.firstInstance = 0;

        vkCmdPushConstants(cmd, m_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(addr), &addr);
    }

private:
    uint32_t m_active_count = 0;
};
```

---

## Интеграция с Flecs Job System

### Параллельная обработка

```cpp
#include <flecs.h>

void CreateParallelPhysicsSystems(flecs::world& world, JPH::PhysicsSystem& physics) {
    // Система для параллельного обновления динамики
    world.system<PhysicsBody, Transform>("ParallelDynamics")
        .kind(flecs::PostUpdate)
        .each([&physics](PhysicsBody& body, Transform& transform) {
            // Обработка каждого тела
        });
}
```

### ECS Observer для физических событий

```cpp
class PhysicsEventBridge {
public:
    PhysicsEventBridge(flecs::world& world, JPH::PhysicsSystem& physics)
        : m_world(world), m_physics(physics)
    {
        m_physics.SetContactListener(this);
    }

    void OnContactAdded(
        const JPH::Body& body1,
        const JPH::Body& body2,
        const JPH::ContactManifold& manifold,
        JPH::ContactSettings& settings) override
    {
        JPH::uint32 id1 = body1.GetID().GetIndex();
        JPH::uint32 id2 = body2.GetID().GetIndex();

        m_world.event().emit<ContactEvent>(id1, id2);
    }

private:
    flecs::world& m_world;
    JPH::PhysicsSystem& m_physics;
};

struct ContactEvent {
    uint32_t body1;
    uint32_t body2;
};
```

---

## Профилирование с Tracy

### Интеграция

```cpp
#include <tracy/Tracy.hpp>

class ProfiledPhysics {
public:
    void Update(JPH::PhysicsSystem& physics, float dt) {
        ZoneScopedN("PhysicsUpdate");

        JPH::JobHandle job;
        {
            ZoneNamedN(zone, "CreateJobs", true);
            job = physics.Update(dt, 1, m_temp_allocator.get(), m_job_system.get());
        }

        {
            ZoneNamedN(zone, "WaitForJobs", true);
            physics.GetJobSystem()->WaitForJob(job);
        }

        FrameMarkN("PhysicsFrame");
    }
};
```

### Кастомные зоны

```cpp
void ProfileBodySync(JPH::BodyInterface& interface, JPH::BodyID id) {
    ZoneScopedN("BodySync");

    JPH::RVec3 pos = interface.GetCenterOfMassPosition(id);
    JPH::Quat rot = interface.GetRotation(id);
    JPH::Vec3 vel = interface.GetLinearVelocity(id);

    TracyPlot("BodyPosX", pos.GetX());
    TracyPlot("BodyPosY", pos.GetY());
    TracyPlot("BodyPosZ", pos.GetZ());
    TracyPlot("BodyVel", vel.Length());
}
```

---

## Оптимизация коллизий

### Spatial Hash Grid

```cpp
class SpatialHashGrid {
public:
    void Insert(JPH::BodyID id, const JPH::AABox& bounds) {
        int32_t min_x = static_cast<int32_t>(bounds.mMin.GetX() / m_cell_size);
        int32_t min_z = static_cast<int32_t>(bounds.mMin.GetZ() / m_cell_size);
        int32_t max_x = static_cast<int32_t>(bounds.mMax.GetX() / m_cell_size);
        int32_t max_z = static_cast<int32_t>(bounds.mMax.GetZ() / m_cell_size);

        for (int32_t x = min_x; x <= max_x; ++x) {
            for (int32_t z = min_z; z <= max_z; ++z) {
                m_grid[GetKey(x, z)].push_back(id);
            }
        }
    }

    std::vector<JPH::BodyID> Query(const JPH::AABox& bounds) {
        std::vector<JPH::BodyID> results;

        int32_t min_x = static_cast<int32_t>(bounds.mMin.GetX() / m_cell_size);
        int32_t min_z = static_cast<int32_t>(bounds.mMin.GetZ() / m_cell_size);
        int32_t max_x = static_cast<int32_t>(bounds.mMax.GetX() / m_cell_size);
        int32_t max_z = static_cast<int32_t>(bounds.mMax.GetZ() / m_cell_size);

        for (int32_t x = min_x; x <= max_x; ++x) {
            for (int32_t z = min_z; z <= max_z; ++z) {
                auto it = m_grid.find(GetKey(x, z));
                if (it != m_grid.end()) {
                    results.insert(results.end(), it->second.begin(), it->second.end());
                }
            }
        }

        return results;
    }

private:
    uint64_t GetKey(int32_t x, int32_t z) const {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint32_t>(z);
    }

    float m_cell_size = 10.0f;
    std::unordered_map<uint64_t, std::vector<JPH::BodyID>> m_grid;
};
```

### Object Pool для тел

```cpp
class BodyPool {
public:
    BodyPool(JPH::PhysicsSystem& physics) : m_physics(physics) {}

    JPH::BodyID Acquire() {
        if (!m_free_list.empty()) {
            JPH::BodyID id = m_free_list.back();
            m_free_list.pop_back();
            return id;
        }
        return JPH::BodyID::sInvalid;
    }

    void Release(JPH::BodyID id) {
        m_free_list.push_back(id);
    }

private:
    JPH::PhysicsSystem& m_physics;
    std::vector<JPH::BodyID> m_free_list;
};
```

---

## Алерты и best practices

### Правило 1: Избегайте блокировок - используйте batch операции

```cpp
// Плохо: последовательные вызовы с потенциальными конфликтами
for (auto& body : bodies) {
    interface.SetPosition(body.id, body.pos);  // Может вызывать внутренние блокировки
}

// Хорошо: batch операция без блокировок
std::vector<JPH::BodyID> ids;
std::vector<JPH::RVec3> positions;
ids.reserve(bodies.size());
positions.reserve(bodies.size());

for (auto& body : bodies) {
    ids.push_back(body.id);
    positions.push_back(body.pos);
}

interface.SetPositions(ids, positions);  // Один атомарный вызов
```

**Важно:** В ProjectV запрещены `std::mutex`, `std::scoped_lock`, `std::lock_guard`. Используйте:

1. **Batch операции** - как показано выше
2. **Lock-free структуры** - атомарные операции
3. **Thread-local данные** - каждый поток работает со своей копией
4. **Job System** - stdexec для асинхронной обработки

### Правило 2: Batch операции

```cpp
// Плохо
for (auto& settings : all_settings) {
    interface.CreateAndAddBody(settings, activation);
}

// Хорошо
interface.CreateAndAddBodies(all_settings, activation);
```

### Правило 3: Избегайте pointer chasing

```cpp
// Плохо: указатель на данные
JPH::Body* body = interface.GetBody(id);
glm::vec3 pos = glm::vec3(body->GetPosition().GetX(), ...);

// Хорошо: прямая работа с ID
JPH::RVec3 pos = interface.GetPosition(id);
```

### Правило 4: Предварительное выделение

```cpp
// Предварительно распределяем память
m_position_buffer.reserve(max_bodies);
m_velocity_buffer.reserve(max_bodies);
```

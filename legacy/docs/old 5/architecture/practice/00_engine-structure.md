# Структура движка ProjectV [🟢 Уровень 1]

**🟢 Уровень 1: Базовый** — Организация кодовой базы ProjectV.

## Обзор

ProjectV следует **Data-Oriented Design (DOD)** и **Entity-Component-System (ECS)** архитектуре. Код организован по
слоям: от низкоуровневого ядра до высокоуровневой игровой логики.

---

## Структура директорий

```
src/
├── core/                    # Ядро движка (низкий уровень)
│   ├── memory/              # Аллокаторы, memory arenas
│   ├── containers/          # DOD контейнеры (PodVector, FlatMap)
│   ├── jobs/                # Job System (M:N threading)
│   ├── io/                  # Файловая система, сериализация
│   └── platform/            # Платформозависимый код
│
├── render/                  # Рендеринг (Vulkan 1.4)
│   ├── device/              # Vulkan device, queues, command buffers
│   ├── pipeline/            # Pipeline layouts, shader modules
│   ├── passes/              # Render passes (gbuffer, deferred, etc.)
│   ├── resources/           # Buffers, images, descriptors
│   └── svo/                 # SVO renderer (ray marching, mesh gen)
│
├── voxel/                   # Воксельная система
│   ├── svo/                 # Sparse Voxel Octree
│   ├── chunk/               # Управление чанками
│   ├── mesh/                # Генерация mesh'ей
│   └── editing/             # Редактирование вокселей
│
├── physics/                 # Физика (JoltPhysics)
│   ├── world/               # Физический мир
│   ├── shapes/              # Коллайдеры
│   └── queries/             # Raycasts, overlaps
│
├── gameplay/                # Игровая логика (ECS)
│   ├── components/          # ECS компоненты (data only)
│   ├── systems/             # ECS системы (logic)
│   ├── blocks/              # Типы блоков
│   └── player/              # Игрок
│
├── audio/                   # Аудио (miniaudio)
│   ├── engine/              # Audio engine
│   ├── sources/             # Audio sources
│   └── listener/            # Audio listener
│
├── ui/                      # Пользовательский интерфейс
│   ├── imgui/               # Debug UI
│   └── game/                # Game UI
│
└── main.cpp                 # Точка входа
```

---

## Слои архитектуры

```
┌─────────────────────────────────────────────────────────────────┐
│                      Gameplay Layer                              │
│  (Components, Systems, Blocks, Player)                         │
├─────────────────────────────────────────────────────────────────┤
│                       Voxel Layer                                │
│  (SVO, Chunks, Mesh Generation, Editing)                       │
├─────────────────────────────────────────────────────────────────┤
│                      Physics Layer                               │
│  (JoltPhysics, Colliders, Queries)                             │
├─────────────────────────────────────────────────────────────────┤
│                      Render Layer                                │
│  (Vulkan Device, Pipelines, Passes, Resources)                 │
├─────────────────────────────────────────────────────────────────┤
│                       Core Layer                                 │
│  (Memory, Containers, Jobs, IO, Platform)                      │
└─────────────────────────────────────────────────────────────────┘
```

---

## 1. Core Layer

### 1.1 Memory (`src/core/memory/`)

```cpp
// memory_arena.hpp
namespace projectv::core {

// Linear allocator для временных данных
class LinearArena {
public:
    explicit LinearArena(size_t capacity);
    ~LinearArena();

    // No-copy
    LinearArena(const LinearArena&) = delete;
    LinearArena& operator=(const LinearArena&) = delete;

    void* allocate(size_t size, size_t alignment = 8);
    void reset();  // Освободить всё сразу

    size_t capacity() const { return capacity_; }
    size_t used() const { return offset_; }

private:
    std::byte* buffer_;
    size_t capacity_;
    size_t offset_;
};

// Pool allocator для объектов одинакового размера
template<typename T>
class PoolAllocator {
public:
    explicit PoolAllocator(size_t capacity);

    T* allocate();
    void deallocate(T* ptr);

private:
    std::unique_ptr<std::byte[]> buffer_;
    std::vector<T*> freeList_;
};

} // namespace projectv::core
```

### 1.2 Containers (`src/core/containers/`)

```cpp
// pod_vector.hpp
namespace projectv::core {

// Вектор для POD типов без инициализации
template<typename T>
requires std::is_trivially_copyable_v<T>
class PodVector {
public:
    PodVector() = default;
    explicit PodVector(size_t count);
    PodVector(const PodVector&) = delete;
    PodVector(PodVector&& other) noexcept;
    ~PodVector();

    T* data() { return data_; }
    const T* data() const { return data_; }
    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }

    T& operator[](size_t i) { return data_[i]; }
    const T& operator[](size_t i) const { return data_[i]; }

    void push_back(const T& value);
    void push_back(T&& value);
    void pop_back();
    void resize(size_t newCount);
    void reserve(size_t newCapacity);
    void clear();

    // Итераторы
    T* begin() { return data_; }
    T* end() { return data_ + size_; }
    const T* begin() const { return data_; }
    const T* end() const { return data_ + size_; }

private:
    T* data_ = nullptr;
    size_t size_ = 0;
    size_t capacity_ = 0;
};

} // namespace projectv::core
```

### 1.3 Job System (`src/core/jobs/`)

```cpp
// job_system.hpp
namespace projectv::core {

using JobFunc = std::move_only_function<void()>;

class JobSystem {
public:
    explicit JobSystem(uint32_t threadCount = 0);  // 0 = auto
    ~JobSystem();

    // Добавить задачу
    void schedule(JobFunc job);

    // Параллельное выполнение диапазона
    void parallelFor(size_t count, std::function<void(size_t)> func);

    // Ожидание завершения всех задач
    void wait();

    uint32_t threadCount() const { return workers_.size(); }

private:
    std::vector<std::thread> workers_;
    std::queue<JobFunc> jobs_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_{true};
    std::atomic<uint32_t> activeJobs_{0};
};

} // namespace projectv::core
```

---

## 2. Render Layer

### 2.1 Device (`src/render/device/`)

```cpp
// vulkan_device.hpp
namespace projectv::render {

struct VulkanDevice {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue computeQueue;
    VkQueue transferQueue;
    uint32_t graphicsFamily;
    uint32_t computeFamily;
    uint32_t transferFamily;
    VmaAllocator allocator;
};

class VulkanDeviceManager {
public:
    std::expected<VulkanDevice, InitError> initialize(
        SDL_Window* window,
        const std::vector<const char*>& requiredExtensions);

    void destroy(VulkanDevice& device);

private:
    VkDebugUtilsMessengerEXT debugMessenger_;

    std::expected<VkInstance, InitError> createInstance(
        const std::vector<const char*>& extensions);

    std::expected<VkPhysicalDevice, InitError> selectPhysicalDevice(
        VkInstance instance);

    std::expected<VkDevice, InitError> createLogicalDevice(
        VkPhysicalDevice physicalDevice,
        const std::vector<uint32_t>& queueFamilies);
};

} // namespace projectv::render
```

### 2.2 Pipeline (`src/render/pipeline/`)

```cpp
// pipeline_builder.hpp
namespace projectv::render {

class PipelineBuilder {
public:
    PipelineBuilder& addShaderStage(VkShaderStageFlagBits stage,
                                     VkShaderModule module,
                                     const char* entryPoint = "main");

    PipelineBuilder& setVertexInput(
        const std::vector<VkVertexInputBindingDescription>& bindings,
        const std::vector<VkVertexInputAttributeDescription>& attributes);

    PipelineBuilder& setInputAssembly(VkPrimitiveTopology topology);
    PipelineBuilder& setRasterization(VkPolygonMode mode);
    PipelineBuilder& setMultisampling(VkSampleCountFlagBits samples);
    PipelineBuilder& setDepthStencil(bool depthTest, bool depthWrite);
    PipelineBuilder& addColorAttachment(VkFormat format, bool blend = false);
    PipelineBuilder& setLayout(VkPipelineLayout layout);
    PipelineBuilder& setRenderPass(VkRenderPass renderPass);

    std::expected<VkPipeline, InitError> build(VkDevice device);

private:
    // ... internal state
};

} // namespace projectv::render
```

---

## 3. Voxel Layer

### 3.1 SVO (`src/voxel/svo/`)

```cpp
// svo_tree.hpp
namespace projectv::voxel {

struct SVONode {
    uint64_t data;
    // Методы для работы с полями (см. 00_svo-architecture.md)
};

class SVOTree {
public:
    SVOTree(uint32_t maxDepth = 10);

    // Получение вокселя по координатам
    std::expected<VoxelData, VoxelError> get(glm::ivec3 coord) const;

    // Установка вокселя
    std::expected<void, VoxelError> set(glm::ivec3 coord, VoxelData data);

    // Построение из плотного массива
    void build(const std::vector<VoxelData>& denseData,
               uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ);

    // DAG сжатие
    void compressDAG();

    // Загрузка на GPU
    void uploadToGPU(VkDevice device, VmaAllocator allocator);

    // Статистика
    size_t nodeCount() const { return nodes_.size(); }
    size_t voxelCount() const { return voxels_.size(); }
    size_t memoryUsage() const;

private:
    std::vector<SVONode> nodes_;
    std::vector<VoxelData> voxels_;
    uint32_t maxDepth_;
    uint32_t rootNode_;

    // GPU data
    VkBuffer nodeBuffer_;
    VmaAllocation nodeAlloc_;
    VkBuffer voxelBuffer_;
    VmaAllocation voxelAlloc_;
};

} // namespace projectv::voxel
```

### 3.2 Chunk Management (`src/voxel/chunk/`)

```cpp
// chunk_manager.hpp
namespace projectv::voxel {

struct ChunkCoord {
    int32_t x, y, z;

    bool operator==(const ChunkCoord& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct ChunkCoordHash {
    size_t operator()(const ChunkCoord& c) const {
        return std::hash<int64_t>{}(
            (int64_t(c.x) << 32) |
            (int64_t(c.y) << 16) |
            int64_t(c.z));
    }
};

class ChunkManager {
public:
    explicit ChunkManager(flecs::world& ecs, uint32_t loadRadius = 10);

    // Обновление при перемещении камеры
    void update(glm::vec3 cameraPosition);

    // Получение чанка
    std::expected<SVOTree*, ChunkError> getChunk(ChunkCoord coord);

    // Проверка существования
    bool hasChunk(ChunkCoord coord) const;

    // Создание чанка
    SVOTree& createChunk(ChunkCoord coord);

    // Удаление чанка
    void destroyChunk(ChunkCoord coord);

    // Количество загруженных чанков
    size_t chunkCount() const { return chunks_.size(); }

private:
    flecs::world& ecs_;
    uint32_t loadRadius_;
    glm::ivec3 lastCameraChunk_;

    std::unordered_map<ChunkCoord, SVOTree, ChunkCoordHash> chunks_;

    void loadChunk(ChunkCoord coord);
    void unloadDistantChunks(glm::ivec3 cameraChunk);
};

} // namespace projectv::voxel
```

---

## 4. Physics Layer

### 4.1 Physics World (`src/physics/world/`)

```cpp
// physics_world.hpp
namespace projectv::physics {

class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    // Шаг симуляции
    void step(float deltaTime);

    // Создание тел
    JPH::BodyID createStaticBody(
        const JPH::RefConst<JPH::Shape>& shape,
        glm::vec3 position);

    JPH::BodyID createDynamicBody(
        const JPH::RefConst<JPH::Shape>& shape,
        glm::vec3 position,
        float mass);

    // Удаление тела
    void destroyBody(JPH::BodyID id);

    // Запросы
    std::expected<RaycastResult, PhysicsError> raycast(
        glm::vec3 origin,
        glm::vec3 direction,
        float maxDistance);

    std::vector<OverlapResult> overlapSphere(
        glm::vec3 center,
        float radius);

    // Управление
    void setBodyPosition(JPH::BodyID id, glm::vec3 position);
    void setBodyVelocity(JPH::BodyID id, glm::vec3 velocity);
    glm::vec3 getBodyPosition(JPH::BodyID id) const;

private:
    JPH::PhysicsSystem* system_;
    JPH::JobSystemThreadPool* jobSystem_;
    JPH::TempAllocatorImpl* tempAllocator_;
};

} // namespace projectv::physics
```

---

## 5. Gameplay Layer (ECS)

### 5.1 Components (`src/gameplay/components/`)

```cpp
// components.hpp
namespace projectv::gameplay {

// Transform
struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    struct glaze {
        using T = TransformComponent;
        static constexpr auto value = glz::object(
            "position", &T::position,
            "rotation", &T::rotation,
            "scale", &T::scale
        );
    };
};

// Velocity
struct VelocityComponent {
    glm::vec3 linear{0.0f};
    glm::vec3 angular{0.0f};
};

// Physics
struct PhysicsBodyComponent {
    JPH::BodyID bodyId;
    bool isDynamic{true};
};

// Render
struct MeshComponent {
    std::shared_ptr<render::Mesh> mesh;
    std::string meshName;  // Для hot reload
};

struct MaterialComponent {
    std::shared_ptr<render::Material> material;
    std::string materialName;
};

// Voxel
struct VoxelChunkComponent {
    voxel::ChunkCoord coord;
    std::shared_ptr<voxel::SVOTree> svo;
    bool needsRebuild{false};
};

// Player
struct PlayerComponent {
    float moveSpeed{5.0f};
    float jumpForce{8.0f};
    float mouseSensitivity{0.1f};
    bool isGrounded{false};
};

// Block
struct BlockTypeComponent {
    uint16_t blockId;
    bool isSolid{true};
    bool isTransparent{false};
    float hardness{1.0f};
};

} // namespace projectv::gameplay
```

### 5.2 Systems (`src/gameplay/systems/`)

```cpp
// systems.hpp
namespace projectv::gameplay {

void registerSystems(flecs::world& ecs) {
    // Transform system
    ecs.system<TransformComponent, VelocityComponent>("UpdateTransforms")
        .multi_threaded()
        .each([](TransformComponent& t, const VelocityComponent& v) {
            t.position += v.linear * 0.016f;
            // Rotation...
        });

    // Physics sync system
    ecs.system<TransformComponent, PhysicsBodyComponent>("SyncPhysics")
        .kind(flecs::PreUpdate)
        .iter([](flecs::iter& it, TransformComponent* t, PhysicsBodyComponent* p) {
            auto* physics = it.world().ctx<physics::PhysicsWorld>();

            for (auto i : it) {
                t[i].position = physics->getBodyPosition(p[i].bodyId);
            }
        });

    // Frustum culling
    ecs.system<TransformComponent, MeshComponent, VisibleComponent>("FrustumCull")
        .kind(flecs::PreUpdate)
        .iter([](flecs::iter& it, TransformComponent* t, MeshComponent* m, VisibleComponent* v) {
            auto* camera = it.world().ctx<Camera>();
            Frustum frustum = camera->getFrustum();

            for (auto i : it) {
                v[i].inFrustum = frustum.intersects(m[i].mesh->bounds, t[i].position);
            }
        });

    // ... more systems
}

} // namespace projectv::gameplay
```

---

## 6. Точка входа

```cpp
// main.cpp
#include <SDL3/SDL.h>
#include <flecs.h>

#include "core/jobs/job_system.hpp"
#include "render/device/vulkan_device.hpp"
#include "voxel/chunk/chunk_manager.hpp"
#include "physics/world/physics_world.hpp"
#include "gameplay/components.hpp"
#include "gameplay/systems.hpp"

int main(int argc, char* argv[]) {
    // 1. Core initialization
    projectv::core::JobSystem jobSystem;

    // 2. SDL initialization
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::cerr << "SDL_Init failed" << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "ProjectV",
        1920, 1080,
        SDL_WINDOW_VULKAN
    );

    // 3. Vulkan initialization
    projectv::render::VulkanDeviceManager deviceManager;
    auto device = deviceManager.initialize(window, {});
    if (!device) {
        std::cerr << "Vulkan init failed" << std::endl;
        return 1;
    }

    // 4. ECS initialization
    flecs::world ecs;

    // Register components
    ecs.component<projectv::gameplay::TransformComponent>();
    ecs.component<projectv::gameplay::VelocityComponent>();
    // ...

    // Register systems
    projectv::gameplay::registerSystems(ecs);

    // 5. Subsystems initialization
    auto physicsWorld = std::make_unique<projectv::physics::PhysicsWorld>();
    auto chunkManager = std::make_unique<projectv::voxel::ChunkManager>(ecs);

    ecs.set<projectv::physics::PhysicsWorld*>(physicsWorld.get());
    ecs.set<projectv::voxel::ChunkManager*>(chunkManager.get());

    // 6. Main loop
    bool running = true;
    Uint64 lastTime = SDL_GetTicks();

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        float deltaTime = (SDL_GetTicks() - lastTime) / 1000.0f;
        lastTime = SDL_GetTicks();

        // Update physics
        physicsWorld->step(deltaTime);

        // Update ECS
        ecs.progress(deltaTime);

        // Render
        // ...
    }

    // 7. Cleanup
    deviceManager.destroy(*device);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
```

---

## Ссылки

- [DOD Philosophy](../../philosophy/03_dod-philosophy.md)
- [ECS Philosophy](../../philosophy/04_ecs-philosophy.md)
- [SVO Architecture](./00_svo-architecture.md)
- [Flecs Patterns](../../libraries/flecs/10_projectv-patterns.md)

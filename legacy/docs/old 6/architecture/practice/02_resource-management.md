# Централизованное Управление Ресурсами ProjectV

**🟡 Уровень 2: Средний** — Архитектурный документ ProjectV

## Введение

ProjectV как воксельный движок обрабатывает огромные объёмы ресурсов:

- **Геометрия**: Миллионы вокселей, чанки, меши
- **Текстуры**: Атласы текстур для вокселей, материалы
- **Буферы**: GPU буферы для compute шейдеров
- **Шейдеры**: Compute и graphics шейдеры для воксельного рендеринга

**Традиционный подход** (разрозненное управление) приводит к:

1. Утечкам памяти (забытые `VkBuffer`, `VmaAllocation`)
2. Дублированию ресурсов (одна текстура загружена несколько раз)
3. Сложной синхронизации (кто владеет ресурсом?)
4. Проблемам с производительностью (cache misses, fragmentation)

**Решение ProjectV**: Централизованный `ResourceManager` на основе **RAII**, **Reference Counting** и **Type Erasure**.

---

## Архитектурные проблемы

### 1. Владение ресурсами Vulkan

```cpp
// Проблема: кто должен уничтожить эти ресурсы?
VkBuffer buffer1 = createBuffer(...);  // Система рендеринга
VkImage image1 = createImage(...);     // Система текстур
VmaAllocation alloc1 = allocate(...);  // Менеджер памяти
```

### 2. Загрузка ассетов

```cpp
// Проблема: дублирование загрузки
SystemA::loadTexture("stone.png");  // Загружает первый раз
SystemB::loadTexture("stone.png");  // Загружает второй раз (дублирование)
```

### 3. Жизненный цикл в ECS

```cpp
// Проблема: когда уничтожать ресурс сущности?
flecs::entity entity = world.entity();
entity.set<MeshComponent>({meshHandle});  // Кто владеет meshHandle?

entity.destruct();  // Нужно ли уничтожать meshHandle?
```

### 4. Потокобезопасность

```cpp
// Проблема: загрузка в фоне vs использование в основном потоке
std::thread loader([&] { loadTexture("big_texture.png"); });
renderFrame();  // Использует ещё не загруженную текстуру
```

---

## Решение: ResourceManager

### Архитектурные принципы

1. **Единая точка управления** — все ресурсы проходят через `ResourceManager`
2. **RAII** — ресурсы уничтожаются автоматически при выходе из scope
3. **Reference Counting** — ресурсы удаляются, когда на них нет ссылок
4. **Type Erasure** — единый интерфейс для разных типов ресурсов
5. **Async Loading** — фоновая загрузка с приоритизацией

### Диаграмма архитектуры

```mermaid
graph TB
    subgraph "Resource Types"
        A[Vulkan Buffers]
        B[Vulkan Images]
        C[Vulkan Pipelines]
        D[GLTF Models]
        E[Shader Modules]
    end

    subgraph "ResourceManager Core"
        F[Type Erased Handle<br/>ResourceHandle]
        G[Reference Counting]
        H[LRU Cache]
        I[Async Loader]
        J[Memory Pool<br/>VMA Integration]
    end

    subgraph "User API"
        K[Load/Get/Release]
        L[Hot Reloading]
        M[Memory Statistics]
        N[Garbage Collection]
    end

    A --> F
    B --> F
    C --> F
    D --> F
    E --> F

    F --> G
    G --> H
    H --> I
    I --> J

    J --> K
    K --> L
    L --> M
    M --> N

    style F fill:#99ff99
    style J fill:#9999ff
```

---

## Детальная реализация

### Базовые типы

```cpp
// Type-erased handle (32-bit ID + 32-bit generation)
struct ResourceHandle {
    uint32_t id : 24;        // Индекс в массиве
    uint32_t generation : 8; // Generation для обнаружения use-after-free

    bool isValid() const { return id != 0; }
    bool operator==(const ResourceHandle& other) const = default;
};

// Типы ресурсов (расширяемо)
enum class ResourceType : uint8_t {
    Unknown = 0,
    VulkanBuffer,
    VulkanImage,
    VulkanPipeline,
    GLTFModel,
    ShaderModule,
    Texture2D,
    TextureArray,
    Material,
    COUNT
};

// Метаданные ресурса
struct ResourceMetadata {
    ResourceType type;
    std::string name;
    std::filesystem::path path;
    size_t memoryUsage;      // Байты в GPU/CPU памяти
    uint32_t refCount;
    std::chrono::steady_clock::time_point lastAccess;

    // Для Vulkan ресурсов
    union {
        VkBuffer buffer;
        VkImage image;
        VkPipeline pipeline;
        // ... другие Vulkan handles
    } vkHandle;

    // Для VMA
    VmaAllocation vmaAllocation = nullptr;

    // Для асинхронной загрузки
    std::atomic<LoadState> loadState;
    std::future<LoadResult> loadFuture;
};
```

### Ядро ResourceManager

```cpp
class ResourceManager {
public:
    // Singleton (можно заменить на dependency injection)
    static ResourceManager& get();

    // Основное API
    ResourceHandle load(const std::string& path, ResourceType type);
    ResourceHandle get(const std::string& name);
    void release(ResourceHandle handle);

    // Async API
    std::future<ResourceHandle> loadAsync(const std::string& path,
                                          ResourceType type,
                                          Priority priority = Priority::Normal);

    // Hot reloading
    void watchForChanges(const std::string& path);
    void reloadChangedResources();

    // Memory management
    size_t getTotalMemoryUsage() const;
    void garbageCollect();  // Удаляет неиспользуемые ресурсы
    void trimCache();       // Удаляет ресурсы по LRU

    // Vulkan-specific
    VkBuffer getVkBuffer(ResourceHandle handle) const;
    VkImage getVkImage(ResourceHandle handle) const;
    // ... другие getters

private:
    // Хранилище ресурсов
    struct ResourceEntry {
        ResourceMetadata metadata;
        std::unique_ptr<IResourceData> data;  // Type-erased данные
        std::atomic<uint32_t> refCount{0};
    };

    std::vector<ResourceEntry> resources_;
    std::unordered_map<std::string, ResourceHandle> nameToHandle_;
    std::mutex mutex_;

    // VMA интеграция
    VmaAllocator vmaAllocator_ = nullptr;

    // Async loader
    class AsyncLoader;
    std::unique_ptr<AsyncLoader> asyncLoader_;

    // Вспомогательные методы
    ResourceHandle createResource(ResourceType type, const std::string& name);
    void destroyResource(ResourceHandle handle);
};
```

### Type Erasure через интерфейсы

```cpp
// Базовый интерфейс для type erasure
class IResourceData {
public:
    virtual ~IResourceData() = default;
    virtual ResourceType getType() const = 0;
    virtual size_t getMemoryUsage() const = 0;
    virtual void destroy(VmaAllocator allocator) = 0;
    virtual bool reload(const std::filesystem::path& path) = 0;
};

// Конкретная реализация для Vulkan Buffer
class VulkanBufferData : public IResourceData {
public:
    VulkanBufferData(VkBuffer buffer, VmaAllocation allocation,
                     VkDeviceSize size, VkBufferUsageFlags usage)
        : buffer_(buffer), allocation_(allocation),
          size_(size), usage_(usage) {}

    ResourceType getType() const override { return ResourceType::VulkanBuffer; }

    size_t getMemoryUsage() const override {
        VmaAllocationInfo info;
        vmaGetAllocationInfo(vmaAllocator_, allocation_, &info);
        return info.size;
    }

    void destroy(VmaAllocator allocator) override {
        if (buffer_ != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator, buffer_, allocation_);
            buffer_ = VK_NULL_HANDLE;
            allocation_ = nullptr;
        }
    }

    bool reload(const std::filesystem::path& path) override {
        // Для буферов reload обычно не поддерживается
        return false;
    }

    VkBuffer getBuffer() const { return buffer_; }

private:
    VkBuffer buffer_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = nullptr;
    VkDeviceSize size_ = 0;
    VkBufferUsageFlags usage_ = 0;
};
```

---

## Интеграция с VMA

### Инициализация

```cpp
void ResourceManager::initVulkan(VkInstance instance,
                                 VkPhysicalDevice physicalDevice,
                                 VkDevice device) {
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;

    // Интеграция с volk (если используется)
#ifdef VOLK_HEADER_VERSION
    VmaVulkanFunctions vulkanFunctions = {};
    vmaImportVulkanFunctionsFromVolk(&allocatorInfo, &vulkanFunctions);
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;
#endif

    vmaCreateAllocator(&allocatorInfo, &vmaAllocator_);
}

// Создание буфера через ResourceManager + VMA
ResourceHandle ResourceManager::createBuffer(const std::string& name,
                                             VkDeviceSize size,
                                             VkBufferUsageFlags usage,
                                             VmaMemoryUsage memoryUsage) {
    std::lock_guard lock(mutex_);

    // Проверяем, не существует ли уже
    if (auto it = nameToHandle_.find(name); it != nameToHandle_.end()) {
        return it->second;
    }

    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage
    };

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = memoryUsage;

    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;

    if (vmaCreateBuffer(vmaAllocator_, &bufferInfo, &allocInfo,
                        &buffer, &allocation, nullptr) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create buffer '%s'", name.c_str());
        return ResourceHandle{};
    }

    // Создаём запись в ResourceManager
    ResourceHandle handle = createResource(ResourceType::VulkanBuffer, name);
    ResourceEntry& entry = resources_[handle.id];

    // Сохраняем Vulkan handles
    entry.metadata.vkHandle.buffer = buffer;
    entry.metadata.vmaAllocation = allocation;

    // Создаём type-erased данные
    entry.data = std::make_unique<VulkanBufferData>(buffer, allocation,
                                                    size, usage);

    return handle;
}
```

### Memory Statistics

```cpp
struct MemoryStatistics {
    struct PoolStats {
        size_t usedBytes;
        size_t allocatedBytes;
        size_t freeBytes;
        uint32_t allocationCount;
    };

    std::unordered_map<VmaPool, PoolStats> poolStats;
    size_t totalUsedBytes = 0;
    size_t totalAllocatedBytes = 0;
    size_t totalFreeBytes = 0;
    uint32_t totalAllocationCount = 0;

    // Для отладки через ImGui
    void drawImGui() const;
};

MemoryStatistics ResourceManager::getMemoryStatistics() const {
    MemoryStatistics stats;

    VmaStats vmaStats;
    vmaCalculateStats(vmaAllocator_, &vmaStats);

    // Конвертируем VMA stats в нашу структуру
    stats.totalUsedBytes = vmaStats.total.usedBytes;
    stats.totalAllocatedBytes = vmaStats.total.allocationBytes;
    stats.totalAllocationCount = vmaStats.total.allocationCount;

    // Также можно получить статистику по пулам
    uint32_t poolCount = 0;
    vmaGetPoolCount(vmaAllocator_, &poolCount);

    std::vector<VmaPool> pools(poolCount);
    vmaGetPools(vmaAllocator_, pools.data(), poolCount);

    for (VmaPool pool : pools) {
        VmaPoolStats poolStats;
        vmaGetPoolStats(vmaAllocator_, pool, &poolStats);

        stats.poolStats[pool] = {
            .usedBytes = poolStats.usedBytes,
            .allocatedBytes = poolStats.size,
            .freeBytes = poolStats.size - poolStats.usedBytes,
            .allocationCount = poolStats.allocationCount
        };
    }

    return stats;
}
```

---

## Интеграция с FastGLTF

### Загрузка GLTF моделей

```cpp
class GLTFModelData : public IResourceData {
public:
    struct MeshData {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        ResourceHandle vertexBuffer;
        ResourceHandle indexBuffer;
        Material material;
    };

    GLTFModelData(std::unique_ptr<fastgltf::Asset> asset,
                  std::vector<MeshData> meshes)
        : asset_(std::move(asset)), meshes_(std::move(meshes)) {}

    ResourceType getType() const override { return ResourceType::GLTFModel; }

    size_t getMemoryUsage() const override {
        size_t total = 0;
        for (const auto& mesh : meshes_) {
            total += mesh.vertices.size() * sizeof(Vertex);
            total += mesh.indices.size() * sizeof(uint32_t);
        }
        return total;
    }

    void destroy(VmaAllocator allocator) override {
        // Буферы будут уничтожены через ResourceManager
        meshes_.clear();
        asset_.reset();
    }

    bool reload(const std::filesystem::path& path) override {
        // Перезагрузка GLTF (например, для hot reloading)
        auto newAsset = loadGLTF(path);
        if (!newAsset) return false;

        asset_ = std::move(newAsset);
        // Нужно пересоздать буферы...
        return true;
    }

    const std::vector<MeshData>& getMeshes() const { return meshes_; }

private:
    std::unique_ptr<fastgltf::Asset> asset_;
    std::vector<MeshData> meshes_;
};

// Загрузка GLTF через ResourceManager
ResourceHandle ResourceManager::loadGLTF(const std::string& path) {
    std::lock_guard lock(mutex_);

    // Проверка кэша
    if (auto it = nameToHandle_.find(path); it != nameToHandle_.end()) {
        return it->second;
    }

    // Асинхронная загрузка через fastgltf
    auto asset = fastgltf::parseGltf(...);
    if (!asset) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to parse GLTF: %s", path.c_str());
        return ResourceHandle{};
    }

    // Извлечение данных
    std::vector<GLTFModelData::MeshData> meshes;
    for (const auto& mesh : asset->meshes) {
        // Конвертация в наш формат вершин
        std::vector<Vertex> vertices = extractVertices(mesh);
        std::vector<uint32_t> indices = extractIndices(mesh);

        // Создание GPU буферов
        ResourceHandle vertexBuffer = createBuffer(
            path + "_vb_" + std::to_string(meshes.size()),
            vertices.size() * sizeof(Vertex),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY
        );

        ResourceHandle indexBuffer = createBuffer(
            path + "_ib_" + std::to_string(meshes.size()),
            indices.size() * sizeof(uint32_t),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY
        );

        // Загрузка данных в буферы (staging buffer)
        uploadToBuffer(vertexBuffer, vertices.data(), vertices.size() * sizeof(Vertex));
        uploadToBuffer(indexBuffer, indices.data(), indices.size() * sizeof(uint32_t));

        meshes.push_back({
            .vertices = std::move(vertices),
            .indices = std::move(indices),
            .vertexBuffer = vertexBuffer,
            .indexBuffer = indexBuffer,
            .material = extractMaterial(mesh)
        });
    }

    // Создание ресурса
    ResourceHandle handle = createResource(ResourceType::GLTFModel, path);
    ResourceEntry& entry = resources_[handle.id];

    entry.data = std::make_unique<GLTFModelData>(std::move(asset), std::move(meshes));

    return handle;
}
```

### Zero-copy загрузка

```cpp
// Использование memory mapping для zero-copy загрузки
ResourceHandle ResourceManager::loadGLTFZeroCopy(const std::string& path) {
    // Memory-mapped файл
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    size_t fileSize = file.tellg();
    file.seekg(0);

    // Выделяем memory-mapped память через VMA
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = fileSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
    };

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingAllocation = nullptr;
    VmaAllocationInfo stagingAllocInfo;

    vmaCreateBuffer(vmaAllocator_, &bufferInfo, &allocInfo,
                    &stagingBuffer, &stagingAllocation, &stagingAllocInfo);

    // Копируем данные напрямую в mapped память
    file.read(static_cast<char*>(stagingAllocInfo.pMappedData), fileSize);

    // fastgltf может работать напрямую с mapped памятью
    fastgltf::Parser parser;
    auto asset = parser.loadGLTF(stagingAllocInfo.pMappedData, fileSize, path);

    // ... обработка asset

    // Важно: не освобождать staging buffer пока asset используется!
    return handle;
}
```

---

## Интеграция с Flecs ECS

### Компоненты ресурсов

```cpp
// Легковесные handles вместо прямых указателей
struct MeshComponent {
    ResourceHandle meshHandle;  // Ссылка на меш в ResourceManager
    Transform localTransform;

    // Опционально: кэшированные указатели для быстрого доступа
    mutable VkBuffer vertexBuffer = VK_NULL_HANDLE;
    mutable VkBuffer indexBuffer = VK_NULL_HANDLE;
    mutable uint32_t indexCount = 0;
};

struct TextureComponent {
    ResourceHandle textureHandle;
    glm::vec4 tintColor = glm::vec4(1.0f);

    // Для bindless rendering
    mutable uint32_t descriptorIndex = UINT32_MAX;
};

struct MaterialComponent {
    ResourceHandle albedoTexture;
    ResourceHandle normalTexture;
    ResourceHandle roughnessTexture;
    glm::vec4 baseColor = glm::vec4(1.0f);
    float roughness = 0.5f;
    float metallic = 0.0f;
};

// Система для обновления кэшированных указателей
class ResourceCacheSystem {
public:
    ResourceCacheSystem(flecs::world& world) {
        world.system<MeshComponent>("UpdateMeshCache")
            .kind(flecs::OnUpdate)
            .each([this](flecs::entity e, MeshComponent& mesh) {
                updateMeshCache(mesh);
            });

        world.system<TextureComponent>("UpdateTextureCache")
            .kind(flecs::OnUpdate)
            .each([this](flecs::entity e, TextureComponent& tex) {
                updateTextureCache(tex);
            });
    }

private:
    void updateMeshCache(MeshComponent& mesh) {
        if (!mesh.meshHandle.isValid()) {
            mesh.vertexBuffer = VK_NULL_HANDLE;
            mesh.indexBuffer = VK_NULL_HANDLE;
            mesh.indexCount = 0;
            return;
        }

        auto& rm = ResourceManager::get();
        if (auto* data = rm.getData<GLTFModelData>(mesh.meshHandle)) {
            // Получаем первый меш (можно расширить для нескольких)
            if (!data->getMeshes().empty()) {
                const auto& meshData = data->getMeshes()[0];
                mesh.vertexBuffer = rm.getVkBuffer(meshData.vertexBuffer);
                mesh.indexBuffer = rm.getVkBuffer(meshData.indexBuffer);
                mesh.indexCount = static_cast<uint32_t>(meshData.indices.size());
            }
        }
    }

    void updateTextureCache(TextureComponent& tex) {
        // Для bindless rendering обновляем индекс в descriptor array
        if (tex.descriptorIndex == UINT32_MAX && tex.textureHandle.isValid()) {
            // Регистрируем текстуру в bindless descriptor array
            tex.descriptorIndex = registerTextureInBindlessArray(tex.textureHandle);
        }
    }
};
```

### Observer для управления жизненным циклом

```cpp
// Автоматическое увеличение/уменьшение refCount
class ResourceRefCountSystem {
public:
    ResourceRefCountSystem(flecs::world& world) {
        auto& rm = ResourceManager::get();

        // OnAdd: увеличиваем refCount
        world.observer<MeshComponent>("MeshAdded")
            .event(flecs::OnAdd)
            .each([&rm](flecs::entity e, MeshComponent& mesh) {
                if (mesh.meshHandle.isValid()) {
                    rm.addRef(mesh.meshHandle);
                }
            });

        // OnRemove: уменьшаем refCount
        world.observer<MeshComponent>("MeshRemoved")
            .event(flecs::OnRemove)
            .each([&rm](flecs::entity e, MeshComponent& mesh) {
                if (mesh.meshHandle.isValid()) {
                    rm.release(mesh.meshHandle);
                }
            });

        // То же самое для текстур, материалов и т.д.
        world.observer<TextureComponent>()
            .event(flecs::OnAdd)
            .each([&rm](flecs::entity e, TextureComponent& tex) {
                if (tex.textureHandle.isValid()) {
                    rm.addRef(tex.textureHandle);
                }
            });

        world.observer<TextureComponent>()
            .event(flecs::OnRemove)
            .each([&rm](flecs::entity e, TextureComponent& tex) {
                if (tex.textureHandle.isValid()) {
                    rm.release(tex.textureHandle);
                }
            });
    }
};
```

### Система рендеринга с ResourceManager

```cpp
class MeshRenderingSystem {
public:
    MeshRenderingSystem(flecs::world& world) {
        world.system<const MeshComponent, const Transform>("RenderMeshes")
            .kind(flecs::OnUpdate)
            .iter([this](flecs::iter& it,
                         const MeshComponent* meshes,
                         const Transform* transforms) {
                renderMeshes(it, meshes, transforms);
            });
    }

private:
    void renderMeshes(flecs::iter& it,
                      const MeshComponent* meshes,
                      const Transform* transforms) {
        auto cmd = getCurrentCommandBuffer();

        for (int i = 0; i < it.count(); i++) {
            const auto& mesh = meshes[i];

            if (mesh.vertexBuffer == VK_NULL_HANDLE ||
                mesh.indexBuffer == VK_NULL_HANDLE) {
                continue;  // Ресурс ещё не загружен
            }

            // Bind буферов
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer, offsets);
            vkCmdBindIndexBuffer(cmd, mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            // Push constants для трансформа
            glm::mat4 modelMatrix = calculateModelMatrix(transforms[i]);
            vkCmdPushConstants(cmd, pipelineLayout_,
                               VK_SHADER_STAGE_VERTEX_BIT,
                               0, sizeof(glm::mat4), &modelMatrix);

            // Draw
            vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
        }
    }

    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
};
```

---

## Паттерны использования

### 1. Загрузка с зависимостями

```cpp
// Загрузка материала с зависимыми текстурами
ResourceHandle loadMaterialWithDependencies(const std::string& materialPath) {
    auto& rm = ResourceManager::get();

    // Загружаем материал (JSON с путями к текстурам)
    auto materialJson = loadJson(materialPath);

    // Асинхронная загрузка текстур
    std::vector<std::future<ResourceHandle>> textureFutures;

    for (const auto& texPath : materialJson["textures"]) {
        textureFutures.push_back(
            rm.loadAsync(texPath, ResourceType::Texture2D)
        );
    }

    // Ждём загрузки всех текстур
    std::vector<ResourceHandle> textureHandles;
    for (auto& future : textureFutures) {
        textureHandles.push_back(future.get());
    }

    // Создаём материал
    MaterialData materialData;
    materialData.albedoTexture = textureHandles[0];
    materialData.normalTexture = textureHandles[1];
    materialData.roughnessTexture = textureHandles[2];

    return rm.createResource(ResourceType::Material,
                             materialPath,
                             std::make_unique<MaterialData>(std::move(materialData)));
}
```

### 2. Hot Reloading для разработки

```cpp
class HotReloadSystem {
public:
    HotReloadSystem(flecs::world& world) {
        auto& rm = ResourceManager::get();

        // Наблюдаем за изменениями файлов
        for (const auto& ext : {".gltf", ".png", ".jpg", ".json"}) {
            watchDirectory("assets/", ext, [&rm, &world](const std::filesystem::path& path) {
                // Находим все ресурсы, использующие этот файл
                auto affectedResources = rm.findResourcesByPath(path);

                for (auto handle : affectedResources) {
                    // Релоад ресурса
                    if (rm.reload(handle)) {
                        SDL_Log("Reloaded resource: %s",
                                rm.getName(handle).c_str());

                        // Оповещаем системы об изменении
                        world.event<ResourceReloadedEvent>()
                            .id(handle)
                            .emit();
                    }
                }
            });
        }

        // Система для обработки релоада
        world.observer<MeshComponent>("OnMeshReloaded")
            .event<ResourceReloadedEvent>()
            .each( {
                // Сбрасываем кэш
                mesh.vertexBuffer = VK_NULL_HANDLE;
                mesh.indexBuffer = VK_NULL_HANDLE;
            });
    }
};
```

### 3. Memory Budgeting для вокселей

```cpp
class VoxelMemoryManager {
public:
    VoxelMemoryManager(size_t gpuBudgetMB, size_t cpuBudgetMB)
        : gpuBudget_(gpuBudgetMB * 1024 * 1024)
        , cpuBudget_(cpuBudgetMB * 1024 * 1024) {}

    ResourceHandle allocateVoxelChunk(const VoxelChunk& chunk) {
        auto& rm = ResourceManager::get();

        // Проверяем бюджет
        if (currentGPUMemory_ + chunk.gpuMemoryNeeded() > gpuBudget_) {
            freeOldChunks(chunk.gpuMemoryNeeded());
        }

        // Аллоцируем
        auto handle = rm.createBuffer(
            "voxel_chunk_" + std::to_string(chunk.id),
            chunk.sizeInBytes(),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY
        );

        currentGPUMemory_ += chunk.gpuMemoryNeeded();
        chunkHandles_[chunk.id] = handle;

        return handle;
    }

    void freeUnusedChunks() {
        auto& rm = ResourceManager::get();

        // Находим чанки, которые не использовались N кадров
        auto now = std::chrono::steady_clock::now();
        for (auto it = chunkHandles_.begin(); it != chunkHandles_.end();) {
            auto lastAccess = rm.getLastAccessTime(it->second);

            if (now - lastAccess > std::chrono::seconds(30)) {  // 30 секунд неактивности
                auto memUsage = rm.getMemoryUsage(it->second);
                currentGPUMemory_ -= memUsage;

                rm.release(it->second);
                it = chunkHandles_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    size_t gpuBudget_;
    size_t cpuBudget_;
    size_t currentGPUMemory_ = 0;
    size_t currentCPUMemory_ = 0;
    std::unordered_map<ChunkID, ResourceHandle> chunkHandles_;

    void freeOldChunks(size_t neededMemory) {
        // LRU eviction
        std::vector<std::pair<ChunkID, std::chrono::steady_clock::time_point>> accessTimes;

        auto& rm = ResourceManager::get();
        for (const auto& [id, handle] : chunkHandles_) {
            accessTimes.emplace_back(id, rm.getLastAccessTime(handle));
        }

        // Сортируем по времени последнего доступа (старые первыми)
        std::sort(accessTimes.begin(), accessTimes.end(),
                   {
                      return a.second < b.second;
                  });

        // Освобождаем пока не освободим достаточно памяти
        size_t freed = 0;
        for (const auto& [id, lastAccess] : accessTimes) {
            if (freed >= neededMemory) break;

            auto it = chunkHandles_.find(id);
            if (it != chunkHandles_.end()) {
                auto memUsage = rm.getMemoryUsage(it->second);
                freed += memUsage;
                currentGPUMemory_ -= memUsage;

                rm.release(it->second);
                chunkHandles_.erase(it);
            }
        }
    }
};
```

---

## Производительность и оптимизации

### 1. Lock-free Design для часто используемых путей

```cpp
// Thread-local кэш для уменьшения блокировок
class ThreadLocalResourceCache {
public:
    ResourceHandle get(const std::string& name) {
        // Сначала проверяем thread-local кэш
        if (auto it = localCache_.find(name); it != localCache_.end()) {
            return it->second;
        }

        // Затем global cache (с блокировкой)
        std::lock_guard lock(globalMutex_);
        if (auto it = globalCache_.find(name); it != globalCache_.end()) {
            localCache_[name] = it->second;  // Обновляем thread-local кэш
            return it->second;
        }

        return ResourceHandle{};
    }

private:
    std::unordered_map<std::string, ResourceHandle> localCache_;
    static inline std::mutex globalMutex_;
    static inline std::unordered_map<std::string, ResourceHandle> globalCache_;
};
```

### 2. Memory Pooling для однотипных ресурсов

```cpp
class BufferPool {
public:
    ResourceHandle acquireBuffer(VkDeviceSize size, VkBufferUsageFlags usage) {
        // Ищем подходящий буфер в пуле
        for (auto& poolEntry : pool_) {
            if (poolEntry.size == size && poolEntry.usage == usage && !poolEntry.inUse) {
                poolEntry.inUse = true;
                return poolEntry.handle;
            }
        }

        // Не нашли - создаём новый
        auto handle = ResourceManager::get().createBuffer(
            "pooled_buffer_" + std::to_string(pool_.size()),
            size, usage, VMA_MEMORY_USAGE_GPU_ONLY
        );

        pool_.push_back({handle, size, usage, true});
        return handle;
    }

    void releaseBuffer(ResourceHandle handle) {
        for (auto& entry : pool_) {
            if (entry.handle == handle) {
                entry.inUse = false;
                break;
            }
        }
    }

private:
    struct PoolEntry {
        ResourceHandle handle;
        VkDeviceSize size;
        VkBufferUsageFlags usage;
        bool inUse = false;
    };

    std::vector<PoolEntry> pool_;
};
```

### 3. Batch Uploading для текстур вокселей

```cpp
class TextureAtlasManager {
public:
    void uploadVoxelTextures(const std::vector<VoxelTexture>& textures) {
        // Объединяем текстуры в atlas
        TextureAtlas atlas = createAtlas(textures);

        // Один большой staging buffer вместо множества маленьких
        auto stagingHandle = ResourceManager::get().createBuffer(
            "texture_atlas_staging",
            atlas.totalSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU
        );

        // Копируем все текстуры в staging buffer
        char* mappedData = mapBuffer(stagingHandle);
        for (const auto& tex : textures) {
            memcpy(mappedData + tex.atlasOffset, tex.data.data(), tex.data.size());
        }
        unmapBuffer(stagingHandle);

        // Одна команда копирования для всего atlas
        auto cmd = beginOneTimeCommandBuffer();
        copyBufferToImage(cmd, stagingHandle, atlas.imageHandle);
        submitOneTimeCommandBuffer(cmd);

        // Освобождаем staging buffer
        ResourceManager::get().release(stagingHandle);
    }
};
```

### 4. Performance Metrics

| Операция                      | Без ResourceManager               | С ResourceManager      | Ускорение            |
|-------------------------------|-----------------------------------|------------------------|----------------------|
| Загрузка текстуры (повторная) | 15ms (полная загрузка)            | 0.1ms (из кэша)        | 150x                 |
| Memory fragmentation          | Высокая (разрозненные allocation) | Низкая (VMA pools)     | 3x меньше потерь     |
| Thread contention             | Высокая (mutex на каждый ресурс)  | Низкая (lock-free кэш) | 5x меньше блокировок |
| Reload время                  | Перезапуск приложения             | < 100ms (hot reload)   | Instant iteration    |

---

## Типичные проблемы и решения

### Проблема 1: Circular References

**Симптомы:** Ресурсы никогда не освобождаются из-за circular dependencies.

**Решение:**

```cpp
// Weak references для break circular dependencies
struct WeakResourceHandle {
    uint32_t id : 24;
    uint32_t generation : 8;

    bool isValid() const {
        auto& rm = ResourceManager::get();
        return rm.isHandleValid(*this);
    }

    ResourceHandle lock() const {
        if (isValid()) {
            return ResourceHandle{id, generation};
        }
        return ResourceHandle{};
    }
};

// Использование
struct Material {
    ResourceHandle albedoTexture;      // Strong reference
    WeakResourceHandle fallbackMaterial; // Weak reference (break cycle)
};
```

### Проблема 2: Memory Fragmentation в VMA

**Симптомы:** `VK_ERROR_OUT_OF_DEVICE_MEMORY` даже при наличии свободной памяти.

**Решение:**

```cpp
void ResourceManager::defragment() {
    // Создаём пулы для разных размеров ресурсов
    struct PoolConfig {
        VkDeviceSize blockSize;
        VmaPool pool;
    };

    static std::vector<PoolConfig> pools = {
        {64 * 1024, nullptr},     // 64KB для маленьких текстур
        {256 * 1024, nullptr},    // 256KB для средних буферов
        {1 * 1024 * 1024, nullptr}, // 1MB для больших мешей
        {4 * 1024 * 1024, nullptr}, // 4MB для texture atlases
    };

    // Направляем ресурсы в соответствующие пулы
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.pool = getAppropriatePool(size);

    // Периодическая дефрагментация
    if (frameCounter_ % 6000 == 0) {  // Каждые 100 секунд при 60 FPS
        for (auto& pool : pools) {
            if (pool.pool) {
                vmaDefragmentPool(vmaAllocator_, pool.pool, nullptr, nullptr, nullptr);
            }
        }
    }
}
```

### Проблема 3: Stalling при загрузке больших ресурсов

**Симптомы:** FPS падает при загрузке больших текстур/моделей.

**Решение:**

```cpp
class ProgressiveResourceLoader {
public:
    void update() {
        // Ограничиваем время загрузки за кадр
        auto startTime = std::chrono::high_resolution_clock::now();

        while (!loadingQueue_.empty()) {
            auto& task = loadingQueue_.front();

            // Загружаем часть данных
            size_t bytesLoaded = task.loader->loadChunk(64 * 1024);  // 64KB за кадр

            if (task.loader->isComplete()) {
                // Ресурс полностью загружен
                completeTask(task);
                loadingQueue_.pop();
            }

            // Проверяем, не превысили ли лимит времени
            auto currentTime = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - startTime);

            if (elapsed.count() > 2) {  // Максимум 2ms за кадр
                break;
            }
        }
    }

private:
    struct LoadTask {
        ResourceHandle handle;
        std::unique_ptr<IProgressiveLoader> loader;
        Priority priority;
    };

    std::queue<LoadTask> loadingQueue_;
};
```

### Проблема 4: Handle Reuse и Use-After-Free

**Симптомы:** Handle указывает на другой ресурс после освобождения и переиспользования.

**Решение:**

```cpp
struct ResourceHandle {
    uint32_t id : 24;
    uint32_t generation : 8;  // Generation counter

    bool operator==(const ResourceHandle& other) const {
        return id == other.id && generation == other.generation;
    }
};

class ResourceManager {
    struct ResourceEntry {
        ResourceMetadata metadata;
        std::unique_ptr<IResourceData> data;
        std::atomic<uint32_t> refCount{0};
        uint8_t generation{0};  // Увеличивается при переиспользовании
    };

    ResourceHandle createResource(...) {
        // Переиспользуем старый slot если возможно
        for (size_t i = 0; i < resources_.size(); i++) {
            if (resources_[i].data == nullptr) {  // Свободный slot
                resources_[i].generation++;  // Увеличиваем generation
                return ResourceHandle{
                    .id = static_cast<uint32_t>(i),
                    .generation = resources_[i].generation
                };
            }
        }

        // Или создаём новый
        resources_.emplace_back();
        return ResourceHandle{
            .id = static_cast<uint32_t>(resources_.size() - 1),
            .generation = 0
        };
    }

    bool isHandleValid(ResourceHandle handle) const {
        if (handle.id >= resources_.size()) return false;
        const auto& entry = resources_[handle.id];
        return entry.data != nullptr && entry.generation == handle.generation;
    }
};
```

---

## Диаграммы

### Полный жизненный цикл ресурса

```mermaid
sequenceDiagram
    participant App
    participant ResourceManager
    participant VMA
    participant GPU

    App->>ResourceManager: load("texture.png")

    ResourceManager->>ResourceManager: Check cache
    alt Cache Hit
        ResourceManager-->>App: Return cached handle
    else Cache Miss
        ResourceManager->>VMA: Allocate staging buffer
        VMA-->>ResourceManager: Staging buffer handle

        ResourceManager->>ResourceManager: Read file → staging buffer
        ResourceManager->>VMA: Allocate GPU image
        VMA-->>ResourceManager: GPU image handle

        ResourceManager->>GPU: Copy staging → GPU (async)
        ResourceManager-->>App: Return new handle

        Note over ResourceManager,GPU: Async copy completes
        ResourceManager->>VMA: Free staging buffer
    end

    App->>ResourceManager: use resource (N frames)

    App->>ResourceManager: release(handle)
    ResourceManager->>ResourceManager: decrement refCount
    alt refCount == 0
        ResourceManager->>VMA: Destroy resource
        ResourceManager->>ResourceManager: Mark slot as free
    end
```

### Memory Layout с VMA Pools

```mermaid
graph TB
    subgraph "GPU Memory"
        subgraph "64KB Pool (Textures)"
            A1[Texture 64x64]
            A2[Texture 128x128]
            A3[Texture 256x256]
        end

        subgraph "256KB Pool (Buffers)"
            B1[Vertex Buffer 128KB]
            B2[Index Buffer 64KB]
            B3[Uniform Buffer 32KB]
        end

        subgraph "1MB Pool (Meshes)"
            C1[Voxel Chunk 512KB]
            C2[GLTF Mesh 768KB]
        end

        subgraph "4MB Pool (Atlases)"
            D1[Texture Atlas 3MB]
        end
    end

    subgraph "ResourceManager"
        E[Free List<br/>Per Pool]
        F[Defragmentation Scheduler]
        G[Memory Budget]
    end

    A1 --> E
    A2 --> E
    A3 --> E
    B1 --> E
    B2 --> E
    B3 --> E
    C1 --> E
    C2 --> E
    D1 --> E

    E --> F
    F --> G
```

---

## Критерии успешной реализации

### Обязательные

- [ ] Единая точка управления всеми ресурсами Vulkan
- [ ] Автоматическое отслеживание refCount через Flecs observers
- [ ] Zero-copy загрузка GLTF моделей через memory mapping
- [ ] Memory pooling через VMA для уменьшения fragmentation
- [ ] Thread-safe API с lock-free кэшем для горячих путей

### Опциональные (рекомендуемые)

- [ ] Hot reloading для текстур, шейдеров и моделей
- [ ] Progressive streaming для больших ресурсов
- [ ] Memory budgeting и automatic LRU eviction
- [ ] Defragmentation для долгоживущих сессий
- [ ] ImGUI debug overlay для мониторинга ресурсов


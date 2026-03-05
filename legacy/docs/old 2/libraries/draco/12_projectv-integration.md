# Интеграция Draco в ProjectV

🔴 **Уровень 3: Продвинутый**

Специфика интеграции Draco в воксельный движок ProjectV: работа с SVO, sparse data, интеграция с VMA и flecs.

## Роль Draco в ProjectV

Draco используется для:

1. **Сжатие воксельных чанков** — уменьшение размера при хранении и передаче
2. **Загрузка glTF моделей** — через KHR_draco_mesh_compression
3. **Сетевая синхронизация** — минимальный bandwidth для multiplayer

### Жизненный цикл данных

```
Воксельный чанк (SoA)
        ↓
VoxelMeshBuilder → draco::Mesh
        ↓
Draco Encoder
        ↓
Compressed Chunk (.drc)
        ↓
Network / Disk
        ↓
Draco Decoder
        ↓
draco::Mesh
        ↓
VMA / Vulkan Buffers
        ↓
ECS Components (flecs)
        ↓
Rendering System
```

---

## Воксельные чанки

### Структура данных чанка

```cpp
#include <draco/mesh/mesh.h>
#include <draco/attributes/geometry_attribute.h>

// SoA данные воксельного чанка
struct VoxelChunkData {
    std::vector<uint8_t> voxelTypes;      // Тип вокселя
    std::vector<uint8_t> occlusion;       // Ambient occlusion
    std::vector<uint16_t> blockLight;     // Уровень света
    // ... другие данные
};

// Конвертация в Draco Mesh
std::unique_ptr<draco::Mesh> voxelChunkToMesh(const VoxelChunkData& chunk) {
    auto mesh = std::make_unique<draco::Mesh>();

    // Для вокселей используем Point Cloud (без connectivity)
    // Или mesh с упрощённой геометрией для greedy meshing

    // Voxel type attribute
    draco::GeometryAttribute voxelTypeAttr;
    voxelTypeAttr.Init(
        draco::GeometryAttribute::GENERIC,
        nullptr, 1, draco::DT_UINT8, false, 1, 0
    );

    int voxelTypeAttrId = mesh->AddAttribute(voxelTypeAttr, true, chunk.voxelTypes.size());

    draco::PointAttribute* attr = mesh->attribute(voxelTypeAttrId);
    for (size_t i = 0; i < chunk.voxelTypes.size(); ++i) {
        attr->SetAttributeValue(draco::AttributeValueIndex(i), &chunk.voxelTypes[i]);
    }

    mesh->set_num_points(chunk.voxelTypes.size());

    return mesh;
}
```

### Квантование для вокселей

Воксели уже дискретны, но атрибуты могут требовать разной точности:

```cpp
draco::ExpertEncoder encoder(*mesh);

// Voxel type — 8 бит достаточно (256 типов)
encoder.SetQuantizationBitsForAttribute(voxelTypeAttrId, 8);

// Occlusion — 4 бита (16 уровней)
encoder.SetQuantizationBitsForAttribute(occlusionAttrId, 4);

// Light — 8 бит для точности
encoder.SetQuantizationBitsForAttribute(lightAttrId, 8);

// Скорость важна для real-time chunk loading
encoder.SetSpeedOptions(5, 10);
encoder.SetEncodingMethod(draco::POINT_CLOUD_SEQUENTIAL_ENCODING);
```

---

## Интеграция с VMA

### Прямая запись в GPU buffer

```cpp
#include <draco/compression/decode.h>
#include <vk_mem_alloc.h>

struct VulkanMeshData {
    VkBuffer vertexBuffer;
    VmaAllocation vertexAllocation;
    VkBuffer indexBuffer;
    VmaAllocation indexAllocation;
    uint32_t vertexCount;
    uint32_t indexCount;
};

class DracoVmaDecoder {
public:
    DracoVmaDecoder(VmaAllocator allocator) : allocator_(allocator) {}

    std::optional<VulkanMeshData> decodeToVulkan(
        const void* data, size_t size) {

        // Декодирование в CPU
        draco::DecoderBuffer buffer;
        buffer.Init(reinterpret_cast<const char*>(data), size);

        draco::Decoder decoder;
        auto result = decoder.DecodeMeshFromBuffer(&buffer);

        if (!result.ok()) return std::nullopt;

        auto mesh = std::move(result).value();

        VulkanMeshData vulkanData = {};

        // Создание vertex buffer
        if (!createVertexBuffer(*mesh, vulkanData)) return std::nullopt;

        // Создание index buffer
        if (!createIndexBuffer(*mesh, vulkanData)) return std::nullopt;

        return vulkanData;
    }

private:
    VmaAllocator allocator_;

    bool createVertexBuffer(const draco::Mesh& mesh, VulkanMeshData& out) {
        const auto* posAttr = mesh.GetNamedAttribute(draco::GeometryAttribute::POSITION);
        if (!posAttr) return false;

        const size_t vertexCount = mesh.num_points();
        const size_t vertexSize = sizeof(float) * 3;  // xyz
        const size_t bufferSize = vertexCount * vertexSize;

        VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                               VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocInfo;
        VkResult result = vmaCreateBuffer(allocator_, &bufferInfo, &allocCreateInfo,
            &out.vertexBuffer, &out.vertexAllocation, &allocInfo);

        if (result != VK_SUCCESS) return false;

        // Прямая запись в mapped memory
        float* dst = static_cast<float*>(allocInfo.pMappedData);
        for (draco::PointIndex i(0); i < vertexCount; ++i) {
            std::array<float, 3> pos;
            posAttr->GetValue(posAttr->mapped_index(i), &pos);
            *dst++ = pos[0];
            *dst++ = pos[1];
            *dst++ = pos[2];
        }

        out.vertexCount = static_cast<uint32_t>(vertexCount);
        return true;
    }

    bool createIndexBuffer(const draco::Mesh& mesh, VulkanMeshData& out) {
        const size_t indexCount = mesh.num_faces() * 3;
        const size_t bufferSize = indexCount * sizeof(uint32_t);

        VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                               VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocInfo;
        VkResult result = vmaCreateBuffer(allocator_, &bufferInfo, &allocCreateInfo,
            &out.indexBuffer, &out.indexAllocation, &allocInfo);

        if (result != VK_SUCCESS) return false;

        uint32_t* dst = static_cast<uint32_t*>(allocInfo.pMappedData);
        for (draco::FaceIndex f(0); f < mesh.num_faces(); ++f) {
            const auto& face = mesh.face(f);
            *dst++ = face[0].value();
            *dst++ = face[1].value();
            *dst++ = face[2].value();
        }

        out.indexCount = static_cast<uint32_t>(indexCount);
        return true;
    }
};
```

### Staging buffer для больших mesh

```cpp
// Для больших mesh используйте staging buffer
// См. docs/guides/cpp/02_memory-management.md для деталей RAII

class StagedMeshLoader {
public:
    void loadMeshWithStaging(VkDevice device, VkCommandPool cmdPool,
                             VkQueue queue, const draco::Mesh& mesh) {
        // 1. Создание staging buffer (HOST_VISIBLE)
        // 2. Копирование данных из Draco в staging
        // 3. Создание device buffer (DEVICE_LOCAL)
        // 4. Copy command buffer
        // 5. Submit и wait
    }
};
```

---

## Интеграция с ECS (flecs)

### Компоненты для геометрии

```cpp
#include <flecs.h>

// Компонент для рендеринга
struct DracoMeshComponent {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation vertexAllocation = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VmaAllocation indexAllocation = VK_NULL_HANDLE;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
};

// Компонент для сжатых данных
struct CompressedChunkComponent {
    std::vector<char> compressedData;
    uint32_t chunkX, chunkY, chunkZ;
    bool needsDecompression = true;
};

// Система загрузки чанков
void RegisterDracoSystems(flecs::world& world) {
    // Система декомпрессии (на worker thread)
    world.system<CompressedChunkComponent, DracoMeshComponent>("DracoDecompress")
        .kind(flecs::OnUpdate)
        .each([](flecs::entity e, CompressedChunkComponent& compressed,
                 DracoMeshComponent& mesh) {
            if (!compressed.needsDecompression) return;

            // Декомпрессия в фоновом потоке
            draco::DecoderBuffer buffer;
            buffer.Init(compressed.compressedData.data(),
                       compressed.compressedData.size());

            draco::Decoder decoder;
            auto result = decoder.DecodeMeshFromBuffer(&buffer);

            if (result.ok()) {
                // Создание Vulkan buffers
                // ...
                compressed.needsDecompression = false;
            }
        });
}
```

### Асинхронная загрузка

```cpp
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

class AsyncDracoLoader {
public:
    struct LoadRequest {
        std::vector<char> compressedData;
        std::function<void(std::unique_ptr<draco::Mesh>)> callback;
    };

    void start() {
        worker_ = std::thread([this]() { workerLoop(); });
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
        }
        cv_.notify_all();
        worker_.join();
    }

    void submit(const std::vector<char>& data,
                std::function<void(std::unique_ptr<draco::Mesh>)> callback) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push({data, callback});
        }
        cv_.notify_one();
    }

private:
    void workerLoop() {
        while (running_) {
            LoadRequest request;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]() { return !queue_.empty() || !running_; });

                if (!running_) break;

                request = std::move(queue_.front());
                queue_.pop();
            }

            draco::DecoderBuffer buffer;
            buffer.Init(request.compressedData.data(),
                       request.compressedData.size());

            draco::Decoder decoder;
            auto result = decoder.DecodeMeshFromBuffer(&buffer);

            if (result.ok()) {
                request.callback(std::move(result).value());
            } else {
                request.callback(nullptr);
            }
        }
    }

    std::thread worker_;
    std::queue<LoadRequest> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool running_ = true;
};
```

---

## SVO и Sparse Data

### Адаптация Draco для SVO

```cpp
// SVO node data
struct SVONodeData {
    uint32_t childMask;      // 8-bit child existence mask
    uint32_t firstChild;     // Index of first child
    uint8_t voxelType;       // Leaf voxel type
};

// Encoding SVO as Draco point cloud
std::unique_ptr<draco::PointCloud> svoToPointCloud(const std::vector<SVONodeData>& nodes) {
    auto pc = std::make_unique<draco::PointCloud>();
    pc->set_num_points(nodes.size());

    // Child mask attribute
    draco::GeometryAttribute childMaskAttr;
    childMaskAttr.Init(draco::GeometryAttribute::GENERIC, nullptr, 1,
                       draco::DT_UINT32, false, 4, 0);
    int childMaskId = pc->AddAttribute(childMaskAttr, true, nodes.size());

    // First child attribute
    draco::GeometryAttribute firstChildAttr;
    firstChildAttr.Init(draco::GeometryAttribute::GENERIC, nullptr, 1,
                        draco::DT_UINT32, false, 4, 0);
    int firstChildId = pc->AddAttribute(firstChildAttr, true, nodes.size());

    // Voxel type attribute
    draco::GeometryAttribute voxelTypeAttr;
    voxelTypeAttr.Init(draco::GeometryAttribute::GENERIC, nullptr, 1,
                       draco::DT_UINT8, false, 1, 0);
    int voxelTypeId = pc->AddAttribute(voxelTypeAttr, true, nodes.size());

    // Fill attributes
    for (size_t i = 0; i < nodes.size(); ++i) {
        pc->attribute(childMaskId)->SetAttributeValue(
            draco::AttributeValueIndex(i), &nodes[i].childMask);
        pc->attribute(firstChildId)->SetAttributeValue(
            draco::AttributeValueIndex(i), &nodes[i].firstChild);
        pc->attribute(voxelTypeId)->SetAttributeValue(
            draco::AttributeValueIndex(i), &nodes[i].voxelType);
    }

    return pc;
}
```

### Metadata для SVO структуры

```cpp
void addSVOMetadata(draco::PointCloud& pc, uint32_t depth, uint32_t leafCount) {
    auto metadata = std::make_unique<draco::GeometryMetadata>();
    metadata->AddEntryString("structure", "svo");
    metadata->AddEntryInt("depth", depth);
    metadata->AddEntryInt("leaf_count", leafCount);
    metadata->AddEntryString("version", "1.0");

    pc.AddMetadata(std::move(metadata));
}
```

---

## Профилирование с Tracy

```cpp
#include <tracy/Tracy.hpp>

class ProfiledDracoDecoder {
public:
    std::unique_ptr<draco::Mesh> decode(const void* data, size_t size) {
        ZoneScopedN("DracoDecode");

        draco::DecoderBuffer buffer;
        buffer.Init(reinterpret_cast<const char*>(data), size);

        draco::Decoder decoder;

        auto start = std::chrono::high_resolution_clock::now();
        auto result = decoder.DecodeMeshFromBuffer(&buffer);
        auto end = std::chrono::high_resolution_clock::now();

        if (result.ok()) {
            auto mesh = std::move(result).value();

            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            TracyPlot("DracoDecodeTime", duration.count());
            TracyPlot("DracoVertexCount", static_cast<int64_t>(mesh->num_points()));
            TracyPlot("DracoFaceCount", static_cast<int64_t>(mesh->num_faces()));

            return mesh;
        }

        return nullptr;
    }
};
```

---

## Сетевая оптимизация

### Delta compression для чанков

```cpp
// Сжатие diff между версиями чанка
std::vector<char> compressChunkDiff(
    const VoxelChunkData& oldChunk,
    const VoxelChunkData& newChunk) {

    // 1. Вычисление diff
    std::vector<uint8_t> diff;
    for (size_t i = 0; i < newChunk.voxelTypes.size(); ++i) {
        if (oldChunk.voxelTypes[i] != newChunk.voxelTypes[i]) {
            // Store index + new value
        }
    }

    // 2. Сжатие diff через Draco
    // ...
}
```

### Приоритет загрузки

```cpp
// Загрузка чанков по расстоянию до игрока
void prioritizeChunkLoading(
    const std::vector<CompressedChunkComponent>& chunks,
    const glm::vec3& playerPos) {

    // Sort chunks by distance to player
    // Load nearest chunks first with higher decode priority
}
```

---

## Best Practices для ProjectV

### Настройки по умолчанию для вокселей

```cpp
draco::Encoder createVoxelChunkEncoder() {
    draco::Encoder encoder;

    // Воксели уже дискретны, достаточно 8 бит
    encoder.SetAttributeQuantization(draco::GeometryAttribute::GENERIC, 8);

    // Быстрое декодирование важно для chunk loading
    encoder.SetSpeedOptions(5, 10);

    // Sequential для point cloud данных
    encoder.SetEncodingMethod(draco::POINT_CLOUD_SEQUENTIAL_ENCODING);

    return encoder;
}
```

### Настройки для glTF моделей

```cpp
draco::Encoder createGltfEncoder() {
    draco::Encoder encoder;

    // Высокое качество для визуальных моделей
    encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, 14);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, 10);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, 12);

    // Хороший compression, разумный decode
    encoder.SetSpeedOptions(3, 5);

    // Edgebreaker для meshes
    encoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);

    return encoder;
}
```

### Память и кэширование

```cpp
// Кэширование декодированных mesh
class MeshCache {
public:
    std::shared_ptr<draco::Mesh> getOrDecode(const std::string& key,
                                             const std::vector<char>& compressed) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_.find(key);
        if (it != cache_.end()) {
            return it->second;
        }

        // Decode and cache
        draco::DecoderBuffer buffer;
        buffer.Init(compressed.data(), compressed.size());

        draco::Decoder decoder;
        auto result = decoder.DecodeMeshFromBuffer(&buffer);

        if (result.ok()) {
            auto mesh = std::make_shared<draco::Mesh>(std::move(result).value());
            cache_[key] = mesh;
            return mesh;
        }

        return nullptr;
    }

private:
    std::unordered_map<std::string, std::shared_ptr<draco::Mesh>> cache_;
    std::mutex mutex_;
};
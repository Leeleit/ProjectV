# Asset Management: Runtime Loading

Загрузка и управление ассетами в runtime.

## Концепция

ProjectV загружает ассеты **напрямую в runtime** используя `fastgltf` для glTF моделей и `glaze` для метаданных.
Кэширование оптимизированных GPU буферов обеспечивает быструю повторную загрузку.

### Почему НЕ Offline Compiler?

| Подход               | Преимущества                           | Недостатки                           |
|----------------------|----------------------------------------|--------------------------------------|
| **Runtime Loading**  | Быстрая итерация, гибкость, hot-reload | Чуть медленнее первый запуск         |
| ~~Offline Compiler~~ | Максимальная скорость загрузки         | Сложный pipeline, медленная итерация |

ProjectV выбрал runtime loading для **быстрой разработки**. Оптимизация происходит автоматически при первом
использовании.

---

## Архитектура

```
┌─────────────────────────────────────────────────────────────────┐
│                     Asset Manager                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  [Source Files]          [GPU Cache]                           │
│  assets/                 cache/                                 │
│  ├── models/             ├── models/                           │
│  │   └── character.gltf  │   └── character.bin (optimized)     │
│  └── textures/           └── textures/                          │
│      └── stone.png           └── stone.ktx (compressed)        │
│                                                                 │
│  При первом запуске:                                            │
│  .gltf → fastgltf → GPU buffers → .bin cache                   │
│                                                                 │
│  При последующих запусках:                                      │
│  .bin cache → напрямую в GPU                                    │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 1. Asset Manager

### 1.1 Основной интерфейс

```cpp
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <glaze/glaze.hpp>
#include <expected>

namespace projectv {

enum class AssetError {
    FileNotFound,
    InvalidFormat,
    GpuUploadFailed,
    CacheWriteFailed
};

template<typename T>
using AssetResult = std::expected<std::shared_ptr<T>, AssetError>;

// Базовый класс для GPU ресурсов
class GPUResource {
public:
    virtual ~GPUResource() = default;
    virtual size_t gpuMemoryUsage() const = 0;
};

// Дескриптор ассета
struct AssetDescriptor {
    std::string name;
    std::filesystem::path sourcePath;
    std::filesystem::path cachePath;
    uint64_t sourceHash;        // Для обнаружения изменений
    uint64_t lastModified;

    struct glaze {
        using T = AssetDescriptor;
        static constexpr auto value = glz::object(
            "name", &T::name,
            "sourcePath", &T::sourcePath,
            "cachePath", &T::cachePath,
            "sourceHash", &T::sourceHash,
            "lastModified", &T::lastModified
        );
    };
};

class AssetManager {
public:
    AssetManager(VkDevice device, VmaAllocator allocator,
                 const std::filesystem::path& assetDir,
                 const std::filesystem::path& cacheDir)
        : device_(device)
        , allocator_(allocator)
        , assetDir_(assetDir)
        , cacheDir_(cacheDir)
    {}

    // Инициализация — загрузка манифеста
    std::expected<void, AssetError> initialize() {
        // Создаём директорию кэша
        std::error_code ec;
        std::filesystem::create_directories(cacheDir_, ec);

        // Загружаем манифест
        auto manifestPath = cacheDir_ / "manifest.json";
        if (std::filesystem::exists(manifestPath)) {
            auto result = glz::read_file_json(manifest_, manifestPath.string());
            if (!result) {
                // Игнорируем ошибки — создадим новый манифест
            }
        }

        return {};
    }

    // Загрузка mesh
    AssetResult<class Mesh> loadMesh(const std::string& name);

    // Загрузка текстуры
    AssetResult<class Texture> loadTexture(const std::string& name);

    // Загрузка материала
    AssetResult<class Material> loadMaterial(const std::string& name);

    // Hot reload
    void checkForChanges();

    // Очистка неиспользуемых ресурсов
    void garbageCollect();

private:
    VkDevice device_;
    VmaAllocator allocator_;
    std::filesystem::path assetDir_;
    std::filesystem::path cacheDir_;

    std::unordered_map<std::string, AssetDescriptor> manifest_;
    std::unordered_map<std::string, std::weak_ptr<GPUResource>> cache_;

    // Проверка существования кэша
    bool hasValidCache(const std::string& name) const;

    // Загрузка из кэша
    template<typename T>
    AssetResult<T> loadFromCache(const std::string& name);

    // Сохранение в кэш
    template<typename T>
    std::expected<void, AssetError> saveToCache(const std::string& name, const T& asset);
};

} // namespace projectv
```

---

## 2. Загрузка glTF с fastgltf

### 2.1 Mesh загрузчик

```cpp
namespace projectv {

struct MeshVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 tangent;
};

class Mesh : public GPUResource {
public:
    VkBuffer vertexBuffer;
    VmaAllocation vertexAllocation;
    VkBuffer indexBuffer;
    VmaAllocation indexAllocation;
    uint32_t indexCount;
    uint32_t vertexCount;
    glm::vec3 boundsMin;
    glm::vec3 boundsMax;

    size_t gpuMemoryUsage() const override {
        return vertexCount * sizeof(MeshVertex) + indexCount * sizeof(uint32_t);
    }

    struct glaze {
        using T = Mesh;
        // Только метаданные для кэша
    };
};

class MeshLoader {
public:
    AssetResult<Mesh> load(const std::filesystem::path& path,
                           VkDevice device,
                           VmaAllocator allocator) {
        // Парсинг glTF
        fastgltf::Parser parser;
        auto gltfPath = path.string();

        auto asset = parser.loadGltfBinary(gltfPath, path.parent_path().string());

        if (asset.error() != fastgltf::Error::None) {
            return std::unexpected(AssetError::InvalidFormat);
        }

        // Извлекаем mesh данные
        std::vector<MeshVertex> vertices;
        std::vector<uint32_t> indices;
        glm::vec3 boundsMin{std::numeric_limits<float>::max()};
        glm::vec3 boundsMax{std::numeric_limits<float>::lowest()};

        for (const auto& mesh : asset->meshes) {
            for (const auto& primitive : mesh.primitives) {
                // Извлечение вершин
                auto positionIt = primitive.findAttribute("POSITION");
                if (positionIt == primitive.attributes.end()) continue;

                auto& positionAccessor = asset->accessors[positionIt->second];
                auto& positionBuffer = asset->buffers[positionAccessor.bufferViewIndex.value()];

                // Чтение позиций
                std::vector<glm::vec3> positions(positionAccessor.count);
                std::memcpy(positions.data(),
                           positionBuffer.data.data() + positionAccessor.byteOffset,
                           positionAccessor.count * sizeof(glm::vec3));

                // Чтение нормалей (если есть)
                std::vector<glm::vec3> normals(positionAccessor.count, {0, 1, 0});
                auto normalIt = primitive.findAttribute("NORMAL");
                if (normalIt != primitive.attributes.end()) {
                    auto& normalAccessor = asset->accessors[normalIt->second];
                    auto& normalBuffer = asset->buffers[normalAccessor.bufferViewIndex.value()];
                    std::memcpy(normals.data(),
                               normalBuffer.data.data() + normalAccessor.byteOffset,
                               normalAccessor.count * sizeof(glm::vec3));
                }

                // Чтение UV (если есть)
                std::vector<glm::vec2> uvs(positionAccessor.count, {0, 0});
                auto uvIt = primitive.findAttribute("TEXCOORD_0");
                if (uvIt != primitive.attributes.end()) {
                    auto& uvAccessor = asset->accessors[uvIt->second];
                    auto& uvBuffer = asset->buffers[uvAccessor.bufferViewIndex.value()];
                    std::memcpy(uvs.data(),
                               uvBuffer.data.data() + uvAccessor.byteOffset,
                               uvAccessor.count * sizeof(glm::vec2));
                }

                // Объединение вершин
                for (size_t i = 0; i < positions.size(); ++i) {
                    MeshVertex v{
                        .position = positions[i],
                        .normal = normals[i],
                        .uv = uvs[i],
                        .tangent = {1, 0, 0, 1}
                    };
                    vertices.push_back(v);

                    boundsMin = glm::min(boundsMin, positions[i]);
                    boundsMax = glm::max(boundsMax, positions[i]);
                }

                // Извлечение индексов
                if (primitive.indicesAccessor.has_value()) {
                    auto& indexAccessor = asset->accessors[primitive.indicesAccessor.value()];
                    auto& indexBuffer = asset->buffers[indexAccessor.bufferViewIndex.value()];

                    uint32_t indexOffset = static_cast<uint32_t>(vertices.size() - positions.size());

                    if (indexAccessor.componentType == fastgltf::ComponentType::UnsignedShort) {
                        std::vector<uint16_t> tempIndices(indexAccessor.count);
                        std::memcpy(tempIndices.data(),
                                   indexBuffer.data.data() + indexAccessor.byteOffset,
                                   indexAccessor.count * sizeof(uint16_t));

                        for (auto idx : tempIndices) {
                            indices.push_back(indexOffset + idx);
                        }
                    } else if (indexAccessor.componentType == fastgltf::ComponentType::UnsignedInt) {
                        std::vector<uint32_t> tempIndices(indexAccessor.count);
                        std::memcpy(tempIndices.data(),
                                   indexBuffer.data.data() + indexAccessor.byteOffset,
                                   indexAccessor.count * sizeof(uint32_t));

                        for (auto idx : tempIndices) {
                            indices.push_back(indexOffset + idx);
                        }
                    }
                }
            }
        }

        // Оптимизация mesh (cache optimization)
        optimizeMesh(vertices, indices);

        // Создание GPU буферов
        auto mesh = std::make_shared<Mesh>();

        // Vertex buffer
        VkBufferCreateInfo vbInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = vertices.size() * sizeof(MeshVertex),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT
        };

        VmaAllocationCreateInfo vbAllocInfo{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO
        };

        vmaCreateBuffer(allocator, &vbInfo, &vbAllocInfo,
                       &mesh->vertexBuffer, &mesh->vertexAllocation, nullptr);

        // Копирование вершин
        void* vertexData;
        vmaMapMemory(allocator, mesh->vertexAllocation, &vertexData);
        std::memcpy(vertexData, vertices.data(), vertices.size() * sizeof(MeshVertex));
        vmaUnmapMemory(allocator, mesh->vertexAllocation);

        // Index buffer
        VkBufferCreateInfo ibInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = indices.size() * sizeof(uint32_t),
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT
        };

        VmaAllocationCreateInfo ibAllocInfo{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO
        };

        vmaCreateBuffer(allocator, &ibInfo, &ibAllocInfo,
                       &mesh->indexBuffer, &mesh->indexAllocation, nullptr);

        // Копирование индексов
        void* indexData;
        vmaMapMemory(allocator, mesh->indexAllocation, &indexData);
        std::memcpy(indexData, indices.data(), indices.size() * sizeof(uint32_t));
        vmaUnmapMemory(allocator, mesh->indexAllocation);

        mesh->vertexCount = static_cast<uint32_t>(vertices.size());
        mesh->indexCount = static_cast<uint32_t>(indices.size());
        mesh->boundsMin = boundsMin;
        mesh->boundsMax = boundsMax;

        return mesh;
    }

private:
    void optimizeMesh(std::vector<MeshVertex>& vertices,
                      std::vector<uint32_t>& indices) {
        // Cache-optimized triangle reordering
        meshopt_optimizeIndexCache(indices.data(), indices.data(),
                                    indices.size(), vertices.size());

        // Vertex fetch optimisation
        meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(),
                                     vertices.data(), vertices.size(), sizeof(MeshVertex));
    }
};

} // namespace projectv
```

---

## 3. Кэширование GPU буферов

### 3.1 Формат кэша

```cpp
namespace projectv {

// Заголовок кэша mesh
#pragma pack(push, 1)
struct MeshCacheHeader {
    uint32_t magic = 0x4D455348;  // "MESH"
    uint32_t version = 1;
    uint32_t vertexCount;
    uint32_t indexCount;
    glm::vec3 boundsMin;
    glm::vec3 boundsMax;
    uint32_t vertexDataOffset;
    uint32_t indexDataOffset;
    uint32_t reserved[4];
};
#pragma pack(pop)

class MeshCache {
public:
    // Сохранение mesh в кэш
    std::expected<void, AssetError> save(const std::string& name,
                                          const Mesh& mesh,
                                          const std::filesystem::path& cacheDir) {
        auto cachePath = cacheDir / "meshes" / (name + ".bin");
        std::filesystem::create_directories(cachePath.parent_path());

        std::ofstream file(cachePath, std::ios::binary);
        if (!file) {
            return std::unexpected(AssetError::CacheWriteFailed);
        }

        MeshCacheHeader header{
            .vertexCount = mesh.vertexCount,
            .indexCount = mesh.indexCount,
            .boundsMin = mesh.boundsMin,
            .boundsMax = mesh.boundsMax
        };

        file.write(reinterpret_cast<const char*>(&header), sizeof(header));

        // Vertex data
        void* vertexData;
        vmaMapMemory(allocator_, mesh.vertexAllocation, &vertexData);
        file.write(static_cast<const char*>(vertexData),
                   mesh.vertexCount * sizeof(MeshVertex));
        vmaUnmapMemory(allocator_, mesh.vertexAllocation);

        // Index data
        void* indexData;
        vmaMapMemory(allocator_, mesh.indexAllocation, &indexData);
        file.write(static_cast<const char*>(indexData),
                   mesh.indexCount * sizeof(uint32_t));
        vmaUnmapMemory(allocator_, mesh.indexAllocation);

        return {};
    }

    // Загрузка mesh из кэша
    AssetResult<Mesh> load(const std::string& name,
                           const std::filesystem::path& cacheDir) {
        auto cachePath = cacheDir / "meshes" / (name + ".bin");

        if (!std::filesystem::exists(cachePath)) {
            return std::unexpected(AssetError::FileNotFound);
        }

        std::ifstream file(cachePath, std::ios::binary);
        if (!file) {
            return std::unexpected(AssetError::FileNotFound);
        }

        MeshCacheHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        if (header.magic != 0x4D455348) {
            return std::unexpected(AssetError::InvalidFormat);
        }

        auto mesh = std::make_shared<Mesh>();
        mesh->vertexCount = header.vertexCount;
        mesh->indexCount = header.indexCount;
        mesh->boundsMin = header.boundsMin;
        mesh->boundsMax = header.boundsMax;

        // Создание буферов и загрузка данных
        createBuffersFromCache(mesh, file);

        return mesh;
    }

private:
    VmaAllocator allocator_;

    void createBuffersFromCache(std::shared_ptr<Mesh>& mesh, std::ifstream& file);
};

} // namespace projectv
```

---

## 4. Загрузка текстур

### 4.1 Texture Loader

```cpp
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace projectv {

class Texture : public GPUResource {
public:
    VkImage image;
    VmaAllocation allocation;
    VkImageView imageView;
    uint32_t width;
    uint32_t height;
    uint32_t mipLevels;
    VkFormat format;

    size_t gpuMemoryUsage() const override {
        // Приближённый расчёт
        size_t size = width * height * 4;  // RGBA
        for (uint32_t i = 1; i < mipLevels; ++i) {
            size += (width >> i) * (height >> i) * 4;
        }
        return size;
    }
};

class TextureLoader {
public:
    AssetResult<Texture> load(const std::filesystem::path& path,
                              VkDevice device,
                              VmaAllocator allocator,
                              VkCommandPool cmdPool,
                              VkQueue uploadQueue) {
        // Загрузка изображения
        int width, height, channels;
        stbi_uc* pixels = stb_image_load(path.string().c_str(),
                                         &width, &height, &channels, 4);

        if (!pixels) {
            return std::unexpected(AssetError::InvalidFormat);
        }

        auto texture = std::make_shared<Texture>();
        texture->width = static_cast<uint32_t>(width);
        texture->height = static_cast<uint32_t>(height);
        texture->mipLevels = calculateMipLevels(width, height);
        texture->format = VK_FORMAT_R8G8B8A8_SRGB;

        // Создание staging buffer
        VkDeviceSize imageSize = width * height * 4;

        VkBuffer stagingBuffer;
        VmaAllocation stagingAllocation;

        VkBufferCreateInfo bufferInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = imageSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        };

        VmaAllocationCreateInfo allocInfo{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO
        };

        vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
                       &stagingBuffer, &stagingAllocation, nullptr);

        void* data;
        vmaMapMemory(allocator, stagingAllocation, &data);
        std::memcpy(data, pixels, imageSize);
        vmaUnmapMemory(allocator, stagingAllocation);

        stbi_image_free(pixels);

        // Создание image
        VkImageCreateInfo imageInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = texture->format,
            .extent = {texture->width, texture->height, 1},
            .mipLevels = texture->mipLevels,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                     VK_IMAGE_USAGE_SAMPLED_BIT
        };

        VmaAllocationCreateInfo imageAllocInfo{
            .usage = VMA_MEMORY_USAGE_GPU_ONLY
        };

        vmaCreateImage(allocator, &imageInfo, &imageAllocInfo,
                      &texture->image, &texture->allocation, nullptr);

        // Переход layout и копирование
        transitionImageLayout(device, cmdPool, uploadQueue,
                             texture->image, VK_FORMAT_R8G8B8A8_SRGB,
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        copyBufferToImage(device, cmdPool, uploadQueue,
                          stagingBuffer, texture->image,
                          texture->width, texture->height);

        // Генерация mipmaps
        generateMipmaps(device, cmdPool, uploadQueue, texture);

        // Очистка staging
        vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);

        // Создание image view
        VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = texture->image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = texture->format,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = texture->mipLevels,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        vkCreateImageView(device, &viewInfo, nullptr, &texture->imageView);

        return texture;
    }

private:
    static uint32_t calculateMipLevels(int width, int height) {
        return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
    }

    void transitionImageLayout(VkDevice device, VkCommandPool pool, VkQueue queue,
                               VkImage image, VkFormat format,
                               VkImageLayout oldLayout, VkImageLayout newLayout);

    void copyBufferToImage(VkDevice device, VkCommandPool pool, VkQueue queue,
                           VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    void generateMipmaps(VkDevice device, VkCommandPool pool, VkQueue queue,
                         std::shared_ptr<Texture> texture);
};

} // namespace projectv
```

---

## 5. Hot Reload

### 5.1 File Watcher

```cpp
namespace projectv {

class FileWatcher {
public:
    using Callback = std::function<void(const std::string& name)>;

    void watch(const std::filesystem::path& path, Callback callback) {
        watchPaths_[path] = {
            .lastModified = std::filesystem::last_write_time(path),
            .callback = callback
        };
    }

    void checkChanges() {
        for (auto& [path, info] : watchPaths_) {
            auto currentModified = std::filesystem::last_write_time(path);

            if (currentModified != info.lastModified) {
                info.lastModified = currentModified;

                // Извлекаем имя ассета из пути
                std::string name = path.stem().string();
                info.callback(name);
            }
        }
    }

private:
    struct WatchInfo {
        std::filesystem::file_time_type lastModified;
        Callback callback;
    };

    std::unordered_map<std::filesystem::path, WatchInfo> watchPaths_;
};

// Интеграция с AssetManager
class AssetManager {
    // ...

    void enableHotReload() {
        for (const auto& [name, desc] : manifest_) {
            fileWatcher_.watch(assetDir_ / desc.sourcePath,
                [this, name](const std::string&) {
                    reloadAsset(name);
                });
        }
    }

    void checkForChanges() {
        fileWatcher_.checkChanges();
    }

    void reloadAsset(const std::string& name) {
        // Инвалидируем кэш
        cache_.erase(name);

        // Удаляем файл кэша
        auto& desc = manifest_[name];
        std::filesystem::remove(cacheDir_ / desc.cachePath);

        // Уведомляем системы
        onAssetReloaded_.publish(name);
    }

private:
    FileWatcher fileWatcher_;
    Event<std::string> onAssetReloaded_;
};

} // namespace projectv
```

---

## 6. Интеграция с ECS

```cpp
namespace projectv {

// Компонент для mesh
struct MeshComponent {
    std::shared_ptr<Mesh> mesh;
    std::string meshName;  // Для hot reload
};

// Компонент для материала
struct MaterialComponent {
    std::shared_ptr<Material> material;
    std::string materialName;
};

// Система для обновления после hot reload
ecs.system<MeshComponent>("UpdateMeshReferences")
    .kind(flecs::OnLoad)
    .iter([](flecs::iter& it, MeshComponent* meshes) {
        auto* am = it.world().ctx<AssetManager>();

        for (auto i : it) {
            if (!meshes[i].mesh || meshes[i].mesh.use_count() == 1) {
                // Перезагружаем
                auto result = am->loadMesh(meshes[i].meshName);
                if (result) {
                    meshes[i].mesh = *result;
                }
            }
        }
    });

} // namespace projectv
```

---

## 7. Рекомендации

### Когда кэшировать

| Тип       | Кэшировать? | Причина                     |
|-----------|-------------|-----------------------------|
| Mesh      | Да          | Оптимизация занимает время  |
| Texture   | Да          | Генерация mips на GPU       |
| Materials | Нет         | Загрузка мгновенная (glaze) |
| Shaders   | Да          | Компиляция SPIR-V           |

### Структура директорий

```
project/
├── assets/
│   ├── models/
│   │   └── character.gltf
│   ├── textures/
│   │   └── stone.png
│   └── materials/
│       └── stone.json
├── cache/
│   ├── meshes/
│   │   └── character.bin
│   ├── textures/
│   │   └── stone.ktx
│   └── manifest.json
└── src/
    └── ...
```

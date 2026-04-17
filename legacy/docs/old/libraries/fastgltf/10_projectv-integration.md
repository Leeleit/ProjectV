# Интеграция fastgltf в ProjectV

🔴 **Уровень 3: Продвинутый**

Данный документ содержит ProjectV-специфичные рекомендации и примеры кода для интеграции библиотеки fastgltf в
воксельный движок.

## Роль fastgltf в ProjectV

fastgltf используется для загрузки 3D-моделей (glTF/GLB) в Vulkan-рендерер ProjectV. Данные буферов можно писать
напрямую в GPU через `setBufferAllocationCallback`. Библиотека работает с VMA и Vulkan для создания vertex/index buffers
и текстур.

### Жизненный цикл загрузки

```
Файл .gltf/.glb
      ↓
GltfDataBuffer / GltfFileStream
      ↓
Parser::loadGltf
      ↓
Asset
      ↓
iterateAccessor / copyFromAccessor
      ↓
VMA / Vulkan vertex/index buffers
      ↓
Создание компонентов flecs
      ↓
Система рендеринга ProjectV
```

---

## Интеграция с Vulkan и VMA

### Стратегии загрузки данных в GPU

| Стратегия                        | Использование               | Преимущества              | Недостатки                 |
|----------------------------------|-----------------------------|---------------------------|----------------------------|
| **Zero-copy через CustomBuffer** | Воксельные чанки, streaming | Нулевые накладные расходы | Требует управления памятью |
| **Staging буферы**               | Статические меши, ландшафты | Простота, совместимость   | Двойное копирование        |
| **Прямое маппирование**          | Анимированные персонажи     | Минимальная задержка      | Сложность синхронизации    |

### Загрузка меша через VMA

```cpp
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <glm/glm.hpp>

struct VulkanMeshData {
    VkBuffer vertexBuffer;
    VmaAllocation vertexAllocation;
    VkBuffer indexBuffer;
    VmaAllocation indexAllocation;
    uint32_t vertexCount;
    uint32_t indexCount;
};

std::optional<VulkanMeshData> loadGltfMeshToVulkan(
    const std::filesystem::path& path,
    VmaAllocator allocator) {

    fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_basisu);
    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (data.error() != fastgltf::Error::None) return std::nullopt;

    auto asset = parser.loadGltf(
        data.get(),
        path.parent_path(),
        fastgltf::Options::LoadExternalBuffers,
        fastgltf::Category::OnlyRenderable
    );

    if (asset.error() != fastgltf::Error::None) return std::nullopt;

    VulkanMeshData result = {};

    if (!asset->meshes.empty()) {
        const auto& mesh = asset->meshes[0];

        for (const auto& primitive : mesh.primitives) {
            // ПОЗИЦИИ ВЕРШИН
            if (auto* posAttr = primitive.findAttribute("POSITION");
                posAttr != primitive.attributes.cend()) {

                const auto& accessor = asset->accessors[posAttr->accessorIndex];
                result.vertexCount = static_cast<uint32_t>(accessor.count);

                std::vector<glm::vec3> positions(accessor.count);
                fastgltf::copyFromAccessor<glm::vec3>(
                    asset.get(), accessor, positions.data());

                VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
                bufferInfo.size = positions.size() * sizeof(glm::vec3);
                bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT;

                VmaAllocationCreateInfo allocCreateInfo = {};
                allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
                allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

                vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo,
                    &result.vertexBuffer, &result.vertexAllocation, nullptr);

                void* mappedData;
                vmaMapMemory(allocator, result.vertexAllocation, &mappedData);
                memcpy(mappedData, positions.data(), bufferInfo.size);
                vmaUnmapMemory(allocator, result.vertexAllocation);
            }

            // ИНДЕКСЫ
            if (primitive.indicesAccessor.has_value()) {
                const auto& accessor = asset->accessors[primitive.indicesAccessor.value()];
                result.indexCount = static_cast<uint32_t>(accessor.count);

                std::vector<uint32_t> indices(accessor.count);
                if (accessor.componentType == fastgltf::ComponentType::UnsignedInt) {
                    fastgltf::copyFromAccessor<uint32_t>(
                        asset.get(), accessor, indices.data());
                } else if (accessor.componentType == fastgltf::ComponentType::UnsignedShort) {
                    std::vector<uint16_t> shortIndices(accessor.count);
                    fastgltf::copyFromAccessor<uint16_t>(
                        asset.get(), accessor, shortIndices.data());
                    for (size_t i = 0; i < accessor.count; ++i) {
                        indices[i] = shortIndices[i];
                    }
                }

                // Создание index buffer...
            }
        }
    }

    return result;
}
```

### Zero-copy через BufferAllocationCallback

```cpp
struct VulkanContext {
    VmaAllocator allocator;
    std::vector<VkBuffer> allocatedBuffers;
    std::vector<VmaAllocation> allocations;
};

fastgltf::Parser createGpuParser(VulkanContext& ctx) {
    fastgltf::Parser parser;
    parser.setUserPointer(&ctx);

    parser.setBufferAllocationCallback(
        [](void* userPointer, size_t bufferSize, fastgltf::BufferAllocateFlags)
        -> fastgltf::BufferInfo {

            auto* ctx = static_cast<VulkanContext*>(userPointer);

            VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            bufferInfo.size = bufferSize;
            bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                              VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT;

            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                             VMA_ALLOCATION_CREATE_MAPPED_BIT;

            VkBuffer buffer;
            VmaAllocation allocation;
            VmaAllocationInfo allocInfoOut;
            vmaCreateBuffer(ctx->allocator, &bufferInfo, &allocInfo,
                &buffer, &allocation, &allocInfoOut);

            ctx->allocatedBuffers.push_back(buffer);
            ctx->allocations.push_back(allocation);

            return fastgltf::BufferInfo{
                .mappedMemory = allocInfoOut.pMappedData,
                .customId = ctx->allocatedBuffers.size() - 1
            };
        },
        nullptr,
        nullptr
    );

    return parser;
}
```

---

## Работа с ECS (flecs)

### Компоненты для анимационных данных

```cpp
#include <flecs.h>

struct AnimationComponent {
    std::vector<glm::mat4> jointMatrices;
    float currentTime = 0.0f;
    float duration = 0.0f;
    bool playing = false;
};

struct SkeletonComponent {
    std::vector<glm::mat4> inverseBindMatrices;
    std::vector<std::string> jointNames;
};

struct MorphTargetsComponent {
    std::vector<fastgltf::MorphTarget> targets;
    std::vector<float> weights;
};
```

### Извлечение анимаций для ECS

```cpp
void extractAnimationsForECS(
    const fastgltf::Asset& asset,
    flecs::world& world,
    flecs::entity entity) {

    for (const auto& animation : asset.animations) {
        AnimationComponent animComp;

        for (const auto& channel : animation.channels) {
            if (!channel.nodeIndex.has_value()) continue;

            const auto& sampler = animation.samplers[channel.samplerIndex];

            // Извлечение ключевых кадров
            std::vector<float> keyframeTimes;
            fastgltf::iterateAccessor<float>(
                asset,
                asset.accessors[sampler.inputAccessor],
                [&](float time) { keyframeTimes.push_back(time); }
            );

            // Определение длительности
            if (!keyframeTimes.empty()) {
                animComp.duration = std::max(animComp.duration,
                    keyframeTimes.back());
            }
        }

        entity.set<AnimationComponent>(animComp);
    }
}
```

### Загрузка скелета

```cpp
void extractSkeletonForECS(
    const fastgltf::Asset& asset,
    flecs::world& world,
    flecs::entity entity) {

    if (asset.skins.empty()) return;

    const auto& skin = asset.skins[0];
    SkeletonComponent skeleton;

    if (skin.inverseBindMatrices.has_value()) {
        const auto& accessor = asset.accessors[*skin.inverseBindMatrices];
        skeleton.inverseBindMatrices.resize(accessor.count);
        fastgltf::copyFromAccessor<glm::mat4>(
            asset, accessor, skeleton.inverseBindMatrices.data());
    }

    for (size_t jointIdx : skin.joints) {
        skeleton.jointNames.push_back(asset.nodes[jointIdx].name);
    }

    entity.set<SkeletonComponent>(skeleton);
}
```

---

## Система материалов

### Конвертация glTF материалов в формат ProjectV

```cpp
struct ProjectVMaterial {
    glm::vec4 albedo;
    float roughness;
    float metallic;
    float emission;
    uint32_t voxelType;
};

ProjectVMaterial convertGltfMaterial(const fastgltf::Material& gltfMat) {
    ProjectVMaterial mat{};

    // Базовый цвет
    if (gltfMat.pbrData.baseColorFactor.size() >= 3) {
        mat.albedo = glm::vec4(
            gltfMat.pbrData.baseColorFactor[0],
            gltfMat.pbrData.baseColorFactor[1],
            gltfMat.pbrData.baseColorFactor[2],
            gltfMat.pbrData.baseColorFactor.size() > 3 ?
                gltfMat.pbrData.baseColorFactor[3] : 1.0f
        );
    }

    // Metallic-roughness
    if (gltfMat.pbrData.metallicFactor.has_value()) {
        mat.metallic = *gltfMat.pbrData.metallicFactor;
    }
    if (gltfMat.pbrData.roughnessFactor.has_value()) {
        mat.roughness = *gltfMat.pbrData.roughnessFactor;
    }

    // Эмиссия для светящихся вокселей
    if (gltfMat.emissiveFactor.size() >= 3) {
        mat.emission = std::max({
            gltfMat.emissiveFactor[0],
            gltfMat.emissiveFactor[1],
            gltfMat.emissiveFactor[2]
        });
    }

    return mat;
}
```

### Расширения для воксельных материалов

```cpp
fastgltf::Extensions voxelMaterialExtensions =
    fastgltf::Extensions::KHR_materials_volume |      // Объёмные материалы
    fastgltf::Extensions::KHR_materials_transmission | // Пропускание света
    fastgltf::Extensions::KHR_materials_clearcoat |    // Clearcoat
    fastgltf::Extensions::KHR_materials_anisotropy;    // Анизотропия

fastgltf::Parser parser(voxelMaterialExtensions);
```

---

## Профилирование с Tracy

```cpp
#include <tracy/Tracy.hpp>

auto loadWithProfiling(const std::filesystem::path& path) {
    ZoneScopedN("GltfLoadTotal");

    {
        ZoneScopedN("LoadFileData");
        auto data = fastgltf::GltfDataBuffer::FromPath(path);
    }

    fastgltf::Parser parser;

    {
        ZoneScopedN("ParseJson");
        auto asset = parser.loadGltf(data.get(), path.parent_path(),
            fastgltf::Options::LoadExternalBuffers);
    }

    {
        ZoneScopedN("CopyToVulkan");
        TracyPlot("VertexCount", vertexCount);
        TracyPlot("BufferSizeMB", bufferSize / (1024 * 1024));
    }

    FrameMark;
}
```

---

## Примеры кода

| Документация                                             | Описание                                        |
|----------------------------------------------------------|-------------------------------------------------|
| [01_quickstart.md](01_quickstart.md)                     | Загрузка glTF/GLB, обход сцены, извлечение меша |
| [02_concepts.md](02_concepts.md)                         | Концепции и структуры данных                    |
| [08_projectv-integration.md](08_projectv-integration.md) | Интеграция с Vulkan и VMA                       |

---

## Лучшие практики для ProjectV

1. Используйте `Category::OnlyRenderable` для статического контента — экономит 30-40% времени загрузки
2. Применяйте zero-copy стратегии с VMA — минимизируйте копирование между CPU и GPU
3. Используйте sparse accessors для partial updates — критично для динамических воксельных миров
4. Интегрируйте с Tracy для профилирования — отслеживайте bottleneck'и
5. Кэшируйте загруженные модели — переиспользуйте Asset структуры
6. Используйте асинхронную загрузку — не блокируйте основной поток

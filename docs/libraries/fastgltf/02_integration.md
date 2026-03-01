# Интеграция fastgltf в ProjectV

**Архитектурный контекст:** Fastgltf служит архитектурным мостом между иерархическими данными glTF и плоскими массивами
Data-Oriented Design в ProjectV. Библиотека выполняет роль архитектурного трансформатора, преобразующего древовидные
структуры glTF в оптимизированные для кэша процессора SoA (Structure of Arrays) форматы.

**Архитектурные цели интеграции в ProjectV:**

1. **Производительность**: SIMD-оптимизированный парсинг для воксельного рендеринга
2. **Data-Oriented Design**: Преобразование иерархии в плоские массивы для ECS (Flecs)
3. **GPU-ready архитектура**: Прямая загрузка в Vulkan буферы через VMA
4. **Современный C++26**: Использование `std::expected`, `std::span`, `std::print`

## CMake интеграция

### Базовая интеграция с движком

```cmake
# В корневом CMakeLists.txt вашего движка
add_subdirectory(external/fastgltf)

# В целевом CMakeLists.txt
target_link_libraries(YourEngine PRIVATE fastgltf::fastgltf)
```

### Конфигурация для C++26

```cmake
# Убедитесь, что fastgltf компилируется с C++26
set_target_properties(fastgltf::fastgltf PROPERTIES
    CXX_STANDARD 26
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
```

### Опции CMake для высокопроизводительных систем

```cmake
# Рекомендуемые опции для Data-Oriented Design систем
set(FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL ON CACHE BOOL "Отключить pmr-аллокатор для совместимости с DOD-аллокаторами" FORCE)
set(FASTGLTF_COMPILE_AS_CPP20 OFF CACHE BOOL "Использовать C++26" FORCE)
set(FASTGLTF_ENABLE_CPP_MODULES OFF CACHE BOOL "Отключить модули C++20" FORCE)

add_subdirectory(external/fastgltf)
```

## Архитектурное преобразование Data-Oriented Design

> **Архитектурная метафора:** Представьте glTF как древовидную организацию компании с CEO (сцена), менеджерами (узлы) и
> сотрудниками (меши). Data-Oriented Design — это переход к плоской матричной структуре, где все сотрудники одного отдела
> сидят в одном open-space (SoA массиве) для быстрого общения (кэш-локальность).

### Иерархия glTF → SoA массивы

**Проблема:** glTF использует иерархические структуры (деревья узлов), которые плохо подходят для Data-Oriented Design.
Каждый узел хранит трансформацию и ссылки на детей, что приводит к cache misses при обходе.

**Решение:** Преобразование в плоские SoA массивы, где данные одного типа (позиции, нормали) хранятся в отдельных
выровненных массивах:

```cpp
#include <print>
#include <expected>
#include <span>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>

// SoA структура для Data-Oriented Design
struct MeshSoA {
    // Structure of Arrays (SoA) для вершин
    alignas(64) std::vector<glm::vec3> positions;  // Выравнивание для кэш-линий
    alignas(64) std::vector<glm::vec3> normals;
    alignas(64) std::vector<glm::vec2> texcoords;
    alignas(64) std::vector<uint32_t> indices;

    // Метаданные
    alignas(16) std::vector<uint32_t> materialIds;  // Выравнивание для GPU
    alignas(16) std::vector<glm::mat4> transforms;
};

std::expected<MeshSoA, fastgltf::Error> convertToSoA(
    const fastgltf::Asset& asset,
    std::size_t meshIndex = 0) {

    if (meshIndex >= asset.meshes.size()) {
        return std::unexpected(fastgltf::Error::InvalidGltf);
    }

    const auto& mesh = asset.meshes[meshIndex];
    MeshSoA result;

    for (const auto& primitive : mesh.primitives) {
        // Извлечение позиций
        if (auto posAttr = primitive.findAttribute("POSITION");
            posAttr != primitive.attributes.cend()) {

            const auto& accessor = asset.accessors[posAttr->accessorIndex];
            std::vector<glm::vec3> primitivePositions(accessor.count);

            if (auto err = fastgltf::copyFromAccessor<glm::vec3>(
                asset, accessor, primitivePositions.data()); err != fastgltf::Error::None) {
                return std::unexpected(err);
            }

            // Добавляем в SoA массив
            result.positions.insert(result.positions.end(),
                primitivePositions.begin(), primitivePositions.end());
        }

        // Аналогично для нормалей, текстурных координат
    }

    return result;
}
```

### Пакетная конвертация для производительности

```cpp
struct BatchSoAConverter {
    // Пакетная обработка нескольких мешей
    std::expected<std::vector<MeshSoA>, fastgltf::Error> convertBatch(
        const fastgltf::Asset& asset,
        std::span<const std::size_t> meshIndices) {

        std::vector<MeshSoA> results;
        results.reserve(meshIndices.size());

        for (auto meshIdx : meshIndices) {
            if (auto meshSoA = convertToSoA(asset, meshIdx)) {
                results.push_back(std::move(*meshSoA));
            } else {
                return std::unexpected(meshSoA.error());
            }
        }

        return results;
    }
};
```

## Интеграция с Vulkan и VMA

> **Архитектурная метафора:** BufferAllocationCallback — это как "диспетчерская доставки". Вместо того чтобы груз (
> данные) сначала привезти на склад (RAM), а потом отправить на фабрику (GPU), диспетчер сразу вызывает курьера (VMA) с
> пустым грузовиком (буфером), и fastgltf загружает данные прямо в него.

### Zero-copy загрузка через BufferAllocationCallback

```cpp
#include <print>
#include <expected>
#include <fastgltf/core.hpp>
#include <vk_mem_alloc.h>

struct GpuContext {
    VmaAllocator allocator;
    std::vector<std::pair<VkBuffer, VmaAllocation>> allocatedBuffers;
};

std::expected<fastgltf::Asset, fastgltf::Error> loadGltfWithZeroCopy(
    const std::filesystem::path& path,
    GpuContext& ctx) {

    fastgltf::Parser parser;
    parser.setUserPointer(&ctx);

    parser.setBufferAllocationCallback(
        [](void* userPointer, size_t bufferSize,
           fastgltf::BufferAllocateFlags flags) -> fastgltf::BufferInfo {

            auto* ctx = static_cast<GpuContext*>(userPointer);

            VkBufferCreateInfo bufferInfo{
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = bufferSize,
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                         VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };

            VmaAllocationCreateInfo allocInfo{
                .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
            };

            VkBuffer buffer;
            VmaAllocation allocation;
            VmaAllocationInfo allocInfoResult;

            if (vmaCreateBuffer(ctx->allocator, &bufferInfo, &allocInfo,
                                &buffer, &allocation, &allocInfoResult) != VK_SUCCESS) {
                return fastgltf::BufferInfo{};
            }

            ctx->allocatedBuffers.emplace_back(buffer, allocation);

            return fastgltf::BufferInfo{
                .mappedMemory = allocInfoResult.pMappedData,
                .customId = reinterpret_cast<fastgltf::CustomBufferId>(buffer)
            };
        },
        nullptr,  // unmap
        nullptr   // deallocate
    );

    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (data.error() != fastgltf::Error::None) {
        return std::unexpected(data.error());
    }

    return parser.loadGltf(data.get(), path.parent_path(),
                           fastgltf::Options::LoadExternalBuffers);
}
```

### Загрузка меша в Vulkan буферы

```cpp
struct VulkanMeshData {
    VkBuffer vertexBuffer;
    VmaAllocation vertexAllocation;
    VkBuffer indexBuffer;
    VmaAllocation indexAllocation;
    uint32_t vertexCount;
    uint32_t indexCount;
};

std::expected<VulkanMeshData, fastgltf::Error> loadGltfMeshToVulkan(
    const std::filesystem::path& path,
    VmaAllocator allocator) {

    fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_basisu);
    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (data.error() != fastgltf::Error::None) {
        return std::unexpected(data.error());
    }

    auto options = fastgltf::Options::LoadExternalBuffers
                 | fastgltf::Options::DecomposeNodeMatrices;
    auto asset = parser.loadGltf(data.get(), path.parent_path(), options,
                                  fastgltf::Category::OnlyRenderable);
    if (asset.error() != fastgltf::Error::None) {
        return std::unexpected(asset.error());
    }

    if (asset->meshes.empty()) {
        return std::unexpected(fastgltf::Error::InvalidGltf);
    }

    const auto& mesh = asset->meshes[0];
    VulkanMeshData result{};
    uint32_t totalVertexCount = 0;
    uint32_t totalIndexCount = 0;

    // Подсчёт общего количества вершин и индексов
    for (const auto& primitive : mesh.primitives) {
        if (auto posAttr = primitive.findAttribute("POSITION");
            posAttr != primitive.attributes.cend()) {
            const auto& accessor = asset->accessors[posAttr->accessorIndex];
            totalVertexCount += static_cast<uint32_t>(accessor.count);
        }
        if (primitive.indicesAccessor.has_value()) {
            const auto& accessor = asset->accessors[*primitive.indicesAccessor];
            totalIndexCount += static_cast<uint32_t>(accessor.count);
        }
    }

    // Создание VMA буферов
    VkBufferCreateInfo vertexBufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = totalVertexCount * sizeof(glm::vec3),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VmaAllocationCreateInfo vertexAllocInfo{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
    };

    if (vmaCreateBuffer(allocator, &vertexBufferInfo, &vertexAllocInfo,
                        &result.vertexBuffer, &result.vertexAllocation, nullptr) != VK_SUCCESS) {
        return std::unexpected(fastgltf::Error::FileBufferAllocationFailed);
    }

    // Аналогично для index buffer
    VkBufferCreateInfo indexBufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = totalIndexCount * sizeof(uint32_t),
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VmaAllocationCreateInfo indexAllocInfo{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
    };

    if (vmaCreateBuffer(allocator, &indexBufferInfo, &indexAllocInfo,
                        &result.indexBuffer, &result.indexAllocation, nullptr) != VK_SUCCESS) {
        vmaDestroyBuffer(allocator, result.vertexBuffer, result.vertexAllocation);
        return std::unexpected(fastgltf::Error::FileBufferAllocationFailed);
    }

    result.vertexCount = totalVertexCount;
    result.indexCount = totalIndexCount;

    return result;
}
```

## Интеграция с ECS (Flecs)

### SoA-компоненты для мешей

```cpp
#include <flecs.h>
#include <print>
#include <span>
#include <fastgltf/core.hpp>

// SoA компоненты для Data-Oriented Design
struct PositionComponent {
    alignas(64) std::vector<glm::vec3> positions;
};

struct NormalComponent {
    alignas(64) std::vector<glm::vec3> normals;
};

struct TexcoordComponent {
    alignas(64) std::vector<glm::vec2> texcoords;
};

struct IndexComponent {
    alignas(64) std::vector<uint32_t> indices;
};

struct MeshBoundsComponent {
    glm::vec3 min;
    glm::vec3 max;
    float radius;
};

// Система загрузки glTF в ECS
class GltfLoaderSystem {
public:
    static std::expected<flecs::entity, fastgltf::Error> loadMeshToEcs(
        flecs::world& world,
        const std::filesystem::path& path,
        flecs::entity parent = flecs::entity()) {

        fastgltf::Parser parser;
        auto data = fastgltf::GltfDataBuffer::FromPath(path);
        if (data.error() != fastgltf::Error::None) {
            return std::unexpected(data.error());
        }

        auto asset = parser.loadGltf(data.get(), path.parent_path(),
                                     fastgltf::Options::LoadExternalBuffers,
                                     fastgltf::Category::OnlyRenderable);
        if (asset.error() != fastgltf::Error::None) {
            return std::unexpected(asset.error());
        }

        if (asset->meshes.empty()) {
            return std::unexpected(fastgltf::Error::InvalidGltf);
        }

        flecs::entity meshEntity;
        if (parent) {
            meshEntity = parent.child("mesh");
        } else {
            meshEntity = world.entity("loaded_mesh");
        }

        // Создание SoA компонентов
        auto& posComp = meshEntity.set<PositionComponent>({});
        auto& normComp = meshEntity.set<NormalComponent>({});
        auto& texComp = meshEntity.set<TexcoordComponent>({});
        auto& idxComp = meshEntity.set<IndexComponent>({});

        const auto& mesh = asset->meshes[0];
        glm::vec3 minBounds(FLT_MAX);
        glm::vec3 maxBounds(-FLT_MAX);

        for (const auto& primitive : mesh.primitives) {
            // Загрузка позиций
            if (auto posAttr = primitive.findAttribute("POSITION");
                posAttr != primitive.attributes.cend()) {

                const auto& accessor = asset->accessors[posAttr->accessorIndex];
                std::vector<glm::vec3> primitivePositions(accessor.count);

                if (auto err = fastgltf::copyFromAccessor<glm::vec3>(
                    *asset, accessor, primitivePositions.data()); err != fastgltf::Error::None) {
                    return std::unexpected(err);
                }

                // Обновление границ
                for (const auto& pos : primitivePositions) {
                    minBounds = glm::min(minBounds, pos);
                    maxBounds = glm::max(maxBounds, pos);
                }

                posComp.positions.insert(posComp.positions.end(),
                    primitivePositions.begin(), primitivePositions.end());
            }
        }

        // Установка компонента границ
        meshEntity.set<MeshBoundsComponent>({
            .min = minBounds,
            .max = maxBounds,
            .radius = glm::length(maxBounds - minBounds) * 0.5f
        });

        return meshEntity;
    }
};
```

### Система рендеринга с SoA

```cpp
// Система рендеринга, оптимизированная для SoA
class MeshRenderSystem {
public:
    MeshRenderSystem(flecs::world& world) {
        world.system<const PositionComponent, const NormalComponent,
                     const TexcoordComponent, const IndexComponent,
                     const MeshBoundsComponent>()
            .kind(flecs::OnUpdate)
            .each([](flecs::entity e,
                     const PositionComponent& pos,
                     const NormalComponent& norm,
                     const TexcoordComponent& tex,
                     const IndexComponent& idx,
                     const MeshBoundsComponent& bounds) {

                // Batch rendering с SoA данными
                if (pos.positions.empty() || idx.indices.empty()) {
                    return;
                }

                // GPU-driven рендеринг
                // 1. Передача SoA данных в GPU буферы
                // 2. Indirect drawing с compute shaders
                // 3. Frustum culling на основе bounds

                renderMeshSoA(pos.positions, norm.normals, tex.texcoords,
                              idx.indices, bounds);
            });
    }

private:
    void renderMeshSoA(const std::vector<glm::vec3>& positions,
                       const std::vector<glm::vec3>& normals,
                       const std::vector<glm::vec2>& texcoords,
                       const std::vector<uint32_t>& indices,
                       const MeshBoundsComponent& bounds) {
        // Здесь будет вызов DrawIndexed или DrawMeshTasks
        // Все данные уже лежат в плоских массивах (SoA),
        // готовые для пакетной передачи в GPU Command Buffer.

        // Пример логирования для отладки
        std::println("Rendering SoA Mesh: {} vertices, radius: {:.2f}",
                     positions.size(), bounds.radius);
    }
};

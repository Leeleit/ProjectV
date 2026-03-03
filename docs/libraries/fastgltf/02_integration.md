# Интеграция fastgltf в ProjectV

**Архитектурный контекст:** Fastgltf служит архитектурным мостом между иерархическими данными glTF и плоскими массивами
Data-Oriented Design в ProjectV. Библиотека выполняет роль архитектурного трансформатора, преобразующего древовидные
структуры glTF в оптимизированные для кэша процессора SoA (Structure of Arrays) форматы.

**Архитектурные цели интеграции в ProjectV:**

1. **Производительность**: SIMD-оптимизированный парсинг для воксельного рендеринга
2. **Data-Oriented Design**: Преобразование иерархии в плоские массивы для ECS (Flecs)
3. **GPU-ready архитектура**: Прямая загрузка в Vulkan буферы через VMA
4. **Современный C++26**: Использование `std::expected`, `std::span`, `std::print`
5. **ProjectV Integration**: Полная интеграция с MemoryManager, Logging и Profiling системами

## C++26 Module интеграция с ProjectV

```cpp
// ProjectV.FastGltf.cppm - C++26 Module для интеграции fastgltf с ProjectV
module;

// Global Module Fragment: Изоляция сторонних заголовков
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>

// ProjectV Core Dependencies
#include <projectv/core/memory/allocator.hpp>
#include <projectv/core/logging/log.hpp>
#include <projectv/core/profiling/tracy.hpp>

export module ProjectV.Assets.FastGltf;

import ProjectV.Core.Memory;
import ProjectV.Core.Logging;
import ProjectV.Core.Profiling;

namespace projectv::assets {

// Класс-интегратор fastgltf для ProjectV
class FastGltfLoader {
private:
    projectv::core::memory::ArenaAllocator arena_;
    projectv::core::memory::PoolAllocator pool_;

public:
    FastGltfLoader()
        : arena_(projectv::core::memory::MemoryManager::get_arena_allocator("fastgltf_temp")),
          pool_(projectv::core::memory::MemoryManager::get_pool_allocator("fastgltf_objects")) {

        PV_PROFILE_ZONE("FastGltfLoader::ctor");
        PV_LOG_INFO("FastGltfLoader initialized with ProjectV MemoryManager");
    }

    ~FastGltfLoader() {
        PV_PROFILE_ZONE("FastGltfLoader::dtor");
        PV_LOG_DEBUG("FastGltfLoader destroyed");
    }

    // Основной метод загрузки GLTF с интеграцией ProjectV
    std::expected<fastgltf::Asset, fastgltf::Error> load_with_projectv(
        const std::filesystem::path& path,
        fastgltf::Options options = fastgltf::Options::LoadExternalBuffers) {

        PV_PROFILE_ZONE("FastGltfLoader::load_with_projectv");
        PV_LOG_INFO("Loading GLTF: {}", path.string());

        // Настройка парсера с аллокаторами ProjectV
        fastgltf::Parser parser;
        setup_projectv_allocators(parser);

        // Загрузка данных через ProjectV MemoryManager
        auto data = load_data_with_projectv(path);
        if (data.error() != fastgltf::Error::None) {
            PV_LOG_ERROR("Failed to load GLTF data: {}", static_cast<int>(data.error()));
            return std::unexpected(data.error());
        }

        // Парсинг GLTF с профилированием
        PV_PROFILE_ZONE_NAMED("fastgltf_parse");
        auto asset = parser.loadGltf(data.get(), path.parent_path(), options);

        if (asset.error() != fastgltf::Error::None) {
            PV_LOG_ERROR("Failed to parse GLTF: {}", static_cast<int>(asset.error()));
            return std::unexpected(asset.error());
        }

        PV_LOG_INFO("GLTF loaded successfully: {} meshes, {} textures",
                   asset->meshes.size(), asset->textures.size());
        return asset;
    }

private:
    void setup_projectv_allocators(fastgltf::Parser& parser) {
        // Настройка кастомных аллокаторов для временных буферов
        parser.setBufferAllocationCallback(
            [this](void* userPointer, size_t bufferSize,
                   fastgltf::BufferAllocateFlags flags) -> fastgltf::BufferInfo {

                PV_PROFILE_ZONE("FastGltfLoader::buffer_allocate");

                // Используем ArenaAllocator для временных буферов загрузки
                void* memory = arena_.allocate(bufferSize, 16);
                if (!memory) {
                    PV_LOG_ERROR("Failed to allocate {} bytes for GLTF buffer", bufferSize);
                    return fastgltf::BufferInfo{};
                }

                PV_LOG_DEBUG("Allocated GLTF buffer: {} bytes from ArenaAllocator", bufferSize);
                return fastgltf::BufferInfo{
                    .mappedMemory = memory,
                    .customId = reinterpret_cast<fastgltf::CustomBufferId>(memory)
                };
            },
            [this](void* userPointer, fastgltf::CustomBufferId customId) {
                // Освобождение через ArenaAllocator
                PV_PROFILE_ZONE("FastGltfLoader::buffer_deallocate");
                void* memory = reinterpret_cast<void*>(customId);
                arena_.deallocate(memory);
                PV_LOG_DEBUG("Deallocated GLTF buffer from ArenaAllocator");
            },
            nullptr  // unmap не требуется для ArenaAllocator
        );
    }

    fastgltf::Expected<fastgltf::GltfDataBuffer> load_data_with_projectv(
        const std::filesystem::path& path) {

        PV_PROFILE_ZONE("FastGltfLoader::load_data_with_projectv");

        // Используем PoolAllocator для объектов fastgltf
        auto* dataBuffer = pool_.construct<fastgltf::GltfDataBuffer>();
        if (!dataBuffer) {
            PV_LOG_ERROR("Failed to allocate GltfDataBuffer from PoolAllocator");
            return fastgltf::unexpected(fastgltf::Error::FileBufferAllocationFailed);
        }

        auto result = dataBuffer->loadFromFile(path);
        if (result.error() != fastgltf::Error::None) {
            pool_.destroy(dataBuffer);
            return fastgltf::unexpected(result.error());
        }

        return *dataBuffer;
    }
};

} // namespace projectv::assets
```

## CMake интеграция с ProjectV

### Полная интеграция для ProjectV

```cmake
# ProjectV CMake интеграция fastgltf
# В корневом CMakeLists.txt ProjectV
add_subdirectory(external/fastgltf)

# Настройка fastgltf для интеграции с ProjectV
set(FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL ON CACHE BOOL
    "Отключить pmr-аллокатор для совместимости с ProjectV MemoryManager" FORCE)
set(FASTGLTF_COMPILE_AS_CPP20 OFF CACHE BOOL "Использовать C++26" FORCE)
set(FASTGLTF_ENABLE_CPP_MODULES OFF CACHE BOOL "Отключить модули C++20" FORCE)
set(FASTGLTF_BUILD_TESTS OFF CACHE BOOL "Отключить тесты" FORCE)
set(FASTGLTF_BUILD_EXAMPLES OFF CACHE BOOL "Отключить примеры" FORCE)

# Создание модуля ProjectV для fastgltf
add_library(ProjectV.FastGltf INTERFACE)
target_link_libraries(ProjectV.FastGltf INTERFACE
    fastgltf::fastgltf
    ProjectV.Core.Memory
    ProjectV.Core.Logging
    ProjectV.Core.Profiling)

# Настройка C++26 стандарта
target_compile_features(ProjectV.FastGltf INTERFACE cxx_std_26)
set_target_properties(ProjectV.FastGltf PROPERTIES
    CXX_STANDARD 26
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Интеграция с системой аллокаторов ProjectV
target_compile_definitions(ProjectV.FastGltf INTERFACE
    FASTGLTF_DISABLE_CUSTOM_MEMORY_POOL
    PROJECTV_FASTGLTF_INTEGRATION
)

# Подключение к основному движку ProjectV
target_link_libraries(ProjectV.Engine PRIVATE ProjectV.FastGltf)
```

### Конфигурация для высокопроизводительных систем

```cmake
# Оптимизации для Data-Oriented Design и воксельного рендеринга
target_compile_options(ProjectV.FastGltf INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:/O2 /Ob2 /Oi /Ot /GL>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O3 -march=native -mtune=native>
)

# SIMD оптимизации для парсинга GLTF
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(ProjectV.FastGltf INTERFACE
        -msse4.2
        -mavx2
        -mfma
        -ftree-vectorize
    )
endif()

# Предкомпилированные заголовки для ускорения сборки
if(MSVC)
    target_precompile_headers(ProjectV.FastGltf INTERFACE
        <fastgltf/core.hpp>
        <fastgltf/tools.hpp>
        <projectv/core/memory/allocator.hpp>
    )
endif()
```

### Интеграция с системой сборки ProjectV

```cmake
# В основном CMakeLists.txt ProjectV
include(ExternalProject)

# Автоматическая настройка fastgltf при сборке ProjectV
macro(projectv_setup_fastgltf)
    message(STATUS "Setting up fastgltf for ProjectV integration")

    # Проверка зависимостей
    find_package(Vulkan REQUIRED)
    find_package(glm REQUIRED)

    # Настройка путей для fastgltf
    set(FASTGLTF_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/fastgltf/include")
    set(FASTGLTF_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/fastgltf/src")

    # Добавление fastgltf в проект
    add_subdirectory(external/fastgltf)

    # Создание цели интеграции
    add_library(projectv_fastgltf_integration OBJECT)
    target_sources(projectv_fastgltf_integration PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/assets/fastgltf_integration.cpp
    )

    target_link_libraries(projectv_fastgltf_integration PRIVATE
        fastgltf::fastgltf
        ProjectV.Core.Memory
        ProjectV.Core.Logging
        Vulkan::Vulkan
    )

    # Экспорт настроек для других модулей
    set(PROJECTV_FASTGLTF_FOUND TRUE PARENT_SCOPE)
    set(PROJECTV_FASTGLTF_LIBRARIES projectv_fastgltf_integration PARENT_SCOPE)
    set(PROJECTV_FASTGLTF_INCLUDE_DIRS
        ${FASTGLTF_INCLUDE_DIR}
        ${Vulkan_INCLUDE_DIRS}
        ${glm_INCLUDE_DIRS}
        PARENT_SCOPE
    )

    message(STATUS "fastgltf integration ready for ProjectV")
endmacro()
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
#include <expected>
#include <span>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>

// ProjectV интеграция
#include <projectv/core/memory/allocator.hpp>
#include <projectv/core/logging/log.hpp>
#include <projectv/core/profiling/tracy.hpp>

// SoA структура для Data-Oriented Design с интеграцией ProjectV
struct MeshSoA {
    // Structure of Arrays (SoA) для вершин с выравниванием для ProjectV MemoryManager
    alignas(64) std::vector<glm::vec3> positions;  // Выравнивание для кэш-линий
    alignas(64) std::vector<glm::vec3> normals;
    alignas(64) std::vector<glm::vec2> texcoords;
    alignas(64) std::vector<uint32_t> indices;

    // Метаданные с выравниванием для GPU
    alignas(16) std::vector<uint32_t> materialIds;
    alignas(16) std::vector<glm::mat4> transforms;

    // ProjectV аллокатор для управления памятью
    projectv::core::memory::ArenaAllocator* allocator{nullptr};
};

std::expected<MeshSoA, fastgltf::Error> convertToSoA(
    const fastgltf::Asset& asset,
    std::size_t meshIndex = 0,
    projectv::core::memory::ArenaAllocator* tempAllocator = nullptr) {

    PV_PROFILE_ZONE("convertToSoA");

    if (meshIndex >= asset.meshes.size()) {
        PV_LOG_ERROR("Invalid mesh index: {} (total meshes: {})", meshIndex, asset.meshes.size());
        return std::unexpected(fastgltf::Error::InvalidGltf);
    }

    const auto& mesh = asset.meshes[meshIndex];
    MeshSoA result;
    result.allocator = tempAllocator;

    PV_LOG_DEBUG("Converting mesh {} to SoA format ({} primitives)", meshIndex, mesh.primitives.size());

    for (const auto& primitive : mesh.primitives) {
        PV_PROFILE_ZONE_NAMED("process_primitive");

        // Извлечение позиций
        if (auto posAttr = primitive.findAttribute("POSITION");
            posAttr != primitive.attributes.cend()) {

            const auto& accessor = asset.accessors[posAttr->accessorIndex];
            PV_LOG_TRACE("Processing POSITION attribute: {} vertices", accessor.count);

            // Используем временный аллокатор ProjectV если предоставлен
            std::vector<glm::vec3> primitivePositions;
            if (tempAllocator) {
                // Выделяем память через ArenaAllocator для временных данных
                void* tempMemory = tempAllocator->allocate(accessor.count * sizeof(glm::vec3), 16);
                if (tempMemory) {
                    primitivePositions.resize(accessor.count);
                    PV_LOG_TRACE("Allocated {} bytes for temporary positions via ProjectV ArenaAllocator",
                                accessor.count * sizeof(glm::vec3));
                }
            } else {
                primitivePositions.resize(accessor.count);
            }

            if (auto err = fastgltf::copyFromAccessor<glm::vec3>(
                asset, accessor, primitivePositions.data()); err != fastgltf::Error::None) {

                PV_LOG_ERROR("Failed to copy POSITION data: {}", static_cast<int>(err));
                return std::unexpected(err);
            }

            // Добавляем в SoA массив
            result.positions.insert(result.positions.end(),
                primitivePositions.begin(), primitivePositions.end());

            PV_LOG_TRACE("Added {} positions to SoA array (total: {})",
                        primitivePositions.size(), result.positions.size());
        }

        // Аналогично для нормалей, текстурных координат с профилированием
        // ...
    }

    PV_LOG_INFO("Successfully converted mesh {} to SoA: {} vertices, {} indices",
               meshIndex, result.positions.size(), result.indices.size());
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
#include <span>
#include <fastgltf/core.hpp>

// ProjectV интеграция
#include <projectv/core/memory/allocator.hpp>
#include <projectv/core/logging/log.hpp>
#include <projectv/core/profiling/tracy.hpp>

// SoA компоненты для Data-Oriented Design с интеграцией ProjectV
struct PositionComponent {
    alignas(64) std::vector<glm::vec3> positions;
    projectv::core::memory::ArenaAllocator* allocator{nullptr};
};

struct NormalComponent {
    alignas(64) std::vector<glm::vec3> normals;
    projectv::core::memory::ArenaAllocator* allocator{nullptr};
};

struct TexcoordComponent {
    alignas(64) std::vector<glm::vec2> texcoords;
    projectv::core::memory::ArenaAllocator* allocator{nullptr};
};

struct IndexComponent {
    alignas(64) std::vector<uint32_t> indices;
    projectv::core::memory::ArenaAllocator* allocator{nullptr};
};

struct MeshBoundsComponent {
    glm::vec3 min;
    glm::vec3 max;
    float radius;
};

// Система загрузки glTF в ECS с интеграцией ProjectV
class GltfLoaderSystem {
public:
    static std::expected<flecs::entity, fastgltf::Error> loadMeshToEcs(
        flecs::world& world,
        const std::filesystem::path& path,
        flecs::entity parent = flecs::entity(),
        projectv::core::memory::ArenaAllocator* tempAllocator = nullptr) {

        PV_PROFILE_ZONE("GltfLoaderSystem::loadMeshToEcs");
        PV_LOG_INFO("Loading GLTF to ECS: {}", path.string());

        fastgltf::Parser parser;
        auto data = fastgltf::GltfDataBuffer::FromPath(path);
        if (data.error() != fastgltf::Error::None) {
            PV_LOG_ERROR("Failed to load GLTF data: {}", static_cast<int>(data.error()));
            return std::unexpected(data.error());
        }

        auto asset = parser.loadGltf(data.get(), path.parent_path(),
                                     fastgltf::Options::LoadExternalBuffers,
                                     fastgltf::Category::OnlyRenderable);
        if (asset.error() != fastgltf::Error::None) {
            PV_LOG_ERROR("Failed to parse GLTF: {}", static_cast<int>(asset.error()));
            return std::unexpected(asset.error());
        }

        if (asset->meshes.empty()) {
            PV_LOG_ERROR("No meshes found in GLTF file");
            return std::unexpected(fastgltf::Error::InvalidGltf);
        }

        flecs::entity meshEntity;
        if (parent) {
            meshEntity = parent.child("mesh");
            PV_LOG_DEBUG("Creating child mesh entity under parent {}", parent.name());
        } else {
            meshEntity = world.entity("loaded_mesh");
            PV_LOG_DEBUG("Creating standalone mesh entity");
        }

        // Создание SoA компонентов с аллокаторами ProjectV
        auto& posComp = meshEntity.set<PositionComponent>({});
        auto& normComp = meshEntity.set<NormalComponent>({});
        auto& texComp = meshEntity.set<TexcoordComponent>({});
        auto& idxComp = meshEntity.set<IndexComponent>({});

        // Настройка аллокаторов если предоставлены
        if (tempAllocator) {
            posComp.allocator = tempAllocator;
            normComp.allocator = tempAllocator;
            texComp.allocator = tempAllocator;
            idxComp.allocator = tempAllocator;
            PV_LOG_TRACE("Using ProjectV ArenaAllocator for ECS components");
        }

        const auto& mesh = asset->meshes[0];
        glm::vec3 minBounds(FLT_MAX);
        glm::vec3 maxBounds(-FLT_MAX);

        PV_LOG_DEBUG("Processing mesh with {} primitives", mesh.primitives.size());

        for (const auto& primitive : mesh.primitives) {
            PV_PROFILE_ZONE_NAMED("process_primitive_ecs");

            // Загрузка позиций
            if (auto posAttr = primitive.findAttribute("POSITION");
                posAttr != primitive.attributes.cend()) {

                const auto& accessor = asset->accessors[posAttr->accessorIndex];
                PV_LOG_TRACE("Loading POSITION data: {} vertices", accessor.count);

                std::vector<glm::vec3> primitivePositions;
                if (tempAllocator) {
                    // Используем временный аллокатор для данных
                    void* tempMemory = tempAllocator->allocate(accessor.count * sizeof(glm::vec3), 16);
                    if (tempMemory) {
                        primitivePositions.resize(accessor.count);
                        PV_LOG_TRACE("Allocated {} bytes via ProjectV ArenaAllocator",
                                    accessor.count * sizeof(glm::vec3));
                    }
                } else {
                    primitivePositions.resize(accessor.count);
                }

                if (auto err = fastgltf::copyFromAccessor<glm::vec3>(
                    *asset, accessor, primitivePositions.data()); err != fastgltf::Error::None) {

                    PV_LOG_ERROR("Failed to copy POSITION data: {}", static_cast<int>(err));
                    return std::unexpected(err);
                }

                // Обновление границ
                for (const auto& pos : primitivePositions) {
                    minBounds = glm::min(minBounds, pos);
                    maxBounds = glm::max(maxBounds, pos);
                }

                posComp.positions.insert(posComp.positions.end(),
                    primitivePositions.begin(), primitivePositions.end());

                PV_LOG_TRACE("Added {} positions to ECS component", primitivePositions.size());
            }
        }

        // Установка компонента границ
        float radius = glm::length(maxBounds - minBounds) * 0.5f;
        meshEntity.set<MeshBoundsComponent>({
            .min = minBounds,
            .max = maxBounds,
            .radius = radius
        });

        PV_LOG_INFO("Mesh loaded to ECS: entity={}, vertices={}, bounds=({:.2f}, {:.2f}, {:.2f})->({:.2f}, {:.2f}, {:.2f}), radius={:.2f}",
                   meshEntity.name(), posComp.positions.size(),
                   minBounds.x, minBounds.y, minBounds.z,
                   maxBounds.x, maxBounds.y, maxBounds.z,
                   radius);

        return meshEntity;
    }
};
```

### Система рендеринга с SoA и интеграцией ProjectV

```cpp
// Система рендеринга, оптимизированная для SoA с интеграцией ProjectV
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

                PV_PROFILE_ZONE("MeshRenderSystem::render");
                
                // Batch rendering с SoA данными
                if (pos.positions.empty() || idx.indices.empty()) {
                    PV_LOG_TRACE("Skipping empty mesh: entity={}", e.name());
                    return;
                }

                PV_LOG_DEBUG("Rendering SoA mesh: entity={}, vertices={}, indices={}, radius={:.2f}",
                           e.name(), pos.positions.size(), idx.indices.size(), bounds.radius);

                // GPU-driven рендеринг с интеграцией ProjectV
                // 1. Передача SoA данных в GPU буферы через ProjectV MemoryManager
                // 2. Indirect drawing с compute shaders
                // 3. Frustum culling на основе bounds с оптимизациями ProjectV

                renderMeshSoA(pos.positions, norm.normals, tex.texcoords,
                              idx.indices, bounds, e);
            });
    }

private:
    void renderMeshSoA(const std::vector<glm::vec3>& positions,
                       const std::vector<glm::vec3>& normals,
                       const std::vector<glm::vec2>& texcoords,
                       const std::vector<uint32_t>& indices,
                       const MeshBoundsComponent& bounds,
                       flecs::entity entity) {
        
        PV_PROFILE_ZONE_NAMED("render_mesh_soa");
        
        // Здесь будет вызов DrawIndexed или DrawMeshTasks
        // Все данные уже лежат в плоских массивах (SoA),
        // готовые для пакетной передачи в GPU Command Buffer через ProjectV.

        // Пример логирования для отладки с интеграцией ProjectV
        PV_LOG_TRACE("Rendering SoA Mesh: entity={}, vertices={}, indices={}, radius={:.2f}",
                    entity.name(), positions.size(), indices.size(), bounds.radius);

        // Оптимизации ProjectV для SoA рендеринга:
        // 1. Batch processing через ArenaAllocator для временных данных
        // 2. SIMD-оптимизированные операции для матричных преобразований
        // 3. GPU-driven pipeline с минимальным CPU overhead
        // 4. Tracy hooks для профилирования каждого этапа рендеринга
        
        // Frustum culling с оптимизациями ProjectV
        if (!isMeshInFrustum(bounds)) {
            PV_LOG_TRACE("Mesh culled: entity={}, radius={:.2f}", entity.name(), bounds.radius);
            return;
        }
        
        // Подготовка данных для GPU с использованием аллокаторов ProjectV
        prepareGpuData(positions, normals, texcoords, indices);
        
        // Вызов рендеринга через Vulkan API с интеграцией ProjectV
        submitRenderCommands();
        
        PV_LOG_TRACE("Mesh rendered successfully: entity={}", entity.name());
    }
    
    bool isMeshInFrustum(const MeshBoundsComponent& bounds) {
        PV_PROFILE_ZONE("frustum_culling");
        // Реализация frustum culling с SIMD оптимизациями ProjectV
        // ...
        return true; // Временная заглушка
    }
    
    void prepareGpuData(const std::vector<glm::vec3>& positions,
                       const std::vector<glm::vec3>& normals,
                       const std::vector<glm::vec2>& texcoords,
                       const std::vector<uint32_t>& indices) {
        PV_PROFILE_ZONE("prepare_gpu_data");
        // Подготовка данных для GPU с использованием аллокаторов ProjectV
        // ...
    }
    
    void submitRenderCommands() {
        PV_PROFILE_ZONE("submit_render_commands");
        // Отправка команд рендеринга через Vulkan API
        // ...
    }
};
```

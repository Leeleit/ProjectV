# Интеграция meshoptimizer в ProjectV

> Как подключить meshoptimizer к воксельному движку на Vulkan 1.4 + Flecs ECS + VMA с интеграцией ProjectV MemoryManager, Logging и Profiling.

---

## CMake

### Подключение как подмодуль

```cmake
# root CMakeLists.txt
add_subdirectory(external/meshoptimizer)

# Отключаем ненужные опции для ProjectV
set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "Disable demo for ProjectV")
set(MESHOPT_BUILD_EXAMPLES OFF CACHE BOOL "Disable examples for ProjectV")

target_link_libraries(ProjectV PRIVATE meshoptimizer)

# Подключение ProjectV модулей
target_link_libraries(ProjectV PRIVATE
    projectv_core_memory
    projectv_core_logging
    projectv_core_profiling
)
```

### Опции сборки

| Опция                       | По умолчанию | Описание                             | ProjectV рекомендация |
|-----------------------------|--------------|--------------------------------------|-----------------------|
| `MESHOPT_BUILD_SHARED_LIBS` | OFF          | Собирать как shared library          | OFF (static)          |
| `MESHOPT_BUILD_DEMO`        | OFF          | Сборка демо                          | OFF                   |
| `MESHOPT_BUILD_EXAMPLES`    | OFF          | Сборка примеров                      | OFF                   |
| `MESHOPT_STABLE_EXPORTS`    | OFF          | Экспортировать только стабильные API | OFF                   |

### Требования к компилятору

| Компилятор | Минимальная версия | ProjectV версия |
|------------|--------------------|-----------------|
| GCC        | 11 (для C++26)     | 13+             |
| Clang      | 16                 | 17+             |
| MSVC       | 2022 (19.x)        | 2022 17.8+      |

---

## C++26 Module

### Global Module Fragment для изоляции заголовков

```cpp
// meshoptimizer.ixx
module;

// Global Module Fragment: изоляция сторонних заголовков
#include <meshoptimizer.h>
#include <vk_mem_alloc.h>
#include <flecs.h>

export module projectv.meshoptimizer;

import projectv.core.memory;
import projectv.core.logging;
import projectv.core.profiling;

export {
    // Re-export meshoptimizer типы
    using meshopt_Meshlet = ::meshopt_Meshlet;
    using meshopt_Bounds = ::meshopt_Bounds;
    using meshopt_Stream = ::meshopt_Stream;
    
    // Re-export meshoptimizer функции
    using ::meshopt_generateVertexRemap;
    using ::meshopt_remapVertexBuffer;
    using ::meshopt_remapIndexBuffer;
    using ::meshopt_optimizeVertexCache;
    using ::meshopt_optimizeVertexFetch;
    using ::meshopt_optimizeOverdraw;
    using ::meshopt_simplify;
    using ::meshopt_buildMeshlets;
    using ::meshopt_optimizeMeshlet;
    using ::meshopt_computeMeshletBounds;
    using ::meshopt_encodeVertexBuffer;
    using ::meshopt_decodeVertexBuffer;
    using ::meshopt_encodeIndexBuffer;
    using ::meshopt_decodeIndexBuffer;
    using ::meshopt_analyzeVertexCache;
    using ::meshopt_analyzeOverdraw;
    using ::meshopt_analyzeVertexFetch;
}
```

### MemoryManager Integration

meshoptimizer — stateless библиотека, работающая с предоставленными буферами. ProjectV использует ArenaAllocator для временных данных.

```cpp
export namespace projectv::meshoptimizer {

struct MeshOptimizerArena {
    projectv::core::memory::ArenaAllocator& arena;
    
    // Выделение временного буфера для remap таблицы
    [[nodiscard]]
    auto allocateRemapTable(size_t vertex_count) -> std::span<uint32_t> {
        return arena.allocate<uint32_t>(vertex_count);
    }
    
    // Выделение временного буфера для индексов
    [[nodiscard]]
    auto allocateIndices(size_t index_count) -> std::span<uint32_t> {
        return arena.allocate<uint32_t>(index_count);
    }
    
    // Выделение временного буфера для вершин
    template<typename Vertex>
    [[nodiscard]]
    auto allocateVertices(size_t vertex_count) -> std::span<Vertex> {
        return arena.allocate<Vertex>(vertex_count);
    }
    
    // Сброс арены (вызывается в конце кадра/задачи)
    void reset() {
        arena.reset();
    }
};

// Оптимизация с использованием ArenaAllocator
template<typename Vertex>
struct OptimizedMeshData {
    std::span<Vertex> vertices;
    std::span<uint32_t> indices;
    float acmr = 0.0f;
    float atvr = 0.0f;
};

template<typename Vertex>
[[nodiscard]]
auto optimizeMeshWithArena(
    std::span<const Vertex> src_vertices,
    std::span<const uint32_t> src_indices,
    MeshOptimizerArena& arena
) -> std::expected<OptimizedMeshData<Vertex>, std::string> {
    ZoneScopedN("meshoptimizer::optimizeMeshWithArena");
    
    // 1. Выделение временных буферов из арены
    auto remap = arena.allocateRemapTable(src_vertices.size());
    if (remap.empty()) {
        return std::unexpected("Failed to allocate remap table");
    }
    
    // 2. Генерация remap таблицы
    const size_t unique_count = meshopt_generateVertexRemap(
        remap.data(),
        src_indices.data(),
        src_indices.size(),
        src_vertices.data(),
        src_vertices.size(),
        sizeof(Vertex)
    );
    
    if (unique_count == 0) {
        return std::unexpected("Failed to generate remap table");
    }
    
    // 3. Выделение буферов для результата
    auto vertices = arena.allocateVertices<Vertex>(unique_count);
    auto indices = arena.allocateIndices(src_indices.size());
    
    if (vertices.empty() || indices.empty()) {
        return std::unexpected("Failed to allocate result buffers");
    }
    
    // 4. Применение remap
    meshopt_remapVertexBuffer(
        vertices.data(),
        src_vertices.data(),
        src_vertices.size(),
        sizeof(Vertex),
        remap.data()
    );
    
    meshopt_remapIndexBuffer(
        indices.data(),
        src_indices.data(),
        src_indices.size(),
        remap.data()
    );
    
    // 5. Vertex cache optimization
    meshopt_optimizeVertexCache(
        indices.data(),
        indices.data(),
        indices.size(),
        unique_count
    );
    
    // 6. Vertex fetch optimization
    meshopt_optimizeVertexFetch(
        vertices.data(),
        indices.data(),
        indices.size(),
        vertices.data(),
        unique_count,
        sizeof(Vertex)
    );
    
    // 7. Анализ результата
    const auto stats = meshopt_analyzeVertexCache(
        indices.data(),
        indices.size(),
        unique_count,
        16, 0, 0
    );
    
    return OptimizedMeshData<Vertex>{
        .vertices = vertices,
        .indices = indices,
        .acmr = stats.acmr,
        .atvr = stats.atvr
    };
}

} // namespace projectv::meshoptimizer
```

### Logging Integration

Все сообщения meshoptimizer перенаправляются в ProjectV Logging System:

```cpp
export namespace projectv::meshoptimizer {

class MeshOptimizerLogger {
public:
    static void logOptimizationStart(size_t vertex_count, size_t index_count) {
        projectv::core::Log::info("MeshOptimizer",
            "Starting optimization: {} vertices, {} indices",
            vertex_count, index_count);
    }
    
    static void logOptimizationResult(float acmr, float atvr, size_t unique_vertices) {
        projectv::core::Log::debug("MeshOptimizer",
            "Optimization complete: ACMR={:.3f}, ATVR={:.3f}, unique={}",
            acmr, atvr, unique_vertices);
    }
    
    static void logError(std::string_view message) {
        projectv::core::Log::error("MeshOptimizer", "{}", message);
    }
    
    static void logWarning(std::string_view message) {
        projectv::core::Log::warning("MeshOptimizer", "{}", message);
    }
};

} // namespace projectv::meshoptimizer
```

### Profiling Integration

Tracy hooks для всех операций meshoptimizer:

```cpp
export namespace projectv::meshoptimizer {

class MeshOptimizerProfiler {
public:
    struct Scope {
        Scope(std::string_view name) {
            ZoneScopedN(name.data());
            TracyPlot("MeshOptimizer/ActiveOperations", 1);
        }
        
        ~Scope() {
            TracyPlot("MeshOptimizer/ActiveOperations", -1);
        }
    };
    
    static void plotVertexCount(size_t count) {
        TracyPlot("MeshOptimizer/VertexCount", static_cast<int64_t>(count));
    }
    
    static void plotIndexCount(size_t count) {
        TracyPlot("MeshOptimizer/IndexCount", static_cast<int64_t>(count));
    }
    
    static void plotAcmr(float acmr) {
        TracyPlot("MeshOptimizer/ACMR", static_cast<double>(acmr));
    }
    
    static void plotAtvr(float atvr) {
        TracyPlot("MeshOptimizer/ATVR", static_cast<double>(atvr));
    }
};

} // namespace projectv::meshoptimizer
```

---

## Архитектура интеграции

### Поток данных

```
glTF / Voxel Data
        │
        ▼
   fastgltf Parser
        │
        ├── Проверка: сжат ли меш?
        │
        ▼ (EXT_meshopt_compression)
   meshoptimizer Pipeline
        │
        ├── generateVertexRemap
        ├── optimizeVertexCache
        ├── optimizeVertexFetch
        │
        ▼
   Vulkan + VMA
        │
        ├── Vertex Buffer (DEVICE_LOCAL)
        ├── Index Buffer (DEVICE_LOCAL)
        └── Upload через staging
```

### ECS компоненты

```cpp
#include <flecs.h>
#include <meshoptimizer.h>
#include <vk_mem_alloc.h>

// Основной компонент меши
struct StaticMesh {
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkBuffer index_buffer = VK_NULL_HANDLE;
    VmaAllocation vertex_allocation = VK_NULL_HANDLE;
    VmaAllocation index_allocation = VK_NULL_HANDLE;
    uint32_t vertex_count = 0;
    uint32_t index_count = 0;

    // Метрики оптимизации
    float acmr = 0.0f;
    float atvr = 0.0f;
};

// LOD компонент
struct MeshLODs {
    static constexpr size_t MAX_LODS = 4;

    std::array<StaticMesh, MAX_LODS> lods;
    std::array<float, MAX_LODS> errors;
    size_t lod_count = 0;
};

// Meshlet данные для mesh shading
struct MeshletGeometry {
    VkBuffer meshlets_buffer = VK_NULL_HANDLE;
    VkBuffer vertices_buffer = VK_NULL_HANDLE;
    VkBuffer triangles_buffer = VK_NULL_HANDLE;
    VkBuffer bounds_buffer = VK_NULL_HANDLE;
    VmaAllocation meshlets_allocation = VK_NULL_HANDLE;
    VmaAllocation vertices_allocation = VK_NULL_HANDLE;
    VmaAllocation triangles_allocation = VK_NULL_HANDLE;
    VmaAllocation bounds_allocation = VK_NULL_HANDLE;

    uint32_t meshlet_count = 0;
};

// Теги
struct OptimizedTag {};
struct NeedsOptimizationTag {};
struct NeedsMeshletsTag {};
```

---

## Vulkan Интеграция с ProjectV

### Оптимизация и загрузка меши с интеграцией ProjectV

```cpp
// Использование ProjectV Module
import projectv.meshoptimizer;
import projectv.core.memory;
import projectv.core.logging;
import projectv.core.profiling;
import projectv.vulkan;

#include <vk_mem_alloc.h>
#include <span>
#include <expected>

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

struct MeshCreationError {
    enum class Code {
        OutOfMemory,
        InvalidData,
        VulkanError
    } code;
    std::string message;
};

struct OptimizedMesh {
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkBuffer index_buffer = VK_NULL_HANDLE;
    VmaAllocation vertex_allocation = VK_NULL_HANDLE;
    VmaAllocation index_allocation = VK_NULL_HANDLE;
    uint32_t vertex_count = 0;
    uint32_t index_count = 0;
    float acmr = 0.0f;
    float atvr = 0.0f;
};

class MeshOptimizer {
public:
    MeshOptimizer(
        VmaAllocator allocator, 
        VkDevice device,
        VkQueue transfer_queue, 
        VkCommandPool transfer_pool,
        projectv::core::memory::ArenaAllocator& arena
    )
        : allocator_(allocator)
        , device_(device)
        , transfer_queue_(transfer_queue)
        , transfer_pool_(transfer_pool)
        , arena_(arena) 
    {
        projectv::core::Log::info("MeshOptimizer", 
            "Initialized with arena capacity: {} bytes", arena.capacity());
    }

    [[nodiscard]]
    auto optimizeAndUpload(
        std::span<const Vertex> src_vertices,
        std::span<const uint32_t> src_indices
    ) -> std::expected<OptimizedMesh, MeshCreationError> {
        ZoneScopedN("MeshOptimizer::optimizeAndUpload");
        
        projectv::meshoptimizer::MeshOptimizerLogger::logOptimizationStart(
            src_vertices.size(), src_indices.size());
        
        TracyPlot("MeshOptimizer/InputVertices", static_cast<int64_t>(src_vertices.size()));
        TracyPlot("MeshOptimizer/InputIndices", static_cast<int64_t>(src_indices.size()));

        // CPU: оптимизация с использованием ArenaAllocator
        auto optimized = optimizeCPUWithArena(src_vertices, src_indices);
        if (!optimized) {
            projectv::meshoptimizer::MeshOptimizerLogger::logError(
                "CPU optimization failed: " + optimized.error().message);
            return std::unexpected(optimized.error());
        }

        // GPU: загрузка
        auto result = uploadToGPU(*optimized);
        if (result) {
            projectv::meshoptimizer::MeshOptimizerLogger::logOptimizationResult(
                result->acmr, result->atvr, result->vertex_count);
            
            projectv::meshoptimizer::MeshOptimizerProfiler::plotAcmr(result->acmr);
            projectv::meshoptimizer::MeshOptimizerProfiler::plotAtvr(result->atvr);
        }

        // Сброс арены для следующей задачи
        arena_.reset();

        return result;
    }

private:
    VmaAllocator allocator_;
    VkDevice device_;
    VkQueue transfer_queue_;
    VkCommandPool transfer_pool_;
    projectv::core::memory::ArenaAllocator& arena_;

    struct CPUOptimizedData {
        std::span<Vertex> vertices;
        std::span<uint32_t> indices;
        float acmr = 0.0f;
        float atvr = 0.0f;
    };

    [[nodiscard]]
    auto optimizeCPUWithArena(
        std::span<const Vertex> src_vertices,
        std::span<const uint32_t> src_indices
    ) -> std::expected<CPUOptimizedData, MeshCreationError> {
        ZoneScopedN("MeshOptimizer::optimizeCPUWithArena");
        
        // Используем ProjectV meshoptimizer интеграцию с ArenaAllocator
        projectv::meshoptimizer::MeshOptimizerArena mesh_arena{arena_};
        
        auto result = projectv::meshoptimizer::optimizeMeshWithArena<Vertex>(
            src_vertices, src_indices, mesh_arena);
        
        if (!result) {
            return std::unexpected{MeshCreationError{
                .code = MeshCreationError::Code::InvalidData,
                .message = std::string(result.error())
            }};
        }

        return CPUOptimizedData{
            .vertices = result->vertices,
            .indices = result->indices,
            .acmr = result->acmr,
            .atvr = result->atvr
        };
    }

    [[nodiscard]]
    auto uploadToGPU(const CPUOptimizedData& data)
        -> std::expected<OptimizedMesh, MeshCreationError> {
        OptimizedMesh result;
        result.vertex_count = static_cast<uint32_t>(data.vertices.size());
        result.index_count = static_cast<uint32_t>(data.indices.size());
        result.acmr = data.acmr;
        result.atvr = data.atvr;

        // Staging buffer для вершин
        auto vb_staging = createStagingBuffer(
            data.vertices.data(),
            data.vertices.size() * sizeof(Vertex)
        );
        if (!vb_staging) {
            return std::unexpected{vb_staging.error()};
        }

        // Staging buffer для индексов
        auto ib_staging = createStagingBuffer(
            data.indices.data(),
            data.indices.size() * sizeof(uint32_t)
        );
        if (!ib_staging) {
            return std::unexpected{ib_staging.error()};
        }

        // Device local vertex buffer
        VkBufferCreateInfo vb_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        vb_info.size = data.vertices.size() * sizeof(Vertex);
        vb_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo vb_alloc = {};
        vb_alloc.usage = VMA_MEMORY_USAGE_AUTO;
        vb_alloc.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        if (vmaCreateBuffer(allocator_, &vb_info, &vb_alloc,
                           &result.vertex_buffer, &result.vertex_allocation, nullptr)
            != VK_SUCCESS) {
            return std::unexpected{MeshCreationError{
                .code = MeshCreationError::Code::VulkanError,
                .message = "Failed to create vertex buffer"
            }};
        }

        // Device local index buffer
        VkBufferCreateInfo ib_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        ib_info.size = data.indices.size() * sizeof(uint32_t);
        ib_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo ib_alloc = {};
        ib_alloc.usage = VMA_MEMORY_USAGE_AUTO;
        ib_alloc.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        if (vmaCreateBuffer(allocator_, &ib_info, &ib_alloc,
                           &result.index_buffer, &result.index_allocation, nullptr)
            != VK_SUCCESS) {
            vmaDestroyBuffer(allocator_, result.vertex_buffer, result.vertex_allocation);
            return std::unexpected{MeshCreationError{
                .code = MeshCreationError::Code::VulkanError,
                .message = "Failed to create index buffer"
            }};
        }

        // Copy commands
        VkCommandBuffer cmd = beginSingleTimeCommands();

        VkBufferCopy vb_copy = {};
        vb_copy.size = vb_info.size;
        vkCmdCopyBuffer(cmd, vb_staging->buffer, result.vertex_buffer, 1, &vb_copy);

        VkBufferCopy ib_copy = {};
        ib_copy.size = ib_info.size;
        vkCmdCopyBuffer(cmd, ib_staging->buffer, result.index_buffer, 1, &ib_copy);

        endSingleTimeCommands(cmd);

        return result;
    }

    struct StagingBuffer {
        VkBuffer buffer;
        VmaAllocation allocation;
    };

    [[nodiscard]]
    auto createStagingBuffer(const void* data, VkDeviceSize size)
        -> std::expected<StagingBuffer, MeshCreationError> {
        StagingBuffer result;

        VkBufferCreateInfo info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = size;
        info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo alloc = {};
        alloc.usage = VMA_MEMORY_USAGE_AUTO;
        alloc.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        VmaAllocationInfo mapped_info;
        if (vmaCreateBuffer(allocator_, &info, &alloc,
                           &result.buffer, &result.allocation, &mapped_info)
            != VK_SUCCESS) {
            return std::unexpected{MeshCreationError{
                .code = MeshCreationError::Code::VulkanError,
                .message = "Failed to create staging buffer"
            }};
        }

        std::memcpy(mapped_info.pMappedData, data, size);

        return result;
    }

    VkCommandBuffer beginSingleTimeCommands() {
        VkCommandBufferAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = transfer_pool_,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(device_, &alloc_info, &cmd);

        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };

        vkBeginCommandBuffer(cmd, &begin_info);
        return cmd;
    }

    void endSingleTimeCommands(VkCommandBuffer cmd) {
        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd
        };

        vkQueueSubmit(transfer_queue_, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(transfer_queue_);

        vkFreeCommandBuffers(device_, transfer_pool_, 1, &cmd);
    }
};
```

---

## ECS Системы с ProjectV Integration

### Система оптимизации при загрузке с ProjectV Logging

```cpp
import projectv.core.logging;
import projectv.core.profiling;

class MeshOptimizationSystem {
public:
    MeshOptimizationSystem(flecs::world& world,
                           std::shared_ptr<MeshOptimizer> optimizer)
        : optimizer_(optimizer) {

        // Система оптимизации мешей при загрузке
        world.system<StaticMesh, NeedsOptimizationTag>("OptimizeMesh")
            .kind(flecs::OnLoad)
            .each([this](flecs::entity e, StaticMesh& mesh) {
                ZoneScopedN("MeshOptimizationSystem::OptimizeMesh");
                
                if (mesh.vertex_buffer != VK_NULL_HANDLE) {
                    return; // Уже загружено
                }

                // Получаем исходные данные (из кэша или loader)
                auto source = getSourceMeshData(e);
                if (!source) {
                    projectv::core::Log::error("MeshOptimizationSystem",
                        "Failed to get source mesh data for entity {}: {}",
                        e.name(), source.error().message);
                    return;
                }

                // Оптимизация и загрузка на GPU
                auto result = optimizer_->optimizeAndUpload(
                    source->vertices, source->indices
                );

                if (result) {
                    mesh = std::move(*result);
                    e.remove<NeedsOptimizationTag>();
                    e.add<OptimizedTag>();
                    
                    projectv::core::Log::debug("MeshOptimizationSystem",
                        "Mesh optimized for entity {}: {} vertices, {} indices, ACMR={:.3f}",
                        e.name(), mesh.vertex_count, mesh.index_count, mesh.acmr);
                } else {
                    projectv::core::Log::error("MeshOptimizationSystem",
                        "Mesh optimization failed for entity {}: {}",
                        e.name(), result.error().message);
                }
            });

        // Система генерации LOD
        world.system<StaticMesh, MeshLODs>("GenerateLODs")
            .kind(flecs::OnLoad)
            .with<NeedsOptimizationTag>()  // После оптимизации
            .each([this](flecs::entity e, StaticMesh& base, MeshLODs& lods) {
                ZoneScopedN("MeshOptimizationSystem::GenerateLODs");
                
                if (lods.lod_count > 0) return; // Уже сгенерировано

                // Генерация LOD цепочки
                auto result = generateLODs(e, base);
                if (result) {
                    lods = std::move(*result);
                    
                    projectv::core::Log::debug("MeshOptimizationSystem",
                        "Generated {} LODs for entity {}", lods.lod_count, e.name());
                }
            });
    }

private:
    std::shared_ptr<MeshOptimizer> optimizer_;

    struct SourceMeshData {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    [[nodiscard]]
    std::expected<SourceMeshData, MeshCreationError>
    getSourceMeshData(flecs::entity e) {
        // Получение исходных данных из компонента MeshSource
        // Требует наличия компонента MeshSource с сырыми данными вершин/индексов
        const auto* source = e.get<MeshSource>();
        if (!source) {
            return std::unexpected{MeshCreationError{
                .code = MeshCreationError::Code::InvalidData,
                .message = "Entity has no MeshSource component"
            }};
        }

        return SourceMeshData{
            .vertices = source->vertices,
            .indices = source->indices
        };
    }

    [[nodiscard]]
    std::expected<MeshLODs, MeshCreationError>
    generateLODs(flecs::entity e, const StaticMesh& base) {
        MeshLODs result;

        // Загружаем данные с GPU для упрощения (упрощение работает на CPU)
        // Для этого используем VMA mapped memory или читаем через vkCmdCopyBuffer
        auto source_data = downloadFromGPU(base);
        if (!source_data) {
            return std::unexpected(source_data.error());
        }

        auto& [vertices, indices] = *source_data;

        // LOD 0 - оптимизированная базовая мешь
        result.lods[0] = base;
        result.lods[0].vertex_count = static_cast<uint32_t>(vertices.size());
        result.lods[0].index_count = static_cast<uint32_t>(indices.size());
        result.errors[0] = 0.0f;
        result.lod_count = 1;

        // Генерация последующих LOD
        std::vector<uint32_t> prev_indices = indices;

        for (size_t i = 1; i < MeshLODs::MAX_LODS; ++i) {
            const float ratios[] = {0.75f, 0.5f, 0.25f, 0.1f};
            size_t target = indices.size() * ratios[i - 1];

            std::vector<uint32_t> lod_indices(prev_indices.size());
            float lod_error = 0.0f;

            const size_t simplified = meshopt_simplify(
                lod_indices.data(),
                prev_indices.data(),
                prev_indices.size(),
                vertices.data(),
                vertices.size(),
                sizeof(Vertex),
                target,
                1e-2f,  // 1% ошибка
                meshopt_SimplifyLockBorder,
                &lod_error
            );

            if (simplified < 12) break;

            lod_indices.resize(simplified);

            // Оптимизация для vertex cache
            meshopt_optimizeVertexCache(
                lod_indices.data(),
                lod_indices.data(),
                simplified,
                vertices.size()
            );

            result.errors[i] = result.errors[i-1] + lod_error;

            // Создание GPU буферов для LOD через uploadToGPU
            auto lod_result = uploadLODToGPU(vertices, lod_indices);
            if (lod_result) {
                result.lods[static_cast<size_t>(i)] = std::move(*lod_result);
            }

            result.lod_count++;

            prev_indices = std::move(lod_indices);
        }

        return result;
    }

private:
    // Загрузка данных с GPU обратно на CPU для LOD генерации
    [[nodiscard]]
    std::expected<std::pair<std::vector<Vertex>, std::vector<uint32_t>>, MeshCreationError>
    downloadFromGPU(const StaticMesh& mesh) {
        std::vector<Vertex> vertices(mesh.vertex_count);
        std::vector<uint32_t> indices(mesh.index_count);

        // Используем VMA для чтения данных
        void* vertex_ptr = nullptr;
        VmaAllocationInfo vertex_info;
        if (vmaMapMemory(get_allocator(), mesh.vertex_allocation, &vertex_ptr) != VK_SUCCESS) {
            return std::unexpected{MeshCreationError{
                .code = MeshCreationError::Code::VulkanError,
                .message = "Failed to map vertex memory"
            }};
        }
        std::memcpy(vertices.data(), vertex_ptr, mesh.vertex_count * sizeof(Vertex));
        vmaUnmapMemory(get_allocator(), mesh.vertex_allocation);

        void* index_ptr = nullptr;
        if (vmaMapMemory(get_allocator(), mesh.index_allocation, &index_ptr) != VK_SUCCESS) {
            return std::unexpected{MeshCreationError{
                .code = MeshCreationError::Code::VulkanError,
                .message = "Failed to map index memory"
            }};
        }
        std::memcpy(indices.data(), index_ptr, mesh.index_count * sizeof(uint32_t));
        vmaUnmapMemory(get_allocator(), mesh.index_allocation);

        return std::make_pair(std::move(vertices), std::move(indices));
    }

    // Загрузка LOD данных на GPU
    [[nodiscard]]
    std::expected<StaticMesh, MeshCreationError>
    uploadLODToGPU(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
        // Используем staging буферы для загрузки
        auto vb_staging = createStagingBuffer(vertices.data(), vertices.size() * sizeof(Vertex));
        if (!vb_staging) {
            return std::unexpected(vb_staging.error());
        }

        auto ib_staging = createStagingBuffer(indices.data(), indices.size() * sizeof(uint32_t));
        if (!ib_staging) {
            vmaDestroyBuffer(get_allocator(), vb_staging->buffer, vb_staging->allocation);
            return std::unexpected(ib_staging.error());
        }

        // Device local buffers
        StaticMesh lod_mesh;
        lod_mesh.vertex_count = static_cast<uint32_t>(vertices.size());
        lod_mesh.index_count = static_cast<uint32_t>(indices.size());

        // Создание vertex buffer
        VkBufferCreateInfo vb_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        vb_info.size = vertices.size() * sizeof(Vertex);
        vb_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo vb_alloc = {};
        vb_alloc.usage = VMA_MEMORY_USAGE_AUTO;
        vb_alloc.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        if (vmaCreateBuffer(get_allocator(), &vb_info, &vb_alloc,
                           &lod_mesh.vertex_buffer, &lod_mesh.vertex_allocation, nullptr) != VK_SUCCESS) {
            return std::unexpected{MeshCreationError{
                .code = MeshCreationError::Code::VulkanError,
                .message = "Failed to create LOD vertex buffer"
            }};
        }

        // Создание index buffer
        VkBufferCreateInfo ib_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        ib_info.size = indices.size() * sizeof(uint32_t);
        ib_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        if (vmaCreateBuffer(get_allocator(), &ib_info, &vb_alloc,
                           &lod_mesh.index_buffer, &lod_mesh.index_allocation, nullptr) != VK_SUCCESS) {
            vmaDestroyBuffer(get_allocator(), lod_mesh.vertex_buffer, lod_mesh.vertex_allocation);
            return std::unexpected{MeshCreationError{
                .code = MeshCreationError::Code::VulkanError,
                .message = "Failed to create LOD index buffer"
            }};
        }

        // Копирование через command buffer
        VkCommandBuffer cmd = beginSingleTimeCommands();

        VkBufferCopy vb_copy = {.size = vb_info.size};
        vkCmdCopyBuffer(cmd, vb_staging->buffer, lod_mesh.vertex_buffer, 1, &vb_copy);

        VkBufferCopy ib_copy = {.size = ib_info.size};
        vkCmdCopyBuffer(cmd, ib_staging->buffer, lod_mesh.index_buffer, 1, &ib_copy);

        endSingleTimeCommands(cmd);

        // Очистка staging буферов
        vmaDestroyBuffer(get_allocator(), vb_staging->buffer, vb_staging->allocation);
        vmaDestroyBuffer(get_allocator(), ib_staging->buffer, ib_staging->allocation);

        return lod_mesh;
    }

    VmaAllocator get_allocator() const {
        // Требуется передать allocator при инициализации системы
        return VK_NULL_HANDLE;
    }
};
```

---

## fastgltf Интеграция

### Оптимизация при загрузке glTF

```cpp
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <meshoptimizer.h>
#include <span>

struct GLTFVertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

auto loadAndOptimizeGLTFMesh(
    const fastgltf::Asset& asset,
    const fastgltf::Mesh& mesh,
    size_t primitive_index
) -> std::pair<std::vector<GLTFVertex>, std::vector<uint32_t>> {
    const auto& primitive = mesh.primitives[primitive_index];

    // Загрузка позиций
    auto* position_attr = primitive.findAttribute("POSITION");
    auto& position_accessor = asset.accessors[position_attr->second];

    std::vector<glm::vec3> positions(position_accessor.count);
    fastgltf::copyFromAccessor(asset, position_accessor, positions.data());

    // Загрузка нормалей (опционально)
    std::vector<glm::vec3> normals;
    if (auto* normal_attr = primitive.findAttribute("NORMAL")) {
        normals.resize(position_accessor.count);
        fastgltf::copyFromAccessor(asset, asset.accessors[normal_attr->second],
                                   normals.data());
    }

    // Загрузка UV (опционально)
    std::vector<glm::vec2> uvs;
    if (auto* uv_attr = primitive.findAttribute("TEXCOORD_0")) {
        uvs.resize(position_accessor.count);
        fastgltf::copyFromAccessor(asset, asset.accessors[uv_attr->second],
                                   uvs.data());
    }

    // Сборка вершин
    std::vector<GLTFVertex> vertices(position_accessor.count);
    for (size_t i = 0; i < position_accessor.count; ++i) {
        vertices[i].px = positions[i].x;
        vertices[i].py = positions[i].y;
        vertices[i].pz = positions[i].z;

        if (!normals.empty()) {
            vertices[i].nx = normals[i].x;
            vertices[i].ny = normals[i].y;
            vertices[i].nz = normals[i].z;
        }

        if (!uvs.empty()) {
            vertices[i].u = uvs[i].x;
            vertices[i].v = uvs[i].y;
        }
    }

    // Загрузка индексов
    std::vector<uint32_t> indices;
    if (primitive.indicesAccessor) {
        auto& index_accessor = asset.accessors[primitive.indicesAccessor.value()];
        indices.resize(index_accessor.count);
        fastgltf::copyFromAccessor(asset, index_accessor, indices.data());
    } else {
        // Генерация индексов для неиндексированной геометрии
        indices.resize(position_accessor.count);
        for (size_t i = 0; i < position_accessor.count; ++i) {
            indices[i] = static_cast<uint32_t>(i);
        }
    }

    // Оптимизация: indexing + cache + fetch
    return optimizeGLTF(vertices, indices);
}

auto optimizeGLTF(
    std::vector<GLTFVertex>& vertices,
    std::vector<uint32_t>& indices
) -> std::pair<std::vector<GLTFVertex>, std::vector<uint32_t>> {
    // 1. Генерация remap
    std::vector<uint32_t> remap(vertices.size());
    const size_t unique_count = meshopt_generateVertexRemap(
        remap.data(),
        indices.data(),
        indices.size(),
        vertices.data(),
        vertices.size(),
        sizeof(GLTFVertex)
    );

    // 2. Применение remap
    std::vector<GLTFVertex> unique_vertices(unique_count);
    meshopt_remapVertexBuffer(
        unique_vertices.data(),
        vertices.data(),
        vertices.size(),
        sizeof(GLTFVertex),
        remap.data()
    );

    meshopt_remapIndexBuffer(
        indices.data(),
        indices.data(),
        indices.size(),
        remap.data()
    );

    vertices = std::move(unique_vertices);

    // 3. Vertex cache
    meshopt_optimizeVertexCache(
        indices.data(),
        indices.data(),
        indices.size(),
        vertices.size()
    );

    // 4. Vertex fetch
    meshopt_optimizeVertexFetch(
        vertices.data(),
        indices.data(),
        indices.size(),
        vertices.data(),
        vertices.size(),
        sizeof(GLTFVertex)
    );

    return {std::move(vertices), std::move(indices)};
}
```

---

## Streaming и Сжатие

### Кодирование для хранения

```cpp
#include <meshoptimizer.h>
#include <vector>
#include <span>

struct CompressedMesh {
    std::vector<uint8_t> vertex_data;
    std::vector<uint8_t> index_data;
    uint32_t vertex_count;
    uint32_t index_count;
};

auto compressMesh(
    std::span<const Vertex> vertices,
    std::span<const uint32_t> indices
) -> CompressedMesh {
    CompressedMesh result;
    result.vertex_count = static_cast<uint32_t>(vertices.size());
    result.index_count = static_cast<uint32_t>(indices.size());

    // Кодирование вершин
    const size_t vb_bound = meshopt_encodeVertexBufferBound(
        vertices.size(), sizeof(Vertex)
    );
    result.vertex_data.resize(vb_bound);
    result.vertex_data.resize(
        meshopt_encodeVertexBuffer(
            result.vertex_data.data(), vb_bound,
            vertices.data(), vertices.size(), sizeof(Vertex)
        )
    );

    // Кодирование индексов
    const size_t ib_bound = meshopt_encodeIndexBufferBound(
        indices.size(), vertices.size()
    );
    result.index_data.resize(ib_bound);
    result.index_data.resize(
        meshopt_encodeIndexBuffer(
            result.index_data.data(), ib_bound,
            indices.data(), indices.size()
        )
    );

    return result;
}

struct MeshDecodeError {
    enum class Code {
        VertexDecodeFailed,
        IndexDecodeFailed
    } code;
    std::string message;
};

auto decompressMesh(
    std::span<const uint8_t> vertex_data,
    std::span<const uint8_t> index_data,
    uint32_t vertex_count,
    uint32_t index_count
) -> std::expected<std::pair<std::vector<Vertex>, std::vector<uint32_t>>, MeshDecodeError> {
    std::vector<Vertex> vertices(vertex_count);
    std::vector<uint32_t> indices(index_count);

    const int vres = meshopt_decodeVertexBuffer(
        vertices.data(), vertex_count, sizeof(Vertex),
        vertex_data.data(), vertex_data.size()
    );

    if (vres != 0) {
        return std::unexpected{MeshDecodeError{
            .code = MeshDecodeError::Code::VertexDecodeFailed,
            .message = "Vertex decode failed: " + std::to_string(vres)
        }};
    }

    const int ires = meshopt_decodeIndexBuffer(
        indices.data(), index_count,
        index_data.data(), index_data.size()
    );

    if (ires != 0) {
        return std::unexpected{MeshDecodeError{
            .code = MeshDecodeError::Code::IndexDecodeFailed,
            .message = "Index decode failed: " + std::to_string(ires)
        }};
    }

    return std::make_pair(std::move(vertices), std::move(indices));
}
```

---

## Meshlet Pipeline для Mesh Shading

```cpp
struct MeshletGPUData {
    VkBuffer meshlets_buffer = VK_NULL_HANDLE;
    VkBuffer vertices_buffer = VK_NULL_HANDLE;
    VkBuffer triangles_buffer = VK_NULL_HANDLE;
    VkBuffer bounds_buffer = VK_NULL_HANDLE;
    VmaAllocation meshlets_alloc;
    VmaAllocation vertices_alloc;
    VmaAllocation triangles_alloc;
    VmaAllocation bounds_alloc;
    uint32_t meshlet_count = 0;
};

auto buildMeshlets(
    VmaAllocator allocator,
    std::span<const Vertex> vertices,
    std::span<const uint32_t> indices
) -> MeshletGPUData {
    constexpr size_t max_vertices = 64;
    constexpr size_t max_triangles = 126;
    constexpr float cone_weight = 0.25f;

    // Оценка
    const size_t max_meshlets = meshopt_buildMeshletsBound(
        indices.size(), max_vertices, max_triangles
    );

    // CPU данные
    std::vector<meshopt_Meshlet> meshlets(max_meshlets);
    std::vector<uint32_t> meshlet_vertices(max_meshlets * max_vertices);
    std::vector<uint8_t> meshlet_triangles(max_meshlets * max_triangles * 3);

    // Построение
    const size_t meshlet_count = meshopt_buildMeshlets(
        meshlets.data(),
        meshlet_vertices.data(),
        meshlet_triangles.data(),
        indices.data(),
        indices.size(),
        &vertices[0].px,
        vertices.size(),
        sizeof(Vertex),
        max_vertices,
        max_triangles,
        cone_weight
    );

    meshlets.resize(meshlet_count);

    // Обрезка
    const auto& last = meshlets.back();
    meshlet_vertices.resize(last.vertex_offset + last.vertex_count);
    meshlet_triangles.resize(last.triangle_offset + last.triangle_count * 3);

    // Оптимизация
    for (auto& m : meshlets) {
        meshopt_optimizeMeshlet(
            &meshlet_vertices[m.vertex_offset],
            &meshlet_triangles[m.triangle_offset],
            m.triangle_count,
            m.vertex_count
        );
    }

    // Bounds для culling
    std::vector<meshopt_Bounds> bounds(meshlet_count);
    for (size_t i = 0; i < meshlet_count; ++i) {
        const auto& m = meshlets[i];
        bounds[i] = meshopt_computeMeshletBounds(
            &meshlet_vertices[m.vertex_offset],
            &meshlet_triangles[m.triangle_offset],
            m.triangle_count,
            &vertices[0].px,
            vertices.size(),
            sizeof(Vertex)
        );
    }

    // Загрузка meshlet данных на GPU через VMA
    MeshletGPUData result;
    result.meshlet_count = static_cast<uint32_t>(meshlet_count);

    // Создание и загрузка meshlets buffer
    if (!uploadMeshletBuffer(allocator, meshlets.data(), meshlets.size() * sizeof(meshlets[0]),
                             result.meshlets_buffer, result.meshlets_alloc)) {
        return result; // Частичный результат
    }

    // Создание и загрузка vertices buffer
    if (!uploadMeshletBuffer(allocator, meshlet_vertices.data(), meshlet_vertices.size() * sizeof(meshlet_vertices[0]),
                             result.vertices_buffer, result.vertices_alloc)) {
        vmaDestroyBuffer(allocator, result.meshlets_buffer, result.meshlets_alloc);
        return result;
    }

    // Создание и загрузка triangles buffer
    if (!uploadMeshletBuffer(allocator, meshlet_triangles.data(), meshlet_triangles.size() * sizeof(meshlet_triangles[0]),
                             result.triangles_buffer, result.triangles_alloc)) {
        vmaDestroyBuffer(allocator, result.meshlets_buffer, result.meshlets_alloc);
        vmaDestroyBuffer(allocator, result.vertices_buffer, result.vertices_alloc);
        return result;
    }

    // Создание и загрузка bounds buffer
    if (!uploadMeshletBuffer(allocator, bounds.data(), bounds.size() * sizeof(bounds[0]),
                             result.bounds_buffer, result.bounds_alloc)) {
        vmaDestroyBuffer(allocator, result.meshlets_buffer, result.meshlets_alloc);
        vmaDestroyBuffer(allocator, result.vertices_buffer, result.vertices_alloc);
        vmaDestroyBuffer(allocator, result.triangles_buffer, result.triangles_alloc);
        return result;
    }

    return result;
}

[[nodiscard]]
bool uploadMeshletBuffer(VmaAllocator allocator, const void* data, size_t size,
                         VkBuffer& buffer, VmaAllocation& allocation) {
    // Staging буфер
    VkBuffer staging = VK_NULL_HANDLE;
    VmaAllocation staging_alloc = VK_NULL_HANDLE;

    VkBufferCreateInfo staging_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    staging_info.size = size;
    staging_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo staging_alloc_info = {};
    staging_alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
    staging_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                              VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    VmaAllocationInfo mapped;
    if (vmaCreateBuffer(allocator, &staging_info, &staging_alloc_info,
                        &staging, &staging_alloc, &mapped) != VK_SUCCESS) {
        return false;
    }

    std::memcpy(mapped.pMappedData, data, size);
    vmaUnmapMemory(allocator, staging_alloc);

    // Device local буфер
    VkBufferCreateInfo buffer_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
    alloc_info.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if (vmaCreateBuffer(allocator, &buffer_info, &alloc_info,
                        &buffer, &allocation, nullptr) != VK_SUCCESS) {
        vmaDestroyBuffer(allocator, staging, staging_alloc);
        return false;
    }

    // Копирование (упрощено - в реальном коде нужен command buffer)
    // Примечание: требуется выполнить vkCmdCopyBuffer через transient command buffer
    vmaDestroyBuffer(allocator, staging, staging_alloc);

    return true;
}
```

---

## Ограничения

### Когда НЕ использовать meshoptimizer

| Ситуация                          | Причина                            |
|-----------------------------------|------------------------------------|
| Runtime упрощение вокселей        | Топология меняется динамически     |
| Очень маленькие меши (<100 триг.) | Накладные расходы превышают выгоду |
| Procedural geometry каждый кадр   | Слишком медленно                   |

### Производительность

| Операция              | Скорость   | Потокобезопасность |
|-----------------------|------------|--------------------|
| `optimizeVertexCache` | O(n log n) | Да (разные меши)   |
| `simplify`            | O(n log n) | Да                 |
| `buildMeshlets`       | O(n)       | Да                 |
| `decodeVertexBuffer`  | 3–6 GB/s   | Да                 |

---

## Полный пример: ECS загрузка меша

```cpp
// main.cpp - инициализация системы
#include <flecs.h>
#include <meshoptimizer.h>
#include <vk_mem_alloc.h>

struct MeshSource {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

int main() {
    // Инициализация Flecs
    flecs::world world;

    // Регистрация компонентов
    world.component<MeshSource>();
    world.component<StaticMesh>();
    world.component<MeshLODs>();
    world.component<MeshletGeometry>();
    world.component<NeedsOptimizationTag>();
    world.component<OptimizedTag>();

    // Создание optimizer
    auto mesh_optimizer = std::make_shared<MeshOptimizer>(
        allocator, device, transfer_queue, transfer_pool
    );

    // Добавление системы
    MeshOptimizationSystem system(world, mesh_optimizer);

    // Загрузка меша (пример)
    world.entity("PlayerMesh")
        .set(MeshSource{{/* vertices */}, {/* indices */}})
        .add<NeedsOptimizationTag>();

    // Запуск systems (в игровом цикле)
    world.progress();

    return 0;
}

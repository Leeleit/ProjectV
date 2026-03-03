# Zstd: Интеграция в ProjectV

## C++26 Module Integration

```cpp
// ProjectV.Compression.Zstd.cpp - C++26 Module для интеграции Zstd
module;

// Global Module Fragment: изоляция сторонних заголовков
#include <zstd.h>
#include <projectv/core/memory_manager.hxx>
#include <projectv/core/log.hxx>
#include <projectv/core/profiling.hxx>

export module ProjectV.Compression.Zstd;

import ProjectV.Core.Memory;
import ProjectV.Core.Log;
import ProjectV.Core.Profiling;

namespace projectv::compression {

// Класс-интегратор Zstd для ProjectV
class ZstdDecompressor {
public:
    struct Config {
        int compression_level = 5;
        bool use_dictionary = false;
        std::span<const uint8_t> dictionary;
        projectv::core::memory::PoolAllocator* pool_allocator = nullptr;
    };

    explicit ZstdDecompressor(Config config)
        : config_(config)
        , pool_allocator_(config.pool_allocator
            ? config.pool_allocator
            : &projectv::core::memory::get_default_pool_allocator())
    {
        PV_PROFILE_FUNCTION();

        if (auto result = init(); !result) {
            PV_LOG_ERROR("Failed to initialize ZstdDecompressor: {}", result.error());
        }
    }

    ~ZstdDecompressor() {
        cleanup();
    }

    // Декомпрессия с использованием MemoryManager
    std::expected<std::span<uint8_t>, std::string> decompress(
        std::span<const uint8_t> compressed_data
    ) {
        PV_PROFILE_FUNCTION();

        if (!dctx_) {
            return std::unexpected("Decompressor not initialized");
        }

        // Определяем размер оригинальных данных
        const unsigned long long original_size = ZSTD_getFrameContentSize(
            compressed_data.data(), compressed_data.size()
        );

        if (original_size == ZSTD_CONTENTSIZE_ERROR) {
            PV_LOG_ERROR("Invalid compressed data frame");
            return std::unexpected("Invalid compressed data");
        }

        if (original_size == ZSTD_CONTENTSIZE_UNKNOWN) {
            PV_LOG_ERROR("Unknown content size - streaming decompression required");
            return std::unexpected("Unknown content size");
        }

        // Выделяем память через PoolAllocator
        auto* arena = pool_allocator_->allocate_arena(
            "ZstdDecompression",
            static_cast<size_t>(original_size)
        );

        if (!arena) {
            PV_LOG_ERROR("Failed to allocate arena for decompression");
            return std::unexpected("Memory allocation failed");
        }

        auto* decompressed_data = arena->allocate<uint8_t>(original_size);
        if (!decompressed_data) {
            PV_LOG_ERROR("Failed to allocate decompression buffer");
            return std::unexpected("Buffer allocation failed");
        }

        // Выполняем декомпрессию
        const size_t result = ZSTD_decompressDCtx(
            dctx_,
            decompressed_data, original_size,
            compressed_data.data(), compressed_data.size()
        );

        if (ZSTD_isError(result)) {
            PV_LOG_ERROR("Decompression failed: {}", ZSTD_getErrorName(result));
            arena->deallocate(decompressed_data);
            return std::unexpected(ZSTD_getErrorName(result));
        }

        if (result != original_size) {
            PV_LOG_WARN("Decompressed size mismatch: expected {}, got {}",
                       original_size, result);
        }

        PV_LOG_DEBUG("Decompressed {} bytes to {} bytes",
                    compressed_data.size(), result);

        return std::span<uint8_t>(decompressed_data, result);
    }

    // Сжатие с использованием MemoryManager
    std::expected<std::span<uint8_t>, std::string> compress(
        std::span<const uint8_t> data,
        int level = -1
    ) {
        PV_PROFILE_FUNCTION();

        if (!cctx_) {
            return std::unexpected("Compressor not initialized");
        }

        const size_t bound = ZSTD_compressBound(data.size());

        // Выделяем память через PoolAllocator
        auto* arena = pool_allocator_->allocate_arena(
            "ZstdCompression",
            bound
        );

        if (!arena) {
            PV_LOG_ERROR("Failed to allocate arena for compression");
            return std::unexpected("Memory allocation failed");
        }

        auto* compressed_data = arena->allocate<uint8_t>(bound);
        if (!compressed_data) {
            PV_LOG_ERROR("Failed to allocate compression buffer");
            return std::unexpected("Buffer allocation failed");
        }

        // Настраиваем уровень сжатия если указан
        if (level != -1) {
            ZSTD_CCtx_setParameter(cctx_, ZSTD_c_compressionLevel, level);
        }

        // Выполняем сжатие
        const size_t result = ZSTD_compress2(
            cctx_,
            compressed_data, bound,
            data.data(), data.size()
        );

        if (ZSTD_isError(result)) {
            PV_LOG_ERROR("Compression failed: {}", ZSTD_getErrorName(result));
            arena->deallocate(compressed_data);
            return std::unexpected(ZSTD_getErrorName(result));
        }

        PV_LOG_DEBUG("Compressed {} bytes to {} bytes (ratio: {:.2f}%)",
                    data.size(), result,
                    (static_cast<double>(result) / data.size()) * 100.0);

        return std::span<uint8_t>(compressed_data, result);
    }

private:
    std::expected<void, std::string> init() {
        cctx_ = ZSTD_createCCtx();
        dctx_ = ZSTD_createDCtx();

        if (!cctx_ || !dctx_) {
            cleanup();
            return std::unexpected("Failed to create Zstd contexts");
        }

        // Настраиваем контекст сжатия
        ZSTD_CCtx_setParameter(cctx_, ZSTD_c_compressionLevel, config_.compression_level);
        ZSTD_CCtx_setParameter(cctx_, ZSTD_c_checksumFlag, 1);

        // Настраиваем словарь если требуется
        if (config_.use_dictionary && !config_.dictionary.empty()) {
            cdict_ = ZSTD_createCDict(
                config_.dictionary.data(),
                config_.dictionary.size(),
                config_.compression_level
            );

            ddict_ = ZSTD_createDDict(
                config_.dictionary.data(),
                config_.dictionary.size()
            );

            if (!cdict_ || !ddict_) {
                cleanup();
                return std::unexpected("Failed to create Zstd dictionaries");
            }

            ZSTD_CCtx_refCDict(cctx_, cdict_);
            ZSTD_DCtx_refDDict(dctx_, ddict_);
        }

        PV_LOG_INFO("ZstdDecompressor initialized with level {}", config_.compression_level);
        return {};
    }

    void cleanup() {
        if (cdict_) ZSTD_freeCDict(cdict_);
        if (ddict_) ZSTD_freeDDict(ddict_);
        if (cctx_) ZSTD_freeCCtx(cctx_);
        if (dctx_) ZSTD_freeDCtx(dctx_);

        cdict_ = nullptr;
        ddict_ = nullptr;
        cctx_ = nullptr;
        dctx_ = nullptr;
    }

    Config config_;
    projectv::core::memory::PoolAllocator* pool_allocator_;
    ZSTD_CCtx* cctx_ = nullptr;
    ZSTD_DCtx* dctx_ = nullptr;
    ZSTD_CDict* cdict_ = nullptr;
    ZSTD_DDict* ddict_ = nullptr;
};

} // namespace projectv::compression
```

## CMake-конфигурация для ProjectV

### Интеграция через add_subdirectory

```cmake
# В ProjectV/CMakeLists.txt
add_subdirectory(external/zstd/build/cmake)

# Настройка Zstd для ProjectV
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "No CLI tools")
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "No tests")
set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "No contrib")
set(ZSTD_BUILD_STATIC ON CACHE BOOL "Static only")
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "No shared libs")
set(ZSTD_MULTITHREAD_SUPPORT ON CACHE BOOL "Enable threading")

# Подключение модулей ProjectV
target_link_libraries(ProjectV.PRIVATE
    libzstd_static
  projectv_core_memory
  projectv_core_log
  projectv_core_profiling
)

target_compile_definitions(ProjectV.PRIVATE
  ZSTD_MULTITHREAD
    ZSTD_STATIC_LINKING
)

# Подключение заголовков
target_include_directories(ProjectV.PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/external/zstd/lib
  ${CMAKE_CURRENT_SOURCE_DIR}/external/zstd/lib/common
)
```

### Вариант 2: Git Submodule

```cmake
add_subdirectory(external/zstd/build/cmake)

target_link_libraries(ProjectV PRIVATE
    libzstd_static
)

target_compile_definitions(ProjectV PRIVATE
    ZSTD_MULTITHREAD
)
```

---

## Компиляция Zstd для Vulkan-ориентированного движка

### Критические флаги

```cmake
# Отключаем всё лишнее
set(ZSTD_BUILD_PROGRAMS OFF)
set(ZSTD_BUILD_TESTS OFF)
set(ZSTD_BUILD_CONTRIB OFF)
set(ZSTD_BUILD_STATIC ON)
set(ZSTD_BUILD_SHARED OFF)

# Включаем многопоточность
set(ZSTD_MULTITHREAD_SUPPORT ON)

# Оптимизация размера кода
add_compile_options($<$<CONFIG:Release>:-Oz>)
```

### Проверка архитектуры

```cmake
# x64-only для ProjectV
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(STATUS "Zstd: 64-bit build - OK")
else()
    message(FATAL_ERROR "Zstd: 32-bit builds not supported")
endif()
```

---

## Интеграция с Vulkan 1.4

### Сценарий: Сжатие GPU-буферов перед передачей

```cpp
// Использование ZstdDecompressor для Vulkan-буферов
#include <projectv/compression/zstd.hxx>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <span>
#include <expected>

namespace projectv::vulkan {

// Bridge для сжатия Vulkan-буферов с использованием ProjectV MemoryManager
class VulkanBufferCompressor {
public:
    struct Config {
        int compression_level = 5;
        bool use_dictionary = false;
        std::span<const uint8_t> dictionary;
        projectv::core::memory::PoolAllocator* pool_allocator = nullptr;
    };

    explicit VulkanBufferCompressor(Config config)
        : zstd_decompressor_({
            .compression_level = config.compression_level,
            .use_dictionary = config.use_dictionary,
            .dictionary = config.dictionary,
            .pool_allocator = config.pool_allocator
        })
    {
        PV_PROFILE_FUNCTION();
        PV_LOG_INFO("VulkanBufferCompressor initialized with level {}",
                   config.compression_level);
    }

    // Сжатие данных после чтения из GPU-буфера
    std::expected<std::span<uint8_t>, std::string> compress_gpu_buffer(
        VmaAllocator allocator,
        VkBuffer src_buffer,
        VkDeviceSize buffer_size
    ) {
        PV_PROFILE_FUNCTION();

        // 1. Чтение данных из GPU-буфера
        void* mapped_data = nullptr;
        if (auto result = vmaMapMemory(allocator,
                                      vmaGetAllocationInfo(allocator, src_buffer).allocation,
                                      &mapped_data);
            result != VK_SUCCESS) {
            PV_LOG_ERROR("Failed to map GPU buffer: {}", result);
            return std::unexpected("Failed to map GPU buffer");
        }

        // 2. Сжатие данных
        std::span<const uint8_t> gpu_data(
            static_cast<const uint8_t*>(mapped_data),
            static_cast<size_t>(buffer_size)
        );

        auto compressed_result = zstd_decompressor_.compress(gpu_data);

        // 3. Размапирование буфера
        vmaUnmapMemory(allocator,
                      vmaGetAllocationInfo(allocator, src_buffer).allocation);

        if (!compressed_result) {
            PV_LOG_ERROR("GPU buffer compression failed: {}", compressed_result.error());
            return std::unexpected(compressed_result.error());
        }

        PV_LOG_DEBUG("Compressed GPU buffer: {} bytes -> {} bytes",
                    buffer_size, compressed_result->size());

        return compressed_result;
    }

    // Декомпрессия перед записью в GPU-буфер
    std::expected<void, std::string> decompress_to_gpu_buffer(
        VmaAllocator allocator,
        VkBuffer dst_buffer,
        std::span<const uint8_t> compressed_data
    ) {
        PV_PROFILE_FUNCTION();

        // 1. Декомпрессия данных
        auto decompressed_result = zstd_decompressor_.decompress(compressed_data);
        if (!decompressed_result) {
            PV_LOG_ERROR("GPU buffer decompression failed: {}", decompressed_result.error());
            return std::unexpected(decompressed_result.error());
        }

        // 2. Запись данных в GPU-буфер
        void* mapped_data = nullptr;
        if (auto result = vmaMapMemory(allocator,
                                      vmaGetAllocationInfo(allocator, dst_buffer).allocation,
                                      &mapped_data);
            result != VK_SUCCESS) {
            PV_LOG_ERROR("Failed to map GPU buffer for writing: {}", result);
            return std::unexpected("Failed to map GPU buffer");
        }

        std::memcpy(mapped_data, decompressed_result->data(), decompressed_result->size());

        // 3. Размапирование буфера
        vmaUnmapMemory(allocator,
                      vmaGetAllocationInfo(allocator, dst_buffer).allocation);

        PV_LOG_DEBUG("Decompressed to GPU buffer: {} bytes -> {} bytes",
                    compressed_data.size(), decompressed_result->size());

        return {};
    }

    // Пакетное сжатие нескольких буферов
    std::expected<std::vector<std::span<uint8_t>>, std::string> compress_gpu_buffers(
        VmaAllocator allocator,
        std::span<const std::pair<VkBuffer, VkDeviceSize>> buffers
    ) {
        PV_PROFILE_FUNCTION();

        std::vector<std::span<uint8_t>> results;
        results.reserve(buffers.size());

        for (size_t i = 0; i < buffers.size(); ++i) {
            const auto& [buffer, size] = buffers[i];

            PV_PROFILE_SCOPE("CompressSingleBuffer");

            auto result = compress_gpu_buffer(allocator, buffer, size);
            if (!result) {
                PV_LOG_ERROR("Failed to compress buffer {}: {}", i, result.error());
                return std::unexpected(result.error());
            }

            results.push_back(*result);
        }

        PV_LOG_INFO("Compressed {} GPU buffers", buffers.size());
        return results;
    }

private:
    projectv::compression::ZstdDecompressor zstd_decompressor_;
};

} // namespace projectv::vulkan
```

### Интеграция с VMA и ProjectV MemoryManager

```cpp
// VMA-интеграция с использованием ProjectV MemoryManager
#include <projectv/compression/zstd.hxx>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <span>
#include <expected>

namespace projectv::vulkan {

// Bridge для VMA-аллокаций с использованием ZstdDecompressor
class VMACompressedAllocator {
public:
    struct Config {
        int compression_level = 5;
        bool use_dictionary = false;
        std::span<const uint8_t> dictionary;
        projectv::core::memory::PoolAllocator* pool_allocator = nullptr;
    };

    explicit VMACompressedAllocator(Config config)
        : zstd_decompressor_({
            .compression_level = config.compression_level,
            .use_dictionary = config.use_dictionary,
            .dictionary = config.dictionary,
            .pool_allocator = config.pool_allocator
        })
    {
        PV_PROFILE_FUNCTION();
        PV_LOG_INFO("VMACompressedAllocator initialized");
    }

    // Создание сжатого буфера через VMA с использованием MemoryManager
    std::expected<CompressedAllocation, std::string> create_compressed_buffer(
        VmaAllocator allocator,
        std::span<const uint8_t> data,
        VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        int level = -1
    ) {
        PV_PROFILE_FUNCTION();

        // 1. Сжатие данных через ZstdDecompressor
        auto compressed_result = zstd_decompressor_.compress(data, level);
        if (!compressed_result) {
            PV_LOG_ERROR("Failed to compress data for VMA buffer: {}", compressed_result.error());
            return std::unexpected(compressed_result.error());
        }

        // 2. Создание VMA-буфера
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = compressed_result->size();
        buffer_info.usage = usage;

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocationInfo allocation_info = {};

        const VkResult vma_result = vmaCreateBuffer(
            allocator,
            &buffer_info,
            &alloc_info,
            &buffer,
            &allocation,
            &allocation_info
        );

        if (vma_result != VK_SUCCESS) {
            PV_LOG_ERROR("VMA buffer creation failed: {}", vma_result);
            return std::unexpected("VMA allocation failed");
        }

        // 3. Копирование сжатых данных в GPU-буфер
        void* mapped_data = nullptr;
        if (auto map_result = vmaMapMemory(allocator, allocation, &mapped_data);
            map_result != VK_SUCCESS) {
            PV_LOG_ERROR("Failed to map VMA buffer: {}", map_result);
            vmaDestroyBuffer(allocator, buffer, allocation);
            return std::unexpected("Failed to map VMA buffer");
        }

        std::memcpy(mapped_data, compressed_result->data(), compressed_result->size());
        vmaUnmapMemory(allocator, allocation);

        // 4. Возвращаем результат
        CompressedAllocation result;
        result.buffer = buffer;
        result.allocation = allocation;
        result.compressed_size = compressed_result->size();
        result.original_size = data.size();
        result.compression_ratio = static_cast<double>(compressed_result->size()) / data.size();

        PV_LOG_DEBUG("Created compressed VMA buffer: {} bytes -> {} bytes (ratio: {:.2f}%)",
                    data.size(), compressed_result->size(), result.compression_ratio * 100.0);

        return result;
    }

    // Декомпрессия VMA-буфера в CPU-память
    std::expected<std::span<uint8_t>, std::string> decompress_vma_buffer(
        VmaAllocator allocator,
        const CompressedAllocation& compressed_alloc
    ) {
        PV_PROFILE_FUNCTION();

        // 1. Чтение сжатых данных из GPU-буфера
        void* mapped_data = nullptr;
        if (auto result = vmaMapMemory(allocator, compressed_alloc.allocation, &mapped_data);
            result != VK_SUCCESS) {
            PV_LOG_ERROR("Failed to map compressed VMA buffer: {}", result);
            return std::unexpected("Failed to map VMA buffer");
        }

        std::span<const uint8_t> compressed_data(
            static_cast<const uint8_t*>(mapped_data),
            compressed_alloc.compressed_size
        );

        // 2. Декомпрессия через ZstdDecompressor
        auto decompressed_result = zstd_decompressor_.decompress(compressed_data);

        // 3. Размапирование буфера
        vmaUnmapMemory(allocator, compressed_alloc.allocation);

        if (!decompressed_result) {
            PV_LOG_ERROR("Failed to decompress VMA buffer: {}", decompressed_result.error());
            return std::unexpected(decompressed_result.error());
        }

        PV_LOG_DEBUG("Decompressed VMA buffer: {} bytes -> {} bytes",
                    compressed_alloc.compressed_size, decompressed_result->size());

        return decompressed_result;
    }

    // Освобождение сжатого VMA-буфера
    void destroy_compressed_buffer(
        VmaAllocator allocator,
        CompressedAllocation& alloc
    ) {
        PV_PROFILE_FUNCTION();

        if (alloc.buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator, alloc.buffer, alloc.allocation);
            alloc = {};
            PV_LOG_DEBUG("Destroyed compressed VMA buffer");
        }
    }

    // Пакетное создание сжатых буферов
    std::expected<std::vector<CompressedAllocation>, std::string> create_compressed_buffers(
        VmaAllocator allocator,
        std::span<const std::pair<std::span<const uint8_t>, VkBufferUsageFlags>> buffers_config
    ) {
        PV_PROFILE_FUNCTION();

        std::vector<CompressedAllocation> results;
        results.reserve(buffers_config.size());

        for (size_t i = 0; i < buffers_config.size(); ++i) {
            const auto& [data, usage] = buffers_config[i];

            PV_PROFILE_SCOPE("CreateCompressedBuffer");

            auto result = create_compressed_buffer(allocator, data, usage);
            if (!result) {
                PV_LOG_ERROR("Failed to create compressed buffer {}: {}", i, result.error());

                // Освобождаем уже созданные буферы
                for (auto& alloc : results) {
                    destroy_compressed_buffer(allocator, alloc);
                }

                return std::unexpected(result.error());
            }

            results.push_back(*result);
        }

        PV_LOG_INFO("Created {} compressed VMA buffers", buffers_config.size());
        return results;
    }

    struct CompressedAllocation {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        size_t compressed_size = 0;
        size_t original_size = 0;
        double compression_ratio = 0.0;
    };

private:
    projectv::compression::ZstdDecompressor zstd_decompressor_;
};

} // namespace projectv::vulkan
```

---

## Интеграция с Flecs ECS и ProjectV MemoryManager

### Компоненты для ECS с использованием ZstdDecompressor

```cpp
// ECS-интеграция с использованием ProjectV MemoryManager
#include <projectv/compression/zstd.hxx>
#include <flecs.h>
#include <span>
#include <expected>

namespace projectv::ecs {

// Компонент: сжатые данные с использованием MemoryManager
struct CompressedData {
    std::span<uint8_t> data;
    size_t original_size = 0;
    int compression_level = 5;
    projectv::core::memory::ArenaAllocator* arena = nullptr;
};

// Тег: данные готовы к отправке по сети
struct NetworkReadyTag {};

// Bridge для ECS-интеграции сжатия
class ZstdEcsBridge {
public:
    struct Config {
        int default_compression_level = 5;
        bool use_dictionary = false;
        std::span<const uint8_t> dictionary;
        projectv::core::memory::PoolAllocator* pool_allocator = nullptr;
    };

    explicit ZstdEcsBridge(flecs::world& world, Config config)
        : world_(world)
        , zstd_decompressor_({
            .compression_level = config.default_compression_level,
            .use_dictionary = config.use_dictionary,
            .dictionary = config.dictionary,
            .pool_allocator = config.pool_allocator
        })
    {
        PV_PROFILE_FUNCTION();
        setup_ecs_systems();
        PV_LOG_INFO("ZstdEcsBridge initialized for ECS world");
    }

    // Система: сжатие данных сущности
    void compress_entity_data(flecs::entity e, std::span<const uint8_t> data, int level = -1) {
        PV_PROFILE_FUNCTION();

        auto compressed_result = zstd_decompressor_.compress(data, level);
        if (!compressed_result) {
            PV_LOG_ERROR("Failed to compress entity data: {}", compressed_result.error());
            return;
        }

        // Создаем компонент с сжатыми данными
        auto* arena = zstd_decompressor_.get_pool_allocator()->allocate_arena(
            "ECSCompressedData",
            compressed_result->size()
        );

        if (!arena) {
            PV_LOG_ERROR("Failed to allocate arena for ECS compressed data");
            return;
        }

        // Копируем данные в arena
        auto* compressed_data = arena->allocate<uint8_t>(compressed_result->size());
        if (!compressed_data) {
            PV_LOG_ERROR("Failed to allocate compressed data buffer");
            return;
        }

        std::memcpy(compressed_data, compressed_result->data(), compressed_result->size());

        // Устанавливаем компонент
        world_.set(e, CompressedData{
            .data = std::span<uint8_t>(compressed_data, compressed_result->size()),
            .original_size = data.size(),
            .compression_level = (level == -1) ? zstd_decompressor_.get_config().compression_level : level,
            .arena = arena
        });

        PV_LOG_DEBUG("Compressed entity data: {} bytes -> {} bytes",
                    data.size(), compressed_result->size());
    }

    // Система: декомпрессия данных сущности
    std::expected<std::span<uint8_t>, std::string> decompress_entity_data(flecs::entity e) {
        PV_PROFILE_FUNCTION();

        auto* compressed_data = world_.get<CompressedData>(e);
        if (!compressed_data) {
            return std::unexpected("Entity has no compressed data");
        }

        auto decompressed_result = zstd_decompressor_.decompress(compressed_data->data);
        if (!decompressed_result) {
            PV_LOG_ERROR("Failed to decompress entity data: {}", decompressed_result.error());
            return std::unexpected(decompressed_result.error());
        }

        PV_LOG_DEBUG("Decompressed entity data: {} bytes -> {} bytes",
                    compressed_data->data.size(), decompressed_result->size());

        return decompressed_result;
    }

    // Система: пакетное сжатие всех сущностей с определенным тегом
    void compress_all_with_tag(flecs::entity tag, int level = -1) {
        PV_PROFILE_FUNCTION();

        world_.each([&](flecs::entity e) {
            if (e.has(tag)) {
                // Получаем данные для сжатия (зависит от конкретной реализации)
                // Здесь предполагается, что данные доступны через другой компонент
                PV_PROFILE_SCOPE("CompressEntity");
                // Реализация зависит от структуры данных сущности
            }
        });

        PV_LOG_INFO("Compressed all entities with tag");
    }

    // Система: очистка сжатых данных сущности
    void cleanup_entity_data(flecs::entity e) {
        PV_PROFILE_FUNCTION();

        auto* compressed_data = world_.get<CompressedData>(e);
        if (compressed_data && compressed_data->arena) {
            // Освобождаем arena через MemoryManager
            compressed_data->arena->reset();
            world_.remove<CompressedData>(e);
            PV_LOG_DEBUG("Cleaned up compressed data for entity");
        }
    }

private:
    void setup_ecs_systems() {
        // Система обработки очереди сжатия
        world_.system<CompressedData>("ProcessCompressionQueue")
            .kind(flecs::OnUpdate)
            .each([&](flecs::entity e, CompressedData& data) {
                PV_PROFILE_SCOPE("ProcessCompressionQueue");
                // Обработка очереди сжатия
            });

        // Система автоматической очистки устаревших данных
        world_.system<CompressedData>("CleanupOldCompressedData")
            .kind(flecs::PostUpdate)
            .interval(5.0) // Каждые 5 секунд
            .each([&](flecs::entity e, CompressedData& data) {
                PV_PROFILE_SCOPE("CleanupOldCompressedData");
                // Логика очистки старых данных
            });

        // Система мониторинга сжатия
        world_.system<>("CompressionMonitoring")
            .kind(flecs::OnStore)
            .iter([&](flecs::iter& it) {
                PV_PROFILE_SCOPE("CompressionMonitoring");

                size_t total_compressed = 0;
                size_t total_original = 0;

                it.world().each([&](flecs::entity e, const CompressedData& data) {
                    total_compressed += data.data.size();
                    total_original += data.original_size;
                });

                if (total_original > 0) {
                    double compression_ratio = static_cast<double>(total_compressed) / total_original;
                    PV_LOG_DEBUG("ECS compression stats: {} entities, ratio: {:.2f}%",
                                it.world().count<CompressedData>(), compression_ratio * 100.0);
                }
            });

        PV_LOG_DEBUG("Setup {} ECS systems for Zstd integration", 3);
    }

    flecs::world& world_;
    projectv::compression::ZstdDecompressor zstd_decompressor_;
};

// Событие: сжатие завершено
struct CompressionCompleteEvent {
    flecs::entity entity;
    size_t original_size;
    size_t compressed_size;
    double compression_ratio;
};

// Событие: декомпрессия завершена
struct DecompressionCompleteEvent {
    flecs::entity entity;
    size_t decompressed_size;
};

} // namespace projectv::ecs
```

### Интеграция с системой событий Flecs и stdexec

```cpp
// Асинхронная обработка сжатия через stdexec
#include <projectv/compression/zstd.hxx>
#include <flecs.h>
#include <stdexec/execution.hpp>
#include <span>
#include <expected>

namespace projectv::ecs::async {

// Асинхронный Bridge для ECS-сжатия с использованием stdexec
class AsyncZstdEcsBridge {
public:
    struct Config {
        int default_compression_level = 5;
        bool use_dictionary = false;
        std::span<const uint8_t> dictionary;
        projectv::core::memory::PoolAllocator* pool_allocator = nullptr;
        stdexec::scheduler auto scheduler = stdexec::schedule();
    };

    explicit AsyncZstdEcsBridge(flecs::world& world, Config config)
        : world_(world)
        , zstd_decompressor_({
            .compression_level = config.default_compression_level,
            .use_dictionary = config.use_dictionary,
            .dictionary = config.dictionary,
            .pool_allocator = config.pool_allocator
        })
        , scheduler_(config.scheduler)
    {
        PV_PROFILE_FUNCTION();
        setup_async_systems();
        PV_LOG_INFO("AsyncZstdEcsBridge initialized with stdexec scheduler");
    }

    // Асинхронное сжатие данных сущности
    stdexec::sender auto async_compress_entity(
        flecs::entity e,
        std::span<const uint8_t> data,
        int level = -1
    ) {
        return stdexec::schedule(scheduler_)
             | stdexec::then([this, e, data, level]() -> std::expected<void, std::string> {
                PV_PROFILE_SCOPE("AsyncCompressEntity");

                auto compressed_result = zstd_decompressor_.compress(data, level);
                if (!compressed_result) {
                    PV_LOG_ERROR("Async compression failed for entity: {}", compressed_result.error());
                    return std::unexpected(compressed_result.error());
                }

                // Создаем компонент в основном потоке ECS
                world_.defer([this, e, data, compressed = *compressed_result, level]() {
                    PV_PROFILE_SCOPE("CreateECSComponent");

                    auto* arena = zstd_decompressor_.get_pool_allocator()->allocate_arena(
                        "AsyncECSCompressedData",
                        compressed.size()
                    );

                    if (!arena) {
                        PV_LOG_ERROR("Failed to allocate arena for async compressed data");
                        return;
                    }

                    auto* compressed_data = arena->allocate<uint8_t>(compressed.size());
                    if (!compressed_data) {
                        PV_LOG_ERROR("Failed to allocate async compressed data buffer");
                        return;
                    }

                    std::memcpy(compressed_data, compressed.data(), compressed.size());

                    world_.set(e, CompressedData{
                        .data = std::span<uint8_t>(compressed_data, compressed.size()),
                        .original_size = data.size(),
                        .compression_level = (level == -1) ? zstd_decompressor_.get_config().compression_level : level,
                        .arena = arena
                    });

                    // Emit событие
                    world_.event().emit<CompressionCompleteEvent>({
                        .entity = e,
                        .original_size = data.size(),
                        .compressed_size = compressed.size(),
                        .compression_ratio = static_cast<double>(compressed.size()) / data.size()
                    });

                    PV_LOG_DEBUG("Async compressed entity data: {} bytes -> {} bytes",
                                data.size(), compressed.size());
                });

                return {};
             });
    }

    // Асинхронная пакетная обработка
    stdexec::sender auto async_batch_compress(
        std::span<const std::pair<flecs::entity, std::span<const uint8_t>>> entities_data,
        int level = -1
    ) {
        return stdexec::schedule(scheduler_)
             | stdexec::then([this, entities_data, level]() -> std::expected<size_t, std::string> {
                PV_PROFILE_FUNCTION();

                size_t success_count = 0;
                size_t total_compressed = 0;
                size_t total_original = 0;

                for (const auto& [entity, data] : entities_data) {
                    PV_PROFILE_SCOPE("BatchCompressSingle");

                    auto compressed_result = zstd_decompressor_.compress(data, level);
                    if (!compressed_result) {
                        PV_LOG_WARN("Batch compression failed for entity: {}", compressed_result.error());
                        continue;
                    }

                    world_.defer([this, entity, data, compressed = *compressed_result, level]() {
                        // Аналогично async_

---

## Интеграция с SDL3

### Сценарий: Сжатие сетевых пакетов

```cpp
#include <SDL3/SDL_net.h>
#include <zstd.h>
#include <vector>
#include <span>
#include <expected>

// Сжатие данных SDL Net перед отправкой
class NetworkPacketCompressor {
public:
    struct PacketConfig {
        int level = 3;           // Быстрый уровень для сети
        bool use_checksum = true;
        std::span<const uint8_t> dictionary;
    };

    explicit NetworkPacketCompressor(PacketConfig config)
        : config_(config)
    {
        init();
    }

    // Сжатие пакета для отправки
    std::expected<std::vector<uint8_t>, std::string> compress(
        std::span<const uint8_t> packet
    ) {
        // Добавляем size header для декомпрессии
        std::vector<uint8_t> buffer;
        buffer.reserve(sizeof(uint32_t) + packet.size());

        uint32_t size = static_cast<uint32_t>(packet.size());
        buffer.insert(buffer.end(),
            reinterpret_cast<uint8_t*>(&size),
            reinterpret_cast<uint8_t*>(&size) + sizeof(size));
        buffer.insert(buffer.end(), packet.begin(), packet.end());

        const size_t bound = ZSTD_compressBound(buffer.size());
        std::vector<uint8_t> compressed(bound);

        size_t result = ZSTD_compress(
            compressed.data(), bound,
            buffer.data(), buffer.size(),
            config_.level
        );

        if (ZSTD_isError(result)) {
            return std::unexpected(ZSTD_getErrorName(result));
        }

        compressed.resize(result);
        return compressed;
    }

    // Декомпрессия полученного пакета
    std::expected<std::vector<uint8_t>, std::string> decompress(
        std::span<const uint8_t> compressed
    ) {
        // Определяем размер
        const unsigned long long original_size = ZSTD_getFrameContentSize(
            compressed.data(), compressed.size()
        );

        if (original_size == ZSTD_CONTENTSIZE_ERROR) {
            return std::unexpected("Invalid packet");
        }

        std::vector<uint8_t> decompressed(original_size);

        size_t result = ZSTD_decompress(
            decompressed.data(), decompressed.size(),
            compressed.data(), compressed.size()
        );

        if (ZSTD_isError(result)) {
            return std::unexpected(ZSTD_getErrorName(result));
        }

        decompressed.resize(result);

        // Провяряем размер
        if (decompressed.size() < sizeof(uint32_t)) {
            return std::unexpected("Packet too small");
        }

        uint32_t expected_size;
        std::memcpy(&expected_size, decompressed.data(), sizeof(expected_size));

        if (expected_size != decompressed.size() - sizeof(expected_size)) {
            return std::unexpected("Size mismatch");
        }

        // Возвращаем данные без header
        std::vector<uint8_t> payload(
            decompressed.begin() + sizeof(expected_size),
            decompressed.end()
        );

        return payload;
    }

private:
    void init() {
        cctx_ = ZSTD_createCCtx();
        dctx_ = ZSTD_createDCtx();

        if (cctx_) {
            ZSTD_CCtx_setParameter(cctx_, ZSTD_c_compressionLevel, config_.level);
            ZSTD_CCtx_setParameter(cctx_, ZSTD_c_checksumFlag, config_.use_checksum ? 1 : 0);
        }
    }

    PacketConfig config_;
    ZSTD_CCtx* cctx_ = nullptr;
    ZSTD_DCtx* dctx_ = nullptr;
};
```

---

## Интеграция с системой сохранения

### Сценарий: Сжатие миров

```cpp
#include <zstd.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <span>
#include <expected>

// Сжатие данных сейва
class SaveGameCompressor {
public:
    struct SaveHeader {
        uint32_t magic = 0x5A534156; // 'ZSAV'
        uint32_t version = 1;
        uint32_t compressed_size;
        uint32_t original_size;
        uint32_t checksum;
        int compression_level;
    };

    std::expected<void, std::string> save(
        const std::filesystem::path& path,
        std::span<const uint8_t> data,
        int level = 10
    ) {
        // Сжатие
        const size_t bound = ZSTD_compressBound(data.size());
        std::vector<uint8_t> compressed(bound);

        const size_t compressed_size = ZSTD_compress(
            compressed.data(), bound,
            data.data(), data.size(),
            level
        );

        if (ZSTD_isError(compressed_size)) {
            return std::unexpected(ZSTD_getErrorName(compressed_size));
        }

        compressed.resize(compressed_size);

        // Вычисление checksum
        const uint32_t checksum = calculate_checksum(compressed);

        // Запись
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            return std::unexpected("Failed to open file");
        }

        SaveHeader header;
        header.compressed_size = static_cast<uint32_t>(compressed_size);
        header.original_size = static_cast<uint32_t>(data.size());
        header.checksum = checksum;
        header.compression_level = level;

        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        file.write(reinterpret_cast<const char*>(compressed.data()), compressed_size);

        return {};
    }

    std::expected<std::vector<uint8_t>, std::string> load(
        const std::filesystem::path& path
    ) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            return std::unexpected("Failed to open file");
        }

        const size_t file_size = file.tellg();
        file.seekg(0);

        // Чтение header
        SaveHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        if (header.magic != 0x5A534156) {
            return std::unexpected("Invalid save file");
        }

        // Чтение сжатых данных
        const size_t data_size = file_size - sizeof(header);
        std::vector<uint8_t> compressed(data_size);
        file.read(reinterpret_cast<char*>(compressed.data()), data_size);

        // Проверка checksum
        const uint32_t actual_checksum = calculate_checksum(compressed);
        if (actual_checksum != header.checksum) {
            return std::unexpected("Checksum mismatch");
        }

        // Декомпрессия
        std::vector<uint8_t> decompressed(header.original_size);

        const size_t result = ZSTD_decompress(
            decompressed.data(), decompressed.size(),
            compressed.data(), compressed.size()
        );

        if (ZSTD_isError(result)) {
            return std::unexpected(ZSTD_getErrorName(result));
        }

        if (result != header.original_size) {
            return std::unexpected("Size mismatch");
        }

        return decompressed;
    }

private:
    static uint32_t calculate_checksum(std::span<const uint8_t> data) {
        // Простой checksum - xxHash64 был бы лучше
        uint32_t sum = 0;
        for (size_t i = 0; i < data.size(); ++i) {
            sum = sum * 31 + data[i];
        }
        return sum;
    }
};
```

---

## Типичные ошибки интеграции

### Ошибка 1: Утечка памяти контекста

```cpp
// ПЛОХО: забыли освободить контекст
void bad_compress() {
    ZSTD_CCtx* cctx = ZSTD_createCCtx();
    // ... используем cctx
    // Забыли: ZSTD_freeCCtx(cctx);
}

// ХОРОШО: RAII обёртка
class Compressor {
    ZSTD_CCtx* cctx_ = ZSTD_createCCtx();
public:
    ~Compressor() { if (cctx_) ZSTD_freeCCtx(cctx_); }
};
```

### Ошибка 2: Неправильный размер буфера

```cpp
// ПЛОХО: недостаточный буфер
void bad_size() {
    size_t size = ZSTD_compressBound(input.size());
    // ZSTD_compressBound возвращает max, но реальный размер может быть больше
    // при очень маленьких данных
}

// ХОРОШО: запас
void good_size() {
    size_t bound = ZSTD_compressBound(input.size());
    // Добавляем небольшой запас для краевых случаев
    std::vector<uint8_t> buffer(bound + 16);
}
```

### Ошибка 3: Потокобезопасность контекста

```cpp
// ПЛОХО: один контекст в нескольких потоках
ZSTD_CCtx* shared_ctx; // ОПАСНО!

// ХОРОШО: thread-local или отдельный контекст на поток
thread_local ZSTD_CCtx* tls_ctx = ZSTD_createCCtx();
```

---

## Сборка в ProjectV

### Проверка зависимостей

```cmake
# В ProjectVConfig.cmake
find_package(Zstd REQUIRED)

# Проверка версии
if(Zstd_VERSION VERSION_LESS "1.5.0")
    message(FATAL_ERROR "Zstd 1.5.0+ required")
endif()

# Проверка потокобезопасности
if(NOT ZSTD_MULTITHREAD_SUPPORT)
    message(FATAL_ERROR "Zstd must be built with threading support")
endif()
```

# Zstd: Интеграция в ProjectV

## CMake-конфигурация

### Вариант 1: FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    zstd
    # zstd repository configuration
)

set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "No CLI tools")
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "Static only")
set(ZSTD_MULTITHREAD_SUPPORT ON CACHE BOOL "Enable threading")

FetchContent_MakeAvailable(zstd)

target_link_libraries(ProjectV PRIVATE
    libzstd_static
)

target_include_directories(ProjectV PRIVATE
    ${zstd_SOURCE_DIR}/lib
    ${zstd_SOURCE_DIR}/lib/common
)

target_compile_definitions(ProjectV PRIVATE
    ZSTD_STATIC_LINKING
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
#include <zstd.h>
#include <vulkan/vulkan.h>
#include <span>
#include <vector>
#include <expected>

// Сжатие данных из Vulkan-буфера (GPU -> CPU -> сжатие)
class VulkanBufferCompressor {
public:
    struct Config {
        int compression_level = 5;
        bool use_dictionary = true;
        std::span<const uint8_t> dictionary;
    };

    explicit VulkanBufferCompressor(Config config)
        : config_(config)
    {
        init_compressor();
    }

    // Сжатие данных после чтения из GPU-буфера
    std::expected<std::vector<uint8_t>, std::string> compress(
        std::span<const uint8_t> src
    ) {
        if (!cctx_) {
            return std::unexpected("Compressor not initialized");
        }

        if (config_.use_dictionary && cdict_) {
            ZSTD_CCtx_refCDict(cctx_, cdict_);
        }

        const size_t bound = ZSTD_compressBound(src.size());
        std::vector<uint8_t> dst(bound);

        const size_t result = ZSTD_compress2(
            cctx_,
            dst.data(), bound,
            src.data(), src.size()
        );

        if (ZSTD_isError(result)) {
            return std::unexpected(ZSTD_getErrorName(result));
        }

        dst.resize(result);
        return dst;
    }

    // Декомпрессия перед записью в GPU-буфер
    std::expected<std::vector<uint8_t>, std::string> decompress(
        std::span<const uint8_t> src
    ) {
        if (!dctx_) {
            return std::unexpected("Decompressor not initialized");
        }

        if (config_.use_dictionary && ddict_) {
            ZSTD_DCtx_refDDict(dctx_, ddict_);
        }

        const unsigned long long original_size = ZSTD_getFrameContentSize(
            src.data(), src.size()
        );

        if (original_size == ZSTD_CONTENTSIZE_ERROR) {
            return std::unexpected("Invalid compressed data");
        }

        std::vector<uint8_t> dst(static_cast<size_t>(original_size));

        const size_t result = ZSTD_decompressDCtx(
            dctx_,
            dst.data(), dst.size(),
            src.data(), src.size()
        );

        if (ZSTD_isError(result)) {
            return std::unexpected(ZSTD_getErrorName(result));
        }

        dst.resize(result);
        return dst;
    }

private:
    void init_compressor() {
        cctx_ = ZSTD_createCCtx();
        dctx_ = ZSTD_createDCtx();

        if (cctx_) {
            ZSTD_CCtx_setParameter(cctx_, ZSTD_c_compressionLevel, config_.compression_level);
            ZSTD_CCtx_setParameter(cctx_, ZSTD_c_checksumFlag, 1);
        }

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
        }
    }

    Config config_;
    ZSTD_CCtx* cctx_ = nullptr;
    ZSTD_DCtx* dctx_ = nullptr;
    ZSTD_CDict* cdict_ = nullptr;
    ZSTD_DDict* ddict_ = nullptr;
};
```

### Интеграция с VMA

```cpp
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <zstd.h>
#include <vector>
#include <span>
#include <expected>

// Сжатие данных в VMA-аллокации
class VMAAwareCompressor {
public:
    // Аллоцировать сжатый буфер через VMA
    std::expected<VmaAllocation, std::string> compress_to_vma(
        VmaAllocator allocator,
        std::span<const uint8_t> src,
        int level = 5
    ) {
        // Сжатие
        const size_t bound = ZSTD_compressBound(src.size());
        std::vector<uint8_t> compressed(bound);

        const size_t compressed_size = ZSTD_compress(
            compressed.data(), bound,
            src.data(), src.size(),
            level
        );

        if (ZSTD_isError(compressed_size)) {
            return std::unexpected(ZSTD_getErrorName(compressed_size));
        }

        compressed.resize(compressed_size);

        // VMA аллокация
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = compressed_size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
        alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;

        const VkResult result = vmaCreateBuffer(
            allocator,
            &buffer_info,
            &alloc_info,
            &buffer,
            &allocation,
            nullptr
        );

        if (result != VK_SUCCESS) {
            return std::unexpected("VMA allocation failed");
        }

        // Копирование сжатых данных
        void* mapped = nullptr;
        vmaMapMemory(allocator, allocation, &mapped);
        std::memcpy(mapped, compressed.data(), compressed_size);
        vmaUnmapMemory(allocator, allocation);

        // Возвращаем структуру с буфером
        CompressedAllocation result_alloc;
        result_alloc.buffer = buffer;
        result_alloc.allocation = allocation;
        result_alloc.compressed_size = compressed_size;
        result_alloc.original_size = src.size();

        return result_alloc;
    }

    void free_vma(VmaAllocator allocator, CompressedAllocation& alloc) {
        if (alloc.buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator, alloc.buffer, alloc.allocation);
        }
        alloc = {};
    }

    struct CompressedAllocation {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        size_t compressed_size = 0;
        size_t original_size = 0;
    };

private:
    Config config_;
};
```

---

## Интеграция с Flecs ECS

### Компоненты для ECS

```cpp
#include <flecs.h>
#include <zstd.h>
#include <vector>
#include <span>

// Компонент: сжатые данные
struct CompressedData {
    std::vector<uint8_t> data;
    size_t original_size = 0;
    int compression_level = 5;
};

// Тег: данные готовы к отправке по сети
struct NetworkReadyTag {};

// Система: сжатие данных сущности
struct CompressionSystem {
    // Сжатие данных компонента
    static void compress(
        flecs::world& world,
        int level = 5,
        std::span<const uint8_t> dictionary = {}
    ) {
        auto cctx = ZSTD_createCCtx();
        auto ddict = !dictionary.empty()
            ? ZSTD_createCDict(dictionary.data(), dictionary.size(), level)
            : nullptr;

        if (ddict) {
            ZSTD_CCtx_refCDict(cctx, ddict);
        }

        ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, level);

        world.each([&](flecs::entity e, CompressedData& data) {
            const size_t bound = ZSTD_compressBound(data.original_size);
            data.data.resize(bound);

            const size_t result = ZSTD_compress2(
                cctx,
                data.data.data(), bound,
                nullptr, data.original_size
            );

            if (!ZSTD_isError(result)) {
                data.data.resize(result);
                data.compression_level = level;
            }
        });

        if (cctx) ZSTD_freeCCtx(cctx);
        if (ddict) ZSTD_freeCDict(ddict);
    }

    // Декомпрессия данных компонента
    static void decompress(flecs::world& world) {
        auto dctx = ZSTD_createDCtx();

        world.each([&](flecs::entity e, CompressedData& data) {
            const unsigned long long original = ZSTD_getFrameContentSize(
                data.data.data(), data.data.size()
            );

            if (original == ZSTD_CONTENTSIZE_ERROR) return;

            std::vector<uint8_t> decompressed(original);

            const size_t result = ZSTD_decompressDCtx(
                dctx,
                decompressed.data(), decompressed.size(),
                data.data.data(), data.data.size()
            );

            if (!ZSTD_isError(result)) {
                data.data.assign(decompressed.begin(), decompressed.end());
                data.original_size = result;
            }
        });

        if (dctx) ZSTD_freeDCtx(dctx);
    }
};
```

### Интеграция с системой событий Flecs

```cpp
#include <flecs.h>
#include <zstd.h>

// Событие: сжатие завершено
struct CompressionCompleteEvent {
    flecs::entity entity;
    size_t original_size;
    size_t compressed_size;
};

// Событие: декомпрессия завершена
struct DecompressionCompleteEvent {
    flecs::entity entity;
};

// Bridge: асинхронное сжатие через Flecs worker threads
class ZstdEcsBridge {
public:
    ZstdEcsBridge(flecs::world& world)
        : world_(world)
    {
        // Регистрация событий
        world_.event<CompressionCompleteEvent>()
            .desc("Fired when compression completes");
        world_.event<DecompressionCompleteEvent>()
            .desc("Fired when decompression completes");

        // Система обработки очереди сжатия
        world_.system("ProcessCompressionQueue")
            .kind(flecs::OnUpdate)
            .each([&](CompressionQueue& queue) {
                while (!queue.pending.empty()) {
                    auto task = queue.pending.front();
                    queue.pending.pop();

                    process_compression_task(task);
                }
            });
    }

    void queue_compression(flecs::entity e, std::span<const uint8_t> data, int level) {
        CompressionTask task;
        task.entity = e;
        task.data.assign(data.begin(), data.end());
        task.level = level;

        compression_queue_.pending.push(task);
    }

private:
    void process_compression_task(CompressionTask& task) {
        const size_t bound = ZSTD_compressBound(task.data.size());
        std::vector<uint8_t> compressed(bound);

        const size_t result = ZSTD_compress(
            compressed.data(), bound,
            task.data.data(), task.data.size(),
            task.level
        );

        if (!ZSTD_isError(result)) {
            compressed.resize(result);

            // Установка компонента
            world_.set(task.entity, CompressedData{
                .data = std::move(compressed),
                .original_size = task.data.size(),
                .compression_level = task.level
            });

            // Emit событие
            world_.event().emit<CompressionCompleteEvent>({
                .entity = task.entity,
                .original_size = task.data.size(),
                .compressed_size = result
            });
        }
    }

    flecs::world& world_;

    struct CompressionTask {
        flecs::entity entity;
        std::vector<uint8_t> data;
        int level;
    };

    struct CompressionQueue {
        std::queue<CompressionTask> pending;
    } compression_queue_;
};
```

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

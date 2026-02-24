# Zstd — Integration

<!-- anchor: 02_integration -->

Интеграция Zstd в ProjectV: CMake конфигурация, многопоточное сжатие воксельных данных и интеграция с сетевым протоколом.

---

## CMake Integration

### Вариант 1: Git Submodules (рекомендуется)

```cmake
# Добавить подмодуль
# git submodule add https://github.com/facebook/zstd.git external/zstd

add_subdirectory(external/zstd/build/cmake)

target_link_libraries(ProjectV PRIVATE
    libzstd_static
)

# Включить многопоточную поддержку
target_compile_definitions(ProjectV PRIVATE
    ZSTD_MULTITHREAD
)

# Установить минимальную версию Zstd
set_target_properties(libzstd_static PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)
```

### Вариант 2: FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    zstd
    GIT_REPOSITORY https://github.com/facebook/zstd.git
    GIT_TAG v1.5.5
)

set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "Don't build CLI programs")
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "Build static library")
set(ZSTD_MULTITHREAD_SUPPORT ON CACHE BOOL "Enable multithreading")

FetchContent_MakeAvailable(zstd)

target_link_libraries(ProjectV PRIVATE
    libzstd_static
)

# Добавить include директории
target_include_directories(ProjectV PRIVATE
    ${zstd_SOURCE_DIR}/lib
    ${zstd_SOURCE_DIR}/lib/common
)
```

### Вариант 3: vcpkg

```
# vcpkg.json
{
    "dependencies": [
        {
            "name": "zstd",
            "features": ["threading"]
        }
    ]
}

# CMakeLists.txt
find_package(zstd REQUIRED CONFIG)

target_link_libraries(ProjectV PRIVATE
    zstd::libzstd_static
)
```

---

## Многопоточное сжатие воксельных чанков

### Thread Pool для параллельного сжатия

```cpp
#include <zstd.h>
#include <zstd_errors.h>
#include <print>
#include <vector>
#include <thread>
#include <atomic>
#include <span>
#include <memory>

class ParallelCompressor {
    struct CompressionTask {
        std::span<const uint8_t> input;
        std::vector<uint8_t> output;
        std::atomic<bool> completed{false};
        std::atomic<size_t> compressed_size{0};
        ZSTD_CCtx* cctx = nullptr;
        int compression_level;

        CompressionTask(std::span<const uint8_t> in, int level)
            : input(in), compression_level(level) {
            output.resize(ZSTD_compressBound(input.size()));

            // Создание контекста сжатия для этого потока
            cctx = ZSTD_createCCtx();
            if (!cctx) {
                throw std::runtime_error("Failed to create ZSTD compression context");
            }

            // Настройка уровня сжатия
            ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, level);

            // Включение checksum для целостности данных
            ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1);
        }

        ~CompressionTask() {
            if (cctx) {
                ZSTD_freeCCtx(cctx);
            }
        }
    };

    std::vector<std::thread> workers_;
    std::atomic<bool> stop_{false};
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<std::shared_ptr<CompressionTask>> task_queue_;

public:
    ParallelCompressor(size_t thread_count = std::thread::hardware_concurrency()) {
        workers_.reserve(thread_count);

        for (size_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back([this, i]() {
                worker_thread(i);
            });
        }

        std::print("Parallel compressor started with {} threads\n", thread_count);
    }

    ~ParallelCompressor() {
        stop_ = true;
        queue_cv_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    // Асинхронное сжатие чанка
    std::shared_ptr<CompressionTask> compress_async(std::span<const uint8_t> data,
                                                   int compression_level = 5) {
        auto task = std::make_shared<CompressionTask>(data, compression_level);

        {
            std::lock_guard lock(queue_mutex_);
            task_queue_.push(task);
        }

        queue_cv_.notify_one();
        return task;
    }

    // Синхронное сжатие нескольких чанков
    std::vector<std::vector<uint8_t>> compress_batch(
        const std::vector<std::span<const uint8_t>>& chunks,
        int compression_level = 5) {

        std::vector<std::shared_ptr<CompressionTask>> tasks;
        tasks.reserve(chunks.size());

        // Запуск всех задач
        for (const auto& chunk : chunks) {
            tasks.push_back(compress_async(chunk, compression_level));
        }

        // Ожидание завершения всех задач
        std::vector<std::vector<uint8_t>> results;
        results.reserve(chunks.size());

        for (auto& task : tasks) {
            while (!task->completed.load()) {
                std::this_thread::yield();
            }

            std::vector<uint8_t> compressed(task->compressed_size);
            std::copy(task->output.begin(),
                     task->output.begin() + task->compressed_size,
                     compressed.begin());

            results.push_back(std::move(compressed));
        }

        return results;
    }

private:
    void worker_thread(size_t thread_id) {
        while (!stop_.load()) {
            std::shared_ptr<CompressionTask> task;

            {
                std::unique_lock lock(queue_mutex_);
                queue_cv_.wait(lock, [this]() {
                    return !task_queue_.empty() || stop_.load();
                });

                if (stop_.load() && task_queue_.empty()) {
                    return;
                }

                if (!task_queue_.empty()) {
                    task = task_queue_.front();
                    task_queue_.pop();
                }
            }

            if (task) {
                process_task(*task);
            }
        }
    }

    void process_task(CompressionTask& task) {
        try {
            // Сжатие данных
            size_t compressed_size = ZSTD_compress2(
                task.cctx,
                task.output.data(), task.output.size(),
                task.input.data(), task.input.size()
            );

            if (ZSTD_isError(compressed_size)) {
                std::print(stderr, "Compression error in thread: {}\n",
                           ZSTD_getErrorName(compressed_size));
                task.compressed_size = 0;
            } else {
                task.compressed_size = compressed_size;
                task.output.resize(compressed_size);
            }

            task.completed = true;

        } catch (const std::exception& e) {
            std::print(stderr, "Exception in compression thread: {}\n", e.what());
            task.compressed_size = 0;
            task.completed = true;
        }
    }
};
```

### Интеграция с системой чанков ProjectV

```cpp
#include <zstd.h>
#include <print>
#include <span>
#include <vector>
#include <memory>

class ChunkCompressionSystem {
    ParallelCompressor compressor_;
    VoxelDictionary dictionary_;

    // Кэш сжатых чанков
    struct CompressedChunk {
        glm::ivec3 coord;
        uint32_t lod;
        std::vector<uint8_t> data;
        std::chrono::system_clock::time_point last_accessed;
        size_t original_size;

        struct glaze {
            using T = CompressedChunk;
            static constexpr auto value = glz::object(
                "coord", &T::coord,
                "lod", &T::lod,
                "data", &T::data,
                "last_accessed", &T::last_accessed,
                "original_size", &T::original_size
            );
        };
    };

    std::unordered_map<uint64_t, CompressedChunk> chunk_cache_;
    size_t max_cache_size_ = 1024 * 1024 * 1024;  // 1GB
    size_t current_cache_size_ = 0;

public:
    ChunkCompressionSystem(size_t thread_count = 4)
        : compressor_(thread_count) {

        // Загрузка или обучение словаря
        load_or_train_dictionary();
    }

    // Сжатие воксельного чанка
    std::vector<uint8_t> compress_chunk(const VoxelChunk& chunk,
                                       int compression_level = 5) {
        uint64_t chunk_key = calculate_chunk_key(chunk.coord, chunk.lod);

        // Проверка кэша
        if (auto it = chunk_cache_.find(chunk_key); it != chunk_cache_.end()) {
            it->second.last_accessed = std::chrono::system_clock::now();
            return it->second.data;
        }

        // Сериализация чанка в бинарный формат
        std::vector<uint8_t> serialized = serialize_voxel_chunk(chunk);

        // Сжатие с использованием словаря
        ZSTD_CCtx* cctx = ZSTD_createCCtx();
        ZSTD_CCtx_refCDict(cctx, dictionary_.compression_dict());
        ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, compression_level);

        size_t bound = ZSTD_compressBound(serialized.size());
        std::vector<uint8_t> compressed(bound);

        size_t compressed_size = ZSTD_compress2(
            cctx,
            compressed.data(), bound,
            serialized.data(), serialized.size()
        );

        ZSTD_freeCCtx(cctx);

        if (ZSTD_isError(compressed_size)) {
            std::print(stderr, "Failed to compress chunk {}: {}\n",
                       chunk.coord, ZSTD_getErrorName(compressed_size));
            return serialized;  // Возвращаем несжатые данные
        }

        compressed.resize(compressed_size);

        // Сохранение в кэш
        add_to_cache(chunk_key, chunk.coord, chunk.lod,
                    compressed, serialized.size());

        std::print("Chunk {} compressed: {} -> {} bytes ({:.1f}x)\n",
                   chunk.coord, serialized.size(), compressed_size,
                   static_cast<double>(serialized.size()) / compressed_size);

        return compressed;
    }

    // Асинхронное сжатие нескольких чанков
    std::vector<std::shared_ptr<ParallelCompressor::CompressionTask>>
    compress_chunks_async(const std::vector<VoxelChunk>& chunks,
                         int compression_level = 5) {

        std::vector<std::shared_ptr<ParallelCompressor::CompressionTask>> tasks;
        tasks.reserve(chunks.size());

        for (const auto& chunk : chunks) {
            // Сериализация чанка
            std::vector<uint8_t> serialized = serialize_voxel_chunk(chunk);

            // Запуск асинхронного сжатия
            auto task = compressor_.compress_async(serialized, compression_level);
            tasks.push_back(task);
        }

        return tasks;
    }

    // Декомпрессия чанка
    std::expected<VoxelChunk, std::string> decompress_chunk(
        std::span<const uint8_t> compressed_data,
        const glm::ivec3& expected_coord,
        uint32_t expected_lod) {

        // Проверка кэша
        uint64_t chunk_key = calculate_chunk_key(expected_coord, expected_lod);
        if (auto it = chunk_cache_.find(chunk_key); it != chunk_cache_.end()) {
            if (it->second.data.size() == compressed_data.size() &&
                memcmp(it->second.data.data(), compressed_data.data(),
                      compressed_data.size()) == 0) {
                // Данные уже в кэше, десериализуем
                return deserialize_voxel_chunk(it->second.data);
            }
        }

        // Декомпрессия с использованием словаря
        ZSTD_DCtx* dctx = ZSTD_createDCtx();
        ZSTD_DCtx_refDDict(dctx, dictionary_.decompression_dict());

        // Получение размера оригинальных данных
        unsigned long long original_size = ZSTD_getFrameContentSize(
            compressed_data.data(), compressed_data.size());

        if (original_size == ZSTD_CONTENTSIZE_ERROR ||
            original_size == ZSTD_CONTENTSIZE_UNKNOWN) {
            ZSTD_freeDCtx(dctx);
            return std::unexpected("Invalid compressed data");
        }

        std::vector<uint8_t> decompressed(original_size);

        size_t decompressed_size = ZSTD_decompressDCtx(
            dctx,
            decompressed.data(), decompressed.size(),
            compressed_data.data(), compressed_data.size()
        );

        ZSTD_freeDCtx(dctx);

        if (ZSTD_isError(decompressed_size) || decompressed_size != original_size) {
            return std::unexpected(ZSTD_getErrorName(decompressed_size));
        }

        // Десериализация чанка
        auto chunk = deserialize_voxel_chunk(decompressed);

        // Проверка координат
        if (chunk.coord != expected_coord || chunk.lod != expected_lod) {
            return std::unexpected("Chunk coordinate/lod mismatch after decompression");
        }

        // Сохранение в кэш
        std::vector<uint8_t> compressed_copy(compressed_data.begin(),
                                           compressed_data.end());
        add_to_cache(chunk_key, expected_coord, expected_lod,
                    compressed_copy, decompressed_size);

        return chunk;
    }

private:
    void load_or_train_dictionary() {
        std::filesystem::path dict_path = "data/dictionaries/voxel.zstdict";

        if (std::filesystem::exists(dict_path)) {
            // Загрузка существующего словаря
            if (auto result = dictionary_.load(dict_path); !result) {
                std::print(stderr, "Failed to load dictionary: {}\n", result.error());
                train_new_dictionary();
            } else {
                std::print("Dictionary loaded from {}\n", dict_path.string());
            }
        } else {
            // Обучение нового словаря
            train_new_dictionary();
        }
    }

    void train_new_dictionary() {
        std::print("Training new voxel dictionary...\n");

        // Сбор образцов для обучения
        auto samples = collect_training_samples();

        // Обучение словаря
        if (auto result = dictionary_.train(samples); !result) {
            std::print(stderr, "Failed to train dictionary: {}\n", result.error());
            throw std::runtime_error("Dictionary training failed");
        }

        // Сохранение словаря
        std::filesystem::create_directories("data/dictionaries");
        if (!dictionary_.save("data/dictionaries/voxel.zstdict")) {
            std::print(stderr, "Failed to save dictionary\n");
        }

        std::print("Dictionary trained and saved\n");
    }

    uint64_t calculate_chunk_key(const glm::ivec3& coord, uint32_t lod) {
        // Хэш координат и LOD
        uint64_t key = 0;
        key ^= static_cast<uint64_t>(coord.x) << 0;
        key ^= static_cast<uint64_t>(coord.y) << 16;
        key ^= static_cast<uint64_t>(coord.z) << 32;
        key ^= static_cast<uint64_t>(lod) << 48;
        return key;
    }

    void add_to_cache(uint64_t key, const glm::ivec3& coord, uint32_t lod,
                     const std::vector<uint8_t>& data, size_t original_size) {
        // Проверка размера кэша
        if (current_cache_size_ + data.size() > max_cache_size_) {
            evict_old_chunks();
        }

        CompressedChunk chunk{
            .coord = coord,
            .lod = lod,
            .data = data,
            .last_accessed = std::chrono::system_clock::now(),
            .original_size = original_size
        };

        chunk_cache_[key] = std::move(chunk);
        current_cache_size_ += data.size();
    }

    void evict_old_chunks() {
        // Находим самые старые чанки
        std::vector<std::pair<uint64_t, std::chrono::system_clock::time_point>> chunks;
        chunks.reserve(chunk_cache_.size());

        for (const auto& [key, chunk] : chunk_cache_) {
            chunks.emplace_back(key, chunk.last_accessed);
        }

        // Сортируем по времени последнего доступа
        std::sort(chunks.begin(), chunks.end(),
                 [](const auto& a, const auto& b) {
                     return a.second < b.second;
                 });

        // Удаляем 20% самых старых чанков
        size_t chunks_to_remove = chunks.size() / 5;
        for (size_t i = 0; i < chunks_to_remove; ++i) {
            auto it = chunk_cache_.find(chunks[i].first);
            if (it != chunk_cache_.end()) {
                current_cache_size_ -= it->second.data.size();
                chunk_cache_.erase(it);
            }
        }
    }

    std::vector<uint8_t> serialize_voxel_chunk(const VoxelChunk& chunk) {
        // Сериализация чанка в бинарный формат
        std::vector<uint8_t> buffer;
        glz::write_binary(chunk, buffer);
        return buffer;
    }

    VoxelChunk deserialize_voxel_chunk(std::span<const uint8_t> data) {
        VoxelChunk chunk;
        auto error = glz::read_binary(chunk, data);

        if (error) {
            throw std::runtime_error(glz::format_error(error, "chunk data"));
        }

        return chunk;
    }
};
```

---

## Интеграция с сетевым протоколом

### Сжатие сетевых пакетов

```cpp
#include <zstd.h>
#include <print>
#include <span>
#include <vector>

class NetworkCompressor {
    ZSTD_CCtx* compression_ctx_ = nullptr;
    ZSTD_DCtx* decompression_ctx_ = nullptr;
    VoxelDictionary dictionary_;

    // Статистика для мониторинга
    struct Stats {
        size_t total_packets_compressed = 0;
        size_t total_packets_decompressed = 0;
        size_t total_input_bytes = 0;
        size_t total_output_bytes = 0;
        double average_compression_ratio = 0.0;

        void update_compression_stats(size_t input_size, size_t output_size) {
            total_packets_compressed++;
            total_input_bytes += input_size;
            total_output_bytes += output_size;

            double ratio = static_cast<double>(input_size) / output_size;
            average_compression_ratio =
                (average_compression_ratio * (total_packets_compressed - 1) + ratio) /
                total_packets_compressed;
        }
    } stats_;

public:
    NetworkCompressor() {
        compression_ctx_ = ZSTD_createCCtx();
        decompression_ctx_ = ZSTD_createDCtx();

        if (!compression_ctx_ || !decompression_ctx_) {
            throw std::runtime_error("Failed to create ZSTD contexts");
        }

        // Настройка для сетевых пакетов (минимальная задержка)
        ZSTD_CCtx_setParameter(compression_ctx_, ZSTD_c_compressionLevel, 1);
        ZSTD_CCtx_setParameter(compression_ctx_, ZSTD_c_checksumFlag, 1);
        ZSTD_CCtx_setParameter(compression_ctx_, ZSTD_c_contentSizeFlag, 1);

        // Загрузка сетевого словаря
        load_network_dictionary();
    }

    ~NetworkCompressor() {
        if (compression_ctx_) ZSTD_freeCCtx(compression_ctx_);
        if (decompression_ctx_) ZSTD_freeDCtx(decompression_ctx_);

        // Логирование статистики
        std::print("Network compression stats:\n");
        std::print("  Packets compressed: {}\n", stats_.total_packets_compressed);
        std::print("  Packets decompressed: {}\n", stats_.total_packets_decompressed);
        std::print("  Total input: {} bytes\n", stats_.total_input_bytes);
        std::print("  Total output: {} bytes\n", stats_.total_output_bytes);
        std::print("  Average ratio: {:.2f}x\n", stats_.average_compression_ratio);
        std::print("  Bandwidth saved: {} bytes\n",
                   stats_.total_input_bytes - stats_.total_output_bytes);
    }

    // Сжатие сетевого пакета
    std::vector<uint8_t> compress_packet(std::span<const uint8_t> packet) {
        size_t bound = ZSTD_compressBound(packet.size());
        std::vector<uint8_t> compressed(bound);

        // Используем словарь для сетевых пакетов
        ZSTD_CCtx_refCDict(compression_ctx_, dictionary_.compression_dict());

        size_t compressed_size = ZSTD_compress2(
            compression_ctx_,
            compressed.data(), bound,
            packet.data(), packet.size()
        );

        if (ZSTD_isError(compressed_size)) {
            std::print(stderr, "Packet compression failed: {}\n",
                       ZSTD_getErrorName(compressed_size));
            return std::vector<uint8_t>(packet.begin(), packet.end());
        }

        compressed.resize(compressed_size);

        // Обновление статистики
        stats_.update_compression_stats(packet.size(), compressed_size);

        return compressed;
    }

    // Декомпрессия сетевого пакета
    std::expected<std::vector<uint8_t>, std::string>
    decompress_packet(std::span<const uint8_t> compressed) {
        // Получение размера оригинального пакета
        unsigned long long original_size = ZSTD_getFrameContentSize(
            compressed.data(), compressed.size());

        if (original_size == ZSTD_CONTENTSIZE_ERROR) {
            return std::unexpected("Invalid compressed packet");
        }

        if (original_size == ZSTD_CONTENTSIZE_UNKNOWN) {
            // Для пакетов без размера используем максимальный размер
            original_size = 64 * 1024;  // 64KB максимум для сетевого пакета
        }

        std::vector<uint8_t> decompressed(original_size);

        // Используем словарь для декомпрессии
        ZSTD_DCtx_refDDict(decompression_ctx_, dictionary_.decompression_dict());

        size_t decompressed_size = ZSTD_decompressDCtx(
            decompression_ctx_,
            decompressed.data(), decompressed.size(),
            compressed.data(), compressed.size()
        );

        if (ZSTD_isError(decompressed_size)) {
            return std::unexpected(ZSTD_getErrorName(decompressed_size));
        }

        decompressed.resize(decompressed_size);
        stats_.total_packets_decompressed++;

        return decompressed;
    }

    // Пакетная обработка (для bulk операций)
    std::vector<std::vector<uint8_t>> compress_packets_batch(
        const std::vector<std::span<const uint8_t>>& packets) {

        std::vector<std::vector<uint8_t>> results;
        results.reserve(packets.size());

        for (const auto& packet : packets) {
            results.push_back(compress_packet(packet));
        }

        return results;
    }

private:
    void load_network_dictionary() {
        std::filesystem::path dict_path = "data/dictionaries/network.zstdict";

        if (std::filesystem::exists(dict_path)) {
            if (auto result = dictionary_.load(dict_path); !result) {
                std::print(stderr, "Failed to load network dictionary: {}\n",
                           result.error());
                train_network_dictionary();
            }
        } else {
            train_network_dictionary();
        }
    }

    void train_network_dictionary() {
        // Обучение словаря на типичных сетевых пакетах
        VoxelDictionary::TrainingSamples samples;

        // Добавляем образцы сетевых пакетов
        samples.add_voxel_chunk(create_chunk_update_packet());
        samples.add_voxel_chunk(create_player_move_packet());
        samples.add_voxel_chunk(create_chat_message_packet());
        samples.add_voxel_chunk(create_entity_spawn_packet());

        // Обучение словаря
        if (auto result = dictionary_.train(samples, 50 * 1024); !result) {
            std::print(stderr, "Failed to train network dictionary: {}\n",
                       result.error());
            return;
        }

        // Сохранение словаря
        std::filesystem::create_directories("data/dictionaries");
        dictionary_.save("data/dictionaries/network.zstdict");

        std::print("Network dictionary trained and saved\n");
    }

    std::vector<uint8_t> create_chunk_update_packet() {
        // Создание образца пакета обновления чанка
        ChunkUpdatePacket packet{
            .coord = {0, 0, 0},
            .compressed_data = std::vector<uint8_t>(4096, 1),
            .timestamp = 1234567890
        };

        std::vector<uint8_t> buffer;
        glz::write_binary(packet, buffer);
        return buffer;
    }

    std::vector<uint8_t> create_player_move_packet() {
        // Создание образца пакета движения игрока
        PlayerMovePacket packet{
            .player_id = 1,
            .position = {100.0f, 50.0f, 200.0f},
            .rotation = {0.0f, 0.0f, 0.0f},
            .velocity = {10.0f, 0.0f, 5.0f}
        };

        std::vector<uint8_t> buffer;
        glz::write_binary(packet, buffer);
        return buffer;
    }

    // ... другие методы создания образцов пакетов
};
```

---

## Интеграция с системой сохранения мира

### Сжатие сейвов с прогрессивным уровнем сжатия

```cpp
#include <zstd.h>
#include <print>
#include <filesystem>
#include <fstream>

class WorldSaveCompressor {
    struct SaveMetadata {
        std::string version;
        std::chrono::system_clock::time_point save_time;
        size_t original_size;
        size_t compressed_size;
        int compression_level;
        double compression_ratio;
        std::string checksum;

        struct glaze {
            using T = SaveMetadata;
            static constexpr auto value = glz::object(
                "version", &T::version,
                "save_time", &T::save_time,
                "original_size", &T::original_size,
                "compressed_size", &T::compressed_size,
                "compression_level", &T::compression_level,
                "compression_ratio", &T::compression_ratio,
                "checksum", &T::checksum
            );
        };
    };

public:
    // Сохранение мира с автоматическим выбором уровня сжатия
    bool save_world(const WorldSave& world, const std::filesystem::path& path,
                   int target_compression_level = 12) {

        // Сериализация мира
        std::vector<uint8_t> serialized;
        glz::write_binary(world, serialized);

        // Прогрессивное сжатие (начинаем с высокого уровня, уменьшаем если медленно)
        int actual_level = target_compression_level;
        std::vector<uint8_t> compressed;
        std::chrono::milliseconds compression_time;

        while (actual_level >= 1) {
            auto start = std::chrono::high_resolution_clock::now();

            compressed = compress_with_level(serialized, actual_level);

            auto end = std::chrono::high_resolution_clock::now();
            compression_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            // Если сжатие заняло меньше 5 секунд, используем этот уровень
            if (compression_time < std::chrono::seconds(5)) {
                break;
            }

            // Уменьшаем уровень сжатия для следующей попытки
            actual_level -= 3;
            std::print("Compression level {} too slow ({}ms), trying level {}\n",
                       actual_level + 3, compression_time.count(), actual_level);
        }

        // Создание метаданных
        SaveMetadata metadata{
            .version = "1.0.0",
            .save_time = std::chrono::system_clock::now(),
            .original_size = serialized.size(),
            .compressed_size = compressed.size(),
            .compression_level = actual_level,
            .compression_ratio = static_cast<double>(serialized.size()) / compressed.size(),
            .checksum = calculate_checksum(compressed)
        };

        // Сохранение в файл
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            std::print(stderr, "Failed to open save file: {}\n", path.string());
            return false;
        }

        // Запись метаданных
        std::vector<uint8_t> metadata_buffer;
        glz::write_binary(metadata, metadata_buffer);

        uint32_t metadata_size = static_cast<uint32_t>(metadata_buffer.size());
        file.write(reinterpret_cast<const char*>(&metadata_size), sizeof(metadata_size));
        file.write(reinterpret_cast<const char*>(metadata_buffer.data()), metadata_buffer.size());

        // Запись сжатых данных
        file.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());

        std::print("World saved: {} -> {} bytes ({:.2f}x) in {}ms (level {})\n",
                   serialized.size(), compressed.size(),
                   metadata.compression_ratio,
                   compression_time.count(), actual_level);

        return true;
    }

    // Загрузка мира
    std::expected<WorldSave, std::string> load_world(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return std::unexpected("Failed to open save file");
        }

        // Чтение метаданных
        uint32_t metadata_size = 0;
        file.read(reinterpret_cast<char*>(&metadata_size), sizeof(metadata_size));

        std::vector<uint8_t> metadata_buffer(metadata_size);
        file.read(reinterpret_cast<char*>(metadata_buffer.data()), metadata_size);

        SaveMetadata metadata;
        auto error = glz::read_binary(metadata, metadata_buffer);
        if (error) {
            return std::unexpected("Failed to parse save metadata");
        }

        // Чтение сжатых данных
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        size_t data_size = file_size - sizeof(metadata_size) - metadata_size;

        file.seekg(sizeof(metadata_size) + metadata_size);
        std::vector<uint8_t> compressed(data_size);
        file.read(reinterpret_cast<char*>(compressed.data()), data_size);

        // Проверка checksum
        std::string actual_checksum = calculate_checksum(compressed);
        if (actual_checksum != metadata.checksum) {
            return std::unexpected("Save file checksum mismatch");
        }

        // Декомпрессия
        std::vector<uint8_t> decompressed = decompress_data(compressed);

        if (decompressed.size() != metadata.original_size) {
            return std::unexpected("Decompressed size mismatch");
        }

        // Десериализация мира
        WorldSave world;
        error = glz::read_binary(world, decompressed);

        if (error) {
            return std::unexpected(glz::format_error(error, "world save"));
        }

        std::print("World loaded: {} -> {} bytes ({:.2f}x)\n",
                   metadata.compressed_size, metadata.original_size,
                   metadata.compression_ratio);

        return world;
    }

private:
    std::vector<uint8_t> compress_with_level(std::span<const uint8_t> data, int level) {
        ZSTD_CCtx* cctx = ZSTD_createCCtx();
        ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, level);
        ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1);

        size_t bound = ZSTD_compressBound(data.size());
        std::vector<uint8_t> compressed(bound);

        size_t compressed_size = ZSTD_compress2(
            cctx,
            compressed.data(), bound,
            data.data(), data.size()
        );

        ZSTD_freeCCtx(cctx);

        if (ZSTD_isError(compressed_size)) {
            throw std::runtime_error(ZSTD_getErrorName(compressed_size));
        }

        compressed.resize(compressed_size);
        return compressed;
    }

    std::vector<uint8_t> decompress_data(std::span<const uint8_t> compressed) {
        unsigned long long original_size = ZSTD_getFrameContentSize(
            compressed.data(), compressed.size());

        if (original_size == ZSTD_CONTENTSIZE_ERROR ||
            original_size == ZSTD_CONTENTSIZE_UNKNOWN) {
            throw std::runtime_error("Invalid compressed data");
        }

        std::vector<uint8_t> decompressed(original_size);

        ZSTD_DCtx* dctx = ZSTD_createDCtx();
        size_t decompressed_size = ZSTD_decompressDC

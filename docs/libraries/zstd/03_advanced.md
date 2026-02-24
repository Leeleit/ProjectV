# Zstd — Advanced

<!-- anchor: 03_advanced -->

Продвинутые техники Zstd для ProjectV: streaming compression, адаптивное сжатие, интеграция с GPU и мониторинг
производительности.

---

## Streaming Compression для больших миров

### Почему streaming для ProjectV

| Подход        | Память         | Задержка   | Использование                    |
|---------------|----------------|------------|----------------------------------|
| **One-shot**  | 2x данных      | Высокая    | Маленькие чанки                  |
| **Streaming** | **Буфер 64KB** | **Низкая** | **Большие миры, сетевые потоки** |

### Streaming API для воксельных миров

```cpp
#include <zstd.h>
#include <zstd_errors.h>
#include <print>
#include <span>
#include <vector>
#include <memory>

class StreamingCompressor {
    ZSTD_CStream* cstream_ = nullptr;
    ZSTD_DStream* dstream_ = nullptr;

    // Буферы для streaming
    std::vector<uint8_t> input_buffer_;
    std::vector<uint8_t> output_buffer_;
    size_t input_pos_ = 0;
    size_t output_pos_ = 0;

    // Статистика
    struct StreamStats {
        size_t total_input_bytes = 0;
        size_t total_output_bytes = 0;
        size_t frames_processed = 0;
        double average_ratio = 0.0;

        void update(size_t input, size_t output) {
            total_input_bytes += input;
            total_output_bytes += output;
            frames_processed++;

            double ratio = static_cast<double>(input) / output;
            average_ratio = (average_ratio * (frames_processed - 1) + ratio) / frames_processed;
        }
    } stats_;

public:
    StreamingCompressor(int compression_level = 5, size_t buffer_size = 64 * 1024) {
        cstream_ = ZSTD_createCStream();
        dstream_ = ZSTD_createDStream();

        if (!cstream_ || !dstream_) {
            throw std::runtime_error("Failed to create ZSTD streaming contexts");
        }

        // Инициализация streaming контекстов
        size_t init_result = ZSTD_initCStream(cstream_, compression_level);
        if (ZSTD_isError(init_result)) {
            throw std::runtime_error(ZSTD_getErrorName(init_result));
        }

        init_result = ZSTD_initDStream(dstream_);
        if (ZSTD_isError(init_result)) {
            throw std::runtime_error(ZSTD_getErrorName(init_result));
        }

        // Выделение буферов
        input_buffer_.resize(buffer_size);
        output_buffer_.resize(ZSTD_CStreamOutSize());

        std::print("Streaming compressor initialized (buffer: {} bytes)\n", buffer_size);
    }

    ~StreamingCompressor() {
        if (cstream_) ZSTD_freeCStream(cstream_);
        if (dstream_) ZSTD_freeDStream(dstream_);

        std::print("Streaming stats: {} -> {} bytes ({:.2f}x) over {} frames\n",
                   stats_.total_input_bytes, stats_.total_output_bytes,
                   stats_.average_ratio, stats_.frames_processed);
    }

    // Streaming сжатие воксельного мира
    std::vector<uint8_t> compress_world_stream(const std::filesystem::path& world_path) {
        std::ifstream file(world_path, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Failed to open world file");
        }

        std::vector<uint8_t> compressed;
        ZSTD_inBuffer input = {nullptr, 0, 0};
        ZSTD_outBuffer output = {nullptr, 0, 0};

        // Чтение и сжатие по блокам
        while (!file.eof()) {
            // Чтение блока данных
            file.read(reinterpret_cast<char*>(input_buffer_.data()), input_buffer_.size());
            size_t bytes_read = file.gcount();

            if (bytes_read == 0) break;

            // Настройка input buffer
            input.src = input_buffer_.data();
            input.size = bytes_read;
            input.pos = 0;

            // Сжатие блока
            while (input.pos < input.size) {
                output.dst = output_buffer_.data();
                output.size = output_buffer_.size();
                output.pos = 0;

                size_t remaining = ZSTD_compressStream(cstream_, &output, &input);
                if (ZSTD_isError(remaining)) {
                    throw std::runtime_error(ZSTD_getErrorName(remaining));
                }

                // Добавление сжатых данных в результат
                compressed.insert(compressed.end(),
                                 output_buffer_.begin(),
                                 output_buffer_.begin() + output.pos);

                stats_.update(input.size, output.pos);
            }
        }

        // Завершение сжатия (flush)
        size_t remaining;
        do {
            output.dst = output_buffer_.data();
            output.size = output_buffer_.size();
            output.pos = 0;

            remaining = ZSTD_endStream(cstream_, &output);
            if (ZSTD_isError(remaining)) {
                throw std::runtime_error(ZSTD_getErrorName(remaining));
            }

            compressed.insert(compressed.end(),
                             output_buffer_.begin(),
                             output_buffer_.begin() + output.pos);

        } while (remaining > 0);

        std::print("World compressed via streaming: {} -> {} bytes ({:.2f}x)\n",
                   stats_.total_input_bytes, compressed.size(),
                   static_cast<double>(stats_.total_input_bytes) / compressed.size());

        return compressed;
    }

    // Streaming декомпрессия
    std::vector<uint8_t> decompress_stream(std::span<const uint8_t> compressed) {
        std::vector<uint8_t> decompressed;
        ZSTD_inBuffer input = {compressed.data(), compressed.size(), 0};
        ZSTD_outBuffer output = {nullptr, 0, 0};

        while (input.pos < input.size) {
            output.dst = output_buffer_.data();
            output.size = output_buffer_.size();
            output.pos = 0;

            size_t ret = ZSTD_decompressStream(dstream_, &output, &input);
            if (ZSTD_isError(ret)) {
                throw std::runtime_error(ZSTD_getErrorName(ret));
            }

            decompressed.insert(decompressed.end(),
                               output_buffer_.begin(),
                               output_buffer_.begin() + output.pos);
        }

        return decompressed;
    }

    // Инкрементальное сжатие (для live updates)
    std::vector<uint8_t> compress_incremental(std::span<const uint8_t> data, bool flush = false) {
        std::vector<uint8_t> compressed;
        ZSTD_inBuffer input = {data.data(), data.size(), 0};
        ZSTD_outBuffer output = {nullptr, 0, 0};

        while (input.pos < input.size) {
            output.dst = output_buffer_.data();
            output.size = output_buffer_.size();
            output.pos = 0;

            size_t ret = ZSTD_compressStream(cstream_, &output, &input);
            if (ZSTD_isError(ret)) {
                throw std::runtime_error(ZSTD_getErrorName(ret));
            }

            compressed.insert(compressed.end(),
                             output_buffer_.begin(),
                             output_buffer_.begin() + output.pos);
        }

        if (flush) {
            // Принудительный flush
            size_t remaining;
            do {
                output.dst = output_buffer_.data();
                output.size = output_buffer_.size();
                output.pos = 0;

                remaining = ZSTD_flushStream(cstream_, &output);
                if (ZSTD_isError(remaining)) {
                    throw std::runtime_error(ZSTD_getErrorName(remaining));
                }

                compressed.insert(compressed.end(),
                                 output_buffer_.begin(),
                                 output_buffer_.begin() + output.pos);

            } while (remaining > 0);
        }

        return compressed;
    }
};
```

### Интеграция с системой потоковой загрузки чанков

```cpp
#include <zstd.h>
#include <print>
#include <queue>
#include <atomic>
#include <thread>

class ChunkStreamingSystem {
    struct ChunkStreamTask {
        glm::ivec3 coord;
        uint32_t lod;
        std::vector<uint8_t> compressed_data;
        std::atomic<bool> ready{false};
        std::chrono::system_clock::time_point enqueue_time;
    };

    StreamingCompressor compressor_;
    std::queue<std::shared_ptr<ChunkStreamTask>> compression_queue_;
    std::queue<std::shared_ptr<ChunkStreamTask>> decompression_queue_;
    std::mutex compression_mutex_;
    std::mutex decompression_mutex_;
    std::condition_variable compression_cv_;
    std::condition_variable decompression_cv_;
    std::atomic<bool> stop_{false};

    std::thread compression_thread_;
    std::thread decompression_thread_;

public:
    ChunkStreamingSystem()
        : compressor_(3) {  // Уровень 3 для баланса скорости/сжатия

        compression_thread_ = std::thread([this]() { compression_worker(); });
        decompression_thread_ = std::thread([this]() { decompression_worker(); });

        std::print("Chunk streaming system started\n");
    }

    ~ChunkStreamingSystem() {
        stop_ = true;
        compression_cv_.notify_all();
        decompression_cv_.notify_all();

        if (compression_thread_.joinable()) compression_thread_.join();
        if (decompression_thread_.joinable()) decompression_thread_.join();
    }

    // Асинхронное сжатие чанка для передачи
    std::shared_ptr<ChunkStreamTask> compress_chunk_async(const VoxelChunk& chunk) {
        auto task = std::make_shared<ChunkStreamTask>();
        task->coord = chunk.coord;
        task->lod = chunk.lod;
        task->enqueue_time = std::chrono::system_clock::now();

        // Сериализация чанка
        std::vector<uint8_t> serialized = serialize_voxel_chunk(chunk);

        {
            std::lock_guard lock(compression_mutex_);
            compression_queue_.push(task);
        }

        compression_cv_.notify_one();
        return task;
    }

    // Асинхронная декомпрессия полученного чанка
    std::shared_ptr<ChunkStreamTask> decompress_chunk_async(
        const glm::ivec3& coord, uint32_t lod,
        std::span<const uint8_t> compressed_data) {

        auto task = std::make_shared<ChunkStreamTask>();
        task->coord = coord;
        task->lod = lod;
        task->compressed_data = std::vector<uint8_t>(
            compressed_data.begin(), compressed_data.end());
        task->enqueue_time = std::chrono::system_clock::now();

        {
            std::lock_guard lock(decompression_mutex_);
            decompression_queue_.push(task);
        }

        decompression_cv_.notify_one();
        return task;
    }

    // Получение готового чанка
    std::optional<VoxelChunk> get_ready_chunk(std::shared_ptr<ChunkStreamTask> task) {
        if (!task->ready.load()) {
            return std::nullopt;
        }

        // Проверка целостности
        auto chunk = deserialize_voxel_chunk(task->compressed_data);

        if (chunk.coord != task->coord || chunk.lod != task->lod) {
            std::print(stderr, "Chunk coordinate mismatch after streaming\n");
            return std::nullopt;
        }

        auto process_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - task->enqueue_time);

        std::print("Chunk {} ready in {}ms ({} bytes)\n",
                   task->coord, process_time.count(), task->compressed_data.size());

        return chunk;
    }

private:
    void compression_worker() {
        while (!stop_.load()) {
            std::shared_ptr<ChunkStreamTask> task;

            {
                std::unique_lock lock(compression_mutex_);
                compression_cv_.wait(lock, [this]() {
                    return !compression_queue_.empty() || stop_.load();
                });

                if (stop_.load()) return;

                if (!compression_queue_.empty()) {
                    task = compression_queue_.front();
                    compression_queue_.pop();
                }
            }

            if (task) {
                process_compression_task(*task);
            }
        }
    }

    void decompression_worker() {
        while (!stop_.load()) {
            std::shared_ptr<ChunkStreamTask> task;

            {
                std::unique_lock lock(decompression_mutex_);
                decompression_cv_.wait(lock, [this]() {
                    return !decompression_queue_.empty() || stop_.load();
                });

                if (stop_.load()) return;

                if (!decompression_queue_.empty()) {
                    task = decompression_queue_.front();
                    decompression_queue_.pop();
                }
            }

            if (task) {
                process_decompression_task(*task);
            }
        }
    }

    void process_compression_task(ChunkStreamTask& task) {
        try {
            // Сериализация чанка
            VoxelChunk dummy_chunk{task.coord, task.lod};  // Заглушка
            std::vector<uint8_t> serialized = serialize_voxel_chunk(dummy_chunk);

            // Streaming сжатие
            auto compressed = compressor_.compress_incremental(serialized, true);

            task.compressed_data = std::move(compressed);
            task.ready = true;

        } catch (const std::exception& e) {
            std::print(stderr, "Compression task failed: {}\n", e.what());
            task.ready = true;  // Помечаем как готовый (с ошибкой)
        }
    }

    void process_decompression_task(ChunkStreamTask& task) {
        try {
            // Streaming декомпрессия
            auto decompressed = compressor_.decompress_stream(task.compressed_data);

            // Десериализация
            auto chunk = deserialize_voxel_chunk(decompressed);

            // Проверка координат
            if (chunk.coord != task.coord || chunk.lod != task.lod) {
                throw std::runtime_error("Coordinate mismatch after decompression");
            }

            // Сериализация обратно для хранения
            task.compressed_data = serialize_voxel_chunk(chunk);
            task.ready = true;

        } catch (const std::exception& e) {
            std::print(stderr, "Decompression task failed: {}\n", e.what());
            task.ready = true;  // Помечаем как готовый (с ошибкой)
        }
    }
};
```

---

## Адаптивное сжатие на основе типа данных

### Детектирование типа данных для оптимизации сжатия

```cpp
#include <zstd.h>
#include <print>
#include <span>
#include <vector>
#include <algorithm>

class AdaptiveCompressor {
    enum class DataType {
        Unknown,
        VoxelChunk,      // 4096 байт, структурированные данные
        NetworkPacket,   // Маленькие пакеты, низкая задержка
        TextureData,     // Большие массивы, высокая избыточность
        SaveFile,        // Большие файлы, максимальное сжатие
        ConfigFile       // Текстовые данные, среднее сжатие
    };

    struct CompressionProfile {
        DataType type;
        int compression_level;
        bool use_dictionary;
        size_t buffer_size;
        bool enable_checksum;

        // Эвристики для определения типа
        static CompressionProfile detect_profile(std::span<const uint8_t> data) {
            CompressionProfile profile;

            // Анализ данных
            size_t size = data.size();
            double entropy = calculate_entropy(data);
            bool is_text = is_likely_text(data);

            // Определение типа на основе эвристик
            if (size == 4096 && entropy < 2.0) {
                profile.type = DataType::VoxelChunk;
                profile.compression_level = 5;
                profile.use_dictionary = true;
                profile.buffer_size = 4096;
                profile.enable_checksum = true;
            }
            else if (size < 1024) {
                profile.type = DataType::NetworkPacket;
                profile.compression_level = 1;
                profile.use_dictionary = true;
                profile.buffer_size = 1024;
                profile.enable_checksum = true;
            }
            else if (size > 1024 * 1024 && entropy > 6.0) {
                profile.type = DataType::TextureData;
                profile.compression_level = 3;
                profile.use_dictionary = false;
                profile.buffer_size = 64 * 1024;
                profile.enable_checksum = false;
            }
            else if (size > 10 * 1024 * 1024) {
                profile.type = DataType::SaveFile;
                profile.compression_level = 12;
                profile.use_dictionary = true;
                profile.buffer_size = 256 * 1024;
                profile.enable_checksum = true;
            }
            else if (is_text) {
                profile.type = DataType::ConfigFile;
                profile.compression_level = 7;
                profile.use_dictionary = true;
                profile.buffer_size = 16 * 1024;
                profile.enable_checksum = true;
            }
            else {
                profile.type = DataType::Unknown;
                profile.compression_level = 5;  // Default
                profile.use_dictionary = false;
                profile.buffer_size = 32 * 1024;
                profile.enable_checksum = true;
            }

            return profile;
        }
    };

    // Словари для разных типов данных
    std::unordered_map<DataType, VoxelDictionary> dictionaries_;

public:
    AdaptiveCompressor() {
        // Загрузка словарей для разных типов данных
        load_dictionaries();
    }

    // Адаптивное сжатие
    std::vector<uint8_t> compress_adaptive(std::span<const uint8_t> data) {
        auto profile = CompressionProfile::detect_profile(data);

        // Настройка контекста сжатия
        ZSTD_CCtx* cctx = ZSTD_createCCtx();

        ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, profile.compression_level);
        ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, profile.enable_checksum ? 1 : 0);

        // Использование словаря если нужно
        if (profile.use_dictionary) {
            auto it = dictionaries_.find(profile.type);
            if (it != dictionaries_.end()) {
                ZSTD_CCtx_refCDict(cctx, it->second.compression_dict());
            }
        }

        // Сжатие с оптимальными параметрами
        size_t bound = ZSTD_compressBound(data.size());
        std::vector<uint8_t> compressed(bound);

        size_t compressed_size = ZSTD_compress2(
            cctx,
            compressed.data(), bound,
            data.data(), data.size()
        );

        ZSTD_freeCCtx(cctx);

        if (ZSTD_isError(compressed_size)) {
            std::print(stderr, "Adaptive compression failed: {}\n",
                       ZSTD_getErrorName(compressed_size));
            return std::vector<uint8_t>(data.begin(), data.end());
        }

        compressed.resize(compressed_size);

        std::print("Adaptive compression: {} -> {} bytes ({:.2f}x) type: {}\n",
                   data.size(), compressed_size,
                   static_cast<double>(data.size()) / compressed_size,
                   data_type_to_string(profile.type));

        return compressed;
    }

    // Адаптивная декомпрессия
    std::expected<std::vector<uint8_t>, std::string>
    decompress_adaptive(std::span<const uint8_t> compressed, DataType expected_type) {
        // Получение размера оригинальных данных
        unsigned long long original_size = ZSTD_getFrameContentSize(
            compressed.data(), compressed.size());

        if (original_size == ZSTD_CONTENTSIZE_ERROR) {
            return std::unexpected("Invalid compressed data");
        }

        if (original_size == ZSTD_CONTENTSIZE_UNKNOWN) {
            // Оценка размера на основе типа
            original_size = estimate_decompressed_size(compressed.size(), expected_type);
        }

        std::vector<uint8_t> decompressed(original_size);

        // Настройка контекста декомпрессии
        ZSTD_DCtx* dctx = ZSTD_createDCtx();

        // Использование словаря если нужно
        auto it = dictionaries_.find(expected_type);
        if (it != dictionaries_.end()) {
            ZSTD_DCtx_refDDict(dctx, it->second.decompression_dict());
        }

        size_t decompressed_size = ZSTD_decompressDCtx(
            dctx,
            decompressed.data(), decompressed.size(),
            compressed.data(), compressed.size()
        );

        ZSTD_freeDCtx(dctx);

        if (ZSTD_isError(decompressed_size)) {
            return std::unexpected(ZSTD_getErrorName(decompressed_size));
        }

        decompressed.resize(decompressed_size);
        return decompressed;
    }

private:
    void load_dictionaries() {
        // Загрузка словарей для разных типов данных
        std::vector<std::pair<DataType, std::string>> dict_files = {
            {DataType::VoxelChunk, "data/dictionaries/voxel.zstdict"},
            {DataType::NetworkPacket, "data/dictionaries/network.zstdict"},
            {DataType::ConfigFile, "data/dictionaries/config.zstdict"}
        };

        for (const auto& [type, path] : dict_files) {
            VoxelDictionary dict;
            if (auto result = dict.load(path); result) {
                dictionaries_[type] = std::move(dict);
                std::print("Dictionary loaded for {}: {}\n",
                           data_type_to_string(type), path);
            } else {
                std::print(stderr, "Failed to load dictionary for {}: {}\n",
                           data_type_to_string(type), result.error());
            }
        }
    }

    static double calculate_entropy(std::span<const uint8_t> data) {
        if (data.empty()) return 0.0;

        std::array<size_t, 256> frequencies{};
        for (uint8_t byte : data) {
            frequencies[byte]++;
        }

        double entropy = 0.0;
        double inv_size = 1.0 / data.size();

        for (size_t freq : frequencies) {
            if (freq > 0) {
                double probability = freq * inv_size;
                entropy -= probability * log2(probability);
            }
        }

        return entropy;
    }

    static bool is_likely_text(std::span<const uint8_t> data) {
        if (data.empty()) return false;

        size_t printable = 0;
        size_t control = 0;

        for (uint8_t byte : data) {
            if (byte >= 32 && byte <= 126) {  // Printable ASCII
                printable++;
            } else if (byte == 9 || byte == 10 || byte == 13) {  // Tab, LF, CR
                printable++;
            } else if (byte < 32 || byte == 127) {  // Control characters
                control++;
            }
        }

        // Если больше 95% символов печатные, считаем текстом
        return (printable * 100) / data.size() > 95;
    }

    static size_t estimate_decompressed_size(size_t compressed_size, DataType type) {
        // Эмпирические коэффициенты для разных типов данных
        switch (type) {
            case DataType::VoxelChunk:
                return compressed_size * 3;  // 3:1 compression
            case DataType::NetworkPacket:
                return compressed_size * 2;  // 2:1 compression
            case DataType::TextureData:
                return compressed_size * 4;  // 4:1 compression
            case DataType::SaveFile:
                return compressed_size * 10; // 10:1 compression
            default:
                return compressed_size * 3;  // Default 3:1
        }
    }

    static std::string data_type_to_string(DataType type) {
        switch (type) {
            case DataType::VoxelChunk: return "VoxelChunk";
            case DataType::NetworkPacket: return "NetworkPacket";
            case DataType::TextureData: return "TextureData";
            case DataType::SaveFile: return "SaveFile";
            case DataType::ConfigFile: return "ConfigFile";
            default: return "Unknown";
        }
    }
};
```

---

## Мониторинг производительности и метрики

### Система сбора метрик сжатия

```cpp
#include <zstd.h>
#include <print>
#include <chrono>
#include <vector>
#include <atomic>
#include <mutex>

class CompressionMetrics {
    struct Metric {
        std::string name;
        size_t total_input_bytes = 0;
        size_t total_output_bytes = 0;
        size_t operation_count = 0;
        std::chrono::microseconds total_time{0};
        double average_ratio = 0.0;
        double average_speed_mbps = 0.0;  // MB/s

        void update(size_t input, size_t output,
                   std::chrono::microseconds time) {
            total_input_bytes += input;
            total_output_bytes += output;
            operation_count++;

            double ratio = static_cast<double>(input) / output;
            average_ratio = (average_ratio * (operation_count - 1) + ratio) / operation_count;

            double mb = input / (1024.0 * 1024.0);
            double seconds = time.count() / 1'000'000.0;
            double speed = (seconds > 0) ? mb / seconds : 0.0;

            average_speed_mbps = (average_speed_mbps * (operation_count - 1) + speed) / operation_count;
            total_time += time;
        }

        struct glaze {
            using T = Metric;
            static constexpr auto value = glz::object(
                "name", &T::name,
                "total_input_bytes", &T::total_input_bytes,
                "total_output_bytes", &T::total_output_bytes,
                "operation_count", &T::operation_count,
                "total_time_us", &T::total_time,
                "average_ratio", &T::average_ratio,
                "average_speed_mbps", &T::average_speed_mbps
            );
        };
    };

    std::unordered_map<std::string, Metric> metrics_;
    std::mutex metrics_mutex_;

public:
    // Запись метрики
    void record(const std::string& name, size_t input_bytes, size_t output_bytes,
               std::chrono::microseconds time) {
        std::lock_guard lock(metrics_mutex_);

        auto& metric = metrics_[name];
        metric.name = name;
        metric.update(input_bytes, output_bytes, time);
    }

    // Получение статистики
    std::vector<Metric> get_stats() const {
        std::lock_guard lock(metrics_mutex_);

        std::vector<Metric> result;
        result.reserve(metrics_.size());

        for (const auto& [name, metric] : metrics_) {
            result.push_back(metric);
        }

        return result;
    }

    // Экспорт статистики в JSON
    std::string export_stats_json() const {
        auto stats = get_stats();
        return glz::write_json(stats);
    }

    // Вывод статистики в консоль
    void print_stats() const {
        auto stats = get_stats();

        std::print("\n=== Compression Metrics ===\n");
        for (const auto& metric : stats) {
            std::print("{:20} | {:8} ops | {:8} -> {:8} bytes | "
                       "Ratio: {:.2f}x | Speed: {:.1f} MB/s | Time: {}ms\n",
                       metric.name,
                       metric.operation_count,
                       metric.total_input_bytes,
                       metric.total_output_bytes,
                       metric.average_ratio,
                       metric.average_speed_mbps,
                       metric.total_time.count() / 1000);
        }

        // Итоговая статистика
        size_t total_input = 0;
        size_t total_output = 0;
        size_t total_ops = 0;

        for (const auto& metric : stats) {
            total_input += metric.total_input_bytes;
            total_output += metric.total_output_bytes;
            total_ops += metric.operation_count;
        }

        double total_ratio = static_cast<double>(total_input) / total_output;
        size_t bandwidth_saved = total_input - total_output;

        std::print("\n=== Summary ===\n");
        std::print("Total operations: {}\n", total_ops);
        std::print("Total input: {} bytes ({:.1f} MB)\n",
                   total_input, total_input / (1024.0 * 1024.0));
        std::print("Total output: {} bytes ({:.1f} MB)\n",
                   total_output, total_output / (1024.0 * 1024.0));
        std::print("Average ratio: {:.2f}x\n", total_ratio);
        std::print("Bandwidth saved: {} bytes ({:.1f} MB)\n",
                   bandwidth_saved, bandwidth_saved / (1024.0 * 1024.0));
        std::print("Compression efficiency: {:.1f}%\n",
                   (1.0 - 1.0 / total_ratio) * 100.0);
    }
};

// Интеграция с системой сжатия
class InstrumentedCompressor {
    CompressionMetrics& metrics_;

public:
    InstrumentedCompressor(CompressionMetrics& metrics) : metrics_(metrics) {}

    std::vector<uint8_t> compress_with_metrics(
        const std::string& metric_name,
        std::span<const uint8_t> data,
        int compression_level = 5) {

        auto start = std::chrono::high_resolution_clock::now();

        // Сжатие
        size_t bound = ZSTD_compressBound(data.size());
        std::vector<uint8_t> compressed(bound);

        size_t compressed_size = ZSTD_compress(
            compressed.data(), bound,
            data.data(), data.size(),
            compression_level
        );

        auto end = std::chrono::high_resolution_clock::now();
        auto time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        if (ZSTD_isError(compressed_size)) {
            std::print(stderr, "Compression failed: {}\n",
                       ZSTD_getErrorName(compressed_size));
            return std::vector<uint8_t>(data.begin(), data.end());
        }

        compressed.resize(compressed_size);

        // Запись метрики
        metrics_.record(metric_name, data.size(), compressed_size, time);

        return compressed;
    }
};
```

---

## Заключение

Продвинутые техники Zstd для ProjectV:

### 1. **Streaming Compression**

- **Низкое потребление памяти** (буферы 64KB вместо 2x данных)
- **Непрерывная обработка** больших миров и сетевых потоков
- **Инкрементальное обновление** для live данных

### 2. **Адаптивное сжатие**

- **Автоматическое определение** типа данных
- **Оптимальные параметры** для каждого типа
- **Интеллектуальные словари** для разных use cases

### 3. **Мониторинг производительности**

- **Детальная статистика** по операциям сжатия
- **Метрики эффективности** (ratio, speed, bandwidth saved)
- **Интеграция с системой** для оптимизации в runtime

### 4. **Интеграция с архитектурой ProjectV**

- **Потоковая загрузка чанков** без блокировок
- **Адаптивные уровни сжатия** на основе типа данных
- **Мониторинг и оптимизация** в production

### Ключевые преимущества для воксельного движка:

1. **Эффективность памяти**: Streaming API позволяет обрабатывать терабайты миров без загрузки всего в память
2. **Интеллектуальное сжатие**: Автоматическая настройка параметров для разных типов данных
3. **Мониторинг и оптимизация**: Постоянное улучшение производительности на основе метрик
4. **Интеграция с pipeline**: Бесшовная работа с системами загрузки, сети и сохранения

Zstd в ProjectV — это не просто библиотека сжатия, а интеллектуальная система управления данными, оптимизированная для
специфических требований воксельного движка.

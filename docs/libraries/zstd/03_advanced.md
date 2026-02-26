# Zstd: Продвинутые оптимизации и DOD

## Data-Oriented Design

### SoA для сжатых данных

Традиционный подход (AoS) группирует данные по объекту:

```cpp
// AoS - плохо для кэша
struct CompressedChunkAoS {
    glm::ivec3 coord;
    std::vector<uint8_t> data;
    size_t original_size;
    int compression_level;
};
```

SoA (Structure of Arrays) разделяет данные для последовательного доступа:

```cpp
// SoA - хорошо для кэша
alignas(64) struct CompressedChunksSoA {
    // Хотя данные - векторы, структура выровнена
    std::vector<glm::ivec3> coords;
    std::vector<std::vector<uint8_t>> datas;
    std::vector<size_t> original_sizes;
    std::vector<int> compression_levels;
    std::vector<size_t> compressed_sizes;
};
```

### Hot/Cold Separation

Горячие данные (читаются каждый кадр):

- Размер сжатых данных
- Указатель на буфер
- Флаг готовности

Холодные данные (читаются редко):

- Уровень сжатия
- Dictionary ID
- Метаданные

```cpp
alignas(64) struct alignas(64) HotChunkMetadata {
    uint32_t compressed_size;
    uint32_t original_size;
    uint32_t buffer_offset;
    uint8_t flags;           // Ready, InProgress, Error
    char padding[3];
};

struct ColdChunkMetadata {
    int compression_level;
    uint32_t dictionary_id;
    std::chrono::system_clock::time_point created;
    uint64_t checksum;
};
```

### Cache Line Alignment

```cpp
// Выравнивание по 64 байта (кэш-линия)
alignas(64) struct alignas(64) AlignedCompressionBuffer {
    uint8_t data[65536];  // 64 KB - типичный размер блока
    size_t position;
    size_t capacity;
};

static_assert(sizeof(AlignedCompressionBuffer) == 65536,
              "Must be cache-aligned");
```

---

## Zero-Copy сжатие

### Прямое сжатие в GPU-буфер

```cpp
#include <zstd.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <span>

// Zero-copy: сжатие напрямую в pre-аллоцированный буфер
class ZeroCopyCompressor {
public:
    // Инициализация с GPU-буфером
    void init(VmaAllocator allocator, size_t max_size) {
        allocator_ = allocator;

        // Аллокация GPU-буфера для сжатых данных
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = max_size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        vmaCreateBuffer(allocator_, &buffer_info, &alloc_info,
                       &gpu_buffer_, &gpu_allocation_, nullptr);

        // CPU-буфер для результата
        cpu_buffer_.resize(max_size);
    }

    // Сжатие с минимальным копированием
    std::expected<size_t, std::string> compress(
        std::span<const uint8_t> src,
        ZSTD_CDict* cdict = nullptr
    ) {
        ZSTD_CCtx* cctx = ZSTD_createCCtx();
        if (!cctx) {
            return std::unexpected("Failed to create context");
        }

        if (cdict) {
            ZSTD_CCtx_refCDict(cctx, cdict);
        }

        ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, 5);

        // Сжатие напрямую в наш буфер
        const size_t result = ZSTD_compress2(
            cctx,
            cpu_buffer_.data(), cpu_buffer_.size(),
            src.data(), src.size()
        );

        ZSTD_freeCCtx(cctx);

        if (ZSTD_isError(result)) {
            return std::unexpected(ZSTD_getErrorName(result));
        }

        // Теперь копируем в GPU буфер
        copy_to_gpu(result);

        return result;
    }

    VkBuffer GetGPUBuffer() const { return gpu_buffer_; }

private:
    void copy_to_gpu(size_t size) {
        // CPU->GPU copy через staging
        // В реальном коде использовать async operations
    }

    VmaAllocator allocator_;
    VkBuffer gpu_buffer_ = VK_NULL_HANDLE;
    VmaAllocation gpu_allocation_ = nullptr;
    std::vector<uint8_t> cpu_buffer_;
};
```

### Memory Mapping для больших файлов

```cpp
#include <sys/mman.h>
#include <zstd.h>
#include <span>

// Memory-mapped сжатие для больших данных
class MappedFileCompressor {
public:
    bool map(const std::string& path) {
        fd_ = open(path.c_str(), O_RDONLY);
        if (fd_ < 0) return false;

        struct stat sb;
        fstat(fd_, &sb);
        size_ = sb.st_size;

        // Memory map файла
        mapped_ = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
        if (mapped_ == MAP_FAILED) {
            close(fd_);
            return false;
        }

        // Подсказки ОС
        madvise(mapped_, size_, MADV_SEQUENTIAL);
        posix_fadvise(fd_, 0, 0, POSIX_FADV_SEQUENTIAL);

        return true;
    }

    // Сжатие mmap'd данных без копирования
    std::expected<std::vector<uint8_t>, std::string> compress_mapped(
        size_t offset,
        size_t length
    ) {
        std::span<const uint8_t> src(
            static_cast<const uint8_t*>(mapped_) + offset,
            length
        );

        const size_t bound = ZSTD_compressBound(length);
        std::vector<uint8_t> compressed(bound);

        const size_t result = ZSTD_compress(
            compressed.data(), bound,
            src.data(), src.size(),
            5
        );

        if (ZSTD_isError(result)) {
            return std::unexpected(ZSTD_getErrorName(result));
        }

        compressed.resize(result);
        return compressed;
    }

    ~MappedFileCompressor() {
        if (mapped_ && mapped_ != MAP_FAILED) {
            munmap(mapped_, size_);
        }
        if (fd_ >= 0) close(fd_);
    }

private:
    int fd_ = -1;
    size_t size_ = 0;
    void* mapped_ = nullptr;
};
```

---

## Job System интеграция

### Параллельное сжатие чанков

```cpp
#include <flecs.h>
#include <zstd.h>
#include <vector>
#include <span>
#include <expected>

// Job для сжатия одного чанка
struct CompressionJob {
    uint64_t chunk_key;
    std::span<const uint8_t> data;
    int level;
    std::vector<uint8_t> result;
};

// Параллельное сжатие через Flecs worker threads
class ParallelChunkCompressor {
public:
    ParallelChunkCompressor(flecs::world& world, uint32_t worker_count)
        : world_(world)
    {
        // Запускаем worker threads для сжатия
        for (uint32_t i = 0; i < worker_count; ++i) {
            workers_.emplace_back([this, i]() {
                worker_loop(i);
            });
        }
    }

    ~ParallelChunkCompressor() {
        running_ = false;
        cv_.notify_all();
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
    }

    // Поставить задачу в очередь
    void enqueue(CompressionJob job) {
        {
            std::lock_guard lock(queue_mutex_);
            job_queue_.push(std::move(job));
        }
        cv_.notify_one();
    }

    // Обработка очереди в ECS системе
    void process_queue() {
        while (true) {
            std::optional<CompressionJob> job;
            {
                std::lock_guard lock(queue_mutex_);
                if (result_queue_.empty()) break;
                job = std::move(result_queue_.front());
                result_queue_.pop();
            }

            if (job && !job->result.empty()) {
                // Установка результата в ECS
                world_.set<CompressedChunkData>(flecs::entity(job->chunk_key),
                    CompressedChunkData{
                        .data = std::move(job->result),
                        .original_size = job->data.size()
                    });
            }
        }
    }

private:
    void worker_loop(uint32_t id) {
        while (running_) {
            CompressionJob job;
            {
                std::unique_lock lock(queue_mutex_);
                cv_.wait(lock, [this]() {
                    return !job_queue_.empty() || !running_;
                });

                if (!running_ && job_queue_.empty()) return;
                if (job_queue_.empty()) continue;

                job = std::move(job_queue_.front());
                job_queue_.pop();
            }

            // Выполнение работы
            execute_job(job);
        }
    }

    void execute_job(CompressionJob& job) {
        const size_t bound = ZSTD_compressBound(job.data.size());
        job.result.resize(bound);

        const size_t result = ZSTD_compress(
            job.result.data(), bound,
            job.data.data(), job.data.size(),
            job.level
        );

        if (!ZSTD_isError(result)) {
            job.result.resize(result);
        } else {
            job.result.clear();
        }

        // Результат в очередь результатов
        {
            std::lock_guard lock(queue_mutex_);
            result_queue_.push(std::move(job));
        }
    }

    flecs::world& world_;
    std::vector<std::thread> workers_;
    std::atomic<bool> running_{true};

    std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::queue<CompressionJob> job_queue_;
    std::queue<CompressionJob> result_queue_;
};

// ECS система для обработки результатов
struct CompressionResultSystem {
    static void run(flecs::world& world, ParallelChunkCompressor& compressor) {
        compressor.process_queue();
    }
};
```

### Интеграция с Job System черезFlecs

```cpp
#include <flecs.h>
#include <zstd.h>

// Компонент для асинхронного сжатия
struct AsyncCompressRequest {
    std::vector<uint8_t> source_data;
    int compression_level = 5;
    bool completed = false;
    std::vector<uint8_t> result;
};

// ECS система сжатия
struct AsyncCompressSystem {
    static void run(flecs::world& world) {
        // Создаём задачи для каждого запроса
        world.each([&](flecs::entity e, AsyncCompressRequest& req) {
            if (req.completed) return;

            const size_t bound = ZSTD_compressBound(req.source_data.size());
            req.result.resize(bound);

            const size_t result = ZSTD_compress(
                req.result.data(), bound,
                req.source_data.data(), req.source_data.size(),
                req.compression_level
            );

            if (!ZSTD_isError(result)) {
                req.result.resize(result);
                req.completed = true;

                // Удаляем исходные данные
                std::vector<uint8_t>().swap(req.source_data);
            }
        });
    }
};
```

---

## SPSC Queue для async сжатия

### Lock-free очередь

```cpp
#include <atomic>
#include <array>
#include <span>
#include <cstddef>

// Single Producer Single Consumer queue для сжатия
template<typename T, size_t N>
class SPSCQueue {
public:
    static_assert((N & (N - 1)) == 0, "Size must be power of 2");

    bool push(T&& item) {
        const size_t write = write_idx_.load(std::memory_order_relaxed);
        const size_t next = (write + 1) & (N - 1);

        if (next == read_idx_.load(std::memory_order_acquire)) {
            return false; // Full
        }

        buffer_[write] = std::move(item);
        write_idx_.store(next, std::memory_order_release);

        return true;
    }

    bool pop(T& item) {
        const size_t read = read_idx_.load(std::memory_order_relaxed);

        if (read == write_idx_.load(std::memory_order_acquire)) {
            return false; // Empty
        }

        item = std::move(buffer_[read]);
        read_idx_.store((read + 1) & (N - 1), std::memory_order_release);

        return true;
    }

    bool empty() const {
        return read_idx_.load(std::memory_order_acquire) ==
               write_idx_.load(std::memory_order_acquire);
    }

    size_t capacity() const { return N; }

private:
    alignas(64) std::array<T, N> buffer_;
    std::atomic<size_t> write_idx_{0};
    std::atomic<size_t> read_idx_{0};
};

// Использование с Zstd
class AsyncZstdCompressor {
public:
    struct CompressTask {
        uint64_t id;
        std::vector<uint8_t> data;
        int level;
    };

    AsyncZstdCompressor() {
        worker_ = std::thread([this]() { worker_loop(); });
    }

    ~AsyncZstdCompressor() {
        running_ = false;
        if (worker_.joinable()) worker_.join();
    }

    // Асинхронное сжатие
    uint64_t compress_async(std::vector<uint8_t> data, int level = 5) {
        const uint64_t id = next_id_++;

        task_queue_.push(CompressTask{id, std::move(data), level});

        return id;
    }

    // Попытка получить результат
    bool try_pop_result(std::vector<uint8_t>& result) {
        return result_queue_.pop(result);
    }

private:
    void worker_loop() {
        CompressTask task;

        while (running_) {
            if (task_queue_.pop(task)) {
                const size_t bound = ZSTD_compressBound(task.data.size());
                std::vector<uint8_t> compressed(bound);

                const size_t result = ZSTD_compress(
                    compressed.data(), bound,
                    task.data.data(), task.data.size(),
                    task.level
                );

                if (!ZSTD_isError(result)) {
                    compressed.resize(result);
                    result_queue_.push(std::move(compressed));
                }
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }

    std::thread worker_;
    std::atomic<bool> running_{true};
    std::atomic<uint64_t> next_id_{0};

    SPSCQueue<CompressTask, 256> task_queue_;
    SPSCQueue<std::vector<uint8_t>, 256> result_queue_;
};
```

---

## SIMD и аппаратное ускорение

### Параллельное сжатие блоков

```cpp
#include <zstd.h>
#include <immintrin.h>
#include <vector>
#include <span>

// Параллельное сжатие нескольких блоков через SIMD
class SIMDBlockCompressor {
public:
    // Сжатие 4 блоков параллельно (демонстрация концепции)
    std::array<std::vector<uint8_t>, 4> compress_parallel(
        std::array<std::span<const uint8_t>, 4> inputs,
        int level = 5
    ) {
        std::array<std::vector<uint8_t>, 4> outputs;

        // Параметры для каждого блока
        std::array<ZSTD_CCtx*, 4> contexts;
        std::array<size_t, 4> bounds;

        // Инициализация контекстов
        for (int i = 0; i < 4; ++i) {
            contexts[i] = ZSTD_createCCtx();
            ZSTD_CCtx_setParameter(contexts[i], ZSTD_c_compressionLevel, level);
            bounds[i] = ZSTD_compressBound(inputs[i].size());
            outputs[i].resize(bounds[i]);
        }

        // Параллельное сжатие (в реальном коде - через OpenMP или аналоги)
        #pragma omp parallel for
        for (int i = 0; i < 4; ++i) {
            const size_t result = ZSTD_compress2(
                contexts[i],
                outputs[i].data(), bounds[i],
                inputs[i].data(), inputs[i].size()
            );

            if (!ZSTD_isError(result)) {
                outputs[i].resize(result);
            }
        }

        // Очистка
        for (int i = 0; i < 4; ++i) {
            ZSTD_freeCCtx(contexts[i]);
        }

        return outputs;
    }
};

// AVX-оптимизированная проверка данных
class SIMDDataValidator {
public:
    // Быстрая проверка на нули через AVX
    static bool contains_nonzero(std::span<const uint8_t> data) {
        const size_t simd_size = 32; // AVX 256-bit
        const size_t limit = data.size() - (data.size() % simd_size);

        for (size_t i = 0; i < limit; i += simd_size) {
            __m256i chunk = _mm256_loadu_si256(
                reinterpret_cast<const __m256i*>(data.data() + i)
            );

            // Проверка: если все нули - продолжаем
            const int mask = _mm256_movemask_epi8(chunk);
            if (mask != 0) return true;
        }

        // Обработка остатка
        for (size_t i = limit; i < data.size(); ++i) {
            if (data[i] != 0) return true;
        }

        return false;
    }
};
```

---

## Алерты и best practices

### Правило 1: Переиспользование контекстов

```cpp
// ПЛОХО: создание контекста на каждый вызов
void bad_compress() {
    ZSTD_CCtx* cctx = ZSTD_createCCtx();
    ZSTD_compress(cctx, ...);
    ZSTD_freeCCtx(cctx);
}

// ХОРОШО: переиспользование контекста
class Compressor {
    ZSTD_CCtx* cctx_ = ZSTD_createCCtx();
public:
    ~Compressor() { ZSTD_freeCCtx(cctx_); }

    size_t compress(...) {
        return ZSTD_compress2(cctx_, ...);
    }

    void reset() {
        ZSTD_CCtx_reset(cctx_, ZSTD_reset_session_only);
    }
};
```

### Правило 2: Предварительное выделение

```cpp
// ПЛОХО: reallocation каждый раз
void bad() {
    std::vector<uint8_t> buffer;
    for (auto& chunk : chunks) {
        buffer.resize(ZSTD_compressBound(chunk.size()));
        ZSTD_compress(buffer.data(), ...);
    }
}

// ХОРОШО: единое выделение
void good() {
    const size_t max_size = /* max compressBound */;
    std::vector<uint8_t> buffer(max_size);

    for (auto& chunk : chunks) {
        const size_t result = ZSTD_compress(buffer.data(), max_size, ...);
        // Используем buffer.data() и result
    }
}
```

### Правило 3: Dictionary кэширование

```cpp
// ПЛОХО: создание словаря каждый раз
void bad() {
    for (auto& data : dataset) {
        ZSTD_CDict* dict = ZSTD_createCDict(dict_data, ...);
        ZSTD_compress_usingCDict(ctx, ..., dict);
        ZSTD_freeCDict(dict); // Утечка в случае ошибки
    }
}

// ХОРОШО: переиспользование словаря
class DictCompressor {
    ZSTD_CDict* cdict_ = nullptr;
    ZSTD_DDict* ddict_ = nullptr;

public:
    void set_dict(std::span<const uint8_t> dict, int level) {
        cdict_ = ZSTD_createCDict(dict.data(), dict.size(), level);
        ddict_ = ZSTD_createDDict(dict.data(), dict.size());
    }

    ~Compressor() {
        if (cdict_) ZSTD_freeCDict(cdict_);
        if (ddict_) ZSTD_freeDDict(ddict_);
    }
};
```

### Правило 4: Streaming вместо huge allocation

```cpp
// ПЛОХО: огромный буфер
void bad() {
    size_t huge = 100 * 1024 * 1024; // 100 MB
    std::vector<uint8_t> buffer(huge);
    ZSTD_compress(buffer.data(), huge, src, src_size, 5);
}

// ХОРОШО: streaming
void good() {
    ZSTD_CStream* cstream = ZSTD_createCStream();
    ZSTD_initCStream(cstream, 5);

    std::vector<uint8_t> chunk_buffer(16 * 1024); // 16 KB чанки
    std::vector<uint8_t> output(16 * 1024);

    // Читаем и сжимаем чанками
    while (has_more_data()) {
        auto input = read_chunk();

        ZSTD_inBuffer in{input.data(), input.size(), 0};
        ZSTD_outBuffer out{output.data(), output.size(), 0};

        ZSTD_compressStream2(cstream, &out, &in, ZSTD_e_continue);
        // Записываем output.data() [0..out.pos)
    }

    // Финализация
    ZSTD_inBuffer end{nullptr, 0, 0};
    ZSTD_outBuffer final{output.data(), output.size(), 0};
    ZSTD_compressStream2(cstream, &final, &end, ZSTD_e_end);

    ZSTD_freeCStream(cstream);
}
```

---

## Производительность: числа

### Типичные результаты

| Операция     | Размер данных | Уровень | Время  | Скорость  |
|--------------|---------------|---------|--------|-----------|
| Сжатие       | 1 MB          | 3       | ~3ms   | 333 MB/s  |
| Сжатие       | 1 MB          | 10      | ~12ms  | 83 MB/s   |
| Сжатие       | 100 MB        | 3       | ~200ms | 500 MB/s  |
| Сжатие       | 100 MB        | 10      | ~1.5s  | 67 MB/s   |
| Декомпрессия | 1 MB          | -       | ~0.5ms | 2000 MB/s |
| Декомпрессия | 100 MB        | -       | ~50ms  | 2000 MB/s |

### Факторы производительности

1. **Размер данных**: маленькие данные - накладные расходы на контекст
2. **Уровень**: 1-3 - аппаратное ускорение, 19+ - оптимальный парсер
3. **Dictionary**: 10-50% улучшение сжатия
4. **LDM**: значительное улучшение для периодических данных
5. **Потоки**: оверхед на синхронизацию, эффективно для >10 MB

---

## Интеграция с Tracy

```cpp
#include <tracy/Tracy.hpp>
#include <zstd.h>

class ProfiledCompressor {
public:
    std::expected<std::vector<uint8_t>, std::string> compress(
        std::span<const uint8_t> src,
        int level = 5
    ) {
        ZoneScopedN("ZstdCompress");

        const size_t bound = ZSTD_compressBound(src.size());
        std::vector<uint8_t> dst(bound);

        const auto start = std::chrono::high_resolution_clock::now();

        const size_t result = ZSTD_compress(
            dst.data(), bound,
            src.data(), src.size(),
            level
        );

        const auto end = std::chrono::high_resolution_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        // Логирование
        TracyPlot("Compression/Input (KB)", static_cast<double>(src.size()) / 1024.0);
        TracyPlot("Compression/Output (KB)", static_cast<double>(result) / 1024.0);
        TracyPlot("Compression/Time (us)", static_cast<double>(ms.count()));

        if (src.size() > 0) {
            const double speed_mbs = (src.size() / (1024.0 * 1024.0)) / (ms.count() / 1'000'000.0);
            TracyPlot("Compression/Speed (MB/s)", speed_mbs);
        }

        if (ZSTD_isError(result)) {
            return std::unexpected(ZSTD_getErrorName(result));
        }

        dst.resize(result);
        return dst;
    }
};
```

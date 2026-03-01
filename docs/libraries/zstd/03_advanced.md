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
    // Плоский массив данных вместо вектора векторов
    std::vector<glm::ivec3> coords;
    std::vector<uint8_t> datas;                    // Плоский массив всех данных
    std::vector<size_t> data_offsets;              // Смещения для каждого чанка
    std::vector<size_t> data_sizes;                // Размеры каждого чанка
    std::vector<size_t> original_sizes;
    std::vector<int> compression_levels;
    std::vector<size_t> compressed_sizes;
    
    // Метод для доступа к данным чанка
    std::span<const uint8_t> get_chunk_data(size_t chunk_index) const {
        if (chunk_index >= data_offsets.size()) return {};
        const size_t offset = data_offsets[chunk_index];
        const size_t size = data_sizes[chunk_index];
        if (offset + size > datas.size()) return {};
        return std::span<const uint8_t>(datas.data() + offset, size);
    }
    
    // Метод для добавления данных чанка
    void add_chunk(glm::ivec3 coord, std::span<const uint8_t> data, 
                   size_t original_size, int compression_level) {
        coords.push_back(coord);
        data_offsets.push_back(datas.size());
        data_sizes.push_back(data.size());
        datas.insert(datas.end(), data.begin(), data.end());
        original_sizes.push_back(original_size);
        compression_levels.push_back(compression_level);
        compressed_sizes.push_back(data.size());
    }
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
#include <stdexec/execution.hpp>

// Job для сжатия одного чанка
struct CompressionJob {
    uint64_t chunk_key;
    std::span<const uint8_t> data;
    int level;
    std::vector<uint8_t> result;
};

// Результат сжатия
struct CompressionResult {
    uint64_t chunk_key;
    size_t compressed_size;
};

// Параллельное сжатие через stdexec Job System
class ParallelChunkCompressor {
public:
    ParallelChunkCompressor(flecs::world& world, stdexec::scheduler auto scheduler)
        : world_(world), scheduler_(scheduler)
    {
        // Инициализация sender для обработки задач
        task_sender_ = stdexec::schedule(scheduler_)
                     | stdexec::then([this]() {
                           return process_tasks();
                       });
    }

    // Поставить задачу в очередь и вернуть sender для отслеживания
    stdexec::sender auto enqueue(CompressionJob job) {
        const uint64_t job_id = next_job_id_++;
        
        // Добавляем задачу в очередь
        {
            std::lock_guard lock(queue_mutex_);
            pending_jobs_.emplace(job_id, std::move(job));
        }

        // Создаем sender для этой конкретной задачи
        return stdexec::schedule(scheduler_)
             | stdexec::then([this, job_id]() -> std::expected<CompressionResult, std::string> {
                   return execute_single_job(job_id);
               });
    }

    // Массовое сжатие через stdexec::bulk
    stdexec::sender auto compress_bulk(std::span<const CompressionJob> jobs) {
        return stdexec::schedule(scheduler_)
             | stdexec::bulk(jobs.size(), [jobs](size_t idx) {
                   const auto& job = jobs[idx];
                   const size_t bound = ZSTD_compressBound(job.data.size());
                   std::vector<uint8_t> result(bound);
                   
                   const size_t compressed_size = ZSTD_compress(
                       result.data(), bound,
                       job.data.data(), job.data.size(),
                       job.level
                   );
                   
                   if (!ZSTD_isError(compressed_size)) {
                       result.resize(compressed_size);
                       return std::make_pair(job.chunk_key, std::move(result));
                   }
                   return std::make_pair(job.chunk_key, std::vector<uint8_t>{});
               });
    }

    // Обработка результатов в ECS системе
    void process_results() {
        std::vector<std::pair<uint64_t, std::vector<uint8_t>>> results;
        
        // Забираем все готовые результаты
        {
            std::lock_guard lock(result_mutex_);
            results.swap(completed_results_);
        }

        // Устанавливаем результаты в ECS
        for (auto& [chunk_key, data] : results) {
            if (!data.empty()) {
                world_.set<CompressedChunkData>(flecs::entity(chunk_key),
                    CompressedChunkData{
                        .data = std::move(data),
                        .original_size = 0  // Будет установлено позже
                    });
            }
        }
    }

private:
    std::expected<CompressionResult, std::string> execute_single_job(uint64_t job_id) {
        std::optional<CompressionJob> job;
        
        // Извлекаем задачу
        {
            std::lock_guard lock(queue_mutex_);
            auto it = pending_jobs_.find(job_id);
            if (it == pending_jobs_.end()) {
                return std::unexpected("Job not found");
            }
            job = std::move(it->second);
            pending_jobs_.erase(it);
        }

        if (!job) {
            return std::unexpected("Invalid job");
        }

        const size_t bound = ZSTD_compressBound(job->data.size());
        std::vector<uint8_t> result(bound);

        const size_t compressed_size = ZSTD_compress(
            result.data(), bound,
            job->data.data(), job->data.size(),
            job->level
        );

        if (ZSTD_isError(compressed_size)) {
            return std::unexpected(ZSTD_getErrorName(compressed_size));
        }

        result.resize(compressed_size);
        
        // Сохраняем результат
        {
            std::lock_guard lock(result_mutex_);
            completed_results_.emplace_back(job->chunk_key, std::move(result));
        }

        return CompressionResult{job->chunk_key, compressed_size};
    }

    stdexec::sender auto process_tasks() {
        return stdexec::schedule(scheduler_)
             | stdexec::then([this]() {
                   // Периодическая обработка задач
                   process_results();
                   return true;
               });
    }

    flecs::world& world_;
    stdexec::scheduler auto scheduler_;
    stdexec::sender auto task_sender_;

    std::atomic<uint64_t> next_job_id_{0};
    
    std::mutex queue_mutex_;
    std::unordered_map<uint64_t, CompressionJob> pending_jobs_;
    
    std::mutex result_mutex_;
    std::vector<std::pair<uint64_t, std::vector<uint8_t>>> completed_results_;
};

// ECS система для обработки результатов
struct CompressionResultSystem {
    static void run(flecs::world& world, ParallelChunkCompressor& compressor) {
        compressor.process_results();
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

// Использование с Zstd через stdexec Job System
class AsyncZstdCompressor {
public:
    struct CompressTask {
        uint64_t id;
        std::vector<uint8_t> data;
        int level;
    };

    struct CompressResult {
        uint64_t id;
        std::vector<uint8_t> compressed_data;
        size_t original_size;
        size_t compressed_size;
    };

    AsyncZstdCompressor(stdexec::scheduler auto scheduler)
        : scheduler_(scheduler)
    {
        // Инициализация обработчика задач через stdexec
        task_processor_ = stdexec::schedule(scheduler_)
                        | stdexec::then([this]() {
                              return process_tasks();
                          });
    }

    ~AsyncZstdCompressor() {
        // stdexec автоматически управляет жизненным циклом задач
        // Нет необходимости вручную останавливать потоки
    }

    // Асинхронное сжатие через stdexec sender
    stdexec::sender auto compress_async(CompressTask task) {
        const uint64_t task_id = next_task_id_++;
        
        // Добавляем задачу в очередь
        {
            std::lock_guard lock(queue_mutex_);
            pending_tasks_.emplace(task_id, std::move(task));
        }

        // Возвращаем sender для отслеживания выполнения
        return stdexec::schedule(scheduler_)
             | stdexec::then([this, task_id]() -> std::expected<CompressResult, std::string> {
                   return execute_compression_task(task_id);
               });
    }

    // Массовое асинхронное сжатие через stdexec::bulk
    stdexec::sender auto compress_bulk_async(std::span<const CompressTask> tasks) {
        return stdexec::schedule(scheduler_)
             | stdexec::bulk(tasks.size(), [tasks](size_t idx) {
                   const auto& task = tasks[idx];
                   const size_t bound = ZSTD_compressBound(task.data.size());
                   std::vector<uint8_t> result(bound);
                   
                   const size_t compressed_size = ZSTD_compress(
                       result.data(), bound,
                       task.data.data(), task.data.size(),
                       task.level
                   );
                   
                   if (!ZSTD_isError(compressed_size)) {
                       result.resize(compressed_size);
                       return CompressResult{
                           .id = task.id,
                           .compressed_data = std::move(result),
                           .original_size = task.data.size(),
                           .compressed_size = compressed_size
                       };
                   }
                   return CompressResult{
                       .id = task.id,
                       .compressed_data = {},
                       .original_size = task.data.size(),
                       .compressed_size = 0
                   };
               });
    }

    // Получение готовых результатов
    std::vector<CompressResult> get_completed_results() {
        std::vector<CompressResult> results;
        {
            std::lock_guard lock(result_mutex_);
            results.swap(completed_results_);
        }
        return results;
    }

private:
    std::expected<CompressResult, std::string> execute_compression_task(uint64_t task_id) {
        std::optional<CompressTask> task;
        
        // Извлекаем задачу из очереди
        {
            std::lock_guard lock(queue_mutex_);
            auto it = pending_tasks_.find(task_id);
            if (it == pending_tasks_.end()) {
                return std::unexpected("Task not found");
            }
            task = std::move(it->second);
            pending_tasks_.erase(it);
        }

        if (!task) {
            return std::unexpected("Invalid task");
        }

        const size_t bound = ZSTD_compressBound(task->data.size());
        std::vector<uint8_t> compressed_data(bound);

        const size_t compressed_size = ZSTD_compress(
            compressed_data.data(), bound,
            task->data.data(), task->data.size(),
            task->level
        );

        if (ZSTD_isError(compressed_size)) {
            return std::unexpected(ZSTD_getErrorName(compressed_size));
        }

        compressed_data.resize(compressed_size);

        CompressResult result{
            .id = task->id,
            .compressed_data = std::move(compressed_data),
            .original_size = task->data.size(),
            .compressed_size = compressed_size
        };

        // Сохраняем результат
        {
            std::lock_guard lock(result_mutex_);
            completed_results_.push_back(result);
        }

        return result;
    }

    stdexec::sender auto process_tasks() {
        return stdexec::schedule(scheduler_)
             | stdexec::then([this]() {
                   // Периодическая обработка задач (например, очистка старых результатов)
                   cleanup_old_results();
                   return true;
               });
    }

    void cleanup_old_results() {
        // Очистка старых результатов, если нужно
        // В реальной реализации можно добавить TTL для результатов
    }

    stdexec::scheduler auto scheduler_;
    stdexec::sender auto task_processor_;

    std::atomic<uint64_t> next_task_id_{0};
    
    std::mutex queue_mutex_;
    std::unordered_map<uint64_t, CompressTask> pending_tasks_;
    
    std::mutex result_mutex_;
    std::vector<CompressResult> completed_results_;
};

// Пример использования с ProjectV Job System
class ProjectVZstdCompressor {
public:
    ProjectVZstdCompressor(projectv::core::jobs::ThreadPool& thread_pool)
        : compressor_(thread_pool.get_scheduler())
    {}

    // Интеграция с ECS системой
    stdexec::sender auto compress_for_entity(flecs::entity entity, std::vector<uint8_t> data) {
        AsyncZstdCompressor::CompressTask task{
            .id = static_cast<uint64_t>(entity.id()),
            .data = std::move(data),
            .level = 5
        };

        return compressor_.compress_async(std::move(task))
             | stdexec::then([entity](std::expected<AsyncZstdCompressor::CompressResult, std::string> result) {
                   if (result) {
                       // Устанавливаем результат в ECS компонент
                       entity.set<CompressedDataComponent>(CompressedDataComponent{
                           .data = std::move(result->compressed_data),
                           .original_size = result->original_size,
                           .compression_ratio = static_cast<float>(result->compressed_size) / result->original_size
                       });
                   }
                   return result.has_value();
               });
    }

private:
    AsyncZstdCompressor compressor_;
};

// SPSC очередь для интеграции с stdexec
template<typename T, size_t N>
class StdexecSPSCQueue {
public:
    static_assert((N & (N - 1)) == 0, "Size must be power of 2");

    // Push с возвратом sender для отслеживания
    stdexec::sender auto push_async(T&& item) {
        return stdexec::schedule(stdexec::inline_scheduler{})
             | stdexec::then([this, item = std::move(item)]() mutable -> bool {
                   return push(std::move(item));
               });
    }

    // Pop с возвратом sender
    stdexec::sender auto pop_async() {
        return stdexec::schedule(stdexec::inline_scheduler{})
             | stdexec::then([this]() -> std::optional<T> {
                   T item;
                   if (pop(item)) {
                       return item;
                   }
                   return std::nullopt;
               });
    }

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

// Пример использования SPSC очереди с stdexec
class AsyncCompressionPipeline {
public:
    AsyncCompressionPipeline(stdexec::scheduler auto scheduler)
        : scheduler_(scheduler), input_queue_(), output_queue_()
    {
        // Запускаем pipeline обработки
        pipeline_ = stdexec::schedule(scheduler_)
                  | stdexec::then([this]() {
                        return run_pipeline();
                    });
    }

    stdexec::sender auto submit_compression_task(AsyncZstdCompressor::CompressTask task) {
        return input_queue_.push_async(std::move(task))
             | stdexec::then([this](bool pushed) {
                   if (pushed) {
                       // Запускаем обработку, если очередь не пуста
                       return stdexec::schedule(scheduler_)
                            | stdexec::then([this]() { return process_next_task(); });
                   }
                   return stdexec::just(false);
               });
    }

private:
    stdexec::sender auto run_pipeline() {
        return stdexec::schedule(scheduler_)
             | stdexec::then([this]() {
                   // Непрерывная обработка задач из очереди
                   while (!input_queue_.empty()) {
                       process_next_task();
                   }
                   return true;
               });
    }

    stdexec::sender auto process_next_task() {
        return input_queue_.pop_async()
             | stdexec::then([this](std::optional<AsyncZstdCompressor::CompressTask> task) {
                   if (!task) return stdexec::just(false);
                   
                   // Выполняем сжатие
                   const size_t bound = ZSTD_compressBound(task->data.size());
                   std::vector<uint8_t> result(bound);
                   
                   const size_t compressed_size = ZSTD_compress(
                       result.data(), bound,
                       task->data.data(), task->data.size(),
                       task->level
                   );
                   
                   if (!ZSTD_isError(compressed_size)) {
                       result.resize(compressed_size);
                       
                       AsyncZstdCompressor::CompressResult comp_result{
                           .id = task->id,
                           .compressed_data = std::move(result),
                           .original_size = task->data.size(),
                           .compressed_size = compressed_size
                       };
                       
                       output_queue_.push(std::move(comp_result));
                   }
                   
                   return stdexec::just(true);
               });
    }

    stdexec::scheduler auto scheduler_;
    stdexec::sender auto pipeline_;
    
    StdexecSPSCQueue<AsyncZstdCompressor::CompressTask, 1024> input_queue_;
    StdexecSPSCQueue<AsyncZstdCompressor::CompressResult, 1024> output_queue_;
};

// Заключение: stdexec обеспечивает более чистую и производительную архитектуру
// по сравнению с ручным управлением потоками через std::thread

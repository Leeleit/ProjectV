# Многопоточность: Flecs + stdexec Контракты

Формализация многопоточности в ProjectV с четкими контрактами владения потоками.

---

## Обзор

ProjectV использует **Flecs** для логических потоков ECS и **stdexec (M:N Job System)** для тяжелых задач:
- **Mesh Generation** — генерация мешей воксельных чанков
- **CA Updates** — клеточные автоматы и физика
- **Asset Loading** — асинхронная загрузка ресурсов
- **Physics Simulation** — симуляция физики JoltPhysics

---

## 1. Контракты Владения Потоками

### 1.1 Типы потоков

```cpp
// ProjectV.Threading.Types.cppm
export module ProjectV.Threading.Types;

import std;
import std.execution;

export namespace projectv::threading {

/// Типы потоков в системе.
export enum class ThreadType : uint8_t {
    Main = 0,           ///< Главный поток (UI, Input)
    Render = 1,         ///< Рендер поток (Vulkan)
    Logic = 2,          ///< Логический поток (Flecs ECS)
    Worker = 3,         ///< Worker поток (stdexec пул)
    IO = 4,             ///< IO поток (файловая система)
    Physics = 5,        ///< Physics поток (JoltPhysics)
    Count = 6
};

/// Контракт владения ресурсами.
export struct ThreadOwnership {
    ThreadType owner{ThreadType::Main};
    std::atomic<bool> locked{false};
    uint64_t lock_timestamp{0};

    /// Проверка, может ли поток получить доступ.
    [[nodiscard]] auto can_access(ThreadType requester) const noexcept -> bool {
        // Главный поток имеет доступ ко всему
        if (requester == ThreadType::Main) return true;

        // Владелец имеет доступ
        if (requester == owner) return true;

        // Worker потоки могут читать, но не писать
        if (requester == ThreadType::Worker && !locked) return true;

        return false;
    }

    /// Блокировка ресурса.
    auto lock(ThreadType locker) noexcept -> bool {
        if (!can_access(locker)) return false;

        bool expected = false;
        if (locked.compare_exchange_strong(expected, true)) {
            owner = locker;
            lock_timestamp = get_timestamp();
            return true;
        }
        return false;
    }

    /// Разблокировка ресурса.
    auto unlock(ThreadType unlocker) noexcept -> bool {
        if (owner != unlocker) return false;

        locked.store(false);
        return true;
    }

private:
    static auto get_timestamp() noexcept -> uint64_t {
        return std::chrono::steady_clock::now().time_since_epoch().count();
    }
};

} // namespace projectv::threading
```

### 1.2 Контракты доступа к данным

```cpp
// ProjectV.Threading.Contracts.cppm
export module ProjectV.Threading.Contracts;

import std;
import ProjectV.Threading.Types;

export namespace projectv::threading {

/// Контракт для воксельных чанков.
export struct ChunkAccessContract {
    ThreadOwnership ownership;
    std::atomic<uint32_t> reader_count{0};
    std::atomic<uint32_t> writer_count{0};

    /// Чтение чанка.
    [[nodiscard]] auto begin_read(ThreadType reader) noexcept -> bool {
        if (!ownership.can_access(reader)) return false;

        // Инкрементируем счетчик читателей
        reader_count.fetch_add(1, std::memory_order_acquire);
        return true;
    }

    /// Завершение чтения.
    auto end_read() noexcept -> void {
        reader_count.fetch_sub(1, std::memory_order_release);
    }

    /// Запись в чанк.
    [[nodiscard]] auto begin_write(ThreadType writer) noexcept -> bool {
        if (!ownership.lock(writer)) return false;

        // Ждем, пока все читатели закончат
        while (reader_count.load(std::memory_order_acquire) > 0) {
            std::this_thread::yield();
        }

        writer_count.store(1, std::memory_order_release);
        return true;
    }

    /// Завершение записи.
    auto end_write(ThreadType writer) noexcept -> bool {
        if (writer_count.load(std::memory_order_acquire) != 1) return false;

        writer_count.store(0, std::memory_order_release);
        return ownership.unlock(writer);
    }
};

/// Контракт для ECS компонентов.
export struct ComponentAccessContract {
    ThreadOwnership ownership;
    std::atomic<bool> modifying{false};

    /// Получение доступа на чтение.
    [[nodiscard]] auto get_read_access(ThreadType reader) const noexcept -> bool {
        return ownership.can_access(reader) && !modifying.load(std::memory_order_acquire);
    }

    /// Получение доступа на запись.
    [[nodiscard]] auto get_write_access(ThreadType writer) noexcept -> bool {
        if (!ownership.lock(writer)) return false;

        bool expected = false;
        if (modifying.compare_exchange_strong(expected, true)) {
            return true;
        }

        ownership.unlock(writer);
        return false;
    }

    /// Освобождение доступа на запись.
    auto release_write_access(ThreadType writer) noexcept -> bool {
        if (!modifying.load(std::memory_order_acquire)) return false;

        modifying.store(false, std::memory_order_release);
        return ownership.unlock(writer);
    }
};

} // namespace projectv::threading
```

---

## 2. Flecs + stdexec Интеграция

### 2.1 Job System на stdexec

```cpp
// ProjectV.Threading.JobSystem.cppm
export module ProjectV.Threading.JobSystem;

import std;
import std.execution;
import ProjectV.Threading.Contracts;

export namespace projectv::threading {

/// Типы задач.
export enum class JobType : uint8_t {
    MeshGeneration = 0,
    CAUpdate = 1,
    Physics = 2,
    AssetLoad = 3,
    Serialization = 4,
    Count = 5
};

/// Приоритеты задач.
export enum class JobPriority : uint8_t {
    Critical = 0,   ///< Критические задачи (физика, ввод)
    High = 1,       ///< Высокий приоритет (рендер)
    Normal = 2,     ///< Нормальный приоритет (логика)
    Low = 3,        ///< Низкий приоритет (фоновые задачи)
    Background = 4  ///< Фоновые задачи (сохранения)
};

/// Задача для выполнения.
export struct Job {
    JobType type{JobType::MeshGeneration};
    JobPriority priority{JobPriority::Normal};
    std::function<void()> task;
    std::atomic<bool> completed{false};
    std::atomic<bool> cancelled{false};

    /// Выполнение задачи.
    auto execute() noexcept -> void {
        if (cancelled.load()) return;

        try {
            task();
            completed.store(true);
        } catch (...) {
            // Логирование ошибки
            completed.store(true);
        }
    }
};

/// Job System на stdexec.
export class JobSystem {
public:
    explicit JobSystem(size_t worker_count = std::thread::hardware_concurrency())
        : scheduler_(std::execution::make_scheduler())
        , stop_token_(std::stop_source{}.get_token())
    {
        workers_.reserve(worker_count);
        for (size_t i = 0; i < worker_count; ++i) {
            workers_.emplace_back([this, i] { worker_thread(i); });
        }
    }

    ~JobSystem() {
        stop_token_.request_stop();
        for (auto& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
    }

    /// Добавление задачи.
    auto submit(Job job) -> std::future<void> {
        std::promise<void> promise;
        auto future = promise.get_future();

        job.task = [job = std::move(job), promise = std::move(promise)]() mutable {
            job.execute();
            promise.set_value();
        };

        {
            std::lock_guard lock(queue_mutex_);
            job_queue_.push(std::move(job));
        }

        queue_cv_.notify_one();
        return future;
    }

    /// Добавление задачи с приоритетом.
    auto submit_priority(Job job, JobPriority priority) -> std::future<void> {
        job.priority = priority;
        return submit(std::move(job));
    }

    /// Ожидание завершения всех задач.
    auto wait_all() -> void {
        std::unique_lock lock(queue_mutex_);
        completion_cv_.wait(lock, [this] {
            return job_queue_.empty() && active_jobs_ == 0;
        });
    }

    /// Получение статистики.
    auto get_stats() const -> struct {
        size_t pending_jobs;
        size_t active_jobs;
        size_t completed_jobs;
        size_t worker_count;
    } {
        std::lock_guard lock(queue_mutex_);
        return {
            .pending_jobs = job_queue_.size(),
            .active_jobs = active_jobs_,
            .completed_jobs = completed_jobs_,
            .worker_count = workers_.size()
        };
    }

private:
    std::vector<std::thread> workers_;
    std::priority_queue<Job> job_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::condition_variable completion_cv_;
    std::stop_token stop_token_;
    std::execution::scheduler scheduler_;

    std::atomic<size_t> active_jobs_{0};
    std::atomic<size_t> completed_jobs_{0};

    /// Worker thread.
    auto worker_thread(size_t worker_id) -> void {
        while (!stop_token_.stop_requested()) {
            std::unique_lock lock(queue_mutex_);

            queue_cv_.wait(lock, [this] {
                return !job_queue_.empty() || stop_token_.stop_requested();
            });

            if (stop_token_.stop_requested()) break;

            auto job = std::move(job_queue_.top());
            job_queue_.pop();

            ++active_jobs_;
            lock.unlock();

            // Выполнение задачи
            job.execute();

            --active_jobs_;
            ++completed_jobs_;

            completion_cv_.notify_all();
        }
    }
};

} // namespace projectv::threading
```

### 2.2 Flecs Multi-threading

```cpp
// ProjectV.ECS.Multithreading.cppm
export module ProjectV.ECS.Multithreading;

import std;
import flecs;
import ProjectV.Threading.JobSystem;
import ProjectV.Threading.Contracts;

export namespace projectv::ecs {

/// Multi-threaded система Flecs.
export class MultithreadedSystem {
public:
    explicit MultithreadedSystem(flecs::world& world, threading::JobSystem& job_system)
        : world_(world)
        , job_system_(job_system)
    {}

    /// Параллельное выполнение системы.
    template<typename Func>
    auto parallel_for_each(Func&& func) -> void {
        // Разделение сущностей на чанки
        auto query = world_.query_builder().build();
        auto entities = query.iter();

        const size_t chunk_size = 64;
        const size_t total_entities = entities.count();
        const size_t num_chunks = (total_entities + chunk_size - 1) / chunk_size;

        std::vector<std::future<void>> futures;
        futures.reserve(num_chunks);

        for (size_t chunk_idx = 0; chunk_idx < num_chunks; ++chunk_idx) {
            const size_t start = chunk_idx * chunk_size;
            const size_t end = std::min(start + chunk_size, total_entities);

            threading::Job job{
                .type = threading::JobType::CAUpdate,
                .priority = threading::JobPriority::Normal,
                .task = [this, start, end, &func] {
                    auto chunk_query = world_.query_builder().build();
                    auto chunk_iter = chunk_query.iter();

                    size_t idx = 0;
                    while (chunk_iter.next()) {
                        for (size_t i = 0; i < chunk_iter.count(); ++i) {
                            if (idx >= start && idx < end) {
                                func(chunk_iter.entity(i), chunk_iter);
                            }
                            ++idx;
                            if (idx >= end) break;
                        }
                        if (idx >= end) break;
                    }
                }
            };

            futures.push_back(job_system_.submit(std::move(job)));
        }

        // Ожидание завершения всех задач
        for (auto& future : futures) {
            future.wait();
        }
    }

    /// Параллельная обработка компонентов с контрактами.
    template<typename T, typename Func>
    auto parallel_process(Func&& func) -> void {
        // Получение доступа на чтение
        threading::ComponentAccessContract contract;
        if (!contract.get_read_access(threading::ThreadType::Worker)) {
            throw std::runtime_error("Failed to get read access for component");
        }

        try {
            parallel_for_each([&func](flecs::entity e, flecs::iter& it) {
                if (auto* comp = it.get<T>()) {
                    func(e, *comp);
                }
            });
        } catch (...) {
            // Гарантируем освобождение контракта
            throw;
        }
    }

private:
    flecs::world& world_;
    threading::JobSystem& job_system_;
};

} // namespace projectv::ecs
```

---

## 3. Контракты для Специфических Задач

### 3.1 Mesh Generation

```cpp
// ProjectV.Voxel.MeshGeneration.cppm
export module ProjectV.Voxel.MeshGeneration;

import std;
import glm;
import ProjectV.Threading.JobSystem;
import ProjectV.Threading.Contracts;

export namespace projectv::voxel {

/// Контракт для генерации мешей.
export struct MeshGenerationContract {
    threading::ChunkAccessContract chunk_access;
    std::atomic<bool> generating{false};
    std::atomic<uint32_t> generated_meshes{0};

    /// Начало генерации меша.
    [[nodiscard]] auto begin_generation(threading::ThreadType generator) noexcept -> bool {
        if (!chunk_access.begin_write(generator)) return false;

        bool expected = false;
        return generating.compare_exchange_strong(expected, true);
    }

    /// Завершение генерации меша.
    auto end_generation(threading::ThreadType generator) noexcept -> bool {
        if (!generating.load()) return false;

        generating.store(false);
        generated_meshes.fetch_add(1);
        return chunk_access.end_write(generator);
    }

    /// Получение статистики.
    [[nodiscard]] auto get_stats() const noexcept -> struct {
        bool is_generating;
        uint32_t generated_count;
        bool is_locked;
    } {
        return {
            .is_generating = generating.load(),
            .generated_count = generated_meshes.load(),
            .is_locked = chunk_access.ownership.locked.load()
        };
    }
};

/// Job для генерации мешей.
export class MeshGenerationJob {
public:
    explicit MeshGenerationJob(
        threading::JobSystem& job_system,
        MeshGenerationContract& contract
    ) : job_system_(job_system), contract_(contract) {}

    /// Запуск генерации меша.
    auto generate_mesh(Chunk& chunk) -> std::future<void> {
        threading::Job job{
            .type = threading::JobType::MeshGeneration,
            .priority = threading::JobPriority::High,
            .task = [this, &chunk] {
                // Проверка контракта
                if (!contract_.begin_generation(threading::ThreadType::Worker)) {
                    throw std::runtime_error("Failed to acquire generation contract");
                }

                try {
                    // Генерация меша
                    generate_mesh_impl(chunk);

                    // Завершение генерации
                    if (!contract_.end_generation(threading::ThreadType::Worker)) {
                        throw std::runtime_error("Failed to release generation contract");
                    }
                } catch (...) {
                    // Гарантируем освобождение контракта
                    contract_.end_generation(threading::ThreadType::Worker);
                    throw;
                }
            }
        };

        return job_system_.submit(std::move(job));
    }

private:
    threading::JobSystem& job_system_;
    MeshGenerationContract& contract_;

    /// Реализация генерации меша.
    auto generate_mesh_impl(Chunk& chunk) -> void {
        // Реализация генерации меша воксельного чанка
        // ...
    }
};

} // namespace projectv::voxel
```

### 3.2 Physics Simulation

```cpp
// ProjectV.Physics.Multithreading.cppm
export module ProjectV.Physics.Multithreading;

import std;
import JoltPhysics;
import ProjectV.Threading.JobSystem;
import ProjectV.Threading.Contracts;

export namespace projectv::physics {

/// Контракт для

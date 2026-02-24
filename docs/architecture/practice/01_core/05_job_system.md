# Спецификация Job System на базе P2300 (std::execution)

---

## Обзор

Документ описывает архитектуру **Job System** на базе提案 P2300 (`std::execution`) — асинхронной модели программирования
с Sender/Receiver. Поскольку P2300 может быть не полностью реализован в стандартных библиотеках на начало 2026 года,
используется референсная имплементация **stdexec** от NVIDIA.

---

## 1. Математическая модель std::execution (P2300)

### 1.1 Формальное определение

Модель P2300 (`std::execution`) определяет три ключевых концепта:

$$\text{Sender} : \text{Receiver} \to \text{OperationState}$$

где:

- **Sender** — производитель значений, описывает асинхронную операцию
- **Receiver** — потребитель результатов или ошибок
- **OperationState** — состояние выполняемой операции, управляет lifetime

### 1.0 Важное замечание об API

**Публичный API ProjectV использует `std::execution` (C++26 P2300).**

Референсная имплементация `stdexec` от NVIDIA используется **только** в приватных `.cpp` файлах через PIMPL-паттерн, до
полной поддержки `std::execution` в Clang 19+.

```cpp
// .cppm (публичный интерфейс) — используем std::execution
export module ProjectV.Core.Jobs.ThreadPool;
import std.execution;  // C++26 P2300

// .cpp (приватная реализация) — используем stdexec
module ProjectV.Core.Jobs.ThreadPool;
#include <stdexec/execution.hpp>  // NVIDIA stdexec
```

### 1.2 Алгебра Sender'ов

Для двух sender'ов $S_1$ и $S_2$:

**Последовательная композиция:**
$$S_1 \gg S_2 = \text{then}(S_1, S_2)$$

**Параллельная композиция:**
$$S_1 \parallel S_2 = \text{when\_all}(S_1, S_2)$$

**Условная композиция:**
$$S_1 \triangleright S_2 = \text{let\_value}(S_1, \lambda v. S_2(v))$$

### 1.3 Типы Sender'ов

| Тип               | Описание                            | Пример                |
|-------------------|-------------------------------------|-----------------------|
| `sender_of<T>`    | Производит ровно одно значение `T`  | `just(42)`            |
| `sender_of<T, E>` | Может завершиться с ошибкой `E`     | `async_read()`        |
| `typed_sender`    | Знает свои типы на этапе компиляции | `schedule(scheduler)` |

---

## 2. Интеграция stdexec через CMake

### 2.1 FetchContent конфигурация

```cmake
# cmake/Dependencies/stdexec.cmake

# === stdexec (NVIDIA reference implementation of P2300) ===
# Используем последнюю версию с GitHub (main branch)
FetchContent_Declare(
    stdexec
    GIT_REPOSITORY https://github.com/NVIDIA/stdexec.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
    FETCH_SUBMODULES
)

# Опции сборки stdexec
set(STDEXEC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(STDEXEC_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(STDEXEC_ENABLE_CUDA OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(stdexec)
```

### 2.2 Интеграция в модуль Job System

```cmake
# src/core/jobs/CMakeLists.txt

add_library(ProjectV.Core.Jobs)

target_sources(ProjectV.Core.Jobs PUBLIC
    FILE_SET CXX_MODULES FILES
        ProjectV.Core.Jobs.cppm
        ProjectV.Core.Jobs.ThreadPool.cppm
        ProjectV.Core.Jobs.TaskGraph.cppm
    PRIVATE
        ProjectV.Core.Jobs.ThreadPool.cpp
        ProjectV.Core.Jobs.TaskGraph.cpp
)

# stdexec — C++ библиотека, требуется PIMPL
target_link_libraries(ProjectV.Core.Jobs
    PRIVATE
        stdexec
        std::thread
)

# Tracy для профилирования
target_include_directories(ProjectV.Core.Jobs
    PRIVATE
        ${CMAKE_SOURCE_DIR}/external/tracy/public/tracy
)

target_compile_definitions(ProjectV.Core.Jobs
    PRIVATE
        $<$<CONFIG:Debug>:TRACY_ENABLE>
)
```

### 2.3 Корневой CMakeLists.txt обновление

```cmake
# CMakeLists.txt (добавить после других зависимостей)

# === stdexec (P2300 reference implementation) ===
include(FetchContent)
FetchContent_Declare(
    stdexec
    GIT_REPOSITORY https://github.com/NVIDIA/stdexec.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
set(STDEXEC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(STDEXEC_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(stdexec)
```

---

## 3. Thread Pool на базе stdexec::static_thread_pool

### 3.1 Интерфейс модуля

```cpp
// ProjectV.Core.Jobs.ThreadPool.cppm
export module ProjectV.Core.Jobs.ThreadPool;

import std;
import std.execution;  // C++26 P2300

export namespace projectv::core::jobs {

/// Конфигурация Thread Pool.
export struct ThreadPoolConfig {
    uint32_t thread_count{0};           ///< 0 = auto (hardware_concurrency)
    bool set_affinity{false};           ///< Привязка потоков к ядрам (platform-specific)
    uint32_t stack_size_kb{0};          ///< 0 = default stack
    std::string_view name_prefix{"ProjectV-Worker"};
};

/// Статистика Thread Pool.
export struct ThreadPoolStats {
    uint32_t thread_count{0};
    uint64_t tasks_completed{0};
    uint64_t tasks_pending{0};
    float avg_task_time_us{0.0f};
    uint64_t total_idle_time_us{0};
};

/// Thread Pool — обёртка над std::execution scheduler.
///
/// ## P2300 Compliance
/// Публичный API использует std::execution (C++26).
/// Внутренняя реализация использует stdexec::static_thread_pool (PIMPL).
///
/// ## Thread Safety
/// - schedule(): thread-safe
/// - shutdown(): NOT thread-safe, вызывать только из main thread
///
/// ## Invariants
/// - После shutdown() новые задачи не принимаются
/// - Все pending задачи завершаются перед destruction
export class ThreadPool {
public:
    /// Создаёт Thread Pool.
    ///
    /// @param config Конфигурация
    /// @pre thread_count > 0 или thread_count == 0 (auto-detect)
    /// @post size() == config.thread_count или hardware_concurrency
    [[nodiscard]] static auto create(ThreadPoolConfig const& config = {}) noexcept
        -> std::expected<ThreadPool, JobError>;

    ~ThreadPool() noexcept;

    ThreadPool(ThreadPool&&) noexcept;
    ThreadPool& operator=(ThreadPool&&) noexcept;
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /// Получает scheduler для использования с sender algorithms.
    ///
    /// @return Scheduler для stdexec algorithms
    /// @pre !is_shutdown()
    [[nodiscard]] auto scheduler() noexcept -> stdexec::scheduler auto;

    /// Планирует выполнение задачи.
    ///
    /// @param f Функция для выполнения
    /// @return Sender, который можно скомбинировать с другими
    ///
    /// ## Example
    /// ```cpp
    /// auto sender = pool.schedule_on([] { return 42; })
    ///                | stdexec::then([](int x) { return x * 2; });
    /// auto [result] = stdexec::sync_wait(sender).value();
    /// // result == 84
    /// ```
    template<stdexec::sender F>
    [[nodiscard]] auto schedule_on(F&& f) noexcept
        -> stdexec::sender auto;

    /// Планирует выполнение sender'а на этом pool.
    template<stdexec::sender S>
    [[nodiscard]] auto execute(S&& sender) noexcept
        -> stdexec::sender auto;

    /// Получает количество потоков.
    [[nodiscard]] auto size() const noexcept -> uint32_t;

    /// Проверяет, остановлен ли pool.
    [[nodiscard]] auto is_shutdown() const noexcept -> bool;

    /// Останавливает pool (graceful shutdown).
    /// Завершает все pending задачи перед возвратом.
    auto shutdown() noexcept -> void;

    /// Получает статистику.
    [[nodiscard]] auto stats() const noexcept -> ThreadPoolStats;

private:
    ThreadPool() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Глобальный Thread Pool для движка.
/// Инициализируется при старте, уничтожается при shutdown.
export namespace global {
    [[nodiscard]] auto thread_pool() noexcept -> ThreadPool&;
    auto init_thread_pool(ThreadPoolConfig const& config = {}) noexcept -> void;
    auto shutdown_thread_pool() noexcept -> void;
}

} // namespace projectv::core::jobs
```

### 3.2 Реализация Thread Pool

```cpp
// ProjectV.Core.Jobs.ThreadPool.cpp
module ProjectV.Core.Jobs.ThreadPool;

// stdexec headers — только в .cpp (PIMPL)
#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>

import std;
import ProjectV.Core.Jobs.ThreadPool;

namespace projectv::core::jobs {

struct ThreadPool::Impl {
    exec::static_thread_pool pool;
    std::atomic<bool> shutdown{false};
    std::atomic<uint64_t> tasks_completed{0};
    std::atomic<uint64_t> total_task_time_ns{0};

    explicit Impl(uint32_t threads)
        : pool(threads > 0 ? threads : std::thread::hardware_concurrency())
    {}
};

auto ThreadPool::create(ThreadPoolConfig const& config) noexcept
    -> std::expected<ThreadPool, JobError> {

    ThreadPool result;
    try {
        uint32_t threads = config.thread_count;
        if (threads == 0) {
            threads = std::thread::hardware_concurrency();
            // Reserve 1-2 threads for main/render
            if (threads > 4) {
                threads -= 2;
            }
        }

        result.impl_ = std::make_unique<Impl>(threads);

        // Thread affinity setup (platform-specific)
        if (config.set_affinity) {
            // Windows: SetThreadAffinityMask
            // Linux: pthread_setaffinity_np
            // TODO: Implement in platform-specific code
        }

        return result;
    } catch (std::exception const& e) {
        return std::unexpected(JobError::ThreadPoolCreationFailed);
    }
}

ThreadPool::~ThreadPool() noexcept {
    if (impl_) {
        shutdown();
    }
}

ThreadPool::ThreadPool(ThreadPool&&) noexcept = default;
ThreadPool& ThreadPool::operator=(ThreadPool&&) noexcept = default;

auto ThreadPool::scheduler() noexcept -> stdexec::scheduler auto {
    return impl_->pool.get_scheduler();
}

template<stdexec::sender F>
auto ThreadPool::schedule_on(F&& f) noexcept -> stdexec::sender auto {
    using namespace stdexec;

    return schedule(scheduler())
         | then(std::forward<F>(f));
}

auto ThreadPool::size() const noexcept -> uint32_t {
    return static_cast<uint32_t>(impl_->pool.available_parallelism());
}

auto ThreadPool::is_shutdown() const noexcept -> bool {
    return impl_->shutdown.load(std::memory_order_acquire);
}

auto ThreadPool::shutdown() noexcept -> void {
    if (impl_->shutdown.exchange(true, std::memory_order_acq_rel)) {
        return; // Already shutdown
    }
    impl_->pool.request_stop();
}

auto ThreadPool::stats() const noexcept -> ThreadPoolStats {
    return {
        .thread_count = size(),
        .tasks_completed = impl_->tasks_completed.load(std::memory_order_relaxed),
        .tasks_pending = 0, // stdexec doesn't expose this directly
        .avg_task_time_us = 0.0f, // Computed from total_task_time_ns
        .total_idle_time_us = 0
    };
}

// Global thread pool
namespace global {
    static std::unique_ptr<ThreadPool> g_thread_pool;

    auto thread_pool() noexcept -> ThreadPool& {
        if (!g_thread_pool) {
            init_thread_pool();
        }
        return *g_thread_pool;
    }

    auto init_thread_pool(ThreadPoolConfig const& config) noexcept -> void {
        if (!g_thread_pool) {
            g_thread_pool = std::make_unique<ThreadPool>(
                ThreadPool::create(config).value()
            );
        }
    }

    auto shutdown_thread_pool() noexcept -> void {
        if (g_thread_pool) {
            g_thread_pool->shutdown();
            g_thread_pool.reset();
        }
    }
}

} // namespace projectv::core::jobs
```

---

## 4. Task Graph с Sender/Receiver пайпами

### 4.1 Интерфейс Task Graph

```cpp
// ProjectV.Core.Jobs.TaskGraph.cppm
export module ProjectV.Core.Jobs.TaskGraph;

import std;
import stdexec;
import ProjectV.Core.Jobs.ThreadPool;

export namespace projectv::core::jobs {

/// Task ID в графе.
export using TaskId = uint32_t;

/// Результат выполнения задачи.
export template<typename T>
using TaskResult = std::expected<T, JobError>;

/// Task Graph Node.
export struct TaskNode {
    TaskId id{0};
    std::string name;
    std::vector<TaskId> dependencies;
    bool is_critical{false};  ///< Blocks main thread if true
};

/// Task Graph — Directed Acyclic Graph задач.
///
/// ## Execution Model
/// 1. Топологическая сортировка
/// 2. Параллельный запуск независимых задач
/// 3. WhenAll для синхронизации зависимостей
///
/// ## Complexity
/// - add_task(): O(1)
/// - build(): O(V + E)
/// - execute(): O(V + E) с параллелизмом
export class TaskGraph {
public:
    TaskGraph() noexcept;
    ~TaskGraph() noexcept = default;

    TaskGraph(TaskGraph&&) noexcept = default;
    TaskGraph& operator=(TaskGraph&&) noexcept = default;
    TaskGraph(const TaskGraph&) = delete;
    TaskGraph& operator=(const TaskGraph&) = delete;

    /// Добавляет задачу в граф.
    ///
    /// @param name Имя задачи (для профилирования)
    /// @param sender Sender для выполнения
    /// @param dependencies Список ID задач, от которых зависит эта
    /// @return TaskId добавленной задачи
    template<stdexec::sender S>
    auto add_task(std::string_view name, S&& sender,
                  std::span<TaskId const> dependencies = {}) noexcept
        -> TaskId;

    /// Добавляет функцию как задачу.
    ///
    /// @param name Имя задачи
    /// @param f Функция для выполнения
    /// @param dependencies Зависимости
    /// @return TaskId
    template<std::invocable F>
    auto add_task(std::string_view name, F&& f,
                  std::span<TaskId const> dependencies = {}) noexcept
        -> TaskId;

    /// Строит граф для выполнения.
    /// Вызывает topological sort и готовит sender'ы.
    ///
    /// @pre Есть хотя бы одна задача
    /// @return Ошибка если есть цикл
    auto build() noexcept -> std::expected<void, JobError>;

    /// Выполняет граф задач.
    ///
    /// @param pool Thread pool для выполнения
    /// @return Sender, завершающийся когда все задачи выполнены
    [[nodiscard]] auto execute(ThreadPool& pool) noexcept
        -> stdexec::sender auto;

    /// Выполняет синхронно (блокирует текущий поток).
    auto execute_sync(ThreadPool& pool) noexcept
        -> std::expected<void, JobError>;

    /// Очищает граф.
    auto clear() noexcept -> void;

    /// Получает количество задач.
    [[nodiscard]] auto size() const noexcept -> size_t;

    /// Проверяет пустоту.
    [[nodiscard]] auto empty() const noexcept -> bool;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Примеры Task Graph построения.
export namespace task_graph_examples {

/// Параллельная обработка чанков.
/// Использует stdexec::bulk для параллельного обхода.
auto parallel_chunk_processing(
    ThreadPool& pool,
    std::span<ChunkData> chunks
) noexcept -> stdexec::sender auto;

/// Pipeline обработки: Load → Process → Upload.
auto processing_pipeline(
    ThreadPool& pool,
    std::vector<VoxelData> data
) noexcept -> stdexec::sender auto;

} // namespace task_graph_examples

} // namespace projectv::core::jobs
```

### 4.2 Реализация Task Graph

```cpp
// ProjectV.Core.Jobs.TaskGraph.cpp
module ProjectV.Core.Jobs.TaskGraph;

#include <stdexec/execution.hpp>
#include <exec/when_any.hpp>

import std;
import ProjectV.Core.Jobs.ThreadPool;
import ProjectV.Core.Jobs.TaskGraph;

namespace projectv::core::jobs {

struct TaskGraph::Impl {
    struct Task {
        TaskId id;
        std::string name;
        std::vector<TaskId> dependencies;
        stdexec::sender auto sender;
        bool built{false};
    };

    std::vector<Task> tasks;
    std::unordered_map<TaskId, size_t> task_index;
    std::vector<TaskId> topological_order;
    TaskId next_id{0};
    bool has_cycle{false};

    auto topological_sort() -> std::expected<void, JobError> {
        // Kahn's algorithm
        std::vector<size_t> in_degree(tasks.size(), 0);

        for (auto const& task : tasks) {
            for (auto dep : task.dependencies) {
                if (task_index.contains(dep)) {
                    in_degree[task_index[dep]]++;
                }
            }
        }

        std::queue<size_t> queue;
        for (size_t i = 0; i < in_degree.size(); ++i) {
            if (in_degree[i] == 0) {
                queue.push(i);
            }
        }

        topological_order.clear();
        topological_order.reserve(tasks.size());

        while (!queue.empty()) {
            size_t idx = queue.front();
            queue.pop();

            topological_order.push_back(tasks[idx].id);

            for (auto dep : tasks[idx].dependencies) {
                if (task_index.contains(dep)) {
                    size_t dep_idx = task_index[dep];
                    if (--in_degree[dep_idx] == 0) {
                        queue.push(dep_idx);
                    }
                }
            }
        }

        if (topological_order.size() != tasks.size()) {
            return std::unexpected(JobError::TaskGraphHasCycle);
        }

        return {};
    }
};

TaskGraph::TaskGraph() noexcept
    : impl_(std::make_unique<Impl>()) {}

auto TaskGraph::build() noexcept -> std::expected<void, JobError> {
    return impl_->topological_sort();
}

auto TaskGraph::execute(ThreadPool& pool) noexcept
    -> stdexec::sender auto {
    using namespace stdexec;

    auto scheduler = pool.scheduler();

    // Build when_all sender for independent tasks
    std::vector<sender auto> independent_senders;

    for (auto const& task_id : impl_->topological_order) {
        auto& task = impl_->tasks[impl_->task_index[task_id]];

        if (task.dependencies.empty()) {
            // Independent task, can start immediately
            independent_senders.push_back(
                schedule(scheduler) | then([&task] {
                    // Execute task sender
                    return stdexec::sync_wait(task.sender);
                })
            );
        } else {
            // Dependent task, wait for dependencies
            // Build when_all for dependencies
            // ...
        }
    }

    return when_all(std::move(independent_senders));
}

auto TaskGraph::execute_sync(ThreadPool& pool) noexcept
    -> std::expected<void, JobError> {
    try {
        auto sender = execute(pool);
        stdexec::sync_wait(std::move(sender));
        return {};
    } catch (std::exception const&) {
        return std::unexpected(JobError::TaskExecutionFailed);
    }
}

auto TaskGraph::clear() noexcept -> void {
    impl_->tasks.clear();
    impl_->task_index.clear();
    impl_->topological_order.clear();
    impl_->next_id = 0;
}

auto TaskGraph::size() const noexcept -> size_t {
    return impl_->tasks.size();
}

auto TaskGraph::empty() const noexcept -> bool {
    return impl_->tasks.empty();
}

// Example implementations
namespace task_graph_examples {

auto parallel_chunk_processing(
    ThreadPool& pool,
    std::span<ChunkData> chunks
) noexcept -> stdexec::sender auto {
    using namespace stdexec;

    auto scheduler = pool.scheduler();

    // Use bulk for parallel processing
    return schedule(scheduler)
         | bulk(chunks.size(), [&](size_t i) {
               // Process chunk i
               process_chunk(chunks[i]);
           });
}

auto processing_pipeline(
    ThreadPool& pool,
    std::vector<VoxelData> data
) noexcept -> stdexec::sender auto {
    using namespace stdexec;

    auto scheduler = pool.scheduler();

    // Stage 1: Load (parallel)
    auto load_stage = schedule(scheduler)
                    | bulk(data.size(), [&](size_t i) {
                          load_voxel_data(data[i]);
                      });

    // Stage 2: Process (depends on load)
    auto process_stage = load_stage
                       | then([&]() {
                             return schedule(scheduler)
                                  | bulk(data.size(), [&](size_t i) {
                                        process_voxel_data(data[i]);
                                    });
                         })
                       | let_value([](auto s) { return s; });

    // Stage 3: Upload (depends on process)
    auto upload_stage = process_stage
                      | then([&]() {
                            return schedule(scheduler)
                                 | bulk(data.size(), [&](size_t i) {
                                       upload_voxel_data(data[i]);
                                   });
                        })
                      | let_value([](auto s) { return s; });

    return upload_stage;
}

} // namespace task_graph_examples

} // namespace projectv::core::jobs
```

---

## 5. Примеры Sender/Receiver пайпов

### 5.1 Базовые пайпы

```cpp
// ProjectV.Core.Jobs.Examples.cppm
export module ProjectV.Core.Jobs.Examples;

import std;
import stdexec;
import ProjectV.Core.Jobs.ThreadPool;
import ProjectV.Core.Jobs.TaskGraph;

export namespace projectv::core::jobs::examples {

/// Пример 1: Простая асинхронная задача.
auto example_simple_async(ThreadPool& pool) -> int {
    using namespace stdexec;

    // Создаём sender, который выполняется на pool
    auto work = schedule(pool.scheduler())
              | then([] {
                    // Выполняется на worker thread
                    return 42;
                });

    // Синхронное ожидание результата
    auto [result] = sync_wait(work).value();
    return result;
}

/// Пример 2: Цепочка трансформаций.
auto example_chain(ThreadPool& pool) -> int {
    using namespace stdexec;

    auto pipeline = schedule(pool.scheduler())
                  | then([] { return 10; })              // 10
                  | then([](int x) { return x * 2; })    // 20
                  | then([](int x) { return x + 5; });   // 25

    auto [result] = sync_wait(pipeline).value();
    return result;
}

/// Пример 3: Параллельное выполнение (when_all).
auto example_parallel(ThreadPool& pool) -> std::tuple<int, double, std::string> {
    using namespace stdexec;

    auto task1 = schedule(pool.scheduler()) | then([] { return 1; });
    auto task2 = schedule(pool.scheduler()) | then([] { return 2.0; });
    auto task3 = schedule(pool.scheduler()) | then([] { return std::string("three"); });

    // when_all выполняет параллельно и собирает результаты
    auto all = when_all(std::move(task1), std::move(task2), std::move(task3));

    auto [r1, r2, r3] = sync_wait(all).value();
    return {r1, r2, r3};
}

/// Пример 4: Bulk параллелизм для обхода чанков.
auto example_bulk_chunks(ThreadPool& pool, std::span<Chunk> chunks) -> void {
    using namespace stdexec;

    // bulk автоматически распараллеливает итерации
    auto work = schedule(pool.scheduler())
              | bulk(chunks.size(), [&](size_t i) {
                    update_chunk(chunks[i]);
                });

    sync_wait(work);
}

/// Пример 5: Условное выполнение (let_value).
auto example_conditional(ThreadPool& pool, bool condition) -> int {
    using namespace stdexec;

    auto work = schedule(pool.scheduler())
              | then([&] { return condition ? 1 : 0; })
              | let_value([](int flag) -> sender_of<int> {
                    if (flag == 1) {
                        return just(100);
                    } else {
                        return just(0);
                    }
                });

    auto [result] = sync_wait(work).value();
    return result;
}

/// Пример 6: Обработка ошибок.
auto example_error_handling(ThreadPool& pool) -> std::expected<int, JobError> {
    using namespace stdexec;

    auto work = schedule(pool.scheduler())
              | then([] -> int {
                    if (should_fail()) {
                        throw std::runtime_error("Failed");
                    }
                    return 42;
                });

    try {
        auto [result] = sync_wait(work).value();
        return result;
    } catch (std::exception const&) {
        return std::unexpected(JobError::TaskExecutionFailed);
    }
}

/// Пример 7: Комплексный pipeline для voxel updates.
auto example_voxel_pipeline(
    ThreadPool& pool,
    std::span<VoxelChunk const> dirty_chunks
) -> stdexec::sender auto {
    using namespace stdexec;

    auto scheduler = pool.scheduler();

    // Phase 1: Parallel voxel simulation (bulk)
    auto simulate = schedule(scheduler)
                  | bulk(dirty_chunks.size(), [&](size_t i) {
                        simulate_chunk(dirty_chunks[i]);
                    });

    // Phase 2: Mesh generation (parallel per chunk)
    auto generate_meshes = simulate
                         | then([&]() {
                               return schedule(scheduler)
                                    | bulk(dirty_chunks.size(), [&](size_t i) {
                                          generate_chunk_mesh(dirty_chunks[i]);
                                      });
                           })
                         | let_value([](auto s) { return s; });

    // Phase 3: Upload to GPU (serial, single thread)
    auto upload = generate_meshes
                | then([&]() {
                      upload_all_meshes(dirty_chunks);
                  });

    return upload;
}

} // namespace projectv::core::jobs::examples
```

### 5.2 Параллельный обход чанков с bulk

```cpp
// Пример использования bulk для параллельного CA обновления
export namespace projectv::core::jobs {

/// Параллельное обновление Cellular Automata чанков.
///
/// ## Algorithm
/// 1. Разделение dirty чанков на batches по worker count
/// 2. bulk параллельное выполнение batch updates
/// 3. Синхронизация результатов
///
/// ## Complexity
/// $T_{\text{parallel}} = O(n / p)$ где $p$ = thread count
auto parallel_ca_update(
    ThreadPool& pool,
    std::span<voxel::ChunkCAState> chunks,
    float delta_time
) noexcept -> stdexec::sender auto {
    using namespace stdexec;

    auto scheduler = pool.scheduler();
    auto thread_count = pool.size();

    return schedule(scheduler)
         | bulk(chunks.size(), [&](size_t i) {
               // Каждый chunk обрабатывается независимо
               update_chunk_ca(chunks[i], delta_time);
           });
}

/// Параллельная генерация mesh для чанков.
auto parallel_mesh_generation(
    ThreadPool& pool,
    std::span<voxel::ChunkData const> chunks,
    std::span<render::ChunkMesh> output_meshes
) noexcept -> stdexec::sender auto {
    using namespace stdexec;

    assert(chunks.size() == output_meshes.size());

    return schedule(pool.scheduler())
         | bulk(chunks.size(), [&](size_t i) {
               output_meshes[i] = generate_greedy_mesh(chunks[i]);
           });
}

} // namespace projectv::core::jobs
```

---

## 6. Интеграция с Flecs

### 6.1 Архитектура интеграции

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Flecs + stdexec Integration                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                     Main Thread                                   │    │
│  │                                                                   │    │
│  │  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐          │    │
│  │  │ InputSystem │───▶│ Flecs ECS   │───▶│ RenderSystem│          │    │
│  │  │             │    │ (sequenced) │    │             │          │    │
│  │  └─────────────┘    └──────┬──────┘    └─────────────┘          │    │
│  │                            │                                      │    │
│  │                            │ ecs.progress()                      │    │
│  │                            ▼                                      │    │
│  │                     ┌──────────────┐                             │    │
│  │                     │ Flecs Worker │                             │    │
│  │                     │   Threads    │                             │    │
│  │                     │ (set_threads)│                             │    │
│  │                     └──────────────┘                             │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                   Job System (stdexec)                           │    │
│  │                                                                   │    │
│  │  ┌─────────────────────────────────────────────────────────────┐│    │
│  │  │              stdexec::static_thread_pool                    ││    │
│  │  │                                                             ││    │
│  │  │  Worker 0 ──▶ Worker 1 ──▶ Worker 2 ──▶ Worker N           ││    │
│  │  │                                                             ││    │
│  │  │  ┌─────────────────────────────────────────────────────┐   ││    │
│  │  │  │ Task Graph (DAG)                                    │   ││    │
│  │  │  │                                                     │   ││    │
│  │  │  │  [CA Update] ──▶ [Mesh Gen] ──▶ [GPU Upload]       │   ││    │
│  │  │  │       │              │                               │   ││    │
│  │  │  │       └──────────────┴──▶ [Physics Sync]           │   ││    │
│  │  │  └─────────────────────────────────────────────────────┘   ││    │
│  │  └─────────────────────────────────────────────────────────────┘│    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 6.2 Код интеграции

```cpp
// ProjectV.ECS.FlecsScheduler.cppm
export module ProjectV.ECS.FlecsScheduler;

import std;
import stdexec;
import flecs;
import ProjectV.Core.Jobs.ThreadPool;
import ProjectV.Core.Jobs.TaskGraph;

export namespace projectv::ecs {

/// Конфигурация гибридного шедулера.
export struct HybridSchedulerConfig {
    uint32_t flecs_threads{2};      ///< Потоки для Flecs ECS systems
    uint32_t job_system_threads{0}; ///< Потоки для stdexec (0 = auto)
    bool separate_pools{true};      ///< Раздельные пулы или общий
};

/// Гибридный шедулер: Flecs для ECS logic, stdexec для parallel algorithms.
///
/// ## Architecture
/// - Flecs: управляет entity/component lifetime, sequenced systems
/// - stdexec: параллельные алгоритмы (bulk, when_all)
///
/// ## Thread Distribution
/// При 8 потоках:
/// - 2 потока: Flecs workers (ecs.set_threads(2))
/// - 6 потоков: stdexec static_thread_pool
///
/// ## When to use which?
/// - Flecs: Systems с component iteration, queries
/// - stdexec: Parallel loops, independent tasks, pipelines
export class HybridScheduler {
public:
    /// Создаёт гибридный шедулер.
    [[nodiscard]] static auto create(
        flecs::world& world,
        HybridSchedulerConfig const& config = {}
    ) noexcept -> std::expected<HybridScheduler, JobError>;

    ~HybridScheduler() noexcept;

    HybridScheduler(HybridScheduler&&) noexcept;
    HybridScheduler& operator=(HybridScheduler&&) noexcept;
    HybridScheduler(const HybridScheduler&) = delete;
    HybridScheduler& operator=(const HybridScheduler&) = delete;

    /// Выполняет ECS frame.
    /// 1. Flecs ecs.progress() на Flecs threads
    /// 2. Pending stdexec tasks на job system threads
    ///
    /// @param delta_time Время кадра
    /// @return true если успешно
    auto progress(float delta_time) noexcept -> bool;

    /// Добавляет параллельную задачу для выполнения в этом кадре.
    template<stdexec::sender S>
    auto schedule_parallel_task(S&& sender) noexcept -> void;

    /// Получает Thread Pool для stdexec.
    [[nodiscard]] auto job_pool() noexcept -> jobs::ThreadPool&;

    /// Получает Flecs world.
    [[nodiscard]] auto world() noexcept -> flecs::world&;

    /// Ожидает завершения всех параллельных задач.
    auto wait_parallel_tasks() noexcept -> void;

private:
    HybridScheduler() noexcept = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Flecs System Builder с интеграцией stdexec.
///
/// ## Example: Parallel iteration over chunks
/// ```cpp
/// world.system<ChunkComponent>("UpdateChunks")
///     .iter(stdexec_parallel_iteration)
///     .each([](flecs::iter& it, size_t i, ChunkComponent& chunk) {
///         update_chunk(chunk);
///     });
/// ```
export class FlecsStdexecBridge {
public:
    /// Регистрирует систему с параллельной итерацией.
    /// Использует stdexec::bulk вместо Flecs built-in threading.
    static auto register_parallel_system(
        flecs::world& world,
        std::string_view name,
        auto component_types,
        auto&& update_func,
        jobs::ThreadPool& pool
    ) -> flecs::system;

    /// Выполняет Flecs query параллельно через stdexec.
    template<typename... Components>
    static auto parallel_query(
        flecs::world& world,
        jobs::ThreadPool& pool,
        auto&& func
    ) -> stdexec::sender auto;
};

} // namespace projectv::ecs
```

### 6.3 Реализация интеграции

```cpp
// ProjectV.ECS.FlecsScheduler.cpp
module ProjectV.ECS.FlecsScheduler;

#include <flecs.h>
#include <stdexec/execution.hpp>

import std;
import ProjectV.ECS.FlecsScheduler;
import ProjectV.Core.Jobs.ThreadPool;

namespace projectv::ecs {

struct HybridScheduler::Impl {
    flecs::world* world{nullptr};
    std::unique_ptr<jobs::ThreadPool> job_pool;
    std::vector<stdexec::sender auto> pending_tasks;
    uint32_t flecs_threads{2};
};

auto HybridScheduler::create(
    flecs::world& world,
    HybridSchedulerConfig const& config
) noexcept -> std::expected<HybridScheduler, JobError> {

    HybridScheduler result;
    result.impl_ = std::make_unique<Impl>();

    result.impl_->world = &world;
    result.impl_->flecs_threads = config.flecs_threads;

    // Настройка Flecs threads
    if (config.flecs_threads > 0) {
        world.set_threads(static_cast<int32_t>(config.flecs_threads));
    }

    // Создание отдельного Thread Pool для stdexec
    if (config.separate_pools) {
        uint32_t job_threads = config.job_system_threads;
        if (job_threads == 0) {
            auto hw_threads = std::thread::hardware_concurrency();
            job_threads = hw_threads > config.flecs_threads
                        ? hw_threads - config.flecs_threads
                        : 1;
        }

        auto pool_result = jobs::ThreadPool::create({
            .thread_count = job_threads,
            .name_prefix = "PV-Job"
        });

        if (!pool_result) {
            return std::unexpected(pool_result.error());
        }

        result.impl_->job_pool = std::make_unique<jobs::ThreadPool>(
            std::move(*pool_result)
        );
    }

    return result;
}

HybridScheduler::~HybridScheduler() noexcept = default;
HybridScheduler::HybridScheduler(HybridScheduler&&) noexcept = default;
HybridScheduler& HybridScheduler::operator=(HybridScheduler&&) noexcept = default;

auto HybridScheduler::progress(float delta_time) noexcept -> bool {
    // 1. Запуск pending parallel tasks на job pool
    if (!impl_->pending_tasks.empty()) {
        auto all_tasks = stdexec::when_all(std::move(impl_->pending_tasks));
        impl_->pending_tasks.clear();

        // Non-blocking start
        // TODO: Store operation state for later sync
    }

    // 2. Flecs progress на Flecs threads
    return impl_->world->progress(delta_time);
}

auto HybridScheduler::job_pool() noexcept -> jobs::ThreadPool& {
    return *impl_->job_pool;
}

auto HybridScheduler::world() noexcept -> flecs::world& {
    return *impl_->world;
}

auto HybridScheduler::wait_parallel_tasks() noexcept -> void {
    // Sync all pending stdexec tasks
    // TODO: Implement with stored operation states
}

// FlecsStdexecBridge implementation
template<typename... Components>
auto FlecsStdexecBridge::parallel_query(
    flecs::world& world,
    jobs::ThreadPool& pool,
    auto&& func
) -> stdexec::sender auto {
    using namespace stdexec;

    // Собираем все entities matching query
    auto query = world.query<Components...>();
    std::vector<std::tuple<Components*...>> components;

    query.each([&](Components&... comps) {
        components.emplace_back(&comps...);
    });

    // Параллельная обработка через bulk
    return schedule(pool.scheduler())
         | bulk(components.size(), [&](size_t i) {
               std::apply(func, components[i]);
           });
}

} // namespace projectv::ecs
```

---

## 7. Tracy Profiling Integration

### 7.1 Профилирование Sender'ов

```cpp
// ProjectV.Core.Jobs.Profiling.cppm
export module ProjectV.Core.Jobs.Profiling;

import std;
import stdexec;
import ProjectV.Core.Jobs.ThreadPool;

export namespace projectv::core::jobs {

/// Tracy-инструментированный sender.
///
/// ## Usage
/// ```cpp
/// auto profiled = with_tracy("MyTask", original_sender);
/// ```
template<stdexec::sender S>
auto with_tracy(std::string_view name, S&& sender) noexcept
    -> stdexec::sender auto {

    return stdexec::then(std::forward<S>(sender), [name](auto&&... args) {
        TracyMessageL(name.data());
        return std::forward<decltype(args)>(args)...;
    });
}

/// Frame-level profiling для Job System.
export class JobSystemProfiler {
public:
    /// Начинает frame profiling.
    static auto begin_frame() noexcept -> void;

    /// Заканчивает frame profiling.
    static auto end_frame() noexcept -> void;

    /// Регистрирует task start.
    static auto task_start(std::string_view name) noexcept -> void;

    /// Регистрирует task end.
    static auto task_end(std::string_view name) noexcept -> void;

    /// Получает статистику кадра.
    struct FrameStats {
        uint32_t tasks_executed{0};
        float total_task_time_ms{0.0f};
        float avg_task_time_us{0.0f};
        uint32_t parallelism_level{0};
    };

    static auto frame_stats() noexcept -> FrameStats;
};

/// RAII scoped profiler для задач.
export struct ScopedTaskProfile {
    std::string_view name;

    explicit ScopedTaskProfile(std::string_view task_name) noexcept
        : name(task_name) {
        JobSystemProfiler::task_start(name);
    }

    ~ScopedTaskProfile() noexcept {
        JobSystemProfiler::task_end(name);
    }
};

} // namespace projectv::core::jobs

// Tracy macros
#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#define JOB_PROFILE(name) ZoneScopedN(name)
#define JOB_PROFILE_FRAME() FrameMark
#else
#define JOB_PROFILE(name)
#define JOB_PROFILE_FRAME()
#endif
```

---

## 8. Error Handling

```cpp
// ProjectV.Core.Jobs.Error.cppm
export module ProjectV.Core.Jobs.Error;

import std;

export namespace projectv::core::jobs {

/// Коды ошибок Job System.
export enum class JobError : uint8_t {
    Success = 0,
    ThreadPoolCreationFailed,
    TaskExecutionFailed,
    TaskGraphHasCycle,
    TaskDependencyNotFound,
    SchedulerNotAvailable,
    Timeout,
    Cancelled,
    OutOfMemory
};

/// Конвертирует JobError в строку.
[[nodiscard]] auto to_string(JobError error) noexcept -> std::string_view {
    switch (error) {
        case JobError::Success:
            return "Success";
        case JobError::ThreadPoolCreationFailed:
            return "Failed to create thread pool";
        case JobError::TaskExecutionFailed:
            return "Task execution failed";
        case JobError::TaskGraphHasCycle:
            return "Task graph contains cycle";
        case JobError::TaskDependencyNotFound:
            return "Task dependency not found";
        case JobError::SchedulerNotAvailable:
            return "Scheduler not available";
        case JobError::Timeout:
            return "Operation timed out";
        case JobError::Cancelled:
            return "Operation cancelled";
        case JobError::OutOfMemory:
            return "Out of memory";
        default:
            return "Unknown error";
    }
}

} // namespace projectv::core::jobs
```

---

## Статус

| Компонент                    | Статус         | Приоритет |
|------------------------------|----------------|-----------|
| ThreadPool (stdexec wrapper) | Специфицирован | P0        |
| TaskGraph                    | Специфицирован | P0        |
| Sender/Receiver Examples     | Специфицирован | P0        |
| Flecs Integration            | Специфицирован | P1        |
| Tracy Profiling              | Специфицирован | P1        |

---

## Ссылки

- [P2300: std::execution](https://wg21.link/p2300)
- [stdexec (NVIDIA)](https://github.com/NVIDIA/stdexec)
- [Flecs Threading](https://www.flecs.dev/flecs/md_docs_2Multithreading.html)

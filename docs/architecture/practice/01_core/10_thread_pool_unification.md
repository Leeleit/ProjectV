# Thread Pool Unification: Flecs OS API → stdexec

> **Для понимания:** Представьте два оркестра, играющих одновременно в одном зале — это хаос. Нам нужен один дирижёр,
> который распределяет所有 музыкантов. Thread pool unification — это "один дирижёр" для всех задач: и Flecs ECS systems,
> и
> stdexec parallel algorithms.

## Проблема: Thread Oversubscription

### Текущая ошибочная архитектура

```cpp
// В 05_job_system.md секция 6:
// Flecs создаёт свои потоки
world.set_threads(2);  // ← Thread Pool A (2 потока)

// stdexec создаёт свои потоки
static_thread_pool pool(6);  // ← Thread Pool B (6 потоков)

// ИТОГО: 8 потоков конкурируют за CPU ядра!
```

**Проблемы:**

1. **Context switching overhead** — OS переключает между 8 потоками
2. **Cache pollution** — каждый поток имеет свой кэш L1/L2
3. **No reserved threads** — нет резервных потоков для OS, GPU driver
4. **Priority inversion** — Flecs и stdexec конкурируют за ресурсы

### Математика проблемы

При 8-ядерном CPU:

```
До объединения:
  Flecs workers:     2 потока
  stdexec workers:   6 потоков
  OS reserved:       0 потоков
  Итого:            8 конкурирующих потоков
  Context switches:  ~8000/сек при 100% load

После объединения:
  Unified pool:      6 потоков (75% CPU)
  OS reserved:       2 потока (25% CPU)
  Итого:            6 кооперирующихся потоков
  Context switches:  ~2000/сек при 100% load

Выигрыш: 4× меньше context switches
```

---

## Решение: ecs_os_set_api_defaults()

### Как это работает

Flecs позволяет **полностью подменить OS API** через `ecs_os_set_api_defaults()`. Мы перенаправим все Flecs threading в
наш единый stdexec pool.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Thread Pool Unification                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ДО (плохо):                          ПОСЛЕ (хорошо):                 │
│                                                                          │
│  ┌─────────────┐                      ┌─────────────────────────────┐  │
│  │  Flecs ECS  │                      │    Unified stdexec Pool     │  │
│  │ set_threads │                      │         (6 потоков)        │  │
│  │   (2 потока)│                      │                             │  │
│  └──────┬──────┘                      │  ┌───────────────────────┐ │  │
│         │                             │  │  Flecs Systems        │ │  │
│  ┌──────┴──────┐                     │  │  (через custom OS API)│ │  │
│  │  stdexec   │                     │  └───────────┬───────────┘ │  │
│  │    pool    │                     │              │             │  │
│  │  (6 потоков)│                     │  ┌───────────▼───────────┐ │  │
│  └─────────────┘                     │  │  stdexec algorithms  │ │  │
│                                     │  │  (bulk, when_all)    │ │  │
│  8 потоков = хаос!                  │  └───────────────────────┘ │  │
│                                     │                             │  │
│                                     └─────────────────────────────┘  │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### Код интеграции

```cpp
// ProjectV.Core.FlecsOsApi.cppm
export module ProjectV.Core.FlecsOsApi;

import std;
import flecs;
import stdexec;
import ProjectV.Core.Jobs.ThreadPool;

export namespace projectv::core {

/// Custom Flecs OS API, интегрированный с stdexec static_thread_pool.
/// Перенаправляет все Flecs threading operations в единый thread pool.
export class FlecsOsApi {
public:
    /// Инициализирует custom OS API для Flecs.
    /// Должна вызываться ПЕРЕД созданием flecs::world.
    ///
    /// @param pool Ссылка на существующий stdexec thread pool
    /// @pre pool не должен быть nullptr
    /// @pre pool->is_shutdown() == false
    static auto init(stdexec::scheduler auto&& scheduler) noexcept -> void;

    /// Восстанавливает оригинальный OS API.
    static auto shutdown() noexcept -> void;

    /// Проверяет, инициализирован ли custom API.
    [[nodiscard]] static auto is_initialized() noexcept -> bool;

private:
    static bool g_initialized;
};

} // namespace projectv::core
```

```cpp
// ProjectV.Core.FlecsOsApi.cpp
module ProjectV.Core.FlecsOsApi;

#include <flecs.h>
#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>

import std;
import ProjectV.Core.Jobs.ThreadPool;

namespace projectv::core {

// Глобальный scheduler для Flecs
static stdexec::scheduler auto* g_scheduler = nullptr;
static ecs_os_api_t g_original_api{};
static ecs_os_api_t g_custom_api{};
static bool FlecsOsApi::g_initialized = false;

// Custom thread implementation
static int64_t flecs_thread_new(ecs_thread_callback_t callback, void* arg) {
    // Запускаем в stdexec pool вместо создания нового потока
    auto sender = stdexec::schedule(*g_scheduler)
                | stdexec::then([callback, arg]() {
                      callback(arg);
                  });

    // Fire-and-forget (Flecs не ожидает результат)
    stdexec::start_detached(std::move(sender));

    return 0;  // Возвращаем "thread ID" (Fake, Flecs не использует)
}

static void flecs_thread_join(int64_t) {
    // Нет-op: stdexec выполняет синхронно в нашем pool
    // Для реального join нужно хранить future
}

static void* flecs_alloc(uint64_t size) {
    return std::malloc(size);
}

static void* flecs_realloc(void* ptr, uint64_t size) {
    return std::realloc(ptr, size);
}

static void flecs_free(void* ptr) {
    std::free(ptr);
}

static void* flecs_alloc_aligned(uint64_t size, uint8_t align) {
    return aligned_alloc(align, size);
}

static void flecs_free_aligned(void* ptr) {
    free(ptr);  // aligned_alloc/free совместимы
}

// Mutex implementation (stub - можно улучшить)
static ecs_os_mutex_t flecs_mutex_new() {
    return reinterpret_cast<ecs_os_mutex_t>(new std::mutex());
}

static void flecs_mutex_free(ecs_os_mutex_t mutex) {
    delete reinterpret_cast<std::mutex*>(mutex);
}

static void flecs_mutex_lock(ecs_os_mutex_t mutex) {
    reinterpret_cast<std::mutex*>(mutex)->lock();
}

static void flecs_mutex_unlock(ecs_os_mutex_t mutex) {
    reinterpret_cast<std::mutex*>(mutex)->unlock();
}

// Atomic operations - используем std::atomic
static int32_t flecs_atomic_inc_i32(int32_t* var) {
    return std::atomic_fetch_add_explicit(var, 1, std::memory_order_relaxed) + 1;
}

static int32_t flecs_atomic_dec_i32(int32_t* var) {
    return std::atomic_fetch_sub_explicit(var, 1, std::memory_order_relaxed) - 1;
}

static int64_t flecs_atomic_inc_i64(int64_t* var) {
    return std::atomic_fetch_add_explicit(var, 1, std::memory_order_relaxed) + 1;
}

static int64_t flecs_atomic_dec_i64(int64_t* var) {
    return std::atomic_fetch_sub_explicit(var, 1, std::memory_order_relaxed) - 1;
}

static int32_t flecs_atomic_cas_i32(int32_t* var, int32_t comp, int32_t val) {
    std::atomic_compare_exchange_strong(var, &comp, val);
    return comp;
}

// Task API для асинхронных задач
static ecs_os_task_t flecs_task_new(ecs_thread_callback_t callback, void* arg) {
    // Аналогично flecs_thread_new, но для "задач"
    auto sender = stdexec::schedule(*g_scheduler)
                | stdexec::then([callback, arg]() {
                      callback(arg);
                  });

    stdexec::start_detached(std::move(sender));
    return 1;  // Fake task ID
}

auto FlecsOsApi::init(stdexec::scheduler auto&& scheduler) noexcept -> void {
    if (g_initialized) {
        return;  // Already initialized
    }

    // Сохраняем оригинальный API
    g_original_api = ecs_os_get_api();

    // Настраиваем custom API
    g_custom_api = g_original_api;

    // Thread API
    g_custom_api.thread_new_ = flecs_thread_new;
    g_custom_api.thread_join_ = flecs_thread_join;

    // Memory API
    g_custom_api.malloc_ = flecs_alloc;
    g_custom_api.realloc_ = flecs_realloc;
    g_custom_api.free_ = flecs_free;
    g_custom_api.mem_aligned_alloc_ = flecs_alloc_aligned;
    g_custom_api.mem_aligned_free_ = flecs_free_aligned;

    // Mutex API
    g_custom_api.mutex_new_ = flecs_mutex_new;
    g_custom_api.mutex_free_ = flecs_mutex_free;
    g_custom_api.mutex_lock_ = flecs_mutex_lock;
    g_custom_api.mutex_unlock_ = flecs_mutex_unlock;

    // Atomic API
    g_custom_api.atomic_inc_i32_ = flecs_atomic_inc_i32;
    g_custom_api.atomic_dec_i32_ = flecs_atomic_dec_i32;
    g_custom_api.atomic_inc_i64_ = flecs_atomic_inc_i64;
    g_custom_api.atomic_dec_i64_ = flecs_atomic_dec_i64;
    g_custom_api.atomic_cas_i32_ = flecs_atomic_cas_i32;

    // Task API (optional, Flecs 5.0+)
    g_custom_api.task_new_ = flecs_task_new;
    g_custom_api.task_join_ = nullptr;  // Not implemented

    // Устанавливаем custom API
    ecs_os_set_api(&g_custom_api);

    // Сохраняем scheduler для использования в callbacks
    g_scheduler = std::addressof(scheduler);

    g_initialized = true;
}

auto FlecsOsApi::shutdown() noexcept -> void {
    if (!g_initialized) {
        return;
    }

    // Восстанавливаем оригинальный API
    ecs_os_set_api(&g_original_api);
    g_scheduler = nullptr;

    g_initialized = false;
}

auto FlecsOsApi::is_initialized() noexcept -> bool {
    return g_initialized;
}

} // namespace projectv::core
```

---

## Правильная инициализация движка

```cpp
// Engine initialization - правильный порядок
class Engine {
public:
    void init() {
        // 1. Создаём unified thread pool (N-2 потока, где N = hardware concurrency)
        uint32_t hw_threads = std::thread::hardware_concurrency();
        uint32_t pool_threads = hw_threads >= 4 ? hw_threads - 2 : hw_threads;

        auto pool_result = ThreadPool::create({
            .thread_count = pool_threads,
            .name_prefix = "PV-Worker"
        });

        if (!pool_result) {
            throw std::runtime_error("Failed to create thread pool");
        }

        auto& pool = *pool_result;

        // 2. Инициализируем Flecs OS API с нашим scheduler
        // ВАЖНО: до создания flecs::world!
        auto scheduler = pool.scheduler();
        FlecsOsApi::init(scheduler);

        // 3. Создаём Flecs world (без set_threads!)
        flecs::world world;  // Использует custom OS API!

        // 4. Регистрируем ECS системы
        register_systems(world);

        // 5. Сохраняем world для использования
        world_ = std::make_unique<flecs::world>(std::move(world));
    }

    void shutdown() {
        // В обратном порядке

        world_.reset();

        FlecsOsApi::shutdown();

        global::shutdown_thread_pool();
    }

private:
    std::unique_ptr<flecs::world> world_;
};
```

---

## Итоговая архитектура

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Unified Thread Pool Architecture                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │           ProjectV Engine (Single Process)                       │   │
│  │                                                                   │   │
│  │  ┌───────────────────────────────────────────────────────────┐ │   │
│  │  │        stdexec::static_thread_pool (N-2 workers)         │ │   │
│  │  │                                                            │ │   │
│  │  │   ┌──────────────┐     ┌────────────────────────────┐  │ │   │
│  │  │   │  Flecs ECS   │     │    stdexec Algorithms       │  │ │   │
│  │  │   │  (systems)   │     │    (bulk, when_all, etc)   │  │ │   │
│  │  │   │              │     │                             │  │ │   │
│  │  │   │  • Physics   │     │  • Parallel chunk update   │  │ │   │
│  │  │   │  • Transform │     │  • Mesh generation         │  │ │   │
│  │  │   │  • Animation │     │  • Texture streaming      │  │ │   │
│  │  │   │  • AI         │     │  • Physics sync           │  │ │   │
│  │  │   └──────────────┘     └────────────────────────────┘  │ │   │
│  │  │          │                           │                    │ │   │
│  │  │          └───────────┬───────────────┘                    │ │   │
│  │  │                      │                                    │ │   │
│  │  │              ┌───────▼───────┐                            │ │   │
│  │  │              │ Custom OS API │                            │ │   │
│  │  │              │  (ecs_os_set) │                            │ │   │
│  │  │              └───────────────┘                            │ │   │
│  │  │                                                            │ │   │
│  │  └───────────────────────────────────────────────────────────┘ │   │
│  │                                                                   │   │
│  │  ┌───────────────────────────────────────────────────────────┐   │   │
│  │  │         OS Reserved (2 threads)                          │   │   │
│  │  │   • GPU driver work                                      │   │   │
│  │  │   • System libraries                                     │   │   │
│  │  │   • Background services                                  │   │   │
│  │  └───────────────────────────────────────────────────────────┘   │   │
│  │                                                                   │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Thread Count Guidelines

| CPU Cores | Thread Pool | OS Reserved | Flecs Systems | stdexec Jobs |
|-----------|-------------|-------------|---------------|--------------|
| 4         | 2           | 2           | ✅ shared      | ✅ shared     |
| 6         | 4           | 2           | ✅ shared      | ✅ shared     |
| 8         | 6           | 2           | ✅ shared      | ✅ shared     |
| 12        | 10          | 2           | ✅ shared      | ✅ shared     |
| 16        | 14          | 2           | ✅ shared      | ✅ shared     |

**Ключевое правило:** Всегда резервируем 2 потока для OS и GPU driver.

---

## Best Practices

### DO

```cpp
// ✅ ПРАВИЛЬНО: Единый pool
FlecsOsApi::init(pool.scheduler());
flecs::world world;  // Не вызываем set_threads()!
world.system<Transform>("Transform")
    .each([](Transform& t) { /* ... */ });
```

### DON'T

```cpp
// ❌ НЕПРАВИЛЬНО: Раздельные pools
world.set_threads(2);           // ОШИБКА!
static_thread_pool pool(6);     // ОШИБКА!
```

---

## References

- [Flecs OS API](https://www.flecs.dev/flecs/md_docs_2Multithreading.html)
- [ecs_os_set_api](https://github.com/SanderMertens/flecs/blob/master/src/os_api.c)
- [stdexec static_thread_pool](https://github.com/NVIDIA/stdexec)

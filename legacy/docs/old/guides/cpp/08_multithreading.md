# Многопоточность для игрового движка

**🔴 Уровень 3: Продвинутый** — Параллельное программирование в ProjectV для максимальной производительности.

Воксельный движок ProjectV обрабатывает миллионы вокселей, что требует эффективного использования всех ядер процессора.
Этот раздел охватывает современные подходы к многопоточности в C++20+ для игровой разработки.

> **Связь с философией:** Многопоточность — это продолжение DOD-мышления: данные независимы, поэтому их можно
> обрабатывать параллельно. См. [12_concurrency-philosophy.md](../../philosophy/12_concurrency-philosophy.md).

## 1. Основные концепции многопоточности

### Потоки (Threads) vs Асинхронность (Async)

- **`std::thread`**: Низкоуровневые потоки ОС. Полный контроль, но требует ручного управления.
- **`std::async`/`std::future`**: Высокоуровневая абстракция. Проще в использовании, но меньше контроля.

```cpp
// Создание потока для генерации чанка
std::thread chunkThread([&world, chunkPos]() {
    generateVoxelChunk(world, chunkPos);
});

// Асинхронная задача с возвратом результата
auto future = std::async(std::launch::async,  {
    return calculatePhysics();
});
```

### Атомики (Atomics)

Атомарные операции гарантируют корректность при одновременном доступе из нескольких потоков.

> **Для понимания:** Атомик — это операция, которая выполняется как единое целое. Либо она завершилась полностью, либо
> не начиналась. Нет промежуточного состояния, которое может увидеть другой поток. Под капотом процессор использует
> инструкции вроде `LOCK CMPXCHG` (x86), которые блокируют шину памяти на время операции.

```cpp
#include <atomic>

std::atomic<int> activeChunks{0};

// Потокобезопасный инкремент
void chunkLoaded() {
    activeChunks.fetch_add(1, std::memory_order_relaxed);
}

// Чтение без блокировок
int getActiveChunks() {
    return activeChunks.load(std::memory_order_acquire);
}
```

## 2. Модель памяти C++ (Memory Model)

Понимание порядка операций критично для многопоточности:

- **`memory_order_relaxed`**: Только атомарность, нет гарантий порядка.
- **`memory_order_acquire`**: Чтение, которое видит все записи до release.
- **`memory_order_release`**: Запись, которую увидят все последующие acquire.
- **`memory_order_seq_cst`**: Полная последовательная согласованность (по умолчанию, медленнее).

```cpp
// Producer-consumer паттерн
std::atomic<bool> dataReady{false};
std::vector<int> sharedData;

// Producer поток
void producer() {
    sharedData = generateData();
    dataReady.store(true, std::memory_order_release); // Release!
}

// Consumer поток
void consumer() {
    while (!dataReady.load(std::memory_order_acquire)) { // Acquire!
        std::this_thread::yield();
    }
    processData(sharedData);
}
```

## 3. Lock-free структуры данных

Lock-free алгоритмы избегают мьютексов, что критично для производительности в hot path.

### CAS (Compare-And-Swap) — Основа Lock-free

> **Метафора:** CAS — это как покупка билета в кассе. Вы говорите: "Если на этом месте ещё свободно, продайте мне
> билет". Если кто-то успел раньше — кассир говорит "уже занято", и вы пытаетесь снова. В коде:
`compare_exchange_weak(expected, desired)` — если текущее значение равно `expected`, заменяем на `desired`. Если нет — в
`expected` записывается актуальное значение, и вы решаете, что делать дальше.

```cpp
// CAS в действии: неблокирующий инкремент
std::atomic<int> counter{0};

void increment() {
    int expected = counter.load(std::memory_order_relaxed);
    // Пытаемся заменить expected на expected + 1
    // Если другой поток уже изменил counter, expected обновится
    while (!counter.compare_exchange_weak(expected, expected + 1,
                                         std::memory_order_release,
                                         std::memory_order_relaxed)) {
        // CAS failed, expected теперь содержит актуальное значение
        // Цикл повторится с новым expected
    }
}
```

> **Важно:** `compare_exchange_weak` может давать ложные сбои (spurious failure) на некоторых архитектурах (ARM, POWER).
> Это нормально — просто повторяйте попытку. `compare_exchange_strong` гарантирует успех только при реальном совпадении,
> но работает чуть медленнее.

### Lock-free очередь для задач

```cpp
template<typename T>
class LockFreeQueue {
    struct Node {
        T data;
        std::atomic<Node*> next{nullptr};
    };

    std::atomic<Node*> head{nullptr};
    std::atomic<Node*> tail{nullptr};

public:
    void push(T value) {
        Node* newNode = new Node{std::move(value)};
        Node* oldTail = tail.load(std::memory_order_relaxed);

        while (!tail.compare_exchange_weak(oldTail, newNode,
                                          std::memory_order_release,
                                          std::memory_order_relaxed)) {
            // CAS failed, retry
        }

        if (oldTail) {
            oldTail->next.store(newNode, std::memory_order_release);
        } else {
            head.store(newNode, std::memory_order_release);
        }
    }

    std::optional<T> pop() {
        Node* oldHead = head.load(std::memory_order_acquire);
        if (!oldHead) return std::nullopt;

        while (!head.compare_exchange_weak(oldHead, oldHead->next,
                                          std::memory_order_release,
                                          std::memory_order_relaxed)) {
            if (!oldHead) return std::nullopt;
        }

        T value = std::move(oldHead->data);
        delete oldHead;
        return value;
    }
};
```

## 4. Параллельные алгоритмы C++17/20

### `std::execution` политики

```cpp
#include <execution>
#include <algorithm>

// Параллельная сортировка вокселей по расстоянию
std::vector<Voxel> voxels = getVoxels();
std::sort(std::execution::par, voxels.begin(), voxels.end(),
           {
              return a.distance < b.distance;
          });

// Параллельный for_each для обработки чанков
std::for_each(std::execution::par_unseq, chunks.begin(), chunks.end(),
               {
                  chunk.updateLighting();
              });
```

### Параллельная редукция

```cpp
// Суммирование значений вокселей параллельно
std::vector<int> voxelValues = getVoxelValues();
int total = std::reduce(std::execution::par,
                       voxelValues.begin(), voxelValues.end(),
                       0, std::plus<>());
```

## 5. Worker pool для ProjectV

Пул потоков для эффективного распределения задач.

```cpp
class ThreadPool {
    std::vector<std::thread> workers;
    LockFreeQueue<std::function<void()>> tasks;
    std::atomic<bool> stop{false};

public:
    ThreadPool(size_t numThreads = std::thread::hardware_concurrency()) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this]() {
                while (!stop.load(std::memory_order_acquire)) {
                    auto task = tasks.pop();
                    if (task) {
                        (*task)();
                    } else {
                        std::this_thread::yield();
                    }
                }
            });
        }
    }

    template<typename F>
    auto submit(F&& func) -> std::future<decltype(func())> {
        using ReturnType = decltype(func());
        auto promise = std::make_shared<std::promise<ReturnType>>();

        tasks.push([func = std::forward<F>(func), promise]() {
            try {
                if constexpr (std::is_void_v<ReturnType>) {
                    func();
                    promise->set_value();
                } else {
                    promise->set_value(func());
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });

        return promise->get_future();
    }

    ~ThreadPool() {
        stop.store(true, std::memory_order_release);
        for (auto& worker : workers) {
            if (worker.joinable()) worker.join();
        }
    }
};
```

## 6. Многопоточная генерация чанков

Пример оптимизации генерации воксельного мира.

```cpp
class ChunkGenerator {
    ThreadPool pool;
    std::atomic<int> pendingChunks{0};

public:
    void generateRegion(const glm::ivec3& start, const glm::ivec3& end) {
        std::vector<std::future<Chunk>> futures;

        // Запускаем генерацию каждого чанка в отдельном потоке
        for (int x = start.x; x < end.x; ++x) {
            for (int y = start.y; y < end.y; ++y) {
                for (int z = start.z; z < end.z; ++z) {
                    pendingChunks.fetch_add(1, std::memory_order_relaxed);

                    futures.push_back(pool.submit([x, y, z, this]() {
                        Chunk chunk = generateSingleChunk(x, y, z);
                        pendingChunks.fetch_sub(1, std::memory_order_relaxed);
                        return chunk;
                    }));
                }
            }
        }

        // Собираем результаты
        for (auto& future : futures) {
            Chunk chunk = future.get();
            world.addChunk(chunk);
        }
    }

    int getPendingChunks() const {
        return pendingChunks.load(std::memory_order_acquire);
    }
};
```

## 7. Потокобезопасный ECS

Интеграция многопоточности с flecs.

```cpp
// Потокобезопасный доступ к компонентам
class ThreadSafeWorld {
    flecs::world world;
    mutable std::shared_mutex mutex;

public:
    template<typename T>
    void set(flecs::entity entity, T&& value) {
        std::unique_lock lock(mutex);
        entity.set<T>(std::forward<T>(value));
    }

    template<typename T>
    const T* get(flecs::entity entity) const {
        std::shared_lock lock(mutex);
        return entity.get<T>();
    }

    // Параллельная обработка систем
    template<typename... Components>
    void parallelEach(std::function<void(Components&...)> func) {
        std::shared_lock lock(mutex);

        auto view = world.view<Components...>();
        const size_t chunkSize = view.count() / std::thread::hardware_concurrency();

        std::vector<std::thread> threads;
        auto it = view.begin();

        for (size_t i = 0; i < std::thread::hardware_concurrency(); ++i) {
            auto start = it;
            std::advance(it, chunkSize);
            auto end = (i == std::thread::hardware_concurrency() - 1) ? view.end() : it;

            threads.emplace_back([start, end, &func]() {
                for (auto entity = start; entity != end; ++entity) {
                    func(entity.get<Components>()...);
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }
    }
};
```

## 8. Профилирование многопоточности

### Tracy для многопоточного профилирования

```cpp
#include <tracy/Tracy.hpp>

void processChunkParallel(Chunk& chunk) {
    ZoneScopedN("ProcessChunkParallel"); // Tracy zone

    std::vector<std::thread> threads;
    const size_t voxelsPerThread = chunk.voxels.size() / 4;

    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&chunk, i, voxelsPerThread]() {
            ZoneScopedN("ChunkThread"); // Имя потока в Tracy

            size_t start = i * voxelsPerThread;
            size_t end = (i == 3) ? chunk.voxels.size() : start + voxelsPerThread;

            for (size_t j = start; j < end; ++j) {
                processVoxel(chunk.voxels[j]);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}
```

## 9. Распространенные ошибки и решения

### Data race (состояние гонки)

**Проблема:** Несколько потоков одновременно изменяют одни данные.
**Решение:** Используйте мьютексы или атомики.

```cpp
// ПЛОХО: Data race
int counter = 0;
std::thread t1([&]() { ++counter; });
std::thread t2([&]() { ++counter; });

// ХОРОШО: Атомик
std::atomic<int> safeCounter{0};
std::thread t1([&]() { safeCounter.fetch_add(1); });
std::thread t2([&]() { safeCounter.fetch_add(1); });
```

### Deadlock (взаимная блокировка)

**Проблема:** Два потока ждут друг друга.
**Решение:** Всегда блокируйте мьютексы в одинаковом порядке.

```cpp
// ПЛОХО: Возможен deadlock
std::mutex m1, m2;
std::thread t1([&]() {
    std::lock_guard lock1(m1);
    std::lock_guard lock2(m2);
});
std::thread t2([&]() {
    std::lock_guard lock2(m2);  // Другой порядок!
    std::lock_guard lock1(m1);
});

// ХОРОШО: std::lock для нескольких мьютексов
std::thread t1([&]() {
    std::scoped_lock lock(m1, m2); // Блокирует оба атомарно
});
```

### False sharing (ложное разделение)

**Проблема:** Разные потоки работают с разными данными в одной cache line.

> **Метафора:** Представьте двух писателей за одним столом. Каждый пишет в свою тетрадь, но тетради лежат рядом. Когда
> один берёт ручку со стола, второй не может взять свою — они мешают друг другу, хотя пишут в разных местах. Cache
> line (
> 64 байта) — это "стол". Если данные разных потоков лежат в одной cache line, они конкурируют за него, хотя логически
> независимы.

**Решение:** Выравнивание данных по границам cache line.

```cpp
// ПЛОХО: Оба счётчика в одной cache line (64 байта)
struct Counters {
    std::atomic<int> counter1; // Поток 1
    std::atomic<int> counter2; // Поток 2
    // Оба в одной cache line — false sharing!
};

// ХОРОШО: Каждый счётчик в своей cache line
struct alignas(64) CacheAlignedCounter {
    std::atomic<int> value;
    char padding[64 - sizeof(std::atomic<int>)]; // Дополняем до 64 байт
};

CacheAlignedCounter counters[4]; // Каждый счётчик в своей cache line
```

> **Для понимания:** `alignas(64)` гарантирует, что структура начинается с границы 64 байт. Размер структуры тоже 64
> байта (значение + паддинг). Теперь каждый счётчик живёт в своём cache line, и потоки не мешают друг другу.

## 10. Best practices для ProjectV

1. **Используйте пул потоков** вместо создания потоков на каждую задачу.
2. **Минимизируйте блокировки** в hot path (физика, рендеринг).
3. **Профилируйте scaling** — добавление потоков должно ускорять работу.
4. **Тестируйте на разном железе** — разное количество ядер требует разной настройки.
5. **Используйте `std::atomic` с правильным memory order**.
6. **Избегайте блокировок в деструкторах**.
7. **Используйте lock-free структуры** для высокочастотных операций.


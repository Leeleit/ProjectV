# Паттерны использования fastgltf в ProjectV

🔴 **Уровень 3: Продвинутый**

Практические рецепты для типичных задач в воксельном движке.

## Обзор паттернов

| Паттерн                  | Когда использовать               | Ключевое преимущество       |
|--------------------------|----------------------------------|-----------------------------|
| **Асинхронная загрузка** | Большие модели, множество файлов | Не блокирует основной поток |
| **Кэширование моделей**  | Переиспользование ресурсов       | Минимизация загрузок        |
| **Streaming загрузка**   | Open World, большие сцены        | Экономия памяти             |
| **Пул парсеров**         | Многопоточная загрузка           | Параллелизм                 |

---

## Паттерн 1: Асинхронная загрузка моделей

### Задача

Загрузка больших glTF моделей без блокировки рендеринга.

### Решение

```cpp
#include <future>
#include <mutex>
#include <queue>

class AsyncModelLoader {
public:
    struct LoadResult {
        std::string modelName;
        std::shared_ptr<fastgltf::Asset> asset;
        std::vector<VulkanMeshData> meshes;
        bool success = false;
    };

    AsyncModelLoader(size_t threadCount = 2) {
        for (size_t i = 0; i < threadCount; ++i) {
            workers.emplace_back(&AsyncModelLoader::workerThread, this);
        }
    }

    ~AsyncModelLoader() {
        {
            std::unique_lock lock(queueMutex);
            shutdown = true;
        }
        cv.notify_all();
        for (auto& t : workers) t.join();
    }

    std::future<LoadResult> loadAsync(const std::filesystem::path& path) {
        auto promise = std::make_shared<std::promise<LoadResult>>();
        auto future = promise->get_future();

        {
            std::unique_lock lock(queueMutex);
            taskQueue.emplace(Task{path, promise});
        }
        cv.notify_one();

        return future;
    }

private:
    struct Task {
        std::filesystem::path path;
        std::shared_ptr<std::promise<LoadResult>> promise;
    };

    void workerThread() {
        thread_local fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_basisu);

        while (true) {
            Task task;
            {
                std::unique_lock lock(queueMutex);
                cv.wait(lock, [this] { return shutdown || !taskQueue.empty(); });

                if (shutdown) return;

                task = std::move(taskQueue.front());
                taskQueue.pop();
            }

            LoadResult result;
            result.modelName = task.path.stem().string();

            auto data = fastgltf::GltfDataBuffer::FromPath(task.path);
            if (data.error() != fastgltf::Error::None) {
                task.promise->set_value(result);
                continue;
            }

            auto asset = parser.loadGltf(
                data.get(),
                task.path.parent_path(),
                fastgltf::Options::LoadExternalBuffers,
                fastgltf::Category::OnlyRenderable
            );

            if (asset.error() == fastgltf::Error::None) {
                result.asset = std::make_shared<fastgltf::Asset>(std::move(asset.get()));
                result.success = true;
            }

            task.promise->set_value(std::move(result));
        }
    }

    std::vector<std::thread> workers;
    std::queue<Task> taskQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    bool shutdown = false;
};
```

### Использование

```cpp
AsyncModelLoader loader(2);

auto future = loader.loadAsync("models/character.glb");

// Продолжаем рендеринг...

if (future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
    auto result = future.get();
    if (result.success) {
        // Создаём GPU ресурсы в основном потоке
        createGpuResources(result.asset);
    }
}
```

---

## Паттерн 2: Кэширование моделей

### Задача

Избежать повторной загрузки одинаковых моделей.

### Решение

```cpp
#include <unordered_map>
#include <memory>
#include <shared_mutex>

class ModelCache {
public:
    struct CachedModel {
        std::shared_ptr<fastgltf::Asset> asset;
        VulkanMeshData meshData;
        std::chrono::steady_clock::time_point lastAccess;
    };

    std::shared_ptr<fastgltf::Asset> getOrLoad(
        const std::filesystem::path& path,
        fastgltf::Parser& parser) {

        std::string key = path.string();

        // Сначала пытаемся найти в кэше (shared lock)
        {
            std::shared_lock lock(cacheMutex);
            auto it = cache.find(key);
            if (it != cache.end()) {
                it->second.lastAccess = std::chrono::steady_clock::now();
                return it->second.asset;
            }
        }

        // Загружаем (без блокировки кэша)
        auto data = fastgltf::GltfDataBuffer::FromPath(path);
        if (data.error() != fastgltf::Error::None) return nullptr;

        auto asset = parser.loadGltf(
            data.get(),
            path.parent_path(),
            fastgltf::Options::LoadExternalBuffers
        );

        if (asset.error() != fastgltf::Error::None) return nullptr;

        auto sharedAsset = std::make_shared<fastgltf::Asset>(std::move(asset.get()));

        // Добавляем в кэш (unique lock)
        {
            std::unique_lock lock(cacheMutex);
            cache[key] = CachedModel{
                .asset = sharedAsset,
                .lastAccess = std::chrono::steady_clock::now()
            };
        }

        return sharedAsset;
    }

    void evictUnused(std::chrono::seconds maxAge = std::chrono::seconds(300)) {
        auto now = std::chrono::steady_clock::now();

        std::unique_lock lock(cacheMutex);
        for (auto it = cache.begin(); it != cache.end(); ) {
            auto age = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second.lastAccess);
            if (age > maxAge && it->second.asset.use_count() == 1) {
                it = cache.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    std::unordered_map<std::string, CachedModel> cache;
    mutable std::shared_mutex cacheMutex;
};
```

---

## Паттерн 3: Streaming загрузка для Open World

### Задача

Загрузка только видимой части большого мира.

### Решение

```cpp
#include <fastgltf/GltfFileStream.hpp>

class StreamingWorldLoader {
public:
    struct Chunk {
        glm::ivec3 coord;
        std::shared_ptr<fastgltf::Asset> asset;
        bool loaded = false;
        bool priority = false;
    };

    void updatePlayerPosition(const glm::vec3& position) {
        glm::ivec3 currentChunk = worldToChunk(position);

        // Определяем какие чанки нужны
        std::vector<glm::ivec3> neededChunks;
        for (int dz = -loadRadius; dz <= loadRadius; ++dz) {
            for (int dy = -loadRadius; dy <= loadRadius; ++dy) {
                for (int dx = -loadRadius; dx <= loadRadius; ++dx) {
                    neededChunks.push_back(currentChunk + glm::ivec3(dx, dy, dz));
                }
            }
        }

        // Загружаем новые чанки
        for (const auto& coord : neededChunks) {
            std::string key = chunkToKey(coord);
            if (loadedChunks.find(key) == loadedChunks.end()) {
                requestChunkLoad(coord);
            }
        }

        // Выгружаем далёкие чанки
        std::erase_if(loadedChunks, [&](const auto& pair) {
            auto coord = keyToChunk(pair.first);
            float distance = glm::distance(glm::vec3(coord), glm::vec3(currentChunk));
            return distance > unloadRadius;
        });
    }

private:
    void requestChunkLoad(const glm::ivec3& coord) {
        std::string key = chunkToKey(coord);
        std::filesystem::path path = chunkToPath(coord);

        if (!std::filesystem::exists(path)) return;

        // Потоковая загрузка через GltfFileStream
        fastgltf::Parser parser;
        auto stream = fastgltf::GltfFileStream(path);

        auto asset = parser.loadGltf(
            stream,
            path.parent_path(),
            fastgltf::Options::None,  // Без LoadExternalBuffers для streaming
            fastgltf::Category::OnlyRenderable
        );

        if (asset.error() == fastgltf::Error::None) {
            loadedChunks[key] = std::make_shared<fastgltf::Asset>(std::move(asset.get()));
        }
    }

    glm::ivec3 worldToChunk(const glm::vec3& pos) const {
        return glm::ivec3(
            static_cast<int>(std::floor(pos.x / chunkSize)),
            static_cast<int>(std::floor(pos.y / chunkSize)),
            static_cast<int>(std::floor(pos.z / chunkSize))
        );
    }

    std::string chunkToKey(const glm::ivec3& coord) const {
        return std::format("chunk_{}_{}_{}", coord.x, coord.y, coord.z);
    }

    std::filesystem::path chunkToPath(const glm::ivec3& coord) const {
        return worldPath / std::format("chunk_{}_{}_{}.glb", coord.x, coord.y, coord.z);
    }

    std::unordered_map<std::string, std::shared_ptr<fastgltf::Asset>> loadedChunks;
    std::filesystem::path worldPath = "world";
    float chunkSize = 16.0f;
    int loadRadius = 2;
    int unloadRadius = 4;
};
```

---

## Паттерн 4: Пул парсеров для многопоточности

### Задача

Parser не потокобезопасен, но нужен параллелизм.

### Решение

```cpp
class ParserPool {
public:
    explicit ParserPool(size_t count,
                        fastgltf::Extensions extensions = fastgltf::Extensions::None) {
        for (size_t i = 0; i < count; ++i) {
            pool.emplace_back(std::make_unique<fastgltf::Parser>(extensions));
            available.push(i);
        }
    }

    class ParserHandle {
    public:
        ParserHandle(fastgltf::Parser* p, std::mutex& m, size_t idx, ParserPool* pool)
            : parser(p), mutex(&m), index(idx), owner(pool) {}

        ~ParserHandle() {
            if (owner) {
                owner->returnParser(index);
            }
        }

        fastgltf::Parser* operator->() { return parser; }
        fastgltf::Parser& operator*() { return *parser; }

    private:
        fastgltf::Parser* parser;
        std::mutex* mutex;
        size_t index;
        ParserPool* owner;
    };

    ParserHandle acquire() {
        std::unique_lock lock(poolMutex);
        cv.wait(lock, [this] { return !available.empty(); });

        size_t idx = available.front();
        available.pop();

        return ParserHandle(pool[idx].get(), parserMutexes[idx], idx, this);
    }

private:
    void returnParser(size_t idx) {
        std::unique_lock lock(poolMutex);
        available.push(idx);
        cv.notify_one();
    }

    std::vector<std::unique_ptr<fastgltf::Parser>> pool;
    std::queue<size_t> available;
    std::mutex poolMutex;
    std::vector<std::mutex> parserMutexes;
    std::condition_variable cv;
};
```

### Использование

```cpp
ParserPool pool(4, fastgltf::Extensions::KHR_texture_basisu);

std::vector<std::future<void>> futures;

for (const auto& file : modelFiles) {
    futures.push_back(std::async(std::launch::async, [&pool, file] {
        auto parser = pool.acquire();

        auto data = fastgltf::GltfDataBuffer::FromPath(file);
        auto asset = parser->loadGltf(data.get(), file.parent_path(),
            fastgltf::Options::LoadExternalBuffers);

        // Обработка...
    }));
}

for (auto& f : futures) f.wait();
```

---

## Паттерн 5: Progressive Loading с приоритетами

### Задача

Загрузка LOD0 сначала, затем более высоких LOD по запросу.

### Решение

```cpp
class ProgressiveModelLoader {
public:
    enum class LodLevel {
        LOD0 = 0,  // Низкая детализация
        LOD1 = 1,  // Средняя
        LOD2 = 2   // Высокая
    };

    struct LodModel {
        std::filesystem::path path;
        std::shared_ptr<fastgltf::Asset> asset;
        bool loaded = false;
        int priority = 0;
    };

    void loadModel(const std::string& modelId,
                   const std::filesystem::path& basePath) {
        // Загружаем LOD0 синхронно
        auto lod0Path = basePath / "lod0.glb";
        auto parser = acquireParser();

        auto data = fastgltf::GltfDataBuffer::FromPath(lod0Path);
        auto asset = parser->loadGltf(data.get(), lod0Path.parent_path(),
            fastgltf::Options::LoadExternalBuffers,
            fastgltf::Category::OnlyRenderable);

        models[modelId][LodLevel::LOD0] = {
            .path = lod0Path,
            .asset = std::make_shared<fastgltf::Asset>(std::move(asset.get())),
            .loaded = true
        };

        // Ставим в очередь более высокие LOD
        for (int lod = 1; lod <= 2; ++lod) {
            auto lodPath = basePath / std::format("lod{}.glb", lod);
            if (std::filesystem::exists(lodPath)) {
                loadQueue.push({
                    .modelId = modelId,
                    .lod = static_cast<LodLevel>(lod),
                    .path = lodPath
                });
            }
        }
    }

    void requestHigherLod(const std::string& modelId, LodLevel lod) {
        // Увеличиваем приоритет загрузки
        std::unique_lock lock(queueMutex);
        for (auto& task : loadQueue) {
            if (task.modelId == modelId && task.lod == lod) {
                task.priority = 10;  // Высокий приоритет
                break;
            }
        }
        cv.notify_one();
    }

private:
    struct LoadTask {
        std::string modelId;
        LodLevel lod;
        std::filesystem::path path;
        int priority = 0;
    };

    std::map<std::string, std::map<LodLevel, LodModel>> models;
    std::priority_queue<LoadTask> loadQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
};
```

---

## Резюме

| Паттерн              | Сложность | Память      | Параллелизм |
|----------------------|-----------|-------------|-------------|
| Асинхронная загрузка | Средняя   | Низкая      | Высокий     |
| Кэширование моделей  | Низкая    | Высокая     | Низкий      |
| Streaming загрузка   | Высокая   | Оптимальная | Средний     |
| Пул парсеров         | Низкая    | Низкая      | Высокий     |
| Progressive Loading  | Высокая   | Оптимальная | Средний     |

Выбирайте паттерн в зависимости от требований проекта к производительности и сложности реализации.

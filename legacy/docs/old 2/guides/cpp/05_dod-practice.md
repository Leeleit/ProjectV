# Практика Data-Oriented Design в ProjectV

**Уровень: Продвинутый** — Data-Oriented Design паттерны для высокопроизводительной обработки воксельных данных.

Воксельный движок ProjectV обрабатывает миллионы вокселей, что требует максимальной производительности. Data-Oriented
Design (DOD) — это архитектурный подход, ориентированный на эффективное использование кэша процессора и параллельную
обработку данных.

> **Связь с философией:** Этот документ — практическое
> продолжение [03_dod-philosophy.md](../../philosophy/03_dod-philosophy.md). Там объясняется "почему", здесь — "как".
> Если
> вы не читали философию DOD, начните с неё.

---

## Содержание

1. [Почему ООП не работает для вокселей](#1-почему-ооп-не-работает-для-вокселей)
2. [Structure of Arrays (SoA) vs Array of Structures (AoS)](#2-structure-of-arrays-soa-vs-array-of-structures-aos)
3. [Cache-friendly итерация](#3-cache-friendly-итерация)
4. [Hot/Cold Data Splitting](#4-hotcold-data-splitting)
5. [Пакетная обработка (Batching)](#5-пакетная-обработка-batching)
6. [Жадный мешинг (Greedy Meshing)](#6-жадный-мешинг-greedy-meshing)
7. [Пространственное разделение](#7-пространственное-разделение)
8. [SIMD оптимизации](#8-simd-оптимизации)
9. [Потокобезопасные структуры](#9-потокобезопасные-структуры)
10. [GPU-friendly данные](#10-gpu-friendly-данные)

---

## 1. Почему ООП не работает для вокселей

### Проблема ООП в GameDev

В классическом ООП мы создаём иерархии: `BaseObject -> Actor -> Player`. Это создаёт несколько фундаментальных проблем
для игровых движков:

```cpp
// ❌ ТАК МЫ БОЛЬШЕ НЕ ДЕЛАЕМ (OOP)
class Entity {
public:
    virtual void update(float dt) = 0;
    virtual void render() = 0;
    virtual ~Entity() = default;
};

class PhysicsEntity : public Entity {
    void update(float dt) override { /* ... */ }
    void render() override { /* ... */ }
};

class Player : public PhysicsEntity {
    void update(float dt) override { /* ... */ }
    void render() override { /* ... */ }
};
```

**Проблемы ООП в ProjectV:**

1. **Cache Misses**: Объекты разбросаны по памяти (heap allocation). Процессор тратит время на ожидание данных из RAM.
2. **Virtual Table (vtable)**: Вызов виртуальной функции — это "прыжок" в неизвестность для процессора (Indirect Jump).
   Это сбивает предсказатель переходов (branch predictor).
3. **Diamond Problem**: Сложные иерархии ломаются при попытке добавить новые свойства.
4. **Жёсткая архитектура**: Невозможно динамически менять поведение объектов во время выполнения.

### Решение: DOD (Data-Oriented Design)

Мы разделяем данные и логику. Данные лежат в памяти плотными массивами. Процессор читает их линейно (Prefetching), что
в десятки раз быстрее.

```cpp
// ✅ ТАК МЫ ДЕЛАЕМ В PROJECTV (DOD)
// Компоненты - чистые данные
struct Transform {
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale = glm::vec3(1.0f);
};

struct Velocity {
    glm::vec3 linear;
    glm::vec3 angular;
};

struct Health {
    float current;
    float max;
};

// Системы - чистая логика
void updateMovementSystem(flecs::world& world, float dt) {
    world.each([dt](Transform& transform, const Velocity& velocity) {
        transform.position += velocity.linear * dt;
        transform.rotation = glm::rotate(transform.rotation,
                                        velocity.angular.length() * dt,
                                        glm::normalize(velocity.angular));
    });
}
```

---

## 2. Structure of Arrays (SoA) vs Array of Structures (AoS)

### Проблема: Array of Structures (AoS)

```cpp
// ТРАДИЦИОННЫЙ ПОДХОД (медленно)
struct Voxel {
    uint8_t type;
    glm::vec3 position;
    uint8_t lightLevel;
    float temperature;
    // ... другие поля
};

std::vector<Voxel> voxels; // Все данные перемешаны
```

При обработке только типа вокселя процессор загружает в кэш все поля структуры, даже те, которые не нужны. Это приводит
к cache thrashing.

### Решение: Structure of Arrays (SoA)

```cpp
// DOD ПОДХОД (быстро)
struct VoxelChunk {
    // Каждое поле в отдельном массиве
    std::vector<uint8_t> types;
    std::vector<glm::vec3> positions;
    std::vector<uint8_t> lightLevels;
    std::vector<float> temperatures;

    size_t count{0};

    // Добавление вокселя
    void addVoxel(uint8_t type, glm::vec3 pos, uint8_t light, float temp) {
        types.push_back(type);
        positions.push_back(pos);
        lightLevels.push_back(light);
        temperatures.push_back(temp);
        ++count;
    }

    // Обработка только типов (cache-friendly)
    void updateTypes() {
        for (size_t i = 0; i < count; ++i) {
            if (types[i] == VOXEL_WATER) {
                types[i] = VOXEL_ICE; // Только нужные данные в кэше
            }
        }
    }
};
```

### Сравнение производительности

| Операция             | AoS  | SoA  | Ускорение |
|----------------------|------|------|-----------|
| Итерация по типам    | 100% | 25%  | 4x        |
| Итерация по позициям | 100% | 33%  | 3x        |
| Обновление освещения | 100% | 20%  | 5x        |
| Сериализация         | 100% | 110% | 0.9x      |

**Вывод:** SoA выигрывает при выборочной обработке полей. AoS может быть лучше при работе со всеми полями одновременно.

---

## 3. Cache-friendly итерация

### Предвычисление индексов

```cpp
class VoxelChunk {
    std::vector<uint8_t> types;
    std::vector<glm::vec3> positions;

    // Индексы для быстрого доступа
    std::vector<size_t> solidVoxels;    // Индексы твердых вокселей
    std::vector<size_t> liquidVoxels;   // Индексы жидких вокселей
    std::vector<size_t> transparentVoxels; // Индексы прозрачных вокселей

    void rebuildIndices() {
        solidVoxels.clear();
        liquidVoxels.clear();
        transparentVoxels.clear();

        for (size_t i = 0; i < types.size(); ++i) {
            if (isSolid(types[i])) {
                solidVoxels.push_back(i);
            } else if (isLiquid(types[i])) {
                liquidVoxels.push_back(i);
            } else if (isTransparent(types[i])) {
                transparentVoxels.push_back(i);
            }
        }
    }

    // Быстрая обработка только твердых вокселей
    void processSolidVoxels() {
        for (size_t idx : solidVoxels) {
            processSolidVoxel(types[idx], positions[idx]);
        }
    }
};
```

### Линейный доступ vs Случайный

```cpp
// ❌ Плохо: случайный доступ (cache thrashing)
for (int i = 0; i < indices.size(); ++i) {
    process(voxels[indices[random(i)]]);
}

// ✅ Хорошо: линейный доступ (cache prefetching)
for (size_t i = 0; i < voxels.size(); ++i) {
    process(voxels[i]);
}

// ✅ Ещё лучше: отсортированные индексы
std::sort(indices.begin(), indices.end());
for (size_t idx : indices) {
    process(voxels[idx]); // Предсказуемый доступ
}
```

---

## 4. Hot/Cold Data Splitting

Разделяем часто используемые (hot) и редко используемые (cold) данные.

```cpp
struct VoxelData {
    // Hot данные (используются каждый кадр)
    std::vector<uint8_t> types;
    std::vector<glm::vec3> positions;
    std::vector<uint8_t> lightLevels;

    // Cold данные (используются редко)
    struct ColdData {
        float creationTime;
        uint32_t ownerId;
        std::string metadata;
    };
    std::vector<ColdData> cold;

    // Методы работают только с hot данными
    void render() {
        for (size_t i = 0; i < types.size(); ++i) {
            if (types[i] != VOXEL_AIR && lightLevels[i] > 0) {
                renderVoxel(positions[i], types[i]);
            }
        }
    }
};
```

### Преимущества

1. **Кэш не засоряется** редко используемыми данными
2. **Память используется эффективнее** — cold данные можно выгружать
3. **Предсказуемая производительность** — нет сюрпризов с cache misses

---

## 5. Пакетная обработка (Batching)

### Пакетные операции над массивами

```cpp
class VoxelBatchProcessor {
    struct Batch {
        size_t start;
        size_t end;
        uint8_t operation;
    };

    std::vector<Batch> batches;

public:
    // Добавление пакетной операции
    void addBatch(size_t start, size_t end, uint8_t op) {
        batches.push_back({start, end, op});
    }

    // Выполнение всех пакетов
    void execute(VoxelChunk& chunk) {
        for (const auto& batch : batches) {
            switch (batch.operation) {
                case BATCH_UPDATE_LIGHT:
                    updateLightBatch(chunk, batch.start, batch.end);
                    break;
                case BATCH_UPDATE_PHYSICS:
                    updatePhysicsBatch(chunk, batch.start, batch.end);
                    break;
                case BATCH_GENERATE_MESH:
                    generateMeshBatch(chunk, batch.start, batch.end);
                    break;
            }
        }
        batches.clear();
    }

private:
    void updateLightBatch(VoxelChunk& chunk, size_t start, size_t end) {
        // SIMD-friendly цикл
        for (size_t i = start; i < end; i += 4) {
            // Обработка 4 вокселей за итерацию
            simd::processLight(chunk.lightLevels.data() + i,
                              chunk.types.data() + i,
                              chunk.positions.data() + i);
        }
    }
};
```

---

## 6. Жадный мешинг (Greedy Meshing)

Оптимизация рендеринга через объединение соседних вокселей.

```cpp
struct GreedyMesher {
    struct Face {
        glm::ivec3 start;
        glm::ivec3 size;
        uint8_t type;
        glm::vec3 normal;
    };

    std::vector<Face> greedyMesh(const VoxelChunk& chunk) {
        std::vector<Face> faces;

        // Проход по всем 6 направлениям
        for (int axis = 0; axis < 3; ++axis) {
            for (int direction = -1; direction <= 1; direction += 2) {
                extractFaces(chunk, axis, direction, faces);
            }
        }

        return faces;
    }

private:
    void extractFaces(const VoxelChunk& chunk, int axis, int direction,
                     std::vector<Face>& faces) {
        const int width = (axis == 0) ? CHUNK_SIZE_Y : CHUNK_SIZE_X;
        const int height = (axis == 2) ? CHUNK_SIZE_Y : CHUNK_SIZE_Z;

        // Массив для отслеживания обработанных вокселей
        std::vector<bool> processed(width * height, false);

        for (int x = 0; x < width; ++x) {
            for (int y = 0; y < height; ++y) {
                if (processed[x * height + y]) continue;

                uint8_t type = getVoxelType(chunk, axis, direction, x, y);
                if (type == VOXEL_AIR) continue;

                // Находим максимальный прямоугольник одинаковых вокселей
                int rectWidth = 1;
                int rectHeight = 1;

                // Расширяем по ширине
                while (x + rectWidth < width &&
                       !processed[(x + rectWidth) * height + y] &&
                       getVoxelType(chunk, axis, direction, x + rectWidth, y) == type) {
                    ++rectWidth;
                }

                // Расширяем по высоте
                bool canExpandHeight = true;
                while (canExpandHeight && y + rectHeight < height) {
                    for (int dx = 0; dx < rectWidth; ++dx) {
                        if (processed[(x + dx) * height + (y + rectHeight)] ||
                            getVoxelType(chunk, axis, direction, x + dx, y + rectHeight) != type) {
                            canExpandHeight = false;
                            break;
                        }
                    }
                    if (canExpandHeight) ++rectHeight;
                }

                // Помечаем как обработанные
                for (int dx = 0; dx < rectWidth; ++dx) {
                    for (int dy = 0; dy < rectHeight; ++dy) {
                        processed[(x + dx) * height + (y + dy)] = true;
                    }
                }

                // Добавляем грань
                faces.push_back(createFace(axis, direction, x, y, rectWidth, rectHeight, type));
            }
        }
    }
};
```

---

## 7. Пространственное разделение

### Octree для воксельных данных

```cpp
class VoxelOctree {
    struct Node {
        glm::vec3 min;
        glm::vec3 max;
        bool isLeaf{true};
        uint8_t uniformType{VOXEL_AIR}; // Если все воксели одинаковые
        std::unique_ptr<Node> children[8];
        std::vector<size_t> voxelIndices; // Только для leaf nodes
    };

    std::unique_ptr<Node> root;
    VoxelChunk& chunk;

public:
    VoxelOctree(VoxelChunk& chunk, int maxDepth = 6) : chunk(chunk) {
        root = buildOctree(glm::vec3(0), glm::vec3(CHUNK_SIZE), 0, maxDepth);
    }

    // Быстрый поиск вокселей в области
    std::vector<size_t> queryRange(const glm::vec3& min, const glm::vec3& max) {
        std::vector<size_t> result;
        queryRangeRecursive(root.get(), min, max, result);
        return result;
    }

    // Обновление октодерева
    void updateVoxel(size_t index) {
        const auto& pos = chunk.positions[index];
        updateVoxelRecursive(root.get(), pos, index);
    }

private:
    std::unique_ptr<Node> buildOctree(const glm::vec3& min, const glm::vec3& max,
                                     int depth, int maxDepth) {
        auto node = std::make_unique<Node>();
        node->min = min;
        node->max = max;

        if (depth == maxDepth) {
            // Leaf node - собираем индексы вокселей в этой области
            for (size_t i = 0; i < chunk.count; ++i) {
                if (contains(min, max, chunk.positions[i])) {
                    node->voxelIndices.push_back(i);
                }
            }

            // Проверяем, все ли воксели одинаковые
            if (!node->voxelIndices.empty()) {
                uint8_t firstType = chunk.types[node->voxelIndices[0]];
                bool uniform = true;
                for (size_t idx : node->voxelIndices) {
                    if (chunk.types[idx] != firstType) {
                        uniform = false;
                        break;
                    }
                }
                if (uniform) {
                    node->uniformType = firstType;
                    node->voxelIndices.clear(); // Экономим память
                }
            }
        } else {
            // Internal node - рекурсивно строим детей
            node->isLeaf = false;
            glm::vec3 center = (min + max) * 0.5f;

            for (int i = 0; i < 8; ++i) {
                glm::vec3 childMin, childMax;
                calculateChildBounds(i, min, center, max, childMin, childMax);
                node->children[i] = buildOctree(childMin, childMax, depth + 1, maxDepth);
            }
        }

        return node;
    }
};
```

### Spatial Grid для ECS

```cpp
class SpatialGrid {
    std::vector<std::vector<flecs::entity>> grid_;
    int cellSize_;

public:
    void updateEntities(const std::vector<flecs::entity>& entities) {
        clearGrid();

        for (auto entity : entities) {
            if (auto transform = entity.get<Transform>()) {
                int cellX = static_cast<int>(transform->position.x) / cellSize_;
                int cellY = static_cast<int>(transform->position.y) / cellSize_;
                int cellZ = static_cast<int>(transform->position.z) / cellSize_;

                int index = getGridIndex(cellX, cellY, cellZ);
                grid_[index].push_back(entity);
            }
        }
    }

    std::vector<flecs::entity> getEntitiesInRange(const glm::vec3& center, float radius) const {
        std::vector<flecs::entity> result;
        int minX = static_cast<int>(center.x - radius) / cellSize_;
        int maxX = static_cast<int>(center.x + radius) / cellSize_;
        // ... аналогично для Y и Z

        for (int x = minX; x <= maxX; ++x) {
            for (int y = minY; y <= maxY; ++y) {
                for (int z = minZ; z <= maxZ; ++z) {
                    int index = getGridIndex(x, y, z);
                    result.insert(result.end(), grid_[index].begin(), grid_[index].end());
                }
            }
        }
        return result;
    }
};
```

---

## 8. SIMD оптимизации

### Векторизованная обработка вокселей

```cpp
#include <immintrin.h> // AVX/AVX2

class SIMDVoxelProcessor {
public:
    // Обработка 8 вокселей одновременно (AVX)
    void processLightingAVX(VoxelChunk& chunk) {
        const size_t count = chunk.count;
        const size_t alignedCount = count - (count % 8);

        for (size_t i = 0; i < alignedCount; i += 8) {
            // Загружаем 8 light levels
            __m256i lights = _mm256_loadu_si256(
                reinterpret_cast<const __m256i*>(&chunk.lightLevels[i]));

            // Загружаем 8 типов (нужно расширить до 32-bit для операций)
            __m256i types = _mm256_cvtepu8_epi32(
                _mm_loadu_si128(reinterpret_cast<const __m128i*>(&chunk.types[i])));

            // Маска для AIR вокселей
            __m256i airMask = _mm256_cmpeq_epi32(types, _mm256_set1_epi32(VOXEL_AIR));

            // Обнуляем light для AIR вокселей
            __m256i result = _mm256_andnot_si256(airMask, lights);

            // Уменьшаем light на 1 (но не ниже 0)
            __m256i decremented = _mm256_sub_epi32(result, _mm256_set1_epi32(1));
            result = _mm256_max_epi32(decremented, _mm256_setzero_si256());

            // Сохраняем результат (конвертируем обратно в 8-bit)
            __m128i result8 = _mm256_cvtepi32_epi8(result);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&chunk.lightLevels[i]), result8);
        }

        // Обработка оставшихся вокселей скалярно
        for (size_t i = alignedCount; i < count; ++i) {
            if (chunk.types[i] != VOXEL_AIR && chunk.lightLevels[i] > 0) {
                --chunk.lightLevels[i];
            }
        }
    }
};
```

---

## 9. Потокобезопасные структуры

### Lock-free воксельные операции

```cpp
class ConcurrentVoxelChunk {
    struct alignas(64) CacheLineAligned {
        std::atomic<uint32_t> version{0};
        std::array<std::atomic<uint8_t>, 1024> types;
        char padding[64 - sizeof(std::atomic<uint32_t>) -
                    sizeof(std::array<std::atomic<uint8_t>, 1024>)];
    };

    std::vector<CacheLineAligned> data;

public:
    ConcurrentVoxelChunk(size_t size) : data((size + 1023) / 1024) {}

    // Lock-free чтение с versioning
    std::pair<uint8_t, uint32_t> read(size_t index) {
        size_t block = index / 1024;
        size_t offset = index % 1024;

        uint32_t v1, v2;
        uint8_t value;

        do {
            v1 = data[block].version.load(std::memory_order_acquire);
            value = data[block].types[offset].load(std::memory_order_relaxed);
            v2 = data[block].version.load(std::memory_order_acquire);
        } while (v1 != v2); // Повторяем если была concurrent модификация

        return {value, v1};
    }

    // Lock-free запись
    bool write(size_t index, uint8_t newValue, uint32_t expectedVersion) {
        size_t block = index / 1024;
        size_t offset = index % 1024;

        // CAS на version
        uint32_t expected = expectedVersion;
        uint32_t desired = expectedVersion + 1;

        if (data[block].version.compare_exchange_weak(expected, desired,
                                                     std::memory_order_release,
                                                     std::memory_order_relaxed)) {
            data[block].types[offset].store(newValue, std::memory_order_release);
            return true;
        }

        return false; // Concurrent modification detected
    }
};
```

---

## 10. GPU-friendly данные

### Оптимизация для Vulkan

```cpp
// GPU-friendly структура данных для вокселей
struct GPUVoxelData {
    // SoA для шейдеров
    std::vector<glm::vec4> positions;      // x,y,z,type
    std::vector<glm::vec4> normals;        // nx,ny,nz,material
    std::vector<glm::vec4> colors;         // r,g,b,a

    // Буферы для Vulkan
    VkBuffer positionBuffer;
    VkBuffer normalBuffer;
    VkBuffer colorBuffer;

    void uploadToGPU(VkDevice device, VmaAllocator allocator) {
        createBuffer(device, allocator, positions, &positionBuffer);
        createBuffer(device, allocator, normals, &normalBuffer);
        createBuffer(device, allocator, colors, &colorBuffer);
    }

    // Пакетное обновление для минимизации вызовов Vulkan
    void updateBatch(const std::vector<VoxelUpdate>& updates) {
        std::vector<glm::vec4> positionUpdates;
        std::vector<glm::vec4> normalUpdates;
        std::vector<glm::vec4> colorUpdates;

        for (const auto& update : updates) {
            positionUpdates.push_back(update.position);
            normalUpdates.push_back(update.normal);
            colorUpdates.push_back(update.color);
        }

        updateBuffer(positionBuffer, positionUpdates);
        updateBuffer(normalBuffer, normalUpdates);
        updateBuffer(colorBuffer, colorUpdates);
    }
};
```

---

## Распространённые ошибки

| Ошибка                            | Проблема               | Решение                             |
|-----------------------------------|------------------------|-------------------------------------|
| Виртуальные методы в компонентах  | Cache miss, нет inline | Компоненты — только данные          |
| AoS для независимой обработки     | Cache thrashing        | SoA для выборочных операций         |
| Случайный доступ в горячих циклах | Cache misses           | Линейный или отсортированный доступ |
| Преждевременная оптимизация       | Сложность без пользы   | Сначала измеряй (Tracy)             |
| Попытка сделать всё через DOD     | Overengineering        | ООП для high-level, DOD для данных  |


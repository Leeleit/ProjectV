# RmlUi: Хардкорные оптимизации

> **Для понимания:** Этот раздел — для тех, кто выжал из RmlUi всё по умолчанию. Мы говорим о микросекундах: кэш-линии,
> zero-copy, batch rendering, Job System. Железо не врёт — данные лежат в памяти, а процессор их читает. Оптимизируй
> данные — ускоришь рендеринг.

## Architecture: Почему RmlUi медленный?

RmlUi по умолчанию создан для гибкости, не для скорости. Вот типичные bottleneck-и:

| Проблема                                | Влияние            | Решение            |
|-----------------------------------------|--------------------|--------------------|
| **RenderGeometry каждый кадр**          | CPU → GPU transfer | Compiled Geometry  |
| **Множественные small texture uploads** | Pipeline stalls    | Texture atlas      |
| **Scissor переключения**                | Pipeline stalls    | Batch по текстурам |
| **SetInnerRML**                         | DOM re-parse       | Data Bindings      |
| **Heap аллокации в кадре**              | Cache misses       | Pool allocator     |

## Compiled Geometry: GPU-Driven подход

> **Для понимания:** Это как построить конструктор LEGO один раз, а потом просто доставать из коробки. Вместо того чтобы
> каждый кадр заново собирать модель из деталей (данные → вершины → GPU), вы собираете её один раз при загрузке, а потом
> только перемещаете.

### Концепция

```cpp
// AoS: Плохо для кэша
struct DrawCall {
    Rml::Vertex* vertices;
    int vertexCount;
    int* indices;
    int indexCount;
    Rml::TextureHandle texture;
    Rml::Vector2f translation;
};

// SoA: Хорошо для SIMD/кэша
struct UIBatchData {
    std::span<Rml::Vertex> vertices;  // 64-byte aligned
    std::span<int> indices;
    Rml::TextureHandle texture;
    Rml::Vector2f translation;
};
```

### Vulkan Implementation

```cpp
// src/ui/compiled_geometry.hpp
#pragma once

#include <RmlUi/Core/RenderInterface.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <span>
#include <vector>
#include <array>

// SoA структура для максимальной производительности
alignas(64) struct UIGeometryBatch {
    // Vertex data - выровнено по 64 байта (кэш-линия)
    std::vector<Rml::Vertex> vertices;
    std::vector<int> indices;

    // Metadata
    Rml::TextureHandle texture = 0;
    uint32_t indexCount = 0;
    uint32_t vertexCount = 0;

    // GPU resources - lifetime управляется отдельно
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VmaAllocation vertexAllocation = VK_NULL_HANDLE;
    VmaAllocation indexAllocation = VK_NULL_HANDLE;
};

class CompiledGeometryPool {
public:
    explicit CompiledGeometryPool(VmaAllocator allocator, VkDevice device) noexcept;
    ~CompiledGeometryPool();

    // Compile - создаёт GPU буферы один раз
    [[nodiscard]] size_t compile(
        std::span<const Rml::Vertex> vertices,
        std::span<const int> indices,
        Rml::TextureHandle texture
    ) noexcept;

    // Render - просто bind и draw
    void render(
        VkCommandBuffer cmd,
        VkPipeline pipeline,
        VkPipelineLayout pipelineLayout,
        size_t batchIndex,
        const Rml::Vector2f& translation
    ) const noexcept;

    // Release - возвращает в пул
    void release(size_t batchIndex) noexcept;

    // Batch сортировка по текстуре
    void sortByTexture() noexcept;

private:
    VmaAllocator allocator_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;

    // Пулы - SoA layout
    std::vector<UIGeometryBatch> batches_;
    std::vector<size_t> freeSlots_;  // Stack для быстрого allocate

    // Staging для загрузки данных
    VkBuffer stagingBuffer_ = VK_NULL_HANDLE;
    VmaAllocation stagingAllocation_ = VK_NULL_HANDLE;
    size_t stagingCapacity_ = 0;

    [[nodiscard]] bool ensureStagingCapacity(size_t requiredSize) noexcept;
    void uploadToGPU(UIGeometryBatch& batch,
                    std::span<const Rml::Vertex> vertices,
                    std::span<const int> indices) noexcept;
};

constexpr size_t MAX_UI_BATCHES = 512;
constexpr size_t MAX_STAGING_SIZE = 16 * 1024 * 1024; // 16MB
```

### Реализация

```cpp
// src/ui/compiled_geometry.cpp
#include "compiled_geometry.hpp"
#include <print>
#include <algorithm>

CompiledGeometryPool::CompiledGeometryPool(VmaAllocator allocator, VkDevice device) noexcept
    : allocator_(allocator)
    , device_(device)
{
    batches_.reserve(MAX_UI_BATCHES);
    ensureStagingCapacity(64 * 1024); // 64KB initial
}

CompiledGeometryPool::~CompiledGeometryPool() {
    for (auto& batch : batches_) {
        if (batch.vertexBuffer) {
            vmaDestroyBuffer(allocator_, batch.vertexBuffer, batch.vertexAllocation);
        }
        if (batch.indexBuffer) {
            vmaDestroyBuffer(allocator_, batch.indexBuffer, batch.indexAllocation);
        }
    }

    if (stagingBuffer_) {
        vmaDestroyBuffer(allocator_, stagingBuffer_, stagingAllocation_);
    }
}

bool CompiledGeometryPool::ensureStagingCapacity(size_t requiredSize) noexcept {
    if (stagingCapacity_ >= requiredSize) return true;

    if (stagingBuffer_) {
        vmaDestroyBuffer(allocator_, stagingBuffer_, stagingAllocation_);
    }

    VkBufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = requiredSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    };

    VmaAllocationCreateInfo allocInfo = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_CPU_ONLY
    };

    if (vmaCreateBuffer(allocator_, &info, &allocInfo,
                        &stagingBuffer_, &stagingAllocation_, nullptr) != VK_SUCCESS) {
        return false;
    }

    stagingCapacity_ = requiredSize;
    return true;
}

void CompiledGeometryPool::uploadToGPU(
    UIGeometryBatch& batch,
    std::span<const Rml::Vertex> vertices,
    std::span<const int> indices
) noexcept {
    // Vertex buffer
    VkBufferCreateInfo vbInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = vertices.size_bytes(),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
    };

    VmaAllocationCreateInfo allocInfo = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY
    };

    vmaCreateBuffer(allocator_, &vbInfo, &allocInfo,
                    &batch.vertexBuffer, &batch.vertexAllocation, nullptr);

    // Index buffer
    VkBufferCreateInfo ibInfo = vbInfo;
    ibInfo.size = indices.size_bytes();

    vmaCreateBuffer(allocator_, &ibInfo, &allocInfo,
                    &batch.indexBuffer, &batch.indexAllocation, nullptr);

    // Staging copy
    void* stagingData;
    vmaMapMemory(allocator_, stagingAllocation_, &stagingData);

    // Copy vertices
    std::memcpy(stagingData, vertices.data(), vertices.size_bytes());

    // Copy indices
    std::memcpy(static_cast<char*>(stagingData) + vertices.size_bytes(),
                indices.data(), indices.size_bytes());

    vmaUnmapMemory(allocator_, stagingAllocation_);

    // Transfer to GPU (simplified - in real code use command buffer)
    // ...
}

size_t CompiledGeometryPool::compile(
    std::span<const Rml::Vertex> vertices,
    std::span<const int> indices,
    Rml::TextureHandle texture
) noexcept {
    size_t slot;

    if (!freeSlots_.empty()) {
        slot = freeSlots_.back();
        freeSlots_.pop_back();
    } else {
        slot = batches_.size();
        batches_.emplace_back();
    }

    auto& batch = batches_[slot];
    batch.texture = texture;
    batch.vertexCount = static_cast<uint32_t>(vertices.size());
    batch.indexCount = static_cast<uint32_t>(indices.size());

    // Copy data
    batch.vertices.assign(vertices.begin(), vertices.end());
    batch.indices.assign(indices.begin(), indices.end());

    // Upload to GPU
    uploadToGPU(batch, vertices, indices);

    return slot;
}

void CompiledGeometryPool::render(
    VkCommandBuffer cmd,
    VkPipeline pipeline,
    VkPipelineLayout pipelineLayout,
    size_t batchIndex,
    const Rml::Vector2f& translation
) const noexcept {
    const auto& batch = batches_[batchIndex];

    if (!batch.vertexBuffer) return;

    // Bind
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &batch.vertexBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, batch.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Push constants
    struct PushConstants {
        Rml::Vector2f translation;
        float padding[2];
    } pc = {translation, {}};

    vkCmdPushConstants(cmd, pipelineLayout,
                      VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

    // Draw
    vkCmdDrawIndexed(cmd, batch.indexCount, 1, 0, 0, 0);
}

void CompiledGeometryPool::release(size_t batchIndex) noexcept {
    auto& batch = batches_[batchIndex];

    // Release GPU resources
    if (batch.vertexBuffer) {
        vmaDestroyBuffer(allocator_, batch.vertexBuffer, batch.vertexAllocation);
        batch.vertexBuffer = VK_NULL_HANDLE;
    }
    if (batch.indexBuffer) {
        vmaDestroyBuffer(allocator_, batch.indexBuffer, batch.indexAllocation);
        batch.indexBuffer = VK_NULL_HANDLE;
    }

    batch.vertices.clear();
    batch.indices.clear();
    batch.texture = 0;

    freeSlots_.push_back(batchIndex);
}

void CompiledGeometryPool::sortByTexture() noexcept {
    // SoA batch сортировка по текстуре для минимизации bind
    std::sort(batches_.begin(), batches_.end(),
        [](const UIGeometryBatch& a, const UIGeometryBatch& b) {
            return a.texture < b.texture;
        });
}
```

## Batch Rendering: Сортировка по текстуре

> **Для понимания:** Это как собирать посуду для мытья. Вы же не моете тарелку, потом ложку, потом опять тарелку? Вы
> моете все тарелки, потом все ложки. Batch rendering работает так же: сгруппируй все draw calls с одной текстурой.

### Паттерн

```cpp
class BatchRenderer {
public:
    struct DrawCall {
        size_t geometryIndex;
        Rml::Vector2f translation;
    };

    void addDrawCall(size_t geometryIndex, Rml::TextureHandle texture,
                    const Rml::Vector2f& translation) {
        batches_[texture].push_back({geometryIndex, translation});
    }

    void flush(VkCommandBuffer cmd) {
        // Сортировка не нужна - мы уже группируем по текстуре при добавлении

        for (auto& [texture, calls] : batches_) {
            if (calls.empty()) continue;

            // Bind текстуру один раз
            bindTexture(texture);

            // Нарисовать все вызовы
            for (const auto& call : calls) {
                geometryPool_.render(cmd, pipeline_, layout_,
                                    call.geometryIndex, call.translation);
            }

            calls.clear(); // reuse vector
        }
    }

private:
    // SoA: texture -> calls
    std::unordered_map<Rml::TextureHandle, std::vector<DrawCall>> batches_;
    CompiledGeometryPool geometryPool_;
};
```

## Texture Atlas: Один draw call

> **Для понимания:** Вместо того чтобы звонить другу 100 раз по разным номерам (100 texture binds), вы звоните один раз
> по одному номеру (один atlas texture bind). Результат тот же, но на порядок быстрее.

### Реализация

```cpp
class TextureAtlas {
public:
    struct Region {
        uint32_t x, y, width, height;
    };

    void addImage(std::string_view name, const Rml::byte* data,
                 uint32_t width, uint32_t height) {
        // Pack into atlas (simplified - use bin packing algorithm)
        Region region = findFreeSpace(width, height);

        // Copy to atlas
        copyToAtlas(region, data, width, height);

        regions_[std::string(name)] = region;
    }

    Region getRegion(std::string_view name) const {
        auto it = regions_.find(std::string(name));
        return it != regions_.end() ? it->second : Region{0, 0, 0, 0};
    }

    VkImageView getAtlasImageView() const { return atlasImageView_; }

private:
    // Atlas: 2048x2048
    static constexpr uint32_t ATLAS_SIZE = 2048;

    std::unordered_map<std::string, Region> regions_;
    std::vector<Rml::byte> atlasData_(ATLAS_SIZE * ATLAS_SIZE * 4);

    VkImage atlasImage_ = VK_NULL_HANDLE;
    VkImageView atlasImageView_ = VK_NULL_HANDLE;

    Region findFreeSpace(uint32_t width, uint32_t height);
    void copyToAtlas(const Region& region, const Rml::byte* data,
                    uint32_t width, uint32_t height);
};
```

## Data Binding: Zero-Copy Sync

> **Для понимания:** Это как Excel: изменили ячейку — таблица обновилась. Никаких ручных вызовов SetInnerRML. Но мы
> пойдём дальше — сделаем это без копирования данных.

### SoA для ECS данных

```cpp
// ECS UI data - SoA layout
alignas(64) struct UIPlayerDataSoA {
    // Hot data - часто меняется
    std::vector<int> health;
    std::vector<int> maxHealth;
    std::vector<float> stamina;

    // Cold data - редко меняется
    std::vector<std::string> name;  // String pool нужен для zero-copy
    std::vector<uint64_t> entityIds;

    // Padding для избежания false sharing
    alignas(64) char padding[64];
};

class UIPlayerDataSync {
public:
    explicit UIPlayerDataSync(flecs::world& world) noexcept
        : world_(world)
    {
        // Pre-allocate
        data_.health.reserve(128);
        data_.maxHealth.reserve(128);
        data_.stamina.reserve(128);
    }

    void sync() {
        // Only sync changed data - avoid full rebuild
        world_.each([&](flecs::entity e, const struct Player& player) {
            // Find or add
            auto it = findEntityIndex(e.id());
            if (it == std::nullopt) {
                // Add new
                it = data_.entityIds.size();
                data_.entityIds.push_back(e.id());
                data_.health.push_back(0);
                data_.maxHealth.push_back(0);
                data_.stamina.push_back(0.0f);
            }

            // Update - только если изменилось
            if (data_.health[*it] != player.health) {
                data_.health[*it] = player.health;
                dirtyFlags_[*it] = true;
            }
            // ... similar for other fields
        });

        // Notify RmlUi - только для dirty переменных
        notifyRmlUi();
    }

private:
    flecs::world& world_;
    UIPlayerDataSoA data_;
    std::vector<bool> dirtyFlags_;

    void notifyRmlUi() {
        // RmlUi DataModelHandle.DirtyVariable() для каждой dirty переменной
    }

    std::optional<size_t> findEntityIndex(uint64_t entityId) {
        for (size_t i = 0; i < data_.entityIds.size(); ++i) {
            if (data_.entityIds[i] == entityId) return i;
        }
        return std::nullopt;
    }
};
```

## Job System: Параллельная загрузка

> **Для понимания:** Загрузка UI документов и текстур не должна блокировать main thread. Используйте Job System (
> см. [../flecs/](../flecs)) для параллельной загрузки.

### Паттерн

```cpp
class UIJobSystem {
public:
    using Job = std::function<void()>;

    // Enqueue - не блокирует main thread
    void enqueueLoadDocument(std::string_view path,
                            std::promise<Rml::ElementDocument*>&& promise) {
        jobQueue_.enqueue([this, path, promise = std::move(promise)]() mutable {
            // Load in background thread
            Rml::ElementDocument* doc = nullptr;

            // Thread-safe file load
            {
                std::lock_guard lock(fileMutex_);
                doc = context_->LoadDocument(std::string(path).c_str());
            }

            promise.set_value(doc);
        });
    }

    // Вызывать в main thread после frame
    void processCompleted() {
        // Handle completed promises
    }

private:
    // Thread-safe queue - SPSC (Single Producer Single Consumer)
    moodycamel::ConcurrentQueue<Job> jobQueue_;
    std::mutex fileMutex_;  // Для FileInterface
};
```

## Memory Management: Pool Allocator

> **Для понимания:** Каждый раз, когда вы делаете `new`, процессор ищет свободную память. Это как искать место на
> парковке каждый раз, когда приезжает машина. Pool allocator — это как постоянное место: приехал — место уже ждёт.

### UI Element Pool

```cpp
template<typename T>
class ObjectPool {
public:
    explicit ObjectPool(size_t capacity) {
        pool_.resize(capacity);
        for (auto& obj : pool_) {
            freeSlots_.push_back(&obj);
        }
    }

    T* allocate() {
        if (freeSlots_.empty()) return nullptr;

        T* obj = freeSlots_.back();
        freeSlots_.pop_back();
        ++allocatedCount_;

        return obj;
    }

    void deallocate(T* obj) {
        // Reset object
        obj->~T();
        new (obj) T();

        freeSlots_.push_back(obj);
        --allocatedCount_;
    }

private:
    std::vector<T> pool_;
    std::vector<T*> freeSlots_;
    size_t allocatedCount_ = 0;
};

// Использование для UI данных
ObjectPool<UIHealthBar> healthBarPool(256);
ObjectPool<UIInventory> inventoryPool(64);
```

### Staging Buffer Pool

```cpp
class StagingBufferPool {
public:
    struct StagingBuffer {
        VkBuffer buffer;
        VmaAllocation allocation;
        void* mapped;
        size_t capacity;
        bool inUse;
    };

    StagingBufferPool(VmaAllocator allocator, VkDevice device);
    ~StagingBufferPool();

    StagingBuffer* acquire(size_t requiredSize);
    void release(StagingBuffer* buffer);

private:
    // Pre-allocated buffers
    std::vector<StagingBuffer> buffers_;
    // ... implementation
};
```

## Tracy Integration: Профилирование

```cpp
#include <tracy/Tracy.hpp>

void UISystem::update(float deltaTime) {
    ZoneScopedN("RmlUi Update");

    // Sync data
    {
        ZoneScopedN("UI Data Sync");
        syncFromECS();
    }

    // Update context
    {
        ZoneScopedN("RmlUi Context Update");
        context_->Update();
    }
}

void UISystem::render(VkCommandBuffer cmd) {
    ZoneScopedN("RmlUi Render");

    // Начало render pass для UI
    // ...

    context_->Render();

    // Конец render pass
    // ...
}
```

## Profiling: Метрики

```cpp
struct UIStats {
    uint32_t drawCalls = 0;
    uint32_t vertices = 0;
    uint32_t textures = 0;
    double updateTime = 0.0;
    double renderTime = 0.0;
};

class UIProfiler {
public:
    void beginFrame() {
        stats_ = {};
        updateTimer_.start();
    }

    void endFrame() {
        updateTimer_.stop();
        renderTimer_.stop();

        stats_.updateTime = updateTimer_.elapsed();
        stats_.renderTime = renderTimer_.elapsed();
    }

    const UIStats& getStats() const { return stats_; }

private:
    UIStats stats_;
    // Use Tracy timers in real implementation
};

void logUIStats(const UIStats& stats) {
    std::println("UI Stats: {} draw calls, {} vertices, {} textures, "
                 "update: {:.2f}ms, render: {:.2f}ms",
                 stats.drawCalls, stats.vertices, stats.textures,
                 stats.updateTime * 1000.0, stats.renderTime * 1000.0);
}
```

## Сравнение производительности

| Техника           | Draw Calls | CPU    | GPU     | Сложность |
|-------------------|------------|--------|---------|-----------|
| Default RmlUi     | 100-500    | High   | Low     | Low       |
| Compiled Geometry | 10-50      | Low    | Medium  | Medium    |
| Texture Atlas     | 5-20       | Low    | High    | High      |
| Batch + Atlas     | 1-5        | Lowest | Highest | Very High |

## Чеклист оптимизаций

- [ ] Compiled Geometry для статической геометрии
- [ ] Texture Atlas для всех UI текстур
- [ ] Batch сортировка по текстуре
- [ ] Data Bindings вместо SetInnerRML
- [ ] Pool allocator для частых аллокаций
- [ ] Tracy профилирование
- [ ] Zero-copy sync из ECS
- [ ] Job System для загрузки
- [ ] Staging buffer pool

## Резюме

1. **GPU-driven rendering**: Compiled Geometry → один раз в GPU, потом только draw
2. **Batch по текстурам**: минимизация pipeline stalls
3. **SoA data layout**: кэш-friendly синхронизация ECS ↔ UI
4. **Pool allocation**: избегание heap fragmentation
5. **Job System**: асинхронная загрузка без блокировки main thread

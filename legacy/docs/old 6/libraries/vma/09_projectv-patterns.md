# VMA в ProjectV: Паттерны

**🔴 Уровень 3: Продвинутый**

Паттерны управления памятью для воксельного движка.

---

## Паттерн 1: Пул для воксельных чанков

### Проблема

Воксельный движок создаёт множество чанков одинакового размера. Выделение каждого чанка отдельно ведёт к фрагментации и
медленным аллокациям.

### Решение

```cpp
class ChunkMemoryPool {
    VmaAllocator m_allocator;
    VmaPool m_pool;
    VkDeviceSize m_chunkSize;

public:
    ChunkMemoryPool(VmaAllocator allocator, VkDeviceSize chunkSize, size_t maxBlocks)
        : m_allocator(allocator), m_chunkSize(chunkSize)
    {
        // Определение типа памяти для storage buffers
        VkBufferCreateInfo sampleBufferInfo = {};
        sampleBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        sampleBufferInfo.size = chunkSize;
        sampleBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo sampleAllocInfo = {};
        sampleAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

        uint32_t memoryTypeIndex;
        vmaFindMemoryTypeIndexForBufferInfo(allocator, &sampleBufferInfo,
                                           &sampleAllocInfo, &memoryTypeIndex);

        // Создание пула
        VmaPoolCreateInfo poolInfo = {};
        poolInfo.memoryTypeIndex = memoryTypeIndex;
        poolInfo.blockSize = 64 * 1024 * 1024;  // 64 MB
        poolInfo.minBlockCount = 1;
        poolInfo.maxBlockCount = maxBlocks;
        poolInfo.flags = 0;

        vmaCreatePool(allocator, &poolInfo, &m_pool);
    }

    ~ChunkMemoryPool() {
        if (m_pool) {
            vmaDestroyPool(m_allocator, m_pool);
        }
    }

    struct ChunkAllocation {
        VkBuffer buffer;
        VmaAllocation allocation;
    };

    ChunkAllocation allocate() {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = m_chunkSize;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.pool = m_pool;
        allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;

        ChunkAllocation result;
        vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo,
                       &result.buffer, &result.allocation, nullptr);

        return result;
    }

    void deallocate(ChunkAllocation& chunk) {
        vmaDestroyBuffer(m_allocator, chunk.buffer, chunk.allocation);
        chunk.buffer = VK_NULL_HANDLE;
        chunk.allocation = VK_NULL_HANDLE;
    }

    VmaStatistics getStatistics() {
        VmaStatistics stats;
        vmaGetPoolStatistics(m_allocator, m_pool, &stats);
        return stats;
    }
};
```

---

## Паттерн 2: Интеграция с Tracy

### Проблема

Нужно отслеживать потребление памяти VMA в реальном времени.

### Решение

```cpp
#ifdef TRACY_ENABLE
#include "Tracy.hpp"
#endif

namespace projectv::memory {

enum class MemoryCategory {
    Chunks,
    Textures,
    Uniforms,
    Staging,
    Compute
};

class TrackedAllocation {
    VmaAllocation m_allocation;
    VkDeviceSize m_size;
    MemoryCategory m_category;

public:
    TrackedAllocation(VmaAllocation allocation, VkDeviceSize size, MemoryCategory category)
        : m_allocation(allocation), m_size(size), m_category(category)
    {
#ifdef TRACY_ENABLE
        const char* categoryName = getCategoryName(category);
        TracyAllocN(m_allocation, m_size, categoryName);
#endif
    }

    ~TrackedAllocation() {
        if (m_allocation) {
#ifdef TRACY_ENABLE
            TracyFreeN(m_allocation, getCategoryName(m_category));
#endif
        }
    }

    static const char* getCategoryName(MemoryCategory category) {
        switch (category) {
            case MemoryCategory::Chunks: return "VMA_Chunks";
            case MemoryCategory::Textures: return "VMA_Textures";
            case MemoryCategory::Uniforms: return "VMA_Uniforms";
            case MemoryCategory::Staging: return "VMA_Staging";
            case MemoryCategory::Compute: return "VMA_Compute";
            default: return "VMA_Unknown";
        }
    }
};

// Отслеживание статистики
void updateMemoryStats(VmaAllocator allocator) {
#ifdef TRACY_ENABLE
    VmaTotalStatistics stats;
    vmaCalculateStatistics(allocator, &stats);

    TracyPlot("VMA_TotalAllocations", (int64_t)stats.total.statistics.allocationCount);
    TracyPlot("VMA_UsedMemoryMB", (int64_t)(stats.total.statistics.allocationBytes / (1024 * 1024)));
    TracyPlot("VMA_FreeMemoryMB", (int64_t)(stats.total.statistics.unusedBytes / (1024 * 1024)));

    // Бюджет памяти
    VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets(allocator, budgets);

    for (uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; ++i) {
        if (budgets[i].budget > 0) {
            float usage = float(budgets[i].usage) / float(budgets[i].budget) * 100.0f;
            TracyPlot("VMA_HeapUsagePercent", usage);
        }
    }
#endif
}

} // namespace projectv::memory
```

---

## Паттерн 3: Интеграция с flecs ECS

> **См. также:** [ECS философия](../../philosophy/04_ecs-philosophy.md) — принципы проектирования ECS компонентов.

### Проблема

GPU ресурсы нужно связать с ECS сущностями для автоматического управления жизненным циклом.

### Решение

```cpp
#include <flecs.h>

namespace projectv::ecs {

// Компоненты
struct VmaBufferComponent {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
};

struct VmaImageComponent {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkExtent3D extent = {};
    VkFormat format = VK_FORMAT_UNDEFINED;
};

// Компонент-тег для загрузки
struct NeedsGpuUpload {};

// Система очистки ресурсов
void registerVmaCleanupSystem(flecs::world& world, VmaAllocator allocator) {
    // Observer для буферов
    world.observer<VmaBufferComponent>()
        .event(flecs::OnRemove)
        .iter([allocator](flecs::iter& it, VmaBufferComponent* buffers) {
            for (auto i : it) {
                if (buffers[i].buffer != VK_NULL_HANDLE) {
                    vmaDestroyBuffer(allocator, buffers[i].buffer, buffers[i].allocation);
                }
            }
        });

    // Observer для изображений
    world.observer<VmaImageComponent>()
        .event(flecs::OnRemove)
        .iter([allocator](flecs::iter& it, VmaImageComponent* images) {
            for (auto i : it) {
                if (images[i].image != VK_NULL_HANDLE) {
                    vmaDestroyImage(allocator, images[i].image, images[i].allocation);
                }
            }
        });
}

// Система загрузки чанков
class ChunkUploadSystem {
    VmaAllocator m_allocator;
    VkDeviceSize m_chunkSize;

public:
    ChunkUploadSystem(flecs::world& world, VmaAllocator allocator, VkDeviceSize chunkSize)
        : m_allocator(allocator), m_chunkSize(chunkSize)
    {
        world.system<VmaBufferComponent, const NeedsGpuUpload>("ChunkUploadSystem")
            .iter([this](flecs::iter& it, VmaBufferComponent* buffers, const NeedsGpuUpload*) {
                for (auto i : it) {
                    if (buffers[i].buffer == VK_NULL_HANDLE) {
                        buffers[i] = createChunkBuffer();
                        it.entity(i).remove<NeedsGpuUpload>();
                    }
                }
            });
    }

private:
    VmaBufferComponent createChunkBuffer() {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = m_chunkSize;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

        VmaBufferComponent result;
        result.size = m_chunkSize;
        vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo,
                       &result.buffer, &result.allocation, nullptr);

        return result;
    }
};

} // namespace projectv::ecs
```

---

## Паттерн 4: Double buffering для compute

### Проблема

Compute shader обновляет данные, которые читаются на следующем кадре. Нужна синхронизация без ожидания.

### Решение

```cpp
template<typename T>
class DoubleBufferedCompute {
    struct Buffer {
        VmaBuffer buffer;
        VkDescriptorSet descriptorSet;
    };

    std::array<Buffer, 2> m_buffers;
    size_t m_currentIndex = 0;
    VmaAllocator m_allocator;

public:
    DoubleBufferedCompute(VmaAllocator allocator, VkDeviceSize size,
                         VkDescriptorPool descriptorPool)
        : m_allocator(allocator)
    {
        for (size_t i = 0; i < 2; ++i) {
            m_buffers[i].buffer = VmaBuffer(allocator, size,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VMA_MEMORY_USAGE_AUTO);

            // Выделение descriptor set
            // ...
        }
    }

    VkBuffer getCurrent() const {
        return m_buffers[m_currentIndex].buffer.get();
    }

    VkBuffer getPrevious() const {
        return m_buffers[1 - m_currentIndex].buffer.get();
    }

    VkDescriptorSet getCurrentDescriptorSet() const {
        return m_buffers[m_currentIndex].descriptorSet;
    }

    void swap() {
        m_currentIndex = 1 - m_currentIndex;
    }

    // Барьер для синхронизации
    void insertBarrier(VkCommandBuffer cmd) {
        VkMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &barrier, 0, nullptr, 0, nullptr);
    }
};
```

---

## Паттерн 5: Загрузка текстур через staging

### Проблема

Текстуры материалов загружаются асинхронно, требуется эффективный staging.

### Решение

```cpp
class TextureLoader {
    VmaAllocator m_allocator;

public:
    struct TextureResult {
        VkImage image;
        VmaAllocation allocation;
        VkImageView view;
    };

    TextureResult loadFromMemory(VmaAllocator allocator,
                                 const void* pixelData,
                                 uint32_t width, uint32_t height,
                                 VkFormat format,
                                 VkCommandBuffer cmdBuffer)
    {
        VkDeviceSize imageSize = width * height * 4;  // RGBA8

        // 1. Staging буфер
        VkBufferCreateInfo stagingInfo = {};
        stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingInfo.size = imageSize;
        stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo stagingAllocInfo = {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VkBuffer stagingBuffer;
        VmaAllocation stagingAllocation;
        VmaAllocationInfo stagingAllocResult;
        vmaCreateBuffer(allocator, &stagingInfo, &stagingAllocInfo,
                       &stagingBuffer, &stagingAllocation, &stagingAllocResult);

        // 2. Копирование данных
        memcpy(stagingAllocResult.pMappedData, pixelData, imageSize);
        vmaFlushAllocation(allocator, stagingAllocation, 0, VK_WHOLE_SIZE);

        // 3. Создание изображения
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {width, height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        VmaAllocationCreateInfo imageAllocInfo = {};
        imageAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

        TextureResult result;
        vmaCreateImage(allocator, &imageInfo, &imageAllocInfo,
                      &result.image, &result.allocation, nullptr);

        // 4. Переход layout + копирование
        // ... (VkImageMemoryBarrier + vkCmdCopyBufferToImage)

        // 5. Освобождение staging
        vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);

        return result;
    }
};
```

---

## Паттерн 6: Кольцевой staging buffer

### Проблема

Частые мелкие загрузки создают много временных staging буферов.

### Решение

```cpp
class RingStagingBuffer {
    VmaAllocator m_allocator;
    VkBuffer m_buffer;
    VmaAllocation m_allocation;
    void* m_mappedData;

    VkDeviceSize m_size;
    VkDeviceSize m_offset = 0;
    size_t m_frameCount = 0;
    static constexpr size_t FRAME_LAG = 3;

public:
    RingStagingBuffer(VmaAllocator allocator, VkDeviceSize size)
        : m_allocator(allocator), m_size(size)
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                         VMA_ALLOCATION_CREATE_MAPPED_BIT;

        vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
                       &m_buffer, &m_allocation, nullptr);

        VmaAllocationInfo allocInfoResult;
        vmaGetAllocationInfo(allocator, m_allocation, &allocInfoResult);
        m_mappedData = allocInfoResult.pMappedData;
    }

    ~RingStagingBuffer() {
        vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
    }

    struct Allocation {
        VkBuffer buffer;
        VkDeviceSize offset;
    };

    std::optional<Allocation> allocate(VkDeviceSize size) {
        if (size > m_size) {
            return std::nullopt;
        }

        if (m_offset + size > m_size) {
            // Переход на следующий круг
            m_offset = 0;
        }

        Allocation result = {m_buffer, m_offset};
        m_offset += size;

        return result;
    }

    void* getPtr(VkDeviceSize offset) {
        return static_cast<char*>(m_mappedData) + offset;
    }

    void flush() {
        vmaFlushAllocation(m_allocator, m_allocation, 0, VK_WHOLE_SIZE);
    }

    void onFrameEnd() {
        m_frameCount++;
        if (m_frameCount >= FRAME_LAG) {
            // Можно переиспользовать память
        }
    }

    VkBuffer getBuffer() const { return m_buffer; }
};
```

---

## Рекомендации

### Чего избегать

1. **Создание/уничтожение буферов каждый кадр** — используйте пулы и ring buffers
2. **Игнорирование `vmaFlushAllocation`** — может работать на одной GPU и ломаться на другой
3. **Прямой доступ к device-local памяти** — используйте staging
4. **Аллокации без ограничений** — устанавливайте `maxBlockCount` для пулов

### Оптимизации

1. **Batch-загрузка** — объединяйте мелкие загрузки в одну
2. **Pre-allocation** — создавайте пулы заранее
3. **Memory aliasing** — для ресурсов, используемых в разное время
4. **Defragmentation** — периодически для долгоживущих ресурсов

# VMA в ProjectV: Продвинутые паттерны

> **Для понимания:** Продвинутые паттерны VMA — это как турбонаддув для двигателя. Базовый движок работает, но чтобы
> выжать максимум, нужно добавить интеркулер (double buffering), турбину (пулы) и систему зажигания (Tracy). Без этого —
> просто громкий шум без скорости.

**Производительность — это не опция, а архитектурное требование.** В воксельном движке управление памятью определяет
границы возможного.

---

## Архитектура памяти для воксельного движка

### Требования к памяти

Воксельный движок предъявляет особые требования к управлению памятью:

| Тип ресурса         | Характеристики                            | Частота обновления           | Стратегия VMA                   |
|---------------------|-------------------------------------------|------------------------------|---------------------------------|
| Воксельные чанки    | Множество объектов фиксированного размера | При загрузке/выгрузке чанков | `MIN_MEMORY` + пулы             |
| Текстуры материалов | Крупные ресурсы                           | Редко                        | `MIN_MEMORY` + dedicated        |
| Compute buffers     | Storage buffers для шейдеров              | Зависит от алгоритма         | Зависит от частоты              |
| Uniform buffers     | Матрицы камеры, параметры                 | Каждый кадр                  | `MIN_TIME` + persistent mapping |
| Staging buffers     | Временные буферы                          | Часто                        | `MIN_TIME`                      |

### Архитектура памяти

```
VmaAllocator
    │
    ├── ChunkPool (VmaPool)
    │       ├── ChunkAllocation[0] ─── VkBuffer (16KB)
    │       ├── ChunkAllocation[1] ─── VkBuffer (16KB)
    │       └── ...
    │
    ├── TexturePool (опционально)
    │       └── TextureAllocation ─── VkImage
    │
    ├── UniformBuffers
    │       ├── UniformAllocation[0] ─── VkBuffer (persistent mapped)
    │       ├── UniformAllocation[1]
    │       └── UniformAllocation[2]
    │
    └── StagingBuffers
            └── StagingAllocation ─── VkBuffer (host-visible)
```

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

**Преимущества пулов:**

1. **Уменьшение фрагментации** — чанки одного размера аллоцируются из предварительно выделенных блоков
2. **Быстрые аллокации** — не требуется поиск в общей куче
3. **Контроль памяти** — можно ограничить максимальный размер пула
4. **Локализация данных** — чанки одного типа располагаются рядом

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

## Паттерн 3: Double buffering для compute

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

## Паттерн 4: Кольцевой staging buffer

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
        {
            // Барьер для перехода из UNDEFINED в TRANSFER_DST_OPTIMAL
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = result.image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            vkCmdPipelineBarrier(cmdBuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &barrier);
        }

        // Копирование данных из staging буфера в изображение
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};

        vkCmdCopyBufferToImage(cmdBuffer, stagingBuffer, result.image,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        // Барьер для перехода из TRANSFER_DST_OPTIMAL в SHADER_READ_ONLY_OPTIMAL
        {
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = result.image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(cmdBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &barrier);
        }

        // 5. Освобождение staging
        vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);

        return result;
    }
};
```

---

## Troubleshooting

### Ошибки компиляции

#### VK_NO_PROTOTYPES при использовании volk

**Сообщение:**

```
To use volk, you need to define VK_NO_PROTOTYPES before including vulkan.h
```

**Решение:** В каждом .cpp перед любыми includes:

```cpp
#define VK_NO_PROTOTYPES
#include "volk.h"
#include "vma/vk_mem_alloc.h"
```

### Ошибки линковки

#### Неразрешённые символы: vmaCreateAllocator, vmaCreateBuffer

**Причина:** Макрос `VMA_IMPLEMENTATION` не определён ни в одном .cpp.

**Решение:** В одном .cpp файле:

```cpp
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"
```

#### Дублирование символов VMA

**Причина:** `VMA_IMPLEMENTATION` определён в нескольких .cpp файлах.

**Решение:** Убедиться, что макрос определён только в одном файле. Использовать `#ifndef VMA_IMPLEMENTATION`:

```cpp
// src/vma_init.cpp
#ifndef VMA_IMPLEMENTATION
#define VMA_IMPLEMENTATION
#endif
#include "vma/vk_mem_alloc.h"
```

### Ошибки выполнения

#### VMA не может найти подходящий тип памяти

**Причина:** Запрошенные флаги использования несовместимы с доступными типами памяти.

**Решение:** Проверить `VkPhysicalDeviceMemoryProperties` и использовать `VMA_MEMORY_USAGE_AUTO`:

```cpp
VmaAllocationCreateInfo allocInfo = {};
allocInfo.usage = VMA_MEMORY_USAGE_AUTO;  // Пусть VMA сам выберет
// Не указывать memoryTypeIndex вручную
```

#### Утечки памяти при использовании пулов

**Причина:** Ресурсы из пула не освобождены перед уничтожением пула.

**Решение:** Правильный порядок очистки:

```cpp
// 1. Уничтожить все буферы/изображения из пула
for (auto& buffer : buffers) {
    vmaDestroyBuffer(allocator, buffer, allocation);
}

// 2. Уничтожить пул
vmaDestroyPool(allocator, pool);

// 3. Уничтожить аллокатор (опционально)
vmaDestroyAllocator(allocator);
```

## Современный C++26: паттерны для воксельного движка

### Паттерн 6: SoA для воксельных данных с выравниванием

```cpp
#include <print>
#include <expected>
#include "vma/result.hpp"

// SoA (Structure of Arrays) вместо AoS (Array of Structures)
struct alignas(64) VoxelChunkSoA {
    // Позиции вокселей (16384 вокселей * 3 float)
    alignas(16) float positions[16384][3];  // 192KB

    // Типы материалов (отдельный массив для лучшей локализации)
    alignas(4) uint32_t materials[16384];   // 64KB

    // Освещение (опционально)
    alignas(4) float lighting[16384];       // 64KB

    // Флаги (сжатые)
    alignas(1) uint8_t flags[16384];        // 16KB

    // Общий размер: ~336KB, выровнено по 64 байта
    static constexpr size_t SIZE = 16384;
    static constexpr size_t BYTE_SIZE = sizeof(VoxelChunkSoA);
};

class VoxelChunkBuffer {
    VmaAllocator m_allocator;
    VkBuffer m_buffer;
    VmaAllocation m_allocation;

public:
    VmaResult<void> create(VmaAllocator allocator) {
        m_allocator = allocator;

        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = VoxelChunkSoA::BYTE_SIZE;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;

        if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
                           &m_buffer, &m_allocation, nullptr) != VK_SUCCESS) {
            std::println(stderr, "Failed to create voxel chunk buffer");
            return std::unexpected(VmaError::BufferCreationFailed);
        }

        std::println("Created voxel chunk buffer: {} KB aligned to 64 bytes",
                     VoxelChunkSoA::BYTE_SIZE / 1024);
        return {};
    }

    ~VoxelChunkBuffer() {
        if (m_buffer) {
            vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
        }
    }

    VmaResult<void> upload(const VoxelChunkSoA& data) {
        void* mapped;
        if (vmaMapMemory(m_allocator, m_allocation, &mapped) != VK_SUCCESS) {
            return std::unexpected(VmaError::MapFailed);
        }

        memcpy(mapped, &data, VoxelChunkSoA::BYTE_SIZE);
        vmaFlushAllocation(m_allocator, m_allocation, 0, VK_WHOLE_SIZE);
        vmaUnmapMemory(m_allocator, m_allocation);

        return {};
    }
};
```

### Паттерн 7: Triple buffering для compute с timeline semaphores

```cpp
#include <array>
#include <print>

class ComputeTripleBuffer {
    struct Frame {
        VmaBuffer buffer;
        uint64_t timelineValue = 0;
        bool ready = false;
    };

    std::array<Frame, 3> m_frames;
    size_t m_currentFrame = 0;
    VmaAllocator m_allocator;

public:
    VmaResult<void> initialize(VmaAllocator allocator, VkDeviceSize size) {
        m_allocator = allocator;

        for (size_t i = 0; i < 3; ++i) {
            auto result = VmaBuffer::create(allocator, size,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VMA_MEMORY_USAGE_AUTO,
                VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT);

            if (!result) {
                std::println(stderr, "Failed to create compute buffer {}", i);
                return std::unexpected(result.error());
            }

            m_frames[i].buffer = std::move(*result);
            m_frames[i].timelineValue = i;
        }

        std::println("Initialized compute triple buffer: {} MB per frame",
                     size / (1024.0 * 1024.0));
        return {};
    }

    Frame& get_current() { return m_frames[m_currentFrame]; }
    Frame& get_next() { return m_frames[(m_currentFrame + 1) % 3]; }
    Frame& get_previous() { return m_frames[(m_currentFrame + 2) % 3]; }

    void advance() {
        m_currentFrame = (m_currentFrame + 1) % 3;
    }

    void wait_for_frame(size_t frameIndex, VkSemaphore semaphore, uint64_t waitValue) {
        // Используем timeline semaphore для синхронизации
        VkSemaphoreWaitInfo waitInfo = {};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.semaphoreCount = 1;
        waitInfo.pSemaphores = &semaphore;
        waitInfo.pValues = &waitValue;

        // Ждём, пока GPU закончит работу с этим кадром
        vkWaitSemaphores(device, &waitInfo, UINT64_MAX);
        m_frames[frameIndex].ready = true;
    }
};
```

### Паттерн 8: Memory budget с предупреждениями

```cpp
#include <chrono>
#include <print>

class MemoryBudgetMonitor {
    VmaAllocator m_allocator;
    std::chrono::steady_clock::time_point m_lastCheck;
    float m_warningThreshold = 0.9f;  // 90%

public:
    MemoryBudgetMonitor(VmaAllocator allocator) : m_allocator(allocator) {
        m_lastCheck = std::chrono::steady_clock::now();
    }

    void check_budget() {
        auto now = std::chrono::steady_clock::now();
        if (now - m_lastCheck < std::chrono::seconds(1)) {
            return;  // Проверяем не чаще раза в секунду
        }

        m_lastCheck = now;

        VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
        vmaGetHeapBudgets(m_allocator, budgets);

        for (uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; ++i) {
            if (budgets[i].budget > 0) {
                float usage = float(budgets[i].usage) / float(budgets[i].budget);

                if (usage > m_warningThreshold) {
                    std::println(stderr, "WARNING: Heap {} usage: {:.1f}% (budget: {} MB)",
                                 i, usage * 100.0f, budgets[i].budget / (1024 * 1024));

                    // Можно принять меры: очистить кэш, уменьшить качество и т.д.
                    on_memory_pressure(i, usage);
                }
            }
        }
    }

private:
    void on_memory_pressure(uint32_t heapIndex, float usage) {
        // Реакция на нехватку памяти:
        // 1. Очистить LRU кэш текстур
        // 2. Уменьшить дальность прорисовки
        // 3. Выгрузить неиспользуемые чанки
        std::println("Memory pressure on heap {}: {:.1f}%", heapIndex, usage * 100.0f);
    }
};
```

## Итог: философия продвинутых паттернов

1. **SoA всегда, AoS никогда** — данные должны быть выровнены для кэша и SIMD
2. **Triple buffering для compute** — избегаем stalls между кадрами
3. **Memory budget monitoring** — предупреждаем о нехватке памяти до её исчерпания
4. **Timeline semaphores** — современная синхронизация вместо старых барьеров
5. **Профилирование с Tracy** — видимость = контроль

> **Почему именно так?** Потому что воксельный движок — это не про красоту, а про эффективность. Каждый лишний байт,
> каждая лишняя синхронизация, каждый промах кэша — это потерянные FPS. Продвинутые паттерны VMA дают контроль над
> памятью
> на уровне, недоступном при ручном управлении.

---

## Что дальше?

После освоения продвинутых паттернов VMA вы готовы к:

1. **Интеграции с volk** — мета-лоадер для прямых вызовов драйвера
2. **Интеграции с flecs** — ECS для управления ресурсами как компонентами
3. **GPU-driven rendering** — вынос логики рендеринга в compute shaders
4. **Bindless rendering** — отказ от descriptor sets в пользу giant descriptor arrays

Каждый следующий шаг строится на правильном управлении памятью через VMA. Без этого фундамента — всё остальное
бессмысленно.

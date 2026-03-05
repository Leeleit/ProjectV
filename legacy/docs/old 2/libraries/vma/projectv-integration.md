# Интеграция VMA в ProjectV

**🔴 Уровень 3: Продвинутый**

Специализированные паттерны и оптимизации для использования Vulkan Memory Allocator в воксельном движке ProjectV. Этот
документ содержит специфичные для ProjectV сценарии, рекомендации и примеры кода.

---

## Содержание

1. [Архитектура памяти для воксельного движка](#архитектура-памяти-для-воксельного-движка)
2. [Управление воксельными чанками](#управление-воксельными-чанками)
3. [Текстуры и атласы для вокселей](#текстуры-и-атласы-для-вокселей)
4. [Compute buffers для воксельных алгоритмов](#compute-buffers-для-воксельных-алгоритмов)
5. [Интеграция с Tracy для профилирования памяти](#интеграция-с-tracy-для-профилирования-памяти)
6. [Интеграция с ECS (flecs)](#интеграция-с-ecs-flecs)
7. [Паттерны производительности для воксельного рендеринга](#паттерны-производительности-для-воксельного-рендеринга)
8. [Примеры кода в ProjectV](#примеры-кода-в-projectv)

---

## Архитектура памяти для воксельного движка

Воксельный движок ProjectV предъявляет особые требования к управлению памятью:

### Ключевые вызовы

- **Частые аллокации/освобождения** при загрузке/выгрузке чанков
- **Крупные текстуры** для материалов и атласов
- **Compute buffers** для алгоритмов генерации и маркировки вокселей
- **Uniform buffers** для камеры и трансформаций, обновляемых каждый кадр

### Стратегии распределения памяти в ProjectV

| Тип ресурса         | Рекомендуемая стратегия VMA         | Обоснование                                                     |
|---------------------|-------------------------------------|-----------------------------------------------------------------|
| Воксельные чанки    | `MIN_MEMORY` + пулы                 | Долгоживущие, много объектов, важно минимизировать фрагментацию |
| Текстуры материалов | `MIN_MEMORY` + dedicated allocation | Большие ресурсы, загружаются один раз, редко освобождаются      |
| Uniform buffers     | `MIN_TIME` + persistent mapping     | Частые обновления каждый кадр, важна скорость аллокации         |
| Staging буферы      | `MIN_TIME`                          | Временные, создаются/уничтожаются часто, важна скорость         |
| Compute buffers     | Зависит от частоты обновления       | `MIN_MEMORY` для статических, `MIN_TIME` для динамических       |

---

## Управление воксельными чанками

Для эффективного управления памятью воксельных чанков (фиксированного размера) рекомендуется использовать пулы (
`VmaPool`).

### Создание пула для чанков

```cpp
VmaPool createChunkPool(VmaAllocator allocator, VkDeviceSize chunkSize) {
    // Определение типа памяти для чанков (device-local, оптимальный для storage buffers)
    VkBufferCreateInfo sampleBufferInfo = {};
    sampleBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    sampleBufferInfo.size = chunkSize;
    sampleBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VmaAllocationCreateInfo sampleAllocInfo = {};
    sampleAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    uint32_t memoryTypeIndex;
    vmaFindMemoryTypeIndexForBufferInfo(allocator, &sampleBufferInfo, &sampleAllocInfo, &memoryTypeIndex);

    // Создание пула
    VmaPoolCreateInfo poolInfo = {};
    poolInfo.memoryTypeIndex = memoryTypeIndex;
    poolInfo.blockSize = 64 * 1024 * 1024; // 64MB блоки
    poolInfo.minBlockCount = 1;
    poolInfo.maxBlockCount = 16; // Максимум 16 блоков (1GB)
    poolInfo.flags = 0;

    VmaPool pool = VK_NULL_HANDLE;
    VkResult result = vmaCreatePool(allocator, &poolInfo, &pool);
    if (result != VK_SUCCESS) {
        // Обработка ошибки
    }
    return pool;
}
```

### Аллокация чанка из пула

```cpp
VkBuffer allocateVoxelChunk(VmaAllocator allocator, VmaPool pool, VkDeviceSize size, VmaAllocation* allocation) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.pool = pool;
    allocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT; // Для долгоживущих чанков

    VkBuffer buffer = VK_NULL_HANDLE;
    VkResult result = vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, allocation, nullptr);

    if (result != VK_SUCCESS) {
        // Обработка ошибки
    }
    return buffer;
}
```

### Преимущества пулов для воксельного движка

1. **Уменьшение фрагментации** — чанки одного размера аллоцируются из предварительно выделенных блоков
2. **Быстрые аллокации/освобождения** — не требуется поиск подходящего блока в общей куче
3. **Лучшая локализация данных** — чанки одного типа располагаются рядом в памяти
4. **Упрощённое управление жизненным циклом** — можно освободить весь пул сразу

---

## Текстуры и атласы для вокселей

Для текстур материалов вокселей рекомендуется использовать dedicated allocation для больших ресурсов.

### Создание атласа текстур

```cpp
struct TextureAtlas {
    VkImage image;
    VmaAllocation allocation;
    uint32_t width, height;
};

TextureAtlas createTextureAtlas(VmaAllocator allocator, uint32_t width, uint32_t height, VkFormat format) {
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

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT; // Dedicated для больших текстур

    TextureAtlas atlas = {};
    VkResult result = vmaCreateImage(allocator, &imageInfo, &allocInfo, &atlas.image, &atlas.allocation, nullptr);

    if (result == VK_SUCCESS) {
        atlas.width = width;
        atlas.height = height;
    }

    return atlas;
}
```

### Загрузка текстур через staging

```cpp
void uploadTextureToAtlas(VmaAllocator allocator, VkCommandBuffer cmdBuffer,
                         VkImage atlasImage, const void* textureData,
                         uint32_t offsetX, uint32_t offsetY,
                         uint32_t textureWidth, uint32_t textureHeight) {
    // Создание staging буфера
    VkDeviceSize stagingSize = textureWidth * textureHeight * 4; // RGBA8
    VkBufferCreateInfo stagingInfo = {};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = stagingSize;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo stagingAllocInfo = {};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                            VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;
    VmaAllocationInfo stagingAllocInfoResult;
    vmaCreateBuffer(allocator, &stagingInfo, &stagingAllocInfo,
                    &stagingBuffer, &stagingAllocation, &stagingAllocInfoResult);

    // Копирование данных в staging
    memcpy(stagingAllocInfoResult.pMappedData, textureData, stagingSize);
    vmaFlushAllocation(allocator, stagingAllocation, 0, VK_WHOLE_SIZE);

    // Копирование из staging в атлас
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {static_cast<int32_t>(offsetX), static_cast<int32_t>(offsetY), 0};
    region.imageExtent = {textureWidth, textureHeight, 1};

    vkCmdCopyBufferToImage(cmdBuffer, stagingBuffer, atlasImage,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Освобождение staging (после выполнения команд)
    // В реальном коде нужно дождаться завершения команд
    vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
}
```

---

## Compute buffers для воксельных алгоритмов

Compute shaders в ProjectV используются для генерации мешей, маркировки вокселей и других алгоритмов.

### Создание compute buffers

```cpp
struct ComputeBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VkDeviceSize size;
};

ComputeBuffer createComputeBuffer(VmaAllocator allocator, VkDeviceSize size, bool hostVisible) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (hostVisible) {
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    } else {
        // GPU-only, данные загружаются через staging
        bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    ComputeBuffer result = {};
    result.size = size;
    VkResult vkResult = vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
                                       &result.buffer, &result.allocation, nullptr);

    return result;
}
```

### Double buffering для compute алгоритмов

Для алгоритмов, которые обновляются каждый кадр (например, маркировка вокселей), используйте double buffering:

```cpp
class ComputeDoubleBuffer {
public:
    ComputeDoubleBuffer(VmaAllocator allocator, VkDeviceSize size)
        : allocator_(allocator), size_(size), currentIndex_(0) {
        for (int i = 0; i < 2; ++i) {
            buffers_[i] = createComputeBuffer(allocator, size, false);
        }
    }

    VkBuffer getCurrent() const { return buffers_[currentIndex_].buffer; }
    VkBuffer getPrevious() const { return buffers_[1 - currentIndex_].buffer; }

    void swap() { currentIndex_ = 1 - currentIndex_; }

private:
    VmaAllocator allocator_;
    VkDeviceSize size_;
    ComputeBuffer buffers_[2];
    int currentIndex_;
};
```

---

## Интеграция с Tracy для профилирования памяти

Tracy — мощный инструмент профилирования реального времени, который можно использовать для мониторинга использования
памяти VMA в ProjectV.

### Подключение Tracy в CMake

```cmake
# В корневом CMakeLists.txt
add_subdirectory(external/tracy)

target_link_libraries(ProjectV PRIVATE Tracy::TracyClient)

# Для включения профилирования VMA
add_definitions(-DTRACY_ENABLE)
```

### Инструментирование функций VMA

```cpp
#ifdef TRACY_ENABLE
#include "Tracy.hpp"
#endif

// Обёртка для vmaCreateBuffer с профилированием
VkResult vmaCreateBufferTraced(VmaAllocator allocator,
                               const VkBufferCreateInfo* pBufferCreateInfo,
                               const VmaAllocationCreateInfo* pAllocationCreateInfo,
                               VkBuffer* pBuffer,
                               VmaAllocation* pAllocation,
                               VmaAllocationInfo* pAllocationInfo) {
#ifdef TRACY_ENABLE
    ZoneScopedN("vmaCreateBuffer");
#endif
    VkResult result = vmaCreateBuffer(allocator, pBufferCreateInfo,
                                      pAllocationCreateInfo, pBuffer, pAllocation, pAllocationInfo);
#ifdef TRACY_ENABLE
    if (result == VK_SUCCESS && pAllocationInfo) {
        TracyPlot("VMA Memory Allocated (MB)", float(pAllocationInfo->size) / (1024.0f * 1024.0f));
    }
#endif
    return result;
}

// Обёртка для vmaDestroyBuffer
void vmaDestroyBufferTraced(VmaAllocator allocator, VkBuffer buffer, VmaAllocation allocation) {
#ifdef TRACY_ENABLE
    ZoneScopedN("vmaDestroyBuffer");
    if (allocation) {
        VmaAllocationInfo info;
        vmaGetAllocationInfo(allocator, allocation, &info);
        TracyPlot("VMA Memory Freed (MB)", float(info.size) / (1024.0f * 1024.0f));
    }
#endif
    vmaDestroyBuffer(allocator, buffer, allocation);
}
```

### Мониторинг статистики пулов и бюджетов

```cpp
void logVmaStatistics(VmaAllocator allocator) {
#ifdef TRACY_ENABLE
    ZoneScopedN("VMA Statistics");

    // Получение статистики по всем пулам
    VmaStatInfo statInfo;
    vmaCalculateStatistics(allocator, &statInfo);

    TracyPlot("VMA Total Allocations", float(statInfo.allocationCount));
    TracyPlot("VMA Total Memory (MB)", float(statInfo.total.usedBytes) / (1024.0f * 1024.0f));
    TracyPlot("VMA Free Memory (MB)", float(statInfo.total.unusedBytes) / (1024.0f * 1024.0f));

    // Бюджет памяти если включено расширение VK_EXT_memory_budget
    VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets(allocator, budgets);

    for (uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; ++i) {
        if (budgets[i].budget > 0) {
            float usagePercent = (float(budgets[i].usage) / float(budgets[i].budget)) * 100.0f;
            TracyPlotConfig("VMA Heap Usage %", tracy::PlotFormatType::Percentage);
            TracyPlot("VMA Heap Usage %", usagePercent);
        }
    }
#endif
}
```

### Отслеживание аллокаций по типам для воксельного движка

```cpp
enum class VoxelMemoryCategory {
    Chunks,
    Textures,
    ComputeBuffers,
    UniformBuffers,
    StagingBuffers,
    Count
};

// Глобальные счётчики по категориям
std::array<size_t, static_cast<size_t>(VoxelMemoryCategory::Count)> g_categoryAllocations{{0}};
std::array<size_t, static_cast<size_t>(VoxelMemoryCategory::Count)> g_categoryMemory{{0}};

// Создание буфера с категоризацией
VkResult createVoxelBuffer(VmaAllocator allocator,
                          VkBufferCreateInfo bufferInfo,
                          VmaAllocationCreateInfo allocInfo,
                          VoxelMemoryCategory category,
                          VkBuffer* buffer,
                          VmaAllocation* allocation) {
#ifdef TRACY_ENABLE
    ZoneScopedN("createVoxelBuffer");
    TracyPlotConfig("Voxel Memory Category", tracy::PlotFormatType::Memory);
#endif

    VkResult result = vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, buffer, allocation, nullptr);

    if (result == VK_SUCCESS) {
        VmaAllocationInfo info;
        vmaGetAllocationInfo(allocator, *allocation, &info);

        size_t catIdx = static_cast<size_t>(category);
        g_categoryAllocations[catIdx]++;
        g_categoryMemory[catIdx] += info.size;

#ifdef TRACY_ENABLE
        TracyPlot("Voxel Memory Category", float(info.size));
#endif
    }

    return result;
}
```

---

## Интеграция с ECS (flecs)

Для управления GPU ресурсами через ECS можно создать компоненты, содержащие VMA аллокации.

### Компоненты для VMA ресурсов

```cpp
// Компоненты для управления GPU ресурсами
struct VmaBufferComponent {
    VkBuffer buffer;
    VmaAllocation allocation;
    VkDeviceSize size;
};

struct VmaImageComponent {
    VkImage image;
    VmaAllocation allocation;
    VkExtent3D extent;
    VkFormat format;
};

// Система для очистки GPU ресурсов
class VmaResourceCleanupSystem : public flecs::system {
public:
    VmaResourceCleanupSystem(flecs::world& world, VmaAllocator allocator)
        : flecs::system(world), allocator_(allocator) {

        // Observer для очистки буферов
        world.observer<VmaBufferComponent>()
            .event(flecs::OnRemove)
            .each([this](VmaBufferComponent& c) {
                if (c.buffer != VK_NULL_HANDLE && c.allocation != VK_NULL_HANDLE) {
                    vmaDestroyBuffer(allocator_, c.buffer, c.allocation);
                    c.buffer = VK_NULL_HANDLE;
                    c.allocation = VK_NULL_HANDLE;
                }
            });

        // Observer для очистки изображений
        world.observer<VmaImageComponent>()
            .event(flecs::OnRemove)
            .each([this](VmaImageComponent& c) {
                if (c.image != VK_NULL_HANDLE && c.allocation != VK_NULL_HANDLE) {
                    vmaDestroyImage(allocator_, c.image, c.allocation);
                    c.image = VK_NULL_HANDLE;
                    c.allocation = VK_NULL_HANDLE;
                }
            });
    }

private:
    VmaAllocator allocator_;
};
```

### Система для загрузки текстур вокселей

```cpp
class VoxelTextureLoaderSystem : public flecs::system {
public:
    VoxelTextureLoaderSystem(flecs::world& world, VmaAllocator allocator)
        : flecs::system(world), allocator_(allocator) {

        kind = flecs::OnUpdate;

        query = world.query_builder<VoxelChunkComponent, VmaImageComponent>()
            .term(flecs::IsA, world.component<NeedsTextureLoading>())
            .build();
    }

    void run(iter& it) override {
        auto chunk = it.field<const VoxelChunkComponent>(1);
        auto image = it.field<VmaImageComponent>(2);

        for (int i : it) {
            if (image[i].image == VK_NULL_HANDLE) {
                // Создание текстуры для чанка
                image[i] = createVoxelChunkTexture(allocator_, chunk[i]);

                // Удаление компонента NeedsTextureLoading
                it.entity(i).remove<NeedsTextureLoading>();
            }
        }
    }

private:
    VmaAllocator allocator_;
    flecs::query<const VoxelChunkComponent, VmaImageComponent> query;

    VmaImageComponent createVoxelChunkTexture(VmaAllocator allocator, const VoxelChunkComponent& chunk) {
        // Создание текстуры для воксельного чанка
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_3D;
        imageInfo.extent = {chunk.sizeX, chunk.sizeY, chunk.sizeZ};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

        VmaImageComponent result = {};
        VkResult vkResult = vmaCreateImage(allocator, &imageInfo, &allocInfo,
                                          &result.image, &result.allocation, nullptr);

        if (vkResult == VK_SUCCESS) {
            result.extent = imageInfo.extent;
            result.format = imageInfo.format;
        }

        return result;
    }
};
```

---

## Паттерны производительности для воксельного рендеринга

### Стратегии аллокации для разных типов ресурсов

| Тип ресурса                   | Рекомендуемая стратегия VMA         | Обоснование                                                     |
|-------------------------------|-------------------------------------|-----------------------------------------------------------------|
| Воксельные чанки              | `MIN_MEMORY` + пулы                 | Долгоживущие, много объектов, важно минимизировать фрагментацию |
| Текстуры материалов           | `MIN_MEMORY` + dedicated allocation | Большие ресурсы, загружаются один раз                           |
| Uniform buffers (каждый кадр) | `MIN_TIME`                          | Частые аллокации/освобождения, важно быстродействие             |
| Staging буферы                | `MIN_TIME`                          | Временные, создаются/уничтожаются часто                         |
| Compute buffers               | Зависит от частоты обновления       | `MIN_MEMORY` для статических, `MIN_TIME` для динамических       |

### Мониторинг и отладка

1. **Включите `VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT`**: Отслеживайте использование памяти через
   `vmaGetHeapBudgets`.
2. **Интегрируйте с Tracy**: Используйте Tracy zones для профилирования аллокаций и освобождений.
3. **Статистика пулов**: Регулярно проверяйте `vmaGetPoolStatistics` для пулов чанков.
4. **Дефрагментация**: Рассмотрите использование `vmaDefragment` после освобождения множества воксельных чанков.

### Рекомендации для ProjectV

1. **Управление воксельными чанками**
  - Используйте пулы (`VmaPool`) для чанков фиксированного размера
  - Batch-аллокации: выделяйте память для нескольких чанков за один вызов
  - Рассмотрите стратегию `VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT` для долгоживущих чанков

2. **Текстуры и атласы текстур**
  - Device-local изображения с `VK_IMAGE_TILING_OPTIMAL`
  - Staging один раз, использование много раз
  - Mipmaps через compute shaders или готовые через staging

3. **Compute buffers для воксельных алгоритмов**
  - Storage buffers для compute shaders
  - GPU-only для частых обновлений
  - Persistent mapping для параметров генерации

4. **Uniform buffers для камеры и трансформаций**
  - Triple buffering для избежания конфликтов CPU-GPU
  - Persistent mapping с `VMA_ALLOCATION_CREATE_MAPPED_BIT`
  - Выравнивание по 256 байт

---

## Примеры кода в ProjectV

ProjectV содержит примеры интеграции VMA:

| Пример            | Описание                                     | Ссылка                                          |
|-------------------|----------------------------------------------|-------------------------------------------------|
| Базовый буфер VMA | Минимальный пример создания буфера через VMA | [vma_buffer.cpp](../../examples/vma_buffer.cpp) |

### Использование примера vma_buffer.cpp

Пример `vma_buffer.cpp` демонстрирует:

1. Создание аллокатора VMA с интеграцией volk
2. Выделение host-visible буфера для записи с CPU
3. Использование map/unmap для записи данных
4. Корректное освобождение ресурсов

Для ProjectV этот пример можно расширить:

- Добавить пулы для воксельных чанков
- Реализовать double buffering для uniform буферов
- Интегрировать с Tracy для профилирования

---

## Следующие шаги

1. **Интеграция в существующий код**: Добавьте VMA в систему рендеринга ProjectV
2. **Профилирование**: Настройте Tracy для мониторинга использования памяти
3. **Оптимизация**: Настройте пулы и стратегии аллокации под ваши паттерны использования
4. **Мониторинг**: Регулярно проверяйте статистику памяти через `vmaGetHeapBudgets`

---

## Связанные разделы

- [VMA Документация](README.md) — общая документация VMA
- [Vulkan Документация](../vulkan/README.md) — графика и рендеринг
- [Tracy Документация](../tracy/README.md) — профилирование производительности
- [flecs Документация](../flecs/README.md) — ECS система

← [Вернуться к основной документации VMA](README.md)

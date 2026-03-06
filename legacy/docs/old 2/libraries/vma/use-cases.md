# Сценарии использования VMA

**🟡 Уровень 2: Средний**

---

## Содержание

- [Пулы для оптимизации множественных аллокаций](#пулы-для-оптимизации-множественных-аллокаций)
- [Staging буферы для загрузки данных](#staging-буферы-для-загрузки-данных)
- [Persistent mapping для часто обновляемых данных](#persistent-mapping-для-часто-обновляемых-данных)
- [Readback буферы для чтения данных GPU→CPU](#readback-буферы-для-чтения-данных-gpu→cpu)
- [Оптимизация изображений и текстур](#оптимизация-изображений-и-текстур)
- [Выбор стратегии аллокации](#выбор-стратегии-аллокации)
- [Мониторинг и отладка](#мониторинг-и-отладка)

---

## Пулы для оптимизации множественных аллокаций

Пулы (`VmaPool`) позволяют выделять память для множества объектов одного типа, уменьшая фрагментацию и ускоряя
аллокации.

### Создание пула для объектов фиксированного размера

```cpp
// Создание пула для буферов размером 16KB
VmaPoolCreateInfo poolInfo = {};
poolInfo.memoryTypeIndex = UINT32_MAX; // Автоматический выбор типа памяти
poolInfo.blockSize = 16 * 1024 * 1024; // Блоки по 16MB
poolInfo.minBlockCount = 1;
poolInfo.maxBlockCount = 16; // Максимум 16 блоков (256MB)

VmaPool bufferPool;
VkResult result = vmaCreatePool(allocator, &poolInfo, &bufferPool);
if (result != VK_SUCCESS) {
    // Обработка ошибки
}
```

### Аллокация буферов из пула

```cpp
// Создание буфера из пула
VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
bufferInfo.size = 16 * 1024; // 16KB
bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

VmaAllocationCreateInfo allocInfo = {};
allocInfo.pool = bufferPool; // Ключевой параметр: используем пул

VkBuffer buffer;
VmaAllocation allocation;
result = vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);

// Освобождение буфера (возврат памяти в пул)
vmaDestroyBuffer(allocator, buffer, allocation);
```

### Преимущества пулов

| Преимущество                | Описание                                                        | Когда использовать                                        |
|-----------------------------|-----------------------------------------------------------------|-----------------------------------------------------------|
| **Уменьшение фрагментации** | Память выделяется блоками, внутри которых размещаются объекты   | Когда создаётся много мелких объектов одинакового размера |
| **Быстрые аллокации**       | Поиск свободного места внутри блока быстрее поиска в общей куче | Для объектов, которые часто создаются и уничтожаются      |
| **Контроль над памятью**    | Можно ограничить максимальный размер пула                       | Для управления памятью определённых типов ресурсов        |

---

## Staging буферы для загрузки данных

Staging буферы используются для передачи данных с CPU на GPU через host-visible память.

### Паттерн загрузки вершинных данных

```cpp
// 1. Создание staging буфера (CPU → GPU)
VkBufferCreateInfo stagingInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
stagingInfo.size = vertexDataSize;
stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

VmaAllocationCreateInfo stagingAllocInfo = {};
stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

VkBuffer stagingBuffer;
VmaAllocation stagingAllocation;
vmaCreateBuffer(allocator, &stagingInfo, &stagingAllocInfo, 
                &stagingBuffer, &stagingAllocation, nullptr);

// 2. Запись данных в staging буфер
void* mappedData;
vmaMapMemory(allocator, stagingAllocation, &mappedData);
memcpy(mappedData, vertexData, vertexDataSize);
vmaFlushAllocation(allocator, stagingAllocation, 0, VK_WHOLE_SIZE);
vmaUnmapMemory(allocator, stagingAllocation);

// 3. Создание GPU-only буфера
VkBufferCreateInfo gpuBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
gpuBufferInfo.size = vertexDataSize;
gpuBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

VmaAllocationCreateInfo gpuAllocInfo = {};
gpuAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

VkBuffer vertexBuffer;
VmaAllocation vertexAllocation;
vmaCreateBuffer(allocator, &gpuBufferInfo, &gpuAllocInfo, 
                &vertexBuffer, &vertexAllocation, nullptr);

// 4. Копирование staging → GPU буфер в командном буфере
// vkCmdCopyBuffer(commandBuffer, stagingBuffer, vertexBuffer, 1, &copyRegion);

// 5. Освобождение staging буфера после копирования
vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
```

### Кольцевой буфер для потоковой загрузки

```cpp
struct RingStagingBuffer {
    VkBuffer buffers[3];
    VmaAllocation allocations[3];
    size_t currentIndex = 0;
    
    void uploadData(VmaAllocator allocator, const void* data, size_t size) {
        // Запись в текущий буфер
        void* mapped;
        vmaMapMemory(allocator, allocations[currentIndex], &mapped);
        memcpy(mapped, data, size);
        vmaFlushAllocation(allocator, allocations[currentIndex], 0, VK_WHOLE_SIZE);
        vmaUnmapMemory(allocator, allocations[currentIndex]);
        
        // Переход к следующему буферу
        currentIndex = (currentIndex + 1) % 3;
    }
};
```

---

## Persistent mapping для часто обновляемых данных

Persistent mapping позволяет избежать вызовов `vmaMapMemory`/`vmaUnmapMemory` каждый кадр.

### Uniform буферы для матриц камеры

```cpp
// Создание uniform буфера с persistent mapping
VkBufferCreateInfo uniformInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
uniformInfo.size = sizeof(UniformData);
uniformInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

VmaAllocationCreateInfo uniformAllocInfo = {};
uniformAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
uniformAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                        VMA_ALLOCATION_CREATE_MAPPED_BIT;

VkBuffer uniformBuffer;
VmaAllocation uniformAllocation;
VmaAllocationInfo allocInfo;
vmaCreateBuffer(allocator, &uniformInfo, &uniformAllocInfo, 
                &uniformBuffer, &uniformAllocation, &allocInfo);

// Указатель доступен сразу без вызова vmaMapMemory
UniformData* uniformData = static_cast<UniformData*>(allocInfo.pMappedData);

// Обновление данных каждый кадр
uniformData->viewMatrix = camera.getViewMatrix();
uniformData->projectionMatrix = camera.getProjectionMatrix();

// Если память не host-coherent, нужен flush
vmaFlushAllocation(allocator, uniformAllocation, 0, VK_WHOLE_SIZE);
```

### Triple buffering для избежания конфликтов CPU-GPU

```cpp
struct TripleBufferedUniform {
    VkBuffer buffers[3];
    VmaAllocation allocations[3];
    VmaAllocationInfo allocInfos[3];
    uint32_t currentFrame = 0;
    
    void init(VmaAllocator allocator, size_t size) {
        VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        
        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                         VMA_ALLOCATION_CREATE_MAPPED_BIT;
        
        for (int i = 0; i < 3; ++i) {
            vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, 
                           &buffers[i], &allocations[i], &allocInfos[i]);
        }
    }
    
    void* getCurrentMappedData() {
        return allocInfos[currentFrame].pMappedData;
    }
    
    void advanceFrame() {
        currentFrame = (currentFrame + 1) % 3;
    }
};
```

---

## Readback буферы для чтения данных GPU→CPU

Readback буферы используются для получения результатов вычислений с GPU.

### Чтение результатов compute shader

```cpp
// Создание readback буфера
VkBufferCreateInfo readbackInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
readbackInfo.size = resultDataSize;
readbackInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

VmaAllocationCreateInfo readbackAllocInfo = {};
readbackAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
readbackAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

VkBuffer readbackBuffer;
VmaAllocation readbackAllocation;
vmaCreateBuffer(allocator, &readbackInfo, &readbackAllocInfo, 
                &readbackBuffer, &readbackAllocation, nullptr);

// После того как GPU записал данные в буфер...
// 1. Копирование GPU → readback буфер в командном буфере
// 2. Ожидание завершения копирования

// Чтение данных с CPU
void* mappedData;
vmaMapMemory(allocator, readbackAllocation, &mappedData);
vmaInvalidateAllocation(allocator, readbackAllocation, 0, VK_WHOLE_SIZE);
// Теперь можно читать данные
processResults(static_cast<const ResultData*>(mappedData));
vmaUnmapMemory(allocator, readbackAllocation);
```

### Упрощённый подход с vmaCopyAllocationToMemory

```cpp
// Альтернатива: копирование в одну операцию
std::vector<uint8_t> cpuBuffer(resultDataSize);
vmaCopyAllocationToMemory(allocator, readbackAllocation, 0,
                         cpuBuffer.data(), resultDataSize);
// Данные уже скопированы в cpuBuffer
```

---

## Оптимизация изображений и текстур

VMA упрощает управление памятью для изображений Vulkan.

### Создание текстур с оптимальным tiling

```cpp
// Создание texture atlas
VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
imageInfo.imageType = VK_IMAGE_TYPE_2D;
imageInfo.extent = { 2048, 2048, 1 };
imageInfo.mipLevels = 1;
imageInfo.arrayLayers = 1;
imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

VmaAllocationCreateInfo imageAllocInfo = {};
imageAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

VkImage textureImage;
VmaAllocation textureAllocation;
vmaCreateImage(allocator, &imageInfo, &imageAllocInfo, 
               &textureImage, &textureAllocation, nullptr);
```

### Dedicated allocation для больших ресурсов

```cpp
// Для очень больших текстур (4K+) может быть полезно dedicated allocation
VmaAllocationCreateInfo dedicatedAllocInfo = {};
dedicatedAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
dedicatedAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

// VMA автоматически использует VK_KHR_dedicated_allocation если доступно
```

---

## Выбор стратегии аллокации

VMA предоставляет две основные стратегии выбора блока памяти.

### Сравнение стратегий

| Стратегия                                                        | Описание                                 | Преимущества                                           | Недостатки                        | Когда использовать                                  |
|------------------------------------------------------------------|------------------------------------------|--------------------------------------------------------|-----------------------------------|-----------------------------------------------------|
| **MIN_MEMORY** (`VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT`) | Выбирает самый маленький подходящий блок | Минимизирует фрагментацию, лучшее использование памяти | Медленнее поиск подходящего блока | Долгоживущие ресурсы (текстуры, меши)               |
| **MIN_TIME** (`VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT`)     | Выбирает первый подходящий блок          | Быстрые аллокации, меньше накладных расходов CPU       | Может увеличить фрагментацию      | Временные ресурсы (staging буферы, frame resources) |

### Пример выбора стратегии

```cpp
// Для долгоживущих текстур
VmaAllocationCreateInfo textureAllocInfo = {};
textureAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
textureAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;

// Для временных staging буферов
VmaAllocationCreateInfo stagingAllocInfo = {};
stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                        VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT;
```

---

## Мониторинг и отладка

VMA предоставляет инструменты для мониторинга использования памяти.

### Получение статистики

```cpp
// Статистика по всему аллокатору
VmaStatInfo statInfo;
vmaCalculateStatistics(allocator, &statInfo);

// Статистика по пулу
VmaPoolStatistics poolStats;
vmaGetPoolStatistics(allocator, pool, &poolStats);
```

### Бюджет памяти

```cpp
// Требует VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT при создании аллокатора
VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
vmaGetHeapBudgets(allocator, budgets);

for (uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; ++i) {
    if (budgets[i].budget > 0) {
        float usagePercent = (float(budgets[i].usage) / float(budgets[i].budget)) * 100.0f;
        if (usagePercent > 90.0f) {
            // Близко к исчерпанию бюджета
        }
    }
}
```

---

## Следующие шаги

1. **Интеграция**: Подключите VMA к вашему проекту через [Интеграция](integration.md)
2. **Быстрый старт**: Создайте первый буфер через [Быстрый старт](quickstart.md)
3. **Углублённое изучение**: Изучите [Основные понятия](concepts.md) для понимания паттернов памяти
4. **Проектная интеграция**: Для специфичных сценариев воксельного рендеринга
   смотрите [projectv-integration.md](projectv-integration.md)

---

## Связанные разделы

- [Основные понятия](concepts.md) — паттерны управления памятью Vulkan
- [Быстрый старт](quickstart.md) — минимальные работающие примеры
- [Справочник API](api-reference.md) — полное описание функций VMA
- [Решение проблем](troubleshooting.md) — отладка и устранение ошибок
- [Глоссарий](glossary.md) — термины и определения

← [Назад к документации VMA](README.md)
# Основные понятия VMA

**🟡 Уровень 2: Средний**

Краткое введение в управление памятью Vulkan и паттерны использования VMA.

---

## Зачем нужен VMA

В Vulkan память выделяется вручную:

1. Создаёте буфер/изображение (`vkCreateBuffer` / `vkCreateImage`)
2. Запрашиваете требования к памяти (`vkGetBufferMemoryRequirements`)
3. Выбираете тип памяти из `VkPhysicalDeviceMemoryProperties`
4. Выделяете блок (`vkAllocateMemory`)
5. Привязываете его к ресурсу (`vkBindBufferMemory`)

VMA делает это автоматически: одна функция `vmaCreateBuffer` или `vmaCreateImage` создаёт и ресурс, и подходящую память.

---

## Типы памяти GPU

Память видеокарты разделена на типы (memory types) с разными свойствами:

### Device-local

Быстрая память на GPU. Идеальна для вершин, индексов, текстур, depth buffer. CPU не имеет прямого доступа.

```
VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
```

### Host-visible

Память, которую CPU может отображать через `vkMapMemory`. Обычно медленнее для GPU. Используется для staging и
uniform-буферов.

```
VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
```

### Host-coherent vs Host-cached

**Host-coherent** (`VK_MEMORY_PROPERTY_HOST_COHERENT_BIT`):

- Кэш CPU и GPU синхронизирован автоматически
- Flush/invalidate не нужны

**Host-cached** (`VK_MEMORY_PROPERTY_HOST_CACHED_BIT`):

- Кэшируется для быстрого чтения с CPU
- Обычно не coherent
- Требует `vmaFlushAllocation` после записи CPU
- Требует `vmaInvalidateAllocation` перед чтением CPU

---

## Паттерн: GPU-only ресурс

Ресурс только для GPU: CPU один раз загружает данные через staging и больше не обращается.

```cpp
VkBufferCreateInfo bufferInfo = {};
bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
bufferInfo.size = vertexDataSize;
bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

VmaAllocationCreateInfo allocInfo = {};
allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

VkBuffer buffer;
VmaAllocation allocation;
vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);
```

---

## Паттерн: Staging (загрузка CPU → GPU)

Для заполнения device-local буфера или изображения:

1. **Staging-буфер** — host-visible, `TRANSFER_SRC_BIT`
2. **Целевой буфер/изображение** — device-local, `TRANSFER_DST_BIT`
3. Отобразить staging, записать данные, flush (если не coherent)
4. В командном буфере: `vkCmdCopyBuffer` или `vkCmdCopyBufferToImage`
5. Уничтожить или переиспользовать staging

```cpp
// Staging-буфер
VkBufferCreateInfo stagingInfo = {};
stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
stagingInfo.size = dataSize;
stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

VmaAllocationCreateInfo stagingAllocInfo = {};
stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

VkBuffer stagingBuffer;
VmaAllocation stagingAllocation;
vmaCreateBuffer(allocator, &stagingInfo, &stagingAllocInfo,
                &stagingBuffer, &stagingAllocation, nullptr);

// Запись данных
void* mappedData;
vmaMapMemory(allocator, stagingAllocation, &mappedData);
memcpy(mappedData, sourceData, dataSize);
vmaFlushAllocation(allocator, stagingAllocation, 0, VK_WHOLE_SIZE);
vmaUnmapMemory(allocator, stagingAllocation);

// Копирование в GPU-only буфер (в командном буфере)
// vkCmdCopyBuffer(cmd, stagingBuffer, gpuBuffer, 1, &copyRegion);

// Освобождение staging
vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
```

---

## Паттерн: Persistent mapping

Для uniform-буферов, обновляемых каждый кадр:

```cpp
VkBufferCreateInfo bufferInfo = {};
bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
bufferInfo.size = uniformDataSize;
bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

VmaAllocationCreateInfo allocInfo = {};
allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT;

VkBuffer buffer;
VmaAllocation allocation;
VmaAllocationInfo allocationInfo;
vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, &allocationInfo);

// Указатель доступен сразу
void* mappedData = allocationInfo.pMappedData;

// Обновление каждый кадр
memcpy(mappedData, &uniformData, uniformDataSize);
vmaFlushAllocation(allocator, allocation, 0, VK_WHOLE_SIZE);
```

---

## Паттерн: Readback (GPU → CPU)

Буфер для чтения данных, записанных GPU:

```cpp
VkBufferCreateInfo bufferInfo = {};
bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
bufferInfo.size = dataSize;
bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

VmaAllocationCreateInfo allocInfo = {};
allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

VkBuffer buffer;
VmaAllocation allocation;
vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);

// После того как GPU записал данные...

void* ptr;
vmaMapMemory(allocator, allocation, &ptr);
vmaInvalidateAllocation(allocator, allocation, 0, VK_WHOLE_SIZE);
// Теперь можно читать через ptr
vmaUnmapMemory(allocator, allocation);
```

---

## Стратегии аллокации

### MIN_MEMORY

Минимизирует расход памяти, выбирая самый маленький подходящий блок.

```cpp
allocInfo.flags |= VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
```

**Когда использовать:**

- Долгоживущие ресурсы (текстуры, меши)
- Системы с ограниченной видеопамятью

### MIN_TIME

Минимизирует время аллокации, выбирая первый подходящий блок.

```cpp
allocInfo.flags |= VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT;
```

**Когда использовать:**

- Временные ресурсы (staging буферы)
- Аллокации в реальном времени

---

## Пулы памяти

Пулы позволяют выделять память одного типа из предварительно созданных блоков.

```cpp
// Определение типа памяти
VkBufferCreateInfo sampleBufferInfo = {};
sampleBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
sampleBufferInfo.size = bufferSize;
sampleBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

VmaAllocationCreateInfo sampleAllocInfo = {};
sampleAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

uint32_t memoryTypeIndex;
vmaFindMemoryTypeIndexForBufferInfo(allocator, &sampleBufferInfo,
                                    &sampleAllocInfo, &memoryTypeIndex);

// Создание пула
VmaPoolCreateInfo poolInfo = {};
poolInfo.memoryTypeIndex = memoryTypeIndex;
poolInfo.blockSize = 64 * 1024 * 1024; // 64 MB
poolInfo.minBlockCount = 1;
poolInfo.maxBlockCount = 16;

VmaPool pool;
vmaCreatePool(allocator, &poolInfo, &pool);

// Аллокация из пула
VmaAllocationCreateInfo allocFromPoolInfo = {};
allocFromPoolInfo.pool = pool;

VkBuffer buffer;
VmaAllocation allocation;
vmaCreateBuffer(allocator, &sampleBufferInfo, &allocFromPoolInfo,
               &buffer, &allocation, nullptr);

// Освобождение
vmaDestroyBuffer(allocator, buffer, allocation);
vmaDestroyPool(allocator, pool);
```

**Преимущества пулов:**

- Уменьшение фрагментации
- Быстрые аллокации/освобождения
- Контроль над потреблением памяти

---

## Dedicated allocation

Отдельный блок `VkDeviceMemory` под один ресурс. VMA может использовать автоматически с `VK_KHR_dedicated_allocation`
или принудительно:

```cpp
VmaAllocationCreateInfo allocInfo = {};
allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
```

**Когда рассматривать:**

- Большие ресурсы (текстуры 4K+)
- Render target'ы

---

## Бюджет памяти

При включённом `VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT`:

```cpp
VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
vmaGetHeapBudgets(allocator, budgets);

for (uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; ++i) {
    if (budgets[i].budget > 0) {
        float usagePercent = float(budgets[i].usage) / float(budgets[i].budget) * 100.0f;
        // usagePercent — текущее использование в процентах
    }
}
```

Флаг `VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT` возвращает ошибку при превышении бюджета вместо неопределённого
поведения.

---

## Дефрагментация

VMA может перемещать аллокации для уменьшения фрагментации:

```cpp
VmaDefragmentationInfo defragInfo = {};
// Настройка параметров дефрагментации

VmaDefragmentationContext context;
vmaDefragmentationBegin(allocator, &defragInfo, nullptr, &context);
vmaDefragmentationEnd(allocator, context);
```

Полезно при большом количестве мелких аллокаций и освобождений.

---

## Общая схема

```
VkDevice создан
    │
    ▼
vmaCreateAllocator
    │
    ▼
vmaCreateBuffer / vmaCreateImage
    │
    ├── GPU-only: вершины, текстуры
    │       └── Загрузка через staging
    │
    ├── Host-visible (staging): upload CPU→GPU
    │       └── vmaMapMemory → memcpy → vmaFlushAllocation
    │
    ├── Host-visible (persistent): uniform, частые обновления
    │       └── VMA_ALLOCATION_CREATE_MAPPED_BIT
    │
    └── Host-visible (readback): GPU→CPU
            └── vmaInvalidateAllocation → vmaMapMemory → чтение
    │
    ▼
vmaDestroyBuffer / vmaDestroyImage
    │
    ▼
vmaDestroyAllocator
```

---

## VMA_MEMORY_USAGE

| Значение                                | Описание                                              |
|-----------------------------------------|-------------------------------------------------------|
| `VMA_MEMORY_USAGE_AUTO`                 | Автовыбор по usage буфера/изображения. Рекомендуется. |
| `VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE`   | Приоритет device-local.                               |
| `VMA_MEMORY_USAGE_AUTO_PREFER_HOST`     | Приоритет host-памяти.                                |
| `VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED` | Transient attachment (мобильные).                     |

Для mappable-аллокаций с `AUTO` обязательно указать флаг доступа:

- `VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT` — запись
- `VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT` — чтение или read/write

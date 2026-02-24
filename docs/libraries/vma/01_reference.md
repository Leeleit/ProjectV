# Vulkan Memory Allocator (VMA)

**VMA — это дефрагментатор хаоса в видеопамяти.** Библиотека, которая берёт на себя грязную работу по общению с
драйвером, позволяя думать о данных, а не о выравнивании памяти в 256 байт.

---

## Архитектура

VMA создаёт абстракцию поверх Vulkan Memory Model, превращая рутинные операции в единый интерфейс:

```
VkDevice
    │
    ▼
VmaAllocator ──────────────────────────────────────────┐
    │                                                   │
    ├── VmaPool (опционально)                          │
    │       │                                          │
    │       ├── VmaAllocation (буфер/изображение)      │
    │       ├── VmaAllocation                          │
    │       └── VmaAllocation                          │
    │                                                   │
    ├── VmaAllocation (буфер) ─── VkBuffer + VkDeviceMemory
    ├── VmaAllocation (изображение) ─── VkImage + VkDeviceMemory
    └── VmaAllocation (память) ─── VkDeviceMemory
```

**Ключевые объекты:**

| Объект          | Роль                                                             |
|-----------------|------------------------------------------------------------------|
| `VmaAllocator`  | Диспетчер памяти. Создаётся после `VkDevice`.                    |
| `VmaAllocation` | Handle выделенного куска. Хранит `VkDeviceMemory`, offset, size. |
| `VmaPool`       | Пользовательский пул памяти определённого типа.                  |

---

## Возможности

### Автоматический выбор типа памяти

По подсказке использования (GPU-only, CPU→GPU, GPU→CPU) VMA выбирает оптимальный memory type, учитывая:

- `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`
- `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`
- `VK_MEMORY_PROPERTY_HOST_COHERENT_BIT`
- `VK_MEMORY_PROPERTY_HOST_CACHED_BIT`

### Создание ресурсов с памятью

Одна операция вместо пяти:

```cpp
VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
bufferInfo.size = 65536;
bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

VmaAllocationCreateInfo allocInfo = {};
allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

VkBuffer buffer;
VmaAllocation allocation;
vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);
```

### Управление блоками памяти

Библиотека выделяет большие блоки `VkDeviceMemory` и раздаёт из них части, уменьшая фрагментацию.

### Пулы памяти

Пользовательские пулы для объектов одного типа — контроль над распределением памяти для группы ресурсов.

### Дефрагментация

Перемещение аллокаций для уменьшения фрагментации — полезно при большом количестве мелких аллокаций и освобождений.

### Статистика

Мониторинг использования памяти через `vmaCalculateStatistics`, `vmaGetHeapBudgets`.

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

## Паттерны использования

### GPU-only ресурс

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

### Staging (загрузка CPU → GPU)

Для заполнения device-local буфера или изображения:

1. **Staging-буфер** — host-visible, `TRANSFER_SRC_BIT`
2. **Целевой буфер/изображение** — device-local, `TRANSFER_DST_BIT`
3. Отобразить staging, записать данные, flush (если не coherent)
4. В командном буфере: `vkCmdCopyBuffer` или `vkCmdCopyBufferToImage`
5. Уничтожить или переиспользовать staging

### Persistent mapping

Для uniform-буферов, обновляемых каждый кадр:

```cpp
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
```

### Readback (GPU → CPU)

Буфер для чтения данных, записанных GPU:

```cpp
VmaAllocationCreateInfo allocInfo = {};
allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

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

## Глоссарий

| Термин                   | Определение                                                                    |
|--------------------------|--------------------------------------------------------------------------------|
| **VMA**                  | Vulkan Memory Allocator — библиотека для управления памятью в Vulkan.          |
| **VmaAllocator**         | Главный объект VMA. Создаётся через `vmaCreateAllocator()`.                    |
| **VmaAllocation**        | Handle выделенного куска памяти.                                               |
| **VmaPool**              | Пользовательский пул памяти определённого типа.                                |
| **Memory type**          | Один из вариантов памяти в `VkPhysicalDeviceMemoryProperties`.                 |
| **Memory heap**          | Физический блок видеопамяти. Один heap может содержать несколько memory types. |
| **Staging buffer**       | Временный host-visible буфер для передачи данных с CPU на GPU.                 |
| **GPU-only resource**    | Ресурс в device-local памяти: вершинные/индексные буферы, текстуры.            |
| **Persistent mapping**   | Постоянно отображённая память.                                                 |
| **Readback**             | Чтение данных с GPU на CPU.                                                    |
| **Dedicated allocation** | Отдельный блок `VkDeviceMemory` под один ресурс.                               |

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

---

## Флаги

| Термин                               | Определение                                                        |
|--------------------------------------|--------------------------------------------------------------------|
| **MAPPED_BIT**                       | `VMA_ALLOCATION_CREATE_MAPPED_BIT` — сразу получить `pMappedData`. |
| **HOST_ACCESS_SEQUENTIAL_WRITE_BIT** | Последовательная запись с CPU (staging upload).                    |
| **HOST_ACCESS_RANDOM_BIT**           | Случайный доступ с CPU (readback, uniform с read/write).           |
| **DEDICATED_MEMORY_BIT**             | Принудительная dedicated-аллокация.                                |
| **WITHIN_BUDGET_BIT**                | Возврат ошибки при превышении бюджета памяти.                      |
| **STRATEGY_MIN_MEMORY_BIT**          | Алгоритм выбора блока: минимум памяти (best-fit).                  |
| **STRATEGY_MIN_TIME_BIT**            | Алгоритм выбора блока: минимум времени (first-fit).                |

---

## Функции

| Термин                      | Определение                                                    |
|-----------------------------|----------------------------------------------------------------|
| **vmaCreateAllocator**      | Создаёт главный объект VMA после VkDevice.                     |
| **vmaDestroyAllocator**     | Уничтожает аллокатор (после освобождения всех ресурсов).       |
| **vmaCreateBuffer**         | Создаёт VkBuffer и выделяет для него память.                   |
| **vmaCreateImage**          | Создаёт VkImage и выделяет для него память.                    |
| **vmaDestroyBuffer**        | Уничтожает буфер и освобождает аллокацию.                      |
| **vmaDestroyImage**         | Уничтожает изображение и освобождает аллокацию.                |
| **vmaMapMemory**            | Отображает host-visible аллокацию в адресное пространство CPU. |
| **vmaUnmapMemory**          | Отменяет отображение памяти.                                   |
| **vmaFlushAllocation**      | Сброс кэша CPU после записи (для не-coherent памяти).          |
| **vmaInvalidateAllocation** | Инвалидация кэша CPU перед чтением (для readback).             |
| **vmaCreatePool**           | Создаёт пользовательский пул памяти.                           |
| **vmaDestroyPool**          | Уничтожает пул (все аллокации должны быть освобождены).        |
| **vmaGetHeapBudgets**       | Возвращает бюджет памяти по heap'ам.                           |
| **vmaCalculateStatistics**  | Возвращает статистику использования памяти.                    |

---

## Структуры данных

| Термин                | Определение                                                                            |
|-----------------------|----------------------------------------------------------------------------------------|
| **VmaAllocationInfo** | Информация об аллокации: `deviceMemory`, `offset`, `size`, `pMappedData`, `pUserData`. |
| **VmaBudget**         | Данные по heap'у: `usage`, `budget`, `statistics`.                                     |
| **VmaStatistics**     | Статистика: `blockCount`, `allocationCount`, `allocationBytes`, `unusedBytes`.         |
| **VmaPoolCreateInfo** | Параметры пула: `memoryTypeIndex`, `blockSize`, `minBlockCount`, `maxBlockCount`.      |

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

## Лицензия

MIT License. Copyright © 2017-2024 Advanced Micro Devices, Inc.

---

## Ссылки

- **Исходный код:
  ** [GitHub - GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
- **Документация:** [Vulkan Memory Allocator (GPUOpen)](https://gpuopen.com/vulkan-memory

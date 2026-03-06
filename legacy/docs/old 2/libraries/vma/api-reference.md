# Справочник API VMA

**🔴 Уровень 3: Продвинутый**

Краткое описание основных функций и структур VMA для игры на Vulkan. Полные объявления и комментарии —
в [vk_mem_alloc.h](../../external/VMA/include/vk_mem_alloc.h).

## Оглавление

- [Когда что использовать](#когда-что-использовать)
- [Функции — инициализация](#функции)
- [Функции — создание ресурсов](#функции)
- [Функции — Map/Unmap, Flush/Invalidate](#vmaflushallocation--vmainvalidateallocation)
- [Функции — статистика](#vmacalculatestatistics--vmagetpoolstatistics)
- [Структуры](#vmaallocatorcreateinfo)
- [Пулы](#пулы-vmapool)
- [Дефрагментация](#дефрагментация)
- [Таблицы usage и флагов](#vmaallocationcreateinfo)

---

## Когда что использовать

| Задача                                              | Функция / тип                                                                                               |
|-----------------------------------------------------|-------------------------------------------------------------------------------------------------------------|
| Создать аллокатор после VkDevice                    | `vmaCreateAllocator`                                                                                        |
| Буфер + память одной операцией                      | `vmaCreateBuffer`                                                                                           |
| Изображение + память одной операцией                | `vmaCreateImage`                                                                                            |
| Получить указатель для записи с CPU                 | `vmaMapMemory` / `vmaUnmapMemory` или `VMA_ALLOCATION_CREATE_MAPPED_BIT` + `VmaAllocationInfo::pMappedData` |
| Уничтожить буфер и освободить аллокацию             | `vmaDestroyBuffer`                                                                                          |
| Уничтожить изображение и освободить аллокацию       | `vmaDestroyImage`                                                                                           |
| Только память (буфер/изображение создаёте отдельно) | `vmaAllocateMemory` + `vmaBindBufferMemory` / `vmaBindImageMemory`; освобождение — `vmaFreeMemory`          |
| Узнать deviceMemory, offset, pMappedData            | `vmaGetAllocationInfo`                                                                                      |
| Ручной выбор типа памяти                            | `vmaFindMemoryTypeIndex` / `vmaFindMemoryTypeIndexForBufferInfo` / `vmaFindMemoryTypeIndexForImageInfo`     |
| Сброс/инвалидация кэша (не-coherent)                | `vmaFlushAllocation` (после записи с CPU), `vmaInvalidateAllocation` (перед чтением с CPU)                  |
| Узнать бюджет по heap'ам                            | `vmaGetHeapBudgets` (требует `VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT`)                                  |
| Копирование CPU→аллокация (альтернатива map+memcpy) | `vmaCopyMemoryToAllocation` — map, memcpy, unmap, flush в одном вызове                                      |
| Копирование аллокация→CPU (readback в одну функцию) | `vmaCopyAllocationToMemory` — invalidate, map, memcpy, unmap                                                |
| Статистика по аллокатору (блоки, байты)             | `vmaCalculateStatistics`                                                                                    |
| Статистика по пулу                                  | `vmaGetPoolStatistics`                                                                                      |
| Уничтожить аллокатор                                | `vmaDestroyAllocator` (после освобождения всех ресурсов)                                                    |

Подробный порядок инициализации: [Интеграция](integration.md).

---

## Функции

### vmaCreateAllocator

```c
VkResult vmaCreateAllocator(
    const VmaAllocatorCreateInfo* pCreateInfo,
    VmaAllocator* pAllocator);
```

Создаёт главный объект VMA. В `pCreateInfo` передают `physicalDevice`, `device`, `instance`, `vulkanApiVersion`,
`flags` (расширения) и при использовании volk — `pVulkanFunctions` (заполняется через
`vmaImportVulkanFunctionsFromVolk`).

**Когда использовать:** Один раз после создания `VkDevice` (и после `volkLoadDevice`, если используется volk).

- **Возвращает:** `VK_SUCCESS` или код ошибки Vulkan.

---

### vmaDestroyAllocator

```c
void vmaDestroyAllocator(VmaAllocator allocator);
```

Уничтожает аллокатор. Перед вызовом нужно уничтожить все буферы и изображения, созданные через этот аллокатор (
`vmaDestroyBuffer`, `vmaDestroyImage`), и освободить аллокации, созданные через `vmaAllocateMemory` (`vmaFreeMemory`).

---

### vmaCreateBuffer

```c
VkResult vmaCreateBuffer(
    VmaAllocator allocator,
    const VkBufferCreateInfo* pBufferCreateInfo,
    const VmaAllocationCreateInfo* pAllocationCreateInfo,
    VkBuffer* pBuffer,
    VmaAllocation* pAllocation,
    VmaAllocationInfo* pAllocationInfo);
```

Создаёт `VkBuffer` и выделяет для него память через VMA. Буфер сразу привязан к памяти. Тип памяти выбирается по
`pAllocationCreateInfo` (usage, flags, опционально pool).

**Параметры:** `pAllocationInfo` может быть `nullptr`, если не нужны данные об аллокации (их можно получить позже через
`vmaGetAllocationInfo`).

**Когда использовать:** Вершинные/индексные буферы, uniform-буферы, staging-буферы — в большинстве случаев вместо
ручного `vkCreateBuffer` + `vkAllocateMemory` + `vkBindBufferMemory`.

---

### vmaCreateImage

```c
VkResult vmaCreateImage(
    VmaAllocator allocator,
    const VkImageCreateInfo* pImageCreateInfo,
    const VmaAllocationCreateInfo* pAllocationCreateInfo,
    VkImage* pImage,
    VmaAllocation* pAllocation,
    VmaAllocationInfo* pAllocationInfo);
```

Создаёт `VkImage` и выделяет для него память. Изображение сразу привязано к памяти. Тип памяти выбирается по
`pAllocationCreateInfo` и `VkImageCreateInfo::usage`.

**Пример (GPU-only render target):**

```cpp
VkImageCreateInfo imgInfo = {};
imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
imgInfo.imageType = VK_IMAGE_TYPE_2D;
imgInfo.extent = { 1920, 1080, 1 };
imgInfo.mipLevels = 1;
imgInfo.arrayLayers = 1;
imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;

VmaAllocationCreateInfo allocInfo = {};
allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

VkImage image;
VmaAllocation allocation;
vmaCreateImage(allocator, &imgInfo, &allocInfo, &image, &allocation, nullptr);
```

**Когда использовать:** Текстуры, render target'ы, depth buffer — вместо ручного создания image и выделения памяти.
См. [Основные понятия — GPU-only](concepts.md#паттерн-gpu-only-ресурс).

---

### vmaDestroyBuffer

```c
void vmaDestroyBuffer(
    VmaAllocator allocator,
    VkBuffer buffer,
    VmaAllocation allocation);
```

Уничтожает буфер и освобождает аллокацию. Эквивалент `vkDestroyBuffer` + освобождение памяти через VMA.

---

### vmaDestroyImage

```c
void vmaDestroyImage(
    VmaAllocator allocator,
    VkImage image,
    VmaAllocation allocation);
```

Уничтожает изображение и освобождает аллокацию.

---

### vmaMapMemory / vmaUnmapMemory

```c
VkResult vmaMapMemory(
    VmaAllocator allocator,
    VmaAllocation allocation,
    void** ppData);

void vmaUnmapMemory(
    VmaAllocator allocator,
    VmaAllocation allocation);
```

`vmaMapMemory` отображает аллокацию в адресное пространство CPU и возвращает указатель в `*ppData`. Допустимо только для
аллокаций в host-visible памяти. Функции **не** выполняют сброс кэша автоматически: для памяти без
`VK_MEMORY_PROPERTY_HOST_COHERENT_BIT` после записи с CPU нужно
вызвать [vmaFlushAllocation](#vmaflushallocation--vmainvalidateallocation) перед использованием на GPU, перед чтением с
CPU — [vmaInvalidateAllocation](#vmaflushallocation--vmainvalidateallocation).

**Когда использовать:** Запись/чтение данных с CPU (staging upload, обновление uniform). Либо один раз map и держать
указатель (persistent mapping), либо map → запись → unmap на каждый upload.

---

### vmaFlushAllocation / vmaInvalidateAllocation

```c
VkResult vmaFlushAllocation(
    VmaAllocator allocator,
    VmaAllocation allocation,
    VkDeviceSize offset,
    VkDeviceSize size);

VkResult vmaInvalidateAllocation(
    VmaAllocator allocator,
    VmaAllocation allocation,
    VkDeviceSize offset,
    VkDeviceSize size);
```

**vmaFlushAllocation** — сбрасывает кэш CPU после записи в отображённую память; вызывать перед использованием аллокации
на GPU, если тип памяти не host-coherent. **vmaInvalidateAllocation** — инвалидирует кэш перед чтением с CPU после
доступа GPU (например, readback). Параметры `offset` и `size` задают область внутри аллокации (относительно начала
аллокации, не блока); `size` может быть `VK_WHOLE_SIZE`. Для всей аллокации вызывайте с `0` и `VK_WHOLE_SIZE`. Для
host-coherent памяти вызовы игнорируются. Возвращают `VK_SUCCESS` или результат `vkFlushMappedMemoryRanges` /
`vkInvalidateMappedMemoryRanges`.

---

### vmaGetHeapBudgets

```c
void vmaGetHeapBudgets(VmaAllocator allocator, VmaBudget* pBudgets);
```

Заполняет массив структур `VmaBudget` по всем heap'ам устройства (количество элементов =
`VkPhysicalDeviceMemoryProperties::memoryHeapCount`). Требует флага `VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT` при
создании аллокатора. В каждой структуре: `usage` (текущее использование в байтах), `budget` (доступный лимит),
`statistics` (VmaStatistics). Разница `budget - usage` — ориентир свободной памяти. См. [VmaBudget](#vmabudget).

---

### vmaCalculateStatistics / vmaGetPoolStatistics

```c
void vmaCalculateStatistics(VmaAllocator allocator, VmaStatistics* pStats);

void vmaGetPoolStatistics(VmaAllocator allocator, VmaPool pool, VmaStatistics* pStats);
```

**vmaCalculateStatistics** заполняет `VmaStatistics` по всему аллокатору: `blockCount`, `allocationCount`, `blockBytes`,
`allocationBytes` и др. **vmaGetPoolStatistics** — то же для указанного пула.

**Когда использовать:** Отладка, мониторинг использования памяти, профилирование.

---

### vmaCopyMemoryToAllocation / vmaCopyAllocationToMemory

```c
VkResult vmaCopyMemoryToAllocation(VmaAllocator allocator,
    const void* pSrcHostPointer, VmaAllocation dstAllocation,
    VkDeviceSize dstAllocationLocalOffset, VkDeviceSize size);

VkResult vmaCopyAllocationToMemory(VmaAllocator allocator,
    VmaAllocation srcAllocation, VkDeviceSize srcAllocationLocalOffset,
    void* pDstHostPointer, VkDeviceSize size);
```

Удобные обёртки: **vmaCopyMemoryToAllocation** выполняет map → memcpy → unmap → flush (копирование с CPU в аллокацию). *
*vmaCopyAllocationToMemory** — invalidate → map → memcpy → unmap (копирование из аллокации на CPU). Работают только с
host-visible аллокациями; для copy-to-allocation желателен sequential-write или random host access, для copy-from —
cached (random) тип.

---

### vmaGetAllocationInfo

```c
void vmaGetAllocationInfo(
    VmaAllocator allocator,
    VmaAllocation allocation,
    VmaAllocationInfo* pAllocationInfo);
```

Заполняет структуру информацией об аллокации: `deviceMemory`, `offset`, `size`, `pMappedData` (если память отображена) и
др. Удобно для получения `pMappedData` при создании с `VMA_ALLOCATION_CREATE_MAPPED_BIT` без отдельного вызова
`vmaMapMemory`.

---

### vmaFreeMemory

```c
void vmaFreeMemory(
    VmaAllocator allocator,
    VmaAllocation allocation);
```

Освобождает аллокацию, созданную через `vmaAllocateMemory` (не через `vmaCreateBuffer`/`vmaCreateImage`). Для буферов и
изображений используют `vmaDestroyBuffer` / `vmaDestroyImage`.

---

### vmaFindMemoryTypeIndex

```c
VkResult vmaFindMemoryTypeIndex(
    VmaAllocator allocator,
    uint32_t memoryTypeBits,
    const VmaAllocationCreateInfo* pAllocationCreateInfo,
    uint32_t* pMemoryTypeIndex);
```

Подбирает индекс типа памяти по битовой маске из `VkMemoryRequirements` и параметрам аллокации. Нужен при ручном
выделении памяти или при создании пула (`VmaPoolCreateInfo::memoryTypeIndex`). Для буферов/изображений удобнее
`vmaFindMemoryTypeIndexForBufferInfo` / `vmaFindMemoryTypeIndexForImageInfo`.

---

## VmaAllocatorCreateInfo

Параметры создания аллокатора. Основные поля: `flags`, `physicalDevice`, `device`, `instance`, `vulkanApiVersion`,
`pVulkanFunctions` (при volk — через `vmaImportVulkanFunctionsFromVolk`). Опционально: `preferredLargeHeapBlockSize` (
0 = по умолчанию), `pAllocationCallbacks`, `pDeviceMemoryCallbacks`, `pHeapSizeLimit` (массив лимитов в байтах по
каждому heap'у). Подробнее — [Интеграция](integration.md) и [vk_mem_alloc.h](../../external/VMA/include/vk_mem_alloc.h).

---

## VmaAllocationCreateInfo

Структура параметров при создании аллокации (буфер, изображение или `vmaAllocateMemory`).

| Поле                               | Описание                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
|------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `flags`                            | Флаги: `VMA_ALLOCATION_CREATE_MAPPED_BIT`, `VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT`, `VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT`, `VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT`, `VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT`, `VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT` (отказ при превышении бюджета), `VMA_ALLOCATION_CREATE_CAN_ALIAS_BIT` (aliasing ресурсов), `VMA_ALLOCATION_CREATE_DONT_BIND_BIT` (создать буфер/образ без привязки к аллокации), `VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT` / `STRATEGY_MIN_TIME_BIT`, `VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT` и др. |
| `usage`                            | Подсказка использования: `VMA_MEMORY_USAGE_AUTO` (рекомендуется), `VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE`, `VMA_MEMORY_USAGE_AUTO_PREFER_HOST`, `VMA_MEMORY_USAGE_GPU_ONLY`, `VMA_MEMORY_USAGE_CPU_TO_GPU`, `VMA_MEMORY_USAGE_GPU_TO_CPU`, `VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED` и др.                                                                                                                                                                                                                                                                                                                                    |
| `requiredFlags` / `preferredFlags` | Ограничения по `VkMemoryPropertyFlags` (опционально).                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| `memoryTypeBits`                   | Биты допустимых типов памяти (0 = любой).                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| `pool`                             | Пул для аллокации (если нужен пул).                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| `pUserData`                        | Произвольный указатель, сохраняется в аллокации, доступен через `VmaAllocationInfo::pUserData`.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| `priority`                         | Приоритет 0…1 при `VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT`.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |

При `VMA_MEMORY_USAGE_AUTO` для mappable-аллокаций нужно указать один из флагов доступа:
`VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT` или `VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT`.

### VMA_MEMORY_USAGE_*

| Значение                                                | Рекомендация                                                                                    |
|---------------------------------------------------------|-------------------------------------------------------------------------------------------------|
| `VMA_MEMORY_USAGE_AUTO`                                 | По умолчанию. Выбор по usage буфера/изображения. Для mappable — обязательно HOST_ACCESS_* флаг. |
| `VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE`                   | Приоритет device-local.                                                                         |
| `VMA_MEMORY_USAGE_AUTO_PREFER_HOST`                     | Приоритет host-памяти.                                                                          |
| `VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED`                 | Transient attachment на мобильных. Может отсутствовать на десктопе.                             |
| `VMA_MEMORY_USAGE_CPU_COPY`                             | Промежуточный staging, host-visible.                                                            |
| `VMA_MEMORY_USAGE_GPU_ONLY`, `CPU_TO_GPU`, `GPU_TO_CPU` | Deprecated, но рабочие.                                                                         |

### Основные флаги VMA_ALLOCATION_CREATE_*

| Флаг                               | Назначение                                              |
|------------------------------------|---------------------------------------------------------|
| `MAPPED_BIT`                       | Сразу получить `pMappedData` без вызова `vmaMapMemory`. |
| `HOST_ACCESS_SEQUENTIAL_WRITE_BIT` | Последовательная запись с CPU (staging).                |
| `HOST_ACCESS_RANDOM_BIT`           | Случайный доступ с CPU (readback, uniform).             |
| `DEDICATED_MEMORY_BIT`             | Принудительная dedicated-аллокация.                     |
| `WITHIN_BUDGET_BIT`                | Отказ при превышении бюджета.                           |
| `STRATEGY_MIN_MEMORY_BIT`          | Алгоритм: минимум памяти (best-fit).                    |
| `STRATEGY_MIN_TIME_BIT`            | Алгоритм: минимум времени (first-fit).                  |

---

## VmaAllocationInfo

Возвращается из `vmaGetAllocationInfo` (или в `pAllocationInfo` при `vmaCreateBuffer`/`vmaCreateImage`).

| Поле           | Описание                                                                                                                      |
|----------------|-------------------------------------------------------------------------------------------------------------------------------|
| `memoryType`   | Индекс типа памяти Vulkan.                                                                                                    |
| `deviceMemory` | Handle `VkDeviceMemory` блока.                                                                                                |
| `offset`       | Смещение внутри блока (в байтах).                                                                                             |
| `size`         | Размер аллокации.                                                                                                             |
| `pMappedData`  | Указатель на отображённую память (если map был выполнен или использован `VMA_ALLOCATION_CREATE_MAPPED_BIT`). Иначе `nullptr`. |
| `pUserData`    | Указатель, заданный в `VmaAllocationCreateInfo::pUserData` или через `vmaSetAllocationUserData`.                              |
| `pName`        | Имя аллокации, заданное через `vmaSetAllocationName` (или устаревший способ с `USER_DATA_COPY_STRING_BIT`).                   |

---

## VmaBudget

Структура с данными по одному heap'у. Поля: `statistics` (VmaStatistics — количество блоков, аллокаций, байты),
`usage` (текущее использование в байтах, с учётом VK_EXT_memory_budget при включённом флаге), `budget` (доступный лимит
в байтах). Заполняется через `vmaGetHeapBudgets(allocator, pBudgets)`.

---

## Пулы (VmaPool)

Пулы позволяют выделять память одного типа из предварительно созданных блоков. **vmaCreatePool** создаёт пул по
`VmaPoolCreateInfo`: `memoryTypeIndex` (индекс типа памяти, можно получить через `vmaFindMemoryTypeIndexForBufferInfo`/
`ForImageInfo`), `blockSize` (размер блока в байтах, 0 = по умолчанию), `minBlockCount`, `maxBlockCount`, `flags` (
например, `VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT` для линейного аллокатора). При создании аллокации укажите
`VmaAllocationCreateInfo::pool` — поля `usage`, `requiredFlags` и т.д. игнорируются. **vmaDestroyPool** уничтожает пул и
освобождает все блоки (все аллокации из пула должны быть освобождены до этого).

---

## Дефрагментация

VMA может перемещать аллокации внутри блоков для уменьшения фрагментации.

**Основные функции:**

- `vmaDefragment` — один проход дефрагментации с копированием через приложение.
- `vmaBeginDefragmentationPass` / `vmaEndDefragmentationPass` — многошаговая дефрагментация.
- `vmaDefragmentationBegin` / `vmaDefragmentationEnd` — альтернативный API.

Приложение получает список перемещений и выполняет копирование данных в новое место. Для простой игры обычно не
требуется; полезно при большом количестве мелких аллокаций и освобождений. Детали —
в [vk_mem_alloc.h](../../external/VMA/include/vk_mem_alloc.h)
и [официальной документации VMA](https://gpuopen.com/vulkan-memory-allocator/).

---

## См. также

- [Интеграция](integration.md) — создание аллокатора с volk, флаги аллокатора.
- [Основные понятия](concepts.md) — паттерны использования памяти.
- [Быстрый старт](quickstart.md) — минимальный пример.

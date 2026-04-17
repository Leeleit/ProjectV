# Быстрый старт

**🟡 Уровень 2: Средний**

Минимальный пример: создание аллокатора VMA (с volk), выделение буфера, запись данных через map/unmap, освобождение.

**Пример в репозитории:** [docs/examples/vma_buffer.cpp](../examples/vma_buffer.cpp)

## Оглавление

- [Что нужно перед началом](#что-нужно-перед-началом)
- [Шаг 1: Реализация VMA](#шаг-1-один-cpp-с-реализацией-vma)
- [Шаг 2: Создание аллокатора](#шаг-2-создание-аллокатора-после-vkdevice)
- [Шаг 3: Создание буфера (host-visible)](#шаг-3-создание-буфера-host-visible-для-записи-с-cpu)
- [Пример 2: GPU-only буфер](#пример-2-gpu-only-буфер-вершины-индексы)
- [Пример 3: Создание изображения (текстура вокселей)](#пример-3-создание-изображения-текстура-вокселей)
- [Пример 4: Persistent mapping для uniform buffer (матрицы вида/проекции)](#пример-4-persistent-mapping-для-uniform-buffer-матрицы-видапроекции)
- [Пример 5: Использование пулов для воксельных чанков](#пример-5-использование-пулов-для-воксельных-чанков)
- [Шаг 4: Запись данных](#шаг-4-запись-данных-map--write--unmap)
- [Шаг 5: Освобождение](#шаг-5-освобождение)
- [Таблица: usage и флаги по задаче](#таблица-usage-и-флаги-по-задаче)
- [Таблица: Паттерны для воксельного рендеринга](#таблица-паттерны-для-воксельного-рендеринга)

---

## Что нужно перед началом

- Vulkan уже инициализирован: есть `VkInstance`, `VkPhysicalDevice`, `VkDevice` (
  см. [volk — Интеграция](../volk/integration.md)).
- В одном .cpp определён `VMA_IMPLEMENTATION` и подключён заголовок VMA (см. [Интеграция](integration.md)).
- В начале каждой единицы трансляции с Vulkan/VMA: `#define VK_NO_PROTOTYPES` и `#include "volk.h"` до любого include,
  тянущего vulkan.h.

---

## Шаг 1: Один .cpp с реализацией VMA

В **одном** файле (например, `vma_init.cpp` или в начале `main.cpp`):

```cpp
#define VK_NO_PROTOTYPES
#include "volk.h"

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"
```

Остальные файлы подключают только `volk.h` и `vk_mem_alloc.h` **без** `VMA_IMPLEMENTATION`.

---

## Шаг 2: Создание аллокатора (после VkDevice)

```cpp
VmaAllocatorCreateInfo allocInfo = {};
allocInfo.physicalDevice = physicalDevice;
allocInfo.device = device;
allocInfo.instance = instance;
allocInfo.vulkanApiVersion = VK_API_VERSION_1_2;  // или ваша версия

#ifdef VOLK_HEADER_VERSION
VmaVulkanFunctions vulkanFunctions = {};
vmaImportVulkanFunctionsFromVolk(&allocInfo, &vulkanFunctions);
allocInfo.pVulkanFunctions = &vulkanFunctions;
#endif

VmaAllocator allocator = VK_NULL_HANDLE;
VkResult result = vmaCreateAllocator(&allocInfo, &allocator);
if (result != VK_SUCCESS) {
    // обработка ошибки
}
```

Флаги `allocInfo.flags` (например, `VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT`) добавляют при включённых расширениях
Vulkan — см. [Интеграция](integration.md).

---

## Шаг 3: Создание буфера (host-visible, для записи с CPU)

Пример: буфер на 1024 байта для записи с CPU (staging или uniform):

```cpp
VkBufferCreateInfo bufferInfo = {};
bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
bufferInfo.size = 1024;
bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;  // для staging — источник копирования

VmaAllocationCreateInfo allocCreateInfo = {};
allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

VkBuffer buffer = VK_NULL_HANDLE;
VmaAllocation allocation = VK_NULL_HANDLE;
result = vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &buffer, &allocation, nullptr);
if (result != VK_SUCCESS) {
    // обработка ошибки
}
```

**Важно:** При `VMA_MEMORY_USAGE_AUTO` VMA учитывает `VkBufferCreateInfo::usage`. Staging-буфер — `TRANSFER_SRC_BIT`;
целевой GPU-буфер — `VERTEX_BUFFER_BIT | TRANSFER_DST_BIT`; readback — `TRANSFER_DST_BIT`.

---

## Пример 2: GPU-only буфер (вершины, индексы)

Буфер, в который данные попадают только через `vkCmdCopyBuffer` (из staging). CPU его не отображает:

```cpp
VkBufferCreateInfo bufferInfo = {};
bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
bufferInfo.size = vertexDataSize;
bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

VmaAllocationCreateInfo allocCreateInfo = {};
allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
// без HOST_ACCESS-флагов — VMA выберет device-local

VkBuffer vertexBuffer = VK_NULL_HANDLE;
VmaAllocation vertexAllocation = VK_NULL_HANDLE;
vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &vertexBuffer, &vertexAllocation, nullptr);
```

Данные загружаются через staging: создать staging с `TRANSFER_SRC_BIT` + `HOST_ACCESS_SEQUENTIAL_WRITE_BIT`, записать в
него, выполнить `vkCmdCopyBuffer`, затем уничтожить staging.
См. [Основные понятия — Staging](concepts.md#паттерн-staging-загрузка-cpu--gpu).

---

## Пример 3: Создание изображения (текстура вокселей)

Изображения (текстуры) для вокселей создаются через `vmaCreateImage`. Типичный сценарий: загрузка данных CPU через
staging буфер, копирование в device-local изображение.

```cpp
// Создание изображения для текстуры вокселей (RGBA 8-bit, 512x512)
VkImageCreateInfo imageInfo = {};
imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
imageInfo.imageType = VK_IMAGE_TYPE_2D;
imageInfo.extent = {512, 512, 1};
imageInfo.mipLevels = 1;
imageInfo.arrayLayers = 1;
imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

VmaAllocationCreateInfo allocCreateInfo = {};
allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
// Без HOST_ACCESS — VMA выберет device-local для оптимального tiling

VkImage textureImage = VK_NULL_HANDLE;
VmaAllocation textureAllocation = VK_NULL_HANDLE;
VkResult result = vmaCreateImage(allocator, &imageInfo, &allocCreateInfo, &textureImage, &textureAllocation, nullptr);
if (result != VK_SUCCESS) {
    // обработка ошибки
}
```

**Загрузка данных:** Для загрузки текстуры создайте staging буфер (`TRANSFER_SRC_BIT` +
`HOST_ACCESS_SEQUENTIAL_WRITE_BIT`),
запишите туда данные, затем выполните `vkCmdCopyBufferToImage`. После копирования переведите изображение в
`VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`.

---

## Пример 4: Persistent mapping для uniform buffer (матрицы вида/проекции)

Uniform буферы, которые обновляются каждый кадр, удобно держать постоянно отображёнными (persistent mapping). Это
избавляет от вызовов `vmaMapMemory`/`vmaUnmapMemory` каждый кадр.

```cpp
// Создание uniform буфера для матриц (пример: 2 матрицы 4x4 = 128 байт)
VkBufferCreateInfo bufferInfo = {};
bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
bufferInfo.size = sizeof(float) * 16 * 2; // 2 матрицы 4x4
bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

VmaAllocationCreateInfo allocCreateInfo = {};
allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | 
                       VMA_ALLOCATION_CREATE_MAPPED_BIT;

VkBuffer uniformBuffer = VK_NULL_HANDLE;
VmaAllocation uniformAllocation = VK_NULL_HANDLE;
VmaAllocationInfo allocInfo = {};
result = vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, 
                         &uniformBuffer, &uniformAllocation, &allocInfo);
if (result != VK_SUCCESS) {
    // обработка ошибки
}

// Указатель на данные доступен сразу без вызова vmaMapMemory
void* mappedData = allocInfo.pMappedData;
// Сохраняем указатель для использования каждый кадр

// Обновление данных каждый кадр:
// memcpy(mappedData, &viewMatrix, sizeof(float) * 16);
// memcpy((char*)mappedData + sizeof(float) * 16, &projectionMatrix, sizeof(float) * 16);

// Если память не host-coherent, перед использованием на GPU:
// vmaFlushAllocation(allocator, uniformAllocation, 0, VK_WHOLE_SIZE);
```

**Важно:** Для частых обновлений используйте double или triple buffering, чтобы избежать конфликтов между CPU и GPU.

---

## Пример 5: Использование пулов для воксельных чанков

Для выделения множества мелких буферов (например, воксельных чанков фиксированного размера) эффективно использовать
пулы (`VmaPool`).

```cpp
// Создание пула для чанков 16KB
VmaPoolCreateInfo poolInfo = {};
poolInfo.memoryTypeIndex = /* получите через vmaFindMemoryTypeIndexForBufferInfo */;
poolInfo.blockSize = 1024 * 1024; // 1 MB блоки
poolInfo.minBlockCount = 1;
poolInfo.maxBlockCount = 0; // без ограничения
poolInfo.flags = 0;

VmaPool chunkPool = VK_NULL_HANDLE;
result = vmaCreatePool(allocator, &poolInfo, &chunkPool);
if (result != VK_SUCCESS) {
    // обработка ошибки
}

// Создание буфера чанка из пула
VkBufferCreateInfo bufferInfo = {};
bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
bufferInfo.size = 16 * 1024; // 16KB чанк
bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; // для compute шейдеров

VmaAllocationCreateInfo allocCreateInfo = {};
allocCreateInfo.pool = chunkPool; // Ключевое: аллокация из пула
allocCreateInfo.flags = 0;

VkBuffer chunkBuffer = VK_NULL_HANDLE;
VmaAllocation chunkAllocation = VK_NULL_HANDLE;
result = vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, 
                         &chunkBuffer, &chunkAllocation, nullptr);

// Освобождение чанка (возврат в пул)
vmaDestroyBuffer(allocator, chunkBuffer, chunkAllocation);

// Уничтожение пула (после освобождения всех чанков)
vmaDestroyPool(allocator, chunkPool);
```

**Преимущества пулов:** уменьшение фрагментации, быстрые аллокации/освобождения для объектов одного размера.

---

## Шаг 4: Запись данных (map → write → unmap)

```cpp
void* mappedData = nullptr;
result = vmaMapMemory(allocator, allocation, &mappedData);
if (result != VK_SUCCESS) {
    // обработка ошибки
}
// Запись в mappedData (например, memcpy или по указателю)
// memcpy(mappedData, sourceData, size);

// Если память не host-coherent, перед использованием на GPU вызовите:
vmaFlushAllocation(allocator, allocation, 0, VK_WHOLE_SIZE);

vmaUnmapMemory(allocator, allocation);
```

Если память не **host-coherent**, вызов `vmaFlushAllocation` обязателен после записи с CPU, иначе GPU может не увидеть
данные. См. [Справочник API — vmaFlushAllocation](api-reference.md#vmaflushallocation--vmainvalidateallocation).

Если нужен постоянный указатель (persistent mapping), при создании буфера добавьте `VMA_ALLOCATION_CREATE_MAPPED_BIT` и
используйте `VmaAllocationInfo::pMappedData` после `vmaCreateBuffer` —
см. [Основные понятия](concepts.md#паттерн-persistent-mapping-часто-обновляемые-данные).

---

## Шаг 5: Освобождение

Перед уничтожением device:

```cpp
vmaDestroyBuffer(allocator, buffer, allocation);
vmaDestroyAllocator(allocator);
```

---

## Таблица: usage и флаги по задаче

| Задача                               | VkBufferCreateInfo::usage               | VmaAllocationCreateInfo                                                      |
|--------------------------------------|-----------------------------------------|------------------------------------------------------------------------------|
| Вершинный/индексный буфер (GPU-only) | `VERTEX_BUFFER_BIT \| TRANSFER_DST_BIT` | `usage = AUTO`, без HOST_ACCESS                                              |
| Staging для upload                   | `TRANSFER_SRC_BIT`                      | `usage = AUTO`, `HOST_ACCESS_SEQUENTIAL_WRITE_BIT`                           |
| Uniform каждый кадр                  | `UNIFORM_BUFFER_BIT`                    | `usage = AUTO`, `HOST_ACCESS_SEQUENTIAL_WRITE_BIT`, опционально `MAPPED_BIT` |
| Readback (GPU → CPU)                 | `TRANSFER_DST_BIT`                      | `usage = AUTO`, `HOST_ACCESS_RANDOM_BIT`                                     |
| Текстура (GPU-only)                  | через vmaCreateImage с `USAGE`          | `usage = AUTO`, без HOST_ACCESS                                              |

---

## Таблица: Паттерны для воксельного рендеринга

| Задача в воксельном движке                | Паттерн VMA                                                      | Пример использования в ProjectV          |
|-------------------------------------------|------------------------------------------------------------------|------------------------------------------|
| Загрузка текстур вокселей                 | Staging buffer → `vmaCreateImage`                                | Текстуры материалов, атласы текстур      |
| Хранение воксельных чанков                | Пулы (`VmaPool`) для буферов фиксированного размера              | Чанки 16x16x16 или 32x32x32 вокселей     |
| Матрицы вида/проекции каждый кадр         | Persistent mapping с `VMA_ALLOCATION_CREATE_MAPPED_BIT`          | Uniform buffer для camera matrices       |
| Compute shaders для воксельных алгоритмов | GPU-only буферы с `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT`           | Маркировка вокселей, генерация мешей     |
| Загрузка моделей (glTF)                   | Staging для вершин/индексов, GPU-only целевые буферы             | Импорт сложных моделей в воксельный мир  |
| Дебаг-информация и профилирование         | Readback буферы с `VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT` | Статистика рендеринга, трассировка лучей |

---

## Что на каждом шаге

| Шаг                           | Действие                                                                                                                     |
|-------------------------------|------------------------------------------------------------------------------------------------------------------------------|
| VMA_IMPLEMENTATION            | В одном .cpp подтягивается реализация VMA; без этого линкер выдаст неразрешённые символы.                                    |
| vmaCreateAllocator            | Создаёт аллокатор; при использовании volk обязательно заполнить `pVulkanFunctions` через `vmaImportVulkanFunctionsFromVolk`. |
| vmaCreateBuffer               | Создаёт `VkBuffer` и выделяет для него память. При AUTO тип памяти зависит от `VkBufferCreateInfo::usage` и флагов.          |
| vmaCreateImage                | Создаёт `VkImage` и выделяет для него память. Оптимальный выбор типа памяти для текстур.                                     |
| vmaMapMemory / vmaUnmapMemory | Даёт CPU указатель на host-visible память для записи/чтения.                                                                 |
| vmaDestroyBuffer              | Уничтожает буфер и освобождает аллокацию.                                                                                    |
| vmaDestroyImage               | Уничтожает изображение и освобождает аллокацию.                                                                              |
| vmaDestroyAllocator           | Уничтожает аллокатор; вызывать после освобождения всех буферов/изображений.                                                  |

---

## Дальше

- **[Интеграция](integration.md)** — CMake, порядок include, флаги аллокатора для ProjectV.
- **[Основные понятия](concepts.md)** — GPU-only, staging, readback, persistent mapping.
- **[Справочник API](api-reference.md)** — vmaCreateImage, vmaGetAllocationInfo, пулы и др.
- **[Use Cases](use-cases.md)** — практические сценарии для воксельного рендеринга в ProjectV.

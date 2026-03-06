# Быстрый старт VMA

**🟢 Уровень 1: Базовый**

Минимальный пример: создание аллокатора, выделение буфера, запись данных через map/unmap, освобождение.

---

## Предварительные требования

- Vulkan инициализирован: есть `VkInstance`, `VkPhysicalDevice`, `VkDevice`
- В одном .cpp определён `VMA_IMPLEMENTATION` и подключён заголовок VMA

---

## Шаг 1: VMA_IMPLEMENTATION в одном .cpp

В **одном** файле проекта (например, `vma_init.cpp`):

```cpp
#define VK_NO_PROTOTYPES
#include "volk.h"

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"
```

Остальные файлы подключают только заголовки **без** `VMA_IMPLEMENTATION`.

---

## Шаг 2: Создание аллокатора

Аллокатор создаётся после `VkDevice`:

```cpp
VmaAllocatorCreateInfo allocInfo = {};
allocInfo.physicalDevice = physicalDevice;
allocInfo.device = device;
allocInfo.instance = instance;
allocInfo.vulkanApiVersion = VK_API_VERSION_1_2;

// При использовании volk
VmaVulkanFunctions vulkanFunctions = {};
vmaImportVulkanFunctionsFromVolk(&allocInfo, &vulkanFunctions);
allocInfo.pVulkanFunctions = &vulkanFunctions;

VmaAllocator allocator = VK_NULL_HANDLE;
VkResult result = vmaCreateAllocator(&allocInfo, &allocator);
if (result != VK_SUCCESS) {
    // обработка ошибки
}
```

---

## Шаг 3: Создание буфера (host-visible)

Буфер для записи с CPU (staging или uniform):

```cpp
VkBufferCreateInfo bufferInfo = {};
bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
bufferInfo.size = 1024;
bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

VmaAllocationCreateInfo allocCreateInfo = {};
allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

VkBuffer buffer = VK_NULL_HANDLE;
VmaAllocation allocation = VK_NULL_HANDLE;
result = vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &buffer, &allocation, nullptr);
```

---

## Шаг 4: Запись данных

```cpp
void* mappedData = nullptr;
result = vmaMapMemory(allocator, allocation, &mappedData);
if (result != VK_SUCCESS) {
    // обработка ошибки
}

// Запись данных
memcpy(mappedData, sourceData, 1024);

// Если память не host-coherent
vmaFlushAllocation(allocator, allocation, 0, VK_WHOLE_SIZE);

vmaUnmapMemory(allocator, allocation);
```

---

## Шаг 5: Освобождение

Перед уничтожением device:

```cpp
vmaDestroyBuffer(allocator, buffer, allocation);
vmaDestroyAllocator(allocator);
```

---

## Пример: GPU-only буфер (вершины)

Буфер, в который данные попадают через `vkCmdCopyBuffer`:

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

Данные загружаются через staging: создать staging-буфер, записать в него, выполнить `vkCmdCopyBuffer`.

---

## Пример: Изображение (текстура)

```cpp
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

VkImage textureImage = VK_NULL_HANDLE;
VmaAllocation textureAllocation = VK_NULL_HANDLE;
vmaCreateImage(allocator, &imageInfo, &allocCreateInfo, &textureImage, &textureAllocation, nullptr);
```

---

## Пример: Persistent mapping

Uniform-буферы, обновляемые каждый кадр:

```cpp
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
vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo,
               &uniformBuffer, &uniformAllocation, &allocInfo);

// Указатель доступен сразу
void* mappedData = allocInfo.pMappedData;

// Обновление каждый кадр:
// memcpy(mappedData, &viewMatrix, sizeof(float) * 16);
```

---

## Таблица: Usage и флаги по задаче

| Задача                               | VkBufferCreateInfo::usage               | VmaAllocationCreateInfo                                                      |
|--------------------------------------|-----------------------------------------|------------------------------------------------------------------------------|
| Вершинный/индексный буфер (GPU-only) | `VERTEX_BUFFER_BIT \| TRANSFER_DST_BIT` | `usage = AUTO`, без HOST_ACCESS                                              |
| Staging для upload                   | `TRANSFER_SRC_BIT`                      | `usage = AUTO`, `HOST_ACCESS_SEQUENTIAL_WRITE_BIT`                           |
| Uniform каждый кадр                  | `UNIFORM_BUFFER_BIT`                    | `usage = AUTO`, `HOST_ACCESS_SEQUENTIAL_WRITE_BIT`, опционально `MAPPED_BIT` |
| Readback (GPU → CPU)                 | `TRANSFER_DST_BIT`                      | `usage = AUTO`, `HOST_ACCESS_RANDOM_BIT`                                     |
| Текстура (GPU-only)                  | через vmaCreateImage                    | `usage = AUTO`, без HOST_ACCESS                                              |

---

## Краткая сводка шагов

| Шаг                                | Действие                                                |
|------------------------------------|---------------------------------------------------------|
| VMA_IMPLEMENTATION                 | В одном .cpp подтягивается реализация VMA               |
| vmaCreateAllocator                 | Создаёт аллокатор; при volk нужен `pVulkanFunctions`    |
| vmaCreateBuffer                    | Создаёт `VkBuffer` и выделяет память                    |
| vmaCreateImage                     | Создаёт `VkImage` и выделяет память                     |
| vmaMapMemory / vmaUnmapMemory      | Доступ к памяти с CPU                                   |
| vmaFlushAllocation                 | Сброс кэша после записи (не-coherent)                   |
| vmaInvalidateAllocation            | Инвалидация кэша перед чтением (readback)               |
| vmaDestroyBuffer / vmaDestroyImage | Уничтожает ресурс и освобождает память                  |
| vmaDestroyAllocator                | Уничтожает аллокатор (после освобождения всех ресурсов) |

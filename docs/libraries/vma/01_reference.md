## Vulkan Memory Allocator (VMA)

<!-- anchor: 00_overview -->


Vulkan Memory Allocator (VMA) — библиотека для управления памятью в Vulkan, которая упрощает выделение GPU-памяти, выбор
оптимальных типов памяти и уменьшает фрагментацию.

---

## Возможности

### Основные функции

- **Автоматический выбор типа памяти** — по подсказке использования (GPU-only, CPU→GPU, GPU→CPU)
- **Создание ресурсов с памятью** — `vmaCreateBuffer` / `vmaCreateImage` создают ресурс и выделяют память одной
  операцией
- **Управление блоками памяти** — библиотека выделяет большие блоки `VkDeviceMemory` и раздаёт из них части
- **Пулы памяти** — пользовательские пулы для объектов одного типа
- **Дефрагментация** — перемещение аллокаций для уменьшения фрагментации
- **Статистика** — мониторинг использования памяти

### Поддержка расширений Vulkan

- `VK_KHR_dedicated_allocation` — автоматические dedicated-аллокации
- `VK_EXT_memory_budget` — бюджет памяти
- `VK_KHR_buffer_device_address` — буферы с адресацией из шейдеров
- `VK_EXT_memory_priority` — приоритеты аллокаций
- `VK_AMD_device_coherent_memory`
- `VK_KHR_external_memory_win32`

---

## Архитектура

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

| Объект          | Описание                                                         |
|-----------------|------------------------------------------------------------------|
| `VmaAllocator`  | Главный объект. Создаётся после `VkDevice`.                      |
| `VmaAllocation` | Handle выделенной памяти. Хранит `VkDeviceMemory`, offset, size. |
| `VmaPool`       | Пользовательский пул памяти определённого типа.                  |

---

## Сравнение с ручным управлением

### Без VMA

```cpp
// 1. Создать буфер
vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);

// 2. Получить требования к памяти
VkMemoryRequirements memReqs;
vkGetBufferMemoryRequirements(device, buffer, &memReqs);

// 3. Найти подходящий тип памяти
uint32_t memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

// 4. Выделить память
VkMemoryAllocateInfo allocInfo = {};
allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
allocInfo.allocationSize = memReqs.size;
allocInfo.memoryTypeIndex = memoryTypeIndex;
vkAllocateMemory(device, &allocInfo, nullptr, &memory);

// 5. Привязать память к буферу
vkBindBufferMemory(device, buffer, memory, 0);
```

### С VMA

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

---

## Лицензия

MIT License. Copyright © 2017-2024 Advanced Micro Devices, Inc.

---

## Ссылки

- **Исходный код:
  ** [GitHub - GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
- **Документация:** [Vulkan Memory Allocator (GPUOpen)](https://gpuopen.com/vulkan-memory-allocator/)
- **API Reference:** [Online Documentation](https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/)

---

## Установка VMA

<!-- anchor: 02_installation -->


Подключение Vulkan Memory Allocator к C++ проекту: CMake, vcpkg, интеграция с volk.

---

## Способы подключения

### 1. add_subdirectory (Git submodules)

```cmake
add_subdirectory(external/VMA)

add_executable(YourApp src/main.cpp)
target_link_libraries(YourApp PRIVATE GPUOpen::VulkanMemoryAllocator)
```

### 2. find_package (установленная библиотека)

```cmake
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
target_link_libraries(YourApp PRIVATE GPUOpen::VulkanMemoryAllocator)
```

### 3. vcpkg

```sh
vcpkg install vulkan-memory-allocator
```

```cmake
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
target_link_libraries(YourApp PRIVATE GPUOpen::VulkanMemoryAllocator)
```

---

## VMA_IMPLEMENTATION

VMA распространяется как header-only библиотека. Реализация подключается через макрос `VMA_IMPLEMENTATION` в **одном**
.cpp файле:

```cpp
// vma_init.cpp
#define VK_NO_PROTOTYPES
#include "volk.h"

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"
```

**Важно:**

- Макрос должен быть определён только в одном .cpp файле
- Файл должен компилироваться как C++ (не C)
- В остальных файлах только `#include "vma/vk_mem_alloc.h"` без макроса

---

## Интеграция с volk

При использовании volk (динамическая загрузка Vulkan) требуется передать указатели на функции:

```cpp
VmaAllocatorCreateInfo allocInfo = {};
allocInfo.physicalDevice = physicalDevice;
allocInfo.device = device;
allocInfo.instance = instance;
allocInfo.vulkanApiVersion = VK_API_VERSION_1_2;

VmaVulkanFunctions vulkanFunctions = {};
vmaImportVulkanFunctionsFromVolk(&allocInfo, &vulkanFunctions);
allocInfo.pVulkanFunctions = &vulkanFunctions;

VmaAllocator allocator;
vmaCreateAllocator(&allocInfo, &allocator);
```

### Порядок include

При использовании volk в каждом .cpp перед любыми includes:

```cpp
#define VK_NO_PROTOTYPES
#include "volk.h"
#include "vma/vk_mem_alloc.h"
```

---

## Интеграция без volk

Если проект линкуется с Vulkan loader:

```cmake
find_package(Vulkan REQUIRED)
target_link_libraries(YourApp PRIVATE
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
)
```

```cpp
// Без VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"
```

---

## CMake: полный пример с volk

```cmake
cmake_minimum_required(VERSION 3.14)
project(YourApp)

find_package(Vulkan REQUIRED)

add_subdirectory(external/volk)
add_subdirectory(external/VMA)

add_executable(YourApp
    src/main.cpp
    src/vma_init.cpp
)

target_link_libraries(YourApp PRIVATE
    volk
    GPUOpen::VulkanMemoryAllocator
    # НЕ линкуйте Vulkan::Vulkan при использовании volk
)

target_compile_definitions(YourApp PRIVATE
    VK_NO_PROTOTYPES
)
```

---

## Флаги аллокатора

Флаги `VmaAllocatorCreateInfo::flags` включают поддержку расширений Vulkan:

| Флаг                                                | Расширение                   | Описание                           |
|-----------------------------------------------------|------------------------------|------------------------------------|
| `VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT`  | —                            | Отключает внутреннюю синхронизацию |
| `VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT` | VK_KHR_dedicated_allocation  | Автоматические dedicated-аллокации |
| `VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT`         | VK_KHR_bind_memory2          | Оптимизация привязки памяти        |
| `VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT`        | VK_EXT_memory_budget         | Бюджет памяти, `vmaGetHeapBudgets` |
| `VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT`    | VK_KHR_buffer_device_address | Поддержка BDA                      |
| `VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT`      | VK_EXT_memory_priority       | Приоритеты аллокаций               |

Пример включения типичных флагов:

```cpp
allocInfo.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT
                | VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;

// Если включены соответствующие расширения:
if (hasMemoryBudget) {
    allocInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
}
if (hasBufferDeviceAddress) {
    allocInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
}
```

---

## Порядок уничтожения

VMA не предоставляет RAII. Уничтожать объекты нужно в правильном порядке:

1. `vmaDestroyBuffer` / `vmaDestroyImage` — все созданные ресурсы
2. `vmaFreeMemory` — все аллокации (если использовали `vmaAllocateMemory`)
3. `vmaDestroyPool` — все пулы (все аллокации из пулов должны быть освобождены)
4. `vmaDestroyAllocator`
5. `vkDestroyDevice`

---

## Проверка установки

Минимальный код для проверки:

```cpp
#include "volk.h"
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

int main() {
    // После инициализации Vulkan...
    VmaAllocatorCreateInfo createInfo = {};
    createInfo.device = device;
    createInfo.physicalDevice = physicalDevice;
    createInfo.instance = instance;
    createInfo.vulkanApiVersion = VK_API_VERSION_1_2;

    VmaAllocator allocator;
    VkResult result = vmaCreateAllocator(&createInfo, &allocator);

    if (result == VK_SUCCESS) {
        // VMA работает
        vmaDestroyAllocator(allocator);
    }

    return 0;
}
```

---

## Частые ошибки

### Неразрешённые символы при линковке

**Причина:** `VMA_IMPLEMENTATION` не определён ни в одном .cpp.

**Решение:** Добавьте макрос в один .cpp перед `#include "vma/vk_mem_alloc.h"`.

### Дублирование символов

**Причина:** `VMA_IMPLEMENTATION` определён в нескольких .cpp.

**Решение:** Оставьте макрос только в одном файле.

### Конфликт с volk

**Причина:** Линковка с `Vulkan::Vulkan` при использовании volk.

**Решение:** Не линкуйте `Vulkan::Vulkan`, используйте только `volk`.

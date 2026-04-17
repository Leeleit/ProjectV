# Интеграция VMA

**🟡 Уровень 2: Средний**

Пошаговое руководство по подключению VMA в C++/Vulkan проекте с volk.

## Оглавление

- [1. CMake](#1-cmake)
- [2. VMA_IMPLEMENTATION в одном .cpp](#2-vma_implementation-в-одном-cpp)
- [3. VK_NO_PROTOTYPES и порядок include](#3-vk_no_prototypes-и-порядок-include)
- [4. Создание аллокатора и интеграция с volk](#4-создание-аллокатора-и-интеграция-с-volk)
- [5. Флаги VmaAllocatorCreateInfo](#5-флаги-vmaallocatorcreateinfo)
- [6. Порядок уничтожения](#6-порядок-уничтожения)
- [7. Чего не делать](#7-чего-не-делать)

---

## 1. CMake

### Добавление VMA как подпроект

VMA поставляется как **INTERFACE**-библиотека: даёт только путь к заголовкам. Реализация подтягивается при включении
`vk_mem_alloc.h` с макросом `VMA_IMPLEMENTATION` в одном из ваших .cpp.

**При сборке с volk** не добавляйте `Vulkan::Vulkan` в `target_link_libraries` — volk загружает loader в runtime,
линковка с loader приведёт к конфликтам. Оставьте только `find_package(Vulkan REQUIRED)` для заголовков.
См. [Решение проблем](troubleshooting.md).

Пример с volk (рекомендуется для YourApp):

```cmake
find_package(Vulkan REQUIRED)

add_subdirectory(external/VMA)
add_subdirectory(external/volk)

add_executable(YourApp src/main.cpp src/vma_init.cpp)
target_link_libraries(YourApp PRIVATE
    GPUOpen::VulkanMemoryAllocator
    volk
    # НЕ линкуйте Vulkan::Vulkan при использовании volk
)
```

Пример без volk (если loader линкуется статически):

```cmake
find_package(Vulkan REQUIRED)
add_subdirectory(external/VMA)
add_executable(YourApp src/main.cpp src/vma_init.cpp)
target_link_libraries(YourApp PRIVATE
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
)
```

`GPUOpen::VulkanMemoryAllocator` добавляет только `include` директорию VMA. Линковать отдельную .lib не нужно — код
попадает в исполняемый файл из того .cpp, где определён `VMA_IMPLEMENTATION`.

### Вариант без отдельного vma_init.cpp

Если не хотите заводить отдельный файл, можно определить `VMA_IMPLEMENTATION` в начале `main.cpp` (или любого одного
.cpp), **до** `#include "vma/vk_mem_alloc.h"`, и не создавать `vma_init.cpp`. Важно: макрос должен быть определён ровно
в одном .cpp по всему проекту.

---

## 2. VMA_IMPLEMENTATION в одном .cpp

В **ровно одном** .cpp файле проекта нужно подтянуть реализацию VMA:

```cpp
#define VK_NO_PROTOTYPES
#include "volk.h"

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"
```

Этот файл может быть пустым (только эти строки) или содержать дополнительный код. Не определяйте `VMA_IMPLEMENTATION` в
заголовках и не в нескольких .cpp — иначе будут дублирование символов или ошибки линковки.

Файл с `VMA_IMPLEMENTATION` должен компилироваться как **C++** (не C): в реализации используются возможности C++.

**Совет по сборке:** Размещение `VMA_IMPLEMENTATION` в отдельном `vma_init.cpp` (а не в `main.cpp`) ускоряет
инкрементальную сборку: VMA — большая библиотека, и при изменениях в main.cpp её не придётся перекомпилировать.

---

## 3. VK_NO_PROTOTYPES и порядок include

VMA включает `vulkan/vulkan.h`. В проекте с volk функции Vulkan не должны объявляться как линкуемые символы, иначе
возникнет конфликт с volk.

В **каждом** .cpp, где подключаются volk или VMA, в самом начале файла (до любых `#include`):

```cpp
#define VK_NO_PROTOTYPES
#include "volk.h"
#include "vma/vk_mem_alloc.h"
```

Порядок: сначала `VK_NO_PROTOTYPES`, затем `volk.h`, затем любой заголовок, который тянет Vulkan (в т.ч.
`vk_mem_alloc.h`).
Подробнее: [volk — Интеграция, раздел 2](../volk/integration.md#2-vk_no_prototypes-и-порядок-include).

---

## 4. Создание аллокатора и интеграция с volk

Аллокатор создаётся **после** создания `VkDevice` (и желательно после `volkLoadDevice(device)`). VMA вызывает внутри
себя функции Vulkan; при использовании volk нужно передать им указатели через `VmaVulkanFunctions`.

Обязательные поля `VmaAllocatorCreateInfo`: `flags`, `physicalDevice`, `device`, `instance`, `vulkanApiVersion`.
Опционально: `preferredLargeHeapBlockSize` (0 = по умолчанию, например 256 MiB для больших heap'ов),
`pAllocationCallbacks` (CPU-аллокации), `pDeviceMemoryCallbacks` (колбэки для vkAllocateMemory/vkFreeMemory),
`pHeapSizeLimit` (массив лимитов в байтах по каждому heap'у; `VK_WHOLE_SIZE` = без лимита). Подробнее —
в [vk_mem_alloc.h](../../external/VMA/include/vk_mem_alloc.h).

```cpp
VmaAllocatorCreateInfo allocInfo = {};
allocInfo.physicalDevice = physicalDevice;
allocInfo.device = device;
allocInfo.instance = instance;
allocInfo.vulkanApiVersion = VK_API_VERSION_1_2;  // или ваша версия
allocInfo.flags = /* см. раздел 5 */;
// allocInfo.preferredLargeHeapBlockSize = 0;  // по умолчанию
// allocInfo.pAllocationCallbacks = nullptr;
// allocInfo.pHeapSizeLimit = nullptr;

#ifdef VOLK_HEADER_VERSION
VmaVulkanFunctions vulkanFunctions = {};
vmaImportVulkanFunctionsFromVolk(&allocInfo, &vulkanFunctions);
allocInfo.pVulkanFunctions = &vulkanFunctions;
#endif

VmaAllocator allocator = VK_NULL_HANDLE;
VkResult result = vmaCreateAllocator(&allocInfo, &allocator);
```

Версию Vulkan для VMA можно зафиксировать макросом до первого включения заголовка:
`#define VMA_VULKAN_VERSION 1002000` (1.2), `1001000` (1.1), `1000000` (1.0) — если нужна явная привязка к версии API.

Если не передать `pVulkanFunctions` при сборке с volk, VMA будет использовать глобальные указатели Vulkan. Явная
передача через `vmaImportVulkanFunctionsFromVolk` надёжнее и не зависит от глобального состояния.

---

## 5. Флаги VmaAllocatorCreateInfo

Флаги `allocInfo.flags` включают возможности VMA, которые опираются на расширения Vulkan. Устанавливать имеет смысл
только те, для которых вы включили соответствующие расширения при создании device/instance.

| Флаг                                                 | Расширение / версия                                                            | Назначение                                                                                 |
|------------------------------------------------------|--------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------|
| `VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT`   | —                                                                              | Отключение внутренней синхронизации (однопоточное использование или внешняя синхронизация) |
| `VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT`  | VK_KHR_dedicated_allocation (или Vulkan 1.1)                                   | Автоматические dedicated-аллокации где выгодно                                             |
| `VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT`          | VK_KHR_bind_memory2 (или Vulkan 1.1)                                           | Поддержка pNext при bind; нужен для части сценариев                                        |
| `VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT`         | VK_EXT_memory_budget (+ VK_KHR_get_physical_device_properties2 или Vulkan 1.1) | Точный бюджет памяти, vmaGetHeapBudgets()                                                  |
| `VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT`     | VK_KHR_buffer_device_address / Vulkan 1.2 + feature                            | Буферы с VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT                                         |
| `VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT`       | VK_EXT_memory_priority                                                         | Приоритеты аллокаций                                                                       |
| `VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE5_BIT`          | VK_KHR_maintenance5                                                            | Рекомендуется при включённом расширении                                                    |
| `VMA_ALLOCATOR_CREATE_KHR_EXTERNAL_MEMORY_WIN32_BIT` | VK_KHR_external_memory_win32                                                   | Экспорт/импорт памяти (Windows)                                                            |

Минимальный набор для YourApp при Vulkan 1.2 и типичных расширениях:

```cpp
allocInfo.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT
                | VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;
if (/* VK_EXT_memory_budget включён */)
    allocInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
if (/* buffer device address включён */)
    allocInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
```

Остальные флаги добавлять по необходимости (см. [vk_mem_alloc.h](../../external/VMA/include/vk_mem_alloc.h),
`VmaAllocatorCreateFlagBits`).

---

## 6. Порядок уничтожения

VMA не предоставляет RAII: все handle'ы нужно явно уничтожать в правильном порядке. Иначе — утечки или краш при вызове
`vkDestroyDevice`.

**Порядок:** сначала освободить все ресурсы VMA, затем аллокатор, затем device:

1. `vmaDestroyBuffer` / `vmaDestroyImage` для всех буферов и изображений, созданных через этот аллокатор.
2. `vmaFreeMemory` для всех аллокаций, созданных через `vmaAllocateMemory` (если использовали).
3. `vmaDestroyPool` для всех пулов (все аллокации из пулов должны быть освобождены до этого).
4. `vmaDestroyAllocator(allocator)`.
5. `vkDestroyDevice(device)`.

Обратный порядок недопустим: нельзя уничтожать аллокатор до освобождения созданных через него ресурсов.

---

## 7. Чего не делать

| Не делайте                                                                | Почему                                                                                                     |
|---------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------|
| Линковать `Vulkan::Vulkan` при использовании volk                         | Конфликт: volk загружает функции в runtime, линковка с loader приведёт к двойным определениям или ошибкам. |
| Определять `VMA_IMPLEMENTATION` в нескольких .cpp                         | Дублирование символов при линковке. Реализация должна быть в **одном** файле.                              |
| Вызывать `vmaDestroyAllocator` до освобождения всех буферов/изображений   | Неопределённое поведение или краш.                                                                         |
| Забывать `vmaFlushAllocation` после записи с CPU (при не-coherent памяти) | GPU не увидит данные.                                                                                      |
| Забывать `vmaInvalidateAllocation` перед чтением с CPU после доступа GPU  | Readback вернёт устаревшие данные из кэша.                                                                 |

---

**Для специфичных сценариев, включая интеграцию с Tracy для профилирования памяти и специализированные паттерны для
воксельного движка, см. документацию [projectv-integration.md](projectv-integration.md).**

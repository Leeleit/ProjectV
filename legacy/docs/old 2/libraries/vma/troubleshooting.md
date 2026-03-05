# Решение проблем

**🟡 Уровень 2: Средний**

Частые ошибки при использовании VMA и способы их исправления.

## Оглавление

- [Ошибки компиляции](#ошибки-компиляции)
- [Ошибки линковки](#ошибки-линковки)
- [Ошибки во время выполнения](#ошибки-во-время-выполнения)
- [Validation layer](#validation-layer)

---

## Ошибки компиляции

### `To use volk, you need to define VK_NO_PROTOTYPES before including vulkan.h` (при сборке с volk)

**Причина:** Заголовок VMA включает `vulkan/vulkan.h`. Если этот include выполняется до volk.h и без `VK_NO_PROTOTYPES`,
срабатывает проверка volk.

**Решение:** В начале каждого .cpp, где подключается VMA, сначала определите макрос и подключите volk:

```cpp
#define VK_NO_PROTOTYPES
#include "volk.h"
#include "vma/vk_mem_alloc.h"
```

См. [volk — Решение проблем](../volk/troubleshooting.md)
и [Интеграция — порядок include](integration.md#3-vk_no_prototypes-и-порядок-include).

---

## Ошибки линковки

### Неразрешённые символы при линковке: `vmaCreateAllocator`, `vmaCreateBuffer` и т.д.

**Причина:** Реализация VMA не попала в сборку. VMA — header-only в смысле распространения: код реализации подключается
только при определении макроса `VMA_IMPLEMENTATION` в одном .cpp.

**Решение:** В ровно одном .cpp файле проекта перед `#include "vma/vk_mem_alloc.h"` добавьте:

```cpp
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"
```

Этот файл должен компилироваться как C++ (не C) и быть в списке источников целевого исполняемого файла/библиотеки.
См. [Интеграция — VMA_IMPLEMENTATION](integration.md#2-vma_implementation-в-одном-cpp).

---

### Символы Vulkan не найдены при использовании VMA и volk (`vkAllocateMemory`, `vkBindBufferMemory` и т.д.)

**Причина:** С volk приложение не должно линковаться с loader (`Vulkan::Vulkan` / `vulkan-1`). Функции Vulkan вызываются
через указатели, заполняемые в runtime. Если при этом включён `VK_NO_PROTOTYPES`, прототипы Vulkan не объявлены как
линкуемые символы, и линкер не находит их реализацию — это ожидаемо: реализацию даёт volk при вызове `volkLoadInstance`/
`volkLoadDevice`. Если же вы линкуете `Vulkan::Vulkan`, возможны конфликты или двойное определение.

**Решение:** Не линкуйте исполняемый файл с `Vulkan::Vulkan` (или с `vulkan`). Оставьте только
`find_package(Vulkan REQUIRED)` для заголовков и путей. VMA при использовании `pVulkanFunctions` (через
`vmaImportVulkanFunctionsFromVolk`) не требует линковки с loader.
См. [volk — Решение проблем, раздел «Символы не найдены»](../volk/troubleshooting.md#символы-vkcreateinstance-vkenumerateinstanceextensionproperties-и-тд-не-найдены).

---

### Дублирование символов VMA (multiple definition of …)

**Причина:** Макрос `VMA_IMPLEMENTATION` определён в нескольких .cpp файлах, и реализация VMA компилируется несколько
раз.

**Решение:** Определяйте `VMA_IMPLEMENTATION` только в **одном** .cpp по всему проекту. В остальных файлах подключайте
только `#include "vma/vk_mem_alloc.h"` без макроса.

---

## Ошибки во время выполнения

### `vmaCreateAllocator` возвращает ошибку (например, `VK_ERROR_INITIALIZATION_FAILED`)

**Причина:** Часто — не переданы указатели на функции Vulkan при использовании volk, либо device/instance не созданы/уже
уничтожены.

**Решение:** Убедитесь, что:

1. `vmaCreateAllocator` вызывается после `vkCreateDevice` и (при volk) после `volkLoadDevice(device)`.
2. При сборке с volk в `VmaAllocatorCreateInfo` передаёте `pVulkanFunctions`, заполненный через
   `vmaImportVulkanFunctionsFromVolk(&allocInfo, &vulkanFunctions)`.
3. В `allocatorCreateInfo` заданы корректные `physicalDevice`, `device`, `instance` и `vulkanApiVersion`.

---

### Крах или некорректная работа при вызове `vmaMapMemory` / при использовании `pMappedData`

**Причина:** Аллокация создана в памяти без `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`, либо не вызваны flush/invalidate при
не-coherent памяти.

**Решение:** Для отображения памяти с CPU используйте при создании аллокации `VMA_MEMORY_USAGE_CPU_TO_GPU`,
`VMA_MEMORY_USAGE_CPU_ONLY` или иной usage с host-visible типом; при `VMA_MEMORY_USAGE_AUTO` добавьте флаг
`VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT` или `VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT`. После
записи с CPU при не-coherent памяти вызывайте `vmaFlushAllocation` перед использованием на GPU; при чтении с CPU —
`vmaInvalidateAllocation` перед чтением.

---

### Данные не видны на GPU после map и записи с CPU

**Причина:** Память не host-coherent; кэш CPU не сброшен. Без `vmaFlushAllocation` GPU может читать устаревшие данные.

**Решение:** После записи в отображённую память и перед использованием на GPU (в командном буфере или при передаче в
дескриптор) вызовите `vmaFlushAllocation(allocator, allocation, 0, VK_WHOLE_SIZE)`.
См. [Основные понятия — Host-coherent vs host-cached](concepts.md#host-coherent-vs-host-cached).

---

### Данные неверны при readback (чтении с GPU на CPU)

**Причина:** Кэш CPU не инвалидирован. После того как GPU записал данные, CPU читает из кэша старые значения.

**Решение:** Перед чтением через `vmaMapMemory` или `pMappedData` вызовите
`vmaInvalidateAllocation(allocator, allocation, 0, VK_WHOLE_SIZE)`. Альтернатива: `vmaCopyAllocationToMemory` —
выполняет invalidate, map, memcpy, unmap за один вызов.

---

### `vmaCreateAllocator` или аллокация возвращают `VK_ERROR_OUT_OF_DEVICE_MEMORY`

**Причина:** Недостаточно видеопамяти или превышен заданный лимит.

**Решение:** При включённом `VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT` вызовите
`vmaGetHeapBudgets(allocator, budgets)` и проверьте по нужному heap'у поля `usage` и `budget` в массиве `VmaBudget` —
разница покажет, сколько ещё можно выделить. Если при создании аллокатора задан `pHeapSizeLimit`, убедитесь, что лимиты
достаточны. Флаг `VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT` при создании аллокации заставляет VMA возвращать эту ошибку
вместо выделения сверх бюджета. Освободите неиспользуемые ресурсы или уменьшите запросы.

**Совет при отладке OOM:** Включите `VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT` при создании аллокатора и перед
проблемной аллокацией вызывайте `vmaGetHeapBudgets` — по полям `usage` и `budget` можно понять, какой heap исчерпан.

---

### Аллокация возвращает `VK_ERROR_FEATURE_NOT_PRESENT`

**Причина:** Запрошена комбинация типа памяти или возможностей, которую устройство не поддерживает.

**Решение:** Проверьте `usage` и флаги в `VmaAllocationCreateInfo`. Например, `VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED`
работает только при наличии типа памяти с `VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT` (часто только на мобильных). Для
изображений убедитесь, что tiling (OPTIMAL vs LINEAR) и другие параметры совместимы с доступными типами памяти.
См. [vk_mem_alloc.h](../../external/VMA/include/vk_mem_alloc.h) и `vmaFindMemoryTypeIndex`.

---

## Validation layer

### Validation: Mapping is not allowed

**Причина:** Вызван `vmaMapMemory` (или использован `VMA_ALLOCATION_CREATE_MAPPED_BIT`) для аллокации с
`VMA_MEMORY_USAGE_AUTO` без флага `VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT` или
`VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT`. VMA при AUTO не выбирает host-visible тип, если не указан доступ с CPU.

**Решение:** При создании mappable-аллокации с `usage = VMA_MEMORY_USAGE_AUTO` добавьте один из флагов:
`VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT` (для staging, записи) или
`VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT` (для readback, read/write).
См. [Таблица usage и флагов](quickstart.md#таблица-usage-и-флаги-по-задаче).

---

### Предупреждение:

`vkBindBufferMemory(): Binding memory to buffer ... but vkGetBufferMemoryRequirements() has not been called on that buffer`

**Причина:** При использовании расширения VK_KHR_dedicated_allocation (или флага
`VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT`) VMA может вызывать bind в порядке, который validation layer считает
нестандартным.

**Решение:** Такое предупреждение можно игнорировать — оно известно при работе с VMA и dedicated allocation (указано в
комментариях в [vk_mem_alloc.h](../../external/VMA/include/vk_mem_alloc.h)). При необходимости отключите validation для
данного сценария или используйте актуальную версию слоёв.

---

## См. также

- [Интеграция](integration.md) — порядок include, создание аллокатора с volk
- [Быстрый старт](quickstart.md) — минимальный рабочий пример
- [volk — Решение проблем](../volk/troubleshooting.md) — VK_NO_PROTOTYPES и линковка с loader

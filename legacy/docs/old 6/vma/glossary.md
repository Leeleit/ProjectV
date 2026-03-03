# Глоссарий

**🟢 Уровень 1: Начинающий**

Словарь терминов VMA и управления памятью Vulkan. Подробные примеры — в [Основных понятиях](concepts.md)
и [Быстром старте](quickstart.md).

## Оглавление

- [Объекты VMA](#объекты-vma)
- [Типы и свойства памяти Vulkan](#типы-и-свойства-памяти-vulkan)
- [Usage и флаги](#usage-и-флаги)
- [Функции](#функции)

---

## Объекты VMA

| Термин                      | Объяснение                                                                                                                                                    |
|-----------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **VMA**                     | Vulkan Memory Allocator — библиотека AMD/GPUOpen. Создаёт буферы и изображения вместе с памятью, выбирает тип памяти, поддерживает пулы и дефрагментацию.     |
| **VmaAllocator**            | Главный объект. Создаётся через `vmaCreateAllocator()` после `VkDevice`. Через него вызывают `vmaCreateBuffer`, `vmaCreateImage`, `vmaAllocateMemory`.        |
| **VmaAllocation**           | Handle выделенного куска памяти. Нужен для `vmaMapMemory`, `vmaGetAllocationInfo`, `vmaDestroyBuffer`/`vmaDestroyImage`.                                      |
| **Pool (VmaPool)**          | Пользовательский пул памяти одного типа. Создаётся через `vmaCreatePool`. Удобен для множества мелких объектов или линейного аллокатора.                      |
| **VMA_IMPLEMENTATION**      | Макрос. Определяется в **одном** .cpp перед `#include "vk_mem_alloc.h"` — подтягивается реализация (STB-style). Без него линкер не найдёт символы.            |
| **VmaAllocationCreateInfo** | Параметры аллокации: `usage`, `flags`, опционально `pool`, `requiredFlags`, `preferredFlags`. См. [Справочник API](api-reference.md#vmaallocationcreateinfo). |
| **VmaVulkanFunctions**      | Указатели на функции Vulkan для VMA. При volk заполняется через `vmaImportVulkanFunctionsFromVolk()`.                                                         |

---

## Типы и свойства памяти Vulkan

| Термин                   | Объяснение                                                                                                                                                                                                                 |
|--------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Memory type**          | Один из вариантов `VkPhysicalDeviceMemoryProperties` с флагами (device-local, host-visible и т.д.). VMA выбирает по `usage` и `flags`.                                                                                     |
| **Device-local**         | Память на GPU. Быстрая для шейдеров; CPU напрямую не обращается. В Vulkan — `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`.                                                                                                         |
| **Host-visible**         | Доступна с CPU (`vkMapMemory` / `vmaMapMemory`). Нужна для staging и uniform-буферов.                                                                                                                                      |
| **Host-coherent**        | `VK_MEMORY_PROPERTY_HOST_COHERENT_BIT`. Кэш CPU и GPU синхронизирован; flush/invalidate не нужны. Без него — обязательны.                                                                                                  |
| **Host-cached**          | `VK_MEMORY_PROPERTY_HOST_CACHED_BIT`. Кэшируемая CPU-память. Удобна для readback (чтение с GPU). Обычно не coherent — нужен invalidate перед чтением.                                                                      |
| **Staging buffer**       | Временный host-visible буфер. CPU пишет в него → `vkCmdCopyBuffer` → device-local буфер/изображение. Паттерн загрузки геометрии и текстур.                                                                                 |
| **Dedicated allocation** | Отдельный блок `VkDeviceMemory` под один ресурс. Может давать лучшую производительность для больших ресурсов. VMA — автоматически (VK_KHR_dedicated_allocation) или по флагу `VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT`. |

---

## Usage и флаги

| Термин                                                                    | Объяснение                                                                                                                                              |
|---------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------|
| **VMA_MEMORY_USAGE_***                                                    | Подсказка VMA о назначении ресурса. Рекомендуется `AUTO` (выбор по usage буфера/изображения). `GPU_ONLY`, `CPU_TO_GPU` и т.д. — deprecated, но рабочие. |
| **VMA_MEMORY_USAGE_AUTO**                                                 | Автовыбор по `VkBufferCreateInfo`/`VkImageCreateInfo`. Для mappable обязательно `HOST_ACCESS_SEQUENTIAL_WRITE_BIT` или `HOST_ACCESS_RANDOM_BIT`.        |
| **VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE**                                   | AUTO с приоритетом device-local.                                                                                                                        |
| **VMA_MEMORY_USAGE_AUTO_PREFER_HOST**                                     | AUTO с приоритетом host-памяти.                                                                                                                         |
| **VMA_MEMORY_USAGE_CPU_COPY**                                             | Промежуточный staging: host-visible, не обязательно самый быстрый тип. Альтернатива CPU_TO_GPU для копирований.                                         |
| **VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED**                                 | Лениво выделяемая GPU-память (`VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT`). Для transient attachment на мобильных. На десктопе может отсутствовать.       |
| **VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT**                | Последовательная запись с CPU (staging upload). Обязателен при AUTO для mappable с записью.                                                             |
| **VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT**                          | Случайный доступ с CPU (readback, uniform). Обязателен при AUTO для mappable с чтением или read/write.                                                  |
| **VMA_ALLOCATION_CREATE_MAPPED_BIT**                                      | Сразу получить `pMappedData` без вызова `vmaMapMemory`. Для persistent mapping.                                                                         |
| **VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT**                            | Принудительная dedicated-аллокация.                                                                                                                     |
| **VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT**                               | Отказ (`VK_ERROR_OUT_OF_DEVICE_MEMORY`) при превышении бюджета вместо undefined behavior.                                                               |
| **VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT / STRATEGY_MIN_TIME_BIT** | Алгоритм подбора блока: минимум памяти или минимум времени.                                                                                             |

---

## Функции

| Термин                                           | Объяснение                                                                                                                           |
|--------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------|
| **vmaMapMemory / vmaUnmapMemory**                | Отображение host-visible аллокации в адресное пространство CPU. Требует flush/invalidate при не-coherent памяти.                     |
| **vmaFlushAllocation / vmaInvalidateAllocation** | Сброс кэша (flush — после записи CPU, перед GPU) или инвалидация (invalidate — перед чтением CPU после GPU). Для не-coherent памяти. |
| **vmaGetHeapBudgets**                            | Заполняет массив `VmaBudget` по heap'ам. Требует `VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT`.                                       |
| **vmaCalculateStatistics**                       | Статистика по аллокатору: блоки, аллокации, байты. Для отладки и мониторинга.                                                        |
| **vmaGetPoolStatistics**                         | Статистика по пулу.                                                                                                                  |
| **VmaBudget**                                    | Данные по heap'у: `usage`, `budget`, `statistics` (VmaStatistics).                                                                   |
| **VmaStatistics**                                | `blockCount`, `allocationCount`, `blockBytes`, `allocationBytes` и др.                                                               |

---

## См. также

- [Основные понятия](concepts.md) — паттерны: GPU-only, staging, readback, mapped.
- [Справочник API](api-reference.md) — полные описания функций и структур.

# Глоссарий VMA

**🟢 Уровень 1: Базовый**

Словарь терминов VMA и управления памятью Vulkan.

---

## Объекты VMA

| Термин                      | Определение                                                                                                                                                                   |
|-----------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **VMA**                     | Vulkan Memory Allocator — библиотека для управления памятью в Vulkan. Создаёт буферы и изображения вместе с памятью, выбирает тип памяти, поддерживает пулы и дефрагментацию. |
| **VmaAllocator**            | Главный объект VMA. Создаётся через `vmaCreateAllocator()` после `VkDevice`. Через него выполняются все операции с памятью.                                                   |
| **VmaAllocation**           | Handle выделенного куска памяти. Хранит `VkDeviceMemory`, offset, size. Используется для map, destroy, получения информации.                                                  |
| **VmaPool**                 | Пользовательский пул памяти определённого типа. Создаётся через `vmaCreatePool`. Позволяет контролировать распределение памяти для группы ресурсов.                           |
| **VMA_IMPLEMENTATION**      | Макрос, который должен быть определён в одном .cpp файле перед включением `vk_mem_alloc.h`. Подключает реализацию библиотеки.                                                 |
| **VmaAllocationCreateInfo** | Структура с параметрами аллокации: `usage`, `flags`, `pool`, `requiredFlags`, `preferredFlags`.                                                                               |
| **VmaVulkanFunctions**      | Структура с указателями на функции Vulkan. Заполняется через `vmaImportVulkanFunctionsFromVolk()` при использовании volk.                                                     |

---

## Типы памяти Vulkan

| Термин            | Определение                                                                                                                    |
|-------------------|--------------------------------------------------------------------------------------------------------------------------------|
| **Memory type**   | Один из вариантов памяти в `VkPhysicalDeviceMemoryProperties` с определёнными свойствами (device-local, host-visible и др.).   |
| **Memory heap**   | Физический блок видеопамяти. Один heap может содержать несколько memory types.                                                 |
| **Device-local**  | Память на GPU (`VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`). Быстрая для шейдеров, CPU не имеет прямого доступа.                     |
| **Host-visible**  | Память, доступная CPU через `vkMapMemory` (`VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`). Используется для staging и uniform-буферов. |
| **Host-coherent** | Память с автоматической синхронизацией кэша CPU и GPU (`VK_MEMORY_PROPERTY_HOST_COHERENT_BIT`). Flush/invalidate не нужны.     |
| **Host-cached**   | Кэшируемая CPU память (`VK_MEMORY_PROPERTY_HOST_CACHED_BIT`). Быстрое чтение с CPU, но требует invalidate после записи GPU.    |

---

## Паттерны использования

| Термин                   | Определение                                                                                                                                |
|--------------------------|--------------------------------------------------------------------------------------------------------------------------------------------|
| **Staging buffer**       | Временный host-visible буфер для передачи данных с CPU на GPU. CPU пишет в staging, затем `vkCmdCopyBuffer` копирует в device-local буфер. |
| **GPU-only resource**    | Ресурс в device-local памяти: вершинные/индексные буферы, текстуры. Данные загружаются через staging.                                      |
| **Persistent mapping**   | Постоянно отображённая память. Аллокация с `VMA_ALLOCATION_CREATE_MAPPED_BIT`, указатель доступен всё время жизни ресурса.                 |
| **Readback**             | Чтение данных с GPU на CPU. Буфер с `HOST_ACCESS_RANDOM_BIT`, требует `vmaInvalidateAllocation` перед чтением.                             |
| **Dedicated allocation** | Отдельный блок `VkDeviceMemory` под один ресурс. Может улучшить производительность для больших ресурсов.                                   |
| **Aliasing**             | Использование одной области памяти разными ресурсами в разное время.                                                                       |

---

## Usage

| Термин                                    | Определение                                                                  |
|-------------------------------------------|------------------------------------------------------------------------------|
| **VMA_MEMORY_USAGE_AUTO**                 | Автоматический выбор типа памяти по usage буфера/изображения. Рекомендуется. |
| **VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE**   | AUTO с приоритетом device-local памяти.                                      |
| **VMA_MEMORY_USAGE_AUTO_PREFER_HOST**     | AUTO с приоритетом host-visible памяти.                                      |
| **VMA_MEMORY_USAGE_GPU_ONLY**             | Только device-local. Deprecated, используйте AUTO.                           |
| **VMA_MEMORY_USAGE_CPU_TO_GPU**           | Host-visible для записи с CPU. Deprecated.                                   |
| **VMA_MEMORY_USAGE_GPU_TO_CPU**           | Host-visible для чтения с CPU. Deprecated.                                   |
| **VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED** | Лениво выделяемая память для transient attachment (мобильные GPU).           |

---

## Флаги

| Термин                               | Определение                                                                                  |
|--------------------------------------|----------------------------------------------------------------------------------------------|
| **MAPPED_BIT**                       | `VMA_ALLOCATION_CREATE_MAPPED_BIT` — сразу получить `pMappedData` без вызова `vmaMapMemory`. |
| **HOST_ACCESS_SEQUENTIAL_WRITE_BIT** | Последовательная запись с CPU (staging upload).                                              |
| **HOST_ACCESS_RANDOM_BIT**           | Случайный доступ с CPU (readback, uniform с read/write).                                     |
| **DEDICATED_MEMORY_BIT**             | Принудительная dedicated-аллокация.                                                          |
| **WITHIN_BUDGET_BIT**                | Возврат ошибки при превышении бюджета памяти.                                                |
| **STRATEGY_MIN_MEMORY_BIT**          | Алгоритм выбора блока: минимум памяти (best-fit).                                            |
| **STRATEGY_MIN_TIME_BIT**            | Алгоритм выбора блока: минимум времени (first-fit).                                          |
| **NEVER_ALLOCATE_BIT**               | Использовать только уже выделенную память.                                                   |
| **CAN_ALIAS_BIT**                    | Разрешить aliasing ресурсов.                                                                 |

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

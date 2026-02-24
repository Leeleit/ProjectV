# VMA в ProjectV: Обзор

**🟡 Уровень 2: Средний**

Архитектура памяти для воксельного движка ProjectV.

---

## Требования к памяти

Воксельный движок предъявляет особые требования к управлению памятью:

| Тип ресурса         | Характеристики                            | Частота обновления           |
|---------------------|-------------------------------------------|------------------------------|
| Воксельные чанки    | Множество объектов фиксированного размера | При загрузке/выгрузке чанков |
| Текстуры материалов | Крупные ресурсы                           | Редко                        |
| Compute buffers     | Storage buffers для шейдеров              | Зависит от алгоритма         |
| Uniform buffers     | Матрицы камеры, параметры                 | Каждый кадр                  |
| Staging buffers     | Временные буферы                          | Часто                        |

---

## Стратегии аллокации по типам ресурсов

| Тип ресурса         | Стратегия VMA                   | Обоснование                                                     |
|---------------------|---------------------------------|-----------------------------------------------------------------|
| Воксельные чанки    | `MIN_MEMORY` + пулы             | Долгоживущие, много объектов, важно минимизировать фрагментацию |
| Текстуры материалов | `MIN_MEMORY` + dedicated        | Большие ресурсы, загружаются один раз                           |
| Uniform buffers     | `MIN_TIME` + persistent mapping | Частые обновления, важна скорость                               |
| Staging буферы      | `MIN_TIME`                      | Временные, создаются/уничтожаются часто                         |
| Compute buffers     | Зависит от частоты              | `MIN_MEMORY` для статических, `MIN_TIME` для динамических       |

---

## Архитектура памяти

```
VmaAllocator
    │
    ├── ChunkPool (VmaPool)
    │       ├── ChunkAllocation[0] ─── VkBuffer (16KB)
    │       ├── ChunkAllocation[1] ─── VkBuffer (16KB)
    │       └── ...
    │
    ├── TexturePool (опционально)
    │       └── TextureAllocation ─── VkImage
    │
    ├── UniformBuffers
    │       ├── UniformAllocation[0] ─── VkBuffer (persistent mapped)
    │       ├── UniformAllocation[1]
    │       └── UniformAllocation[2]
    │
    └── StagingBuffers
            └── StagingAllocation ─── VkBuffer (host-visible)
```

---

## Интеграция с компонентами ProjectV

### Vulkan

VMA работает поверх Vulkan. При использовании volk требуется передать указатели на функции:

```cpp
VmaVulkanFunctions vulkanFunctions = {};
vmaImportVulkanFunctionsFromVolk(&allocInfo, &vulkanFunctions);
allocInfo.pVulkanFunctions = &vulkanFunctions;
```

### Tracy

Профилирование аллокаций через Tracy:

```cpp
#ifdef TRACY_ENABLE
TracyAllocN(allocation, size, "VMA_Chunk");
#endif
```

### flecs (ECS)

Компоненты для GPU ресурсов:

```cpp
struct VmaBufferComponent {
    VkBuffer buffer;
    VmaAllocation allocation;
    VkDeviceSize size;
};
```

---

## Пулы для воксельных чанков

### Преимущества пулов

1. **Уменьшение фрагментации** — чанки одного размера аллоцируются из предварительно выделенных блоков
2. **Быстрые аллокации** — не требуется поиск в общей куче
3. **Контроль памяти** — можно ограничить максимальный размер пула
4. **Локализация данных** — чанки одного типа располагаются рядом

### Типичная конфигурация пула чанков

```cpp
VmaPoolCreateInfo poolInfo = {};
poolInfo.blockSize = 64 * 1024 * 1024;  // 64 MB блоки
poolInfo.minBlockCount = 1;
poolInfo.maxBlockCount = 16;            // Максимум 1 GB
```

---

## Мониторинг памяти

### Бюджет памяти

```cpp
VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
vmaGetHeapBudgets(allocator, budgets);

float heapUsage = float(budgets[0].usage) / float(budgets[0].budget) * 100.0f;
```

### Статистика пула

```cpp
VmaStatistics stats;
vmaGetPoolStatistics(allocator, chunkPool, &stats);

size_t usedMemory = stats.allocationBytes;
size_t freeMemory = stats.unusedBytes;
```

---

## Документация ProjectV

| Файл                                                     | Описание                              |
|----------------------------------------------------------|---------------------------------------|
| [08_projectv-integration.md](08_projectv-integration.md) | CMake конфигурация, интеграция с volk |
| [09_projectv-patterns.md](09_projectv-patterns.md)       | Паттерны: чанки, Tracy, ECS           |
| [10_projectv-examples.md](10_projectv-examples.md)       | Примеры кода                          |

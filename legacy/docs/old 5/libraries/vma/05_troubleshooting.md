# Решение проблем VMA

**🟡 Уровень 2: Средний**

Частые ошибки при использовании VMA и способы их исправления.

---

## Ошибки компиляции

### Ошибка: VK_NO_PROTOTYPES при использовании volk

**Сообщение:**

```
To use volk, you need to define VK_NO_PROTOTYPES before including vulkan.h
```

**Причина:** VMA включает `vulkan/vulkan.h` до volk без макроса `VK_NO_PROTOTYPES`.

**Решение:** В каждом .cpp перед любыми includes:

```cpp
#define VK_NO_PROTOTYPES
#include "volk.h"
#include "vma/vk_mem_alloc.h"
```

---

## Ошибки линковки

### Неразрешённые символы: vmaCreateAllocator, vmaCreateBuffer

**Сообщение:**

```
undefined reference to `vmaCreateAllocator`
undefined reference to `vmaCreateBuffer`
```

**Причина:** Макрос `VMA_IMPLEMENTATION` не определён ни в одном .cpp.

**Решение:** В одном .cpp файле:

```cpp
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"
```

---

### Дублирование символов VMA

**Сообщение:**

```
multiple definition of `vmaCreateAllocator`
```

**Причина:** `VMA_IMPLEMENTATION` определён в нескольких .cpp файлах.

**Решение:** Оставьте макрос только в **одном** файле проекта.

---

### Символы Vulkan не найдены при использовании volk

**Сообщение:**

```
undefined reference to `vkAllocateMemory`
undefined reference to `vkBindBufferMemory`
```

**Причина:** Линковка с `Vulkan::Vulkan` при использовании volk.

**Решение:** Не линкуйте `Vulkan::Vulkan`. Используйте только volk:

```cmake
# Неправильно:
target_link_libraries(YourApp PRIVATE Vulkan::Vulkan volk)

# Правильно:
target_link_libraries(YourApp PRIVATE volk)
```

---

## Ошибки во время выполнения

### vmaCreateAllocator возвращает ошибку

**Причины и решения:**

1. **Отсутствуют указатели на функции при volk:**

```cpp
VmaVulkanFunctions vulkanFunctions = {};
vmaImportVulkanFunctionsFromVolk(&allocInfo, &vulkanFunctions);
allocInfo.pVulkanFunctions = &vulkanFunctions;
```

2. **Некорректные параметры:**

- Убедитесь, что `physicalDevice`, `device`, `instance` валидны
- Проверьте `vulkanApiVersion`

3. **Вызов до создания device:**

- `vmaCreateAllocator` вызывается после `vkCreateDevice`

---

### Крах при vmaMapMemory или использовании pMappedData

**Причина:** Аллокация не в host-visible памяти.

**Решение:** Для CPU-доступа укажите соответствующие флаги:

```cpp
VmaAllocationCreateInfo allocInfo = {};
allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
```

---

### Данные не видны на GPU после записи с CPU

**Причина:** Память не host-coherent, кэш не сброшен.

**Решение:** После записи вызовите flush:

```cpp
memcpy(mappedData, data, size);
vmaFlushAllocation(allocator, allocation, 0, VK_WHOLE_SIZE);
```

---

### Неверные данные при readback (GPU → CPU)

**Причина:** Кэш CPU не инвалидирован.

**Решение:** Перед чтением вызовите invalidate:

```cpp
vmaInvalidateAllocation(allocator, allocation, 0, VK_WHOLE_SIZE);
vmaMapMemory(allocator, allocation, &ptr);
// Теперь можно читать
```

---

### VK_ERROR_OUT_OF_DEVICE_MEMORY

**Причины и решения:**

1. **Недостаточно видеопамяти:**

```cpp
// Проверьте бюджет
VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
vmaGetHeapBudgets(allocator, budgets);
```

2. **Превышен лимит аллокаций:**

- Освободите неиспользуемые ресурсы
- Используйте пулы для уменьшения количества блоков

3. **Фрагментация:**

- Рассмотрите дефрагментацию
- Используйте пулы для объектов одного размера

---

### VK_ERROR_FEATURE_NOT_PRESENT

**Причина:** Запрошена возможность, которую устройство не поддерживает.

**Частые случаи:**

- `VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED` на десктопе
- `VK_KHR_buffer_device_address` без включения расширения

**Решение:** Проверьте поддержку возможностей устройства перед использованием.

---

## Validation Layer

### Mapping is not allowed

**Сообщение:**

```
vkMapMemory(): Mapping memory without VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT is not allowed
```

**Причина:** Аллокация с `VMA_MEMORY_USAGE_AUTO` без флага HOST_ACCESS.

**Решение:**

```cpp
allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
```

---

### vkBindBufferMemory warning

**Сообщение:**

```
vkBindBufferMemory(): Binding memory to buffer ... but vkGetBufferMemoryRequirements() has not been called
```

**Причина:** Известное предупреждение при использовании `VK_KHR_dedicated_allocation`.

**Решение:** Можно игнорировать — это корректное поведение VMA.

---

## Отладка

### Включение debug статистики

```cpp
VmaTotalStatistics stats;
vmaCalculateStatistics(allocator, &stats);

printf("Allocations: %zu\n", stats.total.statistics.allocationCount);
printf("Used memory: %zu MB\n", stats.total.statistics.allocationBytes / (1024 * 1024));
printf("Unused memory: %zu MB\n", stats.total.statistics.unusedBytes / (1024 * 1024));
```

### JSON dump для анализа

```cpp
char* jsonString;
vmaBuildStatsString(allocator, &jsonString, VK_TRUE);
// Сохранить в файл для визуализации
vmaFreeStatsString(allocator, jsonString);
```

### Проверка бюджета памяти

```cpp
VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
vmaGetHeapBudgets(allocator, budgets);

for (uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; ++i) {
    if (budgets[i].budget > 0) {
        float usage = float(budgets[i].usage) / float(budgets[i].budget) * 100.0f;
        if (usage > 90.0f) {
            // Предупреждение: память почти исчерпана
        }
    }
}
```

---

## Чек-лист диагностики

1. `VMA_IMPLEMENTATION` определён только в одном .cpp?
2. При использовании volk: `VK_NO_PROTOTYPES` перед всеми includes?
3. При использовании volk: переданы `pVulkanFunctions`?
4. Для mappable-аллокаций: указан `HOST_ACCESS_*` флаг?
5. Для не-coherent памяти: вызван `vmaFlushAllocation` после записи?
6. Для readback: вызван `vmaInvalidateAllocation` перед чтением?
7. Проверен бюджет памяти через `vmaGetHeapBudgets`?

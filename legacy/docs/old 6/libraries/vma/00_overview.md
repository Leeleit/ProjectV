# Vulkan Memory Allocator (VMA)

**🟢 Уровень 1: Базовый**

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

## Документация

| Файл                                           | Уровень | Описание                                            |
|------------------------------------------------|---------|-----------------------------------------------------|
| [01_quickstart.md](01_quickstart.md)           | 🟢      | Минимальный пример: аллокатор, буфер, map/unmap     |
| [02_installation.md](02_installation.md)       | 🟢      | CMake, vcpkg, VMA_IMPLEMENTATION, интеграция с volk |
| [03_concepts.md](03_concepts.md)               | 🟡      | Типы памяти, паттерны: GPU-only, staging, readback  |
| [04_api-reference.md](04_api-reference.md)     | 🟡      | Справочник функций и структур                       |
| [05_troubleshooting.md](05_troubleshooting.md) | 🟡      | Ошибки и их решение                                 |
| [06_glossary.md](06_glossary.md)               | 🟢      | Термины и определения                               |

---

## Требования

- **C++14** или новее
- **Vulkan 1.0** или новее (рекомендуется 1.1+ для расширенных функций)
- Нет внешних зависимостей кроме Vulkan и стандартной библиотеки

---

## Лицензия

MIT License. Copyright (c) 2017-2024 Advanced Micro Devices, Inc.

---

## Ссылки

- **Исходный код:
  ** [GitHub - GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
- **Документация:** [Vulkan Memory Allocator (GPUOpen)](https://gpuopen.com/vulkan-memory-allocator/)
- **API Reference:** [Online Documentation](https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/)

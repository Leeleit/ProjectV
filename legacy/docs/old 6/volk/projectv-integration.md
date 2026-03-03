# Интеграция volk в ProjectV

**🔴 Уровень 3: Продвинутый** — Специфичные паттерны и рекомендации для воксельного движка.

## Оглавление

- [1. Инициализация с SDL3](#1-инициализация-с-sdl3)
- [2. Интеграция с VMA](#2-интеграция-с-vma)
- [3. Профилирование с Tracy](#3-профилирование-с-tracy)
- [4. Оптимизации для воксельного рендеринга](#4-оптимизации-для-воксельного-рендеринга)
- [5. Многопоточность и множественные устройства](#5-многопоточность-и-множественные-устройства)
- [6. Обработка ошибок и fallback](#6-обработка-ошибок-и-fallback)

---

## 1. Инициализация с SDL3

SDL3 при создании окна с `SDL_WINDOW_VULKAN` загружает Vulkan loader. Чтобы избежать дублирования и конфликтов,
используйте `volkInitializeCustom`.

### Рекомендуемый порядок инициализации

```cpp
#include "SDL3/SDL.h"
#include "volk.h"

bool init_vulkan_with_sdl(SDL_Window* window) {
    // 1. Используем loader, уже загруженный SDL
    auto vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
    if (!vkGetInstanceProcAddr) {
        // SDL не загрузил Vulkan loader
        return false;
    }
    
    // 2. Инициализация volk с кастомным handler
    volkInitializeCustom(vkGetInstanceProcAddr);
    
    // 3. Получаем расширения для SDL
    unsigned int extensionCount;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr)) {
        return false;
    }
    
    std::vector<const char*> extensions(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data());
    
    // 4. Создаем VkInstance с расширениями от SDL
    VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    
    VkInstance instance;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        return false;
    }
    
    // 5. Загружаем instance-функции
    volkLoadInstance(instance);
    
    // 6. Создаем surface через SDL
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
        vkDestroyInstance(instance, nullptr);
        return false;
    }
    
    // 7. Выбор физического устройства и создание логического устройства
    // ... (стандартная процедура Vulkan)
    
    // 8. Загружаем device-функции для оптимизации
    volkLoadDevice(device);
    
    return true;
}
```

### Преимущества подхода

- **Единый loader**: Используем loader, уже загруженный SDL
- **Нет конфликтов**: Избегаем дублирования Vulkan loader в памяти
- **Автоматические расширения**: SDL предоставляет необходимые расширения для платформы

---

## 2. Интеграция с VMA

Vulkan Memory Allocator (VMA) требует передачи указателей на Vulkan функции. volk предоставляет удобный способ импорта
этих функций.

### Импорт функций для VMA

```cpp
#include "volk.h"
#include "vk_mem_alloc.h"

VmaAllocator create_vma_allocator(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device) {
    // 1. Импорт функций volk в VMA
    VmaVulkanFunctions vmaFunctions = {};
    vmaImportVulkanFunctionsFromVolk(&vmaFunctions);
    
    // 2. Создание аллокатора VMA
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.instance = instance;
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.pVulkanFunctions = &vmaFunctions;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    
    VmaAllocator allocator;
    if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    
    return allocator;
}
```

### Особенности для воксельного движка

#### Управление чанками

```cpp
struct VoxelChunk {
    VkBuffer vertexBuffer;
    VkBuffer indexBuffer;
    VmaAllocation vertexAllocation;
    VmaAllocation indexAllocation;
};

VoxelChunk create_voxel_chunk(VmaAllocator allocator, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    VoxelChunk chunk = {};
    
    // Создание vertex buffer
    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = vertices.size() * sizeof(Vertex);
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &chunk.vertexBuffer, &chunk.vertexAllocation, nullptr);
    
    // Создание index buffer
    bufferInfo.size = indices.size() * sizeof(uint32_t);
    bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &chunk.indexBuffer, &chunk.indexAllocation, nullptr);
    
    return chunk;
}
```

#### Оптимизация частых аллокаций

Для воксельного движка с частым созданием/удалением чанков:

- Используйте `VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT` для пулов
- Создайте отдельные пулы для разных типов ресурсов (vertex, index, texture)
- Используйте `VMA_ALLOCATION_CREATE_MAPPED_BIT` для частых обновлений

---

## 3. Профилирование с Tracy

Tracy позволяет измерять производительность Vulkan вызовов. При использовании volk важно передавать правильные указатели
на функции.

### Настройка Tracy для volk

```cpp
#include "volk.h"
#include "tracy/TracyVulkan.hpp"

struct TracyVulkanContext {
    TracyVkCtx context;
    
    TracyVulkanContext(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue, VkCommandBuffer commandBuffer) {
        // Используем указатели функций volk для точных измерений
        auto vkGetInstanceProcAddr = volkGetInstanceProcAddr;
        auto vkGetDeviceProcAddr = volkGetDeviceProcAddr;
        
        // Создание контекста Tracy
        context = TracyVkContextCalibrated(
            physicalDevice,
            device,
            queue,
            commandBuffer,
            vkGetInstanceProcAddr,
            vkGetDeviceProcAddr
        );
    }
    
    ~TracyVulkanContext() {
        TracyVkDestroy(context);
    }
};
```

### Измерение dispatch overhead

```cpp
// В рендер-цикле
void render_voxel_chunk(TracyVkCtx tracyContext, VkCommandBuffer cmd, const VoxelChunk& chunk) {
    ZoneScopedN("Voxel Rendering");
    TracyVkZone(tracyContext, cmd, "Render Voxel Chunk");
    
    // Привязка буферов
    VkBuffer vertexBuffers[] = { chunk.vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmd, chunk.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    
    // Вызов отрисовки (через volk - прямой вызов драйвера)
    vkCmdDrawIndexed(cmd, chunk.indexCount, 1, 0, 0, 0);
}
```

### Анализ производительности

- **Dispatch overhead**: Сравните время выполнения `vkCmdDraw*` с volk и без него
- **Batch rendering**: Измеряйте эффективность multi-draw indirect
- **Memory allocation**: Профилируйте аллокации VMA через Tracy

---

## 4. Оптимизации для воксельного рендеринга

### Multi-Draw Indirect

Для рендеринга тысяч воксельных чанков используйте multi-draw indirect:

```cpp
void setup_multi_draw_indirect(VkDevice device, VmaAllocator allocator, 
                               const std::vector<VoxelChunk>& chunks) {
    std::vector<VkDrawIndexedIndirectCommand> commands;
    commands.reserve(chunks.size());
    
    uint32_t firstIndex = 0;
    uint32_t vertexOffset = 0;
    
    for (const auto& chunk : chunks) {
        VkDrawIndexedIndirectCommand cmd = {};
        cmd.indexCount = chunk.indexCount;
        cmd.instanceCount = 1;
        cmd.firstIndex = firstIndex;
        cmd.vertexOffset = static_cast<int32_t>(vertexOffset);
        cmd.firstInstance = 0;
        
        commands.push_back(cmd);
        
        firstIndex += chunk.indexCount;
        vertexOffset += chunk.vertexCount;
    }
    
    // Создание indirect buffer
    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = commands.size() * sizeof(VkDrawIndexedIndirectCommand);
    bufferInfo.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    
    VkBuffer indirectBuffer;
    VmaAllocation indirectAllocation;
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &indirectBuffer, &indirectAllocation, nullptr);
    
    // Копирование команд в буфер
    // ... (используйте staging buffer или vmaCreateBuffer с VMA_MEMORY_USAGE_CPU_TO_GPU)
    
    // Рендеринг всех чанков одним вызовом
    // vkCmdDrawIndexedIndirect(commandBuffer, indirectBuffer, 0, 
    //                         static_cast<uint32_t>(commands.size()), 
    //                         sizeof(VkDrawIndexedIndirectCommand));
}
```

### Compute Shaders для генерации вокселей

Для compute shaders используйте `VolkDeviceTable` для изоляции compute queue:

```cpp
struct ComputeContext {
    VolkDeviceTable computeTable;
    VkDevice computeDevice;
    VkQueue computeQueue;
    
    ComputeContext(VkDevice device, uint32_t computeQueueFamilyIndex) {
        computeDevice = device;
        
        // Получаем compute queue
        vkGetDeviceQueue(device, computeQueueFamilyIndex, 0, &computeQueue);
        
        // Загружаем отдельную таблицу функций для compute
        volkLoadDeviceTable(&computeTable, device);
    }
    
    void dispatch_voxel_generation(VkCommandBuffer cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
        // Используем функции из таблицы
        computeTable.vkCmdDispatch(cmd, groupCountX, groupCountY, groupCountZ);
    }
};
```

---

## 5. Многопоточность и множественные устройства

### Thread Safety

Глобальные указатели volk не thread-safe во время инициализации:

```cpp
class ThreadSafeVolkInitializer {
    std::once_flag initFlag;
    
public:
    void initialize() {
        std::call_once(initFlag, []() {
            // Инициализация только в одном потоке
            if (volkInitialize() != VK_SUCCESS) {
                throw std::runtime_error("Failed to initialize volk");
            }
        });
    }
};
```

### Несколько VkDevice

Для приложений с несколькими устройствами (main GPU + compute GPU):

```cpp
struct MultiDeviceContext {
    VkDevice mainDevice;
    VkDevice computeDevice;
    VolkDeviceTable mainTable;
    VolkDeviceTable computeTable;
    
    MultiDeviceContext(VkPhysicalDevice mainPhysical, VkPhysicalDevice computePhysical) {
        // Создание основного устройства
        VkDeviceCreateInfo deviceInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        // ... настройка deviceInfo
        
        vkCreateDevice(mainPhysical, &deviceInfo, nullptr, &mainDevice);
        volkLoadDeviceTable(&mainTable, mainDevice);
        
        // Создание compute устройства
        // ... отдельная конфигурация для compute
        
        vkCreateDevice(computePhysical, &deviceInfo, nullptr, &computeDevice);
        volkLoadDeviceTable(&computeTable, computeDevice);
    }
    
    ~MultiDeviceContext() {
        mainTable.vkDestroyDevice(mainDevice, nullptr);
        computeTable.vkDestroyDevice(computeDevice, nullptr);
    }
};
```

---

## 6. Обработка ошибок и fallback

### Graceful Degradation

При отсутствии Vulkan loader можно реализовать fallback:

```cpp
enum class GraphicsAPI {
    Vulkan,
    OpenGL,
    Direct3D,
    Software
};

GraphicsAPI detect_graphics_api() {
    // Пытаемся инициализировать volk
    VkResult result = volkInitialize();
    
    if (result == VK_SUCCESS) {
        uint32_t version = volkGetInstanceVersion();
        if (VK_VERSION_MAJOR(version) >= 1 && VK_VERSION_MINOR(version) >= 2) {
            return GraphicsAPI::Vulkan;
        }
        // Vulkan есть, но версия слишком старая
        volkFinalize();
    }
    
    // Проверяем другие API
    if (detect_opengl()) {
        return GraphicsAPI::OpenGL;
    }
    
    if (detect_direct3d()) {
        return GraphicsAPI::Direct3D;
    }
    
    return GraphicsAPI::Software;
}
```

### Валидация во время выполнения

```cpp
bool validate_volk_initialization() {
    if (volkGetLoadedDevice() == VK_NULL_HANDLE) {
        LOG_ERROR("volkLoadDevice() was not called");
        return false;
    }
    
    if (volkGetLoadedInstance() == VK_NULL_HANDLE) {
        LOG_ERROR("volkLoadInstance() was not called");
        return false;
    }
    
    // Проверяем ключевые функции
    if (!vkCmdDraw || !vkCreateBuffer || !vkQueueSubmit) {
        LOG_ERROR("Critical Vulkan functions are NULL");
        return false;
    }
    
    return true;
}
```

---

## Рекомендации для ProjectV

### Для разработки

1. **Всегда вызывайте `volkLoadDevice(device)`** для основного устройства рендеринга
2. **Используйте `VolkDeviceTable` для compute операций** и многопоточности
3. **Интегрируйте с VMA сразу** через `vmaImportVulkanFunctionsFromVolk`
4. **Профилируйте с Tracy** для измерения реального выигрыша от volk

### Для отладки

1. **Включите validation layers в Debug сборке** - volk полностью совместим
2. **Проверяйте инициализацию** через `volkGetLoadedDevice()` и `volkGetLoadedInstance()`
3. **Измеряйте dispatch overhead** до и после оптимизаций

### Для производства

1. **Отключите validation layers в Release**
2. **Используйте `volkLoadDevice` для всех устройств**
3. **Рассмотрите `VOLK_NO_DEVICE_PROTOTYPES`** для статической проверки типов
4. **Настройте CMake с `VOLK_NAMESPACE`** для избежания конфликтов символов

---

## См. также

- [Основная документация volk](README.md) — общая информация и базовое использование
- [Архитектура volk](concepts.md) — глубокое понимание работы мета-загрузчика
- [Производительность](performance.md) — бенчмарки и оптимизации
- [Решение проблем](troubleshooting.md) — диагностика ошибок инициализации

← [Назад к основной документации volk](README.md)
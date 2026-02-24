# Vulkan Troubleshooting

Решение типичных проблем Vulkan: validation errors, ошибки синхронизации, проблемы с памятью.

---

## Validation Layers

### Включение validation layers

```cpp
const char* layers[] = {
    "VK_LAYER_KHRONOS_validation"
};

VkInstanceCreateInfo createInfo = {};
createInfo.enabledLayerCount = 1;
createInfo.ppEnabledLayerNames = layers;
```

### Расширенные опции валидации

```cpp
VkValidationFeaturesEXT features = {};
features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;

VkValidationFeatureEnableEXT enables[] = {
    VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
    VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
    VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT
};
features.enabledValidationFeatureCount = 3;
features.pEnabledValidationFeatures = enables;

VkInstanceCreateInfo createInfo = {};
createInfo.pNext = &features;
```

### Custom Debug Callback

```cpp
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* userData)
{
    // Фильтрация по severity
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        fprintf(stderr, "[ERROR] %s\n", data->pMessage);
    } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        fprintf(stderr, "[WARNING] %s\n", data->pMessage);
    }

    // Breakpoint при ошибке
    #ifdef _WIN32
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        DebugBreak();
    }
    #endif

    return VK_FALSE;
}
```

---

## Типичные ошибки

### VK_ERROR_OUT_OF_HOST_MEMORY

**Причина:** Недостаточно системной памяти.

**Решение:**

- Проверить утечки памяти
- Уменьшить количество аллокаций
- Использовать VMA для статистики

```cpp
VmaStats stats;
vmaCalculateStats(allocator, &stats);
printf("Allocated: %zu bytes\n", stats.total.allocationBytes);
```

### VK_ERROR_OUT_OF_DEVICE_MEMORY

**Причина:** Недостаточно видеопамяти.

**Решение:**

- Проверить размер буферов/изображений
- Использовать memory aliasing
- Освободить неиспользуемые ресурсы

```cpp
// Запрос бюджета памяти
VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {};
budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;

VkPhysicalDeviceMemoryProperties2 props = {};
props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
props.pNext = &budget;

vkGetPhysicalDeviceMemoryProperties2(physicalDevice, &props);

for (uint32_t i = 0; i < budget.heapCount; i++) {
    printf("Heap %d: %zu / %zu bytes used\n",
           i, budget.heapUsage[i], budget.heapBudget[i]);
}
```

### VK_ERROR_DEVICE_LOST

**Причина:** GPU завис или перезагрузился.

**Возможные причины:**

- Бесконечный цикл в шейдере
- Timeout (TDR) в Windows
- Аппаратная ошибка

**Решение:**

- Увеличить TDR timeout в реестре Windows
- Проверить шейдеры на бесконечные циклы
- Добавить проверки в шейдеры

```
// Защита от бесконечных циклов
for (int i = 0; i < MAX_ITERATIONS; i++) {
    if (converged) break;
}
```

### VK_ERROR_OUT_OF_DATE_KHR

**Причина:** Swapchain не соответствует поверхности (resize, DPI change).

**Решение:**

```cpp
// При resize окна
if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    recreateSwapchain();
}
```

### VK_ERROR_SURFACE_LOST_KHR

**Причина:** Surface уничтожен (окно закрыто/минимизировано).

**Решение:**

```cpp
if (result == VK_ERROR_SURFACE_LOST_KHR) {
    // Surface нужно пересоздать
    vkDestroySurfaceKHR(instance, surface, nullptr);
    surface = createSurface();
}
```

### VK_ERROR_INCOMPATIBLE_DRIVER

**Причина:** Драйвер не поддерживает запрошенную версию Vulkan.

**Решение:**

- Обновить драйверы GPU
- Понизить `apiVersion` в `VkApplicationInfo`
- Проверить поддержку Vulkan драйвером

---

## Проблемы синхронизации

### Признаки проблем синхронизации

- Мигание артефактов
- "Рваные" текстуры
- Случайные вылеты
- Разные результаты каждый кадр

### Validation Sync Validation

```cpp
VkValidationFeatureEnableEXT enables[] = {
    VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
};
```

### Типичные ошибки синхронизации

#### Отсутствие barrier между записью и чтением

```cpp
// НЕПРАВИЛЬНО: Нет синхронизации
vkCmdDispatch(cmd, ...);  // Запись в buffer
vkCmdDraw(cmd, ...);      // Чтение из buffer

// ПРАВИЛЬНО: Явный barrier
vkCmdDispatch(cmd, ...);

VkMemoryBarrier barrier = {};
barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;

vkCmdPipelineBarrier(cmd,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
    0, 1, &barrier, 0, nullptr, 0, nullptr);

vkCmdDraw(cmd, ...);
```

#### Неправильный layout transition

```cpp
// НЕПРАВИЛЬНО: Layout не соответствует использованию
// Image в VK_IMAGE_LAYOUT_UNDEFINED при draw

// ПРАВИЛЬНО: Явный transition
VkImageMemoryBarrier barrier = {};
barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
barrier.srcAccessMask = 0;
barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
barrier.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
barrier.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

vkCmdPipelineBarrier(cmd, ...);
```

#### Забытый fence wait

```cpp
// НЕПРАВИЛЬНО: Повторное использование без ожидания
vkQueueSubmit(queue, 1, &submit, fence);
// Сразу отправляем снова
vkQueueSubmit(queue, 1, &submit, fence);  // FENCE_ALREADY_SUBMITTED

// ПРАВИЛЬНО: Ждать и сбросить
vkQueueSubmit(queue, 1, &submit, fence);
vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
vkResetFences(device, 1, &fence);
```

---

## Проблемы с памятью

### Buffer/Image не привязан к памяти

**Ошибка:** `VUID-vkBindBufferMemory-buffer-01030`

**Причина:** Забыт `vkBindBufferMemory` / `vkBindImageMemory`.

```cpp
VkBuffer buffer;
vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);

VkMemoryRequirements reqs;
vkGetBufferMemoryRequirements(device, buffer, &reqs);

VkDeviceMemory memory;
vkAllocateMemory(device, &allocInfo, nullptr, &memory);

// ОБЯЗАТЕЛЬНО!
vkBindBufferMemory(device, buffer, memory, 0);
```

### Неправильный memory type

```cpp
// Утилита для выбора memory type
uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}
```

### Mapping non-host-visible памяти

**Ошибка:** `VK_ERROR_MEMORY_MAP_FAILED`

**Причина:** Попытка map память без `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`.

```cpp
// Проверить поддержку HOST_VISIBLE
VkMemoryPropertyFlags flags = memProps.memoryTypes[memoryTypeIndex].propertyFlags;
if (!(flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
    // Нельзя map, нужен staging buffer
}
```

---

## Проблемы с Swapchain

### Отсутствие поддержки present

**Ошибка:** Изображения не отображаются.

**Проверка:**

```cpp
VkBool32 supported = SDL_Vulkan_GetPresentationSupport(
    instance, physicalDevice, queueFamilyIndex
);
if (!supported) {
    // Queue family не поддерживает present
}
```

### Неправильный формат

```cpp
// Не все форматы поддерживаются для swapchain
VkSurfaceFormatKHR chooseFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    // Prefer SRGB
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    return formats[0];  // Fallback
}
```

### Swapchain minimization

```cpp
void handleResize() {
    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    // Обработка минимизации
    while (width == 0 || height == 0) {
        SDL_GetWindowSize(window, &width, &height);
        SDL_WaitEvent(nullptr);
    }

    recreateSwapchain();
}
```

---

## Отладка шейдеров

### Shader printf (VK_KHR_shader_non_semantic_info)

```glsl
#version 460
#extension GL_EXT_debug_printf : enable

void main() {
    debugPrintfEXT("Vertex index: %d\n", gl_VertexIndex);
}
```

### Включение debug printf

```cpp
VkDebugUtilsMessengerCreateInfoEXT debugInfo = {};
debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
```

### RenderDoc shader debugging

1. Capture frame в RenderDoc
2. Открыть Pipeline State
3. Выбрать shader stage
4. Просмотреть входные/выходные данные
5. Debugger для SPIR-V

### Common shader errors

| Ошибка GLSL                 | Причина            | Решение             |
|-----------------------------|--------------------|---------------------|
| `undeclared identifier`     | Забытая переменная | Добавить объявление |
| `type mismatch`             | Несовместимые типы | Явное приведение    |
| `array index out of bounds` | Выход за границы   | Проверка индекса    |
| `division by zero`          | Деление на 0       | Добавить проверку   |
| `infinite loop`             | Бесконечный цикл   | Ограничить итерации |

---

## Инструменты отладки

### RenderDoc

```cpp
// Программный trigger capture
#ifdef _WIN32
#include <windows.h>
#include <renderdoc_app.h>

RENDERDOC_API_1_1_2* rdoc_api = nullptr;

void initRenderDoc() {
    HMODULE mod = GetModuleHandleA("renderdoc.dll");
    if (mod) {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI =
            (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
        RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&rdoc_api);
    }
}

void triggerCapture() {
    if (rdoc_api) {
        rdoc_api->TriggerCapture();
    }
}
#endif
```

### Vulkan Configurator (vkconfig)

GUI для настройки:

- Включение/выключение layers
- Настройка validation features
- Сохранение конфигураций

### Vulkan Info

```bash
# Информация о GPU и поддержке Vulkan
vulkaninfo

# JSON для VK_JSON=1
vulkaninfo --json
```

### Nsight Graphics (NVIDIA)

- GPU trace
- Range profiling
- Shader profiler

### Radeon GPU Profiler (AMD)

- Frame profiling
- Wavefront occupancy
- Memory bandwidth

---

## Чеклист отладки

| Проблема          | Проверка          | Инструмент      |
|-------------------|-------------------|-----------------|
| Validation errors | Debug callback    | Layers          |
| GPU hang          | Timeout, TDR      | RenderDoc       |
| Artifacts         | Barriers, layouts | Sync validation |
| Memory issues     | Budget, leaks     | VMA stats       |
| Shader bugs       | Printf, debugger  | RenderDoc       |
| Performance       | Timestamps        | Profiler        |

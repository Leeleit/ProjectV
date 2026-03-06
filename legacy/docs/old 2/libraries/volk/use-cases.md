# Сценарии использования volk

**🔴 Уровень 3: Продвинутый** — Универсальные сценарии использования volk с различными библиотеками и фреймворками.

## Обзор

volk предоставляет гибкие возможности интеграции с различными библиотеками и фреймворками благодаря своей архитектуре
мета-загрузчика. В этом разделе рассматриваются универсальные сценарии использования volk, которые могут быть применены
в различных проектах, не только в ProjectV.

## Сценарий 1: Интеграция с оконными библиотеками

### SDL3

SDL3 при создании окна с `SDL_WINDOW_VULKAN` загружает Vulkan loader. Чтобы избежать дублирования и конфликтов,
используйте `volkInitializeCustom`.

```cpp
bool init_vulkan_with_sdl(SDL_Window* window) {
    // 1. Используем loader, уже загруженный SDL
    auto vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
    volkInitializeCustom(vkGetInstanceProcAddr);
    
    // 2. Получаем необходимые расширения от SDL
    unsigned int extensionCount;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr)) {
        return false;
    }
    
    std::vector<const char*> extensions(extensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data())) {
        return false;
    }
    
    // 3. Создаем VkInstance с расширениями от SDL
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    
    VkInstance instance;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        return false;
    }
    
    // 4. Загружаем instance-функции
    volkLoadInstance(instance);
    
    // 5. Создаем surface через SDL
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
        vkDestroyInstance(instance, nullptr);
        return false;
    }
    
    return true;
}
```

### GLFW

GLFW также предоставляет функции для работы с Vulkan. Интеграция аналогична SDL3:

```cpp
bool init_vulkan_with_glfw(GLFWwindow* window) {
    // 1. Инициализируем volk стандартным способом
    if (volkInitialize() != VK_SUCCESS) {
        return false;
    }
    
    // 2. Получаем необходимые расширения от GLFW
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    
    // 3. Создаем VkInstance
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;
    
    VkInstance instance;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        return false;
    }
    
    // 4. Загружаем instance-функции
    volkLoadInstance(instance);
    
    // 5. Создаем surface через GLFW
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        vkDestroyInstance(instance, nullptr);
        return false;
    }
    
    return true;
}
```

## Сценарий 2: Интеграция с библиотеками управления памятью

### Vulkan Memory Allocator (VMA)

Для корректной работы VMA с volk необходимо передать указатели на функции Vulkan.

```cpp
#include "vk_mem_alloc.h"

// Функция для импорта указателей функций Vulkan из volk в VMA
void vmaImportVulkanFunctionsFromVolk(VmaVulkanFunctions* outFunctions) {
    outFunctions->vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    outFunctions->vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    outFunctions->vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    outFunctions->vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    outFunctions->vkAllocateMemory = vkAllocateMemory;
    outFunctions->vkFreeMemory = vkFreeMemory;
    outFunctions->vkMapMemory = vkMapMemory;
    outFunctions->vkUnmapMemory = vkUnmapMemory;
    outFunctions->vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
    outFunctions->vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
    outFunctions->vkBindBufferMemory = vkBindBufferMemory;
    outFunctions->vkBindImageMemory = vkBindImageMemory;
    outFunctions->vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
    outFunctions->vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
    outFunctions->vkCreateBuffer = vkCreateBuffer;
    outFunctions->vkDestroyBuffer = vkDestroyBuffer;
    outFunctions->vkCreateImage = vkCreateImage;
    outFunctions->vkDestroyImage = vkDestroyImage;
    outFunctions->vkCmdCopyBuffer = vkCmdCopyBuffer;
}

bool init_vma_with_volk(VkDevice device, VkPhysicalDevice physicalDevice, VmaAllocator* outAllocator) {
    VmaVulkanFunctions vmaFunctions = {};
    vmaImportVulkanFunctionsFromVolk(&vmaFunctions);
    
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.pVulkanFunctions = &vmaFunctions;
    allocatorInfo.instance = volkGetLoadedInstance();
    
    return vmaCreateAllocator(&allocatorInfo, outAllocator) == VK_SUCCESS;
}
```

## Сценарий 3: Интеграция с профилировщиками

### Tracy

Tracy требует передачи указателей на функции Vulkan для профилирования GPU.

```cpp
#include "tracy/TracyVulkan.hpp"

bool init_tracy_with_volk(VkDevice device, VkQueue queue, TracyVkContext* outContext) {
    // Убедитесь, что volkLoadDevice был вызван
    if (volkGetLoadedDevice() != device) {
        volkLoadDevice(device);
    }
    
    // Создаем контекст Tracy с указателями функций из volk
    *outContext = TracyVkContext(
        device,
        queue,
        vkQueueSubmit,          // Указатель из volk
        vkCmdBeginQuery,        // Указатель из volk
        vkCmdEndQuery           // Указатель из volk
    );
    
    return *outContext != nullptr;
}

// Использование в рендер-цикле
void render_frame(TracyVkContext tracyContext, VkCommandBuffer commandBuffer) {
    TracyVkZone(tracyContext, commandBuffer, "Frame Rendering");
    
    // Ваш код рендеринга
    vkCmdDraw(commandBuffer, ...);
    
    TracyVkCollect(tracyContext, commandBuffer);
}
```

## Сценарий 4: Многопоточные приложения

volk может использоваться в многопоточных приложениях с некоторыми ограничениями:

```cpp
// Потокобезопасная инициализация volk
class VulkanContext {
public:
    VulkanContext() {
        std::call_once(initFlag, []() {
            if (volkInitialize() != VK_SUCCESS) {
                throw std::runtime_error("Failed to initialize Vulkan");
            }
        });
    }
    
    void createInstance() {
        std::lock_guard<std::mutex> lock(instanceMutex);
        
        VkInstanceCreateInfo createInfo = {};
        // ... настройка createInfo
        
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance");
        }
        
        volkLoadInstance(instance);
    }
    
    void createDevice() {
        std::lock_guard<std::mutex> lock(deviceMutex);
        
        // ... создание устройства
        
        volkLoadDevice(device);
    }
    
    // Использование Vulkan функций из разных потоков безопасно после инициализации
    void drawFromThread() {
        // vkCmdDraw безопасен из любого потока после volkLoadDevice
        vkCmdDraw(commandBuffer, ...);
    }
    
private:
    static std::once_flag initFlag;
    std::mutex instanceMutex;
    std::mutex deviceMutex;
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
};
```

## Сценарий 5: Приложения с несколькими VkDevice

Для приложений, использующих несколько Vulkan устройств (например, multi-GPU рендеринг), используйте таблицы функций:

```cpp
struct DeviceContext {
    VkDevice device;
    VolkDeviceTable table;
    VkQueue graphicsQueue;
};

std::vector<DeviceContext> createMultipleDevices(VkInstance instance) {
    std::vector<DeviceContext> contexts;
    
    // Перечисляем физические устройства
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    
    std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());
    
    // Создаем логическое устройство для каждого физического
    for (VkPhysicalDevice physicalDevice : physicalDevices) {
        DeviceContext context;
        
        // ... создание логического устройства
        
        // Загружаем таблицу функций для этого устройства
        volkLoadDeviceTable(&context.table, context.device);
        
        contexts.push_back(context);
    }
    
    return contexts;
}

// Использование таблиц функций
void renderWithDeviceTable(const DeviceContext& context, VkCommandBuffer cmd) {
    // Вместо прямого вызова vkCmdDraw используем таблицу
    context.table.vkCmdDraw(cmd, ...);
    context.table.vkCmdEndRenderPass(cmd);
}
```

## Сценарий 6: Динамическое переключение между loader и volk

Некоторые приложения могут требовать возможности работы как с системным loader, так и с volk:

```cpp
class VulkanLoader {
public:
    enum class LoaderType {
        SystemLoader,  // Стандартный Vulkan loader
        VolkLoader     // volk мета-загрузчик
    };
    
    VulkanLoader(LoaderType type) : type(type) {
        if (type == LoaderType::VolkLoader) {
            if (volkInitialize() != VK_SUCCESS) {
                throw std::runtime_error("Failed to initialize volk");
            }
        }
        // При использовании SystemLoader ничего не делаем
    }
    
    VkResult createInstance(const VkInstanceCreateInfo* pCreateInfo, VkInstance* pInstance) {
        VkResult result;
        
        if (type == LoaderType::VolkLoader) {
            result = vkCreateInstance(pCreateInfo, nullptr, pInstance);
            if (result == VK_SUCCESS) {
                volkLoadInstance(*pInstance);
            }
        } else {
            // Используем системный loader
            // Требует линковки с vulkan-1.dll/libvulkan.so
            result = vkCreateInstance(pCreateInfo, nullptr, pInstance);
        }
        
        return result;
    }
    
private:
    LoaderType type;
};
```

## Сценарий 7: Поддержка расширений Vulkan

volk автоматически загружает функции расширений после вызова `volkLoadInstance`:

```cpp
bool check_and_use_ray_tracing(VkPhysicalDevice physicalDevice, VkDevice device) {
    // Проверяем поддержку расширения
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data());
    
    bool hasRayTracing = false;
    for (const auto& ext : extensions) {
        if (strcmp(ext.extensionName, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 0) {
            hasRayTracing = true;
            break;
        }
    }
    
    if (hasRayTracing) {
        // Функции расширения уже загружены volkLoadInstance
        // Можно использовать их напрямую
        
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {};
        rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
        
        VkPhysicalDeviceProperties2 properties2 = {};
        properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties2.pNext = &rtProperties;
        
        vkGetPhysicalDeviceProperties2(physicalDevice, &properties2);
        
        // Использование функций расширения
        // vkCmdTraceRaysKHR уже доступна через volk
        
        return true;
    }
    
    return false;
}
```

## Рекомендации по выбору сценария

| Сценарий                  | Когда использовать                   | Преимущества                                         | Недостатки                              |
|---------------------------|--------------------------------------|------------------------------------------------------|-----------------------------------------|
| **Базовый с volk**        | Простые приложения, одно устройство  | Простота, производительность                         | Одно устройство, нет многопоточности    |
| **Table-based**           | Несколько устройств, многопоточность | Поддержка multi-GPU, thread safety                   | Сложнее код, нужно использовать таблицы |
| **Интеграция с SDL/GLFW** | Оконные приложения                   | Автоматическая загрузка расширений, surface creation | Зависимость от оконной библиотеки       |
| **С VMA/Tracy**           | Продвинутые графические приложения   | Управление памятью, профилирование                   | Дополнительные зависимости              |

← [Назад к README](README.md) | [Далее: Производительность](performance.md) →

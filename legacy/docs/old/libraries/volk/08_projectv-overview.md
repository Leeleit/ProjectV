# volk в ProjectV

**🟡 Уровень 2: Средний**

Роль volk в архитектуре движка.

---

## Назначение

volk — ключевой компонент ProjectV для:

1. **Динамической загрузки Vulkan** — движок запускается без установленного Vulkan SDK
2. **Оптимизации производительности** — прямые вызовы драйвера для рендеринга вокселей
3. **Контроля зависимостей** — приложение не зависит от системного loader

---

## Место в архитектуре

```
┌─────────────────────────────────────────────────────┐
│                    ProjectV                         │
├─────────────────────────────────────────────────────┤
│  SDL3 (окна)     │  flecs (ECS)    │  Jolt (физика) │
├─────────────────────────────────────────────────────┤
│                    Vulkan Renderer                  │
│                         ↓                           │
│                      volk                           │
│                         ↓                           │
│              Vulkan Loader (vulkan-1.dll)           │
│                         ↓                           │
│                   GPU Driver                        │
└─────────────────────────────────────────────────────┘
```

---

## Интеграция с SDL3

SDL3 и volk работают вместе для создания Vulkan surface:

```cpp
#define VK_NO_PROTOTYPES
#include "volk.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

int main() {
    // 1. Инициализация SDL
    SDL_Init(SDL_INIT_VIDEO);

    // 2. Инициализация volk
    if (volkInitialize() != VK_SUCCESS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Vulkan not found", nullptr);
        return 1;
    }

    // 3. Создание окна с Vulkan support
    SDL_Window* window = SDL_CreateWindow(
        "ProjectV",
        1280, 720,
        SDL_WINDOW_VULKAN
    );

    // 4. Создание instance
    VkInstance instance;
    VkInstanceCreateInfo createInfo = {};
    // ... заполнение createInfo

    vkCreateInstance(&createInfo, nullptr, &instance);
    volkLoadInstance(instance);

    // 5. Создание surface через SDL
    VkSurfaceKHR surface;
    SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface);

    // ... продолжение инициализации Vulkan
}
```

---

## Интеграция с VMA

Vulkan Memory Allocator требует функции Vulkan. volk обеспечивает их:

```cpp
#define VMA_STATIC_VULKAN_FUNCTIONS 0  // VMA не будет искать функции
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1  // VMA будет использовать переданные

#include "vk_mem_alloc.h"

VmaAllocator createAllocator(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device) {
    VmaAllocatorCreateInfo createInfo = {};
    createInfo.instance = instance;
    createInfo.physicalDevice = physicalDevice;
    createInfo.device = device;
    createInfo.pVulkanFunctions = nullptr;  // VMA будет использовать vkGetInstanceProcAddr

    VmaAllocator allocator;
    vmaCreateAllocator(&createInfo, &allocator);
    return allocator;
}
```

**Важно:** VMA требует `volkLoadInstance` и `volkLoadDevice` перед созданием allocator.

---

## Интеграция с ImGui

ImGui использует Vulkan через volk:

```cpp
// ImGui инициализация Vulkan
ImGui_ImplVulkan_InitInfo initInfo = {};
initInfo.Instance = instance;
initInfo.PhysicalDevice = physicalDevice;
initInfo.Device = device;
initInfo.QueueFamily = queueFamilyIndex;
initInfo.Queue = queue;
initInfo.DescriptorPool = descriptorPool;

ImGui_ImplVulkan_Init(&initInfo, renderPass);
```

ImGui сам получает функции через `vkGetInstanceProcAddr`, который volk предоставляет.

---

## Интеграция с Tracy

Tracy профилировщик требует указателей на Vulkan функции:

```cpp
#include <tracy/TracyVulkan.hpp>

// Создание контекста Tracy для Vulkan
TracyVkCtx tracyCtx = TracyVkContext(
    physicalDevice,
    device,
    queue,
    commandBuffer,
    volkGetInstanceProcAddr,
    volkGetDeviceProcAddr
);

// Использование
{
    TracyVkZone(tracyCtx, commandBuffer, "RenderVoxels");
    vkCmdDraw(...);
}

// Очистка
TracyVkDestroy(tracyCtx);
```

---

## Конфигурация CMake

ProjectV использует статическую библиотеку volk:

```cmake
# CMakeLists.txt ProjectV

# Платформенные defines
if (WIN32)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX AND NOT APPLE)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XLIB_KHR)
elseif(APPLE)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_MACOS_MVK)
endif()

# Подключение volk
add_subdirectory(external/volk)

# Проект
add_executable(ProjectV src/main.cpp)
target_link_libraries(ProjectV PRIVATE volk)

# VK_NO_PROTOTYPES для всех файлов
target_compile_definitions(ProjectV PRIVATE VK_NO_PROTOTYPES)
```

---

## Порядок инициализации

```cpp
bool initializeVulkan() {
    // 1. volk
    if (volkInitialize() != VK_SUCCESS) {
        return false;
    }

    // 2. Instance
    VkInstance instance;
    vkCreateInstance(&instanceInfo, nullptr, &instance);
    volkLoadInstance(instance);

    // 3. Physical device
    VkPhysicalDevice physicalDevice;
    vkEnumeratePhysicalDevices(instance, &count, &physicalDevice);

    // 4. Device
    VkDevice device;
    vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device);
    volkLoadDevice(device);  // Прямые вызовы драйвера

    // 5. VMA
    VmaAllocator allocator;
    vmaCreateAllocator(&allocatorInfo, &allocator);

    // 6. Surface (SDL)
    VkSurfaceKHR surface;
    SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface);

    // 7. Swapchain, pipelines, etc.

    return true;
}
```

---

## Производительность воксельного рендеринга

Для воксельного движка критична производительность draw calls:

| Операция         | Вызовов/кадр | Влияние volk |
|------------------|--------------|--------------|
| Render chunk     | 100-1000     | Значительное |
| Draw voxels      | 1000-10000   | Критическое  |
| Compute dispatch | 10-100       | Умеренное    |

volk с `volkLoadDevice` даёт до 7% прироста для device-intensive workloads.

---

## Обработка ошибок

```cpp
bool checkVulkanSupport() {
    // Проверка наличия Vulkan
    if (volkInitialize() != VK_SUCCESS) {
        showErrorMessage("Vulkan not installed. Please install Vulkan SDK from vulkan.lunarg.com");
        return false;
    }

    // Проверка версии
    uint32_t version = volkGetInstanceVersion();
    if (version < VK_API_VERSION_1_2) {
        showErrorMessage("Vulkan 1.2+ required. Current version: %u.%u",
                        VK_VERSION_MAJOR(version), VK_VERSION_MINOR(version));
        return false;
    }

    return true;
}

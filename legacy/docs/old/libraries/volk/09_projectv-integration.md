# Интеграция volk в ProjectV

**🟡 Уровень 2: Средний**

Практические примеры использования в коде проекта.

---

## Структура файлов

```
ProjectV/
├── src/
│   ├── main.cpp
│   ├── vulkan/
│   │   ├── vulkan_context.hpp
│   │   ├── vulkan_context.cpp
│   │   ├── device.hpp
│   │   └── device.cpp
│   └── ...
├── external/
│   └── volk/
└── CMakeLists.txt
```

---

## Главный файл (main.cpp)

```cpp
#define VK_NO_PROTOTYPES
#define VOLK_IMPLEMENTATION
#include "volk.h"

#include "vulkan/vulkan_context.hpp"
#include <SDL3/SDL.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    // SDL инициализация
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // volk инициализация
    if (volkInitialize() != VK_SUCCESS) {
        fprintf(stderr, "Vulkan not found\n");
        SDL_Quit();
        return 1;
    }

    printf("Vulkan %u.%u.%u\n",
           VK_VERSION_MAJOR(volkGetInstanceVersion()),
           VK_VERSION_MINOR(volkGetInstanceVersion()),
           VK_VERSION_PATCH(volkGetInstanceVersion()));

    // Создание Vulkan context
    VulkanContext vkContext;
    if (!vkContext.initialize()) {
        fprintf(stderr, "Failed to initialize Vulkan\n");
        SDL_Quit();
        return 1;
    }

    // Основной цикл
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        vkContext.render();
    }

    // Очистка
    vkContext.shutdown();
    SDL_Quit();

    return 0;
}
```

---

## Vulkan Context Header

```cpp
// vulkan/vulkan_context.hpp
#pragma once

#define VK_NO_PROTOTYPES
#include "volk.h"
#include <SDL3/SDL_video.h>

class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    bool initialize();
    void render();
    void shutdown();

private:
    bool createInstance();
    bool createSurface();
    bool pickPhysicalDevice();
    bool createDevice();
    bool createSwapchain();

    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;

    SDL_Window* m_window = nullptr;
    uint32_t m_graphicsQueueFamily = 0;
    uint32_t m_presentQueueFamily = 0;
};
```

---

## Vulkan Context Implementation

```cpp
// vulkan/vulkan_context.cpp
#include "vulkan_context.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>
#include <stdio.h>

VulkanContext::VulkanContext() = default;
VulkanContext::~VulkanContext() = default;

bool VulkanContext::initialize() {
    // Создание окна
    m_window = SDL_CreateWindow(
        "ProjectV",
        1280, 720,
        SDL_WINDOW_VULKAN
    );
    if (!m_window) {
        fprintf(stderr, "SDL_CreateWindow failed\n");
        return false;
    }

    if (!createInstance()) return false;
    if (!createSurface()) return false;
    if (!pickPhysicalDevice()) return false;
    if (!createDevice()) return false;
    if (!createSwapchain()) return false;

    return true;
}

bool VulkanContext::createInstance() {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "ProjectV";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.pEngineName = "ProjectV Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    // Получение extensions от SDL
    uint32_t extensionCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = sdlExtensions;

    // Validation layers (только Debug)
#ifdef DEBUG
    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = layers;
#endif

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
        fprintf(stderr, "vkCreateInstance failed\n");
        return false;
    }

    volkLoadInstance(m_instance);
    printf("VkInstance created\n");
    return true;
}

bool VulkanContext::createSurface() {
    if (!SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_surface)) {
        fprintf(stderr, "SDL_Vulkan_CreateSurface failed\n");
        return false;
    }
    return true;
}

bool VulkanContext::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        fprintf(stderr, "No Vulkan devices\n");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    // Выбираем первое дискретное GPU
    for (VkPhysicalDevice device : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);

        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            m_physicalDevice = device;
            printf("Selected GPU: %s\n", props.deviceName);
            break;
        }
    }

    if (m_physicalDevice == VK_NULL_HANDLE) {
        m_physicalDevice = devices[0];
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
        printf("Selected GPU: %s\n", props.deviceName);
    }

    return true;
}

bool VulkanContext::createDevice() {
    // Поиск queue families
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            m_graphicsQueueFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surface, &presentSupport);
        if (presentSupport) {
            m_presentQueueFamily = i;
        }
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfos[2] = {};
    uint32_t queueInfoCount = 0;

    queueInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfos[0].queueFamilyIndex = m_graphicsQueueFamily;
    queueInfos[0].queueCount = 1;
    queueInfos[0].pQueuePriorities = &queuePriority;
    queueInfoCount = 1;

    if (m_presentQueueFamily != m_graphicsQueueFamily) {
        queueInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[1].queueFamilyIndex = m_presentQueueFamily;
        queueInfos[1].queueCount = 1;
        queueInfos[1].pQueuePriorities = &queuePriority;
        queueInfoCount = 2;
    }

    const char* extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = queueInfoCount;
    createInfo.pQueueCreateInfos = queueInfos;
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = extensions;

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
        fprintf(stderr, "vkCreateDevice failed\n");
        return false;
    }

    // Критически важно: загрузка device-функций
    volkLoadDevice(m_device);

    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentQueueFamily, 0, &m_presentQueue);

    printf("VkDevice created\n");
    return true;
}

bool VulkanContext::createSwapchain() {
    // ... реализация swapchain
    return true;
}

void VulkanContext::render() {
    // ... реализация рендеринга
}

void VulkanContext::shutdown() {
    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    }
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
    }
    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    }
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
    }
}
```

---

## CMake конфигурация

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.25)
project(ProjectV LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Платформенные defines для volk
if (WIN32)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX AND NOT APPLE)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XLIB_KHR)
elseif(APPLE)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_MACOS_MVK)
endif()

# Подмодули
add_subdirectory(external/volk)
add_subdirectory(external/SDL)
# ... другие библиотеки

# Исполняемый файл
add_executable(ProjectV
    src/main.cpp
    src/vulkan/vulkan_context.cpp
)

target_include_directories(ProjectV PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ProjectV PRIVATE
    volk
    SDL3::SDL3
)

target_compile_definitions(ProjectV PRIVATE
    VK_NO_PROTOTYPES
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(ProjectV PRIVATE DEBUG)
endif()
```

---

## Ключевые моменты интеграции

1. **VOLK_IMPLEMENTATION** в одном файле (`main.cpp`)
2. **volkLoadDevice** сразу после `vkCreateDevice`
3. **VK_NO_PROTOTYPES** в CMake для всех файлов
4. **Платформенные defines** через `VOLK_STATIC_DEFINES`
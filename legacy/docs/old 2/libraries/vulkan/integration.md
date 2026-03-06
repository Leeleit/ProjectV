# Интеграция Vulkan [🟡 Уровень 2]

**🟡 Уровень 2: Средний** — Настройка окружения, интеграция с библиотеками и порядок инициализации.

## Оглавление

- [1. Подготовка окружения](#1-подготовка-окружения)
  - [1.1. Установка Vulkan SDK](#11-установка-vulkan-sdk)
  - [1.2. CMake конфигурация](#12-cmake-конфигурация)
  - [1.3. Подключение подмодулей](#13-подключение-подмодулей)
- [2. Инициализация Vulkan](#2-инициализация-vulkan)
  - [2.1. Порядок инициализации](#21-порядок-инициализации)
  - [2.2. Инициализация volk](#22-инициализация-volk)
  - [2.3. Создание Instance](#23-создание-instance)
  - [2.4. Выбор Physical Device](#24-выбор-physical-device)
  - [2.5. Создание Device](#25-создание-device)
  - [2.6. Получение очередей](#26-получение-очередей)
- [3. Интеграция с SDL3](#3-интеграция-с-sdl3)
  - [3.1. Создание окна с Vulkan support](#31-создание-окна-с-vulkan-support)
  - [3.2. Получение расширений Surface](#32-получение-расширений-surface)
  - [3.3. Создание Surface](#33-создание-surface)
  - [3.4. Обработка событий SDL](#34-обработка-событий-sdl)
  - [3.5. Обработка изменения размера окна](#35-обработка-изменения-размера-окна)
- [4. Управление памятью с VMA](#4-управление-памятью-с-vma)
  - [4.1. Инициализация VMA](#41-инициализация-vma)
  - [4.2. Создание буферов и изображений](#42-создание-буферов-и-изображений)
  - [4.3. Аллокация памяти для GPU данных](#43-аллокация-памяти-для-gpu-данных)
  - [4.4. Оптимизация памяти GPU](#44-оптимизация-памяти-gpu)
- [5. Создание Swapchain](#5-создание-swapchain)
  - [5.1. Выбор формата и present mode](#51-выбор-формата-и-present-mode)
  - [5.2. Создание Swapchain](#52-создание-swapchain)
  - [5.3. Получение изображений Swapchain](#53-получение-изображений-swapchain)
  - [5.4. Создание Image Views](#54-создание-image-views)
- [6. Создание Render Pass и Framebuffers](#6-создание-render-pass-и-framebuffers)
  - [6.1. Создание Render Pass](#61-создание-render-pass)
  - [6.2. Framebuffers для Swapchain](#62-framebuffers-для-swapchain)
- [7. Обработка ошибок и отладка](#7-обработка-ошибок-и-отладка)
  - [7.1. Validation Layers](#71-validation-layers)
  - [7.2. Debug Utils](#72-debug-utils)
  - [7.3. Обработка VK_ERROR_*](#73-обработка-vk_error)
- [8. Очистка ресурсов](#8-очистка-ресурсов)
  - [8.1. Правильный порядок уничтожения](#81-правильный-порядок-уничтожения)
  - [8.2. Очистка при пересоздании Swapchain](#82-очистка-при-пересоздании-swapchain)
- [9. Пример кода: полная инициализация](#9-пример-кода-полная-инициализация)
  - [9.1. Структура VulkanContext](#91-структура-vulkancontext)
  - [9.2. Функция инициализации](#92-функция-инициализации)
  - [9.3. Основной цикл рендеринга](#93-основной-цикл-рендеринга)
  - [9.4. Очистка](#94-очистка)
- [10. Заключение](#10-заключение)

---

## 1. Подготовка окружения

### 1.1. Установка Vulkan SDK

1. Скачайте и установите Vulkan SDK с [официального сайта](https://vulkan.lunarg.com/)
2. Убедитесь, что переменная окружения `VULKAN_SDK` установлена
3. Проверьте установку: `vulkaninfo` должен выводить информацию о GPU

### 1.2. CMake конфигурация

```cmake
cmake_minimum_required(VERSION 3.25)
project(YourApp)

# Поиск Vulkan SDK
find_package(Vulkan REQUIRED)

# Добавление подмодулей
add_subdirectory(external/volk)
add_subdirectory(external/VMA)
add_subdirectory(external/SDL)

# Целевое приложение
add_executable(YourApp src/main.cpp)

# Включение директорий
target_include_directories(YourApp PRIVATE
        ${Vulkan_INCLUDE_DIRS}
        external/volk
        external/VMA/include
        external/SDL/include
)

# Связывание библиотек
target_link_libraries(YourApp PRIVATE
        Vulkan::Vulkan
        volk
        vma
        SDL3::SDL3
)

# Настройки компиляции
target_compile_features(YourApp PRIVATE cxx_std_26)
set_target_properties(YourApp PROPERTIES
        CXX_EXTENSIONS OFF
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
```

### 1.3. Подключение подмодулей

```bash
git submodule update --init --recursive
```

Убедитесь, что все подмодули обновлены:

- `external/volk` - загрузка Vulkan функций
- `external/VMA` - управление памятью GPU
- `external/SDL` - окна и ввод

---

## 2. Инициализация Vulkan

### 2.1. Порядок инициализации

1. Инициализация volk (`volkInitialize`)
2. Создание Instance (`vkCreateInstance`)
3. Выбор Physical Device (`vkEnumeratePhysicalDevices`)
4. Создание Device (`vkCreateDevice`)
5. Получение очередей (`vkGetDeviceQueue`)

### 2.2. Инициализация volk

```cpp
#define VK_NO_PROTOTYPES
#include "volk.h"

bool init_vulkan() {
    // Инициализация volk - загрузка Vulkan loader
    VkResult result = volkInitialize();
    if (result != VK_SUCCESS) {
        SDL_Log("Failed to initialize volk: %d", result);
        return false;
    }

    // Проверка версии Vulkan
    uint32_t version = 0;
    vkEnumerateInstanceVersion(&version);
    SDL_Log("Vulkan API version: %d.%d.%d",
        VK_VERSION_MAJOR(version),
        VK_VERSION_MINOR(version),
        VK_VERSION_PATCH(version));

    return true;
}
```

### 2.3. Создание Instance

```cpp
VkInstance create_instance() {
    // Информация о приложении
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "YourApp";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    app_info.pEngineName = "YourApp Engine";
    app_info.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    app_info.apiVersion = VK_API_VERSION_1_4;

    // Получение расширений из SDL
    uint32_t extension_count = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
    if (!extensions) {
        SDL_Log("Failed to get SDL Vulkan extensions: %s", SDL_GetError());
        return VK_NULL_HANDLE;
    }

    // Добавление дополнительных расширений
    std::vector<const char*> enabled_extensions;
    for (uint32_t i = 0; i < extension_count; ++i) {
        enabled_extensions.push_back(extensions[i]);
    }

    // Добавляем debug utils для отладки
    enabled_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // Включение validation layers в debug сборке
    std::vector<const char*> enabled_layers;
#ifdef _DEBUG
    enabled_layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    // Создание Instance
    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
    create_info.ppEnabledExtensionNames = enabled_extensions.data();
    create_info.enabledLayerCount = static_cast<uint32_t>(enabled_layers.size());
    create_info.ppEnabledLayerNames = enabled_layers.data();

    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&create_info, nullptr, &instance);
    if (result != VK_SUCCESS) {
        SDL_Log("Failed to create Vulkan instance: %d", result);
        return VK_NULL_HANDLE;
    }

    // Загрузка instance функций в volk
    volkLoadInstance(instance);

    return instance;
}
```

### 2.4. Выбор Physical Device

```cpp
VkPhysicalDevice select_physical_device(VkInstance instance, VkSurfaceKHR surface) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

    if (device_count == 0) {
        SDL_Log("No Vulkan-capable devices found");
        return VK_NULL_HANDLE;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

    // Выбор устройства с поддержкой графики и present
    for (VkPhysicalDevice device : devices) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);

        // Проверка поддержки очередей graphics и present
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

        bool has_graphics = false;
        bool has_present = false;

        for (uint32_t i = 0; i < queue_family_count; ++i) {
            if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                has_graphics = true;
            }

            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
            if (present_support) {
                has_present = true;
            }
        }

        // Предпочтение дискретным GPU
        if (has_graphics && has_present) {
            if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                SDL_Log("Selected discrete GPU: %s", properties.deviceName);
                return device;
            }
        }
    }

    // Если дискретного GPU нет, вернуть первое подходящее устройство
    for (VkPhysicalDevice device : devices) {
        // Проверка поддержки...
        return device;
    }

    return VK_NULL_HANDLE;
}
```

### 2.5. Создание Device

```cpp
VkDevice create_device(VkPhysicalDevice physical_device, uint32_t& graphics_queue_family,
                       uint32_t& present_queue_family, VkSurfaceKHR surface) {
    // Поиск queue families
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_queue_family = i;
        }

        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_support);
        if (present_support) {
            present_queue_family = i;
        }
    }

    // Создание очередей
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    float queue_priority = 1.0f;

    VkDeviceQueueCreateInfo graphics_queue_info = {};
    graphics_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphics_queue_info.queueFamilyIndex = graphics_queue_family;
    graphics_queue_info.queueCount = 1;
    graphics_queue_info.pQueuePriorities = &queue_priority;
    queue_create_infos.push_back(graphics_queue_info);

    // Если graphics и present очереди разные, добавляем present очередь
    if (graphics_queue_family != present_queue_family) {
        VkDeviceQueueCreateInfo present_queue_info = {};
        present_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        present_queue_info.queueFamilyIndex = present_queue_family;
        present_queue_info.queueCount = 1;
        present_queue_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(present_queue_info);
    }

    // Включение необходимых расширений device
    std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
    };

    // Проверка поддержки расширений
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, available_extensions.data());

    std::vector<const char*> enabled_extensions;
    for (const auto& requested : device_extensions) {
        bool found = false;
        for (const auto& available : available_extensions) {
            if (strcmp(requested, available.extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (found) {
            enabled_extensions.push_back(requested);
        } else {
            SDL_Log("Warning: Device extension %s not supported", requested);
        }
    }

    // Включение features
    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    VkPhysicalDeviceDescriptorIndexingFeatures indexing_features = {};
    indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    indexing_features.runtimeDescriptorArray = VK_TRUE;
    indexing_features.descriptorBindingPartiallyBound = VK_TRUE;
    indexing_features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    features2.pNext = &indexing_features;

    VkPhysicalDeviceSynchronization2Features sync2_features = {};
    sync2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    sync2_features.synchronization2 = VK_TRUE;
    indexing_features.pNext = &sync2_features;

    // Создание device
    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pNext = &features2;
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
    create_info.ppEnabledExtensionNames = enabled_extensions.data();

    VkDevice device = VK_NULL_HANDLE;
    VkResult result = vkCreateDevice(physical_device, &create_info, nullptr, &device);
    if (result != VK_SUCCESS) {
        SDL_Log("Failed to create device: %d", result);
        return VK_NULL_HANDLE;
    }

    // Загрузка device функций в volk
    volkLoadDevice(device);

    return device;
}
```

### 2.6. Получение очередей

```cpp
void get_queues(VkDevice device, uint32_t graphics_queue_family, uint32_t present_queue_family,
                VkQueue& graphics_queue, VkQueue& present_queue) {
    vkGetDeviceQueue(device, graphics_queue_family, 0, &graphics_queue);

    if (graphics_queue_family == present_queue_family) {
        present_queue = graphics_queue;
    } else {
        vkGetDeviceQueue(device, present_queue_family, 0, &present_queue);
    }
}
```

---

## 3. Интеграция с SDL3

### 3.1. Создание окна с Vulkan support

```cpp
SDL_Window* create_window(const char* title, int width, int height) {
    // Инициализация SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return nullptr;
    }

    // Создание окна с Vulkan support
    SDL_Window* window = SDL_CreateWindow(title, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        return nullptr;
    }

    return window;
}
```

### 3.2. Получение расширений Surface

```cpp
std::vector<const char*> get_surface_extensions() {
    uint32_t count = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&count);
    if (!extensions) {
        SDL_Log("Failed to get surface extensions: %s", SDL_GetError());
        return {};
    }

    std::vector<const char*> result;
    for (uint32_t i = 0; i < count; ++i) {
        result.push_back(extensions[i]);
    }

    return result;
}
```

### 3.3. Создание Surface

```cpp
VkSurfaceKHR create_surface(SDL_Window* window, VkInstance instance) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        SDL_Log("Failed to create Vulkan surface: %s", SDL_GetError());
        return VK_NULL_HANDLE;
    }
    return surface;
}
```

### 3.4. Обработка событий SDL

```cpp
bool handle_sdl_events(SDL_Window* window, bool& should_close, bool& resize_pending) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                should_close = true;
                break;

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                if (event.window.windowID == SDL_GetWindowID(window)) {
                    should_close = true;
                }
                break;

            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                if (event.window.windowID == SDL_GetWindowID(window)) {
                    resize_pending = true;
                }
                break;

            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE) {
                    should_close = true;
                }
                break;
        }
    }

    return !should_close;
}
```

### 3.5. Обработка изменения размера окна

```cpp
void handle_resize(VkDevice device, VkSwapchainKHR& swapchain,
                   std::vector<VkImageView>& swapchain_image_views,
                   std::vector<VkFramebuffer>& framebuffers,
                   VkRenderPass render_pass, SDL_Window* window) {
    // Ожидание завершения всех операций GPU
    vkDeviceWaitIdle(device);

    // Уничтожение старых ресурсов
    for (auto& framebuffer : framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    framebuffers.clear();

    for (auto& image_view : swapchain_image_views) {
        vkDestroyImageView(device, image_view, nullptr);
    }
    swapchain_image_views.clear();

    // Получение нового размера окна
    int width, height;
    SDL_GetWindowSizeInPixels(window, &width, &height);

    // Пересоздание swapchain с новым размером
    // ... (код пересоздания swapchain)
}
```

---

## 4. Управление памятью с VMA

### 4.1. Инициализация VMA

```cpp
#include "vk_mem_alloc.h"

VmaAllocator create_vma_allocator(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device) {
    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.physicalDevice = physical_device;
    allocator_info.device = device;
    allocator_info.instance = instance;
    allocator_info.vulkanApiVersion = VK_API_VERSION_1_4;

    VmaAllocator allocator = VK_NULL_HANDLE;
    VkResult result = vmaCreateAllocator(&allocator_info, &allocator);
    if (result != VK_SUCCESS) {
        SDL_Log("Failed to create VMA allocator: %d", result);
        return VK_NULL_HANDLE;
    }

    return allocator;
}
```

### 4.2. Создание буферов и изображений

```cpp
struct Buffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocation_info = {};
};

Buffer create_buffer(VmaAllocator allocator, VkDeviceSize size,
                     VkBufferUsageFlags usage, VmaMemoryUsage memory_usage) {
    Buffer result;

    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = memory_usage;
    alloc_info.flags = 0;

    VkResult result_code = vmaCreateBuffer(allocator, &buffer_info, &alloc_info,
                                          &result.buffer, &result.allocation,
                                          &result.allocation_info);
    if (result_code != VK_SUCCESS) {
        SDL_Log("Failed to create buffer: %d", result_code);
        return {};
    }

    return result;
}

struct Image {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocation_info = {};
};

Image create_image(VmaAllocator allocator, uint32_t width, uint32_t height,
                   VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage memory_usage) {
    Image result;

    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = memory_usage;
    alloc_info.flags = 0;

    VkResult result_code = vmaCreateImage(allocator, &image_info, &alloc_info,
                                         &result.image, &result.allocation,
                                         &result.allocation_info);
    if (result_code != VK_SUCCESS) {
        SDL_Log("Failed to create image: %d", result_code);
        return {};
    }

    return result;
}
```

### 4.3. Аллокация памяти для GPU данных

```cpp
Buffer create_storage_buffer(VmaAllocator allocator, VkDeviceSize size) {
    // Создание буфера для хранения данных GPU (vertex, index, uniform, storage)
    Buffer buffer = create_buffer(allocator, size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    return buffer;
}

Buffer create_indirect_buffer(VmaAllocator allocator, uint32_t max_draw_commands) {
    // Создание буфера для indirect draw команд
    VkDeviceSize buffer_size = max_draw_commands * sizeof(VkDrawIndexedIndirectCommand);

    Buffer buffer = create_buffer(allocator, buffer_size,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    return buffer;
}
```

### 4.4. Оптимизация памяти GPU

```cpp
// Использование sparse memory для больших разреженных данных
Buffer create_sparse_buffer(VmaAllocator allocator, VkDeviceSize total_size,
                           VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
    Buffer result;

    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = total_size;
    buffer_info.usage = usage;
    buffer_info.flags = VK_BUFFER_CREATE_SPARSE_BINDING_BIT;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    alloc_info.flags = VMA_ALLOCATION_CREATE_SPARSE_BINDING_BIT;

    VkResult result_code = vmaCreateBuffer(allocator, &buffer_info, &alloc_info,
                                          &result.buffer, &result.allocation,
                                          &result.allocation_info);
    if (result_code != VK_SUCCESS) {
        SDL_Log("Failed to create sparse buffer: %d", result_code);
    }

    return result;
}

// Аллокация staging буфера для загрузки данных CPU → GPU
Buffer create_staging_buffer(VmaAllocator allocator, VkDeviceSize size) {
    return create_buffer(allocator, size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY);
}
```

---

## 5. Создание Swapchain

### 5.1. Выбор формата и present mode

```cpp
struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

SwapchainSupport query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapchainSupport support;

    // Capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &support.capabilities);

    // Formats
    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);
    if (format_count != 0) {
        support.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, support.formats.data());
    }

    // Present modes
    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);
    if (present_mode_count != 0) {
        support.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, support.present_modes.data());
    }

    return support;
}

VkSurfaceFormatKHR choose_swapchain_format(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return formats[0];
}

VkPresentModeKHR choose_swapchain_present_mode(const std::vector<VkPresentModeKHR>& present_modes) {
    // Предпочтение mailbox (triple buffering) для минимальной задержки
    for (const auto& mode : present_modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    // Fallback на FIFO (vsync)
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D choose_swapchain_extent(const VkSurfaceCapabilitiesKHR& capabilities, int width, int height) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    VkExtent2D extent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

    extent.width = std::clamp(extent.width,
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height);

    return extent;
}
```

### 5.2. Создание Swapchain

```cpp
VkSwapchainKHR create_swapchain(VkDevice device, VkPhysicalDevice physical_device,
                               VkSurfaceKHR surface, int width, int height,
                               VkSurfaceFormatKHR& chosen_format, VkExtent2D& extent) {
    SwapchainSupport support = query_swapchain_support(physical_device, surface);

    chosen_format = choose_swapchain_format(support.formats);
    VkPresentModeKHR present_mode = choose_swapchain_present_mode(support.present_modes);
    extent = choose_swapchain_extent(support.capabilities, width, height);

    // Количество изображений в swapchain
    uint32_t image_count = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 &&
        image_count > support.capabilities.maxImageCount) {
        image_count = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = chosen_format.format;
    create_info.imageColorSpace = chosen_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Настройка очередей
    uint32_t queue_family_indices[] = { graphics_queue_family, present_queue_family };
    if (graphics_queue_family != present_queue_family) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = nullptr;
    }

    create_info.preTransform = support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkResult result = vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain);
    if (result != VK_SUCCESS) {
        SDL_Log("Failed to create swapchain: %d", result);
        return VK_NULL_HANDLE;
    }

    return swapchain;
}
```

### 5.3. Получение изображений Swapchain

```cpp
std::vector<VkImage> get_swapchain_images(VkDevice device, VkSwapchainKHR swapchain) {
    uint32_t image_count = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);

    std::vector<VkImage> images(image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, images.data());

    return images;
}
```

### 5.4. Создание Image Views

```cpp
VkImageView create_image_view(VkDevice device, VkImage image, VkFormat format) {
    VkImageViewCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image = image;
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format = format;
    create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    VkImageView image_view = VK_NULL_HANDLE;
    VkResult result = vkCreateImageView(device, &create_info, nullptr, &image_view);
    if (result != VK_SUCCESS) {
        SDL_Log("Failed to create image view: %d", result);
        return VK_NULL_HANDLE;
    }

    return image_view;
}

std::vector<VkImageView> create_swapchain_image_views(VkDevice device,
                                                      const std::vector<VkImage>& images,
                                                      VkFormat format) {
    std::vector<VkImageView> image_views(images.size());

    for (size_t i = 0; i < images.size(); ++i) {
        image_views[i] = create_image_view(device, images[i], format);
        if (image_views[i] == VK_NULL_HANDLE) {
            // Очистка уже созданных image views
            for (size_t j = 0; j < i; ++j) {
                vkDestroyImageView(device, image_views[j], nullptr);
            }
            return {};
        }
    }

    return image_views;
}
```

---

## 6. Создание Render Pass и Framebuffers

### 6.1. Создание Render Pass

```cpp
VkRenderPass create_render_pass(VkDevice device, VkFormat swapchain_format) {
    // Описание color attachment
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = swapchain_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Reference к color attachment
    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Subpass
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    // Dependency для синхронизации с present
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // Создание render pass
    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkResult result = vkCreateRenderPass(device, &render_pass_info, nullptr, &render_pass);
    if (result != VK_SUCCESS) {
        SDL_Log("Failed to create render pass: %d", result);
        return VK_NULL_HANDLE;
    }

    return render_pass;
}
```

### 6.2. Framebuffers для Swapchain

```cpp
std::vector<VkFramebuffer> create_framebuffers(VkDevice device, VkRenderPass render_pass,
                                              const std::vector<VkImageView>& image_views,
                                              VkExtent2D extent) {
    std::vector<VkFramebuffer> framebuffers(image_views.size());

    for (size_t i = 0; i < image_views.size(); ++i) {
        VkFramebufferCreateInfo framebuffer_info = {};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = &image_views[i];
        framebuffer_info.width = extent.width;
        framebuffer_info.height = extent.height;
        framebuffer_info.layers = 1;

        VkResult result = vkCreateFramebuffer(device, &framebuffer_info, nullptr, &framebuffers[i]);
        if (result != VK_SUCCESS) {
            SDL_Log("Failed to create framebuffer %zu: %d", i, result);
            // Очистка уже созданных framebuffers
            for (size_t j = 0; j < i; ++j) {
                vkDestroyFramebuffer(device, framebuffers[j], nullptr);
            }
            return {};
        }
    }

    return framebuffers;
}
```

---

## 7. Обработка ошибок и отладка

### 7.1. Validation Layers

```cpp
VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* user_data) {

    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        SDL_Log("Vulkan Validation: %s", data->pMessage);
    }

    return VK_FALSE;
}

VkDebugUtilsMessengerEXT create_debug_messenger(VkInstance instance) {
    VkDebugUtilsMessengerCreateInfoEXT create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = debug_callback;
    create_info.pUserData = nullptr;

    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");
    if (func) {
        func(instance, &create_info, nullptr, &debug_messenger);
    }

    return debug_messenger;
}
```

### 7.2. Debug Utils

```cpp
void set_debug_object_name(VkDevice device, VkObjectType type, uint64_t handle, const char* name) {
    VkDebugUtilsObjectNameInfoEXT name_info = {};
    name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    name_info.objectType = type;
    name_info.objectHandle = handle;
    name_info.pObjectName = name;

    auto func = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(
        device, "vkSetDebugUtilsObjectNameEXT");
    if (func) {
        func(device, &name_info);
    }
}
```

### 7.3. Обработка VK_ERROR_*

```cpp
const char* vk_error_string(VkResult error) {
    switch (error) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        default: return "Unknown VkResult";
    }
}

void check_vk_result(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) {
        SDL_Log("Vulkan error in %s: %s (%d)", operation, vk_error_string(result), result);
        // В debug сборке можно добавить assert
#ifdef _DEBUG
        assert(result == VK_SUCCESS);
#endif
    }
}
```

---

## 8. Очистка ресурсов

### 8.1. Правильный порядок уничтожения

```cpp
void cleanup_vulkan(VkDevice device, VkSwapchainKHR swapchain,
                   const std::vector<VkFramebuffer>& framebuffers,
                   const std::vector<VkImageView>& image_views,
                   VkRenderPass render_pass, VkSurfaceKHR surface,
                   VkInstance instance, SDL_Window* window) {
    // Ожидание завершения всех операций GPU
    vkDeviceWaitIdle(device);

    // Уничтожение framebuffers
    for (auto& framebuffer : framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    // Уничтожение image views
    for (auto& image_view : image_views) {
        vkDestroyImageView(device, image_view, nullptr);
    }

    // Уничтожение swapchain
    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
    }

    // Уничтожение render pass
    if (render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, render_pass, nullptr);
    }

    // Уничтожение surface
    if (surface != VK_NULL_HANDLE) {
        SDL_Vulkan_DestroySurface(instance, surface, nullptr);
    }

    // Уничтожение device
    if (device != VK_NULL_HANDLE) {
        vkDestroyDevice(device, nullptr);
    }

    // Уничтожение instance
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
    }

    // Уничтожение окна SDL
    if (window != nullptr) {
        SDL_DestroyWindow(window);
    }

    SDL_Quit();
}
```

### 8.2. Очистка при пересоздании Swapchain

```cpp
void cleanup_swapchain_resources(VkDevice device, VkSwapchainKHR& swapchain,
                                std::vector<VkFramebuffer>& framebuffers,
                                std::vector<VkImageView>& image_views) {
    for (auto& framebuffer : framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    framebuffers.clear();

    for (auto& image_view : image_views) {
        vkDestroyImageView(device, image_view, nullptr);
    }
    image_views.clear();

    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
}
```

---

## 9. Пример кода: полная инициализация

### 9.1. Структура VulkanContext

```cpp
struct VulkanContext {
    // Core Vulkan
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue present_queue = VK_NULL_HANDLE;
    uint32_t graphics_queue_family = 0;
    uint32_t present_queue_family = 0;

    // Surface and Swapchain
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchain_format = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchain_extent = {};
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;
    std::vector<VkFramebuffer> framebuffers;

    // Render Pass
    VkRenderPass render_pass = VK_NULL_HANDLE;

    // Memory Management
    VmaAllocator allocator = VK_NULL_HANDLE;

    // Sync
    std::vector<FrameSync> frame_sync;

    // Debug
    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;

    // Window
    SDL_Window* window = nullptr;
    int window_width = 800;
    int window_height = 600;
};
```

### 9.2. Функция инициализации

```cpp
bool init_vulkan_context(VulkanContext& ctx, const char* window_title) {
    // 1. Инициализация SDL и создание окна
    ctx.window = create_window(window_title, ctx.window_width, ctx.window_height);
    if (!ctx.window) return false;

    // 2. Инициализация volk
    if (!init_vulkan()) return false;

    // 3. Создание Instance
    ctx.instance = create_instance();
    if (!ctx.instance) return false;

    // 4. Создание debug messenger
#ifdef _DEBUG
    ctx.debug_messenger = create_debug_messenger(ctx.instance);
#endif

    // 5. Создание Surface
    ctx.surface = create_surface(ctx.window, ctx.instance);
    if (!ctx.surface) return false;

    // 6. Выбор Physical Device
    ctx.physical_device = select_physical_device(ctx.instance, ctx.surface);
    if (!ctx.physical_device) return false;

    // 7. Создание Device
    ctx.device = create_device(ctx.physical_device, ctx.graphics_queue_family,
                              ctx.present_queue_family, ctx.surface);
    if (!ctx.device) return false;

    // 8. Получение очередей
    get_queues(ctx.device, ctx.graphics_queue_family, ctx.present_queue_family,
              ctx.graphics_queue, ctx.present_queue);

    // 9. Инициализация VMA
    ctx.allocator = create_vma_allocator(ctx.instance, ctx.physical_device, ctx.device);
    if (!ctx.allocator) return false;

    // 10. Создание Swapchain
    ctx.swapchain = create_swapchain(ctx.device, ctx.physical_device, ctx.surface,
                                    ctx.window_width, ctx.window_height,
                                    ctx.swapchain_format, ctx.swapchain_extent);
    if (!ctx.swapchain) return false;

    // 11. Получение изображений Swapchain
    ctx.swapchain_images = get_swapchain_images(ctx.device, ctx.swapchain);

    // 12. Создание Image Views
    ctx.swapchain_image_views = create_swapchain_image_views(ctx.device, ctx.swapchain_images,
                                                            ctx.swapchain_format);
    if (ctx.swapchain_image_views.empty()) return false;

    // 13. Создание Render Pass
    ctx.render_pass = create_render_pass(ctx.device, ctx.swapchain_format);
    if (!ctx.render_pass) return false;

    // 14. Создание Framebuffers
    ctx.framebuffers = create_framebuffers(ctx.device, ctx.render_pass,
                                          ctx.swapchain_image_views, ctx.swapchain_extent);
    if (ctx.framebuffers.empty()) return false;

    // 15. Создание синхронизации
    ctx.frame_sync.resize(ctx.swapchain_images.size());
    for (auto& sync : ctx.frame_sync) {
        sync = create_frame_sync(ctx.device);
    }

    return true;
}
```

### 9.3. Основной цикл рендеринга

```cpp
void render_frame(VulkanContext& ctx, uint32_t frame_index) {
    FrameSync& sync = ctx.frame_sync[frame_index];

    // Ожидание завершения предыдущего кадра
    vkWaitForFences(ctx.device, 1, &sync.in_flight, VK_TRUE, UINT64_MAX);
    vkResetFences(ctx.device, 1, &sync.in_flight);

    // Получение следующего изображения swapchain
    uint32_t image_index = 0;
    VkResult result = vkAcquireNextImageKHR(ctx.device, ctx.swapchain, UINT64_MAX,
                                           sync.image_available, VK_NULL_HANDLE,
                                           &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // Требуется пересоздание swapchain
        recreate_swapchain(ctx);
        return;
    } else if (result != VK_SUCCESS) {
        SDL_Log("Failed to acquire swapchain image: %d", result);
        return;
    }

    // Запись команд рендеринга
    VkCommandBuffer cmd = record_rendering_commands(ctx, image_index);

    // Submit команд
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = {sync.image_available};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    VkSemaphore signal_semaphores[] = {sync.render_finished};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    vkQueueSubmit(ctx.graphics_queue, 1, &submit_info, sync.in_flight);

    // Present
    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &ctx.swapchain;
    present_info.pImageIndices = &image_index;

    vkQueuePresentKHR(ctx.present_queue, &present_info);
}
```

### 9.4. Очистка

```cpp
void cleanup_vulkan_context(VulkanContext& ctx) {
    // Очистка frame sync
    for (auto& sync : ctx.frame_sync) {
        if (sync.image_available != VK_NULL_HANDLE) {
            vkDestroySemaphore(ctx.device, sync.image_available, nullptr);
        }
        if (sync.render_finished != VK_NULL_HANDLE) {
            vkDestroySemaphore(ctx.device, sync.render_finished, nullptr);
        }
        if (sync.compute_finished != VK_NULL_HANDLE) {
            vkDestroySemaphore(ctx.device, sync.compute_finished, nullptr);
        }
        if (sync.in_flight != VK_NULL_HANDLE) {
            vkDestroyFence(ctx.device, sync.in_flight, nullptr);
        }
    }

    // Очистка остальных ресурсов
    cleanup_swapchain_resources(ctx.device, ctx.swapchain, ctx.framebuffers, ctx.swapchain_image_views);

    if (ctx.render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(ctx.device, ctx.render_pass, nullptr);
    }

    if (ctx.allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(ctx.allocator);
    }

    if (ctx.surface != VK_NULL_HANDLE) {
        SDL_Vulkan_DestroySurface(ctx.instance, ctx.surface, nullptr);
    }

    if (ctx.debug_messenger != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            ctx.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) {
            func(ctx.instance, ctx.debug_messenger, nullptr);
        }
    }

    if (ctx.device != VK_NULL_HANDLE) {
        vkDestroyDevice(ctx.device, nullptr);
    }

    if (ctx.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(ctx.instance, nullptr);
    }

    if (ctx.window != nullptr) {
        SDL_DestroyWindow(ctx.window);
    }

    SDL_Quit();
}
```

---

## 10. Заключение

Интеграция Vulkan требует тщательной настройки всех компонентов: от инициализации базовых объектов Vulkan до
сложной синхронизации compute и graphics очередей. Ключевые аспекты:

1. **Правильный порядок инициализации** - volk → instance → device → swapchain → render pass
2. **Эффективное управление памятью** - использование VMA для аллокации буферов и изображений
3. **Интеграция с SDL3** - создание окна и поверхности
4. **Создание конвейера рендеринга** - swapchain, render pass, framebuffers
5. **Обработка ошибок и отладка** - validation layers и debug utilities

**Специфичные техники рендеринга** (GPU Driven Rendering, Compute Shaders) для воксельных движков
рассмотрены в отдельном документе:

🔗 **[Специализированные паттерны интеграции для Vulkan](projectv-integration.md)** — специфичные техники рендеринга для
воксельных движков

---

**Следующие шаги:**

- Изучите [Производительность Vulkan](performance.md) для оптимизации рендеринга
- Ознакомьтесь с [Сценариями использования](use-cases.md) для конкретных примеров кода
- Прочитайте [Решение проблем](troubleshooting.md) для отладки распространённых ошибок
- Изучите [Специализированные паттерны интеграции для Vulkan](projectv-integration.md) для специфичных техник рендеринга
  воксельных движков

← **[Назад к основной документации Vulkan](README.md)**

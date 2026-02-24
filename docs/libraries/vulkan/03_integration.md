# Vulkan Integration

Настройка окружения, CMake, инициализация, surface, swapchain.

---

## CMake конфигурация

### Минимальный CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.25)
project(VulkanApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Vulkan SDK
find_package(Vulkan REQUIRED)

# volk (подмодуль или FetchContent)
add_subdirectory(external/volk)

# VMA (Vulkan Memory Allocator)
add_subdirectory(external/VMA)

# Исполняемый файл
add_executable(app src/main.cpp)

target_link_libraries(app PRIVATE
    Vulkan::Vulkan
    volk::volk
    VMA::VMA
)

# Опционально: SDL3 для window/surface
add_subdirectory(external/SDL)
target_link_libraries(app PRIVATE SDL3::SDL3)
```

### Определение версии Vulkan SDK

```cmake
# Проверка версии
if(Vulkan_FOUND)
    message(STATUS "Vulkan SDK: ${Vulkan_VERSION}")
    message(STATUS "Vulkan Headers: ${Vulkan_INCLUDE_DIRS}")
endif()

# Требование минимальной версии
find_package(Vulkan 1.3.250 REQUIRED)
```

---

## volk: Загрузка Vulkan функций

### Зачем нужен volk

volk — это заголовочный загрузчик Vulkan функций, позволяющий:

- Избежать линковки с `vulkan-1.dll` / `libvulkan.so`
- Поддерживать расширения без ручного `vkGetInstanceProcAddr`
- Работать с несколькими instances/devices

### Инициализация

```cpp
#include <volk.h>

int main() {
    // 1. Инициализация volk
    VkResult result = volkInitialize();
    if (result != VK_SUCCESS) {
        // Vulkan loader не найден
        return -1;
    }

    // 2. Получение версии
    uint32_t version = volkGetInstanceVersion();
    uint32_t major = VK_VERSION_MAJOR(version);
    uint32_t minor = VK_VERSION_MINOR(version);
    uint32_t patch = VK_VERSION_PATCH(version);
    printf("Vulkan %d.%d.%d\n", major, minor, patch);

    // 3. Создание instance
    VkInstance instance;
    // ... vkCreateInstance ...

    // 4. Загрузка instance-level функций
    volkLoadInstance(instance);

    // 5. Создание device
    VkDevice device;
    // ... vkCreateDevice ...

    // 6. Загрузка device-level функций
    volkLoadDevice(device);

    // Теперь доступны все vk* функции
}
```

### Multi-device сценарий

```cpp
// Для нескольких устройств используйте таблицы
VolkDeviceTable table1, table2;

volkLoadDeviceTable(&table1, device1);
volkLoadDeviceTable(&table2, device2);

// Вызовы через таблицу
table1.vkCmdDraw(cmdBuffer1, ...);
table2.vkCmdDraw(cmdBuffer2, ...);
```

### Интеграция с SDL3

```cpp
// SDL3 может загрузить Vulkan loader
if (!SDL_Vulkan_LoadLibrary(nullptr)) {
    fprintf(stderr, "Failed to load Vulkan: %s\n", SDL_GetError());
    return -1;
}

// Получение vkGetInstanceProcAddr
PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
    (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();

// Инициализация volk через SDL
volkInitializeCustom(vkGetInstanceProcAddr);
```

---

## Инициализация Instance

### Базовая структура

```cpp
VkInstance createInstance() {
    // Application info
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "My Vulkan App";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    // Extensions
    std::vector<const char*> extensions = getRequiredExtensions();

    // Instance create info
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // Layers (debug)
    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
    #ifdef DEBUG
    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = layers;
    #endif

    VkInstance instance;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create instance");
    }

    return instance;
}
```

### Получение расширений

```cpp
std::vector<const char*> getRequiredExtensions() {
    uint32_t sdlExtensionCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);

    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + sdlExtensionCount);

    #ifdef DEBUG
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    #endif

    return extensions;
}
```

### Цепочка pNext

```cpp
// Для расширений, требующих pNext
VkValidationFeaturesEXT validationFeatures = {};
validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;

VkValidationFeatureEnableEXT enables[] = {
    VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
    VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
};
validationFeatures.enabledValidationFeatureCount = 2;
validationFeatures.pEnabledValidationFeatures = enables;

VkInstanceCreateInfo createInfo = {};
createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
createInfo.pNext = &validationFeatures;  // Цепляем к pNext
```

---

## Debug Messenger

### Создание

```cpp
VkDebugUtilsMessengerEXT debugMessenger;

VkDebugUtilsMessengerCreateInfoEXT debugInfo = {};
debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
debugInfo.messageSeverity =
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
debugInfo.messageType =
    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
debugInfo.pfnUserCallback = debugCallback;
debugInfo.pUserData = nullptr;

vkCreateDebugUtilsMessengerEXT(instance, &debugInfo, nullptr, &debugMessenger);
```

### Callback

```cpp
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    const char* severity = "INFO";
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        severity = "ERROR";
    else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        severity = "WARNING";

    fprintf(stderr, "[Vulkan %s] %s\n", severity, pCallbackData->pMessage);
    return VK_FALSE;  // Не прерывать вызов
}
```

### Уничтожение

```cpp
void cleanup() {
    vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    vkDestroyInstance(instance, nullptr);
}
```

---

## Surface через SDL3

### Создание surface

```cpp
VkSurfaceKHR surface;

// SDL3 signature
if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
    fprintf(stderr, "Failed to create surface: %s\n", SDL_GetError());
    return VK_NULL_HANDLE;
}

return surface;
```

### Проверка поддержки present

```cpp
bool checkPresentSupport(VkPhysicalDevice device, uint32_t queueFamilyIndex) {
    return SDL_Vulkan_GetPresentationSupport(
        instance,
        device,
        queueFamilyIndex
    );
}
```

### Уничтожение surface

```cpp
void cleanup() {
    SDL_Vulkan_DestroySurface(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    SDL_DestroyWindow(window);
    SDL_Vulkan_UnloadLibrary();  // Если загружали через SDL
}
```

---

## Swapchain

### Выбор формата

```cpp
VkSurfaceFormatKHR chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    for (const auto& format : availableFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return availableFormats[0];
}
```

### Выбор present mode

```cpp
VkPresentModeKHR choosePresentMode(
    const std::vector<VkPresentModeKHR>& availableModes)
{
    for (const auto& mode : availableModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;  // Triple buffering, low latency
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;  // VSync, guaranteed
}
```

### Выбор extent

```cpp
VkExtent2D chooseSwapExtent(
    const VkSurfaceCapabilitiesKHR& capabilities,
    uint32_t width, uint32_t height)
{
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    VkExtent2D extent = {width, height};
    extent.width = std::clamp(extent.width,
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height);
    return extent;
}
```

### Создание swapchain

```cpp
struct SwapchainInfo {
    VkSwapchainKHR swapchain;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    VkFormat format;
    VkExtent2D extent;
};

SwapchainInfo createSwapchain(VkDevice device, VkPhysicalDevice physDevice,
                               VkSurfaceKHR surface, uint32_t width, uint32_t height) {
    // Query capabilities
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice, surface, &caps);

    // Query formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surface, &formatCount, formats.data());

    // Query present modes
    uint32_t modeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surface, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surface, &modeCount, modes.data());

    // Choose settings
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(formats);
    VkPresentModeKHR presentMode = choosePresentMode(modes);
    VkExtent2D extent = chooseSwapExtent(caps, width, height);

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    // Create info
    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Queue family indices
    uint32_t queueIndices[] = {graphicsFamily, presentFamily};
    if (graphicsFamily != presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = caps.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    SwapchainInfo info;
    info.format = surfaceFormat.format;
    info.extent = extent;

    vkCreateSwapchainKHR(device, &createInfo, nullptr, &info.swapchain);

    // Get images
    vkGetSwapchainImagesKHR(device, info.swapchain, &imageCount, nullptr);
    info.images.resize(imageCount);
    vkGetSwapchainImagesKHR(device, info.swapchain, &imageCount, info.images.data());

    // Create image views
    info.imageViews.resize(imageCount);
    for (size_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = info.images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = info.format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        vkCreateImageView(device, &viewInfo, nullptr, &info.imageViews[i]);
    }

    return info;
}
```

### Recreate swapchain

```cpp
void recreateSwapchain() {
    // Handle minimization
    int width = 0, height = 0;
    SDL_GetWindowSize(window, &width, &height);
    while (width == 0 || height == 0) {
        SDL_GetWindowSize(window, &width, &height);
        SDL_WaitEvent(nullptr);
    }

    vkDeviceWaitIdle(device);

    cleanupSwapchain();
    swapchain = createSwapchain(device, physDevice, surface, width, height);
    createFramebuffers();
}
```

---

## VMA: Управление памятью

### Инициализация VMA

```cpp
#include <vk_mem_alloc.h>

VmaAllocator allocator;

VmaAllocatorCreateInfo allocInfo = {};
allocInfo.instance = instance;
allocInfo.physicalDevice = physDevice;
allocInfo.device = device;
allocInfo.vulkanApiVersion = VK_API_VERSION_1_3;

vmaCreateAllocator(&allocInfo, &allocator);
```

### Создание буфера

```cpp
struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

AllocatedBuffer createBuffer(VkDeviceSize size,
                              VkBufferUsageFlags usage,
                              VmaMemoryUsage memoryUsage)
{
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = memoryUsage;

    AllocatedBuffer result;
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
                    &result.buffer, &result.allocation, &result.info);

    return result;
}
```

### Создание изображения

```cpp
struct AllocatedImage {
    VkImage image;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

AllocatedImage createImage(uint32_t width, uint32_t height,
                            VkFormat format, VkImageUsageFlags usage)
{
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    AllocatedImage result;
    vmaCreateImage(allocator, &imageInfo, &allocInfo,
                   &result.image, &result.allocation, &result.info);

    return result;
}
```

### Mapping памяти

```cpp
// Для HOST_VISIBLE памяти
void* mapMemory(VmaAllocation allocation) {
    void* data;
    vmaMapMemory(allocator, allocation, &data);
    return data;
}

void unmapMemory(VmaAllocation allocation) {
    vmaUnmapMemory(allocator, allocation);
}

// Краткая форма с callback
template<typename Func>
void withMappedMemory(VmaAllocation allocation, Func&& func) {
    void* data;
    vmaMapMemory(allocator, allocation, &data);
    func(data);
    vmaUnmapMemory(allocator, allocation);
}
```

### Уничтожение ресурсов

```cpp
void destroyBuffer(const AllocatedBuffer& buffer) {
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

void destroyImage(const AllocatedImage& image) {
    vmaDestroyImage(allocator, image.image, image.allocation);
}

void cleanup() {
    vmaDestroyAllocator(allocator);
}
```

### Статистика памяти

```cpp
void printMemoryStats() {
    VmaStats stats;
    vmaCalculateStats(allocator, &stats);

    printf("Total allocated: %zu bytes\n", stats.total.allocationBytes);
    printf("Block count: %u\n", stats.total.blockCount);
    printf("Allocation count: %u\n", stats.total.allocationCount);
}
```

---

## Ключевые моменты

1. **volk** — загрузка функций, вызывается после создания instance/device
2. **SDL3** — предоставляет surface и расширения instance
3. **Swapchain** — требует пересоздания при resize
4. **VMA** — упрощает управление памятью GPU
5. **Validation layers** — обязательно в debug-сборках

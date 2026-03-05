# Vulkan в ProjectV: Интеграция

> **Для понимания:** Инициализация Vulkan — это как запуск ядерного реактора. Нужно проверить сотни параметров,
> подключить системы безопасности, и только потом дать команду "старт". В ProjectV мы делаем это один раз, но делаем
> правильно.

## CMake конфигурация

### Git Submodules с интеграцией ProjectV

ProjectV использует подмодули с полной интеграцией в архитектуру движка:

```
ProjectV/
├── external/
│   ├── volk/                    # Мета-лоадер (Vulkan 1.4)
│   ├── VulkanMemoryAllocator/   # Аллокатор памяти GPU (интегрирован с MemoryManager)
│   └── SDL/                     # Оконная система (с кастомными аллокаторами)
├── src/
│   ├── core/                    # Ядро движка
│   │   ├── memory/              # DOD-аллокаторы
│   │   ├── logging/             # Lock-free логгер
│   │   └── profiling/           # Tracy hooks
│   └── graphics/
│       └── vulkan/              # Vulkan интеграция
└── CMakeLists.txt
```

```cmake
# CMakeLists.txt ProjectV
cmake_minimum_required(VERSION 3.25)
project(ProjectV LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Обязательные макросы для Vulkan
add_compile_definitions(VK_NO_PROTOTYPES)

# --- Внешние зависимости (только подмодули) ---
# Отключаем всё лишнее в подмодулях
set(VOLK_BUILD_STATIC ON CACHE BOOL "Build volk as static library" FORCE)
set(VOLK_BUILD_TESTS OFF CACHE BOOL "Disable volk tests" FORCE)
set(VOLK_BUILD_EXAMPLES OFF CACHE BOOL "Disable volk examples" FORCE)

set(VMA_BUILD_SAMPLE OFF CACHE BOOL "Disable VMA samples" FORCE)
set(VMA_BUILD_TESTS OFF CACHE BOOL "Disable VMA tests" FORCE)
set(VMA_STATIC_VULKAN_FUNCTIONS OFF CACHE BOOL "Use dynamic Vulkan functions" FORCE)

set(SDL_STATIC ON CACHE BOOL "Build SDL as static library" FORCE)
set(SDL_SHARED OFF CACHE BOOL "Disable SDL shared library" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Disable SDL tests" FORCE)

# Подключение подмодулей
add_subdirectory(external/volk)
add_subdirectory(external/VulkanMemoryAllocator)
add_subdirectory(external/SDL)

# --- Ядро ProjectV ---
# MemoryManager модуль
add_library(projectv_core_memory STATIC
    src/core/memory/page_allocator.cpp
    src/core/memory/global_manager.cpp
    src/core/memory/arena_allocator.cpp
    src/core/memory/pool_allocator.cpp
    src/core/memory/linear_allocator.cpp
)

target_compile_features(projectv_core_memory PUBLIC cxx_std_26)
target_include_directories(projectv_core_memory PUBLIC src/core)

# Logging модуль
add_library(projectv_core_logging STATIC
    src/core/logging/logger.cpp
    src/core/logging/ring_buffer.cpp
)

target_compile_features(projectv_core_logging PUBLIC cxx_std_26)
target_include_directories(projectv_core_logging PUBLIC src/core)
target_link_libraries(projectv_core_logging PRIVATE projectv_core_memory)

# Profiling модуль (только для Profile конфигурации)
if(PROJECTV_ENABLE_PROFILING)
    add_library(projectv_core_profiling STATIC
        src/core/profiling/tracy_hooks.cpp
        src/core/profiling/vulkan_profiler.cpp
    )
    target_compile_features(projectv_core_profiling PUBLIC cxx_std_26)
    target_include_directories(projectv_core_profiling PUBLIC src/core)
    target_link_libraries(projectv_core_profiling PRIVATE projectv_core_memory projectv_core_logging)
    target_compile_definitions(projectv_core_profiling PRIVATE TRACY_ENABLE)
endif()

# --- Vulkan модуль движка ---
add_library(projectv_graphics_vulkan STATIC
    src/graphics/vulkan/context.cpp
    src/graphics/vulkan/device.cpp
    src/graphics/vulkan/memory_integration.cpp
)

target_compile_features(projectv_graphics_vulkan PUBLIC cxx_std_26)
target_include_directories(projectv_graphics_vulkan PUBLIC src/graphics)
target_link_libraries(projectv_graphics_vulkan PRIVATE
    projectv_core_memory
    projectv_core_logging
    volk::volk
    GPUOpen::VulkanMemoryAllocator
)

if(PROJECTV_ENABLE_PROFILING)
    target_link_libraries(projectv_graphics_vulkan PRIVATE projectv_core_profiling)
endif()

# --- Исполняемый файл ProjectV ---
add_executable(ProjectV
    src/main.cpp
)

target_link_libraries(ProjectV PRIVATE
    projectv_graphics_vulkan
    SDL3::SDL3
)

# Для отладки
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(ProjectV PRIVATE DEBUG)
    target_compile_definitions(ProjectV PRIVATE VK_VALIDATION)
endif()

# Profile конфигурация
if(PROJECTV_ENABLE_PROFILING)
    target_compile_definitions(ProjectV PRIVATE TRACY_ENABLE)
    target_link_libraries(ProjectV PRIVATE Tracy::TracyClient)
endif()
```

## Инициализация Modern Vulkan 1.4

### 1. C++26 Module с Global Module Fragment

```cpp
// src/graphics/vulkan/context.cppm - Primary Module Interface
module;
// Global Module Fragment: изолируем заголовки Vulkan и SDL
#define VK_NO_PROTOTYPES
#include <volk.h>
#include <vk_mem_alloc.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

export module projectv.graphics.vulkan.context;

import std;
import projectv.core.memory;
import projectv.core.log;
import projectv.core.profiling;

namespace projectv::graphics::vulkan {

// --- Обработка ошибок через std::expected ---
enum class VulkanError {
    VolkInitFailed,
    InstanceCreationFailed,
    DeviceCreationFailed,
    VmaInitFailed,
    SurfaceCreationFailed,
    SwapchainCreationFailed,
    NoSuitableDevice,
    MemoryAllocationFailed,
    DebugMessengerFailed
};

template<typename T>
using VulkanResult = std::expected<T, VulkanError>;

inline std::string to_string(VulkanError error) {
    switch (error) {
        case VulkanError::VolkInitFailed: return "volk initialization failed";
        case VulkanError::InstanceCreationFailed: return "Vulkan instance creation failed";
        case VulkanError::DeviceCreationFailed: return "Vulkan device creation failed";
        case VulkanError::VmaInitFailed: return "VMA initialization failed";
        case VulkanError::SurfaceCreationFailed: return "Surface creation failed";
        case VulkanError::SwapchainCreationFailed: return "Swapchain creation failed";
        case VulkanError::NoSuitableDevice: return "No suitable Vulkan device found";
        case VulkanError::MemoryAllocationFailed: return "Memory allocation failed (ProjectV MemoryManager)";
        case VulkanError::DebugMessengerFailed: return "Debug messenger creation failed";
        default: return "Unknown Vulkan error";
    }
}

// --- MemoryManager интеграция через VkAllocationCallbacks ---
class VulkanAllocator {
public:
    static VkAllocationCallbacks getAllocationCallbacks() noexcept {
        return VkAllocationCallbacks{
            .pUserData = nullptr,
            .pfnAllocation = &vulkanAllocation,
            .pfnReallocation = &vulkanReallocation,
            .pfnFree = &vulkanFree,
            .pfnInternalAllocation = nullptr,
            .pfnInternalFree = nullptr,
        };
    }

private:
    static void* VKAPI_CALL vulkanAllocation(
        void* pUserData, size_t size, size_t alignment,
        VkSystemAllocationScope allocationScope) {

        ZoneScopedN("VulkanAllocation");

        // Используем ProjectV MemoryManager в зависимости от scope
        auto& memoryManager = projectv::core::memory::getGlobalMemoryManager();

        switch (allocationScope) {
            case VK_SYSTEM_ALLOCATION_SCOPE_COMMAND:
                // Временные данные команды - LinearAllocator из thread arena
                return memoryManager.getThreadArena().allocate(size, alignment);

            case VK_SYSTEM_ALLOCATION_SCOPE_OBJECT:
            case VK_SYSTEM_ALLOCATION_SCOPE_CACHE:
                // Объекты Vulkan - PoolAllocator
                return memoryManager.createPool(size, 100).allocate();

            case VK_SYSTEM_ALLOCATION_SCOPE_DEVICE:
            case VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE:
                // Долгоживущие объекты - PageAllocator
                auto result = projectv::core::memory::PageAllocator::allocatePage(size, alignment);
                if (result) {
                    return *result;
                }
                projectv::core::Log::error("Vulkan", "Page allocation failed: size={}, alignment={}", size, alignment);
                return nullptr;

            default:
                return nullptr;
        }
    }

    static void* VKAPI_CALL vulkanReallocation(
        void* pUserData, void* pOriginal, size_t size,
        size_t alignment, VkSystemAllocationScope allocationScope) {

        ZoneScopedN("VulkanReallocation");

        // В реальной реализации нужно копировать данные и отслеживать оригинальный аллокатор
        void* newPtr = vulkanAllocation(pUserData, size, alignment, allocationScope);
        if (newPtr && pOriginal) {
            // В реальном коде нужно знать оригинальный размер
            std::memcpy(newPtr, pOriginal, std::min(size, /* original size */));
            vulkanFree(pUserData, pOriginal);
        }
        return newPtr;
    }

    static void VKAPI_CALL vulkanFree(
        void* pUserData, void* pMemory) {

        if (!pMemory) return;

        // В реальной реализации нужно определить, из какого аллокатора выделена память
        // и вернуть её туда. Для простоты примера - игнорируем.
        // В production коде используется allocation tracking.
    }
};

// --- Debug Messenger с интеграцией в ProjectV Log ---
class VulkanDebugMessenger {
public:
    static VkDebugUtilsMessengerCreateInfoEXT getCreateInfo() noexcept {
        return VkDebugUtilsMessengerCreateInfoEXT{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = &debugCallback,
            .pUserData = nullptr
        };
    }

private:
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {

        // Преобразуем Vulkan severity в ProjectV Log level
        projectv::core::Log::Level level;
        switch (messageSeverity) {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                level = projectv::core::Log::Level::Info;
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                level = projectv::core::Log::Level::Warning;
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                level = projectv::core::Log::Level::Error;
                break;
            default:
                level = projectv::core::Log::Level::Info;
        }

        // Форматируем сообщение в UNIX-way стиле
        std::string messageTypeStr;
        if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
            messageTypeStr = "General";
        } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
            messageTypeStr = "Validation";
        } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
            messageTypeStr = "Performance";
        }

        // Логируем через ProjectV Log
        projectv::core::Log::log(level, "Vulkan",
            "[{}] {} (ID: {}, Name: {})",
            messageTypeStr,
            pCallbackData->pMessage,
            pCallbackData->messageIdNumber,
            pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "N/A");

        return VK_FALSE;  // Не прерываем выполнение
    }
};
```

### 2. Vulkan Context с интеграцией ProjectV

```cpp
// Продолжение модуля projectv.graphics.vulkan.context
// (в том же файле context.cppm после VulkanAllocator и VulkanDebugMessenger)

class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext() {
        ZoneScopedN("VulkanContextDestructor");
        shutdown();
    }

    // Non-copyable
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    // Movable
    VulkanContext(VulkanContext&& other) noexcept;
    VulkanContext& operator=(VulkanContext&& other) noexcept;

    // Инициализация с интеграцией ProjectV
    VulkanResult<void> initialize(SDL_Window* window);
    void shutdown();

    // Getters
    VkInstance instance() const noexcept { return instance_; }
    VkPhysicalDevice physical_device() const noexcept { return physical_device_; }
    VkDevice device() const noexcept { return device_; }
    VmaAllocator allocator() const noexcept { return allocator_; }
    VkQueue graphics_queue() const noexcept { return graphics_queue_; }
    VkQueue compute_queue() const noexcept { return compute_queue_; }
    VkQueue transfer_queue() const noexcept { return transfer_queue_; }

    // Profiling helpers
    void begin_gpu_zone(VkCommandBuffer cmd, const char* name, uint32_t color = 0xFFFFFF) const noexcept;
    void end_gpu_zone(VkCommandBuffer cmd) const noexcept;

private:
    VulkanResult<void> create_instance();
    VulkanResult<void> create_surface(SDL_Window* window);
    VulkanResult<void> select_physical_device();
    VulkanResult<void> create_device();
    VulkanResult<void> create_allocator();
    VulkanResult<void> get_queues();
    VulkanResult<void> setup_debug_messenger();

    // Vulkan handles
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VmaAllocator allocator_ = VK_NULL_HANDLE;
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkQueue compute_queue_ = VK_NULL_HANDLE;
    VkQueue transfer_queue_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;

    // Queue families
    uint32_t graphics_family_ = 0;
    uint32_t compute_family_ = 0;
    uint32_t transfer_family_ = 0;

    // ProjectV интеграция
    VmaVulkanFunctions vulkan_functions_ = {};
    VkAllocationCallbacks allocation_callbacks_ = VulkanAllocator::getAllocationCallbacks();

    // Profiling
#ifdef TRACY_ENABLE
    TracyVkCtx tracy_vk_context_ = nullptr;
#endif
};
```

### 3. Реализация инициализации

```cpp
// vulkan/context.cpp
#include "context.hpp"

VulkanResult<void> VulkanContext::initialize(SDL_Window* window) {
    ZoneScopedN("VulkanContextInitialize");

    // 1. Инициализация volk (ПЕРВЫМ делом!)
    if (volkInitialize() != VK_SUCCESS) {
        projectv::core::Log::error("Vulkan", "volkInitialize failed");
        return std::unexpected(VulkanError::VolkInitFailed);
    }

    uint32_t version = volkGetInstanceVersion();
    projectv::core::Log::info("Vulkan", "Vulkan loader version: {}.{}.{}",
                 VK_VERSION_MAJOR(version),
                 VK_VERSION_MINOR(version),
                 VK_VERSION_PATCH(version));

    // 2. Создание instance
    if (auto result = create_instance(); !result) {
        return std::unexpected(result.error());
    }

    // 3. Загрузка instance функций
    volkLoadInstance(instance_);

    // 4. Создание surface
    if (auto result = create_surface(window); !result) {
        return std::unexpected(result.error());
    }

    // 5. Выбор physical device
    if (auto result = select_physical_device(); !result) {
        return std::unexpected(result.error());
    }

    // 6. Создание logical device
    if (auto result = create_device(); !result) {
        return std::unexpected(result.error());
    }

    // 7. Загрузка device функций (прямые вызовы драйвера!)
    volkLoadDevice(device_);

    // 8. Создание VMA allocator
    if (auto result = create_allocator(); !result) {
        return std::unexpected(result.error());
    }

    // 9. Получение очередей
    if (auto result = get_queues(); !result) {
        return std::unexpected(result.error());
    }

    // 10. Настройка debug messenger (только в Debug)
#ifdef DEBUG
    if (auto result = setup_debug_messenger(); !result) {
        projectv::core::Log::warning("Vulkan", "Debug messenger setup failed, continuing without debug");
    }
#endif

    // 11. Инициализация Tracy Vulkan context (только в Profile)
#ifdef TRACY_ENABLE
    tracy_vk_context_ = TracyVkContext(physical_device_, device_, graphics_queue_, graphics_family_);
    projectv::core::Log::info("Vulkan", "Tracy Vulkan context initialized");
#endif

    projectv::core::Log::info("Vulkan", "Vulkan context initialized successfully");
    return {};
}

VulkanResult<void> VulkanContext::create_instance() {
    // Обязательные extensions для SDL3 и отладки
    const char* instance_extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,  // Windows
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,    // Для отладки
    };

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "ProjectV";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "ProjectV Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;  // Минимум 1.3

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(std::size(instance_extensions));
    create_info.ppEnabledExtensionNames = instance_extensions;

    // Validation layers только в Debug
#ifdef DEBUG
    const char* validation_layers[] = {
        "VK_LAYER_KHRONOS_validation"
    };
    create_info.enabledLayerCount = static_cast<uint32_t>(std::size(validation_layers));
    create_info.ppEnabledLayerNames = validation_layers;
#endif

    if (vkCreateInstance(&create_info, nullptr, &instance_) != VK_SUCCESS) {
        return std::unexpected(VulkanError::InstanceCreationFailed);
    }

    return {};
}

VulkanResult<void> VulkanContext::select_physical_device() {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);

    if (device_count == 0) {
        return std::unexpected(VulkanError::NoSuitableDevice);
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());

    // Проверяем обязательные features Vulkan 1.3
    for (auto device : devices) {
        VkPhysicalDeviceFeatures2 features2 = {};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

        // Timeline semaphores
        VkPhysicalDeviceTimelineSemaphoreFeatures timeline_features = {};
        timeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;

        // Buffer device address (BDA)
        VkPhysicalDeviceBufferDeviceAddressFeatures bda_features = {};
        bda_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;

        // Descriptor indexing (bindless)
        VkPhysicalDeviceDescriptorIndexingFeatures indexing_features = {};
        indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

        // Synchronization2
        VkPhysicalDeviceSynchronization2Features sync2_features = {};
        sync2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;

        // Dynamic rendering
        VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features = {};
        dynamic_rendering_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;

        // Chain pNext
        features2.pNext = &timeline_features;
        timeline_features.pNext = &bda_features;
        bda_features.pNext = &indexing_features;
        indexing_features.pNext = &sync2_features;
        sync2_features.pNext = &dynamic_rendering_features;

        vkGetPhysicalDeviceFeatures2(device, &features2);

        // Проверяем обязательные features
        if (!timeline_features.timelineSemaphore ||
            !bda_features.bufferDeviceAddress ||
            !indexing_features.descriptorBindingPartiallyBound ||
            !indexing_features.runtimeDescriptorArray ||
            !sync2_features.synchronization2 ||
            !dynamic_rendering_features.dynamicRendering) {
            continue;
        }

        physical_device_ = device;
        return {};
    }

    return std::unexpected(VulkanError::NoSuitableDevice);
}

VulkanResult<void> VulkanContext::create_device() {
    // Обязательные device extensions
    const char* device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    };

    // Очереди: graphics + compute + transfer
    float queue_priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queue_infos;

    // Graphics queue
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, families.data());

    // Находим подходящие queue families
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_family_ = i;
            VkDeviceQueueCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            info.queueFamilyIndex = i;
            info.queueCount = 1;
            info.pQueuePriorities = &queue_priority;
            queue_infos.push_back(info);
        }
        if ((families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && i != graphics_family_) {
            compute_family_ = i;
            VkDeviceQueueCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            info.queueFamilyIndex = i;
            info.queueCount = 1;
            info.pQueuePriorities = &queue_priority;
            queue_infos.push_back(info);
        }
        if ((families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
            i != graphics_family_ && i != compute_family_) {
            transfer_family_ = i;
            VkDeviceQueueCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            info.queueFamilyIndex = i;
            info.queueCount = 1;
            info.pQueuePriorities = &queue_priority;
            queue_infos.push_back(info);
        }
    }

    // Features chain
    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    VkPhysicalDeviceTimelineSemaphoreFeatures timeline_features = {};
    timeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    timeline_features.timelineSemaphore = VK_TRUE;

    VkPhysicalDeviceBufferDeviceAddressFeatures bda_features = {};
    bda_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bda_features.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceDescriptorIndexingFeatures indexing_features = {};
    indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    indexing_features.descriptorBindingPartiallyBound = VK_TRUE;
    indexing_features.runtimeDescriptorArray = VK_TRUE;
    indexing_features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

    VkPhysicalDeviceSynchronization2Features sync2_features = {};
    sync2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    sync2_features.synchronization2 = VK_TRUE;

    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features = {};
    dynamic_rendering_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamic_rendering_features.dynamicRendering = VK_TRUE;

    // Chain
    features2.pNext = &timeline_features;
    timeline_features.pNext = &bda_features;
    bda_features.pNext = &indexing_features;
    indexing_features.pNext = &sync2_features;
    sync2_features.pNext = &dynamic_rendering_features;

    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pNext = &features2;
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size());
    create_info.pQueueCreateInfos = queue_infos.data();
    create_info.enabledExtensionCount = static_cast<uint32_t>(std::size(device_extensions));
    create_info.ppEnabledExtensionNames = device_extensions;

    if (vkCreateDevice(physical_device_, &create_info, nullptr, &device_) != VK_SUCCESS) {
        return std::unexpected(VulkanError::DeviceCreationFailed);
    }

    return {};
}

VulkanResult<void> VulkanContext::create_allocator() {
    VmaAllocatorCreateInfo create_info = {};
    create_info.instance = instance_;
    create_info.physicalDevice = physical_device_;
    create_info.device = device_;
    create_info.vulkanApiVersion = VK_API_VERSION_1_3;

    // Включаем BDA поддержку
    create_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    // Интеграция с volk: передаём функции напрямую
    create_info.pVulkanFunctions = &vulkan_functions_;

    // Получаем функции Vulkan через volk
    VmaVulkanFunctions vulkan_functions = {};
    vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    vulkan_functions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vulkan_functions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    vulkan_functions.vkAllocateMemory = vkAllocateMemory;
    vulkan_functions.vkFreeMemory = vkFreeMemory;
    vulkan_functions.vkMapMemory = vkMapMemory;
    vulkan_functions.vkUnmapMemory = vkUnmapMemory;
    vulkan_functions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
    vulkan_functions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
    vulkan_functions.vkBindBufferMemory = vkBindBufferMemory;
    vulkan_functions.vkBindImageMemory = vkBindImageMemory;
    vulkan_functions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
    vulkan_functions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
    vulkan_functions.vkCreateBuffer = vkCreateBuffer;
    vulkan_functions.vkDestroyBuffer = vkDestroyBuffer;
    vulkan_functions.vkCreateImage = vkCreateImage;
    vulkan_functions.vkDestroyImage = vkDestroyImage;
    vulkan_functions.vkCmdCopyBuffer = vkCmdCopyBuffer;

    // Для Vulkan 1.3+ и BDA
    vulkan_functions.vkGetBufferDeviceAddress = vkGetBufferDeviceAddress;
    vulkan_functions.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements;
    vulkan_functions.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;

    create_info.pVulkanFunctions = &vulkan_functions;

    if (vmaCreateAllocator(&create_info, &allocator_) != VK_SUCCESS) {
        return std::unexpected(VulkanError::VmaInitFailed);
    }

    projectv::core::Log::info("Vulkan", "VMA allocator created with BDA support");
    return {};
}

VulkanResult<void> VulkanContext::get_queues() {
    vkGetDeviceQueue(device_, graphics_family_, 0, &graphics_queue_);
    vkGetDeviceQueue(device_, compute_family_, 0, &compute_queue_);
    vkGetDeviceQueue(device_, transfer_family_, 0, &transfer_queue_);

    projectv::core::Log::info("Vulkan", "Queues acquired: graphics={}, compute={}, transfer={}",
                 static_cast<void*>(graphics_queue_),
                 static_cast<void*>(compute_queue_),
                 static_cast<void*>(transfer_queue_));
    return {};
}

VulkanResult<void> VulkanContext::create_surface(SDL_Window* window) {
    if (!SDL_Vulkan_CreateSurface(window, instance_, nullptr, &surface_)) {
        projectv::core::Log::error("Vulkan", "SDL_Vulkan_CreateSurface failed");
        return std::unexpected(VulkanError::SurfaceCreationFailed);
    }

    // Проверяем, что surface поддерживается выбранным устройством
    VkBool32 supported = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(physical_device_, graphics_family_, surface_, &supported);

    if (!supported) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
        projectv::core::Log::error("Vulkan", "Surface not supported by selected device");
        return std::unexpected(VulkanError::SurfaceCreationFailed);
    }

    projectv::core::Log::info("Vulkan", "Vulkan surface created successfully");
    return {};
}

void VulkanContext::shutdown() {
    ZoneScopedN("VulkanContextShutdown");

    // Освобождаем Tracy Vulkan context (только в Profile)
#ifdef TRACY_ENABLE
    if (tracy_vk_context_ != nullptr) {
        TracyVkDestroy(tracy_vk_context_);
        tracy_vk_context_ = nullptr;
        projectv::core::Log::info("Vulkan", "Tracy Vulkan context destroyed");
    }
#endif

    if (allocator_ != VK_NULL_HANDLE) {
        vmaDestroyAllocator(allocator_);
        allocator_ = VK_NULL_HANDLE;
        projectv::core::Log::info("Vulkan", "VMA allocator destroyed");
    }

    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
        projectv::core::Log::info("Vulkan", "Vulkan device destroyed");
    }

    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
        projectv::core::Log::info("Vulkan", "Vulkan surface destroyed");
    }

    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
        projectv::core::Log::info("Vulkan", "Vulkan instance destroyed");
    }

    graphics_queue_ = VK_NULL_HANDLE;
    compute_queue_ = VK_NULL_HANDLE;
    transfer_queue_ = VK_NULL_HANDLE;

    projectv::core::Log::info("Vulkan", "Vulkan context shutdown complete");
}

VulkanResult<void> VulkanContext::setup_debug_messenger() {
#ifdef DEBUG
    auto create_info = VulkanDebugMessenger::getCreateInfo();

    auto vkCreateDebugUtilsMessengerEXT =
        reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));

    if (!vkCreateDebugUtilsMessengerEXT) {
        projectv::core::Log::warning("Vulkan", "vkCreateDebugUtilsMessengerEXT not available");
        return std::unexpected(VulkanError::DebugMessengerFailed);
    }

    if (vkCreateDebugUtilsMessengerEXT(instance_, &create_info, nullptr, &debug_messenger_) != VK_SUCCESS) {
        projectv::core::Log::warning("Vulkan", "Failed to create debug messenger");
        return std::unexpected(VulkanError::DebugMessengerFailed);
    }

    projectv::core::Log::info("Vulkan", "Debug messenger created successfully");
#endif
    return {};
}

void VulkanContext::begin_gpu_zone(VkCommandBuffer cmd, const char* name, uint32_t color) const noexcept {
#ifdef TRACY_ENABLE
    if (tracy_vk_context_ != nullptr) {
        TracyVkZone(tracy_vk_context_, cmd, name);
    }
#endif
}

void VulkanContext::end_gpu_zone(VkCommandBuffer cmd) const noexcept {
#ifdef TRACY_ENABLE
    if (tracy_vk_context_ != nullptr) {
        TracyVkZoneEnd(tracy_vk_context_, cmd);
    }
#endif
}

VulkanContext::VulkanContext(VulkanContext&& other) noexcept
    : instance_(other.instance_)
    , surface_(other.surface_)
    , physical_device_(other.physical_device_)
    , device_(other.device_)
    , allocator_(other.allocator_)
    , graphics_queue_(other.graphics_queue_)
    , compute_queue_(other.compute_queue_)
    , transfer_queue_(other.transfer_queue_)
    , graphics_family_(other.graphics_family_)
    , compute_family_(other.compute_family_)
    , transfer_family_(other.transfer_family_)
    , vulkan_functions_(other.vulkan_functions_) {

    other.instance_ = VK_NULL_HANDLE;
    other.surface_ = VK_NULL_HANDLE;
    other.physical_device_ = VK_NULL_HANDLE;
    other.device_ = VK_NULL_HANDLE;
    other.allocator_ = VK_NULL_HANDLE;
    other.graphics_queue_ = VK_NULL_HANDLE;
    other.compute_queue_ = VK_NULL_HANDLE;
    other.transfer_queue_ = VK_NULL_HANDLE;
}

VulkanContext& VulkanContext::operator=(VulkanContext&& other) noexcept {
    if (this != &other) {
        shutdown();

        instance_ = other.instance_;
        surface_ = other.surface_;
        physical_device_ = other.physical_device_;
        device_ = other.device_;
        allocator_ = other.allocator_;
        graphics_queue_ = other.graphics_queue_;
        compute_queue_ = other.compute_queue_;
        transfer_queue_ = other.transfer_queue_;
        graphics_family_ = other.graphics_family_;
        compute_family_ = other.compute_family_;
        transfer_family_ = other.transfer_family_;
        vulkan_functions_ = other.vulkan_functions_;

        other.instance_ = VK_NULL_HANDLE;
        other.surface_ = VK_NULL_HANDLE;
        other.physical_device_ = VK_NULL_HANDLE;
        other.device_ = VK_NULL_HANDLE;
        other.allocator_ = VK_NULL_HANDLE;
        other.graphics_queue_ = VK_NULL_HANDLE;
        other.compute_queue_ = VK_NULL_HANDLE;
        other.transfer_queue_ = VK_NULL_HANDLE;
    }
    return *this;
}

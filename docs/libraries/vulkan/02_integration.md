# Vulkan Libraries — Integration

<!-- anchor: 02_integration -->

Подключение volk и VMA к ProjectV, инициализация Modern Vulkan 1.4 с обязательными расширениями.

---

## CMake Integration

### Вариант 1: Git Submodules (рекомендуется)

```cmake
# Добавить подмодули
# git submodule add https://github.com/zeux/volk.git external/volk
# git submodule add https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git external/VulkanMemoryAllocator

add_subdirectory(external/volk)
add_subdirectory(external/VulkanMemoryAllocator)

target_link_libraries(ProjectV PRIVATE
  volk::volk
  VulkanMemoryAllocator::VulkanMemoryAllocator
)
```

### Вариант 2: FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
  volk
  GIT_REPOSITORY https://github.com/zeux/volk.git
  GIT_TAG master
)

FetchContent_Declare(
  VMA
  GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
  GIT_TAG master
)

FetchContent_MakeAvailable(volk VMA)

target_link_libraries(ProjectV PRIVATE
  volk::volk
  VulkanMemoryAllocator::VulkanMemoryAllocator
)
```

### Вариант 3: vcpkg

```
# vcpkg.json
{
    "dependencies": [
        "volk",
        "vulkan-memory-allocator"
    ]
}

# CMakeLists.txt
find_package(volk REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)

target_link_libraries(ProjectV PRIVATE
    volk::volk
    VulkanMemoryAllocator::VulkanMemoryAllocator
)
```

---

## Инициализация Modern Vulkan 1.4

### 1. volk инициализация (первым делом!)

```cpp
#include <volk.h>
#include <print>
#include <expected>

enum class VulkanError {
    VolkInitFailed,
    InstanceCreationFailed,
    DeviceCreationFailed,
    VmaInitFailed
};

template<typename T>
using VulkanResult = std::expected<T, VulkanError>;

class VulkanContext {
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VmaAllocator allocator_ = VK_NULL_HANDLE;

public:
    VulkanResult<void> initialize() {
        // 1. Инициализация volk (ДО любого Vulkan вызова!)
        VkResult result = volkInitialize();
        if (result != VK_SUCCESS) {
            std::print(stderr, "volkInitialize failed: {}\n", result);
            return std::unexpected(VulkanError::VolkInitFailed);
        }

        // 2. Проверка версии Vulkan
        uint32_t version = volkGetInstanceVersion();
        if (version < VK_API_VERSION_1_3) {
            std::print(stderr, "Vulkan 1.3+ required, got: {}.{}.{}\n",
                       VK_VERSION_MAJOR(version),
                       VK_VERSION_MINOR(version),
                       VK_VERSION_PATCH(version));
            return std::unexpected(VulkanError::VolkInitFailed);
        }

        // 3. Создание instance с Modern Vulkan features
        if (!create_instance()) {
            return std::unexpected(VulkanError::InstanceCreationFailed);
        }

        // 4. Загрузка instance функций
        volkLoadInstance(instance_);

        // 5. Выбор physical device
        if (!select_physical_device()) {
            return std::unexpected(VulkanError::DeviceCreationFailed);
        }

        // 6. Создание logical device с обязательными расширениями
        if (!create_device()) {
            return std::unexpected(VulkanError::DeviceCreationFailed);
        }

        // 7. Загрузка device функций
        volkLoadDevice(device_);

        // 8. Инициализация VMA
        if (!initialize_vma()) {
            return std::unexpected(VulkanError::VmaInitFailed);
        }

        std::print("Vulkan context initialized: API {}.{}.{}\n",
                   VK_VERSION_MAJOR(version),
                   VK_VERSION_MINOR(version),
                   VK_VERSION_PATCH(version));

        return {};
    }

private:
    bool create_instance() {
        // Обязательные extensions для ProjectV
        const char* instance_extensions[] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,  // или другие платформы
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,    // Для отладки
        };

        // Vulkan 1.3 core features
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

        // Validation layers (только в debug)
        #ifdef _DEBUG
        const char* validation_layers[] = {
            "VK_LAYER_KHRONOS_validation"
        };
        create_info.enabledLayerCount = static_cast<uint32_t>(std::size(validation_layers));
        create_info.ppEnabledLayerNames = validation_layers;
        #endif

        VkResult result = volkCreateInstance(&create_info, nullptr, &instance_);
        return result == VK_SUCCESS;
    }

    bool select_physical_device() {
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);

        if (device_count == 0) {
            std::print(stderr, "No Vulkan devices found\n");
            return false;
        }

        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());

        // Выбираем устройство с поддержкой обязательных features
        for (auto device : devices) {
            if (check_device_features(device)) {
                physical_device_ = device;
                return true;
            }
        }

        std::print(stderr, "No suitable Vulkan device found\n");
        return false;
    }

    bool check_device_features(VkPhysicalDevice device) {
        // Проверяем обязательные features Vulkan 1.3
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
        if (!timeline_features.timelineSemaphore) {
            std::print(stderr, "Device missing timeline semaphores\n");
            return false;
        }
        if (!bda_features.bufferDeviceAddress) {
            std::print(stderr, "Device missing buffer device address\n");
            return false;
        }
        if (!indexing_features.descriptorBindingPartiallyBound ||
            !indexing_features.runtimeDescriptorArray) {
            std::print(stderr, "Device missing descriptor indexing features\n");
            return false;
        }
        if (!sync2_features.synchronization2) {
            std::print(stderr, "Device missing synchronization2\n");
            return false;
        }
        if (!dynamic_rendering_features.dynamicRendering) {
            std::print(stderr, "Device missing dynamic rendering\n");
            return false;
        }

        return true;
    }

    bool create_device() {
        // Обязательные device extensions
        const char* device_extensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        };

        // Опциональные extensions (если поддерживаются)
        std::vector<const char*> optional_extensions;
        if (check_extension_support(VK_EXT_SHADER_OBJECT_EXTENSION_NAME)) {
            optional_extensions.push_back(VK_EXT_SHADER_OBJECT_EXTENSION_NAME);
        }
        if (check_extension_support(VK_EXT_MESH_SHADER_EXTENSION_NAME)) {
            optional_extensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
        }

        // Объединяем обязательные и опциональные
        std::vector<const char*> all_extensions;
        all_extensions.insert(all_extensions.end(),
                             std::begin(device_extensions),
                             std::end(device_extensions));
        all_extensions.insert(all_extensions.end(),
                             optional_extensions.begin(),
                             optional_extensions.end());

        // Очереди: graphics + compute + transfer
        float queue_priority = 1.0f;

        VkDeviceQueueCreateInfo queue_infos[3] = {};

        // Graphics queue
        uint32_t graphics_family = find_queue_family(VK_QUEUE_GRAPHICS_BIT);
        queue_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_infos[0].queueFamilyIndex = graphics_family;
        queue_infos[0].queueCount = 1;
        queue_infos[0].pQueuePriorities = &queue_priority;

        // Compute queue (желательно отдельная)
        uint32_t compute_family = find_queue_family(VK_QUEUE_COMPUTE_BIT);
        if (compute_family != graphics_family) {
            queue_infos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_infos[1].queueFamilyIndex = compute_family;
            queue_infos[1].queueCount = 1;
            queue_infos[1].pQueuePriorities = &queue_priority;
        }

        // Transfer queue (желательно отдельная)
        uint32_t transfer_family = find_queue_family(VK_QUEUE_TRANSFER_BIT);
        if (transfer_family != graphics_family && transfer_family != compute_family) {
            queue_infos[2].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_infos[2].queueFamilyIndex = transfer_family;
            queue_infos[2].queueCount = 1;
            queue_infos[2].pQueuePriorities = &queue_priority;
        }

        // Features chain (как в check_device_features)
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
        create_info.queueCreateInfoCount = 3;
        create_info.pQueueCreateInfos = queue_infos;
        create_info.enabledExtensionCount = static_cast<uint32_t>(all_extensions.size());
        create_info.ppEnabledExtensionNames = all_extensions.data();

        VkResult result = vkCreateDevice(physical_device_, &create_info, nullptr, &device_);
        return result == VK_SUCCESS;
    }

    bool initialize_vma() {
        VmaAllocatorCreateInfo create_info = {};
        create_info.instance = instance_;
        create_info.physicalDevice = physical_device_;
        create_info.device = device_;
        create_info.vulkanApiVersion = VK_API_VERSION_1_3;

        // Включаем BDA поддержку
        create_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

        // Включаем memory budgeting (опционально)
        #ifdef VK_EXT_memory_budget
        if (check_extension_support(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)) {
            create_info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
        }
        #endif

        VkResult result = vmaCreateAllocator(&create_info, &allocator_);
        return result == VK_SUCCESS;
    }

    uint32_t find_queue_family(VkQueueFlags required_flags) {
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, families.data());

        for (uint32_t i = 0; i < queue_family_count; ++i) {
            if (families[i].queueFlags & required_flags) {
                return i;
            }
        }

        return 0;  // Fallback
    }

    bool check_extension_support(const char* extension_name) {
        uint32_t extension_count = 0;
        vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &extension_count, nullptr);

        std::vector<VkExtensionProperties> extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &extension_count, extensions.data());

        for (const auto& extension : extensions) {
            if (strcmp(extension.extensionName, extension_name) == 0) {
                return true;
            }
        }
        return false;
    }
};

// Использование
int main() {
    VulkanContext context;
    if (auto result = context.initialize(); !result) {
        std::print(stderr, "Failed to initialize Vulkan\n");
        return 1;
    }

    std::print("Vulkan ready for ProjectV\n");
    return 0;
}
```

---

## Получение очередей и завершение инициализации

```cpp
class VulkanContext {
    // ... предыдущие поля ...

    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkQueue compute_queue_ = VK_NULL_HANDLE;
    VkQueue transfer_queue_ = VK_NULL_HANDLE;

public:
    VulkanResult<void> initialize() {
        // ... предыдущая инициализация ...

        // 9. Получение очередей
        vkGetDeviceQueue(device_, graphics_family_, 0, &graphics_queue_);
        vkGetDeviceQueue(device_, compute_family_, 0, &compute_queue_);
        vkGetDeviceQueue(device_, transfer_family_, 0, &transfer_queue_);

        // 10. Проверка timeline semaphores
        if (!check_timeline_semaphore_support()) {
            return std::unexpected(VulkanError::DeviceCreationFailed);
        }

        std::print("Vulkan context fully initialized\n");
        return {};
    }

    VkQueue graphics_queue() const { return graphics_queue_; }
    VkQueue compute_queue() const { return compute_queue_; }
    VkQueue transfer_queue() const { return transfer_queue_; }
    VmaAllocator allocator() const { return allocator_; }

private:
    bool check_timeline_semaphore_support() {
        VkSemaphoreTypeCreateInfo timeline_info = {};
        timeline_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timeline_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timeline_info.initialValue = 0;

        VkSemaphoreCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        create_info.pNext = &timeline_info;

        VkSemaphore semaphore = VK_NULL_HANDLE;
        VkResult result = vkCreateSemaphore(device_, &create_info, nullptr, &semaphore);

        if (result == VK_SUCCESS) {
            vkDestroySemaphore(device_, semaphore, nullptr);
            return true;
        }

        return false;
    }

    uint32_t graphics_family_ = 0;
    uint32_t compute_family_ = 0;
    uint32_t transfer_family_ = 0;
};
```

---

## Заключение

Инициализация Modern Vulkan 1.4 для ProjectV требует:

1. **volk** как мета-лоадер для избежания overhead драйвера
2. **VMA** для эффективного управления памятью с TLSF алгоритмом
3. **Обязательные расширения**: timeline semaphores, buffer device address, descriptor indexing, synchronization2,
   dynamic rendering
4. **Опциональные расширения**: shader objects, mesh shaders для будущих оптимизаций
5. **Отдельные очереди**: graphics, compute, transfer для параллелизма

Эта конфигурация обеспечивает основу для:

- Bindless rendering (descriptor indexing)
- GPU-driven rendering (buffer device address)
- Асинхронные вычисления (timeline semaphores)
- Современный рендеринг без legacy (dynamic rendering)

Следующий шаг: продвинутые техники в `03_advanced.md`.

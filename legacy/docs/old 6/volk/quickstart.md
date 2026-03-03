# Быстрый старт volk [🟢 Уровень 1]

**🟢 Уровень 1: Начинающий** — Инициализация и загрузка функций.

Минимальный пример интеграции volk в проект Vulkan. Этот quickstart охватывает базовую настройку CMake, инициализацию и
загрузку функций.

## Оглавление

- [1. CMake настройка](#1-cmake-настройка)
- [2. Базовая инициализация](#2-базовая-инициализация)
- [3. Интеграция с SDL3](#3-интеграция-с-sdl3)
- [4. Интеграция с VMA](#4-интеграция-с-vma)
- [5. Header-only режим](#5-header-only-режим)
- [6. Следующие шаги](#6-следующие-шаги)

---

## 1. CMake настройка

### Способ 1: Статическая библиотека (рекомендуется)

```cmake
# Добавьте volk как подмодуль в external/volk
add_subdirectory(external/volk)

# Подключите к вашему приложению
target_link_libraries(YourApp PRIVATE volk)
```

### Способ 2: Header-only режим

```cmake
add_subdirectory(external/volk)
target_link_libraries(YourApp PRIVATE volk_headers)
```

**Важно:** Определите `VK_NO_PROTOTYPES` во всех единицах трансляции, которые включают Vulkan заголовки:

```cmake
target_compile_definitions(YourApp PRIVATE VK_NO_PROTOTYPES)
```

---

## 2. Базовая инициализация

### 2.1. Минимальный рабочий пример

```cpp
#define VK_NO_PROTOTYPES  // Обязательно!
#include "volk.h"
#include <iostream>

int main() {
    // 1. Инициализация Vulkan loader
    VkResult result = volkInitialize();
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to initialize Vulkan loader: " << result << std::endl;
        return -1;
    }

    // 2. Создание Vulkan instance
    VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.pApplicationName = "Volk Quickstart";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    createInfo.pApplicationInfo = &appInfo;

    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance" << std::endl;
        return -1;
    }

    // 3. Загрузка instance-функций
    volkLoadInstance(instance);

    // 4. Выбор физического устройства
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    if (deviceCount == 0) {
        std::cerr << "No Vulkan devices found" << std::endl;
        vkDestroyInstance(instance, nullptr);
        return -1;
    }

    VkPhysicalDevice physicalDevice = devices[0];

    // 5. Создание logical device
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    queueCreateInfo.queueFamilyIndex = 0; // Нужно определить реальный индекс
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = 0;
    deviceCreateInfo.ppEnabledExtensionNames = nullptr;

    VkDevice device = VK_NULL_HANDLE;
    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan device" << std::endl;
        vkDestroyInstance(instance, nullptr);
        return -1;
    }

    // 6. Загрузка device-функций (оптимизация производительности)
    volkLoadDevice(device);

    std::cout << "Volk initialized successfully!" << std::endl;
    std::cout << "Instance: " << instance << std::endl;
    std::cout << "Device: " << device << std::endl;

    // 7. Очистка (обратный порядок)
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    return 0;
}
```

### 2.2. Проверка инициализации

```cpp
// Проверка состояния после инициализации
VkInstance loadedInstance = volkGetLoadedInstance();
VkDevice loadedDevice = volkGetLoadedDevice();

if (loadedInstance == VK_NULL_HANDLE) {
    std::cerr << "Instance not loaded!" << std::endl;
}

if (loadedDevice == VK_NULL_HANDLE) {
    std::cerr << "Device not loaded!" << std::endl;
}
```

---

## 3. Интеграция с SDL3

volk хорошо интегрируется с SDL3, который может загружать Vulkan loader самостоятельно:

```cpp
#define VK_NO_PROTOTYPES
#include "volk.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <iostream>

int main() {
    // Инициализация SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    // Создание окна с Vulkan поддержкой
    SDL_Window* window = SDL_CreateWindow("Volk + SDL3", 800, 600, SDL_WINDOW_VULKAN);
    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    // SDL предоставляет свой vkGetInstanceProcAddr
    // Используем volkInitializeCustom вместо volkInitialize
    volkInitializeCustom(SDL_vkGetInstanceProcAddr);

    // Дальнейшая инициализация Vulkan как обычно
    VkInstance instance = createVulkanInstance(); // Ваша функция
    volkLoadInstance(instance);

    // Создание surface через SDL
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        std::cerr << "Failed to create Vulkan surface: " << SDL_GetError() << std::endl;
    }

    // ... остальная инициализация Vulkan

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
```

---

## 4. Интеграция с VMA

Vulkan Memory Allocator (VMA) требует указатели на Vulkan функции. volk предоставляет удобную интеграцию:

```cpp
#define VK_NO_PROTOTYPES
#include "volk.h"
#include "vk_mem_alloc.h"
#include <iostream>

int main() {
    // Инициализация volk как обычно
    volkInitialize();

    VkInstance instance = createVulkanInstance();
    volkLoadInstance(instance);

    VkDevice device = createVulkanDevice(instance);
    volkLoadDevice(device);  // Важно: VMA требует загруженные device-функции

    // Создание аллокатора VMA с импортом функций из volk
    VmaAllocator allocator = VK_NULL_HANDLE;
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.instance = instance;
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;

    // Автоматический импорт функций из volk (рекомендуется)
    vmaImportVulkanFunctionsFromVolk(&allocatorInfo);

    // Или вручную
    // VmaVulkanFunctions vkFuncs = {};
    // vkFuncs.vkGetInstanceProcAddr = volkGetInstanceProcAddr;
    // vkFuncs.vkGetDeviceProcAddr = volkGetDeviceProcAddr;
    // allocatorInfo.pVulkanFunctions = &vkFuncs;

    if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS) {
        std::cerr << "Failed to create VMA allocator" << std::endl;
        return -1;
    }

    std::cout << "VMA allocator created successfully with volk integration" << std::endl;

    // Использование аллокатора
    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = 1024 * 1024; // 1 MB
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    VmaAllocationCreateInfo allocationInfo = {};
    allocationInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    vmaCreateBuffer(allocator, &bufferInfo, &allocationInfo, &buffer, &allocation, nullptr);

    // ... использование буфера

    vmaDestroyBuffer(allocator, buffer, allocation);
    vmaDestroyAllocator(allocator);

    return 0;
}
```

---

## 5. Header-only режим

Если вы не хотите использовать CMake target `volk`, можно использовать header-only режим:

```cpp
// В одном файле (.cpp) определите реализацию
#define VK_NO_PROTOTYPES
#define VOLK_IMPLEMENTATION
#include "volk.h"

// В других файлах просто включайте
#define VK_NO_PROTOTYPES
#include "volk.h"
```

**Важные замечания:**

- `VOLK_IMPLEMENTATION` должен быть определён только в одной единице трансляции
- `VK_NO_PROTOTYPES` должен быть определён во всех файлах, включающих `volk.h` или `vulkan.h`
- Убедитесь, что `volk.h` доступен в путях включения

---

## 6. Следующие шаги

После успешной инициализации volk, изучите следующие темы:

1. **Оптимизация производительности:**
  - Использование `VolkDeviceTable` для многопоточного рендеринга
  - Измерение dispatch overhead с Tracy
  - См. [Производительность](performance.md)

2. **Расширенные сценарии:**
  - Несколько Vulkan устройств (multi-GPU)
  - Асинхронные compute очереди
  - Интеграция с validation layers
  - См. [Практические сценарии](use-cases.md)

3. **Решение проблем:**
  - Ошибки компиляции и линковки
  - Runtime ошибки инициализации
  - См. [Решение проблем](troubleshooting.md)

4. **Для ProjectV:**
  - Специфичные паттерны для воксельного рендеринга
  - Интеграция с Tracy, SDL3, VMA
  - Многопоточные оптимизации
  - См. [Интеграция с ProjectV](projectv-integration.md)

---

## См. также

- [Полная интеграция](integration.md) — детальное руководство по настройке CMake, конфигурации и оптимизации
- [Архитектура volk](concepts.md) — понимание мета-загрузчика и dispatch overhead
- [Глоссарий](glossary.md) — определения терминов volk и Vulkan
- [API справочник](api-reference.md) — полный список функций volk

← [Назад к документации volk](README.md)

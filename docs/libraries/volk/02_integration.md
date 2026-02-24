# Интеграция volk в ProjectV

Как подключить volk к ProjectV и использовать его в коде движка.

---

## CMake конфигурация

### Подключение как подмодуль

ProjectV использует volk как Git submodule в `external/volk`. CMake конфигурация включает платформенные defines для
корректной работы:

```cmake
# CMakeLists.txt ProjectV
cmake_minimum_required(VERSION 3.25)
project(ProjectV LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Платформенные defines для volk
if (WIN32)
  set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif (UNIX AND NOT APPLE)
  set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XLIB_KHR)
elseif (APPLE)
  set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_MACOS_MVK)
endif ()

# Подключение volk
add_subdirectory(external/volk)

# Проект
add_executable(ProjectV
  src/main.cpp
  src/vulkan/vulkan_context.cpp
  # ... другие файлы
)

target_include_directories(ProjectV PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ProjectV PRIVATE
  volk
  SDL3::SDL3
  # ... другие библиотеки
)

# Обязательный макрос для всех файлов
target_compile_definitions(ProjectV PRIVATE VK_NO_PROTOTYPES)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(ProjectV PRIVATE DEBUG)
endif ()
```

### Выбор режима сборки

volk предоставляет два CMake target'а:

| Target         | Описание               | Когда использовать в ProjectV                 |
|----------------|------------------------|-----------------------------------------------|
| `volk`         | Статическая библиотека | **Рекомендуется** - лучшая производительность |
| `volk_headers` | Header-only режим      | Для быстрого прототипирования                 |

ProjectV использует статическую библиотеку для максимальной производительности.

---

## Порядок инициализации

### Главный файл (main.cpp)

```cpp
#define VK_NO_PROTOTYPES
#define VOLK_IMPLEMENTATION
#include "volk.h"

#include "vulkan/vulkan_context.hpp"
#include <SDL3/SDL.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    // 1. Инициализация SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // 2. Проверка поддержки Vulkan через volk
    if (volkInitialize() != VK_SUCCESS) {
        fprintf(stderr, "Vulkan not found\n");
        SDL_Quit();
        return 1;
    }

    printf("Vulkan %u.%u.%u\n",
           VK_VERSION_MAJOR(volkGetInstanceVersion()),
           VK_VERSION_MINOR(volkGetInstanceVersion()),
           VK_VERSION_PATCH(volkGetInstanceVersion()));

    // 3. Создание Vulkan контекста
    VulkanContext vkContext;
    if (!vkContext.initialize()) {
        fprintf(stderr, "Failed to initialize Vulkan\n");
        SDL_Quit();
        return 1;
    }

    // 4. Основной цикл
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

    // 5. Очистка
    vkContext.shutdown();
    SDL_Quit();

    return 0;
}
```

### Ключевые моменты инициализации

1. **VOLK_IMPLEMENTATION** должен быть определён только в одном `.cpp` файле (обычно `main.cpp`)
2. **VK_NO_PROTOTYPES** обязателен для всех файлов, включающих Vulkan заголовки
3. **volkInitialize()** проверяет наличие Vulkan loader в системе
4. **volkGetInstanceVersion()** возвращает максимальную поддерживаемую версию Vulkan

---

## Vulkan Context

### Заголовочный файл

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

### Создание Instance

```cpp
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

    // Критически важно: загрузка instance-функций
    volkLoadInstance(m_instance);
    printf("VkInstance created\n");
    return true;
}
```

### Создание Device

```cpp
bool VulkanContext::createDevice() {
    // ... поиск queue families, создание device

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
```

---

## Интеграция с VMA

Vulkan Memory Allocator требует указатели на Vulkan функции. volk обеспечивает их через `volkLoadInstance` и
`volkLoadDevice`:

```cpp
// Убедитесь, что эти макросы определены перед включением vk_mem_alloc.h
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#include "vk_mem_alloc.h"

VmaAllocator createAllocator(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device) {
    // volkLoadInstance и volkLoadDevice уже должны быть вызваны

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

**Важно:** VMA будет использовать `vkGetInstanceProcAddr` и `vkGetDeviceProcAddr`, которые volk уже настроил для прямых
вызовов драйвера.

---

## Интеграция с SDL3

SDL3 и volk работают вместе для создания Vulkan surface:

```cpp
bool VulkanContext::createSurface() {
    if (!SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_surface)) {
        fprintf(stderr, "SDL_Vulkan_CreateSurface failed\n");
        return false;
    }
    return true;
}
```

Порядок важен:

1. `volkInitialize()` - проверка Vulkan
2. `vkCreateInstance()` - создание instance
3. `volkLoadInstance()` - загрузка instance-функций
4. `SDL_Vulkan_CreateSurface()` - создание surface

---

## Интеграция с Tracy

Для профилирования Vulkan вызовов в ProjectV:

```cpp
#include <tracy/TracyVulkan.hpp>

// Создание контекста Tracy для Vulkan
TracyVkCtx tracyCtx = TracyVkContext(
    physicalDevice,
    device,
    queue,
    commandBuffer,
    volkGetInstanceProcAddr,  // volk предоставляет эти функции
    volkGetDeviceProcAddr
);

// Использование в коде рендеринга
{
    TracyVkZone(tracyCtx, commandBuffer, "RenderVoxels");
    vkCmdDraw(...);
}

// Очистка при завершении
TracyVkDestroy(tracyCtx);
```

---

## Обработка ошибок

### Проверка поддержки Vulkan

```cpp
bool checkVulkanSupport() {
    // Проверка наличия Vulkan
    if (volkInitialize() != VK_SUCCESS) {
        showErrorMessage("Vulkan not installed. Please install Vulkan SDK from vulkan.lunarg.com");
        return false;
    }

    // Проверка версии (ProjectV требует Vulkan 1.3+)
    uint32_t version = volkGetInstanceVersion();
    if (version < VK_API_VERSION_1_3) {
        showErrorMessage("Vulkan 1.3+ required. Current version: %u.%u",
                        VK_VERSION_MAJOR(version), VK_VERSION_MINOR(version));
        return false;
    }

    return true;
}
```

### Отладка проблем с линковкой

Если возникают ошибки линковки с неопределёнными символами Vulkan:

1. Убедитесь, что `VK_NO_PROTOTYPES` определён во всех файлах
2. Проверьте, что `volkLoadInstance()` и `volkLoadDevice()` вызываются в правильном порядке
3. Убедитесь, что не включаете `vulkan.h` напрямую (только через `volk.h`)

---

## Конфигурационные макросы

| Макрос                      | Описание                                 | Использование в ProjectV      |
|-----------------------------|------------------------------------------|-------------------------------|
| `VK_NO_PROTOTYPES`          | **Обязателен** - предотвращает конфликты | В CMake для всего проекта     |
| `VOLK_IMPLEMENTATION`       | Включает реализацию                      | Только в main.cpp             |
| `VOLK_NAMESPACE`            | Помещает символы в namespace             | Не используется               |
| `VOLK_NO_DEVICE_PROTOTYPES` | Скрывает прототипы device-функций        | Для table-based интерфейса    |
| `VOLK_VULKAN_H_PATH`        | Кастомный путь к vulkan.h                | Если нужна специфичная версия |

---

## Структура файлов в ProjectV

```
ProjectV/
├── src/
│   ├── main.cpp                    # VOLK_IMPLEMENTATION здесь
│   ├── vulkan/
│   │   ├── vulkan_context.hpp      # VK_NO_PROTOTYPES, include "volk.h"
│   │   ├── vulkan_context.cpp      # VK_NO_PROTOTYPES, include "volk.h"
│   │   ├── device.hpp
│   │   └── device.cpp
│   └── ...
├── external/
│   └── volk/                       # Git submodule
└── CMakeLists.txt                  # target_link_libraries(volk)
```

---

## Ключевые правила интеграции

1. **Один VOLK_IMPLEMENTATION** - только в одном `.cpp` файле
2. **Везде VK_NO_PROTOTYPES** - через CMake для всего проекта
3. **Правильный порядок загрузки** - `volkLoadInstance()` после `vkCreateInstance()`, `volkLoadDevice()` после
   `vkCreateDevice()`
4. **Интеграция с VMA** - VMA использует функции volk автоматически
5. **Проверка версии** - используйте `volkGetInstanceVersion()` для проверки поддержки

Эти правила обеспечивают корректную работу volk в ProjectV и максимальную производительность Vulkan вызовов.

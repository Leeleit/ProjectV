# Интеграция volk в ProjectV

> **Для понимания:** Представьте, что volk — это "прямой провод" между вашим кодом и GPU драйвером. Без volk каждый
> вызов Vulkan проходит через "диспетчера" (системный loader), который проверяет права и перенаправляет вызов. С volk вы
> получаете прямой номер телефона к драйверу — минуя диспетчера, минуя очередь, минуя бюрократию. Это как VIP-пропуск на
> GPU.

Как подключить volk к ProjectV и использовать его в коде воксельного движка.

---

## CMake конфигурация

### Подключение как подмодуль

ProjectV использует volk как Git submodule в `external/volk`. CMake конфигурация включает платформенные defines для
Windows:

```cmake
# CMakeLists.txt ProjectV
cmake_minimum_required(VERSION 3.25)
project(ProjectV LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Платформенные defines для volk (только Windows)
if (WIN32)
  set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif (UNIX AND NOT APPLE)
  set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XLIB_KHR)
endif ()

# Подключение volk
add_subdirectory(external/volk)

# Проект
add_executable(ProjectV
  src/main.cpp
  src/vulkan/vulkan_context.cpp
  # Другие файлы проекта (например: src/vulkan/device.cpp, src/ecs/world.cpp и т.д.)
)

target_include_directories(ProjectV PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ProjectV PRIVATE
  volk
  SDL3::SDL3
  # Другие библиотеки проекта (например: flecs, VMA, Tracy и т.д.)
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

### Главный файл (main.cpp) с C++26

```cpp
#define VK_NO_PROTOTYPES
#define VOLK_IMPLEMENTATION
#include "volk.h"

#include "vulkan/vulkan_context.hpp"
#include <SDL3/SDL.h>
#include <print>
#include <expected>

// RAII-обёртка для SDL с std::expected
struct SDLContext {
    SDLContext() {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            std::println(stderr, "SDL_Init failed: {}", SDL_GetError());
            return;
        }
        initialized = true;
    }

    ~SDLContext() {
        if (initialized) SDL_Quit();
    }

    explicit operator bool() const { return initialized; }

private:
    bool initialized = false;
};

// RAII-обёртка для volk с std::expected
struct VolkContext {
    static std::expected<VolkContext, std::string> create() {
        if (volkInitialize() != VK_SUCCESS) {
            return std::unexpected("Vulkan not found");
        }

        uint32_t version = volkGetInstanceVersion();
        if (version < VK_API_VERSION_1_3) {
            return std::unexpected(std::format(
                "Vulkan 1.3+ required. Current version: {}.{}.{}",
                VK_VERSION_MAJOR(version),
                VK_VERSION_MINOR(version),
                VK_VERSION_PATCH(version)
            ));
        }

        return VolkContext{version};
    }

    void print_version() const {
        std::println("Vulkan {}.{}.{}",
            VK_VERSION_MAJOR(version),
            VK_VERSION_MINOR(version),
            VK_VERSION_PATCH(version));
    }

private:
    VolkContext(uint32_t v) : version(v) {}
    uint32_t version;
};

int main(int argc, char* argv[]) {
    // 1. Инициализация SDL
    SDLContext sdl;
    if (!sdl) return 1;

    // 2. Проверка поддержки Vulkan через volk
    auto volk_result = VolkContext::create();
    if (!volk_result) {
        std::println(stderr, "{}", volk_result.error());
        return 1;
    }

    volk_result->print_version();

    // 3. Создание Vulkan контекста
    VulkanContext vkContext;
    if (!vkContext.initialize()) {
        std::println(stderr, "Failed to initialize Vulkan");
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

    // 5. Очистка (RAII сделает всё автоматически)
    return 0;
}
```

### Ключевые моменты инициализации

1. **VOLK_IMPLEMENTATION** должен быть определён только в одном `.cpp` файле (обычно `main.cpp`)
2. **VK_NO_PROTOTYPES** обязателен для всех файлов, включающих Vulkan заголовки
3. **volkInitialize()** проверяет наличие Vulkan loader в системе
4. **volkGetInstanceVersion()** возвращает максимальную поддерживаемую версию Vulkan

---

## Vulkan Context с C++26

### Заголовочный файл

```cpp
// vulkan/vulkan_context.hpp
#pragma once

#define VK_NO_PROTOTYPES
#include "volk.h"
#include <SDL3/SDL_video.h>
#include <expected>
#include <print>

class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext() { shutdown(); }

    std::expected<void, std::string> initialize();
    void render();
    void shutdown();

private:
    std::expected<void, std::string> createInstance();
    std::expected<void, std::string> createSurface();
    std::expected<void, std::string> pickPhysicalDevice();
    std::expected<void, std::string> createDevice();
    std::expected<void, std::string> createSwapchain();

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

### Создание Instance с std::expected

```cpp
std::expected<void, std::string> VulkanContext::createInstance() {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "ProjectV";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.pEngineName = "ProjectV Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.apiVersion = VK_API_VERSION_1_4;  // ProjectV требует Vulkan 1.4

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
        return std::unexpected("vkCreateInstance failed");
    }

    // Критически важно: загрузка instance-функций
    volkLoadInstance(m_instance);
    std::println("VkInstance created");
    return {};
}
```

### Создание Device с std::expected

```cpp
std::expected<void, std::string> VulkanContext::createDevice() {
    // Поиск queue families и создание device
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilies.data());

    // Поиск graphics queue family
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            m_graphicsQueueFamily = i;
            break;
        }
    }

    // Поиск present queue family
    VkBool32 presentSupport = false;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surface, &presentSupport);
        if (presentSupport) {
            m_presentQueueFamily = i;
            break;
        }
    }

    // Создание device
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = m_graphicsQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
        return std::unexpected("vkCreateDevice failed");
    }

    // Критически важно: загрузка device-функций
    volkLoadDevice(m_device);

    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentQueueFamily, 0, &m_presentQueue);

    std::println("VkDevice created");
    return {};
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
#include <expected>
#include <print>

struct VMAContext {
    VmaAllocator allocator;

    static std::expected<VMAContext, std::string> create(
        VkInstance instance,
        VkPhysicalDevice physicalDevice,
        VkDevice device
    ) {
        // volkLoadInstance и volkLoadDevice уже должны быть вызваны

        VmaAllocatorCreateInfo createInfo = {};
        createInfo.instance = instance;
        createInfo.physicalDevice = physicalDevice;
        createInfo.device = device;
        createInfo.pVulkanFunctions = nullptr;  // VMA будет использовать vkGetInstanceProcAddr

        VmaAllocator allocator;
        if (vmaCreateAllocator(&createInfo, &allocator) != VK_SUCCESS) {
            return std::unexpected("vmaCreateAllocator failed");
        }

        std::println("VMA allocator created");
        return VMAContext{allocator};
    }

    ~VMAContext() {
        if (allocator) vmaDestroyAllocator(allocator);
    }

    VMAContext(const VMAContext&) = delete;
    VMAContext& operator=(const VMAContext&) = delete;
    VMAContext(VMAContext&& other) noexcept : allocator(other.allocator) {
        other.allocator = VK_NULL_HANDLE;
    }

private:
    VMAContext(VmaAllocator a) : allocator(a) {}
};
```

**Важно:** VMA будет использовать `vkGetInstanceProcAddr` и `vkGetDeviceProcAddr`, которые volk уже настроил для прямых
вызовов драйвера.

---

## Интеграция с SDL3

SDL3 и volk работают вместе для создания Vulkan surface:

```cpp
std::expected<void, std::string> VulkanContext::createSurface() {
    if (!SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_surface)) {
        return std::unexpected("SDL_Vulkan_CreateSurface failed");
    }
    return {};
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
#include <print>

// RAII-обёртка для Tracy Vulkan контекста
struct TracyVulkanContext {
    TracyVkCtx ctx;

    TracyVulkanContext(
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        VkQueue queue,
        VkCommandBuffer commandBuffer
    ) : ctx(TracyVkContext(
        physicalDevice,
        device,
        queue,
        commandBuffer,
        volkGetInstanceProcAddr,  // volk предоставляет эти функции
        volkGetDeviceProcAddr
    )) {
        std::println("Tracy Vulkan context created");
    }

    ~TracyVulkanContext() {
        TracyVkDestroy(ctx);
    }

    TracyVulkanContext(const TracyVulkanContext&) = delete;
    TracyVulkanContext& operator=(const TracyVulkanContext&) = delete;
};

// Использование в коде рендеринга
{
    TracyVkZone(tracyCtx.ctx, commandBuffer, "RenderVoxels");
    vkCmdDraw(...);
}
```

---

## Злые хаки ProjectV для volk

### 1. Принудительная загрузка расширений

```cpp
// Загружаем все расширения, даже если они не нужны прямо сейчас
// Это предотвращает stutter при первом использовании
void preloadAllExtensions(VkDevice device) {
    const char* extensions[] = {
        "vkCmdDrawMeshTasksEXT",
        "vkCmdTraceRaysKHR",
        "vkCmdSetFragmentShadingRateKHR",
        "vkCmdBeginRendering",
        "vkCmdEndRendering",
    };

    for (const char* name : extensions) {
        // Просто получаем указатель - загрузится при первом использовании
        [[maybe_unused]] auto func = volkGetDeviceProcAddr(device, name);
    }
}
```

### 2. Table-based интерфейс для многопоточности

```cpp
// Создаём thread-safe таблицу функций для каждого потока
struct ThreadLocalVulkanTable {
    alignas(64) VolkDeviceTable deviceTable;  // Выравнивание для избежания false sharing

    void initialize(VkDevice device) {
        volkLoadDeviceTable(&deviceTable, device);
    }

    // Thread-safe вызовы
    void cmdDraw(VkCommandBuffer cmd, uint32_t vertexCount, uint32_t instanceCount,
                 uint32_t firstVertex, uint32_t firstInstance) {
        deviceTable.vkCmdDraw(cmd, vertexCount, instanceCount, firstVertex, firstInstance);
    }
};

// Каждый worker thread получает свою копию
thread_local ThreadLocalVulkanTable g_vulkanTable;
```

### 3. Hot-path оптимизация для воксельного рендеринга

```cpp
// Кэшируем указатели на часто используемые функции
struct HotPathVulkanFunctions {
    PFN_vkCmdDraw vkCmdDraw = nullptr;
    PFN_vkCmdDrawIndexed vkCmdDrawIndexed = nullptr;
    PFN_vkCmdDispatch vkCmdDispatch = nullptr;

    void initialize(VkDevice device) {
        vkCmdDraw = (PFN_vkCmdDraw)volkGetDeviceProcAddr(device, "vkCmdDraw");
        vkCmdDrawIndexed = (PFN_vkCmdDrawIndexed)volkGetDeviceProcAddr(device, "vkCmdDrawIndexed");
        vkCmdDispatch = (PFN_vkCmdDispatch)volkGetDeviceProcAddr(device, "vkCmdDispatch");
    }

    // Прямой вызов без поиска в таблице
    void drawVoxelChunk(VkCommandBuffer cmd, uint32_t vertexCount) {
        vkCmdDraw(cmd, vertexCount, 1, 0, 0);
    }
};
```

---

## Обработка ошибок с C++26

### Проверка поддержки Vulkan

```cpp
std::expected<void, std::string> checkVulkanSupport() {
    // Проверка наличия Vulkan
    if (volkInitialize() != VK_SUCCESS) {
        return std::unexpected("Vulkan not installed. Please install Vulkan SDK from vulkan.lunarg.com");
    }

    // Проверка версии (ProjectV требует Vulkan 1.4)
    uint32_t version = volkGetInstanceVersion();
    if (version < VK_API_VERSION_1_4) {
        return std::unexpected(std::format(
            "Vulkan 1.4+ required. Current version: {}.{}.{}",
            VK_VERSION_MAJOR(version),
            VK_VERSION_MINOR(version),
            VK_VERSION_PATCH(version)
        ));
    }

    return {};
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
5. **Проверка версии** - используйте `volkGetInstanceVersion()` для проверки поддержки Vulkan 1.4+

Эти правила обеспечивают корректную работу volk в ProjectV и максимальную производительность Vulkan вызовов для
воксельного рендеринга.

---

## Заключение

Volk — это не просто "ещё одна библиотека Vulkan". Это философский выбор ProjectV: **никаких посредников между кодом и
железом**.

В контексте воксельного движка, где каждый кадр — это тысячи draw calls, volk даёт нам:

- **Прямой доступ** к драйверу, минуя системный loader
- **Предсказуемую производительность** без dispatch overhead
- **Гибкость** для table-based многопоточности
- **Интеграцию** с VMA, Tracy и SDL3

Помните: в ProjectV мы не просто "используем Vulkan". Мы строим **прямой провод** к GPU, и volk — это наш паяльник.

> **Философия ProjectV:** Если что-то можно сделать быстрее — сделай. Если можно убрать посредника — убери. Если можно
> получить прямой доступ к железу — получи. Volk — это про отсутствие компромиссов.

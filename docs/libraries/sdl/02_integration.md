# SDL3: Интеграция в ProjectV

## CMake-конфигурация

### Базовое подключение

```cmake
cmake_minimum_required(VERSION 3.21)
project(ProjectV)

# SDL3 как подмодуль
add_subdirectory(external/SDL)

add_executable(${PROJECT_NAME} src/main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)
```

### Статическая линковка

Для максимальной совместимости (без внешних DLL):

```cmake
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)

add_subdirectory(external/SDL)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3-static)
```

### Платформенные зависимости

При статической линковке:

```cmake
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        imm32
        version
        winmm
        setupapi
    )
elseif(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        dl
        pthread
        rt
    )
endif()
```

### Копирование DLL (Windows)

```cmake
if(WIN32 AND SDL_SHARED)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3>"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()
```

## Callback-архитектура

### Базовый каркас приложения

```cpp
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

struct AppState {
    SDL_Window* window = nullptr;
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::println("SDL_Init failed: {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    auto* app = new AppState;
    app->window = SDL_CreateWindow(
        "ProjectV",
        1280, 720,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    if (!app->window) {
        std::println("SDL_CreateWindow failed: {}", SDL_GetError());
        delete app;
        return SDL_APP_FAILURE;
    }

    *appstate = app;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE) {
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    // Обновление логики и рендеринг
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    auto* app = static_cast<AppState*>(appstate);
    if (app) {
        if (app->window) {
            SDL_DestroyWindow(app->window);
        }
        delete app;
    }
}
```

## Vulkan + volk интеграция

### Порядок инициализации

```
1. SDL_Init(SDL_INIT_VIDEO)
2. SDL_CreateWindow с SDL_WINDOW_VULKAN
3. volkInitializeCustom через SDL_Vulkan_GetVkGetInstanceProcAddr
4. SDL_Vulkan_GetInstanceExtensions → массив расширений
5. vkCreateInstance с расширениями от SDL
6. volkLoadInstance
7. SDL_Vulkan_CreateSurface
8. Выбор PhysicalDevice, создание Device
```

### Полный пример

```cpp
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>

#define VK_NO_PROTOTYPES
#include <volk.h>
#include <vulkan/vulkan.h>

struct AppState {
    SDL_Window* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily = 0;
};

std::expected<void, std::string> init_vulkan(AppState* app) {
    // 1. Инициализация volk через SDL
    auto vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)
        SDL_Vulkan_GetVkGetInstanceProcAddr();

    if (!vkGetInstanceProcAddr) {
        return std::unexpected("SDL_Vulkan_GetVkGetInstanceProcAddr failed");
    }

    if (volkInitializeCustom(vkGetInstanceProcAddr) != VK_SUCCESS) {
        return std::unexpected("volkInitializeCustom failed");
    }

    // 2. Получение расширений instance
    Uint32 extensionCount = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

    if (!extensions) {
        return std::unexpected("SDL_Vulkan_GetInstanceExtensions failed");
    }

    // 3. Создание instance
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "ProjectV";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "ProjectV Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = extensionCount;
    instanceInfo.ppEnabledExtensionNames = extensions;

    if (vkCreateInstance(&instanceInfo, nullptr, &app->instance) != VK_SUCCESS) {
        return std::unexpected("vkCreateInstance failed");
    }

    volkLoadInstance(app->instance);

    // 4. Создание surface
    if (!SDL_Vulkan_CreateSurface(app->window, app->instance, nullptr, &app->surface)) {
        return std::unexpected("SDL_Vulkan_CreateSurface failed");
    }

    // 5. Выбор physical device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(app->instance, &deviceCount, nullptr);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(app->instance, &deviceCount, devices.data());

    // Выбираем первый discrete GPU
    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            app->physicalDevice = dev;
            break;
        }
    }
    if (app->physicalDevice == VK_NULL_HANDLE) {
        app->physicalDevice = devices[0];
    }

    // 6. Получение queue family
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(app->physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(app->physicalDevice, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            // Проверка presentation support
            if (SDL_Vulkan_GetPresentationSupport(app->instance, app->physicalDevice, i)) {
                app->graphicsQueueFamily = i;
                break;
            }
        }
    }

    // 7. Создание device
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = {};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = app->graphicsQueueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceVulkan13Features features13 = {};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = true;

    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.pNext = &features13;

    if (vkCreateDevice(app->physicalDevice, &deviceInfo, nullptr, &app->device) != VK_SUCCESS) {
        return std::unexpected("vkCreateDevice failed");
    }

    volkLoadDevice(app->device);

    vkGetDeviceQueue(app->device, app->graphicsQueueFamily, 0, &app->graphicsQueue);

    return {};
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return SDL_APP_FAILURE;
    }

    auto* app = new AppState;
    app->window = SDL_CreateWindow(
        "ProjectV",
        1280, 720,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    if (!app->window) {
        delete app;
        return SDL_APP_FAILURE;
    }

    auto result = init_vulkan(app);
    if (!result) {
        std::println("Vulkan init failed: {}", result.error());
        SDL_DestroyWindow(app->window);
        delete app;
        return SDL_APP_FAILURE;
    }

    *appstate = app;
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    auto* app = static_cast<AppState*>(appstate);
    if (!app) return;

    // 1. Ожидание завершения GPU
    if (app->device) {
        vkDeviceWaitIdle(app->device);
    }

    // 2. Уничтожение surface (ДО window!)
    if (app->surface) {
        SDL_Vulkan_DestroySurface(app->instance, app->surface, nullptr);
    }

    // 3. Уничтожение device и instance
    if (app->device) {
        vkDestroyDevice(app->device, nullptr);
    }
    if (app->instance) {
        vkDestroyInstance(app->instance, nullptr);
    }

    // 4. Уничтожение окна
    if (app->window) {
        SDL_DestroyWindow(app->window);
    }

    delete app;
}
```

## VMA интеграция

```cpp
#include <vma/vk_mem_alloc.h>

std::expected<void, std::string> init_vma(VkPhysicalDevice physicalDevice, VkDevice device) {
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.instance = app->instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    VmaAllocator allocator;
    if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS) {
        return std::unexpected("vmaCreateAllocator failed");
    }

    return {};
}
```

## Flecs ECS интеграция

### События как компоненты

```cpp
#include <flecs.h>

struct InputEvent {
    SDL_Event event;
};

// В AppState добавляем ECS world
struct AppState {
    SDL_Window* window = nullptr;
    flecs::world* ecs = nullptr;
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    auto* app = new AppState;

    SDL_Init(SDL_INIT_VIDEO);
    app->window = SDL_CreateWindow("ProjectV", 1280, 720,
                                    SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    // Инициализация ECS
    app->ecs = new flecs::world();
    init_ecs_systems(app->ecs);

    *appstate = app;
    return SDL_APP_CONTINUE;
}

void init_ecs_systems(flecs::world* ecs) {
    // Система обработки ввода
    ecs->system<InputEvent>("ProcessInput")
        .kind(flecs::OnUpdate)
        .each([](InputEvent& input) {
            const auto& e = input.event;
            if (e.type == SDL_EVENT_KEY_DOWN) {
                // Обработка нажатия клавиши
            }
        });
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* app = static_cast<AppState*>(appstate);

    // Отправляем событие в ECS как компонент
    app->ecs->entity()
        .set<InputEvent>({*event});

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* app = static_cast<AppState*>(appstate);

    // Шаг ECS
    app->ecs->progress();

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    auto* app = static_cast<AppState*>(appstate);

    delete app->ecs;
    if (app->window) SDL_DestroyWindow(app->window);
    delete app;
}
```

## Tracy профилирование

```cpp
#include <tracy/Tracy.hpp>

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    ZoneScopedN("SDL_Event");

    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        TracyPlot("MouseDelta",
                  static_cast<int64_t>(std::abs(event->motion.xrel) +
                                       std::abs(event->motion.yrel)));
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    ZoneScopedN("Frame");

    {
        ZoneScopedN("Update");
        update_logic();
    }

    {
        ZoneScopedN("Render");
        render_frame();
    }

    FrameMark;
    return SDL_APP_CONTINUE;
}
```

## Обработка resize

```cpp
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        auto* app = static_cast<AppState*>(appstate);

        uint32_t width = event->window.data1;
        uint32_t height = event->window.data2;

        // Debounce: игнорировать мелкие изменения
        if (std::abs(static_cast<int>(width) - static_cast<int>(app->lastWidth)) > 5 ||
            std::abs(static_cast<int>(height) - static_cast<int>(app->lastHeight)) > 5) {

            app->pendingResize = true;
            app->newWidth = width;
            app->newHeight = height;
        }
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* app = static_cast<AppState*>(appstate);

    if (app->pendingResize) {
        vkDeviceWaitIdle(app->device);
        recreate_swapchain(app->newWidth, app->newHeight);
        app->lastWidth = app->newWidth;
        app->lastHeight = app->newHeight;
        app->pendingResize = false;
    }

    render_frame();
    return SDL_APP_CONTINUE;
}
```

## Чеклист интеграции

- [ ] SDL_MAIN_USE_CALLBACKS определён до включения заголовков
- [ ] volk инициализирован через SDL_Vulkan_GetVkGetInstanceProcAddr
- [ ] Расширения instance получены от SDL_Vulkan_GetInstanceExtensions
- [ ] Surface создана через SDL_Vulkan_CreateSurface
- [ ] SDL_Vulkan_DestroySurface вызывается до SDL_DestroyWindow
- [ ] Flecs ECS инициализирован в SDL_AppInit
- [ ] Tracy профилирование интегрировано в callbacks
- [ ] Обработка SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED для resize

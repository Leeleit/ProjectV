# Интеграция SDL3 в ProjectV

**🔴 Уровень 3: Продвинутый**

Специфичные паттерны интеграции SDL3 в воксельный движок ProjectV.

---

## Архитектура

ProjectV использует SDL3 с callback архитектурой для интеграции с flecs ECS и Vulkan рендерером.

> **См. также:** [Многопоточность в C++](../../guides/cpp/08_multithreading.md) — паттерны thread-safe очередей и
> синхронизации.

### Выбранные решения

| Компонент        | Решение                  | Обоснование                                |
|------------------|--------------------------|--------------------------------------------|
| Event Loop       | `SDL_MAIN_USE_CALLBACKS` | Кроссплатформенность, интеграция с flecs   |
| Графический API  | Vulkan                   | Производительность воксельного рендеринга  |
| Загрузчик Vulkan | volk                     | Статическая загрузка без системного loader |
| ECS              | flecs                    | События как компоненты                     |

---

## Интеграция с flecs

### Паттерн: SDL события в ECS

```cpp
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <flecs.h>

struct AppState {
    SDL_Window* window = nullptr;
    flecs::world* ecs = nullptr;
};

// Компонент-событие для ECS
struct SDLInputEvent {
    SDL_Event event;
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    auto* app = new AppState;

    // Инициализация SDL
    SDL_Init(SDL_INIT_VIDEO);
    app->window = SDL_CreateWindow("ProjectV", 1280, 720,
                                    SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    // Инициализация ECS
    app->ecs = new flecs::world;
    init_ecs_systems(app->ecs);

    *appstate = app;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* app = static_cast<AppState*>(appstate);

    // Отправка события в ECS
    app->ecs->entity()
        .set<SDLInputEvent>({*event})
        .add<flecs::Union>();

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* app = static_cast<AppState*>(appstate);

    // Шаг ECS: обработка событий, обновление логики, рендеринг
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

### ECS системы обработки ввода

```cpp
void init_ecs_systems(flecs::world* ecs) {
    // Система обработки клавиатуры
    ecs->system<SDLInputEvent>("KeyboardInput")
        .kind(flecs::OnUpdate)
        .each([](SDLInputEvent& input) {
            if (input.event.type == SDL_EVENT_KEY_DOWN) {
                // Обработка нажатия
            }
        });

    // Система обработки мыши
    ecs->system<SDLInputEvent>("MouseInput")
        .kind(flecs::OnUpdate)
        .each([](SDLInputEvent& input) {
            if (input.event.type == SDL_EVENT_MOUSE_MOTION) {
                // Обработка движения
            }
        });
}
```

---

## Интеграция с Vulkan и volk

### Порядок инициализации

```cpp
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    // 1. SDL
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("ProjectV", 1280, 720,
                                           SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    // 2. volk через SDL
    auto vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)
        SDL_Vulkan_GetVkGetInstanceProcAddr();
    volkInitializeCustom(vkGetInstanceProcAddr);

    // 3. Vulkan instance с расширениями от SDL
    Uint32 extensionCount = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

    VkInstanceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.enabledExtensionCount = extensionCount;
    info.ppEnabledExtensionNames = extensions;

    VkInstance instance;
    vkCreateInstance(&info, nullptr, &instance);
    volkLoadInstance(instance);

    // 4. Surface
    VkSurfaceKHR surface;
    SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface);

    // 5. Device, VMA, swapchain...

    return SDL_APP_CONTINUE;
}
```

---

## Интеграция с VMA

```cpp
void init_vma(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice) {
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.instance = instance;
    allocatorInfo.device = device;
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    VmaAllocator allocator;
    vmaCreateAllocator(&allocatorInfo, &allocator);
}
```

---

## Интеграция с Tracy

### Профилирование callbacks

```cpp
#include <tracy/Tracy.hpp>

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    ZoneScopedN("SDL_Event");

    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        TracyPlot("MouseDelta",
                  abs(event->motion.xrel) + abs(event->motion.yrel));
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

---

## CMake конфигурация ProjectV

```cmake
# SDL3
add_subdirectory(external/SDL)

# volk, VMA, flecs, Tracy...

add_executable(ProjectV src/main.cpp)

target_link_libraries(ProjectV PRIVATE
    SDL3::SDL3
    volk::volk
    VMA::VMA
    flecs::flecs_static
    Tracy::TracyClient
)

# Копирование DLL
if(WIN32)
    add_custom_command(TARGET ProjectV POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3>"
            "$<TARGET_FILE_DIR:ProjectV>"
    )
endif()
```

---

## Примеры кода ProjectV

| Пример               | Описание           | Документация                                        |
|----------------------|--------------------|-----------------------------------------------------|
| Базовое окно         | Минимальный пример | [01_quickstart.md](01_quickstart.md)                |
| SDL + flecs          | Интеграция ECS     | [flecs/01_quickstart.md](../flecs/01_quickstart.md) |
| SDL + ImGui + Vulkan | Полный стек        | [imgui/01_quickstart.md](../imgui/01_quickstart.md) |

## Интеграция SDL3

<!-- anchor: 03_integration -->


Настройка проекта, сборка и подключение SDL3.

---

## CMake

### Базовая конфигурация

```cmake
cmake_minimum_required(VERSION 3.16)
project(YourApp)

# Подключение SDL3 как подмодуля Git
add_subdirectory(external/SDL)

add_executable(YourApp src/main.cpp)
target_link_libraries(YourApp PRIVATE SDL3::SDL3)
```

### Статическая линковка

```cmake
# Принудительная статическая линковка
set(SDL_STATIC ON CACHE BOOL "Static linking for SDL3" FORCE)
set(SDL_SHARED OFF CACHE BOOL "Disable shared library" FORCE)

add_subdirectory(external/SDL)

add_executable(YourApp src/main.cpp)
target_link_libraries(YourApp PRIVATE SDL3::SDL3-static)
```

### Платформенные зависимости

При статической линковке может потребоваться явно указать системные библиотеки:

```cmake
if(WIN32)
    target_link_libraries(YourApp PRIVATE
        imm32 version winmm setupapi
    )
elseif(APPLE)
    target_link_libraries(YourApp PRIVATE
        "-framework CoreFoundation"
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework Carbon"
        "-framework IOKit"
        "-framework CoreVideo"
    )
elseif(UNIX AND NOT APPLE)
    target_link_libraries(YourApp PRIVATE dl m pthread rt)
endif()
```

### Копирование DLL (Windows)

```cmake
if(WIN32)
    add_custom_command(TARGET YourApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3>"
            "$<TARGET_FILE_DIR:YourApp>"
    )
endif()
```

---

## Порядок включения заголовков

### Базовый вариант

```cpp
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>  // Для SDL_MAIN_USE_CALLBACKS
```

### С Vulkan

```cpp
#define VK_NO_PROTOTYPES  // Если используется volk

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>  // Vulkan-специфичные функции SDL
#include "volk.h"

#include <vulkan/vulkan.h>
```

### С OpenGL

```cpp
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
```

---

## Инициализация

### Callback архитектура

```cpp
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

struct AppState {
    SDL_Window* window = nullptr;
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    // Метаданные приложения (опционально)
    SDL_SetAppMetadata("YourApp", "1.0.0", "com.example.yourapp");

    // Инициализация подсистем
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Создание окна
    auto* app = new AppState;
    app->window = SDL_CreateWindow(
        "Your Application",
        1280, 720,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN
    );

    if (!app->window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
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
    // Обновление и рендеринг
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
    // SDL_Quit() вызывается автоматически
}
```

### Классический main()

```cpp
#include <SDL3/SDL.h>

int main(int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Your App",
        1280, 720,
        SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        SDL_Quit();
        return 1;
    }

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event->type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        // Обновление и рендеринг
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
```

---

## Очистка ресурсов

### Порядок для Vulkan

```cpp
void cleanup(SDL_Window* window, VkInstance instance, VkSurfaceKHR surface) {
    // 1. Ожидание завершения GPU
    vkDeviceWaitIdle(device);

    // 2. Уничтожение Vulkan ресурсов
    destroy_swapchain();

    // 3. Уничтожение поверхности (до окна!)
    if (surface) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }

    // 4. Уничтожение device и instance
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    // 5. Уничтожение окна SDL
    if (window) {
        SDL_DestroyWindow(window);
    }
}
```

### Callback архитектура

```cpp
void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    auto* app = static_cast<AppState*>(appstate);

    // 1. Очистка пользовательских ресурсов
    cleanup_vulkan_resources(app);

    // 2. Уничтожение окна
    if (app && app->window) {
        SDL_DestroyWindow(app->window);
    }

    // 3. Освобождение состояния
    delete app;

    // SDL_Quit() вызывается автоматически
}
```

---

## OpenGL интеграция

### Создание контекста

```cpp
SDL_Window* init_opengl() {
    // Атрибуты OpenGL
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow(
        "OpenGL App",
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    if (!window) return nullptr;

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        SDL_DestroyWindow(window);
        return nullptr;
    }

    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(1);  // VSync

    return window;
}
```

### Основной цикл

```cpp
void render_loop(SDL_Window* window) {
    bool running = true;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // Рендеринг
        SDL_GL_SwapWindow(window);
    }
}
```

---

## Множественные окна

```cpp
struct MultiWindowState {
    SDL_Window* mainWindow = nullptr;
    SDL_Window* toolWindow = nullptr;
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);

    auto* state = new MultiWindowState;

    state->mainWindow = SDL_CreateWindow(
        "Main Window", 1280, 720,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    state->toolWindow = SDL_CreateWindow(
        "Tools", 300, 600,
        SDL_WINDOW_VULKAN
    );

    *appstate = state;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* state = static_cast<MultiWindowState*>(appstate);

    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        SDL_WindowID windowID = event->window.windowID;

        if (windowID == SDL_GetWindowID(state->mainWindow)) {
            return SDL_APP_SUCCESS;  // Выход при закрытии главного окна
        } else if (windowID == SDL_GetWindowID(state->toolWindow)) {
            SDL_DestroyWindow(state->toolWindow);
            state->toolWindow = nullptr;
        }
    }

    return SDL_APP_CONTINUE;
}
```

---

## Сборка

### Структура проекта

```
YourProject/
├── external/
│   └── SDL/           # Git submodule
├── src/
│   └── main.cpp
└── CMakeLists.txt
```

### Команды сборки

```bash
# Клонирование с подмодулем
git clone --recursive https://github.com/user/YourProject.git
cd YourProject

# Или добавление подмодуля
git submodule add https://github.com/libsdl-org/SDL external/SDL

# Сборка
mkdir build && cd build
cmake ..
cmake --build .
```

---

## Рекомендации

1. **Используйте callback архитектуру** для кроссплатформенности
2. **Проверяйте возвращаемые значения** — SDL функции возвращают `true`/`false`
3. **Вызывайте `SDL_GetError()`** после ошибок для диагностики
4. **Соблюдайте порядок очистки** — Vulkan surface до SDL_DestroyWindow
5. **Не смешивайте `SDL_PollEvent` и callback архитектуру**

---

## SDL3 и Vulkan

<!-- anchor: 04_vulkan -->


Интеграция SDL3 с Vulkan API для создания поверхности рендеринга.

---

## Обзор

Vulkan не создаёт окна — он рендерит в поверхность (`VkSurfaceKHR`), которую нужно получить от оконной системы. SDL3
предоставляет:

- **Окно** с флагом `SDL_WINDOW_VULKAN`
- **Расширения instance** для platform-specific surface
- **Функцию создания поверхности** `SDL_Vulkan_CreateSurface`
- **Доступ к Vulkan loader** через `SDL_Vulkan_GetVkGetInstanceProcAddr`

---

## Порядок инициализации

```
1. SDL_Init(SDL_INIT_VIDEO)
2. SDL_CreateWindow(..., SDL_WINDOW_VULKAN)
3. SDL_Vulkan_GetInstanceExtensions() → получить расширения
4. vkCreateInstance() с расширениями от SDL
5. SDL_Vulkan_CreateSurface() → создать поверхность
6. Выбор PhysicalDevice, создание Device
```

---

## Получение расширений instance

```cpp
// Получение количества расширений
Uint32 extensionCount = 0;
const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

if (!extensions) {
    SDL_Log("SDL_Vulkan_GetInstanceExtensions failed: %s", SDL_GetError());
    return false;
}

// extensions — массив строк, не нужно освобождать
// Пример содержимого: {"VK_KHR_surface", "VK_KHR_win32_surface"}
```

### Использование при создании instance

```cpp
VkInstanceCreateInfo instanceInfo = {};
instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
instanceInfo.enabledExtensionCount = extensionCount;
instanceInfo.ppEnabledExtensionNames = extensions;
// Можно добавить дополнительные расширения (validation layers и т.д.)

VkInstance instance;
if (vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS) {
    return false;
}
```

---

## Создание поверхности

```cpp
bool create_surface(SDL_Window* window, VkInstance instance, VkSurfaceKHR* surface) {
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, surface)) {
        SDL_Log("SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
        return false;
    }
    return true;
}
```

### Параметры SDL_Vulkan_CreateSurface

| Параметр    | Описание                                |
|-------------|-----------------------------------------|
| `window`    | Окно, созданное с `SDL_WINDOW_VULKAN`   |
| `instance`  | VkInstance с расширениями от SDL        |
| `allocator` | VkAllocationCallbacks (можно `nullptr`) |
| `surface`   | Выходной VkSurfaceKHR                   |

---

## Уничтожение поверхности

```cpp
void destroy_surface(VkInstance instance, VkSurfaceKHR surface) {
    // SDL предоставляет обёртку
    SDL_Vulkan_DestroySurface(instance, surface, nullptr);

    // Или напрямую через Vulkan
    // vkDestroySurfaceKHR(instance, surface, nullptr);
}
```

Вызывать **до** `SDL_DestroyWindow`.

---

## Интеграция с volk

volk — загрузчик Vulkan функций. SDL может предоставить `vkGetInstanceProcAddr`:

```cpp
#define VK_NO_PROTOTYPES
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include "volk.h"

bool init_vulkan_with_sdl(SDL_Window* window) {
    // 1. Инициализация volk через SDL
    auto vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)
        SDL_Vulkan_GetVkGetInstanceProcAddr();

    if (!vkGetInstanceProcAddr) {
        SDL_Log("SDL_Vulkan_GetVkGetInstanceProcAddr failed");
        return false;
    }

    if (volkInitializeCustom(vkGetInstanceProcAddr) != VK_SUCCESS) {
        return false;
    }

    // 2. Получение расширений
    Uint32 extensionCount = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

    // 3. Создание instance
    VkInstanceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.enabledExtensionCount = extensionCount;
    info.ppEnabledExtensionNames = extensions;

    VkInstance instance;
    vkCreateInstance(&info, nullptr, &instance);

    // 4. Загрузка instance-level функций
    volkLoadInstance(instance);

    // 5. Создание поверхности
    VkSurfaceKHR surface;
    SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface);

    // 6. Создание device и загрузка device-level функций
    // ...
    volkLoadDevice(device);

    return true;
}
```

---

## Обработка изменения размера

### Событие SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED

```cpp
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        int width = event->window.data1;
        int height = event->window.data2;

        // Пересоздание swapchain
        recreate_swapchain(width, height);
    }
    return SDL_APP_CONTINUE;
}
```

### Получение размера окна

```cpp
void get_window_size(SDL_Window* window, int* width, int* height) {
    // Размер в пикселях (важно для HiDPI)
    SDL_GetWindowSizeInPixels(window, width, height);

    // Размер в оконных координатах (логические единицы)
    // SDL_GetWindowSize(window, width, height);
}
```

Для создания swapchain используйте `SDL_GetWindowSizeInPixels`.

---

## Проверка presentation support

```cpp
bool check_presentation_support(VkInstance instance,
                                 VkPhysicalDevice physicalDevice,
                                 uint32_t queueFamilyIndex) {
    return SDL_Vulkan_GetPresentationSupport(
        instance,
        physicalDevice,
        queueFamilyIndex
    );
}
```

Используется при выборе queue family для рендеринга.

---

## Загрузка Vulkan loader

SDL автоматически загружает Vulkan loader при создании окна с `SDL_WINDOW_VULKAN`. Для явной загрузки:

```cpp
// Явная загрузка (опционально)
if (!SDL_Vulkan_LoadLibrary(nullptr)) {
    SDL_Log("SDL_Vulkan_LoadLibrary failed: %s", SDL_GetError());
    return false;
}

// С кастомным путём
SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, "/path/to/vulkan-1.dll");
SDL_Vulkan_LoadLibrary("/path/to/vulkan-1.dll");

// Выгрузка (парная к LoadLibrary)
SDL_Vulkan_UnloadLibrary();
```

---

## Полный пример инициализации

```cpp
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>

#define VK_NO_PROTOTYPES
#include "volk.h"
#include <vulkan/vulkan.h>

struct AppState {
    SDL_Window* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    // 1. SDL инициализация
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return SDL_APP_FAILURE;
    }

    // 2. Создание окна
    auto* app = new AppState;
    app->window = SDL_CreateWindow(
        "Vulkan App",
        1280, 720,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );
    if (!app->window) {
        delete app;
        return SDL_APP_FAILURE;
    }

    // 3. Инициализация volk через SDL
    auto vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)
        SDL_Vulkan_GetVkGetInstanceProcAddr();
    if (volkInitializeCustom(vkGetInstanceProcAddr) != VK_SUCCESS) {
        SDL_DestroyWindow(app->window);
        delete app;
        return SDL_APP_FAILURE;
    }

    // 4. Получение расширений
    Uint32 extensionCount = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

    // 5. Создание instance
    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.enabledExtensionCount = extensionCount;
    instanceInfo.ppEnabledExtensionNames = extensions;

    if (vkCreateInstance(&instanceInfo, nullptr, &app->instance) != VK_SUCCESS) {
        SDL_DestroyWindow(app->window);
        delete app;
        return SDL_APP_FAILURE;
    }
    volkLoadInstance(app->instance);

    // 6. Создание поверхности
    if (!SDL_Vulkan_CreateSurface(app->window, app->instance, nullptr, &app->surface)) {
        vkDestroyInstance(app->instance, nullptr);
        SDL_DestroyWindow(app->window);
        delete app;
        return SDL_APP_FAILURE;
    }

    // 7. Создание device (опущено для краткости)
    // ...

    *appstate = app;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        int w = event->window.data1;
        int h = event->window.data2;
        // Пересоздание swapchain
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    // Рендеринг
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    auto* app = static_cast<AppState*>(appstate);
    if (app) {
        if (app->device) vkDestroyDevice(app->device, nullptr);
        if (app->surface) SDL_Vulkan_DestroySurface(app->instance, app->surface, nullptr);
        if (app->instance) vkDestroyInstance(app->instance, nullptr);
        if (app->window) SDL_DestroyWindow(app->window);
        delete app;
    }
}
```

---

## Hints

| Hint                      | Описание                         | Когда задавать                                |
|---------------------------|----------------------------------|-----------------------------------------------|
| `SDL_HINT_VULKAN_LIBRARY` | Путь к Vulkan loader             | До `SDL_Vulkan_LoadLibrary` или создания окна |
| `SDL_HINT_VULKAN_DISPLAY` | Индекс дисплея (строка "0", "1") | До `SDL_Vulkan_CreateSurface`                 |

```cpp
SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, "vulkan-1.dll");
SDL_SetHint(SDL_HINT_VULKAN_DISPLAY, "1");
```

---

## Обработка ошибок

```cpp
// Типичные ошибки SDL_Vulkan_CreateSurface:

// 1. Окно без SDL_WINDOW_VULKAN
// Решение: добавить флаг при создании окна

// 2. Instance без расширений от SDL
// Решение: использовать SDL_Vulkan_GetInstanceExtensions

// 3. Vulkan loader не загружен
// Решение: SDL загружает его автоматически при SDL_WINDOW_VULKAN
//          или явно через SDL_Vulkan_LoadLibrary

---

## Интеграция SDL3 в ProjectV

<!-- anchor: 11_projectv-integration -->


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

## Паттерны ProjectV для SDL3

<!-- anchor: 12_projectv-patterns -->


Специфичные паттерны и оптимизации для воксельного движка ProjectV.

---

## Multi-window редактор вокселей

### Менеджер окон

```cpp
class VoxelEditorWindowManager {
    struct WindowData {
        SDL_Window* window = nullptr;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    WindowData mainViewport;
    WindowData toolsPanel;
    WindowData propertiesPanel;

public:
    bool init(VkInstance instance) {
        // Main viewport
        mainViewport.window = SDL_CreateWindow(
            "Voxel Viewport", 1280, 720,
            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
        );
        create_surface(instance, &mainViewport);

        // Tools panel
        toolsPanel.window = SDL_CreateWindow(
            "Tools", 300, 800,
            SDL_WINDOW_VULKAN
        );
        create_surface(instance, &toolsPanel);

        return true;
    }

    void handleClose(SDL_WindowID windowID, VkInstance instance) {
        if (windowID == SDL_GetWindowID(mainViewport.window)) {
            // Главное окно = выход
            return;  // Вызовет SDL_APP_SUCCESS
        }

        // Вспомогательные окна
        if (windowID == SDL_GetWindowID(toolsPanel.window)) {
            destroy_window(instance, &toolsPanel);
        }
    }

    void destroyAll(VkInstance instance) {
        destroy_window(instance, &mainViewport);
        destroy_window(instance, &toolsPanel);
        destroy_window(instance, &propertiesPanel);
    }

private:
    void create_surface(VkInstance instance, WindowData* data) {
        SDL_Vulkan_CreateSurface(data->window, instance, nullptr, &data->surface);
    }

    void destroy_window(VkInstance instance, WindowData* data) {
        if (data->swapchain) {
            vkDestroySwapchainKHR(/* device */, data->swapchain, nullptr);
        }
        if (data->surface) {
            SDL_Vulkan_DestroySurface(instance, data->surface, nullptr);
        }
        if (data->window) {
            SDL_DestroyWindow(data->window);
        }
        data->window = nullptr;
        data->surface = VK_NULL_HANDLE;
        data->swapchain = VK_NULL_HANDLE;
    }
};
```

---

## Оптимизация ввода для редактирования вокселей

### Raw Input для кисти

```cpp
void configure_brush_input() {
    // Отключение системного сглаживания и ускорения
    if (SDL_HasRawMouseMotion()) {
        SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
        SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SCALING, "0");
    }
}

// Включение при начале рисования
void on_brush_start() {
    SDL_SetRelativeMouseMode(SDL_TRUE);
}

// Выключение при завершении
void on_brush_end() {
    SDL_SetRelativeMouseMode(SDL_FALSE);
}
```

### Немедленная обработка для preview

```cpp
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* state = static_cast<AppState*>(appstate);

    // Немедленная обработка для кисти
    if (event->type == SDL_EVENT_MOUSE_MOTION && state->brushActive) {
        // Direct path: событие -> preview без задержки на ECS
        update_brush_preview(event->motion.x, event->motion.y);
    }

    // Остальные события через ECS
    else if (event->type == SDL_EVENT_KEY_DOWN ||
             event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        enqueue_to_ecs(state->ecs, event);
    }

    return SDL_APP_CONTINUE;
}
```

---

## Асинхронное обновление вокселей

### Фоновый поток для вычислений

```cpp
struct VoxelUpdateSystem {
    std::atomic<bool> running{true};
    std::thread updateThread;
    std::mutex queueMutex;
    std::vector<VoxelChunk*> pendingChunks;

    void start() {
        updateThread = std::thread([this]() {
            while (running) {
                VoxelChunk* chunk = nullptr;
                {
                    std::lock_guard lock(queueMutex);
                    if (!pendingChunks.empty()) {
                        chunk = pendingChunks.back();
                        pendingChunks.pop_back();
                    }
                }

                if (chunk) {
                    compute_voxel_mesh(chunk);
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        });
    }

    void enqueue(VoxelChunk* chunk) {
        std::lock_guard lock(queueMutex);
        pendingChunks.push_back(chunk);
    }

    void stop() {
        running = false;
        if (updateThread.joinable()) {
            updateThread.join();
        }
    }
};
```

---

## Потоковая архитектура

```
┌───────────────┐     ┌───────────────┐     ┌───────────────┐
│  SDL Thread   │     │  ECS Thread   │     │ Vulkan Thread │
│  (main)       │     │  (logic)      │     │ (render)      │
├───────────────┤     ├───────────────┤     ├───────────────┤
│ SDL_AppEvent  │────▶│ Event Queue   │────▶│ Render Queue  │
│ SDL_AppIterate│     │ flecs::progress│    │ vkQueueSubmit │
└───────────────┘     └───────────────┘     └───────────────┘
        │                                            │
        └────────────────────────────────────────────┘
                    Low-latency path (brush)
```

---

## DOD-оптимизации

> **Подробнее о DOD:** в standards/cpp и philosophy/02_paradigms — SoA/AoS, cache locality, batch
> processing.

### Пакетная обработка событий

```cpp
struct InputBatch {
    struct MouseMove {
        int32_t x, y;
        int32_t dx, dy;
    };

    std::vector<MouseMove> mouseMoves;
    uint32_t keyStates[16] = {0};  // Bitmask

    void addEvent(const SDL_Event& event) {
        switch (event.type) {
            case SDL_EVENT_MOUSE_MOTION:
                mouseMoves.push_back({
                    event.motion.x, event.motion.y,
                    event.motion.xrel, event.motion.yrel
                });
                break;
            case SDL_EVENT_KEY_DOWN:
                keyStates[event.key.scancode / 32] |= (1u << (event.key.scancode % 32));
                break;
        }
    }

    void clear() {
        mouseMoves.clear();
        memset(keyStates, 0, sizeof(keyStates));
    }
};
```

### Cache-friendly обработка

```cpp
// SoA вместо AoS для воксельных данных
struct VoxelData {
    std::vector<uint8_t> types;      // Тип вокселя
    std::vector<uint8_t> colors;     // Цвет (4 канала)
    std::vector<uint8_t> flags;      // Флаги
    // Плотная упаковка для cache locality
};

void process_voxels_dod(VoxelData& data) {
    // Линейный проход с минимальными cache miss
    for (size_t i = 0; i < data.types.size(); i++) {
        if (data.types[i] == VOXEL_AIR) continue;
        process_voxel(data.types[i], data.colors.data() + i * 4);
    }
}
```

---

## Профилирование с Tracy

### Детальное профилирование кадра

```cpp
SDL_AppResult SDL_AppIterate(void* appstate) {
    ZoneScopedN("Frame");

    {
        ZoneScopedN("Input");
        process_input_batch();
    }

    {
        ZoneScopedN("VoxelUpdate");
        update_voxel_chunks();
    }

    {
        ZoneScopedN("ECS");
        ecs->progress();
    }

    {
        ZoneScopedN("Render");
        render_frame();
    }

    {
        ZoneScopedN("Present");
        present_frame();
    }

    TracyPlot("VoxelCount", voxel_count);
    TracyPlot("ChunkCount", chunk_count);
    TracyPlot("DrawCalls", draw_calls);

    FrameMark;
    return SDL_APP_CONTINUE;
}
```

---

## Интеграция с ImGui

### ImGui для debug UI

```cpp
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

void init_imgui(SDL_Window* window, VkInstance instance, VkDevice device) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui_ImplSDL3_InitForVulkan(window);
    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = instance;
    initInfo.Device = device;
    // ...
    ImGui_ImplVulkan_Init(&initInfo);
}

void render_debug_ui() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Debug info
    ImGui::Begin("Debug");
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Voxels: %zu", voxel_count);
    ImGui::End();

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}
```

---

## Обработка resize

### Пересоздание swapchain с минимальной задержкой

```cpp
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        auto* state = static_cast<AppState*>(appstate);

        uint32_t width = event->window.data1;
        uint32_t height = event->window.data2;

        // Debounce: игнорировать мелкие изменения
        if (abs((int)width - (int)state->lastWidth) > 5 ||
            abs((int)height - (int)state->lastHeight) > 5) {

            state->pendingResize = true;
            state->newWidth = width;
            state->newHeight = height;
        }
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* state = static_cast<AppState*>(appstate);

    // Обработка resize в начале кадра
    if (state->pendingResize) {
        vkDeviceWaitIdle(state->device);
        recreate_swapchain(state->newWidth, state->newHeight);
        state->lastWidth = state->newWidth;
        state->lastHeight = state->newHeight;
        state->pendingResize = false;
    }

    // Основной рендеринг
    render_frame();

    return SDL_APP_CONTINUE;
}
```

---

## Чеклист интеграции ProjectV

- [ ] SDL_MAIN_USE_CALLBACKS включён
- [ ] volk инициализирован через SDL_Vulkan_GetVkGetInstanceProcAddr
- [ ] Расширения instance получены от SDL_Vulkan_GetInstanceExtensions
- [ ] Surface создана через SDL_Vulkan_CreateSurface
- [ ] Интеграция с flecs ECS
- [ ] Профилирование Tracy
- [ ] Raw Input для кисти редактора
- [ ] Асинхронное обновление вокселей
- [ ] Multi-window поддержка (опционально)
- [ ] ImGui для debug UI

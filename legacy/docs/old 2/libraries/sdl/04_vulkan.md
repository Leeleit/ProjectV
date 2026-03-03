# SDL3 и Vulkan

**🟡 Уровень 2: Средний**

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

# Интеграция SDL3

**🟢 Уровень 1: Начинающий**

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
# Быстрый старт SDL3

**🟢 Уровень 1: Начинающий**

Минимальный пример с использованием `SDL_MAIN_USE_CALLBACKS`.

## Шаг 1: CMake

```cmake
add_subdirectory(external/SDL)

add_executable(YourApp src/main.cpp)
target_link_libraries(YourApp PRIVATE SDL3::SDL3)
```

## Шаг 2: main.cpp

```cpp
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

struct AppState {
    SDL_Window* window = nullptr;
};

// Инициализация
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    SDL_SetAppMetadata("YourApp", "1.0", "com.example.yourapp");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return SDL_APP_FAILURE;
    }

    auto* app = new AppState;
    app->window = SDL_CreateWindow("Hello SDL3", 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    
    if (!app->window) {
        return SDL_APP_FAILURE;
    }

    *appstate = app;
    return SDL_APP_CONTINUE;
}

// События
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

// Кадр (Update + Render)
SDL_AppResult SDL_AppIterate(void* appstate) {
    // Рендеринг...
    return SDL_APP_CONTINUE;
}

// Завершение
void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    auto* app = static_cast<AppState*>(appstate);
    if (app) {
        SDL_DestroyWindow(app->window);
        delete app;
    }
}
```

## Дальнейшие шаги

1. **Vulkan**: Создайте поверхность через `SDL_Vulkan_CreateSurface`.
2. **Input**: Обрабатывайте `SDL_EVENT_KEY_DOWN`, `SDL_EVENT_MOUSE_MOTION` в `SDL_AppEvent`.
3. **ImGui**: Подключите интерфейс (см. [imgui](../imgui/README.md)).

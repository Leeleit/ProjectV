# Быстрый старт SDL3

**🟢 Уровень 1: Начинающий**

Минимальный пример приложения с использованием callback архитектуры.

---

## Шаг 1: CMake

```cmake
cmake_minimum_required(VERSION 3.16)
project(YourApp)

# Подключение SDL3 как подмодуля
add_subdirectory(external/SDL)

# Создание исполняемого файла
add_executable(YourApp src/main.cpp)

# Линковка с SDL3
target_link_libraries(YourApp PRIVATE SDL3::SDL3)
```

---

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
    // Метаданные приложения
    SDL_SetAppMetadata("YourApp", "1.0", "com.example.yourapp");

    // Инициализация видеоподсистемы
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Создание состояния приложения
    auto* app = new AppState;
    
    // Создание окна с поддержкой Vulkan
    app->window = SDL_CreateWindow(
        "Hello SDL3", 
        800, 600, 
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    if (!app->window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        delete app;
        return SDL_APP_FAILURE;
    }

    *appstate = app;
    return SDL_APP_CONTINUE;
}

// Обработка событий
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
            
        case SDL_EVENT_KEY_DOWN:
            if (event->key.key == SDLK_ESCAPE) {
                return SDL_APP_SUCCESS;
            }
            break;
            
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            return SDL_APP_SUCCESS;
    }
    
    return SDL_APP_CONTINUE;
}

// Кадр (Update + Render)
SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* app = static_cast<AppState*>(appstate);
    
    // Здесь: обновление логики и рендеринг
    
    return SDL_APP_CONTINUE;
}

// Завершение
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

---

## Сборка и запуск

```bash
mkdir build && cd build
cmake ..
cmake --build .
./YourApp
```

---

## Структура проекта

```
YourProject/
├── external/
│   └── SDL/           # Git submodule
├── src/
│   └── main.cpp
└── CMakeLists.txt
```

---

## Ключевые моменты

1. **`SDL_MAIN_USE_CALLBACKS`** — определяется до включения заголовков
2. **`appstate`** — указатель на состояние приложения, передаётся во все callbacks
3. **`SDL_APP_CONTINUE`** — приложение продолжает работу
4. **`SDL_APP_SUCCESS`** — нормальное завершение
5. **`SDL_APP_FAILURE`** — завершение с ошибкой
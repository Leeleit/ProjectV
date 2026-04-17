# Сценарии использования SDL3

**🟡 Уровень 2: Средний**

Типовые архитектурные паттерны для различных типов приложений.

---

## Игры

### Игровая петля с callbacks

```cpp
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

struct GameState {
    SDL_Window* window = nullptr;
    bool paused = false;
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD);

    auto* game = new GameState;
    game->window = SDL_CreateWindow("Game", 1280, 720,
                                     SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    *appstate = game;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* game = static_cast<GameState*>(appstate);

    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
        case SDL_EVENT_KEY_DOWN:
            if (event->key.key == SDLK_ESCAPE) {
                game->paused = !game->paused;
            }
            break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* game = static_cast<GameState*>(appstate);

    if (!game->paused) {
        update_game();
        render_frame();
    }

    return SDL_APP_CONTINUE;
}
```

### Классический main loop

```cpp
#include <SDL3/SDL.h>

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);
    SDL_Window* window = SDL_CreateWindow("Game", 1280, 720, SDL_WINDOW_VULKAN);

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event->type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        update_game();
        render_frame();
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
```

---

## Графические редакторы

### Multi-window архитектура

```cpp
struct EditorState {
    SDL_Window* viewport = nullptr;      // Главное окно с 3D сценой
    SDL_Window* tools = nullptr;         // Панель инструментов
    SDL_Window* properties = nullptr;    // Панель свойств
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);

    auto* editor = new EditorState;

    editor->viewport = SDL_CreateWindow(
        "Viewport", 1280, 720,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    editor->tools = SDL_CreateWindow(
        "Tools", 300, 600,
        SDL_WINDOW_VULKAN
    );

    editor->properties = SDL_CreateWindow(
        "Properties", 400, 600,
        SDL_WINDOW_VULKAN
    );

    *appstate = editor;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* editor = static_cast<EditorState*>(appstate);

    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        SDL_WindowID id = event->window.windowID;

        if (id == SDL_GetWindowID(editor->viewport)) {
            return SDL_APP_SUCCESS;  // Закрытие главного окна = выход
        }

        // Закрытие вспомогательных окон
        if (id == SDL_GetWindowID(editor->tools)) {
            SDL_DestroyWindow(editor->tools);
            editor->tools = nullptr;
        }
        else if (id == SDL_GetWindowID(editor->properties)) {
            SDL_DestroyWindow(editor->properties);
            editor->properties = nullptr;
        }
    }

    return SDL_APP_CONTINUE;
}
```

### Drag & Drop файлов

```cpp
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    if (event->type == SDL_EVENT_DROP_FILE) {
        const char* filePath = event->drop.file;
        load_file(filePath);
        SDL_free((void*)filePath);
    }

    if (event->type == SDL_EVENT_DROP_COMPLETE) {
        // Завершение операции drop
    }

    return SDL_APP_CONTINUE;
}
```

---

## Медиаплееры

### Fullscreen переключение

```cpp
void toggle_fullscreen(SDL_Window* window) {
    bool isFullscreen = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN;

    if (isFullscreen) {
        SDL_SetWindowFullscreen(window, false);
        SDL_SetWindowSize(window, 1280, 720);
    } else {
        SDL_SetWindowFullscreen(window, true);
    }
}
```

### Borderless fullscreen

```cpp
void set_borderless_fullscreen(SDL_Window* window) {
    SDL_SetWindowBordered(window, false);

    SDL_DisplayMode mode;
    SDL_GetCurrentDisplayMode(0, &mode);

    SDL_SetWindowPosition(window, 0, 0);
    SDL_SetWindowSize(window, mode.w, mode.h);
}
```

---

## Научная визуализация

### Режим реального времени

```cpp
SDL_AppResult SDL_AppIterate(void* appstate) {
    update_simulation();
    render_visualization();

    // Ограничение FPS для стабильности
    SDL_Delay(16);  // ~60 FPS

    return SDL_APP_CONTINUE;
}
```

### Оффлайн рендеринг

```cpp
void offline_render(int totalFrames) {
    for (int frame = 0; frame < totalFrames; frame++) {
        update_simulation();
        render_frame();
        save_frame_to_file(frame);
        // Без задержки для максимальной скорости
    }
}
```

---

## Инструменты разработчика

### Запись и воспроизведение ввода

```cpp
struct InputRecorder {
    struct Record {
        SDL_Event event;
        Uint64 timestamp;
    };

    std::vector<Record> records;
    bool recording = false;
    bool playing = false;
    size_t playbackIndex = 0;
    Uint64 playbackStartTime = 0;

    void record(const SDL_Event& event) {
        if (recording) {
            records.push_back({event, SDL_GetTicks()});
        }
    }

    void startPlayback() {
        playing = true;
        playbackIndex = 0;
        playbackStartTime = SDL_GetTicks();
    }

    SDL_Event* getNextEvent() {
        if (!playing || playbackIndex >= records.size()) return nullptr;

        Uint64 elapsed = SDL_GetTicks() - playbackStartTime;
        if (elapsed >= records[playbackIndex].timestamp) {
            return &records[playbackIndex++].event;
        }
        return nullptr;
    }
};
```

---

## Сравнение архитектур

| Архитектура        | Преимущества                      | Недостатки                      | Когда использовать                              |
|--------------------|-----------------------------------|---------------------------------|-------------------------------------------------|
| **Callbacks**      | Кроссплатформенность, меньше кода | Меньше контроля                 | Мобильные платформы, кроссплатформенные проекты |
| **Classic main()** | Полный контроль, простота отладки | Требует адаптации для мобильных | Десктопные игры, инструменты                    |
| **Multi-window**   | Гибкий интерфейс                  | Сложность управления            | Редакторы, IDE                                  |

---

## Паттерны управления окнами

| Паттерн             | Описание                         | Применение                        |
|---------------------|----------------------------------|-----------------------------------|
| **Single-window**   | Одно главное окно                | Игры, медиаплееры                 |
| **Multi-window**    | Несколько независимых окон       | Графические редакторы, DAW        |
| **Document-view**   | Главное окно с вкладками         | Текстовые редакторы, браузеры     |
| **Floating panels** | Основное окно + плавающие панели | CAD, профессиональные инструменты |

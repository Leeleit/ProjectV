# Сценарии использования SDL3

**🟡 Уровень 2: Средний**

---

## Содержание

- [Архитектуры игр](#архитектуры-игр)
- [Графические редакторы](#графические-редакторы)
- [Медиаплееры и инструменты](#медиаплееры-и-инструменты)
- [Научная визуализация](#научная-визуализация)
- [Интерактивные демо](#интерактивные-демо)

---

## Архитектуры игр

### Классическая игровая петля (Game Loop)

```cpp
// Традиционный подход с main()
int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD);
    SDL_Window* window = SDL_CreateWindow("Game", 1280, 720, SDL_WINDOW_VULKAN);
    
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            // Обработка ввода
            process_input(event);
        }
        
        // Обновление игровой логики
        update_game();
        
        // Рендеринг
        render_frame();
        
        // Ограничение FPS
        SDL_Delay(16); // ~60 FPS
    }
    
    SDL_Quit();
    return 0;
}
```

### Callback архитектура (SDL_MAIN_USE_CALLBACKS)

```cpp
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

struct GameState {
    SDL_Window* window = nullptr;
    // Игровое состояние
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    auto* game = new GameState;
    game->window = SDL_CreateWindow("Game", 1280, 720, SDL_WINDOW_VULKAN);
    *appstate = game;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;
    process_input(event);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    update_game();
    render_frame();
    return SDL_APP_CONTINUE;
}
```

### Преимущества каждой архитектуры:

| Архитектура              | Преимущества                                     | Недостатки                                 | Когда использовать                         |
|--------------------------|--------------------------------------------------|--------------------------------------------|--------------------------------------------|
| **Классическая петля**   | Полный контроль над циклом, простота отладки     | Платформо-специфичные нюансы (iOS/Android) | Десктопные игры, инструменты               |
| **Callback архитектура** | Естественная интеграция с мобильными платформами | Меньше контроля над временем выполнения    | Кроссплатформенные проекты, мобильные игры |

---

## Графические редакторы

### Multi-window архитектура

Графические редакторы часто используют несколько окон для разных панелей инструментов, палитр и viewport'ов.

```cpp
class EditorWindowManager {
    std::vector<SDL_Window*> windows;
    
public:
    SDL_Window* createToolWindow(const char* title, int width, int height) {
        SDL_Window* win = SDL_CreateWindow(title, width, height, 
                                          SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        windows.push_back(win);
        return win;
    }
    
    void destroyAllWindows() {
        for (auto* win : windows) {
            SDL_DestroyWindow(win);
        }
        windows.clear();
    }
};
```

### Паттерн: Event routing между окнами

```cpp
struct EditorState {
    SDL_Window* mainViewport;
    SDL_Window* toolPalette;
    SDL_Window* propertiesPanel;
    
    // Маршрутизация событий к активному окну
    void routeEvent(SDL_Event* event) {
        if (event->type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
            activeWindow = getWindowById(event->window.windowID);
        }
        
        if (activeWindow) {
            handleEventForWindow(activeWindow, event);
        }
    }
};
```

### Drag & Drop поддержка

```cpp
void enableDragAndDrop(SDL_Window* window) {
    // Включить поддержку drag & drop файлов
    SDL_SetHint(SDL_HINT_VIDEO_EXTERNAL_CONTEXT, "0");
    
    // Обработка событий drop
    if (event->type == SDL_EVENT_DROP_FILE) {
        const char* filePath = event->drop.file;
        // Загрузка файла...
        SDL_free((void*)filePath);
    }
    
    if (event->type == SDL_EVENT_DROP_COMPLETE) {
        // Завершение операции drop
    }
}
```

---

## Медиаплееры и инструменты

### Аудио-видео синхронизация

```cpp
struct MediaPlayer {
    SDL_Window* window;
    SDL_AudioDeviceID audioDevice;
    Uint32 videoStartTime;
    Uint32 audioStartTime;
    
    void play(const char* videoFile, const char* audioFile) {
        // Инициализация видео
        // Инициализация аудио
        videoStartTime = SDL_GetTicks();
        audioStartTime = videoStartTime;
        
        // Цикл воспроизведения с синхронизацией
    }
    
    void syncPlayback() {
        Uint32 currentTime = SDL_GetTicks();
        Uint32 videoElapsed = currentTime - videoStartTime;
        Uint32 audioElapsed = currentTime - audioStartTime;
        
        // Корректировка синхронизации при расхождении
        if (abs((int)videoElapsed - (int)audioElapsed) > 30) {
            // Сброс синхронизации
        }
    }
};
```

### Режимы отображения (Fullscreen, Borderless, Windowed)

```cpp
void toggleFullscreen(SDL_Window* window) {
    bool isFullscreen = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN;
    
    if (isFullscreen) {
        SDL_SetWindowFullscreen(window, false);
        SDL_SetWindowSize(window, 1280, 720);
    } else {
        SDL_SetWindowFullscreen(window, true);
    }
}

void setBorderlessFullscreen(SDL_Window* window) {
    SDL_SetWindowBordered(window, false);
    SDL_SetWindowPosition(window, 0, 0);
    
    SDL_DisplayMode dm;
    SDL_GetCurrentDisplayMode(0, &dm);
    SDL_SetWindowSize(window, dm.w, dm.h);
}
```

---

## Научная визуализация

### Режим реального времени vs Оффлайн рендеринг

```cpp
struct VisualizationApp {
    enum class Mode { RealTime, Offline };
    Mode currentMode = Mode::RealTime;
    
    // Для реального времени
    void realTimeLoop() {
        while (running) {
            SDL_PollEvent(&event);
            updateSimulation();
            renderFrame();
            SDL_Delay(16); // 60 FPS
        }
    }
    
    // Для оффлайн рендеринга
    void offlineRender() {
        for (int frame = 0; frame < totalFrames; frame++) {
            updateSimulation();
            renderFrame();
            saveFrameToFile(frame);
            // Без задержки для максимальной скорости
        }
    }
};
```

### Визуализация больших данных

```cpp
void setupHighDPIRendering(SDL_Window* window) {
    // Получение реального размера в пикселях для HiDPI дисплеев
    int pixelWidth, pixelHeight;
    SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight);
    
    // Настройка Vulkan/OpenGL для рендеринга в полном разрешении
    // с последующим downscaling'ом при необходимости
}
```

---

## Интерактивные демо

### Демо-режим (автоматическое проигрывание)

```cpp
struct DemoMode {
    std::vector<DemoStep> steps;
    size_t currentStep = 0;
    Uint32 stepStartTime;
    
    void update(SDL_Event* event) {
        if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_SPACE) {
            // Пауза/продолжение по пробелу
            togglePause();
        }
        
        if (!paused) {
            Uint32 elapsed = SDL_GetTicks() - stepStartTime;
            if (elapsed >= steps[currentStep].duration) {
                nextStep();
            }
        }
    }
};
```

### Запись и воспроизведение ввода

```cpp
struct InputRecorder {
    std::vector<InputEvent> recordedEvents;
    bool isRecording = false;
    bool isPlayingBack = false;
    size_t playbackIndex = 0;
    
    void recordEvent(const SDL_Event& event) {
        if (isRecording) {
            InputRecord record;
            record.event = event;
            record.timestamp = SDL_GetTicks();
            recordedEvents.push_back(record);
        }
    }
    
    void playback() {
        if (isPlayingBack && playbackIndex < recordedEvents.size()) {
            const auto& record = recordedEvents[playbackIndex];
            if (SDL_GetTicks() >= record.timestamp) {
                injectEvent(record.event);
                playbackIndex++;
            }
        }
    }
};
```

---

## Следующие шаги

1. **Выбор архитектуры**: Определитесь с подходящей архитектурой для вашего приложения
2. **Интеграция**: Настройте SDL с другими библиотеками (Vulkan, аудио, ввод)
3. **Оптимизация**: Примените рекомендации из раздела [Производительность](performance.md)
4. **Специализация**: Для специфичных сценариев смотрите [Интеграция в ProjectV](projectv-integration.md)

---

## Связанные разделы

- [Основные понятия](concepts.md) — фундаментальные концепции SDL
- [Быстрый старт](quickstart.md) — минимальный работающий пример
- [Производительность](performance.md) — оптимизация производительности
- [Decision Trees](decision-trees.md) — рекомендации по выбору архитектуры
- [Интеграция в ProjectV](projectv-integration.md) — специфичные паттерны для воксельного движка

← [Назад к документации SDL](README.md)
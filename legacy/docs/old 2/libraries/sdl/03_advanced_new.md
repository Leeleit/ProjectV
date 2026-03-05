## Решение проблем SDL3

<!-- anchor: 06_troubleshooting -->

**🟡 Уровень 2: Средний**

Частые ошибки при работе с SDL3 и способы их решения.

---

## Инициализация

### SDL_Init возвращает false

**Причины:**

- Нет дисплея (headless режим)
- Неподходящий драйвер
- Конфликт с другим приложением

**Решение:**

```cpp
if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
}
```

Проверьте сообщение `SDL_GetError()` для диагностики.

---

### SDL_CreateWindow возвращает NULL

**Причины:**

- `SDL_Init` не был вызван или завершился ошибкой
- Драйвер не поддерживает запрошенные флаги
- Нехватка системных ресурсов

**Решение:**

```cpp
SDL_Window* window = SDL_CreateWindow("App", 1280, 720, SDL_WINDOW_VULKAN);
if (!window) {
    SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
}
```

Для диагностики попробуйте создать окно без флага `SDL_WINDOW_VULKAN`.

---

### SDL_Vulkan_CreateSurface возвращает false

**Причины:**

- Окно создано без `SDL_WINDOW_VULKAN`
- VkInstance создан без расширений от `SDL_Vulkan_GetInstanceExtensions`
- Vulkan loader не загружен

**Решение:**

1. Убедитесь, что окно создано с `SDL_WINDOW_VULKAN`
2. Добавьте все расширения от SDL при создании instance:

```cpp
Uint32 extensionCount = 0;
const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

VkInstanceCreateInfo info = {};
info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
info.enabledExtensionCount = extensionCount;
info.ppEnabledExtensionNames = extensions;
```

3. Убедитесь, что volk загружен (если используется):

```cpp
volkLoadInstance(instance);  // До SDL_Vulkan_CreateSurface
```

---

### SDL_Vulkan_GetInstanceExtensions возвращает NULL

**Причина:** Vulkan loader не загружен.

**Решение:**

SDL загружает Vulkan loader при создании окна с `SDL_WINDOW_VULKAN`. Если окно ещё не создано:

```cpp
SDL_Vulkan_LoadLibrary(nullptr);  // Явная загрузка
```

---

### Окно не отображается или сразу закрывается

**Причины:**

- `SDL_AppInit` возвращает `SDL_APP_FAILURE` или `SDL_APP_SUCCESS`
- `SDL_AppEvent` возвращает `SDL_APP_SUCCESS` на первом событии
- `SDL_AppIterate` возвращает не `SDL_APP_CONTINUE`

**Решение:**

```cpp
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    // ...
    return SDL_APP_CONTINUE;  // Не SUCCESS и не FAILURE
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  // Только при намеренном выходе
    }
    return SDL_APP_CONTINUE;  // Для остальных событий
}
```

---

## Runtime

### События не приходят

**Причина:** При `SDL_MAIN_USE_CALLBACKS` события доставляются в `SDL_AppEvent`, а не через `SDL_PollEvent`.

**Решение:**

Не используйте `SDL_PollEvent` в callback режиме:

```cpp
// НЕПРАВИЛЬНО при SDL_MAIN_USE_CALLBACKS
while (SDL_PollEvent(&event)) { /* ... */ }

// ПРАВИЛЬНО при SDL_MAIN_USE_CALLBACKS
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    // Обработка event
    return SDL_APP_CONTINUE;
}
```

---

### DLL SDL не найдена (Windows)

**Причина:** `SDL3.dll` не в том же каталоге, что exe.

**Решение:**

1. Добавьте копирование DLL в CMake:

```cmake
if(WIN32)
    add_custom_command(TARGET YourApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3>"
            "$<TARGET_FILE_DIR:YourApp>"
    )
endif()
```

2. Или скопируйте DLL вручную в каталог с exe.

---

### Приложение зависает при закрытии

**Причина:** Неверный порядок освобождения Vulkan ресурсов.

**Решение:**

Соблюдайте порядок cleanup:

```cpp
// 1. Ожидание GPU
vkDeviceWaitIdle(device);

// 2. Уничтожение swapchain и связанных ресурсов
destroy_swapchain();

// 3. Уничтожение surface (ДО SDL_DestroyWindow!)
SDL_Vulkan_DestroySurface(instance, surface, nullptr);

// 4. Уничтожение device и instance
vkDestroyDevice(device, nullptr);
vkDestroyInstance(instance, nullptr);

// 5. Уничтожение окна
SDL_DestroyWindow(window);
```

---

### Нужен кастомный путь к Vulkan loader

**Решение:**

```cpp
SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, "path/to/vulkan-1.dll");
// До создания окна или SDL_Vulkan_LoadLibrary
```

---

## Сборка

### undefined reference to SDL_main

**Причина:** Конфликт между `SDL_MAIN_USE_CALLBACKS` и пользовательским `main()`.

**Решение:**

При `SDL_MAIN_USE_CALLBACKS` **не** определяйте `main()`:

```cpp
// НЕПРАВИЛЬНО
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int argc, char* argv[]) {  // Конфликт!
    // ...
}

// ПРАВИЛЬНО
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    // Реализуйте только callbacks
}
```

---

### Глобальные объекты в SDL_MAIN_USE_CALLBACKS

**Проблема:** Глобальные объекты создаются до `SDL_AppInit`, когда SDL ещё не инициализирован.

**Решение 1: App State Pattern**

```cpp
struct AppState {
    SDL_Window* window;
    // Все ресурсы приложения
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    auto* state = new AppState();
    state->window = SDL_CreateWindow(...);
    *appstate = state;
    return SDL_APP_CONTINUE;
}
```

**Решение 2: Lazy Initialization**

```cpp
class VulkanContext {
    SDL_Window* window_ = nullptr;
    bool initialized_ = false;

public:
    void init() {
        if (initialized_) return;
        window_ = SDL_CreateWindow(...);
        initialized_ = true;
    }
};

VulkanContext* g_context = nullptr;  // Указатель, не объект

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    g_context = new VulkanContext();
    g_context->init();
    return SDL_APP_CONTINUE;
}
```

---

### Множественные окна: обработка закрытия

**Проблема:** Как различить окна при закрытии?

**Решение:**

```cpp
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* state = static_cast<AppState*>(appstate);

    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        SDL_WindowID closedID = event->window.windowID;

        if (closedID == SDL_GetWindowID(state->mainWindow)) {
            return SDL_APP_SUCCESS;  // Выход при закрытии главного окна
        } else if (closedID == SDL_GetWindowID(state->toolWindow)) {
            SDL_DestroyWindow(state->toolWindow);
            state->toolWindow = nullptr;
        }
    }
    return SDL_APP_CONTINUE;
}
```

---

## Многопоточность

### SDL не работает из других потоков

**Проблема:** Большинство SDL функций должны вызываться из main thread.

**Решение:**

Фоновые потоки — только для CPU работы, без SDL вызовов:

```cpp
std::thread loadingThread([&]() {
    load_assets_to_memory();  // Только CPU работа
    state->loadingDone = true;
});
loadingThread.detach();

// В SDL_AppIterate (main thread):
if (state->loadingDone) {
    // Теперь можно использовать SDL
    upload_to_gpu();
}
```

---

## Платформенные проблемы

### Linux: X11/Wayland не работает

**Решение:**

```cpp
// Явный выбор драйвера
SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");  // или "wayland"
```

Проверьте установку пакетов:

```bash
# Ubuntu/Debian
sudo apt install libsdl3-dev

# Arch
sudo pacman -S sdl3
```

---

### macOS: Retina масштабирование

**Проблема:** Неверный размер framebuffer на Retina дисплеях.

**Решение:**

Используйте `SDL_GetWindowSizeInPixels`:

```cpp
int pixelWidth, pixelHeight;
SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight);
// Использовать для swapchain/framebuffer
```

---

### HiDPI: Размытое отображение

**Решение:**

```cpp
// Windows: запрос DPI awareness
SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");

// Обработка SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
```

---

## Чеклист диагностики

При проблемах с SDL:

1. Проверьте возвращаемые значения всех SDL функций
2. Вызывайте `SDL_GetError()` сразу после ошибки
3. Убедитесь в правильном порядке инициализации
4. Проверьте флаги окна (`SDL_WINDOW_VULKAN` и др.)
5. Для Vulkan: убедитесь, что расширения instance получены от SDL
6. При callbacks: не используйте `SDL_PollEvent`
7. Проверьте порядок cleanup (surface до window)

---

## Производительность SDL3

<!-- anchor: 07_performance -->

**🟡 Уровень 2: Средний**

Оптимизация работы с SDL3 для достижения максимальной производительности.

---

## Оптимизация ввода

### Raw Input для точного позиционирования

Стандартная обработка мыши может иметь ускорение и сглаживание ОС. Для приложений, требующих точности:

```cpp
void configure_raw_input() {
    if (SDL_HasRawMouseMotion()) {
        SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
        SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SCALING, "0");
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }
}
```

### Относительный режим мыши

```cpp
// Включение относительного режима
SDL_SetRelativeMouseMode(SDL_TRUE);

// В SDL_AppEvent:
if (event->type == SDL_EVENT_MOUSE_MOTION) {
    int xrel = event->motion.xrel;  // Относительное смещение
    int yrel = event->motion.yrel;
    // Использовать для управления камерой
}
```

---

## Обработка событий

### Пакетная обработка

При классическом main loop можно обрабатывать события пакетами:

```cpp
struct BatchedInput {
    std::vector<SDL_Event> keyboardEvents;
    std::vector<SDL_Event> mouseEvents;

    void collect() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP:
                    keyboardEvents.push_back(event);
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    mouseEvents.push_back(event);
                    break;
            }
        }
    }

    void process() {
        for (const auto& e : keyboardEvents) {
            process_keyboard(e);
        }
        for (const auto& e : mouseEvents) {
            process_mouse(e);
        }
        keyboardEvents.clear();
        mouseEvents.clear();
    }
};
```

---

## Оптимизация рендеринга

### Vulkan Swapchain

Для минимальной задержки используйте mailbox present mode:

```cpp
VkSwapchainCreateInfoKHR swapchainInfo = {};
swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
swapchainInfo.minImageCount = 3;  // Triple buffering
swapchainInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;  // Низкая latency
```

### Обработка resize

Избегайте лишних пересозданий swapchain:

```cpp
void handle_resize(int newWidth, int newHeight) {
    static int lastWidth = 0, lastHeight = 0;
    
    // Обновлять только при значительном изменении
    if (abs(newWidth - lastWidth) > 10 || abs(newHeight - lastHeight) > 10) {
        vkDeviceWaitIdle(device);
        recreate_swapchain(newWidth, newHeight);
        lastWidth = newWidth;
        lastHeight = newHeight;
    }
}
```

---

## Оптимизация памяти

### Пулы памяти

Избегайте аллокаций в игровом цикле:

```cpp
template<size_t PoolSize = 64 * 1024>
class FrameAllocator {
    std::array<char, PoolSize> pool_;
    size_t offset_ = 0;

public:
    void* allocate(size_t size, size_t alignment = 16) {
        size_t aligned = (offset_ + alignment - 1) & ~(alignment - 1);
        if (aligned + size > PoolSize) return nullptr;
        
        void* ptr = pool_.data() + aligned;
        offset_ = aligned + size;
        return ptr;
    }

    void reset() { offset_ = 0; }
};

// Использование
FrameAllocator<> frameAllocator;

SDL_AppResult SDL_AppIterate(void* appstate) {
    frameAllocator.reset();
    
    auto* tempData = frameAllocator.allocate(sizeof(TempData));
    // Использовать tempData в течение кадра
    // Не нужно освобождать — сбросится в следующем кадре
    
    return SDL_APP_CONTINUE;
}
```

---

## Профилирование

### Измерение FPS

```cpp
struct FrameMetrics {
    Uint64 frameTimes[60] = {0};
    size_t frameIndex = 0;
    Uint64 lastTime = 0;

    void frame() {
        Uint64 currentTime = SDL_GetPerformanceCounter();
        if (lastTime > 0) {
            Uint64 freq = SDL_GetPerformanceFrequency();
            frameTimes[frameIndex] = currentTime - lastTime;
            frameIndex = (frameIndex + 1) % 60;
        }
        lastTime = currentTime;
    }

    double getFPS() const {
        Uint64 total = 0;
        for (size_t i = 0; i < 60; i++) {
            total += frameTimes[i];
        }
        Uint64 freq = SDL_GetPerformanceFrequency();
        return 60.0 * freq / total;
    }

    double getFrameTimeMS() const {
        size_t idx = (frameIndex + 59) % 60;
        Uint64 freq = SDL_GetPerformanceFrequency();
        return 1000.0 * frameTimes[idx] / freq;
    }
};
```

### Высокоточный таймер

```cpp
Uint64 start = SDL_GetPerformanceCounter();

// Измеряемый код

Uint64 end = SDL_GetPerformanceCounter();
Uint64 freq = SDL_GetPerformanceFrequency();

double seconds = (double)(end - start) / freq;
double milliseconds = seconds * 1000.0;
```

---

## Платформенные оптимизации

### Windows

```cpp
void windows_optimizations() {
    // DPI awareness
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    
    // Отключение compositor для fullscreen
    SDL_SetHint(SDL_HINT_VIDEO_FULLSCREEN_SPACES, "0");
}
```

### Linux

```cpp
void linux_optimizations() {
    // Предпочтительный драйвер
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");  // или "wayland"
    
    // Обход compositor
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "1");
}
```

### macOS

```cpp
void macos_optimizations() {
    // Metal renderer
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");
    
    // Retina support
    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
}
```

---

## Рекомендации

### Делать

- Использовать `SDL_GetWindowSizeInPixels()` для Vulkan/OpenGL
- Обрабатывать `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` для HiDPI
- Использовать пулы памяти для временных данных
- Профилировать перед оптимизацией
- Использовать raw input для приложений с точным позиционированием

### Не делать

- Не вызывать `SDL_PollEvent()` в callback режиме
- Не создавать/уничтожать окна в основном цикле
- Не использовать `SDL_GetTicks()` для точных интервалов (лучше `SDL_GetPerformanceCounter`)
- Не выполнять синхронные операции в рендеринге

---

## Чеклист оптимизации

1. Включить raw input при необходимости точности
2. Настроить swapchain для низкой latency
3. Использовать пулы памяти для кадра
4. Обработка HiDPI корректно
5. Платформо-специфичные оптимизации
6. Профилирование узких мест

---

## Сценарии использования SDL3

<!-- anchor: 08_use-cases -->

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

---

## Decision Trees для SDL3

<!-- anchor: 09_decision-trees -->

**🟡 Уровень 2: Средний**

Руководство по выбору архитектуры и API для проектов на SDL3.

---

## Выбор архитектуры Event Loop

### Критерии выбора

| Критерий            | Callbacks               | Classic main() |
|---------------------|-------------------------|----------------|
| Целевые платформы   | Все (включая мобильные) | Только десктоп |
| Контроль над циклом | Ограниченный            | Полный         |
| Boilerplate код     | Минимум                 | Больше         |
| Отладка             | Сложнее                 | Проще          |

### Решение

```
Нужна поддержка iOS/Android?
├── Да → SDL_MAIN_USE_CALLBACKS (обязательно)
└── Нет → Нужен полный контроль над временем выполнения?
           ├── Да → Classic main() + SDL_PollEvent
           └── Нет → SDL_MAIN_USE_CALLBACKS (рекомендуется)
```

---

## Выбор графического API

### Сравнение

| API              | Производительность | Сложность | Совместимость   |
|------------------|--------------------|-----------|-----------------|
| **Vulkan**       | Максимальная       | Высокая   | Современные GPU |
| **OpenGL**       | Средняя            | Средняя   | Широкая         |
| **SDL_Renderer** | Низкая             | Низкая    | Максимальная    |

### Решение

```
Каковы требования к производительности?
├── Максимальная → Vulkan
│                  └── Нужен volk для загрузки функций
├── Средняя → OpenGL
│             └── SDL_WINDOW_OPENGL + GL context
└── Низкая / Простота разработки → SDL_Renderer
                                    └── Встроенный 2D рендерер
```

---

## Выбор архитектуры окон

### Решение

```
Тип приложения?
├── Игра
│   └── Single-window (fullscreen или windowed)
├── Графический редактор
│   └── Multi-window (viewport + панели)
├── Инструмент / Утилита
│   └── Single-window + диалоги
└── Медиаплеер
    └── Single-window с fullscreen toggle
```

### Паттерны окон

| Паттерн             | Описание                        | Примеры        |
|---------------------|---------------------------------|----------------|
| **Single-window**   | Одно главное окно               | Игры, плееры   |
| **Multi-window**    | Несколько независимых окон      | Редакторы, IDE |
| **Floating panels** | Главное окно + плавающие панели | CAD, DAW       |

---

## Выбор стратегии ввода

### Решение

```
Требуемая точность ввода?
├── Высокая (графические редакторы, CAD)
│   └── Raw Input + относительный режим мыши
├── Средняя (большинство игр)
│   └── Стандартная обработка событий
└── Низкая (меню, интерфейсы)
    └── Простая обработка в SDL_AppEvent
```

### Raw Input

```cpp
// Для приложений, требующих точности
if (SDL_HasRawMouseMotion()) {
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SCALING, "0");
    SDL_SetRelativeMouseMode(SDL_TRUE);
}
```

---

## Выбор стратегии оптимизации

### Решение

```
Требования к производительности?
├── Максимальная (AAA игры, движки)
│   ├── Пулы памяти
│   ├── Lock-free структуры
│   ├── SIMD оптимизации
│   └── Профилирование (Tracy)
├── Сбалансированная (коммерческие проекты)
│   ├── Кэширование
│   ├── Batch processing
│   └── Базовое профилирование
└── Минимальная (прототипы, инструменты)
    └── Читаемость важнее оптимизации
```

### Уровни оптимизации

| Уровень         | Методы                        | Инструменты        |
|-----------------|-------------------------------|--------------------|
| **Агрессивный** | Пулы памяти, lock-free, SIMD  | Tracy, VTune       |
| **Умеренный**   | Кэширование, batch processing | Встроенные таймеры |
| **Минимальный** | Чистый код                    | —                  |

---

## Кроссплатформенные решения

### Решение

```
Сколько платформ?
├── Одна (например, только Windows)
│   └── Можно использовать платформо-специфичные API
├── 2-3 платформы (desktop)
│   └── SDL абстракция + условная компиляция
└── Все платформы (включая мобильные)
    ├── Только SDL API
    ├── SDL_MAIN_USE_CALLBACKS (обязательно)
    └── Абстракция для сложных случаев
```

### Платформенные особенности

| Платформа       | Особенности    | Hints                            |
|-----------------|----------------|----------------------------------|
| **Windows**     | DPI, Game Mode | `SDL_HINT_WINDOWS_DPI_AWARENESS` |
| **macOS**       | Retina, Metal  | `SDL_HINT_RENDER_DRIVER="metal"` |
| **Linux**       | X11/Wayland    | `SDL_HINT_VIDEO_DRIVER`          |
| **iOS/Android** | Жизненный цикл | Callbacks обязательны            |

---

## Быстрые рекомендации по умолчанию

Для нового проекта:

1. **Архитектура**: `SDL_MAIN_USE_CALLBACKS` (кроссплатформенность)
2. **Графика**: Vulkan (производительность), OpenGL (простота)
3. **Окна**: Single-window (игры), Multi-window (редакторы)
4. **Ввод**: Стандартный, Raw Input при необходимости точности
5. **Профилирование**: Tracy с самого начала
6. **Абстракция**: Platform abstraction layer для >2 платформ

---

## Чеклист принятия решений

- [ ] Определены целевые платформы
- [ ] Выбрана архитектура event loop
- [ ] Выбран графический API
- [ ] Определена архитектура окон
- [ ] Выбрана стратегия обработки ввода
- [ ] Определён уровень оптимизации
- [ ] Выбраны инструменты профилирования
# Интеграция SDL3 в ProjectV

**🔴 Уровень 3: Продвинутый**

---

## Содержание

- [Архитектура Event Loop для ECS](#архитектура-event-loop-для-ecs)
- [Multi-window редактор вокселей](#multi-window-редактор-вокселей)
- [Оптимизация ввода для точного редактирования](#оптимизация-ввода-для-точного-редактирования)
- [Decision Trees для ProjectV](#decision-trees-для-projectv)
- [Интеграция с другими библиотеками ProjectV](#интеграция-с-другими-библиотеками-projectv)
- [Примеры кода ProjectV](#примеры-кода-projectv)

---

## Архитектура Event Loop для ECS

ProjectV использует **SDL_MAIN_USE_CALLBACKS** для интеграции с ECS (flecs). Это позволяет избежать boilerplate-кода
традиционного цикла `while` и упрощает портирование.

### Паттерн: SDL + flecs

```cpp
// src/main.cpp
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <flecs.h>

struct AppState {
    SDL_Window* window = nullptr;
    flecs::world* ecs = nullptr;
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    auto* app = new AppState;
    app->ecs = new flecs::world;
    
    // Инициализация систем flecs
    init_voxel_systems(app->ecs);
    
    // Создание окна
    app->window = SDL_CreateWindow("ProjectV", 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    
    *appstate = app;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* app = static_cast<AppState*>(appstate);
    
    // Шаг ECS: в нём обрабатываются и ввод, и физика, и рендер
    app->ecs->progress();
    
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* app = static_cast<AppState*>(appstate);
    
    // Передача событий в ECS как компоненты-события
    process_sdl_event_to_ecs(app->ecs, event);
    
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    auto* app = static_cast<AppState*>(appstate);
    delete app->ecs;
    delete app;
}
```

### Преимущества паттерна:

1. **Естественная интеграция с ECS** — события становятся компонентами
2. **Кроссплатформенность** — работает на iOS/Android без изменений
3. **Упрощённый lifecycle** — SDL управляет вызовами
4. **Гибкость** — можно смешивать с обычным циклом при необходимости

---

## Multi-window редактор вокселей

Воксельный редактор требует нескольких окон (Viewport, Tools, Palette).

### Менеджер окон

```cpp
class WindowManager {
    std::vector<SDL_Window*> windows;
    
public:
    SDL_Window* createToolWindow(const char* title, int width, int height) {
        SDL_Window* win = SDL_CreateWindow(title, width, height, 
                                          SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        // Для каждого окна нужен свой VkSurfaceKHR
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

### Сценарии использования:

1. **Основной viewport** — 3D отображение воксельного мира
2. **Панель инструментов** — выбор кистей, материалов, слоёв
3. **Палитра цветов** — выбор и смешивание цветов
4. **Консоль отладки** — ImGui-окно с логами и метриками
5. **Инспектор свойств** — редактирование свойств объектов

---

## Оптимизация ввода для точного редактирования

Для воксельного редактора критична низкая задержка и высокая точность мыши.

### Raw Input для кисти

Стандартная обработка мыши может иметь ускорение и сглаживание ОС. Для точного редактирования вокселей:

```cpp
void configureRawInput() {
    if (SDL_HasRawMouseMotion()) {
        SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
        SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SCALING, "0");
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }
}
```

### Выделенный поток ввода (Input Thread)

Для исключения блокировок ввода при тяжелом рендеринге можно использовать отдельный поток (экспериментально):

```cpp
struct InputThread {
    std::atomic<bool> running{true};
    // Lock-free очередь для передачи событий в render thread
    EventRingBuffer<SDL_Event, 1024> event_buffer; 
    
    void run() {
        while (running) {
            SDL_PumpEvents(); // Только в главном потоке, если не SDL_MAIN_USE_CALLBACKS
            // ...
        }
    }
};
```

### Профилирование с Tracy

Интеграция с Tracy для анализа Input Latency:

```cpp
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    ZoneScopedN("SDL Event");
    
    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        TracyPlot("Mouse Delta", abs(event->motion.xrel) + abs(event->motion.yrel));
    }
    
    return SDL_APP_CONTINUE;
}
```

---

## Decision Trees для ProjectV

### Быстрые рекомендации для ProjectV:

1. **Event Loop**: `SDL_MAIN_USE_CALLBACKS` с интеграцией flecs систем
2. **Threading**: Dedicated input thread + render thread для редактора
3. **Window Management**: Поддержка single и multi-window архитектур
4. **Input Processing**: Raw input + event timestamping + prediction
5. **Library Integration**: SDL события → flecs компоненты → системы
6. **Performance**: Lock-free queues + event batching + memory pooling
7. **Cross-platform**: Абстракция над платформо-специфичными особенностями

### Дерево принятия решений по умолчанию:

```
Для нового проекта ProjectV:
1. Начните с SDL_MAIN_USE_CALLBACKS + flecs integration
2. Добавьте dedicated input thread при первых признаках input latency
3. Используйте multi-window архитектуру только при наличии ≥2 мониторов
4. Включите Raw Input для точного позиционирования кисти
5. Инструментируйте Tracy zones во всех SDL callbacks
6. Реализуйте platform abstraction layer с самого начала
```

---

## Интеграция с другими библиотеками ProjectV

### SDL + volk + Vulkan

```cpp
// Порядок инициализации:
1. SDL_Init(SDL_INIT_VIDEO)
2. SDL_CreateWindow(..., SDL_WINDOW_VULKAN)
3. volkInitializeCustom(SDL_Vulkan_GetVkGetInstanceProcAddr())
4. SDL_Vulkan_GetInstanceExtensions(...)
5. vkCreateInstance(...)
6. volkLoadInstance(instance)
7. SDL_Vulkan_CreateSurface(...)
```

### SDL + ImGui

```cpp
// Пример: создание ImGui контекста для SDL+Vulkan
ImGui::CreateContext();
ImGui_ImplSDL3_InitForVulkan(window);
ImGui_ImplVulkan_Init(...);
```

### SDL + miniaudio

```cpp
// Совместная работа с аудио (см. пример miniaudio_sdl.cpp)
```

---

## Примеры кода ProjectV

ProjectV содержит несколько примеров интеграции SDL:

| Пример                | Описание                         | Ссылка                                                                 |
|-----------------------|----------------------------------|------------------------------------------------------------------------|
| Базовое окно          | Минимальный пример создания окна | [sdl_window.cpp](../examples/sdl_window.cpp)                           |
| SDL + flecs           | Интеграция событий SDL в ECS     | [sdl_flecs_integration.cpp](../examples/sdl_flecs_integration.cpp)     |
| Multi-window редактор | Управление несколькими окнами    | [sdl_multi_window_editor.cpp](../examples/sdl_multi_window_editor.cpp) |
| SDL + ImGui + Vulkan  | Полный стек GUI                  | [imgui_sdl_vulkan.cpp](../examples/imgui_sdl_vulkan.cpp)               |

---

## Следующие шаги

1. **Базовое понимание**: Изучите [Основные понятия](concepts.md) и [Быстрый старт](quickstart.md)
2. **Интеграция**: Настройте SDL в своём проекте через [Интеграция](integration.md)
3. **Специализация**: Примените паттерны из этого документа для ProjectV
4. **Оптимизация**: Используйте рекомендации из раздела про оптимизацию ввода
5. **Расширение**: Добавьте multi-window поддержку при необходимости

---

## Связанные разделы

- [Основные понятия SDL](concepts.md) — фундаментальные концепции SDL3
- [Быстрый старт](quickstart.md) — минимальный работающий пример
- [Интеграция](integration.md) — настройка CMake и Vulkan
- [flecs](../flecs/README.md) — ECS библиотека ProjectV
- [Vulkan](../vulkan/README.md) — графический API

← [Назад к документации SDL](README.md)
# SDL3: Чистый справочник для студентов

**SDL3** (Simple DirectMedia Layer) — библиотека низкого уровня для кроссплатформенного доступа к графическому
оборудованию, вводу и аудио. Версия 3.x — современный API с поддержкой Vulkan, новой callback-архитектурой и улучшенной
многопоточностью.

> **Для понимания:** SDL — это как "универсальный переводчик" между твоей игрой и операционной системой. Вместо того
> чтобы учить 10 языков (Windows API, X11, Wayland, Cocoa...), ты говоришь на одном — SDL, а библиотека сама общается с
> нужной системой.

## Назначение

SDL3 предоставляет абстракцию над платформенными API:

| Подсистема  | Функциональность                                                    |
|-------------|---------------------------------------------------------------------|
| **Видео**   | Создание окон, управление display modes, интеграция с Vulkan/OpenGL |
| **События** | Обработка ввода, оконные события, пользовательские события          |
| **Ввод**    | Клавиатура, мышь, геймпады, джойстики, сенсоры                      |
| **Аудио**   | Воспроизведение и захват звука                                      |
| **Таймеры** | Высокоточные таймеры, задержки                                      |

SDL3 **не предоставляет** высокоуровневый рендеринг — только окно и surface для рисования графическим API (Vulkan,
OpenGL).

## Архитектура входа в приложение

SDL3 предлагает два принципиально разных подхода к организации приложения.

### Callback-архитектура (рекомендуется)

Приложение реализует функции обратного вызова, SDL управляет циклом:

```cpp
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]);
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event);
SDL_AppResult SDL_AppIterate(void* appstate);
void SDL_AppQuit(void* appstate, SDL_AppResult result);
```

> **Для понимания:** Это как нанять "шеф-повара" (SDL), который сам организует кухню. Ты только говоришь: "Приготовь это
> блюдо" (AppInit), "клиент попросил счёт" (AppEvent), "пора подавать" (AppIterate). SDL сама управляет плитой,
> духовкой,
> посудой.

### Классический main()

Приложение полностью контролирует цикл событий:

```cpp
#include <SDL3/SDL.h>

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("Game", 1280, 720, 0);

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }
    }

    SDL_Quit();
    return 0;
}
```

### Сравнение подходов

| Критерий            | Callback-архитектура   | Классический main() |
|---------------------|------------------------|---------------------|
| Контроль над циклом | Ограниченный           | Полный              |
| Мобильные платформы | Нативная поддержка     | Требует адаптации   |
| Сложность кода      | Минимум boilerplate    | Больше строк        |
| Гибкость            | Ограничена интерфейсом | Полная              |

## Жизненный цикл callback-приложения

```
┌─────────────────────────────────────────────────┐
│                 SDL_AppInit()                   │
│  • SDL_Init, SDL_CreateWindow                  │
│  • return SDL_APP_CONTINUE / SUCCESS / FAILURE │
└─────────────────────┬───────────────────────────┘
                      ▼
┌─────────────────────────────────────────────────┐
│              Главный цикл                       │
├─────────────────────┬───────────────────────────┤
│  SDL_AppEvent()    │    SDL_AppIterate()      │
│  (каждое событие)  │    (каждый кадр)         │
└─────────┬───────────┴───────────┬────────────────┘
          │                       │
          └───────────┬───────────┘
                      ▼
              SDL_APP_CONTINUE?
                      │
           ┌──────────┴──────────┐
           ▼                     ▼
        Продолжить           Выход
                      ▼
┌─────────────────────────────────────────────────┐
│                 SDL_AppQuit()                   │
│  • Освобождение ресурсов                        │
│  • SDL_Quit() вызывается автоматически         │
└─────────────────────────────────────────────────┘
```

## Типы событий

### SDL_Event

Union всех типов событий. Поле `type` определяет активное поле:

```cpp
SDL_Event event;
switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
        // event.key — SDL_KeyboardEvent
        if (event.key.key == SDLK_ESCAPE) { /* ... */ }
        break;

    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        // event.window — SDL_WindowEvent
        break;

    case SDL_EVENT_MOUSE_MOTION:
        // event.motion — SDL_MouseMotionEvent
        break;
}
```

### Часто используемые события

| Событие                                                     | Когда приходит                       |
|-------------------------------------------------------------|--------------------------------------|
| `SDL_EVENT_QUIT`                                            | Глобальный выход                     |
| `SDL_EVENT_WINDOW_CLOSE_REQUESTED`                          | Закрытие окна (крестик)              |
| `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED`                       | Изменение размера в пикселях (HiDPI) |
| `SDL_EVENT_KEY_DOWN` / `SDL_EVENT_KEY_UP`                   | Нажатие / отпускание клавиши         |
| `SDL_EVENT_MOUSE_MOTION`                                    | Движение мыши                        |
| `SDL_EVENT_MOUSE_BUTTON_DOWN` / `SDL_EVENT_MOUSE_BUTTON_UP` | Клик мыши                            |

### SDL_EVENT_QUIT vs SDL_EVENT_WINDOW_CLOSE_REQUESTED

| Событие                            | Семантика                                         |
|------------------------------------|---------------------------------------------------|
| `SDL_EVENT_WINDOW_CLOSE_REQUESTED` | Закрыто конкретное окно (`event.window.windowID`) |
| `SDL_EVENT_QUIT`                   | Глобальный выход (последнее окно, Alt+F4)         |

> **Для понимания:** SDL_EVENT_WINDOW_CLOSE_REQUESTED — это как сигнал "клиент хочет уйти из этого кафе", а
> SDL_EVENT_QUIT — "все клиенты ушли, пора закрываться".

## Клавиатура

### SDL_KeyboardEvent

| Поле       | Тип            | Описание                                                   |
|------------|----------------|------------------------------------------------------------|
| `key`      | `SDL_Keycode`  | Виртуальный код (зависит от раскладки): `SDLK_a`, `SDLK_w` |
| `scancode` | `SDL_Scancode` | Физическая клавиша (не зависит от раскладки)               |
| `mod`      | `SDL_Keymod`   | Модификаторы: Shift, Ctrl, Alt                             |
| `repeat`   | `bool`         | Автоповтор при зажатой клавише                             |

Для игр (WASD) **предпочтительнее scancode** — одна физическая клавиша на любой раскладке:

```cpp
// Правильно для игр (scancode)
if (event.key.scancode == SDL_SCANCODE_W) { /* ... */ }

// Альтернатива (keycode, зависит от раскладки)
if (event.key.key == SDLK_w) { /* ... */ }
```

## Мышь

### SDL_MouseMotionEvent

| Поле           | Описание                      |
|----------------|-------------------------------|
| `x`, `y`       | Абсолютные координаты курсора |
| `xrel`, `yrel` | Относительное смещение        |
| `state`        | Состояние кнопок (bitmask)    |

### SDL_MouseButtonEvent

| Поле     | Описание                                                           |
|----------|--------------------------------------------------------------------|
| `button` | Кнопка: `SDL_BUTTON_LEFT`, `SDL_BUTTON_RIGHT`, `SDL_BUTTON_MIDDLE` |
| `clicks` | Количество кликов (1 = одиночный, 2 = двойной)                     |
| `x`, `y` | Координаты                                                         |

### Относительный режим мыши

Для точного управления камерой или кистью:

```cpp
SDL_SetRelativeMouseMode(SDL_TRUE);
// Теперь xrel/yrel дают точные значения без системного сглаживания
```

## Окно

### SDL_WindowFlags

| Флаг                            | Описание            |
|---------------------------------|---------------------|
| `SDL_WINDOW_VULKAN`             | Окно для Vulkan     |
| `SDL_WINDOW_OPENGL`             | Окно для OpenGL     |
| `SDL_WINDOW_RESIZABLE`          | Изменяемый размер   |
| `SDL_WINDOW_FULLSCREEN`         | Полноэкранный режим |
| `SDL_WINDOW_HIDDEN`             | Изначально скрыто   |
| `SDL_WINDOW_BORDERLESS`         | Без рамки           |
| `SDL_WINDOW_HIGH_PIXEL_DENSITY` | Запрос высокого DPI |

### Размеры окна

```cpp
// Размер в оконных координатах (логические единицы)
int w, h;
SDL_GetWindowSize(window, &w, &h);

// Размер в пикселях (для framebuffer, HiDPI)
int pw, ph;
SDL_GetWindowSizeInPixels(window, &pw, &ph);
```

## Vulkan-интеграция

### Получение расширений instance

```cpp
Uint32 extensionCount = 0;
const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

// extensions — массив строк, НЕ освобождать
// Пример: {"VK_KHR_surface", "VK_KHR_win32_surface"}
```

### Создание surface

```cpp
VkSurfaceKHR surface;
if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
    std::println("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
}
```

### Интеграция с volk

```cpp
auto vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)
    SDL_Vulkan_GetVkGetInstanceProcAddr();
volkInitializeCustom(vkGetInstanceProcAddr);
```

### Уничтожение surface

```cpp
// ВАЖНО: surface ДО window
SDL_Vulkan_DestroySurface(instance, surface, nullptr);
SDL_DestroyWindow(window);
```

## Инициализация подсистем

### SDL_InitFlags

| Флаг                | Описание                               |
|---------------------|----------------------------------------|
| `SDL_INIT_VIDEO`    | Видеоподсистема (подразумевает EVENTS) |
| `SDL_INIT_AUDIO`    | Аудиоподсистема                        |
| `SDL_INIT_JOYSTICK` | Джойстики                              |
| `SDL_INIT_GAMEPAD`  | Геймпады                               |
| `SDL_INIT_HAPTIC`   | Тактильная отдача (вибрация)           |
| `SDL_INIT_SENSOR`   | Сенсоры                                |
| `SDL_INIT_CAMERA`   | Камера                                 |

```cpp
if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    std::println("SDL_Init failed: {}", SDL_GetError());
    return SDL_APP_FAILURE;
}
```

## Результаты callback-функций

### SDL_AppResult

```cpp
typedef enum SDL_AppResult {
    SDL_APP_FAILURE,  // Ошибка, завершить (exit code 1)
    SDL_APP_SUCCESS,  // Успешное завершение (exit code 0)
    SDL_APP_CONTINUE  // Продолжить работу
} SDL_AppResult;
```

## Таймеры

### SDL_GetTicks

Миллисекунды с начала работы SDL:

```cpp
Uint64 ms = SDL_GetTicks();
```

### SDL_GetPerformanceCounter

Высокоточный таймер для замеров производительности:

```cpp
Uint64 start = SDL_GetPerformanceCounter();
// ... измеряемый код ...
Uint64 end = SDL_GetPerformanceCounter();
Uint64 freq = SDL_GetPerformanceFrequency();

double seconds = static_cast<double>(end - start) / freq;
double milliseconds = seconds * 1000.0;
```

## Многопоточность

### Ограничения SDL

- **Большинство функций должны вызываться из main thread**
- События обрабатываются в main thread
- При callback-архитектуре все callbacks выполняются в main thread

> **Для понимания:** SDL — как главный дирижёр оркестра. Он должен стоять на сцене (main thread), а музыканты (
> background threads) могут играть свои партии, но не могут сами управлять оркестром.

### Правильная многопоточность

```cpp
// Фоновый поток — ТОЛЬКО CPU-работа, без SDL-вызовов
std::thread loadingThread([&]() {
    load_textures_from_disk();   // Читать файлы
    decode_audio();               // Декодировать
    // НИКАКИХ SDL-вызовов!
});

// Main thread (SDL_AppIterate)
if (loaded) {
    upload_to_gpu();  // Теперь можно
}
```

## Глоссарий

| Термин               | Определение                                                                            |
|----------------------|----------------------------------------------------------------------------------------|
| **Handle**           | Непрозрачный указатель на внутреннюю структуру SDL (`SDL_Window*`). Не разыменовывать. |
| **Callback**         | Функция, которую реализует приложение и вызывает SDL.                                  |
| **Union**            | C-структура, в которой активно одно поле. `SDL_Event` — union с полем `type`.          |
| **SDL_bool**         | Булев тип: `SDL_TRUE` / `SDL_FALSE`. Возвращаемое `true` = успех.                      |
| **Object Layer**     | Слой объекта (0-31) для управления коллизиями в физических движках.                    |
| **BroadPhase Layer** | Слой для Broad Phase — быстрая грубая фильтрация коллизий через AABB-деревья.          |

## Ключевые функции

| Задача            | Функция                                                        |
|-------------------|----------------------------------------------------------------|
| Инициализация     | `SDL_Init(flags)`                                              |
| Создание окна     | `SDL_CreateWindow(title, w, h, flags)`                         |
| Уничтожение окна  | `SDL_DestroyWindow(window)`                                    |
| Опрос события     | `SDL_PollEvent(event)` (только без callbacks!)                 |
| Vulkan extensions | `SDL_Vulkan_GetInstanceExtensions(count)`                      |
| Vulkan surface    | `SDL_Vulkan_CreateSurface(window, instance, ...)`              |
| Ошибка            | `SDL_GetError()`                                               |
| Таймер            | `SDL_GetPerformanceCounter()`, `SDL_GetPerformanceFrequency()` |

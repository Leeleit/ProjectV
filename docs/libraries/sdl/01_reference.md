## SDL3

<!-- anchor: 00_overview -->


**SDL3** (Simple DirectMedia Layer) — библиотека низкого уровня для доступа к аудио, клавиатуре, мыши, джойстикам и
графическому оборудованию через OpenGL, Direct3D и Vulkan. Предназначена для создания мультимедийных приложений, игр и
инструментов.

Версия: **SDL 3.x** (активная разработка).
Исходники: [libsdl.org](https://libsdl.org), [GitHub](https://github.com/libsdl-org/SDL).

---

## Назначение

SDL3 решает задачу кроссплатформенного доступа к аппаратным ресурсам:

- **Окна** — создание и управление окнами на разных платформах
- **Ввод** — обработка событий клавиатуры, мыши, геймпадов
- **Графика** — интеграция с Vulkan, OpenGL, Direct3D через platform-specific поверхности
- **Аудио** — воспроизведение и захват звука

SDL не предоставляет высокоуровневого рендеринга — только окно и поверхность для рисования графическим API.

---

## Возможности

| Категория            | Функциональность                                                          |
|----------------------|---------------------------------------------------------------------------|
| **Видео**            | Создание окон, управление режимами отображения, Vulkan/OpenGL поверхности |
| **События**          | Обработка ввода, оконные события, пользовательские события                |
| **Ввод**             | Клавиатура, мышь, геймпады, джойстики, сенсорные экраны                   |
| **Аудио**            | Воспроизведение и захват звука                                            |
| **Файловая система** | Пути к ресурсам, Preferences, clipboard                                   |
| **Таймеры**          | Высокоточные таймеры, задержки                                            |

---

## Поддерживаемые платформы

| Платформа      | Описание       |
|----------------|----------------|
| **Windows**    | Win32, UWP     |
| **Linux**      | X11, Wayland   |
| **macOS**      | Cocoa          |
| **iOS**        | UIKit          |
| **Android**    | NativeActivity |
| **Emscripten** | WebAssembly    |

---

## Требования

### Языки и стандарты

- **C11** или новее
- **C++11** или новее

### Платформенные зависимости

Библиотека автоматически линкует необходимые системные библиотеки:

| Платформа   | Зависимости                                                                                                                                               |
|-------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Windows** | Нет внешних зависимостей                                                                                                                                  |
| **Linux**   | `-lpthread -ldl -lm -lrt`                                                                                                                                 |
| **macOS**   | `-framework CoreFoundation -framework CoreAudio -framework AudioToolbox -framework ForceFeedback -framework Carbon -framework IOKit -framework CoreVideo` |
| **Android** | `-llog -landroid`                                                                                                                                         |

---

## Архитектура

SDL3 предоставляет два режима работы приложения:

### Классический main()

Приложение управляет циклом событий самостоятельно:

```cpp
int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("App", 800, 600, 0);

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
        }
        // Рендеринг
    }

    SDL_Quit();
    return 0;
}
```

### Callback архитектура (SDL_MAIN_USE_CALLBACKS)

SDL управляет циклом, вызывая callback-функции:

```cpp
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]);
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event);
SDL_AppResult SDL_AppIterate(void* appstate);
void SDL_AppQuit(void* appstate, SDL_AppResult result);
```

Callback архитектура рекомендуется для кроссплатформенных проектов, особенно с поддержкой мобильных платформ.

---

## Модули

SDL3 разделён на подсистемы, инициализируемые по необходимости:

| Флаг                | Подсистема                                               |
|---------------------|----------------------------------------------------------|
| `SDL_INIT_VIDEO`    | Видеоподсистема и окна (подразумевает `SDL_INIT_EVENTS`) |
| `SDL_INIT_AUDIO`    | Аудио                                                    |
| `SDL_INIT_JOYSTICK` | Джойстики                                                |
| `SDL_INIT_GAMEPAD`  | Геймпады                                                 |
| `SDL_INIT_HAPTIC`   | Тактильная отдача (вибрация)                             |
| `SDL_INIT_SENSOR`   | Сенсоры                                                  |
| `SDL_INIT_CAMERA`   | Камера                                                   |

---

## Основные понятия SDL3

<!-- anchor: 02_concepts -->


Фундаментальные концепции API SDL3, необходимые для понимания работы с оконной системой и событиями.

---

## main() vs SDL_MAIN_USE_CALLBACKS

SDL3 предлагает два режима входа в приложение.

### Классический main()

Приложение само управляет циклом событий:

```cpp
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

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
        // Обновление и рендеринг
    }

    SDL_Quit();
    return 0;
}
```

### Callback архитектура

SDL управляет циклом и вызывает callbacks:

```cpp
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    // Инициализация
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    // Обработка события
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    // Один кадр: обновление, рендеринг
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    // Освобождение ресурсов
}
```

### Сравнение подходов

| Критерий            | Классический main() | Callback архитектура         |
|---------------------|---------------------|------------------------------|
| Контроль над циклом | Полный              | Ограниченный (SDL управляет) |
| Мобильные платформы | Требует адаптации   | Нативная поддержка           |
| Сложность           | Выше (boilerplate)  | Ниже                         |
| Гибкость            | Полная              | Ограничена интерфейсом       |

Callback архитектура рекомендуется для кроссплатформенных проектов.

---

## Жизненный цикл приложения

```
┌─────────────────────────────────────────────────────┐
│                    Запуск                           │
└─────────────────────┬───────────────────────────────┘
                      ▼
┌─────────────────────────────────────────────────────┐
│              SDL_AppInit()                          │
│  - SDL_Init, SDL_CreateWindow                       │
│  - return SDL_APP_CONTINUE | SDL_APP_FAILURE        │
└─────────────────────┬───────────────────────────────┘
                      ▼
          ┌───────────────────────┐
          │   SDL_APP_CONTINUE?   │
          └───────────┬───────────┘
                      │
        ┌─────────────┴─────────────┐
        ▼                           ▼
┌───────────────┐           ┌───────────────┐
│ SDL_AppEvent  │           │ SDL_AppIterate│
│ (каждое       │           │ (каждый кадр) │
│  событие)     │           │               │
└───────┬───────┘           └───────┬───────┘
        │                           │
        └─────────────┬─────────────┘
                      ▼
          ┌───────────────────────┐
          │  SDL_APP_CONTINUE?    │
          └───────────┬───────────┘
                      │ Да
        ┌─────────────┴─────────────┐
        ▼                           ▼
┌───────────────┐           ┌───────────────┐
│ SDL_AppEvent  │           │ SDL_AppIterate│
│     ...       │           │     ...       │
└───────────────┘           └───────────────┘
                      │
                      │ Нет (SUCCESS/FAILURE)
                      ▼
┌─────────────────────────────────────────────────────┐
│              SDL_AppQuit()                          │
│  - SDL_DestroyWindow, очистка ресурсов             │
└─────────────────────────────────────────────────────┘
```

---

## Event Loop

### SDL_AppEvent vs SDL_PollEvent

| Режим                      | Получение событий                               | Обработка             |
|----------------------------|-------------------------------------------------|-----------------------|
| **SDL_MAIN_USE_CALLBACKS** | SDL вызывает `SDL_AppEvent` для каждого события | В теле `SDL_AppEvent` |
| **main()**                 | `while (SDL_PollEvent(&event))`                 | В теле цикла          |

При callback архитектуре **не вызывайте** `SDL_PollEvent` — события доставляются автоматически.

---

## Типы событий

### SDL_EVENT_QUIT и SDL_EVENT_WINDOW_CLOSE_REQUESTED

Это разные события с разной семантикой:

| Событие                              | Когда приходит                                      | Роль                                          |
|--------------------------------------|-----------------------------------------------------|-----------------------------------------------|
| **SDL_EVENT_WINDOW_CLOSE_REQUESTED** | Пользователь нажал крестик на окне                  | Специфично для окна (`event.window.windowID`) |
| **SDL_EVENT_QUIT**                   | Глобальный выход (закрытие последнего окна, Alt+F4) | Выход из приложения                           |

Порядок: сначала `SDL_EVENT_WINDOW_CLOSE_REQUESTED`, затем возможно `SDL_EVENT_QUIT`.

### Часто используемые события

| Тип                                   | Когда                                   |
|---------------------------------------|-----------------------------------------|
| `SDL_EVENT_QUIT`                      | Глобальный выход                        |
| `SDL_EVENT_WINDOW_CLOSE_REQUESTED`    | Крестик на окне                         |
| `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` | Изменение размера в пикселях (HiDPI)    |
| `SDL_EVENT_WINDOW_RESIZED`            | Изменение размера в оконных координатах |
| `SDL_EVENT_KEY_DOWN`                  | Клавиша нажата                          |
| `SDL_EVENT_KEY_UP`                    | Клавиша отпущена                        |
| `SDL_EVENT_MOUSE_MOTION`              | Движение мыши                           |
| `SDL_EVENT_MOUSE_BUTTON_DOWN`         | Нажатие кнопки мыши                     |
| `SDL_EVENT_MOUSE_BUTTON_UP`           | Отпускание кнопки мыши                  |

---

## SDL_Event

Union всех типов событий. Поле `type` определяет активное поле union:

```cpp
SDL_Event event;
// После получения события:
switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
        // event.key — структура SDL_KeyboardEvent
        if (event.key.key == SDLK_ESCAPE) { /* ... */ }
        break;

    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        // event.window — структура SDL_WindowEvent
        // event.window.windowID — ID окна
        break;

    case SDL_EVENT_MOUSE_MOTION:
        // event.motion — структура SDL_MouseMotionEvent
        // event.motion.x, event.motion.y — координаты
        break;
}
```

Данные события **не сохраняются** после возврата из callback — нельзя хранить указатели на поля события.

---

## Клавиатура

### event.key (SDL_KeyboardEvent)

| Поле         | Тип            | Описание                                                   |
|--------------|----------------|------------------------------------------------------------|
| **key**      | `SDL_Keycode`  | Виртуальный код (зависит от раскладки): `SDLK_a`, `SDLK_w` |
| **scancode** | `SDL_Scancode` | Физическая клавиша (не зависит от раскладки)               |
| **mod**      | `SDL_Keymod`   | Модификаторы (Shift, Ctrl, Alt)                            |
| **repeat**   | `bool`         | Автоповтор нажатой клавиши                                 |

Для игр (WASD) предпочтительнее `scancode` — одна клавиша на любой раскладке.

---

## Окно

### Размеры окна

Два способа получения размера:

| Функция                       | Возвращает                                        |
|-------------------------------|---------------------------------------------------|
| `SDL_GetWindowSize()`         | Размер в оконных координатах (логические единицы) |
| `SDL_GetWindowSizeInPixels()` | Размер в пикселях (для HiDPI)                     |

Для создания Vulkan/OpenGL framebuffer используйте `SDL_GetWindowSizeInPixels()`.

### SDL_WindowFlags

| Флаг                            | Описание                 |
|---------------------------------|--------------------------|
| `SDL_WINDOW_VULKAN`             | Окно поддерживает Vulkan |
| `SDL_WINDOW_OPENGL`             | Окно поддерживает OpenGL |
| `SDL_WINDOW_RESIZABLE`          | Окно можно изменять      |
| `SDL_WINDOW_FULLSCREEN`         | Полноэкранный режим      |
| `SDL_WINDOW_HIDDEN`             | Окно изначально скрыто   |
| `SDL_WINDOW_BORDERLESS`         | Без рамки                |
| `SDL_WINDOW_HIGH_PIXEL_DENSITY` | Запрос высокого DPI      |

---

## Мышь

### event.motion (SDL_MouseMotionEvent)

| Поле           | Описание               |
|----------------|------------------------|
| `x`, `y`       | Координаты курсора     |
| `xrel`, `yrel` | Относительное смещение |
| `state`        | Состояние кнопок       |

### event.button (SDL_MouseButtonEvent)

| Поле     | Описание                                             |
|----------|------------------------------------------------------|
| `button` | Кнопка (`SDL_BUTTON_LEFT`, `SDL_BUTTON_RIGHT` и др.) |
| `clicks` | Количество кликов (double click = 2)                 |
| `x`, `y` | Координаты                                           |

---

## Результаты callbacks

### SDL_AppResult

```cpp
typedef enum SDL_AppResult {
    SDL_APP_FAILURE,  // Ошибка, завершить
    SDL_APP_SUCCESS,  // Успешное завершение
    SDL_APP_CONTINUE  // Продолжить работу
} SDL_AppResult;
```

| Значение           | Семантика                           |
|--------------------|-------------------------------------|
| `SDL_APP_CONTINUE` | Приложение продолжает работу        |
| `SDL_APP_SUCCESS`  | Нормальное завершение (exit code 0) |
| `SDL_APP_FAILURE`  | Завершение с ошибкой (exit code 1)  |

---

## Многопоточность

SDL не является полностью thread-safe:

- Большинство функций должны вызываться из main thread
- События обрабатываются в main thread
- При callback архитектуре все callbacks выполняются в main thread

Для фоновых задач используйте отдельные потоки, но не вызывайте SDL функции из них.

---

## Справочник API SDL3

<!-- anchor: 05_api-reference -->

Краткое описание функций и структур SDL3 для работы с окнами и событиями. Полная
документация: [SDL Wiki](https://wiki.libsdl.org/SDL3/FrontPage).

---

## Когда что использовать

| Задача                   | Функция                                                        | Заголовок      |
|--------------------------|----------------------------------------------------------------|----------------|
| Инициализация SDL        | `SDL_Init()`                                                   | `SDL_init.h`   |
| Создание окна            | `SDL_CreateWindow()`                                           | `SDL_video.h`  |
| Уничтожение окна         | `SDL_DestroyWindow()`                                          | `SDL_video.h`  |
| Размер окна (пиксели)    | `SDL_GetWindowSizeInPixels()`                                  | `SDL_video.h`  |
| Размер окна (логический) | `SDL_GetWindowSize()`                                          | `SDL_video.h`  |
| Vulkan расширения        | `SDL_Vulkan_GetInstanceExtensions()`                           | `SDL_vulkan.h` |
| Vulkan поверхность       | `SDL_Vulkan_CreateSurface()`                                   | `SDL_vulkan.h` |
| Метаданные приложения    | `SDL_SetAppMetadata()`                                         | `SDL_init.h`   |
| Callbacks вместо main()  | `SDL_AppInit`, `SDL_AppEvent`, `SDL_AppIterate`, `SDL_AppQuit` | `SDL_main.h`   |
| Ошибка SDL               | `SDL_GetError()`                                               | `SDL_error.h`  |
| Логирование              | `SDL_Log()`, `SDL_LogError()`                                  | `SDL_log.h`    |

---

## Инициализация

### SDL_Init

```c
bool SDL_Init(SDL_InitFlags flags);
```

Инициализирует подсистемы SDL.

**Параметры:**

- `flags` — комбинация флагов: `SDL_INIT_VIDEO`, `SDL_INIT_AUDIO`, `SDL_INIT_JOYSTICK`, `SDL_INIT_GAMEPAD`

**Возвращает:** `true` при успехе, `false` при ошибке.

```cpp
if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init failed: %s", SDL_GetError());
    return -1;
}
```

---

### SDL_Quit

```c
void SDL_Quit(void);
```

Деинициализирует все подсистемы. При `SDL_MAIN_USE_CALLBACKS` вызывается автоматически после `SDL_AppQuit`.

---

### SDL_SetAppMetadata

```c
bool SDL_SetAppMetadata(const char* appname, const char* appversion, const char* appidentifier);
```

Задаёт метаданные приложения.

**Параметры:**

- `appname` — имя приложения
- `appversion` — версия (например, `"1.0.0"`)
- `appidentifier` — идентификатор (например, `"com.example.app"`)

```cpp
SDL_SetAppMetadata("MyApp", "1.0.0", "com.example.myapp");
```

---

### SDL_InitFlags

| Флаг                | Описание                                          |
|---------------------|---------------------------------------------------|
| `SDL_INIT_VIDEO`    | Видеоподсистема (подразумевает `SDL_INIT_EVENTS`) |
| `SDL_INIT_AUDIO`    | Аудио подсистема                                  |
| `SDL_INIT_JOYSTICK` | Джойстики                                         |
| `SDL_INIT_GAMEPAD`  | Геймпады                                          |
| `SDL_INIT_HAPTIC`   | Вибрация                                          |
| `SDL_INIT_SENSOR`   | Сенсоры                                           |
| `SDL_INIT_CAMERA`   | Камера                                            |

---

## Окно

### SDL_CreateWindow

```c
SDL_Window* SDL_CreateWindow(const char* title, int w, int h, SDL_WindowFlags flags);
```

Создаёт окно.

**Параметры:**

- `title` — заголовок окна
- `w`, `h` — ширина и высота
- `flags` — комбинация флагов окна

**Возвращает:** handle окна или `NULL` при ошибке.

```cpp
SDL_Window* window = SDL_CreateWindow(
    "My Window",
    1280, 720,
    SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
);
```

---

### SDL_DestroyWindow

```c
void SDL_DestroyWindow(SDL_Window* window);
```

Уничтожает окно. При Vulkan необходимо уничтожить surface **до** вызова этой функции.

---

### SDL_GetWindowSize

```c
bool SDL_GetWindowSize(SDL_Window* window, int* w, int* h);
```

Возвращает размер окна в оконных координатах (логические единицы). На HiDPI может отличаться от размера в пикселях.

---

### SDL_GetWindowSizeInPixels

```c
bool SDL_GetWindowSizeInPixels(SDL_Window* window, int* w, int* h);
```

Возвращает размер окна в пикселях. Использовать для создания Vulkan/OpenGL framebuffer.

---

### SDL_GetWindowID

```c
SDL_WindowID SDL_GetWindowID(SDL_Window* window);
```

Возвращает уникальный ID окна. Используется для идентификации окна в событиях.

---

### SDL_WindowFlags

| Флаг                            | Описание                 |
|---------------------------------|--------------------------|
| `SDL_WINDOW_VULKAN`             | Окно поддерживает Vulkan |
| `SDL_WINDOW_OPENGL`             | Окно поддерживает OpenGL |
| `SDL_WINDOW_RESIZABLE`          | Окно можно изменять      |
| `SDL_WINDOW_FULLSCREEN`         | Полноэкранный режим      |
| `SDL_WINDOW_HIDDEN`             | Окно скрыто              |
| `SDL_WINDOW_BORDERLESS`         | Без рамки                |
| `SDL_WINDOW_HIGH_PIXEL_DENSITY` | Запрос высокого DPI      |
| `SDL_WINDOW_MOUSE_GRABBED`      | Захват мыши              |

---

## Main callbacks

### SDL_AppInit

```c
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]);
```

Callback инициализации. Вызывается один раз при старте.

**Параметры:**

- `appstate` — записать указатель на состояние приложения
- `argc`, `argv` — аргументы командной строки

**Возвращает:** `SDL_APP_CONTINUE`, `SDL_APP_SUCCESS` или `SDL_APP_FAILURE`

---

### SDL_AppEvent

```c
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event);
```

Callback события. Вызывается для каждого события.

**Параметры:**

- `appstate` — состояние приложения
- `event` — событие (не сохранять указатель)

**Возвращает:** `SDL_APP_SUCCESS` для завершения, `SDL_APP_CONTINUE` для продолжения.

---

### SDL_AppIterate

```c
SDL_AppResult SDL_AppIterate(void* appstate);
```

Callback кадра. Вызывается каждый кадр для обновления и рендеринга.

---

### SDL_AppQuit

```c
void SDL_AppQuit(void* appstate, SDL_AppResult result);
```

Callback завершения. Вызывается перед выходом для освобождения ресурсов.

---

### SDL_AppResult

```c
typedef enum SDL_AppResult {
    SDL_APP_FAILURE,  // Ошибка
    SDL_APP_SUCCESS,  // Успешное завершение
    SDL_APP_CONTINUE  // Продолжить работу
} SDL_AppResult;
```

---

## События

### SDL_Event

Union всех типов событий. Поле `type` определяет активное поле:

```cpp
SDL_Event event;
switch (event.type) {
    case SDL_EVENT_QUIT:
        // event.quit
        break;
    case SDL_EVENT_KEY_DOWN:
        // event.key
        break;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        // event.window
        break;
}
```

---

### SDL_EventType

| Тип                                   | Структура      | Описание                        |
|---------------------------------------|----------------|---------------------------------|
| `SDL_EVENT_QUIT`                      | `event.quit`   | Глобальный выход                |
| `SDL_EVENT_WINDOW_CLOSE_REQUESTED`    | `event.window` | Запрос закрытия окна            |
| `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` | `event.window` | Изменение размера в пикселях    |
| `SDL_EVENT_WINDOW_RESIZED`            | `event.window` | Изменение размера (логического) |
| `SDL_EVENT_KEY_DOWN`                  | `event.key`    | Клавиша нажата                  |
| `SDL_EVENT_KEY_UP`                    | `event.key`    | Клавиша отпущена                |
| `SDL_EVENT_MOUSE_MOTION`              | `event.motion` | Движение мыши                   |
| `SDL_EVENT_MOUSE_BUTTON_DOWN`         | `event.button` | Нажатие кнопки мыши             |
| `SDL_EVENT_MOUSE_BUTTON_UP`           | `event.button` | Отпускание кнопки мыши          |

---

### SDL_WindowEvent

Поля для оконных событий:

| Поле       | Описание                    |
|------------|-----------------------------|
| `windowID` | ID окна                     |
| `data1`    | Зависит от типа (ширина, x) |
| `data2`    | Зависит от типа (высота, y) |

Для `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED`: `data1` = ширина, `data2` = высота в пикселях.

---

### SDL_KeyboardEvent

Поля для клавиатурных событий:

| Поле       | Тип            | Описание                                  |
|------------|----------------|-------------------------------------------|
| `key`      | `SDL_Keycode`  | Виртуальный код (`SDLK_a`, `SDLK_ESCAPE`) |
| `scancode` | `SDL_Scancode` | Физическая клавиша                        |
| `mod`      | `SDL_Keymod`   | Модификаторы (Shift, Ctrl, Alt)           |
| `repeat`   | `bool`         | Автоповтор                                |

---

### SDL_MouseMotionEvent

| Поле           | Описание               |
|----------------|------------------------|
| `x`, `y`       | Координаты курсора     |
| `xrel`, `yrel` | Относительное смещение |
| `state`        | Состояние кнопок       |

---

### SDL_MouseButtonEvent

| Поле     | Описание                                                            |
|----------|---------------------------------------------------------------------|
| `button` | Кнопка (`SDL_BUTTON_LEFT`, `SDL_BUTTON_RIGHT`, `SDL_BUTTON_MIDDLE`) |
| `clicks` | Количество кликов                                                   |
| `x`, `y` | Координаты                                                          |

---

### SDL_PollEvent

```c
bool SDL_PollEvent(SDL_Event* event);
```

Извлекает следующее событие из очереди.

**Возвращает:** `true` если событие получено, `false` если очередь пуста.

При `SDL_MAIN_USE_CALLBACKS` не использовать — события приходят в `SDL_AppEvent`.

---

## Vulkan

### SDL_Vulkan_GetInstanceExtensions

```c
const char* const* SDL_Vulkan_GetInstanceExtensions(Uint32* count);
```

Возвращает массив имён расширений для VkInstance.

**Параметры:**

- `count` — заполняется числом расширений

**Возвращает:** массив строк (не освобождать) или `NULL` при ошибке.

---

### SDL_Vulkan_CreateSurface

```c
bool SDL_Vulkan_CreateSurface(
    SDL_Window* window,
    VkInstance instance,
    const VkAllocationCallbacks* allocator,
    VkSurfaceKHR* surface
);
```

Создаёт VkSurfaceKHR для окна.

**Параметры:**

- `window` — окно с `SDL_WINDOW_VULKAN`
- `instance` — VkInstance с расширениями от SDL
- `allocator` — можно `nullptr`
- `surface` — выходной VkSurfaceKHR

**Возвращает:** `true` при успехе.

---

### SDL_Vulkan_DestroySurface

```c
void SDL_Vulkan_DestroySurface(
    VkInstance instance,
    VkSurfaceKHR surface,
    const VkAllocationCallbacks* allocator
);
```

Уничтожает surface. Вызывать до `SDL_DestroyWindow`.

---

### SDL_Vulkan_GetVkGetInstanceProcAddr

```c
SDL_FunctionPointer SDL_Vulkan_GetVkGetInstanceProcAddr(void);
```

Возвращает указатель на `vkGetInstanceProcAddr`. Привести к `PFN_vkGetInstanceProcAddr` для использования с volk.

```cpp
auto fn = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
volkInitializeCustom(fn);
```

---

### SDL_Vulkan_GetPresentationSupport

```c
bool SDL_Vulkan_GetPresentationSupport(
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    Uint32 queueFamilyIndex
);
```

Проверяет поддержку presentation для queue family.

---

### SDL_Vulkan_LoadLibrary / SDL_Vulkan_UnloadLibrary

```c
bool SDL_Vulkan_LoadLibrary(const char* path);
void SDL_Vulkan_UnloadLibrary(void);
```

Явная загрузка/выгрузка Vulkan loader. Обычно не требуется — SDL загружает loader автоматически при создании окна с
`SDL_WINDOW_VULKAN`.

---

## Ошибки и логирование

### SDL_GetError

```c
const char* SDL_GetError(void);
```

Возвращает строку с описанием последней ошибки. Потокобезопасна.

---

### SDL_Log

```c
void SDL_Log(const char* fmt, ...);
void SDL_LogError(int category, const char* fmt, ...);
void SDL_LogWarn(int category, const char* fmt, ...);
```

Логирование с форматированием.

**Категории:**

- `SDL_LOG_CATEGORY_APPLICATION`
- `SDL_LOG_CATEGORY_ERROR`
- `SDL_LOG_CATEGORY_VIDEO`
- `SDL_LOG_CATEGORY_AUDIO`

```cpp
SDL_Log("Window created: %dx%d", width, height);
SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Failed to create window: %s", SDL_GetError());
```

---

## Таймеры

### SDL_GetTicks

```c
Uint64 SDL_GetTicks(void);
```

Возвращает количество миллисекунд с начала работы SDL.

---

### SDL_GetPerformanceCounter

```c
Uint64 SDL_GetPerformanceCounter(void);
Uint64 SDL_GetPerformanceFrequency(void);
```

Высокоточный таймер.

```cpp
Uint64 start = SDL_GetPerformanceCounter();
// ... работа ...
Uint64 end = SDL_GetPerformanceCounter();
Uint64 freq = SDL_GetPerformanceFrequency();
double seconds = (double)(end - start) / freq;
```

---

### SDL_Delay

```c
void SDL_Delay(Uint32 ms);
```

Задержка выполнения в миллисекундах.

---

## Глоссарий SDL3

<!-- anchor: 10_glossary -->


Термины и определения, используемые в документации SDL3.

---

## Базовые понятия

| Термин             | Определение                                                                                                                   |
|--------------------|-------------------------------------------------------------------------------------------------------------------------------|
| **Handle**         | Непрозрачный указатель на внутреннюю структуру SDL (например, `SDL_Window*`). Не разыменовывается напрямую.                   |
| **Callback**       | Функция, которую реализует приложение и которую SDL вызывает при наступлении события.                                         |
| **Union**          | C-структура, в которой в каждый момент активно одно из полей. `SDL_Event` — union с полем `type`, определяющим активное поле. |
| **SDL_bool**       | Булев тип SDL: `SDL_TRUE` / `SDL_FALSE`. Возвращаемое значение функций: `true` = успех.                                       |
| **Uint32, Uint64** | Целочисленные типы SDL фиксированного размера.                                                                                |

---

## Инициализация

| Термин                 | Определение                                                                                        |
|------------------------|----------------------------------------------------------------------------------------------------|
| **SDL_Init**           | Функция инициализации подсистем SDL. Принимает флаги (`SDL_INIT_VIDEO` и др.).                     |
| **SDL_Quit**           | Деинициализация всех подсистем. Вызывать перед выходом.                                            |
| **SDL_InitFlags**      | Флаги для `SDL_Init`: `SDL_INIT_VIDEO`, `SDL_INIT_AUDIO`, `SDL_INIT_JOYSTICK`, `SDL_INIT_GAMEPAD`. |
| **SDL_INIT_VIDEO**     | Флаг инициализации видеоподсистемы. Подразумевает `SDL_INIT_EVENTS`. Обязателен для создания окна. |
| **SDL_SetAppMetadata** | Установка метаданных приложения (имя, версия, идентификатор). Опционально.                         |

---

## Окно

| Термин                        | Определение                                                                                          |
|-------------------------------|------------------------------------------------------------------------------------------------------|
| **SDL_Window**                | Handle окна. Создаётся через `SDL_CreateWindow`, уничтожается через `SDL_DestroyWindow`.             |
| **SDL_WindowID**              | Уникальный числовой ID окна (`Uint32`). Используется в событиях для идентификации окна.              |
| **SDL_WindowFlags**           | Флаги окна: `SDL_WINDOW_VULKAN`, `SDL_WINDOW_OPENGL`, `SDL_WINDOW_RESIZABLE` и др.                   |
| **SDL_WINDOW_VULKAN**         | Флаг, указывающий что окно будет использоваться с Vulkan. Обязателен для `SDL_Vulkan_CreateSurface`. |
| **SDL_GetWindowSize**         | Возвращает размер окна в оконных координатах (логические единицы).                                   |
| **SDL_GetWindowSizeInPixels** | Возвращает размер окна в пикселях. Для HiDPI отличается от оконного размера.                         |

---

## События

| Термин                                  | Определение                                                                                    |
|-----------------------------------------|------------------------------------------------------------------------------------------------|
| **SDL_Event**                           | Union всех типов событий. Поле `type` определяет активное поле union.                          |
| **SDL_EventType**                       | Перечисление типов событий: `SDL_EVENT_QUIT`, `SDL_EVENT_KEY_DOWN`, `SDL_EVENT_WINDOW_*` и др. |
| **SDL_EVENT_QUIT**                      | Событие глобального выхода из приложения.                                                      |
| **SDL_EVENT_WINDOW_CLOSE_REQUESTED**    | Запрос закрытия конкретного окна (крестик). Приходит до `SDL_EVENT_QUIT`.                      |
| **SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED** | Изменение размера окна в пикселях (resize, DPI). Использовать для пересоздания swapchain.      |
| **SDL_EVENT_KEY_DOWN**                  | Нажатие клавиши. Данные в `event.key`.                                                         |
| **SDL_PollEvent**                       | Извлечение события из очереди. Не использовать при `SDL_MAIN_USE_CALLBACKS`.                   |

---

## Клавиатура

| Термин               | Определение                                                                             |
|----------------------|-----------------------------------------------------------------------------------------|
| **SDL_Keycode**      | Виртуальный код клавиши (зависит от раскладки): `SDLK_a`, `SDLK_ESCAPE`, `SDLK_RETURN`. |
| **SDL_Scancode**     | Физический код клавиши (не зависит от раскладки). Для WASD предпочтительнее.            |
| **SDL_Keymod**       | Модификаторы: `SDL_KMOD_SHIFT`, `SDL_KMOD_CTRL`, `SDL_KMOD_ALT`.                        |
| **event.key.repeat** | Флаг автоповтора при зажатой клавише.                                                   |

---

## Main callbacks

| Термин                     | Определение                                                                              |
|----------------------------|------------------------------------------------------------------------------------------|
| **SDL_MAIN_USE_CALLBACKS** | Макрос для включения callback архитектуры. Определяется до `#include "SDL3/SDL_main.h"`. |
| **SDL_MAIN_HANDLED**       | Макрос для ручного управления entry point. Приложение вызывает `SDL_SetMainReady()`.     |
| **SDL_AppInit**            | Callback инициализации. Вызывается один раз при старте.                                  |
| **SDL_AppEvent**           | Callback события. Вызывается для каждого события.                                        |
| **SDL_AppIterate**         | Callback кадра. Вызывается каждый кадр для обновления и рендеринга.                      |
| **SDL_AppQuit**            | Callback завершения. Вызывается перед выходом для очистки ресурсов.                      |
| **SDL_AppResult**          | Результат callback: `SDL_APP_CONTINUE`, `SDL_APP_SUCCESS`, `SDL_APP_FAILURE`.            |
| **appstate**               | Указатель на состояние приложения. Передаётся во все callbacks.                          |

---

## Vulkan

| Термин                                  | Определение                                                            |
|-----------------------------------------|------------------------------------------------------------------------|
| **SDL_Vulkan_GetInstanceExtensions**    | Возвращает массив имён расширений для `VkInstance`.                    |
| **SDL_Vulkan_CreateSurface**            | Создаёт `VkSurfaceKHR` для окна. Требует `SDL_WINDOW_VULKAN`.          |
| **SDL_Vulkan_DestroySurface**           | Уничтожает surface. Вызывать до `SDL_DestroyWindow`.                   |
| **SDL_Vulkan_GetVkGetInstanceProcAddr** | Возвращает указатель на `vkGetInstanceProcAddr` для интеграции с volk. |
| **SDL_Vulkan_LoadLibrary**              | Явная загрузка Vulkan loader. Обычно не требуется.                     |
| **SDL_Vulkan_GetPresentationSupport**   | Проверка поддержки presentation для queue family.                      |
| **VkSurfaceKHR**                        | Vulkan-поверхность, связывающая Vulkan с окном.                        |

---

## Hints

| Термин                      | Определение                                         |
|-----------------------------|-----------------------------------------------------|
| **SDL_HINT_VULKAN_LIBRARY** | Путь к Vulkan loader. Задаётся через `SDL_SetHint`. |
| **SDL_HINT_VULKAN_DISPLAY** | Индекс дисплея для `SDL_Vulkan_CreateSurface`.      |
| **SDL_SetHint**             | Установка hint (настройки) SDL.                     |

---

## Мышь

| Термин                       | Определение                                                                      |
|------------------------------|----------------------------------------------------------------------------------|
| **SDL_BUTTON_LEFT**          | Левая кнопка мыши.                                                               |
| **SDL_BUTTON_RIGHT**         | Правая кнопка мыши.                                                              |
| **SDL_BUTTON_MIDDLE**        | Средняя кнопка мыши.                                                             |
| **SDL_SetRelativeMouseMode** | Включение относительного режима мыши (скрытый курсор, относительные координаты). |
| **Raw Input**                | Прямой доступ к данным устройства, обход системного сглаживания.                 |

---

## Таймеры

| Термин                          | Определение                                              |
|---------------------------------|----------------------------------------------------------|
| **SDL_GetTicks**                | Миллисекунды с начала работы SDL.                        |
| **SDL_GetPerformanceCounter**   | Высокоточный счётчик. Использовать для точных измерений. |
| **SDL_GetPerformanceFrequency** | Частота высокоточного счётчика.                          |
| **SDL_Delay**                   | Задержка выполнения в миллисекундах.                     |

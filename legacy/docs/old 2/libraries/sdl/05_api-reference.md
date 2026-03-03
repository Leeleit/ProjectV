# Справочник API SDL3

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

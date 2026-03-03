# Справочник API SDL3

Краткое описание функций и структур SDL3, нужных для игры на Vulkan. Полный
перечень — [SDL Wiki](https://wiki.libsdl.org/SDL3/FrontPage).
Исходники: [SDL_events.h](../../external/SDL/include/SDL3/SDL_events.h), [SDL_keycode.h](../../external/SDL/include/SDL3/SDL_keycode.h), [SDL_video.h](../../external/SDL/include/SDL3/SDL_video.h), [SDL_vulkan.h](../../external/SDL/include/SDL3/SDL_vulkan.h).

## На этой странице

- [Когда что использовать](#когда-что-использовать)
- [Init](#init)
- [Window](#window)
- [Main callbacks](#main-callbacks)
- [Events](#events)
- [Vulkan](#vulkan)
- [Hints (Vulkan)](#hints-vulkan)
- [Ошибки и логирование](#ошибки-и-логирование)

---

## Когда что использовать

| Задача                                | Функция / API                                                  | Заголовок    |
|---------------------------------------|----------------------------------------------------------------|--------------|
| Инициализация SDL                     | `SDL_Init(SDL_INIT_VIDEO)`                                     | SDL_init.h   |
| Создание окна для Vulkan              | `SDL_CreateWindow(..., SDL_WINDOW_VULKAN)`                     | SDL_video.h  |
| Уничтожение окна                      | `SDL_DestroyWindow(window)`                                    | SDL_video.h  |
| Получить расширения для Vulkan        | `SDL_Vulkan_GetInstanceExtensions`                             | SDL_vulkan.h |
| Создать Vulkan-поверхность            | `SDL_Vulkan_CreateSurface`                                     | SDL_vulkan.h |
| Загрузка Vulkan loader через SDL      | `SDL_Vulkan_LoadLibrary`                                       | SDL_vulkan.h |
| Получить vkGetInstanceProcAddr от SDL | `SDL_Vulkan_GetVkGetInstanceProcAddr`                          | SDL_vulkan.h |
| Метаданные приложения                 | `SDL_SetAppMetadata`                                           | SDL_init.h   |
| Размер окна в пикселях                | `SDL_GetWindowSizeInPixels`                                    | SDL_video.h  |
| Callbacks вместо main()               | `SDL_AppInit`, `SDL_AppEvent`, `SDL_AppIterate`, `SDL_AppQuit` | SDL_main.h   |
| Presentation support                  | `SDL_Vulkan_GetPresentationSupport`                            | SDL_vulkan.h |
| Ошибка SDL                            | `SDL_GetError()`                                               | SDL_error.h  |
| Логирование                           | `SDL_Log`, `SDL_LogError`                                      | SDL_log.h    |

---

## Init

### SDL_Init

```c
bool SDL_Init(SDL_InitFlags flags);
```

Инициализирует подсистемы SDL. Для игры с Vulkan обычно достаточно `SDL_INIT_VIDEO`.

- **flags** — `SDL_INIT_VIDEO`, `SDL_INIT_AUDIO`, `SDL_INIT_JOYSTICK`, `SDL_INIT_GAMEPAD` и др. (можно комбинировать
  через `|`)
- **Возвращает:** `true` при успехе, `false` при ошибке. После ошибки — `SDL_GetError()`.

---

### SDL_Quit

```c
void SDL_Quit(void);
```

Деинициализирует все подсистемы. При `SDL_MAIN_USE_CALLBACKS` SDL вызывает сам после `SDL_AppQuit`.

---

### SDL_SetAppMetadata

```c
bool SDL_SetAppMetadata(const char* appname, const char* appversion, const char* appidentifier);
```

Опционально задаёт метаданные приложения (имя, версия, идентификатор). Помогает macOS About, системному микшеру и т.д.
Рекомендуется вызывать при старте, до или после `SDL_Init`.

- **appname** — имя приложения
- **appversion** — версия (например, `"1.0.0"`)
- **appidentifier** — идентификатор (например, `"com.mycompany.game"`)

---

### SDL_InitFlags (часто используемые)

| Флаг                | Описание                                                                        |
|---------------------|---------------------------------------------------------------------------------|
| `SDL_INIT_VIDEO`    | Видеоподсистема и окна. Подразумевает `SDL_INIT_EVENTS`. Обязателен для Vulkan. |
| `SDL_INIT_AUDIO`    | Аудио                                                                           |
| `SDL_INIT_JOYSTICK` | Джойстики                                                                       |
| `SDL_INIT_GAMEPAD`  | Геймпады (подмножество джойстиков)                                              |

---

## Window

### SDL_CreateWindow

```c
SDL_Window* SDL_CreateWindow(const char* title, int w, int h, SDL_WindowFlags flags);
```

Создаёт окно.

- **title** — заголовок окна
- **w, h** — ширина и высота в пикселях
- **flags** — `SDL_WINDOW_VULKAN` (для Vulkan), `SDL_WINDOW_RESIZABLE`, `SDL_WINDOW_FULLSCREEN` и др.
- **Возвращает:** handle окна или `NULL` при ошибке

Для Vulkan **обязателен** `SDL_WINDOW_VULKAN`.

---

### SDL_DestroyWindow

```c
void SDL_DestroyWindow(SDL_Window* window);
```

Уничтожает окно. Перед этим при Vulkan нужно вызвать `SDL_Vulkan_DestroySurface` (или `vkDestroySurfaceKHR`) для
surface, созданной через `SDL_Vulkan_CreateSurface`.

---

### SDL_GetWindowSize

```c
bool SDL_GetWindowSize(SDL_Window* window, int* w, int* h);
```

Возвращает размер клиентской области в **оконных координатах** (логические единицы). На HiDPI может отличаться от
размера в пикселях.

---

### SDL_GetWindowSizeInPixels

```c
bool SDL_GetWindowSizeInPixels(SDL_Window* window, int* w, int* h);
```

Возвращает размер клиентской области в **пикселях**. Использовать для создания Vulkan swapchain и framebuffer. При
`SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` перезапросить размер и пересоздать swapchain.

---

### SDL_WindowFlags (часто используемые)

| Флаг                            | Описание                                                        |
|---------------------------------|-----------------------------------------------------------------|
| `SDL_WINDOW_VULKAN`             | Окно поддерживает Vulkan. Нужен для `SDL_Vulkan_CreateSurface`. |
| `SDL_WINDOW_RESIZABLE`          | Окно можно изменять по размеру                                  |
| `SDL_WINDOW_FULLSCREEN`         | Полноэкранный режим                                             |
| `SDL_WINDOW_HIDDEN`             | Окно изначально скрыто                                          |
| `SDL_WINDOW_BORDERLESS`         | Без рамки                                                       |
| `SDL_WINDOW_HIGH_PIXEL_DENSITY` | Запрос высокого DPI back buffer                                 |

---

## Main callbacks

### SDL_AppInit

```c
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]);
```

Вызывается один раз при старте. Инициализация, создание окна, Vulkan.

- **appstate** — записать указатель на состояние приложения (window, instance и т.д.)
- **Возвращает:** `SDL_APP_CONTINUE`, `SDL_APP_SUCCESS`, `SDL_APP_FAILURE`

---

### SDL_AppEvent

```c
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event);
```

Вызывается для каждого события. Обработка закрытия окна, клавиш и т.д.

- **event** — событие (не копировать, данные не сохраняются после возврата)
- **Возвращает:** `SDL_APP_SUCCESS` — завершить приложение; `SDL_APP_CONTINUE` — продолжить

---

### SDL_AppIterate

```c
SDL_AppResult SDL_AppIterate(void* appstate);
```

Вызывается один раз за кадр. Обновление логики, отрисовка.

- **Возвращает:** `SDL_APP_SUCCESS` — завершить; `SDL_APP_CONTINUE` — продолжить

---

### SDL_AppQuit

```c
void SDL_AppQuit(void* appstate, SDL_AppResult result);
```

Вызывается перед выходом. Освобождение ресурсов.

---

### SDL_AppResult

```c
typedef enum SDL_AppResult {
    SDL_APP_FAILURE,
    SDL_APP_SUCCESS,
    SDL_APP_CONTINUE
} SDL_AppResult;
```

- `SDL_APP_CONTINUE` — приложение продолжает работу
- `SDL_APP_SUCCESS` — завершение с успехом (exit code 0)
- `SDL_APP_FAILURE` — завершение с ошибкой

---

## Events

### SDL_Event

Union всех типов событий ([SDL_events.h](../../external/SDL/include/SDL3/SDL_events.h)). Общее поле `type` (Uint32)
определяет, какое поле содержит данные. После проверки `event->type` обращайтесь к `event.key`, `event.window`,
`event.quit` и т.д. Данные события **не сохраняются** после возврата из callback — нельзя хранить указатель на
`event->key.text` и т.п.

| type                               | Структура      | Описание                                          |
|------------------------------------|----------------|---------------------------------------------------|
| `SDL_EVENT_QUIT`                   | `event.quit`   | Глобальный выход приложения                       |
| `SDL_EVENT_WINDOW_CLOSE_REQUESTED` | `event.window` | Менеджер окон запрашивает закрытие окна (крестик) |
| `SDL_EVENT_KEY_DOWN`               | `event.key`    | Клавиша нажата                                    |
| `SDL_EVENT_KEY_UP`                 | `event.key`    | Клавиша отпущена                                  |
| `SDL_EVENT_WINDOW_*`               | `event.window` | События окна (resize, focus и т.д.)               |

---

### event.window (SDL_WindowEvent)

Для событий `SDL_EVENT_WINDOW_*`:

- **windowID** — ID окна
- **data1**, **data2** — зависят от типа события:

| Тип события                           | data1                       | data2             |
|---------------------------------------|-----------------------------|-------------------|
| `SDL_EVENT_WINDOW_RESIZED`            | ширина (оконные координаты) | высота            |
| `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` | ширина в пикселях           | высота в пикселях |
| `SDL_EVENT_WINDOW_MOVED`              | x                           | y                 |
| `SDL_EVENT_WINDOW_DISPLAY_CHANGED`    | индекс дисплея              | —                 |

---

### event.key (SDL_KeyboardEvent)

Для `SDL_EVENT_KEY_DOWN` и `SDL_EVENT_KEY_UP`:

| Поле         | Тип            | Описание                                                                                                                      |
|--------------|----------------|-------------------------------------------------------------------------------------------------------------------------------|
| **key**      | `SDL_Keycode`  | Виртуальный код (зависит от раскладки). Для букв — `SDLK_a`, `SDLK_w` и т.д. Сравнивать с `SDLK_ESCAPE`, `SDLK_RETURN`.       |
| **scancode** | `SDL_Scancode` | Физическая клавиша (не зависит от раскладки). Для WASD в играх предпочтительнее `scancode` — одна клавиша на любой раскладке. |
| **mod**      | `SDL_Keymod`   | Модификаторы (Shift, Ctrl, Alt).                                                                                              |
| **repeat**   | `bool`         | `true` при автоповторе нажатой клавиши. Часто игнорируют в играх.                                                             |

Хедеры: [SDL_events.h](../../external/SDL/include/SDL3/SDL_events.h), [SDL_keycode.h](../../external/SDL/include/SDL3/SDL_keycode.h).

---

### Часто используемые типы событий

| Тип                                   | Когда                                                                     |
|---------------------------------------|---------------------------------------------------------------------------|
| `SDL_EVENT_QUIT`                      | Глобальный выход (закрытие последнего окна, Alt+F4)                       |
| `SDL_EVENT_WINDOW_CLOSE_REQUESTED`    | Крестик на окне. Для Vulkan: уничтожить swapchain до `SDL_DestroyWindow`. |
| `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` | Размер в пикселях изменился (resize, DPI). Пересоздать Vulkan swapchain.  |
| `SDL_EVENT_WINDOW_RESIZED`            | Размер в оконных координатах изменился                                    |
| `SDL_EVENT_KEY_DOWN`                  | Клавиша нажата. `event.key` — см. выше                                    |

---

## Vulkan

### SDL_Vulkan_GetInstanceExtensions

```c
const char* const* SDL_Vulkan_GetInstanceExtensions(Uint32* count);
```

Возвращает массив имён расширений для `VkInstanceCreateInfo`. Окно должно быть создано с `SDL_WINDOW_VULKAN`.

- **count** — заполняется числом расширений
- **Возвращает:** массив строк (не освобождать) или `NULL` при ошибке

---

### SDL_Vulkan_CreateSurface

```c
bool SDL_Vulkan_CreateSurface(SDL_Window* window, VkInstance instance,
    const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface);
```

Создаёт `VkSurfaceKHR` для окна.

- **instance** — создан с расширениями из `SDL_Vulkan_GetInstanceExtensions`
- **allocator** — можно `NULL`
- **surface** — выходной handle
- **Возвращает:** `true` при успехе

---

### SDL_Vulkan_DestroySurface

```c
void SDL_Vulkan_DestroySurface(VkInstance instance, VkSurfaceKHR surface,
    const VkAllocationCallbacks* allocator);
```

Уничтожает surface. Вызывать перед `SDL_DestroyWindow`.

---

### SDL_Vulkan_GetVkGetInstanceProcAddr

```c
SDL_FunctionPointer SDL_Vulkan_GetVkGetInstanceProcAddr(void);
```

Возвращает указатель на `vkGetInstanceProcAddr`. Привести к `PFN_vkGetInstanceProcAddr` и передать в
`volkInitializeCustom` при необходимости.

---

### SDL_Vulkan_GetPresentationSupport

```c
bool SDL_Vulkan_GetPresentationSupport(VkInstance instance, VkPhysicalDevice physicalDevice, Uint32 queueFamilyIndex);
```

Проверяет поддержку presentation для данной queue family на physical device. Полезно при выборе queue family для
отрисовки. Instance должен быть создан с расширениями из `SDL_Vulkan_GetInstanceExtensions`.

---

### SDL_Vulkan_UnloadLibrary

```c
void SDL_Vulkan_UnloadLibrary(void);
```

Выгружает Vulkan loader, загруженный через `SDL_Vulkan_LoadLibrary` или при создании окна с `SDL_WINDOW_VULKAN`.
Вызывать после уничтожения всех Vulkan-ресурсов и окон. SDL ведёт счётчик вызовов Load/Unload — библиотека выгружается
при балансе.

---

## Hints (Vulkan)

| Hint                      | Описание                                                                                    | Когда задавать                                                       |
|---------------------------|---------------------------------------------------------------------------------------------|----------------------------------------------------------------------|
| `SDL_HINT_VULKAN_LIBRARY` | Путь к Vulkan loader (например, `"vulkan-1.dll"`).                                          | До `SDL_Vulkan_LoadLibrary` или создания окна с `SDL_WINDOW_VULKAN`. |
| `SDL_HINT_VULKAN_DISPLAY` | Индекс дисплея для `SDL_Vulkan_CreateSurface` (строка `"0"`, `"1"` и т.д.). По умолчанию 0. | До `SDL_Vulkan_CreateSurface`.                                       |

Задать через `SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, "path")`.

---

## Ошибки и логирование

### SDL_GetError

```c
const char* SDL_GetError(void);
```

Строка с описанием последней ошибки SDL.

---

### SDL_Log

```c
void SDL_Log(const char* fmt, ...);
void SDL_LogError(int category, const char* fmt, ...);
```

Логирование. `SDL_Log` — общий вывод; `SDL_LogError` — с категорией (например, `SDL_LOG_CATEGORY_APPLICATION`).

# Решение проблем

Частые ошибки при использовании SDL3 и способы их исправления.

## На этой странице

- [Инициализация](#инициализация)
  - [SDL_Init возвращает false](#sdl_init-возвращает-false)
  - [SDL_CreateWindow возвращает NULL](#sdl_createwindow-возвращает-null)
  - [SDL_Vulkan_CreateSurface возвращает false](#sdl_vulkan_createsurface-возвращает-false)
  - [SDL_Vulkan_GetInstanceExtensions возвращает NULL](#sdl_vulkan_getinstanceextensions-возвращает-null--sdl_vulkan_createsurface-не-работает)
  - [Окно не отображается или сразу закрывается](#окно-не-отображается-или-сразу-закрывается)
- [Runtime](#runtime)
  - [События не приходят](#события-не-приходят--sdl_pollevent-в-цикле-ничего-не-возвращает)
  - [DLL SDL не найдена (Windows)](#dll-sdl-не-найдена-при-запуске-windows)
  - [Приложение зависает при закрытии](#приложение-зависает-при-закрытии)
  - [Кастомный путь к Vulkan loader](#нужен-кастомный-путь-к-vulkan-loader)
- [Сборка](#сборка)
  - [undefined reference to SDL_main](#undefined-reference-to-sdl_main--линковка-не-находит-entry-point)
- [См. также](#см-также)

---

## Инициализация

### SDL_Init возвращает false

**Причина:** Не удалось инициализировать видеоподсистему. Причины: нет дисплея (headless), неподходящий драйвер,
конфликт с другим приложением.

**Решение:**

1. Вызовите `SDL_GetError()` сразу после `SDL_Init` для диагностики:
   ```cpp
   if (!SDL_Init(SDL_INIT_VIDEO)) {
       SDL_Log("SDL_Init failed: %s", SDL_GetError());
       return SDL_APP_FAILURE;
   }
   ```
2. На Linux: убедитесь, что X11/Wayland доступен, установлен пакет `libsdl3` или SDL собран корректно.
3. На Windows: проверьте, что нет конфликтов с другим полноэкранным приложением.
4. На виртуальной машине: некоторые VM не предоставляют видеодрайвер — может потребоваться программный рендерер или
   другой хост.

---

### SDL_CreateWindow возвращает NULL

**Причина:** Не удалось создать окно. Часто — драйвер не поддерживает запрошенные флаги, или SDL_Init не был успешен.

**Решение:**

1. Проверьте `SDL_GetError()` после вызова:
   ```cpp
   SDL_Window* window = SDL_CreateWindow("Game", 1280, 720, SDL_WINDOW_VULKAN);
   if (!window) {
       SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
       return SDL_APP_FAILURE;
   }
   ```
2. Убедитесь, что `SDL_Init(SDL_INIT_VIDEO)` вернул `true` перед созданием окна.
3. Попробуйте создать окно без `SDL_WINDOW_VULKAN` для проверки — если без флага работает, проблема может быть в
   поддержке Vulkan (см. [volk — troubleshooting](../volk/troubleshooting.md)).
4. Проверьте, что DLL SDL (Windows) лежит рядом с exe — см. [Интеграция, копирование DLL](integration.md#1-cmake).

---

### SDL_Vulkan_CreateSurface возвращает false

**Причина:** Не удалось создать Vulkan-поверхность. Типичные причины:

- Instance создан **без** расширений из `SDL_Vulkan_GetInstanceExtensions`
- Окно создано **без** флага `SDL_WINDOW_VULKAN`
- Vulkan loader не загружен (`volkInitialize` не вызван или завершился ошибкой)

**Решение:**

1. Убедитесь, что окно создано с `SDL_WINDOW_VULKAN`.
2. Добавьте все расширения из `SDL_Vulkan_GetInstanceExtensions` в `VkInstanceCreateInfo`. Полный
   пример — [Интеграция, раздел 4](integration.md#4-полная-последовательность-sdl--volk--vulkan).
3. Вызовите `volkLoadInstance(instance)` **до** `SDL_Vulkan_CreateSurface` — иначе `vkCreate*SurfaceKHR` может быть
   недоступен.
4. Проверьте `SDL_GetError()` для подробностей.

---

### SDL_Vulkan_GetInstanceExtensions возвращает NULL / SDL_Vulkan_CreateSurface не работает

**Причина:** Vulkan loader не загружен. SDL загружает его при создании окна с `SDL_WINDOW_VULKAN`. Если окно создано без
этого флага — loader не загружен. Либо можно явно вызвать `SDL_Vulkan_LoadLibrary(nullptr)` до создания окна.

**Решение:**

1. Убедитесь, что окно создано с `SDL_WINDOW_VULKAN`.
2. Или вызовите `SDL_Vulkan_LoadLibrary(nullptr)` после `SDL_Init`, до `SDL_Vulkan_GetInstanceExtensions`.
3. При кастомном пути к loader: `SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, "path/to/vulkan-1.dll")` до создания окна или до
   `SDL_Vulkan_LoadLibrary`.

---

### Окно не отображается или сразу закрывается

**Причина:** При `SDL_MAIN_USE_CALLBACKS` приложение завершается, если `SDL_AppInit` возвращает `SDL_APP_FAILURE` или
`SDL_APP_SUCCESS`. Либо `SDL_AppEvent` возвращает `SDL_APP_SUCCESS` на первом же событии.

**Решение:**

1. Убедитесь, что `SDL_AppInit` возвращает `SDL_APP_CONTINUE` при успешной инициализации.
2. В `SDL_AppEvent` возвращайте `SDL_APP_SUCCESS` только при намеренном выходе (крестик, Escape). Для остальных
   событий — `SDL_APP_CONTINUE`.
3. В `SDL_AppIterate` возвращайте `SDL_APP_CONTINUE` — иначе цикл прекратится.

---

## Runtime

### События не приходят / SDL_PollEvent в цикле ничего не возвращает

**Причина:** При `SDL_MAIN_USE_CALLBACKS` события доставляются в `SDL_AppEvent`, а не в очередь для `SDL_PollEvent`. SDL
может не вызывать `SDL_PollEvent` или очередь пуста.

**Решение:** Не используйте `SDL_PollEvent` при `SDL_MAIN_USE_CALLBACKS`. Обрабатывайте события в `SDL_AppEvent`:

```cpp
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE)
        return SDL_APP_SUCCESS;
    return SDL_APP_CONTINUE;
}
```

Если нужен классический цикл с `SDL_PollEvent`, не используйте `SDL_MAIN_USE_CALLBACKS` — реализуйте обычный `main()`.

---

### Приложение зависает при закрытии

**Причина:** Неверный порядок освобождения Vulkan-ресурсов. Уничтожение surface или swapchain после окна может привести
к deadlock или краху.

**Решение:** Соблюдайте порядок cleanup: сначала `vkDestroySwapchainKHR` и связанные framebuffers/image views, затем
`SDL_Vulkan_DestroySurface` (или `vkDestroySurfaceKHR`), затем `SDL_DestroyWindow`.
Подробнее: [Интеграция — Порядок уничтожения](integration.md#7-порядок-уничтожения-cleanup).

---

### DLL SDL не найдена при запуске (Windows)

**Причина:** `SDL3.dll` не в том же каталоге, что exe, или не в PATH.

**Решение:**

1. Добавьте POST_BUILD копирование в CMake — см. [Интеграция, копирование DLL](integration.md#1-cmake).
2. Или скопируйте DLL вручную из `build/.../SDL3.dll` в каталог с exe.
3. При отладке проверьте, что рабочая директория — та же, где лежит exe и DLL.

---

### Нужен кастомный путь к Vulkan loader

**Причина:** Vulkan loader в нестандартном месте (portable build, своя копия SDK).

**Решение:** Установите hint до создания Vulkan-окна или до `SDL_Vulkan_LoadLibrary`:

```cpp
SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, "path/to/vulkan-1.dll");  // Windows
// или
SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, "libvulkan.so.1");        // Linux
```

---

## Сборка

### undefined reference to SDL_main / линковка не находит entry point

**Причина:** При `SDL_MAIN_USE_CALLBACKS` SDL переопределяет `main` на `SDL_main`, который вызывает ваши callbacks. Если
вы определили свой `main()` и при этом указали `SDL_MAIN_USE_CALLBACKS`, возникает конфликт.

**Решение:**

1. При `SDL_MAIN_USE_CALLBACKS` **не** определяйте `main()`. Реализуйте только `SDL_AppInit`, `SDL_AppEvent`,
   `SDL_AppIterate`, `SDL_AppQuit`.
2. Если нужен свой `main()`, не определяйте `SDL_MAIN_USE_CALLBACKS`. Используйте обычный цикл с `SDL_PollEvent`.

---

## См. также

- [Интеграция](integration.md) — CMake, порядок вызовов, Vulkan, hints
- [volk — Решение проблем](../volk/troubleshooting.md) — ошибки Vulkan loader
- [Справочник API](api-reference.md) — когда какую функцию вызывать

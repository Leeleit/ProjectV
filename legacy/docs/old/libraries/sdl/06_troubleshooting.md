# Решение проблем SDL3

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

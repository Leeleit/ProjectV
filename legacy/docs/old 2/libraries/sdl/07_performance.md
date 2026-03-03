# Производительность SDL3

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
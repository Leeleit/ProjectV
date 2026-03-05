# Производительность SDL3

**🟡 Уровень 2: Средний**

---

## Содержание

- [Оптимизация ввода (Input Latency)](#оптимизация-ввода-input-latency)
- [Оптимизация рендеринга](#оптимизация-рендеринга)
- [Оптимизация памяти](#оптимизация-памяти)
- [Профилирование](#профилирование)
- [Кроссплатформенные особенности](#кроссплатформенные-особенности)

---

## Оптимизация ввода (Input Latency)

### Raw Input для точного позиционирования

Стандартная обработка мыши может иметь ускорение и сглаживание ОС. Для точного позиционирования (графические редакторы,
игры):

```cpp
void configureRawInput() {
    if (SDL_HasRawMouseMotion()) {
        SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
        SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SCALING, "0");
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }
}
```

### Снижение задержки событий

```cpp
// Используйте высокоприоритетный поток для обработки ввода
void setupLowLatencyInput() {
    // Отключение композиции Windows (если доступно)
    SDL_SetHint(SDL_HINT_VIDEO_WINDOW_SHARE_PIXEL_FORMAT, "1");
    
    // Использование эксклюзивного fullscreen режима для снижения latency
    SDL_SetHint(SDL_HINT_VIDEO_FULLSCREEN_SPACES, "0");
}
```

### Пакетная обработка событий

```cpp
// Вместо обработки каждого события по отдельности
struct BatchedInput {
    std::vector<SDL_Event> keyboardEvents;
    std::vector<SDL_Event> mouseEvents;
    std::vector<SDL_Event> gamepadEvents;
    
    void batchEvents() {
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
                case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                case SDL_EVENT_GAMEPAD_BUTTON_UP:
                case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                    gamepadEvents.push_back(event);
                    break;
            }
        }
    }
    
    void processBatched() {
        // Обработка всех событий клавиатуры за один проход
        for (const auto& e : keyboardEvents) {
            processKeyboard(e);
        }
        // Аналогично для мыши и геймпада
    }
};
```

---

## Оптимизация рендеринга

### Vulkan Surface и Swapchain оптимизации

```cpp
struct VulkanRenderer {
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    
    void optimizeForSDL(SDL_Window* window) {
        // Использование настоящего размера окна в пикселях (важно для HiDPI)
        int pixelWidth, pixelHeight;
        SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight);
        
        // Создание swapchain с оптимальными параметрами
        VkSwapchainCreateInfoKHR swapchainInfo = {};
        swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainInfo.surface = surface;
        swapchainInfo.minImageCount = 3; // Triple buffering
        swapchainInfo.imageExtent = { (uint32_t)pixelWidth, (uint32_t)pixelHeight };
        swapchainInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR; // Низкая latency
        
        // Асинхронная передача кадров
        SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
    }
};
```

### Обработка изменения размера окна

```cpp
void handleResizeOptimized(SDL_Window* window) {
    // Ожидание завершения работы GPU перед изменением размера
    vkDeviceWaitIdle(device);
    
    // Получение нового размера
    int newWidth, newHeight;
    SDL_GetWindowSizeInPixels(window, &newWidth, &newHeight);
    
    // Реальное изменение размера только если оно существенное
    static int lastWidth = 0, lastHeight = 0;
    if (abs(newWidth - lastWidth) > 10 || abs(newHeight - lastHeight) > 10) {
        recreateSwapchain(newWidth, newHeight);
        lastWidth = newWidth;
        lastHeight = newHeight;
    }
}
```

---

## Оптимизация памяти

### Оптимизация аллокаций в игровом цикле

```cpp
struct MemoryPool {
    std::vector<std::unique_ptr<char[]>> pools;
    size_t currentPool = 0;
    size_t currentOffset = 0;
    
    void* allocate(size_t size, size_t alignment = 16) {
        // Выравнивание
        size_t alignedOffset = (currentOffset + alignment - 1) & ~(alignment - 1);
        
        if (alignedOffset + size > POOL_SIZE) {
            // Создание нового пула
            currentPool++;
            currentOffset = 0;
            alignedOffset = 0;
            
            if (currentPool >= pools.size()) {
                pools.emplace_back(new char[POOL_SIZE]);
            }
        }
        
        void* ptr = pools[currentPool].get() + alignedOffset;
        currentOffset = alignedOffset + size;
        return ptr;
    }
    
    void reset() {
        currentPool = 0;
        currentOffset = 0;
    }
};

// Использование в игровом цикле
void gameLoop() {
    static MemoryPool frameMemory;
    frameMemory.reset();
    
    // Все аллокации кадра используют пул
    auto* frameData = frameMemory.allocate(sizeof(FrameData));
    // ...
}
```

### Кэширование SDL объектов

```cpp
struct SDLCache {
    std::unordered_map<std::string, SDL_Texture*> textures;
    std::unordered_map<std::string, SDL_Cursor*> cursors;
    
    SDL_Texture* getTexture(SDL_Renderer* renderer, const char* path) {
        auto it = textures.find(path);
        if (it != textures.end()) {
            return it->second;
        }
        
        // Загрузка и кэширование
        SDL_Texture* texture = SDL_LoadTexture(renderer, path);
        textures[path] = texture;
        return texture;
    }
};
```

---

## Профилирование

### Интеграция с Tracy

```cpp
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    ZoneScopedN("SDL Event Processing");
    
    switch (event->type) {
        case SDL_EVENT_KEY_DOWN:
            TracyPlot("Key Presses", 1);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            TracyPlot("Mouse Delta X", event->motion.xrel);
            TracyPlot("Mouse Delta Y", event->motion.yrel);
            break;
    }
    
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    ZoneScopedN("Game Loop Iteration");
    
    {
        ZoneScopedN("Input Processing");
        processInput();
    }
    
    {
        ZoneScopedN("Game Logic");
        updateGame();
    }
    
    {
        ZoneScopedN("Rendering");
        renderFrame();
    }
    
    FrameMark; // Отметка кадра для Tracy
    return SDL_APP_CONTINUE;
}
```

### Измерение FPS и задержки

```cpp
struct PerformanceMetrics {
    Uint32 frameTimes[60] = {0};
    size_t frameIndex = 0;
    Uint32 lastFrameTime = 0;
    
    void beginFrame() {
        lastFrameTime = SDL_GetTicks();
    }
    
    void endFrame() {
        Uint32 currentTime = SDL_GetTicks();
        frameTimes[frameIndex] = currentTime - lastFrameTime;
        frameIndex = (frameIndex + 1) % 60;
        
        // Расчет среднего FPS
        Uint32 totalTime = 0;
        for (size_t i = 0; i < 60; i++) {
            totalTime += frameTimes[i];
        }
        float avgFPS = 60.0f / (totalTime / 1000.0f);
        
        TracyPlot("FPS", avgFPS);
        TracyPlot("Frame Time (ms)", frameTimes[(frameIndex + 59) % 60]);
    }
};
```

---

## Кроссплатформенные особенности

### Windows оптимизации

```cpp
void windowsOptimizations() {
    // Отключение DPI виртуализации для прямого доступа к пикселям
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    
    // Использование эксклюзивного fullscreen режима
    SDL_SetHint(SDL_HINT_VIDEO_FULLSCREEN_SPACES, "0");
    
    // Оптимизация для игровых режимов Windows (Game Mode)
    SDL_SetHint(SDL_HINT_WINDOWS_ENABLE_MESSAGELOOP, "0");
}
```

### Linux оптимизации

```cpp
void linuxOptimizations() {
    // Предпочтительный выбор драйвера
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11"); // или "wayland"
    
    // Оптимизация для компилятора
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "1");
    
    // Использование пряого рендеринга
    SDL_SetHint(SDL_HINT_RENDER_DIRECT3D_THREADSAFE, "0");
}
```

### macOS оптимизации

```cpp
void macosOptimizations() {
    // Использование Metal вместо OpenGL (если доступно)
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");
    
    // Оптимизация для Retina дисплеев
    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
    
    // Интеграция с системным меню
    SDL_SetHint(SDL_HINT_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK, "1");
}
```

---

## Практические рекомендации

### DOs и DON'Ts

**Делать:**

- Использовать `SDL_GetWindowSizeInPixels()` для Vulkan/OpenGL рендеринга
- Обрабатывать `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` для HiDPI
- Использовать пулы памяти для аллокаций в игровом цикле
- Профилировать с Tracy для выявления узких мест
- Использовать raw input для приложений, требующих точности

**Не делать:**

- Не вызывать `SDL_PollEvent()` в callback режиме (`SDL_MAIN_USE_CALLBACKS`)
- Не создавать/уничтожать окна в основном игровом цикле
- Не полагаться на `SDL_GetTicks()` для точного временного интервала (используйте `SDL_GetPerformanceCounter()`)
- Не использовать синхронные операции в рендеринге

### Чеклист оптимизации

1. [ ] Измерение базового FPS и задержки ввода
2. [ ] Включение raw input если требуется точность
3. [ ] Оптимизация аллокаций (пулы памяти)
4. [ ] Настройка swapchain для низкой latency
5. [ ] Интеграция с Tracy для профилирования
6. [ ] Платформо-специфичные оптимизации
7. [ ] Обработка HiDPI корректно
8. [ ] Использование асинхронной загрузки ресурсов

---

## Следующие шаги

1. **Измерение**: Используйте инструменты профилирования для выявления узких мест
2. **Оптимизация**: Примените соответствующие техники из этого руководства
3. **Тестирование**: Протестируйте на разных платформах и аппаратных конфигурациях
4. **Итерация**: Повторяйте процесс измерения и оптимизации

Для специфичных оптимизаций воксельного движка
смотрите [Интеграция в ProjectV](projectv-integration.md#оптимизация-ввода-для-точного-редактирования).

---

## Связанные разделы

- [Основные понятия](concepts.md) — фундаментальные концепции SDL
- [Сценарии использования](use-cases.md) — архитектурные паттерны
- [Decision Trees](decision-trees.md) — рекомендации по выбору оптимизаций
- [Интеграция в ProjectV](projectv-integration.md) — специфичные оптимизации для воксельного движка
- [Tracy документация](../tracy/README.md) — продвинутое профилирование

← [Назад к документации SDL](README.md)
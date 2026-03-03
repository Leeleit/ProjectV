# Паттерны ProjectV для SDL3

**🔴 Уровень 3: Продвинутый**

Специфичные паттерны и оптимизации для воксельного движка ProjectV.

---

## Multi-window редактор вокселей

### Менеджер окон

```cpp
class VoxelEditorWindowManager {
    struct WindowData {
        SDL_Window* window = nullptr;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    WindowData mainViewport;
    WindowData toolsPanel;
    WindowData propertiesPanel;

public:
    bool init(VkInstance instance) {
        // Main viewport
        mainViewport.window = SDL_CreateWindow(
            "Voxel Viewport", 1280, 720,
            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
        );
        create_surface(instance, &mainViewport);

        // Tools panel
        toolsPanel.window = SDL_CreateWindow(
            "Tools", 300, 800,
            SDL_WINDOW_VULKAN
        );
        create_surface(instance, &toolsPanel);

        return true;
    }

    void handleClose(SDL_WindowID windowID, VkInstance instance) {
        if (windowID == SDL_GetWindowID(mainViewport.window)) {
            // Главное окно = выход
            return;  // Вызовет SDL_APP_SUCCESS
        }

        // Вспомогательные окна
        if (windowID == SDL_GetWindowID(toolsPanel.window)) {
            destroy_window(instance, &toolsPanel);
        }
    }

    void destroyAll(VkInstance instance) {
        destroy_window(instance, &mainViewport);
        destroy_window(instance, &toolsPanel);
        destroy_window(instance, &propertiesPanel);
    }

private:
    void create_surface(VkInstance instance, WindowData* data) {
        SDL_Vulkan_CreateSurface(data->window, instance, nullptr, &data->surface);
    }

    void destroy_window(VkInstance instance, WindowData* data) {
        if (data->swapchain) {
            vkDestroySwapchainKHR(/* device */, data->swapchain, nullptr);
        }
        if (data->surface) {
            SDL_Vulkan_DestroySurface(instance, data->surface, nullptr);
        }
        if (data->window) {
            SDL_DestroyWindow(data->window);
        }
        data->window = nullptr;
        data->surface = VK_NULL_HANDLE;
        data->swapchain = VK_NULL_HANDLE;
    }
};
```

---

## Оптимизация ввода для редактирования вокселей

### Raw Input для кисти

```cpp
void configure_brush_input() {
    // Отключение системного сглаживания и ускорения
    if (SDL_HasRawMouseMotion()) {
        SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
        SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SCALING, "0");
    }
}

// Включение при начале рисования
void on_brush_start() {
    SDL_SetRelativeMouseMode(SDL_TRUE);
}

// Выключение при завершении
void on_brush_end() {
    SDL_SetRelativeMouseMode(SDL_FALSE);
}
```

### Немедленная обработка для preview

```cpp
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* state = static_cast<AppState*>(appstate);

    // Немедленная обработка для кисти
    if (event->type == SDL_EVENT_MOUSE_MOTION && state->brushActive) {
        // Direct path: событие -> preview без задержки на ECS
        update_brush_preview(event->motion.x, event->motion.y);
    }

    // Остальные события через ECS
    else if (event->type == SDL_EVENT_KEY_DOWN ||
             event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        enqueue_to_ecs(state->ecs, event);
    }

    return SDL_APP_CONTINUE;
}
```

---

## Асинхронное обновление вокселей

### Фоновый поток для вычислений

```cpp
struct VoxelUpdateSystem {
    std::atomic<bool> running{true};
    std::thread updateThread;
    std::mutex queueMutex;
    std::vector<VoxelChunk*> pendingChunks;

    void start() {
        updateThread = std::thread([this]() {
            while (running) {
                VoxelChunk* chunk = nullptr;
                {
                    std::lock_guard lock(queueMutex);
                    if (!pendingChunks.empty()) {
                        chunk = pendingChunks.back();
                        pendingChunks.pop_back();
                    }
                }

                if (chunk) {
                    compute_voxel_mesh(chunk);
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        });
    }

    void enqueue(VoxelChunk* chunk) {
        std::lock_guard lock(queueMutex);
        pendingChunks.push_back(chunk);
    }

    void stop() {
        running = false;
        if (updateThread.joinable()) {
            updateThread.join();
        }
    }
};
```

---

## Потоковая архитектура

```
┌───────────────┐     ┌───────────────┐     ┌───────────────┐
│  SDL Thread   │     │  ECS Thread   │     │ Vulkan Thread │
│  (main)       │     │  (logic)      │     │ (render)      │
├───────────────┤     ├───────────────┤     ├───────────────┤
│ SDL_AppEvent  │────▶│ Event Queue   │────▶│ Render Queue  │
│ SDL_AppIterate│     │ flecs::progress│    │ vkQueueSubmit │
└───────────────┘     └───────────────┘     └───────────────┘
        │                                            │
        └────────────────────────────────────────────┘
                    Low-latency path (brush)
```

---

## DOD-оптимизации

> **Подробнее о DOD:** [Data-Oriented Design](../../guides/cpp/05_dod-practice.md) — SoA/AoS, cache locality, batch
> processing.

### Пакетная обработка событий

```cpp
struct InputBatch {
    struct MouseMove {
        int32_t x, y;
        int32_t dx, dy;
    };

    std::vector<MouseMove> mouseMoves;
    uint32_t keyStates[16] = {0};  // Bitmask

    void addEvent(const SDL_Event& event) {
        switch (event.type) {
            case SDL_EVENT_MOUSE_MOTION:
                mouseMoves.push_back({
                    event.motion.x, event.motion.y,
                    event.motion.xrel, event.motion.yrel
                });
                break;
            case SDL_EVENT_KEY_DOWN:
                keyStates[event.key.scancode / 32] |= (1u << (event.key.scancode % 32));
                break;
        }
    }

    void clear() {
        mouseMoves.clear();
        memset(keyStates, 0, sizeof(keyStates));
    }
};
```

### Cache-friendly обработка

```cpp
// SoA вместо AoS для воксельных данных
struct VoxelData {
    std::vector<uint8_t> types;      // Тип вокселя
    std::vector<uint8_t> colors;     // Цвет (4 канала)
    std::vector<uint8_t> flags;      // Флаги
    // Плотная упаковка для cache locality
};

void process_voxels_dod(VoxelData& data) {
    // Линейный проход с минимальными cache miss
    for (size_t i = 0; i < data.types.size(); i++) {
        if (data.types[i] == VOXEL_AIR) continue;
        process_voxel(data.types[i], data.colors.data() + i * 4);
    }
}
```

---

## Профилирование с Tracy

### Детальное профилирование кадра

```cpp
SDL_AppResult SDL_AppIterate(void* appstate) {
    ZoneScopedN("Frame");

    {
        ZoneScopedN("Input");
        process_input_batch();
    }

    {
        ZoneScopedN("VoxelUpdate");
        update_voxel_chunks();
    }

    {
        ZoneScopedN("ECS");
        ecs->progress();
    }

    {
        ZoneScopedN("Render");
        render_frame();
    }

    {
        ZoneScopedN("Present");
        present_frame();
    }

    TracyPlot("VoxelCount", voxel_count);
    TracyPlot("ChunkCount", chunk_count);
    TracyPlot("DrawCalls", draw_calls);

    FrameMark;
    return SDL_APP_CONTINUE;
}
```

---

## Интеграция с ImGui

### ImGui для debug UI

```cpp
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

void init_imgui(SDL_Window* window, VkInstance instance, VkDevice device) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui_ImplSDL3_InitForVulkan(window);
    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = instance;
    initInfo.Device = device;
    // ...
    ImGui_ImplVulkan_Init(&initInfo);
}

void render_debug_ui() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Debug info
    ImGui::Begin("Debug");
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Voxels: %zu", voxel_count);
    ImGui::End();

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}
```

---

## Обработка resize

### Пересоздание swapchain с минимальной задержкой

```cpp
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        auto* state = static_cast<AppState*>(appstate);

        uint32_t width = event->window.data1;
        uint32_t height = event->window.data2;

        // Debounce: игнорировать мелкие изменения
        if (abs((int)width - (int)state->lastWidth) > 5 ||
            abs((int)height - (int)state->lastHeight) > 5) {

            state->pendingResize = true;
            state->newWidth = width;
            state->newHeight = height;
        }
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* state = static_cast<AppState*>(appstate);

    // Обработка resize в начале кадра
    if (state->pendingResize) {
        vkDeviceWaitIdle(state->device);
        recreate_swapchain(state->newWidth, state->newHeight);
        state->lastWidth = state->newWidth;
        state->lastHeight = state->newHeight;
        state->pendingResize = false;
    }

    // Основной рендеринг
    render_frame();

    return SDL_APP_CONTINUE;
}
```

---

## Чеклист интеграции ProjectV

- [ ] SDL_MAIN_USE_CALLBACKS включён
- [ ] volk инициализирован через SDL_Vulkan_GetVkGetInstanceProcAddr
- [ ] Расширения instance получены от SDL_Vulkan_GetInstanceExtensions
- [ ] Surface создана через SDL_Vulkan_CreateSurface
- [ ] Интеграция с flecs ECS
- [ ] Профилирование Tracy
- [ ] Raw Input для кисти редактора
- [ ] Асинхронное обновление вокселей
- [ ] Multi-window поддержка (опционально)
- [ ] ImGui для debug UI

# SDL3: Злые хаки и DOD-оптимизации

## Архитектура многопоточности

### Главное правило

**SDL callbacks выполняются в одном потоке (main thread)**. Это железное правило. Любые SDL-вызовы из background
threads — undefined behavior.

```
┌─────────────────────────────────────────────────────────────────┐
│                        Main Thread                              │
│  ┌─────────────┐    ┌─────────────┐    ┌──────────────────┐  │
│  │ SDL_AppEvent│───▶│  Job Queue  │───▶│ SDL_AppIterate   │  │
│  │  (input)    │    │  (locked)   │    │  (render)        │  │
│  └─────────────┘    └─────────────┘    └──────────────────┘  │
│         │                  ▲                    │              │
└─────────┼──────────────────┼────────────────────┼──────────────┘
          │                  │
          ▼                  │
    ┌───────────┐            │      Worker Threads
    │  Input    │            │    ┌─────────────────┐
    │  Buffer   │            └────│ Voxel Update    │
    │  (SoA)    │                 │  Physics Step   │
    └───────────┘                 │  Mesh Gen       │
                                  └─────────────────┘
```

### Zero-copy очередь событий

```cpp
// Thread-safe SPSC (Single Producer Single Consumer) queue
// Для передачи данных из background threads в main thread

template<typename T, size_t Capacity>
class alignas(64) SpscRingBuffer {
    std::array<T, Capacity> buffer_;
    std::atomic<size_t> writePos_{0};
    std::atomic<size_t> readPos_{0};

public:
    bool push(T&& value) {
        size_t w = writePos_.load(std::memory_order_relaxed);
        size_t r = readPos_.load(std::memory_order_acquire);
        if ((w + 1) % Capacity == r) return false;

        buffer_[w] = std::move(value);
        writePos_.store((w + 1) % Capacity, std::memory_order_release);
        return true;
    }

    bool pop(T& value) {
        size_t r = readPos_.load(std::memory_order_relaxed);
        size_t w = writePos_.load(std::memory_order_acquire);
        if (r == w) return false;

        value = std::move(buffer_[r]);
        readPos_.store((r + 1) % Capacity, std::memory_order_release);
        return true;
    }

    bool empty() const {
        return writePos_.load(std::memory_order_acquire) ==
               readPos_.load(std::memory_order_acquire);
    }
};
```

> **Для понимания:** `alignas(64)` выравнивает структуру по границе кэш-линии. Это предотвращает false sharing —
> ситуацию, когда два потока модифицируют разные данные на одной кэш-линии, вызывая постоянное обновление кэша.

## Data-Oriented Design для ввода

### AoS vs SoA

**AoS (Array of Structures)** — классический подход:

```cpp
// ❌ Плохо для batch processing
struct KeyState {
    bool pressed;
    Uint32 timestamp;
};

std::array<KeyState, 512> keys;
```

**SoA (Structure of Arrays)** — DOD-подход:

```cpp
// ✅ Хорошо для SIMD и cache locality
struct alignas(64) InputState {
    std::bitset<512> pressed;       // 64 байта (одна кэш-линия!)
    std::array<Uint32, 512> timestamps;
};
```

### Пакетная обработка событий

```cpp
struct alignas(64) InputBatch {
    // SoA layout для максимальной производительности
    static constexpr size_t MAX_EVENTS = 256;

    // Compact storage: только нужные данные
    std::array<Uint8, MAX_EVENTS> keyScancodes;
    std::array<Uint8, MAX_EVENTS> keyStates;      // 0 = up, 1 = down
    std::array<int16_t, MAX_EVENTS> mouseX;
    std::array<int16_t, MAX_EVENTS> mouseY;
    std::array<int16_t, MAX_EVENTS> mouseRelX;
    std::array<int16_t, MAX_EVENTS> mouseRelY;
    std::array<Uint8, MAX_EVENTS> mouseButtons;
    std::array<Uint32, MAX_EVENTS> eventTypes;

    std::atomic<size_t> count{0};

    void add_key(SDL_Scancode scancode, bool pressed) {
        size_t idx = count.fetch_add(1, std::memory_order_relaxed);
        if (idx < MAX_EVENTS) {
            keyScancodes[idx] = static_cast<Uint8>(scancode);
            keyStates[idx] = pressed ? 1 : 0;
            eventTypes[idx] = pressed ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        }
    }

    void add_mouse_motion(int x, int y, int rx, int ry, Uint8 buttons) {
        size_t idx = count.fetch_add(1, std::memory_order_relaxed);
        if (idx < MAX_EVENTS) {
            mouseX[idx] = static_cast<int16_t>(x);
            mouseY[idx] = static_cast<int16_t>(y);
            mouseRelX[idx] = static_cast<int16_t>(rx);
            mouseRelY[idx] = static_cast<int16_t>(ry);
            mouseButtons[idx] = buttons;
            eventTypes[idx] = SDL_EVENT_MOUSE_MOTION;
        }
    }

    void reset() { count.store(0, std::memory_order_relaxed); }
};
```

### Потоковая обработка ввода

```cpp
// Main thread: только накопление
InputBatch inputBatch;

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    switch (event->type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            inputBatch.add_key(event->key.scancode,
                              event->type == SDL_EVENT_KEY_DOWN);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            inputBatch.add_mouse_motion(event->motion.x, event->motion.y,
                                        event->motion.xrel, event->motion.yrel,
                                        event->motion.state);
            break;
    }
    return SDL_APP_CONTINUE;
}

// Worker thread: обработка (асинхронно)
void process_input_batch(InputBatch& batch) {
    const size_t n = batch.count.load(std::memory_order_acquire);

    // Линейный проход — отличный cache utilization
    for (size_t i = 0; i < n; ++i) {
        switch (batch.eventTypes[i]) {
            case SDL_EVENT_KEY_DOWN:
                handle_key_down(batch.keyScancodes[i]);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                handle_mouse_move(batch.mouseRelX[i], batch.mouseRelY[i]);
                break;
        }
    }
}
```

## Job System для SDL

### Паттерн: Background Loading с stdexec

> **Важно для ProjectV:** В движке ProjectV запрещено создавать кастомные `std::jthread`, `std::mutex` и
`std::condition_variable`. Используйте глобальный stdexec Job System движка.

```cpp
#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>

// Вместо кастомного JobSystem используйте глобальный stdexec scheduler
class SdlJobSystem {
    // Используем глобальный thread pool движка ProjectV
    exec::static_thread_pool& threadPool_;

public:
    explicit SdlJobSystem(exec::static_thread_pool& pool) : threadPool_(pool) {}

    // Запуск задачи в background через stdexec
    template<typename Func>
    void enqueue(Func&& func) {
        // Создаем sender для выполнения функции
        auto sender = stdexec::schedule(threadPool_.get_scheduler())
                    | stdexec::then(std::forward<Func>(func));

        // Запускаем асинхронно без ожидания
        stdexec::start_detached(std::move(sender));
    }

    // Запуск нескольких задач параллельно
    template<typename Func>
    void enqueue_bulk(size_t count, Func&& func) {
        auto sender = stdexec::schedule(threadPool_.get_scheduler())
                    | stdexec::bulk(count, std::forward<Func>(func));

        stdexec::start_detached(std::move(sender));
    }

    // Ожидание завершения всех задач (для синхронизации при необходимости)
    void wait_all() {
        // В ProjectV используйте stdexec::sync_wait для синхронизации
        // или проектируйте систему как полностью асинхронную
    }
};
```

### Интеграция с ECS

```cpp
// Воксельный движок: background mesh generation
class VoxelMeshJob {
    JobSystem& jobs_;
    SpscRingBuffer<MeshTask, 256>& taskQueue_;

public:
    void schedule_chunk_update(VoxelChunk* chunk) {
        // Задача в очередь — main thread не блокируется
        taskQueue_.push(MeshTask{chunk->id, chunk->data});
    }

    void process_completed() {
        MeshTask task;
        while (taskQueue_.pop(task)) {
            // Результат обратно в ECS
            update_chunk_mesh(task.chunkId, task.vertices, task.indices);
        }
    }
};
```

## GPU-Driven ввод

### Vulkan-специфичные расширения

```cpp
// GPU-driven input: отправляем данные ввода напрямую в GPU буфер

struct GPUInputData {
    // Плотный layout для upload
    alignas(16) glm::vec4 mousePosition;
    alignas(16) glm::vec4 mouseDelta;
    alignas(16) glm::uvec4 keyStates;  // 128 бит = 128 клавиш
    alignas(4) uint32_t frameNumber;
    alignas(4) uint32_t deltaTimeMs;
};

class GPUInputBuffer {
    VmaAllocator& allocator_;
    VkDevice device_;
    VkBuffer buffer_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = VK_NULL_HANDLE;

public:
    void create(VmaAllocator& allocator, VkDevice device) {
        allocator_ = allocator;
        device_ = device;

        VkBufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = sizeof(GPUInputData);
        info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        vmaCreateBuffer(&allocator_, &info, &allocInfo, &buffer_, &allocation_, nullptr);
    }

    void update(const InputBatch& batch) {
        GPUInputData data = {};

        // Компактное представление последнего состояния мыши
        if (batch.count.load() > 0) {
            size_t last = batch.count.load() - 1;
            data.mousePosition = {static_cast<float>(batch.mouseX[last]),
                                  static_cast<float>(batch.mouseY[last]), 0, 1};
            data.mouseDelta = {static_cast<float>(batch.mouseRelX[last]),
                               static_cast<float>(batch.mouseRelY[last]), 0, 0};
        }

        // Bitmask клавиш для шейдера
        for (size_t i = 0; i < batch.count.load(); ++i) {
            if (batch.eventTypes[i] == SDL_EVENT_KEY_DOWN) {
                uint32_t scancode = batch.keyScancodes[i];
                data.keyStates[scancode / 32] |= (1u << (scancode % 32));
            }
        }

        // Copy to mapped memory
        void* mapped;
        vmaMapMemory(&allocator_, allocation_, &mapped);
        std::memcpy(mapped, &data, sizeof(GPUInputData));
        vmaUnmapMemory(&allocator_, allocation_);
    }

    VkBuffer get_buffer() const { return buffer_; }
};
```

### Descriptor Set для ввода

```cpp
// Шейдер получает состояние ввода как uniform
// layout(set = 0, binding = 0) uniform InputData { ... } uInput;

void update_descriptor_set(VkDescriptorSet set, GPUInputBuffer& inputBuffer) {
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = inputBuffer.get_buffer();
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(GPUInputData);

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
}
```

## Cache-Olined ввод

### False sharing предотвращение

```cpp
// ❌ Плохо: два потока на одной кэш-линии
struct BadInput {
    std::atomic<uint64_t> mouseX;
    std::atomic<uint64_t> mouseY;  // False sharing!
};

// ✅ Хорошо: выравнивание по 64 байта
struct alignas(64) CacheLinePaddedInput {
    alignas(64) std::atomic<uint64_t> mouseX;
    alignas(64) std::atomic<uint64_t> mouseY;
    alignas(64) std::atomic<uint32_t> keyBits[16];  // 512 бит клавиш
};
```

### Frame allocator для ввода

```cpp
template<size_t PoolSize = 256 * 1024>
class FrameAllocator {
    std::array<std::byte, PoolSize> pool_;
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

    size_t used() const { return offset_; }
};

// Использование в SDL_AppIterate
FrameAllocator<256 * 1024> frameAlloc;

SDL_AppResult SDL_AppIterate(void* appstate) {
    frameAlloc.reset();  // Начало кадра

    // Аллокация временных данных — zero cost если не используется
    auto* inputSnapshot = frameAlloc.allocate(sizeof(InputSnapshot));

    // ... обработка ...

    // Не нужно освобождать — сброс в начале следующего кадра
    return SDL_APP_CONTINUE;
}
```

## Raw Input для воксельного редактора

### Конфигурация для точности

```cpp
void configure_raw_input_for_editing() {
    // Отключение системного сглаживания
    if (SDL_HasRawMouseMotion()) {
        SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
        SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SCALING, "0");
    }

    // Включение относительного режима для кисти
    SDL_SetRelativeMouseMode(SDL_TRUE);

    // Захват мыши для редактора
    SDL_CaptureMouse(SDL_TRUE);
}
```

### Immediate mode обработка

```cpp
// Низкоуровневый путь: событие → рендер без ECS overhead
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* app = static_cast<AppState*>(appstate);

    // Кисть редактора: минимальная задержка
    if (app->brushActive && event->type == SDL_EVENT_MOUSE_MOTION) {
        // Прямой path: событие → preview mesh
        update_brush_preview(event->motion.x, event->motion.y,
                            app->brushSize, app->brushColor);
        return SDL_APP_CONTINUE;
    }

    // Остальные события через ECS
    app->ecs->entity().set<InputEvent>({*event});

    return SDL_APP_CONTINUE;
}
```

## Multi-window редактор

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

    WindowData viewport_;
    WindowData tools_;
    WindowData properties_;

    VkInstance instance_ = VK_NULL_HANDLE;

public:
    void init(VkInstance instance) {
        instance_ = instance;

        // Viewport — главное окно
        viewport_.window = SDL_CreateWindow(
            "Voxel Viewport", 1280, 720,
            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
        );
        create_surface(viewport_);

        // Tools panel
        tools_.window = SDL_CreateWindow(
            "Tools", 300, 800,
            SDL_WINDOW_VULKAN
        );
        create_surface(tools_);
    }

    void handle_event(const SDL_Event& event) {
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            SDL_WindowID id = event.window.windowID;

            if (id == SDL_GetWindowID(viewport_.window)) {
                // Закрытие viewport = выход
                appShouldExit_ = true;
            } else {
                // Закрытие вспомогательного окна
                destroy_window_by_id(id);
            }
        }

        if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            handle_resize(event.window.windowID,
                         event.window.data1,
                         event.window.data2);
        }
    }

    void destroy_all() {
        destroy_window(viewport_);
        destroy_window(tools_);
        destroy_window(properties_);
    }

private:
    void create_surface(WindowData& data) {
        SDL_Vulkan_CreateSurface(data.window, instance_, nullptr, &data.surface);
    }

    void destroy_window(WindowData& data) {
        if (data.surface) {
            SDL_Vulkan_DestroySurface(instance_, data.surface, nullptr);
            data.surface = VK_NULL_HANDLE;
        }
        if (data.window) {
            SDL_DestroyWindow(data.window);
            data.window = nullptr;
        }
    }
};
```

## Профилирование

### Tracy для детального анализа

```cpp
#include <tracy/Tracy.hpp>

SDL_AppResult SDL_AppIterate(void* appstate) {
    ZoneScopedN("Frame");

    // Input phase
    {
        ZoneScopedN("Input");
        process_input_batch();
    }

    // Update phase (может быть multithreaded)
    {
        ZoneScopedN("Update");
        auto& jobSystem = get_job_system();
        jobSystem.process_pending();

        ZoneScopedN("ECS");
        ecs_->progress();
    }

    // Render phase
    {
        ZoneScopedN("Render");
        render_frame();
    }

    // GPU work submission
    {
        ZoneScopedN("Submit");
        submit_command_buffer();
    }

    TracyPlot("InputEvents", static_cast<int64_t>(inputBatch.count));
    TracyPlot("DrawCalls", static_cast<int64_t>(drawCallCount));
    TracyPlot("Voxels", static_cast<int64_t>(voxelCount));

    FrameMark;
    return SDL_APP_CONTINUE;
}
```

## Чеклист хардкорных оптимизаций

- [ ] SPSC queue для background → main thread коммуникации
- [ ] SoA layout для InputBatch (не AoS!)
- [ ] alignas(64) для разделяемых данных между потоками
- [ ] Frame allocator для zero-allocation в кадре
- [ ] Job System для background mesh generation
- [ ] GPU Input Buffer для передачи ввода в шейдеры
- [ ] Tracy профилирование каждой фазы кадра
- [ ] Raw Input для точного позиционирования кисти
- [ ] Multi-window manager для редактора
- [ ] Immediate mode path для brush preview

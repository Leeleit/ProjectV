# Продвинутое использование volk в ProjectV

Производительность, сложные паттерны, troubleshooting и отладка.

---

## Производительность воксельного рендеринга

### Влияние volk на производительность

Для воксельного движка ProjectV критична производительность draw calls. volk устраняет dispatch overhead, обеспечивая
прямые вызовы драйвера:

| Операция в ProjectV       | Вызовов/кадр | Влияние volk | Прирост производительности |
|---------------------------|--------------|--------------|----------------------------|
| Render voxel chunk        | 100-1000     | Значительное | 3-5%                       |
| Draw voxel instances      | 1000-10000   | Критическое  | 5-7%                       |
| Compute dispatch (voxels) | 10-100       | Умеренное    | 1-2%                       |
| Memory operations         | 50-200       | Минимальное  | <1%                        |

### Измерение производительности

Используйте Tracy для профилирования разницы между volk и системным loader:

```cpp
#include <tracy/TracyVulkan.hpp>

// Без volk (системный loader)
{
    TracyVkZone(tracyCtx, commandBuffer, "SystemLoader_Draw");
    vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
}

// С volk (прямые вызовы драйвера)
{
    TracyVkZone(tracyCtx, commandBuffer, "Volk_Draw");
    vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
}
```

### Оптимизация для массового рендеринга вокселей

```cpp
// Паттерн: массовый рендеринг воксельных чанков
void renderVoxelChunks(VkCommandBuffer cmd, const std::vector<VoxelChunk>& chunks) {
    TracyVkZone(tracyCtx, cmd, "RenderVoxelChunks");

    // Предварительная загрузка всех необходимых функций
    // volk обеспечивает прямые указатели на функции
    auto vkCmdDrawIndexed = volkGetDeviceProcAddr(device, "vkCmdDrawIndexed");

    for (const auto& chunk : chunks) {
        if (chunk.visible) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, chunk.pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                   chunk.pipelineLayout, 0, 1, &chunk.descriptorSet, 0, nullptr);
            vkCmdBindVertexBuffers(cmd, 0, 1, &chunk.vertexBuffer, &chunk.vertexOffset);
            vkCmdBindIndexBuffer(cmd, chunk.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            // Прямой вызов через volk - минимальный overhead
            vkCmdDrawIndexed(cmd, chunk.indexCount, 1, 0, 0, 0);
        }
    }
}
```

---

## Сложные паттерны использования

### Table-based интерфейс для многопоточности

Для безопасного доступа к Vulkan функциям из нескольких потоков:

```cpp
// Создание thread-safe таблицы функций
struct ThreadSafeVulkanTable {
    VolkDeviceTable deviceTable;
    std::mutex mutex;

    void initialize(VkDevice device) {
        std::lock_guard<std::mutex> lock(mutex);
        volkLoadDeviceTable(&deviceTable, device);
    }

    void cmdDraw(VkCommandBuffer cmd, uint32_t vertexCount, uint32_t instanceCount,
                 uint32_t firstVertex, uint32_t firstInstance) {
        std::lock_guard<std::mutex> lock(mutex);
        deviceTable.vkCmdDraw(cmd, vertexCount, instanceCount, firstVertex, firstInstance);
    }
};

// Использование в worker thread'ах
void renderThread(ThreadSafeVulkanTable& table, VkCommandBuffer cmd) {
    for (int i = 0; i < 1000; ++i) {
        table.cmdDraw(cmd, 4, 1, 0, 0);  // Thread-safe вызов
    }
}
```

### Динамическая загрузка extensions

ProjectV может динамически проверять и загружать extensions:

```cpp
struct VulkanExtensions {
    bool hasMeshShading = false;
    bool hasRayTracing = false;
    bool hasFragmentShadingRate = false;

    // Указатели на extension функции
    PFN_vkCmdDrawMeshTasksEXT vkCmdDrawMeshTasksEXT = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;

    void load(VkDevice device) {
        // Проверка поддержки extensions
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data());

        // Загрузка extension функций через volk
        for (const auto& ext : extensions) {
            if (strcmp(ext.extensionName, VK_EXT_MESH_SHADER_EXTENSION_NAME) == 0) {
                hasMeshShading = true;
                vkCmdDrawMeshTasksEXT = (PFN_vkCmdDrawMeshTasksEXT)
                    volkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksEXT");
            }
            if (strcmp(ext.extensionName, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 0) {
                hasRayTracing = true;
                vkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)
                    volkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR");
            }
        }
    }
};
```

### Hot-reload шейдеров с обновлением pipeline

```cpp
class HotReloadShaderManager {
    VkDevice device;
    std::unordered_map<std::string, VkPipeline> pipelines;
    std::unordered_map<std::string, std::filesystem::file_time_type> fileTimestamps;

public:
    void update() {
        for (auto& [shaderPath, pipeline] : pipelines) {
            auto currentTime = std::filesystem::last_write_time(shaderPath);
            if (currentTime != fileTimestamps[shaderPath]) {
                // Пересоздание pipeline
                vkDestroyPipeline(device, pipeline, nullptr);
                pipeline = createPipeline(shaderPath);
                fileTimestamps[shaderPath] = currentTime;

                printf("Hot-reloaded shader: %s\n", shaderPath.c_str());
            }
        }
    }

    VkPipeline createPipeline(const std::string& shaderPath) {
        // Использование volk для доступа к Vulkan функциям
        // при создании нового pipeline
        VkGraphicsPipelineCreateInfo createInfo = {};
        // ... настройка pipeline

        VkPipeline pipeline;
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline);
        return pipeline;
    }
};
```

---

## Интеграция с асинхронной загрузкой ресурсов

### Параллельная инициализация Vulkan в отдельном потоке

```cpp
class AsyncVulkanInitializer {
    std::future<bool> initFuture;
    std::promise<VkDevice> devicePromise;
    std::atomic<bool> initialized{false};

public:
    void startAsyncInitialization() {
        initFuture = std::async(std::launch::async, [this]() -> bool {
            // Инициализация volk в worker thread
            if (volkInitialize() != VK_SUCCESS) {
                return false;
            }

            // Создание instance и device в фоне
            VkInstance instance = createInstance();
            volkLoadInstance(instance);

            VkDevice device = createDevice(instance);
            volkLoadDevice(device);

            devicePromise.set_value(device);
            initialized = true;
            return true;
        });
    }

    VkDevice getDevice() {
        if (initialized) {
            return devicePromise.get_future().get();
        }
        return VK_NULL_HANDLE;
    }

    bool isReady() const {
        return initialized;
    }
};
```

### Lazy-загрузка Vulkan функций

```cpp
class LazyVulkanLoader {
    VkDevice device;
    std::unordered_map<std::string, void*> functionCache;

public:
    template<typename Func>
    Func getFunction(const char* name) {
        auto it = functionCache.find(name);
        if (it != functionCache.end()) {
            return reinterpret_cast<Func>(it->second);
        }

        // Загрузка через volk при первом использовании
        void* func = volkGetDeviceProcAddr(device, name);
        if (func) {
            functionCache[name] = func;
        }

        return reinterpret_cast<Func>(func);
    }

    void preloadCommonFunctions() {
        // Предварительная загрузка часто используемых функций
        const char* commonFunctions[] = {
            "vkCmdDraw",
            "vkCmdDrawIndexed",
            "vkCmdDispatch",
            "vkCmdCopyBuffer",
            "vkCmdPipelineBarrier"
        };

        for (const char* name : commonFunctions) {
            getFunction<void*>(name);
        }
    }
};
```

---

## Troubleshooting

### Распространённые ошибки и решения

#### Ошибка: "undefined reference to vkCreateInstance"

**Причина:** `VK_NO_PROTOTYPES` не определён или `volkLoadInstance()` не вызван.

**Решение:**

```cpp
// Убедитесь в правильном порядке:
#define VK_NO_PROTOTYPES
#define VOLK_IMPLEMENTATION  // Только в одном файле
#include "volk.h"

// В коде:
VkInstance instance;
vkCreateInstance(&createInfo, nullptr, &instance);
volkLoadInstance(instance);  // Критически важно!
```

#### Ошибка: "vkGetDeviceProcAddr returned NULL"

**Причина:** Попытка загрузить функцию до создания device или несуществующая функция.

**Решение:**

```cpp
// Проверка порядка инициализации:
1. volkInitialize()
2. vkCreateInstance() → volkLoadInstance()
3. vkCreateDevice() → volkLoadDevice()
4. Теперь можно загружать device-функции

// Проверка существования функции:
void* func = volkGetDeviceProcAddr(device, "vkSomeFunction");
if (!func) {
    // Функция не поддерживается, нужен fallback
    func = volkGetDeviceProcAddr(device, "vkAlternativeFunction");
}
```

#### Ошибка: Конфликт с другими библиотеками Vulkan

**Причина:** Другие библиотеки (ImGui, VMA) включают `vulkan.h` без `VK_NO_PROTOTYPES`.

**Решение:**

```cpp
// В CMakeLists.txt:
target_compile_definitions(ProjectV PRIVATE VK_NO_PROTOTYPES)

// В коде перед включением любых Vulkan заголовков:
#define VK_NO_PROTOTYPES
#include "volk.h"  // volk включит vulkan.h правильно

// Для ImGui:
#define VK_NO_PROTOTYPES
#include "volk.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
```

### Отладка проблем с производительностью

#### Профилирование dispatch overhead

```cpp
void profileVolkOverhead() {
    const int iterations = 1000000;

    // Без volk (через системный loader)
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        // Симуляция вызова через loader
        volatile auto func = vkGetDeviceProcAddr(device, "vkCmdDraw");
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto loaderTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // С volk (прямой указатель)
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        // Прямой вызов через volk
        volatile auto func = volkGetDeviceProcAddr(device, "vkCmdDraw");
    }
    end = std::chrono::high_resolution_clock::now();
    auto volkTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    printf("Loader: %lld µs, Volk: %lld µs, Speedup: %.2fx\n",
           loaderTime.count(), volkTime.count(),
           (double)loaderTime.count() / volkTime.count());
}
```

#### Tracy для отладки volk

```cpp
// Специальные зоны для отладки volk
#define TRACY_VOLK_ZONE(cmd, name) \
    TracyVkZone(tracyCtx, cmd, name); \
    { \
        static bool volkLoaded = false; \
        if (!volkLoaded) { \
            TracyVkZone(tracyCtx, cmd, "VolkFunctionLoad"); \
            volkLoadDevice(device); \
            volkLoaded = true; \
        } \
    }

// Использование
void renderFrame(VkCommandBuffer cmd) {
    TRACY_VOLK_ZONE(cmd, "RenderFrame");

    // Остальной код рендеринга
    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    // ...
}
```

---

## Расширенные конфигурации

### Кастомный Vulkan loader

```cpp
// Использование кастомного loader вместо системного
bool useCustomVulkanLoader(const char* libraryPath) {
    // Загрузка библиотеки вручную
    void* vulkanLib = SDL_LoadObject(libraryPath);
    if (!vulkanLib) {
        return false;
    }

    // Получение vkGetInstanceProcAddr из библиотеки
    auto getInstanceProcAddr = (PFN_vkGetInstanceProcAddr)
        SDL_LoadFunction(vulkanLib, "vkGetInstanceProcAddr");

    if (!getInstanceProcAddr) {
        SDL_UnloadObject(vulkanLib);
        return false;
    }

    // Инициализация volk с кастомным loader'ом
    volkInitializeCustom(getInstanceProcAddr);
    return true;
}
```

### Минимальная конфигурация для embedded

```cpp
// Конфигурация для embedded систем с ограниченной памятью
#define VOLK_MINIMAL_CONFIG

// Отключение ненужных функций
#define VOLK_NO_INSTANCE_FUNCTIONS
#define VOLK_NO_DEVICE_FUNCTIONS

// Только необходимые функции для ProjectV
const char* requiredFunctions[] = {
    "vkCmdDraw",
    "vkCmdDrawIndexed",
    "vkCmdDispatch",
    "vkCmdCopyBuffer",
    "vkCmdPipelineBarrier"
};

void initializeMinimalVolk(VkDevice device) {
    // Загрузка только необходимых функций
    for (const char* name : requiredFunctions) {
        volkGetDeviceProcAddr(device, name);
    }
}
```

---

## Best Practices для ProjectV

### 1. Единая точка инициализации

```cpp
// vulkan/volk_manager.hpp
class VolkManager {
    static VolkManager* instance;
    VkDevice device = VK_NULL_HANDLE;
    bool initialized = false;

public:
    static VolkManager& get() {
        static VolkManager manager;
        return manager;
    }

    void initialize(VkDevice dev) {
        device = dev;
        volkLoadDevice(device);
        initialized = true;
    }

    template<typename Func>
    Func getFunction(const char* name) {
        assert(initialized && "VolkManager not initialized");
        return reinterpret_cast<Func>(volkGetDeviceProcAddr(device, name));
    }
};
```

### 2. Preloading критических функций

```cpp
// Предварительная загрузка функций, используемых в hot path
void preloadCriticalFunctions(VkDevice device) {
    // Функции рендеринга вокселей
    volkGetDeviceProcAddr(device, "vkCmdDraw");
    volkGetDeviceProcAddr(device, "vkCmdDrawIndexed");
    volkGetDeviceProcAddr(device, "vkCmdDrawIndirect");

    // Функции compute для вокселей
    volkGetDeviceProcAddr(device, "vkCmdDispatch");
    volkGetDeviceProcAddr(device, "vkCmdDispatchIndirect");

    // Функции памяти
    volkGetDeviceProcAddr(device, "vkCmdCopyBuffer");
    volkGetDeviceProcAddr(device, "vkCmdCopyBufferToImage");
}
```

### 3. Валидация конфигурации в runtime

```cpp
bool validateVolkConfiguration() {
    // Проверка, что VK_NO_PROTOTYPES определён
#ifdef VK_NO_PROTOTYPES
    // Проверка, что volk инициализирован
    if (volkGetInstanceProcAddr == nullptr) {
        fprintf(stderr, "volk not initialized\n");
        return false;
    }

    // Проверка загрузки основных функций
    if (volkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance") == nullptr) {
        fprintf(stderr, "Failed to load vkCreateInstance\n");
        return false;
    }

    return true;
#else
    fprintf(stderr, "VK_NO_PROTOTYPES not defined\n");
    return false;
#endif
}
```

### 4. Graceful degradation при отсутствии функций

```cpp
class GracefulVulkanFeatures {
    struct {
        bool meshShadingAvailable = false;
        bool rayTracingAvailable = false;
        bool fragmentShadingRateAvailable = false;
    } features;

    struct {
        PFN_vkCmdDrawMeshTasksEXT vkCmdDrawMeshTasksEXT = nullptr;
        PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
        PFN_vkCmdSetFragmentShadingRateKHR vkCmdSetFragmentShadingRateKHR = nullptr;
    } functions;

public:
    void probeFeatures(VkDevice device) {
        // Проверка доступности extensions
        uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> extensions(count);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, extensions.data());

        for (const auto& ext : extensions) {
            if (strcmp(ext.extensionName, VK_EXT_MESH_SHADER_EXTENSION_NAME) == 0) {
                features.meshShadingAvailable = true;
                functions.vkCmdDrawMeshTasksEXT =
                    (PFN_vkCmdDrawMeshTasksEXT)volkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksEXT");
            }
            // ... проверка других extensions
        }
    }

    void drawVoxels(VkCommandBuffer cmd, DrawMode mode) {
        switch (mode) {
            case DrawMode::MeshShading:
                if (features.meshShadingAvailable && functions.vkCmdDrawMeshTasksEXT) {
                    functions.vkCmdDrawMeshTasksEXT(cmd, groupCountX, groupCountY, groupCountZ);
                } else {
                    // Fallback на традиционный рендеринг
                    vkCmdDraw(cmd, vertexCount, 1, 0, 0);
                }
                break;
            // ... другие режимы
        }
    }
};
```

---

## Отладка и мониторинг

### Инструменты отладки volk

```cpp
class VolkDebugger {
public:
    static void printLoadedFunctions(VkDevice device) {
        printf("=== Loaded Vulkan Functions ===\n");

        const char* functionNames[] = {
            "vkCreateInstance",
            "vkDestroyInstance",
            "vkCreateDevice",
            "vkDestroyDevice",
            "vkCmdDraw",
            "vkCmdDrawIndexed",
            "vkCmdDispatch",
            // ... добавьте другие важные функции
        };

        for (const char* name : functionNames) {
            void* func = volkGetDeviceProcAddr(device, name);
            printf("%-30s: %s\n", name, func ? "LOADED" : "MISSING");
        }

        printf("===============================\n");
    }

    static void checkFunctionPointers() {
        // Проверка, что указатели функций не null
        assert(vkCreateInstance != nullptr && "vkCreateInstance not loaded");
        assert(vkDestroyInstance != nullptr && "vkDestroyInstance not loaded");
        assert(vkCreateDevice != nullptr && "vkCreateDevice not loaded");
        // ... другие проверки
    }

    static void traceFunctionCalls(VkCommandBuffer cmd, const char* operation) {
#ifdef VOLK_TRACE_ENABLED
        static std::unordered_map<std::string, uint64_t> callCounts;
        static std::mutex mutex;

        std::lock_guard<std::mutex> lock(mutex);
        callCounts[operation]++;

        if (callCounts[operation] % 1000 == 0) {
            printf("[VOLK TRACE] %s called %llu times\n", operation, callCounts[operation]);
        }
#endif
    }
};
```

### Мониторинг производительности в реальном времени

```cpp
class VolkPerformanceMonitor {
    struct FrameStats {
        uint64_t drawCalls = 0;
        uint64_t dispatchCalls = 0;
        uint64_t memoryOperations = 0;
        std::chrono::high_resolution_clock::time_point frameStart;
    };

    std::array<FrameStats, 60> frameHistory;
    size_t currentFrame = 0;

public:
    void beginFrame() {
        frameHistory[currentFrame] = FrameStats();
        frameHistory[currentFrame].frameStart = std::chrono::high_resolution_clock::now();
    }

    void recordDrawCall() {
        frameHistory[currentFrame].drawCalls++;
    }

    void recordDispatchCall() {
        frameHistory[currentFrame].dispatchCalls++;
    }

    void endFrame() {
        auto frameEnd = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            frameEnd - frameHistory[currentFrame].frameStart);

        // Анализ производительности
        if (frameHistory[currentFrame].drawCalls > 1000) {
            printf("[PERF] High draw calls: %llu (frame time: %lld µs)\n",
                   frameHistory[currentFrame].drawCalls, duration.count());
        }

        currentFrame = (currentFrame + 1) % frameHistory.size();
    }

    void printStatistics() {
        uint64_t totalDrawCalls = 0;
        uint64_t totalDispatchCalls = 0;

        for (const auto& frame : frameHistory) {
            totalDrawCalls += frame.drawCalls;
            totalDispatchCalls += frame.dispatchCalls;
        }

        printf("=== Volk Performance Statistics ===\n");
        printf("Average draw calls per frame: %llu\n", totalDrawCalls / frameHistory.size());
        printf("Average dispatch calls per frame: %llu\n", totalDispatchCalls / frameHistory.size());
        printf("===================================\n");
    }
};

// Макрос для автоматического отслеживания
#define VOLK_PERF_MONITOR(monitor, operation) \
    monitor.record##operation##Call();

// Использование
VolkPerformanceMonitor monitor;

void renderVoxels(VkCommandBuffer cmd) {
    monitor.beginFrame();

    for (int i = 0; i < 1000; ++i) {
        vkCmdDraw(cmd, 4, 1, 0, 0);
        VOLK_PERF_MONITOR(monitor, Draw);
    }

    monitor.endFrame();
}
```

---

## Заключение

### Ключевые выводы для ProjectV

1. **Производительность критична** - volk даёт до 7% прироста для воксельного рендеринга
2. **Правильный порядок инициализации** - `volkLoadInstance()` и `volkLoadDevice()` должны вызываться сразу после
   создания соответствующих объектов
3. **Thread safety** - используйте table-based интерфейс для многопоточного доступа
4. **Graceful degradation** - проверяйте доступность функций и предоставляйте fallback
5. **Мониторинг** - используйте Tracy и кастомные инструменты для отладки производительности

### Рекомендации по использованию

- **Всегда** определяйте `VK_NO_PROTOTYPES` в CMake
- **Всегда** вызывайте `volkLoadDevice()` после `vkCreateDevice()`
- **Рассмотрите** preloading часто используемых функций
- **Используйте** Tracy для профилирования dispatch overhead
- **Реализуйте** graceful degradation для расширений Vulkan

### Будущие улучшения

Для ProjectV планируются следующие улучшения интеграции с volk:

1. **Автоматическая валидация конфигурации** при запуске
2. **Динамический выбор функций** на основе hardware capabilities
3. **Интеграция с hot-reload системой** шейдеров
4. **Расширенный мониторинг производительности** с детализацией по типам операций

Volk остаётся ключевым компонентом ProjectV, обеспечивая максимальную производительность Vulkan вызовов для воксельного
рендеринга.

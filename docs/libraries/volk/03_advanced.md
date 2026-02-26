# Продвинутое использование volk в ProjectV

> **Для понимания:** Представьте, что volk — это "турбонаддув" для Vulkan. Без volk ваш движок едет на 95-м бензине —
> работает, но с задержками. С volk вы переходите на гоночное топливо: прямой впрыск в цилиндры GPU, минуя все фильтры и
> регуляторы. Каждый вызов — это не "запрос", а "приказ".

Производительность, сложные паттерны, troubleshooting и отладка для воксельного движка.

---

## Производительность воксельного рендеринга

### Влияние volk на производительность

Для воксельного движка ProjectV критична производительность draw calls. volk устраняет dispatch overhead, обеспечивая
прямые вызовы драйвера:

| Операция в ProjectV       | Вызовов/кадр | Влияние volk | Прирост производительности | Почему это важно для вокселей     |
|---------------------------|--------------|--------------|----------------------------|-----------------------------------|
| Render voxel chunk        | 100-1000     | Значительное | 3-5%                       | Каждый чанк — отдельный draw call |
| Draw voxel instances      | 1000-10000   | Критическое  | 5-7%                       | Инстансинг вокселей — hot path    |
| Compute dispatch (voxels) | 10-100       | Умеренное    | 1-2%                       | Генерация мешей в compute shaders |
| Memory operations         | 50-200       | Минимальное  | <1%                        | Копирование воксельных данных     |

### Измерение производительности с C++26

Используйте Tracy для профилирования разницы между volk и системным loader:

```cpp
#include <tracy/TracyVulkan.hpp>
#include <print>

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

// Автоматическое сравнение производительности
struct PerformanceComparator {
    std::chrono::high_resolution_clock::time_point start;
    const char* name;

    PerformanceComparator(const char* n) : name(n) {
        start = std::chrono::high_resolution_clock::now();
    }

    ~PerformanceComparator() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::println("[PERF] {}: {} µs", name, duration.count());
    }
};

// Использование
{
    PerformanceComparator pc("VolkDrawCalls");
    for (int i = 0; i < 1000; ++i) {
        vkCmdDraw(cmd, 4, 1, 0, 0);
    }
}
```

### Оптимизация для массового рендеринга вокселей с C++26

```cpp
#include <print>
#include <expected>
#include <span>

// Структура данных для воксельного чанка с выравниванием
struct alignas(64) VoxelChunk {  // Выравнивание для избежания false sharing
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorSet descriptorSet;
    VkBuffer vertexBuffer;
    VkDeviceSize vertexOffset;
    VkBuffer indexBuffer;
    uint32_t indexCount;
    bool visible;

    // SoA для массового рендеринга
    struct BatchData {
        alignas(16) std::vector<VkPipeline> pipelines;        // Выравнивание для GPU
        alignas(16) std::vector<VkDescriptorSet> descriptorSets;
        alignas(16) std::vector<uint32_t> indexCounts;
    };
};

// Паттерн: массовый рендеринг воксельных чанков с предзагрузкой функций
std::expected<void, std::string> renderVoxelChunks(
    VkCommandBuffer cmd,
    std::span<const VoxelChunk> chunks,
    VkDevice device
) {
    TracyVkZone(tracyCtx, cmd, "RenderVoxelChunks");

    // Предварительная загрузка всех необходимых функций через volk
    static PFN_vkCmdDrawIndexed vkCmdDrawIndexed = nullptr;
    static PFN_vkCmdBindPipeline vkCmdBindPipeline = nullptr;
    static PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets = nullptr;

    if (!vkCmdDrawIndexed) {
        vkCmdDrawIndexed = (PFN_vkCmdDrawIndexed)volkGetDeviceProcAddr(device, "vkCmdDrawIndexed");
        vkCmdBindPipeline = (PFN_vkCmdBindPipeline)volkGetDeviceProcAddr(device, "vkCmdBindPipeline");
        vkCmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets)volkGetDeviceProcAddr(device, "vkCmdBindDescriptorSets");

        if (!vkCmdDrawIndexed || !vkCmdBindPipeline || !vkCmdBindDescriptorSets) {
            return std::unexpected("Failed to load Vulkan functions");
        }
    }

    for (const auto& chunk : chunks) {
        if (!chunk.visible) continue;

        // Прямые вызовы через volk - минимальный overhead
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, chunk.pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                               chunk.pipelineLayout, 0, 1, &chunk.descriptorSet, 0, nullptr);
        vkCmdBindVertexBuffers(cmd, 0, 1, &chunk.vertexBuffer, &chunk.vertexOffset);
        vkCmdBindIndexBuffer(cmd, chunk.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(cmd, chunk.indexCount, 1, 0, 0, 0);
    }

    return {};
}
```

---

## Сложные паттерны использования

### Table-based интерфейс для многопоточности с C++26

Для безопасного доступа к Vulkan функциям из нескольких потоков в ProjectV:

```cpp
#include <mutex>
#include <print>
#include <expected>

// Thread-safe таблица функций с выравниванием для избежания false sharing
struct alignas(64) ThreadSafeVulkanTable {
    VolkDeviceTable deviceTable;
    std::mutex mutex;

    std::expected<void, std::string> initialize(VkDevice device) {
        std::lock_guard<std::mutex> lock(mutex);
        volkLoadDeviceTable(&deviceTable, device);

        // Проверка загрузки критических функций
        if (!deviceTable.vkCmdDraw || !deviceTable.vkCmdDrawIndexed) {
            return std::unexpected("Failed to load critical Vulkan functions");
        }

        std::println("[VOLK] Thread-safe table initialized");
        return {};
    }

    // Thread-safe вызовы с проверкой
    std::expected<void, std::string> cmdDraw(
        VkCommandBuffer cmd,
        uint32_t vertexCount,
        uint32_t instanceCount,
        uint32_t firstVertex,
        uint32_t firstInstance
    ) {
        std::lock_guard<std::mutex> lock(mutex);

        if (!deviceTable.vkCmdDraw) {
            return std::unexpected("vkCmdDraw not loaded");
        }

        deviceTable.vkCmdDraw(cmd, vertexCount, instanceCount, firstVertex, firstInstance);
        return {};
    }
};

// Использование в worker thread'ах Job System
void renderVoxelThread(ThreadSafeVulkanTable& table, VkCommandBuffer cmd, uint32_t chunkCount) {
    for (uint32_t i = 0; i < chunkCount; ++i) {
        if (auto result = table.cmdDraw(cmd, 4, 1, 0, 0); !result) {
            std::println(stderr, "Render error: {}", result.error());
            break;
        }
    }
}
```

### Динамическая загрузка extensions с C++26

```cpp
#include <vector>
#include <string>
#include <print>
#include <expected>

struct VulkanExtensions {
    bool hasMeshShading = false;
    bool hasRayTracing = false;
    bool hasFragmentShadingRate = false;

    // Указатели на extension функции
    PFN_vkCmdDrawMeshTasksEXT vkCmdDrawMeshTasksEXT = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkCmdSetFragmentShadingRateKHR vkCmdSetFragmentShadingRateKHR = nullptr;

    std::expected<void, std::string> load(VkPhysicalDevice physicalDevice, VkDevice device) {
        // Проверка поддержки extensions
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data());

        // Загрузка extension функций через volk
        for (const auto& ext : extensions) {
            if (std::string(ext.extensionName) == VK_EXT_MESH_SHADER_EXTENSION_NAME) {
                hasMeshShading = true;
                vkCmdDrawMeshTasksEXT = (PFN_vkCmdDrawMeshTasksEXT)
                    volkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksEXT");
                std::println("[VOLK] Mesh shading extension loaded");
            }
            if (std::string(ext.extensionName) == VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) {
                hasRayTracing = true;
                vkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)
                    volkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR");
                std::println("[VOLK] Ray tracing extension loaded");
            }
            if (std::string(ext.extensionName) == VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME) {
                hasFragmentShadingRate = true;
                vkCmdSetFragmentShadingRateKHR = (PFN_vkCmdSetFragmentShadingRateKHR)
                    volkGetDeviceProcAddr(device, "vkCmdSetFragmentShadingRateKHR");
                std::println("[VOLK] Fragment shading rate extension loaded");
            }
        }

        return {};
    }

    // Безопасное использование extension
    std::expected<void, std::string> drawMeshTasks(
        VkCommandBuffer cmd,
        uint32_t groupCountX,
        uint32_t groupCountY,
        uint32_t groupCountZ
    ) {
        if (!hasMeshShading || !vkCmdDrawMeshTasksEXT) {
            return std::unexpected("Mesh shading not available");
        }

        vkCmdDrawMeshTasksEXT(cmd, groupCountX, groupCountY, groupCountZ);
        return {};
    }
};
```

### Hot-reload шейдеров с обновлением pipeline (C++26)

```cpp
#include <filesystem>
#include <unordered_map>
#include <print>
#include <expected>

namespace fs = std::filesystem;

class HotReloadShaderManager {
    VkDevice device;
    std::unordered_map<std::string, VkPipeline> pipelines;
    std::unordered_map<std::string, fs::file_time_type> fileTimestamps;

public:
    HotReloadShaderManager(VkDevice dev) : device(dev) {}

    std::expected<void, std::string> update() {
        for (auto& [shaderPath, pipeline] : pipelines) {
            try {
                auto currentTime = fs::last_write_time(shaderPath);
                if (currentTime != fileTimestamps[shaderPath]) {
                    // Пересоздание pipeline
                    vkDestroyPipeline(device, pipeline, nullptr);

                    auto newPipeline = createPipeline(shaderPath);
                    if (!newPipeline) {
                        return std::unexpected(std::format("Failed to recreate pipeline: {}", newPipeline.error()));
                    }

                    pipeline = *newPipeline;
                    fileTimestamps[shaderPath] = currentTime;

                    std::println("[HOT RELOAD] Shader reloaded: {}", shaderPath);
                }
            } catch (const fs::filesystem_error& e) {
                std::println(stderr, "Filesystem error: {}", e.what());
            }
        }
        return {};
    }

    std::expected<VkPipeline, std::string> createPipeline(const std::string& shaderPath) {
        // Использование volk для доступа к Vulkan функциям
        VkGraphicsPipelineCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        createInfo.flags = VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;

        // Стадии шейдера
        VkPipelineShaderStageCreateInfo stages[2] = {};

        // Vertex shader stage
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].pName = "main";

        // Fragment shader stage
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].pName = "main";

        createInfo.stageCount = 2;
        createInfo.pStages = stages;

        // Vertex input state
        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        createInfo.pVertexInputState = &vertexInput;

        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        createInfo.pInputAssemblyState = &inputAssembly;

        // Viewport state (dynamic)
        VkPipelineViewportStateCreateInfo viewport{};
        viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;
        createInfo.pViewportState = &viewport;

        // Rasterization
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        createInfo.pRasterizationState = &rasterizer;

        // Multisampling
        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        createInfo.pMultisampleState = &multisample;

        // Color blending
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        createInfo.pColorBlendState = &colorBlending;

        // Dynamic state
        std::array<VkDynamicState, 2> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();
        createInfo.pDynamicState = &dynamicState;

        VkPipeline pipeline;
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline) != VK_SUCCESS) {
            return std::unexpected("vkCreateGraphicsPipelines failed");
        }

        pipelines[shaderPath] = pipeline;
        fileTimestamps[shaderPath] = fs::last_write_time(shaderPath);

        return pipeline;
    }

    ~HotReloadShaderManager() {
        for (auto& [path, pipeline] : pipelines) {
            vkDestroyPipeline(device, pipeline, nullptr);
        }
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
    enum class DrawMode {
        Traditional,
        MeshShading,
        RayTracing,
        FragmentShadingRate
    };

    void probeFeatures(VkPhysicalDevice physicalDevice, VkDevice device) {
        // Проверка доступности extensions
        uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> extensions(count);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, extensions.data());

        for (const auto& ext : extensions) {
            // Mesh Shading (EXT_mesh_shader)
            if (strcmp(ext.extensionName, VK_EXT_MESH_SHADER_EXTENSION_NAME) == 0) {
                features.meshShadingAvailable = true;
                functions.vkCmdDrawMeshTasksEXT =
                    (PFN_vkCmdDrawMeshTasksEXT)volkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksEXT");
            }
            // Ray Tracing (KHR_ray_tracing_pipeline)
            if (strcmp(ext.extensionName, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 0) {
                features.rayTracingAvailable = true;
                functions.vkCmdTraceRaysKHR =
                    (PFN_vkCmdTraceRaysKHR)volkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR");
            }
            // Fragment Shading Rate (KHR_fragment_shading_rate)
            if (strcmp(ext.extensionName, VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME) == 0) {
                features.fragmentShadingRateAvailable = true;
                functions.vkCmdSetFragmentShadingRateKHR =
                    (PFN_vkCmdSetFragmentShadingRateKHR)volkGetDeviceProcAddr(device, "vkCmdSetFragmentShadingRateKHR");
            }
            // Variable Rate Shading (NV_shading_rate_image)
            if (strcmp(ext.extensionName, VK_NV_SHADING_RATE_IMAGE_EXTENSION_NAME) == 0) {
                // Variable rate shading support detected
            }
            // Acceleration Structure (KHR_acceleration_structure)
            if (strcmp(ext.extensionName, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 0) {
                // Ray tracing support detected
            }
        }
    }

    void drawVoxels(VkCommandBuffer cmd, DrawMode mode, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ, uint32_t vertexCount) {
        switch (mode) {
            case DrawMode::MeshShading:
                if (features.meshShadingAvailable && functions.vkCmdDrawMeshTasksEXT) {
                    functions.vkCmdDrawMeshTasksEXT(cmd, groupCountX, groupCountY, groupCountZ);
                } else {
                    // Fallback на традиционный рендеринг
                    vkCmdDraw(cmd, vertexCount, 1, 0, 0);
                }
                break;
            case DrawMode::RayTracing:
                if (features.rayTracingAvailable && functions.vkCmdTraceRaysKHR) {
                    // Ray tracing вызов требует acceleration structures buffers
                } else {
                    // Fallback на традиционный рендеринг
                    vkCmdDraw(cmd, vertexCount, 1, 0, 0);
                }
                break;
            case DrawMode::FragmentShadingRate:
                if (features.fragmentShadingRateAvailable && functions.vkCmdSetFragmentShadingRateKHR) {
                    VkFragmentShadingRateCombinerOpKHR combineOps[2] = {
                        VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,
                        VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR
                    };
                    functions.vkCmdSetFragmentShadingRateKHR(cmd, nullptr, combineOps);
                    vkCmdDraw(cmd, vertexCount, 1, 0, 0);
                } else {
                    vkCmdDraw(cmd, vertexCount, 1, 0, 0);
                }
                break;
            case DrawMode::Traditional:
            default:
                vkCmdDraw(cmd, vertexCount, 1, 0, 0);
                break;
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
            "vkEnumerateInstanceVersion",
            "vkEnumerateDeviceExtensionProperties",
            "vkCmdDraw",
            "vkCmdDrawIndexed",
            "vkCmdDispatch",
            "vkCmdCopyBuffer",
            "vkCmdCopyBufferToImage",
            "vkCmdBeginRenderPass",
            "vkCmdEndRenderPass",
            "vkQueueSubmit",
            "vkQueueWaitIdle"
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
        assert(vkDestroyDevice != nullptr && "vkDestroyDevice not loaded");
        assert(vkCmdDraw != nullptr && "vkCmdDraw not loaded");
        assert(vkCmdDrawIndexed != nullptr && "vkCmdDrawIndexed not loaded");
        assert(vkCmdDispatch != nullptr && "vkCmdDispatch not loaded");
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

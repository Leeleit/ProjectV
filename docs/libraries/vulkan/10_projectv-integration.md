# Vulkan ProjectV Integration

Специфика интеграции Vulkan в воксельный движок ProjectV.

---

## Архитектура ProjectV + Vulkan

### Общая схема

```
┌─────────────────────────────────────────────────────────────┐
│                      ProjectV Engine                         │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐    │
│  │   flecs  │  │  Jolt    │  │  miniaudio│ │  ImGui   │    │
│  │   ECS    │  │ Physics  │  │   Audio   │ │   UI     │    │
│  └────┬─────┘  └────┬─────┘  └────┬──────┘ └────┬─────┘    │
│       │             │              │             │          │
│       └─────────────┴──────────────┴─────────────┘          │
│                           │                                  │
│                     ┌─────▼─────┐                           │
│                     │  Vulkan   │                           │
│                     │ Renderer  │                           │
│                     └─────┬─────┘                           │
│                           │                                  │
│  ┌────────────────────────┴────────────────────────┐       │
│  │                     volk                         │       │
│  │              (Vulkan function loader)            │       │
│  └────────────────────────┬────────────────────────┘       │
│                           │                                  │
│  ┌────────────────────────┴────────────────────────┐       │
│  │                     VMA                          │       │
│  │           (Vulkan Memory Allocator)              │       │
│  └────────────────────────┬────────────────────────┘       │
│                           │                                  │
│                     ┌─────▼─────┐                           │
│                     │   SDL3    │                           │
│                     │ (Window)  │                           │
│                     └───────────┘                           │
└─────────────────────────────────────────────────────────────┘
```

### Ключевые компоненты

| Компонент       | Роль                | Версия |
|-----------------|---------------------|--------|
| **Vulkan**      | Графический API     | 1.4    |
| **volk**        | Загрузка функций    | latest |
| **VMA**         | Управление памятью  | 3.x    |
| **SDL3**        | Окно, ввод, surface | 3.x    |
| **flecs**       | ECS архитектура     | 4.x    |
| **JoltPhysics** | Физика              | latest |
| **ImGui**       | Debug UI            | 1.90+  |
| **Tracy**       | Профилирование      | 0.10+  |

---

## Стек технологий

### Vulkan 1.4 Core Features

ProjectV использует следующие core features Vulkan 1.4:

| Feature               | Использование          |
|-----------------------|------------------------|
| `timelineSemaphore`   | Async compute sync     |
| `bufferDeviceAddress` | GPU pointers           |
| `descriptorIndexing`  | Bindless textures      |
| `synchronization2`    | Improved barriers      |
| `dynamicRendering`    | No render pass objects |

### Расширения

**Обязательные:**

```cpp
const char* requiredDeviceExtensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
};
```

**Рекомендуемые:**

```cpp
const char* optionalDeviceExtensions[] = {
    VK_EXT_MESH_SHADER_EXTENSION_NAME,       // Voxel rendering
    VK_EXT_SHADER_OBJECT_EXTENSION_NAME,     // Hot reload
    VK_NV_MESH_SHADER_EXTENSION_NAME,        // Fallback (NVIDIA)
    VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,     // Memory monitoring
};
```

### volk Integration

```cpp
#include <volk.h>

class VulkanContext {
public:
    void initialize() {
        // 1. Инициализация volk
        VkResult result = volkInitialize();
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to initialize volk");
        }

        // 2. Проверка версии
        uint32_t version = volkGetInstanceVersion();
        if (version < VK_API_VERSION_1_3) {
            throw std::runtime_error("Vulkan 1.3+ required");
        }

        // 3. Создание instance
        createInstance();

        // 4. Загрузка instance functions
        volkLoadInstance(instance_);

        // 5. Выбор и создание device
        selectPhysicalDevice();
        createDevice();

        // 6. Загрузка device functions
        volkLoadDevice(device_);
    }
};
```

### VMA Integration

```cpp
#include <vk_mem_alloc.h>

class MemoryAllocator {
    VmaAllocator allocator_;

public:
    void initialize(VkInstance instance, VkPhysicalDevice physDevice, VkDevice device) {
        VmaAllocatorCreateInfo createInfo = {};
        createInfo.instance = instance;
        createInfo.physicalDevice = physDevice;
        createInfo.device = device;
        createInfo.vulkanApiVersion = VK_API_VERSION_1_4;
        createInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

        vmaCreateAllocator(&createInfo, &allocator_);
    }

    // Для voxel data
    AllocatedBuffer createVoxelBuffer(VkDeviceSize size) {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        AllocatedBuffer result;
        vmaCreateBuffer(allocator_, &bufferInfo, &allocInfo,
                       &result.buffer, &result.allocation, nullptr);

        // Получаем адрес для BDA
        VkBufferDeviceAddressInfo addrInfo = {};
        addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addrInfo.buffer = result.buffer;
        result.address = vkGetBufferDeviceAddress(device_, &addrInfo);

        return result;
    }
};
```

---

## Требования к GPU

### Минимальные требования

| Характеристика   | Минимум | Рекомендуется |
|------------------|---------|---------------|
| Vulkan Version   | 1.2     | 1.4           |
| VRAM             | 4 GB    | 8+ GB         |
| Compute Queues   | 1       | Dedicated     |
| Descriptor Sets  | 64      | 1024+         |
| Max Texture Size | 8192    | 16384+        |

### Feature Requirements

```cpp
struct GPURequirements {
    // Core features
    bool timelineSemaphore = true;
    bool bufferDeviceAddress = true;
    bool descriptorIndexing = true;

    // Optional but recommended
    bool meshShaders = false;      // Для voxel rendering
    bool shaderObjects = false;    // Для hot reload

    // Limits
    uint32_t maxDescriptorSetSamplers = 1000;
    uint32_t maxDescriptorSetSampledImages = 1000;
    uint32_t maxComputeWorkGroupSize = 1024;
    uint64_t bufferDeviceAddressCaptureReplay = true;
};

bool checkGPURequirements(VkPhysicalDevice device, const GPURequirements& reqs) {
    // Проверка features
    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeatures = {};
    timelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;

    VkPhysicalDeviceBufferDeviceAddressFeatures bdaFeatures = {};
    bdaFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;

    // Chain pNext
    features2.pNext = &timelineFeatures;
    timelineFeatures.pNext = &bdaFeatures;

    vkGetPhysicalDeviceFeatures2(device, &features2);

    if (!timelineFeatures.timelineSemaphore) return false;
    if (!bdaFeatures.bufferDeviceAddress) return false;

    // Проверка limits
    VkPhysicalDeviceProperties2 props2 = {};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    vkGetPhysicalDeviceProperties2(device, &props2);

    auto& limits = props2.properties.limits;
    if (limits.maxDescriptorSetSamplers < reqs.maxDescriptorSetSamplers) return false;

    return true;
}
```

### Hardware Support Matrix

| GPU Series       | Vulkan 1.4 | Mesh Shaders | BDA | Timeline Sem |
|------------------|------------|--------------|-----|--------------|
| NVIDIA RTX 20xx+ | ✅          | ✅            | ✅   | ✅            |
| NVIDIA GTX 16xx  | ✅          | ✅            | ✅   | ✅            |
| AMD RDNA2+       | ✅          | ✅            | ✅   | ✅            |
| AMD RDNA1        | ✅          | ❌            | ✅   | ✅            |
| Intel Arc        | ✅          | ✅            | ✅   | ✅            |
| Intel Iris Xe    | ✅          | ❌            | ✅   | ✅            |

---

## Паттерны инициализации

### Порядок инициализации

```cpp
class Engine {
    std::unique_ptr<VulkanContext> vulkan_;
    std::unique_ptr<MemoryAllocator> allocator_;
    std::unique_ptr<Swapchain> swapchain_;
    std::unique_ptr<Renderer> renderer_;
    flecs::world ecs_;

public:
    void initialize() {
        // 1. SDL initialization
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

        // 2. Vulkan context (volk, instance, device)
        vulkan_ = std::make_unique<VulkanContext>();
        vulkan_->initialize();

        // 3. Memory allocator (VMA)
        allocator_ = std::make_unique<MemoryAllocator>();
        allocator_->initialize(vulkan_->instance(), vulkan_->physicalDevice(), vulkan_->device());

        // 4. Swapchain
        swapchain_ = std::make_unique<Swapchain>();
        swapchain_->initialize(vulkan_.get(), window_);

        // 5. Renderer (pipelines, descriptor sets)
        renderer_ = std::make_unique<Renderer>();
        renderer_->initialize(vulkan_.get(), allocator_.get());

        // 6. ECS systems
        registerECSSystems();
    }
};
```

### flecs + Vulkan Integration

```cpp
// Vulkan context component
struct VulkanContextComponent {
    VulkanContext* context;
    MemoryAllocator* allocator;
};

// Buffer component
struct GPUBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VkDeviceAddress address;
    VkDeviceSize size;
};

// Mesh component
struct MeshComponent {
    GPUBuffer vertexBuffer;
    GPUBuffer indexBuffer;
    uint32_t indexCount;
    uint32_t materialIndex;
};

// ECS system for rendering
void registerRenderSystems(flecs::world& ecs, Renderer* renderer) {
    // Culling system
    ecs.system<RenderableComponent, TransformComponent>("CullingSystem")
        .kind(flecs::OnUpdate)
        .iter([renderer](flecs::iter& it,
                         RenderableComponent* renderable,
                         TransformComponent* transform) {
            // GPU culling через compute shader
        });

    // Render system
    ecs.system<MeshComponent, MaterialComponent>("RenderSystem")
        .kind(flecs::OnStore)
        .iter([renderer](flecs::iter& it,
                         MeshComponent* mesh,
                         MaterialComponent* material) {
            // Recording draw commands
            for (auto i : it) {
                renderer->drawMesh(mesh[i], material[i]);
            }
        });
}
```

---

## Синхронизация в движке

### Frame Sync Pattern

```cpp
class FrameSync {
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    struct FrameData {
        VkFence renderFence;
        VkSemaphore imageAvailableSemaphore;
        VkSemaphore renderFinishedSemaphore;
        VkCommandBuffer commandBuffer;

        // Timeline semaphore для async compute
        VkSemaphore computeSemaphore;
        uint64_t computeValue = 0;
    };

    std::array<FrameData, MAX_FRAMES_IN_FLIGHT> frames_;
    uint32_t currentFrame_ = 0;

public:
    void initialize(VkDevice device) {
        for (auto& frame : frames_) {
            // Fence
            VkFenceCreateInfo fenceInfo = {};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            vkCreateFence(device, &fenceInfo, nullptr, &frame.renderFence);

            // Binary semaphores
            VkSemaphoreCreateInfo semInfo = {};
            semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            vkCreateSemaphore(device, &semInfo, nullptr, &frame.imageAvailableSemaphore);
            vkCreateSemaphore(device, &semInfo, nullptr, &frame.renderFinishedSemaphore);

            // Timeline semaphore
            VkSemaphoreTypeCreateInfo timelineInfo = {};
            timelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
            timelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
            timelineInfo.initialValue = 0;

            VkSemaphoreCreateInfo timelineSemInfo = {};
            timelineSemInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            timelineSemInfo.pNext = &timelineInfo;
            vkCreateSemaphore(device, &timelineSemInfo, nullptr, &frame.computeSemaphore);
        }
    }

    void waitForFrame(VkDevice device) {
        auto& frame = frames_[currentFrame_];
        vkWaitForFences(device, 1, &frame.renderFence, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &frame.renderFence);
    }

    void advanceFrame() {
        currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    }
};
```

### Async Compute Sync

```cpp
// Graphics ждёт compute (voxel mesh generation)
void submitComputeAndGraphics(FrameData& frame, VkQueue computeQueue, VkQueue graphicsQueue) {
    // 1. Submit compute
    frame.computeValue++;

    VkTimelineSemaphoreSubmitInfo computeTimeline = {};
    computeTimeline.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    uint64_t computeSignal = frame.computeValue;
    computeTimeline.signalSemaphoreValueCount = 1;
    computeTimeline.pSignalSemaphoreValues = &computeSignal;

    VkSubmitInfo computeSubmit = {};
    computeSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    computeSubmit.pNext = &computeTimeline;
    computeSubmit.signalSemaphoreCount = 1;
    computeSubmit.pSignalSemaphores = &frame.computeSemaphore;
    computeSubmit.commandBufferCount = 1;
    computeSubmit.pCommandBuffers = &computeCmdBuffer;

    vkQueueSubmit(computeQueue, 1, &computeSubmit, VK_NULL_HANDLE);

    // 2. Submit graphics (wait for compute)
    uint64_t graphicsWait = frame.computeValue;

    VkTimelineSemaphoreSubmitInfo graphicsTimeline = {};
    graphicsTimeline.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    graphicsTimeline.waitSemaphoreValueCount = 1;
    graphicsTimeline.pWaitSemaphoreValues = &graphicsWait;

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

    VkSubmitInfo graphicsSubmit = {};
    graphicsSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    graphicsSubmit.pNext = &graphicsTimeline;
    graphicsSubmit.waitSemaphoreCount = 1;
    graphicsSubmit.pWaitSemaphores = &frame.computeSemaphore;
    graphicsSubmit.pWaitDstStageMask = &waitStage;
    graphicsSubmit.signalSemaphoreCount = 1;
    graphicsSubmit.pSignalSemaphores = &frame.renderFinishedSemaphore;
    graphicsSubmit.commandBufferCount = 1;
    graphicsSubmit.pCommandBuffers = &graphicsCmdBuffer;

    vkQueueSubmit(graphicsQueue, 1, &graphicsSubmit, frame.renderFence);
}
```

---

## Ключевые принципы интеграции

1. **volk сначала** — загрузка функций до любого Vulkan API вызова
2. **VMA для всей памяти** — никаких ручных VkDeviceMemory
3. **Timeline semaphores** — основная синхронизация между очередями
4. **BDA для voxel data** — прямые указатели вместо дескрипторов
5. **flecs systems** — разделение logic и rendering

# GPU профилирование Tracy

**🟡 Уровень 2: Средний** — Профилирование GPU команд для Vulkan, OpenGL, Direct3D, Metal.

---

## Обзор GPU профилирования

Tracy позволяет измерять время выполнения GPU команд путём вставки timestamp queries в командные буферы.

### Поддерживаемые API

| API         | Заголовочный файл |
|-------------|-------------------|
| Vulkan      | `TracyVulkan.hpp` |
| OpenGL      | `TracyOpenGL.hpp` |
| Direct3D 11 | `TracyD3D11.hpp`  |
| Direct3D 12 | `TracyD3D12.hpp`  |
| Metal       | `TracyMetal.hmm`  |
| CUDA        | `TracyCUDA.hpp`   |
| OpenCL      | `TracyOpenCL.hpp` |

---

## Vulkan

### Инициализация контекста

```cpp
#include "tracy/TracyVulkan.hpp"

// Создание контекста
tracy::VkCtx* vulkanCtx = tracy::CreateVkContext(
    physicalDevice,
    device,
    queue,
    commandBuffer  // Для инициализации queries
);
```

### Профилирование команд

```cpp
void renderFrame(VkCommandBuffer cmd) {
    // Начало зоны
    TracyVkZone(vulkanCtx, cmd, "RenderFrame");

    // Команды рендеринга...
    vkCmdDraw(cmd, ...);

    // Конец зоны - автоматический при выходе из scope
}

// Или ручное управление
void renderPass(VkCommandBuffer cmd) {
    TracyVkZoneTransient(vulkanCtx, zone, cmd, "RenderPass", true);

    // Команды...

    zone = nullptr;  // Завершить зону
}
```

### Сборка контекста

```cpp
// Вызвать после vkQueueSubmit
TracyVkCollect(vulkanCtx, queue);
```

### Очистка

```cpp
void cleanup() {
    tracy::DestroyVkContext(vulkanCtx);
}
```

### Полный пример Vulkan

```cpp
#include "tracy/TracyVulkan.hpp"

class VulkanRenderer {
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkCommandPool commandPool;
    tracy::VkCtx* tracyCtx;

public:
    void init() {
        // Инициализация Vulkan...

        // Создание временного command buffer для инициализации
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer initCmd;
        vkAllocateCommandBuffers(device, &allocInfo, &initCmd);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(initCmd, &beginInfo);

        tracyCtx = tracy::CreateVkContext(
            physicalDevice, device, graphicsQueue, initCmd
        );

        vkEndCommandBuffer(initCmd);
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &initCmd;
        vkQueueSubmit(graphicsQueue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);
        vkFreeCommandBuffers(device, commandPool, 1, &initCmd);
    }

    void render(VkCommandBuffer cmd) {
        TracyVkZone(tracyCtx, cmd, "Render");

        {
            TracyVkZone(tracyCtx, cmd, "GeometryPass");
            drawGeometry(cmd);
        }

        {
            TracyVkZone(tracyCtx, cmd, "LightingPass");
            drawLighting(cmd);
        }
    }

    void present() {
        // ... submit command buffers ...
        TracyVkCollect(tracyCtx, graphicsQueue);
    }

    void cleanup() {
        tracy::DestroyVkContext(tracyCtx);
    }
};
```

---

## OpenGL

### Инициализация

```cpp
#include "tracy/TracyOpenGL.hpp"

void init() {
    // После создания OpenGL context
    TracyGpuContext;
}
```

### Профилирование

```cpp
void renderFrame() {
    TracyGpuZone("RenderFrame");

    {
        TracyGpuZone("Geometry");
        drawGeometry();
    }

    {
        TracyGpuZone("Lighting");
        drawLighting();
    }
}
```

### Collect и очистка

```cpp
void present() {
    TracyGpuCollect;  // Собрать данные GPU

    // Swap buffers...
}
```

### Пример OpenGL

```cpp
#include "tracy/TracyOpenGL.hpp"

class OpenGLRenderer {
public:
    void init() {
        // Создание OpenGL context через SDL/GLFW...
        TracyGpuContext;
    }

    void render() {
        TracyGpuZone("Render");

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        {
            TracyGpuZone("DrawMeshes");
            for (auto& mesh : meshes) {
                TracyGpuZone("DrawMesh");
                drawMesh(mesh);
            }
        }

        {
            TracyGpuZone("PostProcess");
            applyPostProcess();
        }
    }

    void present() {
        TracyGpuCollect;
        SDL_GL_SwapWindow(window);
    }
};
```

---

## Direct3D 11

### Инициализация

```cpp
#include "tracy/TracyD3D11.hpp"

// После создания D3D11 device и context
tracy::D3D11Ctx* d3dCtx = tracy::CreateD3D11Context(
    device,
    deviceContext
);
```

### Профилирование

```cpp
void renderFrame() {
    TracyD3D11Zone(d3dCtx, "RenderFrame");

    deviceContext->Draw(...);
}
```

### Collect

```cpp
void present() {
    TracyD3D11Collect(d3dCtx);
}
```

---

## Direct3D 12

### Инициализация

```cpp
#include "tracy/TracyD3D12.hpp"

tracy::D3D12Ctx* d3dCtx = tracy::CreateD3D12Context(
    device,
    commandQueue
);
```

### Профилирование

```cpp
void render(ID3D12GraphicsCommandList* cmdList) {
    TracyD3D12Zone(d3dCtx, cmdList, "Render");

    cmdList->DrawInstanced(...);
}
```

---

## Metal

### Инициализация

```cpp
#include "tracy/TracyMetal.hmm"

// После создания MTLDevice
TracyMetalCtx* metalCtx = TracyMetalContext(device);
```

### Профилирование

```cpp
void render(id<MTLRenderCommandEncoder> encoder) {
    TracyMetalZone(metalCtx, encoder, "Render");

    [encoder drawPrimitives:...];
}
```

---

## CUDA

### Инициализация

```cpp
#include "tracy/TracyCUDA.hpp"

void init() {
    TracyGpuContext;  // Или TracyCudaContext
}
```

### Профилирование

```cpp
void kernel() {
    TracyGpuZone("Kernel");

    // CUDA kernel launch
    myKernel<<<blocks, threads>>>(...);

    // cudaDeviceSynchronize() для синхронизации
}
```

---

## OpenCL

### Инициализация

```cpp
#include "tracy/TracyOpenCL.hpp"

tracy::OpenCLCtx* clCtx = tracy::CreateOpenCLContext(
    context,
    device,
    commandQueue
);
```

### Профилирование

```cpp
void executeKernel() {
    TracyCLZone(clCtx, "Kernel");

    clEnqueueNDRangeKernel(...);

    TracyCLCollect(clCtx);
}
```

---

## Лучшие практики

### Группировка GPU зон

```cpp
void renderFrame(VkCommandBuffer cmd) {
    TracyVkZone(tracyCtx, cmd, "Frame");

    {
        TracyVkZone(tracyCtx, cmd, "ShadowPass");
        renderShadows(cmd);
    }

    {
        TracyVkZone(tracyCtx, cmd, "GBufferPass");
        renderGBuffer(cmd);
    }

    {
        TracyVkZone(tracyCtx, cmd, "LightingPass");
        renderLighting(cmd);
    }

    {
        TracyVkZone(tracyCtx, cmd, "PostProcess");
        renderPostProcess(cmd);
    }
}
```

### Синхронизация CPU и GPU

```cpp
void frame() {
    FrameMark;  // CPU frame

    // GPU команды
    TracyVkZone(tracyCtx, cmd, "GPU Frame");

    // ... submit ...

    TracyVkCollect(tracyCtx, queue);
}
```

### Избегайте избыточных зон

```cpp
// Плохо: зона на каждый draw call
for (auto& obj : objects) {
    TracyVkZone(tracyCtx, cmd, "Draw");  // Слишком много зон!
    draw(cmd, obj);
}

// Хорошо: группировка
{
    TracyVkZone(tracyCtx, cmd, "DrawObjects");
    for (auto& obj : objects) {
        draw(cmd, obj);
    }
}
```

---

## Ограничения

1. **Количество queries**: GPU timestamp queries имеют лимит (обычно 32768)
2. **Overhead**: Каждая зона добавляет 2 timestamp queries
3. **Синхронизация**: Данные GPU доступны только после выполнения команд
4. **Точность**: Timestamp resolution зависит от GPU (обычно наносекунды)

---

## Устранение неполадок

### GPU зоны не отображаются

1. Проверьте вызов `TracyVkCollect` / `TracyGpuCollect`
2. Убедитесь, что очередь правильно передана
3. Проверьте, что timestamp queries поддерживаются

### Высокий overhead

1. Уменьшите количество GPU зон
2. Группируйте мелкие операции
3. Используйте условные зоны

### Ошибки Vulkan

```cpp
// Убедитесь, что queue поддерживает graphics
VkQueueFlags flags;
vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, &queueProps);
// queueFlags должен содержать VK_QUEUE_GRAPHICS_BIT

# Vulkan Performance

**🟡🔴 Уровень 2-3: Средний + Продвинутый** — Оптимизация производительности Vulkan: GPU-driven rendering, async compute,
memory management.

---

## Содержание

1. [GPU-Driven Rendering](#gpu-driven-rendering)
2. [Async Compute](#async-compute)
3. [Memory Optimization](#memory-optimization)
4. [Pipeline Optimization](#pipeline-optimization)
5. [Descriptor Management](#descriptor-management)
6. [Multi-threading](#multi-threading)
7. [Profiling](#profiling)

---

## GPU-Driven Rendering

### Концепция

GPU-Driven Rendering переносит принятие решений о рендеринге с CPU на GPU:

| Подход      | CPU работа                      | GPU работа                      | Draw calls |
|-------------|---------------------------------|---------------------------------|------------|
| Traditional | Culling, sorting, state changes | Vertex processing               | 1000+      |
| GPU-Driven  | Submit commands                 | Culling, sorting, indirect draw | 1-10       |

### Indirect Drawing

```cpp
// VkDrawIndexedIndirectCommand - структура в GPU памяти
struct DrawCommand {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t firstInstance;
};

// Compute shader заполняет draw commands
// CPU только отправляет один vkCmdDrawIndexedIndirect

VkDrawIndexedIndirectCommand* drawCommands;
vkCmdDrawIndexedIndirect(cmd, indirectBuffer, 0, drawCount, sizeof(VkDrawIndexedIndirectCommand));
```

### Indirect Dispatch

```cpp
struct DispatchCommand {
    uint32_t x, y, z;
};

vkCmdDispatchIndirect(cmd, dispatchBuffer, 0);
```

### Multi-Draw Indirect

```cpp
// Один вызов для множества объектов
vkCmdDrawIndexedIndirect(
    cmd,
    indirectBuffer,
    0,                    // offset
    drawCount,            // количество draw commands
    sizeof(DrawCommand)   // stride
);
```

### GPU Culling

```glsl
// Compute shader: Frustum culling
layout(local_size_x = 64) in;

struct DrawCommand {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

layout(std430, binding = 0) buffer DrawCommands {
    DrawCommand commands[];
};

layout(std430, binding = 1) buffer VisibleCount {
    uint count;
};

layout(std430, binding = 2) readonly buffer ObjectBounds {
    vec4 bounds[];  // xyz = center, w = radius
};

uniform mat4 viewProjection;

bool isInFrustum(vec3 center, float radius, mat4 vp) {
    vec4 clip = vp * vec4(center, 1.0);
    float w = clip.w;
    return clip.x > -w - radius && clip.x < w + radius &&
           clip.y > -w - radius && clip.y < w + radius &&
           clip.z > -w - radius && clip.z < w + radius;
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= objectCount) return;

    if (isInFrustum(bounds[idx].xyz, bounds[idx].w, viewProjection)) {
        uint drawIdx = atomicAdd(count, 1);
        commands[drawIdx] = drawData[idx];
        commands[drawIdx].instanceCount = 1;
    }
}
```

---

## Async Compute

### Концепция

Async Compute позволяет выполнять compute shaders параллельно с graphics работой на разных очередях.

```
Timeline:
Graphics Queue:  |----G1----|----G2----|----G3----|
Compute Queue:   |===C1===|====C2====|====C3====|
                           ↑
                    Параллельное выполнение
```

### Настройка очередей

```cpp
// Поиск отдельной compute очереди
uint32_t computeFamily = UINT32_MAX;
uint32_t graphicsFamily = UINT32_MAX;

for (uint32_t i = 0; i < queueFamilyCount; i++) {
    if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        graphicsFamily = i;
    }
    // Dedicated compute queue (без graphics)
    if ((queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
        !(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
        computeFamily = i;
    }
}

// Если dedicated нет, используем graphics queue
if (computeFamily == UINT32_MAX) {
    computeFamily = graphicsFamily;
}
```

### Синхронизация через Timeline Semaphores

```cpp
// Одна timeline semaphore для координации
VkSemaphore timelineSemaphore;
uint64_t timelineValue = 0;

// Graphics queue: render frame N
timelineValue++;
VkTimelineSemaphoreSubmitInfo timelineInfo = {};
timelineInfo.waitSemaphoreValueCount = 1;
uint64_t waitValue = timelineValue - 1;  // Ждать предыдущий compute
timelineInfo.pWaitSemaphoreValues = &waitValue;
timelineInfo.signalSemaphoreValueCount = 1;
uint64_t signalValue = timelineValue;  // Signal для compute
timelineInfo.pSignalSemaphoreValues = &signalValue;

// Compute queue: culling для frame N+1
// Ждать graphics signal, signal для следующего кадра
```

### Паттерн: Compute culling + Graphics rendering

```cpp
void renderFrame(uint32_t frameIndex) {
    // 1. Compute culling (async)
    VkSubmitInfo computeSubmit = {};
    computeSubmit.commandBufferCount = 1;
    computeSubmit.pCommandBuffers = &computeCmdBuffer;
    // Wait: previous graphics done
    // Signal: compute done
    vkQueueSubmit(computeQueue, 1, &computeSubmit, VK_NULL_HANDLE);

    // 2. Graphics rendering
    VkSubmitInfo graphicsSubmit = {};
    graphicsSubmit.commandBufferCount = 1;
    graphicsSubmit.pCommandBuffers = &graphicsCmdBuffer;
    // Wait: compute done
    // Signal: graphics done
    vkQueueSubmit(graphicsQueue, 1, &graphicsSubmit, fence);

    // 3. Present
    vkQueuePresentKHR(presentQueue, &presentInfo);
}
```

---

## Memory Optimization

### Типы памяти

```cpp
// Запрос свойств памяти
VkPhysicalDeviceMemoryProperties memProps;
vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

// Типичная конфигурация
// Type 0: DEVICE_LOCAL (fast, GPU-only)
// Type 1: HOST_VISIBLE | HOST_COHERENT (CPU accessible)
// Type 2: HOST_VISIBLE | HOST_CACHED (fast readback)
```

### Стратегии аллокации

```cpp
// Плохо: Много маленьких аллокаций
for (auto& mesh : meshes) {
    allocateBuffer(mesh.size);
}

// Хорошо: Одной большая аллокация
VkDeviceSize totalSize = 0;
for (auto& mesh : meshes) totalSize += mesh.size;
VkBuffer bigBuffer = allocateBuffer(totalSize);

// Размещение с offset
for (auto& mesh : meshes) {
    mesh.buffer = bigBuffer;
    mesh.offset = currentOffset;
    currentOffset += mesh.size;
}
```

### VMA Best Practices

```cpp
// GPU-only ресурсы
VmaAllocationCreateInfo gpuOnly = {};
gpuOnly.usage = VMA_MEMORY_USAGE_GPU_ONLY;

// Staging буферы
VmaAllocationCreateInfo staging = {};
staging.usage = VMA_MEMORY_USAGE_CPU_ONLY;

// Dynamic uniform buffers
VmaAllocationCreateInfo dynamic = {};
dynamic.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
dynamic.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

// Readback
VmaAllocationCreateInfo readback = {};
readback.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
```

### Memory Aliasing

```cpp
// Одна память для разных ресурсов в разное время
VkBufferCreateInfo bufferInfo = {};
bufferInfo.size = sharedSize;
bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

VkImageCreateInfo imageInfo = {};
imageInfo.extent = {width, height, 1};
imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

// Оба используют один VkDeviceMemory
// Явные barrier между использованием
```

---

## Pipeline Optimization

### Pipeline Cache

```cpp
// Создание cache
VkPipelineCache cache;
VkPipelineCacheCreateInfo cacheInfo = {};
cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
vkCreatePipelineCache(device, &cacheInfo, nullptr, &cache);

// Сохранение на диск
size_t cacheSize;
vkGetPipelineCacheData(device, cache, &cacheSize, nullptr);
std::vector<char> cacheData(cacheSize);
vkGetPipelineCacheData(device, cache, &cacheSize, cacheData.data());
std::ofstream file("pipeline.cache", std::ios::binary);
file.write(cacheData.data(), cacheSize);

// Загрузка при следующем запуске
std::ifstream file("pipeline.cache", std::ios::binary | std::ios::ate);
size_t size = file.tellg();
file.seekg(0);
std::vector<char> data(size);
file.read(data.data(), size);

VkPipelineCacheCreateInfo cacheInfo = {};
cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
cacheInfo.initialDataSize = size;
cacheInfo.pInitialData = data.data();
vkCreatePipelineCache(device, &cacheInfo, nullptr, &cache);
```

### Pipeline Derivatives

```cpp
// Базовый pipeline
VkGraphicsPipelineCreateInfo baseInfo = {};
baseInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
vkCreateGraphicsPipelines(device, cache, 1, &baseInfo, nullptr, &basePipeline);

// Производный pipeline (быстрее создание)
VkGraphicsPipelineCreateInfo derivedInfo = {};
derivedInfo.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
derivedInfo.basePipelineHandle = basePipeline;
derivedInfo.basePipelineIndex = -1;
vkCreateGraphicsPipelines(device, cache, 1, &derivedInfo, nullptr, &derivedPipeline);
```

### Dynamic State

```cpp
// Вместо создания разных pipelines для разных viewport/scissor
VkDynamicState dynamicStates[] = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
    VK_DYNAMIC_STATE_LINE_WIDTH,
    VK_DYNAMIC_STATE_BLEND_CONSTANTS,
};

VkPipelineDynamicStateCreateInfo dynamicInfo = {};
dynamicInfo.dynamicStateCount = 4;
dynamicInfo.pDynamicStates = dynamicStates;

// Установка во время рендеринга
vkCmdSetViewport(cmd, 0, 1, &viewport);
vkCmdSetScissor(cmd, 0, 1, &scissor);
```

---

## Descriptor Management

### Проблема множества descriptor sets

```cpp
// Плохо: Новый set для каждого объекта
for (auto& object : objects) {
    updateDescriptorSet(object.texture);
    vkCmdBindDescriptorSets(cmd, ...);
    vkCmdDrawIndexed(cmd, ...);
}
```

### Bindless подход

```cpp
// Хорошо: Один большой массив текстур
// Все текстуры доступны по индексу
layout(set = 0, binding = 0) uniform sampler2D textures[1024];

// В шейдере
vec4 color = texture(textures[material.textureIndex], uv);

// CPU: только обновляем индекс (push constant)
vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(textureIndex), &idx);
```

### Descriptor Indexing

```cpp
// Создание bindless layout
VkDescriptorSetLayoutBinding binding = {};
binding.binding = 0;
binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
binding.descriptorCount = 1024;  // Большой массив
binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

VkDescriptorBindingFlags flags =
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
```

### Uniform Buffer Strategy

```cpp
// Плохо: Отдельный UBO для каждого объекта
struct ObjectUBO { mat4 model; };
std::vector<VkBuffer> ubos(objectCount);

// Хорошо: Один UBO с массивом
struct SceneUBO {
    mat4 models[MAX_OBJECTS];
    mat4 viewProj;
};
VkBuffer sceneUBO;

// Индексация в шейдере
layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 models[];
    mat4 viewProj;
} scene;

void main() {
    gl_Position = scene.viewProj * scene.models[objectIndex] * vec4(pos, 1.0);
}
```

---

## Multi-threading

### Параллельная запись command buffers

```cpp
// Каждый поток имеет свой command pool
std::vector<VkCommandPool> threadPools(threadCount);
std::vector<VkCommandBuffer> threadCmdBuffers(threadCount);

// Параллельная запись
#pragma omp parallel for
for (int i = 0; i < threadCount; i++) {
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(threadCmdBuffers[i], &beginInfo);
    recordCommands(threadCmdBuffers[i], objectsForThread[i]);
    vkEndCommandBuffer(threadCmdBuffers[i]);
}

// Сборка в primary command buffer
vkCmdExecuteCommands(primaryCmdBuffer, threadCount, threadCmdBuffers.data());
```

### Secondary Command Buffers

```cpp
// Secondary: записываются параллельно
VkCommandBufferAllocateInfo secondaryAlloc = {};
secondaryAlloc.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;

// Secondary должен знать inheritance info
VkCommandBufferInheritanceInfo inheritance = {};
inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
inheritance.renderPass = renderPass;
inheritance.subpass = 0;
inheritance.framebuffer = framebuffer;

VkCommandBufferBeginInfo secondaryBegin = {};
secondaryBegin.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
secondaryBegin.pInheritanceInfo = &inheritance;
```

---

## Profiling

### GPU Timestamps

```cpp
// Создание query pool
VkQueryPoolCreateInfo queryPoolInfo = {};
queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
queryPoolInfo.queryCount = 2;  // Begin и End

VkQueryPool queryPool;
vkCreateQueryPool(device, &queryPoolInfo, nullptr, &queryPool);

// Запись timestamp
vkCmdResetQueryPool(cmd, queryPool, 0, 2);
vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool, 0);

// ... команды для измерения ...

vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, 1);

// Чтение результатов
uint64_t timestamps[2];
vkGetQueryPoolResults(device, queryPool, 0, 2, sizeof(timestamps),
                       timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

float milliseconds = (timestamps[1] - timestamps[0]) * timestampPeriod / 1e6f;
```

### Pipeline Statistics

```cpp
VkQueryPoolCreateInfo statPoolInfo = {};
statPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
statPoolInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
statPoolInfo.queryCount = 1;
statPoolInfo.pipelineStatistics =
    VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
    VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
    VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT;

VkQueryPool statPool;
vkCreateQueryPool(device, &statPoolInfo, nullptr, &statPool);

// Перед render pass
vkCmdBeginQuery(cmd, statPool, 0, 0);

// ... render pass ...

vkCmdEndQuery(cmd, statPool, 0);
```

### RenderDoc Integration

```cpp
#ifdef ENABLE_RENDERDOC
// Включение capture в коде
RENDERDOC_API_1_1_2* rdoc_api = nullptr;
HMODULE mod = GetModuleHandleA("renderdoc.dll");
if (mod) {
    pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
    RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&rdoc_api);
}

// Trigger capture
if (rdoc_api) rdoc_api->TriggerCapture();
#endif
```

---

## Чеклист оптимизации

| Категория   | Оптимизация                  | Приоритет |
|-------------|------------------------------|-----------|
| Draw calls  | Indirect drawing, Multi-draw | Высокий   |
| Culling     | GPU frustum culling          | Высокий   |
| Memory      | Fewer allocations, VMA       | Высокий   |
| Pipeline    | Cache, Derivatives           | Средний   |
| Descriptors | Bindless, Fewer updates      | Средний   |
| Threading   | Parallel recording           | Средний   |
| Profiling   | Timestamps, Statistics       | Низкий    |
